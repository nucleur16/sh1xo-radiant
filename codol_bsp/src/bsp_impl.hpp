#pragma once

// Shared pieces of the compiler.

#include "codol_bsp/bsp.hpp"
#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <unordered_map>

namespace codol::bsp
{
	namespace c1 = codol::c1;
	using zone::zaddr;
	using zone::Zone;
	using zone::ZoneBuildChunk;
	using zone::ZoneBuildPointer;
	using zone::ZoneEditor;

	inline constexpr std::uint32_t FOLLOWS = 0xFFFFFFFFu;		// "the data follows in the stream"
	inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

	inline std::string format(const char* fmt, ...)
	{
		char text[2048];
		va_list args;
		va_start(args, fmt);
		std::vsnprintf(text, sizeof(text), fmt, args);
		va_end(args);
		return text;
	}

	struct compile_error : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	inline void check(const zone::Status& s, const std::string& what)
	{
		if (!s)
		{
			throw compile_error(what + ": " + s.message);
		}
	}

	// ---- an asset as stream chunks --------------------------------------------------------------
	// The chunks are appended in the order the game's loader reads them; ZoneEditor::write runs that loader over them
	// and refuses a sequence that does not fit.
	struct asset_builder
	{
		std::vector<ZoneBuildChunk> chunks;

		std::uint32_t add(const void* data, const std::size_t size)
		{
			ZoneBuildChunk c;
			const auto* p = static_cast<const std::uint8_t*>(data);
			c.bytes.assign(p, p + size);
			chunks.push_back(std::move(c));
			return static_cast<std::uint32_t>(chunks.size() - 1);
		}
		template <typename T>
		std::uint32_t add(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			return add(&value, sizeof(T));
		}
		template <typename T>
		std::uint32_t add_array(const std::vector<T>& values)
		{
			static_assert(std::is_trivially_copyable_v<T>);
			return add(values.data(), values.size() * sizeof(T));
		}
		std::uint32_t add_zero(const std::size_t size)
		{
			ZoneBuildChunk c;
			c.bytes.assign(size, 0);
			chunks.push_back(std::move(c));
			return static_cast<std::uint32_t>(chunks.size() - 1);
		}
		std::uint32_t add_string(const std::string_view text)
		{
			ZoneBuildChunk c;
			c.bytes.assign(text.begin(), text.end());
			c.bytes.push_back(0);
			c.string = true;
			chunks.push_back(std::move(c));
			return static_cast<std::uint32_t>(chunks.size() - 1);
		}
		template <typename T>
		void put(const std::uint32_t chunk, const std::size_t offset, const T& value)
		{
			std::memcpy(chunks[chunk].bytes.data() + offset, &value, sizeof(T));
		}
		template <typename T>
		T get(const std::uint32_t chunk, const std::size_t offset) const
		{
			T value;
			std::memcpy(&value, chunks[chunk].bytes.data() + offset, sizeof(T));
			return value;
		}
		void follows(const std::uint32_t chunk, const std::size_t offset) { put(chunk, offset, FOLLOWS); }
		// pointer to a chunk of this asset ( asset == npos ) or of an asset the zone already has
		void point(const std::uint32_t chunk, const std::size_t offset, const std::uint32_t target, const std::uint32_t target_offset = 0,
			const std::size_t asset = npos)
		{
			ZoneBuildPointer p;
			p.offset = static_cast<std::uint32_t>(offset);
			p.asset = asset;
			p.chunk = target;
			p.targetOffset = target_offset;
			chunks[chunk].pointers.push_back(p);
			put(chunk, offset, std::uint32_t{ 0 });
		}
		// pointer to a top level asset of the zone
		void point_asset(const std::uint32_t chunk, const std::size_t offset, const std::size_t asset)
		{
			ZoneBuildPointer p;
			p.offset = static_cast<std::uint32_t>(offset);
			p.slot = true;
			p.asset = asset;
			chunks[chunk].pointers.push_back(p);
			put(chunk, offset, std::uint32_t{ 0 });
		}
	};

	// where something that was written into the output zone can be pointed at
	struct zone_ref
	{
		bool slot = false;				// a top level asset
		std::size_t asset = npos;
		std::uint32_t chunk = 0, offset = 0;
		bool valid() const { return asset != npos; }
	};

	inline void point_ref(asset_builder& b, const std::uint32_t chunk, const std::size_t offset, const zone_ref& ref)
	{
		if (ref.slot)
		{
			b.point_asset(chunk, offset, ref.asset);
		}
		else
		{
			b.point(chunk, offset, ref.chunk, ref.offset, ref.asset);
		}
	}

	// ---- assets of the install ------------------------------------------------------------------
	struct clip_material_info
	{
		std::int32_t surface_flags = 0;
		std::int32_t contents = 0;
	};

	// What the install's zones offer: which zone holds a material / xmodel, and the surface flags and contents the
	// game's own map compiler gave every clip material ( a property of the material, not of a map ). Built once per
	// install by scanning its zones, kept in a text file.
	struct catalog
	{
		std::map<std::string, std::string, std::less<>> material_zone;		// material name -> fast file ( relative to the install )
		std::map<std::string, std::string, std::less<>> model_zone;			// xmodel that is a top level asset -> fast file
		std::map<std::string, std::string, std::less<>> nested_model_zone;	// xmodel nested in another asset ( a map's gfx_map ) -> fast file
		std::map<std::string, clip_material_info, std::less<>> clip_materials;
		std::map<std::string, std::string, std::less<>> image_zone;			// image with pixels in a zone -> fast file

		bool load(const std::filesystem::path& file, const std::filesystem::path& install);
		bool save(const std::filesystem::path& file, const std::filesystem::path& install) const;
		// scans every fast file of the install ( client_shipRetail/chinese, patch )
		void scan(const std::filesystem::path& install, const zone::ClientKey& key, std::vector<std::string>& warnings);
	};

	// what the compiler has to know about an xmodel that is placed in the world
	struct model_desc
	{
		float mid[3] = {}, half[3] = {}, radius = 0;	// its box, in model space
		int coll_surfs = 0;							// > 0: it has collision
		bool nested = false;							// written again from a model nested in another asset
		float last_lod_dist = 0;						// draw distance of its last LOD ( 1000000: no limit )
		std::vector<float> triangles;					// LOD 0 in model space, 9 floats per triangle ( light bake: shadows )
	};

	// what the compiler has to know about a world material
	struct material_desc
	{
		zone_ref ref;					// !valid(): no zone has it
		std::string why;
		int width = 256, height = 256;	// of its colour map
		int sort_key = 4;
		std::string techset;
		bool sky = false;
		bool alpha_test = false, blend = false;	// by the technique set's name ( wc_l_sm_t0.. / _b0.., unlit_blend .. )
		// A brush with such a face hides nothing: alpha tested, or blended with a decal sort key ( 6 .. 13 ). Blended
		// materials with other keys are left to the mapper's flags: shipped maps have them on whole walls
		// ( wc/me_cinderblock_wall2_sh of mp_killhouse_sh: wc_l_sm_b0c0q0n0s0p0, sort 14 ).
		bool see_through = false;
		std::uint32_t surface_type_bits = 0;
		zone_ref sky_image;				// the cube map of a sky material, when its pixels or header are in a zone
		std::string sky_image_name;
	};

	// Copies materials ( with technique set, shaders, images ) and xmodels out of loaded zones into the zone that is
	// being built, as top level assets in front of the map assets.
	class asset_importer
	{
	public:
		asset_importer(ZoneEditor& out, const zone::ClientKey& key, std::filesystem::path install, std::vector<std::filesystem::path> donors,
			const catalog& cat);
		~asset_importer();

		// the material `name` as an asset of the output zone; !valid(): no zone has it ( `why` says more )
		zone_ref material(std::string_view name, std::string* why = nullptr);
		// the same with what the compiler needs to know about it; cached
		const material_desc& describe(std::string_view name);
		// an image with its pixels ( e.g. a sky cube map, an outdoor map ) or as a reference by name
		zone_ref image(std::string_view name, std::string* why = nullptr);
		// an xmodel that is a top level asset of a donor or of a zone of the install, copied with everything it uses
		zone_ref model(std::string_view name, std::string* why = nullptr, model_desc* desc = nullptr);

		// a material of an open zone
		zone_ref material_from(const Zone& donor, zaddr header);
		zone_ref image_from(const Zone& donor, zaddr header);
		const Zone* open_zone(const std::string& relative, std::string* why = nullptr);
		const ZoneEditor* open_editor(const std::string& relative, std::string* why = nullptr);

		struct state;

	private:
		std::unique_ptr<state> s;
	};

	// tool materials: brush faces with these are never drawn
	bool is_tool_material(std::string_view name);

	struct stopwatch
	{
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
		double seconds() const { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); }
	};
}
