#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"
#include "edit3d.hpp"

#include <codol_edit/handles.hpp>

#include <ImGuizmo.h>

// Geometry editing in the 3D view. See edit3d.hpp for what the tools are; this file is their mouse handling
// ( press / drag / release on pixels of the view, like the 2D views have it on world coordinates ), their overlay
// and the console command that drives them.
//
// A drag applies its whole movement to the document as it was when the mouse went down ( scene::restart_drag ),
// so there is no accumulated error and one undo step per drag.

namespace editor::edit3d
{
	namespace
	{
		constexpr float PICK_RADIUS = 9.0f;		// pixels around a handle that take a click
		constexpr float DEG = 0.0174532925f;

		enum class action
		{
			none,
			swallow,		// the press is taken and does nothing ( a click that must not select )
			click,			// Alt click: through what is behind each other
			move,			// drag on the selection
			resize,			// drag next to a selected brush: the faces on that side follow
			face_drag,		// drag on a selected face: along its normal
			bounds,			// a square on the selection's box
			rect_select,	// Shift drag
			handle_drag,	// vertex / edge tool
			handle_rect,	// .. Shift drag: the handles in the rectangle
			clip_point,
			create,
		};

		enum class handle_kind
		{
			vertex,			// a corner of one or more selected brushes
			edge,
			patch_vertex,
		};

		struct handle
		{
			handle_kind kind = handle_kind::vertex;
			object_id object = edit::no_object;		// patch_vertex: the patch
			int index = -1;							// patch_vertex: index into patch::vertices
			edit::vec3 pos;							// where the handle is drawn ( an edge: its middle )
			edit::vec3 a, b;						// edge: its ends
			bool selected = false;
			bool on_screen = false;
			ImVec2 px;								// pixels of the view
		};

		struct edge_ends
		{
			edit::vec3 a, b;
		};

		struct state_t
		{
			bool edge_mode = false;
			bool create_mode = false;
			primitive prim = primitive::box;
			int sides = 8;

			// the view as it was drawn last
			ImVec2 origin = {};
			int width = 0;
			int height = 0;
			std::uint64_t open_serial = ~0ull;
			tool last_tool = tool::object;

			// ---- the press that is being followed
			action act = action::none;
			bool pressed = false;
			bool scripted = false;			// the press came from the console
			bool moved = false;
			bool drag_open = false;			// a document drag was started
			bool shift = false, ctrl = false, alt = false;
			float press_px[2] = {};
			float now_px[2] = {};
			edit::ray press_ray;
			edit::vec3 anchor;				// the point of the world the mouse took hold of
			int plane_axis = 2;				// the drag plane is perpendicular to this world axis
			edit::vec3 applied;				// the movement the document has now
			std::vector<edit::face_ref> faces;
			edit::vec3 normal;
			int bounds_axis = 0;
			int bounds_side = 1;
			int grabbed = -1;				// handle_drag: index into `handles` at the press
			bool grabbed_was_selected = false;

			// ---- handles of the vertex / edge tool
			std::vector<handle> handles;
			std::vector<edit::vec3> sel_points;					// selected corners, by position
			std::vector<edge_ends> sel_edges;
			std::vector<std::pair<object_id, int>> sel_patch;	// selected control points
			edit::vec3 sel_offset;								// a drag in progress has moved the selected handles by this
			float gizmo_matrix[16] = {};
			float gizmo_start[16] = {};
			bool gizmo_active = false;

			// ---- create
			int create_axis = 2;
			double create_level = 0.0;
			double create_dir = 1.0;
			edit::vec3 create_a, create_b;

			// ---- Alt click
			float last_click[2] = { -1e9f, -1e9f };

			int hovered_bounds = -1;
			std::string hint_text;
		};

		state_t g;

		// ---- small helpers --------------------------------------------------------------------------------

		edit::vec3 to_vec(const float3& v) { return { v.x, v.y, v.z }; }
		float3 to_float(const edit::vec3& v) { return { static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z) }; }

		int dominant_axis(const edit::vec3& v)
		{
			const double x = std::fabs(v.x), y = std::fabs(v.y), z = std::fabs(v.z);
			return x >= y ? (x >= z ? 0 : 2) : (y >= z ? 1 : 2);
		}

		edit::vec3 axis_vector(const int axis, const double length = 1.0)
		{
			edit::vec3 v;
			v[axis] = length;
			return v;
		}

		edit::ray ray_at(const float px, const float py)
		{
			float3 origin, direction;
			view::screen_ray(get_scene().cam, std::max(1, g.width), std::max(1, g.height), px, py, origin, direction);
			return { to_vec(origin), to_vec(direction) };
		}

		edit::vec3 view_forward()
		{
			return to_vec(get_scene().cam.axes().forward);
		}

		// world point -> pixels of the view
		bool project(const edit::vec3& p, ImVec2& out)
		{
			float xy[2], depth = 0.0f;
			if (g.width <= 0 || !view::project(get_scene().cam, g.width, g.height, to_float(p), xy, depth))
			{
				return false;
			}

			out = ImVec2(xy[0], xy[1]);
			return true;
		}

		// a line of the world, cut at the near plane, as two screen positions
		bool segment(const edit::vec3& a, const edit::vec3& b, ImVec2& sa, ImVec2& sb)
		{
			const auto& cam = get_scene().cam;
			const edit::vec3 eye = to_vec(cam.origin), forward = view_forward();
			const double near_z = static_cast<double>(cam.z_near) + 0.05;
			const double da = edit::dot(a - eye, forward), db = edit::dot(b - eye, forward);

			if (da < near_z && db < near_z)
			{
				return false;
			}

			const edit::vec3 ca = da < near_z ? a + (b - a) * ((near_z - da) / (db - da)) : a;
			const edit::vec3 cb = db < near_z ? b + (a - b) * ((near_z - db) / (da - db)) : b;

			if (!project(ca, sa) || !project(cb, sb))
			{
				return false;
			}

			sa.x += g.origin.x; sa.y += g.origin.y;
			sb.x += g.origin.x; sb.y += g.origin.y;
			return true;
		}

		void line3d(ImDrawList* dl, const edit::vec3& a, const edit::vec3& b, const ImU32 color, const float thickness = 1.0f)
		{
			ImVec2 sa, sb;
			if (segment(a, b, sa, sb))
			{
				dl->AddLine(sa, sb, color, thickness);
			}
		}

		void box3d(ImDrawList* dl, const edit::vec3& mins, const edit::vec3& maxs, const ImU32 color, const float thickness = 1.0f)
		{
			static constexpr int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
			const auto corner = [&](const int i) { return edit::vec3{ (i & 1) ? maxs.x : mins.x, (i & 2) ? maxs.y : mins.y, (i & 4) ? maxs.z : mins.z }; };

			for (const auto& e : edges)
			{
				line3d(dl, corner(e[0]), corner(e[1]), color, thickness);
			}
		}

		// a brush / patch of the zone that is not shown ( as the click of the view has it )
		bool hidden_from_view(const object_id id)
		{
			auto& s = get_scene();
			return s.show_brushes != brush_display::all && !s.doc.is_selected(id) && s.imported(id);
		}

		// what the document has under the ray, nearest first, without what the view does not show
		std::vector<edit::pick_result> doc_hits(const edit::ray& ray, const bool entities)
		{
			auto& s = get_scene();
			edit::pick_options options;
			options.entities = entities && s.show_entity_boxes;

			auto hits = s.doc.pick_all(ray, options);
			std::erase_if(hits, [](const edit::pick_result& hit) { return hit.kind != edit::object_kind::entity && hidden_from_view(hit.object); });
			return hits;
		}

		// the first surface under the ray - of the map as it is drawn, or of a brush / patch of the document - with
		// the direction it faces
		bool surface_hit(const edit::ray& ray, edit::vec3& point, edit::vec3& normal, object_id* object = nullptr, int* face = nullptr)
		{
			auto& s = get_scene();
			bool found = false;
			double best = 1e30;

			if (object) { *object = edit::no_object; }
			if (face) { *face = -1; }

			view::pick_result drawn;
			if (s.renderer.loaded() && s.renderer.pick_ray(to_float(ray.origin), to_float(ray.direction), drawn))
			{
				best = drawn.distance;
				point = to_vec(drawn.point);
				normal = to_vec(drawn.normal);
				found = true;
			}

			for (const auto& hit : doc_hits(ray, false))
			{
				if (hit.distance <= best + 0.05)
				{
					point = hit.point;
					normal = -ray.direction;

					if (const auto* geo = s.doc.geometry(hit.object); geo && hit.face >= 0 && static_cast<std::size_t>(hit.face) < geo->planes.size())
					{
						normal = geo->planes[static_cast<std::size_t>(hit.face)].normal;
					}

					if (object) { *object = hit.object; }
					if (face) { *face = hit.face; }
					found = true;
				}

				break;
			}

			return found;
		}

		// where the ray meets the plane through `through` that is perpendicular to world axis `axis`
		bool plane_point(const edit::ray& ray, const edit::vec3& through, const int axis, edit::vec3& out)
		{
			edit::plane pl;
			pl.normal = axis_vector(axis);
			pl.dist = through[axis];

			double t = 0.0;
			if (!edit::ray_plane(ray, pl, t) || t > 1e6)
			{
				return false;
			}

			out = ray.origin + ray.direction * t;
			out[axis] = through[axis];
			return true;
		}

		// where the ray meets the plane through `through` that faces the camera
		bool view_plane_point(const edit::ray& ray, const edit::vec3& through, edit::vec3& out)
		{
			edit::plane pl;
			pl.normal = view_forward();
			pl.dist = edit::dot(pl.normal, through);

			double t = 0.0;
			if (!edit::ray_plane(ray, pl, t))
			{
				return false;
			}

			out = ray.origin + ray.direction * t;
			return true;
		}

		// How far the mouse has pulled along the line through the anchor: the point of the line nearest to the mouse
		// ray. Looking along the line that point is not defined; then up on the screen pulls towards the camera.
		bool line_drag(const edit::ray& ray, const edit::vec3& direction, const float py, double& t)
		{
			const double along = edit::dot(g.press_ray.direction, direction);

			if (std::fabs(along) < 0.94)
			{
				double now = 0.0, start = 0.0;
				if (edit::closest_on_line(ray, g.anchor, direction, now) && edit::closest_on_line(g.press_ray, g.anchor, direction, start))
				{
					t = now - start;
					return true;
				}

				return false;
			}

			const auto& cam = get_scene().cam;
			const double distance = edit::length(g.anchor - to_vec(cam.origin));
			const double per_pixel = distance * 2.0 * std::tan(cam.fov_x * 0.5f * DEG) / std::max(1, g.width);
			t = static_cast<double>(g.press_px[1] - py) * per_pixel * (along < 0.0 ? 1.0 : -1.0);
			return true;
		}

		edit::vec3 snapped(const edit::vec3& v)
		{
			auto& s = get_scene();
			return { s.snapped(v.x), s.snapped(v.y), s.snapped(v.z) };
		}

		// the whole movement since the press, on the document as it was then
		template <typename Fn>
		void apply(const edit::vec3& delta, const char* name, Fn&& fn)
		{
			auto& s = get_scene();

			if (g.drag_open && delta == g.applied)
			{
				return; // the mouse moved, the snapped movement did not
			}

			if (!g.drag_open)
			{
				s.begin_drag(name);
				g.drag_open = true;
			}

			s.restart_drag();
			g.applied = delta;

			if (!(delta == edit::vec3{}))
			{
				fn();
			}
		}

		// ---- handles --------------------------------------------------------------------------------------

		bool same_point(const edit::vec3& a, const edit::vec3& b)
		{
			return edit::length(a - b) < 0.05;
		}

		bool point_selected(const edit::vec3& p)
		{
			for (const auto& q : g.sel_points)
			{
				if (same_point(p, q + g.sel_offset)) { return true; }
			}

			return false;
		}

		bool edge_selected(const edit::vec3& a, const edit::vec3& b)
		{
			for (const auto& e : g.sel_edges)
			{
				const edit::vec3 ea = e.a + g.sel_offset, eb = e.b + g.sel_offset;
				if ((same_point(a, ea) && same_point(b, eb)) || (same_point(a, eb) && same_point(b, ea))) { return true; }
			}

			return false;
		}

		bool patch_selected(const object_id id, const int index)
		{
			return std::find(g.sel_patch.begin(), g.sel_patch.end(), std::make_pair(id, index)) != g.sel_patch.end();
		}

		void clear_handle_selection()
		{
			g.sel_points.clear();
			g.sel_edges.clear();
			g.sel_patch.clear();
			g.sel_offset = {};
		}

		std::size_t selected_handle_count()
		{
			return g.sel_points.size() + g.sel_edges.size() + g.sel_patch.size();
		}

		void set_handle_selected(const handle& h, const bool on)
		{
			if (h.kind == handle_kind::vertex)
			{
				std::erase_if(g.sel_points, [&](const edit::vec3& q) { return same_point(h.pos, q); });
				if (on) { g.sel_points.push_back(h.pos); }
			}
			else if (h.kind == handle_kind::edge)
			{
				std::erase_if(g.sel_edges, [&](const edge_ends& e) { return (same_point(h.a, e.a) && same_point(h.b, e.b)) || (same_point(h.a, e.b) && same_point(h.b, e.a)); });
				if (on) { g.sel_edges.push_back({ h.a, h.b }); }
			}
			else
			{
				std::erase(g.sel_patch, std::make_pair(h.object, h.index));
				if (on) { g.sel_patch.emplace_back(h.object, h.index); }
			}
		}

		// the handles of the selection as the document has it now
		void build_handles()
		{
			auto& doc = get_scene().doc;
			const tool t = current();

			g.handles.clear();

			if ((t != tool::vertex && t != tool::edge) || doc.selection().size() > 256)
			{
				return;
			}

			constexpr std::size_t limit = 6000;

			for (const object_id id : doc.selection())
			{
				if (const auto* geo = doc.geometry(id))
				{
					if (t == tool::vertex)
					{
						for (const auto& p : geo->vertices)
						{
							// a corner brushes share is one handle
							const bool known = std::any_of(g.handles.begin(), g.handles.end(), [&](const handle& h) { return h.kind == handle_kind::vertex && same_point(h.pos, p); });

							if (!known && g.handles.size() < limit)
							{
								handle h;
								h.kind = handle_kind::vertex;
								h.object = id;
								h.pos = p;
								g.handles.push_back(h);
							}
						}
					}
					else
					{
						for (const auto& e : edit::brush_edges(*geo))
						{
							const auto& a = geo->vertices[static_cast<std::size_t>(e.a)];
							const auto& b = geo->vertices[static_cast<std::size_t>(e.b)];
							const bool known = std::any_of(g.handles.begin(), g.handles.end(), [&](const handle& h)
							{
								return h.kind == handle_kind::edge && ((same_point(h.a, a) && same_point(h.b, b)) || (same_point(h.a, b) && same_point(h.b, a)));
							});

							if (!known && g.handles.size() < limit)
							{
								handle h;
								h.kind = handle_kind::edge;
								h.object = id;
								h.a = a;
								h.b = b;
								h.pos = (a + b) * 0.5;
								g.handles.push_back(h);
							}
						}
					}
				}
				else if (const auto* patch = doc.find_patch(id); patch && t == tool::vertex)
				{
					for (std::size_t i = 0; i < patch->vertices.size() && g.handles.size() < limit; i++)
					{
						handle h;
						h.kind = handle_kind::patch_vertex;
						h.object = id;
						h.index = static_cast<int>(i);
						h.pos = patch->vertices[i].xyz;
						g.handles.push_back(h);
					}
				}
			}

			for (auto& h : g.handles)
			{
				h.selected = h.kind == handle_kind::vertex ? point_selected(h.pos) : (h.kind == handle_kind::edge ? edge_selected(h.a, h.b) : patch_selected(h.object, h.index));
				h.on_screen = project(h.pos, h.px);
			}

			// a selected handle that is gone ( undo, the hull dropped the corner, the brush was deselected )
			if (!g.pressed && !g.gizmo_active)
			{
				const auto has = [&](const auto& pred) { return std::any_of(g.handles.begin(), g.handles.end(), pred); };

				std::erase_if(g.sel_points, [&](const edit::vec3& q) { return !has([&](const handle& h) { return h.kind == handle_kind::vertex && same_point(h.pos, q); }); });
				std::erase_if(g.sel_edges, [&](const edge_ends& e)
				{
					return !has([&](const handle& h) { return h.kind == handle_kind::edge && ((same_point(h.a, e.a) && same_point(h.b, e.b)) || (same_point(h.a, e.b) && same_point(h.b, e.a))); });
				});
				std::erase_if(g.sel_patch, [&](const std::pair<object_id, int>& p)
				{
					return !has([&](const handle& h) { return h.kind == handle_kind::patch_vertex && h.object == p.first && h.index == p.second; });
				});
			}
		}

		int handle_at(const float px, const float py)
		{
			int best = -1;
			float best_d = PICK_RADIUS;

			for (std::size_t i = 0; i < g.handles.size(); i++)
			{
				const auto& h = g.handles[i];
				if (!h.on_screen)
				{
					continue;
				}

				const float d = std::max(std::fabs(h.px.x - px), std::fabs(h.px.y - py));

				// of handles at ( nearly ) the same place the selected one wins: it is the one a drag is meant for
				if (d < best_d || (best >= 0 && d < best_d + 0.5f && h.selected && !g.handles[static_cast<std::size_t>(best)].selected && d <= PICK_RADIUS))
				{
					best_d = d;
					best = static_cast<int>(i);
				}
			}

			return best;
		}

		// the selected handles by `delta`, on the document as it was when the drag began
		void move_handles(const edit::vec3& delta)
		{
			auto& doc = get_scene().doc;

			g.sel_offset = {};

			std::vector<edit::vec3> corners = g.sel_points;
			for (const auto& e : g.sel_edges)
			{
				corners.push_back(e.a);
				corners.push_back(e.b);
			}

			bool ok = false;

			if (!corners.empty())
			{
				ok |= doc.move_corners(corners, delta);
			}

			std::map<object_id, std::vector<int>> by_patch;
			for (const auto& [id, index] : g.sel_patch)
			{
				by_patch[id].push_back(index);
			}

			for (const auto& [id, indices] : by_patch)
			{
				ok |= doc.move_patch_vertices(id, indices, delta);
			}

			if (ok)
			{
				g.sel_offset = delta;
			}
		}

		// the drag is done: the selected handles are where it left them
		void commit_handle_offset()
		{
			for (auto& p : g.sel_points) { p += g.sel_offset; }
			for (auto& e : g.sel_edges) { e.a += g.sel_offset; e.b += g.sel_offset; }
			g.sel_offset = {};
		}

		// ---- the squares on the selection's box -----------------------------------------------------------

		struct bounds_handle
		{
			int axis = 0;
			int side = 1;
			edit::vec3 pos;
			ImVec2 px;
			bool on_screen = false;
		};

		bool selection_has_brush()
		{
			auto& doc = get_scene().doc;

			for (const object_id id : doc.selection())
			{
				if (doc.kind(id) == edit::object_kind::brush)
				{
					return true;
				}
			}

			return false;
		}

		std::vector<bounds_handle> bounds_handles()
		{
			std::vector<bounds_handle> out;
			auto& doc = get_scene().doc;
			const tool t = current();

			if ((t != tool::object && t != tool::create) || doc.selection().empty() || doc.selection().size() > 4096 || !selection_has_brush())
			{
				return out;
			}

			const auto box = doc.selection_bounds();
			if (!box.valid())
			{
				return out;
			}

			const auto center = box.center();

			for (int axis = 0; axis < 3; axis++)
			{
				for (const int side : { -1, 1 })
				{
					bounds_handle h;
					h.axis = axis;
					h.side = side;
					h.pos = center;
					h.pos[axis] = side > 0 ? box.maxs[axis] : box.mins[axis];
					h.on_screen = project(h.pos, h.px);
					out.push_back(h);
				}
			}

			return out;
		}

		int bounds_handle_at(const std::vector<bounds_handle>& handles, const float px, const float py)
		{
			int best = -1;
			float best_d = PICK_RADIUS;

			for (std::size_t i = 0; i < handles.size(); i++)
			{
				if (!handles[i].on_screen)
				{
					continue;
				}

				const float d = std::max(std::fabs(handles[i].px.x - px), std::fabs(handles[i].px.y - py));
				if (d < best_d)
				{
					best_d = d;
					best = static_cast<int>(i);
				}
			}

			return best;
		}

		// ---- selecting ------------------------------------------------------------------------------------

		// Alt click: the object behind the one that is selected now ( the first one when none of them is )
		void cycle_click(const edit::ray& ray, const bool add)
		{
			auto& s = get_scene();
			auto& doc = s.doc;
			const auto hits = doc_hits(ray, true);

			if (hits.empty())
			{
				return;
			}

			std::size_t next = 0;

			for (std::size_t i = hits.size(); i-- > 0;)
			{
				if (doc.is_selected(hits[i].object))
				{
					next = (i + 1) % hits.size();
					break;
				}
			}

			s.inspecting = false;
			doc.select(hits[next].object, add ? edit::select_mode::add : edit::select_mode::replace);
		}

		// the planes of the part of the view inside a rectangle of pixels, pointing out of it
		std::vector<edit::plane> rect_planes(float x0, float y0, float x1, float y1)
		{
			if (x0 > x1) { std::swap(x0, x1); }
			if (y0 > y1) { std::swap(y0, y1); }

			const edit::ray corners[4] = { ray_at(x0, y0), ray_at(x1, y0), ray_at(x1, y1), ray_at(x0, y1) };
			const edit::vec3 middle = ray_at((x0 + x1) * 0.5f, (y0 + y1) * 0.5f).direction;
			const edit::vec3 eye = corners[0].origin;

			std::vector<edit::plane> planes;

			for (int i = 0; i < 4; i++)
			{
				edit::plane pl;
				pl.normal = edit::normalize(edit::cross(corners[i].direction, corners[(i + 1) % 4].direction));

				if (edit::dot(pl.normal, middle) > 0.0)
				{
					pl.normal = -pl.normal;
				}

				pl.dist = edit::dot(pl.normal, eye);
				planes.push_back(pl);
			}

			// nothing behind the camera
			edit::plane near_plane;
			near_plane.normal = -view_forward();
			near_plane.dist = edit::dot(near_plane.normal, eye);
			planes.push_back(near_plane);
			return planes;
		}

		void rect_select(const float x0, const float y0, const float x1, const float y1)
		{
			auto& s = get_scene();
			auto& doc = s.doc;

			edit::pick_options options;
			options.entities = s.show_entity_boxes;

			const auto planes = rect_planes(x0, y0, x1, y1);
			std::vector<object_id> wanted;

			for (const object_id id : doc.query_volume(planes, false, options))
			{
				if (doc.kind(id) == edit::object_kind::entity || !hidden_from_view(id))
				{
					wanted.push_back(id);
				}
			}

			if (!wanted.empty())
			{
				s.inspecting = false;
				doc.select(std::span<const object_id>(wanted), edit::select_mode::add);
			}
		}

		// ---- clipper --------------------------------------------------------------------------------------

		bool clip_plane(edit::plane& out)
		{
			const auto& clipper = ui::get().clipper;
			const auto& points = clipper.points;

			if (points.size() < 2)
			{
				return false;
			}

			// as ui::apply_clip has it: two points make a plane along the clipper's axis
			edit::vec3 third = points.size() >= 3 ? points[2] : points[0];
			if (points.size() < 3)
			{
				third[clipper.axis] += 64.0;
			}

			return edit::plane_from_points(points[0], points[1], third, out);
		}

		// ---- create ---------------------------------------------------------------------------------------

		// the box the drag spans: the rectangle on the plane, as high as the last selection was
		bool create_box(edit::vec3& mins, edit::vec3& maxs)
		{
			auto& s = get_scene();
			const int axis = g.create_axis;

			for (int i = 0; i < 3; i++)
			{
				mins[i] = std::min(g.create_a[i], g.create_b[i]);
				maxs[i] = std::max(g.create_a[i], g.create_b[i]);
			}

			double height = std::max(64.0, s.grid);
			if (s.last_selection_box.valid() && s.last_selection_box.size()[axis] >= 1.0)
			{
				height = s.last_selection_box.size()[axis];
			}

			mins[axis] = g.create_dir > 0.0 ? g.create_level : g.create_level - height;
			maxs[axis] = g.create_dir > 0.0 ? g.create_level + height : g.create_level;

			const int u = axis == 0 ? 1 : 0, v = axis == 2 ? 1 : 2;
			return maxs[u] - mins[u] >= 1.0 && maxs[v] - mins[v] >= 1.0;
		}

		const char* primitive_name(const primitive p)
		{
			switch (p)
			{
			case primitive::box: return "Box";
			case primitive::cylinder: return "Cylinder";
			case primitive::cone: return "Cone";
			case primitive::wedge: return "Wedge";
			case primitive::terrain: return "Terrain patch";
			}

			return "";
		}

		object_id make_primitive(const edit::vec3& mins, const edit::vec3& maxs)
		{
			auto& doc = get_scene().doc;
			const int axis = g.create_axis;

			switch (g.prim)
			{
			case primitive::box:
				return doc.create_box(mins, maxs);

			case primitive::cylinder:
				return doc.create_cylinder(mins, maxs, g.sides, axis);

			case primitive::cone:
				return doc.create_cone(mins, maxs, g.sides, axis);

			case primitive::wedge:
				// the slope runs along the longer side of the rectangle that was dragged
				return doc.create_wedge(mins, maxs, maxs.x - mins.x >= maxs.y - mins.y ? 0 : 1);

			case primitive::terrain:
			{
				// flat, on the surface; a vertex every 64 units or so
				const int u = axis == 0 ? 1 : 0, v = axis == 2 ? 1 : 2;
				edit::vec3 lo = mins, hi = maxs;
				lo[axis] = hi[axis] = g.create_level;
				const int columns = std::clamp(static_cast<int>(std::lround((maxs[u] - mins[u]) / 64.0)) + 1, 2, 16);
				const int rows = std::clamp(static_cast<int>(std::lround((maxs[v] - mins[v]) / 64.0)) + 1, 2, 16);
				return doc.create_terrain_patch(lo, hi, columns, rows, axis);
			}
			}

			return edit::no_object;
		}

		const char* tool_name(const tool t)
		{
			switch (t)
			{
			case tool::object: return "object";
			case tool::vertex: return "vertex";
			case tool::edge: return "edge";
			case tool::clip: return "clip";
			case tool::create: return "create";
			}

			return "";
		}

		const char* action_name(const action a)
		{
			switch (a)
			{
			case action::none: return "none";
			case action::swallow: return "swallow";
			case action::click: return "click";
			case action::move: return "move";
			case action::resize: return "resize";
			case action::face_drag: return "face";
			case action::bounds: return "bounds";
			case action::rect_select: return "rect";
			case action::handle_drag: return "handle";
			case action::handle_rect: return "handle rect";
			case action::clip_point: return "clip point";
			case action::create: return "create";
			}

			return "";
		}
	}

	// ---- tools -----------------------------------------------------------------------------------------------

	tool current()
	{
		const auto& st = ui::get();

		if (st.clipper.on)
		{
			g.create_mode = false;
			return tool::clip;
		}

		if (st.vertex_mode)
		{
			g.create_mode = false;
			return g.edge_mode ? tool::edge : tool::vertex;
		}

		g.edge_mode = false;
		return g.create_mode ? tool::create : tool::object;
	}

	void set_tool(const tool t)
	{
		if (busy())
		{
			cancel();
		}

		if (t == tool::clip)
		{
			ui::set_vertex_mode(false);

			if (!ui::get().clipper.on)
			{
				ui::set_clipper(true);
			}
		}
		else
		{
			if (ui::get().clipper.on)
			{
				ui::set_clipper(false);
			}

			ui::set_vertex_mode(t == tool::vertex || t == tool::edge);
		}

		g.edge_mode = t == tool::edge;
		g.create_mode = t == tool::create;

		if (t != g.last_tool)
		{
			g.last_tool = t;
			clear_handle_selection();
		}
	}

	bool busy()
	{
		return g.pressed;
	}

	bool scripted()
	{
		return g.pressed && g.scripted;
	}

	void cancel()
	{
		if (g.drag_open)
		{
			g.drag_open = false;
			get_scene().cancel_drag();
		}

		if (g.act == action::clip_point)
		{
			ui::get().clipper.dragging = -1;
		}

		g.sel_offset = {};
		g.pressed = false;
		g.moved = false;
		g.act = action::none;
	}

	bool replaces_gizmo()
	{
		const tool t = current();
		return t == tool::vertex || t == tool::edge || t == tool::clip;
	}

	// ---- mouse -----------------------------------------------------------------------------------------------

	bool press(const float px, const float py, const bool shift, const bool ctrl, const bool alt)
	{
		auto& s = get_scene();
		auto& doc = s.doc;
		auto& st = ui::get();

		if (!s.open || s.dragging() || g.width <= 0)
		{
			return false;
		}

		g.act = action::none;
		g.scripted = false;
		g.moved = false;
		g.drag_open = false;
		g.shift = shift;
		g.ctrl = ctrl;
		g.alt = alt;
		g.press_px[0] = g.now_px[0] = px;
		g.press_px[1] = g.now_px[1] = py;
		g.press_ray = ray_at(px, py);
		g.applied = {};
		g.faces.clear();
		g.grabbed = -1;
		g.plane_axis = dominant_axis(view_forward());

		const edit::ray& ray = g.press_ray;
		const tool t = current();

		const auto try_bounds = [&]
		{
			const auto handles = bounds_handles();
			const int at = shift || ctrl || alt ? -1 : bounds_handle_at(handles, px, py);

			if (at >= 0)
			{
				g.bounds_axis = handles[static_cast<std::size_t>(at)].axis;
				g.bounds_side = handles[static_cast<std::size_t>(at)].side;
				g.anchor = handles[static_cast<std::size_t>(at)].pos;
				g.act = action::bounds;
			}

			return at >= 0;
		};

		if (t == tool::clip)
		{
			auto& points = st.clipper.points;
			st.clipper.dragging = -1;
			float best = PICK_RADIUS;

			for (std::size_t i = 0; i < points.size(); i++)
			{
				ImVec2 at;
				if (project(points[i], at) && std::max(std::fabs(at.x - px), std::fabs(at.y - py)) < best)
				{
					best = std::max(std::fabs(at.x - px), std::fabs(at.y - py));
					st.clipper.dragging = static_cast<int>(i);
				}
			}

			if (st.clipper.dragging < 0 && points.size() < 3)
			{
				edit::vec3 point, normal;

				if (surface_hit(ray, point, normal))
				{
					if (points.empty())
					{
						// two points cut straight through the surface they were put on
						st.clipper.axis = dominant_axis(normal);
					}

					points.push_back(snapped(point));
					st.clipper.dragging = static_cast<int>(points.size()) - 1;
				}
			}

			// a click next to the points does not change the selection that is about to be cut
			g.act = st.clipper.dragging >= 0 ? action::clip_point : action::swallow;
		}
		else if (t == tool::vertex || t == tool::edge)
		{
			build_handles();
			const int at = handle_at(px, py);

			if (at >= 0)
			{
				const handle h = g.handles[static_cast<std::size_t>(at)];
				g.grabbed = at;
				g.grabbed_was_selected = h.selected;

				if (!h.selected)
				{
					if (!shift && !ctrl)
					{
						clear_handle_selection();
					}

					set_handle_selected(h, true);
				}

				g.anchor = h.pos;
				g.act = action::handle_drag;
			}
			else if (shift)
			{
				g.act = action::handle_rect;
			}
		}
		else if (t == tool::create && !shift && !ctrl && !alt)
		{
			if (!try_bounds())
			{
				edit::vec3 point, normal;

				if (surface_hit(ray, point, normal))
				{
					g.create_axis = dominant_axis(normal);
					g.create_dir = normal[g.create_axis] >= 0.0 ? 1.0 : -1.0;
				}
				else
				{
					// nothing there: the floor the last brush stood on
					g.create_axis = 2;
					edit::vec3 floor;
					floor.z = s.last_selection_box.valid() ? s.last_selection_box.mins.z : 0.0;
					g.create_dir = ray.origin.z >= floor.z ? 1.0 : -1.0;

					if (!plane_point(ray, floor, 2, point))
					{
						point = ray.origin + ray.direction * 256.0;
					}
				}

				g.create_level = s.snapped(point[g.create_axis]);
				g.create_a = snapped(point);
				g.create_a[g.create_axis] = g.create_level;
				g.create_b = g.create_a;
				g.act = action::create;
			}
		}
		else if (t == tool::object || t == tool::create)
		{
			if (shift && ctrl)
			{
				// the face under the mouse: the click of the view
			}
			else if (alt)
			{
				g.act = action::click;
			}
			else if (shift)
			{
				g.act = action::rect_select;
			}
			else if (!ctrl && !try_bounds() && (!doc.selection().empty() || !doc.selected_faces().empty()))
			{
				const auto hits = doc_hits(ray, true);

				// a selected face: along its normal
				for (const auto& hit : hits)
				{
					if (hit.kind == edit::object_kind::brush && hit.face >= 0 && doc.is_face_selected({ hit.object, hit.face }))
					{
						if (const auto* geo = doc.geometry(hit.object))
						{
							g.normal = geo->planes[static_cast<std::size_t>(hit.face)].normal;
							g.anchor = hit.point;
							g.faces.assign(doc.selected_faces().begin(), doc.selected_faces().end());
							g.act = action::face_drag;
						}
					}

					break;
				}

				// the selection itself: it moves
				if (g.act == action::none)
				{
					for (const auto& hit : hits)
					{
						if (doc.is_selected(hit.object))
						{
							g.anchor = hit.point;
							g.act = action::move;
							break;
						}
					}
				}

				// next to a selected brush: the faces on that side
				if (g.act == action::none)
				{
					for (const object_id id : doc.selection())
					{
						if (const auto* geo = doc.geometry(id); geo && doc.kind(id) == edit::object_kind::brush)
						{
							for (const int f : edit::drag_faces(*geo, ray))
							{
								g.faces.push_back({ id, f });
							}
						}
					}

					if (!g.faces.empty())
					{
						// the depth of the drag: where the ray passes the selection
						const auto center = doc.selection_bounds().center();
						g.anchor = ray.origin + ray.direction * std::max(1.0, edit::dot(center - ray.origin, ray.direction));
						g.act = action::resize;
					}
				}
			}
		}

		g.pressed = g.act != action::none;
		return g.pressed;
	}

	void drag(const float px, const float py, const bool force)
	{
		if (!g.pressed)
		{
			return;
		}

		if (!g.moved && !force && std::fabs(px - g.press_px[0]) + std::fabs(py - g.press_px[1]) < 4.0f)
		{
			return; // still a click
		}

		auto& s = get_scene();
		auto& doc = s.doc;
		auto& st = ui::get();

		g.moved = true;
		g.now_px[0] = px;
		g.now_px[1] = py;

		const edit::ray ray = ray_at(px, py);

		// the movement of the anchor on the drag plane ( with Alt: along the axis the plane is perpendicular to )
		const auto planar = [&](edit::vec3& delta)
		{
			if (g.alt)
			{
				double t = 0.0;
				if (!line_drag(ray, axis_vector(g.plane_axis), py, t))
				{
					return false;
				}

				delta = axis_vector(g.plane_axis, s.snapped(t));
				return true;
			}

			edit::vec3 now, start;
			if (!plane_point(ray, g.anchor, g.plane_axis, now) || !plane_point(g.press_ray, g.anchor, g.plane_axis, start))
			{
				return false;
			}

			delta = snapped(now - start);
			delta[g.plane_axis] = 0.0;
			return true;
		};

		switch (g.act)
		{
		case action::move:
		{
			edit::vec3 delta;
			if (planar(delta))
			{
				apply(delta, "Move", [&] { s.translate(delta); });
			}

			break;
		}

		case action::resize:
		{
			edit::vec3 now;
			if (view_plane_point(ray, g.anchor, now))
			{
				const edit::vec3 delta = snapped(now - g.anchor);
				apply(delta, "Resize", [&] { doc.move_faces(g.faces, delta); });
			}

			break;
		}

		case action::face_drag:
		{
			double t = 0.0;
			if (line_drag(ray, g.normal, py, t))
			{
				const edit::vec3 delta = g.normal * s.snapped(t);
				apply(delta, "Move face", [&] { doc.move_faces(g.faces, delta); });
			}

			break;
		}

		case action::bounds:
		{
			double t = 0.0;
			if (line_drag(ray, axis_vector(g.bounds_axis), py, t))
			{
				const edit::vec3 delta = axis_vector(g.bounds_axis, s.snapped(t));
				apply(delta, "Resize", [&] { s.resize_selection(g.bounds_axis, g.bounds_side, delta[g.bounds_axis]); });
			}

			break;
		}

		case action::handle_drag:
		{
			edit::vec3 delta;
			if (planar(delta))
			{
				apply(delta, current() == tool::edge ? "Move edges" : "Move vertices", [&] { move_handles(delta); });

				if (delta == edit::vec3{})
				{
					g.sel_offset = {};
				}
			}

			break;
		}

		case action::clip_point:
			if (st.clipper.dragging >= 0 && static_cast<std::size_t>(st.clipper.dragging) < st.clipper.points.size())
			{
				edit::vec3 point, normal;
				if (surface_hit(ray, point, normal))
				{
					st.clipper.points[static_cast<std::size_t>(st.clipper.dragging)] = snapped(point);
				}
			}

			break;

		case action::create:
		{
			edit::vec3 level, now;
			level[g.create_axis] = g.create_level;

			if (plane_point(ray, level, g.create_axis, now))
			{
				g.create_b = snapped(now);
				g.create_b[g.create_axis] = g.create_level;
			}

			break;
		}

		default:
			break; // the rectangles are drawn and take effect on release
		}
	}

	void release()
	{
		if (!g.pressed)
		{
			return;
		}

		auto& s = get_scene();
		auto& doc = s.doc;

		const action act = g.act;
		g.pressed = false;
		g.act = action::none;

		if (g.drag_open)
		{
			g.drag_open = false;
			s.end_drag();

			if (act == action::handle_drag)
			{
				commit_handle_offset();
			}
		}

		if (act == action::clip_point)
		{
			ui::get().clipper.dragging = -1;
			return;
		}

		const float px = g.press_px[0], py = g.press_px[1];

		if (!g.moved)
		{
			switch (act)
			{
			case action::click:
				cycle_click(g.press_ray, g.shift);
				break;

			case action::handle_drag:
				// a click on a handle that was selected, with Ctrl or Shift: out of the selection; without: only it
				if (g.grabbed >= 0 && static_cast<std::size_t>(g.grabbed) < g.handles.size() && g.grabbed_was_selected)
				{
					const handle h = g.handles[static_cast<std::size_t>(g.grabbed)];

					if (g.shift || g.ctrl)
					{
						set_handle_selected(h, false);
					}
					else
					{
						clear_handle_selection();
						set_handle_selected(h, true);
					}
				}

				break;

			case action::rect_select:
			case action::handle_rect:
				ui::viewport_click(px, py, true, false);
				break;

			case action::move:
			case action::resize:
			case action::face_drag:
			case action::bounds:
			case action::create:
				ui::viewport_click(px, py, false, false);
				break;

			default:
				break;
			}

			return;
		}

		if (act == action::rect_select)
		{
			rect_select(g.press_px[0], g.press_px[1], g.now_px[0], g.now_px[1]);
		}
		else if (act == action::handle_rect)
		{
			const float x0 = std::min(g.press_px[0], g.now_px[0]), x1 = std::max(g.press_px[0], g.now_px[0]);
			const float y0 = std::min(g.press_px[1], g.now_px[1]), y1 = std::max(g.press_px[1], g.now_px[1]);

			build_handles();

			for (const auto& h : g.handles)
			{
				if (h.on_screen && h.px.x >= x0 && h.px.x <= x1 && h.px.y >= y0 && h.px.y <= y1)
				{
					set_handle_selected(h, true);
				}
			}
		}
		else if (act == action::create)
		{
			edit::vec3 mins, maxs;

			if (create_box(mins, maxs))
			{
				ui::report(make_primitive(mins, maxs) != edit::no_object, g.prim == primitive::terrain ? "New patch" : "New brush");
			}
		}
	}

	// ---- material drop ---------------------------------------------------------------------------------------

	bool drop_material(const std::string& material, const float px, const float py, const bool whole_brush)
	{
		auto& s = get_scene();
		auto& doc = s.doc;

		if (!s.open || material.empty() || g.width <= 0)
		{
			return false;
		}

		const edit::ray ray = ray_at(px, py);
		edit::vec3 point, normal;
		object_id target = edit::no_object;
		int face = -1;

		if (!surface_hit(ray, point, normal, &target, &face) || target == edit::no_object)
		{
			ImGui::Toast(ImGuiToastType_Warning, "Set material", "Drop it on a brush or a patch. ( The world of a shipped zone is not made of brushes. )", 3500);
			game::printf_to_console("Set material: no brush or patch there\n");
			return false;
		}

		// The operation works on the selection. Inside one step the selection can be anything: the step remembers the
		// one from before it began and the one at its end.
		const std::vector<object_id> objects(doc.selection().begin(), doc.selection().end());
		const std::vector<edit::face_ref> faces(doc.selected_faces().begin(), doc.selected_faces().end());

		doc.begin_transaction("Set material");
		doc.deselect_all();

		if (doc.kind(target) == edit::object_kind::brush && !whole_brush && face >= 0)
		{
			doc.select_face({ target, face });
		}
		else
		{
			doc.select(target);
		}

		const bool ok = doc.set_material(material);

		doc.deselect_all();
		doc.select(std::span<const object_id>(objects));

		for (const auto& f : faces)
		{
			doc.select_face(f, edit::select_mode::add);
		}

		doc.end_transaction();
		return ui::report(ok, "Set material");
	}

	// ---- keys ------------------------------------------------------------------------------------------------

	bool shortcuts()
	{
		auto& s = get_scene();
		auto& doc = s.doc;
		auto& st = ui::get();
		const auto& io = ImGui::GetIO();
		const bool ctrl = io.KeyCtrl, shift = io.KeyShift, alt = io.KeyAlt;
		const auto pressed = [](const ImGuiKey key, const bool repeat = false) { return ImGui::IsKeyPressed(key, repeat); };

		if (!ctrl && !alt && !shift)
		{
			// E is the rotate gizmo; while the handles are shown - where nothing is rotated - it is Radiant's edge mode
			if (st.vertex_mode && pressed(ImGuiKey_E))
			{
				g.edge_mode = !g.edge_mode;
				clear_handle_selection();
				return true;
			}

			// V in edge mode goes back to the vertices ( and, from there, out )
			if (st.vertex_mode && g.edge_mode && pressed(ImGuiKey_V))
			{
				g.edge_mode = false;
				clear_handle_selection();
				return true;
			}

			if (pressed(ImGuiKey_B))
			{
				set_tool(current() == tool::create ? tool::object : tool::create);
				return true;
			}

			if (pressed(ImGuiKey_Escape))
			{
				// one thing at a time ( the clipper's points and tools come after, in ui::process_shortcuts )
				if (st.vertex_mode && selected_handle_count() > 0)
				{
					clear_handle_selection();
					return true;
				}

				if (g.create_mode)
				{
					g.create_mode = false;
					return true;
				}
			}
		}

		// ---- textures of the selected faces / of the faces of the selection, as Radiant has them
		if (doc.selection().empty() && doc.selected_faces().empty())
		{
			return false;
		}

		const auto step = [&](const char* name, const std::uint64_t key, const auto& fn)
		{
			// a key that is held is one undo step
			doc.begin_transaction(name, key);
			const bool ok = fn();
			doc.end_transaction();
			ui::report(ok, name);
			return true;
		};

		if (shift && !ctrl && !alt)
		{
			const double amount = std::max(1.0, s.grid);

			if (pressed(ImGuiKey_LeftArrow, true)) { return step("Shift texture", 0x3D7E0001, [&] { return doc.shift_texture(-amount, 0.0); }); }
			if (pressed(ImGuiKey_RightArrow, true)) { return step("Shift texture", 0x3D7E0001, [&] { return doc.shift_texture(amount, 0.0); }); }
			if (pressed(ImGuiKey_UpArrow, true)) { return step("Shift texture", 0x3D7E0001, [&] { return doc.shift_texture(0.0, amount); }); }
			if (pressed(ImGuiKey_DownArrow, true)) { return step("Shift texture", 0x3D7E0001, [&] { return doc.shift_texture(0.0, -amount); }); }
			if (pressed(ImGuiKey_PageUp, true)) { return step("Rotate texture", 0x3D7E0002, [&] { return doc.rotate_texture(s.snap_angle); }); }
			if (pressed(ImGuiKey_PageDown, true)) { return step("Rotate texture", 0x3D7E0002, [&] { return doc.rotate_texture(-s.snap_angle); }); }
		}

		if (ctrl && !shift && !alt)
		{
			constexpr double factor = 1.05;

			if (pressed(ImGuiKey_LeftArrow, true)) { return step("Scale texture", 0x3D7E0003, [&] { return doc.scale_texture(1.0 / factor, 1.0); }); }
			if (pressed(ImGuiKey_RightArrow, true)) { return step("Scale texture", 0x3D7E0003, [&] { return doc.scale_texture(factor, 1.0); }); }
			if (pressed(ImGuiKey_UpArrow, true)) { return step("Scale texture", 0x3D7E0003, [&] { return doc.scale_texture(1.0, factor); }); }
			if (pressed(ImGuiKey_DownArrow, true)) { return step("Scale texture", 0x3D7E0003, [&] { return doc.scale_texture(1.0, 1.0 / factor); }); }
		}

		return false;
	}

	// ---- gizmo on the selected handles -------------------------------------------------------------------------

	bool handle_gizmo(ImDrawList* dl, const ImVec2& origin, const int width, const int height)
	{
		auto& s = get_scene();
		const auto& io = ImGui::GetIO();
		const tool t = current();

		const auto stop = [&]
		{
			if (g.gizmo_active)
			{
				g.gizmo_active = false;
				s.end_drag();
				commit_handle_offset();
			}

			return false;
		};

		if ((t != tool::vertex && t != tool::edge) || selected_handle_count() == 0)
		{
			return stop();
		}

		// a drag with the mouse on a handle owns the document
		if (s.dragging() && !g.gizmo_active)
		{
			return false;
		}

		if (!g.gizmo_active)
		{
			edit::vec3 sum;
			int count = 0;

			for (const auto& h : g.handles)
			{
				if (h.selected)
				{
					sum += h.pos;
					count++;
				}
			}

			if (count == 0)
			{
				return false;
			}

			std::memset(g.gizmo_matrix, 0, sizeof(g.gizmo_matrix));
			g.gizmo_matrix[0] = g.gizmo_matrix[5] = g.gizmo_matrix[10] = g.gizmo_matrix[15] = 1.0f;
			g.gizmo_matrix[12] = static_cast<float>(sum.x / count);
			g.gizmo_matrix[13] = static_cast<float>(sum.y / count);
			g.gizmo_matrix[14] = static_cast<float>(sum.z / count);
		}

		float view_m[16], proj_m[16];
		view::view_matrix(s.cam, view_m);
		view::projection_matrix(s.cam, width, height, proj_m);

		// the renderer's projection has no far plane; the gizmo wants one
		const float n = s.cam.z_near, f = 262144.0f;
		proj_m[10] = f / (f - n);
		proj_m[14] = -n * f / (f - n);
		proj_m[11] = 1.0f;
		proj_m[15] = 0.0f;

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist(dl);
		ImGuizmo::SetRect(origin.x, origin.y, static_cast<float>(width), static_cast<float>(height));
		ImGuizmo::SetGizmoSizeClipSpace(0.10f);

		const bool snap = s.snap != io.KeyCtrl;
		const float step = static_cast<float>(s.grid);
		const float snap_values[3] = { step, step, step };

		dl->PushClipRect(origin, ImVec2(origin.x + static_cast<float>(width), origin.y + static_cast<float>(height)), true);
		const bool changed = ImGuizmo::Manipulate(view_m, proj_m, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, g.gizmo_matrix, nullptr, snap ? snap_values : nullptr);
		dl->PopClipRect();

		const bool using_it = ImGuizmo::IsUsing();

		if (using_it && !g.gizmo_active)
		{
			g.gizmo_active = true;
			std::memcpy(g.gizmo_start, g.gizmo_matrix, sizeof(g.gizmo_start));
			s.begin_drag(t == tool::edge ? "Move edges" : "Move vertices");
		}

		if (g.gizmo_active && changed)
		{
			s.restart_drag();
			move_handles({ static_cast<double>(g.gizmo_matrix[12]) - g.gizmo_start[12], static_cast<double>(g.gizmo_matrix[13]) - g.gizmo_start[13], static_cast<double>(g.gizmo_matrix[14]) - g.gizmo_start[14] });
		}

		if (!using_it && g.gizmo_active)
		{
			stop();
		}

		return using_it || ImGuizmo::IsOver();
	}

	// ---- toolbar ---------------------------------------------------------------------------------------------

	void toolbar()
	{
		const tool now = current();

		const auto button = [&](const char* label, const tool which, const char* tip)
		{
			const bool on = now == which;
			if (on)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}

			if (ImGui::Button(label))
			{
				set_tool(on && which != tool::object ? tool::object : which);
			}

			if (on)
			{
				ImGui::PopStyleColor();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", tip);
			}

			ImGui::SameLine();
		};

		button("Select", tool::object, "Click selects. Drag on the selection: move it. Drag next to a selected brush: the faces on that side follow.\n"
			"The squares on the selection's box resize it. Ctrl+Shift click: a face; drag a selected face along its normal.\nShift drag: select what is in the rectangle. Alt click: what is behind.");
		button("Vertices", tool::vertex, "V. Handles on the corners of the selected brushes and the control points of the selected patches.\n"
			"Click / Ctrl click / Shift drag selects handles; drag moves them on the grid ( Alt: up and down ); the arrows move them along one axis.");
		button("Edges", tool::edge, "E while the vertices are shown. Handles on the edges of the selected brushes; an edge moves as a whole.");
		button("Clipper", tool::clip, "C. Click two or three points on surfaces; Enter clips ( the side of the arrow stays ),\nCtrl+Enter keeps the other side, Shift+Enter splits.");
		button("Create", tool::create, "B. Drag a rectangle on the surface under the mouse: a new brush stands on it, as high as the last selection.\nThen drag the squares on its box.");

		if (now == tool::create)
		{
			ImGui::SetNextItemWidth(130.0f);

			if (ImGui::BeginCombo("##ed3d_primitive", primitive_name(g.prim)))
			{
				for (const primitive p : { primitive::box, primitive::cylinder, primitive::cone, primitive::wedge, primitive::terrain })
				{
					if (ImGui::Selectable(primitive_name(p), g.prim == p))
					{
						g.prim = p;
					}
				}

				ImGui::EndCombo();
			}

			if (g.prim == primitive::cylinder || g.prim == primitive::cone)
			{
				ImGui::SameLine();
				ImGui::SetNextItemWidth(90.0f);
				ImGui::InputInt("sides##ed3d_sides", &g.sides);
				g.sides = std::clamp(g.sides, 3, 64);
			}
		}
		else
		{
			ImGui::NewLine();
		}
	}

	const char* hint()
	{
		switch (current())
		{
		case tool::vertex:
		case tool::edge:
			g.hint_text = current() == tool::edge ? "EDGES" : "VERTICES";
			g.hint_text += "    Click a handle: select ( Ctrl / Shift adds )    Shift drag: handles in the rectangle    Drag a handle: move on the grid ( Alt: along the third axis )\n"
				"Arrows of the gizmo: along one axis    E: edges / vertices    V: leave    Esc: deselect handles    Ctrl+Z: undo    Right drag: look + W A S D";
			break;

		case tool::clip:
			g.hint_text = "CLIPPER    Click on a surface: a clip point ( two or three; drag to move them )    Enter: clip, the side of the arrow stays\n"
				"Ctrl+Enter: keep the other side    Shift+Enter: split    Esc: remove the points / leave    Right drag: look + W A S D";
			break;

		case tool::create:
			g.hint_text = "CREATE ";
			g.hint_text += primitive_name(g.prim);
			g.hint_text += "    Drag on a surface: the base of a new brush, as high as the last selection    Then drag the squares on its box to size it\n"
				"Click: select    B / Esc: leave    Right drag: look + W A S D";
			break;

		default:
			g.hint_text = "Click: select ( Ctrl / Shift adds, both: face, Alt: what is behind )    Shift drag: select in the rectangle    Right click: create entity    Space: clone\n"
				"Drag the selection: move    Drag next to a brush: its faces on that side    Squares: resize    Drag a selected face: along its normal\n"
				"V vertices   C clipper   B create   W E R gizmo   Arrows: nudge   Shift / Ctrl+Arrows, Shift+PgUp / PgDn: texture shift / scale / rotate   Right drag: look + W A S D";
			break;
		}

		return g.hint_text.c_str();
	}

	// ---- overlay ---------------------------------------------------------------------------------------------

	void draw(ImDrawList* dl, const ImVec2& origin, const int width, const int height, const bool hovered)
	{
		auto& s = get_scene();
		auto& doc = s.doc;
		auto& st = ui::get();
		const auto& io = ImGui::GetIO();

		g.origin = origin;
		g.width = width;
		g.height = height;

		// another map: nothing of the old one is left to hold on to
		if (g.open_serial != s.open_serial)
		{
			g.open_serial = s.open_serial;
			g.pressed = false;
			g.drag_open = false;
			g.act = action::none;
			g.gizmo_active = false;
			clear_handle_selection();
		}

		const tool t = current();

		if (t != g.last_tool)
		{
			g.last_tool = t;
			clear_handle_selection();
		}

		if (g.pressed && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			cancel();
		}

		const float mx = io.MousePos.x - origin.x, my = io.MousePos.y - origin.y;
		const auto at = [&](const ImVec2& px) { return ImVec2(origin.x + px.x, origin.y + px.y); };

		// ---- the squares on the selection's box
		g.hovered_bounds = -1;

		if (!g.pressed || g.act == action::bounds)
		{
			const auto handles = bounds_handles();
			const int over = hovered && !g.pressed ? bounds_handle_at(handles, mx, my) : -1;
			g.hovered_bounds = over;

			for (std::size_t i = 0; i < handles.size(); i++)
			{
				const auto& h = handles[i];
				if (!h.on_screen)
				{
					continue;
				}

				const bool hot = static_cast<int>(i) == over || (g.act == action::bounds && g.pressed && h.axis == g.bounds_axis && h.side == g.bounds_side);
				const ImU32 color = h.axis == 0 ? IM_COL32(255, 90, 90, 255) : (h.axis == 1 ? IM_COL32(110, 230, 110, 255) : IM_COL32(100, 150, 255, 255));
				const float r = hot ? 6.0f : 4.5f;
				const ImVec2 c = at(h.px);
				dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), hot ? IM_COL32(255, 255, 255, 255) : color);
				dl->AddRect(ImVec2(c.x - r - 1.0f, c.y - r - 1.0f), ImVec2(c.x + r + 1.0f, c.y + r + 1.0f), IM_COL32(0, 0, 0, 220));
			}

			if (over >= 0)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
			}
		}

		// ---- vertex / edge handles
		if (t == tool::vertex || t == tool::edge)
		{
			build_handles();
			const int over = hovered && !g.pressed ? handle_at(mx, my) : -1;

			if (t == tool::edge)
			{
				for (const auto& h : g.handles)
				{
					if (h.selected)
					{
						line3d(dl, h.a, h.b, IM_COL32(255, 230, 60, 255), 3.0f);
					}
				}
			}

			for (std::size_t i = 0; i < g.handles.size(); i++)
			{
				const auto& h = g.handles[i];
				if (!h.on_screen || h.px.x < -8.0f || h.px.y < -8.0f || h.px.x > static_cast<float>(width) + 8.0f || h.px.y > static_cast<float>(height) + 8.0f)
				{
					continue;
				}

				const bool hot = static_cast<int>(i) == over;
				const ImU32 color = h.selected ? IM_COL32(255, 230, 60, 255)
					: (h.kind == handle_kind::edge ? IM_COL32(90, 170, 255, 255) : (h.kind == handle_kind::patch_vertex ? IM_COL32(255, 130, 220, 255) : IM_COL32(90, 255, 120, 255)));
				const float r = (h.selected || hot) ? 5.0f : 3.5f;
				const ImVec2 c = at(h.px);
				dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), hot && !h.selected ? IM_COL32(255, 255, 255, 255) : color);
				dl->AddRect(ImVec2(c.x - r - 1.0f, c.y - r - 1.0f), ImVec2(c.x + r + 1.0f, c.y + r + 1.0f), IM_COL32(0, 0, 0, 220));
			}
		}

		// ---- clipper: the points and the cut
		if (t == tool::clip && !st.clipper.points.empty())
		{
			const auto& points = st.clipper.points;
			const ImU32 color = IM_COL32(80, 200, 255, 255);

			edit::plane plane;
			if (clip_plane(plane))
			{
				bool arrow = false;

				for (const object_id id : doc.selection())
				{
					const auto* geo = doc.geometry(id);
					edit::winding cut;

					if (!geo || !edit::cut_winding(*geo, plane, cut))
					{
						continue;
					}

					std::vector<ImVec2> poly;
					bool whole = true;

					for (const auto& p : cut)
					{
						ImVec2 px;
						whole &= project(p, px);
						poly.push_back(at(px));
					}

					if (whole)
					{
						dl->AddConvexPolyFilled(poly.data(), static_cast<int>(poly.size()), IM_COL32(80, 200, 255, 70));
					}

					edit::vec3 middle;

					for (std::size_t i = 0; i < cut.size(); i++)
					{
						line3d(dl, cut[i], cut[(i + 1) % cut.size()], color, 2.0f);
						middle += cut[i];
					}

					// the side that stays when Enter is pressed: in front of the plane
					if (!arrow)
					{
						arrow = true;
						middle = middle * (1.0 / static_cast<double>(cut.size()));
						const double reach = std::clamp(edit::length(geo->box.size()) * 0.25, 12.0, 96.0);
						const edit::vec3 tip = middle + plane.normal * reach;
						line3d(dl, middle, tip, color, 2.0f);

						ImVec2 a, b;
						if (segment(middle, tip, a, b))
						{
							const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);

							if (len > 6.0f)
							{
								const float ux = dx / len, uy = dy / len;
								dl->AddLine(b, ImVec2(b.x - ux * 9.0f - uy * 5.0f, b.y - uy * 9.0f + ux * 5.0f), color, 2.0f);
								dl->AddLine(b, ImVec2(b.x - ux * 9.0f + uy * 5.0f, b.y - uy * 9.0f - ux * 5.0f), color, 2.0f);
							}

							dl->AddText(ImVec2(b.x + 6.0f, b.y - 8.0f), color, "keep");
						}
					}
				}
			}

			if (points.size() >= 2)
			{
				for (std::size_t i = 0; i + 1 < points.size(); i++)
				{
					line3d(dl, points[i], points[i + 1], color, 1.0f);
				}
			}

			for (std::size_t i = 0; i < points.size(); i++)
			{
				ImVec2 px;
				if (project(points[i], px))
				{
					const ImVec2 c = at(px);
					dl->AddCircleFilled(c, 5.0f, color);
					dl->AddCircle(c, 6.0f, IM_COL32(0, 0, 0, 220));
					const char label[2] = { static_cast<char>('1' + i), 0 };
					dl->AddText(ImVec2(c.x + 7.0f, c.y - 17.0f), color, label);
				}
			}
		}

		// ---- previews of a drag
		if (g.pressed && g.moved)
		{
			const ImVec2 a(origin.x + g.press_px[0], origin.y + g.press_px[1]), b(origin.x + g.now_px[0], origin.y + g.now_px[1]);

			if (g.act == action::rect_select || g.act == action::handle_rect)
			{
				dl->AddRectFilled(a, b, IM_COL32(120, 170, 255, 40));
				dl->AddRect(a, b, IM_COL32(120, 170, 255, 255));
			}
			else if (g.act == action::create)
			{
				edit::vec3 mins, maxs;
				const bool ok = create_box(mins, maxs);
				box3d(dl, mins, maxs, ok ? IM_COL32(255, 58, 32, 255) : IM_COL32(150, 150, 150, 255), 2.0f);

				char text[64];
				std::snprintf(text, sizeof(text), "%g x %g x %g", maxs.x - mins.x, maxs.y - mins.y, maxs.z - mins.z);
				dl->AddText(ImVec2(b.x + 12.0f, b.y + 8.0f), IM_COL32(255, 200, 190, 255), text);
			}
			else if (g.drag_open)
			{
				// how far the drag has moved things
				char text[64];
				std::snprintf(text, sizeof(text), "%g %g %g", g.applied.x, g.applied.y, g.applied.z);
				dl->AddText(ImVec2(b.x + 12.0f, b.y + 8.0f), IM_COL32(255, 230, 150, 255), text);
			}
		}

		// ---- the tool that is on
		if (t != tool::object)
		{
			char text[96];

			if (t == tool::vertex || t == tool::edge)
			{
				std::snprintf(text, sizeof(text), "%s    %d selected", t == tool::edge ? "EDGES" : "VERTICES", static_cast<int>(selected_handle_count()));
			}
			else if (t == tool::clip)
			{
				std::snprintf(text, sizeof(text), "CLIPPER    %d of 3 points", static_cast<int>(st.clipper.points.size()));
			}
			else
			{
				std::snprintf(text, sizeof(text), "CREATE    %s", primitive_name(g.prim));
			}

			const ImVec2 size = ImGui::CalcTextSize(text);
			const ImVec2 pos(origin.x + 8.0f, origin.y + 8.0f);
			dl->AddRectFilled(ImVec2(pos.x - 4.0f, pos.y - 2.0f), ImVec2(pos.x + size.x + 4.0f, pos.y + size.y + 2.0f), IM_COL32(0, 0, 0, 160), 3.0f);
			dl->AddText(pos, IM_COL32(255, 220, 120, 255), text);
		}
	}

	// ---- console ---------------------------------------------------------------------------------------------

	// ed3d <tool object|vertex|edge|clip|create | primitive <name> [sides] | press px py [shift] [ctrl] [alt] | drag px py |
	//       release | click px py [..] | wpress x y z [..] | wdrag x y z | wclick x y z [..] | drop <material> px py [brush] |
	//       wdrop <material> x y z [brush] | mouse px py | wmouse x y z | handles | select <n> [add] | pixel x y z | info>
	static void command_ed3d(const std::vector<std::string>& args)
	{
		auto& s = get_scene();
		const std::string what = args.size() > 1 ? args[1] : "";
		const auto number = [&](const std::size_t i) { return i < args.size() ? static_cast<float>(std::atof(args[i].c_str())) : 0.0f; };
		const auto flag = [&](const char* name, const std::size_t from) { return std::find(args.begin() + static_cast<std::ptrdiff_t>(std::min(from, args.size())), args.end(), name) != args.end(); };

		// a point of the world as a pixel of the view
		const auto world_pixel = [&](const std::size_t i, ImVec2& px)
		{
			if (args.size() < i + 3 || !project({ number(i), number(i + 1), number(i + 2) }, px))
			{
				game::printf_to_console("^1ed3d: that point is not in front of the camera ( or the 3D view has not been drawn )\n");
				return false;
			}

			return true;
		};

		if (what == "tool" && args.size() > 2)
		{
			const std::string name = args[2];
			set_tool(name == "vertex" ? tool::vertex : (name == "edge" ? tool::edge : (name == "clip" ? tool::clip : (name == "create" ? tool::create : tool::object))));
		}
		else if (what == "primitive" && args.size() > 2)
		{
			const std::string name = args[2];
			g.prim = name == "cylinder" ? primitive::cylinder : (name == "cone" ? primitive::cone : (name == "wedge" ? primitive::wedge : (name == "terrain" ? primitive::terrain : primitive::box)));

			if (args.size() > 3)
			{
				g.sides = std::clamp(std::atoi(args[3].c_str()), 3, 64);
			}
		}
		else if (what == "press" && args.size() > 3)
		{
			const bool took = press(number(2), number(3), flag("shift", 4), flag("ctrl", 4), flag("alt", 4));
			g.scripted = took;
			game::printf_to_console("ed3d press %.0f %.0f: %s\n", number(2), number(3), took ? action_name(g.act) : "not taken ( the view's own click / camera )");
		}
		else if (what == "drag" && args.size() > 3)
		{
			drag(number(2), number(3), true);
		}
		else if (what == "release")
		{
			release();
		}
		else if (what == "click" && args.size() > 3)
		{
			const bool shift = flag("shift", 4), ctrl = flag("ctrl", 4);

			if (press(number(2), number(3), shift, ctrl, flag("alt", 4)))
			{
				release();
			}
			else
			{
				ui::viewport_click(number(2), number(3), !(shift && ctrl) && (shift || ctrl), shift && ctrl);
			}
		}
		else if (what == "wpress" || what == "wdrag" || what == "wclick")
		{
			ImVec2 px;
			if (!world_pixel(2, px))
			{
				return;
			}

			if (what == "wdrag")
			{
				drag(px.x, px.y, true);
			}
			else
			{
				const bool shift = flag("shift", 5), ctrl = flag("ctrl", 5);
				const bool took = press(px.x, px.y, shift, ctrl, flag("alt", 5));
				g.scripted = took;
				game::printf_to_console("ed3d %s at pixel %.1f %.1f: %s\n", what.c_str(), px.x, px.y, took ? action_name(g.act) : "not taken ( the view's own click / camera )");

				if (what == "wclick")
				{
					if (took)
					{
						release();
					}
					else
					{
						ui::viewport_click(px.x, px.y, !(shift && ctrl) && (shift || ctrl), shift && ctrl);
					}
				}
			}
		}
		else if (what == "drop" && args.size() > 4)
		{
			drop_material(args[2], number(3), number(4), flag("brush", 5));
		}
		else if (what == "wdrop" && args.size() > 5)
		{
			ImVec2 px;
			if (world_pixel(3, px))
			{
				drop_material(args[2], px.x, px.y, flag("brush", 6));
			}
		}
		else if (what == "mouse" && args.size() > 3)
		{
			// the real ( injected ) mouse to a pixel of the view
			char line[96];
			std::snprintf(line, sizeof(line), "ui_mouse %.1f %.1f", g.origin.x + number(2), g.origin.y + number(3));
			command::execute(line);
		}
		else if (what == "wmouse")
		{
			ImVec2 px;
			if (world_pixel(2, px))
			{
				char line[96];
				std::snprintf(line, sizeof(line), "ui_mouse %.1f %.1f", g.origin.x + px.x, g.origin.y + px.y);
				command::execute(line);
				game::printf_to_console("ed3d wmouse: view pixel %.1f %.1f, window %.1f %.1f\n", px.x, px.y, g.origin.x + px.x, g.origin.y + px.y);
			}
		}
		else if (what == "pixel")
		{
			ImVec2 px;
			if (world_pixel(2, px))
			{
				game::printf_to_console("ed3d pixel: view %.1f %.1f, window %.1f %.1f\n", px.x, px.y, g.origin.x + px.x, g.origin.y + px.y);
			}
		}
		else if (what == "select" && args.size() > 2)
		{
			// a handle by its number in `ed3d handles`
			build_handles();
			const int index = std::atoi(args[2].c_str());

			if (index >= 0 && static_cast<std::size_t>(index) < g.handles.size())
			{
				if (!flag("add", 3))
				{
					clear_handle_selection();
				}

				set_handle_selected(g.handles[static_cast<std::size_t>(index)], true);
			}
		}
		else if (what == "handles")
		{
			build_handles();
			game::printf_to_console("ed3d: %d handles ( %s tool ), %d selected\n", static_cast<int>(g.handles.size()), tool_name(current()), static_cast<int>(selected_handle_count()));

			for (std::size_t i = 0; i < g.handles.size() && i < 64; i++)
			{
				const auto& h = g.handles[i];
				game::printf_to_console("  %2d %s #%u  %g %g %g  pixel %.1f %.1f%s%s\n", static_cast<int>(i),
					h.kind == handle_kind::vertex ? "vertex" : (h.kind == handle_kind::edge ? "edge" : "patch vertex"), h.object, h.pos.x, h.pos.y, h.pos.z,
					h.px.x, h.px.y, h.on_screen ? "" : "  ( not in view )", h.selected ? "  selected" : "");
			}

			for (const auto& h : bounds_handles())
			{
				game::printf_to_console("  box handle %c%c  %g %g %g  pixel %.1f %.1f\n", h.side > 0 ? '+' : '-', "xyz"[h.axis], h.pos.x, h.pos.y, h.pos.z, h.px.x, h.px.y);
			}
		}
		else if (what == "faces")
		{
			// the faces of the selected brushes: what the texture keys and a dropped material changed
			auto& doc = s.doc;

			for (const object_id id : doc.selection())
			{
				const auto* b = doc.find_brush(id);
				const auto* geo = doc.geometry(id);

				for (std::size_t f = 0; b && geo && f < b->faces.size() && f < geo->planes.size(); f++)
				{
					const auto& tex = b->faces[f].texture;
					const auto& n = geo->planes[f].normal;
					game::printf_to_console("  #%u face %d  normal %g %g %g  %s  size %g %g  shift %g %g  rotation %g%s\n", id, static_cast<int>(f), n.x, n.y, n.z,
						tex.material.c_str(), tex.size_x, tex.size_y, tex.shift_x, tex.shift_y, tex.rotation, doc.is_face_selected({ id, static_cast<int>(f) }) ? "  selected" : "");
				}

				if (const auto* p = doc.find_patch(id))
				{
					game::printf_to_console("  #%u patch %d x %d  %s\n", id, p->width, p->height, p->material.c_str());
				}
			}
		}
		else if (what != "info")
		{
			game::printf_to_console("usage: ed3d <tool object|vertex|edge|clip|create | primitive box|cylinder|cone|wedge|terrain [sides] | press px py [shift] [ctrl] [alt] | drag px py | release\n"
				"   | click px py [..] | wpress x y z [..] | wdrag x y z | wclick x y z [..] | drop <material> px py [brush] | wdrop <material> x y z [brush]\n"
				"   | mouse px py | wmouse x y z | handles | faces | select <n> [add] | pixel x y z | info>     px py: pixels of the 3D view; x y z: a point of the world\n");
			return;
		}

		const auto& st = ui::get();
		game::printf_to_console("ed3d: tool %s, action %s%s, view %d x %d at %.0f %.0f, %d handles selected, %d clip points ( axis %d ), primitive %s\n", tool_name(current()),
			action_name(g.act), g.pressed ? " ( pressed )" : "", g.width, g.height, g.origin.x, g.origin.y, static_cast<int>(selected_handle_count()),
			static_cast<int>(st.clipper.points.size()), st.clipper.axis, primitive_name(g.prim));
		(void)s;
	}

	void register_commands()
	{
		command::register_command("ed3d", [](const std::vector<std::string>& args) { command_ed3d(args); });
	}
}
