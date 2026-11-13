// codol_mapconv: rebuild an editable radiant .map from a shipped CoDOL map.
//
//   codol_mapconv --map <name> --ff <map.ff> --exe <client exe> --out <file.map> [--ents-out <file>]
//       native: entities, brushes and static models are read straight from the 1.1.1.13 fastfile
//
//   codol_mapconv --map <name> --ents <entities> --out <file.map>
//                 [--brushes <brushes.map.json>] [--models <smodels.map.json>] [--trigger-models <n>]
//
//   --ents      the entity string of the map: plain text, a .d3dbsp.ents override, or a decrypted .zone
//   --brushes   metadata of `CodolSoundDumper export-map --scope brushes --layout separate`
//   --models    metadata of `CodolSoundDumper export-map --scope static-models --layout instanced`
//
// Exit code 0 on success. A one line summary goes to stdout, errors to stderr.

#include "std_include.hpp"
#include "codol/map_zone.hpp"

#include <chrono>

using namespace codol;

namespace
{
	bool read_file(const std::string& path, std::string& out)
	{
		std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		return true;
	}

	bool ends_with_ci(const std::string& s, const std::string& suffix)
	{
		return s.size() >= suffix.size() && script::to_lower(s.substr(s.size() - suffix.size())) == suffix;
	}

	int fail(const std::string& message)
	{
		std::fprintf(stderr, "codol_mapconv: %s\n", message.c_str());
		return 1;
	}
}

int main(int argc, char** argv)
{
	std::string map_name, ents_path, out_path, brushes_path, models_path, ff_path, exe_path, ents_out_path;
	int trigger_models = -1;

	for (int i = 1; i + 1 < argc; i += 2)
	{
		const std::string key = argv[i];
		const std::string value = argv[i + 1];

		if (key == "--map") { map_name = value; }
		else if (key == "--ents") { ents_path = value; }
		else if (key == "--out") { out_path = value; }
		else if (key == "--brushes") { brushes_path = value; }
		else if (key == "--models") { models_path = value; }
		else if (key == "--ff") { ff_path = value; }
		else if (key == "--exe") { exe_path = value; }
		else if (key == "--ents-out") { ents_out_path = value; }
		else if (key == "--trigger-models") { trigger_models = std::atoi(value.c_str()); }
		else { return fail("unknown option " + key); }
	}

	if (map_name.empty() || (ents_path.empty() && ff_path.empty()) || out_path.empty() || (!ff_path.empty() && exe_path.empty()))
	{
		return fail("usage: codol_mapconv --map <name> --ff <map.ff> --exe <client exe> --out <file.map> [--ents-out <file>]\n       codol_mapconv --map <name> --ents <entities> --out <file.map> [--brushes <json>] [--models <json>] [--trigger-models <n>]");
	}

	const auto t0 = std::chrono::steady_clock::now();

	mapgeo::map_geometry geometry;
	std::string text;
	std::string error;

	if (!ff_path.empty())
	{
		// native: one pass over the fastfile
		mapgeo::zone_map zm;
		if (!mapgeo::read_zone_map(std::filesystem::u8path(ff_path), std::filesystem::u8path(exe_path), zm, error))
		{
			return fail(ff_path + ": " + error);
		}

		geometry = std::move(zm.geometry);
		text = std::move(zm.entity_text);
		trigger_models = zm.trigger_models;

		if (!ents_out_path.empty())
		{
			std::ofstream file(std::filesystem::u8path(ents_out_path), std::ios::binary | std::ios::trunc);
			file.write(text.data(), static_cast<std::streamsize>(text.size()));
		}
	}
	else if (!read_file(ents_path, text))
	{
		return fail("can not read " + ents_path);
	}

	if (ff_path.empty() && ends_with_ci(ents_path, ".zone"))
	{
		std::string zone;
		zone.swap(text);

		if (!mapents::extract_from_zone(zone, text, &trigger_models))
		{
			return fail(ents_path + " does not contain a map entity string");
		}
	}
	else if (ff_path.empty())
	{
		text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	}

	mapents::entity_set set;

	if (!mapents::parse_entity_string(text, set, error))
	{
		return fail("entities: " + error);
	}

	if (!set.has_crc)
	{
		return fail("entities: no \"// base_crc32\" line, the zone they belong to is unknown");
	}

	set.trigger_models = trigger_models;

	// geometry exported by the external tool ( legacy input )

	if (!brushes_path.empty())
	{
		std::string json;
		if (!read_file(brushes_path, json))
		{
			return fail("can not read " + brushes_path);
		}

		if (!mapgeo::read_brushes_json(json, geometry, error))
		{
			return fail(brushes_path + ": " + error);
		}
	}

	if (!models_path.empty())
	{
		std::string json;
		if (!read_file(models_path, json))
		{
			return fail("can not read " + models_path);
		}

		if (!mapgeo::read_static_models_json(json, geometry, error))
		{
			return fail(models_path + ": " + error);
		}
	}

	const auto map = mapents::to_radiant_map(set, map_name, geometry.empty() ? nullptr : &geometry);

	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::u8path(out_path).parent_path(), ec);

	{
		std::ofstream file(std::filesystem::u8path(out_path), std::ios::binary | std::ios::trunc);
		if (!file.is_open())
		{
			return fail("can not write " + out_path);
		}

		file.write(map.data(), static_cast<std::streamsize>(map.size()));
	}

	std::size_t world_brushes = 0, entity_brushes = 0;
	std::unordered_set<int> sub_models;

	for (const auto& b : geometry.brushes)
	{
		(b.sub_model == 0 ? world_brushes : entity_brushes)++;
		if (b.sub_model != 0)
		{
			sub_models.insert(b.sub_model);
		}
	}

	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

	std::printf("%s: %zu entities, %zu world brushes, %zu brushes in %zu brush entities, %zu static models, base_crc32 %08x -> %s ( %lld ms )\n",
		map_name.c_str(), set.ents.size(), world_brushes, entity_brushes, sub_models.size(), geometry.models.size(), set.base_crc,
		out_path.c_str(), static_cast<long long>(ms));

	return 0;
}
