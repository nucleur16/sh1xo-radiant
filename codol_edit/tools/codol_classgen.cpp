// codol_classgen: builds the entity class table of codol_edit from the shipped maps.
//
//   codol_classgen --game <install dir> --out <codol_edit/src/class_catalog_data.inc> [--report <file.txt>]
//
// Opens every fast file in <install>/client_shipRetail/patch and <install>/client_shipRetail/chinese
// ( *.ff; "<name>_load.ff" loading screens are skipped; a name that is in both folders is read from patch ),
// takes the entity string of each zone that has map entities and counts, per classname: entities, maps,
// entities with a compiled model ( "model" "*N" / "?N" ), every key with its most frequent values, and the
// spawnflags values. The install is only read.

#include <codol_zone/fastfile.hpp>
#include <codol_zone/zone.hpp>
#include <codol_zone/assets.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace codol;

namespace
{
	struct key_stats
	{
		int count = 0;
		std::map<std::string, int> values;
	};

	struct class_stats
	{
		int count = 0, point = 0, brush = 0;
		std::set<std::string> maps;
		std::map<std::string, key_stats> keys;
		std::map<int, int> spawnflags;
	};

	using pairs = std::vector<std::pair<std::string, std::string>>;

	// "{ "k" "v" .. } { .. }"
	std::vector<pairs> parse_entities(const std::string_view text)
	{
		std::vector<pairs> out;
		std::size_t pos = 0;
		bool inside = false;
		pairs current;

		const auto read_string = [&](std::string& s)
		{
			const auto end = text.find('"', pos + 1);
			if (end == std::string_view::npos)
			{
				pos = text.size();
				return false;
			}

			s = std::string(text.substr(pos + 1, end - pos - 1));
			pos = end + 1;
			return true;
		};

		while (pos < text.size())
		{
			const char c = text[pos];

			if (c == '{')
			{
				inside = true;
				current.clear();
				pos++;
			}
			else if (c == '}')
			{
				if (inside)
				{
					out.push_back(current);
				}

				inside = false;
				pos++;
			}
			else if (c == '"' && inside)
			{
				std::string key, value;
				if (!read_string(key))
				{
					break;
				}

				while (pos < text.size() && text[pos] != '"' && text[pos] != '}')
				{
					pos++;
				}

				if (pos >= text.size() || text[pos] != '"' || !read_string(value))
				{
					break;
				}

				current.emplace_back(std::move(key), std::move(value));
			}
			else
			{
				pos++;
			}
		}

		return out;
	}

	std::string c_string(const std::string_view s)
	{
		std::string out = "\"";
		for (const char ch : s)
		{
			const auto c = static_cast<unsigned char>(ch);
			if (c == '"' || c == '\\')
			{
				out += '\\';
				out += ch;
			}
			else if (c < 0x20 || c >= 0x7F)
			{
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\%03o", c);
				out += buf;
			}
			else if (c == '?')
			{
				out += "\\?";	// no trigraphs
			}
			else
			{
				out += ch;
			}
		}

		return out + "\"";
	}
}

int main(const int argc, char** argv)
{
	std::string game, out_path, report_path;

	for (int i = 1; i + 1 < argc; i += 2)
	{
		const std::string key = argv[i];
		if (key == "--game") { game = argv[i + 1]; }
		else if (key == "--out") { out_path = argv[i + 1]; }
		else if (key == "--report") { report_path = argv[i + 1]; }
	}

	if (game.empty() || out_path.empty())
	{
		std::fprintf(stderr, "usage: codol_classgen --game <install dir> --out <class_catalog_data.inc> [--report <file>]\n");
		return 1;
	}

	const std::filesystem::path root = std::filesystem::path(game);

	zone::ClientKey key;
	if (const auto status = key.load_from_exe(root / "C1.exe"); !status)
	{
		std::fprintf(stderr, "codol_classgen: client key: %s\n", status.message.c_str());
		return 1;
	}

	std::map<std::string, class_stats> classes;
	std::set<std::string> seen;
	std::vector<std::string> used_zones;
	int opened = 0, failed = 0;

	for (const char* dir : { "patch", "chinese" })
	{
		std::error_code ec;
		std::vector<std::filesystem::path> files;

		for (const auto& entry : std::filesystem::directory_iterator(root / "client_shipRetail" / dir, ec))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".ff")
			{
				files.push_back(entry.path());
			}
		}

		std::sort(files.begin(), files.end());

		for (const auto& file : files)
		{
			const std::string name = file.stem().string();
			if (name.ends_with("_load") || !seen.insert(name).second)
			{
				continue;
			}

			zone::Zone z;
			if (const auto status = z.open(file, key); !status)
			{
				std::fprintf(stderr, "  %s: %s\n", name.c_str(), status.message.c_str());
				failed++;
				continue;
			}

			opened++;

			zone::MapEntsView ents;
			const auto address = zone::find_map_ents(z);
			if (!address || !zone::read_map_ents(z, address, ents))
			{
				continue;
			}

			const auto entities = parse_entities(ents.entityString);
			used_zones.push_back(name);
			std::printf("  %-40s %6zu entities\n", name.c_str(), entities.size());

			for (const auto& ent : entities)
			{
				std::string classname;
				bool compiled_model = false;

				for (const auto& [k, v] : ent)
				{
					if (k == "classname")
					{
						classname = v;
					}
					else if (k == "model" && v.size() > 1 && (v[0] == '*' || v[0] == '?'))
					{
						compiled_model = true;
					}
				}

				if (classname.empty())
				{
					continue;
				}

				class_stats& cs = classes[classname];
				cs.count++;
				(compiled_model ? cs.brush : cs.point)++;
				cs.maps.insert(name);

				for (const auto& [k, v] : ent)
				{
					if (k == "classname")
					{
						continue;
					}

					key_stats& ks = cs.keys[k];
					ks.count++;
					ks.values[v.substr(0, 96)]++;

					if (k == "spawnflags")
					{
						cs.spawnflags[std::atoi(v.c_str())]++;
					}
				}
			}
		}
	}

	// ---- write the table
	std::string out;
	out += "// Generated by codol_classgen from the entity strings of " + std::to_string(used_zones.size())
		+ " map zones of Call of Duty Online 1.1.1.13 - do not edit.\n// zones:";

	for (std::size_t i = 0; i < used_zones.size(); i++)
	{
		out += (i % 8 == 0) ? "\n//   " : " ";
		out += used_zones[i];
	}

	out += "\n\nconstexpr int catalog_map_count = " + std::to_string(used_zones.size()) + ";\n\n";

	std::string examples = "constexpr std::string_view catalog_examples[] =\n{\n";
	std::string keys = "constexpr raw_key catalog_keys[] =\n{\n";
	std::string flags = "constexpr raw_flag catalog_flags[] =\n{\n";
	std::string rows = "constexpr raw_class catalog_classes[] =\n{\n";
	std::string report;

	int example_index = 0, key_index = 0, flag_index = 0;

	for (const auto& [name, cs] : classes)
	{
		// keys: most frequent first
		std::vector<std::pair<std::string, const key_stats*>> sorted_keys;
		for (const auto& [k, ks] : cs.keys)
		{
			sorted_keys.emplace_back(k, &ks);
		}

		std::stable_sort(sorted_keys.begin(), sorted_keys.end(), [](const auto& a, const auto& b) { return a.second->count > b.second->count; });

		const int first_key = key_index;
		keys += "\t// " + name + "\n";
		report += name + "  ( " + std::to_string(cs.count) + " in " + std::to_string(cs.maps.size()) + " maps, " + std::to_string(cs.point)
			+ " point, " + std::to_string(cs.brush) + " with a compiled model )\n";

		for (const auto& [k, ks] : sorted_keys)
		{
			std::vector<std::pair<std::string, int>> values(ks->values.begin(), ks->values.end());
			std::stable_sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

			const int first_example = example_index;
			const std::size_t count = std::min<std::size_t>(values.size(), 4);

			examples += "\t";
			report += "    " + k + " ( " + std::to_string(ks->count) + " ):";

			for (std::size_t i = 0; i < count; i++)
			{
				examples += c_string(values[i].first) + ", ";
				report += " \"" + values[i].first + "\"";
				example_index++;
			}

			examples += "\n";
			report += "\n";

			keys += "\t{ " + c_string(k) + ", " + std::to_string(ks->count) + ", " + std::to_string(first_example) + ", " + std::to_string(count) + " },\n";
			key_index++;
		}

		std::vector<std::pair<int, int>> sorted_flags(cs.spawnflags.begin(), cs.spawnflags.end());
		std::stable_sort(sorted_flags.begin(), sorted_flags.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

		const int first_flag = flag_index;
		const std::size_t flag_count = std::min<std::size_t>(sorted_flags.size(), 8);

		for (std::size_t i = 0; i < flag_count; i++)
		{
			flags += "\t{ " + std::to_string(sorted_flags[i].first) + ", " + std::to_string(sorted_flags[i].second) + " },\n";
			flag_index++;
		}

		rows += "\t{ " + c_string(name) + ", " + std::to_string(cs.count) + ", " + std::to_string(cs.maps.size()) + ", " + std::to_string(cs.point) + ", "
			+ std::to_string(cs.brush) + ", " + std::to_string(first_key) + ", " + std::to_string(key_index - first_key) + ", " + std::to_string(first_flag) + ", "
			+ std::to_string(flag_count) + " },\n";
	}

	// arrays must not be empty
	examples += "\t\"\",\n};\n\n";
	keys += "\t{ \"\", 0, 0, 0 },\n};\n\n";
	flags += "\t{ 0, 0 },\n};\n\n";
	rows += "\t{ \"\", 0, 0, 0, 0, 0, 0, 0, 0 },\n};\n";

	out += examples + keys + flags + rows;

	{
		std::ofstream file(std::filesystem::path(out_path), std::ios::binary | std::ios::trunc);
		if (!file.is_open())
		{
			std::fprintf(stderr, "codol_classgen: can not write %s\n", out_path.c_str());
			return 1;
		}

		file.write(out.data(), static_cast<std::streamsize>(out.size()));
	}

	if (!report_path.empty())
	{
		std::ofstream file(std::filesystem::path(report_path), std::ios::binary | std::ios::trunc);
		file.write(report.data(), static_cast<std::streamsize>(report.size()));
	}

	std::printf("codol_classgen: %d zones opened ( %d failed ), %zu with map entities, %zu classes, %d keys -> %s\n",
		opened, failed, used_zones.size(), classes.size(), key_index, out_path.c_str());
	return 0;
}
