#pragma once

// Call of Duty Online 1.1.1.13 fast file container ( version 334 ): header, RSA wrapped AES key, AES-192-CTR,
// SHA-256 chunk authentication, zlib.
//
//   file      : FastFileHeader ( 0x15 bytes ) | RSA-OAEP( SHA-256 ) block ( 0x100 bytes: 24 byte key + 16 byte IV ) | body
//   IWff0100  : signed. body = 0x2000 byte chunks, each AES-CTR encrypted:
//                 auth chunk ( "IWffs100", IV = the IV of the key block; holds 244 master block hashes )
//                 then per master block: 1 chunk of 256 SHA-256 hashes ( IV = first 16 bytes of its own hash ),
//                 followed by up to 256 data chunks ( IV = first 16 bytes of the chunk's hash )
//               the concatenated data chunks are one zlib stream
//   IWffu100  : unsigned. body = zlib stream; only the part that lies inside the first 0x40000 bytes of the file
//               is AES-CTR encrypted ( IV of the key block )
//   zone      : the inflated stream: XFileHeader ( 40 bytes ) + XFile.size bytes
//
// Reference: CODOL-AssetExplorer FastFileReader.cs / FastFileKeyProvider.cs / LibTomCryptCtr.cs
// ( DB_LoadXFile_Internal 0x004B5D50, DB_AuthLoad_* 0x004B56A0 .. 0x004B5AC0 ).
//
// Nothing here throws; every function reports failure through Status.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace codol::zone
{
	struct Status
	{
		bool ok = true;
		std::string message;

		explicit operator bool() const { return ok; }
		static Status error(std::string text) { return Status{ false, std::move(text) }; }
	};

	inline constexpr std::uint32_t FASTFILE_VERSION = 334;
	inline constexpr std::size_t FASTFILE_HEADER_SIZE = 0x15;
	inline constexpr std::size_t FASTFILE_KEY_BLOCK_SIZE = 0x100;
	inline constexpr std::size_t FASTFILE_CHUNK_SIZE = 0x2000;
	inline constexpr std::size_t FASTFILE_UNSIGNED_ENCRYPTED_SPAN = 0x40000;

	struct FastFileHeader
	{
		char magic[9] = {};					// "IWff0100" ( signed ) or "IWffu100" ( unsigned )
		std::uint32_t version = 0;			// 334
		std::uint8_t allowOnlineUpdate = 0;
		std::uint32_t fileTimeHigh = 0;		// FILETIME of the build
		std::uint32_t fileTimeLow = 0;

		bool is_signed() const;
		bool is_unsigned() const;
	};

	struct FastFileInfo
	{
		FastFileHeader header;
		std::uint64_t fileSize = 0;
		std::string authName;				// zone name stored in the auth header ( signed files )
		bool subheaderHashValid = false;	// SHA-256 over the auth sub header matches ( signed files )
		std::uint32_t masterBlocks = 0;
		std::uint32_t dataChunks = 0;
		std::uint32_t hashFailures = 0;		// only when verification is not fatal
		std::uint64_t compressedSize = 0;
		std::uint64_t zoneSize = 0;
	};

	// RSA-2048 private key of the client ( PKCS#1 DER, g_ffRsaPrivateKeyDer @ 0x00C3D248 of 1.1.1.13 ). It unwraps the
	// AES key / IV block of every fast file.
	class ClientKey
	{
	public:
		ClientKey();
		~ClientKey();
		ClientKey(ClientKey&&) noexcept;
		ClientKey& operator=(ClientKey&&) noexcept;
		ClientKey(const ClientKey&) = delete;
		ClientKey& operator=(const ClientKey&) = delete;

		// finds the key in a client executable ( C1.exe or codoMP_client_shipRetail.exe ): first at the known file
		// offset 0x83C248, then by scanning for the DER prefix
		Status load_from_exe(const std::filesystem::path& exe);
		Status load_from_der(std::span<const std::uint8_t> der);

		bool valid() const;

		// RSA-OAEP( SHA-256 ) decryption of one 256 byte block
		Status unwrap(std::span<const std::uint8_t> block, std::vector<std::uint8_t>& plain) const;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};

	struct FastFileOptions
	{
		bool failOnHashMismatch = true;		// signed files: a wrong chunk hash is an error
	};

	Status parse_fastfile_header(std::span<const std::uint8_t> file, FastFileHeader& header);

	// decrypts and inflates a fast file that is already in memory; `zone` receives XFileHeader + zone data
	Status decode_fastfile(std::span<const std::uint8_t> file, const ClientKey& key, std::vector<std::uint8_t>& zone,
		FastFileInfo* info = nullptr, const FastFileOptions& options = {});

	Status read_fastfile(const std::filesystem::path& path, const ClientKey& key, std::vector<std::uint8_t>& zone,
		FastFileInfo* info = nullptr, const FastFileOptions& options = {});

	// zlib ( RFC 1950 ) stream -> bytes. `sizeHint` reserves the output. Used for the zone body and for compressed
	// RawFile assets.
	Status zlib_inflate(std::span<const std::uint8_t> compressed, std::vector<std::uint8_t>& out, std::size_t sizeHint = 0,
		std::size_t* consumed = nullptr);

	Status read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& data);
}
