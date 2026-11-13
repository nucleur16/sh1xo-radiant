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
//
// EDITING. On top of drawing the shipped map the renderer is the viewport of a map editor ( README, "Editor API" ):
// camera math, an overlay hook inside render(), triangle accurate picking, hiding and moving objects, placing
// new models, host supplied meshes drawn with game materials, a material / model catalog with thumbnails.
// None of it changes a frame while it is not used.

#include "codol_render/code_constants.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
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

		// ---- editing ----------------------------------------------------------------------------------------
		// Keep CPU side positions / indices of the world, the brush models and every model, with a BVH each, for
		// map_renderer::pick() and object_triangles(). Costs statistics::pick_bytes of system memory.
		bool keep_pick_geometry = false;
		// Keep the light grid and the collision sight trace of the map, so that a moved or newly placed model is
		// lit from the light grid at its new position and a dynamic mesh gets the ambient light of where it is.
		// Without it a moved model keeps the lighting it had and a new model / mesh gets a neutral ambient.
		bool keep_light_grid = false;
		// Zones ( names without ".ff" ) whose materials and models can also be used by add_model / create_mesh /
		// the catalogs, searched after the map, level and shared zones. Each is loaded and frozen once in load_map.
		std::vector<std::string> extra_zones;
	};

	// What the overlay hook gets. Matrices are 16 floats, row-major with row vectors ( D3DMATRIX: v' = v * M,
	// translation in the fourth row ) - they can go to IDirect3DDevice9::SetTransform as they are.
	struct overlay_context
	{
		IDirect3DDevice9* device = nullptr;
		camera cam;
		int width = 0;
		int height = 0;
		float view[16] = {};			// world space -> view space ( = view_matrix() )
		float projection[16] = {};		// = projection_matrix()
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

		// ---- editing ----------------------------------------------------------------------------------------
		bool draw_placed_models = true;				// models of map_renderer::add_model
		bool draw_meshes = true;					// meshes of map_renderer::create_mesh
		// Called once per render(), after the scene is drawn and before the host's state is restored, inside the
		// scene ( between BeginScene and EndScene ). The colour target and the depth buffer of the scene are bound
		// and the viewport is the whole target. The device is in this state:
		//   no vertex shader, no pixel shader, FVF D3DFVF_XYZ | D3DFVF_DIFFUSE, no texture on stage 0, stage 0
		//   colour and alpha = diffuse, stage 1 disabled, lighting off, fog off;
		//   world = identity, view = overlay_context::view, projection = overlay_context::projection
		//   ( so vertices are plain world space positions and are depth tested against the scene );
		//   z test on ( D3DCMP_LESSEQUAL ), z write on, culling off, alpha blend off, alpha test off, stencil off,
		//   solid fill, colour writes on, no depth bias, scissor off.
		// Draw with DrawPrimitiveUP / DrawIndexedPrimitiveUP ( or anything else ) and change any state: render()
		// restores everything of the host afterwards as it does for its own drawing. Do not call map_renderer
		// functions that change the scene from inside the hook; pick() and the queries are fine.
		// Geometry drawn exactly on a surface z-fights with it ( the scene is drawn camera relative by shaders, the
		// overlay through the fixed function pipeline ): set a small negative D3DRS_DEPTHBIAS or offset the lines.
		std::function<void(const overlay_context&)> overlay;

		// ---- render modes of an editor ( none of them changes a frame while it has its default ) -------------
		// Lit materials are drawn with the "unlit" technique of their technique set where it has one: the colour
		// map without lightmap, sun, primary light or specular. What the game does for r_fullbright.
		bool fullbright = false;
		// false: nothing whose technique set is one of water is drawn
		bool draw_water = true;
		// Filter of the material textures: 0 as the material says, 1 point ( no mip maps ), 2 bilinear ( no mip
		// maps ), 3 trilinear, 4 anisotropic. Lightmaps, shadow maps and the other code textures keep theirs.
		int texture_filter = 0;
		// added to the mip level of the material textures ( 1 = half the resolution, 2 = a quarter .. )
		float mip_bias = 0;
		// the specular map / normal map of every material reads as black / as flat
		bool specular_maps = true;
		bool normal_maps = true;
		// > 0: only what touches the cube of this half size around the camera is drawn ( cubic clipping )
		float far_clip = 0;
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

		// ---- editing ----------------------------------------------------------------------------------------
		int placed_models = 0;				// add_model instances drawn in the last frame
		int placed_model_count = 0;			// .. existing
		int meshes = 0;						// create_mesh meshes drawn in the last frame
		int mesh_count = 0;					// .. existing
		int hidden_objects = 0;				// objects hidden with set_hidden
		int moved_objects = 0;				// map objects with a transform override
		std::uint64_t pick_bytes = 0;		// system memory of the pick geometry ( positions, indices, BVHs ); 0 without keep_pick_geometry
		std::int64_t pick_triangles = 0;	// triangles in it ( every distinct model once )
		double pick_build_ms = 0;			// part of load_ms spent building it
		std::uint64_t light_grid_bytes = 0;	// system memory kept for options::keep_light_grid ( estimate )
	};

	enum class object_kind
	{
		world_surface,
		static_model,
		entity_model,
		brush_model,
		placed_model,		// added with map_renderer::add_model
		dynamic_mesh,		// added with map_renderer::create_mesh
	};

	// Names one object. `id` is: the surface index ( world_surface ), the static model index ( static_model ), the
	// entity index ( entity_model, brush_model ), or the handle add_model / create_mesh returned.
	struct object_ref
	{
		object_kind kind = object_kind::world_surface;
		int id = -1;

		constexpr bool operator==(const object_ref&) const = default;
	};

	// Placement of a model: game axes ( X forward, Y left, Z up ), angles in degrees as in an entity's "angles"
	// key ( pitch, yaw, roll ), uniform scale ( "modelscale" ).
	struct transform
	{
		float3 origin;
		float3 angles;			// x = pitch, y = yaw, z = roll
		float scale = 1.0f;
	};

	// a box that is not axis aligned: corners are center +- axis[0] * half_size.x +- axis[1] * half_size.y +- axis[2] * half_size.z
	struct oriented_box
	{
		float3 center;
		float3 axis[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };		// unit length, world space
		float3 half_size;
	};

	struct pick_options
	{
		bool world = true;
		bool static_models = true;
		bool entity_models = true;
		bool brush_models = true;
		bool special_ops_brush_models = false;		// as frame_options::draw_special_ops_brush_models
		bool placed_models = true;
		bool meshes = true;
		bool sky = false;							// the sky box ( materials of the "wc_sky" technique sets )
		// false: a triangle is only hit from the side the game draws it from ( by the material's cull mode ), so
		// the hit is what is visible. true: both sides of everything.
		bool backfaces = false;
		float max_distance = 0;						// > 0: ignore hits farther away
	};

	struct pick_result
	{
		object_ref object;
		int entity = -1;			// entity index, -1 when the object is not an entity's
		float distance = 0;			// from the ray origin, world units
		float3 point;				// world space
		float3 normal;				// of the triangle, unit length, towards the ray origin
		int surface = -1;			// world surface index, or the surface of the model ( LOD 0 ) that was hit
		int triangle = -1;			// triangle within that surface ( within the mesh for dynamic_mesh )
		std::string material;		// "" when the surface has none
	};

	// ---- dynamic meshes ( edited brushes ) --------------------------------------------------------------------
	// 36 bytes, laid out as D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1
	struct mesh_vertex
	{
		float3 position;						// world space
		float3 normal;							// unit length, pointing out of the visible side
		std::uint32_t color = 0xFFFFFFFF;		// D3DCOLOR ( ARGB ); multiplies the material where its shaders read the vertex colour
		float uv[2] = {};						// texture coordinates of the colour map ( 1 = one repeat )
	};

	// Triangle list. The winding of a triangle does not matter: each is oriented by its vertex normals ( the side
	// the normals point to is the visible one ). `two_sided` draws both sides.
	struct mesh_data
	{
		const mesh_vertex* vertices = nullptr;
		std::size_t vertex_count = 0;
		const std::uint32_t* indices = nullptr;		// null: vertices are used three by three
		std::size_t index_count = 0;
		std::string material;						// a name of map_renderer::material_names()
		std::uint32_t fallback_color = 0xFFB0B0B0;	// D3DCOLOR used when the material does not exist ( or has no colour map )
		bool two_sided = false;						// draw without back face culling whatever the material says
	};

	// how a mesh ended up being drawn
	enum class mesh_shading
	{
		none,				// no such mesh
		game_material,		// the material's own technique set and shaders ( sun, sun shadow map, ambient from the light grid )
		color_map,			// fixed function: the material's colour map, sun N.L + ambient
		flat_color,			// fixed function: mesh_data::fallback_color, sun N.L + ambient
	};

	// ---- catalogs -------------------------------------------------------------------------------------------
	struct material_info
	{
		std::string name;
		std::string technique_set;
		std::string color_map;					// image name ( images/<name>.iwi ), "" when the material has none
		int width = 0;							// size of the colour map in pixels, 0 when unknown
		int height = 0;
		std::uint32_t surface_type_bits = 0;	// Material::surfaceTypeBits ( 1.1.1.13 materials store no content flags )
		std::uint8_t game_flags = 0;
		std::uint8_t sort_key = 0;
		std::uint8_t camera_region = 0;			// >= 4: the game never draws it ( tool textures )
		bool world_material = false;			// its technique set reads lightmaps ( made for brushes / terrain )
		bool blends = false;					// drawn in the transparent pass
		bool alpha_tested = false;				// has holes: pixels below an alpha threshold are not drawn
		bool writes_depth = true;				// false: sky, decals, most blended materials
		std::string zone;						// the zone that defines it
	};

	// ---- camera math ----------------------------------------------------------------------------------------
	// The matrices render() draws with. 16 floats, row-major, row vectors ( D3DMATRIX layout: v' = v * M,
	// translation in elements 12 .. 14 ). View space is +X right, +Y up, +Z forward; the projection has an
	// infinite far plane: clip.z = view.z - z_near, clip.w = view.z, so depth = 1 - z_near / view.z.
	// ( render() itself draws camera relative: world = translate( -origin ), view = the rotation only. view_matrix
	// is the product of the two, which is what a host needs for world space vertices. )
	void view_matrix(const camera& cam, float out[16]);
	void projection_matrix(const camera& cam, int width, int height, float out[16]);
	// Pixel coordinates are those of Direct3D 9: whole numbers are pixel centres, ( 0, 0 ) is the centre of the
	// top left pixel of the target - a mouse position in client pixels can be passed as it is.
	// The ray through a pixel: origin = the camera origin, direction unit length, world space.
	void screen_ray(const camera& cam, int width, int height, float px, float py, float3& origin, float3& direction);
	// World space point -> pixel coordinates and depth buffer value ( 0 near .. 1 far ). False when the point is
	// not in front of the near plane ( the outputs are then not written ).
	bool project(const camera& cam, int width, int height, const float3& world, float screen_xy[2], float& depth);

	// One drawable thing of the map, for picking / selection in an editor
	struct scene_object
	{
		object_kind kind = object_kind::world_surface;
		int id = 0;				// surface index ( GfxWorld.dpvs.surfaces ), static model index ( smodelDrawInsts ), entity index, or handle ( object_ref )
		int entity = -1;		// index of the entity in the entity string; -1 for world surfaces and static models
		float3 center;			// world space bounds
		float3 half_size;
		std::string name;		// material ( surfaces, meshes ), model name, or "*N"
		bool drawn = false;		// drawn in the last frame rendered with frame_options::track_drawn_objects
		bool hidden = false;	// set_hidden
		bool moved = false;		// has a transform override ( set_transform )

		object_ref ref() const { return { kind, id }; }
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
		// The textures the host is given - the result of render(), material_thumbnail() - are not released at the
		// moment the renderer drops them ( unload(), load_map(), a zone registered again, a target of another size
		// keeps its own rule: see render() ): a gui has usually put them into draw commands of the frame it is
		// building and draws that frame later. They are released here. Call it once per frame at a point where no
		// draw command of an earlier frame is left - before the first window of a frame. ( on_device_lost() and
		// shutdown() release them as well. )
		void release_retired();
		bool loaded() const;
		const std::string& map_name() const;

		// Draws the map. Returns the colour texture of the internal render target, or nullptr when the caller
		// supplied the target, nothing is loaded or the device is lost.
		IDirect3DTexture9* render(const camera& cam, int width, int height, const frame_options& frame = {});

		// Before / after IDirect3DDevice9::Reset. on_device_reset() returns false when a resource could not be
		// recreated ( text in last_error() ).
		void on_device_lost();
		bool on_device_reset();

		// The depth of the last frame drawn with frame_options::float_z as an R32F texture of the frame's size, as
		// the depth pre-pass of the game's shaders writes it ( the opaque world, models and meshes that have a
		// technique for it ). nullptr when there is none. Owned by the renderer; gone with on_device_lost().
		IDirect3DTexture9* depth_texture() const;

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
		// one object of objects() without building the list; false when there is no such object
		bool object(const object_ref& ref, scene_object& out) const;

		// ---- picking ( needs options::keep_pick_geometry ) --------------------------------------------------
		// The nearest triangle under a pixel / along a ray, against the scene as it is placed now: hidden objects
		// are skipped, moved ones are hit where they are. Only what the game draws is hit ( surfaces whose material
		// is never drawn are skipped; LOD 0 of models ). Of surfaces in the same place ( a decal on a wall, layers
		// of terrain ) the one drawn last - the visible one - is hit. Alpha tested materials ( fences, foliage ) are
		// hit on their whole triangles, holes included.
		// `direction` need not be unit length. False when nothing is hit or there is no pick geometry ( dynamic
		// meshes can be picked without keep_pick_geometry ).
		bool pick(const camera& cam, int width, int height, float px, float py, pick_result& out, const pick_options& options = {}) const;
		bool pick_ray(const float3& origin, const float3& direction, pick_result& out, const pick_options& options = {}) const;

		// ---- object edits ( no reload ) -----------------------------------------------------------------------
		// Any object can be hidden. False when there is no such object.
		bool set_hidden(const object_ref& ref, bool hidden);
		bool is_hidden(const object_ref& ref) const;
		void show_all();
		// Transform override of a static_model, entity_model, brush_model or placed_model. The object is drawn,
		// culled, shadowed and picked at the new placement; a model is lit again for the new position ( light
		// grid, primary light, reflection probe - options::keep_light_grid ). A moved static model is no longer
		// culled by cells, only by the frustum. A brush model keeps its baked lightmaps.
		bool set_transform(const object_ref& ref, const transform& placement);
		// the current placement ( for a static model the angles are derived from its stored axis )
		bool get_transform(const object_ref& ref, transform& out) const;
		// back to the placement and lighting of the map ( not for placed_model )
		bool reset_transform(const object_ref& ref);

		// ---- new model instances ----------------------------------------------------------------------------
		// Places an XModel of the map zone or of any zone of the library ( level, shared, options::extra_zones ).
		// Loads the model on first use ( its zone is read again; that takes a moment - see release_zone_cache ).
		// Returns a handle > 0, 0 on failure. Move it with set_transform( { object_kind::placed_model, handle }, .. ).
		int add_model(const std::string& xmodel_name, const transform& placement, std::string* error = nullptr);
		bool remove_model(int handle);
		// bounds of the model in its own space ( LOD 0 vertices ); loads the model on first use
		bool model_bounds(const std::string& xmodel_name, float3& mins, float3& maxs);
		// every XModel name add_model accepts, sorted; cheap ( built once per map )
		const std::vector<std::string>& model_names() const;
		// add_model / model_bounds / create_mesh / material_thumbnail keep the last zone they had to read in memory
		// so that the next call is fast; it is dropped after 120 frames without such a call, or here
		void release_zone_cache();

		// ---- zones at run time ------------------------------------------------------------------------------
		// A fast file anywhere on disk ( a zone of new assets made by codol_assets ) becomes a zone of the library
		// under its file name without ".ff", exactly like a name of options::extra_zones - also for every map that
		// is loaded later. With a map loaded its materials and models can be used at once ( material_names,
		// material_thumbnail, create_mesh, model_names, add_model ): nothing of the map is loaded again.
		// Registering the name again reads the file again ( the zone was rebuilt ). Meshes and placed models that
		// exist keep what they were made with - also when the asset changed or is gone - until the host makes them
		// again ( update_mesh, remove_model + add_model ); what they use stays in memory until unload(). False
		// when the file cannot be read as a zone ( with a map loaded the zone registered before stays in use ).
		bool register_zone(const std::filesystem::path& fastfile, std::string* error = nullptr);
		// the zone is no longer searched and not loaded with the next map; false when it was not registered
		bool unregister_zone(const std::string& name);

		// ---- dynamic meshes ---------------------------------------------------------------------------------
		// Host supplied triangles drawn with a game material ( see mesh_shading and the README for the lighting ).
		// The data is copied. Returns a handle > 0, 0 on failure ( no vertices, an index out of range ).
		int create_mesh(const mesh_data& data, std::string* error = nullptr);
		// replaces vertices, indices, material and fallback colour
		bool update_mesh(int handle, const mesh_data& data, std::string* error = nullptr);
		bool destroy_mesh(int handle);
		bool set_mesh_visible(int handle, bool visible);		// = set_hidden( { dynamic_mesh, handle }, !visible )
		mesh_shading mesh_shading_of(int handle) const;

		// ---- materials --------------------------------------------------------------------------------------
		// every material name of the map zone and the library, sorted; cheap ( built once per map )
		const std::vector<std::string>& material_names() const;
		// Details of one material. The size of the colour map of a streamed image is read from its IWI file on
		// first use ( cached ). False when there is no such material.
		bool material_details(const std::string& name, material_info& out);
		// The colour map of a material as a texture no larger than `max_size` ( the nearest mip level; the whole
		// image when it has no mips ), loaded on first use and cached per name ( the first max_size asked for a
		// name wins ). Owned by the renderer: valid until the release_retired() after unload() / load_map(), until
		// shutdown(), and with the default pool until on_device_lost(). nullptr when the material has no 2D colour
		// map or it cannot be read.
		IDirect3DTexture9* material_thumbnail(const std::string& name, int max_size = 128);

		// ---- selection highlight ----------------------------------------------------------------------------
		// The bounds of an object as placed: the model's box under its placement for models and brush models, the
		// axis aligned bounds for world surfaces and meshes.
		bool object_box(const object_ref& ref, oriented_box& out) const;
		// The triangles of an object as placed, world space, three points each ( appended to `out` ). Needs
		// options::keep_pick_geometry ( dynamic meshes work without it ).
		bool object_triangles(const object_ref& ref, std::vector<float3>& out) const;

	private:
		struct impl;
		std::unique_ptr<impl> impl_;
	};
}
