// codol_asset_verify: round trips of codol_assets against the shipped assets of an install. Every number in the
// README comes from this tool.
//
//   codol_asset_verify iwi-png <install> <C1.exe> <image> <out.png>            a shipped image as PNG + channel statistics
//   codol_asset_verify image-roundtrip <install> <C1.exe> <use> <image> ..     shipped image -> source -> compile -> IWI ->
//                                                                              codol_render; PSNR per level
//   codol_asset_verify image-file <file> <use> <out.iwi> [<out.png>]           a source file through the compiler
//   codol_asset_verify material-roundtrip <install> <C1.exe> <library cache> [<every n-th>] [<dump dir>]
//                                                 shipped material -> definition text -> compile; match rate per field

#include "verify.hpp"

#include "codol_assets/image.hpp"

#include "codol_render/iwd.hpp"
#include "codol_render/texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace codol::assets;
namespace fs = std::filesystem;
namespace r = codol::render;

namespace
{
	bool parse_use(const std::string& text, ImageUse& use)
	{
		const std::pair<const char*, ImageUse> names[] = { { "color", ImageUse::color }, { "normal", ImageUse::normal }, { "specular", ImageUse::specular },
			{ "detail", ImageUse::detail }, { "ui", ImageUse::ui }, { "function", ImageUse::function } };
		for (const auto& [name, value] : names)
		{
			if (text == name)
			{
				use = value;
				return true;
			}
		}
		return false;
	}

	struct Archives
	{
		r::image_archive_sets sets;
		fs::path exe;
		bool open(const fs::path& install, const fs::path& exePath)
		{
			exe = exePath;
			r::iwd_key key;
			std::string error;
			if (!r::derive_iwd_key(exe, key, &error) || !r::open_image_archives(install, key, sets, &error))
			{
				std::fprintf(stderr, "%s\n", error.c_str());
				return false;
			}
			return true;
		}
		bool read(const std::string& name, IwiLevels& levels)
		{
			std::vector<std::uint8_t> file;
			std::string error;
			if (!sets.read(name, file, &error))
			{
				std::fprintf(stderr, "%s: %s\n", name.c_str(), error.c_str());
				return false;
			}
			if (const Status s = read_iwi_levels(file, exe, levels); !s)
			{
				std::fprintf(stderr, "%s: %s\n", name.c_str(), s.message.c_str());
				return false;
			}
			return true;
		}
	};

	void channel_stats(const Bitmap& b)
	{
		for (int c = 0; c < 4; c++)
		{
			int low = 255, high = 0;
			double sum = 0;
			for (std::size_t k = static_cast<std::size_t>(c); k < b.rgba.size(); k += 4)
			{
				low = std::min<int>(low, b.rgba[k]);
				high = std::max<int>(high, b.rgba[k]);
				sum += b.rgba[k];
			}
			std::printf("  %c: min %3d max %3d mean %6.2f\n", "RGBA"[c], low, high, sum / (static_cast<double>(b.rgba.size()) / 4));
		}
	}

	int iwi_png(Archives& archives, const std::string& name, const fs::path& out)
	{
		IwiLevels levels;
		if (!archives.read(name, levels))
		{
			return 1;
		}
		std::printf("%s: %d x %d, format %u ( %s ), flags 0x%08X, %d levels, %d faces\n", name.c_str(), levels.width, levels.height, levels.format,
			r::iwi_format_name(static_cast<r::iwi_format>(levels.format)), levels.flags, levels.levelCount, levels.faces);
		channel_stats(levels.levels[0]);
		const Status s = save_png(out, levels.levels[0]);
		if (!s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
		}
		return s ? 0 : 1;
	}

	// A shipped image is the source: its top level, decoded. It goes through the compiler ( own mips, own encoder ),
	// the IWI writer and back through codol_render.
	//   vs source   compiled level against the uncompressed level the encoder was given: what the encoder loses
	//   vs shipped  compiled level against the same level of the shipped file: both are lossy views of one image
	int image_roundtrip(Archives& archives, const ImageUse use, const std::vector<std::string>& names)
	{
		int failures = 0;
		for (const std::string& name : names)
		{
			IwiLevels shipped;
			if (!archives.read(name, shipped))
			{
				failures++;
				continue;
			}
			SourceImage source;
			for (int face = 0; face < shipped.faces; face++)
			{
				source.faces.push_back(shipped.levels[static_cast<std::size_t>(face)]);
			}
			const bool normal = use == ImageUse::normal;
			if (normal)
			{
				// stored: x in alpha, y in green. The source a modder has: x in red, y in green, z in blue.
				for (Bitmap& face : source.faces)
				{
					for (std::size_t k = 0; k < face.rgba.size(); k += 4)
					{
						const float x = face.rgba[k + 3] / 127.5f - 1.0f, y = face.rgba[k + 1] / 127.5f - 1.0f;
						const float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));
						face.rgba[k] = face.rgba[k + 3];
						face.rgba[k + 2] = static_cast<std::uint8_t>(std::min(255.0f, (z + 1.0f) * 127.5f + 0.5f));
						face.rgba[k + 3] = 255;
					}
				}
			}
			ImageOptions options;
			options.use = use;
			options.streamed = true;
			options.mips = shipped.levelCount > 1 ? 1 : 0;
			options.noPicmip = (shipped.flags & r::iwi_flags::NO_PICMIP) ? 1 : 0;
			options.clampU = (shipped.flags & r::iwi_flags::CLAMP_U) != 0;
			options.clampV = (shipped.flags & r::iwi_flags::CLAMP_V) != 0;
			switch (static_cast<r::iwi_format>(shipped.format))
			{
			case r::iwi_format::dxt1: options.format = ImageFormat::dxt1; break;
			case r::iwi_format::dxt3: options.format = ImageFormat::dxt3; break;
			case r::iwi_format::dxt5: options.format = ImageFormat::dxt5; break;
			default: break;
			}
			CompiledImage compiled;
			if (const Status s = compile_image(name, source, options, compiled); !s)
			{
				std::fprintf(stderr, "%s\n", s.message.c_str());
				failures++;
				continue;
			}
			const std::vector<std::uint8_t> file = write_iwi(compiled);
			IwiLevels back;
			if (const Status s = read_iwi_levels(file, archives.exe, back); !s)
			{
				std::fprintf(stderr, "%s: written file: %s\n", name.c_str(), s.message.c_str());
				failures++;
				continue;
			}
			const bool header = back.flags == shipped.flags && back.format == shipped.format && back.width == shipped.width && back.height == shipped.height
				&& back.levelCount == shipped.levelCount && back.fileSizeForPicmip == shipped.fileSizeForPicmip;
			std::printf("%s: %d x %d %s flags 0x%X, %d levels; header + level table %s ( flags 0x%X, sizes %d %d %d %d )\n", name.c_str(), back.width, back.height,
				r::iwi_format_name(static_cast<r::iwi_format>(back.format)), back.flags, back.levelCount, header ? "equal to the shipped file" : "DIFFER from the shipped file",
				shipped.flags, shipped.fileSizeForPicmip[0], shipped.fileSizeForPicmip[1], shipped.fileSizeForPicmip[2], shipped.fileSizeForPicmip[3]);
			// a normal map is read as x = alpha, y = green
			const unsigned mask = normal ? 0xA : (static_cast<r::iwi_format>(back.format) == r::iwi_format::dxt1 ? 0x7 : 0xF);
			std::printf("  level   size        vs source   vs shipped   ( PSNR dB, channels %s )\n", normal ? "G A" : mask == 7 ? "R G B" : "R G B A");
			for (int level = 0; level < back.levelCount && level < shipped.levelCount; level++)
			{
				const Bitmap& mine = back.levels[static_cast<std::size_t>(level) * back.faces];
				const Bitmap& theirs = shipped.levels[static_cast<std::size_t>(level) * shipped.faces];
				const Bitmap& reference = compiled.reference[static_cast<std::size_t>(level) * compiled.faces];
				std::printf("  %5d   %4d x %-4d %9.2f   %9.2f\n", level, mine.width, mine.height, psnr(mine, reference, mask), psnr(mine, theirs, mask));
			}
			failures += header ? 0 : 1;
		}
		return failures ? 1 : 0;
	}

	int image_file(const fs::path& in, const ImageUse use, const fs::path& outIwi, const fs::path& outPng)
	{
		SourceImage source;
		if (const Status s = load_image(in, source); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
		}
		ImageOptions options;
		options.use = use;
		options.streamed = true;
		CompiledImage compiled;
		if (const Status s = compile_image(in.stem().string(), source, options, compiled); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
		}
		const std::vector<std::uint8_t> file = write_iwi(compiled);
		IwiLevels back;
		if (const Status s = read_iwi_levels(file, {}, back); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
		}
		std::printf("%s: %d x %d, %zu face(s), alpha %s -> %s, flags 0x%X, %d levels, %zu bytes\n", in.string().c_str(), back.width, back.height, source.faces.size(),
			source.hasAlpha ? "yes" : "no", r::iwi_format_name(static_cast<r::iwi_format>(back.format)), back.flags, back.levelCount, file.size());
		for (int level = 0; level < back.levelCount; level++)
		{
			const Bitmap& mine = back.levels[static_cast<std::size_t>(level) * back.faces];
			std::printf("  level %d  %4d x %-4d  PSNR %.2f dB\n", level, mine.width, mine.height,
				psnr(mine, compiled.reference[static_cast<std::size_t>(level) * compiled.faces], use == ImageUse::normal ? 0xA : 0xF));
		}
		if (const Status s = codol::zone::write_file(outIwi, file); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
		}
		if (!outPng.empty())
		{
			save_png(outPng, back.levels[0]);
		}
		return 0;
	}
}

int main(const int argc, char** argv)
{
	const std::string command = argc > 1 ? argv[1] : "";
	ImageUse use = ImageUse::color;
	if (command == "iwi-png" && argc == 6)
	{
		Archives archives;
		return archives.open(argv[2], argv[3]) ? iwi_png(archives, argv[4], argv[5]) : 1;
	}
	if (command == "image-roundtrip" && argc >= 6 && parse_use(argv[4], use))
	{
		Archives archives;
		return archives.open(argv[2], argv[3]) ? image_roundtrip(archives, use, std::vector<std::string>(argv + 5, argv + argc)) : 1;
	}
	if (command == "material-roundtrip" && argc >= 5)
	{
		return material_roundtrip(argv[2], argv[3], argv[4], argc > 5 ? std::max(1, std::atoi(argv[5])) : 1, argc > 6 ? argv[6] : "");
	}
	if (command == "image-file" && argc >= 5 && parse_use(argv[3], use))
	{
		return image_file(argv[2], use, argv[4], argc > 5 ? argv[5] : "");
	}
	std::fprintf(stderr, "usage: see the head of asset_verify.cpp\n");
	return 2;
}
