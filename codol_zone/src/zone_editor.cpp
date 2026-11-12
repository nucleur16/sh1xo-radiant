// Zone capture ( stream -> asset graph ) and zone writing ( asset graph -> stream ). See zone_editor.hpp.
//
// Both run the translated loaders through a LoadTracer:
//   Capture  watches a normal load. Every DB_LoadXFileData / Load_XStringData becomes a chunk. A 32 bit store that
//            replaces the stream value of a field ( block << 28 | offset ) + 1 with the address that value stands for
//            ( DB_ConvertOffsetToPointer ) or with the word at that address ( DB_ConvertOffsetToAlias ) marks the field
//            as a relocation. Stores to g_streamPos that move it forward without a stream read are allocations
//            ( DB_InsertPointer slots, runtime block arrays, alignment ); they are numbered per unit so that a
//            relocation into one can be found again when the zone is laid out anew.
//   Writer   supplies the chunks in order while the loaders run, with every relocation encoded for the new layout.

#include "codol_zone/zone_editor.hpp"
#include "codol_zone/assets.hpp"
#include "zone_impl.hpp"

#include <algorithm>
#include <cstdarg>
#include <map>

namespace codol::zone
{
	namespace
	{
		constexpr std::uint32_t NONE = 0xFFFFFFFFu;
		constexpr std::uint32_t INLINE = 0xFFFFFFFFu;			// zone pointer: the data follows in the stream
		constexpr std::uint32_t XFILE_HEADER_SIZE = 40;
		constexpr std::uint32_t XASSET_SIZE = 16;				// type, stream offset, header, stream size
		constexpr std::uint32_t XASSET_HEADER_OFFSET = 8;

		std::string format(const char* fmt, ...)
		{
			char text[768];
			va_list args;
			va_start(args, fmt);
			std::vsnprintf(text, sizeof(text), fmt, args);
			va_end(args);
			return text;
		}

		std::uint32_t get_u32(const std::uint8_t* p)
		{
			std::uint32_t v;
			std::memcpy(&v, p, 4);
			return v;
		}

		void put_u32(std::uint8_t* p, const std::uint32_t v)
		{
			std::memcpy(p, &v, 4);
		}

		// ( block << 28 | offset ) + 1 with a block the zone has
		bool is_zone_offset(const std::uint32_t value)
		{
			return value != 0 && value < 0xFFFFFFFEu && ((value - 1) >> 28) < static_cast<std::uint32_t>(BLOCK_COUNT);
		}

		std::uint32_t offset_to_address(const std::uint32_t value)
		{
			return block_base(static_cast<int>((value - 1) >> 28)) + ((value - 1) & 0x0FFFFFFFu);
		}

		std::uint32_t address_to_offset(const std::uint32_t address)
		{
			const int block = Zone::block_of(address);
			return ((static_cast<std::uint32_t>(block) << 28) | (address - block_base(block))) + 1;
		}

		struct Unit;

		enum class TargetKind : std::uint8_t
		{
			RAW,		// nothing the loaders placed: the stream value is kept
			CHUNK,		// unit, chunk index, offset
			ALLOC,		// unit, index into Unit::allocOrdinals, offset
			SLOT,		// header slot of top level asset `unit` in the XAsset table, offset inside the 16 byte entry
		};

		struct Reloc
		{
			std::uint32_t chunk = 0;		// field: chunk index and offset inside it
			std::uint32_t offset = 0;
			std::uint32_t raw = 0;			// stream value of the source zone
			TargetKind kind = TargetKind::RAW;
			const Unit* unit = nullptr;
			std::uint32_t index = 0;
			std::uint32_t targetOffset = 0;
		};

		struct ScriptRef
		{
			std::uint32_t chunk = 0;
			std::uint32_t offset = 0;
		};

		struct Chunk
		{
			std::uint64_t source = 0;		// offset into Unit::blob or Unit::local
			std::uint32_t size = 0;			// strings include the terminator
			std::uint32_t dest = 0;			// where the source zone's loader put it; 0 for data that was never loaded
			std::uint32_t loadedSize = 0;	// size it had there
			bool string = false;			// read by Load_XStringData
			bool local = false;
			bool foreign = false;			// taken from another zone: `dest` is an address of that zone
			std::uint32_t uid = 0;			// ZoneDataId, assigned when the typed editing API first needs it
		};

		// An allocation the loader makes without reading the stream, as a relocation target: the g_streamPos store
		// number `sub` after `anchor` chunks of the unit were read. Anchoring at the chunk keeps the key valid when
		// chunks are inserted or removed elsewhere in the unit.
		struct AllocKey
		{
			std::uint32_t anchor = 0;
			std::uint32_t sub = 0;
			bool operator<(const AllocKey& o) const { return anchor != o.anchor ? anchor < o.anchor : sub < o.sub; }
			bool operator==(const AllocKey& o) const { return anchor == o.anchor && sub == o.sub; }
		};

		struct Unit
		{
			std::int32_t type = -1;			// -1: the XAssetList root
			std::uint32_t rawHeader = INLINE;	// header word of the XAsset entry
			std::string name;
			std::shared_ptr<const std::vector<std::uint8_t>> blob;	// the zone stream the unit was captured from
			std::vector<std::uint8_t> local;						// edited / new data
			std::vector<Chunk> chunks;
			std::vector<Reloc> relocs;								// sorted by ( chunk, offset )
			std::vector<AllocKey> allocOrdinals;					// g_streamPos stores that are relocation targets
			std::vector<ScriptRef> scriptRefs;						// u16 fields that hold an index into the zone's script string list
			bool modified = false;
			std::shared_ptr<const Unit> importedFrom;				// the unit of a donor zone this one is an unchanged copy of

			const std::uint8_t* data(const Chunk& c) const
			{
				return (c.local ? local.data() : blob->data()) + c.source;
			}

			// moves a chunk's bytes into `local` so that they can be changed
			std::uint8_t* make_local(const std::uint32_t index)
			{
				Chunk& c = chunks[index];
				if (!c.local)
				{
					const std::uint8_t* from = data(c);
					const std::size_t at = local.size();
					local.insert(local.end(), from, from + c.size);
					c.source = at;
					c.local = true;
				}
				return local.data() + c.source;
			}

			void set_data(const std::uint32_t index, const std::span<const std::uint8_t> bytes)
			{
				Chunk& c = chunks[index];
				c.source = local.size();
				c.size = static_cast<std::uint32_t>(bytes.size());
				c.local = true;
				local.insert(local.end(), bytes.begin(), bytes.end());
			}

			std::uint32_t add_chunk(const std::span<const std::uint8_t> bytes, const bool string)
			{
				Chunk c;
				c.string = string;
				chunks.push_back(c);
				set_data(static_cast<std::uint32_t>(chunks.size() - 1), bytes);
				return static_cast<std::uint32_t>(chunks.size() - 1);
			}

			std::uint32_t add_string(const std::string_view text)
			{
				std::vector<std::uint8_t> bytes(text.begin(), text.end());
				bytes.push_back(0);
				return add_chunk(bytes, true);
			}

			std::string_view string_at(const std::uint32_t index) const
			{
				const Chunk& c = chunks[index];
				return { reinterpret_cast<const char*>(data(c)), c.size ? c.size - 1 : 0 };
			}

			std::uint64_t stream_bytes() const
			{
				std::uint64_t total = 0;
				for (const Chunk& c : chunks)
				{
					total += c.size;
				}
				return total;
			}
		};

		std::string describe(const Unit& unit, const std::size_t index)
		{
			if (unit.type < 0)
			{
				return "the XAssetList";
			}
			return format("asset #%zu %s '%s'", index, asset_type_name(unit.type).c_str(), unit.name.c_str());
		}

		// ---- capture ------------------------------------------------------------------------------

		struct Capture final : LoadTracer
		{
			struct CChunk
			{
				std::uint32_t dest, size;
				std::uint64_t source;
				std::uint32_t unit;
				bool string;
			};
			struct CReloc
			{
				std::uint32_t gid, offset, raw;
			};
			struct CAlloc
			{
				std::uint32_t start, size, unit;
				AllocKey key;
			};

			VMem* mem = nullptr;
			const std::vector<std::uint8_t>& stream;
			std::vector<CChunk> chunks;
			std::vector<std::uint32_t> blockChunks[BLOCK_COUNT];	// chunk ids by address, blocks 1 .. 7
			std::map<std::uint32_t, std::uint32_t> temp;			// block 0 ( reused ): address -> chunk id of what is there now
			std::vector<CReloc> relocs;
			std::vector<CAlloc> allocs[BLOCK_COUNT];
			std::vector<std::uint32_t> unitFirst{ 0 };				// first chunk id of every unit
			std::vector<std::pair<std::uint32_t, std::uint32_t>> scriptRefs;	// chunk id, offset
			std::uint32_t windowBase = 0;							// the script string array ( handles after the load of the list )
			std::uint32_t pendingEntry = NONE;						// array entry the loader read last
			std::vector<std::pair<std::uint32_t, std::uint32_t>> headers;	// pooled header -> chunk id
			std::uint32_t unit = 0;
			std::uint32_t posWrites = 0;							// g_streamPos stores since the last chunk of the unit
			std::uint32_t lastDest = 0, lastSize = 0;

			explicit Capture(const std::vector<std::uint8_t>& s) : stream(s) {}

			void attach(VMem& memory) override { mem = &memory; }

			std::uint32_t find_chunk(const std::uint32_t address) const
			{
				const int b = Zone::block_of(address);
				if (b == 0)
				{
					auto it = temp.upper_bound(address);
					if (it == temp.begin())
					{
						return NONE;
					}
					--it;
					return address - it->first < chunks[it->second].size ? it->second : NONE;
				}
				if (b < 0)
				{
					return NONE;
				}
				const auto& list = blockChunks[b];
				auto it = std::upper_bound(list.begin(), list.end(), address,
					[&](const std::uint32_t a, const std::uint32_t gid) { return a < chunks[gid].dest; });
				if (it == list.begin())
				{
					return NONE;
				}
				--it;
				return address - chunks[*it].dest < chunks[*it].size ? *it : NONE;
			}

			void on_stream_read(const std::uint32_t dest, const std::uint32_t size, const std::uint64_t offset, const bool string) override
			{
				const std::uint32_t gid = static_cast<std::uint32_t>(chunks.size());
				chunks.push_back(CChunk{ dest, size, offset, unit, string });
				lastDest = dest;
				lastSize = size;
				posWrites = 0;
				const int b = Zone::block_of(dest);
				if (b == 0)
				{
					auto it = temp.lower_bound(dest);
					if (it != temp.begin())
					{
						const auto before = std::prev(it);
						if (before->first + chunks[before->second].size > dest)
						{
							temp.erase(before);
						}
					}
					while (it != temp.end() && it->first - dest < size)
					{
						it = temp.erase(it);
					}
					temp.emplace(dest, gid);
				}
				else if (b > 0)
				{
					auto& list = blockChunks[b];
					if (list.empty() || chunks[list.back()].dest <= dest)
					{
						list.push_back(gid);
					}
					else
					{
						list.insert(std::upper_bound(list.begin(), list.end(), dest,
							[&](const std::uint32_t a, const std::uint32_t g) { return a < chunks[g].dest; }), gid);
					}
				}
			}

			void on_asset_begin(const std::int32_t, const std::uint64_t) override
			{
				unit++;
				unitFirst.push_back(static_cast<std::uint32_t>(chunks.size()));
				posWrites = 0;
			}

			void on_add_asset(const std::int32_t, const std::uint32_t loaded, const std::uint32_t pooled) override
			{
				const std::uint32_t gid = find_chunk(loaded);
				if (gid != NONE && chunks[gid].dest == loaded)
				{
					headers.emplace_back(pooled, gid);
				}
			}

			void on_script_strings(const std::uint32_t array, std::uint32_t) override
			{
				windowBase = array;
			}

			// Load_ScriptString: *field = list.strings[*field], a 16 bit read of the array followed by a 16 bit store
			void on_window_read(const std::uint32_t address) override
			{
				pendingEntry = address;
			}

			void on_write16(const std::uint32_t address, const std::uint16_t value) override
			{
				if (pendingEntry == NONE)
				{
					return;
				}
				const std::uint32_t gid = find_chunk(address);
				if (gid == NONE)
				{
					return;
				}
				const CChunk& c = chunks[gid];
				const std::uint32_t offset = address - c.dest;
				const std::uint8_t* handle = mem->peek(pendingEntry, 2);
				if (offset + 2 > c.size || !handle)
				{
					return;
				}
				std::uint16_t index, expected;
				std::memcpy(&index, stream.data() + c.source + offset, 2);
				std::memcpy(&expected, handle, 2);
				if (windowBase + 4u * index == pendingEntry && expected == value)
				{
					scriptRefs.emplace_back(gid, offset);
					pendingEntry = NONE;
				}
			}

			void on_write32(const std::uint32_t address, const std::uint32_t value) override
			{
				if (address == G_STREAM_POS)
				{
					const AllocKey key{ static_cast<std::uint32_t>(chunks.size()) - unitFirst.back(), posWrites++ };
					const std::uint8_t* p = mem->peek(address, 4);
					const std::uint32_t old = p ? get_u32(p) : 0;
					const int b = Zone::block_of(old);
					if (b >= 1 && value > old && Zone::block_of(value) == b && !(old == lastDest && value - old == lastSize))
					{
						allocs[b].push_back(CAlloc{ old, value - old, unit, key });
					}
					return;
				}
				if (address - block_base(0) >= static_cast<std::uint32_t>(BLOCK_COUNT) << 28)
				{
					return;
				}
				const std::uint32_t gid = find_chunk(address);
				if (gid == NONE)
				{
					return;
				}
				const CChunk& c = chunks[gid];
				const std::uint32_t offset = address - c.dest;
				if (offset + 4 > c.size)
				{
					return;
				}
				const std::uint32_t raw = get_u32(stream.data() + c.source + offset);
				if (!is_zone_offset(raw))
				{
					return;
				}
				const std::uint8_t* now = mem->peek(address, 4);
				if (!now || get_u32(now) != raw)
				{
					return;		// already converted
				}
				const std::uint32_t target = offset_to_address(raw);
				if (value != target)
				{
					const std::uint8_t* slot = mem->peek(target, 4);
					if (!slot || get_u32(slot) != value)
					{
						return;
					}
				}
				relocs.push_back(CReloc{ gid, offset, raw });
			}
		};

		// ---- writer -------------------------------------------------------------------------------

		struct Writer final : LoadTracer
		{
			VMem& mem;
			std::vector<const Unit*> order;									// [0] the root as written, then the assets
			std::unordered_map<const Unit*, std::uint32_t> indexOf;			// unit identity -> index in order
			std::vector<std::vector<std::uint32_t>> newDest, allocNew;
			std::vector<std::uint8_t> out;
			std::uint32_t u = 0, i = 0, r = 0;
			std::uint32_t posWrites = 0, allocCursor = 0;
			std::uint32_t tableChunk = NONE;								// index of the XAsset table in the root
			std::uint32_t tableDest = 0;
			std::size_t tableOut = 0;
			struct Patch
			{
				std::size_t out;
				const Reloc* reloc;
			};
			std::vector<Patch> patches;
			std::uint64_t relocCount = 0, chunkCount = 0, forward = 0;

			explicit Writer(VMem& m) : mem(m) {}

			[[noreturn]] void fail(const std::string& text) const
			{
				throw LoadError(text + format(" [%s, chunk %u of %zu, written stream 0x%zX]", describe(*order[u], u ? u - 1 : 0).c_str(), i,
					order[u]->chunks.size(), out.size()));
			}

			bool supplies_stream() const override { return true; }

			bool resolve(const Reloc& reloc, std::uint32_t& value) const
			{
				if (reloc.kind == TargetKind::RAW)
				{
					value = reloc.raw;
					return true;
				}
				const auto it = indexOf.find(reloc.unit);
				if (it == indexOf.end())
				{
					fail("a pointer leads to an asset that is not part of the zone");
				}
				std::uint32_t address = 0;
				switch (reloc.kind)
				{
				case TargetKind::CHUNK:
					address = newDest[it->second][reloc.index];
					break;
				case TargetKind::ALLOC:
					address = allocNew[it->second][reloc.index];
					break;
				default:
					address = tableDest && it->second ? tableDest + XASSET_SIZE * (it->second - 1) : 0;
					break;
				}
				if (!address)
				{
					return false;
				}
				if (Zone::block_of(address) < 1)
				{
					fail("a pointer leads to data the loader put into the temp block");
				}
				value = address_to_offset(address + reloc.targetOffset);
				return true;
			}

			const Chunk& next(const std::uint32_t dest, const bool string, const std::uint32_t size)
			{
				const Unit& unit = *order[u];
				if (i >= unit.chunks.size())
				{
					fail(string ? "the loader reads a string the asset graph does not hold"
						: format("the loader reads %u bytes the asset graph does not hold", size));
				}
				const Chunk& c = unit.chunks[i];
				if (c.string != string)
				{
					fail(string ? format("the loader reads a string where the asset graph holds %u bytes of data", c.size)
						: format("the loader reads %u bytes where the asset graph holds a string", size));
				}
				if (!string && c.size != size)
				{
					fail(format("the loader reads %u bytes where the asset graph holds %u ( a count in the asset does not match its data )", size, c.size));
				}
				const std::size_t at = out.size();
				const std::uint8_t* bytes = unit.data(c);
				out.insert(out.end(), bytes, bytes + c.size);
				for (; r < unit.relocs.size() && unit.relocs[r].chunk == i; r++)
				{
					const Reloc& reloc = unit.relocs[r];
					std::uint32_t value = reloc.raw;
					if (!resolve(reloc, value))
					{
						patches.push_back(Patch{ at + reloc.offset, &reloc });
					}
					put_u32(out.data() + at + reloc.offset, value);
					relocCount++;
				}
				newDest[u][i] = dest;
				posWrites = 0;
				if (u == 0 && i == tableChunk)
				{
					tableDest = dest;
					tableOut = at;
				}
				i++;
				chunkCount++;
				return c;
			}

			const std::uint8_t* supply(const std::uint32_t dest, const std::uint32_t size) override
			{
				const Chunk& c = next(dest, false, size);
				return out.data() + out.size() - c.size;
			}

			const std::uint8_t* supply_string(const std::uint32_t dest, std::uint32_t& size) override
			{
				const Chunk& c = next(dest, true, 0);
				size = c.size;
				return out.data() + out.size() - c.size;
			}

			void check_consumed() const
			{
				if (i != order[u]->chunks.size())
				{
					fail(format("the loader stopped %zu chunks before the end of the asset ( its data does not match its header )",
						order[u]->chunks.size() - i));
				}
			}

			void on_asset_begin(const std::int32_t index, const std::uint64_t) override
			{
				check_consumed();
				u = static_cast<std::uint32_t>(index) + 1;
				i = r = 0;
				posWrites = allocCursor = 0;
				if (u >= order.size())
				{
					u = static_cast<std::uint32_t>(order.size() - 1);
					fail("the XAsset table has more entries than the asset graph has assets");
				}
			}

			void on_write32(const std::uint32_t address, std::uint32_t) override
			{
				if (address != G_STREAM_POS)
				{
					return;
				}
				const AllocKey key{ i, posWrites++ };
				const auto& ordinals = order[u]->allocOrdinals;
				if (allocCursor < ordinals.size() && ordinals[allocCursor] == key)
				{
					const std::uint8_t* p = mem.peek(address, 4);
					allocNew[u][allocCursor++] = p ? get_u32(p) : 0;
				}
			}

			void finish()
			{
				check_consumed();
				if (u + 1 != order.size())
				{
					fail("the asset graph has more assets than the XAsset table");
				}
				for (const Patch& patch : patches)
				{
					std::uint32_t value = 0;
					if (!resolve(*patch.reloc, value))
					{
						throw LoadError("a pointer leads to data the loaders never placed ( the asset graph is inconsistent )");
					}
					put_u32(out.data() + patch.out, value);
				}
				forward = patches.size();
			}
		};

		template <typename F>
		void for_each_unit(const std::shared_ptr<Unit>& root, const std::vector<std::shared_ptr<Unit>>& assets, F&& f)
		{
			f(*root, static_cast<std::size_t>(0));
			for (std::size_t k = 0; k < assets.size(); k++)
			{
				f(*assets[k], k);
			}
		}
	}

	struct ZoneEditor::Impl
	{
		std::string name;
		Zone zone;
		bool loaded = false;
		bool hasBase = false;							// captured from a zone: header fields are carried over
		XFileHeader baseHeader;
		std::uint32_t baseUsed[BLOCK_COUNT] = {};
		std::vector<std::string> scriptStrings;
		ZoneCaptureStats stats;

		std::shared_ptr<Unit> root;
		std::uint32_t tableChunk = NONE;				// XAsset table chunk of the root
		std::vector<std::shared_ptr<Unit>> assets;
		std::unordered_map<zaddr, std::pair<const Unit*, std::uint32_t>> headerChunk;	// pooled header -> chunk it was read as

		std::size_t index_of(const Unit* unit) const
		{
			for (std::size_t k = 0; k < assets.size(); k++)
			{
				if (assets[k].get() == unit)
				{
					return k;
				}
			}
			return npos;
		}

		// first pointer of another unit that leads into `unit` and that `wanted` accepts; empty when there is none
		template <typename F>
		std::string inbound(const Unit* unit, F&& wanted) const
		{
			std::string found;
			for_each_unit(root, assets, [&](const Unit& other, const std::size_t k)
			{
				if (!found.empty() || &other == unit)
				{
					return;
				}
				for (const Reloc& reloc : other.relocs)
				{
					if (reloc.unit == unit && reloc.kind != TargetKind::RAW && wanted(reloc))
					{
						found = describe(other, k);
						return;
					}
				}
			});
			return found;
		}

		// ---- typed editing: data ids ----------------------------------------------------------
		struct Located
		{
			Unit* unit = nullptr;
			std::size_t asset = npos;		// npos: the root
			std::uint32_t index = 0;
		};
		struct AddressEntry
		{
			zaddr dest;
			std::uint32_t size, uid;
		};
		std::uint32_t nextUid = 1;
		bool idsDirty = true;
		std::size_t idsTotal = 0;
		std::unordered_map<std::uint32_t, std::pair<std::size_t, std::uint32_t>> hints;		// uid -> asset ( npos: root ), chunk index
		std::vector<AddressEntry> addressIndex;
		bool addressIndexed = false;

		Unit* unit_of(const std::size_t asset) const
		{
			return asset == npos ? root.get() : asset < assets.size() ? assets[asset].get() : nullptr;
		}

		void ensure_ids()
		{
			std::size_t total = root->chunks.size();
			for (const auto& unit : assets)
			{
				total += unit->chunks.size();
			}
			if (!idsDirty && total == idsTotal)
			{
				return;
			}
			bool fresh = false;
			for_each_unit(root, assets, [&](Unit& unit, std::size_t)
			{
				for (Chunk& c : unit.chunks)
				{
					if (!c.uid)
					{
						c.uid = nextUid++;
						fresh = true;
					}
				}
			});
			idsDirty = false;
			idsTotal = total;
			if (fresh)
			{
				addressIndexed = false;
			}
		}

		bool locate(const ZoneDataId id, Located& out)
		{
			if (!id)
			{
				return false;
			}
			ensure_ids();
			const auto check = [&](const std::size_t asset, const std::uint32_t index)
			{
				Unit* unit = unit_of(asset);
				if (unit && index < unit->chunks.size() && unit->chunks[index].uid == id)
				{
					out = Located{ unit, asset, index };
					return true;
				}
				return false;
			};
			if (const auto it = hints.find(id); it != hints.end() && check(it->second.first, it->second.second))
			{
				return true;
			}
			for (std::size_t a = 0; a <= assets.size(); a++)
			{
				const std::size_t asset = a == assets.size() ? npos : a;
				const Unit* unit = unit_of(asset);
				for (std::uint32_t k = 0; k < unit->chunks.size(); k++)
				{
					if (unit->chunks[k].uid == id)
					{
						hints[id] = { asset, k };
						return check(asset, k);
					}
				}
			}
			return false;
		}

		std::uint32_t uid_of(const Unit* unit, const std::uint32_t index)
		{
			ensure_ids();
			return unit && index < unit->chunks.size() ? unit->chunks[index].uid : 0;
		}

		ZoneTarget target_of(const Reloc& reloc)
		{
			ZoneTarget t;
			t.offset = reloc.targetOffset;
			t.raw = reloc.raw;
			switch (reloc.kind)
			{
			case TargetKind::CHUNK:
				t.kind = ZoneTarget::Kind::data;
				t.data = uid_of(reloc.unit, reloc.index);
				break;
			case TargetKind::ALLOC:
				t.kind = ZoneTarget::Kind::allocation;
				t.asset = index_of(reloc.unit);
				t.allocation = reloc.index;
				break;
			case TargetKind::SLOT:
				t.kind = ZoneTarget::Kind::asset;
				t.asset = index_of(reloc.unit);
				break;
			default:
				t.kind = ZoneTarget::Kind::raw;
				break;
			}
			return t;
		}

		Status make_reloc(const ZoneTarget& t, Reloc& reloc)
		{
			reloc.targetOffset = t.offset;
			reloc.raw = t.raw;
			switch (t.kind)
			{
			case ZoneTarget::Kind::data:
			{
				Located at;
				if (!locate(t.data, at))
				{
					return Status::error("a pointer names data that is not part of the zone");
				}
				if (t.offset > at.unit->chunks[at.index].size)
				{
					return Status::error("a pointer leads past the end of its target");
				}
				reloc.kind = TargetKind::CHUNK;
				reloc.unit = at.unit;
				reloc.index = at.index;
				return {};
			}
			case ZoneTarget::Kind::asset:
				if (t.asset >= assets.size() || t.offset + 4 > XASSET_SIZE)
				{
					return Status::error("a pointer names an asset that is not part of the zone");
				}
				reloc.kind = TargetKind::SLOT;
				reloc.unit = assets[t.asset].get();
				return {};
			case ZoneTarget::Kind::allocation:
			{
				const Unit* unit = unit_of(t.asset);
				if (!unit || t.allocation >= unit->allocOrdinals.size())
				{
					return Status::error("a pointer names a loader allocation that does not exist");
				}
				reloc.kind = TargetKind::ALLOC;
				reloc.unit = unit;
				reloc.index = t.allocation;
				return {};
			}
			case ZoneTarget::Kind::raw:
				reloc.kind = TargetKind::RAW;
				return {};
			default:
				return Status::error("a pointer without a target");
			}
		}

		// relocations of other chunks follow a chunk that was inserted at / removed from `at` of `unit`
		void renumber(Unit& unit, const std::uint32_t at, const bool inserted)
		{
			const auto shift = [&](std::uint32_t& index)
			{
				if (inserted ? index >= at : index > at)
				{
					index += inserted ? 1u : 0xFFFFFFFFu;
				}
			};
			for (Reloc& reloc : unit.relocs)
			{
				shift(reloc.chunk);
			}
			for (ScriptRef& ref : unit.scriptRefs)
			{
				shift(ref.chunk);
			}
			for_each_unit(root, assets, [&](Unit& other, std::size_t)
			{
				for (Reloc& reloc : other.relocs)
				{
					if (reloc.unit == &unit && reloc.kind == TargetKind::CHUNK)
					{
						shift(reloc.index);
					}
				}
			});
			for (auto& [pooled, where] : headerChunk)
			{
				if (where.first == &unit)
				{
					shift(where.second);
				}
			}
			// an allocation that follows the chunks before `at` stays with them
			for (AllocKey& key : unit.allocOrdinals)
			{
				if (key.anchor > at)
				{
					key.anchor += inserted ? 1u : 0xFFFFFFFFu;
				}
			}
			hints.clear();
		}

		void build(const Capture& capture);
		Status import(const ZoneEditor::Impl& donor, std::size_t donorIndex, std::size_t position, bool withDependencies,
			std::vector<std::shared_ptr<Unit>>& working, std::vector<std::string>& strings, std::shared_ptr<Unit>& imported, bool closure = false) const;
		void set_script_strings(const std::vector<std::string>& strings);
	};

	// appends the strings that `strings` has beyond the current list to the root: count, array entry, string
	void ZoneEditor::Impl::set_script_strings(const std::vector<std::string>& strings)
	{
		const auto insert = [&](const std::uint32_t at, const std::span<const std::uint8_t> bytes, const bool string)
		{
			Chunk c;
			c.string = string;
			root->chunks.insert(root->chunks.begin() + at, c);
			root->set_data(at, bytes);
			for (Reloc& reloc : root->relocs)
			{
				reloc.chunk += reloc.chunk >= at ? 1 : 0;
			}
			for_each_unit(root, assets, [&](Unit& unit, std::size_t)
			{
				for (Reloc& reloc : unit.relocs)
				{
					if (reloc.unit == root.get() && reloc.kind == TargetKind::CHUNK && reloc.index >= at)
					{
						reloc.index++;
					}
				}
			});
			if (tableChunk != NONE && tableChunk >= at)
			{
				tableChunk++;
			}
		};
		const std::uint8_t inlinePointer[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
		for (std::size_t k = scriptStrings.size(); k < strings.size(); k++)
		{
			std::uint8_t* list = root->make_local(0);
			const std::uint32_t count = get_u32(list);
			put_u32(list, count + 1);
			put_u32(list + 4, INLINE);
			if (!count)
			{
				insert(1, inlinePointer, false);
			}
			else
			{
				std::vector<std::uint8_t> array(root->data(root->chunks[1]), root->data(root->chunks[1]) + root->chunks[1].size);
				array.insert(array.end(), inlinePointer, inlinePointer + 4);
				root->set_data(1, array);
			}
			std::vector<std::uint8_t> text(strings[k].begin(), strings[k].end());
			text.push_back(0);
			insert(tableChunk != NONE ? tableChunk : static_cast<std::uint32_t>(root->chunks.size()), text, true);
			scriptStrings.push_back(strings[k]);
		}
	}

	void ZoneEditor::Impl::build(const Capture& capture)
	{
		const auto blob = root->blob;
		const std::size_t unitCount = capture.unitFirst.size();
		std::vector<std::shared_ptr<Unit>> units(unitCount);
		std::vector<std::uint32_t> indexInUnit(capture.chunks.size());
		for (std::size_t k = 0; k < unitCount; k++)
		{
			const std::shared_ptr<Unit> unit = k == 0 ? root : std::make_shared<Unit>();
			unit->blob = blob;
			const std::uint32_t first = capture.unitFirst[k];
			const std::uint32_t last = k + 1 < unitCount ? capture.unitFirst[k + 1] : static_cast<std::uint32_t>(capture.chunks.size());
			unit->chunks.reserve(last - first);
			for (std::uint32_t gid = first; gid < last; gid++)
			{
				const Capture::CChunk& c = capture.chunks[gid];
				Chunk chunk;
				chunk.source = c.source;
				chunk.size = c.size;
				chunk.dest = c.dest;
				chunk.loadedSize = c.size;
				chunk.string = c.string;
				indexInUnit[gid] = static_cast<std::uint32_t>(unit->chunks.size());
				unit->chunks.push_back(chunk);
			}
			units[k] = unit;
		}
		stats.chunks = capture.chunks.size();

		// the XAsset table is the last thing the root reads
		std::uint32_t tableGid = NONE;
		const auto entries = zone.assets();
		if (unitCount > 1)
		{
			tableGid = capture.unitFirst[1] - 1;
			if (capture.chunks[tableGid].size != XASSET_SIZE * entries.size() || entries.size() + 1 != unitCount)
			{
				throw LoadError("the XAsset table was not read the way the zone capture expects");
			}
			tableChunk = indexInUnit[tableGid];
			const std::uint8_t* table = blob->data() + capture.chunks[tableGid].source;
			for (std::size_t k = 0; k < entries.size(); k++)
			{
				Unit& unit = *units[k + 1];
				unit.type = entries[k].type;
				unit.rawHeader = get_u32(table + XASSET_SIZE * k + XASSET_HEADER_OFFSET);
				unit.name = std::string(zone.asset_name(entries[k].type, entries[k].header));
			}
		}

		// relocations, in field order
		std::vector<Capture::CReloc> relocs = capture.relocs;
		std::sort(relocs.begin(), relocs.end(), [](const Capture::CReloc& a, const Capture::CReloc& b)
		{
			return a.gid != b.gid ? a.gid < b.gid : a.offset < b.offset;
		});
		relocs.erase(std::unique(relocs.begin(), relocs.end(), [](const Capture::CReloc& a, const Capture::CReloc& b)
		{
			return a.gid == b.gid && a.offset == b.offset;
		}), relocs.end());

		std::vector<Capture::CAlloc> allocs[BLOCK_COUNT];
		for (int b = 1; b < BLOCK_COUNT; b++)
		{
			allocs[b] = capture.allocs[b];
			std::stable_sort(allocs[b].begin(), allocs[b].end(), [](const Capture::CAlloc& a, const Capture::CAlloc& c) { return a.start < c.start; });
		}

		struct Pending
		{
			Unit* unit;
			std::size_t reloc;
			std::uint32_t allocUnit;
			AllocKey ordinal;
		};
		std::vector<Pending> allocTargets;
		for (const Capture::CReloc& cr : relocs)
		{
			const Capture::CChunk& field = capture.chunks[cr.gid];
			Unit& unit = *units[field.unit];
			Reloc reloc;
			reloc.chunk = indexInUnit[cr.gid];
			reloc.offset = cr.offset;
			reloc.raw = cr.raw;
			const std::uint32_t target = offset_to_address(cr.raw);
			const int b = Zone::block_of(target);
			stats.relocations++;
			if (b >= 1)
			{
				const std::uint32_t gid = capture.find_chunk(target);
				if (gid != NONE)
				{
					const Capture::CChunk& c = capture.chunks[gid];
					if (gid == tableGid)
					{
						const std::uint32_t entry = (target - c.dest) / XASSET_SIZE;
						reloc.kind = TargetKind::SLOT;
						reloc.unit = units[entry + 1].get();
						reloc.targetOffset = (target - c.dest) % XASSET_SIZE;
						stats.toAssetSlot++;
					}
					else
					{
						reloc.kind = TargetKind::CHUNK;
						reloc.unit = units[c.unit].get();
						reloc.index = indexInUnit[gid];
						reloc.targetOffset = target - c.dest;
						(c.unit == field.unit ? stats.toChunk : stats.toOtherUnit)++;
					}
				}
				else
				{
					const auto& list = allocs[b];
					auto it = std::upper_bound(list.begin(), list.end(), target, [](const std::uint32_t a, const Capture::CAlloc& c) { return a < c.start; });
					if (it != list.begin() && target - std::prev(it)->start < std::prev(it)->size)
					{
						--it;
						reloc.kind = TargetKind::ALLOC;
						reloc.unit = units[it->unit].get();
						reloc.targetOffset = target - it->start;
						allocTargets.push_back(Pending{ &unit, unit.relocs.size(), it->unit, it->key });
						units[it->unit]->allocOrdinals.push_back(it->key);
						stats.toAllocation++;
					}
				}
			}
			if (reloc.kind == TargetKind::RAW)
			{
				stats.unresolved++;
				if (const char* debug = std::getenv("CODOL_ZONE_DEBUG_UNRESOLVED"); debug && (*debug == 'a' || stats.unresolved <= 60))
				{
					std::fprintf(stderr, "unresolved: unit %u type %s chunk %u ( %u bytes at 0x%08X ) + 0x%X = 0x%08X -> 0x%08X\n", field.unit,
						field.unit ? asset_type_name(zone.assets()[field.unit - 1].type).c_str() : "root", reloc.chunk, field.size, field.dest, cr.offset,
						cr.raw, target);
				}
			}
			unit.relocs.push_back(reloc);
		}
		for (const auto& unit : units)
		{
			auto& ordinals = unit->allocOrdinals;
			std::sort(ordinals.begin(), ordinals.end());
			ordinals.erase(std::unique(ordinals.begin(), ordinals.end()), ordinals.end());
		}
		for (const Pending& p : allocTargets)
		{
			const auto& ordinals = units[p.allocUnit]->allocOrdinals;
			p.unit->relocs[p.reloc].index = static_cast<std::uint32_t>(std::lower_bound(ordinals.begin(), ordinals.end(), p.ordinal) - ordinals.begin());
		}

		for (const auto& [gid, offset] : capture.scriptRefs)
		{
			units[capture.chunks[gid].unit]->scriptRefs.push_back(ScriptRef{ indexInUnit[gid], offset });
		}
		for (const auto& unit : units)
		{
			std::sort(unit->scriptRefs.begin(), unit->scriptRefs.end(), [](const ScriptRef& a, const ScriptRef& b)
			{
				return a.chunk != b.chunk ? a.chunk < b.chunk : a.offset < b.offset;
			});
		}
		for (const auto& [pooled, gid] : capture.headers)
		{
			headerChunk[pooled] = { units[capture.chunks[gid].unit].get(), indexInUnit[gid] };
		}
		assets.assign(units.begin() + 1, units.end());
	}

	ZoneEditor::ZoneEditor() = default;
	ZoneEditor::~ZoneEditor() = default;
	ZoneEditor::ZoneEditor(ZoneEditor&&) noexcept = default;
	ZoneEditor& ZoneEditor::operator=(ZoneEditor&&) noexcept = default;

	namespace
	{
		const std::string EMPTY_NAME;
		const Zone EMPTY_ZONE;
		const ZoneCaptureStats EMPTY_STATS;
	}

	bool ZoneEditor::loaded() const { return impl_ && impl_->loaded; }
	const std::string& ZoneEditor::name() const { return impl_ ? impl_->name : EMPTY_NAME; }
	const Zone& ZoneEditor::zone() const { return impl_ ? impl_->zone : EMPTY_ZONE; }
	const ZoneCaptureStats& ZoneEditor::capture_stats() const { return impl_ ? impl_->stats : EMPTY_STATS; }
	std::size_t ZoneEditor::asset_count() const { return impl_ ? impl_->assets.size() : 0; }

	Status ZoneEditor::load(std::vector<std::uint8_t> zoneBytes, std::string name)
	try
	{
		impl_.reset();
		auto impl = std::make_unique<Impl>();
		impl->name = name;
		const auto blob = std::make_shared<const std::vector<std::uint8_t>>(std::move(zoneBytes));
		impl->root = std::make_shared<Unit>();
		impl->root->blob = blob;

		Capture tracer(*blob);
		if (const Status s = impl->zone.load_traced(*blob, std::move(name), &tracer); !s)
		{
			return s;
		}

		impl->hasBase = true;
		impl->baseHeader = impl->zone.header();
		for (int b = 0; b < BLOCK_COUNT; b++)
		{
			impl->baseUsed[b] = impl->zone.block_usage(b).used;
		}
		impl->scriptStrings = impl->zone.script_strings();
		impl->build(tracer);
		impl->loaded = true;
		impl_ = std::move(impl);
		return {};
	}
	catch (const std::exception& e)
	{
		impl_.reset();
		return Status::error(e.what());
	}

	Status ZoneEditor::open(const std::filesystem::path& fastfile, const ClientKey& key, FastFileInfo* info)
	try
	{
		std::vector<std::uint8_t> bytes;
		if (const Status s = read_fastfile(fastfile, key, bytes, info); !s)
		{
			return s;
		}
		Status s = load(std::move(bytes), fastfile.stem().string());
		if (!s)
		{
			s.message = fastfile.filename().string() + ": " + s.message;
		}
		return s;
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::create(std::string name)
	try
	{
		auto impl = std::make_unique<Impl>();
		impl->name = std::move(name);
		impl->root = std::make_shared<Unit>();
		const std::uint8_t list[16] = {};		// no script strings, no assets
		impl->root->add_chunk(list, false);
		impl->root->chunks[0].dest = G_XASSET_LIST;
		impl->loaded = true;
		impl_ = std::move(impl);
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	EditorAsset ZoneEditor::asset(const std::size_t index) const
	{
		EditorAsset out;
		if (impl_ && index < impl_->assets.size())
		{
			const Unit& unit = *impl_->assets[index];
			out.type = unit.type;
			out.name = unit.name;
			out.chunks = static_cast<std::uint32_t>(unit.chunks.size());
			out.relocations = static_cast<std::uint32_t>(unit.relocs.size());
			out.streamBytes = unit.stream_bytes();
			out.modified = unit.modified;
			out.usesScriptStrings = !unit.scriptRefs.empty();
		}
		return out;
	}

	std::size_t ZoneEditor::find_asset(const std::int32_t type, const std::string_view name) const
	{
		if (impl_)
		{
			for (std::size_t k = 0; k < impl_->assets.size(); k++)
			{
				if (impl_->assets[k]->type == type && impl_->assets[k]->name == name)
				{
					return k;
				}
			}
		}
		return npos;
	}

	std::uint64_t ZoneEditor::asset_fingerprint(const std::size_t index) const
	{
		if (!impl_ || index >= impl_->assets.size())
		{
			return 0;
		}
		std::uint64_t hash = 0xCBF29CE484222325ull;
		const auto bytes = [&](const void* data, const std::size_t size)
		{
			const auto* p = static_cast<const std::uint8_t*>(data);
			for (std::size_t k = 0; k < size; k++)
			{
				hash = (hash ^ p[k]) * 0x100000001B3ull;
			}
		};
		const auto number = [&](const std::uint64_t value) { bytes(&value, sizeof(value)); };

		const Unit& unit = *impl_->assets[index];
		number(static_cast<std::uint32_t>(unit.type));
		number(unit.rawHeader);
		bytes(unit.name.data(), unit.name.size());
		number(unit.chunks.size());
		std::size_t r = 0, s = 0;
		std::vector<std::uint8_t> masked;
		for (std::uint32_t k = 0; k < unit.chunks.size(); k++)
		{
			const Chunk& c = unit.chunks[k];
			const std::uint8_t* data = unit.data(c);
			number(c.size);
			number(c.string ? 1u : 0u);
			const bool fields = (r < unit.relocs.size() && unit.relocs[r].chunk == k) || (s < unit.scriptRefs.size() && unit.scriptRefs[s].chunk == k);
			if (!fields)
			{
				bytes(data, c.size);
				continue;
			}
			// pointer and script string fields count by what they stand for
			masked.assign(data, data + c.size);
			for (; r < unit.relocs.size() && unit.relocs[r].chunk == k; r++)
			{
				const Reloc& reloc = unit.relocs[r];
				std::memset(masked.data() + reloc.offset, 0, 4);
				number(reloc.offset);
				number(static_cast<std::uint32_t>(reloc.kind));
				if (reloc.kind == TargetKind::RAW)
				{
					number(reloc.raw);
					continue;
				}
				if (reloc.unit != &unit)
				{
					// another unit, by what it is rather than where it is in the list
					number(static_cast<std::uint32_t>(reloc.unit->type));
					bytes(reloc.unit->name.data(), reloc.unit->name.size());
				}
				if (reloc.kind == TargetKind::ALLOC)
					{
						number(reloc.unit->allocOrdinals[reloc.index].anchor);
						number(reloc.unit->allocOrdinals[reloc.index].sub);
					}
					else
					{
						number(reloc.index);
					}
				number(reloc.targetOffset);
			}
			for (; s < unit.scriptRefs.size() && unit.scriptRefs[s].chunk == k; s++)
			{
				const ScriptRef& ref = unit.scriptRefs[s];
				std::uint16_t stringIndex;
				std::memcpy(&stringIndex, masked.data() + ref.offset, 2);
				std::memset(masked.data() + ref.offset, 0, 2);
				number(ref.offset);
				if (stringIndex < impl_->scriptStrings.size())
				{
					bytes(impl_->scriptStrings[stringIndex].data(), impl_->scriptStrings[stringIndex].size());
				}
			}
			bytes(masked.data(), masked.size());
		}
		return hash;
	}

	// ---- edits ------------------------------------------------------------------------------------

	Status ZoneEditor::set_map_ents(const std::string_view entityString)
	try
	{
		if (!loaded() || !impl_->zone.loaded())
		{
			return Status::error("no zone loaded");
		}
		const Zone& zone = impl_->zone;
		const zaddr header = find_map_ents(zone);
		if (!header)
		{
			return Status::error("the zone has no MapEnts asset");
		}
		const auto it = impl_->headerChunk.find(header);
		const std::size_t owner = it == impl_->headerChunk.end() ? npos : impl_->index_of(it->second.first);
		if (owner == npos)
		{
			return Status::error("the MapEnts asset is no longer part of the zone ( its owner was removed or replaced )");
		}
		Unit& unit = *impl_->assets[owner];
		const std::uint32_t headerIndex = it->second.second;
		if (unit.chunks[headerIndex].size != sizeof(c1::MapEnts))
		{
			return Status::error("the MapEnts header was not read as one 0x2C byte struct");
		}
		std::uint8_t* raw = unit.make_local(headerIndex);
		if (get_u32(raw + offsetof(c1::MapEnts, entityString)) != INLINE)
		{
			return Status::error("the entity string is not stored inline with the MapEnts asset");
		}
		const zaddr textAddress = zone.get<c1::MapEnts>(header).entityString;
		std::uint32_t textIndex = NONE;
		for (std::uint32_t k = headerIndex + 1; k < unit.chunks.size(); k++)
		{
			if (unit.chunks[k].dest == textAddress)
			{
				textIndex = k;
				break;
			}
		}
		if (textIndex == NONE)
		{
			return Status::error("the stream read of the entity string was not found");
		}
		for (const Reloc& reloc : unit.relocs)
		{
			if (reloc.chunk == textIndex)
			{
				return Status::error("the entity string holds zone pointers");
			}
		}
		const std::uint32_t newSize = static_cast<std::uint32_t>(entityString.size()) + 1;
		const auto into_text = [&](const Reloc& reloc)
		{
			return reloc.kind == TargetKind::CHUNK && reloc.index == textIndex && reloc.targetOffset >= newSize;
		};
		if (const std::string who = impl_->inbound(&unit, into_text); !who.empty())
		{
			return Status::error(who + " points into the part of the entity string that would be cut off");
		}
		for (const Reloc& reloc : unit.relocs)
		{
			if (reloc.unit == &unit && into_text(reloc))
			{
				return Status::error("the map's own data points into the part of the entity string that would be cut off");
			}
		}

		std::vector<std::uint8_t> text(entityString.begin(), entityString.end());
		text.push_back(0);
		unit.set_data(textIndex, text);
		raw = unit.make_local(headerIndex);		// set_data may have moved `local`
		put_u32(raw + offsetof(c1::MapEnts, numEntityChars), newSize);
		unit.modified = true;
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::set_rawfile(const std::string_view name, const std::span<const std::uint8_t> contents, const bool compress)
	try
	{
		if (!loaded())
		{
			return Status::error("no zone loaded");
		}
		impl_->idsDirty = true;
		if (name.empty() || contents.size() > 0x7FFFFFF0u)
		{
			return Status::error("rawfile needs a name and less than 2 GiB of contents");
		}
		// Load_RawFile reads compressedLen bytes when the file is deflated, len + 1 ( the terminator ) when it is not
		std::vector<std::uint8_t> stored;
		c1::RawFile header{};
		header.name = INLINE;
		header.buffer = INLINE;
		header.len = static_cast<std::int32_t>(contents.size());
		if (compress && !contents.empty())
		{
			if (const Status s = zlib_deflate(contents, stored, 9); !s)
			{
				return s;
			}
			header.compressedLen = static_cast<std::int32_t>(stored.size());
		}
		else
		{
			stored.assign(contents.begin(), contents.end());
			stored.push_back(0);
		}

		const std::size_t index = find_asset(c1::ASSET_TYPE_RAWFILE, name);
		if (index == npos)
		{
			auto unit = std::make_shared<Unit>();
			unit->type = c1::ASSET_TYPE_RAWFILE;
			unit->name = std::string(name);
			unit->modified = true;
			unit->add_chunk({ reinterpret_cast<const std::uint8_t*>(&header), sizeof(header) }, false);
			unit->add_string(name);
			unit->add_chunk(stored, false);
			impl_->assets.push_back(std::move(unit));
			return {};
		}

		Unit& unit = *impl_->assets[index];
		if (unit.chunks.empty() || unit.chunks[0].size != sizeof(c1::RawFile) || unit.chunks.size() > 3)
		{
			return Status::error("rawfile '" + std::string(name) + "' was not read as header, name and buffer");
		}
		c1::RawFile old;
		std::memcpy(&old, unit.data(unit.chunks[0]), sizeof(old));
		std::uint32_t bufferIndex = NONE;
		if (old.buffer == INLINE)
		{
			bufferIndex = static_cast<std::uint32_t>(unit.chunks.size() - 1);
			if (bufferIndex == 0 || unit.chunks[bufferIndex].string)
			{
				return Status::error("the buffer of rawfile '" + std::string(name) + "' was not found in the stream");
			}
		}
		else if (old.buffer != 0)
		{
			return Status::error("the buffer of rawfile '" + std::string(name) + "' is shared with other data");
		}
		if (bufferIndex != NONE)
		{
			const auto into_buffer = [&](const Reloc& reloc) { return reloc.kind == TargetKind::CHUNK && reloc.index == bufferIndex; };
			if (const std::string who = impl_->inbound(&unit, into_buffer); !who.empty())
			{
				return Status::error(who + " points into the buffer of rawfile '" + std::string(name) + "'");
			}
			unit.set_data(bufferIndex, stored);
		}
		else
		{
			unit.add_chunk(stored, false);
		}
		std::uint8_t* raw = unit.make_local(0);
		put_u32(raw + offsetof(c1::RawFile, compressedLen), static_cast<std::uint32_t>(header.compressedLen));
		put_u32(raw + offsetof(c1::RawFile, len), static_cast<std::uint32_t>(header.len));
		put_u32(raw + offsetof(c1::RawFile, buffer), INLINE);
		unit.modified = true;
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	std::int32_t string_table_hash(const std::string_view text)
	{
		// StringTable_HashString: hash = tolower( c ) + 31 * hash
		std::uint32_t hash = 0;
		for (const char c : text)
		{
			const unsigned char u = static_cast<unsigned char>(c);
			hash = static_cast<std::uint32_t>(u >= 'A' && u <= 'Z' ? u + 32 : u) + 31u * hash;
		}
		return static_cast<std::int32_t>(hash);
	}

	Status ZoneEditor::set_string_table(const std::string_view name, const StringTableData& table)
	try
	{
		if (!loaded())
		{
			return Status::error("no zone loaded");
		}
		impl_->idsDirty = true;
		if (name.empty() || table.columns < 0 || table.rows < 0
			|| table.cells.size() != static_cast<std::size_t>(table.columns) * static_cast<std::size_t>(table.rows))
		{
			return Status::error("string table needs a name and rows * columns cells");
		}
		Unit built;
		built.type = c1::ASSET_TYPE_STRINGTABLE;
		built.name = std::string(name);
		built.modified = true;
		c1::StringTable header{};
		header.name = INLINE;
		header.columnCount = table.columns;
		header.rowCount = table.rows;
		header.values = table.cells.empty() ? 0 : INLINE;
		built.add_chunk({ reinterpret_cast<const std::uint8_t*>(&header), sizeof(header) }, false);
		built.add_string(name);
		if (!table.cells.empty())
		{
			std::vector<c1::StringTableCell> cells(table.cells.size());
			for (std::size_t k = 0; k < cells.size(); k++)
			{
				cells[k].string = INLINE;
				cells[k].hash = string_table_hash(table.cells[k]);
			}
			built.add_chunk({ reinterpret_cast<const std::uint8_t*>(cells.data()), cells.size() * sizeof(c1::StringTableCell) }, false);
			for (const std::string& cell : table.cells)
			{
				built.add_string(cell);
			}
		}

		const std::size_t index = find_asset(c1::ASSET_TYPE_STRINGTABLE, name);
		if (index == npos)
		{
			impl_->assets.push_back(std::make_shared<Unit>(std::move(built)));
			return {};
		}
		Unit& unit = *impl_->assets[index];
		// other assets may share the table's name string, which stays where it was; anything else would dangle
		const bool sameName = unit.chunks.size() > 1 && unit.chunks[1].string && unit.string_at(1) == name;
		const auto dangling = [&](const Reloc& reloc)
		{
			return reloc.kind == TargetKind::ALLOC || (reloc.kind == TargetKind::CHUNK && !(sameName && reloc.index == 1));
		};
		if (const std::string who = impl_->inbound(&unit, dangling); !who.empty())
		{
			return Status::error(who + " shares data with string table '" + std::string(name) + "', which therefore cannot be rebuilt");
		}
		built.rawHeader = unit.rawHeader;
		unit = std::move(built);
		for (auto it = impl_->headerChunk.begin(); it != impl_->headerChunk.end();)
		{
			it = it->second.first == &unit ? impl_->headerChunk.erase(it) : std::next(it);
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::remove_asset(const std::size_t index)
	try
	{
		if (!loaded() || index >= impl_->assets.size())
		{
			return Status::error("no such asset");
		}
		const Unit* unit = impl_->assets[index].get();
		if (const std::string who = impl_->inbound(unit, [](const Reloc&) { return true; }); !who.empty())
		{
			return Status::error(describe(*unit, index) + " cannot be removed: " + who + " points to it or shares data with it");
		}
		impl_->assets.erase(impl_->assets.begin() + static_cast<std::ptrdiff_t>(index));
		impl_->idsDirty = true;
		impl_->hints.clear();
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	// copies donor asset `donorIndex` into `working` in front of `position`; pointers that leave the asset are re-targeted
	Status ZoneEditor::Impl::import(const ZoneEditor::Impl& donor, const std::size_t donorIndex, std::size_t position, const bool withDependencies,
		std::vector<std::shared_ptr<Unit>>& working, std::vector<std::string>& scriptList, std::shared_ptr<Unit>& imported, const bool closure) const
	{
		const Unit& source = *donor.assets[donorIndex];
		const std::string what = describe(source, donorIndex) + " of zone '" + donor.name + "'";
		auto unit = std::make_shared<Unit>(source);
		unit->modified = true;
		unit->importedFrom = donor.assets[donorIndex];
		imported = unit;
		for (Chunk& c : unit->chunks)
		{
			c.foreign = true;	// its addresses and data ids are those of the donor zone
			c.uid = 0;
		}

		// script string indices: the donor's list -> this zone's list, which grows by what it does not have
		for (const ScriptRef& ref : unit->scriptRefs)
		{
			std::uint8_t* field = unit->make_local(ref.chunk) + ref.offset;
			std::uint16_t index;
			std::memcpy(&index, field, 2);
			if (index >= donor.scriptStrings.size())
			{
				return Status::error(what + " holds a script string index outside its zone's list");
			}
			const std::string& text = donor.scriptStrings[index];
			std::size_t here = static_cast<std::size_t>(std::find(scriptList.begin(), scriptList.end(), text) - scriptList.begin());
			if (here == scriptList.size())
			{
				if (here >= 0xFFFF)
				{
					return Status::error("the script string list is full ( 65535 strings )");
				}
				scriptList.push_back(text);
			}
			index = static_cast<std::uint16_t>(here);
			std::memcpy(field, &index, 2);
		}
		if (position > working.size())
		{
			position = working.size();
		}
		working.insert(working.begin() + static_cast<std::ptrdiff_t>(position), unit);

		// equal strings of this zone, first one wins
		std::unordered_map<std::string_view, std::pair<const Unit*, std::uint32_t>> strings;
		bool indexed = false;
		const auto index_strings = [&]
		{
			indexed = true;
			const auto add = [&](const Unit& other)
			{
				for (std::uint32_t k = 0; k < other.chunks.size(); k++)
				{
					if (other.chunks[k].string && (other.chunks[k].dest == 0 || Zone::block_of(other.chunks[k].dest) > 0))
					{
						strings.emplace(other.string_at(k), std::make_pair(&other, k));
					}
				}
			};
			add(*root);
			for (const auto& other : working)
			{
				if (other != unit)
				{
					add(*other);
				}
			}
		};

		for (Reloc& reloc : unit->relocs)
		{
			if (reloc.kind == TargetKind::RAW)
			{
				continue;
			}
			if (reloc.unit == &source)
			{
				reloc.unit = unit.get();
				continue;
			}
			const std::size_t other = donor.index_of(reloc.unit);
			if (reloc.kind == TargetKind::SLOT && other != npos)
			{
				const Unit& wanted = *donor.assets[other];
				const Unit* here = nullptr;
				for (const auto& candidate : working)
				{
					if (candidate->type == wanted.type && candidate->name == wanted.name)
					{
						here = candidate.get();
						break;
					}
				}
				if (!here)
				{
					if (!withDependencies)
					{
						return Status::error(what + " refers to " + describe(wanted, other) + ", which this zone does not have");
					}
					std::shared_ptr<Unit> dependency;
					const auto at = static_cast<std::size_t>(std::find(working.begin(), working.end(), unit) - working.begin());
					if (const Status s = import(donor, other, at, true, working, scriptList, dependency, closure); !s)
					{
						return s;
					}
					here = dependency.get();
					indexed = false;
				}
				reloc.unit = here;
				continue;
			}
			if (closure && other != npos && (reloc.kind == TargetKind::CHUNK || reloc.kind == TargetKind::ALLOC))
			{
				// data of another asset of the donor: that asset comes along ( once ), the pointer keeps its place in it
				const Unit* here = nullptr;
				for (const auto& candidate : working)
				{
					if (candidate->importedFrom.get() == reloc.unit)
					{
						here = candidate.get();
						break;
					}
				}
				if (!here)
				{
					std::shared_ptr<Unit> dependency;
					const auto at = static_cast<std::size_t>(std::find(working.begin(), working.end(), unit) - working.begin());
					if (const Status s = import(donor, other, at, true, working, scriptList, dependency, true); !s)
					{
						return s;
					}
					here = dependency.get();
					indexed = false;
				}
				reloc.unit = here;
				continue;
			}
			if (reloc.kind == TargetKind::CHUNK)
			{
				const Unit& from = *reloc.unit;
				if (from.chunks[reloc.index].string)
				{
					if (!indexed)
					{
						strings.clear();
						index_strings();
					}
					const std::string_view text = from.string_at(reloc.index);
					if (const auto found = strings.find(text); found != strings.end())
					{
						reloc.unit = found->second.first;
						reloc.index = found->second.second;
						continue;
					}
					return Status::error(what + " shares the string \"" + std::string(text) + "\" with another asset of its zone, and this zone has no such string");
				}
			}
			return Status::error(what + " shares data with " + (other != npos ? describe(*donor.assets[other], other) : std::string("the XAssetList"))
				+ " of its zone");
		}
		return {};
	}

	Status ZoneEditor::add_asset(const ZoneEditor& donor, const std::size_t donorIndex, const std::size_t position, const bool withDependencies)
	try
	{
		if (!loaded() || !donor.loaded() || donorIndex >= donor.impl_->assets.size())
		{
			return Status::error("no such asset");
		}
		std::vector<std::shared_ptr<Unit>> working = impl_->assets;
		std::vector<std::string> strings = impl_->scriptStrings;
		std::shared_ptr<Unit> imported;
		if (const Status s = impl_->import(*donor.impl_, donorIndex, position, withDependencies, working, strings, imported); !s)
		{
			return s;
		}
		impl_->assets = std::move(working);
		impl_->set_script_strings(strings);
		impl_->idsDirty = true;
		impl_->hints.clear();
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::add_asset_closure(const ZoneEditor& donor, const std::size_t donorIndex, const std::size_t position)
	try
	{
		if (!loaded() || !donor.loaded() || donorIndex >= donor.impl_->assets.size())
		{
			return Status::error("no such asset");
		}
		std::vector<std::shared_ptr<Unit>> working = impl_->assets;
		std::vector<std::string> strings = impl_->scriptStrings;
		std::shared_ptr<Unit> imported;
		if (const Status s = impl_->import(*donor.impl_, donorIndex, position, true, working, strings, imported, true); !s)
		{
			return s;
		}
		impl_->assets = std::move(working);
		impl_->set_script_strings(strings);
		impl_->idsDirty = true;
		impl_->hints.clear();
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::add_new_asset(const std::int32_t type, const std::string_view name, const std::span<const ZoneNewPiece> pieces, const std::size_t position)
	try
	{
		if (!loaded())
		{
			return Status::error("no zone loaded");
		}
		if (type < 0 || type >= c1::ASSET_TYPE_COUNT || pieces.empty() || pieces[0].bytes.size() != c1::XASSET_HEADER_SIZE[type])
		{
			return Status::error("a new asset needs a known type and its header as the first piece");
		}
		auto unit = std::make_shared<Unit>();
		unit->type = type;
		unit->name = std::string(name);
		unit->modified = true;
		std::vector<std::string> strings = impl_->scriptStrings;
		for (std::uint32_t k = 0; k < pieces.size(); k++)
		{
			const ZoneNewPiece& piece = pieces[k];
			if (piece.bytes.empty() || (piece.string && piece.bytes.back() != 0))
			{
				return Status::error("a piece of a new asset cannot be empty, a string has to end with its terminator");
			}
			unit->add_chunk(piece.bytes, piece.string);
			for (const ZoneNewPointer& p : piece.pointers)
			{
				if (p.offset > piece.bytes.size() || piece.bytes.size() - p.offset < 4)
				{
					return Status::error("a pointer field does not lie inside its piece");
				}
				Reloc reloc;
				reloc.chunk = k;
				reloc.offset = p.offset;
				if (p.asset != npos)
				{
					if (p.asset >= impl_->assets.size())
					{
						return Status::error("a pointer names an asset that is not part of the zone");
					}
					reloc.kind = TargetKind::SLOT;
					reloc.unit = impl_->assets[p.asset].get();
					reloc.targetOffset = 8;
				}
				else
				{
					if (p.piece >= pieces.size() || p.pieceOffset > pieces[p.piece].bytes.size())
					{
						return Status::error("a pointer leads outside the pieces of its asset");
					}
					reloc.kind = TargetKind::CHUNK;
					reloc.unit = unit.get();
					reloc.index = p.piece;
					reloc.targetOffset = p.pieceOffset;
				}
				unit->relocs.push_back(reloc);
			}
			for (const auto& [offset, text] : piece.scriptStrings)
			{
				if (offset > piece.bytes.size() || piece.bytes.size() - offset < 2)
				{
					return Status::error("a script string field does not lie inside its piece");
				}
				std::size_t here = static_cast<std::size_t>(std::find(strings.begin(), strings.end(), text) - strings.begin());
				if (here == strings.size())
				{
					if (here >= 0xFFFF)
					{
						return Status::error("the script string list is full ( 65535 strings )");
					}
					strings.push_back(text);
				}
				const auto index = static_cast<std::uint16_t>(here);
				std::memcpy(unit->local.data() + unit->chunks[k].source + offset, &index, 2);
				unit->scriptRefs.push_back({ k, offset });
			}
		}
		std::sort(unit->relocs.begin(), unit->relocs.end(),
			[](const Reloc& a, const Reloc& b) { return a.chunk != b.chunk ? a.chunk < b.chunk : a.offset < b.offset; });
		impl_->assets.insert(impl_->assets.begin() + static_cast<std::ptrdiff_t>(std::min(position, impl_->assets.size())), std::move(unit));
		impl_->set_script_strings(strings);
		impl_->idsDirty = true;
		impl_->hints.clear();
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::add_built_asset(const std::int32_t type, const std::string_view name, const std::span<const ZoneBuildChunk> chunks,
		std::size_t position, std::size_t* index)
	try
	{
		if (!loaded())
		{
			return Status::error("no zone loaded");
		}
		if (type < 0 || type >= c1::ASSET_TYPE_COUNT || c1::XASSET_HEADER_SIZE[type] == 0)
		{
			return Status::error("asset type without a loader");
		}
		if (chunks.empty() || chunks[0].string || chunks[0].bytes.size() != c1::XASSET_HEADER_SIZE[type])
		{
			return Status::error("the first chunk of a built asset is its header ( " + std::to_string(c1::XASSET_HEADER_SIZE[type]) + " bytes for "
				+ asset_type_name(type) + " )");
		}
		auto unit = std::make_shared<Unit>();
		unit->type = type;
		unit->name = std::string(name);
		unit->modified = true;
		for (const ZoneBuildChunk& c : chunks)
		{
			unit->add_chunk(c.bytes, c.string);
		}
		for (std::uint32_t k = 0; k < chunks.size(); k++)
		{
			const ZoneBuildChunk& c = chunks[k];
			std::vector<ZoneBuildPointer> sorted(c.pointers.begin(), c.pointers.end());
			std::sort(sorted.begin(), sorted.end(), [](const ZoneBuildPointer& a, const ZoneBuildPointer& b) { return a.offset < b.offset; });
			for (const ZoneBuildPointer& p : sorted)
			{
				if (static_cast<std::uint64_t>(p.offset) + 4 > c.bytes.size())
				{
					return Status::error("a pointer field of a built asset lies outside its chunk");
				}
				Reloc reloc;
				reloc.chunk = k;
				reloc.offset = p.offset;
				if (p.slot)
				{
					if (p.asset >= impl_->assets.size())
					{
						return Status::error("a built asset points at an asset the zone does not have");
					}
					reloc.kind = TargetKind::SLOT;
					reloc.unit = impl_->assets[p.asset].get();
					reloc.index = 0;
					reloc.targetOffset = 8;
				}
				else
				{
					const Unit* target = unit.get();
					if (p.asset != npos)
					{
						if (p.asset >= impl_->assets.size())
						{
							return Status::error("a built asset points into an asset the zone does not have");
						}
						target = impl_->assets[p.asset].get();
					}
					if (p.chunk >= target->chunks.size() || p.targetOffset > target->chunks[p.chunk].size)
					{
						return Status::error("a pointer of a built asset leads outside its target");
					}
					reloc.kind = TargetKind::CHUNK;
					reloc.unit = target;
					reloc.index = p.chunk;
					reloc.targetOffset = p.targetOffset;
				}
				unit->relocs.push_back(reloc);
			}
			for (const std::uint32_t offset : c.scriptStrings)
			{
				if (static_cast<std::uint64_t>(offset) + 2 > c.bytes.size())
				{
					return Status::error("a script string field of a built asset lies outside its chunk");
				}
				unit->scriptRefs.push_back(ScriptRef{ k, offset });
			}
		}
		std::sort(unit->scriptRefs.begin(), unit->scriptRefs.end(), [](const ScriptRef& a, const ScriptRef& b)
		{
			return a.chunk != b.chunk ? a.chunk < b.chunk : a.offset < b.offset;
		});
		if (position > impl_->assets.size())
		{
			position = impl_->assets.size();
		}
		impl_->assets.insert(impl_->assets.begin() + static_cast<std::ptrdiff_t>(position), std::move(unit));
		if (index)
		{
			*index = position;
		}
		impl_->idsDirty = true;
		impl_->hints.clear();
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	std::uint16_t ZoneEditor::script_string(const std::string_view text)
	{
		if (!loaded())
		{
			return 0;
		}
		std::vector<std::string> list = impl_->scriptStrings;
		const std::size_t here = static_cast<std::size_t>(std::find(list.begin(), list.end(), text) - list.begin());
		if (here == list.size())
		{
			if (here >= 0xFFFF)
			{
				return 0;
			}
			list.emplace_back(text);
			impl_->set_script_strings(list);
			impl_->idsDirty = true;
		}
		return static_cast<std::uint16_t>(here);
	}

	Status ZoneEditor::replace_asset(const std::size_t index, const ZoneEditor& donor, const std::size_t donorIndex)
	try
	{
		if (!loaded() || !donor.loaded() || index >= impl_->assets.size() || donorIndex >= donor.impl_->assets.size())
		{
			return Status::error("no such asset");
		}
		const std::shared_ptr<Unit> target = impl_->assets[index];
		if (target->type != donor.impl_->assets[donorIndex]->type)
		{
			return Status::error("an asset can only be replaced with one of the same type");
		}
		const auto shares = [](const Reloc& reloc) { return reloc.kind != TargetKind::SLOT; };
		if (const std::string who = impl_->inbound(target.get(), shares); !who.empty())
		{
			return Status::error(describe(*target, index) + " cannot be replaced: " + who + " shares data with it");
		}
		// the replaced asset must not serve as a dependency of the new one
		std::vector<std::shared_ptr<Unit>> working = impl_->assets;
		working.erase(working.begin() + static_cast<std::ptrdiff_t>(index));
		std::vector<std::string> strings = impl_->scriptStrings;
		std::shared_ptr<Unit> imported;
		if (const Status s = impl_->import(*donor.impl_, donorIndex, index, false, working, strings, imported); !s)
		{
			return s;
		}
		// keep the identity of the replaced unit: other assets point at its header slot
		for (Reloc& reloc : imported->relocs)
		{
			if (reloc.unit == imported.get())
			{
				reloc.unit = target.get();
			}
		}
		*target = std::move(*imported);
		for (auto& slot : working)
		{
			if (slot == imported)
			{
				slot = target;
			}
		}
		for (auto it = impl_->headerChunk.begin(); it != impl_->headerChunk.end();)
		{
			it = it->second.first == target.get() ? impl_->headerChunk.erase(it) : std::next(it);
		}
		impl_->assets = std::move(working);
		impl_->set_script_strings(strings);
		impl_->idsDirty = true;
		impl_->hints.clear();
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	// ---- typed, relocation aware editing ------------------------------------------------------------

	ZoneDataId ZoneEditor::data_at(const zaddr address, std::uint32_t* offset) const
	{
		if (!loaded() || Zone::block_of(address) < 1)
		{
			return 0;
		}
		Impl& impl = *impl_;
		impl.ensure_ids();
		if (!impl.addressIndexed)
		{
			impl.addressIndex.clear();
			for_each_unit(impl.root, impl.assets, [&](const Unit& unit, std::size_t)
			{
				for (const Chunk& c : unit.chunks)
				{
					if (!c.foreign && c.loadedSize && Zone::block_of(c.dest) >= 1)
					{
						impl.addressIndex.push_back(Impl::AddressEntry{ c.dest, c.loadedSize, c.uid });
					}
				}
			});
			std::sort(impl.addressIndex.begin(), impl.addressIndex.end(), [](const Impl::AddressEntry& a, const Impl::AddressEntry& b) { return a.dest < b.dest; });
			impl.addressIndexed = true;
		}
		auto it = std::upper_bound(impl.addressIndex.begin(), impl.addressIndex.end(), address,
			[](const zaddr a, const Impl::AddressEntry& e) { return a < e.dest; });
		if (it == impl.addressIndex.begin())
		{
			return 0;
		}
		--it;
		if (address - it->dest >= it->size)
		{
			return 0;
		}
		Impl::Located at;
		if (!impl.locate(it->uid, at))
		{
			return 0;		// removed since
		}
		if (offset)
		{
			*offset = address - it->dest;
		}
		return it->uid;
	}

	ZoneDataId ZoneEditor::header_data(const zaddr pooledHeader) const
	{
		if (!loaded())
		{
			return 0;
		}
		const auto it = impl_->headerChunk.find(pooledHeader);
		if (it == impl_->headerChunk.end() || (it->second.first != impl_->root.get() && impl_->index_of(it->second.first) == npos))
		{
			return 0;
		}
		return impl_->uid_of(it->second.first, it->second.second);
	}

	ZoneDataId ZoneEditor::asset_data(const std::size_t asset, const std::uint32_t index) const
	{
		if (!loaded() || asset >= impl_->assets.size())
		{
			return 0;
		}
		return impl_->uid_of(impl_->assets[asset].get(), index);
	}

	bool ZoneEditor::data_info(const ZoneDataId id, ZoneDataInfo& info) const
	{
		info = {};
		Impl::Located at;
		if (!loaded() || !impl_->locate(id, at))
		{
			return false;
		}
		const Chunk& c = at.unit->chunks[at.index];
		info.id = id;
		info.asset = at.asset;
		info.index = at.index;
		info.size = c.size;
		info.address = c.foreign ? 0 : c.dest;
		info.string = c.string;
		const auto& relocs = at.unit->relocs;
		for (auto it = std::lower_bound(relocs.begin(), relocs.end(), at.index, [](const Reloc& r, const std::uint32_t chunk) { return r.chunk < chunk; });
			it != relocs.end() && it->chunk == at.index; ++it)
		{
			info.pointers++;
		}
		return true;
	}

	std::span<const std::uint8_t> ZoneEditor::data_bytes(const ZoneDataId id) const
	{
		Impl::Located at;
		if (!loaded() || !impl_->locate(id, at))
		{
			return {};
		}
		const Chunk& c = at.unit->chunks[at.index];
		return { at.unit->data(c), c.size };
	}

	std::vector<ZonePointer> ZoneEditor::data_pointers(const ZoneDataId id) const
	{
		std::vector<ZonePointer> out;
		Impl::Located at;
		if (!loaded() || !impl_->locate(id, at))
		{
			return out;
		}
		const auto& relocs = at.unit->relocs;
		auto it = std::lower_bound(relocs.begin(), relocs.end(), at.index, [](const Reloc& r, const std::uint32_t chunk) { return r.chunk < chunk; });
		for (; it != relocs.end() && it->chunk == at.index; ++it)
		{
			out.push_back(ZonePointer{ it->offset, impl_->target_of(*it) });
		}
		return out;
	}

	ZoneTarget ZoneEditor::pointer_at(const ZoneDataId id, const std::uint32_t offset) const
	{
		Impl::Located at;
		if (loaded() && impl_->locate(id, at))
		{
			const auto& relocs = at.unit->relocs;
			const auto it = std::lower_bound(relocs.begin(), relocs.end(), std::make_pair(at.index, offset),
				[](const Reloc& r, const std::pair<std::uint32_t, std::uint32_t>& key) { return r.chunk != key.first ? r.chunk < key.first : r.offset < key.second; });
			if (it != relocs.end() && it->chunk == at.index && it->offset == offset)
			{
				return impl_->target_of(*it);
			}
		}
		return {};
	}

	std::vector<ZoneInbound> ZoneEditor::pointers_to(const ZoneDataId id) const
	{
		std::vector<ZoneInbound> out;
		Impl::Located at;
		if (!loaded() || !impl_->locate(id, at))
		{
			return out;
		}
		for_each_unit(impl_->root, impl_->assets, [&](const Unit& other, std::size_t)
		{
			for (const Reloc& reloc : other.relocs)
			{
				if (reloc.unit == at.unit && reloc.kind == TargetKind::CHUNK && reloc.index == at.index)
				{
					out.push_back(ZoneInbound{ other.chunks[reloc.chunk].uid, reloc.offset, reloc.targetOffset });
				}
			}
		});
		return out;
	}

	namespace
	{
		// the relocations of chunk `index` of `unit` are replaced by `fresh` ( sorted by offset )
		void replace_relocs(Unit& unit, const std::uint32_t index, std::vector<Reloc> fresh)
		{
			auto& relocs = unit.relocs;
			const auto first = std::lower_bound(relocs.begin(), relocs.end(), index, [](const Reloc& r, const std::uint32_t chunk) { return r.chunk < chunk; });
			auto last = first;
			while (last != relocs.end() && last->chunk == index)
			{
				++last;
			}
			const auto at = relocs.erase(first, last);
			relocs.insert(at, fresh.begin(), fresh.end());
		}
	}

	Status ZoneEditor::set_data(const ZoneDataId id, const std::span<const std::uint8_t> bytes, const std::span<const ZonePointer> pointers)
	try
	{
		Impl::Located at;
		if (!loaded() || !impl_->locate(id, at))
		{
			return Status::error("no such data");
		}
		Unit& unit = *at.unit;
		const Chunk& chunk = unit.chunks[at.index];
		if (bytes.empty() || bytes.size() > 0xFFFFFFF0u)
		{
			return Status::error("data cannot be empty ( remove it instead )");
		}
		if (chunk.string && (bytes.back() != 0 || std::find(bytes.begin(), bytes.end() - 1, std::uint8_t{ 0 }) != bytes.end() - 1))
		{
			return Status::error("a string has to end with its terminator and hold no other");
		}
		for (const ScriptRef& ref : unit.scriptRefs)
		{
			if (ref.chunk == at.index)
			{
				return Status::error("the data holds script string indices and cannot be replaced");
			}
		}
		const std::uint32_t size = static_cast<std::uint32_t>(bytes.size());
		std::string problem;
		for_each_unit(impl_->root, impl_->assets, [&](const Unit& other, const std::size_t k)
		{
			for (const Reloc& reloc : other.relocs)
			{
				if (problem.empty() && reloc.unit == &unit && reloc.kind == TargetKind::CHUNK && reloc.index == at.index && reloc.targetOffset > size
					&& !(&other == &unit && reloc.chunk == at.index))
				{
					problem = describe(other, k) + " points past the new end of the data";
				}
			}
		});
		if (!problem.empty())
		{
			return Status::error(problem);
		}
		std::vector<Reloc> fresh;
		fresh.reserve(pointers.size());
		for (const ZonePointer& p : pointers)
		{
			if (p.offset > size || size - p.offset < 4 || (!fresh.empty() && p.offset < fresh.back().offset + 4))
			{
				return Status::error("the pointer fields have to lie inside the data, in ascending order");
			}
			Reloc reloc;
			reloc.chunk = at.index;
			reloc.offset = p.offset;
			if (const Status s = impl_->make_reloc(p.target, reloc); !s)
			{
				return s;
			}
			if (reloc.kind == TargetKind::CHUNK && reloc.unit == &unit && reloc.index == at.index && reloc.targetOffset > size)
			{
				return Status::error("a pointer leads past the end of its target");
			}
			fresh.push_back(reloc);
		}
		replace_relocs(unit, at.index, std::move(fresh));
		unit.set_data(at.index, bytes);
		unit.modified = true;
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::patch_data(const ZoneDataId id, const std::uint32_t offset, const std::span<const std::uint8_t> bytes)
	try
	{
		Impl::Located at;
		if (!loaded() || !impl_->locate(id, at))
		{
			return Status::error("no such data");
		}
		Unit& unit = *at.unit;
		const Chunk& chunk = unit.chunks[at.index];
		if (offset > chunk.size || bytes.size() > chunk.size - offset)
		{
			return Status::error("the patch does not lie inside the data");
		}
		for (const Reloc& reloc : unit.relocs)
		{
			if (reloc.chunk == at.index && reloc.offset < offset + bytes.size() && offset < reloc.offset + 4)
			{
				return Status::error("the patch touches a zone pointer field ( use set_pointer )");
			}
		}
		if (!bytes.empty())
		{
			std::memcpy(unit.make_local(at.index) + offset, bytes.data(), bytes.size());
			unit.modified = true;
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::set_pointer(const ZoneDataId id, const std::uint32_t offset, const ZoneTarget& target, const std::uint32_t plain)
	try
	{
		Impl::Located at;
		if (!loaded() || !impl_->locate(id, at))
		{
			return Status::error("no such data");
		}
		Unit& unit = *at.unit;
		const Chunk& chunk = unit.chunks[at.index];
		if (offset > chunk.size || chunk.size - offset < 4)
		{
			return Status::error("the pointer field does not lie inside the data");
		}
		Reloc fresh;
		fresh.chunk = at.index;
		fresh.offset = offset;
		if (target.kind != ZoneTarget::Kind::none)
		{
			if (const Status s = impl_->make_reloc(target, fresh); !s)
			{
				return s;
			}
		}
		auto& relocs = unit.relocs;
		auto it = std::lower_bound(relocs.begin(), relocs.end(), fresh,
			[](const Reloc& a, const Reloc& b) { return a.chunk != b.chunk ? a.chunk < b.chunk : a.offset < b.offset; });
		for (auto k = it; k != relocs.end() && k->chunk == at.index && k->offset < offset + 4; ++k)
		{
			if (k->offset != offset)
			{
				return Status::error("the field overlaps another zone pointer field");
			}
		}
		if (it != relocs.begin() && std::prev(it)->chunk == at.index && std::prev(it)->offset + 4 > offset)
		{
			return Status::error("the field overlaps another zone pointer field");
		}
		const bool exists = it != relocs.end() && it->chunk == at.index && it->offset == offset;
		if (target.kind == ZoneTarget::Kind::none)
		{
			if (exists)
			{
				relocs.erase(it);
			}
			put_u32(unit.make_local(at.index) + offset, plain);
		}
		else
		{
			if (exists)
			{
				*it = fresh;
			}
			else
			{
				relocs.insert(it, fresh);
			}
			put_u32(unit.make_local(at.index) + offset, fresh.raw);
		}
		unit.modified = true;
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::insert_data(const ZoneDataId anchor, const bool after, const std::span<const std::uint8_t> bytes,
		const std::span<const ZonePointer> pointers, ZoneDataId& created, const bool string)
	try
	{
		created = 0;
		Impl::Located at;
		if (!loaded() || !impl_->locate(anchor, at))
		{
			return Status::error("no such data");
		}
		if (at.asset == npos)
		{
			return Status::error("data cannot be added to the XAssetList");
		}
		if (bytes.empty() || (string && bytes.back() != 0))
		{
			return Status::error("data cannot be empty, a string has to end with its terminator");
		}
		Unit& unit = *at.unit;
		const std::uint32_t index = at.index + (after ? 1u : 0u);
		Chunk c;
		c.string = string;
		c.uid = impl_->nextUid++;
		unit.chunks.insert(unit.chunks.begin() + index, c);
		unit.set_data(index, bytes);
		impl_->renumber(unit, index, true);
		impl_->idsTotal++;
		unit.modified = true;
		created = c.uid;
		if (!pointers.empty())
		{
			if (const Status s = set_data(created, bytes, pointers); !s)
			{
				unit.chunks.erase(unit.chunks.begin() + index);
				impl_->renumber(unit, index, false);
				impl_->idsTotal--;
				created = 0;
				return s;
			}
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::remove_data(const ZoneDataId id)
	try
	{
		Impl::Located at;
		if (!loaded() || !impl_->locate(id, at))
		{
			return Status::error("no such data");
		}
		if (at.asset == npos)
		{
			return Status::error("data of the XAssetList cannot be removed");
		}
		Unit& unit = *at.unit;
		std::string problem;
		for_each_unit(impl_->root, impl_->assets, [&](const Unit& other, const std::size_t k)
		{
			for (const Reloc& reloc : other.relocs)
			{
				if (problem.empty() && reloc.unit == &unit && reloc.kind == TargetKind::CHUNK && reloc.index == at.index
					&& !(&other == &unit && reloc.chunk == at.index))
				{
					problem = describe(other, k) + " still points into the data";
				}
			}
		});
		if (!problem.empty())
		{
			return Status::error(problem);
		}
		for (const ScriptRef& ref : unit.scriptRefs)
		{
			if (ref.chunk == at.index)
			{
				return Status::error("the data holds script string indices and cannot be removed");
			}
		}
		for (const auto& [pooled, where] : impl_->headerChunk)
		{
			if (where.first == &unit && where.second == at.index)
			{
				return Status::error("an asset header cannot be removed");
			}
		}
		replace_relocs(unit, at.index, {});
		unit.chunks.erase(unit.chunks.begin() + at.index);
		impl_->renumber(unit, at.index, false);
		impl_->idsTotal--;
		unit.modified = true;
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status ZoneEditor::remove_data_range(const ZoneDataId first, const ZoneDataId last)
	try
	{
		Impl::Located from, to;
		if (!loaded() || !impl_->locate(first, from) || !impl_->locate(last, to))
		{
			return Status::error("no such data");
		}
		if (from.asset == npos || from.unit != to.unit || from.index > to.index)
		{
			return Status::error("a range has to lie inside one asset, first piece first");
		}
		Unit& unit = *from.unit;
		const std::uint32_t a = from.index, b = to.index, count = b - a + 1;
		const auto inside = [&](const Unit& holder, const std::uint32_t chunk) { return &holder == &unit && chunk >= a && chunk <= b; };

		// allocations: one that only pointers of the range lead to goes with it, any other one has to be outside
		auto& keys = unit.allocOrdinals;
		std::vector<std::uint8_t> used_inside(keys.size(), 0), used_outside(keys.size(), 0);
		std::string problem;
		for_each_unit(impl_->root, impl_->assets, [&](const Unit& other, const std::size_t k)
		{
			for (const Reloc& reloc : other.relocs)
			{
				if (reloc.unit != &unit || reloc.kind == TargetKind::RAW || reloc.kind == TargetKind::SLOT)
				{
					continue;
				}
				const bool in_range = inside(other, reloc.chunk);
				if (reloc.kind == TargetKind::ALLOC)
				{
					(in_range ? used_inside : used_outside)[reloc.index] = 1;
				}
				else if (!in_range && reloc.index >= a && reloc.index <= b && problem.empty())
				{
					problem = describe(other, k) + " points into the data";
				}
			}
		});
		if (!problem.empty())
		{
			return Status::error(problem);
		}
		std::vector<std::uint32_t> remap(keys.size(), 0);
		std::vector<AllocKey> kept;
		for (std::size_t k = 0; k < keys.size(); k++)
		{
			const bool boundary = keys[k].anchor == a || keys[k].anchor == b + 1;
			const bool within = keys[k].anchor > a && keys[k].anchor <= b;
			if (used_inside[k] && !used_outside[k] && (within || boundary))
			{
				remap[k] = NONE;
				continue;
			}
			if (within || (boundary && used_outside[k]))
			{
				return Status::error("a loader allocation at the range is used by data outside of it");
			}
			if (used_inside[k] && !within && !boundary)
			{
				// pointers of the range to an allocation elsewhere go away with the range; the allocation stays
			}
			remap[k] = static_cast<std::uint32_t>(kept.size());
			AllocKey key = keys[k];
			if (key.anchor > b)
			{
				key.anchor -= count;
			}
			kept.push_back(key);
		}

		// from here on nothing fails
		for (auto it = impl_->headerChunk.begin(); it != impl_->headerChunk.end();)
		{
			it = it->second.first == &unit && it->second.second >= a && it->second.second <= b ? impl_->headerChunk.erase(it) : std::next(it);
		}
		std::erase_if(unit.relocs, [&](const Reloc& r) { return r.chunk >= a && r.chunk <= b; });
		std::erase_if(unit.scriptRefs, [&](const ScriptRef& r) { return r.chunk >= a && r.chunk <= b; });
		unit.chunks.erase(unit.chunks.begin() + a, unit.chunks.begin() + b + 1);
		const auto shift = [&](std::uint32_t& index)
		{
			if (index > b)
			{
				index -= count;
			}
		};
		for (Reloc& reloc : unit.relocs)
		{
			shift(reloc.chunk);
		}
		for (ScriptRef& ref : unit.scriptRefs)
		{
			shift(ref.chunk);
		}
		for_each_unit(impl_->root, impl_->assets, [&](Unit& other, std::size_t)
		{
			for (Reloc& reloc : other.relocs)
			{
				if (reloc.unit != &unit)
				{
					continue;
				}
				if (reloc.kind == TargetKind::CHUNK)
				{
					shift(reloc.index);
				}
				else if (reloc.kind == TargetKind::ALLOC)
				{
					reloc.index = remap[reloc.index];
				}
			}
		});
		for (auto& [pooled, where] : impl_->headerChunk)
		{
			if (where.first == &unit)
			{
				shift(where.second);
			}
		}
		keys = std::move(kept);
		impl_->hints.clear();
		impl_->idsTotal -= count;
		unit.modified = true;
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	// ---- output -----------------------------------------------------------------------------------

	Status ZoneEditor::write(std::vector<std::uint8_t>& zoneBytes, ZoneWriteReport* report) const
	try
	{
		zoneBytes.clear();
		if (!loaded())
		{
			return Status::error("no zone loaded");
		}
		const std::size_t count = impl_->assets.size();

		// the root as it is written: asset count and XAsset table follow the asset list
		Unit root = *impl_->root;
		std::uint8_t* list = root.make_local(0);
		put_u32(list + 8, static_cast<std::uint32_t>(count));
		put_u32(list + 12, count ? INLINE : 0);
		std::vector<std::uint8_t> table(XASSET_SIZE * count);
		for (std::size_t k = 0; k < count; k++)
		{
			put_u32(table.data() + XASSET_SIZE * k, static_cast<std::uint32_t>(impl_->assets[k]->type));
			put_u32(table.data() + XASSET_SIZE * k + XASSET_HEADER_OFFSET, impl_->assets[k]->rawHeader);
		}
		std::uint32_t tableChunk = impl_->tableChunk;
		if (tableChunk != NONE && !count)
		{
			root.chunks.erase(root.chunks.begin() + tableChunk);
			tableChunk = NONE;
		}
		else if (tableChunk != NONE)
		{
			root.set_data(tableChunk, table);
		}
		else if (count)
		{
			tableChunk = root.add_chunk(table, false);
		}

		Zone::Impl image;
		image.name = impl_->name;
		Writer writer(image.mem);
		writer.tableChunk = tableChunk;
		writer.order.push_back(&root);
		writer.indexOf.emplace(impl_->root.get(), 0u);
		std::uint64_t total = XFILE_HEADER_SIZE + root.stream_bytes();
		for (const auto& unit : impl_->assets)
		{
			writer.indexOf.emplace(unit.get(), static_cast<std::uint32_t>(writer.order.size()));
			writer.order.push_back(unit.get());
			total += unit->stream_bytes();
		}
		if (total > 0xFFFFFFF0ull)
		{
			return Status::error("the zone would exceed 4 GiB");
		}
		for (const Unit* unit : writer.order)
		{
			writer.newDest.emplace_back(unit->chunks.size(), 0u);
			writer.allocNew.emplace_back(unit->allocOrdinals.size(), 0u);
		}
		writer.out.reserve(static_cast<std::size_t>(total));
		writer.out.resize(XFILE_HEADER_SIZE);

		run_zone_loaders(image, {}, &writer);
		writer.finish();

		std::vector<std::uint8_t>& out = writer.out;
		XFileHeader header;
		header.size = static_cast<std::uint32_t>(out.size() - XFILE_HEADER_SIZE);
		header.externalSize = impl_->hasBase ? impl_->baseHeader.externalSize : 0;
		for (int b = 0; b < BLOCK_COUNT; b++)
		{
			const std::uint32_t used = image.blocks[b].used;
			if (impl_->hasBase)
			{
				// what the source zone declared on top of what its loaders reach is kept
				header.blockSize[b] = impl_->baseHeader.blockSize[b] + used - impl_->baseUsed[b];
			}
			else
			{
				// the shipped zones declare 16 bytes more temp memory than their loaders reach ( ZoneWriter.cs )
				header.blockSize[b] = !used ? 0 : b == 0 ? used + 0x10 : used;
			}
			if (report)
			{
				report->blockUsed[b] = used;
			}
		}
		put_u32(out.data(), header.size);
		put_u32(out.data() + 4, header.externalSize);
		for (int b = 0; b < BLOCK_COUNT; b++)
		{
			put_u32(out.data() + 8 + 4 * static_cast<std::size_t>(b), header.blockSize[b]);
		}
		if (count)
		{
			if (image.entries.size() != count || !writer.tableOut)
			{
				return Status::error("the written XAsset table does not match the asset list");
			}
			for (std::size_t k = 0; k < count; k++)
			{
				put_u32(out.data() + writer.tableOut + XASSET_SIZE * k + 4, image.entries[k].dataOffset);
				put_u32(out.data() + writer.tableOut + XASSET_SIZE * k + 12, image.entries[k].dataSize);
			}
		}
		if (report)
		{
			report->header = header;
			report->streamSize = out.size();
			report->chunks = writer.chunkCount;
			report->relocations = writer.relocCount;
			report->forwardPatches = writer.forward;
		}
		zoneBytes = std::move(out);
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}
}
