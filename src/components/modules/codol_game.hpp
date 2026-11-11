#pragma once

namespace components
{
	// Bridge between radiant and a running CoDOL 1.1.1.13 stack:
	//  - paths of the CoDOL install / script tree ( dvars, saved )
	//  - rcon to the local dedicated server ( reload scripts / change map )
	//  - embedding the game client window into a radiant dock ( "Game" window )
	class codol_game : public component
	{
	public:
		codol_game();
		~codol_game() override;
		const char* get_name() override { return "codol_game"; }

		static void register_dvars();

		// ---------------------------------------------------------------------------------
		// paths

		// script roots in priority order ( editable root first ), only existing directories
		static std::vector<std::string> script_roots();
		static std::string install_path();

		// ---------------------------------------------------------------------------------
		// rcon ( asynchronous, results are delivered on the gui thread via frame() )

		using rcon_callback = std::function<void(bool /*answered*/, const std::string& /*response*/)>;
		static void rcon(const std::string& command, const rcon_callback& callback = nullptr);

		enum RELOAD_MODE : int
		{
			RELOAD_FAST_RESTART = 0,	// keeps compiled scripts, restarts the round
			RELOAD_MAP_RESTART = 1,		// reloads the level, engine dependent if scripts are recompiled
			RELOAD_MAP = 2,				// "map <current map>": full reload, always recompiles scripts from disk
		};

		// reload the running level so that changed scripts are picked up ( uses codol_reload_mode ).
		// runs the optional pre-check command first and aborts if it fails.
		static void reload_scripts(const std::string& reason);

		// "map <name>" on the server
		static void load_map(const std::string& map_name);

		// ---------------------------------------------------------------------------------
		// game window embedding

		struct embed_state
		{
			HWND game = nullptr;
			HWND parent = nullptr;
			LONG_PTR old_style = 0;
			LONG_PTR old_exstyle = 0;
			LONG_PTR old_parent_style = 0;
			RECT old_rect = {};
			RECT last_rect = {};
			bool visible = false;
		};

		// top level window of a running game client ( process name matches codol_client_exe ), or null
		static HWND find_game_window();
		static bool is_embedded();
		static bool attach_game_window(HWND parent);
		static void detach_game_window();

		// position the embedded window ( client coordinates of the parent ); hides it if !visible
		static void place_game_window(int x, int y, int w, int h, bool visible);

		// start the client using codol_launch_cmd
		static bool launch_game();

		// helpers for other codol code
		static void post_to_gui(std::function<void()> func);	// run on the gui thread ( next frame ), callable from any thread
		static int run_command(const std::string& command_line, const std::string& working_dir, std::string& output); // blocking, -1: could not start

		// called once per gui frame
		static void frame();

		// development aids, driven by environment variables so radiant can be tested unattended:
		//   CODOL_STARTUP_CMDS = "cmd one;wait 5;cmd two"  console commands that run once the gui is up
		//   CODOL_AUTOSHOT     = <file.png>           the rendered gui frame is saved there every 2 seconds
		static void dev_capture_frame();

		// last line(s) reported by the server / the bridge, shown in the workbench status bar
		static std::string status();
		static void set_status(const std::string& text);
	};
}
