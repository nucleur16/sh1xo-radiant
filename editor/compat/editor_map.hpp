#pragma once

namespace ggui
{
	// "Open map": the maps of the CoDOL install. In the standalone editor this takes the place of the radiant
	// plugin's map entity dialog ( same name and entry points, so the shared console commands work in both ).
	class codol_map_ents_dialog final : public ggui::ggui_module
	{
	public:
		codol_map_ents_dialog() = default;

		bool gui() override;
		void on_open() override;

		// open a map: a zone name ( "mp_rust" ), or the path of its .ff
		void import_file(const std::string& path, const std::string& map_name);

		// write the entities of the open map as <script folder>/maps/mp/<map>.d3dbsp.ents
		void export_current_map();

		bool m_reload_after_export = true;

	private:
		struct source
		{
			std::string map;
			std::string path;
			std::uintmax_t size = 0;
		};

		void refresh_sources();

		std::vector<source> m_sources;
		std::string m_filter;
		bool m_scanned = false;
		bool m_maps_only = true;
	};
}
