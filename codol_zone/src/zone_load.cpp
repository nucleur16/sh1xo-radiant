// Runs the 1.1.1.13 zone loaders over an inflated zone.
//
// The loaders themselves ( loaders_1_1_1_13.inc ) are a machine translation of the client's Load_* functions; they
// address the game's own globals ( stream position, var* pointers ) inside VMem. This file supplies the handful of
// engine functions they call ( Natives ) and the driver that the client has in DB_LoadXFile_Internal 0x004B5D50 /
// Load_XAssetArray 0x004B5CC0. Both follow CODOL-AssetExplorer ( ZoneLoadEmulator.cs, LoaderNatives.cs ).

#include "zone_impl.hpp"

#include <cstdarg>
#include <cstdio>

namespace codol::zone
{
	namespace
	{
		// globals of codoMP_client_shipRetail.exe 1.1.1.13 ( GameBuild.cs, V1_1_1_13 )
		constexpr std::uint32_t G_STREAM_POS = 0x01EABC3Cu;
		constexpr std::uint32_t G_STREAM_POS_INDEX = 0x01EBDC44u;
		constexpr std::uint32_t G_STREAM_BLOCKS = 0x01EABBDCu;
		constexpr std::uint32_t G_STREAM_POS_STACK_INDEX = 0x01DEA3CCu;
		constexpr std::uint32_t G_STREAM_BLOCK_POS = 0x0203B2A4u;
		constexpr std::uint32_t G_XASSET_LIST = 0x017203ECu;
		constexpr std::uint32_t G_VAR_XASSET_LIST = 0x017C6B6Cu;
		constexpr std::uint32_t G_VAR_SCRIPT_STRING_LIST = 0x017C6E08u;
		constexpr std::uint32_t G_VAR_SCRIPT_STRING_ARRAY = 0x017C6B08u;
		constexpr std::uint32_t G_VAR_XASSET_ARRAY = 0x017C6A60u;
		constexpr std::uint32_t G_VAR_XASSET_HEADER = 0x017C6B48u;

		constexpr std::uint32_t XFILE_HEADER_SIZE = 40;
		constexpr std::uint32_t XASSET_SIZE = 16;				// type, stream offset, header, stream size
		constexpr std::uint32_t XASSET_HEADER_OFFSET = 8;
		constexpr std::uint32_t POOL_ENTRY_HEADER_OFFSET = 8;
		constexpr std::uint32_t MAX_PRESERVED_IMAGE_BYTES = 64u << 20;

		using uint = std::uint32_t;
		using sbyte = std::int8_t;
		using byte = std::uint8_t;
		using ushort = std::uint16_t;
		using ulong = std::uint64_t;

		namespace Rt
		{
			template <typename T>
			T PreAdd(T& v, const int d)
			{
				v = static_cast<T>(static_cast<std::uint32_t>(v) + static_cast<std::uint32_t>(d));
				return v;
			}

			template <typename T>
			T PostAdd(T& v, const int d)
			{
				const T old = v;
				v = static_cast<T>(static_cast<std::uint32_t>(v) + static_cast<std::uint32_t>(d));
				return old;
			}

			template <typename T>
			void Discard(T)
			{
			}
		}

		std::string format(const char* fmt, ...)
		{
			char text[512];
			va_list args;
			va_start(args, fmt);
			std::vsnprintf(text, sizeof(text), fmt, args);
			va_end(args);
			return text;
		}

		// the engine functions the loaders call
		struct Natives
		{
			Zone::Impl& zone;
			VMem& M;
			const std::vector<std::uint8_t>& bytes;
			std::uint64_t streamOffset = 0;
			std::uint32_t poolTop = POOL_BASE;
			std::uint32_t entryTop = ENTRY_BASE;
			std::uint32_t tempHighWater = 0;
			std::int32_t currentAsset = -1;
			std::int32_t currentType = -1;
			std::unordered_map<std::string, std::uint16_t> scriptStringLookup;

			[[noreturn]] void fail(const std::string& detail) const
			{
				std::string where = format(" [zone stream 0x%llX of 0x%llX", static_cast<unsigned long long>(streamOffset),
					static_cast<unsigned long long>(bytes.size()));
				if (currentAsset >= 0)
				{
					where += format(", asset #%d %s", currentAsset, asset_type_name(currentType).c_str());
				}
				throw LoadError(detail + where + "]");
			}

			// DB_LoadXFileData 0x004B69C0
			void DB_LoadXFileData(const uint ptr, const uint size)
			{
				if (streamOffset + size > bytes.size())
				{
					fail(format("read of %u bytes past the end of the zone", size));
				}
				M.write(ptr, bytes.data() + streamOffset, size);
				streamOffset += size;
				track_temp(ptr, size);
			}

			// Load_XStringData 0x004CEEF0
			int Load_XStringData(const uint strPtrAddress)
			{
				const uint dest = M.U32(strPtrAddress);
				const void* nul = streamOffset < bytes.size()
					? std::memchr(bytes.data() + streamOffset, 0, static_cast<std::size_t>(bytes.size() - streamOffset))
					: nullptr;
				if (!nul)
				{
					fail("unterminated string at the end of the zone");
				}
				const uint length = static_cast<uint>(static_cast<const std::uint8_t*>(nul) - (bytes.data() + streamOffset));
				M.write(dest, bytes.data() + streamOffset, length + 1);
				streamOffset += length + 1;
				track_temp(dest, length + 1);
				const uint pos = M.U32(G_STREAM_POS) + length + 1;
				M.WU32(G_STREAM_POS, pos);
				return static_cast<int>(pos);
			}

			uint Memset(const uint destination, const uint value, const uint length)
			{
				return M.Memset(destination, value, length);
			}

			// DB_AddXAsset 0x004CC390: the header moves from the temp block into the asset pool
			uint DB_AddXAsset(const uint type, const uint headerPtrAddress)
			{
				const uint header = M.U32(headerPtrAddress);
				const uint size = type < static_cast<uint>(c1::ASSET_TYPE_COUNT) ? c1::XASSET_HEADER_SIZE[type] : 0;
				if (!size)
				{
					fail(format("DB_AddXAsset called for asset type %u that has no header size", type));
				}
				const uint pooled = alloc_pool(size);
				M.Memcpy(pooled, header, size);
				if (type == static_cast<uint>(c1::ASSET_TYPE_IMAGE))
				{
					preserve_temp_image_load_def(pooled);
				}
				if (entryTop + 24 >= ENTRY_LIMIT)
				{
					fail("asset entry table exhausted");
				}
				const uint entry = entryTop;
				entryTop += 24;
				M.WU32(entry, type);
				M.WU32(entry + POOL_ENTRY_HEADER_OFFSET, pooled);
				zone.records.push_back(AssetRecord{ static_cast<std::int32_t>(type), pooled, currentAsset });
				return entry;
			}

			// DB_FindXAssetHeader 0x004CB8E0: an asset of another zone, known by name only
			uint DB_FindXAssetHeader(const uint type, const uint namePtr)
			{
				uint size = type < static_cast<uint>(c1::ASSET_TYPE_COUNT) ? c1::XASSET_HEADER_SIZE[type] : 0;
				if (size < 4)
				{
					size = 4;
				}
				const uint placeholder = alloc_pool(size);
				std::string name = read_c_string(namePtr);
				if (!name.empty())
				{
					zone.references.emplace(placeholder, AssetReference{ static_cast<std::int32_t>(type), std::move(name) });
				}
				return placeholder;
			}

			int Sys_EnterCriticalSection(uint) { return 0; }
			int Sys_LeaveCriticalSection() { return 0; }

			// MSS_FixupLoadedSoundInfo 0x007401C0
			int MSS_FixupLoadedSoundInfo(const uint mssSound, uint)
			{
				return static_cast<int>(M.U32(mssSound + 40));
			}

			// SL_GetStringOfSize 0x005CAFF0: one handle per distinct string, 0 is reserved
			uint SL_GetStringOfSize(const uint str, uint, uint)
			{
				std::string value = read_c_string(str);
				if (const auto it = scriptStringLookup.find(value); it != scriptStringLookup.end())
				{
					return it->second;
				}
				if (zone.scriptStringHandles.size() > 0xFFFF)
				{
					fail("script string table exceeds 16 bit handles");
				}
				const auto handle = static_cast<std::uint16_t>(zone.scriptStringHandles.size());
				zone.scriptStringHandles.push_back(value);
				scriptStringLookup.emplace(std::move(value), handle);
				return handle;
			}

			// renderer / sound / registry calls that do not touch the zone stream
			uint Stub(uint) { return 0; }

			std::string read_c_string(const uint address)
			{
				std::string out;
				if (!address)
				{
					return out;
				}
				for (uint i = 0; i < 4096; i++)
				{
					const std::uint8_t c = M.U8(address + i);
					if (!c)
					{
						break;
					}
					out.push_back(static_cast<char>(c));
				}
				return out;
			}

			uint alloc_pool(const uint size)
			{
				const uint address = poolTop;
				poolTop = (poolTop + size + 3) & ~3u;
				if (poolTop >= POOL_LIMIT)
				{
					fail("asset pool exhausted");
				}
				M.at(address, size ? size : 1);
				return address;
			}

			// a GfxImageLoadDef that was read into the temp block would be overwritten by the next asset
			void preserve_temp_image_load_def(const uint pooledImage)
			{
				const uint loadDef = M.U32(pooledImage);
				if (!loadDef || Zone::block_of(loadDef) != 0)
				{
					return;
				}
				const uint resourceSize = M.U32(loadDef + 12);
				if (resourceSize > MAX_PRESERVED_IMAGE_BYTES)
				{
					return;
				}
				const uint copy = alloc_pool(16 + resourceSize);
				M.Memcpy(copy, loadDef, 16 + resourceSize);
				M.WU32(pooledImage, copy);
			}

			void track_temp(const uint destination, const uint size)
			{
				if (Zone::block_of(destination) == 0)
				{
					const uint top = destination + size - block_base(0);
					if (top > tempHighWater)
					{
						tempHighWater = top;
					}
				}
			}
		};

#pragma warning(push)
#pragma warning(disable : 4100)	// unreferenced parameter
#pragma warning(disable : 4101 4189)	// unreferenced / unused local ( registers of the original function )
#pragma warning(disable : 4102)	// unreferenced label
#pragma warning(disable : 4127)	// conditional expression is constant
#pragma warning(disable : 4702)	// unreachable code ( `return {}` after a return )
#pragma warning(disable : 4245 4244 4310)	// explicit truncating casts of the original arithmetic

		// translated loaders: member names are the addresses of the client's functions
		struct Loaders
		{
			VMem& M;
			Natives& Nat;

#include "loaders_1_1_1_13.inc"
		};

#pragma warning(pop)

		struct Driver
		{
			Zone::Impl& zone;
			VMem& m;
			Natives nat;
			Loaders loaders;

			Driver(Zone::Impl& z, const std::vector<std::uint8_t>& bytes)
				: zone(z), m(z.mem), nat{ z, z.mem, bytes }, loaders{ z.mem, nat }
			{
			}

			void push(const int block) { loaders.F_4CEE10(block); }	// DB_PushStreamPos
			void pop() { loaders.F_4CEE70(); }						// DB_PopStreamPos

			void align_stream_pos(const std::uint32_t listField, const std::uint32_t var)
			{
				const std::uint32_t pos = (m.U32(G_STREAM_POS) + 3) & ~3u;
				m.WU32(G_STREAM_POS, pos);
				m.WU32(listField, pos);
				m.WU32(var, pos);
			}

			void run()
			{
				const auto& bytes = nat.bytes;
				if (bytes.size() < XFILE_HEADER_SIZE + 16)
				{
					throw LoadError("zone is shorter than the XFile header and the asset list");
				}

				const auto u32 = [&](const std::size_t offset)
				{
					std::uint32_t v;
					std::memcpy(&v, bytes.data() + offset, 4);
					return v;
				};
				zone.header.size = u32(0);
				zone.header.externalSize = u32(4);
				for (int b = 0; b < BLOCK_COUNT; b++)
				{
					zone.header.blockSize[b] = u32(8 + 4 * static_cast<std::size_t>(b));
				}
				zone.streamSize = bytes.size();
				if (static_cast<std::uint64_t>(zone.header.size) + XFILE_HEADER_SIZE != bytes.size())
				{
					zone.warnings.push_back(format("XFile.size 0x%X + 0x28 does not match the inflated length 0x%llX", zone.header.size,
						static_cast<unsigned long long>(bytes.size())));
				}

				m.reset();
				m.reserve_region(0, 0x02100000u);
				Loaders::write_initial_globals(m);

				for (int b = 0; b < BLOCK_COUNT; b++)
				{
					const std::uint32_t size = zone.header.blockSize[b];
					if (size >= VMem::REGION_SIZE)
					{
						throw LoadError(format("block %d size 0x%X exceeds the 256 MB block window", b, size));
					}
					m.reserve_region(static_cast<std::uint32_t>(b + 1), size);
					m.WU32(XBLOCK_ARRAY + 8u * static_cast<std::uint32_t>(b), block_base(b));
					m.WU32(XBLOCK_ARRAY + 8u * static_cast<std::uint32_t>(b) + 4, size);
					m.WU32(G_STREAM_BLOCK_POS + 4u * static_cast<std::uint32_t>(b), block_base(b));
				}
				m.WU32(G_STREAM_POS, block_base(0));
				m.WU32(G_STREAM_BLOCKS, XBLOCK_ARRAY);
				m.WU32(G_STREAM_POS_INDEX, 0);
				m.WU32(G_STREAM_POS_STACK_INDEX, 0);

				nat.streamOffset = XFILE_HEADER_SIZE;
				zone.scriptStringHandles.assign(1, std::string());

				const std::uint32_t list = G_XASSET_LIST;
				m.WU32(G_VAR_XASSET_LIST, list);
				nat.DB_LoadXFileData(list, 16);
				const std::int32_t stringCount = m.I32(list);
				const std::int32_t assetCount = m.I32(list + 8);
				if (stringCount < 0 || stringCount > 1000000 || assetCount < 0 || assetCount > 1000000)
				{
					nat.fail("implausible XAssetList counts");
				}

				// DB_LoadXFile_Internal 0x004B5D50
				push(3);
				push(3);
				m.WU32(G_VAR_SCRIPT_STRING_LIST, list);
				if (m.U32(list + 4) != 0)
				{
					align_stream_pos(list + 4, G_VAR_SCRIPT_STRING_ARRAY);
					loaders.F_4B7000(stringCount);	// Load_ScriptStringArray
				}
				pop();
				pop();

				push(3);
				if (m.U32(list + 12) != 0)
				{
					align_stream_pos(list + 12, G_VAR_XASSET_ARRAY);
					load_asset_array(assetCount);
				}
				pop();

				if (const std::uint32_t strings = m.U32(list + 4); strings != 0)
				{
					zone.scriptStrings.reserve(static_cast<std::size_t>(stringCount));
					for (std::int32_t i = 0; i < stringCount; i++)
					{
						const std::uint32_t handle = m.U32(strings + 4u * static_cast<std::uint32_t>(i));
						zone.scriptStrings.push_back(handle < zone.scriptStringHandles.size() ? zone.scriptStringHandles[handle] : std::string());
					}
				}

				if (nat.streamOffset != bytes.size())
				{
					nat.currentAsset = -1;
					nat.fail("zone stream not fully consumed");
				}

				const std::int32_t currentBlock = m.I32(G_STREAM_POS_INDEX);
				for (int b = 0; b < BLOCK_COUNT; b++)
				{
					const std::uint32_t pos = b == currentBlock ? m.U32(G_STREAM_POS) : m.U32(G_STREAM_BLOCK_POS + 4u * static_cast<std::uint32_t>(b));
					zone.blocks[b].declared = zone.header.blockSize[b];
					zone.blocks[b].used = b == 0 ? nat.tempHighWater : pos - block_base(b);
				}
			}

			// Load_XAssetArray 0x004B5CC0
			void load_asset_array(const std::int32_t count)
			{
				const std::uint32_t size = XASSET_SIZE * static_cast<std::uint32_t>(count);
				const std::uint32_t array = m.U32(G_VAR_XASSET_ARRAY);
				if (size)
				{
					if (m.I32(G_STREAM_POS_INDEX) == 2)
					{
						m.Memset(array, 0, size);
					}
					else
					{
						nat.DB_LoadXFileData(array, size);
					}
					m.WU32(G_STREAM_POS, m.U32(G_STREAM_POS) + size);
				}

				zone.entries.reserve(static_cast<std::size_t>(count));
				std::uint32_t cursor = array;
				for (std::int32_t i = 0; i < count; i++, cursor += XASSET_SIZE)
				{
					XAssetEntry entry;
					entry.type = m.I32(cursor);
					entry.dataOffset = m.U32(cursor + 4);
					entry.dataSize = m.U32(cursor + 12);
					nat.currentAsset = i;
					nat.currentType = entry.type;

					const std::uint64_t expectedStart = static_cast<std::uint64_t>(XFILE_HEADER_SIZE) + entry.dataOffset;
					if (nat.streamOffset != expectedStart)
					{
						nat.fail(format("asset starts at stream 0x%llX, the asset table says 0x%llX",
							static_cast<unsigned long long>(nat.streamOffset), static_cast<unsigned long long>(expectedStart)));
					}
					if (entry.type < 0 || entry.type >= c1::ASSET_TYPE_COUNT)
					{
						nat.fail(format("asset type %d is outside the %d asset types of 1.1.1.13", entry.type, static_cast<int>(c1::ASSET_TYPE_COUNT)));
					}
					const bool loadable = c1::XASSET_HEADER_SIZE[entry.type] != 0 && entry.type != c1::ASSET_TYPE_SNDDRIVER_GLOBALS;
					if (!loadable)
					{
						zone.warnings.push_back(format("asset #%d is %s: Load_XAsset has no case for this type, the game skips it", i,
							asset_type_name(entry.type).c_str()));
					}

					m.WU32(G_VAR_XASSET_ARRAY, cursor);
					m.WU32(G_VAR_XASSET_HEADER, cursor + XASSET_HEADER_OFFSET);
					loaders.F_4C8ED0(0);	// Load_XAsset

					const std::uint64_t expectedEnd = expectedStart + entry.dataSize;
					if (nat.streamOffset != expectedEnd)
					{
						nat.fail(format("asset ended at stream 0x%llX, the asset table says 0x%llX ( delta %lld )",
							static_cast<unsigned long long>(nat.streamOffset), static_cast<unsigned long long>(expectedEnd),
							static_cast<long long>(nat.streamOffset) - static_cast<long long>(expectedEnd)));
					}

					entry.header = loadable ? m.U32(cursor + XASSET_HEADER_OFFSET) : 0;
					zone.entries.push_back(entry);
				}
				m.WU32(G_VAR_XASSET_ARRAY, cursor - size);
				nat.currentAsset = -1;
				nat.currentType = -1;
			}
		};
	}

	void run_zone_loaders(Zone::Impl& zone, const std::vector<std::uint8_t>& bytes)
	{
		// the driver holds ~13000 lines of loaders but no data of its own
		Driver driver(zone, bytes);
		driver.run();

		// only the blocks and the pool are part of the image
		zone.mem.release_region(0);
		zone.mem.release_region(1);		// temp block: reused per asset, nothing valid points into it
		zone.mem.release_region(ENTRY_BASE >> VMem::REGION_BITS);
		zone.mem.release_region(VMem::STACK_BASE >> VMem::REGION_BITS);
		zone.mem.shrink();
	}
}
