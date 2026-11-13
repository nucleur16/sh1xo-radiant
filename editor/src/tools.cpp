#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"
#include "ext.hpp"

#include <random>

// The Tools menu.
//
//   Rope Generator   a cable between two points that hangs by its slack: as terrain meshes ( a tube ), or as a chain
//                    of models along the curve
//   Noise Generator  hills on the selected terrain meshes: amplitude, wave length, octaves, seed
//   Mesh Painter     models scattered onto what the mouse is dragged over in the 3D view: radius, density, random
//                    yaw and scale, aligned to the surface
//   PhysX Movement   not here: CoD4 Radiant's plugin walks its own collision world with the PhysX of the mod tools;
//                    this editor has neither

namespace editor::map_tools
{
	namespace
	{
		constexpr float DEG = 0.0174532925f;

		// ---- rope
		struct rope_state
		{
			bool show = false;
			float from[3] = { 0.0f, 0.0f, 128.0f };
			float to[3] = { 256.0f, 0.0f, 128.0f };
			float slack = 32.0f;
			float radius = 1.5f;
			int segments = 12;
			int sides = 6;
			std::string material;		// empty: the material of the Materials window
			int kind = 0;				// 0 mesh, 1 chain of models
			std::string model;
			float spacing = 32.0f;
			std::string note;
		};

		rope_state g_rope;

		// ---- noise
		struct noise_state
		{
			bool show = false;
			float amplitude = 32.0f;
			float wavelength = 256.0f;
			int seed = 1;
			int octaves = 3;
			int direction = 2;			// world axis
			bool keep_edges = true;
			int patch_size = 1024;
			int patch_points = 16;
			std::string note;
		};

		noise_state g_noise;

		// ---- painter
		struct painter_state
		{
			bool show = false;
			bool on = false;
			std::string models;			// names, separated by spaces, commas or lines
			float radius = 96.0f;
			int density = 3;			// models per step of a stroke
			float spacing = 64.0f;		// distance the brush moves between two steps
			bool random_yaw = true;
			float scale_min = 1.0f, scale_max = 1.0f;
			bool align = false;
			float sink = 0.0f;			// units into the surface
			bool on_models = false;		// also paint on models ( else the world and brushes only )

			// the stroke in progress
			// the 3D view as it was drawn last ( strokes that come from the console )
			int view_width = 0, view_height = 0;
			bool stroking = false;
			std::uint64_t stroke = 1;
			float3 last{};
			int placed = 0;
			std::mt19937 random{ 12345u };
		};

		painter_state g_paint;

		edit::vec3 to_vec(const float v[3])
		{
			return { v[0], v[1], v[2] };
		}

		void camera_target(float out[3])
		{
			auto& s = get_scene();
			const auto p = edit::snap_point(s.ray_target(s.cam.origin, s.cam.axes().forward), s.snap ? s.grid : 0.0);
			out[0] = static_cast<float>(p.x);
			out[1] = static_cast<float>(p.y);
			out[2] = static_cast<float>(p.z);
		}

		// ---- rope ------------------------------------------------------------------------------------------------

		bool make_rope()
		{
			auto& s = get_scene();
			auto& doc = s.doc;

			if (!s.open)
			{
				g_rope.note = "no map is open";
				return false;
			}

			if (g_rope.kind == 0)
			{
				edit::document::rope_params p;
				p.from = to_vec(g_rope.from);
				p.to = to_vec(g_rope.to);
				p.slack = g_rope.slack;
				p.radius = g_rope.radius;
				p.segments = g_rope.segments;
				p.sides = g_rope.sides;
				p.material = g_rope.material;

				std::vector<object_id> made;
				const bool ok = doc.create_rope(p, &made);
				g_rope.note = ok ? "rope: " + std::to_string(made.size()) + " mesh" + (made.size() == 1 ? "" : "es") + ", " + std::to_string(g_rope.segments) + " segments" : doc.last_error();
				return ok;
			}

			// a chain of models: one every `spacing` units along the same curve, turned the way the rope runs
			if (g_rope.model.empty())
			{
				g_rope.note = "name the model of the chain ( the Models window lists them )";
				return false;
			}

			const edit::vec3 from = to_vec(g_rope.from), to = to_vec(g_rope.to), span = to - from;
			const auto centre = [&](const double t)
			{
				edit::vec3 p = from + span * t;
				p.z -= static_cast<double>(g_rope.slack) * 4.0 * t * (1.0 - t);
				return p;
			};

			// the length of the curve, by steps
			double length = 0.0;
			for (int i = 1; i <= 64; i++) { length += edit::length(centre(i / 64.0) - centre((i - 1) / 64.0)); }

			const int count = std::clamp(static_cast<int>(length / std::max(1.0f, g_rope.spacing)) + 1, 2, 512);

			doc.begin_transaction("Rope");
			std::vector<object_id> made;

			for (int i = 0; i < count; i++)
			{
				const double t = static_cast<double>(i) / (count - 1);
				const edit::vec3 at = centre(t);
				const edit::vec3 ahead = centre(std::min(1.0, t + 0.01)) - centre(std::max(0.0, t - 0.01));
				const double yaw = std::atan2(ahead.y, ahead.x) / DEG;
				const double pitch = -std::atan2(ahead.z, std::sqrt(ahead.x * ahead.x + ahead.y * ahead.y)) / DEG;

				const object_id id = s.create_model_entity(g_rope.model, at, 0.0);
				if (!id) { continue; }

				doc.set_key(id, "angles", edit::format_key_vec3({ std::round(pitch * 100.0) / 100.0, std::round(yaw * 100.0) / 100.0, 0.0 }));
				made.push_back(id);
			}

			doc.select(std::span<const object_id>(made), edit::select_mode::replace);
			doc.end_transaction();

			g_rope.note = made.empty() ? doc.last_error() : "rope: " + std::to_string(made.size()) + " models of " + g_rope.model;
			return !made.empty();
		}

		// two selected entities ( their origins ), or the ends of the selection's box
		bool points_from_selection()
		{
			auto& doc = get_scene().doc;
			const auto ents = doc.selected_entities();

			if (ents.size() == 2)
			{
				const auto a = doc.find_entity(ents[0])->origin(), b = doc.find_entity(ents[1])->origin();
				for (int i = 0; i < 3; i++) { g_rope.from[i] = static_cast<float>(a[i]); g_rope.to[i] = static_cast<float>(b[i]); }
				return true;
			}

			if (const auto box = doc.selection_bounds(); box.valid())
			{
				for (int i = 0; i < 3; i++) { g_rope.from[i] = static_cast<float>(box.mins[i]); g_rope.to[i] = static_cast<float>(box.maxs[i]); }
				g_rope.from[2] = g_rope.to[2] = static_cast<float>(box.maxs.z);
				return true;
			}

			return false;
		}

		// ---- noise -----------------------------------------------------------------------------------------------

		bool make_noise()
		{
			auto& doc = get_scene().doc;
			edit::document::noise_params p;
			p.amplitude = g_noise.amplitude;
			p.frequency = 1.0 / std::max(1.0f, g_noise.wavelength);
			p.seed = static_cast<std::uint32_t>(g_noise.seed);
			p.octaves = g_noise.octaves;
			p.keep_edges = g_noise.keep_edges;
			p.direction = {};
			p.direction[std::clamp(g_noise.direction, 0, 2)] = 1.0;

			const bool ok = doc.patch_noise(p);
			g_noise.note = ok ? "noise applied ( Ctrl+Z takes it back; another seed gives other hills )" : doc.last_error();
			return ok;
		}

		// ---- painter ---------------------------------------------------------------------------------------------

		std::vector<std::string> painter_models()
		{
			std::vector<std::string> out;
			std::string word;

			for (const char c : g_paint.models + " ")
			{
				if (c == ' ' || c == ',' || c == ';' || c == '\n' || c == '\r' || c == '\t')
				{
					if (!word.empty()) { out.push_back(word); word.clear(); }
				}
				else
				{
					word.push_back(c);
				}
			}

			return out;
		}

		view::pick_options paint_pick()
		{
			view::pick_options options;
			options.entity_models = g_paint.on_models;
			options.placed_models = g_paint.on_models;
			options.static_models = g_paint.on_models;
			return options;
		}

		// "angles" of a model that stands on a surface with normal n, turned by yaw about it
		edit::vec3 angles_on(const float3& n, const float yaw_degrees)
		{
			const float yaw = yaw_degrees * DEG;
			float f[3] = { std::cos(yaw), std::sin(yaw), 0.0f };
			const float d = f[0] * n.x + f[1] * n.y + f[2] * n.z;
			f[0] -= n.x * d; f[1] -= n.y * d; f[2] -= n.z * d;
			float len = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);

			if (len < 1e-4f)
			{
				return { 0.0, yaw_degrees, 0.0 };
			}

			for (float& v : f) { v /= len; }

			// left = up x forward
			const float l[3] = { n.y * f[2] - n.z * f[1], n.z * f[0] - n.x * f[2], n.x * f[1] - n.y * f[0] };
			const double pitch = -std::asin(std::clamp(f[2], -1.0f, 1.0f)) / DEG;
			const double out_yaw = std::atan2(f[1], f[0]) / DEG;
			const double roll = std::atan2(l[2], n.z) / DEG;
			return { std::round(pitch * 100.0) / 100.0, std::round(out_yaw * 100.0) / 100.0, std::round(roll * 100.0) / 100.0 };
		}

		// one step of a stroke: `density` models around the point the brush is on
		int paint_step(const view::pick_result& hit)
		{
			auto& s = get_scene();
			auto& doc = s.doc;
			const auto models = painter_models();

			if (models.empty())
			{
				return 0;
			}

			// two directions across the normal
			const float3 n = hit.normal;
			float3 a = std::fabs(n.z) < 0.9f ? float3{ -n.y, n.x, 0.0f } : float3{ 1.0f, 0.0f, 0.0f };
			const float al = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
			a = { a.x / al, a.y / al, a.z / al };
			const float3 b{ n.y * a.z - n.z * a.y, n.z * a.x - n.x * a.z, n.x * a.y - n.y * a.x };

			std::uniform_real_distribution<float> unit(0.0f, 1.0f);
			const auto options = paint_pick();
			int placed = 0;

			// consecutive steps of one stroke are one undo step
			doc.begin_transaction("Paint models", 0x7061696e74000000ull + g_paint.stroke);
			const std::vector<object_id> before(doc.selection().begin(), doc.selection().end());

			for (int i = 0; i < g_paint.density; i++)
			{
				// evenly over the disc
				const float r = g_paint.radius * std::sqrt(unit(g_paint.random)), angle = unit(g_paint.random) * 6.2831853f;
				const float du = std::cos(angle) * r, dv = std::sin(angle) * r;
				const float3 above{ hit.point.x + a.x * du + b.x * dv + n.x * 48.0f, hit.point.y + a.y * du + b.y * dv + n.y * 48.0f, hit.point.z + a.z * du + b.z * dv + n.z * 48.0f };

				// down onto the surface from above it
				view::pick_result ground;
				if (!s.renderer.pick_ray(above, { -n.x, -n.y, -n.z }, ground, options) || ground.distance > 48.0f + g_paint.radius)
				{
					continue;
				}

				const std::string& model = models[static_cast<std::size_t>(unit(g_paint.random) * static_cast<float>(models.size())) % models.size()];
				const float yaw = g_paint.random_yaw ? std::floor(unit(g_paint.random) * 360.0f) : 0.0f;
				const float scale = g_paint.scale_min + (g_paint.scale_max - g_paint.scale_min) * unit(g_paint.random);
				const edit::vec3 at{ ground.point.x - ground.normal.x * g_paint.sink, ground.point.y - ground.normal.y * g_paint.sink, ground.point.z - ground.normal.z * g_paint.sink };

				const object_id id = s.create_model_entity(model, { std::round(at.x * 10.0) / 10.0, std::round(at.y * 10.0) / 10.0, std::round(at.z * 10.0) / 10.0 }, 0.0);
				if (!id) { continue; }

				const edit::vec3 angles = g_paint.align ? angles_on(ground.normal, yaw) : edit::vec3{ 0.0, yaw, 0.0 };
				if (angles.x != 0.0 || angles.y != 0.0 || angles.z != 0.0) { doc.set_key(id, "angles", edit::format_key_vec3(angles)); }
				if (std::fabs(scale - 1.0f) > 0.005f) { doc.set_key(id, "modelscale", edit::format_key_number(std::round(scale * 100.0) / 100.0)); }
				placed++;
			}

			// painting does not change what is selected
			doc.select(std::span<const object_id>(before), edit::select_mode::replace);
			doc.end_transaction();
			g_paint.placed += placed;
			return placed;
		}

		// the brush in the 3D view: a ring on the surface under the mouse
		void draw_brush_ring(ImDrawList* dl, const ImVec2& origin, const int width, const int height, const view::pick_result& hit)
		{
			auto& s = get_scene();
			const float3 n = hit.normal;
			float3 a = std::fabs(n.z) < 0.9f ? float3{ -n.y, n.x, 0.0f } : float3{ 1.0f, 0.0f, 0.0f };
			const float al = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
			a = { a.x / al, a.y / al, a.z / al };
			const float3 b{ n.y * a.z - n.z * a.y, n.z * a.x - n.x * a.z, n.x * a.y - n.y * a.x };

			ImVec2 points[49];
			int count = 0;

			for (int i = 0; i <= 48; i++)
			{
				const float angle = static_cast<float>(i) / 48.0f * 6.2831853f, du = std::cos(angle) * g_paint.radius, dv = std::sin(angle) * g_paint.radius;
				float xy[2], depth = 0.0f;

				if (view::project(s.cam, width, height, { hit.point.x + a.x * du + b.x * dv + n.x, hit.point.y + a.y * du + b.y * dv + n.y, hit.point.z + a.z * du + b.z * dv + n.z }, xy, depth))
				{
					points[count++] = ImVec2(origin.x + xy[0], origin.y + xy[1]);
				}
			}

			dl->AddPolyline(points, count, IM_COL32(110, 255, 150, 255), 0, 2.0f);

			float xy[2], depth = 0.0f, tip[2];
			if (view::project(s.cam, width, height, hit.point, xy, depth)
				&& view::project(s.cam, width, height, { hit.point.x + n.x * 32.0f, hit.point.y + n.y * 32.0f, hit.point.z + n.z * 32.0f }, tip, depth))
			{
				dl->AddLine(ImVec2(origin.x + xy[0], origin.y + xy[1]), ImVec2(origin.x + tip[0], origin.y + tip[1]), IM_COL32(110, 255, 150, 255), 2.0f);
			}
		}

		void end_stroke()
		{
			if (g_paint.stroking)
			{
				g_paint.stroking = false;
				g_paint.stroke++;
				game::printf_to_console("[painter] stroke: %d models\n", g_paint.placed);
			}
		}
	}

	// ---- actions -------------------------------------------------------------------------------------------------

	void register_actions()
	{
		using namespace actions;

		add("tool.rope", "Rope Generator ..", "", [] { g_rope.show = !g_rope.show; }, anywhere, no_map).toggle([] { return g_rope.show; });
		add("tool.noise", "Noise Generator ..", "", [] { g_noise.show = !g_noise.show; }, anywhere, no_map).toggle([] { return g_noise.show; });
		add("tool.mesh_painter", "Mesh Painter", "", []
		{
			// the button turns the brush on and off; the window comes with it
			g_paint.on = !g_paint.on;
			g_paint.show = g_paint.show || g_paint.on;
			if (!g_paint.on) { end_stroke(); }
		}, anywhere, no_map).toggle([] { return g_paint.on; }).help("Left drag in the 3D view scatters the models of the Mesh Painter window onto what the mouse is over.");
		add("tool.mesh_painter_window", "Mesh Painter settings ..", "", [] { g_paint.show = !g_paint.show; }, anywhere, no_map).toggle([] { return g_paint.show; });

		using args_t = const std::vector<std::string>&;
		const auto number = [](args_t args, const std::size_t i, const double fallback) { return i < args.size() ? std::atof(args[i].c_str()) : fallback; };

		// tool_rope x1 y1 z1 x2 y2 z2 [slack] [radius] [segments] [sides] [material]  |  tool_rope chain <model> x1 y1 z1 x2 y2 z2 [slack] [spacing]  |  tool_rope window
		command::register_command("tool_rope", [number](args_t args)
		{
			if (args.size() > 1 && args[1] == "window") { g_rope.show = true; return; }

			const bool chain = args.size() > 1 && args[1] == "chain";
			const std::size_t first = chain ? 3 : 1;

			if (args.size() < first + 6)
			{
				game::printf_to_console("usage: tool_rope x1 y1 z1 x2 y2 z2 [slack] [radius] [segments] [sides] [material]\n       tool_rope chain <model> x1 y1 z1 x2 y2 z2 [slack] [spacing]\n");
				return;
			}

			for (int i = 0; i < 3; i++)
			{
				g_rope.from[i] = static_cast<float>(number(args, first + static_cast<std::size_t>(i), 0.0));
				g_rope.to[i] = static_cast<float>(number(args, first + 3 + static_cast<std::size_t>(i), 0.0));
			}

			g_rope.slack = static_cast<float>(number(args, first + 6, g_rope.slack));
			g_rope.kind = chain ? 1 : 0;

			if (chain)
			{
				g_rope.model = args[2];
				g_rope.spacing = static_cast<float>(number(args, first + 7, g_rope.spacing));
			}
			else
			{
				g_rope.radius = static_cast<float>(number(args, first + 7, g_rope.radius));
				g_rope.segments = static_cast<int>(number(args, first + 8, g_rope.segments));
				g_rope.sides = static_cast<int>(number(args, first + 9, g_rope.sides));
				g_rope.material = args.size() > first + 10 ? args[first + 10] : std::string();
			}

			const bool ok = make_rope();
			game::printf_to_console("%s%s\n", ok ? "" : "^1", g_rope.note.c_str());
		});

		// tool_noise [amplitude] [wave length] [seed] [octaves] [x|y|z] [edges 0|1]  |  tool_noise window  |  tool_noise terrain [size] [points]
		command::register_command("tool_noise", [number](args_t args)
		{
			if (args.size() > 1 && args[1] == "window") { g_noise.show = true; return; }

			if (args.size() > 1 && args[1] == "terrain")
			{
				auto& s = get_scene();
				if (!s.open) { game::printf_to_console("^1no map is open\n"); return; }

				float at[3];
				camera_target(at);
				const double half = number(args, 2, g_noise.patch_size) * 0.5;
				const int points = std::clamp(static_cast<int>(number(args, 3, g_noise.patch_points)), 2, 16);
				const auto id = s.doc.create_terrain_patch({ at[0] - half, at[1] - half, at[2] }, { at[0] + half, at[1] + half, at[2] }, points, points);
				if (id) { game::printf_to_console("terrain mesh #%u\n", id); }
				else { game::printf_to_console("^1%s\n", s.doc.last_error().c_str()); }
				return;
			}

			g_noise.amplitude = static_cast<float>(number(args, 1, g_noise.amplitude));
			g_noise.wavelength = static_cast<float>(number(args, 2, g_noise.wavelength));
			g_noise.seed = static_cast<int>(number(args, 3, g_noise.seed));
			g_noise.octaves = static_cast<int>(number(args, 4, g_noise.octaves));
			if (args.size() > 5) { g_noise.direction = args[5] == "x" ? 0 : (args[5] == "y" ? 1 : 2); }
			if (args.size() > 6) { g_noise.keep_edges = args[6] != "0"; }

			const bool ok = make_noise();
			game::printf_to_console("%s%s\n", ok ? "" : "^1", g_noise.note.c_str());
		});

		// tool_paint <on|off|window> | models <names ..> | radius <r> | density <n> | spacing <d> | scale <min> <max> | align 0|1 | yaw 0|1 | at <px> <py>
		command::register_command("tool_paint", [number](args_t args)
		{
			const std::string what = args.size() > 1 ? args[1] : "";

			if (what == "on" || what == "off") { g_paint.on = what == "on"; g_paint.show = g_paint.show || g_paint.on; if (!g_paint.on) { end_stroke(); } }
			else if (what == "window") { g_paint.show = true; }
			else if (what == "models") { g_paint.models.clear(); for (std::size_t i = 2; i < args.size(); i++) { g_paint.models += (i > 2 ? " " : "") + args[i]; } }
			else if (what == "radius") { g_paint.radius = static_cast<float>(number(args, 2, g_paint.radius)); }
			else if (what == "density") { g_paint.density = std::clamp(static_cast<int>(number(args, 2, g_paint.density)), 1, 64); }
			else if (what == "spacing") { g_paint.spacing = static_cast<float>(number(args, 2, g_paint.spacing)); }
			else if (what == "scale") { g_paint.scale_min = static_cast<float>(number(args, 2, 1.0)); g_paint.scale_max = static_cast<float>(number(args, 3, g_paint.scale_min)); }
			else if (what == "align") { g_paint.align = number(args, 2, 1.0) != 0.0; }
			else if (what == "yaw") { g_paint.random_yaw = number(args, 2, 1.0) != 0.0; }
			else if (what == "seed") { g_paint.random.seed(static_cast<unsigned>(number(args, 2, 12345.0))); }
			else if (what == "at")
			{
				// one step of a stroke at a pixel of the 3D view ( what a click with the brush does )
				auto& s = get_scene();
				if (!s.open) { game::printf_to_console("^1no map is open\n"); return; }

				view::pick_result hit;
				// through the pixel of the view as it was drawn last; through its middle without a pixel
				float3 origin = s.cam.origin, direction = s.cam.axes().forward;
				if (args.size() > 3 && g_paint.view_width > 0) { view::screen_ray(s.cam, g_paint.view_width, g_paint.view_height, static_cast<float>(number(args, 2, 0)), static_cast<float>(number(args, 3, 0)), origin, direction); }

				if (!s.renderer.pick_ray(origin, direction, hit, paint_pick())) { game::printf_to_console("^1[painter] nothing under the brush\n"); return; }
				const int placed = paint_step(hit);
				g_paint.stroke++;
				game::printf_to_console("[painter] %d models at %.0f %.0f %.0f ( normal %.2f %.2f %.2f )\n", placed, hit.point.x, hit.point.y, hit.point.z, hit.normal.x, hit.normal.y, hit.normal.z);
			}
			else
			{
				game::printf_to_console("[painter] %s; models \"%s\"; radius %g, density %d, spacing %g, scale %g .. %g, align %d, random yaw %d\n", g_paint.on ? "on" : "off", g_paint.models.c_str(),
					g_paint.radius, g_paint.density, g_paint.spacing, g_paint.scale_min, g_paint.scale_max, g_paint.align ? 1 : 0, g_paint.random_yaw ? 1 : 0);
			}
		});
	}

	void draw_menu()
	{
		if (!editor::menu::begin("Tools"))
		{
			return;
		}

		actions::menu_item("tool.rope");
		actions::menu_item("tool.noise");
		actions::menu_item("tool.mesh_painter");
		actions::menu_item("tool.mesh_painter_window");
		ImGui::Separator();
		editor::menu::item("PhysX Movement", nullptr, false, false);

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Not in this editor: CoD4 Radiant's plugin walks the map with the PhysX runtime of the mod tools.\nThe camera of the 3D view flies ( right drag + W A S D ).");
		}

		editor::menu::end();
	}

	// ---- windows ---------------------------------------------------------------------------------------------------

	void draw_windows()
	{
		auto& s = get_scene();
		auto& doc = s.doc;

		// ---- rope
		if (g_rope.show)
		{
			ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_FirstUseEver);

			if (ImGui::Begin("Rope Generator##window", &g_rope.show, ImGuiWindowFlags_NoCollapse))
			{
				ImGui::SetNextItemWidth(-130.0f);
				ImGui::InputFloat3("##rope_from", g_rope.from, "%.1f");
				ImGui::SameLine();
				if (ImGui::Button("A = camera", ImVec2(-1.0f, 0.0f))) { camera_target(g_rope.from); }
				ImGui::SetNextItemWidth(-130.0f);
				ImGui::InputFloat3("##rope_to", g_rope.to, "%.1f");
				ImGui::SameLine();
				if (ImGui::Button("B = camera", ImVec2(-1.0f, 0.0f))) { camera_target(g_rope.to); }

				if (ImGui::Button("A and B from the selection"))
				{
					if (!points_from_selection()) { g_rope.note = "select two entities ( the rope runs between their origins ), or something whose box gives the two ends"; }
				}

				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Two selected entities: their origins. Anything else: the two ends of the selection's box, at its top.\n\"camera\": the point the camera of the 3D view looks at."); }

				ImGui::Separator();
				ImGui::DragFloat("slack", &g_rope.slack, 1.0f, -2000.0f, 2000.0f, "%.1f units");
				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("How far the middle hangs below the straight line between A and B"); }

				ImGui::RadioButton("Mesh ( a tube )", &g_rope.kind, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Chain of models", &g_rope.kind, 1);

				if (g_rope.kind == 0)
				{
					ImGui::DragFloat("radius", &g_rope.radius, 0.1f, 0.05f, 512.0f, "%.2f units");
					ImGui::SliderInt("segments", &g_rope.segments, 1, 128);
					ImGui::SliderInt("sides", &g_rope.sides, 3, 15);
					ImGui::InputTextWithHint("material", std::string(doc.default_material()).c_str(), &g_rope.material);
					ImGui::TextDisabled("Terrain meshes of %d x %d vertices; a rope of more than 15 segments is a chain of meshes.", std::min(g_rope.segments, 15) + 1, g_rope.sides + 1);
				}
				else
				{
					ImGui::InputTextWithHint("model", "a model of the Models window", &g_rope.model);
					ImGui::DragFloat("spacing", &g_rope.spacing, 1.0f, 1.0f, 4096.0f, "%.1f units");
					ImGui::TextDisabled("One %s per step, turned the way the rope runs.", s.model_class.c_str());
				}

				if (!s.open) { ImGui::BeginDisabled(); }
				if (ImGui::Button("Generate", ImVec2(140.0f, 0.0f))) { ui::report(make_rope(), "Rope"); }
				if (!s.open) { ImGui::EndDisabled(); }

				if (!g_rope.note.empty()) { ImGui::TextWrapped("%s", g_rope.note.c_str()); }
			}

			ImGui::End();
		}

		// ---- noise
		if (g_noise.show)
		{
			ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);

			if (ImGui::Begin("Noise Generator##window", &g_noise.show, ImGuiWindowFlags_NoCollapse))
			{
				int patches = 0;
				for (const object_id id : doc.selection()) { patches += doc.kind(id) == edit::object_kind::patch; }

				ImGui::TextColored(patches ? ImVec4(1.0f, 0.72f, 0.25f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), patches ? "%d terrain mesh( es ) selected" : "Select terrain meshes.", patches);
				ImGui::DragFloat("amplitude", &g_noise.amplitude, 0.5f, 0.0f, 4096.0f, "%.1f units");
				ImGui::DragFloat("wave length", &g_noise.wavelength, 1.0f, 4.0f, 16384.0f, "%.0f units");
				ImGui::SliderInt("octaves", &g_noise.octaves, 1, 8);
				ImGui::InputInt("seed", &g_noise.seed);
				ImGui::Combo("direction", &g_noise.direction, "X\0Y\0Z\0");
				ImGui::Checkbox("Keep the edges ( the border vertices stay )", &g_noise.keep_edges);

				if (!patches) { ImGui::BeginDisabled(); }
				if (ImGui::Button("Apply", ImVec2(120.0f, 0.0f))) { ui::report(make_noise(), "Noise"); }
				ImGui::SameLine();
				if (ImGui::Button("Next seed + apply")) { if (doc.undo_name() == "Noise") { doc.undo(); } g_noise.seed++; ui::report(make_noise(), "Noise"); }
				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Takes the last noise back and applies the next one: flip through hills until they fit."); }
				if (!patches) { ImGui::EndDisabled(); }

				ImGui::Separator();
				ImGui::SetNextItemWidth(120.0f);
				ImGui::InputInt("size", &g_noise.patch_size, 64);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.0f);
				ImGui::SliderInt("vertices", &g_noise.patch_points, 2, 16);

				if (!s.open) { ImGui::BeginDisabled(); }

				if (ImGui::Button("New terrain mesh where the camera looks"))
				{
					float at[3];
					camera_target(at);
					const double half = std::max(16, g_noise.patch_size) * 0.5;
					ui::report(doc.create_terrain_patch({ at[0] - half, at[1] - half, at[2] }, { at[0] + half, at[1] + half, at[2] }, g_noise.patch_points, g_noise.patch_points) != edit::no_object, "Terrain");
				}

				if (!s.open) { ImGui::EndDisabled(); }
				if (!g_noise.note.empty()) { ImGui::TextWrapped("%s", g_noise.note.c_str()); }
			}

			ImGui::End();
		}

		// ---- painter
		if (g_paint.show)
		{
			ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_FirstUseEver);

			if (ImGui::Begin("Mesh Painter##window", &g_paint.show, ImGuiWindowFlags_NoCollapse))
			{
				if (ImGui::Checkbox("Paint with the left button in the 3D view", &g_paint.on) && !g_paint.on) { end_stroke(); }
				ImGui::InputTextMultiline("##paint_models", &g_paint.models, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4.0f));
				ImGui::TextDisabled("Models, one per line ( or separated by spaces ): each model placed is one of them.");

				if (ImGui::Button("Models window")) { ui::get().show_models = true; ImGui::SetWindowFocus("Models##window"); }
				ImGui::SameLine();

				if (ImGui::Button("Add the model of the selected entity"))
				{
					for (const object_id id : doc.selection())
					{
						if (const auto* e = doc.find_entity(id); e && !e->value("model").empty() && e->value("model")[0] != '*')
						{
							const std::string model(e->value("model"));
							const auto have = painter_models();
							if (std::find(have.begin(), have.end(), model) == have.end()) { g_paint.models += (g_paint.models.empty() ? "" : "\n") + model; }
						}
					}
				}

				ImGui::Separator();
				ImGui::DragFloat("radius", &g_paint.radius, 1.0f, 1.0f, 4096.0f, "%.0f units");
				ImGui::SliderInt("density", &g_paint.density, 1, 32, "%d per step");
				ImGui::DragFloat("spacing", &g_paint.spacing, 1.0f, 4.0f, 4096.0f, "%.0f units between steps");
				ImGui::Checkbox("Random yaw", &g_paint.random_yaw);
				ImGui::SameLine();
				ImGui::Checkbox("Align to the surface", &g_paint.align);
				ImGui::DragFloatRange2("scale", &g_paint.scale_min, &g_paint.scale_max, 0.01f, 0.05f, 20.0f, "min %.2f", "max %.2f");
				ImGui::DragFloat("sink", &g_paint.sink, 0.25f, -256.0f, 256.0f, "%.1f units into the surface");
				ImGui::Checkbox("Also paint on models", &g_paint.on_models);
				ImGui::TextDisabled("New entities are %s ( the class of the Models window ). A stroke is one undo step.", s.model_class.c_str());
				if (painter_models().empty()) { ImGui::TextColored(ImVec4(0.96f, 0.55f, 0.35f, 1.0f), "Name at least one model."); }
			}

			ImGui::End();
		}
	}

	// ---- the 3D view ---------------------------------------------------------------------------------------------

	void viewport_input(const ImVec2& origin, const int width, const int height)
	{
		auto& s = get_scene();
		g_paint.view_width = width;
		g_paint.view_height = height;

		if (!g_paint.on || !s.open || !s.renderer.loaded())
		{
			end_stroke();
			return;
		}

		const auto& io = ImGui::GetIO();

		// the right and middle button stay the camera's: the view gets the mouse while one of them is down
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			end_stroke();
			return;
		}

		// over the view, before its own button: this one takes the left button
		ImGui::SetCursorScreenPos(origin);
		ImGui::InvisibleButton("##vp_paint", ImVec2(static_cast<float>(width), static_cast<float>(height)), ImGuiButtonFlags_MouseButtonLeft);
		const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
		ImGui::SetCursorScreenPos(origin);

		if (hovered)
		{
			ui::get().next_hovered_3d = true;
		}

		if (!hovered && !active)
		{
			end_stroke();
			return;
		}

		view::pick_result hit;
		const bool on_surface = s.renderer.pick(s.cam, width, height, io.MousePos.x - origin.x, io.MousePos.y - origin.y, hit, paint_pick());

		if (on_surface)
		{
			auto* dl = ImGui::GetWindowDrawList();
			dl->PushClipRect(origin, ImVec2(origin.x + static_cast<float>(width), origin.y + static_cast<float>(height)), true);
			draw_brush_ring(dl, origin, width, height, hit);
			dl->PopClipRect();
		}

		if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && on_surface)
		{
			const float dx = hit.point.x - g_paint.last.x, dy = hit.point.y - g_paint.last.y, dz = hit.point.z - g_paint.last.z;

			if (!g_paint.stroking || dx * dx + dy * dy + dz * dz >= g_paint.spacing * g_paint.spacing)
			{
				if (!g_paint.stroking)
				{
					g_paint.stroking = true;
					g_paint.placed = 0;
				}

				g_paint.last = hit.point;
				paint_step(hit);
			}
		}
		else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			end_stroke();
		}
	}
}
