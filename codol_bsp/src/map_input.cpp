// document -> world: brushes with windings, entities, lights.

#include "world.hpp"

namespace codol::bsp
{
	std::string_view map_entity::value(const std::string_view key, const std::string_view fallback) const
	{
		for (const auto& [k, v] : pairs)
		{
			if (k == key)
			{
				return v;
			}
		}
		return fallback;
	}

	void map_entity::set(const std::string_view key, const std::string_view text)
	{
		for (auto& [k, v] : pairs)
		{
			if (k == key)
			{
				v = std::string(text);
				return;
			}
		}
		pairs.emplace_back(std::string(key), std::string(text));
	}

	std::uint32_t pack_unit_vector(const float n[3])
	{
		// PackedUnitVec: bytes ( n * 127 + 127 ), scale byte 63 = 1 / 127 ( codol_zone unpack_unit_vector )
		std::uint32_t packed = 63u << 24;
		for (int k = 0; k < 3; k++)
		{
			const long byte = std::clamp(std::lround(n[k] * 127.0f) + 127l, 0l, 255l);
			packed |= static_cast<std::uint32_t>(byte) << (8 * k);
		}
		return packed;
	}

	std::uint32_t plane_pool::add(const double normal_in[3], const double dist_in)
	{
		double n[3] = { normal_in[0], normal_in[1], normal_in[2] };
		const double length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		if (!(length > 1e-9) || !std::isfinite(length) || !std::isfinite(dist_in))
		{
			throw compile_error("a plane without a usable normal");
		}
		double dist = dist_in / length;
		int axial = -1;
		for (int k = 0; k < 3; k++)
		{
			n[k] /= length;
			if (std::fabs(n[k]) > 1.0 - 1e-7)
			{
				axial = k;
			}
		}
		if (axial >= 0)
		{
			// exactly axial
			const double sign = n[axial] > 0 ? 1.0 : -1.0;
			n[0] = n[1] = n[2] = 0;
			n[axial] = sign;
		}
		c1::cplane_s p{};
		for (int k = 0; k < 3; k++)
		{
			p.normal[k] = static_cast<float>(n[k]);
		}
		p.dist = static_cast<float>(dist);
		// as the shipped zones have them: the axis for a plane that faces + x / y / z, 3 for all others; signbits 0
		p.type = axial >= 0 && n[axial] > 0 ? static_cast<std::uint8_t>(axial) : 3;
		const auto quant = [](const float v, const double step) { return static_cast<std::int64_t>(std::llround(v / step)); };
		const std::uint64_t key = static_cast<std::uint64_t>(quant(p.normal[0], 1e-4) * 73856093ll) ^ static_cast<std::uint64_t>(quant(p.normal[1], 1e-4) * 19349663ll)
			^ static_cast<std::uint64_t>(quant(p.normal[2], 1e-4) * 83492791ll) ^ static_cast<std::uint64_t>(quant(p.dist, 0.25) * 2654435761ll);
		// neighbours of the distance bucket are not searched: a near duplicate only costs 20 bytes
		const auto range = lookup.equal_range(key);
		for (auto it = range.first; it != range.second; ++it)
		{
			const c1::cplane_s& q = planes[it->second];
			if (std::fabs(q.dist - p.dist) < 1e-3f && std::fabs(q.normal[0] - p.normal[0]) < 1e-5f && std::fabs(q.normal[1] - p.normal[1]) < 1e-5f
				&& std::fabs(q.normal[2] - p.normal[2]) < 1e-5f)
			{
				return it->second;
			}
		}
		planes.push_back(p);
		lookup.emplace(key, static_cast<std::uint32_t>(planes.size() - 1));
		return static_cast<std::uint32_t>(planes.size() - 1);
	}

	namespace
	{
		bool parse_floats(const std::string_view text, float* out, const int count)
		{
			const std::string copy(text);
			const char* at = copy.c_str();
			for (int k = 0; k < count; k++)
			{
				char* end = nullptr;
				out[k] = std::strtof(at, &end);
				if (end == at)
				{
					return false;
				}
				at = end;
			}
			return true;
		}

		void angles_forward(const float pitch, const float yaw, float out[3])
		{
			const float p = pitch * 3.14159265f / 180.0f, y = yaw * 3.14159265f / 180.0f;
			out[0] = std::cos(p) * std::cos(y);
			out[1] = std::cos(p) * std::sin(y);
			out[2] = -std::sin(p);
		}

		// brushes that take no part in anything the game sees
		bool is_ignored_material(const std::string_view name)
		{
			return name.rfind("hint", 0) == 0 || name == "skip" || name.rfind("portal", 0) == 0 || name.rfind("lightgrid", 0) == 0;
		}
	}

	void read_document(const edit::document& doc, const options&, world& w, report& rep)
	{
		w.lights.clear();
		w.lights.resize(2);
		w.lights[0].type = 0;
		w.lights[0].primary = true;
		w.lights[1].type = 1;

		for (const edit::object_id entity_id : doc.entities())
		{
			const edit::entity* e = doc.find_entity(entity_id);
			if (!e)
			{
				continue;
			}
			map_entity out;
			out.pairs = e->pairs;
			out.classname = std::string(e->classname());
			out.origin = e->origin();
			out.world = e->is_worldspawn();
			const int entity_index = static_cast<int>(w.entities.size());
			if (out.world && entity_index != 0)
			{
				throw compile_error("the map has more than one worldspawn");
			}
			if (!out.world && entity_index == 0)
			{
				throw compile_error("the first entity of the map is not the worldspawn");
			}

			if (out.classname == "light")
			{
				map_light light;
				light.type = 3;
				float color[3] = { 1, 1, 1 }, value = 1;
				parse_floats(out.value("_color", "1 1 1"), color, 3);
				parse_floats(out.value("intensity", "1"), &value, 1);
				for (int k = 0; k < 3; k++)
				{
					light.color[k] = color[k] * value;
				}
				light.origin[0] = static_cast<float>(out.origin.x);
				light.origin[1] = static_cast<float>(out.origin.y);
				light.origin[2] = static_cast<float>(out.origin.z);
				parse_floats(out.value("radius", "300"), &light.radius, 1);
				if (const std::string_view def = out.value("def"); !def.empty())
				{
					light.def = std::string(def);
				}
				float outer = 0;
				if (parse_floats(out.value("fov_outer", ""), &outer, 1) && outer > 0)
				{
					float inner = outer * 0.5f;
					parse_floats(out.value("fov_inner", ""), &inner, 1);
					light.type = 2;
					light.cos_half_fov_outer = std::cos(outer * 0.5f * 3.14159265f / 180.0f);
					light.cos_half_fov_inner = std::cos(std::min(inner, outer) * 0.5f * 3.14159265f / 180.0f);
					const vec3 angles = e->angles();
					float forward[3];
					angles_forward(static_cast<float>(angles.x), static_cast<float>(angles.y), forward);
					for (int k = 0; k < 3; k++)
					{
						light.dir[k] = -forward[k];
					}
				}
				w.bake_lights.push_back(light);
				rep.lights++;
				continue;		// not part of the entity string
			}

			for (const edit::object_id child : e->children)
			{
				if (doc.kind(child) == edit::object_kind::patch)
				{
					rep.patches++;
					// a patch ( terrain mesh or curve ): its tessellation is drawn, and every triangle gets a thin
					// brush under it to collide with
					edit::mesh mesh;
					const edit::patch* source_patch = doc.find_patch(child);
					if (!out.world || !source_patch || !doc.build_mesh(child, mesh, 4) || mesh.indices.size() < 3)
					{
						if (!out.world)
						{
							rep.warnings.push_back(format("patch %u belongs to a brush entity; patches are only compiled in the worldspawn", child));
						}
						continue;
					}
					map_patch patch;
					patch.material = source_patch->material;
					if (patch.material.rfind("wc/", 0) == 0)
					{
						patch.material.erase(0, 3);
					}
					patch.vertices = mesh.vertices;
					patch.indices = mesh.indices;
					const int patch_index = static_cast<int>(w.patches.size());
					for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
					{
						vec3 corner[3];
						for (int k = 0; k < 3; k++)
						{
							const edit::mesh_vertex& v = mesh.vertices[mesh.indices[t + static_cast<std::size_t>(k)]];
							corner[k] = { v.position[0], v.position[1], v.position[2] };
						}
						vec3 normal = cross(corner[1] - corner[0], corner[2] - corner[0]);
						const double area = length(normal);
						if (area < 1.0)
						{
							continue;
						}
						normal = normal * (1.0 / area);
						const vec3 points[6] = { corner[0], corner[1], corner[2], corner[0] - normal * 8.0, corner[1] - normal * 8.0, corner[2] - normal * 8.0 };
						std::vector<edit::plane> planes;
						std::vector<std::vector<int>> hull;
						if (!edit::convex_hull(points, planes, hull))
						{
							continue;
						}
						map_brush b;
						b.entity = entity_index;
						b.patch = patch_index;
						b.detail = true;
						b.corners.assign(points, points + 6);
						for (const vec3& p : points)
						{
							b.box.add(p);
						}
						for (std::size_t f = 0; f < planes.size(); f++)
						{
							map_face face;
							face.plane = planes[f];
							face.material = patch.material;
							face.texture.material = patch.material;
							face.corner = hull[f];
							for (const int v : hull[f])
							{
								face.winding.push_back(points[v]);
							}
							face.visible = false;		// the patch is drawn, not the brush
							b.faces.push_back(std::move(face));
						}
						out.brushes.push_back(static_cast<int>(w.brushes.size()));
						w.brushes.push_back(std::move(b));
					}
					w.patches.push_back(std::move(patch));
					continue;
				}
				const edit::brush* source = doc.find_brush(child);
				const edit::brush_geometry* geo = doc.geometry(child);
				if (!source || source->opaque)
				{
					continue;
				}
				rep.brushes++;
				if (!geo || !geo->valid)
				{
					rep.warnings.push_back(format("brush %u of entity %d is not a closed convex volume and is left out", child, entity_index));
					continue;
				}
				map_brush b;
				b.id = child;
				b.entity = entity_index;
				b.detail = source->is_detail();
				b.corners = geo->vertices;
				for (const vec3& v : geo->vertices)
				{
					b.box.add(v);
				}
				bool ignored = false;
				for (std::size_t f = 0; f < source->faces.size() && f < geo->planes.size(); f++)
				{
					if (geo->face_vertices[f].size() < 3)
					{
						continue;
					}
					map_face face;
					face.plane = geo->planes[f];
					face.texture = source->faces[f].texture;
					face.material = face.texture.material;
					if (face.material.rfind("wc/", 0) == 0)
					{
						face.material.erase(0, 3);
					}
					face.corner = geo->face_vertices[f];
					for (const int v : face.corner)
					{
						face.winding.push_back(geo->vertices[static_cast<std::size_t>(v)]);
					}
					ignored = ignored || is_ignored_material(face.material);
					b.faces.push_back(std::move(face));
				}
				if (ignored || b.faces.size() < 4)
				{
					// a portal brush: its "portal" faces separate cells, the brush itself is nothing
					for (map_face& face : b.faces)
					{
						if (face.material == "portal" && out.world)
						{
							w.portal_faces.push_back(std::move(face));
						}
					}
					continue;
				}
				if (source->has_content("nonColliding"))
				{
					b.collides = false;
				}
				out.brushes.push_back(static_cast<int>(w.brushes.size()));
				w.brushes.push_back(std::move(b));
			}
			if (!out.world && !out.brushes.empty())
			{
				out.model = w.model_count++;
				for (const int b : out.brushes)
				{
					w.brushes[static_cast<std::size_t>(b)].model = out.model;
				}
				out.set("model", format("*%d", out.model));
			}
			if (out.classname == "misc_model")
			{
				// Not compiled into the world as a static model ( that needs the xmodel carried into the zone, cells,
				// collision ): the model becomes a script_model entity, which the game spawns by name from the
				// zones it has loaded. Drawn and lit by the light grid; no collision, no baked shadow.
				// An entity each: more than a handful would use up the game's entities, so the rest is left out.
				constexpr std::size_t MODEL_ENTITIES = 128;
				rep.static_models++;
				if (rep.static_models > MODEL_ENTITIES)
				{
					if (rep.static_models == MODEL_ENTITIES + 1)
					{
						rep.warnings.push_back(format("misc_model: only the first %zu are kept ( as script_model entities ); static models are not compiled into the world",
							MODEL_ENTITIES));
					}
					continue;
				}
				out.classname = "script_model";
				out.set("classname", "script_model");
			}
			if (out.classname == "misc_prefab" || out.classname == "reflection_probe" || out.classname == "info_null")
			{
				out.in_ents = false;
			}
			w.entities.push_back(std::move(out));
		}
		if (w.entities.empty())
		{
			throw compile_error("the map has no worldspawn");
		}
		rep.entities = w.entities.size();

		// the sun and the ambient light, from the worldspawn keys the shipped maps carry
		const map_entity& ws = w.entities[0];
		float direction[3] = { -50, 40, 0 }, color[3] = { 1, 0.95f, 0.85f }, sunlight = 1.3f, ambient = 0.1f, sky_color[3] = { 0.8f, 0.9f, 1.0f }, skylight = 0.4f;
		parse_floats(ws.value("sundirection", ""), direction, 3);
		parse_floats(ws.value("suncolor", ""), color, 3);
		parse_floats(ws.value("sunlight", ""), &sunlight, 1);
		parse_floats(ws.value("ambient", ""), &ambient, 1);
		parse_floats(ws.value("skycolor", ""), sky_color, 3);
		parse_floats(ws.value("skylight", ""), &skylight, 1);
		angles_forward(direction[0], direction[1], w.sun_dir);
		for (int k = 0; k < 3; k++)
		{
			w.sun_color[k] = color[k] * sunlight;
			w.ambient[k] = ambient + sky_color[k] * skylight;
			w.ambient_light[k] = ambient;
			w.sky_light[k] = sky_color[k] * skylight;
			w.lights[1].color[k] = w.sun_color[k];
			w.lights[1].dir[k] = w.sun_dir[k];
		}

		for (const map_brush& b : w.brushes)
		{
			if (b.model == 0)
			{
				w.bounds.add(b.box);
				(b.detail ? rep.detail_brushes : rep.structural_brushes)++;
			}
		}
		if (!w.bounds.valid())
		{
			throw compile_error("the worldspawn has no brushes");
		}
	}

	std::string build_entity_string(const world& w)
	{
		std::string out;
		for (const map_entity& e : w.entities)
		{
			if (!e.in_ents)
			{
				continue;
			}
			out += "{\n";
			for (const auto& [k, v] : e.pairs)
			{
				out += "\"" + k + "\" \"" + v + "\"\n";
			}
			out += "}\n";
		}
		return out;
	}
}
