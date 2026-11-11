#pragma once

// Writing Call of Duty Online 1.1.1.13 zones.
//
// A ZoneEditor loads a zone the way Zone does, and additionally records how the game's loaders consumed the zone
// stream: every read ( "chunk": where it landed in which memory block, how long it is, whether it is a string ), and
// every zone pointer the loaders converted ( "relocation": which field of which chunk, and what it points at ). That
// gives an asset graph that is independent of addresses:
//
//   unit        the XAssetList root ( script strings, XAsset table ) or one top level asset with everything the
//               loaders read for it, nested assets included
//   chunk       a run of stream bytes, kept exactly as the stream has them
//   relocation  a field holding ( block << 28 | offset ) + 1, stored as its target: a chunk + offset, an
//               allocation the loader made without reading the stream ( DB_InsertPointer slots, the runtime block ),
//               or the header slot of a top level asset in the XAsset table
//
// write() runs the same loaders again, this time feeding them the chunks in order ( the approach of
// CODOL-AssetExplorer Porting/ZoneWriter.cs ): the loaders decide block, alignment and order of everything exactly
// as the game will when it loads the result, and every relocation is encoded for where its target ended up. The
// XFile header ( size, block sizes ) and the XAsset table ( stream offset and size per asset ) are recomputed.
// An unedited zone therefore comes out byte identical, and after an edit that changes sizes all offsets follow.
//
// What the editor does not know is the meaning of the non pointer bytes of a chunk: except for the asset types with
// an explicit edit below ( MapEnts, RawFile, StringTable ) an asset can be kept, removed, or taken whole from another
// zone, not modified field by field.

#include "codol_zone/zone.hpp"

namespace codol::zone
{
	struct EditorAsset
	{
		std::int32_t type = 0;
		std::string name;
		std::uint32_t chunks = 0;				// stream reads of this asset
		std::uint32_t relocations = 0;
		std::uint64_t streamBytes = 0;
		bool modified = false;					// edited, added or taken from another zone
		bool usesScriptStrings = false;			// holds indices into the zone's script string list
	};

	struct ZoneCaptureStats
	{
		std::uint64_t chunks = 0;
		std::uint64_t relocations = 0;			// all converted zone pointers
		std::uint64_t toChunk = 0;				// .. into stream data of the same unit
		std::uint64_t toOtherUnit = 0;			// .. into stream data of another unit ( shared strings and the like )
		std::uint64_t toAllocation = 0;			// .. to memory the loader allocated without reading the stream
		std::uint64_t toAssetSlot = 0;			// .. to the header slot of a top level asset in the XAsset table
		std::uint64_t unresolved = 0;			// .. to nothing the loaders placed: written back unchanged
	};

	struct ZoneWriteReport
	{
		XFileHeader header;
		std::uint64_t streamSize = 0;			// including the XFile header
		std::uint32_t blockUsed[BLOCK_COUNT] = {};
		std::uint64_t chunks = 0;
		std::uint64_t relocations = 0;
		std::uint64_t forwardPatches = 0;		// relocations whose target was laid out after the field
	};

	struct StringTableData
	{
		int columns = 0;
		int rows = 0;
		std::vector<std::string> cells;			// rows * columns, row major
	};

	class ZoneEditor
	{
	public:
		static constexpr std::size_t npos = static_cast<std::size_t>(-1);

		ZoneEditor();
		~ZoneEditor();
		ZoneEditor(ZoneEditor&&) noexcept;
		ZoneEditor& operator=(ZoneEditor&&) noexcept;
		ZoneEditor(const ZoneEditor&) = delete;
		ZoneEditor& operator=(const ZoneEditor&) = delete;

		// loads an inflated zone ( XFileHeader + data ) and captures its asset graph. The bytes are kept.
		Status load(std::vector<std::uint8_t> zoneBytes, std::string name);

		// read_fastfile + load
		Status open(const std::filesystem::path& fastfile, const ClientKey& key, FastFileInfo* info = nullptr);

		// a zone without script strings and assets, to be filled with set_rawfile / set_string_table / add_asset
		Status create(std::string name);

		bool loaded() const;
		const std::string& name() const;

		// the zone as it was loaded ( not updated by edits ); not loaded after create()
		const Zone& zone() const;
		const ZoneCaptureStats& capture_stats() const;

		// ---- the asset list ( top level assets, in file order ) ---------------------------------
		std::size_t asset_count() const;
		EditorAsset asset(std::size_t index) const;
		std::size_t find_asset(std::int32_t type, std::string_view name) const;		// npos when absent

		// Hash of everything the zone stream holds for an asset, independent of where the loaders put it: the bytes
		// of every chunk, and for every zone pointer what it points at ( chunk and offset, named asset ) instead of
		// its value. Two captures of the same asset - before and after other assets of the zone changed size, were
		// added or removed - give the same fingerprint.
		std::uint64_t asset_fingerprint(std::size_t index) const;

		// ---- edits --------------------------------------------------------------------------
		// Replaces the entity string of the zone's MapEnts ( top level or nested in another asset ). The terminator
		// is added; MapEnts.numEntityChars follows.
		Status set_map_ents(std::string_view entityString);

		// Replaces the contents of the rawfile `name`, or appends a new rawfile asset when the zone has none of that
		// name. compress: store it deflated ( as the shipped zones store scripts ) instead of as plain bytes.
		Status set_rawfile(std::string_view name, std::span<const std::uint8_t> contents, bool compress = true);

		// Replaces or appends a string table. Cell hashes are computed the way the shipped tables have them.
		Status set_string_table(std::string_view name, const StringTableData& table);

		// Removes a top level asset. Fails when another asset of the zone points into it.
		Status remove_asset(std::size_t index);

		// Copies a top level asset of another zone in front of `position` ( npos: at the end ). Pointers of the asset
		// that lead to other assets of the donor zone are re-targeted at the asset of the same type and name in this
		// zone; withDependencies also copies those that are missing here. Shared strings are re-targeted at an equal
		// string of this zone. Fails ( and changes nothing ) when a pointer cannot be re-targeted, or when the
		// asset uses script strings and the two zones do not have the same script string list.
		Status add_asset(const ZoneEditor& donor, std::size_t donorIndex, std::size_t position = npos, bool withDependencies = false);

		// Replaces a top level asset with one of another zone ( same type ); pointers of other assets to the
		// replaced asset's header keep working. Same rules as add_asset.
		Status replace_asset(std::size_t index, const ZoneEditor& donor, std::size_t donorIndex);

		// ---- output -------------------------------------------------------------------------
		// Lays the graph out again and returns the inflated zone ( XFileHeader + data ), ready for encode_fastfile.
		Status write(std::vector<std::uint8_t>& zoneBytes, ZoneWriteReport* report = nullptr) const;

		struct Impl;

	private:
		std::unique_ptr<Impl> impl_;
	};

	// the hash stored next to every string table cell
	std::int32_t string_table_hash(std::string_view text);
}
