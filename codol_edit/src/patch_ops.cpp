// patch_ops.hpp: brush primitives and the operations of radiant's Patch menu.
//
// Every operation is computed on copies of the patches first ( plain functions over a `patch` value, most of them
// written for columns; the row variant runs on the transposed grid ) and stored in one transaction afterwards, so
// an operation that fails leaves nothing behind.

#include "doc_impl.hpp"
#include "codol_edit/patch_ops.hpp"

namespace codol::edit
{
	// the one door into a document's private state for the operations of this file
	struct ops_access
	{
		static document::impl& of(document& doc) { return *doc.d; }
		static const document::impl& of(const document& doc) { return *doc.d; }
	};

	namespace
	{
		using impl = document::impl;
		using line_t = std::vector<patch_vertex>;		// one column ( or row ) of a patch

		constexpr double SAME_POINT = 1e-4;

		// 64 for a 63.99999999999999 that came out of a sine: brush corners on the grid stay on it
		double tidy(const double v)
		{
			const double r = std::round(v * 1e6) / 1e6;
			return std::fabs(r - v) < 1e-9 ? r : v;
		}

		std::string new_object_layer(const impl& d)
		{
			for (std::size_t i = 0; i < d.layers.size(); i++)
			{
				if (d.layers[i].active)
				{
					return i == 0 ? std::string() : d.layers[i].name;
				}
			}

			return {};
		}

		bool grid_ok(const patch& p)
		{
			return p.width >= 2 && p.height >= 2 && p.vertices.size() == static_cast<std::size_t>(p.width) * static_cast<std::size_t>(p.height);
		}

		bool bezier(const patch& p)
		{
			return p.type == patch_type::curve && p.width >= 3 && p.height >= 3 && (p.width & 1) && (p.height & 1);
		}

		bool turned(const patch_vertex& v)
		{
			return v.has_flag && v.flag != 0;
		}

		// a + ( b - a ) * t for everything a vertex carries; t may lie outside 0 .. 1. No flag.
		patch_vertex mix(const patch_vertex& a, const patch_vertex& b, const double t)
		{
			patch_vertex r;
			r.xyz = a.xyz + (b.xyz - a.xyz) * t;
			r.tex.resize(std::max(a.tex.size(), b.tex.size()));

			for (std::size_t i = 0; i < r.tex.size(); i++)
			{
				const double av = i < a.tex.size() ? a.tex[i] : 0.0;
				const double bv = i < b.tex.size() ? b.tex[i] : 0.0;
				r.tex[i] = av + (bv - av) * t;
			}

			r.has_color = a.has_color || b.has_color;
			for (int i = 0; i < 4; i++)
			{
				const double c = a.color[i] + (static_cast<double>(b.color[i]) - a.color[i]) * t;
				r.color[i] = static_cast<std::uint8_t>(std::clamp(std::floor(c + 0.5), 0.0, 255.0));
			}

			return r;
		}

		patch_vertex bezier3(const patch_vertex& a, const patch_vertex& b, const patch_vertex& c, const double t)
		{
			return mix(mix(a, b, t), mix(b, c, t), t);
		}

		line_t mix(const line_t& a, const line_t& b, const double t)
		{
			line_t r(a.size());
			for (std::size_t i = 0; i < a.size(); i++)
			{
				r[i] = mix(a[i], b[i], t);
			}

			return r;
		}

		void copy_flags(line_t& to, const line_t& from)
		{
			for (std::size_t i = 0; i < to.size() && i < from.size(); i++)
			{
				to[i].has_flag = from[i].has_flag;
				to[i].flag = from[i].flag;
			}
		}

		std::vector<line_t> columns_of(const patch& p)
		{
			std::vector<line_t> cols(static_cast<std::size_t>(p.width));
			for (int c = 0; c < p.width; c++)
			{
				cols[static_cast<std::size_t>(c)].assign(p.vertices.begin() + static_cast<std::ptrdiff_t>(c) * p.height,
					p.vertices.begin() + static_cast<std::ptrdiff_t>(c + 1) * p.height);
			}

			return cols;
		}

		void set_columns(patch& p, const std::vector<line_t>& cols)
		{
			p.width = static_cast<int>(cols.size());
			p.height = cols.empty() ? 0 : static_cast<int>(cols[0].size());
			p.vertices.clear();

			for (const line_t& col : cols)
			{
				p.vertices.insert(p.vertices.end(), col.begin(), col.end());
			}
		}

		line_t row_of(const patch& p, const int row)
		{
			line_t out;
			for (int c = 0; c < p.width; c++)
			{
				out.push_back(p.at(c, row));
			}

			return out;
		}

		patch transposed(const patch& p)
		{
			patch q = p;
			q.width = p.height;
			q.height = p.width;

			for (int c = 0; c < q.width; c++)
			{
				for (int r = 0; r < q.height; r++)
				{
					q.at(c, r) = p.at(r, c);
				}
			}

			return q;
		}

		// The surface faces the other way and keeps its shape: the columns in reverse order. The diagonal of a terrain
		// quad is named from its first vertex, so the flags move to the new first column of each quad and change.
		void reverse_columns(patch& p)
		{
			const patch old = p;
			const bool mesh = !bezier(p);

			for (int c = 0; c < p.width; c++)
			{
				for (int r = 0; r < p.height; r++)
				{
					patch_vertex& v = p.at(c, r);
					v = old.at(p.width - 1 - c, r);
					v.has_flag = false;
					v.flag = 0;

					if (mesh && c + 1 < p.width && r + 1 < p.height && !turned(old.at(p.width - 2 - c, r)))
					{
						v.has_flag = true;
						v.flag = 1;
					}
				}
			}
		}

		bool closed_columns(const patch& p)
		{
			for (int r = 0; r < p.height; r++)
			{
				if (length(p.at(0, r).xyz - p.at(p.width - 1, r).xyz) > SAME_POINT)
				{
					return false;
				}
			}

			return true;
		}

		// sum of the cell normals ( cross( along columns, along rows ) ): direction the patch faces, length ~ area
		vec3 area_normal(const patch& p)
		{
			vec3 n;
			for (int c = 0; c + 1 < p.width; c++)
			{
				for (int r = 0; r + 1 < p.height; r++)
				{
					n += cross(p.at(c + 1, r + 1).xyz - p.at(c, r).xyz, p.at(c, r + 1).xyz - p.at(c + 1, r).xyz) * 0.5;
				}
			}

			return n;
		}

		int dominant_axis(const vec3& n)
		{
			const double x = std::fabs(n.x), y = std::fabs(n.y), z = std::fabs(n.z);
			return z >= x && z >= y ? 2 : (x >= y ? 0 : 1);
		}

		// normal at every control point, from its neighbours; wraps around a closed tube
		std::vector<vec3> control_normals(const patch& p)
		{
			const bool wrap_c = closed_columns(p);
			const patch t = transposed(p);
			const bool wrap_r = closed_columns(t);

			std::vector<vec3> normals(p.vertices.size());
			std::vector<char> good(p.vertices.size(), 0);

			const auto step = [](const int i, const int d, const int n, const bool wrap)
			{
				int k = i + d;
				if (k < 0) { k = wrap ? n - 2 : 0; }
				if (k >= n) { k = wrap ? 1 : n - 1; }
				return k;
			};

			for (int c = 0; c < p.width; c++)
			{
				for (int r = 0; r < p.height; r++)
				{
					const vec3 dc = p.at(step(c, 1, p.width, wrap_c), r).xyz - p.at(step(c, -1, p.width, wrap_c), r).xyz;
					const vec3 dr = p.at(c, step(r, 1, p.height, wrap_r)).xyz - p.at(c, step(r, -1, p.height, wrap_r)).xyz;
					const vec3 n = cross(dc, dr);
					const std::size_t i = static_cast<std::size_t>(c) * p.height + r;

					if (length(n) > 1e-9)
					{
						normals[i] = normalize(n);
						good[i] = 1;
					}
				}
			}

			// a collapsed spot ( the tip of a cone ): the mean of what surrounds it
			for (int pass = 0; pass < 4; pass++)
			{
				bool missing = false;

				for (int c = 0; c < p.width; c++)
				{
					for (int r = 0; r < p.height; r++)
					{
						const std::size_t i = static_cast<std::size_t>(c) * p.height + r;
						if (good[i])
						{
							continue;
						}

						vec3 sum;
						for (std::size_t k = 0; k < p.vertices.size(); k++)
						{
							if (good[k] == 1 && length(p.vertices[k].xyz - p.vertices[i].xyz) < SAME_POINT)
							{
								sum += normals[k];
							}
						}

						for (const auto& [dc, dr] : { std::pair{ 1, 0 }, std::pair{ -1, 0 }, std::pair{ 0, 1 }, std::pair{ 0, -1 } })
						{
							const int nc = c + dc, nr = r + dr;
							if (nc >= 0 && nc < p.width && nr >= 0 && nr < p.height && good[static_cast<std::size_t>(nc) * p.height + nr] == 1)
							{
								sum += normals[static_cast<std::size_t>(nc) * p.height + nr];
							}
						}

						if (length(sum) > 1e-9)
						{
							normals[i] = normalize(sum);
							good[i] = 2;		// counts from the next pass on
						}
						else
						{
							missing = true;
						}
					}
				}

				for (char& g : good)
				{
					if (g == 2) { g = 1; }
				}

				if (!missing)
				{
					break;
				}
			}

			return normals;
		}

		patch tessellated(const patch& p);

		void ensure_tex(patch_vertex& v)
		{
			if (v.tex.size() < 4)
			{
				v.tex.resize(4, 0.0);
			}
		}

		void natural_texture(patch& p, const double scale)
		{
			std::vector<double> s(static_cast<std::size_t>(p.width), 0.0), t(static_cast<std::size_t>(p.height), 0.0);

			for (int c = 1; c < p.width; c++)
			{
				double sum = 0.0;
				for (int r = 0; r < p.height; r++)
				{
					sum += length(p.at(c, r).xyz - p.at(c - 1, r).xyz);
				}

				s[static_cast<std::size_t>(c)] = s[static_cast<std::size_t>(c) - 1] + sum / p.height;
			}

			for (int r = 1; r < p.height; r++)
			{
				double sum = 0.0;
				for (int c = 0; c < p.width; c++)
				{
					sum += length(p.at(c, r).xyz - p.at(c, r - 1).xyz);
				}

				t[static_cast<std::size_t>(r)] = t[static_cast<std::size_t>(r) - 1] + sum / p.width;
			}

			const double total_t = t.back();

			for (int c = 0; c < p.width; c++)
			{
				for (int r = 0; r < p.height; r++)
				{
					patch_vertex& v = p.at(c, r);
					ensure_tex(v);
					v.tex[0] = s[static_cast<std::size_t>(c)] / scale;
					v.tex[1] = (total_t - t[static_cast<std::size_t>(r)]) / scale;		// the first row is the bottom of the image
					v.tex[2] = s[static_cast<std::size_t>(c)] / 16.0;
					v.tex[3] = t[static_cast<std::size_t>(r)] / 16.0;
				}
			}
		}

		void projection_axes(const int axis, int& a, int& b)
		{
			a = axis == 0 ? 1 : 0;
			b = axis == 2 ? 1 : 2;
		}

		void cap_texture(patch& p, int axis)
		{
			if (axis < 0 || axis > 2)
			{
				axis = dominant_axis(area_normal(bezier(p) ? tessellated(p) : p));
			}

			int a, b;
			projection_axes(axis, a, b);

			for (patch_vertex& v : p.vertices)
			{
				ensure_tex(v);
				v.tex[0] = v.xyz[a] * 4.0;
				v.tex[1] = -v.xyz[b] * 4.0;
				v.tex[2] = v.xyz[a] / 16.0;
				v.tex[3] = v.xyz[b] / 16.0;
			}
		}

		// axis the texture is projected along now, -1: none
		int projected_axis(const patch& p)
		{
			for (int axis = 0; axis < 3; axis++)
			{
				int a, b;
				projection_axes(axis, a, b);

				const bool all = std::all_of(p.vertices.begin(), p.vertices.end(), [&](const patch_vertex& v)
				{
					return v.tex.size() >= 2 && std::fabs(v.tex[0] - v.xyz[a] * 4.0) < 0.01 && std::fabs(v.tex[1] + v.xyz[b] * 4.0) < 0.01;
				});

				if (all)
				{
					return axis;
				}
			}

			return -1;
		}

		// ---- operations on one patch, column wise --------------------------------------------------

		bool insert_columns(patch& p, const bool at_end, const int index, std::string& why)
		{
			if (p.width + 2 > PATCH_MAX_LINES)
			{
				why = "the patch has as many lines as it can take";
				return false;
			}

			std::vector<line_t> cols = columns_of(p);

			if (bezier(p))
			{
				const int segments = (p.width - 1) / 2;
				const int seg = index >= 0 ? std::min(index / 2, segments - 1) : (at_end ? segments - 1 : 0);
				const auto first = static_cast<std::size_t>(seg) * 2;

				// de Casteljau at 0.5: the two halves are the same curve
				line_t a = mix(cols[first], cols[first + 1], 0.5);
				line_t b = mix(cols[first + 1], cols[first + 2], 0.5);
				line_t m = mix(a, b, 0.5);

				cols[first + 1] = std::move(a);
				cols.insert(cols.begin() + static_cast<std::ptrdiff_t>(first) + 2, { std::move(m), std::move(b) });
			}
			else
			{
				const int cell = index >= 0 ? std::clamp(index, 0, p.width - 2) : (at_end ? p.width - 2 : 0);
				const auto first = static_cast<std::size_t>(cell);

				line_t a = mix(cols[first], cols[first + 1], 1.0 / 3.0);
				line_t b = mix(cols[first], cols[first + 1], 2.0 / 3.0);
				copy_flags(a, cols[first]);
				copy_flags(b, cols[first]);
				cols.insert(cols.begin() + static_cast<std::ptrdiff_t>(first) + 1, { std::move(a), std::move(b) });
			}

			set_columns(p, cols);
			return true;
		}

		bool delete_columns(patch& p, const bool at_end, const int count, std::string& why)
		{
			if (p.width - count < (bezier(p) ? 3 : 2))
			{
				why = "the patch would have too few lines left";
				return false;
			}

			std::vector<line_t> cols = columns_of(p);
			if (at_end)
			{
				cols.resize(cols.size() - static_cast<std::size_t>(count));
			}
			else
			{
				cols.erase(cols.begin(), cols.begin() + count);
			}

			set_columns(p, cols);
			return true;
		}

		bool insert_terrain_column(patch& p, const int after, std::string& why)
		{
			if (bezier(p))
			{
				why = "not a terrain mesh";
				return false;
			}

			if (p.width + 1 > PATCH_MAX_LINES)
			{
				why = "the patch has as many lines as it can take";
				return false;
			}

			std::vector<line_t> cols = columns_of(p);
			const auto cell = static_cast<std::size_t>(after >= 0 ? std::clamp(after, 0, p.width - 2) : p.width - 2);

			line_t made = mix(cols[cell], cols[cell + 1], 0.5);
			copy_flags(made, cols[cell]);
			cols.insert(cols.begin() + static_cast<std::ptrdiff_t>(cell) + 1, std::move(made));
			set_columns(p, cols);
			return true;
		}

		bool extrude_terrain_column(patch& p, const bool at_end, std::string& why)
		{
			if (bezier(p))
			{
				why = "not a terrain mesh";
				return false;
			}

			if (p.width + 1 > PATCH_MAX_LINES)
			{
				why = "the patch has as many lines as it can take";
				return false;
			}

			std::vector<line_t> cols = columns_of(p);

			if (at_end)
			{
				cols.push_back(mix(cols[cols.size() - 2], cols[cols.size() - 1], 2.0));
			}
			else
			{
				cols.insert(cols.begin(), mix(cols[1], cols[0], 2.0));
			}

			set_columns(p, cols);
			return true;
		}

		bool delete_terrain_column(patch& p, const int index, std::string& why)
		{
			if (bezier(p))
			{
				why = "not a terrain mesh";
				return false;
			}

			if (p.width <= 2)
			{
				why = "the patch would have too few lines left";
				return false;
			}

			std::vector<line_t> cols = columns_of(p);
			const int at = index >= 0 ? std::min(index, p.width - 1) : p.width - 1;
			cols.erase(cols.begin() + at);
			set_columns(p, cols);
			return true;
		}

		// the columns evenly on the straight line from the first to the last, per row
		bool disperse_columns_straight(patch& p)
		{
			bool changed = false;

			for (int r = 0; r < p.height; r++)
			{
				const vec3 a = p.at(0, r).xyz, b = p.at(p.width - 1, r).xyz;
				if (length(b - a) < SAME_POINT)
				{
					continue;	// a closed row: there is no line to spread along
				}

				for (int c = 1; c + 1 < p.width; c++)
				{
					const vec3 to = a + (b - a) * (static_cast<double>(c) / (p.width - 1));
					changed = changed || length(to - p.at(c, r).xyz) > 1e-9;
					p.at(c, r).xyz = to;
				}
			}

			return changed;
		}

		// the columns at even distances along the line the control points of each row form
		bool disperse_columns_along(patch& p)
		{
			bool changed = false;

			for (int r = 0; r < p.height; r++)
			{
				const line_t row = row_of(p, r);
				std::vector<double> at(row.size(), 0.0);

				for (std::size_t c = 1; c < row.size(); c++)
				{
					at[c] = at[c - 1] + length(row[c].xyz - row[c - 1].xyz);
				}

				if (at.back() < SAME_POINT)
				{
					continue;
				}

				for (int c = 1; c + 1 < p.width; c++)
				{
					const double target = at.back() * c / (p.width - 1);
					std::size_t k = 0;
					while (k + 2 < row.size() && at[k + 1] < target)
					{
						k++;
					}

					const double span = at[k + 1] - at[k];
					patch_vertex made = mix(row[k], row[k + 1], span > 1e-12 ? (target - at[k]) / span : 0.0);
					made.has_flag = p.at(c, r).has_flag;
					made.flag = p.at(c, r).flag;

					changed = changed || length(made.xyz - p.at(c, r).xyz) > 1e-9;
					p.at(c, r) = std::move(made);
				}
			}

			return changed;
		}

		bool split_columns(const patch& p, int index, patch& first, patch& second, std::string& why)
		{
			if (bezier(p))
			{
				if (index < 0)
				{
					index = ((p.width - 1) / 4) * 2;
				}

				if ((index & 1) || index < 2 || index > p.width - 3)
				{
					why = "a curve splits at an even control line that is not its edge ( it needs 5 lines or more )";
					return false;
				}
			}
			else
			{
				if (index < 0)
				{
					index = p.width / 2;
				}

				if (index < 1 || index > p.width - 2)
				{
					why = "the line to split at has to lie inside the patch ( it needs 3 lines or more )";
					return false;
				}
			}

			const std::vector<line_t> cols = columns_of(p);
			first = p;
			second = p;
			set_columns(first, { cols.begin(), cols.begin() + index + 1 });
			set_columns(second, { cols.begin() + index, cols.end() });

			// the last column of a terrain mesh starts no quad
			for (int r = 0; r < first.height; r++)
			{
				first.at(first.width - 1, r).has_flag = false;
				first.at(first.width - 1, r).flag = 0;
			}

			return true;
		}

		// vertices of the tessellation of a curve
		patch tessellated(const patch& p)
		{
			const int n = patch_subdivisions(p);
			const int segs_c = (p.width - 1) / 2, segs_r = (p.height - 1) / 2;

			patch q = p;
			q.type = patch_type::mesh;
			q.width = segs_c * n + 1;
			q.height = segs_r * n + 1;
			q.vertices.assign(static_cast<std::size_t>(q.width) * q.height, {});

			std::vector<patch_vertex> along(static_cast<std::size_t>(p.width) * q.height);

			for (int c = 0; c < p.width; c++)
			{
				for (int r = 0; r < q.height; r++)
				{
					const int seg = std::min(r / n, segs_r - 1);
					const double t = static_cast<double>(r - seg * n) / n;
					along[static_cast<std::size_t>(c) * q.height + r] = bezier3(p.at(c, seg * 2), p.at(c, seg * 2 + 1), p.at(c, seg * 2 + 2), t);
				}
			}

			for (int c = 0; c < q.width; c++)
			{
				const int seg = std::min(c / n, segs_c - 1);
				const double t = static_cast<double>(c - seg * n) / n;

				for (int r = 0; r < q.height; r++)
				{
					q.at(c, r) = bezier3(along[static_cast<std::size_t>(seg * 2) * q.height + r], along[static_cast<std::size_t>(seg * 2 + 1) * q.height + r],
						along[static_cast<std::size_t>(seg * 2 + 2) * q.height + r], t);
				}
			}

			return q;
		}

		// ---- plumbing ------------------------------------------------------------------------------

		void store(impl& d, const object_id id, const patch& q)
		{
			patch& e = d.edit_patch(id);
			const std::uint64_t revision = e.revision;
			e = q;
			e.revision = revision;
			e.source_body = {};
		}

		object_id add_like(impl& d, const patch& like, patch made)
		{
			made.layer = like.layer;
			made.contents = like.contents;
			made.tool_flags = like.tool_flags;
			made.extra.clear();
			return d.add_patch(std::move(made), like.owner);
		}

		// fn( patch&, why ) -> changed; for every selected patch. One undo step.
		template <typename Fn>
		bool for_selected(document& doc, const std::string_view name, const std::string_view nothing, Fn&& fn)
		{
			impl& d = ops_access::of(doc);
			std::vector<std::pair<object_id, patch>> results;
			std::string why;
			bool any = false;

			for (const object_id id : d.sel.objects)
			{
				const patch* p = d.get_patch(id);
				if (!p || !grid_ok(*p))
				{
					continue;
				}

				any = true;
				patch q = *p;
				if (fn(q, why) && grid_ok(q))
				{
					results.emplace_back(id, std::move(q));
				}
			}

			if (results.empty())
			{
				return d.fail(std::string(name) + ": " + (!any ? "no patch is selected" : (why.empty() ? std::string(nothing) : why)));
			}

			transaction_scope tx(d, name);
			for (const auto& [id, q] : results)
			{
				store(d, id, q);
			}

			return true;
		}

		// runs a column operation on the columns or, through the transposed grid, on the rows
		template <typename Fn>
		bool on_lines(patch& p, const patch_line line, Fn&& fn)
		{
			if (line == patch_line::column)
			{
				return fn(p);
			}

			patch t = transposed(p);
			if (!fn(t))
			{
				return false;
			}

			p = transposed(t);
			return true;
		}

		patch new_patch(const impl& d, const patch_type type, const std::string_view material)
		{
			patch p;
			p.type = type;
			p.material = std::string(material.empty() ? std::string_view(d.default_material) : material);
			p.lightmap = "lightmap_gray";
			p.layer = new_object_layer(d);
			return p;
		}

		object_id add_world_patch(impl& d, patch p, const std::string_view name)
		{
			transaction_scope tx(d, name);
			const object_id id = d.add_patch(std::move(p), d.world());
			d.sel.faces.clear();
			d.set_selection({ id });
			return id;
		}

		bool box_in_world(impl& d, const vec3& mins, const vec3& maxs, const std::string_view what)
		{
			for (int i = 0; i < 3; i++)
			{
				if (std::fabs(mins[i]) > WORLD_EXTENT || std::fabs(maxs[i]) > WORLD_EXTENT)
				{
					return d.fail(std::string(what) + ": outside the world");
				}
			}

			return true;
		}
	}

	int patch_subdivisions(const patch& p)
	{
		if (p.type != patch_type::curve)
		{
			return 1;
		}

		return p.has_header_scale && p.has_header_int ? std::clamp(p.header_int / 2, 1, 16) : 4;
	}

	// ---- brush primitives ---------------------------------------------------------------------------

	object_id create_brush_shape(document& doc, const brush_shape shape, const vec3& mins, const vec3& maxs, const int sides, const int axis,
		const std::string_view material)
	{
		impl& d = ops_access::of(doc);

		if (axis < 0 || axis > 2)
		{
			d.fail("Create brush: the axis is 0, 1 or 2");
			return no_object;
		}

		const int a = (axis + 1) % 3, b = (axis + 2) % 3;
		const vec3 center = (mins + maxs) * 0.5;
		const double ra = (maxs[a] - mins[a]) * 0.5, rb = (maxs[b] - mins[b]) * 0.5, rc = (maxs[axis] - mins[axis]) * 0.5;

		const auto degenerate = [&](const char* what)
		{
			if (!(ra > ON_EPSILON && rb > ON_EPSILON && rc > ON_EPSILON))
			{
				d.fail(std::string(what) + ": the box has no volume");
				return true;
			}

			return false;
		};

		switch (shape)
		{
		case brush_shape::box:
			return doc.create_box(mins, maxs, material);

		case brush_shape::wedge:
			return doc.create_wedge(mins, maxs, axis == 1 ? 1 : 0, material);

		case brush_shape::prism:
		case brush_shape::cone:
		{
			const char* what = shape == brush_shape::cone ? "Create cone" : "Create cylinder";
			if (sides < 3 || sides > 64)
			{
				d.fail(std::string(what) + ": 3 to 64 sides");
				return no_object;
			}

			if (degenerate(what))
			{
				return no_object;
			}

			// the faces touch the box ( radiant's Brush_MakeSided ): the corners lie outside the inscribed ellipse
			const double out = 1.0 / std::cos(PI / sides);
			std::vector<vec3> points;

			for (int i = 0; i < sides; i++)
			{
				const double angle = (i + 0.5) * 2.0 * PI / sides;
				vec3 p = center;
				p[a] = tidy(p[a] + std::cos(angle) * ra * out);
				p[b] = tidy(p[b] + std::sin(angle) * rb * out);
				p[axis] = mins[axis];
				points.push_back(p);

				if (shape == brush_shape::prism)
				{
					p[axis] = maxs[axis];
					points.push_back(p);
				}
			}

			if (shape == brush_shape::cone)
			{
				vec3 apex = center;
				apex[axis] = maxs[axis];
				points.push_back(apex);
			}

			doc.begin_transaction(what);
			const object_id id = doc.create_brush_from_points(points, material);
			doc.end_transaction();
			return id;
		}

		case brush_shape::sphere:
		{
			if (sides < 4 || sides > 16)
			{
				d.fail("Create sphere: 4 to 16 sides");
				return no_object;
			}

			if (degenerate("Create sphere"))
			{
				return no_object;
			}

			const int bands = std::max(2, (sides + 1) / 2);
			std::vector<vec3> points;

			vec3 pole = center;
			pole[axis] = mins[axis];
			points.push_back(pole);

			for (int j = 1; j < bands; j++)
			{
				const double lat = -PI * 0.5 + PI * j / bands;

				for (int i = 0; i < sides; i++)
				{
					const double lon = i * 2.0 * PI / sides;
					vec3 p = center;
					p[a] = tidy(p[a] + std::cos(lat) * std::cos(lon) * ra);
					p[b] = tidy(p[b] + std::cos(lat) * std::sin(lon) * rb);
					p[axis] = tidy(p[axis] + std::sin(lat) * rc);
					points.push_back(p);
				}
			}

			pole[axis] = maxs[axis];
			points.push_back(pole);

			doc.begin_transaction("Create sphere");
			const object_id id = doc.create_brush_from_points(points, material);
			doc.end_transaction();
			return id;
		}
		}

		return no_object;
	}

	object_id brush_shape_from_selection(document& doc, const brush_shape shape, const int sides, const int axis)
	{
		impl& d = ops_access::of(doc);

		std::vector<object_id> brushes;
		bounds box;

		for (const object_id id : d.sel.objects)
		{
			const brush* b = d.get_brush(id);
			if (b && !b->opaque && !b->faces.empty())
			{
				brushes.push_back(id);
				box.add(d.object_bounds(id));
			}
		}

		if (brushes.empty() || !box.valid())
		{
			d.fail("Make brush: select the brush whose box the shape is to fill");
			return no_object;
		}

		const std::string material = d.get_brush(brushes[0])->faces[0].texture.material;
		const std::vector<object_id> selection_before = d.sel.objects;

		doc.begin_transaction("Make brush shape");

		const object_id made = create_brush_shape(doc, shape, box.mins, box.maxs, sides, axis, material);
		if (!made)
		{
			const std::string error = d.last_error;
			doc.cancel_transaction();
			d.last_error = error;
			return no_object;
		}

		const std::vector<face> faces = d.get_brush(made)->faces;
		d.remove_object(made);
		d.edit_brush(brushes[0]).faces = faces;

		for (std::size_t i = 1; i < brushes.size(); i++)
		{
			const object_id owner = d.get_brush(brushes[i])->owner;
			d.remove_object(brushes[i]);

			const entity* e = d.get_entity(owner);
			if (e && !e->is_worldspawn() && e->children.empty())
			{
				d.remove_object(owner);
			}
		}

		std::vector<object_id> selection;
		for (const object_id id : selection_before)
		{
			if (d.kind(id) != object_kind::none && (id == brushes[0] || !d.get_brush(id)))
			{
				selection.push_back(id);
			}
		}

		d.sel.faces.clear();
		d.set_selection(std::move(selection));
		doc.end_transaction();
		return brushes[0];
	}

	// ---- creating patches ---------------------------------------------------------------------------

	object_id create_curve_patch(document& doc, const vec3& mins, const vec3& maxs, const int columns, const int rows, const int axis,
		const std::string_view material)
	{
		impl& d = ops_access::of(doc);

		if (columns < 3 || rows < 3 || columns > PATCH_MAX_LINES || rows > PATCH_MAX_LINES || !(columns & 1) || !(rows & 1) || axis < 0 || axis > 2)
		{
			d.fail("Create curve: 3 to 31 columns and rows, odd numbers");
			return no_object;
		}

		int a, b;
		projection_axes(axis, a, b);

		if (!(maxs[a] - mins[a] > ON_EPSILON) || !(maxs[b] - mins[b] > ON_EPSILON))
		{
			d.fail("Create curve: the rectangle has no area");
			return no_object;
		}

		if (!box_in_world(d, mins, maxs, "Create curve"))
		{
			return no_object;
		}

		patch p = new_patch(d, patch_type::curve, material);
		p.width = columns;
		p.height = rows;

		for (int c = 0; c < columns; c++)
		{
			for (int r = 0; r < rows; r++)
			{
				patch_vertex v;
				v.xyz[axis] = mins[axis];
				v.xyz[a] = mins[a] + (maxs[a] - mins[a]) * c / (columns - 1);
				v.xyz[b] = mins[b] + (maxs[b] - mins[b]) * r / (rows - 1);
				p.vertices.push_back(std::move(v));
			}
		}

		// x - z plane: columns along x and rows along z face -y; the other two planes face along their axis
		natural_texture(p, 0.25);
		return add_world_patch(d, std::move(p), "Create curve");
	}

	object_id create_patch_shape(document& doc, const patch_shape shape, const vec3& mins, const vec3& maxs, const int axis, const std::string_view material)
	{
		impl& d = ops_access::of(doc);

		if (axis < 0 || axis > 2)
		{
			d.fail("Create patch: the axis is 0, 1 or 2");
			return no_object;
		}

		for (int i = 0; i < 3; i++)
		{
			if (!(maxs[i] - mins[i] > ON_EPSILON))
			{
				d.fail("Create patch: the box has no volume");
				return no_object;
			}
		}

		if (!box_in_world(d, mins, maxs, "Create patch"))
		{
			return no_object;
		}

		// the outline in the plane across the axis, counter clockwise seen from the maxs end, -1 .. 1
		struct uv { double u, v; };
		std::vector<uv> outline;
		int rows = 3;

		switch (shape)
		{
		case patch_shape::bevel:
			outline = { { -1, -1 }, { 1, -1 }, { 1, 1 } };
			break;

		case patch_shape::square_bevel:
			outline = { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 } };
			break;

		case patch_shape::endcap:
			outline = { { 1, -1 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, -1 } };
			break;

		case patch_shape::square_endcap:
			outline = { { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
			break;

		case patch_shape::square_cylinder:
			outline = { { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
			break;

		case patch_shape::dense_cylinder:
			rows = 5;
			[[fallthrough]];
		case patch_shape::very_dense_cylinder:
			rows = shape == patch_shape::very_dense_cylinder ? 9 : rows;
			[[fallthrough]];
		case patch_shape::cylinder:
		case patch_shape::cone:
			outline = { { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 } };
			break;
		}

		const int a = (axis + 1) % 3, b = (axis + 2) % 3;
		const vec3 center = (mins + maxs) * 0.5;
		const double ra = (maxs[a] - mins[a]) * 0.5, rb = (maxs[b] - mins[b]) * 0.5;

		patch p = new_patch(d, patch_type::curve, material);
		p.width = static_cast<int>(outline.size());
		p.height = rows;

		for (const uv& o : outline)
		{
			for (int r = 0; r < rows; r++)
			{
				const double up = static_cast<double>(r) / (rows - 1);
				const double size = shape == patch_shape::cone ? 1.0 - up : 1.0;

				patch_vertex v;
				v.xyz = center;
				v.xyz[a] += o.u * ra * size;
				v.xyz[b] += o.v * rb * size;
				v.xyz[axis] = mins[axis] + (maxs[axis] - mins[axis]) * up;
				p.vertices.push_back(std::move(v));
			}
		}

		natural_texture(p, 0.25);

		static const char* const names[] = { "Create bevel", "Create square bevel", "Create end cap", "Create square end cap", "Create cylinder",
			"Create dense cylinder", "Create very dense cylinder", "Create square cylinder", "Create cone" };
		return add_world_patch(d, std::move(p), names[static_cast<int>(shape)]);
	}

	bool curve_to_terrain(document& doc)
	{
		return for_selected(doc, "Curve to terrain", "no curve is selected", [](patch& p, std::string&)
		{
			if (!bezier(p))
			{
				return false;
			}

			p = tessellated(p);
			return true;
		});
	}

	bool faces_to_terrain(document& doc, const bool delete_brushes)
	{
		impl& d = ops_access::of(doc);

		std::vector<face_ref> faces = d.sel.faces;
		std::vector<object_id> brushes;

		for (const object_id id : d.sel.objects)
		{
			const brush* b = d.get_brush(id);
			if (!b || b->opaque)
			{
				continue;
			}

			brushes.push_back(id);

			if (d.sel.faces.empty())
			{
				for (int f = 0; f < static_cast<int>(b->faces.size()); f++)
				{
					faces.push_back({ id, f });
				}
			}
		}

		struct made_patch
		{
			patch p;
			object_id owner;
		};

		std::vector<made_patch> made;
		int skipped = 0;

		for (const face_ref& ref : faces)
		{
			const brush* b = d.get_brush(ref.brush);
			const brush_geometry* geo = d.geometry(ref.brush);
			if (!b || b->opaque || !geo || ref.face < 0 || ref.face >= static_cast<int>(b->faces.size()))
			{
				continue;
			}

			const winding w = geo->face_winding(ref.face);
			if (w.size() < 3 || w.size() > 4)
			{
				skipped += w.size() > 4;
				continue;
			}

			const face& f = b->faces[static_cast<std::size_t>(ref.face)];
			int tw, th;
			d.texture_dims(f.texture.material, tw, th);

			vec3 u_axis, v_axis;
			face_texture_axes(geo->planes[static_cast<std::size_t>(ref.face)].normal, 0.0, u_axis, v_axis);

			patch p = new_patch(d, patch_type::mesh, f.texture.material);
			p.layer = b->layer;
			p.contents = b->contents;
			p.width = 2;
			p.height = 2;

			// counter clockwise from outside: 0 1 2 3 -> ( 0,0 ) ( 1,0 ) ( 1,1 ) ( 0,1 ); a triangle doubles its last corner
			const vec3 corners[4] = { w[0], w[1], w[2], w.size() == 4 ? w[3] : w[2] };
			const int order[4] = { 0, 3, 1, 2 };		// vertices[ column * 2 + row ]

			for (const int k : order)
			{
				const vec2 uv = face_uv(corners[k], geo->planes[static_cast<std::size_t>(ref.face)].normal, f.texture, tw, th);

				patch_vertex v;
				v.xyz = corners[k];
				v.tex = { uv.x * tw, uv.y * th, dot(corners[k], u_axis) / 16.0, dot(corners[k], v_axis) / 16.0 };
				p.vertices.push_back(std::move(v));
			}

			made.push_back({ std::move(p), b->owner });

			if (std::find(brushes.begin(), brushes.end(), ref.brush) == brushes.end())
			{
				brushes.push_back(ref.brush);
			}
		}

		if (made.empty())
		{
			return d.fail(skipped ? "Faces to terrain: only faces with 3 or 4 corners become a terrain patch"
				: "Faces to terrain: select brushes or brush faces");
		}

		transaction_scope tx(d, "Faces to terrain");
		std::vector<object_id> selection;

		for (made_patch& m : made)
		{
			selection.push_back(d.add_patch(std::move(m.p), m.owner));
		}

		if (delete_brushes)
		{
			for (const object_id id : brushes)
			{
				d.remove_object(id);
			}
		}

		d.sel.faces.clear();
		d.set_selection(std::move(selection));
		return true;
	}

	// ---- rows and columns ---------------------------------------------------------------------------

	bool patch_insert(document& doc, const patch_line line, const bool at_end, const int index)
	{
		return for_selected(doc, line == patch_line::column ? "Insert columns" : "Insert rows", "nothing to insert", [&](patch& p, std::string& why)
		{
			return on_lines(p, line, [&](patch& q) { return insert_columns(q, at_end, index, why); });
		});
	}

	bool patch_insert_terrain(document& doc, const patch_line line, const int after)
	{
		return for_selected(doc, line == patch_line::column ? "Add terrain column" : "Add terrain row", "no terrain mesh is selected", [&](patch& p, std::string& why)
		{
			return on_lines(p, line, [&](patch& q) { return insert_terrain_column(q, after, why); });
		});
	}

	bool patch_extrude_terrain(document& doc, const patch_line line, const bool at_end)
	{
		return for_selected(doc, line == patch_line::column ? "Extrude terrain column" : "Extrude terrain row", "no terrain mesh is selected", [&](patch& p, std::string& why)
		{
			return on_lines(p, line, [&](patch& q) { return extrude_terrain_column(q, at_end, why); });
		});
	}

	bool patch_delete(document& doc, const patch_line line, const bool at_end)
	{
		return for_selected(doc, line == patch_line::column ? "Delete columns" : "Delete rows", "nothing to delete", [&](patch& p, std::string& why)
		{
			return on_lines(p, line, [&](patch& q) { return delete_columns(q, at_end, 2, why); });
		});
	}

	bool patch_delete_terrain(document& doc, const patch_line line, const int index)
	{
		return for_selected(doc, line == patch_line::column ? "Remove terrain column" : "Remove terrain row", "no terrain mesh is selected", [&](patch& p, std::string& why)
		{
			return on_lines(p, line, [&](patch& q) { return delete_terrain_column(q, index, why); });
		});
	}

	bool patch_transpose(document& doc)
	{
		return for_selected(doc, "Transpose patch", "nothing to transpose", [](patch& p, std::string&)
		{
			p = transposed(p);
			return true;
		});
	}

	bool patch_invert(document& doc)
	{
		return for_selected(doc, "Invert patch", "nothing to invert", [](patch& p, std::string&)
		{
			reverse_columns(p);
			return true;
		});
	}

	bool patch_redisperse(document& doc, const patch_disperse mode)
	{
		static const char* const names[] = { "Redisperse vertices", "Redisperse columns", "Redisperse rows" };

		return for_selected(doc, names[static_cast<int>(mode)], "the vertices are spread evenly already", [&](patch& p, std::string&)
		{
			switch (mode)
			{
			case patch_disperse::columns:
				return disperse_columns_straight(p);

			case patch_disperse::rows:
				return on_lines(p, patch_line::row, [](patch& q) { return disperse_columns_straight(q); });

			default:
			{
				const bool columns = disperse_columns_along(p);
				const bool rows = on_lines(p, patch_line::row, [](patch& q) { return disperse_columns_along(q); });
				return columns || rows;
			}
			}
		});
	}

	bool patch_split(document& doc, const patch_line line, const int index)
	{
		impl& d = ops_access::of(doc);

		struct result
		{
			object_id id;
			patch first, second;
		};

		std::vector<result> results;
		std::string why;
		bool any = false;

		for (const object_id id : d.sel.objects)
		{
			const patch* p = d.get_patch(id);
			if (!p || !grid_ok(*p))
			{
				continue;
			}

			any = true;
			result res{ id, {}, {} };

			if (line == patch_line::column)
			{
				if (split_columns(*p, index, res.first, res.second, why))
				{
					results.push_back(std::move(res));
				}
			}
			else
			{
				patch a, b;
				if (split_columns(transposed(*p), index, a, b, why))
				{
					res.first = transposed(a);
					res.second = transposed(b);
					results.push_back(std::move(res));
				}
			}
		}

		if (results.empty())
		{
			return d.fail("Split patch: " + (!any ? std::string("no patch is selected") : why));
		}

		transaction_scope tx(d, "Split patch");
		std::vector<object_id> selection = d.sel.objects;

		for (result& res : results)
		{
			store(d, res.id, res.first);
			selection.push_back(add_like(d, res.first, std::move(res.second)));
		}

		d.set_selection(std::move(selection));
		return true;
	}

	bool patch_change_subdivision(document& doc, const int steps)
	{
		return for_selected(doc, steps > 0 ? "Increase subdivision" : "Decrease subdivision", "no curve is selected, or it is at the limit", [&](patch& p, std::string&)
		{
			if (!bezier(p))
			{
				return false;
			}

			const int now = p.has_header_scale && p.has_header_int ? p.header_int : 8;
			const int next = std::clamp(now + steps * 2, 2, 32);
			if (next == now && p.has_header_int)
			{
				return false;
			}

			p.has_header_scale = true;
			p.has_header_int = true;
			p.header_int = next;
			return true;
		});
	}

	// ---- welding ------------------------------------------------------------------------------------

	bool patch_weld(document& doc, const double tolerance)
	{
		impl& d = ops_access::of(doc);

		std::vector<vec3> anchors;
		std::vector<std::pair<object_id, patch>> results;
		bool any = false;

		for (const object_id id : d.sel.objects)
		{
			const patch* p = d.get_patch(id);
			if (!p)
			{
				continue;
			}

			any = true;
			patch q = *p;
			bool changed = false;

			for (patch_vertex& v : q.vertices)
			{
				const vec3* found = nullptr;
				double best = tolerance;

				for (const vec3& a : anchors)
				{
					const double dist = length(a - v.xyz);
					if (dist <= best)
					{
						best = dist;
						found = &a;
					}
				}

				if (!found)
				{
					anchors.push_back(v.xyz);
				}
				else if (!(*found == v.xyz))
				{
					v.xyz = *found;
					changed = true;
				}
			}

			if (changed)
			{
				results.emplace_back(id, std::move(q));
			}
		}

		if (results.empty())
		{
			return d.fail(any ? "Weld: no vertices lie closer together than the tolerance without being in the same place" : "Weld: no patch is selected");
		}

		transaction_scope tx(d, "Weld patches");
		for (const auto& [id, q] : results)
		{
			store(d, id, q);
		}

		return true;
	}

	bool patch_weld_to_brushes(document& doc, const double tolerance)
	{
		impl& d = ops_access::of(doc);

		std::vector<std::pair<object_id, patch>> results;
		bool any = false;

		for (const object_id id : d.sel.objects)
		{
			const patch* p = d.get_patch(id);
			if (!p)
			{
				continue;
			}

			any = true;
			bounds box;
			for (const patch_vertex& v : p->vertices)
			{
				box.add(v.xyz);
			}

			std::vector<vec3> corners;
			for (std::size_t other = 1; other < d.slots.size(); other++)
			{
				const auto oid = static_cast<object_id>(other);
				const brush* b = d.get_brush(oid);
				if (!b || b->opaque || !d.is_visible(oid))
				{
					continue;
				}

				const brush_geometry* geo = d.geometry(oid);
				if (geo && geo->box.intersects(box, tolerance))
				{
					corners.insert(corners.end(), geo->vertices.begin(), geo->vertices.end());
				}
			}

			patch q = *p;
			bool changed = false;

			for (patch_vertex& v : q.vertices)
			{
				double best = tolerance;
				const vec3* found = nullptr;

				for (const vec3& c : corners)
				{
					const double dist = length(c - v.xyz);
					if (dist <= best)
					{
						best = dist;
						found = &c;
					}
				}

				if (found && !(*found == v.xyz))
				{
					v.xyz = *found;
					changed = true;
				}
			}

			if (changed)
			{
				results.emplace_back(id, std::move(q));
			}
		}

		if (results.empty())
		{
			return d.fail(any ? "Weld to brushes: no patch vertex lies that close to a brush corner" : "Weld to brushes: no patch is selected");
		}

		transaction_scope tx(d, "Weld to brushes");
		for (const auto& [id, q] : results)
		{
			store(d, id, q);
		}

		return true;
	}

	bool move_patch_vertices_welded(document& doc, const object_id id, const std::span<const int> vertices, const vec3& delta,
		const double weld_tolerance, const std::span<const int> locked)
	{
		impl& d = ops_access::of(doc);

		const patch* p = d.get_patch(id);
		if (!p || vertices.empty())
		{
			return d.fail("Move vertices: not a patch");
		}

		std::vector<int> moving;
		std::vector<vec3> from;

		for (const int index : vertices)
		{
			if (index < 0 || index >= static_cast<int>(p->vertices.size()))
			{
				return d.fail("Move vertices: bad vertex index");
			}

			if (std::find(locked.begin(), locked.end(), index) == locked.end() && std::find(moving.begin(), moving.end(), index) == moving.end())
			{
				moving.push_back(index);
				from.push_back(p->vertices[static_cast<std::size_t>(index)].xyz);
			}
		}

		if (moving.empty())
		{
			return d.fail("Move vertices: the vertices are locked");
		}

		std::vector<std::pair<object_id, std::vector<int>>> targets;
		targets.emplace_back(id, moving);

		if (weld_tolerance >= 0.0)
		{
			std::vector<object_id> others{ id };
			for (const object_id other : d.sel.objects)
			{
				if (other != id && d.get_patch(other))
				{
					others.push_back(other);
				}
			}

			for (const object_id other : others)
			{
				const patch* q = d.get_patch(other);
				std::vector<int> along;

				for (int i = 0; i < static_cast<int>(q->vertices.size()); i++)
				{
					if (other == id && (std::find(moving.begin(), moving.end(), i) != moving.end() || std::find(locked.begin(), locked.end(), i) != locked.end()))
					{
						continue;
					}

					const vec3& at = q->vertices[static_cast<std::size_t>(i)].xyz;
					if (std::any_of(from.begin(), from.end(), [&](const vec3& f) { return length(f - at) <= weld_tolerance; }))
					{
						along.push_back(i);
					}
				}

				if (other == id)
				{
					targets[0].second.insert(targets[0].second.end(), along.begin(), along.end());
				}
				else if (!along.empty())
				{
					targets.emplace_back(other, std::move(along));
				}
			}
		}

		transaction_scope tx(d, "Move vertices");
		for (const auto& [target, list] : targets)
		{
			patch& edit = d.edit_patch(target);
			for (const int index : list)
			{
				edit.vertices[static_cast<std::size_t>(index)].xyz += delta;
			}
		}

		return true;
	}

	// ---- thicken / cap ------------------------------------------------------------------------------

	namespace
	{
		// a strip from line `a` to line `b` ( both run along the rows of the strip ), facing `outward`
		patch make_strip(const patch& like, const line_t& a, const line_t& b, const bool curve, const vec3& outward)
		{
			patch s = like;
			s.type = curve ? patch_type::curve : patch_type::mesh;
			s.extra.clear();

			std::vector<line_t> cols;
			cols.push_back(a);
			if (curve)
			{
				cols.push_back(mix(a, b, 0.5));
			}
			cols.push_back(b);

			for (line_t& col : cols)
			{
				for (patch_vertex& v : col)
				{
					v.has_flag = false;
					v.flag = 0;
				}
			}

			set_columns(s, cols);

			if (dot(area_normal(curve ? tessellated(s) : s), outward) < 0.0)
			{
				reverse_columns(s);
			}

			natural_texture(s, 0.25);
			return s;
		}
	}

	bool patch_thicken(document& doc, const double thickness, const bool seams)
	{
		impl& d = ops_access::of(doc);

		if (std::fabs(thickness) < ON_EPSILON)
		{
			return d.fail("Thicken: the thickness is zero");
		}

		struct made_patch
		{
			patch p;
			const patch* like;
		};

		std::vector<made_patch> made;

		for (const object_id id : d.sel.objects)
		{
			const patch* p = d.get_patch(id);
			if (!p || !grid_ok(*p))
			{
				continue;
			}

			const std::vector<vec3> normals = control_normals(*p);

			patch back = *p;
			back.extra.clear();
			for (std::size_t i = 0; i < back.vertices.size(); i++)
			{
				back.vertices[i].xyz -= normals[i] * thickness;
			}

			const patch offset = back;		// same grid as the original
			if (thickness > 0.0)
			{
				reverse_columns(back);
			}

			made.push_back({ std::move(back), p });

			if (!seams)
			{
				continue;
			}

			const bool curve = bezier(*p);
			const patch pt = transposed(*p), ot = transposed(offset);

			if (!closed_columns(*p))
			{
				for (const int c : { 0, p->width - 1 })
				{
					const int inner = c == 0 ? 1 : p->width - 2;
					const vec3 outward = p->at(c, p->height / 2).xyz - p->at(inner, p->height / 2).xyz;
					made.push_back({ make_strip(*p, columns_of(*p)[static_cast<std::size_t>(c)], columns_of(offset)[static_cast<std::size_t>(c)], curve, outward), p });
				}
			}

			if (!closed_columns(pt))
			{
				for (const int r : { 0, p->height - 1 })
				{
					const int inner = r == 0 ? 1 : p->height - 2;
					vec3 outward;
					for (int c = 0; c < p->width; c++)
					{
						outward += p->at(c, r).xyz - p->at(c, inner).xyz;
					}

					made.push_back({ make_strip(*p, columns_of(pt)[static_cast<std::size_t>(r)], columns_of(ot)[static_cast<std::size_t>(r)], curve, outward), p });
				}
			}
		}

		if (made.empty())
		{
			return d.fail("Thicken: no patch is selected");
		}

		transaction_scope tx(d, "Thicken patch");
		std::vector<object_id> selection = d.sel.objects;

		// `like` points into the document: add_patch replaces the owner entity, not the patches
		for (made_patch& m : made)
		{
			const patch like = *m.like;
			selection.push_back(add_like(d, like, std::move(m.p)));
		}

		d.set_selection(std::move(selection));
		return true;
	}

	bool patch_cap(document& doc, const patch_cap_type type, const bool first_end, const bool last_end)
	{
		impl& d = ops_access::of(doc);

		std::vector<std::pair<patch, patch>> made;		// cap, the patch it belongs to
		bool any = false;

		for (const object_id id : d.sel.objects)
		{
			const patch* p = d.get_patch(id);
			if (!p || !grid_ok(*p) || !bezier(*p))
			{
				continue;
			}

			any = true;
			const bool closed = closed_columns(*p);

			for (const int end : { 0, 1 })
			{
				if ((end == 0 && !first_end) || (end == 1 && !last_end))
				{
					continue;
				}

				const int r = end == 0 ? 0 : p->height - 1;
				const int inner = end == 0 ? 1 : p->height - 2;
				const line_t outline = row_of(*p, r);
				const auto w = static_cast<int>(outline.size());

				// an end that is a single point ( the tip of a cone ) needs no cap
				bool point = true;
				vec3 outward;
				for (int c = 0; c < w; c++)
				{
					point = point && length(outline[static_cast<std::size_t>(c)].xyz - outline[0].xyz) < SAME_POINT;
					outward += outline[static_cast<std::size_t>(c)].xyz - p->at(c, inner).xyz;
				}

				if (point)
				{
					continue;
				}

				patch_cap_type use = type;
				if (use == patch_cap_type::automatic || (closed && (use == patch_cap_type::bevel || use == patch_cap_type::endcap)))
				{
					use = closed ? patch_cap_type::cylinder : patch_cap_type::bevel;
				}

				line_t target = outline;

				if (use == patch_cap_type::cylinder)
				{
					vec3 centre;
					int count = 0;
					for (int c = 0; c + (closed ? 1 : 0) < w; c += 2)
					{
						centre += outline[static_cast<std::size_t>(c)].xyz;
						count++;
					}

					centre = centre * (1.0 / std::max(count, 1));
					for (patch_vertex& v : target)
					{
						v.xyz = centre;
					}
				}
				else if (use == patch_cap_type::bevel || use == patch_cap_type::endcap)
				{
					for (int c = 0; c < w; c++)
					{
						target[static_cast<std::size_t>(c)].xyz = outline.front().xyz + (outline.back().xyz - outline.front().xyz) * (static_cast<double>(c) / (w - 1));
					}
				}
				else
				{
					for (int c = 0; c < w; c++)
					{
						const int corner = (c & 1) ? c : (c == 0 ? 1 : (c == w - 1 ? w - 2 : c));
						target[static_cast<std::size_t>(c)].xyz = outline[static_cast<std::size_t>(corner)].xyz;
					}
				}

				patch cap = *p;
				cap.extra.clear();
				cap.width = w;
				cap.height = 3;
				cap.vertices.assign(static_cast<std::size_t>(w) * 3, {});

				for (int c = 0; c < w; c++)
				{
					cap.at(c, 0) = outline[static_cast<std::size_t>(c)];
					cap.at(c, 2) = target[static_cast<std::size_t>(c)];
					cap.at(c, 1) = mix(cap.at(c, 0), cap.at(c, 2), 0.5);

					for (int k = 0; k < 3; k++)
					{
						cap.at(c, k).has_flag = false;
						cap.at(c, k).flag = 0;
					}
				}

				// of the surface, not of the control points: those of an inverted cap lie on one line
				const vec3 facing = area_normal(tessellated(cap));
				if (length(facing) < 1e-6)
				{
					continue;	// the outline encloses nothing
				}

				if (dot(facing, outward) < 0.0)
				{
					reverse_columns(cap);
				}

				cap_texture(cap, -1);
				made.emplace_back(std::move(cap), *p);
			}
		}

		if (made.empty())
		{
			return d.fail(any ? "Cap: the ends of the patch enclose no area" : "Cap: no curve patch is selected");
		}

		transaction_scope tx(d, "Cap patch");
		std::vector<object_id> selection = d.sel.objects;

		for (auto& [cap, like] : made)
		{
			selection.push_back(add_like(d, like, std::move(cap)));
		}

		d.set_selection(std::move(selection));
		return true;
	}

	// ---- terrain edges ------------------------------------------------------------------------------

	namespace
	{
		void turn(patch& p, const int column, const int row)
		{
			patch_vertex& v = p.at(column, row);
			const bool now = turned(v);
			v.has_flag = !now;
			v.flag = now ? 0 : 1;
		}
	}

	bool patch_turn_edges(document& doc)
	{
		return for_selected(doc, "Turn terrain edges", "no terrain mesh is selected", [](patch& p, std::string&)
		{
			if (bezier(p))
			{
				return false;
			}

			for (int c = 0; c + 1 < p.width; c++)
			{
				for (int r = 0; r + 1 < p.height; r++)
				{
					turn(p, c, r);
				}
			}

			return true;
		});
	}

	bool patch_turn_edge(document& doc, const object_id id, const int column, const int row)
	{
		impl& d = ops_access::of(doc);
		const patch* p = d.get_patch(id);

		if (!p || !grid_ok(*p) || bezier(*p))
		{
			return d.fail("Turn edge: not a terrain mesh");
		}

		if (column < 0 || row < 0 || column + 1 >= p->width || row + 1 >= p->height)
		{
			return d.fail("Turn edge: no such quad");
		}

		transaction_scope tx(d, "Turn terrain edge");
		turn(d.edit_patch(id), column, row);
		return true;
	}

	bool patch_pick_quad(const document& doc, const object_id id, const ray& r, int& column, int& row, double& distance)
	{
		const patch* p = ops_access::of(doc).get_patch(id);
		if (!p || !grid_ok(*p) || bezier(*p))
		{
			return false;
		}

		bool hit = false;
		distance = 1e30;

		for (int c = 0; c + 1 < p->width; c++)
		{
			for (int q = 0; q + 1 < p->height; q++)
			{
				const vec3& v00 = p->at(c, q).xyz, & v10 = p->at(c + 1, q).xyz, & v11 = p->at(c + 1, q + 1).xyz, & v01 = p->at(c, q + 1).xyz;
				const bool t = turned(p->at(c, q));

				double d1 = 0.0, d2 = 0.0;
				const bool h1 = t ? ray_triangle(r, v00, v10, v01, d1) : ray_triangle(r, v00, v10, v11, d1);
				const bool h2 = t ? ray_triangle(r, v10, v11, v01, d2) : ray_triangle(r, v00, v11, v01, d2);

				for (const auto& [h, dist] : { std::pair{ h1, d1 }, std::pair{ h2, d2 } })
				{
					if (h && dist < distance)
					{
						distance = dist;
						column = c;
						row = q;
						hit = true;
					}
				}
			}
		}

		return hit;
	}

	// ---- textures -----------------------------------------------------------------------------------

	bool patch_cap_texture(document& doc, const int axis)
	{
		return for_selected(doc, "Cap texture", "nothing to texture", [&](patch& p, std::string&)
		{
			cap_texture(p, axis);
			return true;
		});
	}

	bool patch_cycle_cap_texture(document& doc)
	{
		return for_selected(doc, "Cycle cap texture", "nothing to texture", [](patch& p, std::string&)
		{
			const int now = projected_axis(p);
			cap_texture(p, now < 0 ? -1 : (now + 1) % 3);
			return true;
		});
	}

	bool patch_natural_texture(document& doc, const double scale)
	{
		if (!(scale > 1e-6))
		{
			return ops_access::of(doc).fail("Natural texture: the scale has to be positive");
		}

		return for_selected(doc, "Natural texture", "nothing to texture", [&](patch& p, std::string&)
		{
			natural_texture(p, scale);
			return true;
		});
	}

	bool patch_fit_texture(document& doc, const double repeat_x, const double repeat_y)
	{
		impl& d = ops_access::of(doc);

		return for_selected(doc, "Fit texture", "nothing to texture", [&](patch& p, std::string&)
		{
			int tw, th;
			d.texture_dims(p.material, tw, th);

			for (int c = 0; c < p.width; c++)
			{
				for (int r = 0; r < p.height; r++)
				{
					patch_vertex& v = p.at(c, r);
					ensure_tex(v);
					v.tex[0] = static_cast<double>(c) / (p.width - 1) * tw * repeat_x;
					v.tex[1] = (1.0 - static_cast<double>(r) / (p.height - 1)) * th * repeat_y;
				}
			}

			return true;
		});
	}

	// ---- vertices -----------------------------------------------------------------------------------

	bool edit_patch(document& doc, const object_id id, const std::string_view undo_name, const std::function<bool(patch&)>& fn)
	{
		impl& d = ops_access::of(doc);
		const patch* p = d.get_patch(id);

		if (!p)
		{
			return d.fail(std::string(undo_name) + ": not a patch");
		}

		patch q = *p;
		if (!fn(q))
		{
			return d.fail(std::string(undo_name) + ": nothing to do");
		}

		if (!grid_ok(q))
		{
			return d.fail(std::string(undo_name) + ": the patch needs width x height vertices, 2 x 2 or more");
		}

		transaction_scope tx(d, undo_name);
		store(d, id, q);
		return true;
	}

	namespace
	{
		template <typename Fn>
		bool for_vertices(document& doc, const object_id id, const std::span<const int> vertices, const std::string_view name, Fn&& fn)
		{
			return edit_patch(doc, id, name, [&](patch& p)
			{
				bool changed = false;

				if (vertices.empty())
				{
					for (std::size_t i = 0; i < p.vertices.size(); i++)
					{
						changed = fn(p.vertices[i], i) || changed;
					}
				}

				for (const int index : vertices)
				{
					if (index >= 0 && index < static_cast<int>(p.vertices.size()))
					{
						changed = fn(p.vertices[static_cast<std::size_t>(index)], static_cast<std::size_t>(index)) || changed;
					}
				}

				return changed;
			});
		}
	}

	bool patch_set_color(document& doc, const object_id id, const std::span<const int> vertices, const std::uint8_t rgba[4])
	{
		return for_vertices(doc, id, vertices, "Vertex colour", [&](patch_vertex& v, std::size_t)
		{
			const bool same = v.has_color && std::equal(v.color, v.color + 4, rgba);
			v.has_color = true;
			std::copy(rgba, rgba + 4, v.color);
			return !same;
		});
	}

	bool patch_add_noise(document& doc, const object_id id, const std::span<const int> vertices, const vec3& scale, const std::uint32_t seed)
	{
		const patch* p = doc.find_patch(id);
		if (!p)
		{
			return ops_access::of(doc).fail("Noise: not a patch");
		}

		// vertices in one place ( the seam of a tube ) get the same offset: the hash is taken from the position
		const auto offset = [&](const vec3& at, const int axis)
		{
			std::uint64_t h = seed * 0x9E3779B97F4A7C15ull + static_cast<std::uint64_t>(axis) * 0xBF58476D1CE4E5B9ull;
			for (int i = 0; i < 3; i++)
			{
				h ^= static_cast<std::uint64_t>(std::llround(at[i] * 64.0)) + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
			}

			h ^= h >> 31;
			h *= 0x94D049BB133111EBull;
			h ^= h >> 29;
			return (static_cast<double>(h & 0xFFFFFFull) / 0xFFFFFF) * 2.0 - 1.0;
		};

		return for_vertices(doc, id, vertices, "Vertex noise", [&](patch_vertex& v, std::size_t)
		{
			const vec3 at = v.xyz;
			for (int i = 0; i < 3; i++)
			{
				v.xyz[i] += offset(at, i) * scale[i];
			}

			return !(v.xyz == at);
		});
	}

	bool patch_set_vertex(document& doc, const object_id id, const int index, const patch_vertex& v)
	{
		return edit_patch(doc, id, "Edit vertex", [&](patch& p)
		{
			if (index < 0 || index >= static_cast<int>(p.vertices.size()))
			{
				return false;
			}

			p.vertices[static_cast<std::size_t>(index)] = v;
			return true;
		});
	}
}
