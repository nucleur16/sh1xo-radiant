#pragma once

#include "codol_zone/zone.hpp"
#include "vmem.hpp"

#include <unordered_map>

namespace codol::zone
{
	// address space layout shared by the loader and the accessors ( see zone.hpp )
	inline constexpr std::uint32_t XBLOCK_ARRAY = 0x90000000u;	// { base, size } per block, read by DB_ConvertOffsetToPointer
	inline constexpr std::uint32_t POOL_BASE = 0x90010000u;
	inline constexpr std::uint32_t POOL_LIMIT = 0x9F000000u;
	inline constexpr std::uint32_t ENTRY_BASE = 0xA0000000u;
	inline constexpr std::uint32_t ENTRY_LIMIT = 0xAF000000u;

	struct Zone::Impl
	{
		VMem mem;
		bool loaded = false;
		std::string name;
		XFileHeader header;
		std::uint64_t streamSize = 0;
		BlockUsage blocks[BLOCK_COUNT];
		std::vector<std::string> warnings;

		std::vector<std::string> scriptStrings;				// zone order
		std::vector<std::string> scriptStringHandles;		// handle -> string, [0] is the empty string

		std::vector<XAssetEntry> entries;
		std::vector<AssetRecord> records;
		std::unordered_map<zaddr, AssetReference> references;
		std::unordered_map<std::string, zaddr> byName;		// "<type>:<name>" -> header
	};

	// runs the translated loaders over zone bytes; throws LoadError
	void run_zone_loaders(Zone::Impl& zone, const std::vector<std::uint8_t>& bytes);
}
