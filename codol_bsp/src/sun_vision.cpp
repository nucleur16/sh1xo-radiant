// The vision file and the sun flare of a map.
//
// vision/<map>.vision is a rawfile the client asks for when a level starts ( visionSetNaked( mapname ) in the level
// scripts ): glow, the film colour tweaks and the strength of the primary lights. sun/<map>.sun describes the sun's
// sprite, flare, blinding and glare; the game's map compiler turns it into the sun block of the GfxWorld, which is
// what the client draws from, and the shipped map zones carry the file as a rawfile next to it.
//
// In this game both files name their settings by NUMBER: the dvars of 1.1.1.13 have numeric names. The numbers were
// read off the shipped files ( vision/default.vision and vision/mp_vacant.vision of common_mp, sun/<map>.sun of the
// map zones ), which have the layout, order and values of the files of the earlier games:
//
//   vision  511 r_glow  516 r_glowRadius0  517 r_glowRadius1  512 r_glowBloomCutoff  513 r_glowBloomDesaturation
//           514 r_glowBloomIntensity0  515 r_glowBloomIntensity1  518 / 519 r_glowSkyBleedIntensity0 / 1
//           520 r_filmEnable  522 r_filmContrast  521 r_filmBrightness  523 r_filmDesaturation
//           524 r_filmDesaturationDark  525 r_filmInvert  526 r_filmLightTint  527 r_filmMediumTint
//           528 r_filmDarkTint  529 r_primaryLightUseTweaks  530 r_primaryLightTweakDiffuseStrength
//           531 r_primaryLightTweakSpecularStrength
//   sun     1704 r_sunsprite_shader  1705 r_sunsprite_size  1706 r_sunflare_shader  1707 r_sunflare_min_size
//           1708 r_sunflare_min_angle  1709 r_sunflare_max_size  1710 r_sunflare_max_angle  1711 r_sunflare_max_alpha
//           1712 r_sunflare_fadein  1713 r_sunflare_fadeout  1714 r_sunblind_min_angle  1715 r_sunblind_max_angle
//           1716 r_sunblind_max_darken  1717 r_sunblind_fadein  1718 r_sunblind_fadeout  1719 r_sunglare_min_angle
//           1720 r_sunglare_max_angle  1721 r_sunglare_max_lighten  1722 r_sunglare_fadein  1723 r_sunglare_fadeout
//           1724 r_sun_fx_position
//
// The sun numbers were checked against the sun blocks of the same zones ( codol_bsp_probe sun ): mp_killhouse_sh has
// 1705 "23.9" and sprite size 23.9, 1707 "343" / 1709 "528" and flare sizes 171.5 / 264 ( half ), 1708 "71" /
// 1710 "10.4909" and the cosines 0.3256 / 0.9833, 1712 "2.8" and 2800 ms, 1724 "351.691 181.278 -18.5703" and the
// direction ( -0.989 -0.022 0.145 ). A mapper's file may use the names or the numbers.

#include "world.hpp"

#include <fstream>
#include <sstream>

namespace codol::bsp
{
	namespace
	{
		struct setting
		{
			const char* name;
			int id;
		};

		constexpr setting VISION[] =
		{
			{ "r_glow", 511 }, { "r_glowRadius0", 516 }, { "r_glowRadius1", 517 }, { "r_glowBloomCutoff", 512 }, { "r_glowBloomDesaturation", 513 },
			{ "r_glowBloomIntensity0", 514 }, { "r_glowBloomIntensity1", 515 }, { "r_glowSkyBleedIntensity0", 518 }, { "r_glowSkyBleedIntensity1", 519 },
			{ "r_filmEnable", 520 }, { "r_filmContrast", 522 }, { "r_filmBrightness", 521 }, { "r_filmDesaturation", 523 }, { "r_filmDesaturationDark", 524 },
			{ "r_filmInvert", 525 }, { "r_filmLightTint", 526 }, { "r_filmMediumTint", 527 }, { "r_filmDarkTint", 528 }, { "r_primaryLightUseTweaks", 529 },
			{ "r_primaryLightTweakDiffuseStrength", 530 }, { "r_primaryLightTweakSpecularStrength", 531 },
		};

		constexpr setting SUN[] =
		{
			{ "r_sunsprite_shader", 1704 }, { "r_sunsprite_size", 1705 }, { "r_sunflare_shader", 1706 }, { "r_sunflare_min_size", 1707 },
			{ "r_sunflare_min_angle", 1708 }, { "r_sunflare_max_size", 1709 }, { "r_sunflare_max_angle", 1710 }, { "r_sunflare_max_alpha", 1711 },
			{ "r_sunflare_fadein", 1712 }, { "r_sunflare_fadeout", 1713 }, { "r_sunblind_min_angle", 1714 }, { "r_sunblind_max_angle", 1715 },
			{ "r_sunblind_max_darken", 1716 }, { "r_sunblind_fadein", 1717 }, { "r_sunblind_fadeout", 1718 }, { "r_sunglare_min_angle", 1719 },
			{ "r_sunglare_max_angle", 1720 }, { "r_sunglare_max_lighten", 1721 }, { "r_sunglare_fadein", 1722 }, { "r_sunglare_fadeout", 1723 },
			{ "r_sun_fx_position", 1724 },
		};

		bool lower_equal(const std::string_view a, const std::string_view b)
		{
			if (a.size() != b.size())
			{
				return false;
			}
			for (std::size_t k = 0; k < a.size(); k++)
			{
				if (std::tolower(static_cast<unsigned char>(a[k])) != std::tolower(static_cast<unsigned char>(b[k])))
				{
					return false;
				}
			}
			return true;
		}

		// A settings file: per line a key ( a name of the table, or a number ) and a value, quoted or not; // starts a
		// comment. Returns ( number, value ) in the order of the file; names the table does not have are reported.
		template <std::size_t N>
		std::vector<std::pair<int, std::string>> parse_settings(const std::string& text, const setting (&table)[N], const std::string& what, report& rep)
		{
			std::vector<std::pair<int, std::string>> out;
			std::istringstream in(text);
			std::string line;
			while (std::getline(in, line))
			{
				if (const std::size_t comment = line.find("//"); comment != std::string::npos)
				{
					line.erase(comment);
				}
				std::size_t at = line.find_first_not_of(" \t\r");
				if (at == std::string::npos)
				{
					continue;
				}
				std::size_t end = line.find_first_of(" \t\r", at);
				std::string key = line.substr(at, end == std::string::npos ? std::string::npos : end - at);
				if (key.size() >= 2 && key.front() == '"' && key.back() == '"')
				{
					key = key.substr(1, key.size() - 2);
				}
				std::string value = end == std::string::npos ? std::string() : line.substr(end);
				const std::size_t first = value.find_first_not_of(" \t\r\""), last = value.find_last_not_of(" \t\r\"");
				value = first == std::string::npos ? std::string() : value.substr(first, last - first + 1);
				int id = 0;
				if (!key.empty() && std::all_of(key.begin(), key.end(), [](const char c) { return c >= '0' && c <= '9'; }))
				{
					id = std::atoi(key.c_str());
				}
				for (const setting& s : table)
				{
					if (lower_equal(key, s.name))
					{
						id = s.id;
					}
				}
				if (id == 0)
				{
					rep.warnings.push_back(what + ": '" + key + "' is no setting this compiler knows; the line is left out");
					continue;
				}
				out.emplace_back(id, value);
			}
			return out;
		}

		std::string settings_text(const std::vector<std::pair<int, std::string>>& settings)
		{
			std::string text;
			for (const auto& [id, value] : settings)
			{
				text += std::to_string(id) + " \"" + value + "\"\n";
			}
			return text;
		}

		bool read_text(const std::filesystem::path& file, std::string& text)
		{
			std::ifstream in(file, std::ios::binary);
			if (!in)
			{
				return false;
			}
			text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			return true;
		}

		// the mapper's file: the one given on the command line, else <map name>.<extension> or <.map file's name>.<extension>
		// next to the .map
		std::filesystem::path mapper_file(const compile_context& ctx, const std::filesystem::path& given, const char* extension)
		{
			if (!given.empty())
			{
				return given;
			}
			std::error_code ec;
			for (const std::string& stem : { ctx.w.name, ctx.opts.map.stem().string() })
			{
				const std::filesystem::path candidate = ctx.opts.map.parent_path() / (stem + extension);
				if (!stem.empty() && std::filesystem::is_regular_file(candidate, ec))
				{
					return candidate;
				}
			}
			return {};
		}

		bool has_rawfile(const options& opts, const std::string& name)
		{
			return std::any_of(opts.rawfiles.begin(), opts.rawfiles.end(), [&](const auto& r) { return r.first == name; });
		}

		std::vector<std::uint8_t> bytes_of(const std::string& text)
		{
			return { text.begin(), text.end() };
		}

		// What a map gets when the mapper gives no vision file: a little glow, the film tweaks switched on without
		// changing a colour, the primary lights as they are.
		const char* const DEFAULT_VISION =
			"r_glow \"1\"\n"
			"r_glowRadius0 \"5\"\n"
			"r_glowBloomCutoff \"0.9\"\n"
			"r_glowBloomDesaturation \"0\"\n"
			"r_glowBloomIntensity0 \"0.25\"\n"
			"r_filmEnable \"1\"\n"
			"r_filmContrast \"1\"\n"
			"r_filmBrightness \"0\"\n"
			"r_filmDesaturation \"0\"\n"
			"r_filmDesaturationDark \"0\"\n"
			"r_filmInvert \"0\"\n"
			"r_filmLightTint \"1 1 1\"\n"
			"r_filmMediumTint \"1 1 1\"\n"
			"r_filmDarkTint \"1 1 1\"\n"
			"r_primaryLightUseTweaks \"0\"\n"
			"r_primaryLightTweakDiffuseStrength \"1\"\n"
			"r_primaryLightTweakSpecularStrength \"1\"\n";

		float number(const std::vector<std::pair<int, std::string>>& settings, const int id, const float fallback)
		{
			for (const auto& [key, value] : settings)
			{
				if (key == id && !value.empty())
				{
					char* end = nullptr;
					const float v = std::strtof(value.c_str(), &end);
					return end == value.c_str() ? fallback : v;
				}
			}
			return fallback;
		}

		std::string word(const std::vector<std::pair<int, std::string>>& settings, const int id, const std::string& fallback)
		{
			for (const auto& [key, value] : settings)
			{
				if (key == id)
				{
					return value;
				}
			}
			return fallback;
		}
	}

	void build_vision_and_sun(compile_context& ctx)
	{
		world& w = ctx.w;
		// ---- the vision file
		const std::string vision_name = "vision/" + w.name + ".vision";
		if (!has_rawfile(ctx.opts, vision_name))
		{
			std::string text = DEFAULT_VISION, source = "the compiler's default";
			if (const std::filesystem::path file = mapper_file(ctx, ctx.opts.vision, ".vision"); !file.empty())
			{
				if (read_text(file, text))
				{
					source = file.string();
				}
				else
				{
					ctx.rep.warnings.push_back("vision: cannot read " + file.string() + "; the default is written");
					text = DEFAULT_VISION;
				}
			}
			const std::vector<std::pair<int, std::string>> settings = parse_settings(text, VISION, "vision", ctx.rep);
			check(ctx.editor.set_rawfile(vision_name, bytes_of(settings_text(settings)), true), "adding rawfile " + vision_name);
			ctx.rep.notes.push_back(format("vision: %s with %zu settings from %s", vision_name.c_str(), settings.size(), source.c_str()));
		}

		// ---- the sun: a flare for a map whose sun shines ( "sunlight" above 0 ) on a sky, or what the mapper's file says
		const std::filesystem::path sun_file = mapper_file(ctx, ctx.opts.sun, ".sun");
		bool sky = false;
		for (const map_brush& brush : w.brushes)
		{
			for (const map_face& face : brush.faces)
			{
				sky = sky || face.material.rfind("sky", 0) == 0;
			}
		}
		const bool shines = w.sun_color[0] + w.sun_color[1] + w.sun_color[2] > 0;
		if (sun_file.empty() && !(sky && shines))
		{
			return;
		}
		std::string text, source = "the compiler's default";
		if (!sun_file.empty() && read_text(sun_file, text))
		{
			source = sun_file.string();
		}
		else if (!sun_file.empty())
		{
			ctx.rep.warnings.push_back("sun: cannot read " + sun_file.string() + "; the default flare is written");
		}
		std::vector<std::pair<int, std::string>> settings = parse_settings(text, SUN, "sun", ctx.rep);
		sun_flare& f = w.flare;
		f.sprite = word(settings, 1704, "sun");
		f.flare = word(settings, 1706, "sun_flare");
		f.sprite_size = number(settings, 1705, 16.0f);
		f.flare_min_size = number(settings, 1707, 0.0f);
		f.flare_min_angle = number(settings, 1708, 45.0f);
		f.flare_max_size = number(settings, 1709, 600.0f);
		f.flare_max_angle = number(settings, 1710, 2.0f);
		f.flare_max_alpha = number(settings, 1711, 0.5f);
		f.flare_fadein = number(settings, 1712, 1.0f);
		f.flare_fadeout = number(settings, 1713, 1.0f);
		f.blind_min_angle = number(settings, 1714, 30.0f);
		f.blind_max_angle = number(settings, 1715, 5.0f);
		f.blind_max_darken = number(settings, 1716, 0.0f);
		f.blind_fadein = number(settings, 1717, 0.5f);
		f.blind_fadeout = number(settings, 1718, 3.0f);
		f.glare_min_angle = number(settings, 1719, 30.0f);
		f.glare_max_angle = number(settings, 1720, 5.0f);
		f.glare_max_lighten = number(settings, 1721, 0.0f);
		f.glare_fadein = number(settings, 1722, 0.5f);
		f.glare_fadeout = number(settings, 1723, 3.0f);
		// where the flare stands: "r_sun_fx_position" ( angles ), else where the sun of the worldspawn stands
		std::memcpy(f.direction, w.sun_dir, sizeof(f.direction));
		if (const std::string position = word(settings, 1724, ""); !position.empty())
		{
			float angles[3] = {};
			if (std::sscanf(position.c_str(), "%f %f %f", &angles[0], &angles[1], &angles[2]) >= 2)
			{
				const float p = angles[0] * 3.14159265f / 180.0f, y = angles[1] * 3.14159265f / 180.0f;
				f.direction[0] = std::cos(p) * std::cos(y);
				f.direction[1] = std::cos(p) * std::sin(y);
				f.direction[2] = -std::sin(p);
			}
		}
		std::string why;
		f.sprite_ref = ctx.importer.material(f.sprite, &why);
		f.flare_ref = ctx.importer.material(f.flare, &why);
		if (!f.sprite_ref.valid() || !f.flare_ref.valid())
		{
			ctx.rep.warnings.push_back("sun: the material '" + (f.sprite_ref.valid() ? f.flare : f.sprite) + "' is in no zone ( " + why + " ): the map gets no sun flare");
			return;
		}
		f.valid = true;
		// the file as the shipped map zones carry it, complete
		const float pitch = std::asin(std::clamp(-f.direction[2], -1.0f, 1.0f)) * 180.0f / 3.14159265f;
		const float yaw = std::atan2(f.direction[1], f.direction[0]) * 180.0f / 3.14159265f;
		const std::vector<std::pair<int, std::string>> complete =
		{
			{ 1704, f.sprite }, { 1705, format("%g", f.sprite_size) }, { 1706, f.flare }, { 1707, format("%g", f.flare_min_size) }, { 1708, format("%g", f.flare_min_angle) },
			{ 1709, format("%g", f.flare_max_size) }, { 1710, format("%g", f.flare_max_angle) }, { 1711, format("%g", f.flare_max_alpha) }, { 1712, format("%g", f.flare_fadein) },
			{ 1713, format("%g", f.flare_fadeout) }, { 1714, format("%g", f.blind_min_angle) }, { 1715, format("%g", f.blind_max_angle) }, { 1716, format("%g", f.blind_max_darken) },
			{ 1717, format("%g", f.blind_fadein) }, { 1718, format("%g", f.blind_fadeout) }, { 1719, format("%g", f.glare_min_angle) }, { 1720, format("%g", f.glare_max_angle) },
			{ 1721, format("%g", f.glare_max_lighten) }, { 1722, format("%g", f.glare_fadein) }, { 1723, format("%g", f.glare_fadeout) },
			{ 1724, format("%g %g 0", pitch, yaw) },
		};
		const std::string sun_name = "sun/" + w.name + ".sun";
		if (!has_rawfile(ctx.opts, sun_name))
		{
			check(ctx.editor.set_rawfile(sun_name, bytes_of(settings_text(complete)), true), "adding rawfile " + sun_name);
		}
		ctx.rep.notes.push_back(format("sun: flare from %s - sprite %s ( %g ), flare %s ( %g .. %g at %g .. %g degrees, alpha %g ), towards ( %.3f %.3f %.3f )", source.c_str(),
			f.sprite.c_str(), f.sprite_size, f.flare.c_str(), f.flare_min_size, f.flare_max_size, f.flare_min_angle, f.flare_max_angle, f.flare_max_alpha, f.direction[0],
			f.direction[1], f.direction[2]));
	}
}
