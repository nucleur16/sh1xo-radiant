// codol_render_check - checks codol_render against the real game data and the owner's C# tool as an oracle.
//
//   codol_render_check survey   --game <install> [--samples <n>]
//       every .iwi of main\iw_image_NN.iwd: counts per IWI format, and sample names per format
//   codol_render_check iwd      --game <install> --raw <dir>
//       extracts images/<name>.iwi for every <name>.iwi under <dir> ( the oracle's `export-textures --raw` output )
//       and compares the bytes
//   codol_render_check textures --game <install> --oracle <dir>
//       decodes level 0 of every image that has a <name>.png under <dir> and compares the pixels ( PNG via WIC )
//   codol_render_check zone     --zone <file.zone> [--techsets <techset-dump.txt>] [--materials <zone-materials.txt>]
//                               [--assets <zone-assets.txt>]
//       locates shaders, technique sets and materials in a decrypted zone, parses them with the library and
//       compares with the oracle's dumps
//   codol_render_check names    --zone <file.zone> [--zone ..]
//       what the shaders' constant tables call the registers the code constant / code sampler arguments feed
//
// Everything is read only; nothing is written anywhere. Exit code 0: no mismatch.

#include "tools.hpp"

#include "codol_render/code_tables.hpp"
#include "codol_render/iwd.hpp"
#include "codol_render/material.hpp"
#include "codol_render/render_state.hpp"
#include "codol_render/shader_program.hpp"
#include "codol_render/technique_set.hpp"
#include "codol_render/texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

using namespace codol::render;
namespace fs = std::filesystem;

namespace
{
	struct arguments
	{
		std::multimap<std::string, std::string> values;

		std::string get(const std::string& key, const std::string& fallback = {}) const
		{
			const auto it = values.find(key);
			return it == values.end() ? fallback : it->second;
		}
		std::vector<std::string> all(const std::string& key) const
		{
			std::vector<std::string> out;
			const auto range = values.equal_range(key);
			for (auto it = range.first; it != range.second; ++it)
			{
				out.push_back(it->second);
			}
			return out;
		}
	};

	std::string lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	std::vector<std::string> split(const std::string& s, const char separator)
	{
		std::vector<std::string> out;
		std::size_t at = 0;
		for (;;)
		{
			const std::size_t next = s.find(separator, at);
			out.push_back(s.substr(at, next == std::string::npos ? std::string::npos : next - at));
			if (next == std::string::npos)
			{
				return out;
			}
			at = next + 1;
		}
	}

	std::vector<std::string> read_lines(const fs::path& path)
	{
		std::vector<std::string> lines;
		std::ifstream in(path, std::ios::binary);
		std::string line;
		while (std::getline(in, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}
			lines.push_back(line);
		}
		return lines;
	}

	// ---------------------------------------------------------------------------------------------------
	// the game's image archives

	struct image_archives
	{
		image_archive_sets sets;
		std::shared_ptr<const wavelet_tables> tables;

		bool open(const fs::path& game, const bool need_tables)
		{
			std::string error;
			iwd_key key{};
			const fs::path exe = game / "C1.exe";
			if (!derive_iwd_key(exe, key, &error))
			{
				std::printf("key: %s\n", error.c_str());
				return false;
			}
			if (need_tables)
			{
				tables = wavelet_tables::load_from_executable(exe, &error);
				if (!tables)
				{
					std::printf("wavelet tables: %s\n", error.c_str());
					return false;
				}
			}
			std::vector<std::string> warnings;
			if (!open_image_archives(game, key, sets, &error, &warnings))
			{
				std::printf("%s\n", error.c_str());
				return false;
			}
			for (const auto& warning : warnings)
			{
				std::printf("%s\n", warning.c_str());
			}
			return true;
		}

		// every archive, the set that is searched first in front
		std::vector<const iwd_archive*> all() const
		{
			std::vector<const iwd_archive*> out;
			for (const auto* set : { sets.images.get(), sets.stripped.get() })
			{
				if (set)
				{
					for (const auto& archive : set->archives())
					{
						out.push_back(archive.get());
					}
				}
			}
			return out;
		}
	};

	// ---------------------------------------------------------------------------------------------------
	// survey

	int survey(const arguments& args)
	{
		image_archives game;
		if (!game.open(args.get("game"), false))
		{
			return 2;
		}
		const int samples = std::atoi(args.get("samples", "6").c_str());
		struct sample
		{
			std::string name;
			int width, height;
			std::uint32_t size;
			bool cube;
		};
		std::map<int, std::vector<sample>> by_format;
		std::map<int, int> versions;
		int images = 0, cubes = 0, unreadable = 0, no_mips = 0;
		const auto archives = game.all();
		for (const auto* archive : archives)
		{
			int iwi = 0, other = 0;
			for (const auto& entry : archive->entries())
			{
				if (entry.name.size() < 4 || entry.name.compare(entry.name.size() - 4, 4, ".iwi") != 0)
				{
					other++;
					continue;
				}
				iwi++;
				std::vector<std::uint8_t> head;
				std::string error;
				iwi_header header;
				if (!archive->read_prefix(entry, 32, head, &error) || !parse_iwi_header(head, header, &error))
				{
					unreadable++;
					std::printf("  %s:%s: %s\n", archive->file_name().c_str(), entry.name.c_str(), error.c_str());
					continue;
				}
				images++;
				versions[header.version]++;
				cubes += header.is_cube() ? 1 : 0;
				no_mips += (header.flags & iwi_flags::NO_MIPMAPS) ? 1 : 0;
				std::string name = entry.name.substr(entry.name.find('/') + 1);
				name.resize(name.size() - 4);
				// only the copy the game would load ( the archive that wins the path ) is counted per format
				iwd_archive_set::hit winner;
				if (game.sets.find(name, winner) && winner.entry == &entry)
				{
					by_format[static_cast<int>(header.format)].push_back({ name, header.width, header.height, entry.uncompressed_size, header.is_cube() });
				}
			}
			std::printf("%-18s %6zu entries, %6d .iwi, %d other\n", archive->file_name().c_str(), archive->entries().size(), iwi, other);
		}
		std::printf("\n%d .iwi entries in %zu archives ( %zu distinct images + %zu paths in the stripped archives; %d + %d shadowed duplicates ),"
			" %d unreadable headers, %d cube maps, %d without mips\n", images, archives.size(), game.sets.images->entry_count(),
			game.sets.stripped ? game.sets.stripped->entry_count() : 0, game.sets.images->duplicate_names(), game.sets.stripped ? game.sets.stripped->duplicate_names() : 0,
			unreadable, cubes, no_mips);
		for (const auto& [version, count] : versions)
		{
			std::printf("IWI version %d: %d\n", version, count);
		}
		for (int format = 1; format <= 13; format++)
		{
			auto& list = by_format[format];
			std::printf("format %2d %-18s %6zu images\n", format, iwi_format_name(static_cast<iwi_format>(format)), list.size());
			// samples: spread over the sizes, cube maps first
			std::sort(list.begin(), list.end(), [](const sample& a, const sample& b) { return a.size != b.size ? a.size < b.size : a.name < b.name; });
			std::vector<const sample*> picked;
			for (const auto& s : list)
			{
				if (s.cube && picked.size() < 2)
				{
					picked.push_back(&s);
				}
			}
			for (int i = 0; i < samples && !list.empty(); i++)
			{
				const sample* s = &list[(list.size() - 1) * static_cast<std::size_t>(i) / static_cast<std::size_t>(std::max(samples - 1, 1))];
				if (s->size <= (24u << 20) && std::find(picked.begin(), picked.end(), s) == picked.end())
				{
					picked.push_back(s);
				}
			}
			for (const sample* s : picked)
			{
				std::printf("  sample %d %s %dx%d %u bytes%s\n", format, s->name.c_str(), s->width, s->height, s->size, s->cube ? " cube" : "");
			}
		}
		for (const auto& [format, list] : by_format)
		{
			if (format < 1 || format > 13)
			{
				std::printf("format %d ( unknown ): %zu\n", format, list.size());
			}
		}
		return unreadable == 0 ? 0 : 1;
	}

	// ---------------------------------------------------------------------------------------------------
	// iwd: byte for byte against the oracle's --raw files

	int check_iwd(const arguments& args)
	{
		image_archives game;
		if (!game.open(args.get("game"), false))
		{
			return 2;
		}
		std::size_t total_entries = 0;
		const auto archives = game.all();
		for (const auto* archive : archives)
		{
			total_entries += archive->entries().size();
		}
		std::printf("opened %zu encrypted archives, %zu entries\n", archives.size(), total_entries);

		int compared = 0, identical = 0, missing = 0, failed = 0;
		std::set<std::string> used_archives;
		std::error_code ec;
		for (const auto& file : fs::recursive_directory_iterator(args.get("raw"), ec))
		{
			if (file.path().extension() != ".iwi")
			{
				continue;
			}
			const std::string path = "images/" + file.path().stem().string() + ".iwi";
			iwd_archive_set::hit found;
			if (!game.sets.find(file.path().stem().string(), found))
			{
				missing++;
				std::printf("  %s: not in the archives\n", path.c_str());
				continue;
			}
			std::vector<std::uint8_t> expected, data;
			std::string error;
			if (!tools::read_file(file.path(), expected) || !found.archive->read(*found.entry, data, &error))
			{
				failed++;
				std::printf("  %s: %s\n", path.c_str(), error.c_str());
				continue;
			}
			compared++;
			used_archives.insert(found.archive->file_name());
			if (data == expected)
			{
				identical++;
			}
			else
			{
				std::printf("  %s: %zu bytes differ from the oracle's %zu\n", path.c_str(), data.size(), expected.size());
			}
		}
		std::printf("iwd: %d files compared with the oracle's raw output, %d identical, %d differ, %d not found, %d unreadable; from %zu archives:",
			compared, identical, compared - identical, missing, failed, used_archives.size());
		for (const auto& name : used_archives)
		{
			std::printf(" %s", name.c_str());
		}
		std::printf("\n");
		return compared > 0 && compared == identical && missing == 0 && failed == 0 ? 0 : 1;
	}

	// ---------------------------------------------------------------------------------------------------
	// textures: level 0 against the oracle's PNG

	int check_textures(const arguments& args)
	{
		image_archives game;
		if (!game.open(args.get("game"), true))
		{
			return 2;
		}
		struct stats
		{
			int images = 0;
			int exact = 0;
			int max_diff = 0;
			int surfaces = 0;
			int cubes = 0;
		};
		std::map<int, stats> by_format;
		int problems = 0, compared = 0;
		std::error_code ec;
		std::vector<fs::path> pngs;
		for (const auto& file : fs::recursive_directory_iterator(args.get("oracle"), ec))
		{
			if (file.path().extension() == ".png")
			{
				pngs.push_back(file.path());
			}
		}
		std::sort(pngs.begin(), pngs.end());
		for (const auto& png_path : pngs)
		{
			// the oracle writes <name>.png for level 0 ( face 0 ) and <name>_<face>.png for the faces of a cube map
			std::string name = png_path.stem().string();
			int face = 0;
			iwd_archive_set::hit found;
			bool have = game.sets.find(name, found);
			if (!have)
			{
				static const char* const FACES[6] = { "_px", "_nx", "_py", "_ny", "_pz", "_nz" };
				for (int f = 0; f < 6 && !have; f++)
				{
					const std::string suffix = FACES[f];
					if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
					{
						have = game.sets.find(name.substr(0, name.size() - suffix.size()), found);
						face = f;
					}
				}
			}
			if (!have)
			{
				std::printf("  %s: no such image in the archives ( skipped )\n", name.c_str());
				continue;
			}
			std::vector<std::uint8_t> data;
			std::string error;
			iwi_file iwi;
			texture_upload upload;
			rgba_image mine;
			tools::png_image png;
			if (!found.archive->read(*found.entry, data, &error) || !parse_iwi(data, iwi, &error)
				|| !build_texture_upload(iwi, game.tables.get(), upload, &error))
			{
				problems++;
				std::printf("  %s: %s\n", name.c_str(), error.c_str());
				continue;
			}
			const texture_surface* surface = upload.surface(0, face);
			if (!surface || !upload_surface_to_rgba(upload, *surface, mine, &error) || !tools::load_png(png_path, png, error))
			{
				problems++;
				std::printf("  %s: %s\n", name.c_str(), surface ? error.c_str() : "no such surface");
				continue;
			}
			stats& s = by_format[static_cast<int>(iwi.header.format)];
			if (png.width != mine.width || png.height != mine.height)
			{
				problems++;
				std::printf("  %s: %dx%d, the oracle's PNG is %dx%d\n", name.c_str(), mine.width, mine.height, png.width, png.height);
				continue;
			}
			int diff = 0;
			for (std::size_t i = 0; i < mine.pixels.size(); i++)
			{
				diff = std::max(diff, std::abs(static_cast<int>(mine.pixels[i]) - static_cast<int>(png.rgba[i])));
			}
			compared++;
			s.images++;
			s.exact += diff == 0 ? 1 : 0;
			s.max_diff = std::max(s.max_diff, diff);
			s.surfaces += static_cast<int>(upload.surfaces.size());
			s.cubes += upload.is_cube() ? 1 : 0;
			// every level of the upload must have the size Direct3D expects for it
			for (const auto& level : upload.surfaces)
			{
				if (level.data.size() != static_cast<std::size_t>(level.rows) * level.row_bytes
					|| level.width != iwi_level_dimension(upload.width, level.level) || level.height != iwi_level_dimension(upload.height, level.level))
				{
					problems++;
					std::printf("  %s: level %d face %d has %zu bytes for %d rows of %d\n", name.c_str(), level.level, level.face, level.data.size(), level.rows, level.row_bytes);
				}
			}
			if (static_cast<int>(upload.surfaces.size()) != upload.levels * upload.faces)
			{
				problems++;
				std::printf("  %s: %zu surfaces for %d levels x %d faces\n", name.c_str(), upload.surfaces.size(), upload.levels, upload.faces);
			}
			if (diff != 0)
			{
				std::printf("  %s ( %s, %dx%d ): max channel difference %d\n", name.c_str(), iwi_format_name(iwi.header.format), mine.width, mine.height, diff);
			}
		}
		std::printf("\n%-4s %-18s %7s %7s %9s %9s %6s\n", "fmt", "name", "images", "exact", "max diff", "surfaces", "cubes");
		bool all_exact = true;
		for (int format = 1; format <= 13; format++)
		{
			const stats& s = by_format[format];
			if (s.images == 0)
			{
				std::printf("%-4d %-18s %7s\n", format, iwi_format_name(static_cast<iwi_format>(format)), "-");
				continue;
			}
			std::printf("%-4d %-18s %7d %7d %9d %9d %6d\n", format, iwi_format_name(static_cast<iwi_format>(format)), s.images, s.exact, s.max_diff, s.surfaces, s.cubes);
			all_exact = all_exact && s.exact == s.images;
		}
		std::printf("textures: %d images compared pixel for pixel with the oracle's PNG, %d problems\n", compared, problems);
		return compared > 0 && problems == 0 && all_exact ? 0 : 1;
	}

	// ---------------------------------------------------------------------------------------------------
	// zone: shaders, technique sets, materials

	struct oracle_arg
	{
		int type = 0;
		int dest = 0;
		std::string value;
	};
	struct oracle_pass
	{
		std::string vs, ps;
		int counts[3] = {};
		std::vector<oracle_arg> args;
	};
	struct oracle_technique
	{
		std::string name;
		unsigned flags = 0;
		int pass_count = 0;
		std::vector<oracle_pass> passes;
	};
	struct oracle_techset
	{
		std::string name;
		std::map<int, oracle_technique> slots;
	};

	// the output of `CodolSoundDumper techset-dump --name ""` ( PortCommands.cs:937-998 )
	std::vector<oracle_techset> parse_techset_dump(const fs::path& path)
	{
		std::vector<oracle_techset> sets;
		oracle_technique* technique = nullptr;
		oracle_pass* pass = nullptr;
		for (const std::string& line : read_lines(path))
		{
			char a[512] = {}, b[512] = {};
			int n0 = 0, n1 = 0, n2 = 0, n3 = 0;
			unsigned flags = 0;
			if (line.rfind("techset ", 0) == 0)
			{
				const std::size_t at = line.rfind(" @0x");
				sets.push_back({ line.substr(8, at == std::string::npos ? std::string::npos : at - 8), {} });
				technique = nullptr;
				pass = nullptr;
			}
			else if (!sets.empty() && std::sscanf(line.c_str(), "  [%d] %511s flags %x passes %d", &n0, a, &flags, &n1) == 4)
			{
				technique = &sets.back().slots[n0];
				technique->name = a;
				technique->flags = flags;
				technique->pass_count = n1;
				pass = nullptr;
			}
			else if (technique && std::sscanf(line.c_str(), "    pass %d: vs %511s ps %511s counts %d/%d/%d", &n0, a, b, &n1, &n2, &n3) == 6)
			{
				technique->passes.push_back({});
				pass = &technique->passes.back();
				pass->vs = a;
				pass->ps = b;
				pass->counts[0] = n1;
				pass->counts[1] = n2;
				pass->counts[2] = n3;
			}
			else if (pass && std::sscanf(line.c_str(), "      arg type %d dest %d %511s", &n0, &n1, a) == 3)
			{
				pass->args.push_back({ n0, n1, a });
			}
		}
		return sets;
	}

	struct mismatch_log
	{
		int count = 0;
		void add(const std::string& text)
		{
			if (count++ < 25)
			{
				std::printf("  MISMATCH %s\n", text.c_str());
			}
		}
	};

	std::string shader_text(const std::optional<shader_ref>& shader)
	{
		return !shader ? "?" : (shader->is_reference ? "," : "") + shader->name;
	}

	// What a shader's constant table calls the register an argument feeds, per code index
	struct name_survey
	{
		std::map<int, std::map<std::string, int>> constants;	// code constant index -> CTAB name -> uses
		std::map<int, std::map<std::string, int>> samplers;		// code sampler index -> CTAB name -> uses
		int code_args = 0, code_args_named = 0, code_args_unnamed = 0;
		int material_args = 0, material_hash_ok = 0, material_hash_bad = 0, material_unnamed = 0;
		int row_overflows = 0;
		int passes = 0, passes_without_programs = 0;
		std::set<std::string> bad_hashes;

		void add_pass(const zone_view& zone, const render_pass& pass, std::map<std::uint32_t, shader_program>& programs)
		{
			const auto program_of = [&](const std::optional<shader_ref>& shader) -> const shader_program*
			{
				if (!shader || !shader->has_program())
				{
					return nullptr;
				}
				auto it = programs.find(shader->program);
				if (it == programs.end())
				{
					shader_program parsed;
					if (!parse_shader_program(shader_code(zone, *shader), parsed))
					{
						return nullptr;
					}
					it = programs.emplace(shader->program, std::move(parsed)).first;
				}
				return &it->second;
			};
			const shader_program* vs = program_of(pass.vertex_shader);
			const shader_program* ps = program_of(pass.pixel_shader);
			passes++;
			if (!vs && !ps)
			{
				passes_without_programs++;
			}
			for (const auto& arg : pass.arguments)
			{
				const shader_program* program = arg.is_vertex_stage() ? vs : ps;
				if (!program || arg.is_literal())
				{
					continue;
				}
				// GameMapRenderer.cs:1492-1508 ( ArgumentNames )
				const register_set set = arg.is_sampler() ? register_set::sampler : register_set::float4;
				int row = 0;
				const shader_constant* constant = program->find_constant(set, arg.dest, &row);
				if (arg.is_code_const() || arg.type == shader_argument_type::code_pixel_sampler)
				{
					code_args++;
					if (!constant)
					{
						code_args_unnamed++;
						continue;
					}
					code_args_named++;
					if (arg.type == shader_argument_type::code_pixel_sampler)
					{
						samplers[static_cast<int>(arg.value)][constant->name]++;
					}
					else
					{
						constants[arg.code_index()][constant->name]++;
						if (arg.code_index() >= build::FIRST_CODE_MATRIX && row + arg.row_count() > std::max<int>(constant->register_count, 1))
						{
							row_overflows++;
						}
					}
				}
				else
				{
					// material constants and samplers are found by the hash of their HLSL name
					material_args++;
					if (!constant)
					{
						material_unnamed++;
						continue;
					}
					std::string name = constant->name;
					if (hash_sampler_name(name) == arg.name_hash())
					{
						material_hash_ok++;
						continue;
					}
					if (name.size() > 7 && name.compare(name.size() - 7, 7, "Sampler") == 0 && hash_sampler_name(name.substr(0, name.size() - 7)) == arg.name_hash())
					{
						material_hash_ok++;
						continue;
					}
					material_hash_bad++;
					bad_hashes.insert(name + " != " + hex32(arg.name_hash()));
				}
			}
		}

		void print() const
		{
			std::printf("\ncode constant index -> name of the register in the shaders' constant tables ( uses )\n");
			for (const auto& [index, names] : constants)
			{
				const code_name* expected = code_constant(index);
				std::string text;
				bool match = false;
				for (const auto& [name, uses] : names)
				{
					text += " " + name + "(" + std::to_string(uses) + ")";
					match = match || (expected && name == expected->name);
				}
				std::printf("  %3d %-44s %s%s\n", index, expected ? expected->name : "?", match ? "=" : "!", text.c_str());
			}
			std::printf("code sampler index -> name of the sampler register ( uses )\n");
			for (const auto& [index, names] : samplers)
			{
				const code_name* expected = code_sampler(index);
				std::string text;
				bool match = false;
				for (const auto& [name, uses] : names)
				{
					text += " " + name + "(" + std::to_string(uses) + ")";
					match = match || (expected && name == expected->name);
				}
				std::printf("  %3d %-44s %s%s\n", index, expected ? expected->name : "?", match ? "=" : "!", text.c_str());
			}
			std::printf("passes %d ( %d without a program in this zone ); code arguments %d: %d feed a register the constant table names, %d do not;"
				" matrix arguments that write past their register range: %d\n", passes, passes_without_programs, code_args, code_args_named, code_args_unnamed, row_overflows);
			std::printf("material arguments %d: name hash equals the hash of the register's name %d, differs %d, register not in the table %d\n",
				material_args, material_hash_ok, material_hash_bad, material_unnamed);
			for (const auto& text : bad_hashes)
			{
				std::printf("  hash differs: %s\n", text.c_str());
			}
		}
	};

	void survey_names(const tools::scanned_zone& zone, name_survey& survey)
	{
		const zone_view view = zone.view();
		std::map<std::uint32_t, shader_program> programs;
		std::set<std::uint32_t> seen_techniques;
		for (const std::uint32_t pointer : zone.techsets)
		{
			render_technique_set set;
			if (!read_technique_set(view, pointer, set))
			{
				continue;
			}
			for (const auto& technique : set.techniques)
			{
				if (!seen_techniques.insert(technique.pointer).second)
				{
					continue;
				}
				for (const auto& pass : technique.passes)
				{
					survey.add_pass(view, pass, programs);
				}
			}
		}
	}

	int check_names(const arguments& args)
	{
		name_survey survey;
		for (const std::string& path : args.all("zone"))
		{
			tools::scanned_zone zone;
			std::string error;
			if (!tools::scan_zone(path, zone, error))
			{
				std::printf("%s\n", error.c_str());
				return 2;
			}
			std::printf("%s: %zu technique sets, %zu shaders\n", fs::path(path).filename().string().c_str(), zone.techsets.size(), zone.shaders.size());
			survey_names(zone, survey);
		}
		survey.print();
		return 0;
	}

	int check_zone(const arguments& args)
	{
		tools::scanned_zone zone;
		std::string error;
		if (!tools::scan_zone(args.get("zone"), zone, error))
		{
			std::printf("%s\n", error.c_str());
			return 2;
		}
		const zone_view view = zone.view();
		mismatch_log mismatches;
		std::printf("%s: %zu bytes\n", fs::path(args.get("zone")).filename().string().c_str(), zone.image.size());
		for (const auto& text : zone.rejected)
		{
			std::printf("  rejected %s\n", text.c_str());
		}

		// ---- shaders -----------------------------------------------------------------------------------------
		{
			int parsed = 0, failed = 0, constants = 0, with_header = 0, pixel = 0, vertex = 0, target_mismatch = 0, no_creator = 0, sm3 = 0;
			std::map<std::string, int> named[2];		// [is pixel] name -> count
			for (const auto& shader : zone.shaders)
			{
				shader_program program;
				if (!parse_shader_program(byte_span(zone.image.data() + shader.program_offset, shader.program_size), program, &error))
				{
					failed++;
					std::printf("  shader at 0x%X: %s\n", shader.program_offset, error.c_str());
					continue;
				}
				parsed++;
				constants += static_cast<int>(program.constants.size());
				with_header += shader.header_matches ? 1 : 0;
				(program.is_pixel_shader() ? pixel : vertex)++;
				sm3 += program.major() == 3 ? 1 : 0;
				no_creator += program.creator.empty() ? 1 : 0;
				// the table's target string must agree with the version token
				const std::string expected_target = std::string(program.is_pixel_shader() ? "ps_" : "vs_") + std::to_string(program.major()) + "_" + std::to_string(program.minor());
				target_mismatch += program.target == expected_target ? 0 : 1;
				// every constant's registers must make sense
				for (const auto& c : program.constants)
				{
					if (c.name.empty() || static_cast<int>(c.set) > 3 || c.register_index + c.register_count > 256)
					{
						mismatches.add("shader " + shader.name + ": implausible constant " + c.name);
					}
				}
				if (!shader.name.empty())
				{
					named[program.is_pixel_shader() ? 1 : 0][shader.name]++;
				}
			}
			std::printf("shaders: %zu programs located ( %d pixel, %d vertex, %d shader model 3 ), %d parsed, %d failed, %d constants in their tables;"
				" %d preceded by a MaterialShader header whose programSize matches; target string != version token: %d; without creator: %d\n",
				zone.shaders.size(), pixel, vertex, sm3, parsed, failed, constants, with_header, target_mismatch, no_creator);
			mismatches.count += failed + target_mismatch;

			if (!args.get("assets").empty())
			{
				// zone-assets lists "type name" of every named asset; a ",name" is a reference without a program here
				int listed = 0, found = 0, references = 0;
				std::set<std::string> oracle_names[2];
				for (const std::string& line : read_lines(args.get("assets")))
				{
					const bool is_pixel = line.rfind("pixelshader ", 0) == 0;
					if (!is_pixel && line.rfind("vertexshader ", 0) != 0)
					{
						continue;
					}
					const std::string name = line.substr(line.find(' ') + 1);
					if (!name.empty() && name[0] == ',')
					{
						references++;
						continue;
					}
					listed++;
					oracle_names[is_pixel ? 1 : 0].insert(name);
					if (named[is_pixel ? 1 : 0].count(name))
					{
						found++;
					}
					else
					{
						mismatches.add(std::string(is_pixel ? "pixel" : "vertex") + " shader " + name + " of the oracle's asset list was not located");
					}
				}
				int extra = 0;
				for (int kind = 0; kind < 2; kind++)
				{
					for (const auto& [name, count] : named[kind])
					{
						if (!oracle_names[kind].count(name))
						{
							extra++;
							mismatches.add(std::string(kind ? "pixel" : "vertex") + " shader " + name + " is not in the oracle's asset list");
						}
					}
				}
				std::printf("shaders vs zone-assets: the oracle lists %d shaders with a program ( + %d references ); located with the same name and kind: %d;"
					" located but not listed: %d\n", listed, references, found, extra);
			}
		}

		// ---- technique sets ----------------------------------------------------------------------------------
		std::map<std::string, render_technique_set> sets_by_name;
		{
			int read_ok = 0;
			for (const std::uint32_t pointer : zone.techsets)
			{
				render_technique_set set;
				if (!read_technique_set(view, pointer, set, &error))
				{
					mismatches.add("technique set at " + hex32(pointer) + ": " + error);
					continue;
				}
				read_ok++;
				sets_by_name.emplace(set.name, std::move(set));
			}
			std::printf("technique sets: %d header shaped candidates, %d walked and read ( %zu distinct names ), %d rejected\n",
				zone.techset_candidates, read_ok, sets_by_name.size(), zone.techset_rejected);
		}
		if (!args.get("techsets").empty())
		{
			const auto oracle = parse_techset_dump(args.get("techsets"));
			int oracle_defined = 0, oracle_references = 0, found = 0, found_references = 0;
			int slots_equal = 0, slots_empty_equal = 0, slots_by_reference = 0, techniques = 0, passes = 0, args_compared = 0, literals = 0;
			int shader_names = 0, shader_by_reference = 0, args_by_reference = 0, names_by_reference = 0;
			for (const auto& expected : oracle)
			{
				const bool reference = !expected.name.empty() && expected.name[0] == ',';
				(reference ? oracle_references : oracle_defined)++;
				const auto it = sets_by_name.find(expected.name);
				if (it == sets_by_name.end())
				{
					mismatches.add("technique set " + expected.name + " of the oracle was not located");
					continue;
				}
				(reference ? found_references : found)++;
				const render_technique_set& mine = it->second;
				const byte_span header = view.read(mine.pointer, build::TECHSET_SIZE);
				for (int t = 0; t < build::TECHNIQUE_COUNT; t++)
				{
					const std::uint32_t raw = rd_u32(header.data() + build::TECHSET_TECHNIQUES + 4 * static_cast<std::size_t>(t));
					const auto slot = expected.slots.find(t);
					const std::string where = expected.name + " [" + std::to_string(t) + "]";
					if (raw == 0)
					{
						if (slot != expected.slots.end())
						{
							mismatches.add(where + ": empty here, " + slot->second.name + " in the oracle");
						}
						else
						{
							slots_empty_equal++;
						}
						continue;
					}
					if (slot == expected.slots.end())
					{
						mismatches.add(where + ": a technique here, none in the oracle");
						continue;
					}
					const render_technique* technique = mine.technique(t);
					if (!technique)
					{
						slots_by_reference++;	// loaded earlier in the zone: needs the zone reader
						continue;
					}
					const oracle_technique& e = slot->second;
					bool equal = true;
					const auto differ = [&](const std::string& text)
					{
						equal = false;
						mismatches.add(where + " " + technique->name + ": " + text);
					};
					techniques++;
					if (technique->name.rfind("technique_", 0) == 0)
					{
						names_by_reference++;
					}
					else if (technique->name != e.name)
					{
						differ("name " + e.name + " in the oracle");
					}
					if (technique->flags != e.flags)
					{
						differ("flags");
					}
					if (static_cast<int>(technique->passes.size()) != e.pass_count || e.passes.size() != technique->passes.size())
					{
						differ("pass count");
						continue;
					}
					for (std::size_t p = 0; p < technique->passes.size(); p++)
					{
						const render_pass& pass = technique->passes[p];
						const oracle_pass& ep = e.passes[p];
						passes++;
						if (pass.per_prim_arg_count != ep.counts[0] || pass.per_obj_arg_count != ep.counts[1] || pass.stable_arg_count != ep.counts[2])
						{
							differ("pass " + std::to_string(p) + " argument counts");
						}
						for (int stage = 0; stage < 2; stage++)
						{
							const auto& shader = stage == 0 ? pass.vertex_shader : pass.pixel_shader;
							const std::string& expected_name = stage == 0 ? ep.vs : ep.ps;
							if (!shader)
							{
								shader_by_reference++;
							}
							else if (shader_text(shader) != expected_name)
							{
								differ("pass " + std::to_string(p) + (stage == 0 ? " vs " : " ps ") + shader_text(shader) + ", " + expected_name + " in the oracle");
							}
							else
							{
								shader_names++;
							}
						}
						const std::size_t count = static_cast<std::size_t>(ep.counts[0]) + ep.counts[1] + ep.counts[2];
						if (pass.arguments.size() != count)
						{
							args_by_reference += static_cast<int>(count);
							continue;
						}
						if (ep.args.size() != count)
						{
							differ("pass " + std::to_string(p) + ": the oracle lists " + std::to_string(ep.args.size()) + " arguments");
							continue;
						}
						for (std::size_t a = 0; a < count; a++)
						{
							const shader_argument& arg = pass.arguments[a];
							const oracle_arg& ea = ep.args[a];
							args_compared++;
							bool ok = static_cast<int>(arg.type) == ea.type && arg.dest == ea.dest;
							if (ea.value.rfind("0x", 0) == 0)
							{
								// a literal whose pointer the oracle did not resolve prints the raw field: not comparable with a patched pointer
								ok = ok && (arg.is_literal() || std::strtoul(ea.value.c_str(), nullptr, 16) == arg.value);
							}
							else
							{
								// literal floats, "0.###" each, joined with ','
								literals++;
								const auto parts = split(ea.value, ',');
								ok = ok && arg.is_literal() && parts.size() == 4;
								for (std::size_t k = 0; ok && k < 4; k++)
								{
									ok = std::fabs(std::atof(parts[k].c_str()) - static_cast<double>(arg.literal[k])) <= 0.00051;
								}
							}
							if (!ok)
							{
								char text[160];
								std::snprintf(text, sizeof text, "pass %zu arg %zu: type %d dest %d value 0x%08X literal %g,%g,%g,%g; oracle type %d dest %d %s", p, a,
									static_cast<int>(arg.type), arg.dest, arg.value, arg.literal[0], arg.literal[1], arg.literal[2], arg.literal[3], ea.type, ea.dest, ea.value.c_str());
								differ(text);
							}
						}
					}
					slots_equal += equal ? 1 : 0;
				}
			}
			std::printf("technique sets vs techset-dump: the oracle has %d defined here + %d references; located %d + %d\n", oracle_defined, oracle_references, found, found_references);
			std::printf("  slots: %d with a technique compared ( %d equal ), %d empty in both, %d point at a technique loaded earlier ( not resolvable without the zone reader )\n",
				techniques, slots_equal, slots_empty_equal, slots_by_reference);
			std::printf("  in the compared techniques: %d passes, %d arguments ( %d of them literals ), %d shader names equal;"
				" not comparable because the pointer is a reference: %d technique names, %d shaders, %d arguments\n",
				passes, args_compared, literals, shader_names, names_by_reference, shader_by_reference, args_by_reference);
		}

		// ---- materials ---------------------------------------------------------------------------------------
		std::map<std::string, material> materials_by_name;
		{
			int read_ok = 0, with_state = 0, state_entries = 0, state_entries_bad = 0, translated = 0, implausible = 0, not_drawn = 0;
			int techset_checked = 0, techset_slot_checks = 0, techset_slot_bad = 0, textures = 0, constants = 0, embedded = 0;
			std::set<std::pair<std::uint32_t, std::uint32_t>> distinct_bits;
			for (const std::uint32_t pointer : zone.materials)
			{
				material m;
				if (!read_material(view, pointer, m, &error))
				{
					mismatches.add("material at " + hex32(pointer) + ": " + error);
					continue;
				}
				read_ok++;
				textures += static_cast<int>(m.textures.size());
				constants += static_cast<int>(m.constants.size());
				for (const auto& t : m.textures)
				{
					embedded += t.embedded ? 1 : 0;
				}
				if (m.state)
				{
					with_state++;
					not_drawn += m.state->not_drawn() ? 1 : 0;
					for (const std::uint8_t entry : m.state->entries)
					{
						if (entry != 0xFF)
						{
							state_entries++;
							state_entries_bad += entry < m.state->table.size() ? 0 : 1;
						}
					}
					for (const auto& bits : m.state->table)
					{
						distinct_bits.emplace(bits.bits0, bits.bits1);
					}
					// the material's state entries against its technique set, when that is in this zone:
					// a technique with n passes needs entries entry .. entry + n - 1
					if (m.technique_set != 0 && !m.technique_set_name.empty())
					{
						const auto it = sets_by_name.find(m.technique_set_name);
						render_technique_set inline_set;
						const render_technique_set* set = it != sets_by_name.end() ? &it->second : nullptr;
						if (!set && read_technique_set(view, m.technique_set, inline_set) && !is_reference_name(inline_set.name))
						{
							set = &inline_set;
						}
						if (set && !set->techniques.empty())
						{
							techset_checked++;
							for (int t = 0; t < build::TECHNIQUE_COUNT; t++)
							{
								const render_technique* technique = set->technique(t);
								if (!technique)
								{
									continue;
								}
								techset_slot_checks++;
								const int last_pass = static_cast<int>(technique->passes.size()) - 1;
								if (!m.state->pass_bits(t, 0) || (last_pass > 0 && !m.state->pass_bits(t, last_pass)))
								{
									techset_slot_bad++;
									mismatches.add("material " + m.name + ": technique slot " + std::to_string(t) + " ( " + technique->name + ", "
										+ std::to_string(technique->passes.size()) + " passes ) has no state bits entry for every pass");
								}
							}
						}
					}
				}
				materials_by_name.emplace(m.name, std::move(m));
			}
			// every distinct state bits pair must translate to values Direct3D accepts
			for (const auto& [bits0, bits1] : distinct_bits)
			{
				translated++;
				bool ok = true;
				for (const auto& s : translate_state_bits(bits0, bits1))
				{
					switch (s.state)
					{
					case d3d::RS_SRCBLEND: case d3d::RS_DESTBLEND: case d3d::RS_SRCBLENDALPHA: case d3d::RS_DESTBLENDALPHA: ok = ok && s.value >= 1 && s.value <= 15; break;
					case d3d::RS_BLENDOP: case d3d::RS_BLENDOPALPHA: ok = ok && s.value >= 1 && s.value <= 5; break;
					case d3d::RS_CULLMODE: ok = ok && s.value >= 1 && s.value <= 3; break;
					case d3d::RS_ALPHAFUNC: case d3d::RS_ZFUNC: case d3d::RS_STENCILFUNC: case d3d::RS_CCW_STENCILFUNC: ok = ok && s.value >= 1 && s.value <= 8; break;
					case d3d::RS_STENCILFAIL: case d3d::RS_STENCILZFAIL: case d3d::RS_STENCILPASS:
					case d3d::RS_CCW_STENCILFAIL: case d3d::RS_CCW_STENCILZFAIL: case d3d::RS_CCW_STENCILPASS: ok = ok && s.value >= 1 && s.value <= 8; break;
					default: break;
					}
				}
				if (!ok)
				{
					implausible++;
					if (implausible <= 10)
					{
						std::printf("  state bits %08X %08X translate to a value outside the Direct3D enum range\n", bits0, bits1);
					}
				}
			}
			std::printf("materials: %d header shaped candidates, %d walked and read ( %zu partially: a water map follows ), %d rejected\n",
				zone.material_candidates, read_ok, zone.partial_materials.size(), zone.material_rejected);
			std::printf("  %d texture defs ( %d with pixels in the zone ), %d constants; %d materials with state bits, %d of them not drawn ( camera region >= 4 )\n",
				textures, embedded, constants, with_state, not_drawn);
			std::printf("  state bits entries: %d used, %d index past the table; distinct state bits pairs %d, outside the Direct3D ranges %d\n",
				state_entries, state_entries_bad, translated, implausible);
			std::printf("  material vs its technique set ( both located ): %d materials, %d technique slots, %d without state bits for every pass\n",
				techset_checked, techset_slot_checks, techset_slot_bad);
			mismatches.count += state_entries_bad;
		}
		if (!args.get("materials").empty())
		{
			// zone-materials: name \t techset \t "sort S flags FF sf FF" \t "semantic:image .." \t models ( PortCommands.cs:881-933 )
			int listed = 0, references = 0, found = 0, equal = 0, techset_names = 0, techset_by_reference = 0, image_names = 0, image_by_reference = 0, semantics = 0;
			for (const std::string& line : read_lines(args.get("materials")))
			{
				const auto fields = split(line, '\t');
				if (fields.size() < 4)
				{
					continue;
				}
				if (!fields[0].empty() && fields[0][0] == ',')
				{
					references++;
					const auto it = materials_by_name.find(fields[0].substr(1));
					if (it == materials_by_name.end() || !it->second.is_reference)
					{
						// a reference stub is a header of zeros: the scanner only accepts those it can tell apart; not an error
					}
					continue;
				}
				listed++;
				const auto it = materials_by_name.find(fields[0]);
				if (it == materials_by_name.end())
				{
					mismatches.add("material " + fields[0] + " of the oracle was not located");
					continue;
				}
				found++;
				const material& m = it->second;
				bool ok = true;
				const auto differ = [&](const std::string& text)
				{
					ok = false;
					mismatches.add("material " + m.name + ": " + text + "  | oracle: " + line.substr(0, 200));
				};
				int sort = 0;
				unsigned flags = 0, sf = 0;
				if (std::sscanf(fields[2].c_str(), "sort %d flags %x sf %x", &sort, &flags, &sf) != 3)
				{
					continue;
				}
				const byte_span header = view.read(m.pointer, build::MATERIAL_SIZE);
				if (header[build::MATERIAL_SORT_KEY] != sort || m.game_flags != flags || m.state_flags != sf)
				{
					differ("sort key / game flags / state flags");
				}
				if (m.state && m.state->sort_key != sort)
				{
					differ("state sort key");
				}
				if (m.technique_set_name.empty())
				{
					techset_by_reference++;
				}
				else if (m.technique_set_name != std::string(trim_reference(fields[1])))
				{
					differ("technique set " + m.technique_set_name);
				}
				else
				{
					techset_names++;
				}
				std::vector<std::string> images;
				for (const auto& part : split(fields[3], ' '))
				{
					if (!part.empty())
					{
						images.push_back(part);
					}
				}
				if (images.size() != m.textures.size())
				{
					differ(std::to_string(m.textures.size()) + " textures");
					continue;
				}
				for (std::size_t i = 0; i < images.size(); i++)
				{
					const std::size_t colon = images[i].find(':');
					const int semantic = std::atoi(images[i].substr(0, colon).c_str());
					const std::string image = colon == std::string::npos ? "" : images[i].substr(colon + 1);
					if (semantic != m.textures[i].semantic)
					{
						differ("texture " + std::to_string(i) + " semantic " + std::to_string(m.textures[i].semantic));
					}
					else
					{
						semantics++;
					}
					if (!m.textures[i].has_image || m.textures[i].image_name.empty())
					{
						image_by_reference++;
					}
					else if (m.textures[i].image_name != std::string(trim_reference(image)))
					{
						differ("texture " + std::to_string(i) + " image " + m.textures[i].image_name);
					}
					else
					{
						image_names++;
					}
				}
				equal += ok ? 1 : 0;
			}
			std::printf("materials vs zone-materials: the oracle lists %d materials defined here ( + %d references ); located %d; every compared field equal: %d\n",
				listed, references, found, equal);
			std::printf("  compared: sort key, game flags, state flags of all %d; %d technique set names ( %d by reference ); %d texture semantics; %d image names"
				" ( %d images by reference / water / null )\n", found, techset_names, techset_by_reference, semantics, image_names, image_by_reference);
		}

		name_survey survey;
		survey_names(zone, survey);
		survey.print();

		std::printf("\n%s: %d mismatches\n", fs::path(args.get("zone")).filename().string().c_str(), mismatches.count);
		return mismatches.count == 0 ? 0 : 1;
	}
}

int main(const int argc, char** argv)
{
	if (argc < 2)
	{
		std::printf("usage: codol_render_check survey|iwd|textures|zone|names [--key value ..]   ( see the head of tools/realdata_check.cpp )\n");
		return 2;
	}
	arguments args;
	for (int i = 2; i + 1 < argc; i += 2)
	{
		if (std::strncmp(argv[i], "--", 2) != 0)
		{
			std::printf("unexpected argument %s\n", argv[i]);
			return 2;
		}
		args.values.emplace(argv[i] + 2, argv[i + 1]);
	}
	const std::string command = argv[1];
	if (command == "survey")
	{
		return survey(args);
	}
	if (command == "iwd")
	{
		return check_iwd(args);
	}
	if (command == "textures")
	{
		return check_textures(args);
	}
	if (command == "zone")
	{
		return check_zone(args);
	}
	if (command == "names")
	{
		return check_names(args);
	}
	std::printf("unknown command %s\n", command.c_str());
	return 2;
}
