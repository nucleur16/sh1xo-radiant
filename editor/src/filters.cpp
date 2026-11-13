#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "menu_ext.hpp"

// The Filter window: what is not shown ( by brush contents and materials, patches, entity groups ) and what is
// shown but not picked ( radiant's "don't select .." toolbar buttons ). The filter itself lives in the document
// ( codol_edit view_filter / select_filter ): a filtered object is not drawn by any view, not picked, not part of
// "select all". It is view state - nothing of it is saved with the map.

namespace editor::ui::ext
{
	namespace
	{
		edit::view_filter g_filter;
		edit::select_filter g_select;
		bool g_dirty = true;
		std::uint64_t g_serial = ~0ull;
		char g_class_input[96] = {};

		struct toggle
		{
			const char* name;		// ed_filter <name>
			const char* label;
			bool edit::view_filter::* field;
			const char* tip;
		};

		const toggle BRUSHES[] = {
			{ "structural", "Structural", &edit::view_filter::structural, "World brushes that are not detail and carry no tool material." },
			{ "detail", "Detail", &edit::view_filter::detail, "Brushes with \"contents detail;\"." },
			{ "clip", "Clip", &edit::view_filter::clip, "Brushes with a clip* material ( clip, clip_player, clip_weap .. )." },
			{ "weaponclip", "Weapon clip", &edit::view_filter::weapon_clip, "\"contents weaponClip;\" and clip_weap* brushes." },
			{ "noncolliding", "Non-colliding", &edit::view_filter::non_colliding, "Brushes with \"contents nonColliding;\"." },
			{ "caulk", "Caulk", &edit::view_filter::caulk, "Brushes whose every face is caulk." },
			{ "triggers", "Triggers", &edit::view_filter::triggers, "Brushes with a trigger material and trigger_* entities." },
			{ "portals", "Portals", &edit::view_filter::portals, "Brushes with a portal* material." },
			{ "hints", "Hint / skip", &edit::view_filter::hints, nullptr },
			{ "sky", "Sky", &edit::view_filter::sky, "Brushes with a sky_* material." },
			{ "curves", "Curves", &edit::view_filter::curves, "Curve patches." },
			{ "terrain", "Terrain", &edit::view_filter::terrain, "Terrain patches ( meshes )." },
			{ "world", "All of the world", &edit::view_filter::world, "Every brush and patch of the worldspawn." },
		};

		const toggle ENTITIES[] = {
			{ "entities", "All entities", &edit::view_filter::entities, "Everything but the worldspawn." },
			{ "models", "Models", &edit::view_filter::models, "misc_model, script_model: entities with an xmodel." },
			{ "lights", "Lights", &edit::view_filter::lights, "light, light_spot, light_omni." },
			{ "pathnodes", "Path nodes", &edit::view_filter::pathnodes, "node_pathnode, node_cover_* .." },
			{ "spawns", "Spawns", &edit::view_filter::spawns, "mp_*_spawn*, mp_global_intermission, info_player_start." },
			{ "brushents", "Brush entities", &edit::view_filter::brush_entities, "script_brushmodel and every other entity made of brushes." },
			{ "script", "Script origins / structs", &edit::view_filter::script, "script_origin, script_struct .. ( not models, not brush models )." },
			{ "info", "Info", &edit::view_filter::info, "info_volume, info_notnull .." },
			{ "prefabs", "Prefabs", &edit::view_filter::prefabs, "misc_prefab." },
			{ "other", "Other point entities", &edit::view_filter::other, "Point entities of no group above." },
		};

		struct select_toggle
		{
			const char* name;		// ed_noselect <name>
			const char* label;
			bool edit::select_filter::* field;
		};

		const select_toggle SELECT[] = {
			{ "curves", "Don't select curves / terrain", &edit::select_filter::curves },
			{ "entities", "Don't select entities", &edit::select_filter::entities },
			{ "sky", "Don't select sky", &edit::select_filter::sky },
			{ "models", "Don't select models", &edit::select_filter::models },
			{ "brushes", "Don't select world brushes", &edit::select_filter::brushes },
		};

		void draw_toggles(const toggle* list, const std::size_t count)
		{
			for (std::size_t i = 0; i < count; i++)
			{
				bool shown = !(g_filter.*list[i].field);
				if (ImGui::Checkbox(list[i].label, &shown))
				{
					g_filter.*list[i].field = !shown;
					g_dirty = true;
				}

				if (list[i].tip && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", list[i].tip);
				}
			}
		}

		void print_state()
		{
			std::string hidden, locked;

			for (const auto& t : BRUSHES) { if (g_filter.*t.field) { hidden += std::string(" ") + t.name; } }
			for (const auto& t : ENTITIES) { if (g_filter.*t.field) { hidden += std::string(" ") + t.name; } }
			for (const auto& c : g_filter.classes) { hidden += " class:" + c; }
			for (const auto& t : SELECT) { if (g_select.*t.field) { locked += std::string(" ") + t.name; } }

			auto& doc = get_scene().doc;
			std::size_t visible = 0, total = 0;

			for (const object_id ent : doc.entities())
			{
				const auto* e = doc.find_entity(ent);
				if (e->children.empty())
				{
					if (ent != doc.worldspawn()) { total++; visible += doc.is_visible(ent) ? 1 : 0; }
					continue;
				}

				for (const object_id child : e->children) { total++; visible += doc.is_visible(child) ? 1 : 0; }
			}

			game::printf_to_console("filter: hidden:%s; not selectable:%s; %d of %d objects shown\n", hidden.empty() ? " nothing" : hidden.c_str(),
				locked.empty() ? " nothing" : locked.c_str(), static_cast<int>(visible), static_cast<int>(total));
		}
	}

	void apply_filters()
	{
		auto& s = get_scene();

		if (g_dirty || g_serial != s.open_serial)
		{
			g_dirty = false;
			g_serial = s.open_serial;
			s.doc.set_view_filter(g_filter);
			s.doc.set_select_filter(g_select);
		}
	}

	void cycle_model_filter()
	{
		g_filter.models = !g_filter.models;
		g_dirty = true;
		apply_filters();
		game::printf_to_console("models: %s\n", g_filter.models ? "hidden" : "shown");
	}

	void draw_filter_window()
	{
		auto& st = get();
		if (!st.show_filter)
		{
			return;
		}

		ImGui::SetNextWindowPos(ImVec2(70.0f, 90.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(330.0f, 560.0f), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Filter##window", &st.show_filter, ImGuiWindowFlags_NoCollapse))
		{
			if (ImGui::Button("Show everything"))
			{
				g_filter = {};
				g_dirty = true;
			}

			ImGui::SameLine();
			ImGui::TextDisabled("a tick = shown");

			if (ImGui::CollapsingHeader("Brushes and patches", ImGuiTreeNodeFlags_DefaultOpen))
			{
				draw_toggles(BRUSHES, std::size(BRUSHES));
			}

			if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen))
			{
				draw_toggles(ENTITIES, std::size(ENTITIES));

				ImGui::Spacing();
				ImGui::TextDisabled("Hide a class ( * for any text ):");
				ImGui::SetNextItemWidth(-70.0f);
				const bool enter = ImGui::InputTextWithHint("##filter_class", "trigger_hurt, node_*", g_class_input, sizeof(g_class_input), ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::SameLine();

				if ((ImGui::Button("Hide") || enter) && g_class_input[0])
				{
					g_filter.classes.emplace_back(g_class_input);
					g_class_input[0] = 0;
					g_dirty = true;
				}

				for (std::size_t i = 0; i < g_filter.classes.size(); i++)
				{
					ImGui::PushID(static_cast<int>(i));
					if (ImGui::SmallButton("x"))
					{
						g_filter.classes.erase(g_filter.classes.begin() + static_cast<std::ptrdiff_t>(i));
						g_dirty = true;
						ImGui::PopID();
						break;
					}

					ImGui::SameLine();
					ImGui::TextUnformatted(g_filter.classes[i].c_str());
					ImGui::PopID();
				}
			}

			if (ImGui::CollapsingHeader("Selecting", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const auto& t : SELECT)
				{
					if (ImGui::Checkbox(t.label, &(g_select.*t.field)))
					{
						g_dirty = true;
					}
				}
			}
		}

		ImGui::End();
	}

	void register_filter_commands()
	{
		// ed_filter                         what is hidden now
		// ed_filter <name> [0|1]            hide ( 1, default: toggle ) a group: structural detail clip weaponclip noncolliding
		//                                   caulk triggers portals hints sky curves terrain world entities models lights
		//                                   pathnodes spawns brushents script info prefabs other
		// ed_filter class <pattern> [0|1]   hide a classname ( * = any text )
		// ed_filter none                    show everything
		// ed_filter window                  the Filter window
		command::register_command("ed_filter", [](args_t args)
		{
			if (args.size() > 1 && args[1] == "window")
			{
				get().show_filter = !get().show_filter;
				return;
			}

			if (args.size() > 1 && args[1] == "none")
			{
				g_filter = {};
			}
			else if (args.size() > 2 && args[1] == "class")
			{
				const bool on = number(args, 3, 1.0) != 0.0;
				std::erase(g_filter.classes, args[2]);
				if (on) { g_filter.classes.push_back(args[2]); }
			}
			else if (args.size() > 1)
			{
				bool found = false;

				const auto apply = [&](const toggle& t)
				{
					if (args[1] == t.name)
					{
						g_filter.*t.field = args.size() > 2 ? number(args, 2) != 0.0 : !(g_filter.*t.field);
						found = true;
					}
				};

				for (const auto& t : BRUSHES) { apply(t); }
				for (const auto& t : ENTITIES) { apply(t); }

				if (!found)
				{
					game::printf_to_console("^1ed_filter: no group \"%s\". Groups:", args[1].c_str());
					std::string names;
					for (const auto& t : BRUSHES) { names += std::string(" ") + t.name; }
					for (const auto& t : ENTITIES) { names += std::string(" ") + t.name; }
					game::printf_to_console("%s; also: class <pattern>, none, window\n", names.c_str());
					return;
				}
			}

			g_dirty = true;
			apply_filters();
			print_state();
		});

		// ed_noselect [curves|entities|sky|models|brushes] [0|1]: shown, but a click goes through them
		command::register_command("ed_noselect", [](args_t args)
		{
			if (args.size() > 1 && args[1] == "none")
			{
				g_select = {};
			}
			else if (args.size() > 1)
			{
				bool found = false;
				for (const auto& t : SELECT)
				{
					if (args[1] == t.name)
					{
						g_select.*t.field = args.size() > 2 ? number(args, 2) != 0.0 : !(g_select.*t.field);
						found = true;
					}
				}

				if (!found)
				{
					game::printf_to_console("^1usage: ed_noselect <curves|entities|sky|models|brushes|none> [0|1]\n");
					return;
				}
			}

			g_dirty = true;
			apply_filters();
			print_state();
		});
	}
}
