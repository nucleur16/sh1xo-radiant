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
			// a text without ( enough ) numbers leaves `out` as it is: the caller's default stays. ( It used to write a 0
			// first - a worldspawn without light keys had no sun, no sky and no ambient light at all. )
			float values[4] = {};
			for (int k = 0; k < count && k < 4; k++)
			{
				char* end = nullptr;
				values[k] = std::strtof(at, &end);
				if (end == at)
				{
					return false;
				}
				at = end;
			}
			std::copy(values, values + std::min(count, 4), out);
			return true;
		}

		// a light without a "radius": far enough for a room ( radiant's own default of 200 leaves a room of 512 units
		// dark at the walls )
		constexpr float DEFAULT_LIGHT_RADIUS = 600.0f;

		// the inner angle of a primary omni light's cone when the map gives none ( the outer one is 120 ): see where it
		// is used
		constexpr float OMNI_DEFAULT_FOV_INNER = 60.0f;

		// What a surface under the open sky receives of "skycolor" x "skylight": half. Measured on mp_vacant, whose sun lit
		// surfaces hold ( 0.30 0.60 0.75 ) of light without the sun ( codol_bsp_probe lights: upper layer 118 184 206, lower
		// 97 82 77 ) under skycolor 0.35 0.75 1 x skylight 1.3 and an ambient light of 0.15 x ( 0.71 0.71 1 ).
		constexpr float SKY_SCALE = 0.5f;

		// "_color", "suncolor": the hue of a light. The largest part counts as 1 ( 0.1 0.1 0.1 is white ), as radiant's
		// light entity says and as the suns of the shipped maps show ( mp_vacant: suncolor 0.84 0.81 0.58, sunlight 1.6,
		// ComWorld colour 1.600 1.543 1.105 )
		void normalize_color(float color[3])
		{
			const float largest = std::max({ color[0], color[1], color[2] });
			for (int k = 0; k < 3; k++)
			{
				color[k] = largest > 1e-6f ? std::max(color[k], 0.0f) / largest : 1.0f;
			}
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

	void read_document(const edit::document& doc, const options& opts, world& w, report& rep)
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

			if (out.classname == "script_fx" || out.classname == "script_sound")
			{
				// an effect ( "fx": the effect's name, "delay": seconds between repeats, none: once ) or a looping sound
				// ( "soundalias" ): lines of the generated createfx script, no entity
				map_fx fx;
				fx.fx = std::string(out.value("fx"));
				fx.alias = std::string(out.value("soundalias"));
				fx.origin[0] = static_cast<float>(out.origin.x);
				fx.origin[1] = static_cast<float>(out.origin.y);
				fx.origin[2] = static_cast<float>(out.origin.z);
				parse_floats(out.value("angles", "270 0 0"), fx.angles, 3);
				parse_floats(out.value("delay", "-15"), &fx.delay, 1);
				if (out.classname == "script_sound")
				{
					fx.fx.clear();
				}
				if (!fx.fx.empty() || !fx.alias.empty())
				{
					w.fx.push_back(std::move(fx));
				}
				continue;
			}

			if (out.classname == "light" || out.classname == "light_spot" || out.classname == "light_omni")
			{
				// The keys of radiant's light entity ( README, "Light keys" ). "_color" gives the hue - its largest part
				// counts as 1 -, "intensity" the brightness. "light_spot" / "light_omni" are the classes the shipped maps'
				// primary lights have: a primary spot / omni light whatever the spawnflags say.
				map_light light;
				light.type = 3;
				light.primary = false;
				float color[3] = { 1, 1, 1 }, value = 1;
				parse_floats(out.value("_color", "1 1 1"), color, 3);
				parse_floats(out.value("intensity", "1"), &value, 1);
				normalize_color(color);
				for (int k = 0; k < 3; k++)
				{
					light.color[k] = color[k] * std::max(value, 0.0f);
				}
				light.origin[0] = static_cast<float>(out.origin.x);
				light.origin[1] = static_cast<float>(out.origin.y);
				light.origin[2] = static_cast<float>(out.origin.z);
				light.radius = DEFAULT_LIGHT_RADIUS;
				parse_floats(out.value("radius", ""), &light.radius, 1);
				light.radius = std::max(light.radius, 1.0f);
				if (const std::string_view def = out.value("def"); !def.empty())
				{
					light.def = std::string(def);
				}
				float flags = 0;
				parse_floats(out.value("spawnflags", "0"), &flags, 1);
				const int spawnflags = static_cast<int>(flags);
				// where it points: at its target, else along its angles; straight down without either
				float forward[3] = { 0, 0, -1 };
				float outer = 0, inner = 0, exponent = 0;
				bool aimed = false;
				if (const std::string_view target = out.value("target"); !target.empty())
				{
					for (const edit::object_id other_id : doc.entities())
					{
						const edit::entity* other = doc.find_entity(other_id);
						if (!other || other == e)
						{
							continue;
						}
						bool named = false;
						for (const auto& [k, v] : other->pairs)
						{
							named = named || (k == "targetname" && v == target);
						}
						const vec3 to = other->origin() - out.origin;
						const double distance = length(to);
						if (named && distance > 1e-3)
						{
							forward[0] = static_cast<float>(to.x / distance);
							forward[1] = static_cast<float>(to.y / distance);
							forward[2] = static_cast<float>(to.z / distance);
							// a circle of 64 units around the target, as radiant draws it
							outer = static_cast<float>(2.0 * std::atan(64.0 / distance) * 180.0 / 3.14159265358979);
							aimed = true;
							break;
						}
					}
				}
				else if (!out.value("angles").empty())
				{
					const vec3 angles = e->angles();
					angles_forward(static_cast<float>(angles.x), static_cast<float>(angles.y), forward);
					aimed = true;
				}
				const bool has_fov = parse_floats(out.value("fov_outer", ""), &outer, 1) && outer > 0;
				parse_floats(out.value("fov_inner", ""), &inner, 1);
				parse_floats(out.value("exponent", ""), &exponent, 1);
				// PRIMARY_SCRIPTABLE ( 4 ) makes a light a primary light by itself: a spot when it has a cone or points somewhere
				const bool scriptable = (spawnflags & 4) != 0;
				const bool cone_given = has_fov || (aimed && outer > 0);
				const bool primary_omni = out.classname == "light_omni" || (out.classname == "light" && ((spawnflags & 1) != 0 || (scriptable && !(spawnflags & 2) && !cone_given)));
				const bool primary_spot = !primary_omni && (out.classname == "light_spot" || (spawnflags & 2) != 0 || scriptable);
				const bool spot = primary_spot || (!primary_omni && cone_given);
				if (spot || primary_omni)
				{
					if (outer <= 0)
					{
						// An omni light lights all around whatever its cone is - but its SHADOW MAP is the cone: the game's
						// omni shaders ( lm_hsm_omni_.., read from the byte code ) take `lerp( primary lightmap, shadow map,
						// cone )` as the light's visibility, with the cone 1 inside the inner angle and 0 outside the outer
						// one. So what moves throws a shadow only inside the cone, at full strength inside the inner angle
						// ( seen in the client, seventh round ). The default is the one of the game's compiler: of the 279
						// omni primary lights of the shipped maps 114 have 120 / 60 degrees, straight down ( the next most
						// frequent: 90 / 60 ). A mapper who wants full shadows further out sets "fov_inner".
						outer = primary_omni ? 120.0f : 90.0f;
						inner = primary_omni && inner <= 0 ? OMNI_DEFAULT_FOV_INNER : inner;
					}
					outer = std::min(outer, 179.0f);
					inner = std::clamp(inner, 0.0f, outer);
					light.type = spot ? 2 : 3;
					light.cone = spot;
					light.cos_half_fov_outer = std::cos(outer * 0.5f * 3.14159265f / 180.0f);
					light.cos_half_fov_inner = std::cos(inner * 0.5f * 3.14159265f / 180.0f);
					light.exponent = std::clamp(static_cast<int>(exponent), 0, 255);
					for (int k = 0; k < 3; k++)
					{
						light.dir[k] = -forward[k];
					}
				}
				else
				{
					light.dir[0] = light.dir[1] = 0;
					light.dir[2] = 1;
					light.cos_half_fov_outer = 0.5f;
					light.cos_half_fov_inner = 0.866f;
				}
				// PRIMARY_NOSHADOWMAP, and a cone too wide for one shadow map ( radiant: "implied by fov_outer > 120" )
				light.can_shadow = !(spawnflags & 8) && outer <= 120.0f;
				light.primary = primary_omni || primary_spot;
				rep.lights++;
				if (light.primary && w.lights.size() >= 255)
				{
					rep.warnings.push_back("light: more than 253 primary lights; the light at " + std::string(out.value("origin")) + " is baked only");
					light.primary = false;
				}
				if (light.primary && scriptable)
				{
					// A light a script can switch, dim, colour, turn and move ( setLightIntensity, setLightColor .. ) stays an
					// entity, as in the shipped maps ( mp_highrise, mp_cargoship_sh: classname light_spot, spawnflags 4 ):
					// its first key "pl#" is the index of its primary light, which is how the server ties the two together.
					// "maxturn" ( degrees ) and "maxmove" ( units ) are how far a script may take it from where it was
					// compiled; the light's baked visibility only holds within them.
					float turn = 0, move = 0;
					parse_floats(out.value("maxturn", ""), &turn, 1);
					parse_floats(out.value("maxmove", ""), &move, 1);
					light.scriptable = true;
					light.rotation_limit = std::cos(std::clamp(turn, 0.0f, 180.0f) * 3.14159265f / 180.0f);
					light.translation_limit = std::max(move, 0.0f);
					map_entity kept;
					kept.classname = light.type == 2 ? "light_spot" : "light_omni";
					kept.origin = out.origin;
					kept.pairs.emplace_back("pl#", format("%zu", w.lights.size()));
					for (const auto& [k, v] : out.pairs)
					{
						if (k != "pl#")
						{
							kept.pairs.emplace_back(k, k == "classname" ? kept.classname : v);
						}
					}
					w.lights.push_back(light);
					w.entities.push_back(std::move(kept));
					continue;
				}
				(light.primary ? w.lights : w.bake_lights).push_back(light);
				continue;		// not part of the entity string
			}

			for (const edit::object_id child : e->children)
			{
				if (doc.kind(child) == edit::object_kind::patch)
				{
					rep.patches++;
					// a patch ( terrain mesh or curve ): its tessellation is drawn, and its triangles are the clip map's
					// terrain ( clip_build.cpp )
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
					for (const edit::mesh_vertex& v : mesh.vertices)
					{
						w.patch_bounds.add(vec3{ v.position[0], v.position[1], v.position[2] });
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
				b.weapon_clip = source->has_content("weaponClip");
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
				// a static model of the world: GfxWorld draw instance + clip map entry; no entity
				map_model model;
				model.name = std::string(out.value("model"));
				model.origin[0] = static_cast<float>(out.origin.x);
				model.origin[1] = static_cast<float>(out.origin.y);
				model.origin[2] = static_cast<float>(out.origin.z);
				parse_floats(out.value("modelscale", "1"), &model.scale, 1);
				if (!(model.scale > 0))
				{
					model.scale = 1.0f;
				}
				const vec3 angles = e->angles();
				const double p = angles.x * 3.14159265358979 / 180.0, y = angles.y * 3.14159265358979 / 180.0, r = angles.z * 3.14159265358979 / 180.0;
				const double sp = std::sin(p), cp = std::cos(p), sy = std::sin(y), cy = std::cos(y), sr = std::sin(r), cr = std::cos(r);
				const double axis[3][3] = { { cp * cy, cp * sy, -sp }, { sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp },
					{ cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp } };		// forward, left, up
				for (int a = 0; a < 3; a++)
				{
					for (int k = 0; k < 3; k++)
					{
						model.axis[a][k] = static_cast<float>(axis[a][k]);
					}
				}
				if (!model.name.empty())
				{
					w.models.push_back(std::move(model));
					rep.static_models++;
				}
				continue;
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

		// the sun, the sky and the ambient light, from the worldspawn keys the shipped maps carry ( README, "Light keys" )
		const map_entity& ws = w.entities[0];
		float direction[3] = { -50, 40, 0 }, color[3] = { 1, 0.95f, 0.85f }, sunlight = 1.3f, ambient = 0.1f, ambient_color[3] = { 1, 1, 1 },
			sky_color[3] = { 0.8f, 0.9f, 1.0f }, skylight = 0.4f;
		parse_floats(ws.value("sundirection", ""), direction, 3);
		parse_floats(ws.value("suncolor", ""), color, 3);
		parse_floats(ws.value("sunlight", ""), &sunlight, 1);
		parse_floats(ws.value("ambient", ""), &ambient, 1);
		parse_floats(ws.value("_color", ""), ambient_color, 3);
		parse_floats(ws.value("skycolor", ""), sky_color, 3);
		parse_floats(ws.value("skylight", ""), &skylight, 1);
		// how strong bounced light is ( the shipped maps: 0.8 .. 1.7, written both ways )
		if (!parse_floats(ws.value("radiosityscale", ""), &w.radiosity_scale, 1))
		{
			parse_floats(ws.value("radiosityScale", ""), &w.radiosity_scale, 1);
		}
		w.radiosity_scale = std::clamp(w.radiosity_scale, 0.1f, 4.0f);
		normalize_color(color);
		normalize_color(ambient_color);
		normalize_color(sky_color);
		sunlight = std::max(sunlight, 0.0f);
		ambient = std::max(ambient, 0.0f);
		skylight = std::max(skylight, 0.0f);
		angles_forward(direction[0], direction[1], w.sun_dir);
		for (int k = 0; k < 3; k++)
		{
			w.sun_color[k] = color[k] * sunlight;
			w.ambient[k] = ambient_color[k] * ambient + sky_color[k] * skylight * SKY_SCALE;
			w.ambient_light[k] = ambient_color[k] * ambient;
			w.sky_light[k] = sky_color[k] * skylight * SKY_SCALE;
			w.lights[1].color[k] = w.sun_color[k];
			w.lights[1].dir[k] = w.sun_dir[k];
		}
		rep.notes.push_back(format("light keys: sun ( %.2f %.2f %.2f ) from ( %.3f %.3f %.3f ), ambient ( %.3f %.3f %.3f ), sky ( %.2f %.2f %.2f ); %zu light entities, %zu of them primary",
			w.sun_color[0], w.sun_color[1], w.sun_color[2], w.sun_dir[0], w.sun_dir[1], w.sun_dir[2], w.ambient_light[0], w.ambient_light[1], w.ambient_light[2],
			w.sky_light[0], w.sky_light[1], w.sky_light[2], w.bake_lights.size() + w.lights.size() - 2, w.lights.size() - 2));

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
		w.bounds.add(w.patch_bounds);

		// the minimap: a square around the map; the two corner entities the game's compass script asks for
		{
			const float cx = (w.bounds.mins[0] + w.bounds.maxs[0]) * 0.5f, cy = (w.bounds.mins[1] + w.bounds.maxs[1]) * 0.5f;
			float half = std::max(w.bounds.maxs[0] - w.bounds.mins[0], w.bounds.maxs[1] - w.bounds.mins[1]) * 0.5f;
			bool corners = false;
			box3 given;
			for (const map_entity& e : w.entities)
			{
				if (e.value("targetname") == "minimap_corner")
				{
					corners = true;
					given.add(e.origin);
				}
			}
			w.minimap_mins[0] = corners ? given.mins[0] : cx - half;
			w.minimap_mins[1] = corners ? given.mins[1] : cy - half;
			w.minimap_maxs[0] = corners ? given.maxs[0] : cx + half;
			w.minimap_maxs[1] = corners ? given.maxs[1] : cy + half;
			if (!corners && !opts.minimap.empty())
			{
				for (int k = 0; k < 2; k++)
				{
					map_entity corner;
					corner.classname = "script_origin";
					corner.origin = vec3{ k == 0 ? w.minimap_mins[0] : w.minimap_maxs[0], k == 0 ? w.minimap_maxs[1] : w.minimap_mins[1], w.bounds.mins[2] };
					corner.set("classname", "script_origin");
					corner.set("targetname", "minimap_corner");
					corner.set("origin", format("%g %g %g", corner.origin.x, corner.origin.y, corner.origin.z));
					w.entities.push_back(std::move(corner));
				}
				if (w.entities[0].value("northyaw").empty())
				{
					w.entities[0].set("northyaw", "90");		// north is + y: the top of the picture
				}
				rep.notes.push_back(format("minimap: corners added at ( %g %g ) and ( %g %g ), northyaw 90", w.minimap_mins[0], w.minimap_maxs[1], w.minimap_maxs[0],
					w.minimap_mins[1]));
			}
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
