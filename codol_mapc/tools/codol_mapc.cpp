// codol_mapc: compiles an edited map into the fast file it was imported from.
//
//   codol_mapc --base <map.ff> --exe <C1.exe> --map <edited.map> [--baseline <map.codol_base.map>] --out <new.ff>
//              [--report <file>] [--zone-out <file>] [--donor <zone.ff>]... [--level 0..9]
//              [--no-entities] [--no-models] [--no-brushes] [--no-surfaces]
//
// Exit code 0: compiled; 1: failed ( message on stderr ); 2: usage.

#include "codol_mapc/mapc.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

int main(const int argc, char** argv)
{
	codol::mapc::options opts;
	std::filesystem::path report_file;
	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
		const auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
		if (arg == "--base") { opts.base = value(); }
		else if (arg == "--exe") { opts.exe = value(); }
		else if (arg == "--map") { opts.map = value(); }
		else if (arg == "--baseline") { opts.baseline = value(); }
		else if (arg == "--out") { opts.out = value(); }
		else if (arg == "--zone-out") { opts.zone_out = value(); }
		else if (arg == "--report") { report_file = value(); }
		else if (arg == "--donor") { opts.donors.emplace_back(value()); }
		else if (arg == "--level") { opts.level = std::atoi(value().c_str()); }
		else if (arg == "--no-entities") { opts.entities = false; }
		else if (arg == "--no-models") { opts.models = false; }
		else if (arg == "--no-brushes") { opts.brushes = false; }
		else if (arg == "--no-surfaces") { opts.surfaces = false; }
		else
		{
			std::fprintf(stderr, "unknown argument %s\n", arg.c_str());
			return 2;
		}
	}
	if (opts.base.empty() || opts.exe.empty() || opts.map.empty() || opts.out.empty())
	{
		std::fprintf(stderr,
			"codol_mapc --base <map.ff> --exe <C1.exe> --map <edited.map> [--baseline <map.codol_base.map>] --out <new.ff>\n"
			"           [--report <file>] [--zone-out <file>] [--donor <zone.ff>]... [--level 0..9]\n"
			"           [--no-entities] [--no-models] [--no-brushes] [--no-surfaces]\n");
		return 2;
	}
	codol::mapc::report report;
	std::string error;
	const bool ok = codol::mapc::compile(opts, report, error);
	std::string text = ok ? report.text() : "failed: " + error + "\n";
	if (ok)
	{
		text += "wrote " + opts.out.string() + " ( IWffu100 )\n";
	}
	std::fputs(text.c_str(), ok ? stdout : stderr);
	if (!report_file.empty())
	{
		std::ofstream(report_file, std::ios::binary) << text;
	}
	return ok ? 0 : 1;
}
