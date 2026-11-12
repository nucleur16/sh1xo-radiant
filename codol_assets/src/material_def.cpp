// Material definitions as text: the .mtl format of this library and the material entries of a CoD4 .gdt file.

#include "material_state.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace codol::assets
{
	namespace
	{
		std::string lower(std::string_view text)
		{
			std::string out(text);
			for (char& c : out)
			{
				c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
			}
			return out;
		}

		void set_field(MaterialDef& def, const StateBits mask, const StateBits value)
		{
			def.stateMask |= mask;
			def.stateValue = (def.stateValue & ~mask) | (value & mask);
		}

		bool set_blend(MaterialDef& def, const std::string& name)
		{
			// the shipped materials: lit blend is ONE / INVSRCALPHA, unlit blend SRCALPHA / INVSRCALPHA, so "blend"
			// leaves the source factor to the technique set; add ONE / ONE, multiply ZERO / SRCCOLOR, screen
			// INVDESTCOLOR / ONE
			def.blendFunc = name;
			if (name == "replace")
			{
				set_field(def, 0x700, 0);
			}
			else if (name == "blend")
			{
				set_field(def, 0x7F0, 0x160);
			}
			else if (name == "add")
			{
				set_field(def, 0x7FF, 0x122);
			}
			else if (name == "multiply")
			{
				set_field(def, 0x7FF, 0x131);
			}
			else if (name == "screen" || name == "screen add")
			{
				def.blendFunc = "screen";
				set_field(def, 0x7FF, 0x12A);
			}
			else
			{
				return false;
			}
			return true;
		}

		bool set_alpha_test(MaterialDef& def, const std::string& name)
		{
			def.alphaTest = name;
			if (name == "none" || name == "always")
			{
				def.alphaTest = "none";
				set_field(def, state::ALPHA_TEST, 0x800);
			}
			else if (name == "gt0")
			{
				set_field(def, state::ALPHA_TEST, 0x1000);
			}
			else if (name == "lt128")
			{
				set_field(def, state::ALPHA_TEST, 0x2000);
			}
			else if (name == "ge128")
			{
				set_field(def, state::ALPHA_TEST, 0x3000);
			}
			else
			{
				return false;
			}
			return true;
		}

		bool set_cull(MaterialDef& def, const std::string& name)
		{
			const StateBits value = name == "none" ? 0x4000 : name == "back" ? 0x8000 : name == "front" ? 0xC000 : 0;
			if (value)
			{
				set_field(def, state::CULL, value);
			}
			return value != 0;
		}

		bool set_depth_test(MaterialDef& def, const std::string& name)
		{
			const int value = name == "always" ? 0 : name == "less" ? 4 : name == "equal" ? 8 : name == "lessequal" ? 12 : name == "disable" ? 2 : -1;
			if (value >= 0)
			{
				set_field(def, state::DEPTH_TEST, static_cast<StateBits>(value) << 32);
			}
			return value >= 0;
		}

		bool set_polygon_offset(MaterialDef& def, const std::string& name)
		{
			const int value = name == "none" ? 0 : name == "decal" || name == "static decal" ? 1 : name == "impact" || name == "weapon impact" ? 2 : -1;
			if (value >= 0)
			{
				set_field(def, state::POLYGON_OFFSET, static_cast<StateBits>(value) << 36);
			}
			return value >= 0;
		}

		bool parse_number(const std::string& text, std::uint64_t& value)
		{
			if (text.empty())
			{
				return false;
			}
			char* end = nullptr;
			value = std::strtoull(text.c_str(), &end, 0);
			return end && *end == 0;
		}

		struct Tokens
		{
			std::vector<std::string> words;
			std::vector<int> lines;
			std::vector<bool> quoted;
		};

		// words, quoted strings and the single characters { } ( ) [ ]; // comments
		Tokens tokenize(const std::string_view text)
		{
			Tokens t;
			int line = 1;
			for (std::size_t k = 0; k < text.size();)
			{
				const char c = text[k];
				if (c == '\n')
				{
					line++;
					k++;
				}
				else if (c == ' ' || c == '\t' || c == '\r')
				{
					k++;
				}
				else if (c == '/' && k + 1 < text.size() && text[k + 1] == '/')
				{
					while (k < text.size() && text[k] != '\n')
					{
						k++;
					}
				}
				else if (c == '"')
				{
					std::string word;
					for (k++; k < text.size() && text[k] != '"'; k++)
					{
						if (text[k] == '\\' && k + 1 < text.size() && (text[k + 1] == '"' || text[k + 1] == '\\'))
						{
							k++;
						}
						line += text[k] == '\n' ? 1 : 0;
						word += text[k];
					}
					k++;
					t.words.push_back(word);
					t.lines.push_back(line);
					t.quoted.push_back(true);
				}
				else if (std::strchr("{}()[]", c))
				{
					t.words.emplace_back(1, c);
					t.lines.push_back(line);
					t.quoted.push_back(false);
					k++;
				}
				else
				{
					std::string word;
					while (k < text.size() && !std::strchr(" \t\r\n{}()[]\"", text[k]))
					{
						word += text[k++];
					}
					t.words.push_back(word);
					t.lines.push_back(line);
					t.quoted.push_back(false);
				}
			}
			return t;
		}

		// sampler state from words: tile | clamp | clampu | clampv, nomip | nearest | bilinear | trilinear | aniso | aniso4
		bool sampler_option(const std::string& word, int& state)
		{
			if (state < 0)
			{
				state = 0x0B;		// anisotropic 2x, nearest mip, tiled: 62 % of the shipped texture defs
			}
			if (word == "tile")
			{
				state &= ~0xE0;
			}
			else if (word == "clamp")
			{
				state |= 0xE0;
			}
			else if (word == "clampu")
			{
				state |= 0x20;
			}
			else if (word == "clampv")
			{
				state |= 0x40;
			}
			else if (word == "nomip")
			{
				state &= ~0x18;
			}
			else if (word == "nearest")
			{
				state = (state & ~0x07) | 1;
			}
			else if (word == "bilinear")
			{
				state = (state & ~0x07) | 2;
			}
			else if (word == "aniso")
			{
				state = (state & ~0x07) | 3;
			}
			else if (word == "aniso4")
			{
				state = (state & ~0x07) | 4;
			}
			else if (word == "trilinear")
			{
				state = (state & ~0x18) | 0x10;
			}
			else
			{
				return false;
			}
			return true;
		}

		bool is_sampler_key(const std::string& key)
		{
			return key.size() > 3 && (key.compare(key.size() - 3, 3, "Map") == 0 || key.find("Map") != std::string::npos || key.find("Sampler") != std::string::npos);
		}
	}

	std::uint32_t surface_type_bits(const std::string_view name)
	{
		// bit n - 1 for surface type n; from the shipped materials ( names against surfaceTypeBits )
		static const char* const NAMES[] = { "bark", "brick", "carpet", "cloth", "concrete", "dirt", "flesh", "foliage", "glass", "grass", "gravel", "ice",
			"metal", "mud", "paper", "plaster", "rock", "sand", "snow", "water", "wood", "asphalt", "ceramic", "plastic", "rubber", "cushion", "fruit",
			"paintedmetal" };
		const std::string wanted = lower(name);
		for (std::size_t k = 0; k < std::size(NAMES); k++)
		{
			if (wanted == NAMES[k])
			{
				return 1u << k;
			}
		}
		return 0;
	}

	Status parse_material_text(const std::string_view text, std::vector<MaterialDef>& out)
	try
	{
		const Tokens t = tokenize(text);
		std::size_t k = 0;
		const auto fail = [&](const std::string& what) { return Status::error("line " + std::to_string(k < t.lines.size() ? t.lines[k] : 0) + ": " + what); };
		while (k < t.words.size())
		{
			if (t.words[k] != "material" || k + 2 >= t.words.size() || t.words[k + 2] != "{")
			{
				return fail("expected: material \"name\" {");
			}
			MaterialDef def;
			def.name = t.words[k + 1];
			k += 3;
			while (k < t.words.size() && t.words[k] != "}")
			{
				const std::string key = t.words[k];
				const int line = t.lines[k];
				std::vector<std::string> args;
				for (k++; k < t.words.size() && t.lines[k] == line && t.words[k] != "}"; k++)
				{
					args.push_back(t.words[k]);
				}
				const std::string first = args.empty() ? std::string() : lower(args[0]);
				std::uint64_t number = 0;
				bool ok = !args.empty();
				if (!ok)
				{
				}
				else if (key == "type")
				{
					def.type = first;
				}
				else if (key == "techset")
				{
					def.techset = args[0];
				}
				else if (key == "blendFunc")
				{
					ok = set_blend(def, first);
				}
				else if (key == "blend")
				{
					// numeric: source destination operation [ alpha source, destination, operation ] ( D3DBLEND / D3DBLENDOP )
					std::uint64_t v[6] = {};
					ok = args.size() == 3 || args.size() == 6;
					for (std::size_t a = 0; ok && a < args.size(); a++)
					{
						ok = parse_number(args[a], v[a]);
					}
					if (ok)
					{
						def.blendFunc = v[2] == 0 ? "replace" : v[1] == 6 ? "blend" : v[1] == 3 ? "multiply" : v[0] == 10 ? "screen" : "add";
						set_field(def, args.size() == 6 ? state::BLEND : state::BLEND_COLOR,
							v[0] | (v[1] << 4) | (v[2] << 8) | (v[3] << 16) | (v[4] << 20) | (v[5] << 24));
					}
				}
				else if (key == "alphaTest")
				{
					ok = set_alpha_test(def, first);
				}
				else if (key == "cullFace")
				{
					ok = set_cull(def, first);
				}
				else if (key == "depthWrite")
				{
					ok = first == "on" || first == "off";
					set_field(def, state::DEPTH_WRITE, first == "on" ? state::DEPTH_WRITE : 0);
				}
				else if (key == "depthTest")
				{
					ok = set_depth_test(def, first);
				}
				else if (key == "polygonOffset")
				{
					ok = set_polygon_offset(def, first);
				}
				else if (key == "colorWrite")
				{
					ok = parse_number(args[0], number) && number < 4;
					set_field(def, state::COLOR_WRITE, number << 27);
				}
				else if (key == "gammaWrite")
				{
					ok = first == "on" || first == "off";
					set_field(def, state::GAMMA_WRITE, first == "on" ? state::GAMMA_WRITE : 0);
				}
				else if (key == "stateBits")
				{
					std::uint64_t mask = 0;
					ok = args.size() == 2 && parse_number(args[0], mask) && parse_number(args[1], number);
					set_field(def, mask, number);
				}
				else if (key == "sort")
				{
					ok = parse_number(args[0], number) && number < 256;
					def.sortKey = static_cast<int>(number);
				}
				else if (key == "gameFlags")
				{
					ok = parse_number(args[0], number) && number < 256;
					def.gameFlags = static_cast<int>(number);
				}
				else if (key == "cameraRegion")
				{
					ok = parse_number(args[0], number) && number < 256;
					def.cameraRegion = static_cast<int>(number);
				}
				else if (key == "surfaceType")
				{
					if (parse_number(args[0], number))
					{
						def.surfaceTypeBits = static_cast<std::uint32_t>(number);
					}
					else
					{
						def.surfaceTypeBits = surface_type_bits(first);
						ok = def.surfaceTypeBits != 0 || first == "none" || first == "default";
					}
				}
				else if (key == "atlas")
				{
					std::uint64_t columns = 0;
					ok = args.size() == 2 && parse_number(args[0], number) && parse_number(args[1], columns);
					def.atlasRows = static_cast<int>(number);
					def.atlasColumns = static_cast<int>(columns);
				}
				else if (key == "constant")
				{
					MaterialConstant c;
					ok = args.size() >= 2 && args.size() <= 5 && args[0].size() <= 12;
					c.name = args[0];
					for (std::size_t a = 1; ok && a < args.size(); a++)
					{
						c.value[a - 1] = static_cast<float>(std::atof(args[a].c_str()));
					}
					def.constants.push_back(c);
				}
				else if (is_sampler_key(key))
				{
					MaterialTextureRef ref;
					ref.sampler = key;
					ref.image = args[0];
					for (std::size_t a = 1; ok && a < args.size(); a++)
					{
						const std::string word = lower(args[a]);
						if (word.rfind("semantic=", 0) == 0)
						{
							ref.semantic = std::atoi(word.c_str() + 9);
						}
						else if (word.rfind("sampler=", 0) == 0)
						{
							ok = parse_number(word.substr(8), number);
							ref.samplerState = static_cast<int>(number);
						}
						else
						{
							ok = sampler_option(word, ref.samplerState);
						}
					}
					def.textures.push_back(ref);
				}
				else
				{
					ok = false;
				}
				if (!ok)
				{
					return Status::error("line " + std::to_string(line) + ": cannot read '" + key + "' of material '" + def.name + "'");
				}
			}
			if (k >= t.words.size())
			{
				return fail("material '" + def.name + "' is not closed");
			}
			k++;
			out.push_back(std::move(def));
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	std::string write_material_text(const MaterialDef& def)
	{
		std::string out = "material \"" + def.name + "\"\n{\n";
		char line[256];
		if (!def.techset.empty())
		{
			out += "\ttechset " + def.techset + "\n";
		}
		else
		{
			out += "\ttype \"" + def.type + "\"\n";
		}
		for (const MaterialTextureRef& t : def.textures)
		{
			out += "\t" + t.sampler + " \"" + t.image + "\"";
			if (t.samplerState >= 0)
			{
				std::snprintf(line, sizeof(line), " sampler=0x%02X", t.samplerState);
				out += line;
			}
			if (t.semantic >= 0)
			{
				std::snprintf(line, sizeof(line), " semantic=%d", t.semantic);
				out += line;
			}
			out += "\n";
		}
		const StateBits m = def.stateMask, v = def.stateValue;
		if ((m & state::BLEND) == state::BLEND)
		{
			std::snprintf(line, sizeof(line), "\tblend %u %u %u %u %u %u\n", static_cast<unsigned>(v & 15), static_cast<unsigned>((v >> 4) & 15),
				static_cast<unsigned>((v >> 8) & 7), static_cast<unsigned>((v >> 16) & 15), static_cast<unsigned>((v >> 20) & 15), static_cast<unsigned>((v >> 24) & 7));
			out += line;
		}
		else if (!def.blendFunc.empty())
		{
			out += "\tblendFunc " + def.blendFunc + "\n";
		}
		if (m & state::ALPHA_TEST)
		{
			const StateBits a = v & state::ALPHA_TEST;
			if (a == 0x800 || a == 0x1000 || a == 0x2000 || a == 0x3000)
			{
				out += std::string("\talphaTest ") + (a == 0x800 ? "none" : a == 0x1000 ? "gt0" : a == 0x2000 ? "lt128" : "ge128") + "\n";
			}
			else
			{
				std::snprintf(line, sizeof(line), "\tstateBits 0x%llX 0x%llX\n", static_cast<unsigned long long>(state::ALPHA_TEST), static_cast<unsigned long long>(a));
				out += line;
			}
		}
		if ((m & state::CULL) && (v & state::CULL))
		{
			out += std::string("\tcullFace ") + ((v & state::CULL) == 0x4000 ? "none" : (v & state::CULL) == 0x8000 ? "back" : "front") + "\n";
		}
		else if (m & state::CULL)
		{
			out += "\tstateBits 0xC000 0x0\n";
		}
		if (m & state::DEPTH_WRITE)
		{
			out += std::string("\tdepthWrite ") + ((v & state::DEPTH_WRITE) ? "on" : "off") + "\n";
		}
		if (m & state::DEPTH_TEST)
		{
			const unsigned d = static_cast<unsigned>((v >> 32) & 0xE);
			if (d == 0 || d == 4 || d == 8 || d == 12 || d == 2)
			{
				out += std::string("\tdepthTest ") + (d == 0 ? "always" : d == 4 ? "less" : d == 8 ? "equal" : d == 12 ? "lessequal" : "disable") + "\n";
			}
			else
			{
				std::snprintf(line, sizeof(line), "\tstateBits 0x%llX 0x%llX\n", static_cast<unsigned long long>(state::DEPTH_TEST),
					static_cast<unsigned long long>(v & state::DEPTH_TEST));
				out += line;
			}
		}
		if (m & state::POLYGON_OFFSET)
		{
			const unsigned p = static_cast<unsigned>((v >> 36) & 3);
			if (p < 3)
			{
				out += std::string("\tpolygonOffset ") + (p == 0 ? "none" : p == 1 ? "decal" : "impact") + "\n";
			}
			else
			{
				out += "\tstateBits 0x3000000000 0x3000000000\n";
			}
		}
		if (m & state::COLOR_WRITE)
		{
			std::snprintf(line, sizeof(line), "\tcolorWrite %u\n", static_cast<unsigned>((v >> 27) & 3));
			out += line;
		}
		if (m & state::GAMMA_WRITE)
		{
			out += std::string("\tgammaWrite ") + ((v & state::GAMMA_WRITE) ? "on" : "off") + "\n";
		}
		const StateBits known = state::BLEND | state::ALPHA_TEST | state::CULL | state::DEPTH_WRITE | state::DEPTH_TEST | state::POLYGON_OFFSET
			| state::COLOR_WRITE | state::GAMMA_WRITE;
		if (m & ~known)
		{
			std::snprintf(line, sizeof(line), "\tstateBits 0x%llX 0x%llX\n", static_cast<unsigned long long>(m & ~known), static_cast<unsigned long long>(v & m & ~known));
			out += line;
		}
		if (def.sortKey >= 0)
		{
			out += "\tsort " + std::to_string(def.sortKey) + "\n";
		}
		if (def.gameFlags >= 0)
		{
			std::snprintf(line, sizeof(line), "\tgameFlags 0x%02X\n", def.gameFlags);
			out += line;
		}
		if (def.cameraRegion >= 0)
		{
			out += "\tcameraRegion " + std::to_string(def.cameraRegion) + "\n";
		}
		if (def.surfaceTypeBits)
		{
			std::snprintf(line, sizeof(line), "\tsurfaceType 0x%X\n", def.surfaceTypeBits);
			out += line;
		}
		if (def.atlasRows != 1 || def.atlasColumns != 1)
		{
			out += "\tatlas " + std::to_string(def.atlasRows) + " " + std::to_string(def.atlasColumns) + "\n";
		}
		for (const MaterialConstant& c : def.constants)
		{
			std::snprintf(line, sizeof(line), "\tconstant %s %.9g %.9g %.9g %.9g\n", c.name.c_str(), c.value[0], c.value[1], c.value[2], c.value[3]);
			out += line;
		}
		return out + "}\n";
	}

	Status parse_gdt_materials(const std::string_view text, std::vector<MaterialDef>& out)
	try
	{
		const Tokens t = tokenize(text);
		std::map<std::string, std::map<std::string, std::string>> entries;		// every entry, for inheritance
		std::size_t k = 0;
		if (k < t.words.size() && t.words[k] == "{")
		{
			k++;
		}
		while (k < t.words.size() && t.words[k] != "}")
		{
			const std::string name = t.words[k++];
			std::string gdf, parent;
			if (k + 2 < t.words.size() && t.words[k] == "(")
			{
				gdf = lower(t.words[k + 1]);
				k += 3;
			}
			else if (k + 2 < t.words.size() && t.words[k] == "[")
			{
				parent = t.words[k + 1];
				k += 3;
			}
			if (k >= t.words.size() || t.words[k] != "{")
			{
				return Status::error("gdt line " + std::to_string(k < t.lines.size() ? t.lines[k] : 0) + ": expected { after entry \"" + name + "\"");
			}
			std::map<std::string, std::string> keys;
			if (!parent.empty() && entries.count(parent))
			{
				keys = entries[parent];
				gdf = keys["__gdf"];
			}
			keys["__gdf"] = gdf;
			for (k++; k + 1 < t.words.size() && t.words[k] != "}"; k += 2)
			{
				keys[t.words[k]] = t.words[k + 1];
			}
			k++;
			entries[name] = keys;
			if (gdf != "material.gdf")
			{
				continue;
			}
			MaterialDef def;
			def.name = name;
			const auto value = [&](const char* key) -> std::string
			{
				const auto it = keys.find(key);
				return it == keys.end() ? std::string() : it->second;
			};
			const auto star = [](std::string v)
			{
				// Asset Manager marks defaults with a trailing *
				while (!v.empty() && (v.back() == '*' || v.back() == ' '))
				{
					v.pop_back();
				}
				return v;
			};
			def.type = lower(star(value("materialType")));
			if (def.type.empty())
			{
				def.type = "world phong";
			}
			const std::pair<const char*, const char*> maps[] = { { "colorMap", "Color" }, { "normalMap", "Normal" }, { "specularMap", "Specular" }, { "detailMap", "Detail" } };
			for (const auto& [sampler, suffix] : maps)
			{
				const std::string file = value(sampler);
				if (file.empty())
				{
					continue;
				}
				MaterialTextureRef ref;
				ref.sampler = sampler;
				ref.image = lower(std::filesystem::path(file).stem().string());
				def.imageFiles[sampler] = file;
				const std::string tile = lower(star(value((std::string("tile") + suffix).c_str())));
				const std::string filter = lower(star(value((std::string("filter") + suffix).c_str())));
				if (!tile.empty() || !filter.empty())
				{
					int s = 0x0B;
					s |= tile == "no tile" ? 0xE0 : tile == "tile horizontal" ? 0x40 : tile == "tile vertical" ? 0x20 : 0;
					if (filter.find("nomip nearest") != std::string::npos)
					{
						s = (s & 0xE0) | 0x01;
					}
					else if (filter.find("nomip bilinear") != std::string::npos)
					{
						s = (s & 0xE0) | 0x02;
					}
					else if (filter.find("4x bilinear") != std::string::npos)
					{
						s = (s & 0xE0) | 0x0C;
					}
					else if (filter.find("2x trilinear") != std::string::npos)
					{
						s = (s & 0xE0) | 0x13;
					}
					else if (filter.find("4x trilinear") != std::string::npos)
					{
						s = (s & 0xE0) | 0x14;
					}
					ref.samplerState = s;
				}
				def.textures.push_back(ref);
			}
			const std::string blend = lower(star(value("blendFunc")));
			if (!blend.empty() && blend != "custom")
			{
				set_blend(def, blend);
			}
			const std::string alpha = lower(star(value("alphaTest")));
			if (!alpha.empty())
			{
				set_alpha_test(def, alpha);
			}
			const std::string cull = lower(star(value("cullFace")));
			if (!cull.empty())
			{
				set_cull(def, cull);
			}
			const std::string depthWrite = lower(star(value("depthWrite")));
			if (depthWrite == "on" || depthWrite == "off")
			{
				set_field(def, state::DEPTH_WRITE, depthWrite == "on" ? state::DEPTH_WRITE : 0);
			}
			const std::string depthTest = lower(star(value("depthTest")));
			if (!depthTest.empty())
			{
				set_depth_test(def, depthTest);
			}
			const std::string offset = lower(star(value("polygonOffset")));
			if (!offset.empty())
			{
				set_polygon_offset(def, offset);
			}
			std::uint64_t number = 0;
			if (parse_number(star(value("sort")), number) && number < 256)
			{
				def.sortKey = static_cast<int>(number);
			}
			def.surfaceTypeBits = surface_type_bits(star(value("surfaceType")));
			if (!value("envMapMin").empty() || !value("envMapMax").empty() || !value("envMapExponent").empty())
			{
				MaterialConstant c;
				c.name = "envMapParms";
				c.value[0] = static_cast<float>(std::atof(value("envMapMin").c_str()));
				c.value[1] = static_cast<float>(std::atof(value("envMapMax").c_str()));
				c.value[2] = static_cast<float>(std::atof(value("envMapExponent").c_str()));
				c.value[3] = 0;
				if (c.value[1] != 0 || c.value[0] != 0)
				{
					def.constants.push_back(c);
				}
			}
			if (!value("colorTint").empty())
			{
				MaterialConstant c;
				c.name = "colorTint";
				if (std::sscanf(value("colorTint").c_str(), "%f %f %f %f", &c.value[0], &c.value[1], &c.value[2], &c.value[3]) >= 3)
				{
					c.value[3] = c.value[3] == 0 ? 1.0f : c.value[3];
					def.constants.push_back(c);
				}
			}
			out.push_back(std::move(def));
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	std::string techset_for(const MaterialDef& def)
	{
		if (!def.techset.empty())
		{
			return def.techset;
		}
		const auto has = [&](const char* sampler)
		{
			return std::any_of(def.textures.begin(), def.textures.end(), [&](const MaterialTextureRef& t) { return t.sampler == sampler; });
		};
		const std::string type = lower(def.type);
		const bool model = type.rfind("model", 0) == 0;
		const std::string prefix = model ? "mc_" : "wc_";
		const std::string blend = def.blendFunc.empty() ? "replace" : def.blendFunc;
		if (type == "2d")
		{
			return "2d";
		}
		if (type == "sky")
		{
			return "wc_sky";
		}
		if (type == "effect")
		{
			return "effect_" + std::string(blend == "replace" ? "blend" : blend);
		}
		if (type.find("unlit") != std::string::npos)
		{
			return prefix + "unlit_" + blend + "_lin";
		}
		// phong: <w|m>c_l_sm_<r|b|t>0c0[d0][n0][s0]: lit, shadow mapped; replace / blend / alpha tested; colour,
		// detail, normal, specular map
		const bool tested = !def.alphaTest.empty() && def.alphaTest != "none";
		std::string name = prefix + "l_sm_" + (blend != "replace" ? "b" : tested ? "t" : "r") + "0c0";
		name += has("detailMap") ? "d0" : "";
		name += has("normalMap") ? "n0" : "";
		name += has("specularMap") ? "s0" : "";
		return name;
	}
}
