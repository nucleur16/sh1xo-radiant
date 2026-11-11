#include "test.hpp"

#include "codol_render/iwd.hpp"

#include <algorithm>
#include <filesystem>

using namespace codol::render;

namespace
{
	// raw deflate vectors produced with zlib ( python: zlib.compressobj( 9, DEFLATED, -15 ) ), as the reference

	// one dynamic Huffman block: 1920 bytes, crc32 0xF8C9B9B8, starts "projection sun projection projec"
	const std::uint8_t DYNAMIC_BLOCK[] =
	{
		0xED, 0x55, 0x3B, 0x4E, 0xC4, 0x30, 0x10, 0xA5, 0xA4, 0xA7, 0x80, 0x0A, 0x59, 0x5A, 0x21, 0xD1, 0x51, 0x21, 0x5A, 0x24, 0x4E, 0x40, 0x09, 0x9D,
		0x77, 0xD7, 0xBB, 0x09, 0x4A, 0xE2, 0x90, 0x78, 0x3F, 0xA2, 0x41, 0x82, 0x8A, 0x0A, 0x2A, 0x4E, 0x43, 0x8D, 0xA8, 0x69, 0xF6, 0x02, 0x5C, 0x01,
		0x51, 0xD0, 0x90, 0xCC, 0x8C, 0xC7, 0xE3, 0x24, 0x48, 0x1C, 0x80, 0x14, 0x56, 0x62, 0x7B, 0xDE, 0xCC, 0x7B, 0xF3, 0x49, 0x59, 0xD9, 0x2B, 0x33,
		0x71, 0xA9, 0x2D, 0x54, 0xBD, 0x28, 0x54, 0x19, 0x3E, 0xC5, 0xEB, 0xCC, 0xCE, 0x55, 0x5D, 0x5A, 0xA7, 0xCA, 0x74, 0x6D, 0x32, 0xB5, 0x34, 0x95,
		0x33, 0x6B, 0xD8, 0xCD, 0xB5, 0xAB, 0xD2, 0xB5, 0xAA, 0x13, 0x3D, 0xB5, 0x2B, 0x65, 0xF3, 0x22, 0xF5, 0xA7, 0xBA, 0x9A, 0x2F, 0x72, 0x53, 0x38,
		0x89, 0x93, 0xA5, 0xF3, 0xC4, 0xE5, 0xBA, 0x84, 0xFB, 0xA6, 0x52, 0xA5, 0xAE, 0x6B, 0x70, 0x4B, 0xF6, 0xCE, 0x4C, 0x92, 0x22, 0xBD, 0x5E, 0x18,
		0x44, 0x5A, 0xD9, 0x2A, 0x9B, 0x4A, 0x7B, 0xE9, 0x86, 0xDE, 0xC9, 0x1B, 0x6C, 0x35, 0xC1, 0x98, 0x2A, 0xD5, 0x19, 0x44, 0x06, 0xD8, 0x01, 0x30,
		0xBC, 0x21, 0x87, 0x5A, 0xE7, 0x65, 0xD6, 0x84, 0x10, 0x1B, 0x0A, 0x5F, 0x95, 0x99, 0x65, 0x7D, 0x25, 0x84, 0x0A, 0x20, 0x0A, 0xA1, 0x50, 0x30,
		0xCC, 0x8F, 0x64, 0x09, 0xB8, 0x6D, 0x30, 0x7D, 0x22, 0x13, 0x5B, 0xD4, 0x4E, 0x37, 0x1A, 0x2D, 0x53, 0xB3, 0x42, 0xFD, 0xDB, 0x8B, 0xBC, 0x2D,
		0x62, 0xF0, 0x9E, 0x5A, 0xAF, 0xD2, 0x8D, 0xC0, 0x87, 0xD8, 0x58, 0x76, 0x14, 0x8F, 0x3F, 0xD9, 0x86, 0x44, 0x6D, 0x0D, 0xD8, 0x3C, 0xE0, 0x00,
		0x33, 0x5C, 0x83, 0x62, 0xC4, 0x06, 0x2D, 0x71, 0x85, 0x80, 0x83, 0x57, 0xE2, 0x83, 0x86, 0x43, 0xE1, 0xC3, 0x55, 0x8E, 0x41, 0x1C, 0xB0, 0x6B,
		0x40, 0xE4, 0x70, 0xA9, 0x40, 0x3C, 0x6B, 0xDE, 0xE7, 0xEB, 0x22, 0x9F, 0x41, 0x56, 0x0A, 0x94, 0x6A, 0x02, 0xF4, 0x44, 0x42, 0x71, 0x29, 0x53,
		0x05, 0xA0, 0x0B, 0x94, 0x1E, 0xE3, 0xEF, 0x68, 0x26, 0xA9, 0x82, 0x09, 0x7B, 0x67, 0x8A, 0xC0, 0x7E, 0x20, 0x1F, 0xB2, 0x4F, 0x10, 0x46, 0x16,
		0x6D, 0x9C, 0xA9, 0x40, 0xC5, 0xB7, 0x45, 0x37, 0x68, 0x76, 0xC7, 0x0E, 0x00, 0x4D, 0xC8, 0x38, 0x94, 0x5E, 0xB1, 0x20, 0x61, 0x91, 0x29, 0x2F,
		0x6C, 0x0C, 0xD8, 0xB8, 0x1C, 0x1B, 0x5A, 0xFD, 0x86, 0x2C, 0xD9, 0x36, 0x23, 0x2D, 0x88, 0xEF, 0x7A, 0x0F, 0xD8, 0xEC, 0x93, 0x56, 0xA4, 0x3C,
		0x39, 0xC1, 0xA3, 0xD0, 0xF5, 0xB2, 0xE7, 0xE8, 0x43, 0x9F, 0x4F, 0x5F, 0xBF, 0x0E, 0x37, 0x7B, 0xC7, 0x27, 0x3B, 0x77, 0x37, 0x6F, 0xF7, 0x9B,
		0xEF, 0xE7, 0x8B, 0xD1, 0xA9, 0xFB, 0x3C, 0x78, 0xCC, 0x92, 0xED, 0xD1, 0xC3, 0xFB, 0xC7, 0xED, 0xFE, 0xEE, 0xE5, 0xD3, 0xCB, 0xD9, 0xD1, 0xD6,
		0xFF, 0xF3, 0xE7, 0x07, 0x13, 0xE8, 0x8B, 0x69, 0x70, 0xA4, 0xF1, 0x44, 0x97, 0x69, 0x8D, 0x3A, 0x9E, 0x27, 0x43, 0x9B, 0x69, 0x44, 0xE4, 0x44,
		0x0E, 0x5C, 0x94, 0xE5, 0xED, 0x87, 0x0B, 0x58, 0xFE, 0xD6, 0xF8, 0xF2, 0x17, 0xD0, 0x1B, 0x8F, 0xF8, 0x27, 0x1A, 0x9B, 0x7E, 0x6F, 0xC4, 0x63,
		0xB9, 0xAD, 0xD6, 0xAE, 0x33, 0xE8, 0xD7, 0x2E, 0x01, 0x5F, 0xB6, 0x22, 0x58, 0x39, 0xCE, 0x06, 0x8E, 0x3B, 0x2D, 0x44, 0xD4, 0xB8, 0x69, 0xE3,
		0xD1, 0x2D, 0xA2, 0x12, 0x80, 0xC3, 0x3F, 0xD3, 0x98, 0xE7, 0x0F,
	};
	constexpr std::size_t DYNAMIC_SIZE = 1920;
	constexpr std::uint32_t DYNAMIC_CRC = 0xF8C9B9B8;

	// one fixed Huffman block with matches: 33 x 'a' + "bcabcabc"
	const std::uint8_t FIXED_BLOCK[] = { 0x4B, 0x4C, 0x24, 0x00, 0x92, 0x92, 0x21, 0x08, 0x00 };

	// fixed Huffman, 465 bytes, crc32 0x5998C4C6
	const std::uint8_t FIXED_TEXT[] =
	{
		0xF3, 0x4D, 0x2C, 0x49, 0x2D, 0xCA, 0x4C, 0xCC, 0x09, 0x49, 0x4D, 0xCE, 0xC8, 0xCB, 0x2C, 0x2C, 0x4D, 0x0D, 0x4E, 0x2D, 0x51, 0x28, 0x81, 0x71,
		0x8A, 0x15, 0x0A, 0x12, 0x8B, 0x8B, 0x81, 0x54, 0x62, 0x51, 0x7A, 0x69, 0x6E, 0x6A, 0x5E, 0x49, 0xB1, 0xB5, 0x82, 0xEF, 0x88, 0xD5, 0x51, 0x92,
		0x91, 0xAA, 0x50, 0x58, 0x9A, 0x99, 0x9C, 0xAD, 0x90, 0x54, 0x94, 0x5F, 0x9E, 0xA7, 0x90, 0x96, 0x5F, 0xA1, 0x90, 0x55, 0x9A, 0x5B, 0x50, 0xAC,
		0x90, 0x5F, 0x96, 0x5A, 0x04, 0x96, 0xCE, 0x49, 0xAC, 0xAA, 0x54, 0x48, 0xC9, 0x4F, 0x57, 0x30, 0x30, 0x34, 0x32, 0x36, 0x31, 0x35, 0x33, 0xB7,
		0xB0, 0xA4, 0xB3, 0x36, 0x00,
	};

	byte_span text(const char* s) { return { reinterpret_cast<const std::uint8_t*>(s), std::strlen(s) }; }

	std::vector<std::uint8_t> unhex(const char* hex)
	{
		std::vector<std::uint8_t> out;
		for (; hex[0] && hex[1]; hex += 2)
		{
			const auto nibble = [](const char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
			out.push_back(static_cast<std::uint8_t>((nibble(hex[0]) << 4) | nibble(hex[1])));
		}
		return out;
	}

	// a zip writer for the archive test: entry names in the central directory and entry data are encrypted the way
	// IwdArchive.cs decrypts them
	struct zip_entry
	{
		std::string name;
		std::uint16_t method = 0;
		std::vector<std::uint8_t> stored;		// as it goes into the file ( deflated for method 8 ), plain
		std::uint32_t crc = 0;
		std::uint32_t uncompressed_size = 0;
	};

	std::vector<std::uint8_t> build_iwd(const std::vector<zip_entry>& entries, const std::string& file_name, const iwd_key* key, const std::size_t junk_before)
	{
		const std::uint32_t hash = crc32(text(file_name.c_str()));
		const auto iv = [](const std::uint32_t a, const std::uint32_t b, const std::uint32_t c, const std::uint32_t d)
		{
			test::bytes v;
			v.u32(a).u32(b).u32(c).u32(d);
			return v.data;
		};
		test::bytes zip;
		zip.zeros(junk_before);		// bytes in front of the zip ( IwdArchive.cs "bytesBefore" ); offsets in the zip do not count them
		std::vector<std::uint32_t> offsets;
		for (const auto& e : entries)
		{
			offsets.push_back(static_cast<std::uint32_t>(zip.size() - junk_before));
			zip.u32(0x04034B50).u16(20).u16(0).u16(e.method).u16(0).u16(0).u32(e.crc).u32(static_cast<std::uint32_t>(e.stored.size())).u32(e.uncompressed_size);
			zip.u16(static_cast<std::uint16_t>(e.name.size())).u16(4);
			zip.raw(e.name.data(), e.name.size()).u32(0xAABBCCDD);		// name ( ignored by the reader ) and a 4 byte extra field
			const std::uint32_t data_pos = static_cast<std::uint32_t>(zip.size() - junk_before);
			std::vector<std::uint8_t> data = e.stored;
			if (key)
			{
				for (std::size_t k = 0; k < data.size(); k += 0x4000)
				{
					const auto counter = iv(data_pos + static_cast<std::uint32_t>(k), hash, 0x01010101, 0x01010101);
					aes_ctr_transform(*key, counter.data(), data.data() + k, std::min<std::size_t>(0x4000, data.size() - k));
				}
			}
			zip.raw(data.data(), data.size());
		}
		const std::uint32_t cd_offset = static_cast<std::uint32_t>(zip.size() - junk_before);
		for (std::size_t i = 0; i < entries.size(); i++)
		{
			const auto& e = entries[i];
			zip.u32(0x02014B50).u16(20).u16(20).u16(0).u16(e.method).u16(0).u16(0).u32(e.crc).u32(static_cast<std::uint32_t>(e.stored.size())).u32(e.uncompressed_size);
			zip.u16(static_cast<std::uint16_t>(e.name.size())).u16(2).u16(3).u16(0).u16(0).u32(0).u32(offsets[i]);
			std::vector<std::uint8_t> name(e.name.begin(), e.name.end());
			if (key)
			{
				const auto counter = iv(0x01010101, hash, static_cast<std::uint32_t>(i), 0x01010101);
				aes_ctr_transform(*key, counter.data(), name.data(), name.size());
			}
			zip.raw(name.data(), name.size()).u16(0x1234).raw("abc", 3);		// extra field and comment
		}
		const std::uint32_t cd_size = static_cast<std::uint32_t>(zip.size() - junk_before) - cd_offset;
		zip.u32(0x06054B50).u16(0).u16(0).u16(static_cast<std::uint16_t>(entries.size())).u16(static_cast<std::uint16_t>(entries.size()));
		zip.u32(cd_size).u32(cd_offset).u16(5).raw("hello", 5);				// with an archive comment
		return zip.data;
	}

	bool write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& data)
	{
		std::FILE* f = _wfopen(path.c_str(), L"wb");
		if (!f)
		{
			return false;
		}
		const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
		std::fclose(f);
		return ok;
	}
}

void test_iwd()
{
	// ---- crc32 ( Crc32.cs ) ---------------------------------------------------------------------------------
	CHECK_EQ(crc32(text("123456789")), 0xCBF43926u);
	CHECK_EQ(crc32(text("")), 0u);
	CHECK_EQ(crc32(text("iw_image_00.iwd")), 0xAFDC4E57u);
	CHECK_EQ(crc32(text("6789"), crc32(text("12345"))), 0xCBF43926u);		// continued

	// ---- inflate ----------------------------------------------------------------------------------------------
	{
		std::vector<std::uint8_t> out;
		std::string error;

		// stored block: BFINAL 1, BTYPE 00, LEN 5, NLEN ~5
		const std::uint8_t stored[] = { 0x01, 0x05, 0x00, 0xFA, 0xFF, 'h', 'e', 'l', 'l', 'o' };
		CHECK(inflate_raw(stored, 5, out, &error));
		CHECK(out.size() == 5 && std::memcmp(out.data(), "hello", 5) == 0);

		// fixed block, a single literal: 'a'
		const std::uint8_t single[] = { 0x4B, 0x04, 0x00 };
		CHECK(inflate_raw(single, 1, out, &error));
		CHECK(out.size() == 1 && out[0] == 'a');

		// fixed block with overlapping matches
		CHECK(inflate_raw(FIXED_BLOCK, 41, out, &error));
		CHECK(out.size() == 41 && std::count(out.begin(), out.begin() + 33, 'a') == 33 && std::memcmp(out.data() + 33, "bcabcabc", 8) == 0);

		CHECK(inflate_raw(FIXED_TEXT, 465, out, &error));
		CHECK_EQ(crc32(out), 0x5998C4C6u);
		CHECK(std::memcmp(out.data(), "MaterialTechniqueSet techniques passes arguments; Mat", 52) == 0);

		// dynamic block
		CHECK(inflate_raw(DYNAMIC_BLOCK, DYNAMIC_SIZE, out, &error));
		CHECK_EQ(out.size(), DYNAMIC_SIZE);
		CHECK_EQ(crc32(out), DYNAMIC_CRC);
		CHECK(std::memcmp(out.data(), "projection sun projection projec", 32) == 0);

		// the size must match exactly
		CHECK(!inflate_raw(DYNAMIC_BLOCK, DYNAMIC_SIZE - 1, out, &error));
		CHECK(!inflate_raw(DYNAMIC_BLOCK, DYNAMIC_SIZE + 1, out, &error));
		// truncated input
		CHECK(!inflate_raw(byte_span(DYNAMIC_BLOCK, sizeof DYNAMIC_BLOCK / 2), DYNAMIC_SIZE, out, &error));
		CHECK(!inflate_raw(byte_span(), 0, out, &error));
		// a prefix from truncated input ( IwdArchive.cs ReadPrefix )
		CHECK(inflate_raw(byte_span(DYNAMIC_BLOCK, 200), 32, out, &error, true));
		CHECK(out.size() == 32 && std::memcmp(out.data(), "projection sun projection projec", 32) == 0);
		CHECK(inflate_raw(DYNAMIC_BLOCK, 32, out, &error, true));
		CHECK_EQ(out.size(), 32u);
		CHECK(!inflate_raw(byte_span(DYNAMIC_BLOCK, 40), 1000, out, &error, true));
		// garbage: block type 3, a bad stored length, a distance before the start of the data
		const std::uint8_t reserved[] = { 0x07, 0x00 };
		CHECK(!inflate_raw(reserved, 1, out, &error));
		const std::uint8_t bad_stored[] = { 0x01, 0x05, 0x00, 0x00, 0x00, 'h', 'e', 'l', 'l', 'o' };
		CHECK(!inflate_raw(bad_stored, 5, out, &error));
		const std::uint8_t bad_distance[] = { 0x03, 0x02, 0x00 };		// fixed block: length 3, distance 1 with no output yet
		CHECK(!inflate_raw(bad_distance, 3, out, &error));
	}

	// ---- AES-192 CTR with a big endian counter ( LibTomCryptCtr.cs ): NIST SP 800-38A F.5.3 -----------------
	iwd_key nist{};
	{
		const auto key = unhex("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b");
		std::copy(key.begin(), key.end(), nist.begin());
		const auto counter = unhex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
		auto data = unhex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
		const auto expected = unhex("1abc932417521ca24f2b0459fe7e6e0b090339ec0aa6faefd5ccc2c6f4ce8e941e36b26bd1ebc670d1bd1d665620abf74f78a7f6d29809585a97daec58c6b050");
		std::string error;
		CHECK(aes_ctr_transform(nist, counter.data(), data.data(), data.size(), &error));
		CHECK(data == expected);
		// a partial last block uses the start of its keystream block; the transform is its own inverse
		auto partial = unhex("6bc1bee22e409f96e93d7e117393172aae2d8a");
		CHECK(aes_ctr_transform(nist, counter.data(), partial.data(), partial.size()));
		CHECK(std::equal(partial.begin(), partial.end(), expected.begin()));
		CHECK(aes_ctr_transform(nist, counter.data(), partial.data(), partial.size()));
		CHECK(partial == unhex("6bc1bee22e409f96e93d7e117393172aae2d8a"));
		// the counter carries across bytes as a big endian number
		const auto wrap = unhex("000000000000000000000000ffffffff");
		const auto next = unhex("00000000000000000000000100000000");
		std::vector<std::uint8_t> two(32, 0), second(16, 0);
		CHECK(aes_ctr_transform(nist, wrap.data(), two.data(), two.size()));
		CHECK(aes_ctr_transform(nist, next.data(), second.data(), second.size()));
		CHECK(std::equal(second.begin(), second.end(), two.begin() + 16));
	}

	// ---- an archive, encrypted and plain ---------------------------------------------------------------------
	{
		std::vector<zip_entry> entries;
		// stored, three cipher chunks
		zip_entry big;
		big.name = "images/big_stored.iwi";
		big.method = 0;
		big.stored.resize(40000);
		for (std::size_t i = 0; i < big.stored.size(); i++)
		{
			big.stored[i] = static_cast<std::uint8_t>((i * 31) ^ (i >> 7));
		}
		big.crc = crc32(big.stored);
		big.uncompressed_size = 40000;
		entries.push_back(big);
		// deflated
		zip_entry text_entry;
		text_entry.name = "images/deflated.iwi";
		text_entry.method = 8;
		text_entry.stored.assign(std::begin(DYNAMIC_BLOCK), std::end(DYNAMIC_BLOCK));
		text_entry.crc = DYNAMIC_CRC;
		text_entry.uncompressed_size = static_cast<std::uint32_t>(DYNAMIC_SIZE);
		entries.push_back(text_entry);
		// empty
		zip_entry empty;
		empty.name = "empty";
		entries.push_back(empty);
		// a wrong CRC
		zip_entry broken = text_entry;
		broken.name = "broken";
		broken.crc ^= 1;
		entries.push_back(broken);

		std::error_code ec;
		const auto dir = std::filesystem::temp_directory_path(ec) / "codol_render_work";
		std::filesystem::create_directories(dir, ec);

		for (int pass = 0; pass < 3; pass++)
		{
			const bool encrypted = pass != 1;
			const std::size_t junk = pass == 2 ? 77 : 0;
			const std::string file_name = pass == 1 ? "codol_render_test_plain.iwd" : "codol_render_test_00.iwd";
			const auto path = dir / file_name;
			CHECK(write_file(path, build_iwd(entries, file_name, encrypted ? &nist : nullptr, junk)));

			std::string error;
			auto archive = iwd_archive::open(path, encrypted ? &nist : nullptr, &error);
			CHECK(archive != nullptr);
			if (!archive)
			{
				std::printf("  %s\n", error.c_str());
				continue;
			}
			CHECK(archive->file_name() == file_name);
			CHECK_EQ(archive->name_hash(), crc32(text(file_name.c_str())));
			CHECK_EQ(archive->entries().size(), 4u);
			const iwd_entry* e0 = archive->find("images/big_stored.iwi");
			const iwd_entry* e1 = archive->find("images/deflated.iwi");
			const iwd_entry* e2 = archive->find("empty");
			const iwd_entry* e3 = archive->find("broken");
			CHECK(e0 && e1 && e2 && e3);
			CHECK(archive->find("images/BIG_STORED.iwi") == nullptr);
			if (!(e0 && e1 && e2 && e3))
			{
				continue;
			}
			CHECK(e0->index == 0 && e0->method == 0 && e0->uncompressed_size == 40000u && e0->compressed_size == 40000u);
			CHECK(e1->index == 1 && e1->method == 8 && e1->uncompressed_size == DYNAMIC_SIZE && e1->compressed_size == sizeof DYNAMIC_BLOCK);
			CHECK_EQ(e1->crc32, DYNAMIC_CRC);

			std::vector<std::uint8_t> data;
			CHECK(archive->read(*e0, data, &error));
			CHECK(data == big.stored);
			CHECK(archive->read(*e1, data, &error));
			CHECK(data.size() == DYNAMIC_SIZE && crc32(data) == DYNAMIC_CRC);
			CHECK(archive->read(*e2, data, &error));
			CHECK(data.empty());
			CHECK(!archive->read(*e3, data, &error));				// CRC mismatch
			CHECK(archive->read(*e3, data, &error, false));			// unless it is not checked
			CHECK_EQ(data.size(), DYNAMIC_SIZE);

			// prefixes ( the 32 byte IWI header )
			CHECK(archive->read_prefix(*e0, 32, data, &error));
			CHECK(data.size() == 32 && std::equal(data.begin(), data.end(), big.stored.begin()));
			CHECK(archive->read_prefix(*e1, 32, data, &error));
			CHECK(data.size() == 32 && std::memcmp(data.data(), "projection sun projection projec", 32) == 0);
			CHECK(archive->read_prefix(*e1, 100000, data, &error));		// longer than the entry: the whole entry
			CHECK_EQ(data.size(), DYNAMIC_SIZE);
			CHECK(archive->read_prefix(*e2, 32, data, &error));
			CHECK(data.empty());

			archive.reset();		// closes the file
			if (pass == 0)
			{
				// the file name is part of the cipher: the same bytes under another name do not decrypt
				const auto renamed = dir / "codol_render_test_01.iwd";
				std::filesystem::copy_file(path, renamed, std::filesystem::copy_options::overwrite_existing, ec);
				auto other = iwd_archive::open(renamed, &nist, &error);
				CHECK(other != nullptr);
				if (other)
				{
					CHECK(other->find("images/big_stored.iwi") == nullptr);
					CHECK_EQ(other->entries().size(), 4u);
					CHECK(!other->read(other->entries()[0], data, &error));
				}
				other.reset();
				std::filesystem::remove(renamed, ec);
				// and without the key the names stay encrypted
				auto plain = iwd_archive::open(path, nullptr, &error);
				CHECK(plain && plain->find("empty") == nullptr);
			}
			std::filesystem::remove(path, ec);
		}

		// not a zip
		const auto junk_path = dir / "codol_render_test_junk.iwd";
		CHECK(write_file(junk_path, std::vector<std::uint8_t>(100, 0x55)));
		std::string error;
		CHECK(iwd_archive::open(junk_path, nullptr, &error) == nullptr);
		CHECK(!error.empty());
		std::filesystem::remove(junk_path, ec);
		CHECK(iwd_archive::open(dir / "does_not_exist.iwd", nullptr, &error) == nullptr);

		iwd_key key{};
		CHECK(!derive_iwd_key(dir / "does_not_exist.exe", key, &error));
	}
}
