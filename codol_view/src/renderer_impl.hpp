#pragma once

// The state of a map_renderer. Port of the fields of Codol.Rendering\GameMapRenderer.cs ( and its partial classes )
// and of GameTextureCache.cs.

#include "codol_view/map_renderer.hpp"

#include "entities.hpp"
#include "light_grid.hpp"
#include "map_types.hpp"
#include "model_surfaces.hpp"
#include "pick_bvh.hpp"
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
#include <optional>
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
		int cull = -1;								// picking: -1 not looked up yet, else the cull bits ( 0 / 1 none, 2 back, 3 front )
		bool sky = false;							// picking: a sky box material ( valid once cull is )

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
		bool hidden = false;			// map_renderer::set_hidden
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
		std::unique_ptr<pick_mesh> pick;		// LOD 0 in model space ( options::keep_pick_geometry )

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
		bool hidden = false;			// map_renderer::set_hidden
		bool moved = false;				// has a transform override: no cell culling
		int handle = 0;					// > 0: placed with map_renderer::add_model
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
		bool hidden = false;
		bool moved = false;
		float scale = 1.0f;
		float3 local_min, local_max;	// GfxBrushModel::bounds, the model's own space
	};

	// what reset_transform puts back
	struct instance_backup
	{
		mat4 placement;
		float3 center, half;
		float radius = 0;
		float scale = 1.0f;
		float3 lighting_origin;
		int reflection_probe = 0;
		int primary_light = 0;
		std::uint32_t ground_lighting = 0;
		std::array<std::uint32_t, 64> block{};		// the model's texels of the lighting volume
	};

	// map_renderer::create_mesh
	struct dynamic_mesh
	{
		int handle = 0;
		std::vector<mesh_vertex> vertices;			// the host's data ( triangles re-wound by their normals )
		std::vector<std::uint32_t> indices;
		std::string material_name;
		std::uint32_t fallback_color = 0xFFB0B0B0;
		bool two_sided = false;
		bool hidden = false;
		bool drawn = false;
		mesh_shading shading = mesh_shading::flat_color;
		material_draw* material = nullptr;			// game_material
		gpu_texture* color_map = nullptr;			// color_map
		int format = 0;								// vertex format of the game material path
		com_ptr<IDirect3DVertexBuffer9> vertex_buffer, layer_buffer;
		com_ptr<IDirect3DIndexBuffer9> index_buffer;
		int vertex_capacity = 0, index_capacity = 0, layer_capacity = 0;
		int vertex_stride = 0;
		float3 center, half;
		float radius = 0;
		// lighting of where the mesh is
		int primary_light = 0;
		int reflection_probe = 0;
		int lighting_handle = 0;
		std::uint32_t ground_lighting = 0;
		gpu_texture ambient;						// 1 x 1: stands in for the secondary lightmap
		std::uint32_t ambient_argb = 0;
		pick_mesh pick;
	};

	// the edits of a map, to put them back after the map was loaded again ( default pool device reset )
	struct edit_snapshot
	{
		std::vector<object_ref> hidden;
		std::vector<std::pair<object_ref, transform>> moved;
		struct placed { int handle; std::string name; transform placement; };
		std::vector<placed> models;
		struct mesh { int handle; std::vector<mesh_vertex> vertices; std::vector<std::uint32_t> indices; std::string material; std::uint32_t color; bool two_sided; };
		std::vector<mesh> meshes;
		int next_handle = 0;
		bool valid = false;
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
		bool two_sided = false;			// a dynamic mesh that is drawn without culling
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

	struct mesh_draw_instance
	{
		dynamic_mesh* mesh = nullptr;
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

		// ---- per map: what the editor functions need after load_map ------------------------------------
		std::shared_ptr<const zone::Zone> held_map_zone;		// only while load_map runs
		std::shared_ptr<const zone::Zone> held_zone;			// the last other zone read ( released when idle )
		int zone_idle_frames = 0;
		std::map<const library_material*, material_draw*> material_by_asset;
		std::map<const material_draw*, bool> lighting_by_material;
		name_map<model_draw*> resolved_models;					// by name: nullptr = not found
		int static_count = 0;									// static_models[ 0 .. static_count ) are the GfxStaticModelDrawInsts
		std::map<int, int> entity_models;						// entity index -> index into static_models
		std::map<int, int> entity_brushes;						// entity index -> index into brush_models
		std::map<int, instance_backup> model_backups;			// index into static_models -> the map's placement
		std::map<int, instance_backup> brush_backups;			// index into brush_models
		std::map<int, std::unique_ptr<static_model_instance>> placed_models;		// by handle
		std::map<int, std::unique_ptr<dynamic_mesh>> meshes;	// by handle
		int next_handle = 0;
		int hidden_count = 0;
		std::optional<light_grid> grid;							// options::keep_light_grid
		std::unique_ptr<world_sight_trace> trace;
		std::uint64_t light_grid_bytes = 0;
		int lighting_handles = 0;								// handles of the lighting volume in use
		std::vector<int> free_lighting_handles;
		com_ptr<IDirect3DVolumeTexture9> lighting_fill;			// managed / system memory copy that is locked
		int lighting_texture_height = 0;
		std::vector<float3> world_positions;					// options::keep_pick_geometry: every GfxWorldVertex
		pick_mesh world_pick;									// the static world ( surface = surface index )
		std::map<int, std::unique_ptr<pick_mesh>> brush_pick;	// by index into brush_models, model space
		std::uint64_t pick_bytes = 0;
		std::int64_t pick_triangles = 0;
		double pick_build_ms = 0;
		std::vector<std::string> model_name_list, material_name_list;
		bool model_names_built = false, material_names_built = false;
		name_map<std::pair<int, int>> image_sizes;
		name_map<com_ptr<IDirect3DTexture9>> thumbnails;		// null: failed
		// Textures the host was given ( the colour target of render(), thumbnails ) that the renderer no longer needs.
		// The host may have put them into draw commands of the frame it is building - a gui draws at the end of
		// its frame -, so they are kept until it says that nothing refers to them ( release_retired ).
		std::vector<com_ptr<IDirect3DTexture9>> host_retired;
		void retire_host_texture(com_ptr<IDirect3DTexture9>& texture)
		{
			if (texture.get())
			{
				host_retired.push_back(std::move(texture));
			}
			texture.reset();
		}
		edit_snapshot pending_edits;							// between on_device_lost and on_device_reset
		// what a zone that was registered again ( register_zone ) left behind: existing meshes and instances may
		// still point at it, and the caches are keyed by these addresses
		std::vector<std::shared_ptr<const library_zone>> retired_zones;
		std::vector<std::unique_ptr<gpu_texture>> retired_textures;
		std::vector<std::unique_ptr<model_draw>> retired_models;

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
		std::vector<mesh_draw_instance> opaque_meshes, transparent_meshes;
		std::vector<dynamic_mesh*> fixed_meshes;				// drawn through the fixed function pipeline
		const map_light* sun = nullptr;
		bool sun_shadows_active = false;
		std::unique_ptr<sun_shadow> current_sun_shadow;
		const light_def* current_light_def = nullptr;
		// what the per object constants were last set for; reset whenever the eye or the shadow state changes
		int constants_light = -2;
		const static_model_instance* constants_instance = nullptr;
		material_draw* bound_material = nullptr;
		IDirect3DVertexShader9* bound_vertex_shader = nullptr;
		IDirect3DPixelShader9* bound_pixel_shader = nullptr;
		struct bound_register { std::int64_t key = 0; int version = 0; };
		std::array<bound_register, 256> bound_vertex{};
		std::array<bound_register, 224> bound_pixel{};
		struct applied_sampler { IDirect3DBaseTexture9* texture = nullptr; std::uint8_t state = 0; bool valid = false; bool material = false; };
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
		std::shared_ptr<const zone::Zone> acquire_zone(const std::string& zone_name);
		material_draw* make_material(const std::shared_ptr<const library_material>& asset, const zone::Zone* defining_zone);
		bool reads_model_lighting(const material_draw& material);
		model_draw* build_model(const zone::Zone& from, zaddr header, const std::string& model_name);
		model_draw* resolve_model(const std::string& model_name);
		int nearest_probe(const float3& point) const;
		bool upload_model_lighting();
		void build_world_pick(std::span<const std::uint8_t> vertex_bytes, std::span<const std::uint8_t> index_bytes);

		// ---- renderer_edit.cpp -----------------------------------------------------------------------------
		static mat4 placement_of(const transform& t);
		static transform transform_of(const mat4& placement, float scale);
		static void placed_box(const float3& min, const float3& max, const mat4& placement, float3& center, float3& half);
		static float3 to_local(const float3& v, const mat4& placement, bool point);
		static_model_instance* find_model(const object_ref& ref);
		const static_model_instance* find_model(const object_ref& ref) const { return const_cast<impl*>(this)->find_model(ref); }
		brush_model_instance* find_brush(const object_ref& ref);
		const brush_model_instance* find_brush(const object_ref& ref) const { return const_cast<impl*>(this)->find_brush(ref); }
		dynamic_mesh* find_mesh(int handle);
		const dynamic_mesh* find_mesh(const int handle) const { return const_cast<impl*>(this)->find_mesh(handle); }
		bool* hidden_flag(const object_ref& ref);
		bool set_hidden(const object_ref& ref, bool hidden);
		void place_model(static_model_instance& instance, const mat4& placement, float scale);
		void light_instance(static_model_instance& instance, int model_light);
		int allocate_lighting_handle();
		void release_lighting_handle(int handle);
		void set_lighting_block(int handle, std::span<const std::uint32_t> block);
		std::uint32_t sample_ground_lighting(const float3& point, int model_light, int* primary_light, std::array<std::uint32_t, 64>* block);
		bool set_transform(const object_ref& ref, const transform& placement);
		bool get_transform(const object_ref& ref, transform& out) const;
		bool reset_transform(const object_ref& ref);
		int add_model(const std::string& name, const transform& placement, int handle, std::string* error);
		bool remove_model(int handle);
		void fill_object(const world_surface& s, scene_object& o) const;
		void fill_object(const static_model_instance& m, scene_object& o) const;
		void fill_object(const brush_model_instance& b, scene_object& o) const;
		void fill_object(const dynamic_mesh& m, scene_object& o) const;
		bool object(const object_ref& ref, scene_object& out) const;
		void build_name_lists();
		bool register_zone(const std::filesystem::path& file, std::string* error);
		bool unregister_zone(const std::string& name);
		void retire_zone(const std::string& name);
		bool material_details(const std::string& name, material_info& out);
		IDirect3DTexture9* material_thumbnail(const std::string& name, int max_size);
		edit_snapshot capture_edits() const;
		void apply_edits(const edit_snapshot& edits);
		void copy_edit_stats(statistics& to) const;
		void release_idle_zone();

		// ---- renderer_pick.cpp -----------------------------------------------------------------------------
		int cull_of(material_draw* material) const;
		bool pick_ray(const float3& origin, const float3& direction, pick_result& out, const pick_options& options) const;
		bool object_box(const object_ref& ref, oriented_box& out) const;
		bool object_triangles(const object_ref& ref, std::vector<float3>& out) const;

		// ---- renderer_mesh.cpp -----------------------------------------------------------------------------
		int create_mesh(const mesh_data& data, int handle, std::string* error);
		bool fill_mesh(dynamic_mesh& mesh, const mesh_data& data, std::string* error);
		bool upload_mesh(dynamic_mesh& mesh);
		void light_mesh(dynamic_mesh& mesh);
		bool destroy_mesh(int handle);
		void collect_meshes(const camera& cam, std::span<const float4> frustum);
		void draw_meshes(const float3& eye, const std::vector<mesh_draw_instance>& draws, int forced_technique = -1);
		void draw_fixed_meshes(const camera& cam, int width, int height);
		void draw_mesh_shadow_casters(std::span<const float4> planes);
		void set_fixed_function_state(const camera& cam, int width, int height);
		void run_overlay(const camera& cam, int width, int height);

		// ---- renderer_draw.cpp -----------------------------------------------------------------------------
		IDirect3DTexture9* render(const camera& cam, int width, int height, const frame_options& options);
		void render_frame(const camera& cam, int width, int height, IDirect3DSurface9* color, IDirect3DSurface9* depth);
		void reset_frame_caches();
		void cull_surfaces(const camera& cam, std::span<const float4> frustum);
		void collect_models(const camera& cam, std::span<const float4> frustum);
		void collect_model(static_model_instance& instance, const camera& cam, std::span<const float4> frustum, const std::vector<std::uint8_t>* cells);
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
		// material_texture: a texture of a material ( the editor's filter and mip bias count for these only )
		void bind_sampler(int sampler, const gpu_texture& texture, std::uint8_t sampler_state, bool material_texture = false);
		// editor: the cube of frame_options::far_clip around the camera does not touch the sphere
		bool beyond_far_clip(const float3& center_from_camera, float radius) const;
		// editor: frame_options::draw_water is off and the material is one of water
		bool water_hidden(const material_draw& material) const;
		void set_constants(const render::shader_argument& arg, const float* values, int rows, std::int64_t key = 0, int version = 0);
		bool already_bound(bool vertex, int reg, int rows, std::int64_t key, int version) const;
		const model_lod_draw& choose_lod(const static_model_instance& instance, const camera& cam) const;

		// ---- renderer_shadow.cpp ---------------------------------------------------------------------------
		void update_sun_shadow_state();
		void draw_sun_shadows(const camera& cam, float aspect);
		void release_shadow_targets();
	};

	// r_picmip: leaves out the `skip` largest levels of a texture that has more of them ( renderer_load.cpp )
	void drop_top_mips(render::texture_upload& upload, int skip);

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
