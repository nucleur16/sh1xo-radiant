#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

namespace codol::script
{
	namespace
	{
		bool iequals(std::string_view a, std::string_view b)
		{
			if (a.size() != b.size())
			{
				return false;
			}

			for (std::size_t i = 0; i < a.size(); i++)
			{
				if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
				{
					return false;
				}
			}

			return true;
		}

		bool in_list(std::string_view word, const std::initializer_list<std::string_view>& list, bool ignore_case = false)
		{
			for (const auto& w : list)
			{
				if (ignore_case ? iequals(w, word) : w == word)
				{
					return true;
				}
			}

			return false;
		}

		bool gsc_keyword(std::string_view w)
		{
			return in_list(w,
			{
				"if", "else", "for", "foreach", "in", "while", "do", "switch", "case", "default",
				"break", "continue", "return", "wait", "waittill", "waittillmatch", "waittillframeend",
				"notify", "endon", "thread", "childthread", "call", "breakpoint", "prof_begin", "prof_end",
			}, true);
		}

		bool gsc_builtin(std::string_view w)
		{
			return in_list(w, { "self", "level", "game", "anim", "true", "false", "undefined", "size" }, true);
		}

		bool playeranim_section(std::string_view w)
		{
			return in_list(w, { "DEFINES", "ANIMATIONS", "CANNED_ANIMATIONS", "STATECHANGES", "EVENTS" });
		}

		bool playeranim_keyword(std::string_view w)
		{
			return in_list(w,
			{
				"set", "state", "default", "AND", "NOT", "both", "legs", "torso", "turret", "sound",
				"playerAnimType", "weaponclass", "position", "enemy_weapon", "underwater", "mounted", "movetype",
				"leaning", "weapon_position", "perk", "damagetype", "hitlocation", "hitdirection", "firing",
				"akimbo", "riotshieldnext", "fastmantle", "strafing", "direction", "slope", "stance",
			}, true);
		}

		void push(std::vector<span>& out, std::size_t start, std::size_t end, tok kind)
		{
			if (end > start)
			{
				out.push_back({ static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(end - start), kind });
			}
		}

		// returns the index one past the closing quote ( or line end )
		std::size_t scan_string(std::string_view line, std::size_t i)
		{
			const char quote = line[i++];
			while (i < line.size())
			{
				if (line[i] == '\\' && i + 1 < line.size())
				{
					i += 2;
					continue;
				}

				if (line[i++] == quote)
				{
					break;
				}
			}

			return i;
		}

		std::size_t scan_number(std::string_view line, std::size_t i)
		{
			if (line[i] == '0' && i + 1 < line.size() && (line[i + 1] == 'x' || line[i + 1] == 'X'))
			{
				i += 2;
				while (i < line.size() && std::isxdigit(static_cast<unsigned char>(line[i])))
				{
					i++;
				}

				return i;
			}

			while (i < line.size() && (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.'))
			{
				i++;
			}

			if (i < line.size() && (line[i] == 'e' || line[i] == 'E'))
			{
				std::size_t j = i + 1;
				if (j < line.size() && (line[j] == '+' || line[j] == '-'))
				{
					j++;
				}

				if (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j])))
				{
					while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j])))
					{
						j++;
					}

					i = j;
				}
			}

			if (i < line.size() && (line[i] == 'f' || line[i] == 'F'))
			{
				i++;
			}

			return i;
		}

		void lex_code(lang l, std::string_view line, std::uint8_t& state, std::vector<span>& out)
		{
			const bool c_like = (l == lang::gsc);
			std::size_t i = 0;
			const std::size_t n = line.size();

			while (i < n)
			{
				// continue / close a block comment
				if (state & LS_BLOCK_COMMENT)
				{
					const auto end = line.find("*/", i);
					if (end == std::string_view::npos)
					{
						push(out, i, n, tok::comment);
						return;
					}

					push(out, i, end + 2, tok::comment);
					i = end + 2;
					state &= ~LS_BLOCK_COMMENT;
					continue;
				}

				const char c = line[i];
				const char c1 = (i + 1 < n) ? line[i + 1] : '\0';

				if (c == ' ' || c == '\t' || c == '\r')
				{
					i++;
					continue;
				}

				if (c == '/' && c1 == '/')
				{
					push(out, i, n, tok::comment);
					return;
				}

				if (c == '/' && c1 == '*')
				{
					const auto end = line.find("*/", i + 2);
					if (end == std::string_view::npos)
					{
						state |= LS_BLOCK_COMMENT;
						push(out, i, n, tok::comment);
						return;
					}

					push(out, i, end + 2, tok::comment);
					i = end + 2;
					continue;
				}

				if (c_like && c == '/' && c1 == '#')
				{
					state |= LS_DEV_BLOCK;
					push(out, i, i + 2, tok::preproc);
					i += 2;
					continue;
				}

				if (c_like && c == '#' && c1 == '/')
				{
					state &= ~LS_DEV_BLOCK;
					push(out, i, i + 2, tok::preproc);
					i += 2;
					continue;
				}

				if (c == '"' || (c_like && c == '\''))
				{
					const auto end = scan_string(line, i);
					push(out, i, end, tok::string);
					i = end;
					continue;
				}

				if (c_like && c == '&' && c1 == '"')
				{
					const auto end = scan_string(line, i + 1);
					push(out, i, end, tok::localized);
					i = end;
					continue;
				}

				if (c_like && c == '#' && is_ident_start(c1))
				{
					std::size_t j = i + 1;
					while (j < n && is_ident_char(line[j]))
					{
						j++;
					}

					push(out, i, j, tok::preproc);
					i = j;
					continue;
				}

				if (c_like && c == '%' && is_anim_percent(line, i))
				{
					std::size_t j = i + 1;
					while (j < n && is_ident_char(line[j]))
					{
						j++;
					}

					push(out, i, j, tok::anim);
					i = j;
					continue;
				}

				if (c_like && c == ':' && c1 == ':')
				{
					std::size_t j = i + 2;
					while (j < n && is_ident_char(line[j]))
					{
						j++;
					}

					push(out, i, j, tok::funcref);
					i = j;
					continue;
				}

				if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && std::isdigit(static_cast<unsigned char>(c1))))
				{
					const auto end = scan_number(line, i);
					push(out, i, end, tok::number);
					i = end;
					continue;
				}

				if (is_ident_start(c))
				{
					std::size_t j = i + 1;
					while (j < n && is_ident_char(line[j]))
					{
						j++;
					}

					// script paths: maps\mp\_utility::func
					if (c_like)
					{
						std::size_t k = j;
						while (k < n && line[k] == '\\' && k + 1 < n && is_ident_start(line[k + 1]))
						{
							k++;
							while (k < n && is_ident_char(line[k]))
							{
								k++;
							}
						}

						if (k != j)
						{
							if (k + 1 < n && line[k] == ':' && line[k + 1] == ':')
							{
								k += 2;
								while (k < n && is_ident_char(line[k]))
								{
									k++;
								}
							}

							push(out, i, k, tok::funcref);
							i = k;
							continue;
						}
					}

					const auto word = line.substr(i, j - i);
					tok kind = tok::text;

					switch (l)
					{
					case lang::gsc:
						kind = gsc_keyword(word) ? tok::keyword : gsc_builtin(word) ? tok::builtin : tok::text;
						break;

					case lang::playeranim:
						kind = playeranim_section(word) ? tok::section : playeranim_keyword(word) ? tok::keyword : tok::text;
						break;

					case lang::atr:
					{
						// a name that is followed by '{' ( on this line ) is a group
						auto k = line.find_first_not_of(" \t\r", j);
						kind = (k != std::string_view::npos && line[k] == '{') ? tok::section : tok::anim;
						break;
					}

					default:
						break;
					}

					push(out, i, j, kind);
					i = j;
					continue;
				}

				push(out, i, i + 1, tok::punct);
				i++;
			}
		}

		void lex_csv(std::string_view line, std::vector<span>& out)
		{
			std::size_t i = 0;
			const std::size_t n = line.size();
			bool first = true;

			if (n >= 2 && line[0] == '/' && line[1] == '/')
			{
				push(out, 0, n, tok::comment);
				return;
			}

			while (i <= n)
			{
				std::size_t j = i;
				if (j < n && line[j] == '"')
				{
					j = scan_string(line, j);
				}

				while (j < n && line[j] != ',')
				{
					j++;
				}

				const auto cell = line.substr(i, j - i);
				tok kind = tok::text;

				if (!cell.empty())
				{
					char* end = nullptr;
					const std::string tmp(cell);
					std::strtod(tmp.c_str(), &end);
					if (end && *end == '\0')
					{
						kind = tok::number;
					}
					else if (first)
					{
						kind = tok::keyword;
					}
				}

				push(out, i, j, kind);
				if (j < n)
				{
					push(out, j, j + 1, tok::punct);
				}

				first = false;
				i = j + 1;
			}
		}

		void lex_cfg(std::string_view line, std::vector<span>& out)
		{
			std::uint8_t state = LS_NONE;
			const auto first_token = out.size();
			lex_code(lang::cfg, line, state, out);

			// first bare word of a line is the key
			if (out.size() > first_token && out[first_token].kind == tok::text)
			{
				out[first_token].kind = tok::keyword;
			}
		}
	}

	bool is_anim_percent(std::string_view line, std::size_t pos)
	{
		if (pos + 1 >= line.size() || !is_ident_start(line[pos + 1]))
		{
			return false;
		}

		// look at what precedes the '%': an operand means modulo
		std::size_t i = pos;
		while (i > 0 && (line[i - 1] == ' ' || line[i - 1] == '\t'))
		{
			i--;
		}

		if (i == 0)
		{
			return true;
		}

		const char p = line[i - 1];
		if (p == ')' || p == ']' || p == '"' || (p >= '0' && p <= '9'))
		{
			return false;
		}

		if (is_ident_char(p))
		{
			// `return %anim` / `case %anim` are references, `a %b` is modulo
			std::size_t s = i;
			while (s > 0 && is_ident_char(line[s - 1]))
			{
				s--;
			}

			const auto word = line.substr(s, i - s);
			return word == "return" || word == "case";
		}

		return true;
	}

	lang lang_from_path(std::string_view path)
	{
		const auto dot = path.find_last_of('.');
		if (dot == std::string_view::npos)
		{
			return lang::plain;
		}

		std::string ext(path.substr(dot + 1));
		for (auto& ch : ext)
		{
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}

		if (ext == "gsc" || ext == "csc" || ext == "gsh")
		{
			return lang::gsc;
		}

		if (ext == "atr")
		{
			return lang::atr;
		}

		if (ext == "script")
		{
			return lang::playeranim;
		}

		if (ext == "csv")
		{
			return lang::csv;
		}

		if (ext == "cfg" || ext == "vision" || ext == "sun" || ext == "shock" || ext == "def" || ext == "str" || ext == "menu" || ext == "ents")
		{
			return lang::cfg;
		}

		return lang::plain;
	}

	const char* lang_name(lang l)
	{
		switch (l)
		{
		case lang::gsc: return "GSC";
		case lang::atr: return "Animtree";
		case lang::playeranim: return "Player anim script";
		case lang::csv: return "CSV";
		case lang::cfg: return "Config";
		default: return "Text";
		}
	}

	void lex_line(lang l, std::string_view line, std::uint8_t& state, std::vector<span>& out)
	{
		out.clear();

		switch (l)
		{
		case lang::gsc:
		case lang::atr:
		case lang::playeranim:
			lex_code(l, line, state, out);
			break;

		case lang::csv:
			lex_csv(line, out);
			break;

		case lang::cfg:
			lex_cfg(line, out);
			break;

		default:
			push(out, 0, line.size(), tok::text);
			break;
		}
	}
}
