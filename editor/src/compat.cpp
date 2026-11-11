#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"

// The part of the radiant plugin's API the shared CoDOL code uses, without radiant: settings ( "dvars" ) kept in
// a text file, console commands, the list of dockable windows, toasts.

namespace game
{
	namespace glob
	{
		IDirect3DDevice9* d3d9_device = nullptr;
	}

	void printf_to_console(const char* fmt, ...)
	{
		char stack[2048];

		va_list args;
		va_start(args, fmt);
		const int length = vsnprintf(stack, sizeof(stack), fmt, args);
		va_end(args);

		if (length < 0)
		{
			return;
		}

		std::string text;

		if (static_cast<std::size_t>(length) < sizeof(stack))
		{
			text.assign(stack, static_cast<std::size_t>(length));
		}
		else
		{
			text.resize(static_cast<std::size_t>(length) + 1);

			va_start(args, fmt);
			vsnprintf(text.data(), text.size(), fmt, args);
			va_end(args);

			text.resize(static_cast<std::size_t>(length));
		}

		editor::log(text);
	}
}

namespace dvars
{
	game::dvar_s* codol_install_path = nullptr;
	game::dvar_s* codol_script_root = nullptr;
	game::dvar_s* codol_script_ref_root = nullptr;
	game::dvar_s* codol_rcon_port = nullptr;
	game::dvar_s* codol_rcon_password = nullptr;
	game::dvar_s* codol_rcon_password_file = nullptr;
	game::dvar_s* codol_rcon_log = nullptr;
	game::dvar_s* codol_reload_on_save = nullptr;
	game::dvar_s* codol_reload_mode = nullptr;
	game::dvar_s* codol_reload_precheck_cmd = nullptr;
	game::dvar_s* codol_reload_precheck_cwd = nullptr;
	game::dvar_s* codol_client_exe = nullptr;
	game::dvar_s* codol_launch_cmd = nullptr;
	game::dvar_s* codol_asset_tool = nullptr;
	game::dvar_s* codol_xanim_list = nullptr;

	namespace
	{
		struct entry
		{
			game::dvar_s dvar = {};
			std::string name;
			std::string description;
			std::string value;		// storage of string dvars
			std::string reset;
		};

		// stable addresses: the shared code keeps pointers
		std::vector<std::unique_ptr<entry>>& entries()
		{
			static std::vector<std::unique_ptr<entry>> list;
			return list;
		}

		std::vector<game::dvar_s*>& pointers()
		{
			static std::vector<game::dvar_s*> list;
			return list;
		}

		// values of the settings file that were read before their dvar was registered
		std::unordered_map<std::string, std::string>& pending()
		{
			static std::unordered_map<std::string, std::string> map;
			return map;
		}

		bool g_config_loaded = false;

		entry* entry_of(const game::dvar_s* dvar)
		{
			for (const auto& e : entries())
			{
				if (&e->dvar == dvar)
				{
					return e.get();
				}
			}

			return nullptr;
		}

		void apply_text(entry& e, const std::string& text)
		{
			switch (e.dvar.type)
			{
			case game::dvar_type::boolean:
				e.dvar.current.enabled = text == "1" || text == "true";
				break;

			case game::dvar_type::integer:
				e.dvar.current.integer = std::clamp(utils::try_stoi(text), e.dvar.min, e.dvar.max);
				break;

			case game::dvar_type::string:
				e.value = text;
				e.dvar.current.string = e.value.c_str();
				break;
			}
		}

		std::string text_of(const entry& e)
		{
			switch (e.dvar.type)
			{
			case game::dvar_type::boolean: return e.dvar.current.enabled ? "1" : "0";
			case game::dvar_type::integer: return std::to_string(e.dvar.current.integer);
			default: return e.value;
			}
		}

		game::dvar_s* add(const char* name, const game::dvar_type type, const std::uint16_t flags, const char* description)
		{
			if (auto* existing = find(name))
			{
				return existing;
			}

			auto e = std::make_unique<entry>();
			e->name = name;
			e->description = description ? description : "";
			e->dvar.name = e->name.c_str();
			e->dvar.description = e->description.c_str();
			e->dvar.flags = static_cast<game::dvar_flags>(flags);
			e->dvar.type = type;
			e->dvar.min = INT_MIN;
			e->dvar.max = INT_MAX;
			e->dvar.current.string = e->value.c_str();

			entries().push_back(std::move(e));
			pointers().push_back(&entries().back()->dvar);
			return &entries().back()->dvar;
		}

		void apply_pending(entry& e)
		{
			if (const auto it = pending().find(e.name); it != pending().end())
			{
				apply_text(e, it->second);
			}
		}

		std::filesystem::path config_path()
		{
			return std::filesystem::u8path(utils::fs::get_basepath()) / "codol_editor.cfg";
		}

		void changed(const game::dvar_s* dvar)
		{
			if (dvar && (dvar->flags & game::dvar_flags::saved) && g_config_loaded)
			{
				save_config();
			}
		}
	}

	game::dvar_s* find(const std::string_view name)
	{
		for (const auto& e : entries())
		{
			if (e->name == name)
			{
				return &e->dvar;
			}
		}

		return nullptr;
	}

	const std::vector<game::dvar_s*>& all()
	{
		return pointers();
	}

	game::dvar_s* register_int(const char* dvar_name, const int value, const int mins, const int maxs, const std::uint16_t flags, const char* description)
	{
		auto* dvar = add(dvar_name, game::dvar_type::integer, flags, description);
		dvar->min = mins;
		dvar->max = maxs;
		dvar->current.integer = value;
		dvar->reset.integer = value;
		apply_pending(*entry_of(dvar));
		return dvar;
	}

	game::dvar_s* register_bool(const char* dvar_name, const char value, const std::uint16_t flags, const char* description)
	{
		auto* dvar = add(dvar_name, game::dvar_type::boolean, flags, description);
		dvar->current.enabled = value != 0;
		dvar->reset.enabled = value != 0;
		apply_pending(*entry_of(dvar));
		return dvar;
	}

	game::dvar_s* register_string(const char* dvar_name, const char* value, const std::uint16_t flags, const char* description)
	{
		auto* dvar = add(dvar_name, game::dvar_type::string, flags, description);
		auto* e = entry_of(dvar);
		e->value = value ? value : "";
		e->reset = e->value;
		dvar->current.string = e->value.c_str();
		dvar->reset.string = e->reset.c_str();
		apply_pending(*e);
		return dvar;
	}

	void set_bool(game::dvar_s* dvar, const bool value)
	{
		if (dvar && dvar->current.enabled != value)
		{
			dvar->current.enabled = value;
			dvar->modified = true;
			changed(dvar);
		}
	}

	void set_int(game::dvar_s* dvar, const int value)
	{
		if (dvar && dvar->current.integer != value)
		{
			dvar->current.integer = std::clamp(value, dvar->min, dvar->max);
			dvar->modified = true;
			changed(dvar);
		}
	}

	void set_string(game::dvar_s* dvar, const char* value)
	{
		if (auto* e = entry_of(dvar); e && e->value != (value ? value : ""))
		{
			e->value = value ? value : "";
			dvar->current.string = e->value.c_str();
			dvar->modified = true;
			changed(dvar);
		}
	}

	// one setting per line: name "value"   ( \\ and \" escaped )
	void load_config()
	{
		std::ifstream file(config_path(), std::ios::binary);
		std::string line;

		while (file.is_open() && std::getline(file, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}

			const auto space = line.find(' ');
			const auto open = line.find('"');
			if (line.empty() || line[0] == '/' || space == std::string::npos || open == std::string::npos || line.back() != '"' || open + 1 > line.size() - 1)
			{
				continue;
			}

			std::string value;
			for (std::size_t i = open + 1; i + 1 < line.size(); i++)
			{
				if (line[i] == '\\' && i + 2 < line.size() && (line[i + 1] == '\\' || line[i + 1] == '"'))
				{
					i++;
				}

				value.push_back(line[i]);
			}

			pending()[line.substr(0, space)] = value;
		}

		for (const auto& e : entries())
		{
			apply_pending(*e);
		}

		g_config_loaded = true;
	}

	void save_config()
	{
		std::string out = "// CoDOL editor settings, written by the editor\n";
		std::unordered_set<std::string> written;

		const auto write = [&](const std::string& name, const std::string& value)
		{
			out += name + " \"";

			for (const char c : value)
			{
				if (c == '\\' || c == '"')
				{
					out.push_back('\\');
				}

				out.push_back(c);
			}

			out += "\"\n";
			written.insert(name);
		};

		for (const auto& e : entries())
		{
			if (e->dvar.flags & game::dvar_flags::saved)
			{
				write(e->name, text_of(*e));
			}
		}

		// keep settings of things that are not registered ( yet )
		for (const auto& [name, value] : pending())
		{
			if (!written.contains(name) && !find(name))
			{
				write(name, value);
			}
		}

		std::ofstream file(config_path(), std::ios::binary | std::ios::trunc);
		file.write(out.data(), static_cast<std::streamsize>(out.size()));
	}
}

namespace utils
{
	std::vector<std::string> split(const std::string& str, const char delimiter)
	{
		std::vector<std::string> out;
		std::string part;

		for (const char c : str)
		{
			if (c == delimiter)
			{
				if (!part.empty())
				{
					out.push_back(part);
					part.clear();
				}
			}
			else
			{
				part.push_back(c);
			}
		}

		if (!part.empty())
		{
			out.push_back(part);
		}

		return out;
	}

	int try_stoi(const std::string& str, bool)
	{
		try
		{
			return std::stoi(str);
		}
		catch (...)
		{
			return 0;
		}
	}

	float try_stof(const std::string& str, bool)
	{
		try
		{
			return std::stof(str);
		}
		catch (...)
		{
			return 0.0f;
		}
	}

	namespace fs
	{
		std::string get_basepath()
		{
			static std::string base;

			if (base.empty())
			{
				wchar_t path[MAX_PATH * 4] = {};
				GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
				base = editor::to_utf8(std::filesystem::path(path).parent_path());
			}

			return base;
		}
	}
}

namespace command
{
	namespace
	{
		std::map<std::string, std::function<void(std::vector<std::string>)>>& table()
		{
			static std::map<std::string, std::function<void(std::vector<std::string>)>> map;
			return map;
		}
	}

	void register_command(const std::string& name, std::function<void(std::vector<std::string>)> callback)
	{
		table()[name] = std::move(callback);
	}

	void execute_command(const std::vector<std::string>& args)
	{
		if (args.empty())
		{
			return;
		}

		if (const auto it = table().find(args[0]); it != table().end())
		{
			it->second(args);
			return;
		}

		// a setting: print or set it
		if (auto* dvar = dvars::find(args[0]))
		{
			if (args.size() > 1)
			{
				std::string value = args[1];
				for (std::size_t i = 2; i < args.size(); i++)
				{
					value += " " + args[i];
				}

				switch (dvar->type)
				{
				case game::dvar_type::boolean: dvars::set_bool(dvar, value == "1" || value == "true"); break;
				case game::dvar_type::integer: dvars::set_int(dvar, utils::try_stoi(value)); break;
				default: dvars::set_string(dvar, value.c_str()); break;
				}
			}

			switch (dvar->type)
			{
			case game::dvar_type::boolean: game::printf_to_console("%s = %d\n", dvar->name, dvar->current.enabled ? 1 : 0); break;
			case game::dvar_type::integer: game::printf_to_console("%s = %d\n", dvar->name, dvar->current.integer); break;
			default: game::printf_to_console("%s = \"%s\"\n", dvar->name, dvar->current.string); break;
			}

			return;
		}

		game::printf_to_console("^1unknown command: %s\n", args[0].c_str());
	}

	// like a console line: words split on spaces, "quoted words" kept together
	void execute(const std::string& line)
	{
		std::vector<std::string> args;
		std::string word;
		bool quoted = false, have = false;

		for (const char c : line)
		{
			if (c == '"')
			{
				quoted = !quoted;
				have = true;
			}
			else if (c == ' ' && !quoted)
			{
				if (have || !word.empty())
				{
					args.push_back(word);
				}

				word.clear();
				have = false;
			}
			else
			{
				word.push_back(c);
			}
		}

		if (have || !word.empty())
		{
			args.push_back(word);
		}

		execute_command(args);
	}

	std::vector<std::string> names()
	{
		std::vector<std::string> out;
		for (const auto& [name, callback] : table())
		{
			out.push_back(name);
		}

		return out;
	}
}

// ---------------------------------------------------------------------------------------------------------

namespace
{
	struct toast
	{
		ImGuiToastType_ type;
		std::string title;
		std::string content;
		double until;
	};

	std::vector<toast> g_toasts;
}

namespace ImGui
{
	void Toast(const ImGuiToastType_ type, const char* title, const char* content, const int time_ms)
	{
		g_toasts.push_back({ type, title ? title : "", content ? content : "", ImGui::GetTime() + time_ms / 1000.0 });

		const char* prefix = type == ImGuiToastType_Error ? "^1" : "";
		editor::log(std::string(prefix) + "[" + (title ? title : "") + "] " + (content ? content : ""));
	}

	void RenderToasts()
	{
		const double now = ImGui::GetTime();
		std::erase_if(g_toasts, [&](const toast& t) { return t.until < now; });

		const auto* viewport = ImGui::GetMainViewport();
		float y = viewport->WorkPos.y + viewport->WorkSize.y - 16.0f;
		int index = 0;

		for (auto it = g_toasts.rbegin(); it != g_toasts.rend() && index < 6; ++it, ++index)
		{
			ImVec4 accent(0.45f, 0.65f, 0.95f, 1.0f);
			switch (it->type)
			{
			case ImGuiToastType_Success: accent = ImVec4(0.40f, 0.80f, 0.45f, 1.0f); break;
			case ImGuiToastType_Warning: accent = ImVec4(0.95f, 0.75f, 0.30f, 1.0f); break;
			case ImGuiToastType_Error: accent = ImVec4(0.96f, 0.45f, 0.40f, 1.0f); break;
			default: break;
			}

			ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 16.0f, y), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
			ImGui::SetNextWindowBgAlpha(0.92f);
			ImGui::SetNextWindowViewport(viewport->ID);

			char name[32];
			snprintf(name, sizeof(name), "##toast%d", index);

			if (ImGui::Begin(name, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
				| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoInputs))
			{
				ImGui::PushTextWrapPos(420.0f);
				ImGui::TextColored(accent, "%s", it->title.c_str());
				ImGui::TextUnformatted(it->content.c_str());
				ImGui::PopTextWrapPos();
				y -= ImGui::GetWindowHeight() + 8.0f;
			}

			ImGui::End();
		}
	}
}

namespace ggui
{
	std::vector<std::unique_ptr<ggui_module>>& loader::get_modules()
	{
		static std::vector<std::unique_ptr<ggui_module>> modules;
		return modules;
	}

	void loader::register_gui(std::unique_ptr<ggui_module>&& module_)
	{
		get_modules().push_back(std::move(module_));
	}

	void loader::destroy_modules()
	{
		get_modules().clear();
	}

	ImVec2 get_initial_window_pos()
	{
		const auto* viewport = ImGui::GetMainViewport();
		return ImVec2(viewport->WorkPos.x + 80.0f, viewport->WorkPos.y + 60.0f);
	}
}
