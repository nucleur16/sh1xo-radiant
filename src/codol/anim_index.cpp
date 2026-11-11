#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

namespace codol::script
{
	std::string to_lower(std::string_view s)
	{
		std::string out(s);
		for (auto& c : out)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}

		return out;
	}

	namespace
	{
		std::string_view trim(std::string_view s)
		{
			const auto b = s.find_first_not_of(" \t\r\n");
			if (b == std::string_view::npos)
			{
				return {};
			}

			const auto e = s.find_last_not_of(" \t\r\n");
			return s.substr(b, e - b + 1);
		}

		// replaces // and /* */ comments with spaces, keeps newlines ( so line numbers stay valid )
		// and leaves string literals untouched
		std::string blank_comments(std::string_view text)
		{
			std::string out(text);
			const std::size_t n = out.size();
			std::size_t i = 0;

			while (i < n)
			{
				const char c = out[i];
				const char c1 = (i + 1 < n) ? out[i + 1] : '\0';

				if (c == '"')
				{
					i++;
					while (i < n && out[i] != '"' && out[i] != '\n')
					{
						i += (out[i] == '\\' && i + 1 < n) ? 2 : 1;
					}

					i++;
				}
				else if (c == '/' && c1 == '/')
				{
					while (i < n && out[i] != '\n')
					{
						out[i++] = ' ';
					}
				}
				else if (c == '/' && c1 == '*')
				{
					out[i++] = ' ';
					out[i++] = ' ';
					while (i < n && !(out[i] == '*' && i + 1 < n && out[i + 1] == '/'))
					{
						if (out[i] != '\n')
						{
							out[i] = ' ';
						}

						i++;
					}

					if (i < n)
					{
						out[i++] = ' ';
						out[i++] = ' ';
					}
				}
				else
				{
					i++;
				}
			}

			return out;
		}

		std::vector<std::string_view> split_words(std::string_view s)
		{
			std::vector<std::string_view> words;
			std::size_t i = 0;

			while (i < s.size())
			{
				while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r'))
				{
					i++;
				}

				std::size_t j = i;
				while (j < s.size() && s[j] != ' ' && s[j] != '\t' && s[j] != '\r')
				{
					j++;
				}

				if (j > i)
				{
					words.push_back(s.substr(i, j - i));
				}

				i = j;
			}

			return words;
		}

		bool is_anim_part(std::string_view w)
		{
			const auto l = to_lower(w);
			return l == "both" || l == "legs" || l == "torso" || l == "turret";
		}
	}

	// ------------------------------------------------------------------------------------------
	// animtree

	bool animtree::contains(std::string_view anim) const
	{
		// every tree has an implicit root node the engine names "root" ( self clearAnim( %root, 0 ) )
		const auto key = to_lower(anim);
		return key == "root" || lookup.contains(key);
	}

	bool parse_animtree(std::string_view text, animtree& out)
	{
		out.nodes.clear();
		out.lookup.clear();
		out.errors.clear();
		out.warnings.clear();

		const std::string src = blank_comments(text);
		std::vector<int> stack;		// open groups
		int last_node = -1;			// candidate group for the next '{'
		int line = 1;
		std::size_t i = 0;

		while (i < src.size())
		{
			const char c = src[i];

			if (c == '\n')
			{
				line++;
				i++;
			}
			else if (c == ' ' || c == '\t' || c == '\r')
			{
				i++;
			}
			else if (c == '{')
			{
				if (last_node < 0)
				{
					out.errors.push_back("line " + std::to_string(line) + ": '{' without a group name");
				}
				else
				{
					out.nodes[last_node].group = true;
					stack.push_back(last_node);
				}

				last_node = -1;
				i++;
			}
			else if (c == '}')
			{
				if (stack.empty())
				{
					out.errors.push_back("line " + std::to_string(line) + ": unmatched '}'");
				}
				else
				{
					stack.pop_back();
				}

				last_node = -1;
				i++;
			}
			else
			{
				std::size_t j = i;
				while (j < src.size() && !std::isspace(static_cast<unsigned char>(src[j])) && src[j] != '{' && src[j] != '}')
				{
					j++;
				}

				// node flags, any number of them up to the end of the line:
				// "name : additive", "name : complete nonloopsync"
				if (src[i] == ':')
				{
					std::size_t k = i + 1;
					bool any = false;

					while (k < src.size())
					{
						while (k < src.size() && (src[k] == ' ' || src[k] == '\t' || src[k] == '\r'))
						{
							k++;
						}

						if (k >= src.size() || src[k] == '\n' || src[k] == '{' || src[k] == '}')
						{
							break;
						}

						std::size_t e = k;
						while (e < src.size() && !std::isspace(static_cast<unsigned char>(src[e])) && src[e] != '{' && src[e] != '}')
						{
							e++;
						}

						if (last_node >= 0)
						{
							auto& flags = out.nodes[last_node].flags;
							flags += (flags.empty() ? "" : " ") + src.substr(k, e - k);
						}

						any = true;
						k = e;
					}

					if (last_node < 0 || !any)
					{
						out.errors.push_back("line " + std::to_string(line) + ": ':' without an animation name or flag");
					}

					i = k;
					continue;
				}

				atr_node node;
				node.name = src.substr(i, j - i);
				node.line = line;
				node.depth = static_cast<int>(stack.size());
				node.parent = stack.empty() ? -1 : stack.back();

				const auto key = to_lower(node.name);
				const auto index = static_cast<int>(out.nodes.size());

				if (const auto it = out.lookup.find(key); it != out.lookup.end())
				{
					out.warnings.push_back("line " + std::to_string(line) + ": duplicate animation '" + node.name
						+ "' ( first defined on line " + std::to_string(it->second >= 0 ? out.nodes[it->second].line : 0) + " )");
				}
				else
				{
					out.lookup.emplace(key, index);
				}

				out.nodes.push_back(std::move(node));
				last_node = index;
				i = j;
			}
		}

		if (!stack.empty())
		{
			out.errors.push_back("unexpected end of file: " + std::to_string(stack.size()) + " unclosed '{'");
		}

		return out.errors.empty();
	}

	// ------------------------------------------------------------------------------------------
	// playeranim.script

	std::string playeranim::block_path(int block) const
	{
		std::vector<const pa_block*> chain;
		for (int b = block; b >= 0 && b < static_cast<int>(blocks.size()); b = blocks[b].parent)
		{
			chain.push_back(&blocks[b]);
		}

		if (chain.empty())
		{
			return {};
		}

		std::string out = chain.back()->section;
		for (auto it = chain.rbegin(); it != chain.rend(); ++it)
		{
			out += " > " + (*it)->label;
		}

		return out;
	}

	bool parse_playeranim(std::string_view text, playeranim& out)
	{
		out.defines.clear();
		out.blocks.clear();
		out.sections.clear();
		out.errors.clear();
		out.command_count = 0;

		const std::string src = blank_comments(text);

		std::string section;
		std::vector<int> stack;
		std::string pending;
		int pending_line = 0;
		int line = 1;

		const auto flush_pending_as_command = [&]()
		{
			const auto label = trim(pending);
			if (label.empty())
			{
				pending.clear();
				return;
			}

			if (stack.empty())
			{
				// statements outside of any block: section headers and defines
				const auto words = split_words(label);

				if (words.size() == 1 && (words[0] == "DEFINES" || words[0] == "ANIMATIONS" || words[0] == "CANNED_ANIMATIONS"
					|| words[0] == "STATECHANGES" || words[0] == "EVENTS"))
				{
					section = std::string(words[0]);
					out.sections.emplace_back(section, pending_line);
				}
				else if (!words.empty() && to_lower(words[0]) == "set")
				{
					pa_define def;
					def.line = pending_line;

					const auto eq = label.find('=');
					if (words.size() >= 3 && eq != std::string_view::npos)
					{
						def.type = std::string(words[1]);
						def.name = std::string(words[2]);
						def.value = std::string(trim(label.substr(eq + 1)));
						out.defines.push_back(std::move(def));
					}
					else
					{
						out.errors.push_back("line " + std::to_string(pending_line) + ": malformed define, expected 'set <type> <name> = <value>'");
					}
				}
				else
				{
					out.errors.push_back("line " + std::to_string(pending_line) + ": unexpected '" + std::string(label) + "' outside of a block");
				}
			}
			else
			{
				const auto words = split_words(label);
				pa_command cmd;
				cmd.line = pending_line;
				cmd.part = std::string(words[0]);

				std::size_t rest = 1;
				if (is_anim_part(words[0]) && words.size() >= 2)
				{
					cmd.anim = std::string(words[1]);
					rest = 2;
				}

				for (std::size_t w = rest; w < words.size(); w++)
				{
					cmd.extra += (cmd.extra.empty() ? "" : " ") + std::string(words[w]);
				}

				out.blocks[stack.back()].commands.push_back(std::move(cmd));
				out.command_count++;
			}

			pending.clear();
		};

		std::size_t i = 0;
		while (i <= src.size())
		{
			const char c = (i < src.size()) ? src[i] : '\n';

			if (c == '\n')
			{
				// a label stays pending across blank lines until we know if a '{' follows
				if (!trim(pending).empty())
				{
					// peek: is the next significant char a '{' ?
					std::size_t k = i + 1;
					while (k < src.size() && std::isspace(static_cast<unsigned char>(src[k])))
					{
						k++;
					}

					if (!(k < src.size() && src[k] == '{'))
					{
						flush_pending_as_command();
					}
				}

				line++;
			}
			else if (c == '{')
			{
				const auto label = trim(pending);
				if (label.empty())
				{
					out.errors.push_back("line " + std::to_string(line) + ": '{' without a label");
				}

				pa_block block;
				block.label = std::string(label);
				block.line = label.empty() ? line : pending_line;
				block.depth = static_cast<int>(stack.size());
				block.parent = stack.empty() ? -1 : stack.back();
				block.section = section;

				stack.push_back(static_cast<int>(out.blocks.size()));
				out.blocks.push_back(std::move(block));
				pending.clear();
			}
			else if (c == '}')
			{
				flush_pending_as_command();

				if (stack.empty())
				{
					out.errors.push_back("line " + std::to_string(line) + ": unmatched '}'");
				}
				else
				{
					stack.pop_back();
				}
			}
			else
			{
				if (trim(pending).empty())
				{
					pending_line = line;
				}

				pending += c;
			}

			i++;
		}

		if (!stack.empty())
		{
			out.errors.push_back("unexpected end of file: " + std::to_string(stack.size()) + " unclosed '{'");
		}

		return out.errors.empty();
	}

	// ------------------------------------------------------------------------------------------
	// gsc

	namespace
	{
		enum class gk : std::uint8_t { ident, string, number, punct, anim, preproc };

		struct gtok
		{
			gk kind;
			std::string_view text;	// string: contents without quotes, anim: without '%', preproc: without '#'
			int line;
		};

		void tokenize_gsc(std::string_view src, std::vector<gtok>& out)
		{
			const std::size_t n = src.size();
			std::size_t i = 0;
			int line = 1;

			const auto prev_is_operand = [&]() -> bool
			{
				if (out.empty())
				{
					return false;
				}

				const auto& p = out.back();
				switch (p.kind)
				{
				case gk::ident: return p.text != "return" && p.text != "case";
				case gk::number:
				case gk::string:
				case gk::anim: return true;
				case gk::punct: return p.text == ")" || p.text == "]";
				default: return false;
				}
			};

			while (i < n)
			{
				const char c = src[i];
				const char c1 = (i + 1 < n) ? src[i + 1] : '\0';

				if (c == '\n')
				{
					line++;
					i++;
				}
				else if (c == ' ' || c == '\t' || c == '\r')
				{
					i++;
				}
				else if (c == '/' && c1 == '/')
				{
					while (i < n && src[i] != '\n')
					{
						i++;
					}
				}
				else if (c == '/' && c1 == '*')
				{
					i += 2;
					while (i < n && !(src[i] == '*' && i + 1 < n && src[i + 1] == '/'))
					{
						if (src[i] == '\n')
						{
							line++;
						}

						i++;
					}

					i = std::min(n, i + 2);
				}
				else if ((c == '/' && c1 == '#') || (c == '#' && c1 == '/'))
				{
					i += 2; // dev block markers
				}
				else if (c == '"')
				{
					std::size_t j = i + 1;
					while (j < n && src[j] != '"' && src[j] != '\n')
					{
						j += (src[j] == '\\' && j + 1 < n) ? 2 : 1;
					}

					j = std::min(j, n);
					out.push_back({ gk::string, src.substr(i + 1, j - i - 1), line });
					i = std::min(n, j + 1);
				}
				else if (c == '#' && is_ident_start(c1))
				{
					std::size_t j = i + 1;
					while (j < n && is_ident_char(src[j]))
					{
						j++;
					}

					out.push_back({ gk::preproc, src.substr(i + 1, j - i - 1), line });
					i = j;
				}
				else if (c == '%' && is_ident_start(c1) && !prev_is_operand())
				{
					std::size_t j = i + 1;
					while (j < n && is_ident_char(src[j]))
					{
						j++;
					}

					out.push_back({ gk::anim, src.substr(i + 1, j - i - 1), line });
					i = j;
				}
				else if (is_ident_start(c))
				{
					std::size_t j = i + 1;
					while (j < n && is_ident_char(src[j]))
					{
						j++;
					}

					out.push_back({ gk::ident, src.substr(i, j - i), line });
					i = j;
				}
				else if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && std::isdigit(static_cast<unsigned char>(c1))))
				{
					std::size_t j = i + 1;
					while (j < n && (std::isalnum(static_cast<unsigned char>(src[j])) || src[j] == '.'))
					{
						j++;
					}

					out.push_back({ gk::number, src.substr(i, j - i), line });
					i = j;
				}
				else
				{
					out.push_back({ gk::punct, src.substr(i, 1), line });
					i++;
				}
			}
		}

		bool is_punct(const gtok& t, char c)
		{
			return t.kind == gk::punct && t.text.size() == 1 && t.text[0] == c;
		}

		bool is_anim_call(std::string_view name)
		{
			const auto l = to_lower(name);
			return l.starts_with("anim_") || l == "setanim" || l == "setanimknob" || l == "setanimknoball" || l == "setflaggedanim"
				|| l == "setflaggedanimknob" || l == "setflaggedanimknoball" || l == "setflaggedanimknoballrestart"
				|| l == "setflaggedanimrestart" || l == "setanimrestart" || l == "setanimknobrestart" || l == "setanimknoballrestart"
				|| l == "clearanim" || l == "scriptmodelplayanim" || l == "playanim" || l == "animscripted" || l == "animrelative";
		}
	}

	void scan_gsc(std::string_view text, gsc_file& out)
	{
		out.trees.clear();
		out.refs.clear();
		out.scenes.clear();
		out.calls.clear();
		out.functions.clear();

		std::vector<gtok> toks;
		toks.reserve(text.size() / 4);
		tokenize_gsc(text, toks);

		std::string tree;
		int depth = 0;
		const std::size_t n = toks.size();

		for (std::size_t i = 0; i < n; i++)
		{
			const auto& t = toks[i];

			if (t.kind == gk::punct)
			{
				if (t.text == "{")
				{
					depth++;
				}
				else if (t.text == "}")
				{
					depth = std::max(0, depth - 1);
				}

				continue;
			}

			if (t.kind == gk::preproc)
			{
				if (to_lower(t.text) == "using_animtree")
				{
					// #using_animtree( "name" );
					for (std::size_t k = i + 1; k < n && k < i + 4; k++)
					{
						if (toks[k].kind == gk::string)
						{
							tree = to_lower(toks[k].text);
							out.trees.emplace_back(tree, t.line);
							break;
						}
					}
				}

				continue;
			}

			if (t.kind == gk::anim)
			{
				out.refs.push_back({ std::string(t.text), tree, t.line });
				continue;
			}

			if (t.kind != gk::ident)
			{
				continue;
			}

			// level.scr_xxx[ "a" ][ "b" ][ 0 ] = value;
			if (t.text == "level" && i + 3 < n && is_punct(toks[i + 1], '.') && toks[i + 2].kind == gk::ident
				&& to_lower(toks[i + 2].text).starts_with("scr_") && is_punct(toks[i + 3], '['))
			{
				gsc_scene scene;
				scene.table = to_lower(toks[i + 2].text);
				scene.tree = tree;
				scene.line = t.line;

				std::size_t k = i + 3;
				int key = 0;

				while (k < n && is_punct(toks[k], '['))
				{
					std::string text_key;
					int bracket = 1;
					k++;

					while (k < n && bracket > 0)
					{
						if (is_punct(toks[k], '['))
						{
							bracket++;
						}
						else if (is_punct(toks[k], ']'))
						{
							if (--bracket == 0)
							{
								break;
							}
						}

						text_key += std::string(toks[k].text);
						k++;
					}

					k++; // closing ]

					switch (key++)
					{
					case 0: scene.animname = text_key; break;
					case 1: scene.scene = text_key; break;
					case 2: scene.index = text_key; break;
					default: break;
					}
				}

				if (k < n && is_punct(toks[k], '=') && !(k + 1 < n && is_punct(toks[k + 1], '=')))
				{
					k++;
					while (k < n && !is_punct(toks[k], ';'))
					{
						if (toks[k].kind == gk::anim)
						{
							scene.value += "%";
						}
						else if (toks[k].kind == gk::preproc)
						{
							scene.value += "#";
						}
						else if (toks[k].kind == gk::string)
						{
							scene.value += "\"" + std::string(toks[k].text) + "\"";
							k++;
							continue;
						}

						scene.value += std::string(toks[k].text);
						k++;
					}

					out.scenes.push_back(std::move(scene));
				}

				// do not skip the tokens: %anims on the right hand side still have to be recorded as refs
				continue;
			}

			if (i + 1 < n && is_punct(toks[i + 1], '('))
			{
				if (depth == 0)
				{
					// function definition: name( args ) {
					std::size_t k = i + 2;
					int paren = 1;
					while (k < n && paren > 0)
					{
						paren += is_punct(toks[k], '(') ? 1 : is_punct(toks[k], ')') ? -1 : 0;
						k++;
					}

					if (k < n && is_punct(toks[k], '{'))
					{
						out.functions.emplace_back(std::string(t.text), t.line);
					}
				}
				else if (is_anim_call(t.text))
				{
					gsc_anim_call call;
					call.func = std::string(t.text);
					call.line = t.line;

					std::size_t k = i + 2;
					int paren = 1;
					while (k < n && paren > 0)
					{
						if (is_punct(toks[k], '('))
						{
							paren++;
						}
						else if (is_punct(toks[k], ')'))
						{
							paren--;
						}
						else if (toks[k].kind == gk::string)
						{
							call.strings.emplace_back(toks[k].text);
						}

						k++;
					}

					out.calls.push_back(std::move(call));
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------
	// files

	bool read_text_file(const std::string& path, std::string& out)
	{
		std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

		if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF && static_cast<unsigned char>(out[1]) == 0xBB
			&& static_cast<unsigned char>(out[2]) == 0xBF)
		{
			out.erase(0, 3);
		}

		return true;
	}

	std::string relative_slash_path(const std::string& root, const std::string& abs_path)
	{
		std::error_code ec;
		auto rel = std::filesystem::relative(std::filesystem::u8path(abs_path), std::filesystem::u8path(root), ec);
		if (ec)
		{
			return abs_path;
		}

		auto s = rel.generic_u8string();
		return std::string(s.begin(), s.end());
	}

	// ------------------------------------------------------------------------------------------
	// index

	void anim_index::clear()
	{
		roots.clear();
		trees.clear();
		gsc.clear();
		player = {};
		has_player = false;
		diagnostics.clear();
		total_refs = 0;
		total_scenes = 0;
	}

	void anim_index::load_file(const std::string& root, const std::string& abs_path)
	{
		const auto rel = relative_slash_path(root, abs_path);
		const auto rel_lower = to_lower(rel);
		const auto ext_pos = rel_lower.find_last_of('.');
		const auto ext = (ext_pos == std::string::npos) ? std::string() : rel_lower.substr(ext_pos);

		if (ext != ".atr" && ext != ".gsc" && ext != ".csc" && rel_lower != "mp/playeranim.script")
		{
			return;
		}

		std::string text;
		if (!read_text_file(abs_path, text))
		{
			return;
		}

		if (ext == ".atr")
		{
			const auto slash = rel_lower.find_last_of('/');
			const auto name = rel_lower.substr(slash == std::string::npos ? 0 : slash + 1, ext_pos - (slash == std::string::npos ? 0 : slash + 1));

			auto& tree = trees[name];
			if (!tree.path.empty() && tree.path != abs_path)
			{
				// the same tree in a later root: the game builds the union of both ( the tree mounted from
				// the zones first, loose files appended ), so only add the names that are new
				animtree other;
				parse_animtree(text, other);

				for (const auto& node : other.nodes)
				{
					tree.lookup.emplace(to_lower(node.name), -1);
				}

				tree.merged_paths.push_back(abs_path);
				return;
			}

			tree.merged_paths.clear();

			tree.name = name;
			tree.path = abs_path;
			parse_animtree(text, tree);
		}
		else if (ext == ".script")
		{
			if (has_player && player.path != abs_path)
			{
				return;
			}

			player.path = abs_path;
			has_player = true;
			parse_playeranim(text, player);
		}
		else
		{
			auto& file = gsc[rel_lower];
			if (!file.path.empty() && file.path != abs_path)
			{
				return;
			}

			file.path = abs_path;
			file.rel = rel;
			scan_gsc(text, file);
		}
	}

	void anim_index::recount()
	{
		total_refs = 0;
		total_scenes = 0;

		for (const auto& [rel, file] : gsc)
		{
			total_refs += file.refs.size();
			total_scenes += file.scenes.size();
		}
	}

	void anim_index::build(const std::vector<std::string>& new_roots)
	{
		const auto keep_xanims = std::move(known_xanims_);
		clear();
		known_xanims_ = keep_xanims;
		roots = new_roots;

		for (const auto& root : roots)
		{
			std::error_code ec;
			const auto root_path = std::filesystem::u8path(root);

			if (!std::filesystem::is_directory(root_path, ec))
			{
				continue;
			}

			auto it = std::filesystem::recursive_directory_iterator(root_path, std::filesystem::directory_options::skip_permission_denied, ec);
			const auto end = std::filesystem::recursive_directory_iterator();

			for (; !ec && it != end; it.increment(ec))
			{
				const auto& entry = *it;
				const auto name = entry.path().filename().u8string();

				if (entry.is_directory(ec))
				{
					if (!name.empty() && name[0] == u8'.')
					{
						it.disable_recursion_pending(); // .git and friends
					}

					continue;
				}

				const auto u8 = entry.path().u8string();
				load_file(root, std::string(u8.begin(), u8.end()));
			}
		}

		recount();
		validate();
	}

	void anim_index::update_file(const std::string& abs_path)
	{
		for (const auto& root : roots)
		{
			const auto rel = relative_slash_path(root, abs_path);
			if (rel.empty() || rel.starts_with("..") || rel == abs_path)
			{
				continue;
			}

			load_file(root, abs_path);
			break;
		}

		recount();
		validate();
	}

	const animtree* anim_index::find_tree(std::string_view name) const
	{
		const auto it = trees.find(to_lower(name));
		return it == trees.end() ? nullptr : &it->second;
	}

	const gsc_file* anim_index::find_gsc(std::string_view abs_path) const
	{
		for (const auto& [rel, file] : gsc)
		{
			if (file.path == abs_path)
			{
				return &file;
			}
		}

		return nullptr;
	}

	void anim_index::set_known_xanims(std::vector<std::string> names)
	{
		known_xanims_.clear();
		for (auto& name : names)
		{
			known_xanims_.insert(to_lower(name));
		}
	}

	bool anim_index::is_known_xanim(std::string_view name) const
	{
		return known_xanims_.contains(to_lower(name));
	}

	void anim_index::validate()
	{
		diagnostics.clear();

		for (const auto& [name, tree] : trees)
		{
			for (const auto& err : tree.errors)
			{
				int line = 0;
				if (err.starts_with("line "))
				{
					line = std::atoi(err.c_str() + 5);
				}

				diagnostics.push_back({ severity::error, tree.path, line, "animtree '" + name + "': " + err });
			}
			for (const auto& warn : tree.warnings)
			{
				diagnostics.push_back({ severity::warning, tree.path, std::atoi(warn.c_str() + 5), "animtree '" + name + "': " + warn });
			}


		}

		if (has_player)
		{
			for (const auto& err : player.errors)
			{
				int line = 0;
				if (err.starts_with("line "))
				{
					line = std::atoi(err.c_str() + 5);
				}

				diagnostics.push_back({ severity::error, player.path, line, "playeranim.script: " + err });
			}

			if (const auto* mp_tree = find_tree("multiplayer"))
			{
				for (const auto& block : player.blocks)
				{
					for (const auto& cmd : block.commands)
					{
						if (!cmd.anim.empty() && !known_xanims_.empty() && !is_known_xanim(cmd.anim))
						{
							diagnostics.push_back({ severity::warning, player.path, cmd.line,
								"player animation '" + cmd.anim + "' is not an xanim of the game ( not in the xanim list )" });
						}

						if (!cmd.anim.empty() && !mp_tree->contains(cmd.anim))
						{
							// a warning, not an error: the client takes both files out of its zones, so a loose
							// multiplayer.atr that is older than the script is common and harmless there
							diagnostics.push_back({ severity::warning, player.path, cmd.line,
								"player animation '" + cmd.anim + "' is not part of animtrees/multiplayer.atr" });
						}
					}
				}
			}
			else
			{
				diagnostics.push_back({ severity::warning, player.path, 0,
					"animtrees/multiplayer.atr was not found, player animations can not be validated" });
			}
		}

		for (const auto& [rel, file] : gsc)
		{
			std::unordered_set<std::string> missing_trees;

			for (const auto& ref : file.refs)
			{
				if (ref.tree.empty())
				{
					diagnostics.push_back({ severity::error, file.path, ref.line,
						"%" + ref.anim + " is used before any #using_animtree" });
					continue;
				}

				const auto* tree = find_tree(ref.tree);
				if (!tree)
				{
					if (missing_trees.insert(ref.tree).second)
					{
						diagnostics.push_back({ severity::warning, file.path, ref.line,
							"animtree '" + ref.tree + "' ( animtrees/" + ref.tree + ".atr ) was not found in the script roots" });
					}

					continue;
				}

				// in the tree, but the game ships no such animation: it plays nothing ( T-pose )
				if (!known_xanims_.empty() && tree->contains(ref.anim) && !is_known_xanim(ref.anim))
				{
					const auto node = tree->lookup.find(to_lower(ref.anim));
					const bool group = node != tree->lookup.end() && node->second >= 0 && tree->nodes[node->second].group;

					if (!group)
					{
						diagnostics.push_back({ severity::warning, file.path, ref.line,
							"%" + ref.anim + " is in animtree '" + ref.tree + "' but is not an xanim of the game ( not in the xanim list )" });
					}
				}

				if (!tree->contains(ref.anim))
				{
					diagnostics.push_back({ severity::error, file.path, ref.line,
						"%" + ref.anim + " is not part of animtree '" + ref.tree
						+ "' - the server would append it and shift every later anim index ( T-pose / wrong clips in-game )" });
				}
			}
		}

		std::stable_sort(diagnostics.begin(), diagnostics.end(), [](const diagnostic& a, const diagnostic& b)
		{
			return static_cast<int>(a.sev) > static_cast<int>(b.sev);
		});
	}
}
