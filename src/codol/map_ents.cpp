#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

namespace codol::mapents
{
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

		// `"key" "value"` -> true
		bool parse_pair(std::string_view line, std::string& key, std::string& value)
		{
			if (line.size() < 5 || line.front() != '"')
			{
				return false;
			}

			const auto k_end = line.find('"', 1);
			if (k_end == std::string_view::npos)
			{
				return false;
			}

			const auto v_begin = line.find('"', k_end + 1);
			const auto v_end = line.rfind('"');
			if (v_begin == std::string_view::npos || v_end <= v_begin)
			{
				return false;
			}

			key = std::string(line.substr(1, k_end - 1));
			value = std::string(line.substr(v_begin + 1, v_end - v_begin - 1));
			return true;
		}

		bool is_model_reference(const std::string* model)
		{
			return model && model->size() >= 2 && ((*model)[0] == '*' || (*model)[0] == '?')
				&& std::isdigit(static_cast<unsigned char>((*model)[1]));
		}

		bool is_codol_key(std::string_view key)
		{
			return key.starts_with("codol_");
		}

		template <typename Fn>
		void for_each_line(std::string_view text, Fn&& fn)
		{
			std::size_t pos = 0;
			int line_no = 1;

			while (pos <= text.size())
			{
				auto end = text.find('\n', pos);
				if (end == std::string_view::npos)
				{
					end = text.size();
				}

				if (!fn(text.substr(pos, end - pos), line_no))
				{
					return;
				}

				pos = end + 1;
				line_no++;
			}
		}

		std::string hex8(const std::uint32_t value)
		{
			char buf[16];
			std::snprintf(buf, sizeof(buf), "%08x", value);
			return buf;
		}

		// entities of a radiant .map, worldspawn first. Brushes / patches are skipped ( has_brushes is set )
		bool read_radiant_map(std::string_view map_text, std::vector<entity>& ents, std::string& error)
		{
			ents.clear();
			entity current;
			int depth = 0;

			for_each_line(map_text, [&](std::string_view raw, const int line_no) -> bool
			{
				const auto line = trim(raw);

				if (line.empty() || line.starts_with("//"))
				{
					return true;
				}

				if (depth == 0)
				{
					// header ( "iwmap 4", layer flags ) is ignored
					if (line == "{")
					{
						depth = 1;
						current = {};
						current.line = line_no;
					}

					return true;
				}

				if (depth == 1)
				{
					if (line == "{")
					{
						depth = 2;
						current.has_brushes = true;
					}
					else if (line == "}")
					{
						depth = 0;
						ents.push_back(std::move(current));
					}
					else
					{
						std::string key, value;
						if (parse_pair(line, key, value))
						{
							current.pairs.emplace_back(std::move(key), std::move(value));
						}
					}

					return true;
				}

				// inside a brush / patch
				if (line == "{")
				{
					depth++;
				}
				else if (line == "}")
				{
					depth--;
				}

				return true;
			});

			if (depth != 0)
			{
				error = "the map file ends inside an entity";
				return false;
			}

			if (ents.empty())
			{
				error = "the map has no entities";
				return false;
			}

			const auto world = std::find_if(ents.begin(), ents.end(), [](const entity& e)
			{
				const auto* cls = e.find("classname");
				return cls && *cls == "worldspawn";
			});

			if (world == ents.end())
			{
				error = "the map has no worldspawn";
				return false;
			}

			// worldspawn first, like the game expects
			std::rotate(ents.begin(), world, world + 1);
			return true;
		}

		bool read_map_identity(const entity& world, export_result& out, std::string& error)
		{
			const auto* crc = world.find(KEY_BASE_CRC);
			const auto* name = world.find(KEY_MAP_NAME);

			if (!crc || !name || name->empty())
			{
				error = "this map was not imported from a CoDOL map ( the worldspawn has no \""
					+ std::string(KEY_BASE_CRC) + "\" / \"" + std::string(KEY_MAP_NAME) + "\" keys )";
				return false;
			}

			char* end = nullptr;
			out.base_crc = static_cast<std::uint32_t>(std::strtoul(crc->c_str(), &end, 16));
			if (!end || *end != '\0' || crc->empty())
			{
				error = "\"" + std::string(KEY_BASE_CRC) + "\" is not a hex number";
				return false;
			}

			out.map_name = *name;
			return true;
		}

		// checks on an entity ( not the worldspawn ) that is about to be exported. false: skip it, a warning was added
		bool exportable(const entity& ent, const std::string& cls_name, std::vector<std::string>& warnings)
		{
			const auto where = "line " + std::to_string(ent.line) + ": ";

			if (cls_name.empty())
			{
				warnings.push_back(where + "entity without a classname skipped");
				return false;
			}

			if (cls_name == "worldspawn")
			{
				warnings.push_back(where + "second worldspawn skipped");
				return false;
			}

			if (cls_name == CLASS_BMODEL)
			{
				warnings.push_back(where + "codol_bmodel without its \"" + KEY_ORIG_CLASS + "\" / \"model\" keys skipped");
				return false;
			}

			if (ent.has_brushes)
			{
				// a brush entity drawn in radiant has no compiled hull in the zone
				warnings.push_back(where + "brush entity '" + cls_name
					+ "' skipped ( brush models can not be added without recompiling the map )");
				return false;
			}

			if (cls_name == "misc_model" || cls_name == "misc_prefab")
			{
				warnings.push_back(where + cls_name
					+ " skipped ( static geometry is baked into the zone, use script_model for placed models )");
				return false;
			}

			return true;
		}

		// codol_bmodel -> its real class, and all editor-only keys removed
		void restore_game_keys(entity& ent)
		{
			const auto* cls = ent.find("classname");
			const auto* original = ent.find(KEY_ORIG_CLASS);

			if (cls && *cls == CLASS_BMODEL && original && !original->empty() && is_model_reference(ent.find("model")))
			{
				const std::string restored = *original;
				ent.set("classname", restored);
			}

			ent.pairs.erase(std::remove_if(ent.pairs.begin(), ent.pairs.end(), [](const auto& p) { return is_codol_key(p.first); }), ent.pairs.end());
		}

		void finish_export(export_result& out, const std::vector<entity>& result)
		{
			out.exported = static_cast<int>(result.size());
			out.text = "// " + out.map_name + ": map entity override, exported from radiant ( iw3xo-radiant-codol ).\n"
				"// The server uses it in place of the zone's entity string while the zone's string has this CRC-32.\n"
				"// base_crc32 " + hex8(out.base_crc) + "\n"
				+ serialize_entity_string(result) + "\n";
		}

		int entity_id(const entity& ent)
		{
			const auto* id = ent.find(KEY_ID);
			if (!id || id->empty())
			{
				return -1;
			}

			char* end = nullptr;
			const long value = std::strtol(id->c_str(), &end, 10);
			return (end && *end == '\0' && value >= 0) ? static_cast<int>(value) : -1;
		}
	}

	const std::string* entity::find(std::string_view key) const
	{
		for (const auto& [k, v] : pairs)
		{
			if (k == key)
			{
				return &v;
			}
		}

		return nullptr;
	}

	void entity::set(std::string_view key, std::string_view value)
	{
		for (auto& [k, v] : pairs)
		{
			if (k == key)
			{
				v = std::string(value);
				return;
			}
		}

		pairs.emplace_back(std::string(key), std::string(value));
	}

	void entity::erase(std::string_view key)
	{
		pairs.erase(std::remove_if(pairs.begin(), pairs.end(), [&](const auto& p) { return p.first == key; }), pairs.end());
	}

	std::uint32_t crc32(std::string_view data)
	{
		std::uint32_t crc = 0xFFFFFFFFu;

		for (const char c : data)
		{
			crc ^= static_cast<std::uint8_t>(c);
			for (int k = 0; k < 8; k++)
			{
				crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
			}
		}

		return ~crc;
	}

	std::string serialize_entity_string(const std::vector<entity>& ents)
	{
		std::string out;

		for (std::size_t i = 0; i < ents.size(); i++)
		{
			out += i ? "\n{\n" : "{\n";

			for (const auto& [k, v] : ents[i].pairs)
			{
				out += "\"" + k + "\" \"" + v + "\"\n";
			}

			out += "}";
		}

		return out;
	}

	bool parse_entity_string(std::string_view text, entity_set& out, std::string& error)
	{
		out = {};
		error.clear();

		bool in_entity = false;
		bool saw_comment = false;
		entity current;

		for_each_line(text, [&](std::string_view raw, const int line_no) -> bool
		{
			const auto line = trim(raw);

			if (line.empty())
			{
				return true;
			}

			if (line.starts_with("//"))
			{
				saw_comment = true;

				unsigned value = 0;
				const std::string tmp(line);
				if (std::sscanf(tmp.c_str(), "// base_crc32 %8x", &value) == 1)
				{
					out.has_crc = true;
					out.base_crc = value;
				}

				return true;
			}

			if (line == "{")
			{
				if (in_entity)
				{
					error = "line " + std::to_string(line_no) + ": nested entity";
					return false;
				}

				in_entity = true;
				current = {};
				current.line = line_no;
				return true;
			}

			if (line == "}")
			{
				if (!in_entity)
				{
					error = "line " + std::to_string(line_no) + ": '}' without an entity";
					return false;
				}

				in_entity = false;
				out.ents.push_back(std::move(current));
				return true;
			}

			std::string key, value;
			if (!in_entity || !parse_pair(line, key, value))
			{
				error = "line " + std::to_string(line_no) + ": unexpected '" + std::string(line.substr(0, 60)) + "'";
				return false;
			}

			current.pairs.emplace_back(std::move(key), std::move(value));
			return true;
		});

		if (!error.empty())
		{
			return false;
		}

		if (in_entity)
		{
			error = "unexpected end: the last entity is not closed";
			return false;
		}

		if (out.ents.empty())
		{
			error = "no entities found";
			return false;
		}

		const auto* cls = out.ents.front().find("classname");
		if (!cls || *cls != "worldspawn")
		{
			error = "the first entity is not the worldspawn";
			return false;
		}

		// a plain entity string ( straight out of a zone ) is its own base
		if (!out.has_crc && !saw_comment)
		{
			auto body = text;
			while (!body.empty() && body.back() == '\0') // the game hashes everything up to the terminator
			{
				body.remove_suffix(1);
			}

			out.has_crc = true;
			out.base_crc = crc32(body);
		}

		return true;
	}

	bool extract_from_zone(std::string_view zone_bytes, std::string& ent_text)
	{
		static constexpr std::string_view needle = "\"classname\" \"worldspawn\"";

		const auto hit = zone_bytes.find(needle);
		if (hit == std::string_view::npos)
		{
			return false;
		}

		const auto start = zone_bytes.rfind('{', hit);
		auto end = zone_bytes.find('\0', hit);

		if (start == std::string_view::npos)
		{
			return false;
		}

		if (end == std::string_view::npos)
		{
			end = zone_bytes.size();
		}

		ent_text = std::string(zone_bytes.substr(start, end - start));
		return true;
	}

	std::string to_radiant_map(const entity_set& set, std::string_view map_name)
	{
		std::string out = "iwmap 4\n";

		for (std::size_t i = 0; i < set.ents.size(); i++)
		{
			entity ent = set.ents[i];

			if (i == 0)
			{
				ent.set(KEY_MAP_NAME, map_name);
				ent.set(KEY_BASE_CRC, hex8(set.base_crc));
			}
			else if (is_model_reference(ent.find("model")))
			{
				// the hull lives in the zone: show it as a point entity and remember what it was
				if (const auto* cls = ent.find("classname"))
				{
					const std::string original = *cls;
					ent.set(KEY_ORIG_CLASS, original);
				}

				ent.set("classname", CLASS_BMODEL);
			}

			ent.set(KEY_ID, std::to_string(i));

			out += "// entity " + std::to_string(i) + "\n{\n";

			for (const auto& [k, v] : ent.pairs)
			{
				out += "\"" + k + "\" \"" + v + "\"\n";
			}

			out += "}\n";
		}

		return out;
	}

	bool from_radiant_map(std::string_view map_text, export_result& out, std::string& error)
	{
		out = {};
		error.clear();

		std::vector<entity> ents;
		if (!read_radiant_map(map_text, ents, error) || !read_map_identity(ents.front(), out, error))
		{
			return false;
		}

		std::vector<entity> result;
		result.reserve(ents.size());

		for (std::size_t i = 0; i < ents.size(); i++)
		{
			entity ent = ents[i];
			restore_game_keys(ent);

			if (i != 0)
			{
				const auto* cls = ent.find("classname");
				if (!exportable(ent, cls ? *cls : std::string(), out.warnings))
				{
					continue;
				}
			}

			result.push_back(std::move(ent));
		}

		finish_export(out, result);
		return true;
	}

	bool merge_radiant_map(std::string_view current_map, std::string_view baseline_map, std::string_view original_ents,
		export_result& out, std::string& error)
	{
		out = {};
		error.clear();

		std::vector<entity> current, baseline;
		entity_set original;

		if (!read_radiant_map(current_map, current, error) || !read_map_identity(current.front(), out, error))
		{
			return false;
		}

		std::string sub_error;
		if (!read_radiant_map(baseline_map, baseline, sub_error))
		{
			error = "baseline map: " + sub_error;
			return false;
		}

		if (!parse_entity_string(original_ents, original, sub_error))
		{
			error = "original entity string: " + sub_error;
			return false;
		}

		// baseline entity by id ( the first one wins if radiant ever wrote duplicates )
		std::unordered_map<int, const entity*> baseline_by_id;
		for (std::size_t i = 0; i < baseline.size(); i++)
		{
			const int id = (i == 0) ? 0 : entity_id(baseline[i]);
			if (id >= 0)
			{
				baseline_by_id.emplace(id, &baseline[i]);
			}
		}

		std::vector<entity> result;
		result.reserve(current.size());

		for (std::size_t i = 0; i < current.size(); i++)
		{
			const entity& cur = current[i];
			const int id = (i == 0) ? 0 : entity_id(cur);

			const entity* orig = (id >= 0 && id < static_cast<int>(original.ents.size())) ? &original.ents[id] : nullptr;
			const entity* base = nullptr;

			if (const auto it = baseline_by_id.find(id); it != baseline_by_id.end())
			{
				base = it->second;
			}

			entity ent;
			ent.line = cur.line;
			ent.has_brushes = (i == 0) ? false : cur.has_brushes; // worldspawn brushes are reference geometry only

			if (!orig || !base)
			{
				// new entity ( or a map without a usable baseline ): take it as radiant saved it
				ent.pairs = cur.pairs;
				restore_game_keys(ent);
			}
			else
			{
				// 1. keys of the original entity, in their original order
				for (const auto& [key, orig_value] : orig->pairs)
				{
					// the class of a hull entity is carried by codol_classname while it is in radiant
					if (key == "classname")
					{
						const auto* cur_cls = cur.find("classname");
						const auto* base_cls = base->find("classname");

						if (cur_cls && *cur_cls == CLASS_BMODEL)
						{
							const auto* cur_real = cur.find(KEY_ORIG_CLASS);
							const auto* base_real = base->find(KEY_ORIG_CLASS);
							const bool edited = cur_real && !cur_real->empty() && (!base_real || *cur_real != *base_real);
							ent.pairs.emplace_back(key, edited ? *cur_real : orig_value);
						}
						else if (cur_cls && (!base_cls || *cur_cls != *base_cls))
						{
							ent.pairs.emplace_back(key, *cur_cls);	// class changed by the user
						}
						else
						{
							ent.pairs.emplace_back(key, orig_value);
						}

						continue;
					}

					const auto* cur_value = cur.find(key);
					const auto* base_value = base->find(key);

					if (!cur_value)
					{
						// gone: deleted by the user if the baseline still had it, otherwise radiant dropped it on load
						if (!base_value)
						{
							ent.pairs.emplace_back(key, orig_value);
						}
					}
					else if (base_value && *cur_value == *base_value)
					{
						ent.pairs.emplace_back(key, orig_value);	// untouched: exactly what the zone had
					}
					else
					{
						ent.pairs.emplace_back(key, *cur_value);	// edited
					}
				}

				// 2. keys the original entity did not have
				for (const auto& [key, cur_value] : cur.pairs)
				{
					if (is_codol_key(key) || orig->find(key))
					{
						continue;
					}

					const auto* base_value = base->find(key);
					if (base_value && *base_value == cur_value)
					{
						continue; // added by radiant itself ( eg. "origin" "0.0 0.0 0.0" on a hull entity )
					}

					ent.pairs.emplace_back(key, cur_value);
				}
			}

			if (i != 0)
			{
				const auto* cls = ent.find("classname");
				if (!exportable(ent, cls ? *cls : std::string(), out.warnings))
				{
					continue;
				}
			}

			result.push_back(std::move(ent));
		}

		finish_export(out, result);
		return true;
	}
}
