#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"

// The 2D views: the classic orthographic XY / XZ / YZ grid view of Radiant, drawn with ImGui draw lists from
// the line caches of the scene. Mouse handling is split in press / drag / release on world coordinates so that
// the console ( view2d click / drag ) runs exactly what the mouse runs.

namespace editor::ui
{
	namespace
	{
		struct corner_target
		{
			object_id object = edit::no_object;
			std::vector<edit::vec3> corners;	// brush corners
			std::vector<int> vertices;			// patch vertices
		};

		enum class action
		{
			none,
			move,			// drag inside the selection
			resize,			// drag outside the selection: the faces on that side follow
			create,			// drag on empty space with nothing selected: a new brush
			rect_select,	// shift drag
			camera,			// drag the camera icon
			clip_point,
			vertex,			// vertex / edge mode
		};

		struct grid_view
		{
			int index = 0;
			bool open = true;
			int axis = 2;				// the world axis the view looks along: 2 top ( XY ), 1 front ( XZ ), 0 side ( YZ )
			double center[2] = { 0.0, 0.0 };
			double zoom = 0.25;			// pixels per unit
			bool show_zone_brushes = true;
			std::uint64_t framed_serial = ~0ull;
			bool fit_request = false;

			// canvas of the last frame
			ImVec2 canvas_pos = {};
			ImVec2 canvas_size = { 800.0f, 600.0f };

			// mouse
			action act = action::none;
			bool pressed = false;
			bool moved = false;
			bool drag_open = false;		// a document drag was started
			double press[2] = {};
			double now[2] = {};
			edit::vec3 applied;			// the movement the document has now ( nothing is done while it stays the same )
			int side[2] = {};
			std::vector<corner_target> targets;
			bool pan_moved = false;

			// repeated clicks at one place go through the objects behind each other
			double last_click[2] = { 1e30, 1e30 };
			std::size_t cycle = 0;

			[[nodiscard]] int h() const { return axis == 0 ? 1 : 0; }
			[[nodiscard]] int v() const { return axis == 2 ? 1 : 2; }

			[[nodiscard]] ImVec2 to_screen(const double wh, const double wv) const
			{
				return { canvas_pos.x + canvas_size.x * 0.5f + static_cast<float>((wh - center[0]) * zoom),
					canvas_pos.y + canvas_size.y * 0.5f - static_cast<float>((wv - center[1]) * zoom) };
			}

			void to_world(const ImVec2& px, double& wh, double& wv) const
			{
				wh = center[0] + (px.x - canvas_pos.x - canvas_size.x * 0.5f) / zoom;
				wv = center[1] - (px.y - canvas_pos.y - canvas_size.y * 0.5f) / zoom;
			}

			[[nodiscard]] double pick_radius() const { return 7.0 / zoom; }

			void frame(const edit::bounds& box);
			void frame_map();
			void click(double wh, double wv, bool toggle);
			void on_press(double wh, double wv, bool shift);
			void on_drag(double wh, double wv, bool force);
			void on_release();
			void draw();
			void draw_grid(ImDrawList* dl) const;
			void draw_cache(ImDrawList* dl, const object_cache& cache, bool is_selection, int& label_budget) const;
		};

		std::vector<grid_view> g_views;

		const char* axis_name(const int axis)
		{
			return axis == 2 ? "XY Top" : (axis == 1 ? "XZ Front" : "YZ Side");
		}

		grid_view* view_at(const int index)
		{
			return index >= 0 && static_cast<std::size_t>(index) < g_views.size() ? &g_views[static_cast<std::size_t>(index)] : nullptr;
		}

		// the coordinate along the view axis for something new: where the last selection was
		double depth_coordinate(const int axis)
		{
			const auto& box = get_scene().last_selection_box;
			return box.valid() ? get_scene().snapped(box.center()[axis]) : 0.0;
		}

		// ---- view -----------------------------------------------------------------------------------------

		void grid_view::frame(const edit::bounds& box)
		{
			if (!box.valid())
			{
				return;
			}

			const auto c = box.center(), size = box.size();
			center[0] = c[h()];
			center[1] = c[v()];

			const double fit_h = canvas_size.x / std::max(16.0, size[h()] * 1.15);
			const double fit_v = canvas_size.y / std::max(16.0, size[v()] * 1.15);
			zoom = std::clamp(std::min(fit_h, fit_v), 0.002, 64.0);
		}

		void grid_view::frame_map()
		{
			auto& s = get_scene();
			const int ah = h(), av = v();

			// Where the game play entities are ( spawn points, triggers, script objects ). The collision brushes, the
			// bounds of the render world and the static models include the sky box and scenery far outside the
			// playable area; a few stray entities are cut off the same way.
			std::vector<float> hs, vs;

			for (const bool with_models : { false, true })
			{
				for (const auto* cache : { &s.statics })
				{
					for (const auto& e : cache->entities)
					{
						if (with_models || e.classname != "misc_model")
						{
							hs.push_back(e.origin[ah]);
							vs.push_back(e.origin[av]);
						}
					}
				}

				if (hs.size() >= 8)
				{
					break;
				}

				hs.clear();
				vs.clear();
			}

			edit::bounds box;

			if (hs.size() >= 8)
			{
				std::sort(hs.begin(), hs.end());
				std::sort(vs.begin(), vs.end());

				const std::size_t lo = hs.size() / 50, hi = hs.size() - 1 - hs.size() / 50;
				edit::vec3 a, b;
				a[ah] = hs[lo];
				a[av] = vs[lo];
				b[ah] = hs[hi];
				b[av] = vs[hi];
				box.add(a);
				box.add(b);
			}
			else
			{
				for (const auto& o : s.statics.objects)
				{
					box.add({ o.mins[0], o.mins[1], o.mins[2] });
					box.add({ o.maxs[0], o.maxs[1], o.maxs[2] });
				}
			}

			frame(box);
		}

		// ---- mouse ----------------------------------------------------------------------------------------

		void grid_view::click(const double wh, const double wv, const bool toggle)
		{
			auto& doc = get_scene().doc;

			// a ray along the view axis, from far outside the world
			edit::ray ray;
			ray.origin[axis] = edit::WORLD_EXTENT * 2.0;
			ray.origin[h()] = wh;
			ray.origin[v()] = wv;
			ray.direction[axis] = -1.0;

			auto hits = doc.pick_all(ray);

			if (!show_zone_brushes)
			{
				std::erase_if(hits, [](const edit::pick_result& hit) { return hit.kind != edit::object_kind::entity && get_scene().imported(hit.object); });
			}

			if (hits.empty())
			{
				if (!toggle)
				{
					deselect();
				}

				return;
			}

			const bool same_place = std::fabs(wh - last_click[0]) < pick_radius() && std::fabs(wv - last_click[1]) < pick_radius();
			cycle = (same_place && !toggle) ? (cycle + 1) % hits.size() : 0;
			last_click[0] = wh;
			last_click[1] = wv;

			get_scene().inspecting = false;
			doc.select(hits[cycle].object, toggle ? edit::select_mode::toggle : edit::select_mode::replace);
		}

		void grid_view::on_press(const double wh, const double wv, const bool shift)
		{
			auto& s = get_scene();
			auto& doc = s.doc;
			auto& st = get();

			pressed = true;
			moved = false;
			drag_open = false;
			press[0] = now[0] = wh;
			press[1] = now[1] = wv;
			side[0] = side[1] = 0;
			targets.clear();
			act = action::none;

			const double radius = pick_radius();
			const int ah = h(), av = v();

			// ---- clipper: place or grab a point
			if (st.clipper.on)
			{
				auto& points = st.clipper.points;

				if (st.clipper.axis != axis)
				{
					points.clear();
					st.clipper.axis = axis;
				}

				st.clipper.dragging = -1;

				for (std::size_t i = 0; i < points.size(); i++)
				{
					if (std::fabs(points[i][ah] - wh) < radius && std::fabs(points[i][av] - wv) < radius)
					{
						st.clipper.dragging = static_cast<int>(i);
					}
				}

				if (st.clipper.dragging < 0 && points.size() < 3)
				{
					edit::vec3 p;
					const auto box = doc.selection_bounds();
					p[axis] = box.valid() ? box.center()[axis] : 0.0;
					p[ah] = s.snapped(wh);
					p[av] = s.snapped(wv);
					points.push_back(p);
					st.clipper.dragging = static_cast<int>(points.size()) - 1;
				}

				act = st.clipper.dragging >= 0 ? action::clip_point : action::none;
				return;
			}

			// ---- vertex / edge mode: the corners under the mouse ( an edge along the view axis is two of them )
			if (st.vertex_mode && !doc.selection().empty())
			{
				double best = radius, grab[2] = {};
				bool found = false;

				const auto consider = [&](const edit::vec3& p)
				{
					const double d = std::max(std::fabs(p[ah] - wh), std::fabs(p[av] - wv));
					if (d < best)
					{
						best = d;
						grab[0] = p[ah];
						grab[1] = p[av];
						found = true;
					}
				};

				for (const object_id id : doc.selection())
				{
					if (const auto* geo = doc.geometry(id))
					{
						for (const auto& p : geo->vertices) { consider(p); }
					}
					else if (const auto* patch = doc.find_patch(id))
					{
						for (const auto& pv : patch->vertices) { consider(pv.xyz); }
					}
				}

				if (found)
				{
					for (const object_id id : doc.selection())
					{
						corner_target target;
						target.object = id;

						if (const auto* geo = doc.geometry(id))
						{
							for (const auto& p : geo->vertices)
							{
								if (std::fabs(p[ah] - grab[0]) < 0.05 && std::fabs(p[av] - grab[1]) < 0.05) { target.corners.push_back(p); }
							}
						}
						else if (const auto* patch = doc.find_patch(id))
						{
							for (std::size_t i = 0; i < patch->vertices.size(); i++)
							{
								const auto& p = patch->vertices[i].xyz;
								if (std::fabs(p[ah] - grab[0]) < 0.05 && std::fabs(p[av] - grab[1]) < 0.05) { target.vertices.push_back(static_cast<int>(i)); }
							}
						}

						if (!target.corners.empty() || !target.vertices.empty())
						{
							targets.push_back(std::move(target));
						}
					}

					act = action::vertex;
					return;
				}
			}

			// ---- the camera icon
			if (!shift && s.renderer.loaded())
			{
				const double ch = (&s.cam.origin.x)[ah], cv = (&s.cam.origin.x)[av];
				if (std::fabs(ch - wh) < radius * 1.5 && std::fabs(cv - wv) < radius * 1.5)
				{
					act = action::camera;
					return;
				}
			}

			if (shift)
			{
				act = action::rect_select;
				return;
			}

			if (doc.selection().empty())
			{
				act = action::create;
				return;
			}

			const auto box = doc.selection_bounds();
			if (!box.valid())
			{
				return;
			}

			if (wh >= box.mins[ah] && wh <= box.maxs[ah] && wv >= box.mins[av] && wv <= box.maxs[av])
			{
				act = action::move;
				return;
			}

			// outside: the sides the mouse is beyond are the ones that move
			bool has_brush = false;
			for (const object_id id : doc.selection())
			{
				has_brush |= doc.kind(id) == edit::object_kind::brush;
			}

			if (has_brush)
			{
				side[0] = wh < box.mins[ah] ? -1 : (wh > box.maxs[ah] ? 1 : 0);
				side[1] = wv < box.mins[av] ? -1 : (wv > box.maxs[av] ? 1 : 0);
				act = action::resize;
			}
		}

		void grid_view::on_drag(const double wh, const double wv, const bool force)
		{
			if (!pressed)
			{
				return;
			}

			auto& s = get_scene();
			auto& doc = s.doc;
			auto& st = get();

			if (!moved && !force && std::max(std::fabs(wh - press[0]), std::fabs(wv - press[1])) * zoom < 4.0)
			{
				return; // still a click
			}

			moved = true;
			now[0] = wh;
			now[1] = wv;

			const int ah = h(), av = v();
			edit::vec3 delta;
			delta[ah] = s.snapped(wh - press[0]);
			delta[av] = s.snapped(wv - press[1]);

			const auto open_drag = [&](const char* name)
			{
				if (!drag_open)
				{
					s.begin_drag(name);
					drag_open = true;
				}

				// the whole movement since the press, on the document as it was then
				s.restart_drag();
				applied = delta;
			};

			// the mouse moved, the snapped movement did not: the document is where it should be
			const bool is_edit = act == action::move || act == action::resize || act == action::vertex;
			if (is_edit && drag_open && delta == applied)
			{
				return;
			}

			switch (act)
			{
			case action::move:
				open_drag("Move");
				s.translate(delta);
				break;

			case action::resize:
				open_drag("Resize");
				if (side[0]) { s.resize_selection(ah, side[0], delta[ah]); }
				if (side[1]) { s.resize_selection(av, side[1], delta[av]); }
				break;

			case action::vertex:
				open_drag("Move vertex");

				for (const auto& target : targets)
				{
					if (!target.corners.empty())
					{
						doc.move_brush_corners(target.object, target.corners, delta);
					}
					else
					{
						doc.move_patch_vertices(target.object, target.vertices, delta);
					}
				}

				break;

			case action::camera:
				(&s.cam.origin.x)[ah] = static_cast<float>(wh);
				(&s.cam.origin.x)[av] = static_cast<float>(wv);
				break;

			case action::clip_point:
				if (st.clipper.dragging >= 0 && static_cast<std::size_t>(st.clipper.dragging) < st.clipper.points.size())
				{
					auto& p = st.clipper.points[static_cast<std::size_t>(st.clipper.dragging)];
					p[ah] = s.snapped(wh);
					p[av] = s.snapped(wv);
				}

				break;

			default:
				break; // create, rect_select: drawn as a preview, done on release
			}
		}

		void grid_view::on_release()
		{
			if (!pressed)
			{
				return;
			}

			auto& s = get_scene();
			auto& doc = s.doc;
			const int ah = h(), av = v();

			pressed = false;

			if (drag_open)
			{
				drag_open = false;
				s.end_drag();
			}

			if (act == action::clip_point || act == action::camera)
			{
				get().clipper.dragging = -1;
				act = action::none;
				return;
			}

			if (!moved)
			{
				// a click: select ( shift adds / removes )
				click(press[0], press[1], act == action::rect_select);
				act = action::none;
				return;
			}

			if (act == action::create)
			{
				// the rectangle that was dragged; along the view axis as deep as the last selection ( like Radiant )
				edit::vec3 mins, maxs;
				mins[ah] = s.snapped(std::min(press[0], now[0]));
				maxs[ah] = s.snapped(std::max(press[0], now[0]));
				mins[av] = s.snapped(std::min(press[1], now[1]));
				maxs[av] = s.snapped(std::max(press[1], now[1]));

				if (s.last_selection_box.valid() && s.last_selection_box.size()[axis] >= 1.0)
				{
					mins[axis] = s.last_selection_box.mins[axis];
					maxs[axis] = s.last_selection_box.maxs[axis];
				}
				else
				{
					mins[axis] = 0.0;
					maxs[axis] = std::max(64.0, s.grid);
				}

				if (maxs[ah] > mins[ah] && maxs[av] > mins[av])
				{
					report(doc.create_box(mins, maxs) != edit::no_object, "New brush");
				}
			}
			else if (act == action::rect_select)
			{
				const auto ids = doc.query_rect(axis, { std::min(press[0], now[0]), std::min(press[1], now[1]) }, { std::max(press[0], now[0]), std::max(press[1], now[1]) }, false);
				std::vector<object_id> wanted;

				for (const object_id id : ids)
				{
					if (show_zone_brushes || doc.kind(id) == edit::object_kind::entity || !s.imported(id))
					{
						wanted.push_back(id);
					}
				}

				doc.select(std::span<const object_id>(wanted), edit::select_mode::add);
			}

			act = action::none;
		}

		// ---- drawing --------------------------------------------------------------------------------------

		void grid_view::draw_grid(ImDrawList* dl) const
		{
			const auto& s = get_scene();

			double wh0, wv0, wh1, wv1;
			to_world(canvas_pos, wh0, wv1);
			to_world(ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), wh1, wv0);

			// the grid of the editor, coarser while its lines would be closer than a few pixels
			double step = s.grid;
			while (step * zoom < 8.0)
			{
				step *= 2.0;
			}

			const double major = step * 8.0;
			const ImU32 minor_color = IM_COL32(46, 48, 52, 255), major_color = IM_COL32(68, 71, 77, 255), axis_color = IM_COL32(112, 116, 128, 255);
			auto* font = ImGui::GetIO().Fonts->Fonts.Size > 2 ? ImGui::GetIO().Fonts->Fonts[2] : ImGui::GetFont();
			char text[32];

			for (int pass = 0; pass < 2; pass++) // minor lines first, so that major lines are on top
			{
				for (double x = std::floor(wh0 / step) * step; x <= wh1; x += step)
				{
					const bool is_major = std::fabs(std::remainder(x, major)) < step * 0.25;
					if (is_major != (pass == 1))
					{
						continue;
					}

					const float px = to_screen(x, 0.0).x;
					dl->AddLine(ImVec2(px, canvas_pos.y), ImVec2(px, canvas_pos.y + canvas_size.y), x == 0.0 ? axis_color : (is_major ? major_color : minor_color));

					if (is_major)
					{
						std::snprintf(text, sizeof(text), "%g", x);
						dl->AddText(font, font->FontSize, ImVec2(px + 3.0f, canvas_pos.y + 2.0f), IM_COL32(150, 154, 162, 255), text);
					}
				}

				for (double y = std::floor(wv0 / step) * step; y <= wv1; y += step)
				{
					const bool is_major = std::fabs(std::remainder(y, major)) < step * 0.25;
					if (is_major != (pass == 1))
					{
						continue;
					}

					const float py = to_screen(0.0, y).y;
					dl->AddLine(ImVec2(canvas_pos.x, py), ImVec2(canvas_pos.x + canvas_size.x, py), y == 0.0 ? axis_color : (is_major ? major_color : minor_color));

					if (is_major)
					{
						std::snprintf(text, sizeof(text), "%g", y);
						dl->AddText(font, font->FontSize, ImVec2(canvas_pos.x + 3.0f, py + 1.0f), IM_COL32(150, 154, 162, 255), text);
					}
				}
			}
		}

		void grid_view::draw_cache(ImDrawList* dl, const object_cache& cache, const bool is_selection, int& label_budget) const
		{
			const int ah = h(), av = v();

			double wh0, wv0, wh1, wv1;
			to_world(canvas_pos, wh0, wv1);
			to_world(ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), wh1, wv0);

			const ImU32 selected_color = IM_COL32(255, 58, 32, 255);

			const auto& doc = get_scene().doc;

			for (const auto& o : cache.objects)
			{
				if (o.maxs[ah] < wh0 || o.mins[ah] > wh1 || o.maxs[av] < wv0 || o.mins[av] > wv1)
				{
					continue;
				}

				// the cache of everything also holds what is selected: that is drawn from the selection, where it is now
				if (!is_selection && doc.is_selected(o.id))
				{
					continue;
				}

				if (!is_selection && !show_zone_brushes && (o.imported || o.compiled))
				{
					continue;
				}

				const double extent = std::max(o.maxs[ah] - o.mins[ah], o.maxs[av] - o.mins[av]) * zoom;
				if (extent < 1.5 && !is_selection)
				{
					continue; // smaller than a pixel or two
				}

				const ImU32 color = is_selection ? selected_color : o.color;

				if (extent < 9.0)
				{
					// too small to tell its edges apart: its box
					const ImVec2 a = to_screen(o.mins[ah], o.maxs[av]), b = to_screen(o.maxs[ah], o.mins[av]);
					dl->AddRect(a, ImVec2(std::max(b.x, a.x + 1.0f), std::max(b.y, a.y + 1.0f)), color);
					continue;
				}

				for (std::uint32_t i = 0; i + 1 < o.count; i += 2)
				{
					const auto& p = cache.points[o.first + i];
					const auto& q = cache.points[o.first + i + 1];
					const float* pf = &p.x;
					const float* qf = &q.x;

					// an edge along the view axis is a point
					if (std::fabs(pf[ah] - qf[ah]) + std::fabs(pf[av] - qf[av]) < 1e-3f)
					{
						continue;
					}

					dl->AddLine(to_screen(pf[ah], pf[av]), to_screen(qf[ah], qf[av]), color);
				}
			}

			auto* font = ImGui::GetIO().Fonts->Fonts.Size > 2 ? ImGui::GetIO().Fonts->Fonts[2] : ImGui::GetFont();

			for (const auto& e : cache.entities)
			{
				if (e.maxs[ah] < wh0 || e.mins[ah] > wh1 || e.maxs[av] < wv0 || e.mins[av] > wv1 || (!is_selection && doc.is_selected(e.id)))
				{
					continue;
				}

				const ImVec2 a = to_screen(e.mins[ah], e.maxs[av]), b = to_screen(e.maxs[ah], e.mins[av]);
				const float extent = std::max(b.x - a.x, b.y - a.y);

				if (extent < 1.5f && !is_selection)
				{
					continue;
				}

				const ImU32 color = is_selection ? selected_color : e.color;
				dl->AddRect(a, ImVec2(std::max(b.x, a.x + 2.0f), std::max(b.y, a.y + 2.0f)), color);

				if (extent >= 10.0f)
				{
					// where it faces ( top view )
					if (axis == 2)
					{
						const float yaw = e.yaw * 0.0174532925f;
						const ImVec2 c = to_screen(e.origin[0], e.origin[1]);
						const float reach = std::min(extent * 0.5f, 24.0f);
						dl->AddLine(c, ImVec2(c.x + std::cos(yaw) * reach, c.y - std::sin(yaw) * reach), color);
					}

					if ((extent >= 28.0f || is_selection) && label_budget > 0)
					{
						label_budget--;
						dl->AddText(font, font->FontSize, ImVec2(a.x, a.y - font->FontSize - 1.0f), color, e.classname.c_str());
					}
				}
			}
		}

		void grid_view::draw()
		{
			auto& s = get_scene();
			auto& doc = s.doc;
			auto& st = get();
			const auto& io = ImGui::GetIO();

			char title[64];
			if (index == 0)
			{
				std::snprintf(title, sizeof(title), "2D View##grid0");
			}
			else
			{
				std::snprintf(title, sizeof(title), "2D View %d##grid%d", index + 1, index);
			}

			ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f), ImGuiCond_FirstUseEver);

			if (!ImGui::Begin(title, &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				ImGui::End();
				return;
			}

			// ---- toolbar
			for (const int a : { 2, 1, 0 })
			{
				const bool on = axis == a;
				if (on)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
				}

				if (ImGui::Button(a == 2 ? "XY" : (a == 1 ? "XZ" : "YZ")) && !on)
				{
					axis = a;
					fit_request = true;
				}

				if (on)
				{
					ImGui::PopStyleColor();
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s ( Ctrl+Tab cycles )", axis_name(a));
				}

				ImGui::SameLine();
			}

			if (ImGui::Button("Fit"))
			{
				fit_request = true;
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Frame the selection, or the whole map");
			}

			ImGui::SameLine(0.0f, 16.0f);

			bool clipper = st.clipper.on;
			if (ImGui::Checkbox("Clipper", &clipper))
			{
				set_clipper(clipper);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("C. Click two or three points; Enter clips ( the side of the arrow stays ),\nCtrl+Enter keeps the other side, Shift+Enter splits.");
			}

			ImGui::SameLine();

			bool vertex = st.vertex_mode;
			if (ImGui::Checkbox("Vertices", &vertex))
			{
				set_vertex_mode(vertex);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("V. Drag the corners of the selected brushes ( an edge that points at you moves as a whole ).");
			}

			ImGui::SameLine();
			ImGui::Checkbox("Zone brushes", &show_zone_brushes);

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("The collision brushes that came with the zone. New and edited brushes are always shown.");
			}

			// ---- canvas
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			canvas_pos = ImGui::GetCursorScreenPos();
			canvas_size = ImVec2(std::max(avail.x, 32.0f), std::max(avail.y, 32.0f));

			if (framed_serial != s.open_serial && s.open)
			{
				framed_serial = s.open_serial;
				frame_map();
			}

			if (fit_request)
			{
				fit_request = false;

				if (const auto box = doc.selection_bounds(); box.valid())
				{
					frame(box);
					zoom = std::min(zoom, 4.0);
				}
				else
				{
					frame_map();
				}
			}

			ImGui::InvisibleButton("##grid_canvas", canvas_size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
			const bool hovered = ImGui::IsItemHovered();
			const bool active = ImGui::IsItemActive();

			if (hovered)
			{
				st.next_hovered_2d = index;
			}

			double mouse_h, mouse_v;
			to_world(io.MousePos, mouse_h, mouse_v);

			// ---- navigation: right / middle drag pans, the wheel zooms about the mouse
			if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
			{
				pan_moved = false;
			}

			if (active && (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)))
			{
				if (std::fabs(io.MouseDelta.x) + std::fabs(io.MouseDelta.y) > 0.0f)
				{
					const ImVec2 total = ImGui::GetMouseDragDelta(ImGui::IsMouseDown(ImGuiMouseButton_Right) ? ImGuiMouseButton_Right : ImGuiMouseButton_Middle, 0.0f);
					pan_moved |= std::fabs(total.x) + std::fabs(total.y) > 4.0f;

					center[0] -= io.MouseDelta.x / zoom;
					center[1] += io.MouseDelta.y / zoom;
				}
			}

			if (hovered && io.MouseWheel != 0.0f)
			{
				zoom = std::clamp(zoom * std::pow(1.2, static_cast<double>(io.MouseWheel)), 0.002, 64.0);

				// the point under the mouse stays where it is
				double after_h, after_v;
				to_world(io.MousePos, after_h, after_v);
				center[0] += mouse_h - after_h;
				center[1] += mouse_v - after_v;
			}

			// a right click without panning: create an entity there
			if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !pan_moved && s.open)
			{
				st.create_position[axis] = depth_coordinate(axis);
				st.create_position[h()] = s.snapped(mouse_h);
				st.create_position[v()] = s.snapped(mouse_v);
				st.open_create_popup = true;
			}

			// ---- editing with the left button
			if (s.open)
			{
				if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					on_press(mouse_h, mouse_v, io.KeyShift);
				}
				else if (pressed && ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					on_drag(mouse_h, mouse_v, false);
				}
				else if (pressed)
				{
					on_release();
				}
			}

			// ---- draw
			auto* dl = ImGui::GetWindowDrawList();
			const ImVec2 corner(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);

			dl->PushClipRect(canvas_pos, corner, true);
			dl->AddRectFilled(canvas_pos, corner, IM_COL32(26, 27, 30, 255));

			// thousands of one pixel lines: no anti aliasing ( four vertices per line instead of many more )
			const ImDrawListFlags old_flags = dl->Flags;
			dl->Flags &= ~(ImDrawListFlags_AntiAliasedLines | ImDrawListFlags_AntiAliasedLinesUseTex | ImDrawListFlags_AntiAliasedFill);

			draw_grid(dl);

			int label_budget = 220;
			draw_cache(dl, s.statics, false, label_budget);

			if (s.show_connections)
			{
				for (const auto& c : s.connections)
				{
					const ImVec2 a = to_screen((&c.from.x)[h()], (&c.from.x)[v()]), b = to_screen((&c.to.x)[h()], (&c.to.x)[v()]);
					dl->AddLine(a, b, IM_COL32(255, 64, 255, 255));

					// arrow head at the target
					const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
					if (len > 24.0f)
					{
						const float ux = dx / len, uy = dy / len;
						dl->AddLine(b, ImVec2(b.x - ux * 9.0f - uy * 4.0f, b.y - uy * 9.0f + ux * 4.0f), IM_COL32(255, 64, 255, 255));
						dl->AddLine(b, ImVec2(b.x - ux * 9.0f + uy * 4.0f, b.y - uy * 9.0f - ux * 4.0f), IM_COL32(255, 64, 255, 255));
					}
				}
			}

			draw_cache(dl, s.selected, true, label_budget);
			dl->Flags = old_flags;

			// ---- the 3D camera: where it is and where it looks
			if (s.renderer.loaded())
			{
				const auto axes = s.cam.axes();
				const ImVec2 c = to_screen((&s.cam.origin.x)[h()], (&s.cam.origin.x)[v()]);
				float fx = (&axes.forward.x)[h()], fy = -(&axes.forward.x)[v()];
				const float len = std::sqrt(fx * fx + fy * fy);

				if (len > 1e-3f)
				{
					fx /= len;
					fy /= len;
				}
				else
				{
					fx = 0.0f;
					fy = -1.0f;
				}

				const ImU32 color = act == action::camera ? IM_COL32(255, 255, 255, 255) : IM_COL32(90, 170, 255, 255);
				const ImVec2 tip(c.x + fx * 22.0f, c.y + fy * 22.0f);
				dl->AddQuadFilled(ImVec2(c.x - 7.0f, c.y), ImVec2(c.x, c.y - 7.0f), ImVec2(c.x + 7.0f, c.y), ImVec2(c.x, c.y + 7.0f), color);
				dl->AddLine(c, tip, color, 2.0f);
				dl->AddLine(tip, ImVec2(tip.x - fx * 7.0f - fy * 5.0f, tip.y - fy * 7.0f + fx * 5.0f), color, 2.0f);
				dl->AddLine(tip, ImVec2(tip.x - fx * 7.0f + fy * 5.0f, tip.y - fy * 7.0f - fx * 5.0f), color, 2.0f);
			}

			// ---- tools
			if (st.vertex_mode && doc.selection().size() <= 256)
			{
				for (const object_id id : doc.selection())
				{
					const auto handle = [&](const edit::vec3& p)
					{
						const ImVec2 at = to_screen(p[h()], p[v()]);
						dl->AddRectFilled(ImVec2(at.x - 3.0f, at.y - 3.0f), ImVec2(at.x + 4.0f, at.y + 4.0f), IM_COL32(90, 255, 120, 255));
					};

					if (const auto* geo = doc.geometry(id))
					{
						for (const auto& p : geo->vertices) { handle(p); }
					}
					else if (const auto* patch = doc.find_patch(id))
					{
						for (const auto& pv : patch->vertices) { handle(pv.xyz); }
					}
				}
			}

			if (st.clipper.on && st.clipper.axis == axis && !st.clipper.points.empty())
			{
				const auto& points = st.clipper.points;
				const ImU32 color = IM_COL32(80, 200, 255, 255);

				for (std::size_t i = 0; i < points.size(); i++)
				{
					const ImVec2 at = to_screen(points[i][h()], points[i][v()]);
					dl->AddCircleFilled(at, 4.5f, color);
					char label[4] = { static_cast<char>('1' + i), 0 };
					dl->AddText(ImVec2(at.x + 6.0f, at.y - 16.0f), color, label);
				}

				if (points.size() >= 2)
				{
					const ImVec2 a = to_screen(points[0][h()], points[0][v()]), b = to_screen(points[1][h()], points[1][v()]);
					dl->AddLine(a, b, color, 2.0f);

					// the side that stays when Enter is pressed: in front of the plane
					edit::vec3 third = points.size() >= 3 ? points[2] : points[0];
					if (points.size() < 3)
					{
						third[axis] += 64.0;
					}

					edit::plane plane;
					if (edit::plane_from_points(points[0], points[1], third, plane))
					{
						float nx = static_cast<float>(plane.normal[h()]), ny = -static_cast<float>(plane.normal[v()]);
						const float len = std::sqrt(nx * nx + ny * ny);

						if (len > 1e-3f)
						{
							nx /= len;
							ny /= len;
							const ImVec2 mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), tip(mid.x + nx * 26.0f, mid.y + ny * 26.0f);
							dl->AddLine(mid, tip, color, 2.0f);
							dl->AddLine(tip, ImVec2(tip.x - nx * 7.0f - ny * 5.0f, tip.y - ny * 7.0f + nx * 5.0f), color, 2.0f);
							dl->AddLine(tip, ImVec2(tip.x - nx * 7.0f + ny * 5.0f, tip.y - ny * 7.0f - nx * 5.0f), color, 2.0f);
							dl->AddText(ImVec2(tip.x + 5.0f, tip.y - 8.0f), color, "keep");
						}
					}
				}
			}

			// ---- previews of a drag
			if (pressed && moved && (act == action::create || act == action::rect_select))
			{
				const bool creating = act == action::create;
				const double x0 = creating ? s.snapped(std::min(press[0], now[0])) : std::min(press[0], now[0]);
				const double x1 = creating ? s.snapped(std::max(press[0], now[0])) : std::max(press[0], now[0]);
				const double y0 = creating ? s.snapped(std::min(press[1], now[1])) : std::min(press[1], now[1]);
				const double y1 = creating ? s.snapped(std::max(press[1], now[1])) : std::max(press[1], now[1]);
				const ImVec2 a = to_screen(x0, y1), b = to_screen(x1, y0);

				if (creating)
				{
					dl->AddRect(a, b, IM_COL32(255, 58, 32, 255), 0.0f, 0, 2.0f);
					char size_text[48];
					std::snprintf(size_text, sizeof(size_text), "%g x %g", x1 - x0, y1 - y0);
					dl->AddText(ImVec2(a.x + 4.0f, a.y + 3.0f), IM_COL32(255, 200, 190, 255), size_text);
				}
				else
				{
					dl->AddRectFilled(a, b, IM_COL32(120, 170, 255, 40));
					dl->AddRect(a, b, IM_COL32(120, 170, 255, 255));
				}
			}

			// ---- corner text
			{
				char info[160];
				std::snprintf(info, sizeof(info), "%s    grid %g    zoom %.3g    %s %.0f  %s %.0f%s%s", axis_name(axis), s.grid, zoom,
					h() == 0 ? "x" : "y", s.snapped(mouse_h), v() == 1 ? "y" : "z", s.snapped(mouse_v),
					st.clipper.on ? "    CLIPPER" : "", st.vertex_mode ? "    VERTICES" : "");

				const ImVec2 size = ImGui::CalcTextSize(info);
				const ImVec2 at(canvas_pos.x + 8.0f, canvas_pos.y + canvas_size.y - size.y - 8.0f);
				dl->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 2.0f), ImVec2(at.x + size.x + 4.0f, at.y + size.y + 2.0f), IM_COL32(0, 0, 0, 160), 3.0f);
				dl->AddText(at, IM_COL32(225, 225, 225, 255), info);
			}

			if (!s.open)
			{
				dl->AddText(ImVec2(canvas_pos.x + 16.0f, canvas_pos.y + 24.0f), IM_COL32(150, 150, 150, 255), "Open a map: double click one in the Maps window, or File > Open map.");
			}

			dl->PopClipRect();
			ImGui::End();
		}
	}

	// ---- interface ---------------------------------------------------------------------------------------

	void add_grid_view()
	{
		// a closed view is opened again before a new one is made
		for (auto& view : g_views)
		{
			if (!view.open)
			{
				view.open = true;
				return;
			}
		}

		grid_view view;
		view.index = static_cast<int>(g_views.size());

		// the usual set: top, front, side
		view.axis = view.index % 3 == 0 ? 2 : (view.index % 3 == 1 ? 1 : 0);
		g_views.push_back(std::move(view));
	}

	int grid_view_axis(const int index)
	{
		const auto* view = view_at(index);
		return view ? view->axis : 2;
	}

	void cycle_grid_axis()
	{
		auto* view = view_at(get().hovered_2d >= 0 ? get().hovered_2d : 0);
		if (view)
		{
			view->axis = view->axis == 2 ? 1 : (view->axis == 1 ? 0 : 2);
			view->fit_request = true;
		}
	}

	void draw_grid_views()
	{
		if (g_views.empty())
		{
			add_grid_view();
		}

		for (std::size_t i = 0; i < g_views.size(); i++)
		{
			if (g_views[i].open)
			{
				g_views[i].draw();
			}
			else if (g_views[i].pressed)
			{
				g_views[i].on_release();
			}
		}
	}

	void draw_grid_view_menu_items()
	{
		for (auto& view : g_views)
		{
			char label[64];
			std::snprintf(label, sizeof(label), "2D View %d ( %s )", view.index + 1, axis_name(view.axis));
			ImGui::MenuItem(label, nullptr, &view.open);
		}

		if (ImGui::MenuItem("New 2D view"))
		{
			bool reopened = false;
			for (auto& view : g_views)
			{
				reopened |= !view.open;
			}

			add_grid_view();

			if (!reopened)
			{
				g_views.back().fit_request = true;
			}
		}
	}

	// view2d <new | axis x|y|z|next | center a b | zoom z | fit | zone 0|1 | click a b [shift] | drag a1 b1 a2 b2 [shift] | rclick a b> [view n]
	void grid_view_command(const std::vector<std::string>& args)
	{
		if (g_views.empty())
		{
			add_grid_view();
		}

		const auto number = [&](const std::size_t i) { return i < args.size() ? std::atof(args[i].c_str()) : 0.0; };
		const std::string what = args.size() > 1 ? args[1] : "";

		// "view <n>" at the end picks the view
		int index = 0;
		bool shift = false;

		for (std::size_t i = 2; i < args.size(); i++)
		{
			if (args[i] == "view" && i + 1 < args.size())
			{
				index = std::atoi(args[i + 1].c_str()) - 1;
			}

			shift |= args[i] == "shift";
		}

		if (what == "new")
		{
			add_grid_view();
			return;
		}

		auto* view = view_at(index);
		if (!view)
		{
			game::printf_to_console("^1there is no 2D view %d\n", index + 1);
			return;
		}

		if (what == "axis" && args.size() > 2)
		{
			view->axis = args[2] == "x" ? 0 : (args[2] == "y" ? 1 : (args[2] == "z" ? 2 : (view->axis == 2 ? 1 : (view->axis == 1 ? 0 : 2))));
			view->fit_request = true;
		}
		else if (what == "center" && args.size() > 3)
		{
			view->center[0] = number(2);
			view->center[1] = number(3);
		}
		else if (what == "zoom" && args.size() > 2)
		{
			view->zoom = std::clamp(number(2), 0.002, 64.0);
		}
		else if (what == "fit")
		{
			view->fit_request = true;
		}
		else if (what == "zone" && args.size() > 2)
		{
			view->show_zone_brushes = number(2) != 0.0;
		}
		else if (what == "click" && args.size() > 3)
		{
			view->on_press(number(2), number(3), shift);
			view->on_release();
		}
		else if (what == "drag" && args.size() > 5)
		{
			view->on_press(number(2), number(3), shift);
			view->on_drag(number(4), number(5), true);
			view->on_release();
		}
		else if (what == "rclick" && args.size() > 3)
		{
			auto& st = get();
			st.create_position[view->axis] = depth_coordinate(view->axis);
			st.create_position[view->h()] = get_scene().snapped(number(2));
			st.create_position[view->v()] = get_scene().snapped(number(3));
			st.open_create_popup = true;
		}
		else if (what != "info")
		{
			game::printf_to_console("usage: view2d <info | new | axis x|y|z|next | center a b | zoom z | fit | zone 0|1 | click a b [shift] | drag a1 b1 a2 b2 [shift] | rclick a b> [view n]\n");
			return;
		}

		game::printf_to_console("2D view %d: %s, centre %g %g, zoom %g\n", view->index + 1, axis_name(view->axis), view->center[0], view->center[1], view->zoom);
	}
}
