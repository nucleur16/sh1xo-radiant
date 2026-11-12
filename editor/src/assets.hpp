#pragma once

// The assets of a map project: what Asset Manager + converter + linker_pc are for a CoD4 Radiant user.
//
//   <editor>\mod\<project>\                       default project = the name of the map
//       images\*.png | *.tga | *.dds              source images; the image asset is named after the file
//       materials\*.mtl                           material definitions ( codol_assets README, ".mtl" )
//       models\<name>.model                       a model: its LOD files, collision LOD, material names
//       models\*.xmodel_export                    LOD files; one that no .model names is a model of one LOD
//       anims\<name>.anim                         an animation: its xanim_export, the model it is for, type, animtree
//       anims\*.xanim_export                      one that no .anim names is an animation of that name, without options
//       soundaliases\*.csv                        sound aliases; sound\<file> is what their file column names,
//                                                 soundaliases\*.vfcurve | *.spkrmap curves and speaker maps
//       fx\<folder>\<name>.efx                    an effect, named as loadfx takes it: <folder>/<name>
//       scripts\..                                files the built map takes as they are, in front of the generated ones:
//                                                 maps\mp\<map>.gsc ( the level script ), animtrees\<tree>.atr ..
//       generated\animtrees\<tree>.atr            written by the build: the animtree of every tree the animations name,
//       generated\animlists\map.animlist          and what becomes animlists/<map>.animlist of the map zone
//       zz_<project>_sounds.iwd                   written by the build: the streamed sound files ( for <game>\main )
//       <project>_assets.ff                       the zone built from all of it ( codol_assets, AssetLinker )
//       build_state.txt                           what the last build made of each source
//   <editor>\mod\codol_matlib.txt                 the material library: one scan of the install, shared by all projects
//
// Building runs on a worker thread and compiles everything again into a new zone; the renderer then takes the
// zone as an extra zone without loading the map again ( map_renderer::register_zone ), so a new material is in
// the Materials browser and on brushes at once, a new model in the Models browser.
//
// Animations, sounds and effects are not shown by the editor. File > Build map carries them from the project's
// zone into the map zone ( tools\package_map.ps1, codol_assetc --carry ), with the animtrees, the animlist and the
// files of scripts\, so that the level script can use them.
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
		anim,
		sound,		// a soundaliases .csv with everything it names
		fx,
		script,		// a file of scripts\: not compiled, the built map takes it as it is
	};

	enum class status : int
	{
		fresh,		// "new": never built
		changed,	// built before, the sources are newer
		built,
		failed,
		unused,		// an image no material of the project names: not part of the zone
		raw,		// a script file: nothing to build
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

	struct anim_request
	{
		std::string name;					// empty: the name of the file
		std::filesystem::path file;			// .xanim_export
		std::string model;					// the skeleton: a model of the project, or an .xmodel_export file; empty: none
		std::string type = "relative";		// relative | delta | delta3d
		bool loop = false;
		std::string tree;					// the animtree a script names; empty: the project's ( default_anim_tree )
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

	// copies the xanim_export ( and a skeleton file from outside the project ), writes the .anim and starts a build
	bool import_anim(const anim_request& request, std::string& error);
	std::string default_anim_tree();					// the animtree of animations that name none: the project's name
	std::vector<std::string> project_models();			// names, for the skeleton of an animation
	// for the Preview window: the files of a model / an animation of the project
	struct preview_files
	{
		std::filesystem::path xanim;						// animation: its .xanim_export
		std::filesystem::path mesh;							// the .xmodel_export of LOD 0 ( an animation: of its model ); empty: none
		std::string model;									// animation: its model when that is a model of the project
		std::map<std::string, std::string> materials;		// export material -> material
		bool loop = false;
	};
	bool model_files(const std::string& name, preview_files& out);
	bool anim_files(const std::string& name, preview_files& out);

	// copies a soundaliases .csv and what it names - sound files, curves, speaker maps - from the tree around it
	// ( `root`: the folder that holds sound\ and soundaliases\; empty: above the csv's soundaliases\ ) and builds
	bool import_sounds(const std::filesystem::path& csv, const std::filesystem::path& root, bool overwrite, std::string& error);
	// copies an .efx to fx\<name>.efx and builds; `name` as loadfx takes it ( "misc/my_effect" ), empty: what follows
	// "fx\" in the path of the file, else <project>/<file name>
	bool import_fx(const std::filesystem::path& file, const std::string& name, bool overwrite, std::string& error);
	// scripts\maps\mp\<map>.gsc: made with what the map compiler would generate when it is not there; the path
	bool level_script(std::filesystem::path& file, std::string& error);

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
	// a .gdt the GDT window, a .mtl is copied into the project; an .xanim_export opens "Import animation", a
	// soundaliases .csv and an .efx are imported as they are
	void files_dropped(const std::vector<std::filesystem::path>& files);

	// ---- the map compile -------------------------------------------------------------------------------------
	// what File > Build map hands to tools\package_map.ps1 besides the zone: " -Project <folder>" ( scripts, the
	// generated animtrees / animlist, the sound archive ); empty without a project folder
	std::string package_arguments();
	// what the built map will not get as the project folder has it now: sources that are new, changed or failed
	// since the zone was built. Lines for the user, said before the build; they do not stop it.
	std::vector<std::string> package_problems();
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
	void open_anim_dialog(const std::filesystem::path& file = {});
	void open_sounds_dialog(const std::filesystem::path& file = {});
	void open_fx_dialog(const std::filesystem::path& file = {});
	void close_dialogs();
	void draw_windows();								// asset_windows.cpp
}
