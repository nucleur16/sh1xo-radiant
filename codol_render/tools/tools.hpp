#pragma once

// Shared declarations of the real-data check tool ( codol_render_check ). Not part of the library.

#include "codol_render/common.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tools
{
	// ---- png_wic.cpp: PNG -> 8 bit RGBA ( straight alpha ) with the Windows Imaging Component ---------------
	struct png_image
	{
		int width = 0;
		int height = 0;
		std::vector<std::uint8_t> rgba;
	};
	bool load_png(const std::filesystem::path& path, png_image& out, std::string& error);

	bool read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& out);

	// ---- zone_scan.cpp: locating techsets / materials / shaders in a decrypted, inflated zone ----------------
	//
	// A zone file is the loader's input stream: structures in load order, a pointer field holding 0xFFFFFFFF /
	// 0xFFFFFFFE where the pointee follows inline, and ( block << 28 | offset ) + 1 where it was loaded earlier.
	// There is no alignment padding in the stream. Without a full zone reader the "loaded earlier" references
	// cannot be resolved ( that needs the position of every block after every asset of the zone ), but everything
	// that is inline can be followed: the scanner finds MaterialTechniqueSet and Material headers by their shape,
	// walks their inline sub-objects in load order and patches every inline pointer field of a COPY of the zone
	// with ZONE_BASE + file offset of the pointee. The result is a flat memory image the library's zone_view
	// readers work on; unresolved references stay as they are and read as "not loaded".
	inline constexpr std::uint32_t ZONE_BASE = 0xC0000000;		// above every block reference ( blocks 0 .. 8 )

	struct scanned_shader
	{
		std::string name;				// empty when the header's name is a reference
		bool is_pixel_shader = false;
		std::uint32_t program_offset = 0;	// file offset of the byte code
		std::uint32_t program_size = 0;		// bytes, from walking the tokens to END
		bool header_matches = false;	// a MaterialShader header precedes it whose programSize * 4 == program_size
	};

	struct scanned_zone
	{
		std::vector<std::uint8_t> image;			// the patched copy
		std::vector<std::uint32_t> techsets;		// zone pointers ( ZONE_BASE + offset ) of walked technique sets
		std::vector<std::uint32_t> materials;		// zone pointers of walked materials
		std::vector<std::uint32_t> partial_materials;	// materials whose walk stopped at a water map ( tables behind it unknown )
		std::vector<scanned_shader> shaders;		// every SM2+ program found by its byte code
		int techset_candidates = 0;					// header shaped byte patterns
		int techset_rejected = 0;					// .. that did not walk ( false positives of the pattern, or a layout the walker does not know )
		int material_candidates = 0;
		int material_rejected = 0;
		std::vector<std::string> rejected;			// descriptions of the rejected candidates

		codol::render::zone_view view() const { return codol::render::flat_zone_view(image, ZONE_BASE); }
	};

	bool scan_zone(const std::filesystem::path& path, scanned_zone& out, std::string& error);
}
