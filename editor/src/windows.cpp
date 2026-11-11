#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"

// The Entity window ( keys of the selected entities, with what the class catalog knows about the class ), the
// "Create entity" popup, the Layers window and the Surface inspector.

namespace editor::ui
{
	namespace
	{
		// A text field shows the value of the document; while it is edited it shows what was typed. The edit is
		// committed when the field loses the keyboard ( Enter, Tab, a click elsewhere ) - one undo step, and no
		// half typed key names or coordinates in the document.
		ImGuiID g_edit_id = 0;
		std::string g_edit_text;

		struct completion
		{
			std::vector<std::string_view> items;
		};

		// Tab completes to the longest common start of the candidates
		int complete_callback(ImGuiInputTextCallbackData* data)
		{
			if (data->EventFlag != ImGuiInputTextFlags_CallbackCompletion || !data->UserData)
			{
				return 0;
			}

			const auto* list = static_cast<const completion*>(data->UserData);
			const std::string_view typed(data->Buf, static_cast<std::size_t>(data->BufTextLen));
			std::string common;
			bool first = true;

			for (const auto item : list->items)
			{
				if (item.size() < typed.size() || item.substr(0, typed.size()) != typed)
				{
					continue;
				}

				if (first)
				{
					common = std::string(item);
					first = false;
				}
				else
				{
					std::size_t n = 0;
					while (n < common.size() && n < item.size() && common[n] == item[n])
					{
						n++;
					}

					common.resize(n);
				}
			}

			if (!first && common.size() > typed.size())
			{
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, common.c_str());
			}

			return 0;
		}

		// true when an edit was committed and differs from `current`; `out` is the new text
		bool edit_field(const char* label, const std::string_view current, std::string& out, const completion* candidates = nullptr)
		{
			const ImGuiID id = ImGui::GetID(label);
			std::string buffer = g_edit_id == id ? g_edit_text : std::string(current);

			const bool changed = candidates
				? ImGui::InputText(label, &buffer, ImGuiInputTextFlags_CallbackCompletion, complete_callback, const_cast<completion*>(candidates))
				: ImGui::InputText(label, &buffer);

			if (changed)
			{
				g_edit_id = id;
				g_edit_text = buffer;
			}

			if (ImGui::IsItemDeactivated() && g_edit_id == id)
			{
				out = g_edit_text;
				g_edit_id = 0;
				return out != current;
			}

			return false;
		}

		// what is being typed into a field right now ( to filter its suggestions )
		std::string typed_text(const char* label, const std::string_view current)
		{
			return g_edit_id == ImGui::GetID(label) ? g_edit_text : std::string(current);
		}

		bool is_import_key(const std::string_view key)
		{
			return key.rfind("codol_", 0) == 0;
		}

		void apply_to(const std::vector<object_id>& entities, const char* name, const std::function<bool(object_id)>& change)
		{
			auto& doc = get_scene().doc;
			bool ok = false;

			doc.begin_transaction(name);
			for (const object_id id : entities)
			{
				ok |= change(id);
			}

			doc.end_transaction();
			report(ok, name);
		}

		std::string entity_label(const object_id id)
		{
			const auto* e = get_scene().doc.find_entity(id);
			if (!e)
			{
				return "#" + std::to_string(id);
			}

			std::string text = "#" + std::to_string(id) + "  " + std::string(e->classname());

			if (const auto name = e->value("targetname"); !name.empty())
			{
				text += "  \"" + std::string(name) + "\"";
			}

			return text;
		}
	}

	// ---- class list --------------------------------------------------------------------------------------

	const edit::class_info* class_picker(std::string& filter, const float height, const bool brush_classes_enabled)
	{
		const auto& catalog = edit::class_catalog::c1();
		const edit::class_info* picked = nullptr;
		const auto needle = codol::script::to_lower(filter);

		if (ImGui::BeginChild("##class_list", ImVec2(0.0f, height), true))
		{
			for (const auto& info : catalog.classes())
			{
				if (info.name == "worldspawn" || (!needle.empty() && info.name.find(needle) == std::string_view::npos))
				{
					continue;
				}

				const bool point = info.is_point();
				const bool enabled = point || brush_classes_enabled;
				const std::string label(info.name);

				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(get_scene().class_color(info.name)));

				if (ImGui::Selectable(label.c_str(), false, enabled ? 0 : ImGuiSelectableFlags_Disabled))
				{
					picked = &info;
				}

				ImGui::PopStyleColor();

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("%s\n%d in %d of the %d shipped maps ( %d as point entity, %d with a brush model )%s%.*s%s", label.c_str(), info.count, info.map_count,
						catalog.map_count(), info.point_count, info.brush_count, info.description.empty() ? "" : "\n",
						static_cast<int>(info.description.size()), info.description.data(),
						enabled ? "" : "\nA brush class: select the brushes it is made of first.");
				}

				ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70.0f);
				ImGui::TextDisabled(point ? "%d" : "%d  brush", info.count);
			}
		}

		ImGui::EndChild();
		return picked;
	}

	void draw_create_entity_popup()
	{
		auto& st = get();
		auto& s = get_scene();
		auto& doc = s.doc;

		if (st.open_create_popup)
		{
			st.open_create_popup = false;
			st.create_filter.clear();
			ImGui::OpenPopup("##create_entity");
		}

		ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f));

		if (!ImGui::BeginPopup("##create_entity"))
		{
			return;
		}

		bool has_brush = false;
		for (const object_id id : doc.selection())
		{
			has_brush |= doc.kind(id) != edit::object_kind::entity;
		}

		ImGui::Text("Create entity at %g %g %g", st.create_position.x, st.create_position.y, st.create_position.z);
		ImGui::TextDisabled(has_brush ? "A brush class takes the selected brushes." : "Brush classes need selected brushes.");

		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##create_filter", "class name", &st.create_filter);

		if (const auto* info = class_picker(st.create_filter, 360.0f, has_brush))
		{
			const object_id id = s.create_entity(info->name, st.create_position);
			report(id != edit::no_object, "Create entity");
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	// ---- Entity ----------------------------------------------------------------------------------------------

	void draw_entity_window()
	{
		auto& st = get();
		if (!st.show_entity)
		{
			return;
		}

		auto& s = get_scene();
		auto& doc = s.doc;

		ImGui::SetNextWindowSize(ImVec2(400.0f, 600.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Entity##window", &st.show_entity, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (!s.open)
		{
			ImGui::TextDisabled("No map is open.");
			ImGui::End();
			return;
		}

		// like Radiant: with no entity selected, the worldspawn
		std::vector<object_id> entities = doc.selected_entities();
		const bool world = entities.empty();

		if (world)
		{
			entities.push_back(doc.worldspawn());
		}

		const auto* first = doc.find_entity(entities[0]);
		if (!first)
		{
			ImGui::End();
			return;
		}

		const auto& catalog = edit::class_catalog::c1();

		// ---- classname
		std::string classname(first->classname());
		bool same_class = true;

		for (const object_id id : entities)
		{
			same_class &= doc.find_entity(id)->classname() == classname;
		}

		if (entities.size() > 1)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "%d entities", static_cast<int>(entities.size()));
			ImGui::SameLine();
			ImGui::TextDisabled("changes go to all of them");
		}
		else
		{
			ImGui::TextDisabled(world ? "worldspawn ( no entity is selected )" : "entity #%u", entities[0]);
		}

		static completion class_names;
		if (class_names.items.empty())
		{
			for (const auto& info : catalog.classes())
			{
				class_names.items.push_back(info.name);
			}
		}

		const auto* info = same_class ? catalog.find(classname) : nullptr;

		if (world)
		{
			ImGui::TextUnformatted("worldspawn");
		}
		else
		{
			std::string edited;
			const float arrow = ImGui::GetFrameHeight();

			ImGui::SetNextItemWidth(-arrow - ImGui::GetStyle().ItemInnerSpacing.x - 80.0f);

			if (edit_field("##classname", same_class ? std::string_view(classname) : std::string_view("< different >"), edited, &class_names) && !edited.empty())
			{
				apply_to(entities, "Set classname", [&](const object_id id) { return doc.set_key(id, "classname", edited); });
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Tab completes the name. The list on the right holds every class the shipped maps use.");
			}

			const std::string typed = codol::script::to_lower(typed_text("##classname", classname));

			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			if (ImGui::BeginCombo("##classname_list", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLarge | ImGuiComboFlags_PopupAlignLeft))
			{
				// what was typed narrows the list, unless it is a complete name already
				const bool narrow = !typed.empty() && !catalog.find(typed);

				for (const auto& c : catalog.classes())
				{
					if (c.name == "worldspawn" || (narrow && c.name.find(typed) == std::string_view::npos))
					{
						continue;
					}

					const std::string label(c.name);
					if (ImGui::Selectable(label.c_str(), c.name == classname))
					{
						apply_to(entities, "Set classname", [&](const object_id id) { return doc.set_key(id, "classname", label); });
						g_edit_id = 0;
					}
				}

				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::TextDisabled(info ? "%d used" : "new class", info ? info->count : 0);
		}

		if (info && !info->description.empty())
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%.*s", static_cast<int>(info->description.size()), info->description.data());
			ImGui::PopTextWrapPos();
		}

		ImGui::Spacing();

		// ---- keys: the union over the selected entities, in the order of the first
		std::vector<std::string> keys;
		for (const object_id id : entities)
		{
			for (const auto& [key, value] : doc.find_entity(id)->pairs)
			{
				if (key != "classname" && std::find(keys.begin(), keys.end(), key) == keys.end())
				{
					keys.push_back(key);
				}
			}
		}

		if (ImGui::BeginTable("##entity_keys", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthStretch, 0.38f);
			ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.62f);
			ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 54.0f);

			for (std::size_t row = 0; row < keys.size(); row++)
			{
				const std::string& key = keys[row];

				// the value when all entities agree
				std::string value;
				bool same = true, have = false;

				for (const object_id id : entities)
				{
					const auto* text = doc.find_entity(id)->find(key);
					const std::string v = text ? *text : "";

					if (!have)
					{
						value = v;
						have = true;
					}
					else
					{
						same &= v == value;
					}
				}

				ImGui::PushID(static_cast<int>(row));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				// keys that tie the entity to the zone are shown, not edited
				if (is_import_key(key))
				{
					ImGui::AlignTextToFramePadding();
					ImGui::TextDisabled("%s", key.c_str());
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					ImGui::TextDisabled("%s", same ? value.c_str() : "< different >");

					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Written by the import: it ties the entity to the zone ( the export needs it ).");
					}

					ImGui::TableNextColumn();
					ImGui::PopID();
					continue;
				}

				std::string edited;

				ImGui::SetNextItemWidth(-1.0f);
				if (edit_field("##key", key, edited) && !edited.empty())
				{
					apply_to(entities, "Rename key", [&](const object_id id) { return doc.find_entity(id)->find(key) && doc.rename_key(id, key, edited); });
				}

				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-1.0f);

				if (edit_field("##value", same ? std::string_view(value) : std::string_view("< different >"), edited))
				{
					apply_to(entities, "Set key", [&](const object_id id) { return doc.set_key(id, key, edited); });
				}

				ImGui::TableNextColumn();

				// values the shipped maps use for this key of this class
				const auto* known = info ? info->find_key(key) : nullptr;

				if (known && !known->examples.empty())
				{
					if (ImGui::BeginCombo("##examples", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft))
					{
						ImGui::TextDisabled("used in the shipped maps");

						for (const auto example : known->examples)
						{
							const std::string text(example);
							if (ImGui::Selectable(text.c_str(), text == value))
							{
								apply_to(entities, "Set key", [&](const object_id id) { return doc.set_key(id, key, text); });
							}
						}

						ImGui::EndCombo();
					}

					ImGui::SameLine(0.0f, 2.0f);
				}
				else
				{
					ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), 0.0f));
					ImGui::SameLine(0.0f, 2.0f);
				}

				if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
				{
					apply_to(entities, "Remove key", [&](const object_id id) { return doc.find_entity(id)->find(key) && doc.remove_key(id, key); });
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Remove the key");
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		// ---- add a key: the keys the class has in the shipped maps, with a value that is used there
		{
			static std::string new_key, new_value;
			static completion key_names;

			key_names.items.clear();
			if (info)
			{
				for (const auto& k : info->keys)
				{
					if (std::find(keys.begin(), keys.end(), k.name) == keys.end())
					{
						key_names.items.push_back(k.name);
					}
				}
			}

			const float arrow = ImGui::GetFrameHeight();
			const float width = ImGui::GetContentRegionAvail().x;

			ImGui::SetNextItemWidth(width * 0.36f - arrow);
			ImGui::InputTextWithHint("##new_key", "new key", &new_key, ImGuiInputTextFlags_CallbackCompletion, complete_callback, &key_names);
			ImGui::SameLine(0.0f, 2.0f);

			if (ImGui::BeginCombo("##new_key_list", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLarge | ImGuiComboFlags_PopupAlignLeft))
			{
				if (!info || key_names.items.empty())
				{
					ImGui::TextDisabled(info ? "every key the class has in the shipped maps is set" : "the class is not in the shipped maps: no known keys");
				}
				else
				{
					ImGui::TextDisabled("keys of %s in the shipped maps", classname.c_str());
				}

				for (const auto name : key_names.items)
				{
					const auto* k = info->find_key(name);
					const std::string label = std::string(name) + "   ( " + std::to_string(k ? k->count : 0) + " of " + std::to_string(info->count) + " )";

					if (ImGui::Selectable(label.c_str()))
					{
						new_key = std::string(name);
						new_value = k && !k->examples.empty() ? std::string(k->examples[0]) : "";
					}
				}

				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(width * 0.64f - 70.0f - ImGui::GetStyle().ItemSpacing.x * 2.0f);
			const bool enter = ImGui::InputTextWithHint("##new_value", "value", &new_value, ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::SameLine();

			if ((ImGui::Button("Add", ImVec2(-1.0f, 0.0f)) || enter) && !new_key.empty())
			{
				const std::string key = new_key, value = new_value;
				apply_to(entities, "Set key", [&](const object_id id) { return doc.set_key(id, key, value); });
				new_key.clear();
				new_value.clear();
			}
		}

		// ---- spawnflags: the bits, and the values the shipped maps have
		const bool has_flags = std::find(keys.begin(), keys.end(), "spawnflags") != keys.end();

		if (has_flags || (info && !info->spawnflags.empty() && !world))
		{
			ImGui::Spacing();
			ImGui::Separator();

			const int current = std::atoi(std::string(first->value("spawnflags", "0")).c_str());
			int all = current, bits = 4;

			if (info)
			{
				for (const auto& f : info->spawnflags)
				{
					all |= f.value;
				}
			}

			while (bits < 24 && (all >> bits) != 0)
			{
				bits++;
			}

			ImGui::TextUnformatted("spawnflags");
			ImGui::SameLine();
			ImGui::TextDisabled("%d", current);

			for (int bit = 0; bit < bits; bit++)
			{
				bool on = (current >> bit) & 1;
				char label[24];
				std::snprintf(label, sizeof(label), "%d##flag%d", 1 << bit, bit);

				if (bit % 4)
				{
					ImGui::SameLine(static_cast<float>(bit % 4) * 92.0f);
				}

				if (ImGui::Checkbox(label, &on))
				{
					const int value = on ? (current | (1 << bit)) : (current & ~(1 << bit));
					apply_to(entities, "Set spawnflags", [&](const object_id id)
					{
						return value ? doc.set_key(id, "spawnflags", std::to_string(value)) : (doc.find_entity(id)->find("spawnflags") ? doc.remove_key(id, "spawnflags") : true);
					});
				}
			}

			if (info && !info->spawnflags.empty())
			{
				std::string seen = "in the shipped maps: ";
				for (std::size_t i = 0; i < info->spawnflags.size() && i < 8; i++)
				{
					seen += (i ? ", " : "") + std::to_string(info->spawnflags[i].value) + " ( " + std::to_string(info->spawnflags[i].count) + "x )";
				}

				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextDisabled("%s", seen.c_str());
				ImGui::PopTextWrapPos();
			}
		}

		// ---- connections
		if (!world)
		{
			ImGui::Spacing();
			ImGui::Separator();

			const bool pair = entities.size() == 2;

			if (!pair)
			{
				ImGui::BeginDisabled();
			}

			if (ImGui::Button("Connect"))
			{
				connect_selected();
			}

			if (!pair)
			{
				ImGui::EndDisabled();
			}

			ImGui::SameLine();

			if (pair)
			{
				ImGui::TextDisabled("%s  ->  %s   ( Ctrl+K )", entity_label(entities[0]).c_str(), entity_label(entities[1]).c_str());
			}
			else
			{
				ImGui::TextDisabled("select two entities: the first targets the second ( Ctrl+K )");
			}

			object_id go_to = edit::no_object;

			const auto list = [&](const char* title, const std::vector<object_id>& ids)
			{
				if (ids.empty())
				{
					return;
				}

				ImGui::TextDisabled("%s", title);

				for (const object_id id : ids)
				{
					ImGui::PushID(static_cast<int>(id));
					ImGui::Bullet();

					if (ImGui::Selectable(entity_label(id).c_str()))
					{
						go_to = id;
					}

					ImGui::PopID();
				}
			};

			if (entities.size() == 1)
			{
				ImGui::PushID("targets");
				list("targets", doc.targets_of(entities[0]));
				ImGui::PopID();
				ImGui::PushID("targeting");
				list("targeted by", doc.targeting(entities[0]));
				ImGui::PopID();
			}

			if (go_to != edit::no_object)
			{
				doc.select(go_to);
				s.look_at_selection();
			}
		}

		// ---- new entity
		ImGui::Spacing();
		ImGui::Separator();

		if (ImGui::Button("Create entity .."))
		{
			st.create_position = edit::snap_point(s.ray_target(s.cam.origin, s.cam.axes().forward), s.snap ? s.grid : 0.0);
			st.open_create_popup = true;
		}

		ImGui::SameLine();
		ImGui::TextDisabled("where the camera looks; or right click in a view");

		ImGui::End();
	}

	// ---- Layers ----------------------------------------------------------------------------------------------

	void draw_layers_window()
	{
		auto& st = get();
		if (!st.show_layers)
		{
			return;
		}

		auto& s = get_scene();
		auto& doc = s.doc;

		ImGui::SetNextWindowSize(ImVec2(340.0f, 420.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Layers##window", &st.show_layers, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (!s.open)
		{
			ImGui::TextDisabled("No map is open.");
			ImGui::End();
			return;
		}

		static std::string chosen;		// the row that is highlighted
		static std::string name_input;

		// the names are copied: the calls below change the table
		struct row
		{
			std::string name;
			bool active, hidden;
		};

		std::vector<row> rows;
		for (const auto& l : doc.layers())
		{
			rows.push_back({ l.name, l.active, l.hidden });
		}

		if (rows.empty())
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("The map has no layers. Add one to sort its objects.");
			ImGui::PopTextWrapPos();
		}

		const float list_height = std::max(80.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 3.0f - 8.0f);

		if (ImGui::BeginChild("##layer_tree", ImVec2(0.0f, list_height), true))
		{
			for (std::size_t i = 0; i < rows.size(); i++)
			{
				const auto& l = rows[i];

				// "a/b/c" is a child of "a/b"
				const auto depth = static_cast<float>(std::count(l.name.begin(), l.name.end(), '/'));
				const auto slash = l.name.find_last_of('/');
				const std::string leaf = slash == std::string::npos ? l.name : l.name.substr(slash + 1);

				ImGui::PushID(static_cast<int>(i));

				if (depth > 0.0f)
				{
					ImGui::Indent(depth * 18.0f);
				}

				bool visible = !l.hidden;
				if (ImGui::Checkbox("##visible", &visible))
				{
					doc.set_layer_hidden(l.name, !visible);
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Show / hide the objects of the layer and of its children");
				}

				ImGui::SameLine();

				if (ImGui::RadioButton("##active", l.active))
				{
					report(doc.set_active_layer(l.name), "Active layer");
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("The active layer: new objects go here");
				}

				ImGui::SameLine();

				if (ImGui::Selectable(leaf.c_str(), chosen == l.name))
				{
					chosen = l.name;
					name_input = l.name;
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", l.name.c_str());
				}

				if (depth > 0.0f)
				{
					ImGui::Unindent(depth * 18.0f);
				}

				ImGui::PopID();
			}
		}

		ImGui::EndChild();

		const bool has_chosen = std::any_of(rows.begin(), rows.end(), [&](const row& r) { return r.name == chosen; });

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##layer_name", "layer name ( parent/child )", &name_input);

		if (ImGui::Button("Add") && !name_input.empty())
		{
			if (report(doc.add_layer(name_input), "Add layer"))
			{
				chosen = name_input;
			}
		}

		ImGui::SameLine();

		if (!has_chosen)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Rename") && !name_input.empty() && name_input != chosen)
		{
			if (report(doc.rename_layer(chosen, name_input), "Rename layer"))
			{
				chosen = name_input;
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Remove"))
		{
			if (report(doc.remove_layer(chosen), "Remove layer"))
			{
				chosen.clear();
			}
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Its objects ( and those of its children ) move to the parent layer");
		}

		const bool can_move = has_chosen && !doc.selection().empty();

		if (has_chosen && !can_move)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Move the selection to this layer", ImVec2(-1.0f, 0.0f)))
		{
			report(doc.move_selection_to_layer(chosen), "Move to layer");
		}

		if (has_chosen && !can_move)
		{
			ImGui::EndDisabled();
		}

		if (!has_chosen)
		{
			ImGui::EndDisabled();
		}

		ImGui::End();
	}

	// ---- Surface inspector -----------------------------------------------------------------------------------

	void draw_surface_window()
	{
		auto& st = get();
		if (!st.show_surface)
		{
			return;
		}

		auto& s = get_scene();
		auto& doc = s.doc;

		ImGui::SetNextWindowSize(ImVec2(360.0f, 430.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Surface##window", &st.show_surface, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		// the first face the texture functions would change
		edit::tex_def tex;
		bool have = false;
		int faces = static_cast<int>(doc.selected_faces().size());

		if (!doc.selected_faces().empty())
		{
			const auto f = doc.selected_faces()[0];
			if (const auto* b = doc.find_brush(f.brush); b && f.face >= 0 && static_cast<std::size_t>(f.face) < b->faces.size())
			{
				tex = b->faces[static_cast<std::size_t>(f.face)].texture;
				have = true;
			}
		}

		for (const object_id id : doc.selection())
		{
			if (const auto* b = doc.find_brush(id))
			{
				faces += static_cast<int>(b->faces.size());

				if (!have && !b->faces.empty())
				{
					tex = b->faces[0].texture;
					have = true;
				}
			}
			else if (const auto* p = doc.find_patch(id))
			{
				faces++;

				if (!have)
				{
					tex.material = p->material;
					have = true;
				}
			}
		}

		if (!have)
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("Select brushes, or single faces with Shift + Ctrl + click in the 3D view.");
			ImGui::PopTextWrapPos();
			ImGui::End();
			return;
		}

		ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "%d face%s", faces, faces == 1 ? "" : "s");

		// ---- material
		std::string edited;
		ImGui::SetNextItemWidth(-1.0f);

		if (edit_field("##surface_material", tex.material, edited) && !edited.empty())
		{
			report(doc.set_material(edited), "Set material");
		}

		if (s.renderer.loaded())
		{
			auto* thumb = s.renderer.material_thumbnail(tex.material, 96);
			if (!thumb)
			{
				thumb = s.renderer.material_thumbnail("wc/" + tex.material, 96);
			}

			if (thumb)
			{
				ImGui::Image(thumb, ImVec2(72.0f, 72.0f));
				ImGui::SameLine();
			}
		}

		ImGui::BeginGroup();
		const std::string browser(doc.default_material());

		if (ImGui::Button("Apply the browser's material"))
		{
			report(doc.set_material(browser), "Set material");
		}

		ImGui::TextDisabled("%s", browser.c_str());

		if (ImGui::Button("Open the Materials window"))
		{
			st.show_materials = true;
			ImGui::SetWindowFocus("Materials##window");
		}

		ImGui::EndGroup();
		ImGui::Separator();

		// ---- relative
		static float shift_step = 8.0f, rotate_step = 15.0f, scale_step = 2.0f;
		static float fit[2] = { 1.0f, 1.0f };
		const float button = ImGui::GetFrameHeight() * 1.3f;

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Shift");
		ImGui::SameLine(70.0f);
		if (ImGui::Button("<##shift", ImVec2(button, 0.0f))) { report(doc.shift_texture(-shift_step, 0.0), "Shift texture"); }
		ImGui::SameLine(0.0f, 2.0f);
		if (ImGui::Button(">##shift", ImVec2(button, 0.0f))) { report(doc.shift_texture(shift_step, 0.0), "Shift texture"); }
		ImGui::SameLine(0.0f, 2.0f);
		if (ImGui::Button("^##shift", ImVec2(button, 0.0f))) { report(doc.shift_texture(0.0, shift_step), "Shift texture"); }
		ImGui::SameLine(0.0f, 2.0f);
		if (ImGui::Button("v##shift", ImVec2(button, 0.0f))) { report(doc.shift_texture(0.0, -shift_step), "Shift texture"); }
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputFloat("##shift_step", &shift_step, 0.0f, 0.0f, "%.3g texels");

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Scale");
		ImGui::SameLine(70.0f);
		if (ImGui::Button("-##scale", ImVec2(button, 0.0f)) && scale_step > 0.0f) { report(doc.scale_texture(1.0 / scale_step, 1.0 / scale_step), "Scale texture"); }
		ImGui::SameLine(0.0f, 2.0f);
		if (ImGui::Button("+##scale", ImVec2(button, 0.0f)) && scale_step > 0.0f) { report(doc.scale_texture(scale_step, scale_step), "Scale texture"); }
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputFloat("##scale_step", &scale_step, 0.0f, 0.0f, "x %.3g");

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Rotate");
		ImGui::SameLine(70.0f);
		if (ImGui::Button("-##rotate", ImVec2(button, 0.0f))) { report(doc.rotate_texture(-rotate_step), "Rotate texture"); }
		ImGui::SameLine(0.0f, 2.0f);
		if (ImGui::Button("+##rotate", ImVec2(button, 0.0f))) { report(doc.rotate_texture(rotate_step), "Rotate texture"); }
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputFloat("##rotate_step", &rotate_step, 0.0f, 0.0f, "%.3g deg");

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Fit");
		ImGui::SameLine(70.0f);
		if (ImGui::Button("Fit", ImVec2(button * 2.0f + 2.0f, 0.0f))) { report(doc.fit_texture(fit[0], fit[1]), "Fit texture"); }
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputFloat2("##fit", fit, "%.3g repeats");

		// ---- absolute: the numbers of the first face, written to all
		ImGui::Separator();

		static edit::tex_def typed;
		static std::uint64_t typed_for = ~0ull;

		// follows the document until a field is edited
		if (typed_for != doc.change_counter() + doc.selection_counter())
		{
			typed_for = doc.change_counter() + doc.selection_counter();
			typed = tex;
		}

		float size[2] = { static_cast<float>(typed.size_x), static_cast<float>(typed.size_y) };
		float shift[2] = { static_cast<float>(typed.shift_x), static_cast<float>(typed.shift_y) };
		float rotation = static_cast<float>(typed.rotation);

		ImGui::SetNextItemWidth(-110.0f);
		if (ImGui::InputFloat2("size", size, "%.4g")) { typed.size_x = size[0]; typed.size_y = size[1]; }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("World units one repeat of the texture covers"); }
		ImGui::SetNextItemWidth(-110.0f);
		if (ImGui::InputFloat2("shift", shift, "%.4g")) { typed.shift_x = shift[0]; typed.shift_y = shift[1]; }
		ImGui::SetNextItemWidth(-110.0f);
		if (ImGui::InputFloat("rotation", &rotation, 0.0f, 0.0f, "%.4g")) { typed.rotation = rotation; }

		if (ImGui::Button("Set on all selected faces", ImVec2(-1.0f, 0.0f)))
		{
			edit::tex_def set = typed;
			set.material.clear(); // the alignment only
			report(doc.set_texture(set), "Set texture");
		}

		bool lock = doc.texture_lock();
		if (ImGui::Checkbox("Texture lock ( Shift+T )", &lock))
		{
			doc.set_texture_lock(lock);
		}

		ImGui::End();
	}
}
