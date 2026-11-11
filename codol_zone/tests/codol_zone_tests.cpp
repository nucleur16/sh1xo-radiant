// Unit tests of codol_zone. The first half needs nothing but the library; the second half runs against a real
// Call of Duty Online 1.1.1.13 install ( CODOL_ZONE_GAME_DIR, set by CMake or the environment ) and is skipped
// when it is not there. CODOL_ZONE_TEST_ALL=1 also loads every fast file of the install.

#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"
#include "crypto_win.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace codol;
using namespace codol::zone;

namespace
{
	int g_checks = 0;
	int g_failures = 0;

#define CHECK(cond) \
	do \
	{ \
		g_checks++; \
		if (!(cond)) \
		{ \
			g_failures++; \
			std::printf("  FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond); \
		} \
	} while (false)

#define CHECK_EQ(a, b) \
	do \
	{ \
		g_checks++; \
		if (static_cast<long long>(a) != static_cast<long long>(b)) \
		{ \
			g_failures++; \
			std::printf("  FAILED %s:%d  %s == %s  ( %lld vs %lld )\n", __FILE__, __LINE__, #a, #b, static_cast<long long>(a), static_cast<long long>(b)); \
		} \
	} while (false)

#include "zlib_vectors.inc"

	// same generator as gen_zlib_vectors.py
	std::string make_text(const std::size_t n)
	{
		static const char* words[] = { "alpha", "bravo", "charlie", "delta", "echo", "foxtrot", "golf", "hotel" };
		std::string out;
		std::uint32_t state = 12345;
		while (out.size() < n)
		{
			state = (state * 1103515245u + 12345u) & 0x7FFFFFFFu;
			out += words[(state >> 16) & 7];
			out += ((state >> 8) & 3) ? ' ' : '\n';
		}
		out.resize(n);
		return out;
	}

	std::string as_string(const std::vector<std::uint8_t>& v)
	{
		return std::string(v.begin(), v.end());
	}

	void put32(std::vector<std::uint8_t>& v, const std::uint32_t x)
	{
		for (int i = 0; i < 4; i++)
		{
			v.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
		}
	}

	void put_string(std::vector<std::uint8_t>& v, const char* s, const bool terminator = true)
	{
		v.insert(v.end(), s, s + std::strlen(s) + (terminator ? 1 : 0));
	}

	// ---- library only ---------------------------------------------------------------------------

	void test_inflate()
	{
		std::printf("inflate\n");
		std::vector<std::uint8_t> out;
		std::size_t consumed = 0;

		CHECK(zlib_inflate(ZLIB_FIXED, out, 0, &consumed));
		CHECK(as_string(out) == "hello hello hello hello");
		CHECK_EQ(consumed, sizeof(ZLIB_FIXED));

		CHECK(zlib_inflate(ZLIB_STORED, out));
		CHECK(as_string(out) == "stored block payload");

		CHECK(zlib_inflate(ZLIB_DYNAMIC, out, 16));	// a hint that is far too small must not matter
		CHECK(as_string(out) == make_text(6000));

		// corruption is detected: a flipped payload byte, a truncated stream, a bad header
		std::vector<std::uint8_t> bad(std::begin(ZLIB_DYNAMIC), std::end(ZLIB_DYNAMIC));
		bad[bad.size() / 2] ^= 0x40;
		CHECK(!zlib_inflate(bad, out));
		bad.assign(std::begin(ZLIB_DYNAMIC), std::end(ZLIB_DYNAMIC) - 40);
		CHECK(!zlib_inflate(bad, out));
		bad.assign(std::begin(ZLIB_FIXED), std::end(ZLIB_FIXED));
		bad[0] = 0x79;
		CHECK(!zlib_inflate(bad, out));
		CHECK(!zlib_inflate(std::span<const std::uint8_t>(), out));
	}

	void test_crypto()
	{
		std::printf("crypto\n");
		// FIPS 180-2: SHA-256( "abc" )
		static const std::uint8_t abc[] = { 'a', 'b', 'c' };
		static const std::uint8_t abcDigest[32] = {
			0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
			0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C, 0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD,
		};
		std::uint8_t digest[32] = {};
		CHECK(crypto::sha256(abc, digest));
		CHECK(!std::memcmp(digest, abcDigest, 32));

		// FIPS 197 C.2: AES-192, key 00 .. 17, block 00112233 .. ff. In CTR mode the keystream of the first block is
		// the encryption of the IV, so XORing zeros with IV = that block must give the FIPS cipher text.
		std::uint8_t key[24], iv[16];
		for (int i = 0; i < 24; i++)
		{
			key[i] = static_cast<std::uint8_t>(i);
		}
		for (int i = 0; i < 16; i++)
		{
			iv[i] = static_cast<std::uint8_t>(i * 0x11);
		}
		static const std::uint8_t expected[16] = { 0xDD, 0xA9, 0x7C, 0xA4, 0x86, 0x4C, 0xDF, 0xE0, 0x6E, 0xAF, 0x70, 0xA0, 0xEC, 0x0D, 0x71, 0x91 };
		crypto::AesCtr aes;
		CHECK(aes.init(key));
		std::vector<std::uint8_t> data(40, 0);
		CHECK(aes.transform(iv, data));
		CHECK(!std::memcmp(data.data(), expected, 16));

		// the counter is little endian: block 1 is the encryption of the IV with its first byte incremented
		std::uint8_t iv1[16];
		std::memcpy(iv1, iv, 16);
		iv1[0]++;
		std::vector<std::uint8_t> second(16, 0);
		CHECK(aes.transform(iv1, second));
		CHECK(!std::memcmp(data.data() + 16, second.data(), 16));

		// and the transform is its own inverse
		CHECK(aes.transform(iv, data));
		CHECK(std::all_of(data.begin(), data.end(), [](const std::uint8_t b) { return b == 0; }));

		// key handling
		static const std::uint8_t garbage[64] = { 0x30, 0x82, 0x04, 0xA3, 0x02, 0x01, 0x00 };
		crypto::RsaPrivateKey parsed;
		CHECK(!crypto::parse_pkcs1_private_key(garbage, parsed));
		ClientKey clientKey;
		CHECK(!clientKey.load_from_der(garbage));
		CHECK(!clientKey.valid());
		CHECK(!clientKey.load_from_exe("this/file/does/not/exist.exe"));
		std::vector<std::uint8_t> plain;
		CHECK(!clientKey.unwrap(garbage, plain));
	}

	void test_decoders()
	{
		std::printf("decoders\n");
		CHECK(half_to_float(0x0000) == 0.0f);
		CHECK(half_to_float(0x3C00) == 1.0f);
		CHECK(half_to_float(0xC000) == -2.0f);
		CHECK(half_to_float(0x3800) == 0.5f);
		CHECK(half_to_float(0x7BFF) == 65504.0f);
		CHECK(half_to_float(0x0001) > 0.0f && half_to_float(0x0001) < 1e-7f);
		CHECK(std::isinf(half_to_float(0x7C00)));

		float v[3];
		unpack_unit_vector(0x3F7F7FFEu, v);		// x 254, y 127, z 127, scale byte 63: ( 1, 0, 0 )
		CHECK(std::fabs(v[0] - 1.0f) < 1e-6f && v[1] == 0.0f && v[2] == 0.0f);
		unpack_unit_vector(0x3F007F7Fu, v);		// z 0: ( 0, 0, -1 )
		CHECK(v[0] == 0.0f && v[1] == 0.0f && std::fabs(v[2] + 1.0f) < 1e-6f);

		CHECK_EQ(Zone::alias_to_address((3u << 28 | 0x10u) + 1), 0x40000010u);
		CHECK_EQ(Zone::alias_to_address(1), 0x10000000u);
		CHECK_EQ(Zone::block_of(0x40000010u), 3);
		CHECK_EQ(Zone::block_of(0x90010000u), -1);
		CHECK_EQ(Zone::block_of(0), -1);
		CHECK(c1::zptr_is_inline(0xFFFFFFFFu) && c1::zptr_is_inline(0xFFFFFFFEu) && !c1::zptr_is_inline(1));

		CHECK_EQ(count_entities("{\n\"classname\" \"worldspawn\"\n}\n{\n\"classname\" \"a{b\"\n}\n"), 2);
		CHECK_EQ(count_entities(""), 0);

		CHECK(asset_type_name(c1::ASSET_TYPE_XMODEL) == "xmodel");
		CHECK(asset_type_name(99) == "type_99");
		CHECK_EQ(asset_type_from_name("gfx_map"), c1::ASSET_TYPE_GFXWORLD);
		CHECK_EQ(asset_type_from_name("nope"), -1);
	}

	void test_container_errors()
	{
		std::printf("container errors\n");
		FastFileHeader header;
		std::vector<std::uint8_t> file;
		CHECK(!parse_fastfile_header(file, header));

		put_string(file, "IWff0100", false);
		put32(file, 334);
		file.push_back(1);
		put32(file, 0x01CF0E3A);
		put32(file, 0x12345678);
		CHECK(parse_fastfile_header(file, header));
		CHECK(header.is_signed() && !header.is_unsigned());
		CHECK_EQ(header.version, 334);
		CHECK_EQ(header.allowOnlineUpdate, 1);
		CHECK_EQ(header.fileTimeHigh, 0x01CF0E3A);
		CHECK_EQ(header.fileTimeLow, 0x12345678);

		file[4] = 'u';
		CHECK(parse_fastfile_header(file, header) && header.is_unsigned());
		file[4] = 'x';
		CHECK(!parse_fastfile_header(file, header));
		file[4] = '0';
		file[8] = 0x4D;		// version 333
		CHECK(!parse_fastfile_header(file, header));
		file[8] = 0x4E;

		// no key: decoding fails cleanly
		ClientKey key;
		std::vector<std::uint8_t> zone;
		file.resize(0x15 + 0x100 + 0x2000);
		CHECK(!decode_fastfile(file, key, zone));
		CHECK(zone.empty());
		CHECK(!read_fastfile("no/such/file.ff", key, zone));

		Zone z;
		CHECK(!z.open("no/such/file.ff", key));
		CHECK(!z.loaded());
	}

	// A hand written zone: XFile header, an asset list with one script string and one rawfile.
	std::vector<std::uint8_t> make_synthetic_zone()
	{
		std::vector<std::uint8_t> body;
		// XAssetList: scriptStringCount, strings ( inline ), assetCount, assets ( inline )
		put32(body, 1);
		put32(body, 0xFFFFFFFFu);
		put32(body, 1);
		put32(body, 0xFFFFFFFFu);
		// script string array: one inline string
		put32(body, 0xFFFFFFFFu);
		put_string(body, "tag_origin");
		// XAsset array: type, stream offset, header ( inline ), stream size
		const std::size_t entry = body.size();
		put32(body, c1::ASSET_TYPE_RAWFILE);
		put32(body, 0);
		put32(body, 0xFFFFFFFFu);
		put32(body, 0);
		// RawFile: name ( inline ), compressedLen, len, buffer ( inline ), then the name and len + 1 bytes
		const std::size_t start = body.size();
		put32(body, 0xFFFFFFFFu);
		put32(body, 0);
		put32(body, 5);
		put32(body, 0xFFFFFFFFu);
		put_string(body, "test/hello.txt");
		put_string(body, "hello");
		const std::uint32_t offset = static_cast<std::uint32_t>(start), size = static_cast<std::uint32_t>(body.size() - start);
		std::memcpy(body.data() + entry + 4, &offset, 4);
		std::memcpy(body.data() + entry + 12, &size, 4);

		std::vector<std::uint8_t> zone;
		put32(zone, static_cast<std::uint32_t>(body.size()));
		put32(zone, 0);
		for (int b = 0; b < BLOCK_COUNT; b++)
		{
			put32(zone, b == 0 || b == 3 ? 0x1000u : 0u);
		}
		zone.insert(zone.end(), body.begin(), body.end());
		return zone;
	}

	void test_synthetic_zone()
	{
		std::printf("synthetic zone\n");
		const std::vector<std::uint8_t> bytes = make_synthetic_zone();

		Zone zone;
		const Status s = zone.load(bytes, "synthetic");
		if (!s)
		{
			std::printf("  load: %s\n", s.message.c_str());
		}
		CHECK(s.ok);
		if (!s)
		{
			return;
		}
		CHECK(zone.loaded());
		CHECK(zone.name() == "synthetic");
		CHECK_EQ(zone.stream_size(), bytes.size());
		CHECK_EQ(zone.script_strings().size(), 1);
		CHECK(zone.script_strings().size() == 1 && zone.script_strings()[0] == "tag_origin");
		CHECK(zone.script_string(1) == "tag_origin");
		CHECK(zone.script_string(0).empty() && zone.script_string(77).empty());
		CHECK_EQ(zone.assets().size(), 1);
		CHECK_EQ(zone.all_assets().size(), 1);

		const XAssetEntry entry = zone.assets()[0];
		CHECK_EQ(entry.type, c1::ASSET_TYPE_RAWFILE);
		CHECK(entry.header != 0 && Zone::block_of(entry.header) == -1);		// headers live in the pool
		CHECK(zone.asset_name(entry.type, entry.header) == "test/hello.txt");
		CHECK_EQ(zone.find_asset(c1::ASSET_TYPE_RAWFILE, "test/hello.txt"), entry.header);
		CHECK_EQ(zone.find_asset(c1::ASSET_TYPE_RAWFILE, "nope"), 0);
		CHECK_EQ(zone.find_asset(c1::ASSET_TYPE_XMODEL, "test/hello.txt"), 0);

		RawFileView raw;
		CHECK(read_rawfile(zone, entry.header, raw));
		CHECK(raw.name == "test/hello.txt");
		CHECK_EQ(raw.raw.len, 5);
		CHECK_EQ(Zone::block_of(raw.raw.buffer), 3);	// file data is in the virtual block
		std::vector<std::uint8_t> contents;
		CHECK(rawfile_contents(raw, contents));
		CHECK(as_string(contents) == "hello");

		// image memory access never leaves the image
		CHECK(zone.bytes(0, 4).empty());
		CHECK(zone.bytes(0xF0000000u, 4).empty());
		CHECK(zone.bytes(raw.raw.buffer, 0x10000000u).empty());
		CHECK(zone.c_string(0).empty());
		CHECK_EQ(zone.get<std::uint32_t>(0xDEADBEEFu), 0);

		// wrong type of header address: no crash, no view
		ModelView model;
		CHECK(!read_model(zone, 0, model));
		CHECK(!read_model(zone, 0x12345678u, model));
		GfxWorldView world;
		CHECK(!read_gfx_world(zone, 0, world));
		CHECK_EQ(find_map_ents(zone), 0);

		// a second zone next to the first, and a moved one
		Zone other;
		CHECK(other.load(bytes, "other"));
		Zone moved(std::move(other));
		CHECK(moved.loaded() && moved.name() == "other" && zone.name() == "synthetic");

		// broken zones fail with a message instead of crashing
		{
			std::vector<std::uint8_t> truncated(bytes.begin(), bytes.end() - 3);
			Zone z;
			CHECK(!z.load(truncated, "truncated"));
			CHECK(!z.loaded());
		}
		{
			std::vector<std::uint8_t> wrongSize = bytes;
			wrongSize[40 + 16 + 4 + 11 + 12]++;		// XAsset.dataSize
			Zone z;
			const Status bad = z.load(wrongSize, "wrong size");
			CHECK(!bad.ok);
			CHECK(bad.message.find("asset") != std::string::npos);
		}
		{
			std::vector<std::uint8_t> wrongType = bytes;
			wrongType[40 + 16 + 4 + 11] = 0x7F;		// XAsset.type
			Zone z;
			CHECK(!z.load(wrongType, "wrong type"));
		}
		{
			Zone z;
			CHECK(!z.load(std::vector<std::uint8_t>(10), "tiny"));
			CHECK(!z.load(std::vector<std::uint8_t>(), "empty"));
		}
		{
			// random bytes after a valid header
			std::vector<std::uint8_t> noise = bytes;
			std::uint32_t state = 99;
			for (std::size_t i = 40; i < noise.size(); i++)
			{
				state = state * 1664525u + 1013904223u;
				noise[i] = static_cast<std::uint8_t>(state >> 24);
			}
			Zone z;
			CHECK(!z.load(noise, "noise"));
		}
	}

	// ---- writing ---------------------------------------------------------------------------------

	void test_deflate()
	{
		std::printf("deflate\n");
		std::vector<std::vector<std::uint8_t>> inputs;
		inputs.emplace_back();
		inputs.push_back({ 'x' });
		{
			const std::string text = make_text(300000);
			inputs.emplace_back(text.begin(), text.end());
		}
		inputs.emplace_back(200000, std::uint8_t(0));
		{
			std::vector<std::uint8_t> noise(150000);
			std::uint32_t state = 7;
			for (auto& b : noise)
			{
				state = state * 1664525u + 1013904223u;
				b = static_cast<std::uint8_t>(state >> 24);
			}
			inputs.push_back(noise);
			// noise, then the same noise again further away than the window, then text
			std::vector<std::uint8_t> mixed = noise;
			mixed.insert(mixed.end(), noise.begin(), noise.begin() + 70000);
			const std::string text = make_text(5000);
			mixed.insert(mixed.end(), text.begin(), text.end());
			inputs.push_back(mixed);
		}
		for (const auto& input : inputs)
		{
			for (const int level : { 0, 1, 4, 6, 9 })
			{
				std::vector<std::uint8_t> packed, unpacked;
				std::size_t consumed = 0;
				CHECK(zlib_deflate(input, packed, level));
				CHECK(zlib_inflate(packed, unpacked, 0, &consumed));
				CHECK(unpacked == input);
				CHECK_EQ(consumed, packed.size());
				if (level >= 4 && (&input == &inputs[2] || &input == &inputs[3]))
				{
					CHECK(packed.size() < input.size() / 3);	// the text and the zeros compress
				}
			}
		}
	}

	std::vector<std::uint8_t> bytes_of(const char* text)
	{
		return std::vector<std::uint8_t>(text, text + std::strlen(text));
	}

	std::string rawfile_text(const Zone& zone, const char* name)
	{
		RawFileView raw;
		std::vector<std::uint8_t> contents;
		if (!read_rawfile(zone, zone.find_asset(c1::ASSET_TYPE_RAWFILE, name), raw) || !rawfile_contents(raw, contents))
		{
			return "<missing>";
		}
		return as_string(contents);
	}

	void test_zone_editor()
	{
		std::printf("zone editor\n");
		const std::vector<std::uint8_t> bytes = make_synthetic_zone();

		// read -> write is the identity
		ZoneEditor editor;
		CHECK(editor.load(bytes, "synthetic"));
		CHECK(editor.loaded() && editor.zone().loaded());
		CHECK_EQ(editor.asset_count(), 1);
		CHECK_EQ(editor.find_asset(c1::ASSET_TYPE_RAWFILE, "test/hello.txt"), 0);
		CHECK(editor.find_asset(c1::ASSET_TYPE_RAWFILE, "nope") == ZoneEditor::npos);
		CHECK_EQ(editor.asset(0).chunks, 3);
		const std::uint64_t helloPrint = editor.asset_fingerprint(0);
		std::vector<std::uint8_t> written;
		ZoneWriteReport report;
		CHECK(editor.write(written, &report));
		CHECK(written == bytes);
		CHECK_EQ(report.streamSize, bytes.size());

		// a longer rawfile, a new compressed one, a string table: everything after them moves
		const std::string longText = make_text(5000);
		CHECK(editor.set_rawfile("test/hello.txt", bytes_of("hello, this is longer than before"), false));
		CHECK(editor.set_rawfile("test/packed.gsc", { reinterpret_cast<const std::uint8_t*>(longText.data()), longText.size() }, true));
		StringTableData table;
		table.columns = 2;
		table.rows = 2;
		table.cells = { "Key", "Value", "weapon", "" };
		CHECK(editor.set_string_table("mp/test.csv", table));
		CHECK(!editor.set_string_table("mp/bad.csv", StringTableData{ 2, 2, { "one" } }));
		CHECK(editor.set_rawfile("test/tail.txt", bytes_of("tail"), false));
		CHECK_EQ(editor.asset_count(), 4);
		CHECK(editor.asset_fingerprint(0) != helloPrint);
		CHECK(editor.write(written, &report));
		{
			Zone zone;
			const Status s = zone.load(written, "edited");
			if (!s)
			{
				std::printf("  load: %s\n", s.message.c_str());
			}
			CHECK(s.ok);
			CHECK_EQ(zone.assets().size(), 4);
			CHECK(zone.warnings().empty());
			CHECK(rawfile_text(zone, "test/hello.txt") == "hello, this is longer than before");
			CHECK(rawfile_text(zone, "test/packed.gsc") == longText);
			CHECK(rawfile_text(zone, "test/tail.txt") == "tail");
			RawFileView packed;
			CHECK(read_rawfile(zone, zone.find_asset(c1::ASSET_TYPE_RAWFILE, "test/packed.gsc"), packed));
			CHECK(packed.raw.compressedLen > 0 && packed.raw.compressedLen < packed.raw.len);
			StringTableView view;
			CHECK(read_stringtable(zone, zone.find_asset(c1::ASSET_TYPE_STRINGTABLE, "mp/test.csv"), view));
			CHECK_EQ(view.raw.columnCount, 2);
			CHECK_EQ(view.raw.rowCount, 2);
			CHECK(view.cell(0, 0) == "Key" && view.cell(1, 0) == "weapon" && view.cell(1, 1).empty());
			CHECK(zone.script_strings().size() == 1 && zone.script_strings()[0] == "tag_origin");
			CHECK_EQ(zone.header().size + 40, written.size());
		}
		CHECK_EQ(string_table_hash("AbC"), string_table_hash("abc"));
		CHECK_EQ(string_table_hash(""), 0);
		CHECK_EQ(string_table_hash("a"), 97);
		CHECK_EQ(string_table_hash("ab"), 97 * 31 + 98);

		// a written zone captures again to the same graph, and writes to the same bytes
		ZoneEditor second;
		CHECK(second.load(written, "second"));
		CHECK_EQ(second.asset_count(), 4);
		for (std::size_t i = 0; i < 4; i++)
		{
			CHECK(second.asset_fingerprint(i) == editor.asset_fingerprint(i));
		}
		std::vector<std::uint8_t> again;
		CHECK(second.write(again));
		CHECK(again == written);

		// removing and replacing assets
		CHECK(second.remove_asset(second.find_asset(c1::ASSET_TYPE_RAWFILE, "test/packed.gsc")));
		CHECK(!second.remove_asset(17));
		CHECK(second.set_string_table("mp/test.csv", StringTableData{ 1, 3, { "a", "b", "c" } }));
		CHECK(second.write(again));
		{
			Zone zone;
			CHECK(zone.load(again, "removed"));
			CHECK_EQ(zone.assets().size(), 3);
			CHECK_EQ(zone.find_asset(c1::ASSET_TYPE_RAWFILE, "test/packed.gsc"), 0);
			CHECK(rawfile_text(zone, "test/tail.txt") == "tail");
			StringTableView view;
			CHECK(read_stringtable(zone, zone.find_asset(c1::ASSET_TYPE_STRINGTABLE, "mp/test.csv"), view));
			CHECK_EQ(view.raw.rowCount, 3);
			CHECK(view.cell(2, 0) == "c");
		}

		// a zone from nothing, and assets taken from another zone
		ZoneEditor fresh;
		CHECK(fresh.create("fresh"));
		CHECK(fresh.loaded() && !fresh.zone().loaded());
		CHECK(!fresh.set_map_ents("{}"));
		CHECK(fresh.write(again));
		{
			Zone zone;
			CHECK(zone.load(again, "empty"));
			CHECK_EQ(zone.assets().size(), 0);
			CHECK_EQ(again.size(), 40 + 16);
		}
		CHECK(fresh.set_rawfile("mod/a.gsc", bytes_of("a()"), false));
		CHECK(fresh.add_asset(editor, editor.find_asset(c1::ASSET_TYPE_RAWFILE, "test/packed.gsc")));
		CHECK(fresh.add_asset(editor, editor.find_asset(c1::ASSET_TYPE_STRINGTABLE, "mp/test.csv"), 0));
		CHECK(!fresh.add_asset(editor, 99));
		CHECK(fresh.replace_asset(fresh.find_asset(c1::ASSET_TYPE_RAWFILE, "mod/a.gsc"), editor, editor.find_asset(c1::ASSET_TYPE_RAWFILE, "test/tail.txt")));
		CHECK(!fresh.replace_asset(0, editor, editor.find_asset(c1::ASSET_TYPE_RAWFILE, "test/tail.txt")));	// a string table is not a rawfile
		CHECK_EQ(fresh.asset_count(), 3);
		CHECK(fresh.asset(0).type == c1::ASSET_TYPE_STRINGTABLE);
		CHECK(fresh.write(again));
		{
			Zone zone;
			const Status s = zone.load(again, "fresh");
			if (!s)
			{
				std::printf("  load: %s\n", s.message.c_str());
			}
			CHECK(s.ok);
			CHECK_EQ(zone.assets().size(), 3);
			CHECK(rawfile_text(zone, "test/packed.gsc") == longText);
			CHECK(rawfile_text(zone, "test/tail.txt") == "tail");
			CHECK_EQ(zone.find_asset(c1::ASSET_TYPE_RAWFILE, "mod/a.gsc"), 0);
			CHECK_EQ(zone.block_usage(0).declared, zone.block_usage(0).used + 0x10);
			CHECK_EQ(zone.block_usage(3).declared, zone.block_usage(3).used);
		}

		// an asset whose data does not match its header is refused, not written
		{
			std::vector<std::uint8_t> broken = bytes;
			ZoneEditor e;
			CHECK(!e.load(std::vector<std::uint8_t>(bytes.begin(), bytes.end() - 2), "truncated"));
			CHECK(!e.loaded());
			CHECK(!e.write(again));
			CHECK(!e.set_rawfile("x", bytes_of("y")));
		}
	}

	// ---- against a real install -----------------------------------------------------------------

	std::filesystem::path game_dir()
	{
		if (const char* env = std::getenv("CODOL_ZONE_GAME_DIR"); env && *env)
		{
			return env;
		}
#ifdef CODOL_ZONE_GAME_DIR
		return CODOL_ZONE_GAME_DIR;
#else
		return {};
#endif
	}

	void test_real_zones()
	{
		const std::filesystem::path game = game_dir();
		const std::filesystem::path zones = game / "client_shipRetail" / "chinese";
		std::error_code ec;
		std::filesystem::path exe = game / "C1.exe";
		if (!std::filesystem::exists(exe, ec))
		{
			exe = game / "codoMP_client_shipRetail.exe";
		}
		if (game.empty() || !std::filesystem::exists(exe, ec) || !std::filesystem::exists(zones / "mp_killhouse_sh.ff", ec))
		{
			std::printf("real zones: SKIPPED ( no 1.1.1.13 install at '%s' )\n", game.string().c_str());
			return;
		}
		std::printf("real zones ( %s )\n", game.string().c_str());

		ClientKey key;
		const Status keyStatus = key.load_from_exe(exe);
		CHECK(keyStatus.ok);
		if (!keyStatus)
		{
			std::printf("  %s\n", keyStatus.message.c_str());
			return;
		}

		// two zones at once, loaded on two threads
		Zone map, common;
		FastFileInfo info;
		Status mapStatus, commonStatus;
		{
			std::thread a([&] { mapStatus = map.open(zones / "mp_killhouse_sh.ff", key, &info); });
			std::thread b([&] { commonStatus = common.open(zones / "common_mp.ff", key); });
			a.join();
			b.join();
		}
		CHECK(mapStatus.ok);
		CHECK(commonStatus.ok);
		if (!mapStatus || !commonStatus)
		{
			std::printf("  %s %s\n", mapStatus.message.c_str(), commonStatus.message.c_str());
			return;
		}

		// container
		CHECK(info.header.is_signed());
		CHECK(info.authName == "mp_killhouse_sh");
		CHECK(info.subheaderHashValid);
		CHECK_EQ(info.hashFailures, 0);
		CHECK_EQ(info.dataChunks, 5433);
		CHECK_EQ(info.masterBlocks, 22);
		CHECK_EQ(map.stream_size(), 106655201);
		CHECK_EQ(map.header().size + 40, map.stream_size());

		// traversal
		CHECK_EQ(map.assets().size(), 345);
		CHECK_EQ(map.script_strings().size(), 467);
		for (int b = 1; b < BLOCK_COUNT; b++)
		{
			CHECK_EQ(map.block_usage(b).used, map.block_usage(b).declared);
		}
		CHECK(map.all_assets().size() > map.assets().size());

		// entities
		MapEntsView ents;
		CHECK(read_map_ents(map, find_map_ents(map), ents));
		CHECK_EQ(ents.entityString.size(), 227009);
		CHECK_EQ(count_entities(ents.entityString), 1967);
		CHECK_EQ(ents.triggerModelCount, 143);
		CHECK(ents.entityString.starts_with("{\n\""));
		CHECK(ents.name == "maps/mp/mp_killhouse_sh.d3dbsp");

		// world
		GfxWorldView world;
		CHECK(read_gfx_world(map, first_asset(map, c1::ASSET_TYPE_GFXWORLD), world));
		CHECK(world.name == "maps/mp/mp_killhouse_sh.d3dbsp");
		CHECK_EQ(world.raw.draw.vertexCount, 657673);
		CHECK_EQ(world.raw.draw.indexCount, 482485 * 3);
		CHECK_EQ(world.vertexBytes.size(), 657673u * sizeof(c1::GfxWorldVertex));
		CHECK_EQ(world.surfaces.size(), 16681);
		CHECK_EQ(world.staticModels.size(), 16335);
		CHECK_EQ(world.brushModels.size(), 338);
		CHECK_EQ(world.cells.size(), 12);
		{
			// every surface stays inside the vertex and index buffers and has a material
			bool inside = true, named = true;
			for (const WorldSurface& s : world.surfaces)
			{
				inside = inside && s.raw.firstVertex + s.raw.vertexCount <= world.raw.draw.vertexCount
					&& s.raw.baseIndex + 3u * s.raw.triCount <= world.raw.draw.indexCount;
				named = named && !s.materialName.empty();
			}
			CHECK(inside);
			CHECK(named);
			bool modelsNamed = true;
			for (const WorldStaticModel& m : world.staticModels)
			{
				modelsNamed = modelsNamed && !m.modelName.empty() && m.raw.scale > 0.0f;
			}
			CHECK(modelsNamed);
		}

		// collision
		ClipMapView clip;
		CHECK(read_clip_map(map, first_asset(map, c1::ASSET_TYPE_CLIPMAP_MP), clip));
		CHECK_EQ(clip.brushes.size(), 27672);
		CHECK_EQ(clip.planes.size(), 14833);
		CHECK_EQ(clip.materials.size(), 1165);
		CHECK_EQ(clip.subModels.size(), 338);
		CHECK_EQ(clip.mapEnts, ents.address);
		{
			bool sidesOk = true;
			for (const Brush& b : clip.brushes)
			{
				sidesOk = sidesOk && b.sides.size() == 6u + b.raw.numsides;
				for (std::size_t i = 6; i < b.sides.size(); i++)
				{
					sidesOk = sidesOk && b.sides[i].planeIndex >= 0 && b.sides[i].materialNum < clip.materials.size();
				}
			}
			CHECK(sidesOk);
		}

		ComWorldView com;
		CHECK(read_com_world(map, first_asset(map, c1::ASSET_TYPE_COMWORLD), com));
		CHECK_EQ(com.raw.primaryLightCount, 30);

		// a rigid model with geometry in the zone
		ModelView model;
		CHECK(read_model(map, map.find_asset(c1::ASSET_TYPE_XMODEL, "vehicle_80s_sedan1_brn_destructible_mp"), model));
		CHECK_EQ(model.bones.size(), 48);
		CHECK_EQ(model.raw.numRootBones, 1);
		CHECK_EQ(model.lods.size(), 3);
		if (model.lods.size() == 3 && model.bones.size() == 48)
		{
			const ModelLod& lod = model.lods[0];
			CHECK(!lod.surfs.external);
			CHECK_EQ(lod.surfs.surfaces.size(), 42);
			std::size_t verts = 0, tris = 0;
			bool indicesOk = true, normalsOk = true, weightsOk = true;
			for (const SurfaceView& s : lod.surfs.surfaces)
			{
				verts += s.vertCount;
				tris += s.triCount;
				for (const std::uint16_t i : s.indices())
				{
					indicesOk = indicesOk && i < s.vertCount;
				}
				for (const SurfaceVertex& v : s.vertices())
				{
					const float len = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] + v.normal[2] * v.normal[2]);
					normalsOk = normalsOk && len > 0.97f && len < 1.03f;
				}
				for (const VertexWeights& w : s.weights(map))
				{
					weightsOk = weightsOk && w.count >= 1 && w.bone[0] < model.bones.size();
				}
			}
			CHECK_EQ(verts, 4311);
			CHECK_EQ(tris, 3600);
			CHECK(indicesOk);
			CHECK(normalsOk);
			CHECK(weightsOk);
			CHECK(model.bones[0].parent == -1 && model.bones[1].parent >= 0);
			CHECK(!model.bones[1].name.empty());
			CHECK(!model.materialNames.empty() && model.materialNames[0] == "mc/mtl_80s_econ_brn");

			MaterialView material;
			CHECK(read_material(map, model.materials[0], material));
			CHECK(material.name == "mc/mtl_80s_econ_brn");
			CHECK(!material.techsetName.empty());
			CHECK(!material.textures.empty());
			CHECK_EQ(material.textures.size(), material.raw.textureCount);
			CHECK_EQ(material.stateBits.size(), material.raw.stateBitsCount);
			for (const MaterialTexture& t : material.textures)
			{
				ImageView image;
				CHECK(read_image(map, t.image, image));
				CHECK(!image.name.empty());
			}
		}

		// an inline image ( lightmap ) has pixels, the usual image is streamed
		ImageView lightmap;
		CHECK(read_image(map, map.find_asset(c1::ASSET_TYPE_IMAGE, "*lightmap0_primary"), lightmap));
		CHECK(!lightmap.streamed);
		CHECK_EQ(lightmap.pixels.size(), lightmap.loadDef.resourceSize);
		CHECK(lightmap.pixels.size() > 0);

		// rawfiles: stored and zlib compressed
		RawFileView raw;
		CHECK(read_rawfile(map, map.find_asset(c1::ASSET_TYPE_RAWFILE, "animlists/mp_killhouse_sh.animlist"), raw));
		std::vector<std::uint8_t> contents;
		CHECK(rawfile_contents(raw, contents));
		CHECK_EQ(contents.size(), 37);
		CHECK(read_rawfile(map, map.find_asset(c1::ASSET_TYPE_RAWFILE, "maps/createfx/mp_killhouse_sh_fx.gsc"), raw));
		CHECK(raw.raw.compressedLen > 0);
		CHECK(rawfile_contents(raw, contents));
		CHECK_EQ(contents.size(), 16058);

		// animation
		AnimView anim;
		CHECK(read_anim(map, map.find_asset(c1::ASSET_TYPE_XANIMPARTS, "ex_airconditioner_fan"), anim));
		CHECK_EQ(anim.raw.numframes, 15);
		CHECK(anim.raw.framerate == 30.0f);
		CHECK_EQ(anim.boneNames.size(), 2);
		CHECK_EQ(anim.notetracks.size(), 1);

		// the second zone is untouched by the first
		CHECK_EQ(common.assets().size(), 3725);
		CHECK(common.find_asset(c1::ASSET_TYPE_XMODEL, "vehicle_80s_sedan1_brn_destructible_mp") == 0);
		CHECK(common.find_asset(c1::ASSET_TYPE_RAWFILE, "animtrees/ac130.atr") != 0);

		// string table and localized string from other zones
		{
			Zone z;
			CHECK(z.open(zones / "common_gametype_mp.ff", key));
			std::size_t tables = 0, cells = 0, wrongHashes = 0;
			for (const AssetRecord& r : z.all_assets())
			{
				StringTableView table;
				if (r.type == c1::ASSET_TYPE_STRINGTABLE && read_stringtable(z, r.header, table))
				{
					tables++;
					cells += table.cells.size();
					CHECK_EQ(table.cells.size(), static_cast<std::size_t>(table.raw.rowCount) * static_cast<std::size_t>(table.raw.columnCount));
					// the hash the writer computes is the one the shipped tables carry
					const auto stored = z.get_array<c1::StringTableCell>(table.raw.values, table.cells.size());
					for (std::size_t i = 0; i < stored.size(); i++)
					{
						wrongHashes += stored[i].hash != string_table_hash(table.cells[i]);
					}
				}
			}
			CHECK_EQ(tables, 120);
			CHECK(cells > 1000);
			CHECK_EQ(wrongHashes, 0);

			// a skinned weapon view model whose surfaces are streamed from an .iwd
			ModelView weapon;
			CHECK(read_model(z, z.find_asset(c1::ASSET_TYPE_XMODEL, "viewmodel_m4"), weapon));
			CHECK_EQ(weapon.bones.size(), 35);
			CHECK(weapon.lods.size() == 1 && weapon.lods[0].surfs.external);
			CHECK(!weapon.lods.empty() && !weapon.lods[0].surfsName.empty());
		}
		{
			Zone z;
			CHECK(z.open(zones / "patch_mp_killhouse_sh.ff", key));
			LocalizeView entry;
			bool found = false;
			for (const AssetRecord& r : z.all_assets())
			{
				if (r.type == c1::ASSET_TYPE_LOCALIZE_ENTRY && read_localize(z, r.header, entry) && !entry.name.empty() && !entry.value.empty())
				{
					found = true;
				}
			}
			CHECK(found);
		}

		// ---- writing: acceptance tests of the zone writer -----------------------------------------
		{
			std::printf("  writer: mp_killhouse_sh\n");
			std::vector<std::uint8_t> original;
			CHECK(read_fastfile(zones / "mp_killhouse_sh.ff", key, original));
			ZoneEditor editor;
			CHECK(editor.load(original, "mp_killhouse_sh"));

			// read -> write gives the same bytes
			std::vector<std::uint8_t> written;
			CHECK(editor.write(written));
			CHECK(written == original);

			// container: write, read back
			FastFileWriteOptions options;
			options.compression = 1;
			std::vector<std::uint8_t> file, reread;
			FastFileInfo writtenInfo;
			CHECK(encode_fastfile(written, key, file, options));
			CHECK(file.size() > FASTFILE_UNSIGNED_ENCRYPTED_SPAN);		// more than the encrypted first chunk
			CHECK(decode_fastfile(file, key, reread, &writtenInfo));
			CHECK(writtenInfo.header.is_unsigned());
			CHECK_EQ(writtenInfo.header.version, FASTFILE_VERSION);
			CHECK(reread == original);
			std::vector<std::uint8_t>().swap(reread);
			std::vector<std::uint8_t>().swap(file);

			// an entity string of another length: every asset still loads, all others are unchanged
			MapEntsView baseEnts;
			CHECK(read_map_ents(editor.zone(), find_map_ents(editor.zone()), baseEnts));
			std::string text(baseEnts.entityString);
			text += "{\n\"classname\" \"script_origin\"\n\"targetname\" \"codol_zone_writer_test\"\n\"origin\" \"1 2 3\"\n}\n";
			std::vector<std::uint64_t> before;
			for (std::size_t i = 0; i < editor.asset_count(); i++)
			{
				before.push_back(editor.asset_fingerprint(i));
			}
			CHECK(editor.set_map_ents(text));
			CHECK(editor.set_rawfile("maps/mp/codol_zone_writer_test.gsc", bytes_of("main() {}")));
			CHECK(editor.write(written));
			CHECK(written.size() > original.size());

			ZoneEditor check;
			const Status loaded = check.load(written, "mp_killhouse_sh");
			if (!loaded)
			{
				std::printf("  %s\n", loaded.message.c_str());
			}
			CHECK(loaded.ok);
			CHECK(check.zone().warnings().empty());
			CHECK_EQ(check.asset_count(), before.size() + 1);
			CHECK_EQ(check.zone().all_assets().size(), editor.zone().all_assets().size() + 1);
			MapEntsView newEnts;
			CHECK(read_map_ents(check.zone(), find_map_ents(check.zone()), newEnts));
			CHECK(newEnts.entityString == text);
			CHECK_EQ(newEnts.raw.numEntityChars, text.size() + 1);
			CHECK_EQ(newEnts.triggerModelCount, baseEnts.triggerModelCount);
			CHECK(rawfile_text(check.zone(), "maps/mp/codol_zone_writer_test.gsc") == "main() {}");
			int changed = 0;
			for (std::size_t i = 0; i < before.size(); i++)
			{
				if (check.asset_fingerprint(i) != before[i])
				{
					changed++;
					CHECK_EQ(check.asset(i).type, c1::ASSET_TYPE_CLIPMAP_MP);	// the MapEnts is nested in the clip map
				}
			}
			CHECK_EQ(changed, 1);

			// an asset of another zone: its script string indices follow this zone's list
			if (std::filesystem::exists(zones / "mp_dome.ff", ec))
			{
				ZoneEditor donor;
				CHECK(donor.open(zones / "mp_dome.ff", key));
				const std::size_t paper = donor.find_asset(c1::ASSET_TYPE_XMODEL, "machinery_windmill");
				CHECK(paper != ZoneEditor::npos);
				if (paper != ZoneEditor::npos)
				{
					CHECK(donor.asset(paper).usesScriptStrings);
					CHECK(editor.find_asset(c1::ASSET_TYPE_XMODEL, "machinery_windmill") == ZoneEditor::npos);
					CHECK(editor.add_asset(donor, paper));
					std::vector<std::uint8_t> withModel;
					CHECK(editor.write(withModel));
					ZoneEditor taken;
					CHECK(taken.load(withModel, "mp_killhouse_sh"));
					const std::size_t here = taken.find_asset(c1::ASSET_TYPE_XMODEL, "machinery_windmill");
					CHECK(here != ZoneEditor::npos);
					CHECK(taken.asset_fingerprint(here) == donor.asset_fingerprint(paper));
					ModelView theirs, ours;
					CHECK(read_model(donor.zone(), donor.zone().find_asset(c1::ASSET_TYPE_XMODEL, "machinery_windmill"), theirs));
					CHECK(read_model(taken.zone(), taken.zone().find_asset(c1::ASSET_TYPE_XMODEL, "machinery_windmill"), ours));
					CHECK_EQ(ours.bones.size(), theirs.bones.size());
					for (std::size_t i = 0; i < ours.bones.size() && i < theirs.bones.size(); i++)
					{
						CHECK(ours.bones[i].name == theirs.bones[i].name);
					}
					for (std::size_t i = 0; i < before.size(); i++)
					{
						CHECK(taken.asset_fingerprint(i) == check.asset_fingerprint(i));
					}
				}
			}

			// the geometry the renderer reads is where the pointers say it is
			GfxWorldView oldWorld, newWorld;
			CHECK(read_gfx_world(editor.zone(), editor.zone().find_asset(c1::ASSET_TYPE_GFXWORLD, "maps/mp/mp_killhouse_sh.d3dbsp"), oldWorld));
			CHECK(read_gfx_world(check.zone(), check.zone().find_asset(c1::ASSET_TYPE_GFXWORLD, "maps/mp/mp_killhouse_sh.d3dbsp"), newWorld));
			CHECK_EQ(newWorld.surfaces.size(), oldWorld.surfaces.size());
			CHECK_EQ(newWorld.staticModels.size(), oldWorld.staticModels.size());
			CHECK_EQ(newWorld.raw.draw.vertexCount, oldWorld.raw.draw.vertexCount);

			// ---- typed editing: data ids, pointers as targets, an array that changes its length
			{
				std::printf("  typed editing: mp_killhouse_sh\n");
				ZoneEditor typed;
				CHECK(typed.load(original, "mp_killhouse_sh"));
				const Zone& z = typed.zone();
				const zaddr clipAddress = first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP);
				ClipMapView typedClip;
				CHECK(read_clip_map(z, clipAddress, typedClip));
				const ZoneDataId header = typed.header_data(clipAddress);
				ZoneDataInfo dataInfo;
				CHECK(typed.data_info(header, dataInfo));
				CHECK_EQ(dataInfo.size, sizeof(c1::clipMap_t));
				CHECK_EQ(dataInfo.index, 0u);
				CHECK(typed.asset(dataInfo.asset).type == c1::ASSET_TYPE_CLIPMAP_MP);

				// the typedClip map's planes are the GfxWorld's: a pointer into data of another asset
				const ZoneTarget planes = typed.pointer_at(header, offsetof(c1::clipMap_t, planes));
				ZoneDataInfo planeInfo;
				CHECK(planes.kind == ZoneTarget::Kind::data && planes.offset == 0);
				CHECK(typed.data_info(planes.data, planeInfo));
				CHECK_EQ(planeInfo.size, sizeof(c1::cplane_s) * static_cast<std::size_t>(typedClip.raw.planeCount));
				CHECK(planeInfo.asset != dataInfo.asset && typed.asset(planeInfo.asset).type == c1::ASSET_TYPE_GFXWORLD);
				CHECK(typed.data_at(typedClip.raw.planes) == planes.data);
				std::uint32_t inside = 0;
				CHECK(typed.data_at(typedClip.raw.planes + 40, &inside) == planes.data && inside == 40);
				CHECK(typed.pointers_to(planes.data).size() > 1000);		// brush sides, BSP nodes
				CHECK(typed.pointer_at(header, offsetof(c1::clipMap_t, planeCount)).kind == ZoneTarget::Kind::none);
				CHECK(typed.data_pointers(planes.data).empty());

				// a field: patched in place; a pointer field is not a number
				CHECK(typed.patch_value(header, offsetof(c1::clipMap_t, checksum), std::uint32_t{ 0x12345678 }));
				CHECK(!typed.patch_value(header, offsetof(c1::clipMap_t, planes), std::uint32_t{ 0 }));
				CHECK(!typed.patch_value(header, sizeof(c1::clipMap_t) - 2, std::uint32_t{ 0 }));
				// data something points into cannot go, alone or as a range
				CHECK(!typed.remove_data(planes.data));
				CHECK(!typed.remove_data_range(planes.data, planes.data));
				// a pointer past the end of its target is refused
				CHECK(!typed.set_pointer(header, offsetof(c1::clipMap_t, planes), ZoneTarget::to_data(planes.data, planeInfo.size + 4)));
				CHECK(typed.set_pointer(header, offsetof(c1::clipMap_t, planes), ZoneTarget::to_data(planes.data, 0)));

				// an array of another length with its count: the entity string, the general way. The result is what the
				// dedicated edit writes.
				MapEntsView typedEnts;
				const zaddr entsAddress = find_map_ents(z);
				CHECK(read_map_ents(z, entsAddress, typedEnts));
				const ZoneDataId stringData = typed.data_at(typedEnts.raw.entityString);
				const ZoneDataId entsHeader = typed.header_data(entsAddress);
				CHECK(stringData != 0 && entsHeader != 0);
				std::vector<std::uint8_t> longer(text.begin(), text.end());
				longer.push_back(0);
				CHECK(typed.set_data(stringData, longer));
				CHECK(typed.patch_value(entsHeader, offsetof(c1::MapEnts, numEntityChars), static_cast<std::int32_t>(longer.size())));
				std::vector<std::uint8_t> general, dedicated;
				CHECK(typed.write(general));
				ZoneEditor other;
				CHECK(other.load(original, "mp_killhouse_sh"));
				CHECK(other.patch_value(other.header_data(clipAddress), offsetof(c1::clipMap_t, checksum), std::uint32_t{ 0x12345678 }));
				CHECK(other.set_map_ents(text));
				CHECK(other.write(dedicated));
				CHECK(general == dedicated);
				// a count that does not match its array is refused by the loaders, with nothing written
				CHECK(typed.patch_value(entsHeader, offsetof(c1::MapEnts, numEntityChars), static_cast<std::int32_t>(longer.size() + 1)));
				std::vector<std::uint8_t> refused;
				CHECK(!typed.write(refused) && refused.empty());

				// data the loaders do not ask for is refused too; taken out again, the zone is the original
				ZoneEditor third;
				CHECK(third.load(original, "mp_killhouse_sh"));
				ZoneDataId extra = 0;
				const std::uint8_t four[4] = { 1, 2, 3, 4 };
				CHECK(third.insert_data(third.header_data(clipAddress), true, four, {}, extra) && extra != 0);
				CHECK(third.data_info(extra, dataInfo) && dataInfo.index == 1 && dataInfo.address == 0);
				CHECK(!third.write(refused));
				CHECK(third.remove_data(extra));
				CHECK(!third.data_info(extra, dataInfo));
				CHECK(third.write(refused));
				CHECK(refused == original);
			}
		}

		// an unsigned ( rebuilt ) zone from the patch folder
		{
			const std::filesystem::path patch = game / "client_shipRetail" / "patch" / "codol_common.ff";
			if (std::filesystem::exists(patch, ec))
			{
				Zone z;
				FastFileInfo patchInfo;
				CHECK(z.open(patch, key, &patchInfo));
				CHECK(patchInfo.header.is_unsigned());
				CHECK(z.assets().size() > 0);
			}
		}

		if (const char* all = std::getenv("CODOL_ZONE_TEST_ALL"); all && *all == '1')
		{
			int count = 0, failed = 0;
			for (const char* folder : { "chinese", "patch" })
			{
				for (const auto& e : std::filesystem::directory_iterator(game / "client_shipRetail" / folder, ec))
				{
					if (e.path().extension() == ".ff")
					{
						Zone z;
						count++;
						if (const Status s = z.open(e.path(), key); !s)
						{
							failed++;
							std::printf("  %s\n", s.message.c_str());
						}
					}
				}
			}
			std::printf("  all zones: %d loaded, %d failed\n", count, failed);
			CHECK_EQ(failed, 0);
			CHECK(count >= 138);
		}
	}
}

int main()
{
	test_inflate();
	test_crypto();
	test_decoders();
	test_container_errors();
	test_synthetic_zone();
	test_deflate();
	test_zone_editor();
	test_real_zones();
	std::printf("%d checks, %d failed\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
