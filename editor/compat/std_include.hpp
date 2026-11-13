#pragma once

// Precompiled header of the standalone CoDOL editor.
//
// The CoDOL parts of the radiant plugin ( src/codol, the script workbench, the game bridge ) are compiled into the
// standalone editor from the same source files. They include "std_include.hpp" and use a small part of the radiant
// plugin's API ( dvars, console commands, gui modules, toasts ). This header is found first on the include path
// of the editor and provides that API without any radiant: see editor/src/compat.cpp.

#ifndef CODOL_STANDALONE
#define CODOL_STANDALONE 1
#endif

#include <WinSock2.h>
#include <windows.h>
#include <shellapi.h>
#include <d3d9.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <misc/cpp/imgui_stdlib.h>

#include <rapidjson/document.h>

using namespace std::literals;

// ---------------------------------------------------------------------------------------------------------
// game:: the few engine things the shared code touches

namespace game
{
	enum dvar_flags : std::uint16_t
	{
		none = 0,
		saved = 1,
	};

	enum class dvar_type : std::uint8_t
	{
		boolean,
		integer,
		string,
	};

	union DvarValue
	{
		bool enabled;
		int integer;
		float value;
		const char* string;
	};

	struct dvar_s
	{
		const char* name;
		const char* description;
		dvar_flags flags;
		dvar_type type;
		bool modified;
		DvarValue current;
		DvarValue reset;
		int min, max;
	};

	// "^1" at the start of a line marks an error, like in the game console
	void printf_to_console(const char* fmt, ...);

	namespace glob
	{
		extern IDirect3DDevice9* d3d9_device;
	}
}

namespace dvars
{
	extern game::dvar_s* codol_install_path;
	extern game::dvar_s* codol_script_root;
	extern game::dvar_s* codol_script_ref_root;
	extern game::dvar_s* codol_rcon_port;
	extern game::dvar_s* codol_rcon_password;
	extern game::dvar_s* codol_rcon_password_file;
	extern game::dvar_s* codol_rcon_log;
	extern game::dvar_s* codol_reload_on_save;
	extern game::dvar_s* codol_reload_mode;
	extern game::dvar_s* codol_reload_precheck_cmd;
	extern game::dvar_s* codol_reload_precheck_cwd;
	extern game::dvar_s* codol_client_exe;
	extern game::dvar_s* codol_launch_cmd;
	extern game::dvar_s* codol_asset_tool;
	extern game::dvar_s* codol_xanim_list;

	game::dvar_s* register_int(const char* dvar_name, int value, int mins, int maxs, std::uint16_t flags, const char* description);
	game::dvar_s* register_bool(const char* dvar_name, char value, std::uint16_t flags, const char* description);
	game::dvar_s* register_string(const char* dvar_name, const char* value, std::uint16_t flags, const char* description);

	void set_bool(game::dvar_s* dvar, bool value);
	void set_int(game::dvar_s* dvar, int value);
	void set_string(game::dvar_s* dvar, const char* value);

	game::dvar_s* find(std::string_view name);
	const std::vector<game::dvar_s*>& all();

	// settings file ( saved dvars ): <editor folder>\codol_editor.cfg
	void load_config();
	void save_config();
}

namespace utils
{
	std::vector<std::string> split(const std::string& str, char delimiter);
	int try_stoi(const std::string& str, bool quiet = true);
	float try_stof(const std::string& str, bool quiet = true);

	namespace fs
	{
		// folder of the editor ( holds the exe, its settings and map_source\ )
		std::string get_basepath();
	}
}

namespace command
{
	void register_command(const std::string& name, std::function<void(std::vector<std::string>)> callback);
	void execute_command(const std::vector<std::string>& args);
	void execute(const std::string& line);
	std::vector<std::string> names();
}

namespace components
{
	class component
	{
	public:
		component() = default;
		virtual ~component() = default;
		virtual const char* get_name() { return "unknown"; }
	};
}

// ---------------------------------------------------------------------------------------------------------
// toasts

enum ImGuiToastType_
{
	ImGuiToastType_None,
	ImGuiToastType_Success,
	ImGuiToastType_Warning,
	ImGuiToastType_Error,
	ImGuiToastType_Info,
};

namespace ImGui
{
	void Toast(ImGuiToastType_ type, const char* title, const char* content, int time_ms = 4000);
	void RenderToasts();
}

// ---------------------------------------------------------------------------------------------------------
// gui modules ( dockable windows )

namespace ggui
{
	enum GUI_TYPE_
	{
		GUI_TYPE_DEF,
		GUI_TYPE_RTT,
	};

	class ggui_module
	{
	public:
		ggui_module() = default;
		virtual ~ggui_module() = default;
		virtual bool gui() { return false; }
		virtual void on_init() {}
		virtual void on_open() {}
		virtual void on_close() {}

		void set_gui_type(GUI_TYPE_) {}

		void frame()
		{
			if (m_active)
			{
				if (!m_was_active)
				{
					on_open();
				}

				m_visible = gui();
				m_was_active = true;
			}
			else if (m_was_active)
			{
				on_close();
				m_was_active = false;
			}
		}

		[[nodiscard]] bool is_active() const { return m_active; }
		[[nodiscard]] bool is_visible() const { return m_active && m_visible; }

		void toggle(const bool manually = false, const bool n_state = false)
		{
			m_active = manually ? n_state : !m_active;
		}

		void open() { m_active = true; }
		void close() { m_active = false; }
		bool* get_p_open() { return &m_active; }

	private:
		bool m_active = false;
		bool m_was_active = false;
		bool m_visible = false;
	};

	class loader
	{
	public:
		template <typename T>
		class installer final
		{
		public:
			installer()
			{
				register_gui(std::make_unique<T>());
			}
		};

		template <typename T>
		static T* get()
		{
			for (const auto& module_ : get_modules())
			{
				if (typeid(*module_.get()) == typeid(T))
				{
					return static_cast<T*>(module_.get());
				}
			}

			assert(false && "GET_GUI nullptr");
			return nullptr;
		}

		static std::vector<std::unique_ptr<ggui_module>>& get_modules();
		static void register_gui(std::unique_ptr<ggui_module>&& module_);
		static void destroy_modules();
	};

#define GET_GUI(name) ggui::loader::get<name>()
#define REGISTER_GUI(name)							\
namespace											\
{													\
	static ggui::loader::installer<name> $_##name;	\
}

	ImVec2 get_initial_window_pos();
}

// ---------------------------------------------------------------------------------------------------------
// shared CoDOL code

#include "components/fonts.hpp"

#include "codol/c1_layouts.hpp"
#include "codol/script_lex.hpp"
#include "codol/anim_index.hpp"
#include "codol/map_geometry.hpp"
#include "codol/map_ents.hpp"

#include "components/modules/codol_game.hpp"
#include "ggui/script_workbench.hpp"
#include "ggui/codol_viewport.hpp"
#include "editor_map.hpp"
