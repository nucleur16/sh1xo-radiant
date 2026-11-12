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
// zone, not modified field by field - unless the caller knows the layout: the typed editing functions ( data_at,
// set_data, patch_data, set_pointer, insert_data .. ) change any piece of data and keep every pointer valid.

#include "codol_zone/zone.hpp"

#include <type_traits>

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

	// ---- typed, relocation aware editing ---------------------------------------------------------
	// A piece of stream data of the asset graph ( one read of the loaders: a struct, an array, a string ). The id
	// stays valid across edits of the editor it came from; 0 is "none".
	using ZoneDataId = std::uint32_t;

	// What a pointer field of the asset graph points at.
	struct ZoneTarget
	{
		enum class Kind : std::uint8_t
		{
			none,			// not a zone pointer: the field is plain bytes ( 0, 0xFFFFFFFF "data follows", a number )
			data,			// `offset` bytes into the stream data `data`
			allocation,		// memory the loader allocates without reading the stream ( opaque: copy it, do not build it )
			asset,			// the XAsset table entry of top level asset `asset` ( offset 8: its header slot )
			raw,			// a value the loaders convert although it points at nothing they placed; written as it is
		};
		Kind kind = Kind::none;
		ZoneDataId data = 0;
		std::uint32_t offset = 0;
		std::size_t asset = static_cast<std::size_t>(-1);
		std::uint32_t allocation = 0;
		std::uint32_t raw = 0;

		static ZoneTarget to_data(const ZoneDataId id, const std::uint32_t offset = 0)
		{
			ZoneTarget t;
			t.kind = Kind::data;
			t.data = id;
			t.offset = offset;
			return t;
		}
		// a pointer to a top level asset, as the zone linker writes it: at the header slot of its XAsset entry
		static ZoneTarget to_asset(const std::size_t index)
		{
			ZoneTarget t;
			t.kind = Kind::asset;
			t.asset = index;
			t.offset = 8;
			return t;
		}
	};

	struct ZonePointer
	{
		std::uint32_t offset = 0;		// of the 32 bit field inside its data
		ZoneTarget target;
	};

	// a pointer field somewhere in the zone that leads into a piece of data
	struct ZoneInbound
	{
		ZoneDataId from = 0;			// data that holds the field
		std::uint32_t offset = 0;		// of the field
		std::uint32_t targetOffset = 0;	// where in the target it points
	};

	struct ZoneDataInfo
	{
		ZoneDataId id = 0;
		std::size_t asset = static_cast<std::size_t>(-1);	// top level asset that owns it; npos: the XAssetList
		std::uint32_t index = 0;		// position in the asset's stream order
		std::uint32_t size = 0;			// current size ( strings: including the terminator )
		zaddr address = 0;				// where the loaded zone ( zone() ) has it; 0 for data made by an edit
		bool string = false;
		std::uint32_t pointers = 0;		// zone pointers inside it
	};

	// ---- assets made from nothing ( add_new_asset ) ------------------------------------------------
	// A pointer field of a new piece of data: to another piece of the same new asset, or to a top level asset
	// that the zone already has.
	struct ZoneNewPointer
	{
		std::uint32_t offset = 0;			// of the 32 bit field inside its piece
		std::uint32_t piece = 0;			// target: index into the piece list ..
		std::uint32_t pieceOffset = 0;
		std::size_t asset = static_cast<std::size_t>(-1);	// .. or, when set, the header slot of this top level asset
	};

	// One read of the game's loaders: a struct, an array, a string ( with its terminator ). Pointer fields that are
	// not listed in `pointers` are plain bytes ( 0: null, 0xFFFFFFFF: the data follows in the stream ).
	struct ZoneNewPiece
	{
		std::vector<std::uint8_t> bytes;
		bool string = false;
		std::vector<ZoneNewPointer> pointers;
		// u16 fields that hold a script string ( bone names .. ): offset and text; the zone's list grows as needed
		std::vector<std::pair<std::uint32_t, std::string>> scriptStrings;
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
		// string of this zone. Script string indices of the asset ( bone names, notetracks .. ) are translated to this
		// zone's script string list, which grows by the strings it lacks. Fails ( and changes nothing ) when a pointer
		// cannot be re-targeted: the asset shares data other than strings with another asset of its zone.
		Status add_asset(const ZoneEditor& donor, std::size_t donorIndex, std::size_t position = npos, bool withDependencies = false);

		// add_asset with dependencies, for assets that share data with other top level assets of their zone ( the
		// technique sets of a map zone share techniques, passes and shaders ): every asset of the donor that the
		// copied one points into is copied as well, in front of it, and so on.
		Status add_asset_closure(const ZoneEditor& donor, std::size_t donorIndex, std::size_t position = npos);

		// Appends ( or inserts in front of `position` ) a top level asset that is given as the pieces of stream data
		// the game's loaders read for it, in the order they read them: header first. Nothing is checked here;
		// write() runs the loaders over it and fails when the pieces do not fit the header.
		Status add_new_asset(std::int32_t type, std::string_view name, std::span<const ZoneNewPiece> pieces, std::size_t position = npos);

		// Replaces a top level asset with one of another zone ( same type ); pointers of other assets to the
		// replaced asset's header keep working. Same rules as add_asset.
		Status replace_asset(std::size_t index, const ZoneEditor& donor, std::size_t donorIndex);

		// ---- typed, relocation aware editing --------------------------------------------------
		// The asset graph below an asset, piece by piece. Every piece of stream data has an id; a pointer field is a
		// ( data, offset ) pair with a target, never a number, so it stays right wherever the zone is laid out.
		//
		// What the loaders read, and in which order, is decided by the data itself ( counts, pointer fields that hold
		// 0xFFFFFFFF = "the data follows" ). An edit therefore has to leave the graph in a state the loaders agree
		// with: a count that matches the array it describes, a piece of data for every pointer that says "follows",
		// in the order the loader visits the fields. write() checks exactly that and fails with a message that names
		// the asset and the piece when it does not hold; nothing is written then.
		//
		// The loaded zone ( zone() ) is not updated by edits: addresses always refer to the zone as it was loaded.

		// the data that holds image address `address` of the loaded zone ( block addresses, not the header pool );
		// 0 when the loaders read nothing there ( null, runtime allocations, another zone's asset )
		ZoneDataId data_at(zaddr address, std::uint32_t* offset = nullptr) const;
		// the stream data an asset header of the loaded zone ( pool address: XAssetEntry::header, a nested asset ) was
		// read as. Fields of the header are edited through it.
		ZoneDataId header_data(zaddr pooledHeader) const;
		// index-th piece of a top level asset, in stream order ( 0 is its header )
		ZoneDataId asset_data(std::size_t asset, std::uint32_t index) const;

		bool data_info(ZoneDataId id, ZoneDataInfo& info) const;
		// current bytes ( pointer fields hold the stream value of the source zone ); valid until the next edit
		std::span<const std::uint8_t> data_bytes(ZoneDataId id) const;
		std::vector<ZonePointer> data_pointers(ZoneDataId id) const;
		// target of the pointer field at `offset`; Kind::none when the field is not a zone pointer
		ZoneTarget pointer_at(ZoneDataId id, std::uint32_t offset) const;
		// every pointer field of the zone that leads into `id`
		std::vector<ZoneInbound> pointers_to(ZoneDataId id) const;

		// Replaces the contents of a piece of data: any length, with the zone pointers it holds ( all former ones are
		// dropped ). Pointers of other data into it keep their offset; fails when one of them would point past the
		// new end. The array "resize": set_data + patch of the count field in the owner.
		Status set_data(ZoneDataId id, std::span<const std::uint8_t> bytes, std::span<const ZonePointer> pointers = {});
		// Overwrites bytes in place ( a struct field ). Fails when the range touches a zone pointer field.
		Status patch_data(ZoneDataId id, std::uint32_t offset, std::span<const std::uint8_t> bytes);
		template <typename T>
		Status patch_value(const ZoneDataId id, const std::uint32_t offset, const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			return patch_data(id, offset, { reinterpret_cast<const std::uint8_t*>(&value), sizeof(T) });
		}
		// Makes the 32 bit field at `offset` a pointer to `target`, or with Kind::none plain bytes holding `plain`
		// ( 0: null, 0xFFFFFFFF: the data follows in the stream ).
		Status set_pointer(ZoneDataId id, std::uint32_t offset, const ZoneTarget& target, std::uint32_t plain = 0);
		// Adds a piece of data to the asset that owns `anchor`, directly behind ( or in front of ) it in stream
		// order. The caller picks the place where the loader will read it ( behind the struct whose pointer says
		// "follows", after the data of the pointer fields before it ).
		Status insert_data(ZoneDataId anchor, bool after, std::span<const std::uint8_t> bytes, std::span<const ZonePointer> pointers,
			ZoneDataId& created, bool string = false);
		// Removes a piece of data. Fails when a pointer field of the zone still leads into it.
		Status remove_data(ZoneDataId id);
		// Removes the pieces `first` .. `last` of one asset ( stream order ) with all pointers, script string fields,
		// nested asset headers and loader allocations inside them: a whole sub tree, such as a nested asset. Fails,
		// changing nothing, when a pointer of data outside the range leads into it or into an allocation the loader
		// makes for it, or when it cannot be told which side of the range an allocation belongs to.
		Status remove_data_range(ZoneDataId first, ZoneDataId last);

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
