#pragma once

// codol_view: native Direct3D 9 renderer for Call of Duty Online 1.1.1.13 maps.
//
// Draws a map's GfxWorld, static models, script_model entities and brush models with the game's own materials,
// technique sets and compiled SM3 shaders, read from the 1.1.1.13 fastfiles ( codol_zone ) and bound the way the
// game binds them ( codol_render ). Port of CODOL-AssetExplorer's Codol.Rendering\GameMapRenderer.
//
// EMBEDDING. The renderer never owns the device: the host creates it and may draw its own scene with it.
//   - render() saves every piece of device state it touches ( an IDirect3DStateBlock9 of type D3DSBT_ALL, plus
//     render target 0, the depth stencil surface and the viewport, which a state block does not hold ) and restores
//     all of it before it returns. It never calls Present, never touches the back buffer and never changes the
//     FPU control word.
//   - BeginScene / EndScene are configurable ( frame_options::scene ).
//   - Works with a plain IDirect3DDevice9 and with an IDirect3DDevice9Ex; the device needs
//     D3DCREATE_HARDWARE_VERTEXPROCESSING or MIXED ( vs_3_0 ). D3DCREATE_MULTITHREADED / FPU_PRESERVE / PUREDEVICE
//     are all fine ( no Get* state call is made ).
//   - Device loss: call on_device_lost() before IDirect3DDevice9::Reset and on_device_reset() after it.
//   - Threading: one renderer is used from one thread at a time ( the thread that uses the device ). There is no
//     global mutable state; several renderers can exist side by side.
//
// Only <d3d9.h> of the Windows SDK is needed - no D3DX.

#include "codol_render/code_constants.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct IDirect3DDevice9;
struct IDirect3DTexture9;
struct IDirect3DSurface9;

namespace codol::view
{
	using camera = render::camera;		// origin, pitch / yaw / roll in degrees, fov_x, z_near ( game axes: X forward, Y left, Z up )
	using float3 = render::float3;

	// where static GPU resources ( textures, vertex / index buffers ) live
	enum class resource_pool
	{
		automatic,		// managed for an IDirect3DDevice9, default pool for an IDirect3DDevice9Ex ( which has no managed pool )
		managed,		// D3DPOOL_MANAGED: survives Reset; the runtime keeps a system memory copy of everything
		default_pool,	// D3DPOOL_DEFAULT: no system memory copy; on_device_reset() loads the map again
	};

	struct options
	{
		std::filesystem::path game_root;							// the 1.1.1.13 install ( holds main\, client_shipRetail\ )
		std::filesystem::path client_exe;							// default: <game_root>\C1.exe ( keys, wavelet tables )
		std::vector<std::filesystem::path> zone_directories;		// default: <game_root>\client_shipRetail\chinese, ..\patch
		resource_pool pool = resource_pool::automatic;
		render::renderer_settings settings;							// the game's dvar defaults the C# renderer uses
		// drop this many top mip levels of streamed textures ( r_picmip ); 0 = full resolution
		int texture_skip_mips = 0;
		// also upload the lower LODs of every model ( needed for frame_options::model_lods )
		bool load_model_lods = false;
		// keep the frozen shared / level zones ( technique sets, shaders, materials by name ) between maps
		bool keep_library = true;
	};

	enum class scene_mode
	{
		automatic,			// BeginScene; when that fails the host is inside a scene already and EndScene is not called
		begin_end,			// always BeginScene .. EndScene
		inside_scene,		// the host is between BeginScene and EndScene: neither is called
	};

	struct frame_options
	{
		// Target. Null: an internally owned A8R8G8B8 render target texture of the requested size ( returned by
		// render() ). Non null: the caller's surface ( render() then returns nullptr ); `depth_stencil` may be given
		// with it, else an internal depth buffer of the size is used ( the target must not be multisampled then ).
		IDirect3DSurface9* target = nullptr;
		IDirect3DSurface9* depth_stencil = nullptr;
		bool clear = true;
		std::uint32_t clear_color = 0xFF000000;		// D3DCOLOR ( GameMapRenderer.cs:807 )
		scene_mode scene = scene_mode::automatic;

		float game_time = 0;						// seconds ( animated materials )
		bool draw_world = true;
		bool draw_static_models = true;				// GfxStaticModelDrawInst
		bool draw_entity_models = true;				// script_model entities
		bool draw_brush_models = true;				// "*N" sub models of entities
		// brush models of entities with "script_specialops" "1": geometry the level script only keeps in special ops
		// games ( map wide blockers ); the multiplayer script deletes them at start
		bool draw_special_ops_brush_models = false;
		bool cell_culling = true;					// cell / portal visibility on top of the frustum
		bool sun_shadows = true;					// dynamic sun shadow map ( two partitions )
		bool float_z = true;						// depth pre-pass into an R32F target for the soft ( z feathered ) materials
		bool model_lods = false;					// pick the XModel LOD by XModelLodInfo::dist ( the C# renderer always draws LOD 0 )
		float lod_scale = 1.0f;						// r_lodScaleRigid
		bool fog = false;							// see map_renderer::fog_source()
		bool wireframe = false;
		// collect scene_object::drawn for map_renderer::objects()
		bool track_drawn_objects = false;
	};

	struct statistics
	{
		// ---- last frame ( GameRenderStats ) -------------------------------------------------------------
		int surfaces = 0;					// world surfaces drawn ( before merging into batches )
		int culled_surfaces = 0;
		int model_instances = 0;			// static + entity models drawn
		int model_surfaces = 0;
		int culled_models = 0;
		int brush_models = 0;				// brush model instances drawn
		int brush_model_surfaces = 0;
		int draw_calls = 0;
		std::int64_t triangles = 0;
		int skipped_no_technique = 0;
		int skipped_incomplete_pass = 0;
		int skipped_hidden = 0;				// materials with cameraRegion >= 4
		int material_changes = 0;
		int shader_changes = 0;
		int texture_binds = 0;
		int sun_shadow_draws = 0;
		int float_z_draws = 0;
		int camera_cell = -1;
		int reached_cells = 0;
		int cell_visits = 0;
		double cpu_ms = 0;					// time inside render(), CPU side

		// ---- since load_map --------------------------------------------------------------------------------
		int world_surface_count = 0;
		int static_model_count = 0;
		int entity_model_count = 0;
		int brush_model_count = 0;			// brush model entities placed
		int distinct_models = 0;
		int streamed_models = 0;			// models whose surfaces came from an IWD
		int foreign_models = 0;				// models read from another zone
		int cell_count = 0;
		int light_count = 0;
		int lightmap_count = 0;
		int reflection_probe_count = 0;
		int materials = 0;
		int vertex_shaders = 0;
		int pixel_shaders = 0;
		int textures = 0;
		int missing_textures = 0;			// names in map_renderer::texture_failures()
		int unbound_constants = 0;			// distinct code constants a drawn shader read that nothing computes
		int light_grid_traces = 0;
		int light_grid_blocked = 0;
		std::uint64_t texture_bytes = 0;	// GPU bytes of the textures
		std::uint64_t geometry_bytes = 0;	// GPU bytes of vertex + index buffers
		std::uint64_t zone_bytes_peak = 0;	// largest sum of inflated zones held at once while loading
		double load_ms = 0;
	};

	enum class object_kind
	{
		world_surface,
		static_model,
		entity_model,
		brush_model,
	};

	// One drawable thing of the map, for picking / selection in an editor
	struct scene_object
	{
		object_kind kind = object_kind::world_surface;
		int id = 0;				// surface index ( GfxWorld.dpvs.surfaces ), static model index ( smodelDrawInsts ), or entity index
		int entity = -1;		// index of the entity in the entity string; -1 for world surfaces and static models
		float3 center;			// world space bounds
		float3 half_size;
		std::string name;		// material ( surfaces ), model name, or "*N"
		bool drawn = false;		// drawn in the last frame rendered with frame_options::track_drawn_objects
	};

	class map_renderer
	{
	public:
		map_renderer();
		~map_renderer();
		map_renderer(const map_renderer&) = delete;
		map_renderer& operator=(const map_renderer&) = delete;

		// Opens the install ( keys, IWD archives ) and creates the device objects that do not depend on a map.
		// The device is AddRef'ed until shutdown().
		bool init(IDirect3DDevice9* device, const options& options, std::string* error = nullptr);
		void shutdown();
		bool initialised() const;

		// Loads <map_name>.ff, the level zones and the shared zones it refers to, builds every GPU resource and
		// releases the zone memory again. Replaces a loaded map.
		bool load_map(const std::string& map_name, std::string* error = nullptr);
		void unload();
		bool loaded() const;
		const std::string& map_name() const;

		// Draws the map. Returns the colour texture of the internal render target, or nullptr when the caller
		// supplied the target, nothing is loaded or the device is lost.
		IDirect3DTexture9* render(const camera& cam, int width, int height, const frame_options& frame = {});

		// Before / after IDirect3DDevice9::Reset. on_device_reset() returns false when a resource could not be
		// recreated ( text in last_error() ).
		void on_device_lost();
		bool on_device_reset();

		statistics stats() const;
		std::vector<std::string> warnings() const;
		std::vector<std::string> texture_failures() const;
		const std::string& last_error() const;

		// ---- for the editor ------------------------------------------------------------------------------
		std::vector<scene_object> objects() const;
		// world bounds of the map ( GfxWorld::bounds ): centre, half size
		bool world_bounds(float3& center, float3& half_size) const;
		// the entity string of the map ( MapEnts ), empty when nothing is loaded
		const std::string& entity_string() const;
		// camera of the n-th spawn like entity ( mp_global_intermission first, then spawn points ); false when out of range
		bool spawn_camera(int n, camera& out) const;
		// where the fog of frame_options::fog comes from, e.g. "none found" - see README
		const std::string& fog_source() const;

	private:
		struct impl;
		std::unique_ptr<impl> impl_;
	};
}
