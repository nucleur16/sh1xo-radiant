#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"

// Light: the part of the Entity window for `light` entities and for the light keys of the worldspawn, and the
// shape of a selected light in the views.
//
// What a key means is what the map compiler makes of it ( codol_bsp map_input.cpp, README "Light keys" ):
// read_light() below is that reading, so that the cone the views draw is the cone the bake uses.
//
// Fields that are dragged ( numbers, colours ) change the document while the mouse is down and are one undo
// step: scene::begin_drag / restart_drag / end_drag, as the gizmo does it. Everything else is one transaction.

namespace editor::ui
{
	namespace
	{
		constexpr float PI = 3.14159265f;
		constexpr float DEFAULT_RADIUS = 600.0f;	// codol_bsp DEFAULT_LIGHT_RADIUS

		// as the compiler: a text without enough numbers leaves `out` as it is
		bool parse_floats(const std::string_view text, float* out, const int count)
		{
			const std::string copy(text);
			const char* at = copy.c_str();
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

		std::string number_text(const double value)
		{
			char buffer[48];
			std::snprintf(buffer, sizeof(buffer), "%.3f", value);
			std::string text = buffer;

			while (text.back() == '0')
			{
				text.pop_back();
			}

			if (text.back() == '.')
			{
				text.pop_back();
			}

			return text == "-0" ? "0" : text;
		}

		std::string numbers_text(const float* values, const int count)
		{
			std::string text;
			for (int i = 0; i < count; i++)
			{
				text += (i ? " " : "") + number_text(values[i]);
			}

			return text;
		}

		void angles_forward(const float pitch, const float yaw, float out[3])
		{
			const float p = pitch * PI / 180.0f, y = yaw * PI / 180.0f;
			out[0] = std::cos(p) * std::cos(y);
			out[1] = std::cos(p) * std::sin(y);
			out[2] = -std::sin(p);
		}
	}

	bool is_light_class(const std::string_view classname)
	{
		return classname == "light" || classname == "light_spot" || classname == "light_omni";
	}

	light_info read_light(const edit::document& doc, const edit::entity& e)
	{
		light_info l;
		const auto origin = e.origin();
		l.origin = { static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(origin.z) };

		parse_floats(e.value("_color", "1 1 1"), l.color, 3);
		parse_floats(e.value("intensity", "1"), &l.intensity, 1);
		l.radius = DEFAULT_RADIUS;
		parse_floats(e.value("radius", ""), &l.radius, 1);
		l.radius = std::max(l.radius, 1.0f);

		float flags = 0;
		parse_floats(e.value("spawnflags", "0"), &flags, 1);
		l.flags = static_cast<int>(flags);

		float forward[3] = { 0, 0, -1 };
		float outer = 0, inner = 0, exponent = 0;
		bool aimed = false;

		if (const std::string_view target = e.value("target"); !target.empty())
		{
			l.has_target = true;

			for (const object_id other_id : doc.entities())
			{
				const auto* other = doc.find_entity(other_id);
				if (!other || other == &e || other->value("targetname") != target)
				{
					continue;
				}

				const auto to = other->origin();
				const double dx = to.x - origin.x, dy = to.y - origin.y, dz = to.z - origin.z;
				const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);

				if (distance > 1e-3)
				{
					forward[0] = static_cast<float>(dx / distance);
					forward[1] = static_cast<float>(dy / distance);
					forward[2] = static_cast<float>(dz / distance);
					// a circle of 64 units around the target, as radiant draws it
					outer = static_cast<float>(2.0 * std::atan(64.0 / distance) * 180.0 / 3.14159265358979);
					aimed = true;
					l.target_found = true;
					l.target = { static_cast<float>(to.x), static_cast<float>(to.y), static_cast<float>(to.z) };
					break;
				}
			}
		}
		else if (!e.value("angles").empty())
		{
			const auto angles = e.angles();
			angles_forward(static_cast<float>(angles.x), static_cast<float>(angles.y), forward);
			aimed = true;
		}

		const bool has_fov = parse_floats(e.value("fov_outer", ""), &outer, 1) && outer > 0;
		parse_floats(e.value("fov_inner", ""), &inner, 1);
		parse_floats(e.value("exponent", ""), &exponent, 1);

		// "light_spot" / "light_omni" are the classes the primary lights of the shipped maps have; spawnflags 4
		// ( scriptable ) makes a light a primary light by itself - a spot when it has a cone or points somewhere
		const std::string_view classname = e.classname();
		l.scriptable = (l.flags & 4) != 0;
		const bool cone_given = has_fov || (aimed && outer > 0);
		l.primary_omni = classname == "light_omni" || (classname == "light" && ((l.flags & 1) != 0 || (l.scriptable && !(l.flags & 2) && !cone_given)));
		l.primary_spot = !l.primary_omni && (classname == "light_spot" || (l.flags & 2) != 0 || l.scriptable);
		l.spot = l.primary_spot || (!l.primary_omni && cone_given);

		if (l.spot || l.primary_omni)
		{
			if (outer <= 0)
			{
				outer = l.primary_omni ? 120.0f : 90.0f;
				inner = l.primary_omni && inner <= 0 ? 60.0f : inner;		// codol_bsp OMNI_DEFAULT_FOV_INNER
			}

			outer = std::min(outer, 179.0f);
			inner = std::clamp(inner, 0.0f, outer);
		}

		l.outer = outer;
		l.inner = inner;
		l.exponent = std::clamp(static_cast<int>(exponent), 0, 255);
		l.forward = { forward[0], forward[1], forward[2] };
		l.can_shadow = !(l.flags & 8) && outer <= 120.0f;
		return l;
	}

	// ---- the shape in the views --------------------------------------------------------------------------------

	namespace
	{
		void add_line(std::vector<line_vertex>& out, const float3& a, const float3& b, const DWORD color)
		{
			out.push_back({ a.x, a.y, a.z, color });
			out.push_back({ b.x, b.y, b.z, color });
		}

		// circle around `centre` in the plane of the unit vectors u, v
		void add_circle(std::vector<line_vertex>& out, const float3& centre, const float3& u, const float3& v, const float radius, const DWORD color, const int segments = 48)
		{
			float3 last{};

			for (int i = 0; i <= segments; i++)
			{
				const float t = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
				const float c = std::cos(t) * radius, s = std::sin(t) * radius;
				const float3 p{ centre.x + u.x * c + v.x * s, centre.y + u.y * c + v.y * s, centre.z + u.z * c + v.z * s };

				if (i)
				{
					add_line(out, last, p, color);
				}

				last = p;
			}
		}

		// two unit vectors at right angles to `f`
		void basis(const float3& f, float3& u, float3& v)
		{
			const float3 up = std::fabs(f.z) > 0.99f ? float3{ 1, 0, 0 } : float3{ 0, 0, 1 };
			u = { f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x };
			const float len = std::sqrt(u.x * u.x + u.y * u.y + u.z * u.z);
			u = { u.x / len, u.y / len, u.z / len };
			v = { f.y * u.z - f.z * u.y, f.z * u.x - f.x * u.z, f.x * u.y - f.y * u.x };
		}

		// the rim of a cone of `degrees` ( whole angle ) at the light's radius, with `spokes` lines from the light
		void add_cone(std::vector<line_vertex>& out, const light_info& l, const float degrees, const DWORD color, const int spokes)
		{
			float3 u, v;
			basis(l.forward, u, v);

			const float half = degrees * 0.5f * PI / 180.0f;
			const float along = std::cos(half) * l.radius, across = std::sin(half) * l.radius;
			const float3 centre{ l.origin.x + l.forward.x * along, l.origin.y + l.forward.y * along, l.origin.z + l.forward.z * along };
			add_circle(out, centre, u, v, across, color);

			for (int i = 0; i < spokes; i++)
			{
				const float t = 2.0f * PI * static_cast<float>(i) / static_cast<float>(spokes);
				const float c = std::cos(t) * across, s = std::sin(t) * across;
				add_line(out, l.origin, { centre.x + u.x * c + v.x * s, centre.y + u.y * c + v.y * s, centre.z + u.z * c + v.z * s }, color);
			}
		}

		DWORD light_color(const light_info& l, const DWORD alpha)
		{
			// the hue, as the compiler takes it: the largest part counts as 1
			const float top = std::max({ l.color[0], l.color[1], l.color[2], 1e-6f });
			const auto part = [&](const int i) { return static_cast<DWORD>(std::clamp(l.color[i] / top, 0.0f, 1.0f) * 215.0f + 40.0f); };
			return (alpha << 24) | (part(0) << 16) | (part(1) << 8) | part(2);
		}
	}

	void light_shape(const light_info& l, std::vector<line_vertex>& out)
	{
		const DWORD reach = light_color(l, 0xD0), faint = light_color(l, 0x70);

		if (l.spot)
		{
			// the cone the bake and the game's spot shader use, cut off at the radius; the inner cone where the fade begins
			add_cone(out, l, l.outer, reach, 8);

			if (l.inner > 0.5f && l.inner < l.outer - 0.5f)
			{
				add_cone(out, l, l.inner, faint, 0);
			}

			const float3 tip{ l.origin.x + l.forward.x * l.radius, l.origin.y + l.forward.y * l.radius, l.origin.z + l.forward.z * l.radius };
			add_line(out, l.origin, tip, faint);

			// where the light ends: the three circles, faint
			add_circle(out, l.origin, { 1, 0, 0 }, { 0, 1, 0 }, l.radius, faint & 0x50FFFFFFu);
		}
		else
		{
			add_circle(out, l.origin, { 1, 0, 0 }, { 0, 1, 0 }, l.radius, reach);
			add_circle(out, l.origin, { 1, 0, 0 }, { 0, 0, 1 }, l.radius, reach);
			add_circle(out, l.origin, { 0, 1, 0 }, { 0, 0, 1 }, l.radius, reach);
			// down to a quarter at half the radius
			add_circle(out, l.origin, { 1, 0, 0 }, { 0, 1, 0 }, l.radius * 0.5f, faint);
		}
	}

	const std::vector<line_vertex>& selected_light_lines()
	{
		static std::vector<line_vertex> lines;
		static std::uint64_t serial = ~0ull, change = ~0ull;

		auto& s = get_scene();
		const auto& doc = s.doc;

		// the selection cache changes with every edit of something selected; a target that moved without being
		// selected shows with the document's change counter
		if (serial == s.selected_serial && change == doc.change_counter())
		{
			return lines;
		}

		serial = s.selected_serial;
		change = doc.change_counter();
		lines.clear();

		if (!s.open)
		{
			return lines;
		}

		for (const object_id id : doc.selected_entities())
		{
			if (const auto* e = doc.find_entity(id); e && is_light_class(e->classname()))
			{
				light_shape(read_light(doc, *e), lines);
			}
		}

		return lines;
	}

	// ---- the panel ---------------------------------------------------------------------------------------------

	namespace
	{
		ImGuiID g_live = 0;		// the field that is being dragged: its transaction is open

		void end_live()
		{
			if (g_live)
			{
				get_scene().end_drag();
				g_live = 0;
			}
		}

		// a change of a field that is dragged: the document gets the value now, the undo step ends with the drag
		void live(const std::vector<object_id>& entities, const ImGuiID id, const char* name, const std::function<bool(object_id)>& change)
		{
			auto& s = get_scene();

			if (g_live != id || !s.dragging())
			{
				end_live();
				s.begin_drag(name);
				g_live = id;
			}
			else
			{
				s.restart_drag();
			}

			for (const object_id entity : entities)
			{
				change(entity);
			}
		}

		void once(const std::vector<object_id>& entities, const char* name, const std::function<bool(object_id)>& change)
		{
			end_live();

			auto& doc = get_scene().doc;
			bool ok = false;

			doc.begin_transaction(name);
			for (const object_id entity : entities)
			{
				ok |= change(entity);
			}

			doc.end_transaction();
			report(ok, name);
		}

		struct key_state
		{
			std::string value;		// of the first entity that has the key
			bool present = false;	// any entity has it
			bool same = true;		// all entities agree
		};

		key_state key_of(const std::vector<object_id>& entities, const char* key)
		{
			const auto& doc = get_scene().doc;
			key_state state;
			bool first = true;
			std::string first_value;

			for (const object_id id : entities)
			{
				const auto* e = doc.find_entity(id);
				const auto* text = e ? e->find(key) : nullptr;
				const std::string value = text ? *text : std::string();

				if (text && !state.present)
				{
					state.present = true;
					state.value = value;
				}

				if (first)
				{
					first_value = value;
					first = false;
				}
				else
				{
					state.same &= value == first_value;
				}
			}

			return state;
		}

		void row_label(const char* label, const char* tip)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);

			if (tip && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", tip);
			}

			ImGui::TableNextColumn();
		}

		// behind a field: "default" while no entity has the key, a button that takes the key away otherwise
		void key_tail(const std::vector<object_id>& entities, const char* key, const key_state& state)
		{
			ImGui::SameLine();

			if (!state.present)
			{
				ImGui::TextDisabled("default");
				return;
			}

			if (!state.same)
			{
				ImGui::TextDisabled("differ");
				ImGui::SameLine();
			}

			ImGui::PushID(key);

			if (ImGui::SmallButton("x"))
			{
				once(entities, "Remove key", [&](const object_id id)
				{
					auto& doc = get_scene().doc;
					return doc.find_entity(id)->find(key) && doc.remove_key(id, key);
				});
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Remove \"%s\": the compiler's default applies", key);
			}

			ImGui::PopID();
		}

		constexpr float FIELD = 150.0f;

		bool float_row(const std::vector<object_id>& entities, const char* label, const char* key, const float fallback, const float speed, const float low, const float high,
			const char* format, const char* tip)
		{
			const auto state = key_of(entities, key);
			float value = fallback;
			parse_floats(state.value, &value, 1);

			row_label(label, tip);
			ImGui::SetNextItemWidth(FIELD);
			const std::string id = std::string("##") + key;
			const bool changed = ImGui::DragFloat(id.c_str(), &value, speed, low, high, format, ImGuiSliderFlags_AlwaysClamp);

			if (changed)
			{
				const std::string text = number_text(value);
				live(entities, ImGui::GetID(id.c_str()), label, [&](const object_id entity) { return get_scene().doc.set_key(entity, key, text); });
			}

			key_tail(entities, key, state);
			return changed;
		}

		bool color_row(const std::vector<object_id>& entities, const char* label, const char* key, const float r, const float g, const float b, const char* tip)
		{
			const auto state = key_of(entities, key);
			float value[3] = { r, g, b };
			parse_floats(state.value, value, 3);

			row_label(label, tip);
			// the swatch opens the picker ( wheel, numbers ); the numbers next to it are the key as it is written
			const std::string id = std::string("##") + key;
			const bool changed = ImGui::ColorEdit3(id.c_str(), value, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
			ImGui::SameLine();
			ImGui::Text("%s", numbers_text(value, 3).c_str());

			if (changed)
			{
				for (float& part : value)
				{
					part = std::clamp(part, 0.0f, 1.0f);
				}

				const std::string text = numbers_text(value, 3);
				live(entities, ImGui::GetID(id.c_str()), label, [&](const object_id entity) { return get_scene().doc.set_key(entity, key, text); });
			}

			key_tail(entities, key, state);
			return changed;
		}

		// pitch and yaw of an "angles" like key; the third number is kept
		bool direction_row(const std::vector<object_id>& entities, const char* label, const char* key, const float pitch, const float yaw, const char* tip)
		{
			const auto state = key_of(entities, key);
			float value[3] = { pitch, yaw, 0.0f };
			parse_floats(state.value, value, 3) || parse_floats(state.value, value, 2);

			row_label(label, tip);
			ImGui::SetNextItemWidth(FIELD);
			const std::string id = std::string("##") + key;
			const bool changed = ImGui::DragFloat2(id.c_str(), value, 0.5f, -360.0f, 360.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);

			if (changed)
			{
				const std::string text = numbers_text(value, 3);
				live(entities, ImGui::GetID(id.c_str()), label, [&](const object_id entity) { return get_scene().doc.set_key(entity, key, text); });
			}

			key_tail(entities, key, state);
			return changed;
		}

		bool begin_rows(const char* id)
		{
			if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
			{
				return false;
			}

			ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 112.0f);
			ImGui::TableSetupColumn("field", ImGuiTableColumnFlags_WidthStretch);
			return true;
		}

		// the sun seen from above ( where it stands ) and from the side ( how high )
		void sun_compass(const float pitch, const float yaw)
		{
			const float size = 56.0f;
			const ImVec2 at = ImGui::GetCursorScreenPos();
			auto* dl = ImGui::GetWindowDrawList();
			const ImU32 line = IM_COL32(150, 154, 162, 255), sun = IM_COL32(255, 214, 96, 255);

			float to_sun[3];
			angles_forward(pitch, yaw, to_sun);

			// top view: north ( +y ) up, the dot is where the sun stands
			const ImVec2 c1(at.x + size * 0.5f, at.y + size * 0.5f);
			const float flat = std::sqrt(to_sun[0] * to_sun[0] + to_sun[1] * to_sun[1]);
			dl->AddCircle(c1, size * 0.5f - 2.0f, line, 32);
			dl->AddLine(c1, ImVec2(c1.x + to_sun[0] * (size * 0.5f - 4.0f), c1.y - to_sun[1] * (size * 0.5f - 4.0f)), sun, 2.0f);
			dl->AddCircleFilled(ImVec2(c1.x + to_sun[0] * (size * 0.5f - 4.0f), c1.y - to_sun[1] * (size * 0.5f - 4.0f)), 3.5f, sun);

			// side view: the ground, the sun above it
			const ImVec2 c2(at.x + size * 1.5f + 14.0f, at.y + size - 6.0f);
			dl->AddLine(ImVec2(c2.x - size * 0.5f, c2.y), ImVec2(c2.x + size * 0.5f, c2.y), line);
			const ImVec2 tip(c2.x + flat * (size * 0.5f - 4.0f), c2.y - to_sun[2] * (size - 12.0f));
			dl->AddLine(c2, tip, sun, 2.0f);
			dl->AddCircleFilled(tip, 3.5f, sun);

			ImGui::Dummy(ImVec2(size * 2.0f + 14.0f, size));

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Left: from above, +y up - where the sun stands. Right: from the side - how high it stands.");
			}
		}

		void draw_world_light(const std::vector<object_id>& entities)
		{
			const auto& doc = get_scene().doc;
			const auto* world = doc.find_entity(entities[0]);

			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("What the light bake of File > Build map reads. A key that is not set has the default.");
			ImGui::PopTextWrapPos();

			if (begin_rows("##world_light"))
			{
				float_row(entities, "Sun", "sunlight", 1.3f, 0.01f, 0.0f, 8.0f, "%.2f", "sunlight: brightness of the sun; the shipped maps have 1.15 .. 1.6. 0: no sun.");
				color_row(entities, "Sun colour", "suncolor", 1.0f, 0.95f, 0.85f, "suncolor: the hue of the sun ( its largest part counts as 1 )");
				direction_row(entities, "Sun direction", "sundirection", -50.0f, 40.0f,
					"sundirection: pitch and yaw towards the sun. Pitch -90 is straight overhead, 0 the horizon; yaw 0 is +x, 90 is +y.");
				float_row(entities, "Sky light", "skylight", 0.4f, 0.005f, 0.0f, 4.0f, "%.2f", "skylight: light of the sky dome; a surface that sees the whole sky receives half of it");
				color_row(entities, "Sky colour", "skycolor", 0.8f, 0.9f, 1.0f, "skycolor: the hue of the sky light");
				float_row(entities, "Ambient", "ambient", 0.1f, 0.002f, 0.0f, 2.0f, "%.3f", "ambient: light that is everywhere, also in a closed room; the shipped maps have 0.01 .. 0.2");
				color_row(entities, "Ambient colour", "_color", 1.0f, 1.0f, 1.0f, "_color: the hue of the ambient light");
				ImGui::EndTable();
			}

			float direction[3] = { -50.0f, 40.0f, 0.0f };
			parse_floats(world->value("sundirection"), direction, 3) || parse_floats(world->value("sundirection"), direction, 2);
			sun_compass(direction[0], direction[1]);

			static const char* const keys[] = { "sunlight", "suncolor", "sundirection", "skylight", "skycolor", "ambient", "_color" };
			bool any = false;
			for (const char* key : keys)
			{
				any |= world->find(key) != nullptr;
			}

			ImGui::SameLine();
			ImGui::BeginDisabled(!any);

			if (ImGui::Button("Defaults"))
			{
				once(entities, "Light defaults", [&](const object_id id)
				{
					auto& d = get_scene().doc;
					bool ok = false;

					for (const char* key : keys)
					{
						ok |= d.find_entity(id)->find(key) && d.remove_key(id, key);
					}

					return ok;
				});
			}

			ImGui::EndDisabled();

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("Removes the light keys: a sun of 1.3 from -50 40 0, a sky light of 0.4, an ambient light of 0.1");
			}

			// a map of your own is drawn in a stage that was built with the light keys: build it again for the new ones
			if (auto& s = get_scene(); s.stage_outdated() && !g_live)
			{
				ImGui::SameLine();

				if (ImGui::Button("Show in 3D"))
				{
					std::string error;
					if (!s.restage(error))
					{
						ImGui::Toast(ImGuiToastType_Error, "3D view", error.c_str(), 8000);
					}
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The 3D view still shows the sun, sky and ambient light the map had when it was opened.\nBuilds its stage again with these keys ( a second or two ).");
				}
			}
		}

		void set_flags(const std::vector<object_id>& entities, const int clear, const int set)
		{
			once(entities, "Set spawnflags", [&](const object_id id)
			{
				auto& doc = get_scene().doc;
				const auto* e = doc.find_entity(id);
				const int value = (std::atoi(std::string(e->value("spawnflags", "0")).c_str()) & ~clear) | set;
				return value ? doc.set_key(id, "spawnflags", std::to_string(value)) : (e->find("spawnflags") ? doc.remove_key(id, "spawnflags") : false);
			});
		}

		void draw_entity_light(const std::vector<object_id>& entities)
		{
			auto& doc = get_scene().doc;
			const auto* first = doc.find_entity(entities[0]);
			const light_info l = read_light(doc, *first);

			if (begin_rows("##light"))
			{
				color_row(entities, "Colour", "_color", 1.0f, 1.0f, 1.0f, "_color: the hue of the light ( its largest part counts as 1 )");
				float_row(entities, "Intensity", "intensity", 1.0f, 0.01f, 0.0f, 100.0f, "%.2f", "intensity: brightness at the light");
				float_row(entities, "Radius", "radius", DEFAULT_RADIUS, 4.0f, 1.0f, 100000.0f, "%.0f",
					"radius: where the light ends; it is down to a quarter at half of it. The views draw it while the light is selected.");

				// ---- baked or primary
				row_label("Kind", "spawnflags 1 PRIMARY_OMNI, 2 PRIMARY_SPOT: a primary light is drawn by the game at run time - it lights the world, the models and\n"
					"whatever moves, with normal map, highlight and shadow map; the lightmap only holds where it reaches. A surface has one primary light.");
				const int kind = l.primary_omni ? 1 : l.primary_spot ? 2 : 0;
				const bool by_class = first->classname() != "light";

				if (by_class)
				{
					// the classes of the shipped maps' primary lights: the class says what it is
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(kind == 1 ? "primary omni ( its class )" : "primary spot ( its class )");
				}
				else
				{
					static const char* const kinds[] = { "baked ( lightmap only )", "primary omni", "primary spot" };
					ImGui::SetNextItemWidth(FIELD + 60.0f);

					if (ImGui::BeginCombo("##kind", kinds[kind]))
					{
						// a scriptable light is a primary light whatever else is set
						if (ImGui::Selectable(kinds[0], kind == 0, l.scriptable ? ImGuiSelectableFlags_Disabled : 0) && kind != 0) { set_flags(entities, 3, 0); }
						if (ImGui::Selectable(kinds[1], kind == 1) && kind != 1) { set_flags(entities, 3, 1); }
						if (ImGui::Selectable(kinds[2], kind == 2) && kind != 2) { set_flags(entities, 3, 2); }
						ImGui::EndCombo();
					}
				}

				row_label("", nullptr);
				bool no_shadow = (l.flags & 8) != 0;
				ImGui::BeginDisabled(kind == 0);

				if (ImGui::Checkbox("no shadow map", &no_shadow))
				{
					set_flags(entities, 8, no_shadow ? 8 : 0);
				}

				ImGui::EndDisabled();

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("spawnflags 8 PRIMARY_NOSHADOWMAP: the primary light casts no shadows of what moves.\nA cone over 120 degrees has none either.");
				}

				row_label("", nullptr);
				bool scriptable = l.scriptable;

				if (ImGui::Checkbox("scriptable", &scriptable))
				{
					set_flags(entities, 4, scriptable ? 4 : 0);
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("spawnflags 4 PRIMARY_SCRIPTABLE: a primary light that stays an entity, so that a level script can switch, dim,\n"
						"colour, turn and move it ( setLightIntensity, setLightColor .. ). Give it a targetname.");
				}

				if (l.scriptable)
				{
					float_row(entities, "Max turn", "maxturn", 0.0f, 0.5f, 0.0f, 180.0f, "%.0f", "maxturn: how far ( degrees ) a script may turn the light from where it was compiled");
					float_row(entities, "Max move", "maxmove", 0.0f, 1.0f, 0.0f, 100000.0f, "%.0f", "maxmove: how far ( units ) a script may move the light from where it was compiled");
				}

				// ---- the falloff
				{
					row_label("Falloff", "def: the light definition - how the brightness falls from the light to its radius.\nlight_point_linear when the key is not set.");
					const auto state = key_of(entities, "def");
					const std::string current = state.present ? state.value : std::string("light_point_linear");
					ImGui::SetNextItemWidth(FIELD + 60.0f);

					if (ImGui::BeginCombo("##def", current.c_str()))
					{
						// the definitions the lights of the shipped maps name
						std::vector<std::string> names{ "light_point_linear" };

						for (const char* cls : { "light", "light_spot", "light_omni" })
						{
							if (const auto* info = edit::class_catalog::c1().find(cls))
							{
								if (const auto* known = info->find_key("def"))
								{
									for (const auto example : known->examples)
									{
										if (std::find(names.begin(), names.end(), example) == names.end())
										{
											names.emplace_back(example);
										}
									}
								}
							}
						}

						if (std::find(names.begin(), names.end(), current) == names.end())
						{
							names.push_back(current);
						}

						for (const auto& name : names)
						{
							if (ImGui::Selectable(name.c_str(), name == current) && name != current)
							{
								once(entities, "Set falloff", [&](const object_id id) { return doc.set_key(id, "def", name); });
							}
						}

						ImGui::EndCombo();
					}

					key_tail(entities, "def", state);
				}

				// ---- the cone
				if (kind == 0)
				{
					row_label("Cone", "A baked light with fov_outer ( or a target ) is a spot.");
					bool cone = l.spot;
					const bool by_target = l.has_target && l.target_found && !first->find("fov_outer");

					ImGui::BeginDisabled(by_target);

					if (ImGui::Checkbox("spot", &cone))
					{
						once(entities, cone ? "Make spot" : "Remove cone", [&](const object_id id)
						{
							if (cone)
							{
								return doc.set_key(id, "fov_outer", "90");
							}

							bool ok = false;
							for (const char* key : { "fov_outer", "fov_inner", "exponent" })
							{
								ok |= doc.find_entity(id)->find(key) && doc.remove_key(id, key);
							}

							return ok;
						});
					}

					ImGui::EndDisabled();

					if (by_target)
					{
						ImGui::SameLine();
						ImGui::TextDisabled("by its target: 64 units around it");
					}
				}

				if (l.spot)
				{
					const auto outer_state = key_of(entities, "fov_outer");
					float outer = l.outer;
					row_label("Outer angle", "fov_outer: the whole angle of the cone, in degrees");
					ImGui::SetNextItemWidth(FIELD);

					if (ImGui::SliderFloat("##fov_outer", &outer, 1.0f, 179.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp))
					{
						const std::string text = number_text(std::round(outer));
						live(entities, ImGui::GetID("##fov_outer"), "Outer angle", [&](const object_id id) { return doc.set_key(id, "fov_outer", text); });
					}

					key_tail(entities, "fov_outer", outer_state);

					const auto inner_state = key_of(entities, "fov_inner");
					float inner = l.inner;
					row_label("Inner angle", "fov_inner: inside it the light is at full strength; it fades from there to the outer angle");
					ImGui::SetNextItemWidth(FIELD);

					if (ImGui::SliderFloat("##fov_inner", &inner, 0.0f, std::max(l.outer, 1.0f), "%.0f", ImGuiSliderFlags_AlwaysClamp))
					{
						const std::string text = number_text(std::round(inner));
						live(entities, ImGui::GetID("##fov_inner"), "Inner angle", [&](const object_id id) { return doc.set_key(id, "fov_inner", text); });
					}

					key_tail(entities, "fov_inner", inner_state);

					const auto exponent_state = key_of(entities, "exponent");
					int exponent = l.exponent;
					row_label("Exponent", "exponent: the fade from the inner to the outer angle. 0 a hard edge, 1 linear, higher steeper.");
					ImGui::SetNextItemWidth(FIELD);

					if (ImGui::SliderInt("##exponent", &exponent, 0, 16, "%d"))
					{
						const std::string text = std::to_string(std::clamp(exponent, 0, 255));
						live(entities, ImGui::GetID("##exponent"), "Exponent", [&](const object_id id) { return doc.set_key(id, "exponent", text); });
					}

					key_tail(entities, "exponent", exponent_state);

					// ---- where it points
					if (!l.has_target)
					{
						direction_row(entities, "Pitch, yaw", "angles", 90.0f, 0.0f,
							"angles: where the spot points. Pitch 90 is straight down ( the default ), 0 level, -90 up; yaw 0 is +x, 90 is +y.");
					}
				}

				if (l.spot || l.has_target)
				{
					row_label("Target", "target: the spot points at the entity with this targetname; it wins over the angles.\nCtrl+K with the light and another entity selected connects them.");

					static std::string typed;
					static object_id typed_for = edit::no_object;
					static bool typing = false;
					const std::string current(first->value("target"));

					if (!typing || typed_for != entities[0])
					{
						typed = current;
						typed_for = entities[0];
					}

					ImGui::SetNextItemWidth(FIELD - ImGui::GetFrameHeight() - 2.0f);
					if (ImGui::InputTextWithHint("##target", "targetname", &typed))
					{
						typing = true;
					}

					if (ImGui::IsItemDeactivated() && typing)
					{
						typing = false;

						if (typed != current)
						{
							const std::string value = typed;
							once(entities, "Set target", [&](const object_id id)
							{
								return value.empty() ? (doc.find_entity(id)->find("target") && doc.remove_key(id, "target")) : doc.set_key(id, "target", value);
							});
						}
					}

					ImGui::SameLine(0.0f, 2.0f);

					if (ImGui::BeginCombo("##targets", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft))
					{
						if (ImGui::Selectable("( none: along the angles )", current.empty()) && !current.empty())
						{
							once(entities, "Clear target", [&](const object_id id) { return doc.find_entity(id)->find("target") && doc.remove_key(id, "target"); });
						}

						std::vector<std::string> names;
						for (const object_id id : doc.entities())
						{
							if (const auto* e = doc.find_entity(id); e && !e->value("targetname").empty())
							{
								const std::string name(e->value("targetname"));
								if (std::find(names.begin(), names.end(), name) == names.end())
								{
									names.push_back(name);
								}
							}
						}

						std::sort(names.begin(), names.end());

						for (const auto& name : names)
						{
							if (ImGui::Selectable(name.c_str(), name == current) && name != current)
							{
								once(entities, "Set target", [&](const object_id id) { return doc.set_key(id, "target", name); });
							}
						}

						ImGui::EndCombo();
					}

					if (l.has_target && !l.target_found)
					{
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(0.96f, 0.45f, 0.40f, 1.0f), "no such entity");
					}
				}

				ImGui::EndTable();
			}

			// ---- what the compiler makes of it
			std::string summary;

			if (l.primary_omni)
			{
				// the game's omni shaders blend the shadow map in by the light's cone ( codol_bsp/README.md, seventh round )
				char text[240];
				std::snprintf(text, sizeof(text), "Primary omni light: lights all around at run time. Players and models throw shadows "
					"only inside its cone - %.0f degrees, full inside %.0f, looking %s.", l.outer, l.inner,
					l.forward.z < -0.99f ? "straight down" : "where it is aimed");
				summary = text;
			}
			else if (l.primary_spot)
			{
				char text[200];
				std::snprintf(text, sizeof(text), "Primary spot: a cone of %.0f degrees ( full inside %.0f )%s.", l.outer, l.inner,
					l.can_shadow ? ", with shadow map" : (l.flags & 8) ? ", no shadow map" : ", no shadow map: the cone is over 120 degrees");
				summary = text;
			}
			else if (l.spot)
			{
				char text[160];
				std::snprintf(text, sizeof(text), "Baked spot: a cone of %.0f degrees, in the lightmap only.", l.outer);
				summary = text;
			}
			else
			{
				summary = "Baked: in the lightmap and the light grid only.";
			}

			if (l.scriptable)
			{
				summary += " Scriptable: it stays an entity of the map.";
			}

			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%s", summary.c_str());
			ImGui::PopTextWrapPos();
		}
	}

	void finish_live_edits()
	{
		if (g_live && !ImGui::IsAnyItemActive())
		{
			end_live();
		}
	}

	void draw_light_panel(const std::vector<object_id>& entities, const bool world)
	{
		if (entities.empty() || !get_scene().doc.find_entity(entities[0]))
		{
			return;
		}

		ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);

		if (!ImGui::CollapsingHeader(world ? "Light of the map" : "Light"))
		{
			return;
		}

		ImGui::PushID(world ? "world_light" : "entity_light");

		if (world)
		{
			draw_world_light(entities);
		}
		else
		{
			draw_entity_light(entities);
		}

		ImGui::PopID();
		ImGui::Spacing();
	}
}
