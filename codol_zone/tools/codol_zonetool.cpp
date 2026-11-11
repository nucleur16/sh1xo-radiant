// codol_zonetool: command line front end of the codol_zone library.

#include "codol_zone/assets.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>

using namespace codol;
using namespace codol::zone;

namespace
{
	const char* USAGE =
		"codol_zonetool - Call of Duty Online 1.1.1.13 fast file reader\n"
		"\n"
		"  info <ff>                    header, blocks, asset counts per type\n"
		"  assets <ff> [--all]          type and name of every asset ( --all: also the nested ones )\n"
		"  check <folder>               load every .ff of a folder, report failures\n"
		"  decrypt <ff> --out <file>    write the decrypted, inflated zone\n"
		"  dump-ents <ff> [--out <f>]   entity string of the map\n"
		"  dump-rawfile <ff> [<name>] [--out <f>]   one rawfile, inflated ( no name: list them )\n"
		"  dump-rawfiles <ff> --out <folder>        every rawfile of the zone\n"
		"  model <ff> <name>            bones, lods, surfaces, materials\n"
		"  models <ff>                  one line per xmodel: bones, lods, lod 0 surfaces / vertices / triangles\n"
		"  material <ff> <name>         techset, textures, constants, state bits\n"
		"  image <ff> <name>            dimensions, inline pixels or streamed\n"
		"  map <ff>                     GfxWorld / clipMap / MapEnts / ComWorld counts\n"
		"  index <ff>                   JSON: names per type, models, anims, localize, images, string tables\n"
		"  peek <ff> <hex addr> [<n>]   hex dump of image memory\n"
		"\n"
		"options:\n"
		"  --exe <client exe>           C1.exe or codoMP_client_shipRetail.exe ( default: searched next to / above the .ff )\n"
		"  --json                       machine readable output ( info, assets, check, model, models, material, image, map )\n";
	struct Args
	{
		std::string command;
		std::vector<std::string> positional;
		std::string exe, out;
		bool json = false, all = false;
	};

	// ---- minimal JSON writer ------------------------------------------------------------------

	struct Json
	{
		std::string text;
		std::vector<bool> first;
		void sep()
		{
			if (!first.empty())
			{
				if (!first.back())
				{
					text += ',';
				}
				first.back() = false;
			}
		}
		void quote(const std::string_view s)
		{
			text += '"';
			for (const char ch : s)
			{
				const unsigned char c = static_cast<unsigned char>(ch);
				if (c == '"' || c == '\\')
				{
					text += '\\';
					text += ch;
				}
				else if (c < 0x20 || c >= 0x7F)
				{
					// bytes outside printable ASCII as \u00XX, so the output is valid JSON whatever the game's encoding is
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					text += buf;
				}
				else
				{
					text += ch;
				}
			}
			text += '"';
		}
		void key(const std::string_view k)
		{
			sep();
			quote(k);
			text += ':';
			pendingValue = true;	// the value that follows must not emit a separator
		}
		void begin(const char open)
		{
			value_prefix();
			text += open;
			first.push_back(true);
		}
		void end(const char close)
		{
			first.pop_back();
			text += close;
		}
		void str(const std::string_view v) { value_prefix(); quote(v); }
		void num(const double v)
		{
			value_prefix();
			char buf[40];
			std::snprintf(buf, sizeof(buf), "%.9g", v);
			text += buf;
		}
		void integer(const long long v) { value_prefix(); text += std::to_string(v); }
		void boolean(const bool v) { value_prefix(); text += v ? "true" : "false"; }
		void kv(const std::string_view k, const std::string_view v) { key(k); str(v); }
		void kv(const std::string_view k, const char* v) { key(k); str(v); }
		void kv(const std::string_view k, const long long v) { key(k); integer(v); }
		void kv(const std::string_view k, const int v) { key(k); integer(v); }
		void kv(const std::string_view k, const std::uint32_t v) { key(k); integer(v); }
		void kv(const std::string_view k, const std::size_t v) { key(k); integer(static_cast<long long>(v)); }
		void kv(const std::string_view k, const double v) { key(k); num(v); }
		void kv(const std::string_view k, const bool v) { key(k); boolean(v); }
	private:
		bool pendingValue = false;
		void value_prefix()
		{
			if (pendingValue)
			{
				pendingValue = false;
			}
			else
			{
				sep();
			}
		}
	};

	void print_json(const Json& j)
	{
		std::fwrite(j.text.data(), 1, j.text.size(), stdout);
		std::fputc('\n', stdout);
	}

	// ---- helpers --------------------------------------------------------------------------------

	bool find_exe(const Args& args, const std::filesystem::path& near, std::filesystem::path& exe)
	{
		if (!args.exe.empty())
		{
			exe = args.exe;
			return true;
		}
		std::error_code ec;
		std::filesystem::path dir = std::filesystem::absolute(near, ec);
		if (!std::filesystem::is_directory(dir, ec))
		{
			dir = dir.parent_path();
		}
		for (int i = 0; i < 5 && !dir.empty(); i++)
		{
			for (const char* name : { "C1.exe", "codoMP_client_shipRetail.exe" })
			{
				if (std::filesystem::exists(dir / name, ec))
				{
					exe = dir / name;
					return true;
				}
			}
			if (dir == dir.parent_path())
			{
				break;
			}
			dir = dir.parent_path();
		}
		return false;
	}

	bool load_key(const Args& args, const std::filesystem::path& near, ClientKey& key)
	{
		std::filesystem::path exe;
		if (!find_exe(args, near, exe))
		{
			std::fprintf(stderr, "error: no client executable found, pass --exe <C1.exe>\n");
			return false;
		}
		if (const Status s = key.load_from_exe(exe); !s)
		{
			std::fprintf(stderr, "error: %s\n", s.message.c_str());
			return false;
		}
		return true;
	}

	bool open_zone(const Args& args, Zone& zone, FastFileInfo* info = nullptr)
	{
		if (args.positional.empty())
		{
			std::fprintf(stderr, "error: no fast file given\n");
			return false;
		}
		ClientKey key;
		if (!load_key(args, args.positional[0], key))
		{
			return false;
		}
		if (const Status s = zone.open(args.positional[0], key, info); !s)
		{
			std::fprintf(stderr, "error: %s\n", s.message.c_str());
			return false;
		}
		return true;
	}

	bool write_output(const std::string& path, const void* data, const std::size_t size)
	{
		if (path.empty())
		{
			std::fwrite(data, 1, size, stdout);
			return true;
		}
		std::ofstream file(path, std::ios::binary);
		file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
		if (!file)
		{
			std::fprintf(stderr, "error: cannot write '%s'\n", path.c_str());
			return false;
		}
		return true;
	}

	std::string sv(const std::string_view s)
	{
		return std::string(s);
	}

	struct TypeCount
	{
		std::size_t topLevel = 0, registered = 0;
		std::uint64_t bytes = 0;
	};

	std::map<std::int32_t, TypeCount> count_types(const Zone& zone)
	{
		std::map<std::int32_t, TypeCount> counts;
		for (const XAssetEntry& e : zone.assets())
		{
			counts[e.type].topLevel++;
			counts[e.type].bytes += e.dataSize;
		}
		for (const AssetRecord& r : zone.all_assets())
		{
			counts[r.type].registered++;
		}
		return counts;
	}

	// ---- commands -------------------------------------------------------------------------------

	int cmd_info(const Args& args)
	{
		Zone zone;
		FastFileInfo info;
		if (!open_zone(args, zone, &info))
		{
			return 1;
		}
		const auto counts = count_types(zone);

		if (args.json)
		{
			Json j;
			j.begin('{');
			j.kv("file", std::filesystem::path(args.positional[0]).filename().string());
			j.kv("magic", info.header.magic);
			j.kv("signed", info.header.is_signed());
			j.kv("version", info.header.version);
			j.kv("allowOnlineUpdate", static_cast<int>(info.header.allowOnlineUpdate));
			j.kv("fileSize", static_cast<long long>(info.fileSize));
			j.kv("authName", info.authName);
			j.kv("subheaderHashValid", info.subheaderHashValid);
			j.kv("masterBlocks", info.masterBlocks);
			j.kv("dataChunks", info.dataChunks);
			j.kv("zoneSize", static_cast<long long>(zone.stream_size()));
			j.kv("xfileSize", zone.header().size);
			j.kv("externalSize", zone.header().externalSize);
			j.key("blocks");
			j.begin('[');
			for (int b = 0; b < BLOCK_COUNT; b++)
			{
				j.begin('{');
				j.kv("declared", zone.block_usage(b).declared);
				j.kv("used", zone.block_usage(b).used);
				j.end('}');
			}
			j.end(']');
			j.kv("assets", zone.assets().size());
			j.kv("registeredAssets", zone.all_assets().size());
			j.kv("scriptStrings", zone.script_strings().size());
			j.key("types");
			j.begin('{');
			for (const auto& [type, c] : counts)
			{
				j.key(asset_type_name(type));
				j.begin('{');
				j.kv("count", c.topLevel);
				j.kv("registered", c.registered);
				j.kv("bytes", static_cast<long long>(c.bytes));
				j.end('}');
			}
			j.end('}');
			j.key("warnings");
			j.begin('[');
			for (const std::string& w : zone.warnings())
			{
				j.str(w);
			}
			j.end(']');
			j.end('}');
			print_json(j);
			return 0;
		}

		std::printf("[FastFile]\n");
		std::printf("File: %s\n", std::filesystem::path(args.positional[0]).filename().string().c_str());
		std::printf("Magic: %s (%s)\n", info.header.magic, info.header.is_signed() ? "signed" : "unsigned");
		std::printf("Version: %u (0x%X)\n", info.header.version, info.header.version);
		std::printf("Allow online update: %u\n", info.header.allowOnlineUpdate);
		std::printf("Size: %llu\n", static_cast<unsigned long long>(info.fileSize));
		if (info.header.is_signed())
		{
			std::printf("Auth header name: %s (subheader hash %s)\n", info.authName.c_str(), info.subheaderHashValid ? "valid" : "INVALID");
			std::printf("Chunks: %u data, %u master, %u hash failures\n", info.dataChunks, info.masterBlocks, info.hashFailures);
		}
		std::printf("Zone size: %llu (XFile.size 0x%X, externalSize 0x%X)\n", static_cast<unsigned long long>(zone.stream_size()), zone.header().size,
			zone.header().externalSize);
		std::printf("Blocks:\n");
		for (int b = 0; b < BLOCK_COUNT; b++)
		{
			std::printf("  block %d: declared 0x%08X used 0x%08X\n", b, zone.block_usage(b).declared, zone.block_usage(b).used);
		}
		std::printf("\n[Assets]\n");
		std::printf("Total assets: %zu (%zu registered including nested)\n", zone.assets().size(), zone.all_assets().size());
		std::printf("Script strings: %zu\n", zone.script_strings().size());
		for (const auto& [type, c] : counts)
		{
			std::printf("  %-18s %6zu %14llu bytes   (%zu registered)\n", asset_type_name(type).c_str(), c.topLevel, static_cast<unsigned long long>(c.bytes),
				c.registered);
		}
		for (const std::string& w : zone.warnings())
		{
			std::printf("warning: %s\n", w.c_str());
		}
		return 0;
	}

	int cmd_assets(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}

		Json j;

		if (args.json)
		{
			j.begin('[');
		}
		if (args.all)
		{
			for (const AssetRecord& r : zone.all_assets())
			{
				const std::string name = sv(zone.asset_name(r.type, r.header));
				const bool reference = zone.c_string(zone.get<zaddr>(r.header + (r.type == c1::ASSET_TYPE_IMAGE ? 32u : r.type == c1::ASSET_TYPE_LOCALIZE_ENTRY ? 4u : 0u))).starts_with(",");
				if (args.json)
				{
					j.begin('{');
					j.kv("type", asset_type_name(r.type));
					j.kv("name", name);
					j.kv("owner", r.owner);
					j.kv("reference", reference);
					j.end('}');
				}
				else
				{
					std::printf("%s\t%s%s\n", asset_type_name(r.type).c_str(), reference ? "," : "", name.c_str());
				}
			}
		}
		else
		{
			int index = 0;
			for (const XAssetEntry& e : zone.assets())
			{
				const std::string name = e.header ? sv(zone.asset_name(e.type, e.header)) : std::string();
				if (args.json)
				{
					j.begin('{');
					j.kv("index", index);
					j.kv("type", asset_type_name(e.type));
					j.kv("name", name);
					j.kv("zoneOffset", e.dataOffset + 40);
					j.kv("header", e.header);
					j.kv("size", e.dataSize);
					j.end('}');
				}
				else
				{
					std::printf("%5d  %2d %-18s 0x%08X %10u  %s\n", index, e.type, asset_type_name(e.type).c_str(), e.dataOffset, e.dataSize, name.c_str());
				}
				index++;
			}
		}

		if (args.json)
		{
			j.end(']');
			print_json(j);
		}
		return 0;
	}

	int cmd_check(const Args& args)
	{
		if (args.positional.empty())
		{
			std::fprintf(stderr, "error: no folder given\n");
			return 1;
		}
		ClientKey key;
		if (!load_key(args, args.positional[0], key))
		{
			return 1;
		}
		std::vector<std::filesystem::path> files;
		std::error_code ec;
		for (const auto& e : std::filesystem::directory_iterator(args.positional[0], ec))
		{
			if (e.is_regular_file() && e.path().extension() == ".ff")
			{
				files.push_back(e.path());
			}
		}
		std::sort(files.begin(), files.end());

		Json j;

		if (args.json)
		{
			j.begin('{');
			j.key("zones");
			j.begin('[');
		}
		int failures = 0;
		std::size_t assets = 0, registered = 0;
		std::uint64_t zoneBytes = 0;
		const auto start = std::chrono::steady_clock::now();

		for (const auto& file : files)
		{
			const auto t0 = std::chrono::steady_clock::now();
			Zone zone;
			FastFileInfo info;
			const Status s = zone.open(file, key, &info);
			const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
			if (!s)
			{
				failures++;
			}
			else
			{
				assets += zone.assets().size();
				registered += zone.all_assets().size();
				zoneBytes += zone.stream_size();
			}
			if (args.json)
			{
				j.begin('{');
				j.kv("file", file.filename().string());
				j.kv("ok", s.ok);
				if (s)
				{
					j.kv("magic", info.header.magic);
					j.kv("zoneSize", static_cast<long long>(zone.stream_size()));
					j.kv("assets", zone.assets().size());
					j.kv("registeredAssets", zone.all_assets().size());
					j.kv("ms", static_cast<long long>(ms));
				}
				else
				{
					j.kv("error", s.message);
				}
				j.end('}');
			}
			else if (s)
			{
				std::printf("ok    %-46s %s %11llu bytes %6zu assets %7zu registered %6lld ms\n", zone.name().c_str(), info.header.magic,
					static_cast<unsigned long long>(zone.stream_size()), zone.assets().size(), zone.all_assets().size(), static_cast<long long>(ms));
			}
			else
			{
				std::printf("FAIL  %s\n", s.message.c_str());
			}
			std::fflush(stdout);
		}
		const auto total = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

		if (args.json)
		{
			j.end(']');
			j.kv("zones_total", files.size());
			j.kv("failures", failures);
			j.kv("assets", assets);
			j.kv("registeredAssets", registered);
			j.end('}');
			print_json(j);
		}
		else
		{
			std::printf("%zu zones, %d failures, %zu assets ( %zu registered ), %llu zone bytes, %.1f s\n", files.size(), failures, assets, registered,
				static_cast<unsigned long long>(zoneBytes), static_cast<double>(total) / 1000.0);
		}
		return failures ? 1 : 0;
	}

	int cmd_decrypt(const Args& args)
	{
		if (args.positional.empty() || args.out.empty())
		{
			std::fprintf(stderr, "error: decrypt <ff> --out <file>\n");
			return 1;
		}
		ClientKey key;
		if (!load_key(args, args.positional[0], key))
		{
			return 1;
		}
		std::vector<std::uint8_t> zone;
		if (const Status s = read_fastfile(args.positional[0], key, zone); !s)
		{
			std::fprintf(stderr, "error: %s\n", s.message.c_str());
			return 1;
		}
		if (!write_output(args.out, zone.data(), zone.size()))
		{
			return 1;
		}
		std::printf("wrote %zu bytes to %s\n", zone.size(), args.out.c_str());
		return 0;
	}

	int cmd_dump_ents(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		MapEntsView ents;
		if (!read_map_ents(zone, find_map_ents(zone), ents))
		{
			std::fprintf(stderr, "error: the zone has no MapEnts asset\n");
			return 1;
		}
		std::fprintf(stderr, "%s: %zu bytes, %u entities, %u trigger models, %u hulls, %u slabs\n", sv(ents.name).c_str(), ents.entityString.size(),
			count_entities(ents.entityString), ents.triggerModelCount, ents.triggerHullCount, ents.triggerSlabCount);
		return write_output(args.out, ents.entityString.data(), ents.entityString.size()) ? 0 : 1;
	}

	int cmd_dump_rawfile(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		if (args.positional.size() < 2)
		{
			// no name: list them
			for (const AssetRecord& r : zone.all_assets())
			{
				RawFileView raw;
				if (r.type == c1::ASSET_TYPE_RAWFILE && read_rawfile(zone, r.header, raw))
				{
					std::printf("%10d %10d  %s\n", raw.raw.len, raw.raw.compressedLen, sv(raw.name).c_str());
				}
			}
			return 0;
		}
		RawFileView raw;
		if (!read_rawfile(zone, zone.find_asset(c1::ASSET_TYPE_RAWFILE, args.positional[1]), raw))
		{
			std::fprintf(stderr, "error: no rawfile '%s' in this zone\n", args.positional[1].c_str());
			return 1;
		}
		std::vector<std::uint8_t> contents;
		if (const Status s = rawfile_contents(raw, contents); !s)
		{
			std::fprintf(stderr, "error: %s\n", s.message.c_str());
			return 1;
		}
		return write_output(args.out, contents.data(), contents.size()) ? 0 : 1;
	}

	int cmd_dump_rawfiles(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		if (args.out.empty())
		{
			std::fprintf(stderr, "error: dump-rawfiles <ff> --out <folder>\n");
			return 1;
		}
		int written = 0, failed = 0;
		for (const AssetRecord& r : zone.all_assets())
		{
			RawFileView raw;
			if (r.type != c1::ASSET_TYPE_RAWFILE || !read_rawfile(zone, r.header, raw) || zone.c_string(raw.raw.name).starts_with(","))
			{
				continue;
			}
			// the name is a relative game path; never let it leave the output folder
			const std::filesystem::path relative = std::filesystem::path(sv(raw.name)).lexically_normal();
			if (relative.empty() || relative.is_absolute() || relative.has_root_name() || *relative.begin() == "..")
			{
				std::fprintf(stderr, "skipped '%s': not a relative path\n", sv(raw.name).c_str());
				failed++;
				continue;
			}
			std::vector<std::uint8_t> contents;
			if (const Status s = rawfile_contents(raw, contents); !s)
			{
				std::fprintf(stderr, "%s: %s\n", sv(raw.name).c_str(), s.message.c_str());
				failed++;
				continue;
			}
			const std::filesystem::path target = std::filesystem::path(args.out) / relative;
			std::error_code ec;
			std::filesystem::create_directories(target.parent_path(), ec);
			if (!write_output(target.string(), contents.data(), contents.size()))
			{
				failed++;
				continue;
			}
			written++;
		}
		std::printf("%d rawfiles written, %d failed\n", written, failed);
		return failed ? 1 : 0;
	}

	// hex dump of image memory ( debugging aid )
	int cmd_peek(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		if (args.positional.size() < 2)
		{
			std::fprintf(stderr, "error: peek <ff> <hex address> [<bytes>]\n");
			return 1;
		}
		const zaddr address = static_cast<zaddr>(std::strtoul(args.positional[1].c_str(), nullptr, 16));
		std::size_t size = args.positional.size() > 2 ? std::strtoul(args.positional[2].c_str(), nullptr, 0) : 64;
		auto data = zone.bytes(address, size);
		while (data.empty() && size > 1)
		{
			data = zone.bytes(address, --size);
		}
		if (data.empty())
		{
			std::fprintf(stderr, "error: 0x%08X is not mapped\n", address);
			return 1;
		}
		for (std::size_t i = 0; i < data.size(); i += 16)
		{
			std::printf("%08X ", address + static_cast<zaddr>(i));
			for (std::size_t k = 0; k < 16; k++)
			{
				if (i + k < data.size())
				{
					std::printf(" %02X", data[i + k]);
				}
				else
				{
					std::printf("   ");
				}
			}
			std::printf("  ");
			for (std::size_t k = 0; k < 16 && i + k < data.size(); k++)
			{
				const unsigned char ch = data[i + k];
				std::putchar(ch >= 0x20 && ch < 0x7F ? ch : '.');
			}
			std::putchar('\n');
		}
		return 0;
	}

	struct LodCounts
	{
		std::size_t surfaces = 0, vertices = 0, triangles = 0;
	};

	LodCounts lod_counts(const ModelLod& lod)
	{
		LodCounts c;
		c.surfaces = lod.surfs.surfaces.size();
		for (const SurfaceView& s : lod.surfs.surfaces)
		{
			c.vertices += s.vertCount;
			c.triangles += s.triCount;
		}
		return c;
	}

	int cmd_model(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		if (args.positional.size() < 2)
		{
			std::fprintf(stderr, "error: model <ff> <name>\n");
			return 1;
		}
		ModelView model;
		if (!read_model(zone, zone.find_asset(c1::ASSET_TYPE_XMODEL, args.positional[1]), model))
		{
			std::fprintf(stderr, "error: no xmodel '%s' in this zone\n", args.positional[1].c_str());
			return 1;
		}

		if (args.json)
		{
			Json j;
			j.begin('{');
			j.kv("name", model.name);
			j.kv("bones", model.bones.size());
			j.kv("rootBones", static_cast<int>(model.raw.numRootBones));
			j.kv("flags", static_cast<int>(model.raw.flags));
			j.kv("contents", model.raw.contents);
			j.key("boneList");
			j.begin('[');
			for (const ModelBone& b : model.bones)
			{
				j.begin('{');
				j.kv("name", b.name);
				j.kv("parent", b.parent);
				j.key("trans");
				j.begin('[');
				for (const float f : b.baseTrans) j.num(f);
				j.end(']');
				j.key("quat");
				j.begin('[');
				for (const float f : b.baseQuat) j.num(f);
				j.end(']');
				j.end('}');
			}
			j.end(']');
			j.key("materials");
			j.begin('[');
			for (const auto& name : model.materialNames) j.str(name);
			j.end(']');
			j.key("lods");
			j.begin('[');
			for (const ModelLod& lod : model.lods)
			{
				j.begin('{');
				j.kv("dist", static_cast<double>(lod.dist));
				j.kv("surfs", lod.surfsName);
				j.kv("external", lod.surfs.external);
				j.kv("numSurfs", static_cast<int>(lod.numSurfs));
				j.kv("surfIndex", static_cast<int>(lod.surfIndex));
				j.key("surfaces");
				j.begin('[');
				std::size_t s = 0;
				for (const SurfaceView& surface : lod.surfs.surfaces)
				{
					const std::size_t slot = lod.surfIndex + s++;
					j.begin('{');
					j.kv("vertices", surface.vertCount);
					j.kv("triangles", surface.triCount);
					j.kv("rigidLists", surface.raw.vertListCount);
					j.kv("blended", surface.raw.vertsBlend != 0);
					j.kv("material", slot < model.materialNames.size() ? model.materialNames[slot] : std::string_view());
					j.end('}');
				}
				j.end(']');
				j.end('}');
			}
			j.end(']');
			j.end('}');
			print_json(j);
			return 0;
		}

		std::printf("xmodel %s\n", sv(model.name).c_str());
		std::printf("  bones %zu ( %u root ), lods %zu, materials %zu, flags 0x%02X, contents 0x%X, scale %g\n", model.bones.size(), model.raw.numRootBones,
			model.lods.size(), model.materials.size(), model.raw.flags, static_cast<unsigned>(model.raw.contents), static_cast<double>(model.raw.scale));
		for (std::size_t i = 0; i < model.bones.size(); i++)
		{
			const ModelBone& b = model.bones[i];
			std::printf("  bone %3zu  parent %3d  ( %9.3f %9.3f %9.3f )  %s\n", i, b.parent, static_cast<double>(b.baseTrans[0]), static_cast<double>(b.baseTrans[1]),
				static_cast<double>(b.baseTrans[2]), sv(b.name).c_str());
		}
		for (std::size_t l = 0; l < model.lods.size(); l++)
		{
			const ModelLod& lod = model.lods[l];
			const LodCounts c = lod_counts(lod);
			std::printf("  lod %zu  dist %g  surfs '%s'%s  %zu surfaces, %zu vertices, %zu triangles\n", l, static_cast<double>(lod.dist), sv(lod.surfsName).c_str(),
				lod.surfs.external ? " ( not in this zone: other fast file or streamed from an .iwd )" : "", c.surfaces, c.vertices, c.triangles);
			std::size_t s = 0;
			for (const SurfaceView& surface : lod.surfs.surfaces)
			{
				const std::size_t slot = lod.surfIndex + s;
				const char* skin = surface.raw.vertsBlend ? "blended" : surface.raw.vertListCount ? "rigid" : "static";
				std::printf("    surface %2zu  %5u verts %5u tris  %-7s %2u rigid lists  %s\n", s, surface.vertCount, surface.triCount, skin, surface.raw.vertListCount,
					slot < model.materialNames.size() ? sv(model.materialNames[slot]).c_str() : "?");
				s++;
			}
		}
		if (!model.lods.empty() && !model.lods[0].surfs.surfaces.empty())
		{
			const SurfaceView& surface = model.lods[0].surfs.surfaces[0];
			const auto verts = surface.vertices();
			const auto indices = surface.indices();
			if (!verts.empty() && indices.size() >= 3)
			{
				const SurfaceVertex& v = verts[0];
				std::printf("  first vertex: pos ( %g %g %g ) normal ( %.3f %.3f %.3f ) uv ( %.4f %.4f ) rgba %u %u %u %u; first triangle %u %u %u\n",
					static_cast<double>(v.position[0]), static_cast<double>(v.position[1]), static_cast<double>(v.position[2]), static_cast<double>(v.normal[0]),
					static_cast<double>(v.normal[1]), static_cast<double>(v.normal[2]), static_cast<double>(v.uv[0]), static_cast<double>(v.uv[1]), v.color[0],
					v.color[1], v.color[2], v.color[3], indices[0], indices[1], indices[2]);
			}
		}
		return 0;
	}

	int cmd_models(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}

		Json j;

		if (args.json)
		{
			j.begin('[');
		}
		else
		{
			std::printf("name,bones,root_bones,lods,lod0_external,lod0_surfaces,lod0_vertices,lod0_triangles,materials\n");
		}
		for (const AssetRecord& r : zone.all_assets())
		{
			ModelView model;
			if (r.type != c1::ASSET_TYPE_XMODEL || !read_model(zone, r.header, model) || model.isReference)
			{
				continue;
			}
			LodCounts c;
			bool external = false;
			if (!model.lods.empty())
			{
				c = lod_counts(model.lods[0]);
				external = model.lods[0].surfs.external;
			}
			std::set<zaddr> distinct(model.materials.begin(), model.materials.end());
			distinct.erase(0);
			if (args.json)
			{
				j.begin('{');
				j.kv("name", model.name);
				j.kv("bones", model.bones.size());
				j.kv("rootBones", static_cast<int>(model.raw.numRootBones));
				j.kv("lods", model.lods.size());
				j.kv("lod0External", external);
				j.kv("lod0Surfaces", c.surfaces);
				j.kv("lod0Vertices", c.vertices);
				j.kv("lod0Triangles", c.triangles);
				j.kv("materials", distinct.size());
				j.end('}');
			}
			else
			{
				std::printf("%s,%zu,%u,%zu,%d,%zu,%zu,%zu,%zu\n", sv(model.name).c_str(), model.bones.size(), model.raw.numRootBones, model.lods.size(), external ? 1 : 0,
					c.surfaces, c.vertices, c.triangles, distinct.size());
			}
		}

		if (args.json)
		{
			j.end(']');
			print_json(j);
		}
		return 0;
	}

	std::string hex(const std::string_view bytes)
	{
		static const char* digits = "0123456789abcdef";
		std::string out;
		out.reserve(bytes.size() * 2);
		for (const char ch : bytes)
		{
			out += digits[static_cast<unsigned char>(ch) >> 4];
			out += digits[static_cast<unsigned char>(ch) & 15];
		}
		return out;
	}

	// everything the verification script compares, as one JSON document
	int cmd_index(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		Json j;
		j.begin('{');
		j.kv("zone", zone.name());
		j.kv("assets", zone.assets().size());

		// names of the assets that have their data in this zone, per type, in load order
		std::map<std::int32_t, std::vector<std::string>> names;
		for (const AssetRecord& r : zone.all_assets())
		{
			const std::uint32_t nameOffset = r.type == c1::ASSET_TYPE_IMAGE ? 32u : r.type == c1::ASSET_TYPE_LOCALIZE_ENTRY ? 4u : 0u;
			if (!zone.c_string(zone.get<zaddr>(r.header + nameOffset)).starts_with(","))
			{
				names[r.type].push_back(sv(zone.asset_name(r.type, r.header)));
			}
		}
		j.key("names");
		j.begin('{');
		for (const auto& [type, list] : names)
		{
			j.key(asset_type_name(type));
			j.begin('[');
			for (const std::string& name : list)
			{
				j.str(hex(name));
			}
			j.end(']');
		}
		j.end('}');

		j.key("models");
		j.begin('[');
		for (const AssetRecord& r : zone.all_assets())
		{
			ModelView model;
			if (r.type != c1::ASSET_TYPE_XMODEL || !read_model(zone, r.header, model) || model.isReference)
			{
				continue;
			}
			LodCounts c;
			bool external = false;
			if (!model.lods.empty())
			{
				c = lod_counts(model.lods[0]);
				external = model.lods[0].surfs.external;
			}
			j.begin('{');
			j.kv("name", model.name);
			j.kv("address", r.header);
			j.kv("bones", model.bones.size());
			j.kv("rootBones", static_cast<int>(model.raw.numRootBones));
			j.kv("lods", model.lods.size());
			j.kv("lod0External", external);
			j.kv("lod0Surfaces", c.surfaces);
			j.kv("lod0Vertices", c.vertices);
			j.kv("lod0Triangles", c.triangles);
			j.key("boneNames");
			j.begin('[');
			for (const ModelBone& bone : model.bones)
			{
				j.str(bone.name);
			}
			j.end(']');
			j.key("materials");
			j.begin('[');
			std::set<zaddr> seen;
			for (std::size_t i = 0; i < model.materials.size(); i++)
			{
				if (model.materials[i] && seen.insert(model.materials[i]).second)
				{
					j.str(model.materialNames[i]);
				}
			}
			j.end(']');
			j.end('}');
		}
		j.end(']');

		j.key("materials");
		j.begin('[');
		for (const AssetRecord& r : zone.all_assets())
		{
			MaterialView material;
			if (r.type != c1::ASSET_TYPE_MATERIAL || !read_material(zone, r.header, material) || material.isReference)
			{
				continue;
			}
			j.begin('{');
			j.kv("name", hex(material.name));
			j.kv("techset", hex(material.techsetName));
			j.key("textures");
			j.begin('[');
			for (const MaterialTexture& texture : material.textures)
			{
				if (texture.image)
				{
					j.str(hex(texture.imageName));
				}
			}
			j.end(']');
			j.end('}');
		}
		j.end(']');

		j.key("anims");
		j.begin('[');
		for (const AssetRecord& r : zone.all_assets())
		{
			AnimView anim;
			if (r.type != c1::ASSET_TYPE_XANIMPARTS || !read_anim(zone, r.header, anim) || anim.isReference)
			{
				continue;
			}
			j.begin('{');
			j.kv("name", anim.name);
			j.kv("lastFrame", static_cast<int>(anim.raw.numframes));
			j.kv("framerate", static_cast<double>(anim.raw.framerate));
			j.kv("bones", anim.boneNames.size());
			j.kv("notetracks", anim.notetracks.size());
			j.kv("flags", static_cast<int>(anim.raw.flags));
			j.end('}');
		}
		j.end(']');

		j.key("locales");
		j.begin('[');
		for (const AssetRecord& r : zone.all_assets())
		{
			LocalizeView entry;
			if (r.type == c1::ASSET_TYPE_LOCALIZE_ENTRY && read_localize(zone, r.header, entry))
			{
				j.begin('{');
				j.kv("key", hex(entry.name));
				j.kv("value", hex(entry.value));
				j.end('}');
			}
		}
		j.end(']');

		j.key("images");
		j.begin('[');
		for (const AssetRecord& r : zone.all_assets())
		{
			ImageView image;
			if (r.type != c1::ASSET_TYPE_IMAGE || !read_image(zone, r.header, image) || image.isReference)
			{
				continue;
			}
			j.begin('{');
			j.kv("name", hex(image.name));
			j.kv("address", r.header);
			j.kv("width", static_cast<int>(image.raw.width));
			j.kv("height", static_cast<int>(image.raw.height));
			j.kv("mapType", static_cast<int>(image.raw.mapType));
			j.kv("streamed", image.streamed);
			j.kv("format", image.loadDef.format);
			j.kv("levels", static_cast<int>(image.loadDef.levelCount));
			j.kv("size", image.pixels.size());
			j.end('}');
		}
		j.end(']');

		j.key("stringtables");
		j.begin('[');
		for (const AssetRecord& r : zone.all_assets())
		{
			StringTableView table;
			if (r.type == c1::ASSET_TYPE_STRINGTABLE && read_stringtable(zone, r.header, table))
			{
				j.begin('{');
				j.kv("name", table.name);
				j.kv("rows", table.raw.rowCount);
				j.kv("columns", table.raw.columnCount);
				j.kv("cells", table.cells.size());
				j.end('}');
			}
		}
		j.end(']');
		j.end('}');
		print_json(j);
		return 0;
	}

	int cmd_material(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		if (args.positional.size() < 2)
		{
			std::fprintf(stderr, "error: material <ff> <name>\n");
			return 1;
		}
		MaterialView material;
		if (!read_material(zone, zone.find_asset(c1::ASSET_TYPE_MATERIAL, args.positional[1]), material))
		{
			std::fprintf(stderr, "error: no material '%s' in this zone\n", args.positional[1].c_str());
			return 1;
		}

		if (args.json)
		{
			Json j;
			j.begin('{');
			j.kv("name", material.name);
			j.kv("techset", material.techsetName);
			j.kv("sortKey", static_cast<int>(material.raw.sortKey));
			j.kv("gameFlags", static_cast<int>(material.raw.gameFlags));
			j.kv("surfaceTypeBits", material.raw.surfaceTypeBits);
			j.kv("cameraRegion", static_cast<int>(material.raw.cameraRegion));
			j.key("textures");
			j.begin('[');
			for (const MaterialTexture& t : material.textures)
			{
				ImageView image;
				read_image(zone, t.image, image);
				j.begin('{');
				j.kv("nameHash", t.raw.nameHash);
				j.kv("semantic", static_cast<int>(t.raw.semantic));
				j.kv("samplerState", static_cast<int>(t.raw.samplerState));
				j.kv("image", t.imageName);
				j.kv("width", static_cast<int>(image.raw.width));
				j.kv("height", static_cast<int>(image.raw.height));
				j.kv("streamed", image.streamed);
				j.end('}');
			}
			j.end(']');
			j.key("constants");
			j.begin('[');
			for (const c1::MaterialConstantDef& c : material.constants)
			{
				j.begin('{');
				j.kv("name", std::string_view(c.name, strnlen(c.name, sizeof(c.name))));
				j.kv("nameHash", c.nameHash);
				j.key("value");
				j.begin('[');
				for (const float f : c.literal) j.num(f);
				j.end(']');
				j.end('}');
			}
			j.end(']');
			j.key("stateBits");
			j.begin('[');
			for (const auto& bits : material.stateBits)
			{
				j.begin('[');
				j.integer(bits[0]);
				j.integer(bits[1]);
				j.end(']');
			}
			j.end(']');
			j.end('}');
			print_json(j);
			return 0;
		}

		std::printf("material %s\n", sv(material.name).c_str());
		std::printf("  techset %s\n", sv(material.techsetName).c_str());
		std::printf("  sortKey %u, gameFlags 0x%02X, surfaceTypeBits 0x%08X, cameraRegion %u\n", material.raw.sortKey, material.raw.gameFlags,
			material.raw.surfaceTypeBits, material.raw.cameraRegion);
		for (const MaterialTexture& t : material.textures)
		{
			ImageView image;
			read_image(zone, t.image, image);
			std::printf("  texture hash 0x%08X semantic %2u sampler 0x%02X  %4ux%-4u %-8s %s\n", t.raw.nameHash, t.raw.semantic, t.raw.samplerState, image.raw.width,
				image.raw.height, !t.image ? "-" : image.isReference ? "extern" : image.streamed ? "streamed" : "inline", sv(t.imageName).c_str());
		}
		for (const c1::MaterialConstantDef& c : material.constants)
		{
			std::printf("  constant %-12.12s hash 0x%08X  ( %g %g %g %g )\n", c.name, c.nameHash, static_cast<double>(c.literal[0]), static_cast<double>(c.literal[1]),
				static_cast<double>(c.literal[2]), static_cast<double>(c.literal[3]));
		}
		for (std::size_t i = 0; i < material.stateBits.size(); i++)
		{
			std::printf("  stateBits[%zu] 0x%08X 0x%08X\n", i, material.stateBits[i][0], material.stateBits[i][1]);
		}
		return 0;
	}

	int cmd_image(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		if (args.positional.size() < 2)
		{
			std::fprintf(stderr, "error: image <ff> <name>\n");
			return 1;
		}
		ImageView image;
		if (!read_image(zone, zone.find_asset(c1::ASSET_TYPE_IMAGE, args.positional[1]), image))
		{
			std::fprintf(stderr, "error: no image '%s' in this zone\n", args.positional[1].c_str());
			return 1;
		}

		if (args.json)
		{
			Json j;
			j.begin('{');
			j.kv("name", image.name);
			j.kv("width", static_cast<int>(image.raw.width));
			j.kv("height", static_cast<int>(image.raw.height));
			j.kv("depth", static_cast<int>(image.raw.depth));
			j.kv("mapType", static_cast<int>(image.raw.mapType));
			j.kv("semantic", static_cast<int>(image.raw.semantic));
			j.kv("category", static_cast<int>(image.raw.category));
			j.kv("streamed", image.streamed);
			j.kv("format", image.loadDef.format);
			j.kv("levels", static_cast<int>(image.loadDef.levelCount));
			j.kv("pixelBytes", image.pixels.size());
			j.end('}');
			print_json(j);
			return 0;
		}
		std::printf("image %s\n  %ux%ux%u mapType %u semantic %u category %u\n", sv(image.name).c_str(), image.raw.width, image.raw.height, image.raw.depth,
			image.raw.mapType, image.raw.semantic, image.raw.category);
		if (image.streamed)
		{
			std::printf("  streamed: pixels come from images/%s.iwi\n", sv(image.name).c_str());
		}
		else
		{
			std::printf("  inline: D3DFORMAT %u (0x%X), %u levels, flags 0x%X, %zu bytes\n", image.loadDef.format, image.loadDef.format, image.loadDef.levelCount,
				image.loadDef.flags, image.pixels.size());
		}
		return 0;
	}

	int cmd_map(const Args& args)
	{
		Zone zone;
		if (!open_zone(args, zone))
		{
			return 1;
		}
		GfxWorldView world;
		ClipMapView clip;
		MapEntsView ents;
		ComWorldView com;
		const bool hasWorld = read_gfx_world(zone, first_asset(zone, c1::ASSET_TYPE_GFXWORLD), world);
		zaddr clipAddress = first_asset(zone, c1::ASSET_TYPE_CLIPMAP_MP);
		if (!clipAddress)
		{
			clipAddress = first_asset(zone, c1::ASSET_TYPE_CLIPMAP_SP);
		}
		const bool hasClip = read_clip_map(zone, clipAddress, clip);
		const bool hasEnts = read_map_ents(zone, find_map_ents(zone), ents);
		const bool hasCom = read_com_world(zone, first_asset(zone, c1::ASSET_TYPE_COMWORLD), com);
		if (!hasWorld && !hasClip && !hasEnts)
		{
			std::fprintf(stderr, "error: the zone has no map assets\n");
			return 1;
		}
		std::set<std::string_view> worldMaterials, distinctModels;
		std::size_t worldTris = 0;
		for (const WorldSurface& s : world.surfaces)
		{
			worldMaterials.insert(s.materialName);
			worldTris += s.raw.triCount;
		}
		for (const WorldStaticModel& m : world.staticModels)
		{
			distinctModels.insert(m.modelName);
		}
		std::size_t brushSides = 0, portals = 0;
		for (const Brush& b : clip.brushes)
		{
			brushSides += b.sides.size();
		}
		for (const WorldCell& c : world.cells)
		{
			portals += c.portals.size();
		}

		if (args.json)
		{
			Json j;
			j.begin('{');
			j.kv("zone", zone.name());
			if (hasWorld)
			{
				j.key("gfxWorld");
				j.begin('{');
				j.kv("name", world.name);
				j.kv("baseName", world.baseName);
				j.kv("vertices", world.raw.draw.vertexCount);
				j.kv("indices", world.raw.draw.indexCount);
				j.kv("surfaces", world.surfaces.size());
				j.kv("staticSurfaces", world.raw.dpvs.staticSurfaceCount);
				j.kv("surfaceTriangles", worldTris);
				j.kv("materials", worldMaterials.size());
				j.kv("staticModels", world.staticModels.size());
				j.kv("distinctStaticModels", distinctModels.size());
				j.kv("brushModels", world.brushModels.size());
				j.kv("cells", world.cells.size());
				j.kv("portals", portals);
				j.kv("lightmaps", world.raw.draw.lightmapCount);
				j.kv("reflectionProbes", world.raw.draw.reflectionProbeCount);
				j.kv("primaryLights", world.raw.primaryLightCount);
				j.kv("checksum", world.raw.checksum);
				j.end('}');
			}
			if (hasClip)
			{
				j.key("clipMap");
				j.begin('{');
				j.kv("name", clip.name);
				j.kv("planes", clip.planes.size());
				j.kv("brushes", clip.brushes.size());
				j.kv("brushSides", brushSides);
				j.kv("materials", clip.materials.size());
				j.kv("subModels", clip.subModels.size());
				j.kv("staticModels", clip.staticModels.size());
				j.kv("collisionVertices", clip.raw.vertCount);
				j.kv("collisionTriangles", clip.raw.triCount);
				j.kv("partitions", clip.raw.partitionCount);
				j.kv("aabbTrees", clip.raw.aabbTreeCount);
				j.kv("leafs", clip.raw.numLeafs);
				j.kv("checksum", clip.raw.checksum);
				j.end('}');
			}
			if (hasEnts)
			{
				j.key("mapEnts");
				j.begin('{');
				j.kv("name", ents.name);
				j.kv("entityChars", ents.entityString.size());
				j.kv("entities", count_entities(ents.entityString));
				j.kv("triggerModels", ents.triggerModelCount);
				j.kv("triggerHulls", ents.triggerHullCount);
				j.kv("triggerSlabs", ents.triggerSlabCount);
				j.end('}');
			}
			if (hasCom)
			{
				j.key("comWorld");
				j.begin('{');
				j.kv("name", com.name);
				j.kv("primaryLights", com.raw.primaryLightCount);
				j.end('}');
			}
			j.end('}');
			print_json(j);
			return 0;
		}

		std::printf("zone %s\n", zone.name().c_str());
		if (hasWorld)
		{
			std::printf("GfxWorld %s ( %s )\n", sv(world.name).c_str(), sv(world.baseName).c_str());
			std::printf("  %u vertices, %u indices ( %u triangles in the index buffer ), %zu surfaces ( %u static, %zu triangles ), %zu materials\n",
				world.raw.draw.vertexCount, world.raw.draw.indexCount, world.raw.draw.indexCount / 3, world.surfaces.size(), world.raw.dpvs.staticSurfaceCount,
				worldTris, worldMaterials.size());
			std::printf("  %zu static models ( %zu distinct ), %zu brush models, %zu cells, %zu portals, %d lightmaps, %u reflection probes, %u primary lights\n",
				world.staticModels.size(), distinctModels.size(), world.brushModels.size(), world.cells.size(), portals, world.raw.draw.lightmapCount,
				world.raw.draw.reflectionProbeCount, world.raw.primaryLightCount);
		}
		if (hasClip)
		{
			std::printf("clipMap %s\n", sv(clip.name).c_str());
			std::printf("  %zu brushes ( %zu sides ), %zu planes, %zu materials, %zu sub models, %zu static models\n", clip.brushes.size(), brushSides, clip.planes.size(),
				clip.materials.size(), clip.subModels.size(), clip.staticModels.size());
			std::printf("  collision: %u vertices, %d triangles, %d partitions, %d aabb trees, %u leafs\n", clip.raw.vertCount, clip.raw.triCount, clip.raw.partitionCount,
				clip.raw.aabbTreeCount, clip.raw.numLeafs);
		}
		if (hasEnts)
		{
			std::printf("MapEnts %s\n  %zu characters, %u entities, %u trigger models, %u hulls, %u slabs\n", sv(ents.name).c_str(), ents.entityString.size(),
				count_entities(ents.entityString), ents.triggerModelCount, ents.triggerHullCount, ents.triggerSlabCount);
		}
		if (hasCom)
		{
			std::printf("ComWorld %s\n  %u primary lights\n", sv(com.name).c_str(), com.raw.primaryLightCount);
		}
		return 0;
	}
}

int main(const int argc, char** argv)
{
	Args args;
	for (int i = 1; i < argc; i++)
	{
		const std::string a = argv[i];
		if (a == "--exe" && i + 1 < argc)
		{
			args.exe = argv[++i];
		}
		else if (a == "--out" && i + 1 < argc)
		{
			args.out = argv[++i];
		}
		else if (a == "--json")
		{
			args.json = true;
		}
		else if (a == "--all")
		{
			args.all = true;
		}
		else if (a == "--help" || a == "-h")
		{
			std::fputs(USAGE, stdout);
			return 0;
		}
		else if (args.command.empty())
		{
			args.command = a;
		}
		else
		{
			args.positional.push_back(a);
		}
	}

	try
	{
		if (args.command == "info") return cmd_info(args);
		if (args.command == "assets") return cmd_assets(args);
		if (args.command == "check") return cmd_check(args);
		if (args.command == "decrypt") return cmd_decrypt(args);
		if (args.command == "dump-ents") return cmd_dump_ents(args);
		if (args.command == "dump-rawfile") return cmd_dump_rawfile(args);
		if (args.command == "dump-rawfiles") return cmd_dump_rawfiles(args);
		if (args.command == "peek") return cmd_peek(args);
		if (args.command == "index") return cmd_index(args);
		if (args.command == "model") return cmd_model(args);
		if (args.command == "models") return cmd_models(args);
		if (args.command == "material") return cmd_material(args);
		if (args.command == "image") return cmd_image(args);
		if (args.command == "map") return cmd_map(args);
	}

	catch (const std::exception& e)
	{
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}

	std::fputs(USAGE, args.command.empty() ? stdout : stderr);
	return args.command.empty() ? 0 : 2;
}
