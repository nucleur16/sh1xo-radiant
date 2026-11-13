// codol_linker: builds Call of Duty Online 1.1.1.13 fast files ( the role linker_pc has in the CoD4 mod tools ).
//
// It starts from an existing zone ( or from nothing ), applies edits to the asset graph, lays the zone out again with
// the game's own loaders and wraps it as an unsigned fast file ( IWffu100 ). See --help.

#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>

using namespace codol;
using namespace codol::zone;
namespace fs = std::filesystem;

namespace
{
	const char USAGE[] =
		"codol_linker - Call of Duty Online 1.1.1.13 fast file writer\n"
		"\n"
		"  codol_linker --base <zone.ff> --exe <C1.exe> [edits] --out <new.ff>\n"
		"  codol_linker --new <zone name> --exe <C1.exe> [edits] --out <new.ff>\n"
		"      edits, applied in the order given:\n"
		"        --set-ents <file>                   replace the MapEnts entity string\n"
		"        --add-rawfile <name>=<file>         replace the rawfile <name>, or add it\n"
		"        --set-stringtable <name>=<file.csv> replace or add a string table\n"
		"        --remove <type>:<name>              remove a top level asset\n"
		"        --take <type>:<name>@<donor.ff>     copy an asset from another zone ( replaces one of the same name )\n"
		"      options:\n"
		"        --with-deps        --take also copies assets the taken asset refers to\n"
		"        --store-rawfiles   store rawfiles uncompressed\n"
		"        --level <0..9>     zlib level of the fast file ( default 6, 0 = stored )\n"
		"        --zone-out <file>  also write the inflated zone\n"
		"        --no-check         skip reading the written file back\n"
		"\n"
		"  codol_linker --list <file.ff> --exe <C1.exe>\n"
		"      the asset list with stream sizes, chunk and relocation counts\n"
		"\n"
		"  codol_linker --verify <file.ff> --exe <C1.exe>\n"
		"      decodes and loads a fast file, then writes its zone again and compares ( read -> write identity )\n"
		"\n"
		"  codol_linker --roundtrip-all <install dir> [--exe <C1.exe>] [--out-dir <dir>] [--filter <text>]\n"
		"                [--no-container] [--level <0..9>] [--keep]\n"
		"      for every .ff below <install>/client_shipRetail: read -> capture -> write must give the same inflated\n"
		"      zone; the zone is then written as IWffu100 into --out-dir, read back and compared. Prints one row per\n"
		"      zone and a summary. The install is only read. Default --exe is <install>/C1.exe.\n";

	struct Edit
	{
		std::string kind, value;
	};

	struct Args
	{
		std::string base, newName, exe, out, zoneOut, list, verify, roundtrip, outDir, filter;
		std::vector<Edit> edits;
		bool withDeps = false, storeRawfiles = false, noCheck = false, noContainer = false, keep = false;
		int level = 6;
	};

	double seconds_since(const std::chrono::steady_clock::time_point start)
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	}

	bool fail(const std::string& text)
	{
		std::fprintf(stderr, "error: %s\n", text.c_str());
		return false;
	}

	// "a,b,"c ""d"""  -> rows of cells
	std::vector<std::vector<std::string>> parse_csv(const std::vector<std::uint8_t>& data)
	{
		std::vector<std::vector<std::string>> rows;
		std::vector<std::string> row;
		std::string cell;
		bool quoted = false, any = false;
		const auto end_row = [&]
		{
			if (any || !cell.empty() || !row.empty())
			{
				row.push_back(cell);
				rows.push_back(row);
			}
			row.clear();
			cell.clear();
			any = false;
		};
		for (std::size_t i = 0; i < data.size(); i++)
		{
			const char c = static_cast<char>(data[i]);
			if (quoted)
			{
				if (c == '"' && i + 1 < data.size() && data[i + 1] == '"')
				{
					cell.push_back('"');
					i++;
				}
				else if (c == '"')
				{
					quoted = false;
				}
				else
				{
					cell.push_back(c);
				}
			}
			else if (c == '"')
			{
				quoted = true;
				any = true;
			}
			else if (c == ',')
			{
				row.push_back(cell);
				cell.clear();
				any = true;
			}
			else if (c == '\n')
			{
				end_row();
			}
			else if (c != '\r')
			{
				cell.push_back(c);
				any = true;
			}
		}
		end_row();
		return rows;
	}

	struct Fingerprint
	{
		std::int32_t type;
		std::string name;
		std::uint64_t hash;
	};

	std::vector<Fingerprint> fingerprints(const ZoneEditor& editor)
	{
		std::vector<Fingerprint> out;
		for (std::size_t i = 0; i < editor.asset_count(); i++)
		{
			const EditorAsset a = editor.asset(i);
			out.push_back(Fingerprint{ a.type, a.name, editor.asset_fingerprint(i) });
		}
		return out;
	}

	std::string label(const std::int32_t type, const std::string& name)
	{
		return asset_type_name(type) + ":" + name;
	}

	bool apply_edit(ZoneEditor& editor, const Edit& edit, const Args& args, const ClientKey& key)
	{
		const auto split = [&](const char separator, std::string& left, std::string& right)
		{
			const std::size_t at = edit.value.find(separator);
			if (at == std::string::npos)
			{
				return false;
			}
			left = edit.value.substr(0, at);
			right = edit.value.substr(at + 1);
			return true;
		};

		if (edit.kind == "--set-ents")
		{
			std::vector<std::uint8_t> text;
			if (const Status s = read_file(edit.value, text); !s)
			{
				return fail(s.message);
			}
			while (!text.empty() && text.back() == 0)
			{
				text.pop_back();
			}
			if (const Status s = editor.set_map_ents({ reinterpret_cast<const char*>(text.data()), text.size() }); !s)
			{
				return fail("--set-ents: " + s.message);
			}
			std::printf("  entity string replaced: %zu characters\n", text.size());
			return true;
		}
		if (edit.kind == "--add-rawfile")
		{
			std::string name, file;
			if (!split('=', name, file))
			{
				return fail("--add-rawfile expects <name>=<file>");
			}
			std::vector<std::uint8_t> data;
			if (const Status s = read_file(file, data); !s)
			{
				return fail(s.message);
			}
			const bool existed = editor.find_asset(c1::ASSET_TYPE_RAWFILE, name) != ZoneEditor::npos;
			if (const Status s = editor.set_rawfile(name, data, !args.storeRawfiles); !s)
			{
				return fail("--add-rawfile " + name + ": " + s.message);
			}
			std::printf("  rawfile %s %s: %zu bytes\n", name.c_str(), existed ? "replaced" : "added", data.size());
			return true;
		}
		if (edit.kind == "--set-stringtable")
		{
			std::string name, file;
			if (!split('=', name, file))
			{
				return fail("--set-stringtable expects <name>=<file.csv>");
			}
			std::vector<std::uint8_t> data;
			if (const Status s = read_file(file, data); !s)
			{
				return fail(s.message);
			}
			const auto rows = parse_csv(data);
			StringTableData table;
			table.rows = static_cast<int>(rows.size());
			for (const auto& row : rows)
			{
				table.columns = std::max(table.columns, static_cast<int>(row.size()));
			}
			for (const auto& row : rows)
			{
				for (int c = 0; c < table.columns; c++)
				{
					table.cells.push_back(static_cast<std::size_t>(c) < row.size() ? row[static_cast<std::size_t>(c)] : std::string());
				}
			}
			if (const Status s = editor.set_string_table(name, table); !s)
			{
				return fail("--set-stringtable " + name + ": " + s.message);
			}
			std::printf("  string table %s: %d rows x %d columns\n", name.c_str(), table.rows, table.columns);
			return true;
		}
		if (edit.kind == "--remove")
		{
			std::string type, name;
			if (!split(':', type, name) || asset_type_from_name(type) < 0)
			{
				return fail("--remove expects <type>:<name> with a type as printed by --list");
			}
			const std::size_t index = editor.find_asset(asset_type_from_name(type), name);
			if (index == ZoneEditor::npos)
			{
				return fail("--remove: the zone has no top level asset " + edit.value);
			}
			if (const Status s = editor.remove_asset(index); !s)
			{
				return fail("--remove: " + s.message);
			}
			std::printf("  removed %s\n", edit.value.c_str());
			return true;
		}
		if (edit.kind == "--take")
		{
			const std::size_t at = edit.value.rfind('@');
			const std::size_t colon = edit.value.find(':');
			if (at == std::string::npos || colon == std::string::npos || colon > at)
			{
				return fail("--take expects <type>:<name>@<donor.ff>");
			}
			const std::string type = edit.value.substr(0, colon);
			const std::string name = edit.value.substr(colon + 1, at - colon - 1);
			const std::string file = edit.value.substr(at + 1);
			const std::int32_t typeId = asset_type_from_name(type);
			if (typeId < 0)
			{
				return fail("--take: unknown asset type '" + type + "'");
			}
			ZoneEditor donor;
			if (const Status s = donor.open(file, key); !s)
			{
				return fail(s.message);
			}
			const std::size_t donorIndex = donor.find_asset(typeId, name);
			if (donorIndex == ZoneEditor::npos)
			{
				return fail("--take: " + file + " has no top level asset " + type + ":" + name);
			}
			const std::size_t existing = editor.find_asset(typeId, name);
			const Status s = existing != ZoneEditor::npos ? editor.replace_asset(existing, donor, donorIndex)
				: editor.add_asset(donor, donorIndex, ZoneEditor::npos, args.withDeps);
			if (!s)
			{
				return fail("--take: " + s.message);
			}
			std::printf("  %s %s from %s\n", existing != ZoneEditor::npos ? "replaced" : "added", (type + ":" + name).c_str(), file.c_str());
			return true;
		}
		return fail("unknown edit " + edit.kind);
	}

	void print_report(const ZoneWriteReport& report)
	{
		std::printf("  zone: %llu bytes, %llu chunks, %llu zone pointers ( %llu forward )\n", static_cast<unsigned long long>(report.streamSize),
			static_cast<unsigned long long>(report.chunks), static_cast<unsigned long long>(report.relocations),
			static_cast<unsigned long long>(report.forwardPatches));
		std::printf("  blocks:");
		for (int b = 0; b < BLOCK_COUNT; b++)
		{
			std::printf(" %X", report.header.blockSize[b]);
		}
		std::printf("\n");
	}

	int cmd_link(const Args& args)
	{
		if (args.out.empty() && args.zoneOut.empty())
		{
			return fail("--out <new.ff> is required"), 2;
		}
		ClientKey key;
		if (const Status s = key.load_from_exe(args.exe); !s)
		{
			return fail(s.message), 1;
		}
		ZoneEditor editor;
		if (!args.base.empty())
		{
			if (const Status s = editor.open(args.base, key); !s)
			{
				return fail(s.message), 1;
			}
			std::printf("base %s: %zu assets, %llu chunks, %llu zone pointers\n", args.base.c_str(), editor.asset_count(),
				static_cast<unsigned long long>(editor.capture_stats().chunks), static_cast<unsigned long long>(editor.capture_stats().relocations));
		}
		else
		{
			if (const Status s = editor.create(args.newName); !s)
			{
				return fail(s.message), 1;
			}
			std::printf("new zone %s\n", args.newName.c_str());
		}
		const std::vector<Fingerprint> before = fingerprints(editor);

		for (const Edit& edit : args.edits)
		{
			if (!apply_edit(editor, edit, args, key))
			{
				return 1;
			}
		}

		std::vector<std::uint8_t> zone;
		ZoneWriteReport report;
		if (const Status s = editor.write(zone, &report); !s)
		{
			return fail("zone layout failed: " + s.message), 1;
		}
		print_report(report);
		if (!args.zoneOut.empty())
		{
			if (const Status s = write_file(args.zoneOut, zone); !s)
			{
				return fail(s.message), 1;
			}
		}
		if (args.out.empty())
		{
			return 0;
		}
		FastFileWriteOptions options;
		options.compression = args.level;
		std::vector<std::uint8_t> file;
		if (const Status s = encode_fastfile(zone, key, file, options); !s)
		{
			return fail(s.message), 1;
		}
		if (const Status s = write_file(args.out, file); !s)
		{
			return fail(s.message), 1;
		}
		std::printf("wrote %s: IWffu100, %zu bytes\n", args.out.c_str(), file.size());
		if (args.noCheck)
		{
			return 0;
		}

		// read the file back the way any consumer would, and compare asset by asset
		std::vector<std::uint8_t> reread;
		if (const Status s = read_fastfile(args.out, key, reread); !s)
		{
			return fail("check: the written file does not decode: " + s.message), 1;
		}
		if (reread != zone)
		{
			return fail("check: the written file does not inflate to the zone that was written"), 1;
		}
		ZoneEditor check;
		if (const Status s = check.load(std::move(reread), fs::path(args.out).stem().string()); !s)
		{
			return fail("check: the written zone does not load: " + s.message), 1;
		}
		std::multimap<std::string, std::uint64_t> old;
		for (const Fingerprint& f : before)
		{
			old.emplace(label(f.type, f.name), f.hash);
		}
		std::size_t same = 0, added = 0;
		std::vector<std::string> changed;
		for (const Fingerprint& f : fingerprints(check))
		{
			const std::string id = label(f.type, f.name);
			const auto range = old.equal_range(id);
			auto hit = range.second;
			for (auto it = range.first; it != range.second; ++it)
			{
				if (it->second == f.hash)
				{
					hit = it;
					break;
				}
			}
			if (hit != range.second)
			{
				same++;
				old.erase(hit);
			}
			else if (range.first != range.second)
			{
				changed.push_back(id);
				old.erase(range.first);
			}
			else
			{
				added++;
				changed.push_back("+" + id);
			}
		}
		for (const auto& [id, hash] : old)
		{
			changed.push_back("-" + id);
		}
		std::printf("check: the file decodes and all %zu assets load ( %zu warnings ). %zu assets unchanged, %zu changed / added / removed:\n",
			check.asset_count(), check.zone().warnings().size(), same, changed.size());
		for (std::size_t i = 0; i < changed.size() && i < 40; i++)
		{
			std::printf("    %s\n", changed[i].c_str());
		}
		return 0;
	}

	int cmd_list(const Args& args)
	{
		ClientKey key;
		if (const Status s = key.load_from_exe(args.exe); !s)
		{
			return fail(s.message), 1;
		}
		ZoneEditor editor;
		if (const Status s = editor.open(args.list, key); !s)
		{
			return fail(s.message), 1;
		}
		const ZoneCaptureStats& stats = editor.capture_stats();
		std::printf("%s: %zu assets, %llu chunks, %llu zone pointers ( %llu own data, %llu other asset's data, %llu loader allocations, %llu asset slots, %llu unresolved )\n",
			editor.name().c_str(), editor.asset_count(), static_cast<unsigned long long>(stats.chunks), static_cast<unsigned long long>(stats.relocations),
			static_cast<unsigned long long>(stats.toChunk), static_cast<unsigned long long>(stats.toOtherUnit),
			static_cast<unsigned long long>(stats.toAllocation), static_cast<unsigned long long>(stats.toAssetSlot),
			static_cast<unsigned long long>(stats.unresolved));
		std::printf("%6s %-22s %10s %8s %8s %s\n", "#", "type", "bytes", "chunks", "pointers", "name");
		for (std::size_t i = 0; i < editor.asset_count(); i++)
		{
			const EditorAsset a = editor.asset(i);
			std::printf("%6zu %-22s %10llu %8u %8u %s%s\n", i, asset_type_name(a.type).c_str(), static_cast<unsigned long long>(a.streamBytes), a.chunks,
				a.relocations, a.name.c_str(), a.usesScriptStrings ? "  [script strings]" : "");
		}
		return 0;
	}

	struct RoundTrip
	{
		bool decoded = false, captured = false, written = false, identical = false;
		bool containerTried = false, containerOk = false;
		std::string detail;
		std::uint64_t zoneSize = 0, fileSize = 0, writtenFileSize = 0;
		ZoneCaptureStats stats;
		std::size_t assets = 0;
		double seconds = 0;
	};

	// where two zones first differ, in terms of the asset list of the original
	std::string first_difference(const Zone& zone, const std::vector<std::uint8_t>& original, const std::vector<std::uint8_t>& written)
	{
		const std::size_t n = std::min(original.size(), written.size());
		std::size_t at = 0;
		while (at < n && original[at] == written[at])
		{
			at++;
		}
		char text[256];
		if (at == n)
		{
			std::snprintf(text, sizeof(text), "length %zu instead of %zu", written.size(), original.size());
			return text;
		}
		if (at < 40)
		{
			std::snprintf(text, sizeof(text), "XFile header differs at byte 0x%zX ( field %s )", at,
				at < 4 ? "size" : at < 8 ? "externalSize" : "blockSize");
			return text;
		}
		const auto entries = zone.assets();
		for (std::size_t i = 0; i < entries.size(); i++)
		{
			const std::uint64_t start = 40ull + entries[i].dataOffset;
			if (at >= start && at < start + entries[i].dataSize)
			{
				std::snprintf(text, sizeof(text), "first difference at 0x%zX: asset #%zu %s '%s' + 0x%llX", at, i, asset_type_name(entries[i].type).c_str(),
					std::string(zone.asset_name(entries[i].type, entries[i].header)).c_str(), static_cast<unsigned long long>(at - start));
				return text;
			}
		}
		std::snprintf(text, sizeof(text), "first difference at 0x%zX ( XAssetList: script strings / XAsset table )", at);
		return text;
	}

	RoundTrip round_trip(const fs::path& file, const ClientKey& key, const Args& args, const fs::path& outDir)
	{
		RoundTrip r;
		const auto start = std::chrono::steady_clock::now();
		std::vector<std::uint8_t> original;
		FastFileInfo info;
		if (const Status s = read_fastfile(file, key, original, &info); !s)
		{
			r.detail = "decode: " + s.message;
			return r;
		}
		r.decoded = true;
		r.zoneSize = original.size();
		r.fileSize = info.fileSize;

		std::vector<std::uint8_t> written;
		{
			ZoneEditor editor;
			if (const Status s = editor.load(original, file.stem().string()); !s)
			{
				r.detail = "capture: " + s.message;
				r.seconds = seconds_since(start);
				return r;
			}
			r.captured = true;
			r.stats = editor.capture_stats();
			r.assets = editor.asset_count();
			if (const Status s = editor.write(written); !s)
			{
				r.detail = "write: " + s.message;
				r.seconds = seconds_since(start);
				return r;
			}
			r.written = true;
			r.identical = written == original;
			if (!r.identical)
			{
				r.detail = first_difference(editor.zone(), original, written);
			}
		}
		std::vector<std::uint8_t>().swap(original);

		if (!args.noContainer)
		{
			r.containerTried = true;
			FastFileWriteOptions options;
			options.compression = args.level;
			const fs::path out = outDir / file.filename();
			std::vector<std::uint8_t> encoded;
			Status s = encode_fastfile(written, key, encoded, options);
			if (s)
			{
				r.writtenFileSize = encoded.size();
				s = write_file(out, encoded);
			}
			std::vector<std::uint8_t>().swap(encoded);
			std::vector<std::uint8_t> reread;
			if (s)
			{
				s = read_fastfile(out, key, reread);
			}
			if (!s)
			{
				r.detail += (r.detail.empty() ? "" : "; ") + ("container: " + s.message);
			}
			else if (reread != written)
			{
				r.detail += (r.detail.empty() ? "" : "; ") + std::string("container: the file read back differs from the zone written");
			}
			else
			{
				r.containerOk = true;
			}
			if (!args.keep)
			{
				std::error_code ec;
				fs::remove(out, ec);
			}
		}
		r.seconds = seconds_since(start);
		return r;
	}

	int cmd_verify(const Args& args)
	{
		ClientKey key;
		if (const Status s = key.load_from_exe(args.exe); !s)
		{
			return fail(s.message), 1;
		}
		std::vector<std::uint8_t> zoneBytes;
		FastFileInfo info;
		if (const Status s = read_fastfile(args.verify, key, zoneBytes, &info); !s)
		{
			return fail(s.message), 1;
		}
		std::printf("%s\n  container: %s, version %u, %llu bytes, zlib stream %llu bytes -> zone %llu bytes\n", args.verify.c_str(), info.header.magic,
			info.header.version, static_cast<unsigned long long>(info.fileSize), static_cast<unsigned long long>(info.compressedSize),
			static_cast<unsigned long long>(info.zoneSize));
		if (info.header.is_signed())
		{
			std::printf("  signed: auth name '%s', sub header hash %s, %u master blocks, %u data chunks, %u hash failures\n", info.authName.c_str(),
				info.subheaderHashValid ? "valid" : "INVALID", info.masterBlocks, info.dataChunks, info.hashFailures);
		}
		else
		{
			std::printf("  unsigned: a retail client refuses this container when it is read from a file ( see README, \"What loads an unsigned file\" )\n");
		}
		ZoneEditor editor;
		if (const Status s = editor.load(zoneBytes, fs::path(args.verify).stem().string()); !s)
		{
			return fail("the zone does not load: " + s.message), 1;
		}
		const Zone& zone = editor.zone();
		std::printf("  zone: %zu assets ( %zu including nested ), %zu script strings, %zu warnings\n", zone.assets().size(), zone.all_assets().size(),
			zone.script_strings().size(), zone.warnings().size());
		for (const std::string& warning : zone.warnings())
		{
			std::printf("    warning: %s\n", warning.c_str());
		}
		std::map<std::string, std::size_t> perType;
		for (const XAssetEntry& entry : zone.assets())
		{
			perType[asset_type_name(entry.type)]++;
		}
		std::printf("  ");
		for (const auto& [type, count] : perType)
		{
			std::printf("%s %zu  ", type.c_str(), count);
		}
		std::printf("\n");
		std::vector<std::uint8_t> written;
		if (const Status s = editor.write(written); !s)
		{
			return fail("the zone loads but cannot be written again: " + s.message), 1;
		}
		if (written == zoneBytes)
		{
			std::printf("  read -> write: identical ( %zu bytes )\n", written.size());
			return 0;
		}
		std::printf("  read -> write: DIFFERENT, %s\n", first_difference(zone, zoneBytes, written).c_str());
		return 1;
	}

	int cmd_roundtrip(const Args& args)
	{
		const fs::path install = args.roundtrip;
		const fs::path exe = args.exe.empty() ? install / "C1.exe" : fs::path(args.exe);
		ClientKey key;
		if (const Status s = key.load_from_exe(exe); !s)
		{
			return fail(s.message), 1;
		}
		std::vector<fs::path> files;
		std::error_code ec;
		for (fs::recursive_directory_iterator it(install / "client_shipRetail", ec), end; !ec && it != end; it.increment(ec))
		{
			if (it->is_regular_file(ec) && it->path().extension() == ".ff"
				&& (args.filter.empty() || it->path().filename().string().find(args.filter) != std::string::npos))
			{
				files.push_back(it->path());
			}
		}
		std::sort(files.begin(), files.end());
		if (files.empty())
		{
			return fail("no .ff files below " + (install / "client_shipRetail").string()), 1;
		}
		const fs::path outDir = args.outDir.empty() ? fs::current_path() / "codol_linker_roundtrip" : fs::path(args.outDir);
		if (!args.noContainer)
		{
			fs::create_directories(outDir, ec);
			if (fs::equivalent(outDir, install, ec) || outDir.string().find(install.string()) == 0)
			{
				return fail("--out-dir must not be inside the install"), 2;
			}
		}

		std::printf("%-40s %-8s %11s %7s %9s %10s %8s %-9s %-9s %6s\n", "zone", "magic", "zone bytes", "assets", "chunks", "pointers", "unresolv", "zone", "container",
			"sec");
		std::size_t identical = 0, containerOk = 0, failed = 0;
		std::uint64_t totalBytes = 0, totalWritten = 0, totalFile = 0;
		ZoneCaptureStats total;
		std::vector<std::string> problems;
		const auto start = std::chrono::steady_clock::now();
		for (const fs::path& file : files)
		{
			FastFileHeader header;
			{
				std::vector<std::uint8_t> head(FASTFILE_HEADER_SIZE);
				if (FILE* f = std::fopen(file.string().c_str(), "rb"))
				{
					const std::size_t got = std::fread(head.data(), 1, head.size(), f);
					std::fclose(f);
					head.resize(got);
				}
				parse_fastfile_header(head, header);
			}
			const RoundTrip r = round_trip(file, key, args, outDir);
			const std::string name = fs::relative(file, install / "client_shipRetail", ec).generic_string();
			std::printf("%-40s %-8s %11llu %7zu %9llu %10llu %8llu %-9s %-9s %6.1f\n", name.c_str(), header.magic, static_cast<unsigned long long>(r.zoneSize),
				r.assets, static_cast<unsigned long long>(r.stats.chunks), static_cast<unsigned long long>(r.stats.relocations),
				static_cast<unsigned long long>(r.stats.unresolved), r.identical ? "identical" : r.written ? "DIFFERENT" : "FAILED",
				!r.containerTried ? "-" : r.containerOk ? "ok" : "FAILED", r.seconds);
			if (!r.detail.empty())
			{
				std::printf("    %s\n", r.detail.c_str());
				problems.push_back(name + ": " + r.detail);
			}
			std::fflush(stdout);
			identical += r.identical;
			containerOk += r.containerOk;
			failed += !r.written;
			totalBytes += r.zoneSize;
			totalFile += r.fileSize;
			totalWritten += r.writtenFileSize;
			total.chunks += r.stats.chunks;
			total.relocations += r.stats.relocations;
			total.toChunk += r.stats.toChunk;
			total.toOtherUnit += r.stats.toOtherUnit;
			total.toAllocation += r.stats.toAllocation;
			total.toAssetSlot += r.stats.toAssetSlot;
			total.unresolved += r.stats.unresolved;
		}
		std::printf("\nsummary: %zu zones, %.1f GiB inflated, %.0f s\n", files.size(), static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0),
			seconds_since(start));
		std::printf("  read -> write byte identical : %zu of %zu ( %zu could not be captured or written )\n", identical, files.size(), failed);
		if (!args.noContainer)
		{
			std::printf("  IWffu100 written, read back  : %zu of %zu ( %.2f GiB written at level %d, the originals are %.2f GiB )\n", containerOk, files.size(),
				static_cast<double>(totalWritten) / (1024.0 * 1024.0 * 1024.0), args.level, static_cast<double>(totalFile) / (1024.0 * 1024.0 * 1024.0));
		}
		std::printf("  stream reads ( chunks )      : %llu\n", static_cast<unsigned long long>(total.chunks));
		std::printf("  zone pointers re-encoded     : %llu ( own data %llu, other asset's data %llu, loader allocations %llu, asset slots %llu, kept as is %llu )\n",
			static_cast<unsigned long long>(total.relocations), static_cast<unsigned long long>(total.toChunk),
			static_cast<unsigned long long>(total.toOtherUnit), static_cast<unsigned long long>(total.toAllocation),
			static_cast<unsigned long long>(total.toAssetSlot), static_cast<unsigned long long>(total.unresolved));
		for (const std::string& problem : problems)
		{
			std::printf("  problem: %s\n", problem.c_str());
		}
		return identical == files.size() && (args.noContainer || containerOk == files.size()) ? 0 : 1;
	}
}

int main(const int argc, char** argv)
{
	Args args;
	for (int i = 1; i < argc; i++)
	{
		const std::string a = argv[i];
		const auto value = [&](std::string& into)
		{
			if (i + 1 >= argc)
			{
				std::fprintf(stderr, "error: %s needs a value\n", a.c_str());
				std::exit(2);
			}
			into = argv[++i];
		};
		if (a == "--base") value(args.base);
		else if (a == "--new") value(args.newName);
		else if (a == "--exe") value(args.exe);
		else if (a == "--out") value(args.out);
		else if (a == "--zone-out") value(args.zoneOut);
		else if (a == "--list") value(args.list);
		else if (a == "--verify") value(args.verify);
		else if (a == "--roundtrip-all") value(args.roundtrip);
		else if (a == "--out-dir") value(args.outDir);
		else if (a == "--filter") value(args.filter);
		else if (a == "--level")
		{
			std::string level;
			value(level);
			args.level = std::atoi(level.c_str());
		}
		else if (a == "--with-deps") args.withDeps = true;
		else if (a == "--store-rawfiles") args.storeRawfiles = true;
		else if (a == "--no-check") args.noCheck = true;
		else if (a == "--no-container") args.noContainer = true;
		else if (a == "--keep") args.keep = true;
		else if (a == "--set-ents" || a == "--add-rawfile" || a == "--set-stringtable" || a == "--remove" || a == "--take")
		{
			Edit edit;
			edit.kind = a;
			value(edit.value);
			args.edits.push_back(edit);
		}
		else if (a == "--help" || a == "-h")
		{
			std::fputs(USAGE, stdout);
			return 0;
		}
		else
		{
			std::fprintf(stderr, "error: unknown argument %s\n\n", a.c_str());
			std::fputs(USAGE, stderr);
			return 2;
		}
	}

	try
	{
		if (!args.roundtrip.empty()) return cmd_roundtrip(args);
		if (args.exe.empty() && (!args.base.empty() || !args.newName.empty() || !args.list.empty() || !args.verify.empty()))
		{
			std::fprintf(stderr, "error: --exe <C1.exe> is required ( the fast file key is taken from the client )\n");
			return 2;
		}
		if (!args.verify.empty()) return cmd_verify(args);
		if (!args.list.empty()) return cmd_list(args);
		if (!args.base.empty() || !args.newName.empty()) return cmd_link(args);
	}
	catch (const std::exception& e)
	{
		std::fprintf(stderr, "error: %s\n", e.what());
		return 1;
	}
	std::fputs(USAGE, stdout);
	return argc > 1 ? 2 : 0;
}
