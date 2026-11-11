#pragma once

namespace ggui
{
	// CoDOL map entities: bring the entities of a shipped map into radiant, edit them and write them
	// back as a server side override ( <scripts>/maps/mp/<map>.d3dbsp.ents ). See codol/map_ents.hpp.
	class codol_map_ents_dialog final : public ggui::ggui_module
	{
	public:
		codol_map_ents_dialog() { set_gui_type(GUI_TYPE_DEF); }

		bool gui() override;
		void on_open() override;

		// import the entities of a .ff / .zone / .d3dbsp.ents / entity text file and open them as a map
		void import_file(const std::string& path, const std::string& map_name);

		// save the current map and write its entities as <script folder>/maps/mp/<map>.d3dbsp.ents
		void export_current_map();

		bool m_reload_after_export = true;

	private:
		enum class source_kind { zone, override_file };

		struct source
		{
			source_kind kind;
			std::string map;		// mp_killhouse_sh
			std::string path;		// .ff or .d3dbsp.ents
			std::string label;
		};

		void refresh_sources();
		void import_source(const source& src);
		void finish_import(const std::string& map_name, const std::string& entity_text, const std::string& origin, int trigger_models = -1);
		void log(const std::string& line, bool error = false);

		std::vector<source> m_sources;
		int m_selected = -1;
		std::string m_filter;
		std::string m_manual_path;
		std::string m_manual_map;
		bool m_busy = false;

		std::vector<std::pair<bool, std::string>> m_log; // error?, text
	};
}
