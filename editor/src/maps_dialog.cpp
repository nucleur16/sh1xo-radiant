#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"

namespace ggui
{
	void codol_map_ents_dialog::refresh_sources()
	{
		m_sources.clear();
		m_scanned = true;

		const auto install = components::codol_game::install_path();
		if (install.empty())
		{
			return;
		}

		const auto root = std::filesystem::u8path(install);
		std::unordered_set<std::string> seen;

		// the patch folder overrides the base zones, like in the game
		for (const auto& dir : { root / "client_shipRetail" / "patch", root / "client_shipRetail" / "chinese" })
		{
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
			{
				if (!entry.is_regular_file(ec) || entry.path().extension() != ".ff")
				{
					continue;
				}

				source src;
				src.map = editor::to_utf8(entry.path().stem());
				src.path = editor::to_utf8(entry.path());
				src.size = entry.file_size(ec);

				if (seen.insert(src.map).second)
				{
					m_sources.push_back(std::move(src));
				}
			}
		}

		std::sort(m_sources.begin(), m_sources.end(), [](const source& a, const source& b) { return a.map < b.map; });
	}

	void codol_map_ents_dialog::on_open()
	{
		if (!m_scanned)
		{
			refresh_sources();
		}
	}

	void codol_map_ents_dialog::import_file(const std::string& path, const std::string& map_name)
	{
		std::string name = map_name.empty() ? path : map_name;

		if (const auto slash = name.find_last_of("\\/"); slash != std::string::npos)
		{
			name.erase(0, slash + 1);
		}

		if (const auto dot = name.find('.'); dot != std::string::npos)
		{
			name.erase(dot);
		}

		GET_GUI(ggui::codol_viewport_dialog)->load_map(name);
	}

	void codol_map_ents_dialog::export_current_map()
	{
		ImGui::Toast(ImGuiToastType_Warning, "CoDOL", "No map document is open.");
	}

	bool codol_map_ents_dialog::gui()
	{
		ImGui::SetNextWindowSize(ImVec2(360.0f, 600.0f), ImGuiCond_FirstUseEver);

		if (!ImGui::Begin("Maps##window", this->get_p_open(), ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return false;
		}

		if (components::codol_game::install_path().empty())
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(ImVec4(0.96f, 0.45f, 0.40f, 1.0f), "The CoDOL install folder is not set.");
			ImGui::TextDisabled("Set it in Script Workbench > Setup, then press Rescan.");
			ImGui::PopTextWrapPos();
		}

		ImGui::SetNextItemWidth(-90.0f);
		ImGui::InputTextWithHint("##maps_filter", "filter", &m_filter);
		ImGui::SameLine();

		if (ImGui::Button("Rescan", ImVec2(-1.0f, 0.0f)))
		{
			refresh_sources();
		}

		ImGui::Checkbox("Maps only", &m_maps_only);

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Off: every zone of the install is listed ( most of them are not maps )");
		}

		const std::string filter = codol::script::to_lower(m_filter);
		const std::string* clicked = nullptr;

		if (ImGui::BeginChild("##maps_list", ImVec2(0.0f, 0.0f), true))
		{
			for (const auto& src : m_sources)
			{
				if (m_maps_only && src.map.rfind("mp_", 0) != 0 && src.map.rfind("so_", 0) != 0 && src.map.rfind("sp_", 0) != 0)
				{
					continue;
				}

				// "_load" zones only hold the loading screen
				if (m_maps_only && src.map.size() > 5 && src.map.compare(src.map.size() - 5, 5, "_load") == 0)
				{
					continue;
				}

				if (!filter.empty() && codol::script::to_lower(src.map).find(filter) == std::string::npos)
				{
					continue;
				}

				if (ImGui::Selectable(src.map.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					clicked = &src.map;
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s\n%.1f MB\nDouble click to open", src.path.c_str(), static_cast<double>(src.size) / (1024.0 * 1024.0));
				}
			}
		}

		ImGui::EndChild();
		ImGui::End();

		if (clicked)
		{
			import_file(*clicked, *clicked);
		}

		return true;
	}

	REGISTER_GUI(codol_map_ents_dialog);
}
