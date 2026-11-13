#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"
#include "ext.hpp"

#include <codol_edit/patch_ops.hpp>

#include <numeric>

// The Brush and Patch menus, their dialogs and their console commands ( brush_*, patch_* ). The work is done by
// codol_edit ( patch_ops.hpp ); this file decides what the operation applies to - the selected brushes' box or the
// last box that was drawn, the selected patches, the selected patch vertices - and says why when it cannot.
//
// Menu, shortcut and command all end in the same functions, so everything here can be scripted.

namespace editor::ui
{
	namespace
	{
		using args_t = const std::vector<std::string>&;
		using edit::brush_shape;
		using edit::patch_shape;
		using edit::patch_line;

		struct bp_state
		{
			// ---- dialogs
			bool show_brush = false;
			int brush_shape_index = 2;		// brush_shape: prism
			int sides = 8;
			int axis = 2;					// axis of prisms, cones, spheres and patch primitives

			bool show_simple = false;
			bool simple_terrain = false;
			int simple_width = 3, simple_height = 3;
			int simple_axis = -1;			// -1: the thinnest side of the box
			bool delete_original = true;	// the brushes whose box a new patch fills

			bool show_thicken = false;
			float thickness = 8.0f;
			bool seams = true;

			bool show_cap = false;
			int cap_type = 0;
			bool cap_first = true, cap_last = true;

			bool show_vertex = false;
			float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			float noise[3] = { 0.0f, 0.0f, 8.0f };
			int noise_seed = 1;

			bool show_advanced = false;
			bool show_points = false;
			std::string points_text;

			// ---- how patches are shown / edited ( the toolbar toggles of radiant )
			bool weld_moves = false;		// vertices that share a place move together
			bool tolerant_weld = false;		// weld over one grid step instead of 0.1 units
			bool drill_down = true;			// a vertex grabbed in a 2D view takes the ones behind it along
			bool edge_mode = false;			// a click on a terrain quad in the 3D view turns its diagonal

			// ---- patch vertices
			object_id vsel_patch = edit::no_object;
			std::vector<int> vsel;
			std::unordered_map<object_id, std::vector<int>> locked;
		};

		bp_state g;

		const char* const SHAPE_NAMES[] = { "Box", "Wedge", "Cylinder ( N sides )", "Cone", "Sphere" };
		const char* const PATCH_SHAPE_KEYS[] = { "bevel", "square_bevel", "endcap", "square_endcap", "cylinder", "dense_cylinder", "very_dense_cylinder",
			"square_cylinder", "cone" };
		const char* const PATCH_SHAPE_LABELS[] = { "Bevel", "Square Bevel", "End Cap", "Square End Cap", "Cylinder", "Dense Cylinder", "Very Dense Cylinder",
			"Square Cylinder", "Cone" };
		const char* const CAP_KEYS[] = { "auto", "bevel", "endcap", "inverted_bevel", "inverted_endcap", "cylinder" };
		const char* const CAP_LABELS[] = { "Automatic", "Bevel", "End cap", "Inverted bevel", "Inverted end cap", "Cylinder" };

		double number(args_t args, const std::size_t index, const double fallback = 0.0)
		{
			return index < args.size() ? std::atof(args[index].c_str()) : fallback;
		}

		bool is_number(const std::string& text)
		{
			char* end = nullptr;
			std::strtod(text.c_str(), &end);
			return !text.empty() && end && *end == 0;
		}

		bool need_map()
		{
			if (!get_scene().open)
			{
				game::printf_to_console("^1no map is open\n");
				return false;
			}

			return true;
		}

		// toast + console line when it failed, a console line when it worked
		bool done(const bool ok, const char* what)
		{
			auto& doc = get_scene().doc;

			if (!report(ok, what))
			{
				return false;
			}

			game::printf_to_console("%s: ok ( %d selected, %d brushes, %d patches )\n", what, static_cast<int>(doc.selection().size()),
				static_cast<int>(doc.brush_count()), static_cast<int>(doc.patch_count()));
			return true;
		}

		bool fail(const char* what, const char* why)
		{
			ImGui::Toast(ImGuiToastType_Warning, what, why, 3000);
			game::printf_to_console("^1%s: %s\n", what, why);
			return false;
		}

		// ---- what is selected ------------------------------------------------------------------------------

		struct selection_info
		{
			int brushes = 0, curves = 0, terrains = 0, faces = 0;
			object_id first_patch = edit::no_object;
			bool box = false;		// there is a box for a new shape

			[[nodiscard]] int patches() const { return curves + terrains; }
		};

		selection_info inspect()
		{
			selection_info info;
			auto& s = get_scene();

			if (!s.open)
			{
				return info;
			}

			for (const object_id id : s.doc.selection())
			{
				if (const auto* b = s.doc.find_brush(id))
				{
					info.brushes += !b->opaque;
				}
				else if (const auto* p = s.doc.find_patch(id))
				{
					(p->type == edit::patch_type::curve ? info.curves : info.terrains)++;
					if (!info.first_patch)
					{
						info.first_patch = id;
					}
				}
			}

			info.faces = static_cast<int>(s.doc.selected_faces().size());
			info.box = info.brushes > 0 || (s.last_selection_box.valid() && s.last_selection_box.size().x + s.last_selection_box.size().y + s.last_selection_box.size().z > 0.0);
			return info;
		}

		// The box a new shape fills: `at x1 y1 z1 x2 y2 z2` of a command, else the selected brushes, else the last box
		// that was selected / drawn. `brushes`: the selected brushes the box came from.
		bool shape_box(args_t args, edit::bounds& box, std::vector<object_id>& brushes, std::string& material, const char* what)
		{
			auto& s = get_scene();
			brushes.clear();
			box = {};
			material.clear();

			for (std::size_t i = 0; i + 6 < args.size(); i++)
			{
				if (args[i] == "at")
				{
					const edit::vec3 a{ number(args, i + 1), number(args, i + 2), number(args, i + 3) };
					const edit::vec3 b{ number(args, i + 4), number(args, i + 5), number(args, i + 6) };
					box.add(a);
					box.add(b);
					return true;
				}
			}

			for (const object_id id : s.doc.selection())
			{
				const auto* b = s.doc.find_brush(id);
				if (b && !b->opaque && !b->faces.empty())
				{
					if (brushes.empty())
					{
						material = b->faces[0].texture.material;
					}

					brushes.push_back(id);
					box.add(s.doc.object_bounds(id));
				}
			}

			if (!brushes.empty() && box.valid())
			{
				return true;
			}

			if (s.last_selection_box.valid())
			{
				box = s.last_selection_box;
				return true;
			}

			return fail(what, "Select the brush whose box the shape is to fill, or draw a box in a 2D view first.");
		}

		// the words of a command that are not the `at ..` box
		std::vector<std::string> plain_args(args_t args)
		{
			std::vector<std::string> out;
			for (std::size_t i = 1; i < args.size(); i++)
			{
				if (args[i] == "at")
				{
					i += 6;
					continue;
				}

				out.push_back(args[i]);
			}

			return out;
		}

		int axis_arg(const std::vector<std::string>& words, const int fallback)
		{
			for (const auto& w : words)
			{
				if (w == "x" || w == "X") { return 0; }
				if (w == "y" || w == "Y") { return 1; }
				if (w == "z" || w == "Z") { return 2; }
			}

			return fallback;
		}

		int thinnest_axis(const edit::bounds& box)
		{
			const auto size = box.size();
			return size.z <= size.x && size.z <= size.y ? 2 : (size.x <= size.y ? 0 : 1);
		}

		// ---- brushes ---------------------------------------------------------------------------------------

		bool make_brush(const brush_shape shape, const int sides, const int axis, args_t args = {})
		{
			if (!need_map())
			{
				return false;
			}

			auto& doc = get_scene().doc;
			const char* what = SHAPE_NAMES[static_cast<int>(shape)];

			edit::bounds box;
			std::vector<object_id> brushes;
			std::string material;
			if (!shape_box(args, box, brushes, material, what))
			{
				return false;
			}

			const object_id id = !brushes.empty() ? edit::brush_shape_from_selection(doc, shape, sides, axis)
				: edit::create_brush_shape(doc, shape, box.mins, box.maxs, sides, axis);

			if (done(id != edit::no_object, what))
			{
				const auto* b = doc.find_brush(id);
				const auto made = doc.object_bounds(id);
				game::printf_to_console("  #%u: %d faces, %s, mins %.6g %.6g %.6g maxs %.6g %.6g %.6g\n", id, b ? static_cast<int>(b->faces.size()) : 0,
					b && !b->faces.empty() ? b->faces[0].texture.material.c_str() : "", made.mins.x, made.mins.y, made.mins.z, made.maxs.x, made.maxs.y, made.maxs.z);
				return true;
			}

			return false;
		}

		bool brush_from_points(const std::vector<edit::vec3>& points)
		{
			if (!need_map())
			{
				return false;
			}

			auto& doc = get_scene().doc;
			return done(doc.create_brush_from_points(points) != edit::no_object, "Brush from points");
		}

		std::vector<edit::vec3> parse_points(const std::string& text)
		{
			std::vector<double> numbers;
			const char* p = text.c_str();

			while (*p)
			{
				char* end = nullptr;
				const double v = std::strtod(p, &end);

				if (end == p)
				{
					p++;
					continue;
				}

				numbers.push_back(v);
				p = end;
			}

			std::vector<edit::vec3> points;
			for (std::size_t i = 0; i + 2 < numbers.size(); i += 3)
			{
				points.emplace_back(numbers[i], numbers[i + 1], numbers[i + 2]);
			}

			return points;
		}

		// ---- new patches -----------------------------------------------------------------------------------

		// creates through `make( box, material )`; the brushes the box came from go ( one undo step )
		template <typename Fn>
		bool make_patch(const char* what, args_t args, const bool delete_brushes, Fn&& make)
		{
			if (!need_map())
			{
				return false;
			}

			auto& doc = get_scene().doc;

			edit::bounds box;
			std::vector<object_id> brushes;
			std::string material;
			if (!shape_box(args, box, brushes, material, what))
			{
				return false;
			}

			doc.begin_transaction(what);
			const object_id id = make(box, material);

			if (!id)
			{
				const std::string error = doc.last_error();
				doc.cancel_transaction();
				return fail(what, error.c_str());
			}

			if (delete_brushes && !brushes.empty())
			{
				doc.select(brushes);
				doc.delete_selection();
				doc.select(id);
			}

			doc.end_transaction();

			const auto* p = doc.find_patch(id);
			game::printf_to_console("%s: ok, #%u %s %d x %d, %s ( %d patches )\n", what, id, p->type == edit::patch_type::curve ? "curve" : "terrain",
				p->width, p->height, p->material.c_str(), static_cast<int>(doc.patch_count()));
			return true;
		}

		bool make_simple(const bool terrain, const int width, const int height, int axis, const bool delete_brushes, args_t args = {})
		{
			return make_patch(terrain ? "Simple terrain patch" : "Simple curve patch", args, delete_brushes, [&](const edit::bounds& box, const std::string& material)
			{
				auto& doc = get_scene().doc;
				const int a = axis >= 0 && axis <= 2 ? axis : thinnest_axis(box);

				// on the far side of the box: the top of a brush for a floor
				edit::vec3 mins = box.mins;
				mins[a] = box.maxs[a];

				return terrain ? doc.create_terrain_patch(mins, box.maxs, width, height, a, material)
					: edit::create_curve_patch(doc, mins, box.maxs, width, height, a, material);
			});
		}

		bool make_shape(const patch_shape shape, const int axis, const bool delete_brushes, args_t args = {})
		{
			return make_patch(PATCH_SHAPE_LABELS[static_cast<int>(shape)], args, delete_brushes, [&](const edit::bounds& box, const std::string& material)
			{
				return edit::create_patch_shape(get_scene().doc, shape, box.mins, box.maxs, axis, material);
			});
		}

		// ---- patch vertices --------------------------------------------------------------------------------

		// the selection of vertices belongs to one selected patch; it goes when that patch does
		const edit::patch* vsel_patch()
		{
			auto& doc = get_scene().doc;
			const auto* p = get_scene().open && doc.is_selected(g.vsel_patch) ? doc.find_patch(g.vsel_patch) : nullptr;

			if (!p)
			{
				g.vsel_patch = edit::no_object;
				g.vsel.clear();
				return nullptr;
			}

			std::erase_if(g.vsel, [&](const int i) { return i < 0 || i >= static_cast<int>(p->vertices.size()); });
			return p;
		}

		// the patch the vertex dialogs work on: the one with selected vertices, else the first selected patch
		object_id active_patch()
		{
			if (vsel_patch() && !g.vsel.empty())
			{
				return g.vsel_patch;
			}

			return inspect().first_patch;
		}

		void select_vertices(const object_id patch, std::vector<int> list, const bool add)
		{
			if (g.vsel_patch != patch || !add)
			{
				g.vsel.clear();
			}

			g.vsel_patch = patch;
			for (const int i : list)
			{
				if (std::find(g.vsel.begin(), g.vsel.end(), i) == g.vsel.end())
				{
					g.vsel.push_back(i);
				}
			}
		}

		// the column / row the selected vertices share, -1: none or not one line
		int selected_line(const patch_line line)
		{
			const auto* p = vsel_patch();
			if (!p || g.vsel.empty())
			{
				return -1;
			}

			int found = -1;
			for (const int i : g.vsel)
			{
				const int at = line == patch_line::column ? i / p->height : i % p->height;
				if (found >= 0 && found != at)
				{
					return -1;
				}

				found = at;
			}

			return found;
		}

		double weld_tolerance()
		{
			return g.tolerant_weld ? std::max(get_scene().grid, 0.1) : 0.1;
		}

		const std::vector<int>& locked_of(const object_id patch)
		{
			static const std::vector<int> none;
			const auto it = g.locked.find(patch);
			return it != g.locked.end() ? it->second : none;
		}

		bool lock_vertices(const bool lock, const bool all)
		{
			const auto* p = vsel_patch();
			const object_id id = p ? g.vsel_patch : inspect().first_patch;

			if (!id)
			{
				return fail(lock ? "Lock vertices" : "Unlock vertices", "Select a patch ( and, in a 2D view in vertex mode or in the Advanced Edit dialog, the vertices ).");
			}

			auto& list = g.locked[id];

			if (!lock && (all || g.vsel.empty()))
			{
				list.clear();
				game::printf_to_console("Unlock vertices: patch #%u has no locked vertices now\n", id);
				return true;
			}

			if (lock && g.vsel.empty())
			{
				return fail("Lock vertices", "Select the vertices to lock: grab them in a 2D view in vertex mode, or click them in the Advanced Edit dialog.");
			}

			for (const int i : g.vsel)
			{
				const auto it = std::find(list.begin(), list.end(), i);
				if (lock && it == list.end()) { list.push_back(i); }
				if (!lock && it != list.end()) { list.erase(it); }
			}

			game::printf_to_console("%s: patch #%u has %d locked vertices\n", lock ? "Lock vertices" : "Unlock vertices", id, static_cast<int>(list.size()));
			return true;
		}

		// ---- operations with their targets -------------------------------------------------------------------

		bool op_insert(const patch_line line, const bool at_end)
		{
			return need_map() && done(edit::patch_insert(get_scene().doc, line, at_end, at_end ? -1 : selected_line(line)), line == patch_line::column
				? (at_end ? "Add 2 columns" : "Insert 2 columns") : (at_end ? "Add 2 rows" : "Insert 2 rows"));
		}

		// the line the selected vertices share decides between row and column; without one: a column in the last cell
		bool op_terrain_add(int line_arg = -1, const int after = -1)
		{
			if (!need_map()) { return false; }
			auto& doc = get_scene().doc;

			if (line_arg < 0)
			{
				const int column = selected_line(patch_line::column), row = selected_line(patch_line::row);
				if (column >= 0 && row < 0) { return done(edit::patch_insert_terrain(doc, patch_line::column, column), "Add terrain column"); }
				if (row >= 0 && column < 0) { return done(edit::patch_insert_terrain(doc, patch_line::row, row), "Add terrain row"); }
				line_arg = 0;
			}

			return done(edit::patch_insert_terrain(doc, line_arg ? patch_line::row : patch_line::column, after), line_arg ? "Add terrain row" : "Add terrain column");
		}

		bool op_terrain_remove(int line_arg = -1, int index = -1)
		{
			if (!need_map()) { return false; }
			auto& doc = get_scene().doc;

			if (line_arg < 0)
			{
				const int column = selected_line(patch_line::column), row = selected_line(patch_line::row);
				if (row >= 0 && column < 0) { line_arg = 1; index = row; }
				else { line_arg = 0; index = column; }
			}

			const bool ok = done(edit::patch_delete_terrain(doc, line_arg ? patch_line::row : patch_line::column, index), line_arg ? "Remove terrain row" : "Remove terrain column");
			if (ok) { g.vsel.clear(); }
			return ok;
		}

		// the edge the selected vertices lie on is pulled out; without one: the last row
		bool op_extrude(int line_arg = -1, int end_arg = -1)
		{
			if (!need_map()) { return false; }
			auto& doc = get_scene().doc;

			if (line_arg < 0)
			{
				const auto* p = vsel_patch();
				const int column = selected_line(patch_line::column), row = selected_line(patch_line::row);

				if (p && column >= 0 && row < 0 && (column == 0 || column == p->width - 1)) { line_arg = 0; end_arg = column != 0; }
				else if (p && row >= 0 && column < 0 && (row == 0 || row == p->height - 1)) { line_arg = 1; end_arg = row != 0; }
				else { line_arg = 1; }
			}

			const bool ok = done(edit::patch_extrude_terrain(doc, line_arg ? patch_line::row : patch_line::column, end_arg != 0), line_arg ? "Extrude terrain row" : "Extrude terrain column");
			if (ok) { g.vsel.clear(); }
			return ok;
		}

		bool op_split(int line_arg = -1, int index = -1)
		{
			if (!need_map()) { return false; }

			if (line_arg < 0)
			{
				const int column = selected_line(patch_line::column), row = selected_line(patch_line::row);
				if (row >= 0 && column < 0) { line_arg = 1; index = row; }
				else { line_arg = 0; index = column; }
			}

			const bool ok = done(edit::patch_split(get_scene().doc, line_arg ? patch_line::row : patch_line::column, index), "Split patch");
			if (ok) { g.vsel.clear(); }
			return ok;
		}

		bool op_turn_edges()
		{
			if (!need_map()) { return false; }
			auto& doc = get_scene().doc;

			// with selected vertices: the quads they start
			const auto* p = vsel_patch();
			if (p && !g.vsel.empty() && p->type == edit::patch_type::mesh)
			{
				doc.begin_transaction("Turn terrain edges");
				bool any = false;
				for (const int i : g.vsel)
				{
					any = edit::patch_turn_edge(doc, g.vsel_patch, i / p->height, i % p->height) || any;
				}

				doc.end_transaction();
				return done(any, "Turn terrain edges");
			}

			return done(edit::patch_turn_edges(doc), "Turn terrain edges");
		}

		bool op_color(const std::uint8_t rgba[4])
		{
			if (!need_map()) { return false; }
			const object_id id = active_patch();
			if (!id) { return fail("Vertex colour", "Select a patch."); }

			return done(edit::patch_set_color(get_scene().doc, id, id == g.vsel_patch ? std::span<const int>(g.vsel) : std::span<const int>(), rgba), "Vertex colour");
		}

		bool op_noise(const edit::vec3& scale, const std::uint32_t seed)
		{
			if (!need_map()) { return false; }
			auto& doc = get_scene().doc;
			const object_id active = active_patch();
			if (!active) { return fail("Vertex noise", "Select a patch."); }

			if (active == g.vsel_patch && !g.vsel.empty())
			{
				return done(edit::patch_add_noise(doc, active, g.vsel, scale, seed), "Vertex noise");
			}

			// every selected patch, one step
			doc.begin_transaction("Vertex noise");
			bool any = false;
			const std::vector<object_id> ids(doc.selection().begin(), doc.selection().end());
			for (const object_id id : ids)
			{
				if (doc.find_patch(id))
				{
					any = edit::patch_add_noise(doc, id, {}, scale, seed) || any;
				}
			}

			doc.end_transaction();
			return done(any, "Vertex noise");
		}

		void set_patches_as(const int mode)
		{
			// one setting for the Patch menu and Renderer > Show Patches As ( menu_renderer.cpp ): the meshes of
			// the patches are made again ( or dropped ) there, and the 3D view builds its lines again
			command::execute(mode != 0 ? "r_patches wireframe" : "r_patches textured");
			get_scene().statics_serial++;
		}

		// ---- menu helpers ----------------------------------------------------------------------------------

		// a menu item that says why it is grey
		bool item(const char* label, const char* shortcut, const bool enabled, const char* why, const char* tip = nullptr)
		{
			const bool clicked = editor::menu::item(label, shortcut, false, enabled);

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				if (!enabled && why) { ImGui::SetTooltip("%s", why); }
				else if (enabled && tip) { ImGui::SetTooltip("%s", tip); }
			}

			return clicked;
		}

		bool toggle(const char* label, bool& value, const char* tip)
		{
			const bool clicked = editor::menu::item(label, nullptr, &value);
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", tip); }
			return clicked;
		}

		void section(const char* title)
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextDisabled("%s", title);
		}

		void axis_radio(int& axis, const bool with_auto)
		{
			if (with_auto)
			{
				ImGui::RadioButton("Auto", &axis, -1);
				ImGui::SameLine();
			}

			ImGui::RadioButton("X", &axis, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Y", &axis, 1);
			ImGui::SameLine();
			ImGui::RadioButton("Z", &axis, 2);
		}

		const char* const NO_MAP = "No map is open.";
		const char* const NO_BOX = "Select the brush whose box the shape is to fill, or draw a box in a 2D view first.";
		const char* const NO_PATCH = "Select a patch.";
		const char* const NO_CURVE = "Select a curve patch.";
		const char* const NO_TERRAIN = "Select a terrain patch ( mesh ).";
	}

	bool patches_as_wire()
	{
		return render_modes::hide_patch_meshes();
	}

	// ---- the 2D vertex mode ------------------------------------------------------------------------------------

	void patch_vertices_grabbed(const object_id patch, std::vector<int>& vertices, const int view_axis)
	{
		const auto* p = get_scene().doc.find_patch(patch);
		if (!p || vertices.empty())
		{
			return;
		}

		// without drill down only the vertex nearest to the viewer ( who looks down the axis ) is taken
		if (!g.drill_down && vertices.size() > 1 && view_axis >= 0 && view_axis <= 2)
		{
			int nearest = vertices[0];
			for (const int i : vertices)
			{
				if (p->vertices[static_cast<std::size_t>(i)].xyz[view_axis] > p->vertices[static_cast<std::size_t>(nearest)].xyz[view_axis])
				{
					nearest = i;
				}
			}

			vertices = { nearest };
		}

		select_vertices(patch, vertices, ImGui::GetIO().KeyShift && g.vsel_patch == patch);
	}

	bool move_patch_vertices(const object_id patch, const std::vector<int>& vertices, const edit::vec3& delta)
	{
		return edit::move_patch_vertices_welded(get_scene().doc, patch, vertices, delta, g.weld_moves ? weld_tolerance() : -1.0, locked_of(patch));
	}

	void patch_vertex_marks(const std::function<void(const edit::vec3&, ImU32)>& mark)
	{
		auto& s = get_scene();
		if (!s.open)
		{
			return;
		}

		for (const auto& [id, list] : g.locked)
		{
			const auto* p = s.doc.is_selected(id) ? s.doc.find_patch(id) : nullptr;
			for (const int i : list)
			{
				if (p && i >= 0 && i < static_cast<int>(p->vertices.size()))
				{
					mark(p->vertices[static_cast<std::size_t>(i)].xyz, IM_COL32(255, 80, 80, 255));
				}
			}
		}

		if (const auto* p = vsel_patch())
		{
			for (const int i : g.vsel)
			{
				mark(p->vertices[static_cast<std::size_t>(i)].xyz, IM_COL32(255, 220, 60, 255));
			}
		}
	}

	bool patch_edge_click(const float3& origin, const float3& direction)
	{
		auto& s = get_scene();
		if (!g.edge_mode || !s.open)
		{
			return false;
		}

		const edit::ray ray{ { origin.x, origin.y, origin.z }, { direction.x, direction.y, direction.z } };
		edit::pick_options options;
		options.brushes = false;
		options.entities = false;

		edit::pick_result hit;
		int column = 0, row = 0;
		double distance = 0.0;

		if (!s.doc.pick(ray, hit, options) || !edit::patch_pick_quad(s.doc, hit.object, ray, column, row, distance))
		{
			return false;
		}

		done(edit::patch_turn_edge(s.doc, hit.object, column, row), "Turn terrain edge");
		return true;
	}

	// ---- shortcuts ---------------------------------------------------------------------------------------------

	// the keys of these menus are actions: action_table.cpp ( patch.* )

	// ---- menus -------------------------------------------------------------------------------------------------

	void draw_brush_menu()
	{
		const bool open = get_scene().open;
		const selection_info info = inspect();
		const bool can = open && info.box;
		const char* why = open ? NO_BOX : NO_MAP;

		if (item("Arbitrary Sided Cylinder ..", nullptr, can, why, "A brush with any number of sides around an axis, filling the box of the selected brush."))
		{
			g.brush_shape_index = static_cast<int>(brush_shape::prism);
			g.show_brush = true;
		}

		if (editor::menu::begin("N Sided", can))
		{
			for (int n = 3; n <= 9; n++)
			{
				const std::string label = std::to_string(n) + " sides";
				if (editor::menu::item(label.c_str())) { make_brush(brush_shape::prism, n, g.axis); }
			}

			editor::menu::end();
		}

		if (item("Cone ..", nullptr, can, why)) { g.brush_shape_index = static_cast<int>(brush_shape::cone); g.show_brush = true; }
		if (item("Sphere ..", nullptr, can, why)) { g.brush_shape_index = static_cast<int>(brush_shape::sphere); g.show_brush = true; }
		ImGui::Separator();

		if (item("Box", nullptr, can, why, "The selected brushes become one box around them.")) { make_brush(brush_shape::box, 0, 2); }
		if (item("Cylinder", nullptr, can, why, "8 sides around the Z axis.")) { make_brush(brush_shape::prism, 8, 2); }
		if (item("Wedge", nullptr, can, why, "The box cut along its diagonal.")) { make_brush(brush_shape::wedge, 0, 0); }
		ImGui::Separator();

		if (item("From Points ..", nullptr, open, NO_MAP, "A brush from the convex hull of a list of points.")) { g.show_points = true; }
	}

	void draw_patch_menu()
	{
		auto& s = get_scene();
		auto& doc = s.doc;
		const bool open = s.open;
		const selection_info info = inspect();

		const bool box = open && info.box;
		const bool any = open && info.patches() > 0;
		const bool curve = open && info.curves > 0;
		const bool terrain = open && info.terrains > 0;

		const char* why_box = open ? NO_BOX : NO_MAP;
		const char* why_any = open ? NO_PATCH : NO_MAP;
		const char* why_curve = open ? NO_CURVE : NO_MAP;
		const char* why_terrain = open ? NO_TERRAIN : NO_MAP;

		if (item("Simple Curve Patch ..", editor::menu::key("patch.simple_curve"), box, why_box)) { g.show_simple = true; g.simple_terrain = false; }
		if (item("Simple Terrain Patch ..", nullptr, box, why_box)) { g.show_simple = true; g.simple_terrain = true; }
		if (item("Curve To Terrain", nullptr, curve, why_curve, "The selected curves become terrain meshes with the vertices they are drawn with.")) { done(edit::curve_to_terrain(doc), "Curve to terrain"); }
		if (item("Faces To Terrain", nullptr, open && (info.brushes > 0 || info.faces > 0), open ? "Select brushes or brush faces." : NO_MAP,
			"One terrain patch per selected face ( or per face of the selected brushes ) with 3 or 4 corners."))
		{
			done(edit::faces_to_terrain(doc, false), "Faces to terrain");
		}

		ImGui::Separator();

		if (editor::menu::begin("Primitives"))
		{
			for (int i = 0; i < static_cast<int>(std::size(PATCH_SHAPE_LABELS)); i++)
			{
				if (i == 4) { ImGui::Separator(); }
				if (item(PATCH_SHAPE_LABELS[i], nullptr, box, why_box)) { make_shape(static_cast<patch_shape>(i), g.axis, g.delete_original); }
			}

			ImGui::Separator();
			ImGui::TextDisabled("Axis");
			ImGui::SameLine();
			axis_radio(g.axis, false);
			ImGui::Checkbox("Delete the brush the box came from", &g.delete_original);
			editor::menu::end();
		}

		ImGui::Separator();

		if (item("Vert Edit Dialog", nullptr, open, NO_MAP, "Vertex colours and noise.")) { g.show_vertex = true; }
		if (item("Advanced Edit Dialog", editor::menu::key("patch.advanced_edit"), open, NO_MAP, "The control points of the selected patch as a table: select rows and columns, edit positions and texture coordinates.")) { g.show_advanced = true; }
		ImGui::Separator();

		if (editor::menu::begin("Insert"))
		{
			if (item("Insert 2 Columns", nullptr, any, why_any, "At the start ( or in the segment of the selected vertices ); a curve keeps its shape.")) { op_insert(patch_line::column, false); }
			if (item("Insert 2 Rows", nullptr, any, why_any)) { op_insert(patch_line::row, false); }
			if (item("Add 2 Columns", editor::menu::key("patch.add_columns"), any, why_any, "At the end.")) { op_insert(patch_line::column, true); }
			if (item("Add 2 Rows", editor::menu::key("patch.add_rows"), any, why_any)) { op_insert(patch_line::row, true); }
			if (item("Add Terrain Row / Column", nullptr, terrain, why_terrain, "One line after the row / column of the selected vertices; without them a column in the last cell.")) { op_terrain_add(); }
			if (item("Extrude Terrain Row", nullptr, terrain, why_terrain, "One more line beyond the edge the selected vertices lie on; without them beyond the last row.")) { op_extrude(); }
			editor::menu::end();
		}

		if (editor::menu::begin("Delete"))
		{
			if (item("First 2 Columns", nullptr, any, why_any)) { done(edit::patch_delete(doc, patch_line::column, false), "Delete first 2 columns"); }
			if (item("First 2 Rows", nullptr, any, why_any)) { done(edit::patch_delete(doc, patch_line::row, false), "Delete first 2 rows"); }
			if (item("Last 2 Columns", editor::menu::key("patch.delete_columns"), any, why_any)) { done(edit::patch_delete(doc, patch_line::column, true), "Delete last 2 columns"); }
			if (item("Last 2 Rows", editor::menu::key("patch.delete_rows"), any, why_any)) { done(edit::patch_delete(doc, patch_line::row, true), "Delete last 2 rows"); }
			if (item("Remove Terrain Row / Column", nullptr, terrain, why_terrain, "The row / column of the selected vertices; without them the last column.")) { op_terrain_remove(); }
			editor::menu::end();
		}

		if (editor::menu::begin("Matrix"))
		{
			if (item("Transpose", editor::menu::key("patch.transpose"), any, why_any, "Rows become columns; the patch then faces the other way.")) { done(edit::patch_transpose(doc), "Transpose patch"); }
			if (item("Re-disperse Vertices", nullptr, any, why_any, "Even spacing along the lines the control points form, both ways.")) { done(edit::patch_redisperse(doc, edit::patch_disperse::vertices), "Redisperse vertices"); }
			if (item("Re-disperse Columns", nullptr, any, why_any, "The columns evenly on the straight line from the first to the last.")) { done(edit::patch_redisperse(doc, edit::patch_disperse::columns), "Redisperse columns"); }
			if (item("Re-disperse Rows", nullptr, any, why_any)) { done(edit::patch_redisperse(doc, edit::patch_disperse::rows), "Redisperse rows"); }
			editor::menu::end();
		}

		if (editor::menu::begin("Weld"))
		{
			if (item("Connect Selection / Weld", nullptr, any, why_any, "Vertices of the selected patches that lie closer together than the tolerance become one place.")) { done(edit::patch_weld(doc, weld_tolerance()), "Weld patches"); }
			if (item("Weld to Brush Vertices", nullptr, any, why_any, "Vertices of the selected patches move onto brush corners within the tolerance.")) { done(edit::patch_weld_to_brushes(doc, weld_tolerance()), "Weld to brushes"); }
			ImGui::Separator();
			toggle("Tolerant Weld", g.tolerant_weld, "Weld over one grid step instead of 0.1 units.");
			toggle("Weld Equal Patch Points During Moves", g.weld_moves, "A vertex dragged in a 2D view takes along every vertex of the selected patches in the same place\n( the seam of a cylinder, the edge two patches share ).");
			item("Weld to Model Vertices", nullptr, false, "Not available: the editor does not read the vertices of placed models for snapping yet.");
			item("Weld to Prefab Vertices", nullptr, false, "Not available: prefabs are not expanded into editable geometry.");
			editor::menu::end();
		}

		if (item("Inc Subdivision", nullptr, curve, why_curve, "Curve patches: more triangles ( in the views and in the compiled map ).")) { done(edit::patch_change_subdivision(doc, 1), "Increase subdivision"); }
		if (item("Dec Subdivision", nullptr, curve, why_curve, "Curve patches: fewer triangles.")) { done(edit::patch_change_subdivision(doc, -1), "Decrease subdivision"); }
		if (item("Split", nullptr, any, why_any, "Two patches: at the row / column of the selected vertices, else in the middle along the columns.")) { op_split(); }
		if (item("Invert", editor::menu::key("patch.invert"), any, why_any, "The patch faces the other way.")) { done(edit::patch_invert(doc), "Invert patch"); }
		if (item("Thicken ..", editor::menu::key("patch.thicken"), any, why_any)) { g.show_thicken = true; }
		if (item("Cap ..", editor::menu::key("patch.cap"), curve, why_curve)) { g.show_cap = true; }
		ImGui::Separator();

		if (item("Turn Terrain Edges", nullptr, terrain, why_terrain, "Turns the diagonal of the quads the selected vertices start; without them of every quad.")) { op_turn_edges(); }
		if (item("Lock Vertices", nullptr, any, why_any, "The selected vertices no longer move when vertices are dragged.")) { lock_vertices(true, false); }
		if (item("Unlock Vertices", nullptr, any, why_any, "Unlocks the selected vertices; without a selection all of the patch.")) { lock_vertices(false, false); }
		ImGui::Separator();

		if (item("Cycle Cap Texture", editor::menu::key("patch.cycle_cap_texture"), any, why_any, "Projects the texture along the next world axis.")) { done(edit::patch_cycle_cap_texture(doc), "Cycle cap texture"); }
		if (item("Cap Texture", nullptr, any, why_any, "Projects the texture flat onto the patch, along the axis it faces most.")) { done(edit::patch_cap_texture(doc), "Cap texture"); }
		if (item("Natural Texture", nullptr, any, why_any, "The texture follows the surface at radiant's default scale.")) { done(edit::patch_natural_texture(doc), "Natural texture"); }
		if (item("Fit Texture", editor::menu::key("texture.fit"), any, why_any, "The texture covers the patch once.")) { done(edit::patch_fit_texture(doc), "Fit texture"); }
		ImGui::Separator();

		// the toolbar toggles of radiant
		bool wire = patches_as_wire();
		if (toggle("Show Patches As Wireframe", wire, "Patches are drawn as lines only.")) { set_patches_as(wire ? 1 : 0); }
		toggle("Drill Down Vertices", g.drill_down, "A vertex grabbed in a 2D view takes the ones that lie behind it along ( a whole row / column seen end on ).");
		toggle("Cycle Terrain Edge Mode", g.edge_mode, "A click on a quad of a terrain patch in the 3D view turns its diagonal.");
	}

	// ---- dialogs -----------------------------------------------------------------------------------------------

	namespace
	{
		void draw_brush_dialog()
		{
			if (!g.show_brush)
			{
				return;
			}

			ImGui::SetNextWindowSize(ImVec2(330.0f, 0.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(460.0f, 120.0f), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Brush primitive##window", &g.show_brush, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Combo("Shape", &g.brush_shape_index, SHAPE_NAMES, static_cast<int>(std::size(SHAPE_NAMES)));
				const auto shape = static_cast<brush_shape>(g.brush_shape_index);

				if (shape == brush_shape::sphere)
				{
					g.sides = std::clamp(g.sides, 4, 16);
					ImGui::SliderInt("Sides", &g.sides, 4, 16);
				}
				else if (shape == brush_shape::prism || shape == brush_shape::cone)
				{
					g.sides = std::clamp(g.sides, 3, 64);
					ImGui::SliderInt("Sides", &g.sides, 3, 64);
				}

				if (shape != brush_shape::box)
				{
					ImGui::TextDisabled("Axis");
					ImGui::SameLine();
					axis_radio(g.axis, false);
				}

				const selection_info info = inspect();
				ImGui::TextDisabled(info.brushes ? "Replaces the selected brush, in its box." : (info.box ? "Fills the last box that was selected." : NO_BOX));

				ImGui::BeginDisabled(!get_scene().open || !info.box);
				if (ImGui::Button("Make", ImVec2(110.0f, 0.0f))) { make_brush(shape, g.sides, g.axis); }
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("Close", ImVec2(110.0f, 0.0f))) { g.show_brush = false; }
			}

			ImGui::End();
		}

		void draw_points_dialog()
		{
			if (!g.show_points)
			{
				return;
			}

			ImGui::SetNextWindowSize(ImVec2(360.0f, 300.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(460.0f, 330.0f), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Brush from points##window", &g.show_points, ImGuiWindowFlags_NoCollapse))
			{
				ImGui::TextDisabled("x y z of four or more points, in any order:");
				ImGui::InputTextMultiline("##points", &g.points_text, ImVec2(-1.0f, -ImGui::GetFrameHeightWithSpacing() * 1.3f));

				const auto points = parse_points(g.points_text);
				ImGui::BeginDisabled(!get_scene().open || points.size() < 4);
				if (ImGui::Button("Make", ImVec2(110.0f, 0.0f))) { brush_from_points(points); }
				ImGui::EndDisabled();
				ImGui::SameLine();
				ImGui::TextDisabled("%d points", static_cast<int>(points.size()));
			}

			ImGui::End();
		}

		void draw_simple_dialog()
		{
			if (!g.show_simple)
			{
				return;
			}

			ImGui::SetNextWindowPos(ImVec2(480.0f, 140.0f), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Simple patch##window", &g.show_simple, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
			{
				int kind = g.simple_terrain ? 1 : 0;
				ImGui::RadioButton("Curve", &kind, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Terrain", &kind, 1);
				g.simple_terrain = kind == 1;

				ImGui::SetNextItemWidth(120.0f);
				ImGui::InputInt("Width ( columns )", &g.simple_width, g.simple_terrain ? 1 : 2);
				ImGui::SetNextItemWidth(120.0f);
				ImGui::InputInt("Height ( rows )", &g.simple_height, g.simple_terrain ? 1 : 2);

				if (g.simple_terrain)
				{
					g.simple_width = std::clamp(g.simple_width, 2, 16);
					g.simple_height = std::clamp(g.simple_height, 2, 16);
				}
				else
				{
					g.simple_width = std::clamp(g.simple_width | 1, 3, edit::PATCH_MAX_LINES);
					g.simple_height = std::clamp(g.simple_height | 1, 3, edit::PATCH_MAX_LINES);
				}

				ImGui::TextDisabled("Plane across");
				ImGui::SameLine();
				axis_radio(g.simple_axis, true);
				ImGui::Checkbox("Delete Original Selection", &g.delete_original);

				const selection_info info = inspect();
				if (!info.box) { ImGui::TextDisabled("%s", NO_BOX); }

				ImGui::BeginDisabled(!get_scene().open || !info.box);
				if (ImGui::Button("Ok", ImVec2(110.0f, 0.0f)))
				{
					if (make_simple(g.simple_terrain, g.simple_width, g.simple_height, g.simple_axis, g.delete_original)) { g.show_simple = false; }
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) { g.show_simple = false; }
			}

			ImGui::End();
		}

		void draw_thicken_dialog()
		{
			if (!g.show_thicken)
			{
				return;
			}

			ImGui::SetNextWindowPos(ImVec2(820.0f, 140.0f), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Thicken patch##window", &g.show_thicken, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::SetNextItemWidth(140.0f);
				ImGui::InputFloat("Thickness", &g.thickness, 1.0f, 8.0f, "%.2f");
				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Units behind the visible side; negative: in front of it."); }
				ImGui::Checkbox("Create Seams", &g.seams);
				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Strips along the edges that close the gap between the two surfaces."); }

				const selection_info info = inspect();
				ImGui::BeginDisabled(info.patches() == 0);
				if (ImGui::Button("Ok", ImVec2(110.0f, 0.0f)))
				{
					if (done(edit::patch_thicken(get_scene().doc, g.thickness, g.seams), "Thicken patch")) { g.show_thicken = false; }
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) { g.show_thicken = false; }
				if (info.patches() == 0) { ImGui::TextDisabled("%s", NO_PATCH); }
			}

			ImGui::End();
		}

		void draw_cap_dialog()
		{
			if (!g.show_cap)
			{
				return;
			}

			ImGui::SetNextWindowPos(ImVec2(820.0f, 300.0f), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Cap patch##window", &g.show_cap, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
			{
				for (int i = 0; i < static_cast<int>(std::size(CAP_LABELS)); i++)
				{
					ImGui::RadioButton(CAP_LABELS[i], &g.cap_type, i);
				}

				ImGui::Separator();
				ImGui::Checkbox("First row", &g.cap_first);
				ImGui::SameLine();
				ImGui::Checkbox("Last row", &g.cap_last);

				const selection_info info = inspect();
				ImGui::BeginDisabled(info.curves == 0 || (!g.cap_first && !g.cap_last));
				if (ImGui::Button("Ok", ImVec2(110.0f, 0.0f)))
				{
					if (done(edit::patch_cap(get_scene().doc, static_cast<edit::patch_cap_type>(g.cap_type), g.cap_first, g.cap_last), "Cap patch")) { g.show_cap = false; }
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) { g.show_cap = false; }
				if (info.curves == 0) { ImGui::TextDisabled("%s", NO_CURVE); }
			}

			ImGui::End();
		}

		void draw_vertex_dialog()
		{
			if (!g.show_vertex)
			{
				return;
			}

			ImGui::SetNextWindowPos(ImVec2(1080.0f, 140.0f), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Patch vertices##window", &g.show_vertex, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
			{
				const object_id id = active_patch();
				const auto* p = get_scene().open ? get_scene().doc.find_patch(id) : nullptr;

				if (!p)
				{
					ImGui::TextDisabled("%s", NO_PATCH);
				}
				else if (id == g.vsel_patch && !g.vsel.empty())
				{
					ImGui::Text("Patch #%u: %d selected vertices", id, static_cast<int>(g.vsel.size()));
				}
				else
				{
					ImGui::Text("Patch #%u: all %d vertices", id, static_cast<int>(p->vertices.size()));
				}

				ImGui::BeginDisabled(!p);
				section("Vertex colour");
				ImGui::SetNextItemWidth(240.0f);
				ImGui::ColorPicker4("##vertex_color", g.color, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB);

				if (ImGui::Button("Apply colour", ImVec2(240.0f, 0.0f)))
				{
					std::uint8_t rgba[4];
					for (int i = 0; i < 4; i++)
					{
						rgba[i] = static_cast<std::uint8_t>(std::clamp(g.color[i] * 255.0f + 0.5f, 0.0f, 255.0f));
					}

					op_color(rgba);
				}

				section("Noise");
				ImGui::SetNextItemWidth(240.0f);
				ImGui::DragFloat3("XYZ scale", g.noise, 0.1f, 0.0f, 4096.0f, "%.2f");
				ImGui::SetNextItemWidth(120.0f);
				ImGui::InputInt("Seed", &g.noise_seed);

				if (ImGui::Button("Add noise", ImVec2(240.0f, 0.0f)))
				{
					op_noise({ g.noise[0], g.noise[1], g.noise[2] }, static_cast<std::uint32_t>(g.noise_seed));
					g.noise_seed++;
				}

				ImGui::EndDisabled();
			}

			ImGui::End();
		}

		void draw_advanced_dialog()
		{
			if (!g.show_advanced)
			{
				return;
			}

			ImGui::SetNextWindowSize(ImVec2(520.0f, 560.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(440.0f, 360.0f), ImGuiCond_FirstUseEver);
			if (!ImGui::Begin("Patch: advanced edit##window", &g.show_advanced, ImGuiWindowFlags_NoCollapse))
			{
				ImGui::End();
				return;
			}

			auto& s = get_scene();
			auto& doc = s.doc;
			const object_id id = s.open ? active_patch() : edit::no_object;
			const auto* p = doc.find_patch(id);

			if (!p)
			{
				ImGui::TextDisabled("%s", NO_PATCH);
				ImGui::End();
				return;
			}

			if (g.vsel_patch != id)
			{
				select_vertices(id, {}, false);
			}

			const bool curve = p->type == edit::patch_type::curve;
			ImGui::Text("#%u  %s  %d columns x %d rows", id, curve ? "curve" : "terrain mesh", p->width, p->height);
			ImGui::TextDisabled("%s%s%s", p->material.c_str(), p->smoothing.empty() ? "" : "   smoothing ", p->smoothing.c_str());

			if (curve)
			{
				ImGui::Text("Subdivision: %d quads per segment", edit::patch_subdivisions(*p));
				ImGui::SameLine();
				if (ImGui::SmallButton("-##subdiv")) { done(edit::patch_change_subdivision(doc, -1), "Decrease subdivision"); }
				ImGui::SameLine();
				if (ImGui::SmallButton("+##subdiv")) { done(edit::patch_change_subdivision(doc, 1), "Increase subdivision"); }
			}

			// ---- the control points: rows downwards, the last row on top ( as the patch stands when its rows go up )
			p = doc.find_patch(id);
			section("Control points");

			const bool add = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
			const auto index_of = [&](const int c, const int r) { return c * p->height + r; };
			const auto selected = [&](const int i) { return std::find(g.vsel.begin(), g.vsel.end(), i) != g.vsel.end(); };
			const auto& locked = locked_of(id);
			const ImVec2 cell(24.0f, 20.0f);

			if (ImGui::BeginChild("##matrix", ImVec2(0.0f, std::min(cell.y * (p->height + 1) + 34.0f, 300.0f)), true, ImGuiWindowFlags_HorizontalScrollbar))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));

				ImGui::Dummy(ImVec2(30.0f, cell.y));
				for (int c = 0; c < p->width; c++)
				{
					ImGui::SameLine();
					ImGui::PushID(c);
					if (ImGui::Button(std::to_string(c).c_str(), cell))
					{
						std::vector<int> list;
						for (int r = 0; r < p->height; r++) { list.push_back(index_of(c, r)); }
						select_vertices(id, list, add);
					}
					if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Column %d", c); }
					ImGui::PopID();
				}

				for (int r = p->height - 1; r >= 0; r--)
				{
					ImGui::PushID(1000 + r);
					if (ImGui::Button(("r" + std::to_string(r)).c_str(), ImVec2(30.0f, cell.y)))
					{
						std::vector<int> list;
						for (int c = 0; c < p->width; c++) { list.push_back(index_of(c, r)); }
						select_vertices(id, list, add);
					}
					if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Row %d", r); }

					for (int c = 0; c < p->width; c++)
					{
						const int i = index_of(c, r);
						const bool sel = selected(i);
						const bool lock = std::find(locked.begin(), locked.end(), i) != locked.end();
						const bool on_curve = !curve || (!(c & 1) && !(r & 1));

						ImGui::SameLine();
						ImGui::PushID(c);
						ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(0.85f, 0.70f, 0.15f, 1.0f) : (on_curve ? ImVec4(0.22f, 0.40f, 0.32f, 1.0f) : ImVec4(0.20f, 0.21f, 0.23f, 1.0f)));

						if (ImGui::Button(lock ? "L" : (p->at(c, r).has_flag && p->at(c, r).flag ? "/" : ""), cell))
						{
							if (add && sel) { std::erase(g.vsel, i); }
							else { select_vertices(id, { i }, add); }
						}

						ImGui::PopStyleColor();
						if (ImGui::IsItemHovered())
						{
							const auto& v = p->at(c, r);
							ImGui::SetTooltip("column %d, row %d ( vertex %d )\n%.6g %.6g %.6g%s%s", c, r, i, v.xyz.x, v.xyz.y, v.xyz.z, lock ? "\nlocked" : "",
								curve ? (on_curve ? "\non the surface" : "\ncontrol point") : "");
						}

						ImGui::PopID();
					}

					ImGui::PopID();
				}

				ImGui::PopStyleVar();
			}

			ImGui::EndChild();

			if (ImGui::Button("All"))
			{
				std::vector<int> list(p->vertices.size());
				std::iota(list.begin(), list.end(), 0);
				select_vertices(id, list, false);
			}
			ImGui::SameLine();
			if (ImGui::Button("None")) { g.vsel.clear(); }
			ImGui::SameLine();
			if (ImGui::Button("Lock")) { lock_vertices(true, false); }
			ImGui::SameLine();
			if (ImGui::Button("Unlock")) { lock_vertices(false, false); }
			ImGui::SameLine();
			ImGui::TextDisabled("%d selected  ( Ctrl / Shift + click adds )", static_cast<int>(g.vsel.size()));

			// ---- the first selected vertex
			if (!g.vsel.empty())
			{
				const int i = g.vsel[0];
				edit::patch_vertex v = p->vertices[static_cast<std::size_t>(i)];
				if (v.tex.size() < 4) { v.tex.resize(4, 0.0); }
				bool changed = false;

				section(("Vertex " + std::to_string(i) + "  ( column " + std::to_string(i / p->height) + ", row " + std::to_string(i % p->height) + " )").c_str());
				ImGui::PushItemWidth(300.0f);

				double xyz[3] = { v.xyz.x, v.xyz.y, v.xyz.z };
				if (ImGui::InputScalarN("Position", ImGuiDataType_Double, xyz, 3, nullptr, nullptr, "%.6g", ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit())
				{
					changed = changed || !(edit::vec3{ xyz[0], xyz[1], xyz[2] } == v.xyz);
					v.xyz = { xyz[0], xyz[1], xyz[2] };
				}

				double st[2] = { v.tex[0], v.tex[1] };
				if (ImGui::InputScalarN("Texture s t ( texels )", ImGuiDataType_Double, st, 2, nullptr, nullptr, "%.6g", ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit())
				{
					changed = changed || st[0] != v.tex[0] || st[1] != v.tex[1];
					v.tex[0] = st[0];
					v.tex[1] = st[1];
				}

				double lm[2] = { v.tex[2], v.tex[3] };
				if (ImGui::InputScalarN("Lightmap s t", ImGuiDataType_Double, lm, 2, nullptr, nullptr, "%.6g", ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit())
				{
					changed = changed || lm[0] != v.tex[2] || lm[1] != v.tex[3];
					v.tex[2] = lm[0];
					v.tex[3] = lm[1];
				}

				ImGui::PopItemWidth();

				if (!curve && i / p->height + 1 < p->width && i % p->height + 1 < p->height)
				{
					bool turned = v.has_flag && v.flag != 0;
					if (ImGui::Checkbox("Turned edge ( the quad this vertex starts )", &turned))
					{
						v.has_flag = turned;
						v.flag = turned ? 1 : 0;
						changed = true;
					}
				}

				if (changed)
				{
					done(edit::patch_set_vertex(doc, id, i, v), "Edit vertex");
				}

				// ---- all selected vertices
				static float move[3] = { 0.0f, 0.0f, 8.0f };
				ImGui::SetNextItemWidth(300.0f);
				ImGui::InputFloat3("##move_by", move, "%.3f");
				ImGui::SameLine();
				if (ImGui::Button("Move selected"))
				{
					done(move_patch_vertices(id, g.vsel, { move[0], move[1], move[2] }), "Move vertices");
				}
			}

			ImGui::End();
		}
	}

	void draw_brush_patch_windows()
	{
		vsel_patch();
		draw_brush_dialog();
		draw_points_dialog();
		draw_simple_dialog();
		draw_thicken_dialog();
		draw_cap_dialog();
		draw_vertex_dialog();
		draw_advanced_dialog();
	}

	// ---- console commands --------------------------------------------------------------------------------------

	void register_brush_patch_commands()
	{
		// ---- brush_<shape> [sides] [x|y|z] [at x1 y1 z1 x2 y2 z2]: the selected brushes become the shape ( in their
		// box ); without a selection the shape fills the last box that was selected, or the box given with `at`
		command::register_command("brush_box", [](args_t args) { make_brush(brush_shape::box, 0, 2, args); });
		command::register_command("brush_wedge", [](args_t args) { make_brush(brush_shape::wedge, 0, axis_arg(plain_args(args), 0), args); });

		const auto sided = [](const brush_shape shape, const int default_sides)
		{
			return [shape, default_sides](args_t args)
			{
				const auto words = plain_args(args);
				const int sides = !words.empty() && is_number(words[0]) ? std::atoi(words[0].c_str()) : default_sides;
				make_brush(shape, sides, axis_arg(words, 2), args);
			};
		};

		command::register_command("brush_cylinder", sided(brush_shape::prism, 8));
		command::register_command("brush_sided", sided(brush_shape::prism, 8));
		command::register_command("brush_cone", sided(brush_shape::cone, 8));
		command::register_command("brush_sphere", sided(brush_shape::sphere, 8));

		// brush_points x y z x y z ..: the convex hull of the points
		command::register_command("brush_points", [](args_t args)
		{
			std::string text;
			for (std::size_t i = 1; i < args.size(); i++) { text += args[i] + " "; }

			const auto points = parse_points(text);
			if (points.size() < 4) { game::printf_to_console("usage: brush_points x y z x y z x y z x y z ..  ( four points or more )\n"); return; }
			brush_from_points(points);
		});

		// brush_dialog [box|wedge|cylinder|cone|sphere]
		command::register_command("brush_dialog", [](args_t args)
		{
			static const char* const keys[] = { "box", "wedge", "cylinder", "cone", "sphere" };
			for (int i = 0; i < 5; i++)
			{
				if (args.size() > 1 && args[1] == keys[i]) { g.brush_shape_index = i; }
			}

			g.show_brush = !(args.size() > 1 && args[1] == "close");
		});

		// ---- patch_curve [columns rows] [x|y|z] [keep] [at ..], patch_terrain ..: a flat patch on the box
		const auto simple = [](const bool terrain)
		{
			return [terrain](args_t args)
			{
				const auto words = plain_args(args);
				const int width = words.size() > 0 && is_number(words[0]) ? std::atoi(words[0].c_str()) : 3;
				const int height = words.size() > 1 && is_number(words[1]) ? std::atoi(words[1].c_str()) : width;
				const bool keep = std::find(words.begin(), words.end(), "keep") != words.end();
				make_simple(terrain, width, height, axis_arg(words, -1), !keep, args);
			};
		};

		command::register_command("patch_curve", simple(false));
		command::register_command("patch_terrain", simple(true));

		// patch_prim <bevel|square_bevel|endcap|square_endcap|cylinder|dense_cylinder|very_dense_cylinder|square_cylinder|cone> [x|y|z] [keep] [at ..]
		command::register_command("patch_prim", [](args_t args)
		{
			const auto words = plain_args(args);
			for (int i = 0; i < static_cast<int>(std::size(PATCH_SHAPE_KEYS)); i++)
			{
				if (!words.empty() && words[0] == PATCH_SHAPE_KEYS[i])
				{
					const bool keep = std::find(words.begin(), words.end(), "keep") != words.end();
					make_shape(static_cast<patch_shape>(i), axis_arg({ words.begin() + 1, words.end() }, 2), !keep, args);
					return;
				}
			}

			game::printf_to_console("usage: patch_prim <bevel|square_bevel|endcap|square_endcap|cylinder|dense_cylinder|very_dense_cylinder|square_cylinder|cone> [x|y|z] [keep] [at x1 y1 z1 x2 y2 z2]\n");
		});

		command::register_command("patch_curve_to_terrain", [](args_t) { if (need_map()) { done(edit::curve_to_terrain(get_scene().doc), "Curve to terrain"); } });
		// patch_faces_to_terrain [delete]
		command::register_command("patch_faces_to_terrain", [](args_t args)
		{
			if (need_map()) { done(edit::faces_to_terrain(get_scene().doc, args.size() > 1 && args[1] == "delete"), "Faces to terrain"); }
		});

		// ---- rows and columns
		const auto line_of = [](const std::string& word, int& line)
		{
			if (word == "column" || word == "columns" || word == "col") { line = 0; return true; }
			if (word == "row" || word == "rows") { line = 1; return true; }
			return false;
		};

		// patch_insert <columns|rows> [index]   ( at the start, or in the segment of control line `index` )
		command::register_command("patch_insert", [line_of](args_t args)
		{
			int line = 0;
			if (!need_map() || args.size() < 2 || !line_of(args[1], line)) { game::printf_to_console("usage: patch_insert <columns|rows> [index]\n"); return; }
			const int index = args.size() > 2 ? std::atoi(args[2].c_str()) : selected_line(line ? patch_line::row : patch_line::column);
			done(edit::patch_insert(get_scene().doc, line ? patch_line::row : patch_line::column, false, index), line ? "Insert 2 rows" : "Insert 2 columns");
		});

		// patch_add <columns|rows>   ( at the end )
		command::register_command("patch_add", [line_of](args_t args)
		{
			int line = 0;
			if (!need_map() || args.size() < 2 || !line_of(args[1], line)) { game::printf_to_console("usage: patch_add <columns|rows>\n"); return; }
			op_insert(line ? patch_line::row : patch_line::column, true);
		});

		// patch_terrain_add [column|row] [after]
		command::register_command("patch_terrain_add", [line_of](args_t args)
		{
			int line = -1;
			if (args.size() > 1 && !line_of(args[1], line)) { game::printf_to_console("usage: patch_terrain_add [column|row] [after]\n"); return; }
			op_terrain_add(line, args.size() > 2 ? std::atoi(args[2].c_str()) : -1);
		});

		// patch_extrude [column|row] [start|end]
		command::register_command("patch_extrude", [line_of](args_t args)
		{
			int line = -1;
			if (args.size() > 1 && !line_of(args[1], line)) { game::printf_to_console("usage: patch_extrude [column|row] [start|end]\n"); return; }
			op_extrude(line, args.size() > 2 && args[2] == "start" ? 0 : 1);
		});

		// patch_delete <first|last> <columns|rows>
		command::register_command("patch_delete", [line_of](args_t args)
		{
			int line = 0;
			if (!need_map() || args.size() < 3 || (args[1] != "first" && args[1] != "last") || !line_of(args[2], line))
			{
				game::printf_to_console("usage: patch_delete <first|last> <columns|rows>\n");
				return;
			}

			done(edit::patch_delete(get_scene().doc, line ? patch_line::row : patch_line::column, args[1] == "last"), "Delete 2 lines");
		});

		// patch_terrain_remove [column|row] [index]
		command::register_command("patch_terrain_remove", [line_of](args_t args)
		{
			int line = -1;
			if (args.size() > 1 && !line_of(args[1], line)) { game::printf_to_console("usage: patch_terrain_remove [column|row] [index]\n"); return; }
			op_terrain_remove(line, args.size() > 2 ? std::atoi(args[2].c_str()) : -1);
		});

		command::register_command("patch_transpose", [](args_t) { if (need_map()) { done(edit::patch_transpose(get_scene().doc), "Transpose patch"); } });
		command::register_command("patch_invert", [](args_t) { if (need_map()) { done(edit::patch_invert(get_scene().doc), "Invert patch"); } });

		// patch_redisperse <vertices|columns|rows>
		command::register_command("patch_redisperse", [](args_t args)
		{
			if (!need_map()) { return; }
			const std::string what = args.size() > 1 ? args[1] : "vertices";
			const auto mode = what == "columns" ? edit::patch_disperse::columns : (what == "rows" ? edit::patch_disperse::rows : edit::patch_disperse::vertices);
			done(edit::patch_redisperse(get_scene().doc, mode), "Redisperse");
		});

		// patch_weld [tolerance];  patch_weld_brushes [tolerance]
		command::register_command("patch_weld", [](args_t args) { if (need_map()) { done(edit::patch_weld(get_scene().doc, number(args, 1, weld_tolerance())), "Weld patches"); } });
		command::register_command("patch_weld_brushes", [](args_t args) { if (need_map()) { done(edit::patch_weld_to_brushes(get_scene().doc, number(args, 1, weld_tolerance())), "Weld to brushes"); } });

		// patch_subdiv <steps>   ( +1 finer, -1 coarser )
		command::register_command("patch_subdiv", [](args_t args)
		{
			if (!need_map()) { return; }
			const int steps = args.size() > 1 ? std::atoi(args[1].c_str()) : 1;
			done(edit::patch_change_subdivision(get_scene().doc, steps), steps > 0 ? "Increase subdivision" : "Decrease subdivision");
		});

		// patch_split [column|row] [index]
		command::register_command("patch_split", [line_of](args_t args)
		{
			int line = -1;
			if (args.size() > 1 && !line_of(args[1], line)) { game::printf_to_console("usage: patch_split [column|row] [index]\n"); return; }
			op_split(line, args.size() > 2 ? std::atoi(args[2].c_str()) : -1);
		});

		// patch_thicken <thickness> [seams 0|1]
		command::register_command("patch_thicken", [](args_t args)
		{
			if (need_map()) { done(edit::patch_thicken(get_scene().doc, number(args, 1, 8.0), number(args, 2, 1.0) != 0.0), "Thicken patch"); }
		});

		// patch_cap [auto|bevel|endcap|inverted_bevel|inverted_endcap|cylinder] [first|last|both]
		command::register_command("patch_cap", [](args_t args)
		{
			if (!need_map()) { return; }

			int type = 0;
			for (int i = 0; i < static_cast<int>(std::size(CAP_KEYS)); i++)
			{
				if (args.size() > 1 && args[1] == CAP_KEYS[i]) { type = i; }
			}

			const std::string ends = args.size() > 2 ? args[2] : "both";
			done(edit::patch_cap(get_scene().doc, static_cast<edit::patch_cap_type>(type), ends != "last", ends != "first"), "Cap patch");
		});

		command::register_command("patch_turn_edges", [](args_t) { op_turn_edges(); });
		// patch_turn_edge <column> <row>   ( of the first selected terrain patch )
		command::register_command("patch_turn_edge", [](args_t args)
		{
			if (!need_map() || args.size() < 3) { game::printf_to_console("usage: patch_turn_edge <column> <row>\n"); return; }
			done(edit::patch_turn_edge(get_scene().doc, inspect().first_patch, std::atoi(args[1].c_str()), std::atoi(args[2].c_str())), "Turn terrain edge");
		});

		command::register_command("patch_lock", [](args_t) { if (need_map()) { lock_vertices(true, false); } });
		command::register_command("patch_unlock", [](args_t args) { if (need_map()) { lock_vertices(false, args.size() > 1 && args[1] == "all"); } });

		// ---- textures
		command::register_command("patch_cycle_cap", [](args_t) { if (need_map()) { done(edit::patch_cycle_cap_texture(get_scene().doc), "Cycle cap texture"); } });
		// patch_cap_texture [x|y|z]
		command::register_command("patch_cap_texture", [](args_t args) { if (need_map()) { done(edit::patch_cap_texture(get_scene().doc, axis_arg(plain_args(args), -1)), "Cap texture"); } });
		// patch_natural [world units per texel]
		command::register_command("patch_natural", [](args_t args) { if (need_map()) { done(edit::patch_natural_texture(get_scene().doc, number(args, 1, 0.25)), "Natural texture"); } });
		// patch_fit [repeat x] [repeat y]
		command::register_command("patch_fit", [](args_t args) { if (need_map()) { done(edit::patch_fit_texture(get_scene().doc, number(args, 1, 1.0), number(args, 2, number(args, 1, 1.0))), "Fit texture"); } });

		// ---- vertices: patch_vsel <none|all|row N|column N|i j ..> [add]   ( of the first selected patch )
		command::register_command("patch_vsel", [line_of](args_t args)
		{
			if (!need_map()) { return; }

			const object_id id = vsel_patch() ? g.vsel_patch : inspect().first_patch;
			const auto* p = get_scene().doc.find_patch(id);
			if (!p) { game::printf_to_console("^1patch_vsel: select a patch\n"); return; }

			const bool add = !args.empty() && args.back() == "add";
			std::vector<int> list;
			int line = 0;

			if (args.size() > 1 && args[1] == "none") { g.vsel.clear(); g.vsel_patch = id; }
			else if (args.size() > 1 && args[1] == "all")
			{
				list.resize(p->vertices.size());
				std::iota(list.begin(), list.end(), 0);
			}
			else if (args.size() > 2 && line_of(args[1], line))
			{
				const int n = std::atoi(args[2].c_str());
				for (int i = 0; i < static_cast<int>(p->vertices.size()); i++)
				{
					if ((line ? i % p->height : i / p->height) == n) { list.push_back(i); }
				}
			}
			else
			{
				for (std::size_t i = 1; i < args.size(); i++)
				{
					if (is_number(args[i]) && std::atoi(args[i].c_str()) >= 0 && std::atoi(args[i].c_str()) < static_cast<int>(p->vertices.size())) { list.push_back(std::atoi(args[i].c_str())); }
				}
			}

			if (!list.empty()) { select_vertices(id, list, add); }
			game::printf_to_console("patch #%u: %d of %d vertices selected, %d locked\n", id, static_cast<int>(g.vsel.size()), static_cast<int>(p->vertices.size()),
				static_cast<int>(locked_of(id).size()));
		});

		// patch_color <r> <g> <b> [a]   ( 0 .. 255; the selected vertices, else all of the patch )
		command::register_command("patch_color", [](args_t args)
		{
			if (args.size() < 4) { game::printf_to_console("usage: patch_color <r> <g> <b> [a]\n"); return; }
			const std::uint8_t rgba[4] = { static_cast<std::uint8_t>(std::clamp(number(args, 1), 0.0, 255.0)), static_cast<std::uint8_t>(std::clamp(number(args, 2), 0.0, 255.0)),
				static_cast<std::uint8_t>(std::clamp(number(args, 3), 0.0, 255.0)), static_cast<std::uint8_t>(std::clamp(number(args, 4, 255.0), 0.0, 255.0)) };
			op_color(rgba);
		});

		// patch_noise <x> <y> <z> [seed]
		command::register_command("patch_noise", [](args_t args)
		{
			if (args.size() < 4) { game::printf_to_console("usage: patch_noise <x> <y> <z> [seed]\n"); return; }
			op_noise({ number(args, 1), number(args, 2), number(args, 3) }, static_cast<std::uint32_t>(number(args, 4, 1.0)));
		});

		// patch_move <dx> <dy> <dz>: the selected vertices ( honours weld / lock )
		command::register_command("patch_move", [](args_t args)
		{
			if (!need_map() || args.size() < 4) { game::printf_to_console("usage: patch_move <dx> <dy> <dz>\n"); return; }
			if (!vsel_patch() || g.vsel.empty()) { fail("Move vertices", "Select vertices first ( patch_vsel )."); return; }
			done(move_patch_vertices(g.vsel_patch, g.vsel, { number(args, 1), number(args, 2), number(args, 3) }), "Move vertices");
		});

		// patch_info: the selected patches
		command::register_command("patch_info", [](args_t)
		{
			if (!need_map()) { return; }
			auto& doc = get_scene().doc;

			for (const object_id id : doc.selection())
			{
				const auto* p = doc.find_patch(id);
				if (!p) { continue; }

				const auto box = doc.object_bounds(id);
				int flags = 0;
				for (const auto& v : p->vertices) { flags += v.has_flag && v.flag != 0; }

				game::printf_to_console("#%u %s %d x %d  %s  subdivision %d  mins %.6g %.6g %.6g  maxs %.6g %.6g %.6g  %d turned edges  %d locked\n", id,
					p->type == edit::patch_type::curve ? "curve" : "terrain", p->width, p->height, p->material.c_str(), edit::patch_subdivisions(*p),
					box.mins.x, box.mins.y, box.mins.z, box.maxs.x, box.maxs.y, box.maxs.z, flags, static_cast<int>(locked_of(id).size()));
			}
		});

		// ---- how patches are shown / edited
		const auto flag = [](const char* name, bool bp_state::* member, const char* text)
		{
			command::register_command(name, [member, text](args_t args)
			{
				g.*member = args.size() > 1 ? std::atoi(args[1].c_str()) != 0 : !(g.*member);
				game::printf_to_console("%s: %s\n", text, g.*member ? "on" : "off");
			});
		};

		flag("patch_weld_moves", &bp_state::weld_moves, "weld equal patch points during moves");
		flag("patch_tolerant", &bp_state::tolerant_weld, "tolerant weld");
		flag("patch_drill", &bp_state::drill_down, "drill down vertices");
		flag("patch_edge_mode", &bp_state::edge_mode, "cycle terrain edge mode");

		// patch_show <0|1>: textured / wireframe
		command::register_command("patch_show", [](args_t args) { set_patches_as(args.size() > 1 ? std::atoi(args[1].c_str()) : (patches_as_wire() ? 0 : 1)); });

		// patch_dialog <simple|terrain|thicken|cap|vertex|advanced|points> [close]
		command::register_command("patch_dialog", [](args_t args)
		{
			const std::string which = args.size() > 1 ? args[1] : "";
			const bool show = !(args.size() > 2 && args[2] == "close");

			if (which == "simple" || which == "curve") { g.show_simple = show; g.simple_terrain = false; }
			else if (which == "terrain") { g.show_simple = show; g.simple_terrain = true; }
			else if (which == "thicken") { g.show_thicken = show; }
			else if (which == "cap") { g.show_cap = show; }
			else if (which == "vertex") { g.show_vertex = show; }
			else if (which == "advanced") { g.show_advanced = show; }
			else if (which == "points") { g.show_points = show; }
			else { game::printf_to_console("usage: patch_dialog <simple|terrain|thicken|cap|vertex|advanced|points> [close]\n"); }
		});
	}
}
