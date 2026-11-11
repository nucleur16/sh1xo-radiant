#include "codol_zone/fastfile.hpp"
#include "crypto_win.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace codol::zone
{
	namespace
	{
		constexpr char SIGNED_MAGIC[] = "IWff0100";
		constexpr char UNSIGNED_MAGIC[] = "IWffu100";
		constexpr char AUTH_MAGIC[] = "IWffs100";

		// DB_AuthHeader ( one 0x2000 byte chunk, DB_AuthLoad_ValidateHeader 0x004B56A0 )
		constexpr std::size_t AUTH_SUBHEADER_HASH = 12;
		constexpr std::size_t AUTH_SUBHEADER_OFFSET = 300;
		constexpr std::size_t AUTH_SUBHEADER_LENGTH = 7844;
		constexpr std::size_t AUTH_MASTER_HASHES = 336;
		constexpr std::size_t AUTH_MASTER_HASH_COUNT = 244;
		constexpr std::size_t HASHES_PER_MASTER_BLOCK = 256;

		// g_ffRsaPrivateKeyDer 0x00C3D248 is at this file offset of the 1.1.1.13 client
		constexpr std::size_t KNOWN_KEY_OFFSET = 0x83C248;
		constexpr std::uint8_t DER_PREFIX[] = { 0x30, 0x82, 0x04, 0xA3, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00 };

		std::uint32_t u32(const std::uint8_t* p)
		{
			return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16)
				| (static_cast<std::uint32_t>(p[3]) << 24);
		}

		std::string hex_offset(const std::size_t value)
		{
			char text[32];
			std::snprintf(text, sizeof(text), "0x%zX", value);
			return text;
		}
	}

	bool FastFileHeader::is_signed() const { return !std::memcmp(magic, SIGNED_MAGIC, 8); }
	bool FastFileHeader::is_unsigned() const { return !std::memcmp(magic, UNSIGNED_MAGIC, 8); }

	// ------------------------------------------------------------------------------------------

	struct ClientKey::Impl
	{
		crypto::Rsa rsa;
	};

	ClientKey::ClientKey() = default;
	ClientKey::~ClientKey() = default;
	ClientKey::ClientKey(ClientKey&&) noexcept = default;
	ClientKey& ClientKey::operator=(ClientKey&&) noexcept = default;

	bool ClientKey::valid() const
	{
		return impl_ && impl_->rsa.valid();
	}

	Status ClientKey::load_from_der(const std::span<const std::uint8_t> der)
	{
		crypto::RsaPrivateKey parsed;
		if (!crypto::parse_pkcs1_private_key(der, parsed) || parsed.modulus.size() != 256)
		{
			return Status::error("not a PKCS#1 RSA-2048 private key");
		}
		auto impl = std::make_unique<Impl>();
		if (!impl->rsa.import(parsed))
		{
			return Status::error("the RSA private key was rejected by Windows CNG");
		}
		impl_ = std::move(impl);
		return {};
	}

	Status ClientKey::load_from_exe(const std::filesystem::path& exe)
	{
		std::vector<std::uint8_t> data;
		if (const Status s = read_file(exe, data); !s)
		{
			return s;
		}

		const auto try_at = [&](const std::size_t offset) -> bool
		{
			if (offset + sizeof(DER_PREFIX) > data.size() || std::memcmp(data.data() + offset, DER_PREFIX, sizeof(DER_PREFIX)) != 0)
			{
				return false;
			}
			const std::size_t length = 4 + ((static_cast<std::size_t>(data[offset + 2]) << 8) | data[offset + 3]);
			if (offset + length > data.size())
			{
				return false;
			}
			return static_cast<bool>(load_from_der({ data.data() + offset, length }));
		};

		if (try_at(KNOWN_KEY_OFFSET))
		{
			return {};
		}
		auto it = data.begin();
		for (;;)
		{
			it = std::search(it, data.end(), std::begin(DER_PREFIX), std::end(DER_PREFIX));
			if (it == data.end())
			{
				break;
			}
			if (try_at(static_cast<std::size_t>(it - data.begin())))
			{
				return {};
			}
			++it;
		}
		return Status::error("no PKCS#1 RSA-2048 private key found in '" + exe.string() + "' ( packed or wrong executable? )");
	}

	Status ClientKey::unwrap(const std::span<const std::uint8_t> block, std::vector<std::uint8_t>& plain) const
	{
		if (!valid())
		{
			return Status::error("no client key loaded");
		}
		if (!impl_->rsa.decrypt_oaep_sha256(block, plain))
		{
			return Status::error("RSA-OAEP unwrap of the key block failed ( wrong client executable or corrupt file )");
		}
		return {};
	}

	// ------------------------------------------------------------------------------------------

	Status read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& data)
	{
		data.clear();
		std::error_code ec;
		const auto size = std::filesystem::file_size(path, ec);
		if (ec)
		{
			return Status::error("cannot open '" + path.string() + "': " + ec.message());
		}
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			return Status::error("cannot open '" + path.string() + "'");
		}
		try
		{
			data.resize(static_cast<std::size_t>(size));
		}
		catch (const std::exception&)
		{
			return Status::error("out of memory reading '" + path.string() + "'");
		}
		file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
		if (static_cast<std::uint64_t>(file.gcount()) != size)
		{
			return Status::error("short read of '" + path.string() + "'");
		}
		return {};
	}

	Status parse_fastfile_header(const std::span<const std::uint8_t> file, FastFileHeader& header)
	{
		if (file.size() < FASTFILE_HEADER_SIZE)
		{
			return Status::error("file is shorter than the fast file header ( 0x15 bytes )");
		}
		std::memcpy(header.magic, file.data(), 8);
		header.magic[8] = 0;
		header.version = u32(file.data() + 8);
		header.allowOnlineUpdate = file[12];
		header.fileTimeHigh = u32(file.data() + 13);
		header.fileTimeLow = u32(file.data() + 17);
		if (!header.is_signed() && !header.is_unsigned())
		{
			return Status::error("unknown fast file magic ( expected IWff0100 or IWffu100 )");
		}
		if (header.version != FASTFILE_VERSION)
		{
			return Status::error("unsupported fast file version " + std::to_string(header.version) + " ( this reader handles 334 = CoDOL 1.1.1.13 )");
		}
		return {};
	}

	namespace
	{
		Status decode_impl(const std::span<const std::uint8_t> file, const ClientKey& key, std::vector<std::uint8_t>& zone, FastFileInfo* info,
			const FastFileOptions& options)
		{
			FastFileInfo local;
			FastFileInfo& out = info ? *info : local;
			out = {};
			out.fileSize = file.size();

			if (const Status s = parse_fastfile_header(file, out.header); !s)
			{
				return s;
			}
			const bool isUnsigned = out.header.is_unsigned();
			const std::size_t bodyStart = FASTFILE_HEADER_SIZE + FASTFILE_KEY_BLOCK_SIZE;
			if (file.size() < bodyStart + (isUnsigned ? 0 : FASTFILE_CHUNK_SIZE))
			{
				return Status::error("file is truncated ( no key block / auth chunk )");
			}

			std::vector<std::uint8_t> keyIv;
			if (const Status s = key.unwrap(file.subspan(FASTFILE_HEADER_SIZE, FASTFILE_KEY_BLOCK_SIZE), keyIv); !s)
			{
				return s;
			}
			if (keyIv.size() != 40)
			{
				return Status::error("unwrapped key block has " + std::to_string(keyIv.size()) + " bytes instead of 40 ( 24 byte key + 16 byte IV )");
			}
			crypto::AesCtr aes;
			if (!aes.init({ keyIv.data(), 24 }))
			{
				return Status::error("AES-192 key setup failed");
			}
			const std::uint8_t* baseIv = keyIv.data() + 24;

			std::vector<std::uint8_t> compressed;

			if (isUnsigned)
			{
				// DB_AuthLoad_InflateInit 0x004B5AC0: a CTR pass over what is left of the reader's first chunk
				compressed.assign(file.begin() + static_cast<std::ptrdiff_t>(bodyStart), file.end());
				const std::size_t encrypted = std::min(compressed.size(), FASTFILE_UNSIGNED_ENCRYPTED_SPAN - bodyStart);
				if (!aes.transform(baseIv, { compressed.data(), encrypted }))
				{
					return Status::error("AES-CTR failed");
				}
				out.authName.clear();
			}
			else
			{
				std::size_t pos = bodyStart;
				std::vector<std::uint8_t> auth(file.begin() + static_cast<std::ptrdiff_t>(pos), file.begin() + static_cast<std::ptrdiff_t>(pos + FASTFILE_CHUNK_SIZE));
				if (!aes.transform(baseIv, auth))
				{
					return Status::error("AES-CTR failed");
				}
				if (std::memcmp(auth.data(), AUTH_MAGIC, 8) != 0)
				{
					return Status::error("decrypted auth header has the wrong magic ( expected IWffs100 ): wrong client executable?");
				}
				pos += FASTFILE_CHUNK_SIZE;

				{
					const char* name = reinterpret_cast<const char*>(auth.data() + AUTH_SUBHEADER_OFFSET);
					out.authName.assign(name, strnlen(name, 32));
					std::uint8_t digest[32];
					out.subheaderHashValid = crypto::sha256({ auth.data() + AUTH_SUBHEADER_OFFSET, AUTH_SUBHEADER_LENGTH }, digest)
						&& !std::memcmp(digest, auth.data() + AUTH_SUBHEADER_HASH, 32);
				}

				compressed.reserve(file.size() - pos);
				std::vector<std::uint8_t> master, chunk;
				std::size_t masterIndex = 0, masterSlot = HASHES_PER_MASTER_BLOCK;
				bool haveMaster = false;

				// decrypts the chunk at pos with the IV taken from its expected hash and verifies full chunks
				const auto open_chunk = [&](const std::uint8_t* expectedHash, std::vector<std::uint8_t>& dst, const char* what) -> Status
				{
					const std::size_t length = std::min(FASTFILE_CHUNK_SIZE, file.size() - pos);
					dst.assign(file.begin() + static_cast<std::ptrdiff_t>(pos), file.begin() + static_cast<std::ptrdiff_t>(pos + length));
					if (!aes.transform(expectedHash, dst))
					{
						return Status::error("AES-CTR failed");
					}
					if (length == FASTFILE_CHUNK_SIZE)
					{
						std::uint8_t digest[32];
						if (!crypto::sha256(dst, digest) || std::memcmp(digest, expectedHash, 32) != 0)
						{
							out.hashFailures++;
							if (options.failOnHashMismatch)
							{
								return Status::error(std::string("authentication hash mismatch in ") + what + " at file offset " + hex_offset(pos));
							}
						}
					}
					pos += length;
					return {};
				};

				while (pos < file.size())
				{
					if (!haveMaster || masterSlot >= HASHES_PER_MASTER_BLOCK)
					{
						if (masterIndex >= AUTH_MASTER_HASH_COUNT)
						{
							return Status::error("file has more data than the auth header can describe ( 244 master blocks )");
						}
						std::uint8_t expected[32];
						std::memcpy(expected, auth.data() + AUTH_MASTER_HASHES + 32 * masterIndex, 32);
						if (const Status s = open_chunk(expected, master, "master block"); !s)
						{
							return s;
						}
						master.resize(FASTFILE_CHUNK_SIZE);
						masterIndex++;
						masterSlot = 0;
						haveMaster = true;
						out.masterBlocks++;
						continue;
					}
					std::uint8_t expected[32];
					std::memcpy(expected, master.data() + masterSlot * 32, 32);
					masterSlot++;
					if (const Status s = open_chunk(expected, chunk, "data chunk"); !s)
					{
						return s;
					}
					compressed.insert(compressed.end(), chunk.begin(), chunk.end());
					out.dataChunks++;
				}
			}

			out.compressedSize = compressed.size();

			if (const Status s = zlib_inflate(compressed, zone, compressed.size() * 3); !s)
			{
				return Status::error("zone decompression failed: " + s.message);
			}
			if (zone.size() < 40)
			{
				return Status::error("inflated zone is shorter than the XFile header");
			}
			const std::uint64_t declared = static_cast<std::uint64_t>(u32(zone.data())) + 40;
			if (zone.size() < declared)
			{
				return Status::error("inflated zone has " + std::to_string(zone.size()) + " bytes, XFile.size says " + std::to_string(declared));
			}
			zone.resize(static_cast<std::size_t>(declared));
			zone.shrink_to_fit();
			out.zoneSize = zone.size();
			return {};
		}
	}

	Status decode_fastfile(const std::span<const std::uint8_t> file, const ClientKey& key, std::vector<std::uint8_t>& zone, FastFileInfo* info,
		const FastFileOptions& options)
	{
		try
		{
			return decode_impl(file, key, zone, info, options);
		}
		catch (const std::exception& e)
		{
			zone.clear();
			return Status::error(std::string("fast file decode failed: ") + e.what());
		}
	}

	Status read_fastfile(const std::filesystem::path& path, const ClientKey& key, std::vector<std::uint8_t>& zone, FastFileInfo* info,
		const FastFileOptions& options)
	{
		std::vector<std::uint8_t> file;
		if (const Status s = read_file(path, file); !s)
		{
			return s;
		}
		Status s = decode_fastfile(file, key, zone, info, options);
		if (!s)
		{
			s.message = path.filename().string() + ": " + s.message;
		}
		return s;
	}
}
