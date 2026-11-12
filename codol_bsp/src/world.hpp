#pragma once

// The map as the compiler stages see it: what was read from the document, and what the stages add.

#include "bsp_impl.hpp"

namespace codol::bsp
{
	using edit::vec3;

	struct box3
	{
		float mins[3] = { 1e30f, 1e30f, 1e30f };
		float maxs[3] = { -1e30f, -1e30f, -1e30f };

		bool valid() const { return mins[0] <= maxs[0] && mins[1] <= maxs[1] && mins[2] <= maxs[2]; }
		void add(const float p[3])
		{
			for (int k = 0; k < 3; k++)
			{
				mins[k] = std::min(mins[k], p[k]);
				maxs[k] = std::max(maxs[k], p[k]);
			}
		}
		void add(const vec3& v)
		{
			const float p[3] = { static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z) };
			add(p);
		}
		void add(const box3& b)
		{
			if (b.valid())
			{
				add(b.mins);
				add(b.maxs);
			}
		}
		void expand(const float by)
		{
			for (int k = 0; k < 3; k++)
			{
				mins[k] -= by;
				maxs[k] += by;
			}
		}
		bool intersects(const box3& b, const float eps = 0.0f) const
		{
			for (int k = 0; k < 3; k++)
			{
				if (b.mins[k] > maxs[k] + eps || b.maxs[k] < mins[k] - eps)
				{
					return false;
				}
			}
			return true;
		}
		bool contains(const float p[3], const float eps = 0.0f) const
		{
			for (int k = 0; k < 3; k++)
			{
				if (p[k] < mins[k] - eps || p[k] > maxs[k] + eps)
				{
					return false;
				}
			}
			return true;
		}
		c1::Bounds bounds() const
		{
			c1::Bounds b{};
			if (valid())
			{
				for (int k = 0; k < 3; k++)
				{
					b.midPoint[k] = (mins[k] + maxs[k]) * 0.5f;
					b.halfSize[k] = (maxs[k] - mins[k]) * 0.5f;
				}
			}
			return b;
		}
		float radius() const
		{
			float r = 0;
			for (int k = 0; k < 3; k++)
			{
				const float h = (maxs[k] - mins[k]) * 0.5f;
				r += h * h;
			}
			return std::sqrt(r);
		}
	};

	struct map_face
	{
		edit::plane plane;				// points out of the brush
		std::string material;			// as the .map names it ( no "wc/" )
		edit::tex_def texture;
		std::vector<vec3> winding;		// counter clockwise seen from outside; empty: the face has no area
		std::vector<int> corner;		// per winding point: index into map_brush::corners
		int clip_material = -1;
		std::vector<int> cells;			// the cells the face is seen from ( vis stage ); empty: cell 0
		bool draw = false;				// has a world material and is not removed by csg
		bool visible = true;			// cleared when the face lies inside another opaque brush / faces the void
	};

	struct map_brush
	{
		edit::object_id id = edit::no_object;
		int entity = 0;					// index into world::entities
		int model = 0;					// sub model: 0 the world, N the brush model "*N"
		std::vector<map_face> faces;
		std::vector<vec3> corners;
		box3 box;
		std::int32_t contents = 0;
		bool detail = false;
		bool collides = true;			// has a cbrush_t
		bool opaque = false;			// structural and solid: blocks sight ( bsp, csg, flood fill )
		int clip_index = -1;			// cbrush_t index
	};

	struct map_entity
	{
		std::vector<std::pair<std::string, std::string>> pairs;
		std::string classname;
		vec3 origin;
		bool world = false;
		bool in_ents = true;			// written to the entity string
		int model = -1;					// brush model index
		std::vector<int> brushes;

		std::string_view value(std::string_view key, std::string_view fallback = {}) const;
		void set(std::string_view key, std::string_view text);
	};

	struct map_light
	{
		int type = 2;					// ComPrimaryLight type: 1 sun, 2 spot, 3 omni
		float color[3] = { 1, 1, 1 };	// linear, with intensity
		float dir[3] = { 0, 0, -1 };	// sun: towards the sun; spot: the way it shines, negated ( engine convention )
		float origin[3] = {};
		float radius = 300;
		float cos_half_fov_outer = 0, cos_half_fov_inner = 0;
		std::string def = "light_point_linear";
		bool primary = true;
	};

	// clipMap / GfxWorld plane array ( one array, stored by the GfxWorld )
	struct plane_pool
	{
		std::vector<c1::cplane_s> planes;
		std::unordered_multimap<std::uint64_t, std::uint32_t> lookup;

		std::uint32_t add(const double normal[3], double dist);
	};

	struct world
	{
		std::string name;				// mp_x
		std::string bsp_name;			// maps/mp/mp_x.d3dbsp
		std::vector<map_entity> entities;	// [0] the worldspawn
		std::vector<map_brush> brushes;
		std::vector<map_light> lights;	// [0] none, [1] the sun, then the light entities
		std::vector<map_face> portal_faces;	// faces with the material "portal": cell boundaries the mapper placed
		std::vector<map_light> bake_lights;	// the light entities: baked into lightmaps and light grid
		plane_pool planes;
		box3 bounds;					// of all brushes of the world model
		int model_count = 1;
		float sun_dir[3] = { 0.35f, 0.25f, 0.9f };		// towards the sun
		float sun_color[3] = { 1.4f, 1.3f, 1.1f };
		float ambient[3] = { 0.25f, 0.27f, 0.32f };		// ambient + sky light: what a flat ( --no-light ) map is lit with
		float ambient_light[3] = { 0.05f, 0.05f, 0.05f };	// worldspawn "ambient"
		float sky_light[3] = { 0.4f, 0.45f, 0.5f };		// worldspawn "skycolor" x "skylight": what a surface that sees the whole sky receives
		std::uint32_t checksum = 0;
		bool planes_final = false;		// the plane array was written: no stage may add to it

		// zone indices of what the stages added
		std::size_t com_asset = npos, gfx_asset = npos;
		std::uint32_t gfx_planes_chunk = 0;		// chunk of the plane array inside the gfx_map asset
		std::uint32_t name_chunk = 1;			// chunk of the bsp name string inside the com_map asset
	};

	// stand-ins for asset indices that are not known while an asset is built: the com_map ( its name string ) and
	// the gfx_map ( its plane array )
	inline constexpr std::size_t REF_COM = npos - 1, REF_GFX = npos - 2;

	std::uint32_t pack_unit_vector(const float n[3]);

	// document -> world ( brushes with windings, entities, lights ); throws compile_error
	void read_document(const edit::document& doc, const options& opts, world& w, report& rep);

	struct compile_context
	{
		const options& opts;
		report& rep;
		world& w;
		ZoneEditor& editor;
		asset_importer& importer;
		const catalog& cat;
	};

	std::string build_entity_string(const world& w);
	void build_com_world(compile_context& ctx);
	void build_fx_world(compile_context& ctx);
	void build_light_defs(compile_context& ctx);
	void build_gfx_world(compile_context& ctx);
	void build_game_world(compile_context& ctx);
	// the clip map as chunks; its pointers into other assets are symbolic ( REF_COM, REF_GFX ) until resolve_refs
	asset_builder build_clip_map(compile_context& ctx);
	void resolve_refs(asset_builder& b, const world& w);
	void validate_render(std::span<const std::uint8_t> zone_bytes, std::vector<std::string>& problems, std::string* summary);
}
