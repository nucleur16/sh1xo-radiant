#pragma once

// The state of a map_renderer. Port of the fields of Codol.Rendering\GameMapRenderer.cs ( and its partial classes )
// and of GameTextureCache.cs.

#include "codol_view/map_renderer.hpp"

#include "entities.hpp"
#include "light_grid.hpp"
#include "map_types.hpp"
#include "model_surfaces.hpp"
#include "sight_trace.hpp"
#include "world_visibility.hpp"
#include "zone_library.hpp"

#include "codol_render/code_tables.hpp"
#include "codol_render/iwd.hpp"
#include "codol_render/render_state.hpp"
#include "codol_render/shader_program.hpp"
#include "codol_render/technique_choice.hpp"
#include "codol_render/texture.hpp"
#include "codol_render/vertex_format.hpp"

#include <d3d9.h>

#include <array>
#include <map>
#include <set>

namespace codol::view
{
	template <typename T>
	class com_ptr
	{
	public:
		com_ptr() = default;
		~com_ptr() { reset(); }
		com_ptr(const com_ptr& o) : p_(o.p_) { if (p_) { p_->AddRef(); } }
		com_ptr(com_ptr&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
		com_ptr& operator=(com_ptr o) noexcept { std::swap(p_, o.p_); return *this; }
		void reset() { if (p_) { p_->Release(); p_ = nullptr; } }
		T* get() const { return p_; }
		T* operator->() const { return p_; }
		T** put() { reset(); return &p_; }
		explicit operator bool() const { return p_ != nullptr; }

	private:
		T* p_ = nullptr;
	};

	// GameTextureCache.cs:8-19 ( GameTexture )
	struct gpu_texture
	{
		com_ptr<IDirect3DBaseTexture9> handle;
		bool cube = false;
		std::string name;
		int width = 0;
		int height = 0;
		std::uint64_t bytes = 0;
	};

	struct material_draw;

	// GameMapRenderer.cs:1649-1656 ( PassPlan ) + the device objects of the pass
	struct pass_plan
	{
		bool complete = false;						// declaration, vertex and pixel shader are all there
		IDirect3DVertexShader9* vertex_shader = nullptr;	// owned by impl::vertex_shaders
		IDirect3DPixelShader9* pixel_shader = nullptr;
		std::string vertex_shader_name, pixel_shader_name;
		const render::vertex_declaration* declaration = nullptr;
		render::vertex_declaration own_declaration;
		std::vector<std::string> names;				// per argument: the register's name in the shader's constant table, "" when none
		std::vector<render::constant_handle> handles;
		int by_index_generation = -1;
		std::array<IDirect3DVertexDeclaration9*, render::vertex_format::COUNT> declarations{};	// owned by impl::declarations
		std::array<std::uint8_t, render::vertex_format::COUNT> declaration_resolved{};
	};

	// GameMapRenderer.cs:181-188 ( MaterialDraw )
	struct material_draw
	{
		std::shared_ptr<const library_material> material;
		std::shared_ptr<const library_techset> techset;
		int id = 0;
		std::uint64_t slot_mask = 0;
		std::vector<gpu_texture*> textures;			// per material texture def; nullptr: not loaded / failed
		std::vector<std::uint8_t> texture_resolved;

		const render::material& asset() const { return material->asset; }
		const render::render_technique_set& set() const { return techset->set; }
		const render::render_technique* technique(const int type) const { return techset->set.technique(type); }
		bool not_drawn() const { return material->asset.state && material->asset.state->not_drawn(); }
	};

	// GameMapRenderer.cs:159-179 ( WorldSurface )
	struct world_surface
	{
		int index = 0;
		material_draw* material = nullptr;
		int first_vertex = 0;
		int vertex_count = 0;
		int triangle_count = 0;
		int base_index = 0;
		std::uint32_t layer_data_offset = 0;
		int lightmap_index = 0;
		int reflection_probe_index = 0;
		int primary_light_index = 0;
		float3 center;
		float3 half;
		float radius = 0;
		bool emissive = false;
		int absolute_index = 0;
		bool brush_model = false;		// not part of the static world: drawn through a brush model instance
	};

	// GameMapRenderer.cs:220-231 ( ModelSurfaceDraw )
	struct model_surface_draw
	{
		material_draw* material = nullptr;
		std::vector<int> rigid_bones;
		int vertex_count = 0;
		int triangle_count = 0;
		int first_vertex = 0;
		int base_index = 0;
	};

	// GameMapRenderer.cs:233-242 ( ModelDraw ): one LOD of a model
	struct model_lod_draw
	{
		std::vector<model_surface_draw> surfaces;
		com_ptr<IDirect3DVertexBuffer9> vertex_buffer;
		com_ptr<IDirect3DIndexBuffer9> index_buffer;
		float dist = 0;					// XModelLodInfo::dist
	};

	struct model_draw
	{
		std::string name;
		std::vector<model_lod_draw> lods;		// lods[ 0 ] always exists
		bool uses_lighting_volume = false;
		float3 min;
		float3 max;
		std::vector<std::string> bone_names;

		const std::vector<model_surface_draw>& surfaces() const { return lods[0].surfaces; }
	};

	// GameMapRenderer.cs:192-218 ( StaticModelInstance )
	struct static_model_instance
	{
		model_draw* draw = nullptr;
		mat4 placement;
		float3 center;
		float radius = 0;
		float3 half;
		int static_index = -1;			// index in dpvs.smodelDrawInsts, -1 for entity models
		int entity_index = -1;
		float3 lighting_origin;
		float cull_distance = 0;
		int reflection_probe = 0;
		int primary_light = 0;
		std::uint8_t flags = 0;
		std::uint32_t ground_lighting = 0;
		int lighting_handle = 0;
		bool is_entity = false;
		std::vector<std::uint8_t> hidden_surfaces;		// empty: none hidden
		float scale = 1.0f;
		bool drawn = false;
	};

	// a "*N" sub model placed by an entity ( not drawn by the C# renderer )
	struct brush_model_instance
	{
		int model = 0;					// index into GfxWorld::models
		int entity_index = -1;
		mat4 placement;					// rotation + origin of the entity
		float3 center;					// world space, of the placed bounds
		float3 half;
		float radius = 0;
		int first_surface = 0;
		int surface_count = 0;
		bool special_ops = false;		// "script_specialops" "1"
		bool drawn = false;
	};

	struct lightmap_pair
	{
		gpu_texture* primary = nullptr;
		gpu_texture* secondary = nullptr;
	};

	struct reflection_probe
	{
		float3 origin;
		gpu_texture* image = nullptr;
	};

	// Codol.Assets\Rendering\RenderLightDef.cs
	struct light_def
	{
		std::string name;
		gpu_texture* attenuation = nullptr;
		std::uint8_t sampler_state = 0;
		int lookup_start = 0;
		int attenuation_width = 0;
	};

	// Codol.Rendering\GameSunShadow.cs
	struct sun_shadow_partition
	{
		mat4 view;
		mat4 projection;
		mat4 view_projection;
		float depth_scale = 0;
	};

	struct sun_shadow
	{
		int map_size = 0;
		int partition_count = 2;
		sun_shadow_partition partitions[2];
		mat4 lookup;
		float4 switch_partition;
		float4 scale;
		float4 pixel_adjust;
	};

	sun_shadow compute_sun_shadow(const camera& cam, float aspect, const float3& sun_direction, const float3& world_mid, const float3& world_half,
		const render::renderer_settings& settings);

	// GameMapRenderer.cs:1272-1280 ( SurfaceDraw )
	struct surface_draw
	{
		IDirect3DVertexBuffer9* vertex_buffer = nullptr;
		int vertex_stride = 0;
		IDirect3DIndexBuffer9* index_buffer = nullptr;
		int base_vertex = 0;
		int min_vertex = 0;
		int vertex_count = 0;
		int start_index = 0;
		int triangle_count = 0;
		IDirect3DVertexBuffer9* layer_buffer = nullptr;
		int layer_offset = 0;
		int layer_stride = 0;
	};

	// GameMapRenderer.cs:1168 ( ModelSurfaceInstance )
	struct model_surface_instance
	{
		static_model_instance* instance = nullptr;
		const model_lod_draw* lod = nullptr;
		const model_surface_draw* surface = nullptr;
		material_draw* material = nullptr;
		int technique = 0;
	};

	struct brush_surface_instance
	{
		brush_model_instance* instance = nullptr;
		world_surface* surface = nullptr;
		material_draw* material = nullptr;
		int technique = 0;
	};

	struct map_renderer::impl
	{
		// ---- init --------------------------------------------------------------------------------------
		com_ptr<IDirect3DDevice9> device;
		options opts;
		bool is_ex = false;
		bool managed = true;				// static resources in D3DPOOL_MANAGED
		D3DCAPS9 caps{};
		zone_library library;
		render::iwd_key iwd_key{};
		render::image_archive_sets images;
		std::unique_ptr<render::iwd_archive_set> model_archives;
		std::shared_ptr<const render::wavelet_tables> wavelet;
		std::string last_error;
		bool device_lost = false;

		// ---- per map: assets ---------------------------------------------------------------------------
		bool map_loaded = false;
		std::string map_name;
		library_scope scope;
		std::unique_ptr<render::code_constants> constants;
		std::set<std::string> warnings;
		std::vector<std::string> texture_failures;
		std::string entity_string;
		std::vector<map_entity> entities;
		std::string fog_source = "nothing loaded";
		bool has_fog = false;
		render::game_fog fog;

		// textures ( GameTextureCache )
		name_map<std::unique_ptr<gpu_texture>> textures;		// by name, or name#dataSize for inline pixels
		name_map<bool> texture_failed;
		gpu_texture white, black, flat_normal;

		// shaders, declarations, plans
		std::map<const shader_blob*, com_ptr<IDirect3DVertexShader9>> vertex_shaders;
		std::map<const shader_blob*, com_ptr<IDirect3DPixelShader9>> pixel_shaders;
		std::map<const shader_blob*, render::shader_program> programs;
		std::map<std::pair<std::string, int>, com_ptr<IDirect3DVertexDeclaration9>> declarations;
		std::map<const render::render_pass*, std::unique_ptr<pass_plan>> pass_plans;
		name_map<std::unique_ptr<material_draw>> materials;	// by name
		int next_material_id = 0;
		std::set<std::string> unbound_constants;

		// world
		float3 world_mid, world_half;
		int last_sun_light = 0;
		std::vector<world_surface> surfaces;
		struct range { int begin, end; bool emissive; };
		std::vector<range> ranges;
		com_ptr<IDirect3DVertexBuffer9> vertex_buffer, layer_buffer;
		com_ptr<IDirect3DIndexBuffer9> index_buffer, world_indices;
		std::vector<lightmap_pair> lightmaps;
		std::vector<reflection_probe> probes;
		std::vector<map_light> lights;
		name_map<light_def> light_defs;
		gpu_texture* outdoor = nullptr;

		// models
		name_map<std::unique_ptr<model_draw>> models;			// by name
		std::vector<static_model_instance> static_models;
		std::vector<brush_model_instance> brush_models;
		std::unique_ptr<model_lighting_volume> lighting_volume;
		gpu_texture model_lighting;
		std::unique_ptr<world_visibility> visibility;

		// ---- per map: device state that depends on the default pool ------------------------------------
		com_ptr<IDirect3DTexture9> target_texture;
		com_ptr<IDirect3DSurface9> target_surface, depth_surface;
		int target_width = 0, target_height = 0;
		int depth_width = 0, depth_height = 0;
		com_ptr<IDirect3DStateBlock9> saved_state;
		com_ptr<IDirect3DTexture9> shadow_texture;
		com_ptr<IDirect3DSurface9> shadow_surface, shadow_depth;
		gpu_texture shadow_map;
		bool shadow_unavailable = false;
		com_ptr<IDirect3DTexture9> float_z_texture;
		com_ptr<IDirect3DSurface9> float_z_surface;
		gpu_texture float_z;
		int float_z_width = 0, float_z_height = 0;
		bool float_z_unavailable = false;

		// ---- per frame -----------------------------------------------------------------------------------
		statistics stat;					// load numbers + the last frame
		statistics frame_stat;				// the frame being drawn
		const frame_options* frame = nullptr;
		std::vector<std::uint8_t> surface_visible;
		std::vector<model_surface_instance> opaque_models, transparent_models;
		std::vector<brush_surface_instance> opaque_brushes, transparent_brushes;
		const map_light* sun = nullptr;
		bool sun_shadows_active = false;
		std::unique_ptr<sun_shadow> current_sun_shadow;
		const light_def* current_light_def = nullptr;
		material_draw* bound_material = nullptr;
		IDirect3DVertexShader9* bound_vertex_shader = nullptr;
		IDirect3DPixelShader9* bound_pixel_shader = nullptr;
		struct bound_register { std::int64_t key = 0; int version = 0; };
		std::array<bound_register, 256> bound_vertex{};
		std::array<bound_register, 224> bound_pixel{};
		struct applied_sampler { IDirect3DBaseTexture9* texture = nullptr; std::uint8_t state = 0; bool valid = false; };
		std::array<applied_sampler, 16> applied_samplers{};
		std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<render::render_state_value>> state_bits;
		const std::vector<render::render_state_value>* applied_state_bits = nullptr;
		std::map<std::uint8_t, render::sampler_state_values> sampler_states;

		// ---- map_renderer.cpp ------------------------------------------------------------------------------
		bool init(IDirect3DDevice9* dev, const options& o, std::string* error);
		void shutdown();
		void unload();
		void release_default_pool();
		bool ensure_targets(int width, int height, bool need_color, bool need_depth, bool need_float_z);
		void draw_float_z(const camera& cam, const mat4& world_matrix);
		void warn(std::string text) { warnings.insert(std::move(text)); }
		D3DPOOL static_pool() const { return managed ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT; }

		// ---- renderer_load.cpp -----------------------------------------------------------------------------
		bool load_map(const std::string& name, std::string* error);
		bool create_solid(gpu_texture& out, const char* name, std::uint32_t argb);
		bool create_texture(const std::string& name, const render::texture_upload& upload, gpu_texture& out, std::string* error);
		gpu_texture* get_streamed_texture(const std::string& name);
		gpu_texture* get_embedded_texture(const std::string& name, const render::embedded_image& image, const zone::Zone& zone);
		gpu_texture* get_image(const zone::Zone& zone, zaddr image);
		gpu_texture* material_texture(material_draw& material, std::size_t index);
		material_draw* get_material(const zone::Zone& zone, zaddr address);
		material_draw* get_material_by_name(const std::string& name, const zone::Zone* defining_zone);
		com_ptr<IDirect3DVertexBuffer9> create_vertex_buffer(std::span<const std::uint8_t> data);
		com_ptr<IDirect3DIndexBuffer9> create_index_buffer(std::span<const std::uint8_t> data, bool index32);
		pass_plan* plan_of(const material_draw& material, int technique_type, int pass_index);
		IDirect3DVertexDeclaration9* declaration_of(pass_plan& plan, int format);
		void prepare_technique(material_draw& material, int technique_type, int format);
		int choose_technique(const material_draw& material, int primary_light_index, bool emissive) const;
		static bool blends(const material_draw& material, int technique_type);
		const map_light* primary_light(int index) const;

		// ---- renderer_draw.cpp -----------------------------------------------------------------------------
		IDirect3DTexture9* render(const camera& cam, int width, int height, const frame_options& options);
		void render_frame(const camera& cam, int width, int height, IDirect3DSurface9* color, IDirect3DSurface9* depth);
		void reset_frame_caches();
		void cull_surfaces(const camera& cam, std::span<const float4> frustum);
		void collect_models(const camera& cam, std::span<const float4> frustum);
		void collect_brush_models(const camera& cam, std::span<const float4> frustum);
		void draw_world_surfaces(int begin, int end, bool emissive, int forced_technique = -1);
		bool resolve_surface(const world_surface& surface, bool emissive, int forced_technique, material_draw*& material, int& technique_type);
		void draw_resolved_surface(const world_surface& surface, material_draw& material, int technique_type, int format);
		void draw_model_surfaces(const float3& eye, const std::vector<model_surface_instance>& draws, int forced_technique = -1);
		void draw_model_surface(const float3& eye, const static_model_instance& instance, const model_lod_draw& lod, const model_surface_draw& surface,
			material_draw& material, int technique_type, int triangles, int vertex_span);
		void draw_brush_surfaces(const float3& eye, const std::vector<brush_surface_instance>& draws);
		void draw_passes(material_draw& material, int technique_type, int format, int reflection_probe, const lightmap_pair* lightmap, const surface_draw& draw);
		void set_primary_light(int index);
		void apply_state_bits(const material_draw& material, int technique_type, int pass);
		void bind_arguments(const render::render_pass& pass, pass_plan& plan, material_draw& material);
		void bind_custom_samplers(const render::render_pass& pass, int reflection_probe, const lightmap_pair* lightmap);
		void bind_sampler(int sampler, const gpu_texture& texture, std::uint8_t sampler_state);
		void set_constants(const render::shader_argument& arg, const float* values, int rows, std::int64_t key = 0, int version = 0);
		bool already_bound(bool vertex, int reg, int rows, std::int64_t key, int version) const;
		const model_lod_draw& choose_lod(const static_model_instance& instance, const camera& cam) const;

		// ---- renderer_shadow.cpp ---------------------------------------------------------------------------
		void update_sun_shadow_state();
		void draw_sun_shadows(const camera& cam, float aspect);
		void release_shadow_targets();
	};

	// GameMapRenderer.cs:1363-1377 ( Frustum ): left, right, bottom, top, near - eye relative, normalised
	std::array<float4, 5> build_frustum(const camera& cam, float aspect);
	// GameMapRenderer.cs:1379-1389 ( Visible )
	bool sphere_visible(std::span<const float4> planes, const float3& center, float radius);

	inline float dot(const float3& a, const float3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline float length(const float3& a) { return std::sqrt(dot(a, a)); }
	inline float3 cross(const float3& a, const float3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	// Vector3.Transform( v, m ): row vector, w = 1
	inline float3 transform_point(const float3& v, const mat4& m)
	{
		return { v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0],
			v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1],
			v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2] };
	}
}
