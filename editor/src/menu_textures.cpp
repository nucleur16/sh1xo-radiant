#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"
#include "assets.hpp"
#include "preview.hpp"
#include "ext.hpp"

// The Textures menu: the texture tools as actions ( flip, rotate, fit, natural, the locks, the edit layer ), the
// find / replace window, the lower part of the Surface window and the Materials browser with its filters.
//
// What CoD4 Radiant calls usage and locale are properties of a material's GDT entry; a zone has no such thing. Here
// they are read off the names ( the prefix of a set, words like "wall" or "floor" ), the surface type off the
// material's surface type bits.

namespace editor::textures
{
	namespace
	{
		// ---- the edit layer: 0 material, 1 lightmap, 2 smoothing ( a word of a patch, nothing to shift )
		int g_layer = 0;
		const char* const LAYER_NAMES[] = { "Material", "Lightmap", "Smoothing" };

		void set_layer(const int layer)
		{
			g_layer = ((layer % 3) + 3) % 3;
			get_scene().doc.set_edit_layer(g_layer == 1 ? edit::document::texture_layer::lightmap : edit::document::texture_layer::material);
			game::printf_to_console("texture edit layer: %s\n", LAYER_NAMES[g_layer]);
		}

		edit::document& doc()
		{
			return get_scene().doc;
		}

		bool has_targets()
		{
			return !doc().selection().empty() || !doc().selected_faces().empty();
		}

		// steps of the Surface window, shared with the keys
		float g_shift_h = 8.0f, g_shift_v = 8.0f, g_scale_h = 2.0f, g_scale_v = 2.0f, g_rotate = 15.0f;

		// ---- find / replace
		bool g_show_replace = false;
		std::string g_find, g_replace;
		int g_scope = 0;		// 0 map, 1 selection, 2 selected faces
		std::string g_result;

		void replace(const std::string& from, const std::string& to, const int scope)
		{
			int changed = 0;
			const bool ok = doc().replace_material(from, to, scope != 0, scope == 2, &changed);
			char text[512];

			if (ok)
			{
				std::snprintf(text, sizeof(text), "%d face%s: %s -> %s", changed, changed == 1 ? "" : "s", from.c_str(), to.c_str());
				note_material_used(to);
			}
			else
			{
				std::snprintf(text, sizeof(text), "%s", doc().last_error().c_str());
			}

			g_result = text;
			game::printf_to_console("%s%s\n", ok ? "replace material: " : "^1", text);
		}

		// ---- materials browser
		std::vector<std::string> g_recent;
		std::unordered_set<std::string> g_favourites;
		bool g_lists_loaded = false;

		std::filesystem::path favourites_file()
		{
			return std::filesystem::u8path(utils::fs::get_basepath()) / "codol_editor_materials.cfg";
		}

		void load_lists()
		{
			if (g_lists_loaded) { return; }

			g_lists_loaded = true;
			std::ifstream file(favourites_file(), std::ios::binary);
			std::string line;

			while (file.is_open() && std::getline(file, line))
			{
				while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) { line.pop_back(); }

				if (line.rfind("favourite ", 0) == 0) { g_favourites.insert(line.substr(10)); }
				else if (line.rfind("recent ", 0) == 0) { g_recent.push_back(line.substr(7)); }
			}
		}

		void save_lists()
		{
			std::string out = "// CoDOL Radiant materials browser: favourites and the materials used last\n";
			std::vector<std::string> sorted(g_favourites.begin(), g_favourites.end());
			std::sort(sorted.begin(), sorted.end());

			for (const auto& name : sorted) { out += "favourite " + name + "\n"; }
			for (const auto& name : g_recent) { out += "recent " + name + "\n"; }

			std::ofstream file(favourites_file(), std::ios::binary | std::ios::trunc);
			file << out;
		}

		// the words a name is made of: between '/', '_' and digits
		bool has_word(const std::string& name, const std::string_view word)
		{
			return name.find(word) != std::string::npos;
		}

		struct usage
		{
			const char* label;
			std::initializer_list<const char*> words;
		};

		const usage USAGES[] = {
			{ "Walls", { "wall", "brick", "plaster", "siding", "panel", "stucco" } },
			{ "Floors", { "floor", "tile", "carpet", "pavement", "sidewalk", "linoleum" } },
			{ "Ceilings", { "ceiling" } },
			{ "Roofs", { "roof", "shingle" } },
			{ "Doors and windows", { "door", "window", "glass", "shutter" } },
			{ "Roads", { "road", "asphalt", "street", "curb" } },
			{ "Concrete", { "concrete", "cement" } },
			{ "Metal", { "metal", "rust", "steel", "iron", "pipe", "grate", "fence", "chainlink", "corrugated" } },
			{ "Wood", { "wood", "plank", "plywood", "bark", "crate" } },
			{ "Terrain", { "dirt", "grass", "sand", "mud", "rock", "gravel", "snow", "terrain", "ground", "cliff", "blend" } },
			{ "Foliage", { "foliage", "tree", "leaf", "leaves", "bush", "plant", "ivy" } },
			{ "Decals and signs", { "decal", "stain", "graffiti", "poster", "sign", "trim", "dcl" } },
			{ "Fabric", { "cloth", "fabric", "tarp", "curtain", "rug" } },
			{ "Water", { "water", "puddle", "ocean" } },
			{ "Sky", { "sky" } },
			{ "Tools", { "clip", "caulk", "trigger", "portal", "nodraw", "hint", "skip", "lightgrid", "mantle", "ladder", "tool", "case" } },
			{ "Effects", { "gfx", "fx_", "smoke", "fire", "flare", "glow" } } };

		// Material::surfaceTypeBits: bit n is surface type n + 1 of the engine's table ( 0 is "default": no bit )
		const char* const SURFACE_TYPES[] = {
			"bark", "brick", "carpet", "cloth", "concrete", "dirt", "flesh", "foliage", "glass", "grass", "gravel", "ice", "metal", "mud", "paper",
			"plaster", "rock", "sand", "snow", "water", "wood", "asphalt", "ceramic", "plastic", "rubber", "cushion", "fruit", "painted metal",
			"riot shield", "slush", "opaque glass" };

		// sets: the first part of the name after its folder, when enough materials share it
		struct locale_set
		{
			std::string prefix;
			int count = 0;
		};

		const std::pair<const char*, const char*> LOCALE_TITLES[] = {
			{ "ch", "Chernobyl" }, { "me", "Middle East" }, { "com", "Common" }, { "ru", "Russia" }, { "bog", "Bog" }, { "cs", "Cargoship" },
			{ "icbm", "ICBM" }, { "launch", "Launch facility" }, { "vil", "Village" }, { "hq", "Headquarters" }, { "ac130", "AC-130" },
			{ "afr", "Africa" }, { "pb", "Pipeline / Bloc" }, { "sub", "Submarine" }, { "ny", "New York" }, { "paris", "Paris" },
			{ "berlin", "Berlin" }, { "london", "London" }, { "fav", "Favela" }, { "dc", "Washington" }, { "arcadia", "Arcadia" } };

		std::string locale_of(const std::string& name)
		{
			const auto slash = name.find('/');
			const std::size_t start = slash == std::string::npos ? 0 : slash + 1;
			const auto end = name.find('_', start);
			return end == std::string::npos ? std::string() : name.substr(start, end - start);
		}

		struct browser
		{
			int source = 0;				// 0 all, 1 in use, 2 favourites, 3 recent
			int usage = -1;
			int locale = -1;
			int surface = -1;			// index into SURFACE_TYPES, -2: no surface type
			int thumb = 96;

			// the catalog the lists were made for
			const void* names_data = nullptr;
			std::size_t names_count = 0;
			std::vector<locale_set> locales;

			// surface types: read on demand, a few hundred materials per frame
			std::vector<std::uint32_t> surface_bits;
			std::vector<std::uint8_t> surface_known;
			std::size_t surface_cursor = 0;

			// what is shown
			std::vector<int> filtered;			// indices into the catalog; -1 - n: g_loose[ n ]
			std::vector<std::string> loose;		// names in use that no zone has
			std::string key;
		};

		browser g;

		void rebuild_locales(const std::vector<std::string>& names)
		{
			std::map<std::string, int> counts;
			for (const auto& name : names)
			{
				if (const auto prefix = locale_of(name); !prefix.empty() && prefix.size() <= 12) { counts[prefix]++; }
			}

			g.locales.clear();
			for (const auto& [prefix, count] : counts)
			{
				if (count >= 12) { g.locales.push_back({ prefix, count }); }
			}

			std::sort(g.locales.begin(), g.locales.end(), [](const locale_set& a, const locale_set& b) { return a.count > b.count; });
			if (g.locales.size() > 60) { g.locales.resize(60); }
			std::sort(g.locales.begin(), g.locales.end(), [](const locale_set& a, const locale_set& b) { return a.prefix < b.prefix; });
		}

		std::string locale_label(const locale_set& l)
		{
			for (const auto& [prefix, title] : LOCALE_TITLES)
			{
				if (l.prefix == prefix) { return l.prefix + "_   " + title + "   ( " + std::to_string(l.count) + " )"; }
			}

			return l.prefix + "_   ( " + std::to_string(l.count) + " )";
		}

		// the name of the catalog a material of the map goes by: as it is, or with "wc/" in front
		int catalog_index(const std::vector<std::string>& names, const std::string& material)
		{
			for (const std::string& candidate : { material, "wc/" + material })
			{
				const auto it = std::lower_bound(names.begin(), names.end(), candidate);
				if (it != names.end() && *it == candidate) { return static_cast<int>(it - names.begin()); }
			}

			return -1;
		}

		void tile_menu(const std::string& name)
		{
			auto& st = ui::get();
			const bool favourite = g_favourites.contains(name);

			ImGui::TextDisabled("%s", name.c_str());
			ImGui::Separator();

			if (editor::menu::item("Apply to the selection", nullptr, false, has_targets()))
			{
				doc().set_default_material(name);
				if (ui::report(doc().set_material(name), "Set material")) { note_material_used(name); }
			}

			if (editor::menu::item("Use for new brushes"))
			{
				doc().set_default_material(name);
			}

			if (editor::menu::item("Select all faces using this material", nullptr, false, get_scene().open))
			{
				const int found = doc().select_by_material(name);
				game::printf_to_console("%d faces / patches use %s\n", found, name.c_str());
				ImGui::Toast(found ? ImGuiToastType_Info : ImGuiToastType_Warning, "Materials", (std::to_string(found) + " faces / patches use " + name).c_str(), 3000);
			}

			ImGui::Separator();

			if (editor::menu::item(favourite ? "Remove from the favourites" : "Add to the favourites"))
			{
				if (favourite) { g_favourites.erase(name); } else { g_favourites.insert(name); }
				save_lists();
				g.key.clear();
			}

			if (editor::menu::item("Find / Replace: find this"))
			{
				g_find = name;
				g_show_replace = true;
			}

			if (editor::menu::item("Find / Replace: replace with this"))
			{
				g_replace = name;
				g_show_replace = true;
			}

			if (editor::menu::item("Preview"))
			{
				st.show_preview = true;
				preview::show_material(name);
			}
		}
	}

	void note_material_used(const std::string& name)
	{
		load_lists();

		if (name.empty() || (!g_recent.empty() && g_recent.front() == name))
		{
			return;
		}

		std::erase(g_recent, name);
		g_recent.insert(g_recent.begin(), name);
		if (g_recent.size() > 32) { g_recent.resize(32); }
		save_lists();
		if (g.source == 3) { g.key.clear(); }
	}

	// ---- actions and commands ------------------------------------------------------------------------------------

	void register_actions()
	{
		using namespace actions;
		const auto targets = [] { return has_targets(); };
		const auto step = [](const char* name, const bool ok) { ui::report(ok, name); };

		add("texture.find_replace", "Find / Replace ..", "", [] { g_show_replace = !g_show_replace; }, anywhere, no_map).toggle([] { return g_show_replace; });
		add("window.materials", "Materials", "T", [] { auto& st = ui::get(); st.show_materials = !st.show_materials; }, views, no_map).toggle([] { return ui::get().show_materials; });
		add("texture.show_in_use", "Materials: show in use", "U", []
		{
			ui::get().show_materials = true;
			g.source = g.source == 1 ? 0 : 1;
			g.key.clear();
			ImGui::SetWindowFocus("Materials##window");
		}, views).toggle([] { return g.source == 1; });

		add("texture.flip_x", "Flip texture X", "", [step] { step("Flip texture", doc().flip_texture(0)); }).when(targets);
		add("texture.flip_y", "Flip texture Y", "", [step] { step("Flip texture", doc().flip_texture(1)); }).when(targets);
		add("texture.rotate_90", "Rotate texture 90", "", [step] { step("Rotate texture", doc().rotate_texture(90.0)); }).when(targets);
		add("texture.rotate_90_back", "Rotate texture -90", "", [step] { step("Rotate texture", doc().rotate_texture(-90.0)); }).when(targets);
		add("texture.fit", "Fit texture", "Ctrl+F", [step] { step("Fit texture", doc().fit_texture(1.0, 1.0)); }).when(targets);
		add("texture.natural", "Natural texture ( reset the alignment )", "", [step] { step("Natural texture", doc().natural_texture()); }).when(targets);

		// Radiant's keys, in the 3D view and the 2D views: shift by the grid size, rotate by the angle snap, scale by 5 %.
		// A key that is held is one undo step.
		const auto held = [](const char* name, const std::uint64_t key, const auto& fn)
		{
			auto& d = doc();
			d.begin_transaction(name, key);
			const bool ok = fn(d);
			d.end_transaction();
			ui::report(ok, name);
		};

		add("texture.shift_left", "Shift texture left", "Shift+Left", [held] { held("Shift texture", 0x3D7E0001, [](auto& d) { return d.shift_texture(-std::max(1.0, get_scene().grid), 0.0); }); }, views, repeat).when(targets);
		add("texture.shift_right", "Shift texture right", "Shift+Right", [held] { held("Shift texture", 0x3D7E0001, [](auto& d) { return d.shift_texture(std::max(1.0, get_scene().grid), 0.0); }); }, views, repeat).when(targets);
		add("texture.shift_up", "Shift texture up", "Shift+Up", [held] { held("Shift texture", 0x3D7E0001, [](auto& d) { return d.shift_texture(0.0, std::max(1.0, get_scene().grid)); }); }, views, repeat).when(targets);
		add("texture.shift_down", "Shift texture down", "Shift+Down", [held] { held("Shift texture", 0x3D7E0001, [](auto& d) { return d.shift_texture(0.0, -std::max(1.0, get_scene().grid)); }); }, views, repeat).when(targets);
		add("texture.rotate_ccw", "Rotate texture", "Shift+PageUp", [held] { held("Rotate texture", 0x3D7E0002, [](auto& d) { return d.rotate_texture(get_scene().snap_angle); }); }, views, repeat).when(targets);
		add("texture.rotate_cw", "Rotate texture back", "Shift+PageDown", [held] { held("Rotate texture", 0x3D7E0002, [](auto& d) { return d.rotate_texture(-get_scene().snap_angle); }); }, views, repeat).when(targets);
		add("texture.scale_h_down", "Texture narrower", "Ctrl+Left", [held] { held("Scale texture", 0x3D7E0003, [](auto& d) { return d.scale_texture(1.0 / 1.05, 1.0); }); }, views, repeat).when(targets);
		add("texture.scale_h_up", "Texture wider", "Ctrl+Right", [held] { held("Scale texture", 0x3D7E0003, [](auto& d) { return d.scale_texture(1.05, 1.0); }); }, views, repeat).when(targets);
		add("texture.scale_v_up", "Texture taller", "Ctrl+Up", [held] { held("Scale texture", 0x3D7E0003, [](auto& d) { return d.scale_texture(1.0, 1.05); }); }, views, repeat).when(targets);
		add("texture.scale_v_down", "Texture lower", "Ctrl+Down", [held] { held("Scale texture", 0x3D7E0003, [](auto& d) { return d.scale_texture(1.0, 1.0 / 1.05); }); }, views, repeat).when(targets);

		add("texture.lock_toggle", "Texture lock", "Shift+T", [] { ui::toggle_texture_lock(); }, views).toggle([] { return doc().texture_lock(); });
		add("texture.lock_moves", "Texture lock: moves", "", []
		{
			doc().set_texture_lock(!doc().texture_lock_moves(), doc().texture_lock_rotations());
			game::printf_to_console("texture lock, moves: %s\n", doc().texture_lock_moves() ? "on" : "off");
		}, anywhere, no_map).toggle([] { return doc().texture_lock_moves(); });
		add("texture.lock_rotations", "Texture lock: rotations", "Shift+R", []
		{
			doc().set_texture_lock(doc().texture_lock_moves(), !doc().texture_lock_rotations());
			game::printf_to_console("texture lock, rotations: %s\n", doc().texture_lock_rotations() ? "on" : "off");
		}, views, no_map).toggle([] { return doc().texture_lock_rotations(); });

		add("texture.cycle_layer", "Edit layer: cycle ( material, lightmap, smoothing )", "Shift+L", [] { set_layer(g_layer + 1); }, views, no_map);
		add("texture.layer_material", "Edit layer: material", "", [] { set_layer(0); }, anywhere, no_map).toggle([] { return g_layer == 0; });
		add("texture.layer_lightmap", "Edit layer: lightmap", "", [] { set_layer(1); }, anywhere, no_map).toggle([] { return g_layer == 1; });
		add("texture.layer_smoothing", "Edit layer: smoothing", "", [] { set_layer(2); }, anywhere, no_map).toggle([] { return g_layer == 2; });

		add("texture.select_by_material", "Select all faces with the material of the selection", "", []
		{
			std::string material;
			if (!doc().selected_faces().empty())
			{
				const auto f = doc().selected_faces()[0];
				if (const auto* b = doc().find_brush(f.brush); b && static_cast<std::size_t>(f.face) < b->faces.size()) { material = b->faces[static_cast<std::size_t>(f.face)].texture.material; }
			}

			for (const object_id id : doc().selection())
			{
				if (!material.empty()) { break; }
				if (const auto* b = doc().find_brush(id); b && !b->faces.empty()) { material = b->faces[0].texture.material; }
				else if (const auto* p = doc().find_patch(id)) { material = p->material; }
			}

			if (material.empty()) { material = std::string(doc().default_material()); }
			game::printf_to_console("%d faces / patches use %s\n", doc().select_by_material(material), material.c_str());
		});

		add("texture.reload", "Reload the project's images and materials", "", []
		{
			// the sources of the project are read again and its zone is built: the renderer takes the new zone
			command::execute("asset_rescan");
			command::execute("asset_build");
		});

		// ---- console
		using args_t = const std::vector<std::string>&;
		const auto need_map = []
		{
			if (!get_scene().open) { game::printf_to_console("^1no map is open\n"); }
			return get_scene().open;
		};

		const auto say = [](const bool ok, const char* what)
		{
			if (ok) { game::printf_to_console("%s: ok\n", what); }
			else { game::printf_to_console("^1%s: %s\n", what, get_scene().doc.last_error().c_str()); }
		};

		// tex_replace <from> <to> [map|selection|faces]
		command::register_command("tex_replace", [need_map](args_t args)
		{
			if (args.size() < 3) { game::printf_to_console("usage: tex_replace <material> <new material> [map|selection|faces]\n"); return; }
			if (need_map()) { replace(args[1], args[2], args.size() > 3 && args[3] == "faces" ? 2 : (args.size() > 3 && args[3] == "selection" ? 1 : 0)); }
		});

		command::register_command("tex_select", [need_map](args_t args)
		{
			if (need_map() && args.size() > 1) { game::printf_to_console("%d faces / patches use %s\n", doc().select_by_material(args[1]), args[1].c_str()); }
		});

		command::register_command("tex_flip", [need_map, say](args_t args)
		{
			if (need_map()) { say(doc().flip_texture(args.size() > 1 && args[1] == "y" ? 1 : 0), "flip texture"); }
		});

		command::register_command("tex_natural", [need_map, say](args_t) { if (need_map()) { say(doc().natural_texture(), "natural texture"); } });

		// tex_layer [material|lightmap|smoothing|cycle]
		command::register_command("tex_layer", [](args_t args)
		{
			if (args.size() > 1)
			{
				set_layer(args[1] == "cycle" ? g_layer + 1 : (args[1] == "lightmap" ? 1 : (args[1] == "smoothing" ? 2 : 0)));
			}
			else
			{
				game::printf_to_console("texture edit layer: %s\n", LAYER_NAMES[g_layer]);
			}
		});

		// tex_lightmap <material> [size]: the lightmap layer of the selection
		command::register_command("tex_lightmap", [need_map, say](args_t args)
		{
			if (!need_map() || args.size() < 2) { return; }
			edit::tex_def t;
			t.material = args[1];
			t.size_x = t.size_y = args.size() > 2 ? std::atof(args[2].c_str()) : 16384.0;
			say(doc().set_lightmap(t), "set lightmap");
		});

		command::register_command("tex_smoothing", [need_map, say](args_t args)
		{
			if (need_map()) { say(doc().set_smoothing(args.size() > 1 ? args[1] : std::string()), "set smoothing"); }
		});

		// tex_lock <moves|rotations> [0|1]
		command::register_command("tex_lock", [](args_t args)
		{
			auto& d = doc();
			bool moves = d.texture_lock_moves(), rotations = d.texture_lock_rotations();

			if (args.size() > 1)
			{
				bool& which = args[1] == "rotations" ? rotations : moves;
				which = args.size() > 2 ? args[2] != "0" : !which;
				d.set_texture_lock(moves, rotations);
			}

			game::printf_to_console("texture lock: moves %s, rotations %s\n", d.texture_lock_moves() ? "on" : "off", d.texture_lock_rotations() ? "on" : "off");
		});

		// tex_inuse: the materials of the map with the number of faces
		command::register_command("tex_inuse", [need_map](args_t)
		{
			if (!need_map()) { return; }
			const auto used = doc().materials_in_use();
			for (const auto& [name, count] : used) { game::printf_to_console("  %6d  %s\n", count, name.c_str()); }
			game::printf_to_console("%d materials in use\n", static_cast<int>(used.size()));
		});

		// ed_material_browser <all|inuse|favourites|recent> | usage <n> | locale <prefix> | surface <name> | thumb <px> | favourite <material>
		command::register_command("ed_material_browser", [](args_t args)
		{
			const std::string what = args.size() > 1 ? args[1] : "";
			const std::string value = args.size() > 2 ? args[2] : "";
			load_lists();

			if (what == "all") { g.source = 0; }
			else if (what == "inuse") { g.source = 1; }
			else if (what == "favourites") { g.source = 2; }
			else if (what == "recent") { g.source = 3; }
			else if (what == "usage") { g.usage = value.empty() ? -1 : std::atoi(value.c_str()); }
			else if (what == "thumb") { g.thumb = std::clamp(std::atoi(value.c_str()), 48, 192); }
			else if (what == "favourite" && !value.empty()) { g_favourites.insert(value); save_lists(); }
			else if (what == "locale")
			{
				g.locale = -1;
				for (std::size_t i = 0; i < g.locales.size(); i++) { if (g.locales[i].prefix == value) { g.locale = static_cast<int>(i); } }
			}
			else if (what == "surface")
			{
				g.surface = value == "none" ? -2 : -1;
				for (std::size_t i = 0; i < std::size(SURFACE_TYPES); i++) { if (value == SURFACE_TYPES[i]) { g.surface = static_cast<int>(i); } }
			}
			else { game::printf_to_console("usage: ed_material_browser all|inuse|favourites|recent | usage <n> | locale <prefix> | surface <name|none> | thumb <px> | favourite <material>\n"); }

			g.key.clear();
		});
	}

	// ---- menu ----------------------------------------------------------------------------------------------------

	void draw_menu()
	{
		if (!editor::menu::begin("Textures"))
		{
			return;
		}

		actions::menu_item("view.surface");
		actions::menu_item("window.materials", "Materials browser");
		actions::menu_item("texture.show_in_use", "Show in use");
		actions::menu_item("texture.find_replace");
		ImGui::Separator();

		if (editor::menu::begin("Texture Lock"))
		{
			actions::menu_item("texture.lock_moves", "Moves");
			actions::menu_item("texture.lock_rotations", "Rotations");
			editor::menu::item("Lightmaps", nullptr, false, false);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) { ImGui::SetTooltip("The lightmap of a map is baked by the map compiler from the geometry: there is no alignment to keep."); }
			ImGui::Separator();
			actions::menu_item("texture.lock_toggle", "Both on / off");
			editor::menu::end();
		}

		if (editor::menu::begin("Edit Layer"))
		{
			actions::menu_item("texture.cycle_layer", "Cycle");
			ImGui::Separator();
			actions::menu_item("texture.layer_material", "Material");
			actions::menu_item("texture.layer_lightmap", "Lightmap");
			actions::menu_item("texture.layer_smoothing", "Smoothing");
			editor::menu::end();
		}

		ImGui::Separator();
		actions::menu_item("texture.flip_x");
		actions::menu_item("texture.flip_y");
		actions::menu_item("texture.rotate_90");
		actions::menu_item("texture.rotate_90_back");
		actions::menu_item("texture.fit");
		actions::menu_item("texture.natural");
		actions::menu_item("texture.select_by_material");
		ImGui::Separator();

		if (editor::menu::begin("Usage Filter"))
		{
			if (editor::menu::item("All", nullptr, g.usage < 0)) { g.usage = -1; g.key.clear(); }
			for (int i = 0; i < static_cast<int>(std::size(USAGES)); i++)
			{
				if (editor::menu::item(USAGES[i].label, nullptr, g.usage == i)) { g.usage = i; g.key.clear(); ui::get().show_materials = true; }
			}

			editor::menu::end();
		}

		if (editor::menu::begin("Locale Filter", !g.locales.empty()))
		{
			if (editor::menu::item("All", nullptr, g.locale < 0)) { g.locale = -1; g.key.clear(); }
			for (int i = 0; i < static_cast<int>(g.locales.size()); i++)
			{
				if (editor::menu::item(locale_label(g.locales[static_cast<std::size_t>(i)]).c_str(), nullptr, g.locale == i)) { g.locale = i; g.key.clear(); ui::get().show_materials = true; }
			}

			editor::menu::end();
		}

		if (editor::menu::begin("Surface Type Filter"))
		{
			if (editor::menu::item("All", nullptr, g.surface == -1)) { g.surface = -1; g.key.clear(); }
			if (editor::menu::item("( none )", nullptr, g.surface == -2)) { g.surface = -2; g.key.clear(); ui::get().show_materials = true; }
			for (int i = 0; i < static_cast<int>(std::size(SURFACE_TYPES)); i++)
			{
				if (editor::menu::item(SURFACE_TYPES[i], nullptr, g.surface == i)) { g.surface = i; g.key.clear(); ui::get().show_materials = true; }
			}

			editor::menu::end();
		}

		ImGui::Separator();
		actions::menu_item("texture.reload");

		if (editor::menu::item("New material ..")) { assets::open_material_dialog(); }
		editor::menu::end();
	}

	// ---- find / replace ------------------------------------------------------------------------------------------

	void draw_windows()
	{
		if (!g_show_replace)
		{
			return;
		}

		auto& s = get_scene();
		ImGui::SetNextWindowSize(ImVec2(640.0f, 0.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Find / Replace##window", &g_show_replace, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		const auto row = [&](const char* label, const char* id, std::string& value)
		{
			ImGui::PushID(id);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			ImGui::SameLine(80.0f);
			ImGui::SetNextItemWidth(-190.0f);
			ImGui::InputText("##name", &value);
			ImGui::SameLine();

			if (ImGui::Button("Browser"))
			{
				value = std::string(s.doc.default_material());
			}

			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("The material picked in the Materials window"); }
			ImGui::SameLine();

			if (ImGui::Button("Selection"))
			{
				if (!s.doc.selected_faces().empty())
				{
					const auto f = s.doc.selected_faces()[0];
					if (const auto* b = s.doc.find_brush(f.brush); b && static_cast<std::size_t>(f.face) < b->faces.size()) { value = b->faces[static_cast<std::size_t>(f.face)].texture.material; }
				}
				else if (!s.doc.selection().empty())
				{
					if (const auto* b = s.doc.find_brush(s.doc.selection()[0]); b && !b->faces.empty()) { value = b->faces[0].texture.material; }
					else if (const auto* p = s.doc.find_patch(s.doc.selection()[0])) { value = p->material; }
				}
			}

			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("The material of the first selected face / brush / patch"); }

			if (s.renderer.loaded() && !value.empty())
			{
				auto* thumb = s.renderer.material_thumbnail(value, 96);
				if (!thumb) { thumb = s.renderer.material_thumbnail("wc/" + value, 96); }
				if (thumb) { ImGui::SameLine(); ImGui::Image(thumb, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())); }
			}

			ImGui::PopID();
		};

		row("Find", "find", g_find);
		row("Replace", "replace", g_replace);

		ImGui::Spacing();
		ImGui::RadioButton("Whole map", &g_scope, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Selection", &g_scope, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Selected faces only", &g_scope, 2);
		ImGui::Spacing();

		const bool can = s.open && !g_find.empty() && !g_replace.empty();
		if (!can) { ImGui::BeginDisabled(); }
		if (ImGui::Button("Replace", ImVec2(120.0f, 0.0f))) { replace(g_find, g_replace, g_scope); }
		if (!can) { ImGui::EndDisabled(); }

		ImGui::SameLine();
		const bool can_find = s.open && !g_find.empty();
		if (!can_find) { ImGui::BeginDisabled(); }

		if (ImGui::Button("Select the faces using \"Find\""))
		{
			const int found = s.doc.select_by_material(g_find);
			g_result = std::to_string(found) + " faces / patches use " + g_find;
		}

		if (!can_find) { ImGui::EndDisabled(); }

		if (!g_result.empty())
		{
			ImGui::TextWrapped("%s", g_result.c_str());
		}

		ImGui::TextDisabled("One undo step. A name with \"wc/\" in front and one without name the same material.");
		ImGui::End();
	}

	// ---- Surface window, lower part ------------------------------------------------------------------------------

	void draw_surface_extras()
	{
		auto& d = doc();
		const float button = ImGui::GetFrameHeight() * 1.3f;
		const auto report = [](const bool ok, const char* what) { ui::report(ok, what); };

		ImGui::Separator();

		// ---- which layer the buttons above and below change
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Layer");
		ImGui::SameLine(70.0f);

		ImGui::SetNextItemWidth(-1.0f);

		if (ImGui::BeginCombo("##edit_layer", LAYER_NAMES[g_layer]))
		{
			for (int i = 0; i < 3; i++)
			{
				if (ImGui::Selectable(LAYER_NAMES[i], g_layer == i)) { set_layer(i); }
			}

			ImGui::EndCombo();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("A face has a material with its alignment and a lightmap with its own; a patch a material, a lightmap\nmaterial, two sets of texture coordinates and a smoothing word. %s cycles.", actions::shortcut("texture.cycle_layer") ? actions::shortcut("texture.cycle_layer") : "Edit layer: cycle");
		}

		if (g_layer == 2)
		{
			// the smoothing of patches: one word
			std::string word;
			int patches = 0;

			for (const object_id id : d.selection())
			{
				if (const auto* p = d.find_patch(id))
				{
					if (!patches) { word = p->smoothing; }
					patches++;
				}
			}

			static std::string typed;
			static std::uint64_t typed_for = ~0ull;
			if (typed_for != d.change_counter() + d.selection_counter()) { typed_for = d.change_counter() + d.selection_counter(); typed = word; }

			ImGui::SetNextItemWidth(-90.0f);
			ImGui::InputTextWithHint("##smoothing", "smoothing word of the patches, eg. smoothing_hard", &typed);
			ImGui::SameLine();
			if (!patches) { ImGui::BeginDisabled(); }
			if (ImGui::Button("Set", ImVec2(-1.0f, 0.0f))) { report(d.set_smoothing(typed), "Set smoothing"); }
			if (!patches) { ImGui::EndDisabled(); }
			ImGui::TextDisabled(patches ? "%d patch( es ) selected." : "Brush faces have no smoothing of their own: select patches.", patches);
		}
		else
		{
			// ---- horizontal and vertical apart
			const auto pair = [&](const char* label, const char* id, float& step_value, const char* format, const auto& minus, const auto& plus)
			{
				ImGui::PushID(id);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(label);
				ImGui::SameLine(70.0f);
				if (ImGui::Button("-", ImVec2(button, 0.0f))) { minus(); }
				ImGui::SameLine(0.0f, 2.0f);
				if (ImGui::Button("+", ImVec2(button, 0.0f))) { plus(); }
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputFloat("##step", &step_value, 0.0f, 0.0f, format);
				ImGui::PopID();
			};

			pair("Shift H", "sh", g_shift_h, "%.3g texels", [&] { report(d.shift_texture(-g_shift_h, 0.0), "Shift texture"); }, [&] { report(d.shift_texture(g_shift_h, 0.0), "Shift texture"); });
			pair("Shift V", "sv", g_shift_v, "%.3g texels", [&] { report(d.shift_texture(0.0, -g_shift_v), "Shift texture"); }, [&] { report(d.shift_texture(0.0, g_shift_v), "Shift texture"); });
			pair("Scale H", "ch", g_scale_h, "x %.3g", [&] { if (g_scale_h > 0.0f) { report(d.scale_texture(1.0 / g_scale_h, 1.0), "Scale texture"); } }, [&] { if (g_scale_h > 0.0f) { report(d.scale_texture(g_scale_h, 1.0), "Scale texture"); } });
			pair("Scale V", "cv", g_scale_v, "x %.3g", [&] { if (g_scale_v > 0.0f) { report(d.scale_texture(1.0, 1.0 / g_scale_v), "Scale texture"); } }, [&] { if (g_scale_v > 0.0f) { report(d.scale_texture(1.0, g_scale_v), "Scale texture"); } });
			pair("Rotate", "ro", g_rotate, "%.3g deg", [&] { report(d.rotate_texture(-g_rotate), "Rotate texture"); }, [&] { report(d.rotate_texture(g_rotate), "Rotate texture"); });

			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Flip");
			ImGui::SameLine(70.0f);
			if (ImGui::Button("X##flip", ImVec2(button, 0.0f))) { report(d.flip_texture(0), "Flip texture"); }
			ImGui::SameLine(0.0f, 2.0f);
			if (ImGui::Button("Y##flip", ImVec2(button, 0.0f))) { report(d.flip_texture(1), "Flip texture"); }
			ImGui::SameLine();
			if (ImGui::Button("Rotate 90")) { report(d.rotate_texture(90.0), "Rotate texture"); }
			ImGui::SameLine();
			if (ImGui::Button("Natural")) { report(d.natural_texture(), "Natural texture"); }

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("The alignment of a new face: no shift, no rotation, the texture at scale 0.25, projected along the world axis\nclosest to the face ( the only projection a .map of this game has ). Patches get their coordinates from\ntheir vertex positions the same way.");
			}
		}

		// ---- the lightmap layer as numbers
		if (g_layer == 1)
		{
			std::string material = "lightmap_gray";
			double sample = 16384.0;
			bool found = false;

			for (const auto& f : d.selected_faces())
			{
				if (const auto* b = d.find_brush(f.brush); b && !found && static_cast<std::size_t>(f.face) < b->faces.size())
				{
					material = b->faces[static_cast<std::size_t>(f.face)].lightmap.material;
					sample = b->faces[static_cast<std::size_t>(f.face)].lightmap.size_x;
					found = true;
				}
			}

			for (const object_id id : d.selection())
			{
				if (found) { break; }
				if (const auto* b = d.find_brush(id); b && !b->faces.empty()) { material = b->faces[0].lightmap.material; sample = b->faces[0].lightmap.size_x; found = true; }
				else if (const auto* p = d.find_patch(id)) { material = p->lightmap; found = true; }
			}

			static std::string typed_material;
			static float typed_size = 16384.0f;
			static std::uint64_t typed_for = ~0ull;

			if (typed_for != d.change_counter() + d.selection_counter())
			{
				typed_for = d.change_counter() + d.selection_counter();
				typed_material = material;
				typed_size = static_cast<float>(sample);
			}

			ImGui::SetNextItemWidth(-110.0f);
			ImGui::InputText("lightmap", &typed_material);
			ImGui::SetNextItemWidth(-110.0f);
			ImGui::InputFloat("lmap size", &typed_size, 0.0f, 0.0f, "%.6g");
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("World units one repeat of the lightmap layer covers ( radiant writes 16384 )"); }

			if (ImGui::Button("Set the lightmap layer", ImVec2(-1.0f, 0.0f)))
			{
				edit::tex_def t;
				t.material = typed_material;
				t.size_x = t.size_y = typed_size;
				report(d.set_lightmap(t), "Set lightmap");
			}

			ImGui::TextDisabled("Written to the .map. The map compiler bakes lightmaps from the geometry; it does not read this layer.");
		}

		// ---- the two locks
		ImGui::Separator();
		bool moves = d.texture_lock_moves(), rotations = d.texture_lock_rotations();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Lock");
		ImGui::SameLine(70.0f);
		if (ImGui::Checkbox("moves", &moves)) { d.set_texture_lock(moves, rotations); }
		ImGui::SameLine();
		if (ImGui::Checkbox("rotations", &rotations)) { d.set_texture_lock(moves, rotations); }
		ImGui::SameLine();
		if (ImGui::Button("Find / Replace ..")) { g_show_replace = true; }
	}

	// ---- Materials -----------------------------------------------------------------------------------------------

	void draw_materials_window(std::string& filter, int& group)
	{
		auto& st = ui::get();
		if (!st.show_materials)
		{
			return;
		}

		auto& s = get_scene();
		auto& d = s.doc;
		load_lists();

		ImGui::SetNextWindowSize(ImVec2(560.0f, 600.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Materials##window", &st.show_materials, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (!s.renderer.loaded())
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("Open a map first: the materials of its zones and of the shared zones are listed here.");
			ImGui::PopTextWrapPos();
			ImGui::End();
			return;
		}

		const auto& names = s.renderer.material_names();

		if (g.names_data != names.data() || g.names_count != names.size())
		{
			g.names_data = names.data();
			g.names_count = names.size();
			g.surface_bits.assign(names.size(), 0);
			g.surface_known.assign(names.size(), 0);
			g.surface_cursor = 0;
			g.locale = -1;
			rebuild_locales(names);
			g.key.clear();
		}

		// ---- filters
		static const char* groups[] = { "World ( wc/ )", "Models ( mc/ )", "Everything" };
		static const char* sources[] = { "All", "In use", "Favourites", "Recently used" };
		const float third = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

		ImGui::SetNextItemWidth(third);
		ImGui::Combo("##material_group", &group, groups, 3);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(third);
		ImGui::Combo("##material_source", &g.source, sources, 4);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(third);
		ImGui::InputTextWithHint("##material_filter", "filter", &filter);

		ImGui::SetNextItemWidth(third);
		if (ImGui::BeginCombo("##material_usage", g.usage < 0 ? "Usage: all" : USAGES[g.usage].label))
		{
			if (ImGui::Selectable("Usage: all", g.usage < 0)) { g.usage = -1; }
			for (int i = 0; i < static_cast<int>(std::size(USAGES)); i++)
			{
				if (ImGui::Selectable(USAGES[i].label, g.usage == i)) { g.usage = i; }
			}

			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(third);
		if (ImGui::BeginCombo("##material_locale", g.locale < 0 || g.locale >= static_cast<int>(g.locales.size()) ? "Locale: all" : locale_label(g.locales[static_cast<std::size_t>(g.locale)]).c_str()))
		{
			if (ImGui::Selectable("Locale: all", g.locale < 0)) { g.locale = -1; }
			for (int i = 0; i < static_cast<int>(g.locales.size()); i++)
			{
				if (ImGui::Selectable(locale_label(g.locales[static_cast<std::size_t>(i)]).c_str(), g.locale == i)) { g.locale = i; }
			}

			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(third);
		if (ImGui::BeginCombo("##material_surface", g.surface == -1 ? "Surface: all" : (g.surface == -2 ? "Surface: none" : SURFACE_TYPES[g.surface])))
		{
			if (ImGui::Selectable("Surface: all", g.surface == -1)) { g.surface = -1; }
			if (ImGui::Selectable("Surface: none", g.surface == -2)) { g.surface = -2; }
			for (int i = 0; i < static_cast<int>(std::size(SURFACE_TYPES)); i++)
			{
				if (ImGui::Selectable(SURFACE_TYPES[i], g.surface == i)) { g.surface = i; }
			}

			ImGui::EndCombo();
		}

		// ---- surface types are read while the filter needs them
		bool indexing = false;

		if (g.surface != -1 && g.surface_cursor < names.size())
		{
			const auto start = std::chrono::steady_clock::now();

			while (g.surface_cursor < names.size() && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(6))
			{
				for (int n = 0; n < 32 && g.surface_cursor < names.size(); n++, g.surface_cursor++)
				{
					view::material_info info;
					if (s.renderer.material_details(names[g.surface_cursor], info)) { g.surface_bits[g.surface_cursor] = info.surface_type_bits; }
					g.surface_known[g.surface_cursor] = 1;
				}
			}

			indexing = g.surface_cursor < names.size();
			g.key.clear();
		}

		// ---- the list
		const std::string key = filter + "\x01" + std::to_string(group) + "\x01" + std::to_string(g.source) + "\x01" + std::to_string(g.usage) + "\x01" + std::to_string(g.locale)
			+ "\x01" + std::to_string(g.surface) + "\x01" + std::to_string(g.source == 1 ? d.change_counter() : 0);

		if (g.key != key)
		{
			g.key = key;
			g.filtered.clear();
			g.loose.clear();

			const auto needle = codol::script::to_lower(filter);
			const char* prefix = group == 0 ? "wc/" : group == 1 ? "mc/" : "";
			const std::string locale_prefix = g.locale >= 0 && g.locale < static_cast<int>(g.locales.size()) ? g.locales[static_cast<std::size_t>(g.locale)].prefix : std::string();

			const auto passes = [&](const std::size_t i, const bool with_group)
			{
				const std::string& name = names[i];

				if (with_group && name.rfind(prefix, 0) != 0) { return false; }
				if (!needle.empty() && name.find(needle) == std::string::npos) { return false; }
				if (!locale_prefix.empty() && locale_of(name) != locale_prefix) { return false; }

				if (g.usage >= 0)
				{
					bool any = false;
					for (const char* word : USAGES[g.usage].words) { any |= has_word(name, word); }
					if (!any) { return false; }
				}

				if (g.surface != -1)
				{
					if (!g.surface_known[i]) { return false; }
					if (g.surface == -2 ? g.surface_bits[i] != 0 : !(g.surface_bits[i] & (1u << g.surface))) { return false; }
				}

				return true;
			};

			if (g.source == 0)
			{
				for (std::size_t i = 0; i < names.size(); i++)
				{
					if (passes(i, true)) { g.filtered.push_back(static_cast<int>(i)); }
				}
			}
			else
			{
				// a list of names: of the map, the favourites, the ones used last - whatever folder they are in
				std::vector<std::string> list;

				if (g.source == 1) { for (const auto& [name, count] : d.materials_in_use()) { list.push_back(name); } }
				else if (g.source == 2) { list.assign(g_favourites.begin(), g_favourites.end()); std::sort(list.begin(), list.end()); }
				else { list = g_recent; }

				for (const auto& name : list)
				{
					const int index = catalog_index(names, name);

					if (index >= 0)
					{
						if (passes(static_cast<std::size_t>(index), false)) { g.filtered.push_back(index); }
					}
					else if (needle.empty() || name.find(needle) != std::string::npos)
					{
						// in the map, in no zone the editor has open
						g.filtered.push_back(-1 - static_cast<int>(g.loose.size()));
						g.loose.push_back(name);
					}
				}
			}
		}

		// the material of new brushes is the document's default material
		const std::string current(d.default_material());
		const bool can_apply = has_targets();

		ImGui::TextDisabled("%d materials%s   new brushes:", static_cast<int>(g.filtered.size()), indexing ? "  ( reading surface types .. )" : "");
		ImGui::SameLine();
		ImGui::TextUnformatted(current.c_str());

		if (!can_apply) { ImGui::BeginDisabled(); }

		if (ImGui::Button("Apply to the selection"))
		{
			if (ui::report(d.set_material(current), "Set material")) { note_material_used(current); }
		}

		if (!can_apply) { ImGui::EndDisabled(); }

		ImGui::SameLine();
		ImGui::Checkbox("Surface inspector", &st.show_surface);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		ImGui::SliderInt("##material_thumb", &g.thumb, 48, 192, "%d px");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Size of the pictures"); }
		ImGui::SameLine();
		ImGui::TextDisabled("( ? )");

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Click: the material of new brushes. Double click: also apply it to the selection.\nRight click: favourites, select the faces that use it, find / replace, preview.\n"
				"W: a world material ( its technique set reads lightmaps: made for brushes and terrain ). A star: a favourite.\n"
				"Usage and locale are read off the names, the surface type off the material.");
		}

		// materials of the map's own project ( assets.hpp ); a row of their own: the window is often narrow
		if (ImGui::Button("New material ..")) { assets::open_material_dialog(); }

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("A material of your own from a colour map ( + normal / specular map ): it is built into the project's zone and listed here.\nImage files can also be dropped on the window.");
		}

		ImGui::SameLine();
		if (ImGui::Button("GDT ..")) { assets::open_gdt_dialog(); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Materials of a CoD4 .gdt: list its material entries and convert the ones you pick."); }

		ImGui::SameLine();
		if (ImGui::Button("Assets"))
		{
			st.show_assets = true;
			ImGui::SetWindowFocus("Assets##window");
		}

		ImGui::SameLine();
		if (ImGui::Button("Find / Replace ..")) { g_show_replace = true; }
		ImGui::SameLine();
		if (ImGui::Button("Reload")) { actions::run("texture.reload"); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Read the sources of the project again and build its zone: changed images and materials show up."); }

		if (ImGui::BeginChild("##material_grid", ImVec2(0.0f, 0.0f), true))
		{
			const float image = static_cast<float>(g.thumb), cell = image + 8.0f;
			const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cell));
			const int rows = (static_cast<int>(g.filtered.size()) + columns - 1) / columns;
			const int wanted = g.thumb > 96 ? 128 : 96;

			ImGuiListClipper clipper;
			clipper.Begin(rows, cell + ImGui::GetTextLineHeightWithSpacing());

			while (clipper.Step())
			{
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
				{
					for (int col = 0; col < columns; col++)
					{
						const int index = row * columns + col;
						if (index >= static_cast<int>(g.filtered.size()))
						{
							break;
						}

						const int entry = g.filtered[static_cast<std::size_t>(index)];
						const bool known = entry >= 0;
						const std::string& name = known ? names[static_cast<std::size_t>(entry)] : g.loose[static_cast<std::size_t>(-1 - entry)];

						if (col > 0)
						{
							ImGui::SameLine(static_cast<float>(col) * cell);
						}

						ImGui::PushID(index);
						ImGui::BeginGroup();

						const ImVec2 pos = ImGui::GetCursorScreenPos();
						const bool chosen = name == current;
						auto* thumb = known ? s.renderer.material_thumbnail(name, wanted) : nullptr;
						auto* dl = ImGui::GetWindowDrawList();

						if (thumb)
						{
							ImGui::Image(thumb, ImVec2(image, image), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1),
								chosen ? ImVec4(1.0f, 0.72f, 0.25f, 1.0f) : ImVec4(0.25f, 0.26f, 0.28f, 1.0f));
						}
						else
						{
							ImGui::Dummy(ImVec2(image, image));
							dl->AddRect(pos, ImVec2(pos.x + image, pos.y + image), chosen ? IM_COL32(255, 184, 64, 255) : IM_COL32(70, 72, 76, 255));
							if (!known) { dl->AddText(ImVec2(pos.x + 6.0f, pos.y + image * 0.5f - 8.0f), IM_COL32(245, 150, 110, 255), "not in a zone"); }
						}

						// drag it onto a face in the 3D view
						if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
						{
							ImGui::SetDragDropPayload("CODOL_MATERIAL", name.c_str(), name.size() + 1);
							ImGui::TextUnformatted(name.c_str());
							ImGui::TextDisabled("Drop on a face in the 3D view ( Shift: the whole brush )");
							ImGui::EndDragDropSource();
						}

						if (ImGui::IsItemClicked())
						{
							d.set_default_material(name);

							// the Preview window follows while it is open
							if (st.show_preview && known)
							{
								preview::show_material(name);
							}
						}

						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							d.set_default_material(name);
							if (ui::report(d.set_material(name), "Set material")) { note_material_used(name); }
						}

						if (ImGui::BeginPopupContextItem("##material_menu"))
						{
							tile_menu(name);
							ImGui::EndPopup();
						}

						// only for what is on screen: the details read the image header the first time
						view::material_info info;
						const bool has_info = known && s.renderer.material_details(name, info);

						if (has_info && info.world_material)
						{
							dl->AddRectFilled(ImVec2(pos.x + 2.0f, pos.y + 2.0f), ImVec2(pos.x + 20.0f, pos.y + 20.0f), IM_COL32(0, 0, 0, 170), 2.0f);
							dl->AddText(ImVec2(pos.x + 5.0f, pos.y + 1.0f), IM_COL32(120, 230, 150, 255), "W");
						}

						if (g_favourites.contains(name))
						{
							// a star in the other corner
							const ImVec2 c(pos.x + image - 11.0f, pos.y + 11.0f);
							ImVec2 star[10];
							for (int k = 0; k < 10; k++)
							{
								const float a = -1.5708f + static_cast<float>(k) * 0.62832f, r = (k & 1) ? 3.5f : 8.0f;
								star[k] = ImVec2(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
							}

							dl->AddCircleFilled(c, 10.0f, IM_COL32(0, 0, 0, 170));
							dl->AddPolyline(star, 10, IM_COL32(255, 205, 70, 255), ImDrawFlags_Closed, 1.5f);
						}

						if (ImGui::IsItemHovered())
						{
							if (has_info)
							{
								std::string surface;
								for (int b = 0; b < static_cast<int>(std::size(SURFACE_TYPES)); b++)
								{
									if (info.surface_type_bits & (1u << b)) { surface += (surface.empty() ? "" : ", ") + std::string(SURFACE_TYPES[b]); }
								}

								ImGui::SetTooltip("%s\n%s\n%d x %d\n%s%s%s%s", name.c_str(), info.technique_set.c_str(), info.width, info.height, info.zone.c_str(),
									info.world_material ? "\nworld material" : "", surface.empty() ? "" : "\nsurface: ", surface.c_str());
							}
							else
							{
								ImGui::SetTooltip("%s", name.c_str());
							}
						}

						// the part after the last '/' fits under the image
						const auto slash = name.find_last_of('/');
						const char* label = name.c_str() + (slash == std::string::npos ? 0 : slash + 1);
						ImGui::PushClipRect(pos, ImVec2(pos.x + image - 2.0f, pos.y + cell + ImGui::GetTextLineHeightWithSpacing()), true);
						ImGui::TextUnformatted(label);
						ImGui::PopClipRect();

						ImGui::EndGroup();
						ImGui::PopID();
					}
				}
			}
		}

		ImGui::EndChild();
		ImGui::End();
	}
}
