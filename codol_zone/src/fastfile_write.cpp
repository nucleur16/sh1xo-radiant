// Fast file container writer: the unsigned form ( IWffu100 ) of version 334.
//
// Reference: CODOL-AssetExplorer FastFile/FastFileWriter.cs ( WriteUnsigned ) and the reader in fastfile.cpp.
//   0x00  "IWffu100"
//   0x08  u32 version ( 334 )
//   0x0C  u8  allowOnlineUpdate
//   0x0D  u32 FILETIME high, u32 FILETIME low
//   0x15  RSA-OAEP( SHA-256 ) block, 0x100 bytes: 24 byte AES-192 key + 16 byte IV, wrapped with the client's key
//   0x115 zlib stream of the zone. DB_AuthLoad_InflateInit 0x004B5AC0 decrypts ( AES-CTR, the IV above ) what is
//         left of the file reader's first 0x40000 byte chunk and nothing after it, so only those bytes are encrypted.

#include "codol_zone/fastfile.hpp"
#include "crypto_win.hpp"

#include <chrono>
#include <cstring>
#include <fstream>

namespace codol::zone
{
	namespace
	{
		void put_u32(std::uint8_t* p, const std::uint32_t v)
		{
			std::memcpy(p, &v, 4);
		}
	}

	Status encode_fastfile(const std::span<const std::uint8_t> zone, const ClientKey& key, std::vector<std::uint8_t>& file,
		const FastFileWriteOptions& options)
	try
	{
		file.clear();
		if (zone.size() < 40)
		{
			return Status::error("zone is shorter than the XFile header");
		}
		std::uint32_t declared;
		std::memcpy(&declared, zone.data(), 4);
		if (static_cast<std::uint64_t>(declared) + 40 != zone.size())
		{
			return Status::error("XFile.size + 40 does not match the zone length");
		}

		std::vector<std::uint8_t> keyIv = options.keyIv;
		if (keyIv.empty())
		{
			keyIv.resize(40);
			if (!crypto::random_bytes(keyIv))
			{
				return Status::error("the system random number generator failed");
			}
		}
		if (keyIv.size() != 40)
		{
			return Status::error("FastFileWriteOptions::keyIv must be 40 bytes ( 24 byte key + 16 byte IV )");
		}
		std::vector<std::uint8_t> block;
		if (const Status s = key.wrap(keyIv, block); !s)
		{
			return s;
		}

		std::vector<std::uint8_t> compressed;
		if (const Status s = zlib_deflate(zone, compressed, options.compression); !s)
		{
			return s;
		}

		if (options.exactFileSize)
		{
			const std::uint64_t plain = FASTFILE_HEADER_SIZE + FASTFILE_KEY_BLOCK_SIZE + compressed.size();
			if (plain > options.exactFileSize)
			{
				return Status::error("the fast file is " + std::to_string(plain) + " bytes, longer than the " + std::to_string(options.exactFileSize) + " asked for");
			}

			// Empty deflate blocks right behind the two byte zlib header ( the stream is byte aligned there ):
			//   stored block, no data:           00 00 00 FF FF                      5 bytes
			//   k empty fixed blocks + stored:   10 bits each, then 3 bits, padding to a byte, 00 00 FF FF
			// One fixed block in front makes 6 bytes, two make 7, three make 9. With those every length from 13
			// up can be put together ( and 5, 6, 7, 9 .. 12 ).
			const auto group = [](const int fixedBlocks, std::vector<std::uint8_t>& out)
			{
				std::uint64_t bits = 0;
				int count = 0;
				for (int i = 0; i < fixedBlocks; i++)
				{
					bits |= static_cast<std::uint64_t>(0b010) << count;		// BFINAL 0, BTYPE 01 ( fixed ), sent low bit first
					count += 3 + 7;											// + the 7 zero bits of the end of block code
				}
				count += 3;													// BFINAL 0, BTYPE 00 ( stored ): three zero bits
				for (int byte = 0; byte < (count + 7) / 8; byte++)
				{
					out.push_back(static_cast<std::uint8_t>(bits >> (byte * 8)));
				}
				out.insert(out.end(), { 0x00, 0x00, 0xFF, 0xFF });			// LEN 0, NLEN
			};

			std::uint64_t pad = options.exactFileSize - plain;
			std::vector<std::uint8_t> filler;
			if (pad)
			{
				switch (pad % 5)
				{
				case 1: if (pad >= 6) { group(1, filler); } break;
				case 2: if (pad >= 7) { group(2, filler); } break;
				case 3: if (pad >= 13) { group(1, filler); group(2, filler); } break;
				case 4: if (pad >= 9) { group(3, filler); } break;
				default: break;
				}
				if ((pad - filler.size()) % 5 != 0)
				{
					return Status::error("the fast file is " + std::to_string(pad) + " bytes short of the length asked for, which empty deflate blocks cannot make up");
				}
				for (std::uint64_t left = pad - filler.size(); left; left -= 5)
				{
					group(0, filler);
				}
				compressed.insert(compressed.begin() + 2, filler.begin(), filler.end());
			}
		}

		std::uint64_t fileTime = (static_cast<std::uint64_t>(options.fileTimeHigh) << 32) | options.fileTimeLow;
		if (!fileTime)
		{
			// FILETIME: 100 ns ticks since 1601-01-01
			const auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			fileTime = static_cast<std::uint64_t>(now) * 10 + 116444736000000000ull;
		}

		const std::size_t bodyStart = FASTFILE_HEADER_SIZE + FASTFILE_KEY_BLOCK_SIZE;
		file.resize(bodyStart + compressed.size());
		std::memcpy(file.data(), "IWffu100", 8);
		put_u32(file.data() + 8, FASTFILE_VERSION);
		file[12] = options.allowOnlineUpdate;
		put_u32(file.data() + 13, static_cast<std::uint32_t>(fileTime >> 32));
		put_u32(file.data() + 17, static_cast<std::uint32_t>(fileTime));
		std::memcpy(file.data() + FASTFILE_HEADER_SIZE, block.data(), FASTFILE_KEY_BLOCK_SIZE);
		std::memcpy(file.data() + bodyStart, compressed.data(), compressed.size());

		crypto::AesCtr aes;
		if (!aes.init({ keyIv.data(), 24 }))
		{
			return Status::error("AES-192 key setup failed");
		}
		const std::size_t encrypted = std::min(file.size(), FASTFILE_UNSIGNED_ENCRYPTED_SPAN) - bodyStart;
		if (!aes.transform(keyIv.data() + 24, { file.data() + bodyStart, encrypted }))
		{
			return Status::error("AES-CTR failed");
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status write_file(const std::filesystem::path& path, const std::span<const std::uint8_t> data)
	try
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			return Status::error("cannot create '" + path.string() + "'");
		}
		file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
		file.close();
		if (!file)
		{
			return Status::error("write to '" + path.string() + "' failed");
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status write_fastfile(const std::filesystem::path& path, const std::span<const std::uint8_t> zone, const ClientKey& key,
		const FastFileWriteOptions& options)
	{
		std::vector<std::uint8_t> file;
		if (const Status s = encode_fastfile(zone, key, file, options); !s)
		{
			return s;
		}
		return write_file(path, file);
	}
}
