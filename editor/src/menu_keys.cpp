#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "ui.hpp"
#include "ext.hpp"

// The keys the hand written menus show: menu::key( "<action id>" ) is the key of the action as it is now, so a key
// changed in Edit Hotkeys shows at once. menu::begin / end / item stand for ImGui::BeginMenu / EndMenu / MenuItem
// and remember what every entry showed; actions_check walks every menu once ( the menu bar is run into a window off
// the screen with every menu "open" ) and reports entries whose key text is not the key of their action.

namespace editor::menu
{
	namespace
	{
		struct seen_item
		{
			std::string id;				// the action the key came from ( "" : the text is not from the registry )
			std::string shown;			// the key text the entry showed
		};

		std::vector<std::string> g_path;			// the menus the entries being drawn are in
		std::map<std::string, seen_item> g_seen;	// "Edit > Delete" -> what it showed
		std::string g_pending_id;
		const char* g_pending_text = nullptr;
		bool g_pending = false;
		bool g_audit = false;
		bool g_audit_request = false;

		std::string_view visible(const char* label)
		{
			std::string_view l(label ? label : "");
			if (const auto hash = l.find("##"); hash != std::string_view::npos) { l = l.substr(0, hash); }
			return l;
		}

		std::string path_of(const char* label)
		{
			std::string out;
			for (const auto& p : g_path) { out += p; out += " > "; }
			out += visible(label);
			return out;
		}

		void note(const char* label, const char* shortcut)
		{
			seen_item s;
			s.shown = shortcut ? shortcut : "";

			if (g_pending && shortcut == g_pending_text)
			{
				s.id = g_pending_id;
			}

			g_pending = false;
			g_seen[path_of(label)] = std::move(s);
		}

		std::string lower(std::string_view t)
		{
			std::string o(t);
			for (char& c : o) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return o;
		}

		std::string key_of(const actions::action& a)
		{
			return actions::chord_text(a.keys[0].empty() ? a.keys[1] : a.keys[0]);
		}
	}

	const char* key(const std::string_view action_id)
	{
		g_pending_id = std::string(action_id);
		g_pending_text = actions::shortcut(action_id);
		g_pending = true;
		return g_pending_text;
	}

	bool begin(const char* label, const bool enabled)
	{
		if (!g_audit && !ImGui::BeginMenu(label, enabled))
		{
			return false;
		}

		g_path.emplace_back(visible(label));
		return true;
	}

	void end()
	{
		if (!g_path.empty()) { g_path.pop_back(); }
		if (!g_audit) { ImGui::EndMenu(); }
	}

	bool item(const char* label, const char* shortcut, const bool selected, const bool enabled)
	{
		note(label, shortcut);
		return g_audit ? false : ImGui::MenuItem(label, shortcut, selected, enabled);
	}

	bool item(const char* label, const char* shortcut, bool* p_selected, const bool enabled)
	{
		note(label, shortcut);
		return g_audit ? false : ImGui::MenuItem(label, shortcut, p_selected, enabled);
	}

	bool auditing() { return g_audit; }
	bool audit_requested() { return g_audit_request; }

	void audit_begin()
	{
		g_audit = true;
		g_path.clear();
		ImGui::SetNextWindowPos(ImVec2(-20000.0f, -20000.0f));
		ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f));
		ImGui::Begin("##menu_audit", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);
	}

	void audit_end()
	{
		ImGui::End();
		g_audit = false;
		g_audit_request = false;
		g_path.clear();

		std::unordered_map<std::string, const actions::action*> by_label;
		for (const auto& a : actions::all()) { by_label.emplace(lower(a->label), a.get()); }

		int with_key = 0, problems = 0;

		for (const auto& [where, s] : g_seen)
		{
			if (!s.id.empty())
			{
				const actions::action* a = actions::find(s.id);
				if (!a)
				{
					game::printf_to_console("^1[actions_check] %s: there is no action %s\n", where.c_str(), s.id.c_str());
					problems++;
					continue;
				}

				const std::string now = key_of(*a);
				if (now != s.shown)
				{
					game::printf_to_console("^1[actions_check] %s shows \"%s\", %s has \"%s\"\n", where.c_str(), s.shown.c_str(), s.id.c_str(), now.c_str());
					problems++;
				}
				else if (!now.empty())
				{
					with_key++;
				}

				continue;
			}

			if (!s.shown.empty())
			{
				game::printf_to_console("^1[actions_check] %s shows \"%s\": a key text that is not from the registry\n", where.c_str(), s.shown.c_str());
				problems++;
				continue;
			}

			// no key shown: does an action of the same name have one?
			const auto cut = where.rfind(" > ");
			const std::string label = cut == std::string::npos ? where : where.substr(cut + 3);

			if (const auto it = by_label.find(lower(label)); it != by_label.end() && !key_of(*it->second).empty())
			{
				game::printf_to_console("^3[actions_check] %s shows no key; the action %s of the same name has %s\n", where.c_str(), it->second->id.c_str(), key_of(*it->second).c_str());
				problems++;
			}
		}

		game::printf_to_console("[actions_check] %d menu entries, %d with a key of the registry, %d problem%s\n",
			static_cast<int>(g_seen.size()), with_key, problems, problems == 1 ? "" : "s");
	}

	void register_commands()
	{
		// actions_check: every menu is walked once ( the next frame ) and its key texts are compared with the registry
		command::register_command("actions_check", [](const std::vector<std::string>&)
		{
			g_seen.clear();
			g_audit_request = true;
		});

		// actions_menus [text]: what the last walk saw ( entry, key, action )
		command::register_command("actions_menus", [](const std::vector<std::string>& args)
		{
			const std::string needle = args.size() > 1 ? lower(args[1]) : std::string();
			for (const auto& [where, s] : g_seen)
			{
				if (!needle.empty() && lower(where).find(needle) == std::string::npos) { continue; }
				game::printf_to_console("  %-60s %-20s %s\n", where.c_str(), s.shown.c_str(), s.id.c_str());
			}
		});
	}
}
