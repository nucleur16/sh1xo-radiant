#pragma once

// A loaded Call of Duty Online 1.1.1.13 zone.
//
// Zone::load() runs the game's own zone loaders ( Load_XAsset 0x004C8ED0 and the 261 functions below it, machine
// translated from the client: src/loaders_1_1_1_13.inc ) over the inflated zone, against a private 32 bit address
// space. The result is the *image*: the eight memory blocks exactly as the game builds them, plus a pool that holds
// every asset header. All pointers inside the image are 32 bit image addresses ( zaddr ), never native pointers,
// so the code is the same in a 32 and a 64 bit build.
//
//   address space of an image
//     0x1000'0000 + n * 0x1000'0000   memory block n ( 0 .. 7 ). Block 0 is the loader's temp block: it is reused
//                                     for every asset, so nothing may point into it after the load.
//     0x9001'0000 ..                  pool: asset headers ( DB_AddXAsset ), reference placeholders
//                                     ( DB_FindXAssetHeader ), preserved GfxImageLoadDefs
//
//   a zone reference in the *file* ( c1::zptr ) is ( block << 28 | offset ) + 1; the loaders turn each of them into
//   an image address, Zone::alias_to_address() does the same for a raw value.
//
// A Zone is immutable after load() and holds no global state: any number of zones can be loaded side by side and
// read from several threads.

#include "codol_zone/c1_layouts.hpp"
#include "codol_zone/fastfile.hpp"

#include <cstring>
#include <string_view>
#include <type_traits>

namespace codol::zone
{
	using zaddr = std::uint32_t;		// address inside a loaded image; 0 is null

	inline constexpr int BLOCK_COUNT = 8;
	inline constexpr zaddr block_base(const int block) { return static_cast<zaddr>(block + 1) << 28; }

	struct XFileHeader
	{
		std::uint32_t size = 0;					// zone bytes after this 40 byte header
		std::uint32_t externalSize = 0;
		std::uint32_t blockSize[BLOCK_COUNT] = {};
	};

	// one row of the zone's XAsset table ( 16 bytes in 1.1.1.13 )
	struct XAssetEntry
	{
		std::int32_t type = 0;					// c1::XAssetType
		std::uint32_t dataOffset = 0;			// start of the asset's data in the zone stream, relative to the XFile header end
		std::uint32_t dataSize = 0;				// bytes of zone stream the asset occupies
		zaddr header = 0;						// loaded asset header ( pool ), 0 when the type has no loader
	};

	// every asset the zone registers, top level and nested ( a material inside an xmodel, an image inside a material .. )
	struct AssetRecord
	{
		std::int32_t type = 0;
		zaddr header = 0;						// pool address of the header
		std::int32_t owner = -1;				// index of the top level XAssetEntry that was being loaded
	};

	// an asset that is referenced by name only: its data is in another zone ( names start with ',' in the file )
	struct AssetReference
	{
		std::int32_t type = 0;
		std::string name;
	};

	struct BlockUsage
	{
		std::uint32_t declared = 0;
		std::uint32_t used = 0;
	};

	class Zone
	{
	public:
		Zone();
		~Zone();
		Zone(Zone&&) noexcept;
		Zone& operator=(Zone&&) noexcept;
		Zone(const Zone&) = delete;
		Zone& operator=(const Zone&) = delete;

		// loads an inflated zone ( XFileHeader + data, as decode_fastfile returns it ). The bytes are consumed.
		Status load(std::vector<std::uint8_t> zoneBytes, std::string name);

		// read_fastfile + load
		Status open(const std::filesystem::path& fastfile, const ClientKey& key, FastFileInfo* info = nullptr);

		bool loaded() const;
		const std::string& name() const;
		const XFileHeader& header() const;
		std::uint64_t stream_size() const;					// inflated zone size including the XFile header
		BlockUsage block_usage(int block) const;
		const std::vector<std::string>& warnings() const;

		// ---- script strings -----------------------------------------------------------------
		// in zone order. u16 script string fields of loaded assets ( bone names, notetracks .. ) hold *handles*.
		const std::vector<std::string>& script_strings() const;
		std::string_view script_string(std::uint32_t handle) const;	// empty for 0 / unknown

		// ---- assets -------------------------------------------------------------------------
		std::span<const XAssetEntry> assets() const;			// the XAsset table, in file order
		std::span<const AssetRecord> all_assets() const;		// every registered asset, in load order

		// name of an asset header ( without the leading ',' of a reference ); empty when the type has no name
		std::string_view asset_name(std::int32_t type, zaddr header) const;

		// first asset of that type and name that has its data in this zone; 0 when there is none
		zaddr find_asset(std::int32_t type, std::string_view name) const;

		// non null when `address` is the placeholder of an asset that lives in another zone
		const AssetReference* reference(zaddr address) const;

		// name of whatever an asset pointer field points to: a header in this zone or a by-name reference
		std::string_view pointee_name(std::int32_t type, zaddr address) const;

		// ---- image memory -------------------------------------------------------------------
		// `size` bytes at `address`; empty when the range is not inside the image ( or size is 0 )
		std::span<const std::uint8_t> bytes(zaddr address, std::size_t size) const;

		// zero terminated string; empty for null / unmapped
		std::string_view c_string(zaddr address) const;

		// copies a packed layout struct out of the image; false ( and a zeroed value ) when it is not mapped
		template <typename T>
		bool get(const zaddr address, T& value) const
		{
			static_assert(std::is_trivially_copyable_v<T>);
			const auto data = bytes(address, sizeof(T));
			if (data.size() != sizeof(T))
			{
				std::memset(&value, 0, sizeof(T));
				return false;
			}
			std::memcpy(&value, data.data(), sizeof(T));
			return true;
		}

		template <typename T>
		T get(const zaddr address) const
		{
			T value;
			get(address, value);
			return value;
		}

		// count elements of T at address, copied; empty when not mapped
		template <typename T>
		std::vector<T> get_array(const zaddr address, const std::size_t count) const
		{
			static_assert(std::is_trivially_copyable_v<T>);
			std::vector<T> out;
			const auto data = bytes(address, count * sizeof(T));
			if (count && data.size() == count * sizeof(T))
			{
				out.resize(count);
				std::memcpy(out.data(), data.data(), data.size());
			}
			return out;
		}

		// ( block << 28 | offset ) + 1  ->  image address
		static constexpr zaddr alias_to_address(const c1::zptr alias)
		{
			return block_base(static_cast<int>(c1::zptr_block(alias))) + c1::zptr_offset(alias);
		}

		// 0 .. 7 for a block address, -1 otherwise ( pool, null )
		static constexpr int block_of(const zaddr address)
		{
			const int b = static_cast<int>(address >> 28) - 1;
			return b >= 0 && b < BLOCK_COUNT ? b : -1;
		}

		struct Impl;

	private:
		std::unique_ptr<Impl> impl_;
	};

	// name of an asset type ( "xmodel", "material" .. ), "type_N" outside the table
	std::string asset_type_name(std::int32_t type);

	// asset type for a name as printed by asset_type_name; -1 when unknown
	std::int32_t asset_type_from_name(std::string_view name);
}
