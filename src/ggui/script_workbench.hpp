#pragma once

namespace ggui
{
	// Script workbench: browse / edit the script tree of the game ( gsc, animtrees, playeranim.script,
	// stringtables .. ) with syntax highlighting, structured views of everything that drives animations
	// ( player animation state machine, animtrees, scripted-animation scene tables ) and validation.
	class script_workbench_dialog final : public ggui::ggui_module
	{
	public:
		script_workbench_dialog() { set_gui_type(GUI_TYPE_DEF); }

		bool gui() override;
		void on_open() override;

		// open ( or focus ) a file and optionally jump to a 1 based line
		void open_file(const std::string& abs_path, int line = 0);

		// re-read the script roots in the background
		void rescan();

		struct document
		{
			std::string path;		// absolute
			std::string rel;		// relative to its root ( display )
			std::string title;		// file name
			codol::script::lang lang = codol::script::lang::plain;

			std::string text;		// utf-8, '\n' line ends
			std::size_t saved_hash = 0;
			bool crlf = false;		// file uses \r\n
			bool gbk = false;		// file is GBK ( cp936 ) encoded, converted for editing
			bool dirty = false;
			bool open = true;
			bool select_tab = false;

			std::vector<std::uint32_t> line_starts;
			std::vector<std::uint8_t> line_states;
			bool layout_dirty = true;

			int cursor = 0;
			int sel_start = 0;
			int sel_end = 0;
			int active_frame = -1;		// last frame the input was active
			int pending_cursor = -1;
			int pending_sel_end = -1;
			bool request_focus = false;

			std::filesystem::file_time_type mtime = {};
			bool changed_on_disk = false;

			std::map<int, std::pair<codol::script::severity, std::string>> markers; // 0 based line
			std::uint32_t markers_generation = 0;

			[[nodiscard]] int line_of(int offset) const;
			void rebuild_layout();
		};

	private:
		struct file_entry
		{
			std::string abs;
			std::string rel;		// forward slashes
			std::string name;
			int root = 0;
		};

		struct dir_node
		{
			std::string name;
			std::vector<int> dirs;	// indices into m_dirs
			std::vector<int> files;	// indices into m_files
		};

		struct scan_result
		{
			std::vector<std::string> roots;
			std::vector<file_entry> files;
			std::vector<dir_node> dirs;
			codol::script::anim_index index;
		};

		static scan_result scan(std::vector<std::string> roots, std::string xanim_list);

		document* find_document(const std::string& abs_path);
		bool save_document(document& doc);
		bool load_document(document& doc);
		void goto_line(document& doc, int line);
		void find_next(document& doc, bool backwards);
		void refresh_markers(document& doc);

		void draw_toolbar();
		void draw_setup_popup();
		void draw_left_panel(float width);
		void draw_files_tab();
		void draw_dir(int dir_index);
		void draw_animations_tab();
		void draw_player_anims();
		void draw_animtrees();
		void draw_scenes();
		void draw_problems_tab();
		void draw_documents();
		void draw_editor(document& doc, const ImVec2& size);
		void draw_status(document* doc);

		std::vector<std::string> m_roots;
		std::vector<file_entry> m_files;
		std::vector<dir_node> m_dirs;
		codol::script::anim_index m_index;
		std::uint32_t m_index_generation = 1;

		std::future<scan_result> m_scan;
		bool m_scanning = false;
		bool m_scanned_once = false;

		std::vector<std::unique_ptr<document>> m_docs;
		document* m_active_doc = nullptr;

		std::string m_file_filter;
		std::string m_anim_filter;
		std::string m_problem_filter;
		std::string m_find;
		std::string m_selected_tree;
		std::string m_selected_scene_file;
		int m_goto_line_input = 1;
		float m_left_width = 400.0f;
		bool m_open_setup = false;
		bool m_show_warnings = true;

		// setup popup buffers
		std::string m_cfg_script_root, m_cfg_ref_root, m_cfg_install, m_cfg_pw_file, m_cfg_precheck, m_cfg_precheck_cwd, m_cfg_launch, m_cfg_client_exe, m_cfg_asset_tool, m_cfg_xanim_list;
		int m_cfg_port = 17733;
	};

	// The running game inside a radiant dock window ( the client window is re-parented into radiant ).
	class game_view_dialog final : public ggui::ggui_module
	{
	public:
		game_view_dialog() { set_gui_type(GUI_TYPE_DEF); }

		bool gui() override;
		void on_close() override;

	private:
		std::string m_map;
	};
}
