#pragma once

// IWD archives of 1.1.1.13 ( main\iw_image_NN.iwd .. ): zip files whose entry names ( in the central directory )
// and entry data are AES-192-CTR encrypted. Port of Codol.FastFiles\Iwd\IwdArchive.cs, Crc32.cs,
// Codol.FastFiles\Crypto\LibTomCryptCtr.cs and the IWD part of FastFileKeyProvider.cs.
//
//   key      24 bytes, the same for every archive: RSA-OAEP( SHA-256 ) decryption of a 256 byte blob in C1.exe
//            with the RSA-2048 private key that is also in C1.exe ( derive_iwd_key ).
//   counter  16 bytes, incremented as a big endian number per AES block. It is seeded per archive and position,
//            which is why the archive's FILE NAME is part of the cipher:
//              entry name   { 0x01010101, crc32( file name ), entry index, 0x01010101 }      ( u32 little endian each )
//              entry data   { data offset of the chunk, crc32( file name ), 0x01010101, 0x01010101 }, restarted
//                           every 0x4000 bytes of stored data
//   The zip structures themselves ( end of central directory, headers, sizes, CRCs ) are in the clear.
//
// AES and RSA come from Windows CNG ( bcrypt ); inflate is codol_render's own ( inflate_raw ).

#include "build_1_1_1_13.hpp"
#include "common.hpp"

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace codol::render
{
	// Crc32.cs: the zip CRC ( polynomial 0xEDB88320 )
	std::uint32_t crc32(byte_span data, std::uint32_t initial = 0);

	// Raw deflate ( RFC 1951 ) of exactly `expected_size` bytes. Fails on malformed data, on output that would
	// exceed expected_size and on input that ends before expected_size bytes are produced.
	// With `allow_truncated_input` the function succeeds as soon as expected_size bytes exist even when the
	// stream is not finished ( reading the head of an entry ).
	bool inflate_raw(byte_span compressed, std::size_t expected_size, std::vector<std::uint8_t>& out, std::string* error = nullptr,
		bool allow_truncated_input = false);

	using iwd_key = std::array<std::uint8_t, 24>;

	// FastFileKeyProvider.cs:13-42 ( LoadFromExecutable ) + :51-89 ( DeriveIwdAesKey ): reads the PKCS#1 RSA
	// private key ( build::EXE_RSA_PRIVATE_KEY_DER, else the first DER key found in the file ) and decrypts the key
	// blob at build::EXE_IWD_KEY_BLOB.
	bool derive_iwd_key(const std::filesystem::path& exe, iwd_key& key, std::string* error = nullptr);

	// LibTomCryptCtr.cs with bigEndianCounter: data ^= AES-ECB( key, counter++ ). `iv` is 16 bytes.
	bool aes_ctr_transform(const iwd_key& key, const std::uint8_t* iv, std::uint8_t* data, std::size_t size, std::string* error = nullptr);

	// IwdArchive.cs:8-16
	struct iwd_entry
	{
		int index = 0;
		std::string name;					// decrypted, e.g. "images/foo.iwi"
		std::uint16_t method = 0;			// 0 stored, 8 deflate
		std::uint16_t flags = 0;
		std::uint32_t crc32 = 0;
		std::uint32_t compressed_size = 0;
		std::uint32_t uncompressed_size = 0;
		std::uint32_t local_header_offset = 0;
	};

	class iwd_archive
	{
	public:
		inline static constexpr std::uint32_t DATA_CHUNK_SIZE = 0x4000;		// IwdArchive.cs:22

		// IwdArchive.cs:50-68 ( Open ). `key` null: a plain zip ( an unencrypted IWD ). The file stays open.
		static std::unique_ptr<iwd_archive> open(const std::filesystem::path& path, const iwd_key* key, std::string* error = nullptr);
		~iwd_archive();
		iwd_archive(const iwd_archive&) = delete;
		iwd_archive& operator=(const iwd_archive&) = delete;

		const std::string& file_name() const { return file_name_; }
		std::uint32_t name_hash() const { return name_hash_; }		// crc32 of the file name
		const std::vector<iwd_entry>& entries() const { return entries_; }
		// exact, case sensitive name; nullptr when the archive has no such entry
		const iwd_entry* find(std::string_view name) const;

		// IwdArchive.cs:70-96 ( Read ): the decrypted, inflated entry; the CRC is checked unless told otherwise.
		// Safe to call from several threads.
		bool read(const iwd_entry& entry, std::vector<std::uint8_t>& out, std::string* error = nullptr, bool verify_crc = true) const;
		// IwdArchive.cs:98-143 ( ReadPrefix ): the first `count` bytes of the entry ( e.g. the 32 byte IWI header )
		bool read_prefix(const iwd_entry& entry, std::uint32_t count, std::vector<std::uint8_t>& out, std::string* error = nullptr) const;

	private:
		iwd_archive() = default;
		bool read_stored(const iwd_entry& entry, std::uint32_t length, std::vector<std::uint8_t>& out, std::string* error) const;

		struct impl;
		std::unique_ptr<impl> impl_;
		std::string file_name_;
		std::uint32_t name_hash_ = 0;
		std::vector<iwd_entry> entries_;
	};

	// The archives of one kind of game file with the game's search order ( IwdArchiveSet.cs ):
	// per directory the "localized_*" archives first, then by file name DESCENDING ( iw_image_23 wins over
	// iw_image_00 ); a path an earlier archive holds hides the same path in a later one. Game paths are compared
	// lower case with forward slashes.
	class iwd_archive_set
	{
	public:
		struct hit
		{
			const iwd_archive* archive = nullptr;
			const iwd_entry* entry = nullptr;
		};

		// IwdArchiveSet.cs:45-68 ( Open ) + :70-98 ( FromArchives ). `directories` in search order; only entries
		// whose normalised path starts with `prefix` are indexed ( "images/" ); `include_archive` filters by file
		// name. Archives that cannot be opened are skipped with a text in `warnings`; archives without a matching
		// entry are closed again.
		static std::unique_ptr<iwd_archive_set> open(const std::vector<std::filesystem::path>& directories, const iwd_key* key, std::string_view prefix,
			const std::function<bool(const std::string& file_name)>& include_archive = {}, std::vector<std::string>* warnings = nullptr);

		static std::string normalize(std::string_view path);		// IwdArchiveSet.cs:36

		const std::vector<std::unique_ptr<iwd_archive>>& archives() const { return archives_; }
		std::size_t entry_count() const { return index_.size(); }
		int duplicate_names() const { return duplicates_; }
		bool find(std::string_view game_path, hit& out) const;		// IwdArchiveSet.cs:102-113
		bool read(std::string_view game_path, std::vector<std::uint8_t>& out, std::string* error = nullptr) const;
		// every indexed path with the archive that wins it, in no particular order
		void for_each(const std::function<void(const std::string& game_path, const hit&)>& visit) const;

	private:
		std::vector<std::unique_ptr<iwd_archive>> archives_;
		std::vector<std::pair<std::string, hit>> index_;		// sorted by path
		int duplicates_ = 0;
	};

	// The image archives of an installation ( Codol.Assets\Game\GameInstallation.cs:343-385, TextureExtractor.cs:155-172 ):
	// <root>\main and, when it exists, <root>\main_shared; `images` are the archives whose name does not contain
	// "imagestripped", `stripped` those that do. An image is looked up in `images` first, then in `stripped`.
	struct image_archive_sets
	{
		std::unique_ptr<iwd_archive_set> images;
		std::unique_ptr<iwd_archive_set> stripped;

		// "images/<name>.iwi" of the streamed image `image_name`
		bool find(std::string_view image_name, iwd_archive_set::hit& out) const;
		bool read(std::string_view image_name, std::vector<std::uint8_t>& out, std::string* error = nullptr) const;
	};
	bool open_image_archives(const std::filesystem::path& game_root, const iwd_key& key, image_archive_sets& out, std::string* error = nullptr,
		std::vector<std::string>* warnings = nullptr);
}
