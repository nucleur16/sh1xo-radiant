#include "test.hpp"

#include "codol_render/texture.hpp"

using namespace codol::render;

namespace
{
	// IwiFile.cs:146: "IWi", version, flags u32, format u8, unused u8, width / height / depth s16, 4 x s32
	test::bytes iwi_header_bytes(const std::uint8_t version, const std::uint32_t flags, const std::uint8_t format, const int width, const int height,
		const int depth = 1)
	{
		test::bytes b;
		b.u8('I').u8('W').u8('i').u8(version).u32(flags).u8(format).u8(0);
		b.u16(static_cast<std::uint16_t>(width)).u16(static_cast<std::uint16_t>(height)).u16(static_cast<std::uint16_t>(depth));
		b.u32(111).u32(222).u32(333).u32(444);
		return b;
	}

	bool pixel(const rgba_image& image, const int x, const int y, const int r, const int g, const int b, const int a)
	{
		const std::uint8_t* p = image.pixels.data() + (static_cast<std::size_t>(y) * image.width + x) * 4;
		const bool ok = p[0] == r && p[1] == g && p[2] == b && p[3] == a;
		if (!ok)
		{
			std::printf("  pixel (%d,%d) = (%d,%d,%d,%d), expected (%d,%d,%d,%d)\n", x, y, p[0], p[1], p[2], p[3], r, g, b, a);
		}
		return ok;
	}

	// Huffman tables for the wavelet tests: every entry has bit length 1, the value depends on the lowest bit of
	// the 12 bit index ( = the next bit of the stream ): 0 -> `zero`, 1 -> `one`
	std::vector<std::uint8_t> table(const std::int16_t zero, const std::int16_t one)
	{
		test::bytes b;
		for (int i = 0; i < 4096; i++)
		{
			b.u16(static_cast<std::uint16_t>((i & 1) ? one : zero)).u16(1);
		}
		return b.data;
	}
}

void test_textures()
{
	std::string error;

	// ---- IWI header and layout ( IwiFile.cs ) ---------------------------------------------------------------
	CHECK_EQ(iwi_mip_level_count(0, 256, 64, 1), 9);
	CHECK_EQ(iwi_mip_level_count(0, 1, 1, 1), 1);
	CHECK_EQ(iwi_mip_level_count(0, 5, 3, 1), 4);			// 1, 2, 4 < 5 -> 4 levels
	CHECK_EQ(iwi_mip_level_count(iwi_flags::NO_MIPMAPS, 256, 256, 1), 1);
	CHECK_EQ(iwi_level_dimension(256, 3), 32);
	CHECK_EQ(iwi_level_dimension(4, 5), 1);
	CHECK_EQ(iwi_surface_size(iwi_format::rgba32, 3, 5), 60);
	CHECK_EQ(iwi_surface_size(iwi_format::rgb24, 3, 5), 45);
	CHECK_EQ(iwi_surface_size(iwi_format::luminance_alpha8, 3, 5), 30);
	CHECK_EQ(iwi_surface_size(iwi_format::alpha8, 3, 5), 15);
	CHECK_EQ(iwi_surface_size(iwi_format::dxt1, 5, 9), 8 * 2 * 3);
	CHECK_EQ(iwi_surface_size(iwi_format::dxt5, 1, 1), 16);
	CHECK_EQ(iwi_surface_size(iwi_format::wavelet_rgb, 4, 4), -1);

	{
		// DXT1 8x4 with mips: levels 3 ( 1x1 ), 2 ( 2x1 ), 1 ( 4x2 ), 0 ( 8x4 ): 8 + 8 + 8 + 16 bytes, smallest first
		auto file = iwi_header_bytes(9, iwi_flags::CLAMP_U | iwi_flags::NORMAL_MAP, 11, 8, 4);
		for (int i = 0; i < 40; i++)
		{
			file.u8(static_cast<std::uint8_t>(i));
		}
		iwi_file iwi;
		CHECK(parse_iwi(file.data, iwi, &error));
		CHECK(iwi.header.version == 9 && iwi.header.format == iwi_format::dxt1 && iwi.header.width == 8 && iwi.header.height == 4);
		CHECK(iwi.header.flags == (iwi_flags::CLAMP_U | iwi_flags::NORMAL_MAP) && iwi.header.is_normal_map() && !iwi.header.is_cube());
		CHECK(iwi.header.file_size_for_picmip[0] == 111 && iwi.header.file_size_for_picmip[3] == 444);
		CHECK_EQ(iwi.header.mip_count(), 4);
		CHECK_EQ(iwi.surfaces.size(), 4u);
		if (iwi.surfaces.size() == 4)
		{
			CHECK(iwi.surfaces[0].level == 3 && iwi.surfaces[0].width == 1 && iwi.surfaces[0].height == 1 && iwi.surfaces[0].offset == 32 && iwi.surfaces[0].size == 8);
			CHECK(iwi.surfaces[1].level == 2 && iwi.surfaces[1].width == 2 && iwi.surfaces[1].height == 1 && iwi.surfaces[1].offset == 40);
			CHECK(iwi.surfaces[2].level == 1 && iwi.surfaces[2].width == 4 && iwi.surfaces[2].height == 2 && iwi.surfaces[2].offset == 48);
			CHECK(iwi.surfaces[3].level == 0 && iwi.surfaces[3].width == 8 && iwi.surfaces[3].height == 4 && iwi.surfaces[3].offset == 56 && iwi.surfaces[3].size == 16);
			CHECK(iwi.surface(0, 0) == &iwi.surfaces[3] && iwi.surface(0, 1) == nullptr && iwi.surface(4, 0) == nullptr);
		}

		// what goes to the GPU: DXT as stored, level 0 first, block rows ( GameTextureCache.cs:85-114, 158-165 )
		texture_upload upload;
		CHECK(build_texture_upload(iwi, nullptr, upload, &error));
		CHECK(upload.d3d_format == d3d::FMT_DXT1 && upload.width == 8 && upload.height == 4 && upload.levels == 4 && upload.faces == 1 && !upload.is_cube());
		CHECK_EQ(upload.surfaces.size(), 4u);
		if (upload.surfaces.size() == 4)
		{
			CHECK(upload.surfaces[0].level == 0 && upload.surfaces[0].rows == 1 && upload.surfaces[0].row_bytes == 16 && upload.surfaces[0].data.size() == 16);
			CHECK(upload.surfaces[0].data[0] == 24 && upload.surfaces[0].data[15] == 39);
			CHECK(upload.surfaces[3].level == 3 && upload.surfaces[3].rows == 1 && upload.surfaces[3].row_bytes == 8 && upload.surfaces[3].data[0] == 0);
		}

		// errors ( IwiFile.cs:172-219 )
		iwi_file bad;
		auto wrong = file;
		wrong.put_u8(3, 7);
		CHECK(!parse_iwi(wrong.data, bad, &error));
		CHECK(error == "IWI version 7, expected 9");
		wrong = file;
		wrong.put_u8(3, 8);			// IW4's version 8 is accepted
		CHECK(parse_iwi(wrong.data, bad, &error));
		wrong = file;
		wrong.put_u8(0, 'X');
		CHECK(!parse_iwi(wrong.data, bad, &error));
		wrong = file;
		wrong.put_u8(8, 14);
		CHECK(!parse_iwi(wrong.data, bad, &error));
		wrong = file;
		wrong.put_u8(8, 0);
		CHECK(!parse_iwi(wrong.data, bad, &error));
		wrong = file;
		wrong.u8(0);				// one byte too many
		CHECK(!parse_iwi(wrong.data, bad, &error));
		CHECK(error == "IWI layout covers 72 bytes but the file has 73");
		wrong = file;
		wrong.data.pop_back();
		CHECK(!parse_iwi(wrong.data, bad, &error));
		wrong = file;
		wrong.put_u16(14, 2);		// depth 2
		CHECK(!parse_iwi(wrong.data, bad, &error));
		wrong = file;
		wrong.put_u16(10, 0);		// width 0
		CHECK(!parse_iwi(wrong.data, bad, &error));
		CHECK(!parse_iwi(byte_span(file.data.data(), 20), bad, &error));
	}
	{
		// a cube map without mips: 6 faces of 2x2 L8, face order as stored
		auto file = iwi_header_bytes(9, iwi_flags::MAPTYPE_CUBE | iwi_flags::NO_MIPMAPS, 4, 2, 2);
		for (int i = 0; i < 24; i++)
		{
			file.u8(static_cast<std::uint8_t>(10 * i));
		}
		iwi_file iwi;
		CHECK(parse_iwi(file.data, iwi, &error));
		CHECK(iwi.header.is_cube() && iwi.header.face_count() == 6 && iwi.header.mip_count() == 1);
		CHECK_EQ(iwi.surfaces.size(), 6u);
		CHECK(iwi.surface(0, 5) && iwi.surface(0, 5)->offset == 32 + 20);
		texture_upload upload;
		CHECK(build_texture_upload(iwi, nullptr, upload, &error));
		CHECK(upload.is_cube() && upload.d3d_format == d3d::FMT_L8 && upload.levels == 1 && upload.surfaces.size() == 6);
		const texture_surface* face3 = upload.surface(0, 3);
		CHECK(face3 && face3->rows == 2 && face3->row_bytes == 2 && face3->data.size() == 4 && face3->data[0] == 120);
		rgba_image rgba;
		CHECK(face3 && upload_surface_to_rgba(upload, *face3, rgba, &error));
		CHECK(pixel(rgba, 1, 1, 150, 150, 150, 255));
	}

	// ---- plain formats -> RGBA ( TextureDecoder.cs:54-122 ) -------------------------------------------------
	{
		rgba_image image;
		const std::uint8_t bgra[] = { 10, 20, 30, 40, 50, 60, 70, 80 };
		CHECK(decode_surface(iwi_format::rgba32, bgra, 2, 1, image, &error));
		CHECK(image.width == 2 && image.height == 1 && image.pixels.size() == 8);
		CHECK(pixel(image, 0, 0, 30, 20, 10, 40) && pixel(image, 1, 0, 70, 60, 50, 80));
		const std::uint8_t bgr[] = { 10, 20, 30, 50, 60, 70 };
		CHECK(decode_surface(iwi_format::rgb24, bgr, 1, 2, image, &error));
		CHECK(pixel(image, 0, 0, 30, 20, 10, 255) && pixel(image, 0, 1, 70, 60, 50, 255));
		const std::uint8_t la[] = { 100, 200, 5, 6 };
		CHECK(decode_surface(iwi_format::luminance_alpha8, la, 2, 1, image, &error));
		CHECK(pixel(image, 0, 0, 100, 100, 100, 200) && pixel(image, 1, 0, 5, 5, 5, 6));
		CHECK(decode_surface(iwi_format::luminance8, la, 2, 2, image, &error));
		CHECK(pixel(image, 1, 0, 200, 200, 200, 255) && pixel(image, 1, 1, 6, 6, 6, 255));
		CHECK(decode_surface(iwi_format::alpha8, la, 2, 2, image, &error));
		CHECK(pixel(image, 0, 0, 255, 255, 255, 100) && pixel(image, 0, 1, 255, 255, 255, 5));
		// too little data, wavelet formats
		CHECK(!decode_surface(iwi_format::rgba32, la, 2, 1, image, &error));
		CHECK(!decode_surface(iwi_format::wavelet_rgba, bgra, 1, 1, image, &error));

		CHECK(iwi_format_from_d3d(21) == iwi_format::rgba32);
		CHECK(iwi_format_from_d3d(28) == iwi_format::alpha8);
		CHECK(iwi_format_from_d3d(50) == iwi_format::luminance8);
		CHECK(iwi_format_from_d3d(51) == iwi_format::luminance_alpha8);
		CHECK(iwi_format_from_d3d(0x31545844) == iwi_format::dxt1);
		CHECK(iwi_format_from_d3d(0x33545844) == iwi_format::dxt3);
		CHECK(iwi_format_from_d3d(0x35545844) == iwi_format::dxt5);
		CHECK(!iwi_format_from_d3d(22).has_value());		// X8R8G8B8 is handled by the callers
		CHECK(iwi_game_path(",me_wall_col") == "images/me_wall_col.iwi");
	}

	// ---- DXT ( TextureDecoder.cs:152-271 ), every expectation computed by hand ------------------------------
	{
		rgba_image image;
		// c0 = 0xF800 ( 255, 0, 0 ) > c1 = 0x001F ( 0, 0, 255 ): four colours,
		// c2 = ( 2 c0 + c1 + 1 ) / 3 = ( 170, 0, 85 ), c3 = ( c0 + 2 c1 + 1 ) / 3 = ( 85, 0, 170 ).
		// indices: row 0 = 0 1 2 3, row 1 = 3 2 1 0, rows 2 and 3 = 0
		const std::uint8_t four[] = { 0x00, 0xF8, 0x1F, 0x00, 0xE4, 0x1B, 0x00, 0x00 };
		CHECK(decode_surface(iwi_format::dxt1, four, 4, 4, image, &error));
		CHECK(pixel(image, 0, 0, 255, 0, 0, 255) && pixel(image, 1, 0, 0, 0, 255, 255) && pixel(image, 2, 0, 170, 0, 85, 255)
			&& pixel(image, 3, 0, 85, 0, 170, 255));
		CHECK(pixel(image, 0, 1, 85, 0, 170, 255) && pixel(image, 3, 1, 255, 0, 0, 255) && pixel(image, 2, 3, 255, 0, 0, 255));

		// c0 = 0x001F <= c1 = 0xF800: three colours, c2 = ( c0 + c1 ) / 2 = ( 127, 0, 127 ), index 3 = transparent black
		const std::uint8_t three[] = { 0x1F, 0x00, 0x00, 0xF8, 0xE4, 0x1B, 0x00, 0x00 };
		CHECK(decode_surface(iwi_format::dxt1, three, 4, 4, image, &error));
		CHECK(pixel(image, 0, 0, 0, 0, 255, 255) && pixel(image, 1, 0, 255, 0, 0, 255) && pixel(image, 2, 0, 127, 0, 127, 255)
			&& pixel(image, 3, 0, 0, 0, 0, 0));

		// 565 expansion: 0x8410 = r 16, g 32, b 16 -> ( 16 << 3 | 16 >> 2, 32 << 2 | 32 >> 4, .. ) = ( 132, 130, 132 )
		const std::uint8_t grey[] = { 0x10, 0x84, 0x10, 0x84, 0x00, 0x00, 0x00, 0x00 };
		CHECK(decode_surface(iwi_format::dxt1, grey, 4, 4, image, &error));
		CHECK(pixel(image, 2, 2, 132, 130, 132, 255));

		// a 2x3 surface is one block, only the texels inside the surface are written
		CHECK(decode_surface(iwi_format::dxt1, four, 2, 3, image, &error));
		CHECK(image.pixels.size() == 24 && pixel(image, 1, 0, 0, 0, 255, 255) && pixel(image, 1, 1, 170, 0, 85, 255) && pixel(image, 0, 2, 255, 0, 0, 255));

		// DXT3: explicit 4 bit alpha ( x 17 ), colour always in four colour mode even though c0 <= c1
		const std::uint8_t dxt3[] = { 0xA0, 0xF5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0xF8, 0xE4, 0x1B, 0x00, 0x00 };
		CHECK(decode_surface(iwi_format::dxt3, dxt3, 4, 4, image, &error));
		CHECK(pixel(image, 0, 0, 0, 0, 255, 0) && pixel(image, 1, 0, 255, 0, 0, 170) && pixel(image, 2, 0, 85, 0, 170, 85)
			&& pixel(image, 3, 0, 170, 0, 85, 255));

		// DXT5, a0 = 255 > a1 = 0: eight alphas, alpha[ 1 + k ] = ( ( 7 - k ) a0 + k a1 + 3 ) / 7 = 219, 182, 146, 109, 73, 36;
		// alpha indices of row 0: 0, 1, 2, 7 -> 255, 0, 219, 36
		const std::uint8_t dxt5_eight[] = { 0xFF, 0x00, 0x88, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
		CHECK(decode_surface(iwi_format::dxt5, dxt5_eight, 4, 4, image, &error));
		CHECK(pixel(image, 0, 0, 255, 0, 0, 255) && pixel(image, 1, 0, 255, 0, 0, 0) && pixel(image, 2, 0, 255, 0, 0, 219)
			&& pixel(image, 3, 0, 255, 0, 0, 36));
		// a0 = 0 <= a1 = 255: six alphas, alpha[ 1 + k ] = ( ( 5 - k ) a0 + k a1 + 2 ) / 5 = 51, 102, 153, 204; 6 -> 0, 7 -> 255;
		// alpha indices of row 0: 2, 5, 6, 7
		const std::uint8_t dxt5_six[] = { 0x00, 0xFF, 0xAA, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 };
		CHECK(decode_surface(iwi_format::dxt5, dxt5_six, 4, 4, image, &error));
		CHECK(pixel(image, 0, 0, 255, 0, 0, 51) && pixel(image, 1, 0, 255, 0, 0, 204) && pixel(image, 2, 0, 255, 0, 0, 0)
			&& pixel(image, 3, 0, 255, 0, 0, 255));
		CHECK(!decode_surface(iwi_format::dxt5, four, 4, 4, image, &error));		// 8 bytes are not a DXT5 block
	}

	// ---- R8G8B8 is expanded for the GPU, A8R8G8B8 / A8L8 go as stored ---------------------------------------
	{
		auto file = iwi_header_bytes(9, iwi_flags::NO_MIPMAPS, 2, 2, 1);
		file.u8(1).u8(2).u8(3).u8(4).u8(5).u8(6);
		iwi_file iwi;
		CHECK(parse_iwi(file.data, iwi, &error));
		texture_upload upload;
		CHECK(build_texture_upload(iwi, nullptr, upload, &error));
		CHECK(upload.d3d_format == d3d::FMT_X8R8G8B8 && upload.surfaces.size() == 1);
		const std::uint8_t expected[] = { 1, 2, 3, 255, 4, 5, 6, 255 };
		CHECK(upload.surfaces.size() == 1 && upload.surfaces[0].data.size() == 8 && std::memcmp(upload.surfaces[0].data.data(), expected, 8) == 0);
		CHECK(upload.surfaces.size() == 1 && upload.surfaces[0].rows == 1 && upload.surfaces[0].row_bytes == 8);
		rgba_image rgba;
		CHECK(upload_surface_to_rgba(upload, upload.surfaces[0], rgba, &error));
		CHECK(pixel(rgba, 0, 0, 3, 2, 1, 255) && pixel(rgba, 1, 0, 6, 5, 4, 255));

		auto la = iwi_header_bytes(9, 0, 3, 2, 2);		// A8L8 with mips: 1x1 ( 2 bytes ) then 2x2 ( 8 bytes )
		for (int i = 0; i < 10; i++)
		{
			la.u8(static_cast<std::uint8_t>(i));
		}
		CHECK(parse_iwi(la.data, iwi, &error));
		CHECK(build_texture_upload(iwi, nullptr, upload, &error));
		CHECK(upload.d3d_format == d3d::FMT_A8L8 && upload.levels == 2 && upload.surfaces.size() == 2);
		CHECK(upload.surfaces[0].level == 0 && upload.surfaces[0].row_bytes == 4 && upload.surfaces[0].rows == 2 && upload.surfaces[0].data[0] == 2);
		CHECK(upload.surfaces[1].level == 1 && upload.surfaces[1].data.size() == 2 && upload.surfaces[1].data[1] == 1);
	}

	// ---- wavelet ( WaveletDecoder.cs ) ----------------------------------------------------------------------
	{
		// primary and secondary are unused by a one channel image; alpha: bit 0 -> 0, bit 1 -> 2
		const auto primary = table(0, 0), secondary = table(0, 0), alpha = table(0, 2);
		auto tables = wavelet_tables::from_memory(primary, secondary, alpha, &error);
		CHECK(tables != nullptr);

		// a table with a bit length outside 1 .. 12 is rejected ( not the 1.1.1.13 executable )
		auto broken = alpha;
		broken[4 * 100 + 2] = 13;
		CHECK(wavelet_tables::from_memory(primary, secondary, broken, &error) == nullptr);
		broken[4 * 100 + 2] = 0;
		CHECK(wavelet_tables::from_memory(primary, secondary, broken, &error) == nullptr);
		CHECK(wavelet_tables::from_memory(byte_span(primary.data(), 100), secondary, alpha, &error) == nullptr);
		CHECK(wavelet_tables::load_from_executable("Z:\\does\\not\\exist.exe", &error) == nullptr);

		if (tables)
		{
			// wavelet luminance ( format 9 ) 2x2: level 1 is one raw byte ( 100 ); level 0 is a bit stream, LSB first:
			//   refine 0, lsb 1, g1 '1' -> 2, g2 '0' -> 0, g3 '1' -> 2       = 0b10110 = 0x16
			//   p2 = 200: tl = 1 + ( 204 >> 1 ) = 103, tr = ( 200 + 2 - 2 - 0 ) >> 1 = 100,
			//             bl = ( 200 + 0 - 2 - 2 ) >> 1 = 98, br = ( 200 + 2 - 0 - 2 ) >> 1 = 100
			auto file = iwi_header_bytes(9, 0, 9, 2, 2);
			file.u8(100).u8(0x16).u8(0);
			iwi_file iwi;
			CHECK(parse_iwi(file.data, iwi, &error));
			CHECK(iwi.header.is_wavelet() && iwi.surfaces.empty());
			std::vector<wavelet_level> levels;
			CHECK(decode_wavelet(iwi, *tables, levels, &error));
			CHECK_EQ(levels.size(), 2u);
			if (levels.size() == 2)
			{
				CHECK(levels[0].level == 1 && levels[0].width == 1 && levels[0].data.size() == 1 && levels[0].data[0] == 100);
				const std::uint8_t expected[] = { 103, 100, 98, 100 };
				CHECK(levels[1].level == 0 && levels[1].width == 2 && levels[1].height == 2 && levels[1].data.size() == 4
					&& std::memcmp(levels[1].data.data(), expected, 4) == 0);
			}
			texture_upload upload;
			CHECK(build_texture_upload(iwi, tables.get(), upload, &error));
			CHECK(upload.d3d_format == d3d::FMT_L8 && upload.levels == 2 && upload.surfaces.size() == 2 && upload.surfaces[0].level == 0);
			CHECK(upload.surfaces[0].data[0] == 103 && upload.surfaces[0].rows == 2 && upload.surfaces[0].row_bytes == 2);
			CHECK(!build_texture_upload(iwi, nullptr, upload, &error));		// no tables

			// the same with the refine bit: the lower level's pixel is corrected by a Huffman delta first
			//   refine 1, delta '1' -> +2 ( low = 102 ), lsb 0, g1 '0', g2 '1' -> 2, g3 '0'    = 0b010011 = 0x13
			//   p2 = 204: tl = 0 + ( 206 >> 1 ) = 103, tr = ( 204 + 0 - 0 - 2 ) >> 1 = 101,
			//             bl = ( 204 + 2 - 0 - 0 ) >> 1 = 103, br = ( 204 + 0 - 2 - 0 ) >> 1 = 101
			file = iwi_header_bytes(9, 0, 10, 2, 2);		// wavelet alpha -> A8
			file.u8(100).u8(0x13).u8(0);
			CHECK(parse_iwi(file.data, iwi, &error));
			CHECK(decode_wavelet(iwi, *tables, levels, &error));
			if (levels.size() == 2)
			{
				const std::uint8_t expected[] = { 103, 101, 103, 101 };
				CHECK(levels[0].data[0] == 100);		// the stored lower level itself is not changed
				CHECK(levels[1].data.size() == 4 && std::memcmp(levels[1].data.data(), expected, 4) == 0);
			}
			CHECK(build_texture_upload(iwi, tables.get(), upload, &error));
			CHECK_EQ(upload.d3d_format, d3d::FMT_A8);

			// escape code: a table value of -32768 reads `escape bits` more bits and subtracts the bias
			//   alpha table: bit 0 -> 0, bit 1 -> escape ( 9 bits, bias 255 )
			//   refine 0, lsb 0, g1 = '1' + 9 bits 0x1FE ( 510 - 255 = 255 ), g2 '0', g3 '0'
			//   bits ( LSB first ): 0 0 1 | 0 1 1 1 1 1 1 1 1 | 0 0  -> byte 0 = 0b11110100 = 0xF4, byte 1 = 0b00001111 = 0x0F
			//   p2 = 0 ( low 0 ): tl = 255 >> 1 = 127, tr = 127, bl = ( -255 ) >> 1 = -128 -> 128, br = 128
			const auto escape = table(0, -32768);
			auto escape_tables = wavelet_tables::from_memory(primary, secondary, escape, &error);
			CHECK(escape_tables != nullptr);
			if (escape_tables)
			{
				file = iwi_header_bytes(9, 0, 9, 2, 2);
				file.u8(0).u8(0xF4).u8(0x0F);
				CHECK(parse_iwi(file.data, iwi, &error));
				CHECK(decode_wavelet(iwi, *escape_tables, levels, &error));
				if (levels.size() == 2)
				{
					const std::uint8_t expected[] = { 127, 127, 128, 128 };
					CHECK(levels[1].data.size() == 4 && std::memcmp(levels[1].data.data(), expected, 4) == 0);
				}
			}

			// wavelet RGB ( format 7 ) 2x2: 4 bytes per pixel B G R + alpha 255; raw level then
			//   refine 0; channel 0: lsb, d1 d2 d3 from the primary table; channels 1 and 2: bit, e = d + secondary
			// with all-zero streams every channel repeats the lower level
			const auto zero_tables = wavelet_tables::from_memory(primary, secondary, primary, &error);
			file = iwi_header_bytes(9, 0, 7, 2, 2);
			file.u8(10).u8(20).u8(30).u8(0).u8(0).u8(0).u8(0);
			CHECK(parse_iwi(file.data, iwi, &error));
			CHECK(zero_tables && decode_wavelet(iwi, *zero_tables, levels, &error));
			if (levels.size() == 2)
			{
				const std::uint8_t first[] = { 10, 20, 30, 255 };
				CHECK(levels[0].data.size() == 4 && std::memcmp(levels[0].data.data(), first, 4) == 0);
				CHECK(levels[1].data.size() == 16 && std::memcmp(levels[1].data.data() + 12, first, 4) == 0);
			}
			CHECK(zero_tables && build_texture_upload(iwi, zero_tables.get(), upload, &error));
			CHECK_EQ(upload.d3d_format, d3d::FMT_X8R8G8B8);
			rgba_image rgba;
			CHECK(upload_surface_to_rgba(upload, upload.surfaces[0], rgba, &error));
			CHECK(pixel(rgba, 1, 1, 30, 20, 10, 255));

			// a file that ends inside the raw level
			file = iwi_header_bytes(9, 0, 7, 2, 2);
			file.u8(10).u8(20);
			CHECK(parse_iwi(file.data, iwi, &error));
			CHECK(zero_tables && !decode_wavelet(iwi, *zero_tables, levels, &error));
		}
	}

	// ---- embedded images ( TextureExtractor.cs:292-389 ) ----------------------------------------------------
	{
		// A8R8G8B8 4x4 with a full mip chain: 64 + 16 + 4 bytes, level 0 first
		std::vector<std::uint8_t> data(84);
		for (std::size_t i = 0; i < data.size(); i++)
		{
			data[i] = static_cast<std::uint8_t>(i);
		}
		embedded_image image;
		image.d3d_format = d3d::FMT_A8R8G8B8;
		image.level_count = 3;
		image.width = 4;
		image.height = 4;
		image.data_size = 84;
		texture_upload upload;
		CHECK(build_embedded_upload(image, data, upload, &error));
		CHECK(upload.levels == 3 && upload.faces == 1 && upload.surfaces.size() == 3);
		if (upload.surfaces.size() == 3)
		{
			CHECK(upload.surfaces[0].level == 0 && upload.surfaces[0].data.size() == 64 && upload.surfaces[0].data[0] == 0);
			CHECK(upload.surfaces[1].level == 1 && upload.surfaces[1].width == 2 && upload.surfaces[1].data.size() == 16 && upload.surfaces[1].data[0] == 64);
			CHECK(upload.surfaces[2].level == 2 && upload.surfaces[2].data.size() == 4 && upload.surfaces[2].data[0] == 80);
		}
		// only the top level stored: the level count of the load def decides ( flags say nothing about mips )
		image.level_count = 1;
		image.data_size = 64;
		CHECK(build_embedded_upload(image, data, upload, &error));
		CHECK(upload.levels == 1 && upload.surfaces.size() == 1);
		// a cube: map type 5, six faces of one level each, face major
		image.map_type = 5;
		image.width = 2;
		image.height = 2;
		image.d3d_format = d3d::FMT_L8;
		image.flags = iwi_flags::NO_MIPMAPS;
		image.data_size = 24;
		CHECK(build_embedded_upload(image, data, upload, &error));
		CHECK(upload.is_cube() && upload.surfaces.size() == 6 && upload.surface(0, 4) && upload.surface(0, 4)->data[0] == 16);
		// a size that matches no layout, an unknown format, missing data
		image.data_size = 25;
		CHECK(!build_embedded_upload(image, data, upload, &error));
		image.data_size = 24;
		image.d3d_format = 113;		// A16B16G16R16F
		CHECK(!build_embedded_upload(image, data, upload, &error));
		image.d3d_format = d3d::FMT_L8;
		CHECK(!build_embedded_upload(image, byte_span(data.data(), 10), upload, &error));
	}
}
