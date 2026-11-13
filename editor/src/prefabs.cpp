#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "menu_ext.hpp"

// Prefabs. A misc_prefab entity names another .map ( "model" "prefabs/crate.map", relative to map_source ) and
// stands for its contents at the entity's origin and angles.
//
//   drawing     every prefab entity of the map gets an instance: the prefab's file ( with the prefabs inside it )
//               is expanded into a small document of its own at the entity's place; its brushes, patches and
//               entity boxes are lines in the 2D and 3D views, its brushes and models are drawn solid by the
//               renderer. The box of the entity is the box of that content, so a click on it picks the entity.
//   editing     Enter Prefab opens the prefab's file as the map ( the map is saved first and remembered ), Leave
//               Prefab saves it and goes back. Stamp replaces the entity by its content, Convert makes a prefab
//               file out of the selection. The files are plain .map files: nothing is CoD4's.
//   compiling   codol_bsp and codol_mapc expand prefabs when they read the map ( codol_edit expand_prefabs ).

namespace editor::ui::ext
{
	namespace
	{
		constexpr DWORD CONTENT_COLOR = 0xFF38C8A8;		// D3DCOLOR of the lines of a prefab's content
		constexpr DWORD MISSING_COLOR = 0xFFFF5050;

		struct file_entry
		{
			std::filesystem::path path;
			std::filesystem::file_time_type time{};
			std::string text;
			std::uint64_t generation = 0;	// changes when the file was read again
		};

		std::unordered_map<std::string, file_entry> g_files;	// by "model" as written
		std::uint64_t g_file_generation = 1;
		double g_next_file_check = 0.0;

		struct instance
		{
			std::uint64_t revision = 0;
			std::uint64_t generation = 0;		// g_file_generation the geometry was built with
			std::string error;
			std::vector<float3> points;			// two per line, world space
			edit::bounds box;					// world space, of the content
			edit::vec3 origin;
			int brushes = 0, patches = 0, entities = 0;

			// what the renderer draws
			std::unique_ptr<edit::document> content;	// the expanded prefab, until its meshes are made
			std::vector<int> meshes, models;
			bool drawn = false;					// meshes / models were made for this geometry
			bool visible = true;
			std::uint64_t stamp = 0;
		};

		std::unordered_map<object_id, instance> g_instances;
		std::vector<line_vertex> g_lines;
		std::uint64_t g_open_serial = ~0ull, g_change = ~0ull, g_visibility = ~0ull, g_stamp = 0;
		bool g_shown = true;
		int g_mesh_total = 0;

		struct level
		{
			std::filesystem::path file;		// the map the prefab was entered from
			std::filesystem::path prefab;	// the file that was opened
			view::camera cam;
			std::string model;
			edit::vec3 origin;
		};

		std::vector<level> g_stack;

		std::vector<std::filesystem::path> roots()
		{
			std::vector<std::filesystem::path> out{ map_source_dir() };
			auto& s = get_scene();

			// inside a prefab the map is prefabs\x.map: the folders above it are searched too
			for (auto& root : edit::prefab_roots_for(s.map_file.empty() ? map_source_dir() / "x.map" : s.map_file))
			{
				out.push_back(std::move(root));
			}

			return out;
		}

		bool load_prefab_text(const std::string_view model, std::string& text, std::string& error)
		{
			const std::string key(model);
			auto& entry = g_files[key];
			std::error_code ec;

			if (entry.path.empty() || !std::filesystem::is_regular_file(entry.path, ec))
			{
				const auto all = roots();
				entry.path = edit::resolve_prefab(model, all);
				entry.text.clear();
				entry.time = {};
			}

			if (entry.path.empty())
			{
				error = "the file was not found below map_source";
				return false;
			}

			const auto time = std::filesystem::last_write_time(entry.path, ec);
			if (entry.text.empty() || time != entry.time)
			{
				if (!read_text(entry.path, entry.text))
				{
					error = "the file cannot be read";
					return false;
				}

				entry.time = time;
				entry.generation = ++g_file_generation;
			}

			text = entry.text;
			return true;
		}

		// once a second: a prefab file that was saved ( by this editor inside the prefab, or by another one ) is read again
		bool files_changed()
		{
			if (ImGui::GetTime() < g_next_file_check)
			{
				return false;
			}

			g_next_file_check = ImGui::GetTime() + 1.0;
			bool changed = false;
			std::error_code ec;

			for (auto& [model, entry] : g_files)
			{
				if (entry.path.empty())
				{
					// a file that was missing may be there now
					const auto all = roots();
					if (!edit::resolve_prefab(model, all).empty())
					{
						g_file_generation++;
						changed = true;
					}

					continue;
				}

				if (std::filesystem::last_write_time(entry.path, ec) != entry.time)
				{
					entry.text.clear();
					g_file_generation++;
					changed = true;
				}
			}

			return changed;
		}

		void add_line(std::vector<float3>& out, const edit::vec3& a, const edit::vec3& b)
		{
			out.push_back({ static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z) });
			out.push_back({ static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z) });
		}

		void add_box_lines(std::vector<float3>& out, const edit::bounds& b)
		{
			const auto corner = [&](const int i) { return edit::vec3{ (i & 1) ? b.maxs.x : b.mins.x, (i & 2) ? b.maxs.y : b.mins.y, (i & 4) ? b.maxs.z : b.mins.z }; };
			static const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };

			for (const auto& e : edges)
			{
				add_line(out, corner(e[0]), corner(e[1]));
			}
		}

		void destroy_drawn(instance& inst)
		{
			auto& renderer = get_scene().renderer;

			for (const int handle : inst.meshes) { renderer.destroy_mesh(handle); }
			for (const int handle : inst.models) { renderer.remove_model(handle); }

			g_mesh_total -= static_cast<int>(inst.meshes.size());
			inst.meshes.clear();
			inst.models.clear();
			inst.drawn = false;
		}

		// the content of the prefab of `e`, in the world: lines, box, and the document the meshes are made from
		void build_geometry(const edit::entity& e, instance& inst)
		{
			inst.revision = e.revision;
			inst.error.clear();
			inst.points.clear();
			inst.box = {};
			inst.origin = e.origin();
			inst.brushes = inst.patches = inst.entities = 0;
			inst.content.reset();

			auto content = std::make_unique<edit::document>();

			// uv of the brushes need the size of the colour maps
			content->set_texture_size_callback([](const std::string_view material, int& w, int& h)
			{
				static std::unordered_map<std::string, std::pair<int, int>> sizes;
				const std::string name(material);

				auto it = sizes.find(name);
				if (it == sizes.end())
				{
					std::pair<int, int> size{ 0, 0 };
					auto& renderer = get_scene().renderer;

					if (renderer.loaded())
					{
						view::material_info info;
						if ((renderer.material_details(name, info) || renderer.material_details("wc/" + name, info)) && info.width > 0 && info.height > 0)
						{
							size = { info.width, info.height };
						}
					}
					else
					{
						return false;	// not cached: ask again once a map is shown
					}

					it = sizes.emplace(name, size).first;
				}

				w = it->second.first;
				h = it->second.second;
				return w > 0;
			});

			std::vector<std::pair<std::string, std::string>> pairs;
			for (const auto& [key, value] : e.pairs)
			{
				if (key == "model" || key == "angles" || key == "angle")
				{
					pairs.emplace_back(key, value);
				}
			}

			content->create_point_entity("misc_prefab", e.origin(), pairs);

			edit::prefab_result result;
			const bool expanded = content->expand_prefabs(load_prefab_text, result);
			// reading a file for the first time moves the generation on: taken after the files were read
			inst.generation = g_file_generation;

			if (!expanded)
			{
				inst.error = result.warnings.empty() ? "the prefab is empty" : result.warnings.front();
				return;
			}

			if (!result.warnings.empty())
			{
				inst.error = result.warnings.front();
			}

			std::vector<edit::vec3> grid;

			for (const object_id ent : content->entities())
			{
				const auto* entity = content->find_entity(ent);

				if (entity->children.empty())
				{
					if (ent != content->worldspawn())
					{
						const auto box = content->object_bounds(ent);
						add_box_lines(inst.points, box);
						inst.box.add(box);
						inst.entities++;
					}

					continue;
				}

				for (const object_id child : entity->children)
				{
					if (const auto* geo = content->geometry(child))
					{
						for (const auto& loop : geo->face_vertices)
						{
							for (std::size_t i = 0; i < loop.size(); i++)
							{
								const int a = loop[i], b = loop[(i + 1) % loop.size()];
								if (a < b)	// every edge is in two faces
								{
									add_line(inst.points, geo->vertices[static_cast<std::size_t>(a)], geo->vertices[static_cast<std::size_t>(b)]);
								}
							}
						}

						inst.box.add(geo->box);
						inst.brushes++;
					}
					else if (const auto* patch = content->find_patch(child))
					{
						int columns = 0, rows = 0;
						edit::tessellate_patch(*patch, 2, grid, columns, rows);

						for (int c = 0; c < columns; c++)
						{
							for (int r = 0; r < rows; r++)
							{
								const auto& p = grid[static_cast<std::size_t>(c) * rows + r];
								if (r + 1 < rows) { add_line(inst.points, p, grid[static_cast<std::size_t>(c) * rows + r + 1]); }
								if (c + 1 < columns) { add_line(inst.points, p, grid[static_cast<std::size_t>(c + 1) * rows + r]); }
								inst.box.add(p);
							}
						}

						inst.patches++;
					}
				}
			}

			inst.content = std::move(content);
		}

		std::uint32_t fallback_color(const std::string_view material)
		{
			std::uint32_t h = 2166136261u;
			for (const char c : material)
			{
				h = (h ^ static_cast<std::uint8_t>(c)) * 16777619u;
			}

			const std::uint32_t r = 110 + (h & 0x3F), g = 110 + ((h >> 6) & 0x3F), b = 110 + ((h >> 12) & 0x3F);
			return 0xFF000000u | (r << 16) | (g << 8) | b;
		}

		// brushes and patches of the content as meshes of the renderer ( one per material ), its models as models
		void build_drawn(instance& inst)
		{
			inst.drawn = true;

			if (!inst.content)
			{
				return;
			}

			auto& s = get_scene();
			auto& renderer = s.renderer;
			const auto& names = renderer.material_names();
			const auto& doc = *inst.content;

			struct bucket
			{
				std::vector<view::mesh_vertex> vertices;
				std::vector<std::uint32_t> indices;
				bool two_sided = false;
			};

			std::map<std::string, bucket> buckets;
			edit::mesh mesh;

			for (const object_id ent : doc.entities())
			{
				const auto* entity = doc.find_entity(ent);

				if (entity->children.empty())
				{
					const auto model = entity->value("model");
					if (ent != doc.worldspawn() && !model.empty() && model[0] != '*' && model[0] != '?' && !edit::document::is_prefab(*entity))
					{
						const auto o = entity->origin(), a = entity->angles();
						view::transform placement;
						placement.origin = { static_cast<float>(o.x), static_cast<float>(o.y), static_cast<float>(o.z) };
						placement.angles = { static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z) };
						placement.scale = static_cast<float>(scene::model_scale(*entity));

						if (const int handle = renderer.add_model(std::string(model), placement))
						{
							inst.models.push_back(handle);
						}
					}

					continue;
				}

				for (const object_id child : entity->children)
				{
					if (!doc.build_mesh(child, mesh, 4) || mesh.indices.empty())
					{
						continue;
					}

					const bool is_patch = doc.kind(child) == edit::object_kind::patch;

					for (const auto& part : mesh.parts)
					{
						auto& b = buckets[part.material + (is_patch ? "\x01" : "")];
						b.two_sided = is_patch;
						const auto base = static_cast<std::uint32_t>(b.vertices.size());

						// the part's vertices, as they are ( a face is a fan of its own vertices )
						std::uint32_t lo = 0xFFFFFFFFu, hi = 0;
						for (std::uint32_t i = 0; i < part.index_count; i++)
						{
							const std::uint32_t index = mesh.indices[part.first_index + i];
							lo = std::min(lo, index);
							hi = std::max(hi, index);
						}

						for (std::uint32_t v = lo; v <= hi && hi != 0xFFFFFFFFu && lo != 0xFFFFFFFFu; v++)
						{
							const auto& src = mesh.vertices[v];
							view::mesh_vertex out;
							out.position = { src.position[0], src.position[1], src.position[2] };
							out.normal = { src.normal[0], src.normal[1], src.normal[2] };
							out.color = (static_cast<std::uint32_t>(src.color[3]) << 24) | (static_cast<std::uint32_t>(src.color[0]) << 16) | (static_cast<std::uint32_t>(src.color[1]) << 8) | src.color[2];
							out.uv[0] = src.uv[0];
							out.uv[1] = src.uv[1];
							b.vertices.push_back(out);
						}

						for (std::uint32_t i = 0; i < part.index_count; i++)
						{
							b.indices.push_back(base + mesh.indices[part.first_index + i] - lo);
						}
					}
				}
			}

			for (auto& [key, b] : buckets)
			{
				std::string material = key;
				if (!material.empty() && material.back() == '\x01')
				{
					material.pop_back();
				}

				view::mesh_data data;
				data.vertices = b.vertices.data();
				data.vertex_count = b.vertices.size();
				data.indices = b.indices.data();
				data.index_count = b.indices.size();
				data.fallback_color = fallback_color(material);
				data.two_sided = b.two_sided;
				data.material = material;

				if (!std::binary_search(names.begin(), names.end(), material) && std::binary_search(names.begin(), names.end(), "wc/" + material))
				{
					data.material = "wc/" + material;
				}

				if (const int handle = renderer.create_mesh(data))
				{
					inst.meshes.push_back(handle);
				}
			}

			g_mesh_total += static_cast<int>(inst.meshes.size());
			inst.content.reset();
		}

		void set_drawn_visible(instance& inst, const bool visible)
		{
			if (inst.visible == visible)
			{
				return;
			}

			inst.visible = visible;
			auto& renderer = get_scene().renderer;

			for (const int handle : inst.meshes) { renderer.set_mesh_visible(handle, visible); }
			for (const int handle : inst.models) { renderer.set_hidden({ view::object_kind::placed_model, handle }, !visible); }
		}

		const edit::entity* selected_prefab()
		{
			auto& doc = get_scene().doc;

			for (const object_id id : doc.selected_entities())
			{
				if (const auto* e = doc.find_entity(id); e && edit::document::is_prefab(*e))
				{
					return e;
				}
			}

			return nullptr;
		}

		void fail(const char* title, const std::string& text)
		{
			game::printf_to_console("^1%s: %s\n", title, text.c_str());
			ImGui::Toast(ImGuiToastType_Error, title, text.c_str(), 6000);
		}

		// ---- the browser ----------------------------------------------------------------------------

		struct browser_item
		{
			std::string model;			// "prefabs/a/b.map"
			std::string info;
			bool looked = false;
		};

		std::vector<browser_item> g_items;
		bool g_items_scanned = false;
		int g_item_selected = -1;
		char g_item_filter[96] = {};

		void scan_items()
		{
			g_items.clear();
			g_items_scanned = true;
			g_item_selected = -1;

			const auto root = map_source_dir();
			std::error_code ec;

			if (!std::filesystem::is_directory(root / "prefabs", ec))
			{
				return;
			}

			for (auto it = std::filesystem::recursive_directory_iterator(root / "prefabs", ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
			{
				if (it->is_regular_file(ec) && it->path().extension() == ".map" && it->path().filename().string().find(".codol_") == std::string::npos)
				{
					browser_item item;
					item.model = to_utf8(std::filesystem::relative(it->path(), root, ec));
					std::replace(item.model.begin(), item.model.end(), '\\', '/');
					g_items.push_back(std::move(item));
				}
			}

			std::sort(g_items.begin(), g_items.end(), [](const browser_item& a, const browser_item& b) { return a.model < b.model; });
		}

		void look_at_item(browser_item& item)
		{
			item.looked = true;
			std::string text, error;
			edit::document doc;

			if (!load_prefab_text(item.model, text, error) || !doc.load_map_text(text, error))
			{
				item.info = error;
				return;
			}

			const auto info = doc.info();
			const auto size = info.box.valid() ? info.box.size() : edit::vec3{};
			char line[256];
			std::snprintf(line, sizeof(line), "%d brushes, %d patches, %d entities ( %d models, %d prefabs )   %.0f x %.0f x %.0f", static_cast<int>(info.brushes),
				static_cast<int>(info.patches), static_cast<int>(info.entities) - 1, static_cast<int>(info.models), static_cast<int>(info.prefabs), size.x, size.y, size.z);
			item.info = line;
		}

		object_id place_prefab(const std::string& model, const edit::vec3& at, const double yaw)
		{
			auto& s = get_scene();
			std::vector<std::pair<std::string, std::string>> pairs{ { "model", model } };

			if (yaw != 0.0)
			{
				pairs.emplace_back("angles", "0 " + edit::format_key_number(yaw) + " 0");
			}

			return s.doc.create_point_entity("misc_prefab", at, pairs);
		}

		edit::vec3 place_position()
		{
			auto& s = get_scene();
			return edit::snap_point(s.ray_target(s.cam.origin, s.cam.axes().forward), s.snap ? s.grid : 0.0);
		}
	}

	// ---- per frame ----------------------------------------------------------------------------------------

	void prefab_frame()
	{
		auto& s = get_scene();
		auto& doc = s.doc;

		if (!s.open)
		{
			// the renderer is gone and with it every handle
			g_instances.clear();
			g_lines.clear();
			g_mesh_total = 0;
			g_stack.clear();
			g_open_serial = s.open_serial;
			return;
		}

		bool force = false;

		if (g_open_serial != s.open_serial)
		{
			g_open_serial = s.open_serial;
			g_instances.clear();
			g_mesh_total = 0;
			force = true;

			// a map that was opened by hand while a prefab was being edited: there is nothing to go back to
			if (!g_stack.empty() && g_stack.back().prefab != s.map_file)
			{
				g_stack.clear();
			}
		}

		// the renderer was loaded again ( the stage was rebuilt, assets were refreshed ): its meshes went with it
		if (g_mesh_total > 0 && s.renderer.loaded() && s.renderer.stats().mesh_count < g_mesh_total)
		{
			for (auto& [id, inst] : g_instances)
			{
				inst.meshes.clear();
				inst.models.clear();
				inst.drawn = false;
				inst.revision = 0;
			}

			g_mesh_total = 0;
			force = true;
		}

		const bool shown = get().show_prefab_contents;
		force |= files_changed() || shown != g_shown;
		g_shown = shown;

		bool pending = false;
		for (const auto& [id, inst] : g_instances)
		{
			pending |= !inst.drawn;
		}

		if (!force && !pending && g_change == doc.change_counter() && g_visibility == doc.visibility_counter())
		{
			return;
		}

		g_change = doc.change_counter();
		g_visibility = doc.visibility_counter();
		g_stamp++;
		g_lines.clear();

		for (const object_id id : doc.entities())
		{
			const auto* e = doc.find_entity(id);
			if (!e || !edit::document::is_prefab(*e))
			{
				continue;
			}

			auto& inst = g_instances[id];
			inst.stamp = g_stamp;

			if (inst.revision != e->revision || inst.generation != g_file_generation)
			{
				destroy_drawn(inst);
				build_geometry(*e, inst);

				if (!inst.error.empty())
				{
					game::printf_to_console("^3[CoDOL] prefab: %s\n", inst.error.c_str());
				}
			}

			const bool visible = shown && doc.is_visible(id);

			// while the entity is dragged only its lines follow
			if (!inst.drawn && visible && !s.dragging() && s.renderer.loaded())
			{
				build_drawn(inst);
				inst.visible = true;
			}

			set_drawn_visible(inst, visible);

			if (visible)
			{
				const DWORD color = doc.is_selected(id) ? 0xFFFFE060 : CONTENT_COLOR;
				for (const auto& p : inst.points)
				{
					g_lines.push_back({ p.x, p.y, p.z, color });
				}

				if (inst.points.empty())
				{
					// a prefab whose file is missing: a cross where it stands
					const auto o = e->origin();
					const float x = static_cast<float>(o.x), y = static_cast<float>(o.y), z = static_cast<float>(o.z);
					for (const auto& d : { float3{ 24, 24, 0 }, float3{ 24, -24, 0 }, float3{ 0, 0, 24 } })
					{
						g_lines.push_back({ x - d.x, y - d.y, z - d.z, MISSING_COLOR });
						g_lines.push_back({ x + d.x, y + d.y, z + d.z, MISSING_COLOR });
					}
				}
			}
		}

		for (auto it = g_instances.begin(); it != g_instances.end();)
		{
			if (it->second.stamp != g_stamp)
			{
				destroy_drawn(it->second);
				it = g_instances.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void append_prefab_lines(std::vector<line_vertex>& out)
	{
		out.insert(out.end(), g_lines.begin(), g_lines.end());
	}

	int prefab_depth()
	{
		return static_cast<int>(g_stack.size());
	}

	bool prefab_selected()
	{
		return get_scene().open && selected_prefab() != nullptr;
	}

	// ---- enter / leave ------------------------------------------------------------------------------------

	bool enter_prefab()
	{
		auto& s = get_scene();
		const auto* e = s.open ? selected_prefab() : nullptr;

		if (!e)
		{
			fail("Enter prefab", "Select a misc_prefab first.");
			return false;
		}

		const std::string model(e->value("model"));
		const auto origin = e->origin();
		const auto all = roots();
		const auto file = edit::resolve_prefab(model, all);

		if (file.empty())
		{
			fail("Enter prefab", "\"" + model + "\" was not found below map_source.");
			return false;
		}

		// the map is saved: it is opened again when the prefab is left
		if ((s.doc.modified() || s.map_file.empty()) && !save())
		{
			return false;
		}

		level l;
		l.file = s.map_file;
		l.prefab = file;
		l.cam = s.cam;
		l.model = model;
		l.origin = origin;

		const auto stack = g_stack;
		std::string error;

		if (!s.open_map_file(file, error))
		{
			fail("Enter prefab", error);
			std::string again;
			s.open_map_file(l.file, again);
			s.cam = l.cam;
			g_stack = stack;
			return false;
		}

		g_stack = stack;
		g_stack.push_back(std::move(l));
		g_open_serial = ~0ull;

		s.doc.select_all();
		s.look_at_selection();
		s.doc.deselect_all();

		game::printf_to_console("[CoDOL] editing prefab %s ( level %d ); Edit > Leave Prefab saves it and goes back\n", model.c_str(), prefab_depth());
		return true;
	}

	bool leave_prefab()
	{
		auto& s = get_scene();

		if (g_stack.empty())
		{
			fail("Leave prefab", "No prefab is being edited.");
			return false;
		}

		if (s.open && s.doc.modified() && !save())
		{
			return false;
		}

		const level l = g_stack.back();
		auto stack = g_stack;
		stack.pop_back();

		std::string error;
		if (!s.open_map_file(l.file, error))
		{
			fail("Leave prefab", error);
			return false;
		}

		g_stack = std::move(stack);
		g_open_serial = ~0ull;
		g_files.erase(l.model);		// read again: it may have been edited
		g_file_generation++;
		s.cam = l.cam;

		// the prefab that was entered is selected again
		for (const object_id id : s.doc.entities())
		{
			const auto* e = s.doc.find_entity(id);
			if (e && edit::document::is_prefab(*e) && e->value("model") == l.model && e->origin() == l.origin)
			{
				s.doc.select(id);
				break;
			}
		}

		game::printf_to_console("[CoDOL] back in %s\n", to_utf8(l.file.filename()).c_str());
		return true;
	}

	// ---- stamp / convert ----------------------------------------------------------------------------------

	bool stamp_prefab()
	{
		auto& s = get_scene();
		edit::prefab_result result;
		const bool ok = s.doc.expand_prefabs(load_prefab_text, result, true);

		for (const auto& warning : result.warnings)
		{
			game::printf_to_console("^3[CoDOL] stamp prefab: %s\n", warning.c_str());
		}

		if (!report(ok, "Stamp prefab"))
		{
			return false;
		}

		game::printf_to_console("stamp prefab: %d prefab(s) became %d brushes, %d patches, %d entities\n", result.expanded, result.brushes, result.patches, result.entities);
		return true;
	}

	bool convert_to_prefab(const std::string& name, const bool overwrite)
	{
		auto& s = get_scene();
		std::string clean;

		for (const char c : name)
		{
			if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
			{
				clean.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
			}
			else if (c == '/' || c == '\\')
			{
				clean.push_back('/');
			}
		}

		if (clean.rfind("prefabs/", 0) == 0)
		{
			clean.erase(0, 8);
		}

		if (clean.empty())
		{
			fail("Convert to prefab", "Give the prefab a name: letters, digits, _ and / for folders.");
			return false;
		}

		const std::string model = "prefabs/" + clean + ".map";
		const auto file = map_source_dir() / std::filesystem::u8path(model);
		std::error_code ec;

		if (std::filesystem::exists(file, ec) && !overwrite)
		{
			fail("Convert to prefab", "map_source\\" + model + " exists already: pick another name ( or prefab_convert <name> overwrite ).");
			return false;
		}

		std::string text;
		const object_id id = s.doc.convert_selection_to_prefab(model, text);

		if (id == edit::no_object)
		{
			report(false, "Convert to prefab");
			return false;
		}

		std::string error;
		if (!write_text(file, text, error))
		{
			s.doc.undo();
			fail("Convert to prefab", error);
			return false;
		}

		g_files.erase(model);
		g_file_generation++;
		g_items_scanned = false;
		game::printf_to_console("[CoDOL] the selection is now map_source\\%s, placed by a misc_prefab ( #%u )\n", model.c_str(), id);
		ImGui::Toast(ImGuiToastType_Success, "Convert to prefab", ("map_source\\" + model).c_str(), 4000);
		return true;
	}

	// ---- menus / windows ----------------------------------------------------------------------------------

	void draw_prefab_menu_items()
	{
		auto& s = get_scene();
		const bool any = s.open && !s.doc.selection().empty();
		const bool prefab = prefab_selected();

		if (editor::menu::item("Enter Prefab", nullptr, false, prefab)) { enter_prefab(); }
		if (editor::menu::item("Leave Prefab", nullptr, false, prefab_depth() > 0)) { leave_prefab(); }
		if (editor::menu::item("Stamp Prefab", nullptr, false, prefab)) { stamp_prefab(); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("The selected misc_prefab is replaced by the brushes and entities of its file."); }
		if (editor::menu::item("Convert Selection To Prefab ..", nullptr, false, any)) { get().open_convert_prefab = true; }
		if (editor::menu::item("Prefab Browser", nullptr, &get().show_prefab_browser)) {}
	}

	void draw_prefab_breadcrumb()
	{
		if (g_stack.empty())
		{
			return;
		}

		std::string text = "  prefab: " + to_utf8(g_stack.front().file.filename());
		for (const auto& l : g_stack)
		{
			text += " > " + l.model;
		}

		ImGui::TextColored(ImVec4(0.30f, 0.86f, 0.70f, 1.0f), "%s", text.c_str());

		if (ImGui::SmallButton("Leave"))
		{
			leave_prefab();
		}
	}

	void draw_prefab_browser()
	{
		auto& st = get();
		auto& s = get_scene();

		if (!st.show_prefab_browser)
		{
			return;
		}

		ImGui::SetNextWindowPos(ImVec2(520.0f, 160.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420.0f, 480.0f), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Prefabs##window", &st.show_prefab_browser, ImGuiWindowFlags_NoCollapse))
		{
			if (!g_items_scanned)
			{
				scan_items();
			}

			if (ImGui::Button("Refresh"))
			{
				scan_items();
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##prefab_filter", "filter", g_item_filter, sizeof(g_item_filter));
			ImGui::TextDisabled("%d files in map_source\\prefabs", static_cast<int>(g_items.size()));

			const float footer = ImGui::GetFrameHeightWithSpacing() * 2.2f;

			if (ImGui::BeginChild("##prefab_list", ImVec2(0.0f, -footer), true))
			{
				for (std::size_t i = 0; i < g_items.size(); i++)
				{
					auto& item = g_items[i];
					if (g_item_filter[0] && item.model.find(g_item_filter) == std::string::npos)
					{
						continue;
					}

					if (ImGui::Selectable(item.model.c_str() + 8, g_item_selected == static_cast<int>(i), ImGuiSelectableFlags_AllowDoubleClick))
					{
						g_item_selected = static_cast<int>(i);
						if (!item.looked) { look_at_item(item); }

						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && s.open)
						{
							report(place_prefab(item.model, place_position(), 0.0) != edit::no_object, "Place prefab");
						}
					}
				}
			}

			ImGui::EndChild();

			if (g_item_selected >= 0 && static_cast<std::size_t>(g_item_selected) < g_items.size())
			{
				auto& item = g_items[static_cast<std::size_t>(g_item_selected)];
				ImGui::TextWrapped("%s", item.info.c_str());

				ImGui::BeginDisabled(!s.open);
				if (ImGui::Button("Place where the camera looks"))
				{
					report(place_prefab(item.model, place_position(), 0.0) != edit::no_object, "Place prefab");
				}
				ImGui::EndDisabled();

				ImGui::SameLine();
				if (ImGui::Button("Open as map"))
				{
					open_map_file(map_source_dir() / std::filesystem::u8path(item.model));
				}
			}
			else
			{
				ImGui::TextDisabled("Double click places a prefab where the camera looks.\nSelection > Prefabs > Convert makes one out of the selection.");
			}
		}

		ImGui::End();
	}

	void register_prefab_commands()
	{
		command::register_command("prefab_enter", [](args_t) { if (need_map()) { enter_prefab(); } });
		command::register_command("prefab_leave", [](args_t) { if (need_map()) { leave_prefab(); } });
		command::register_command("prefab_stamp", [](args_t) { if (need_map()) { stamp_prefab(); } });
		command::register_command("prefab_browser", [](args_t) { get().show_prefab_browser = !get().show_prefab_browser; });

		// prefab_convert <name> [overwrite]: the selection -> map_source\prefabs\<name>.map + a misc_prefab
		command::register_command("prefab_convert", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { game::printf_to_console("usage: prefab_convert <name> [overwrite]\n"); return; }
			convert_to_prefab(args[1], args.size() > 2 && args[2] == "overwrite");
		});

		// prefab_place <prefabs/x.map> [x y z] [yaw]: a misc_prefab ( where the camera looks without a position )
		command::register_command("prefab_place", [](args_t args)
		{
			if (!need_map() || args.size() < 2) { game::printf_to_console("usage: prefab_place <prefabs/x.map> [x y z] [yaw]\n"); return; }
			const object_id id = place_prefab(args[1], args.size() > 4 ? vector(args, 2) : place_position(), number(args, 5));
			if (id) { game::printf_to_console("created #%u misc_prefab %s\n", id, args[1].c_str()); }
			else { game::printf_to_console("^1%s\n", get_scene().doc.last_error().c_str()); }
		});

		// prefab_show [0|1]: the content of the prefabs in the views
		command::register_command("prefab_show", [](args_t args)
		{
			get().show_prefab_contents = args.size() > 1 ? number(args, 1) != 0.0 : !get().show_prefab_contents;
		});

		// prefab_list [text]: the files of map_source\prefabs; prefab_info: the prefabs of the map
		command::register_command("prefab_list", [](args_t args)
		{
			scan_items();
			int shown = 0;

			for (auto& item : g_items)
			{
				if (args.size() > 1 && item.model.find(args[1]) == std::string::npos) { continue; }
				look_at_item(item);
				game::printf_to_console("  %s   %s\n", item.model.c_str(), item.info.c_str());
				shown++;
			}

			game::printf_to_console("%d prefab file(s) in %s\n", shown, to_utf8(map_source_dir() / "prefabs").c_str());
		});

		command::register_command("prefab_info", [](args_t)
		{
			if (!need_map()) { return; }
			auto& doc = get_scene().doc;
			int count = 0;

			for (const object_id id : doc.entities())
			{
				const auto* e = doc.find_entity(id);
				if (!e || !edit::document::is_prefab(*e)) { continue; }

				count++;
				const auto it = g_instances.find(id);
				const auto o = e->origin();

				if (it == g_instances.end())
				{
					game::printf_to_console("  #%u %s at %g %g %g: not built yet\n", id, std::string(e->value("model")).c_str(), o.x, o.y, o.z);
					continue;
				}

				const auto& inst = it->second;
				game::printf_to_console("  #%u %s at %g %g %g angles %s: %d brushes, %d patches, %d entities, %d lines, %d meshes, %d models, box %g %g %g .. %g %g %g%s%s\n", id,
					std::string(e->value("model")).c_str(), o.x, o.y, o.z, std::string(e->value("angles", "0 0 0")).c_str(), inst.brushes, inst.patches, inst.entities,
					static_cast<int>(inst.points.size() / 2), static_cast<int>(inst.meshes.size()), static_cast<int>(inst.models.size()),
					inst.box.mins.x, inst.box.mins.y, inst.box.mins.z, inst.box.maxs.x, inst.box.maxs.y, inst.box.maxs.z, inst.error.empty() ? "" : "  ^1", inst.error.c_str());
			}

			std::string trail;
			for (const auto& l : g_stack) { trail += " > " + l.model; }
			game::printf_to_console("%d prefab(s) in the map; editing level %d%s\n", count, prefab_depth(), trail.c_str());
		});
	}
}

namespace editor::ui
{
	// the box of a misc_prefab entity relative to its origin: the box of its content
	bool prefab_box(const edit::entity& e, edit::bounds& local_box)
	{
		using namespace ext;

		if (!edit::document::is_prefab(e))
		{
			return false;
		}

		auto& inst = g_instances[e.id];
		if (inst.revision != e.revision || inst.generation != g_file_generation)
		{
			if (inst.drawn || !inst.meshes.empty())
			{
				destroy_drawn(inst);
			}

			build_geometry(e, inst);
			inst.stamp = g_stamp;
		}

		if (!inst.box.valid())
		{
			return false;
		}

		local_box.mins = inst.box.mins - inst.origin;
		local_box.maxs = inst.box.maxs - inst.origin;
		return true;
	}
}
