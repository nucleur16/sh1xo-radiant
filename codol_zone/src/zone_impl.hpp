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

	inline constexpr std::uint32_t G_STREAM_POS = 0x01EABC3Cu;		// g_streamPos of the client ( GameBuild.cs, V1_1_1_13 )
	inline constexpr std::uint32_t G_XASSET_LIST = 0x017203ECu;		// where the driver keeps the XAssetList

	// Observer of a load ( zone_editor.cpp ). Two uses:
	//   capture  the zone bytes are read as usual; the tracer is told where every stream read lands
	//   write    supplies_stream(): there are no zone bytes, the tracer hands out what the loaders ask for next and so
	//            produces the stream ( the same idea as CODOL-AssetExplorer Porting/ZoneWriter.cs )
	// Any member may throw LoadError.
	struct LoadTracer : MemWatch
	{
		virtual void attach(VMem& /*memory*/) {}		// the address space the loaders are about to run in
		virtual bool supplies_stream() const { return false; }
		virtual const std::uint8_t* supply(std::uint32_t /*destination*/, std::uint32_t /*size*/) { return nullptr; }
		virtual const std::uint8_t* supply_string(std::uint32_t /*destination*/, std::uint32_t& /*sizeWithTerminator*/) { return nullptr; }
		virtual void on_stream_read(std::uint32_t /*destination*/, std::uint32_t /*size*/, std::uint64_t /*streamOffset*/, bool /*string*/) {}
		virtual void on_asset_begin(std::int32_t /*index*/, std::uint64_t /*streamOffset*/) {}
		virtual void on_asset_end(std::int32_t /*index*/, std::uint64_t /*streamOffset*/) {}
		virtual void on_script_strings(std::uint32_t /*array*/, std::uint32_t /*count*/) {}	// the list is loaded: handles from here on
		virtual void on_add_asset(std::int32_t /*type*/, std::uint32_t /*loadedHeader*/, std::uint32_t /*pooledHeader*/) {}
		void on_write32(std::uint32_t, std::uint32_t) override {}
		void on_write16(std::uint32_t, std::uint16_t) override {}
		void on_window_read(std::uint32_t) override {}
	};

	// runs the translated loaders over zone bytes; throws LoadError. With a tracer that supplies the stream, `bytes`
	// is ignored and zone.streamSize / zone.blocks[].used / zone.entries describe what was written.
	void run_zone_loaders(Zone::Impl& zone, const std::vector<std::uint8_t>& bytes, LoadTracer* tracer = nullptr);
}
