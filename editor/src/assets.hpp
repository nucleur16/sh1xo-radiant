#pragma once

// The assets of a map project: what Asset Manager + converter + linker_pc are for a CoD4 Radiant user.
//
//   <editor>\mod\<project>\                       default project = the name of the map
//       images\*.png | *.tga | *.dds              source images; the image asset is named after the file
//       materials\*.mtl                           material definitions ( codol_assets README, ".mtl" )
//       models\<name>.model                       a model: its LOD files, collision LOD, material names
//       models\*.xmodel_export                    LOD files; one that no .model names is a model of one LOD
//       <project>_assets.ff                       the zone built from all of it ( codol_assets, AssetLinker )
//       build_state.txt                           what the last build made of each source
//   <editor>\mod\codol_matlib.txt                 the material library: one scan of the install, shared by all projects
//
// Building runs on a worker thread and compiles everything again into a new zone; the renderer then takes the
// zone as an extra zone without loading the map again ( map_renderer::register_zone ), so a new material is in
// the Materials browser and on brushes at once, a new model in the Models browser.
//
// Every action is a function here; the windows ( asset_windows.cpp ) and the console commands call the same ones.

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace editor::assets
{
	enum class kind : int
	{
		image,
		material,
		model,
	};

	enum class status : int
	{
		fresh,		// "new": never built
		changed,	// built before, the sources are newer
		built,
		failed,
		unused,		// an image no material of the project names: not part of the zone
	};

	struct item
	{
		kind type = kind::image;
		std::string name;					// the asset: image name, material name ( "wc/x" ), model name
		std::filesystem::path file;			// the source: image file, .mtl, .model or .xmodel_export
		status state = status::fresh;
		std::string message;				// failed: why
		std::string detail;					// built: what it became
		std::vector<std::string> notes;		// what the compiler assumed
		std::string signature;				// sizes and times of the sources it was read from
	};

	// what "New material" can make; the technique set follows from the type and the maps ( codol_assets techset_for )
	struct material_kind
	{
		const char* id;			// console name
		const char* label;
		const char* prefix;		// "wc/", "mc/", ""
		const char* type;		// MaterialDef::type
		const char* extra;		// further lines of the definition
	};

	const std::vector<material_kind>& material_kinds();
	const std::vector<std::string>& surface_types();

	struct material_request
	{
		std::string name;					// without the prefix
		std::string kind = "world";
		std::filesystem::path color, normal, specular;
		std::string surface_type;			// "" = none
		bool overwrite = false;
	};

	struct model_lod
	{
		std::filesystem::path file;
		float distance = 1000000.0f;
	};

	struct model_request
	{
		std::string name;
		std::vector<model_lod> lods;
		int collision_lod = -1;
		std::map<std::string, std::string> materials;	// export material -> material asset
		bool overwrite = false;
	};

	// one material entry of a CoD4 .gdt
	struct gdt_entry
	{
		std::string name;									// the material it becomes
		std::string type;
		std::vector<std::pair<std::string, std::string>> images;	// sampler -> source file as the GDT names it
		std::vector<std::filesystem::path> found;			// the file per image, empty when it was not found
		std::string text;									// the definition as .mtl text
	};

	// ---- project ---------------------------------------------------------------------------------------------
	const std::string& project();						// "" while no map is open
	void set_project(const std::string& name);			// "" = the name of the map
	// a map is about to be loaded by the renderer: its project is chosen and the project's zone registered, so
	// that the zone is one of the map's extra zones from the first frame on
	void map_opening(const std::string& map);
	std::filesystem::path project_dir();
	std::filesystem::path zone_file();					// <project dir>\<project>_assets.ff
	std::string zone_name();
	bool zone_exists();

	const std::vector<item>& items();
	void rescan();
	const item* find(kind type, const std::string& name);

	// ---- actions: each returns false with a text when nothing was done -----------------------------------------
	// copies the images into the project, writes the .mtl and starts a build
	bool new_material(const material_request& request, std::string& error, std::string* made = nullptr);
	// copies the LOD files, writes the .model and starts a build
	bool import_model(const model_request& request, std::string& error);
	// the materials of a LOD file ( for the mapping table ), with what each would become by default
	bool read_export_materials(const std::filesystem::path& file, std::vector<std::string>& out, std::string& error);
	std::string default_material_for(const std::string& export_name);
	bool material_exists(const std::string& name);		// in the project or in the zones of the open map

	// `root`: where the image files are looked for; empty: the folder of the .gdt and the folders above it
	bool read_gdt(const std::filesystem::path& file, const std::filesystem::path& root, std::vector<gdt_entry>& out, std::string& error);
	// writes the .mtl of each entry and copies its images; returns how many were converted, `log` says what happened
	int convert_gdt(const std::vector<gdt_entry>& entries, bool overwrite, std::vector<std::string>& log);

	// starts a build of the whole project on the worker thread; false when one is running or there is nothing to do
	bool build(std::string& error);
	bool busy();
	const std::string& build_step();					// what the worker is doing right now
	const std::vector<std::string>& build_log();		// lines of the last build

	// files dropped on the window ( or named by asset_drop ): images open "New material", exports "Import model",
	// a .gdt the GDT window, a .mtl is copied into the project
	void files_dropped(const std::vector<std::filesystem::path>& files);

	// ---- the map compile -------------------------------------------------------------------------------------
	// " --donor <zone>" for codol_mapc when the project has a zone
	std::string compile_arguments();
	// What of the project will not be in the compiled map, before the compiler runs: lines for the user.
	std::vector<std::string> compile_precheck();
	void begin_compile();								// runs the precheck and keeps its lines for compile_finished
	// after the compiler ran: its warnings about faces and models, and project models it did not take
	void compile_finished(bool ok, const std::string& compiler_output);
	const std::vector<std::string>& compile_problems();	// of the last compile ( or precheck )
	// new faces with this world material collide but are not drawn by the incremental compiler
	bool undrawn_in_game(const std::string& material);

	// ---- gui -------------------------------------------------------------------------------------------------
	void frame();										// follows the open map, draws the windows
	void register_commands();
	void open_material_dialog(const std::vector<std::filesystem::path>& images = {});
	void open_model_dialog(const std::vector<std::filesystem::path>& lods = {});
	void open_gdt_dialog(const std::filesystem::path& file = {});
	void close_dialogs();
	void draw_windows();								// asset_windows.cpp
}
