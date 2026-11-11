#include "crypto_internal.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstdio>

namespace codol::render
{
	namespace
	{
		struct crc_table
		{
			std::uint32_t entries[256];

			constexpr crc_table() : entries()
			{
				// Crc32.cs:21-33
				for (std::uint32_t i = 0; i < 256; i++)
				{
					std::uint32_t c = i;
					for (int k = 0; k < 8; k++)
					{
						c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
					}
					entries[i] = c;
				}
			}
		};
		constexpr crc_table CRC_TABLE;

		bool nt_ok(const NTSTATUS status) { return status >= 0; }

		// ---- minimal DER reader for a PKCS#1 RSAPrivateKey --------------------------------------------------

		struct der_reader
		{
			const std::uint8_t* p;
			const std::uint8_t* end;

			// reads tag + length; `body` / `size` of the element
			bool element(const std::uint8_t tag, const std::uint8_t*& body, std::size_t& size)
			{
				if (end - p < 2 || p[0] != tag)
				{
					return false;
				}
				std::size_t length = p[1];
				p += 2;
				if (length & 0x80)
				{
					const std::size_t bytes = length & 0x7F;
					if (bytes == 0 || bytes > 4 || static_cast<std::size_t>(end - p) < bytes)
					{
						return false;
					}
					length = 0;
					for (std::size_t i = 0; i < bytes; i++)
					{
						length = (length << 8) | *p++;
					}
				}
				if (static_cast<std::size_t>(end - p) < length)
				{
					return false;
				}
				body = p;
				size = length;
				p += length;
				return true;
			}

			// an INTEGER as unsigned big endian bytes without leading zeros
			bool integer(std::vector<std::uint8_t>& value)
			{
				const std::uint8_t* body;
				std::size_t size;
				if (!element(0x02, body, size))
				{
					return false;
				}
				while (size > 1 && *body == 0)
				{
					body++;
					size--;
				}
				value.assign(body, body + size);
				return true;
			}
		};

		// FastFileKeyProvider.cs:101-118 ( TryImport ): a PKCS#1 RSAPrivateKey of 2048 bits that is exactly `der`
		bool import_rsa_private_key(const byte_span der, BCRYPT_ALG_HANDLE algorithm, BCRYPT_KEY_HANDLE& key)
		{
			der_reader outer{ der.data(), der.data() + der.size() };
			const std::uint8_t* body;
			std::size_t size;
			if (!outer.element(0x30, body, size) || outer.p != outer.end)
			{
				return false;
			}
			der_reader r{ body, body + size };
			std::vector<std::uint8_t> version, n, e, d, p, q;
			if (!r.integer(version) || !r.integer(n) || !r.integer(e) || !r.integer(d) || !r.integer(p) || !r.integer(q))
			{
				return false;
			}
			if (n.size() != 256 || p.size() > 128 || q.size() > 128 || e.size() > 8)
			{
				return false;	// not RSA-2048
			}
			// BCRYPT_RSAPRIVATE_BLOB: header, public exponent, modulus, prime1, prime2 ( big endian )
			BCRYPT_RSAKEY_BLOB header{};
			header.Magic = BCRYPT_RSAPRIVATE_MAGIC;
			header.BitLength = 2048;
			header.cbPublicExp = static_cast<ULONG>(e.size());
			header.cbModulus = static_cast<ULONG>(n.size());
			header.cbPrime1 = static_cast<ULONG>(p.size());
			header.cbPrime2 = static_cast<ULONG>(q.size());
			std::vector<std::uint8_t> blob(sizeof header);
			std::memcpy(blob.data(), &header, sizeof header);
			blob.insert(blob.end(), e.begin(), e.end());
			blob.insert(blob.end(), n.begin(), n.end());
			blob.insert(blob.end(), p.begin(), p.end());
			blob.insert(blob.end(), q.begin(), q.end());
			const bool ok = nt_ok(BCryptImportKeyPair(algorithm, nullptr, BCRYPT_RSAPRIVATE_BLOB, &key, blob.data(), static_cast<ULONG>(blob.size()), 0));
			SecureZeroMemory(blob.data(), blob.size());
			return ok;
		}

		// FastFileKeyProvider.cs:13-42: the key at its known offset, else the first importable DER key in the file
		bool find_rsa_private_key(const std::vector<std::uint8_t>& exe, BCRYPT_ALG_HANDLE algorithm, BCRYPT_KEY_HANDLE& key)
		{
			static constexpr std::uint8_t DER_PREFIX[] = { 0x30, 0x82, 0x04, 0xA3, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00 };
			const std::size_t known = static_cast<std::size_t>(build::EXE_RSA_PRIVATE_KEY_DER);
			if (exe.size() >= known + build::EXE_RSA_PRIVATE_KEY_DER_SIZE
				&& std::memcmp(exe.data() + known, DER_PREFIX, sizeof DER_PREFIX) == 0
				&& import_rsa_private_key(byte_span(exe.data() + known, build::EXE_RSA_PRIVATE_KEY_DER_SIZE), algorithm, key))
			{
				return true;
			}
			auto at = exe.begin();
			for (;;)
			{
				at = std::search(at, exe.end(), std::begin(DER_PREFIX), std::end(DER_PREFIX));
				if (at == exe.end())
				{
					return false;
				}
				const std::size_t start = static_cast<std::size_t>(at - exe.begin());
				const std::size_t length = 4 + ((static_cast<std::size_t>(exe[start + 2]) << 8) | exe[start + 3]);
				if (start + length <= exe.size() && import_rsa_private_key(byte_span(exe.data() + start, length), algorithm, key))
				{
					return true;
				}
				++at;
			}
		}
	}

	std::uint32_t crc32(const byte_span data, const std::uint32_t initial)
	{
		// Crc32.cs:10-19
		std::uint32_t crc = ~initial;
		for (const std::uint8_t b : data)
		{
			crc = CRC_TABLE.entries[(crc ^ b) & 0xFF] ^ (crc >> 8);
		}
		return ~crc;
	}

	namespace detail
	{
		aes_ecb::~aes_ecb()
		{
			if (key_)
			{
				BCryptDestroyKey(static_cast<BCRYPT_KEY_HANDLE>(key_));
			}
			if (algorithm_)
			{
				BCryptCloseAlgorithmProvider(static_cast<BCRYPT_ALG_HANDLE>(algorithm_), 0);
			}
			if (!key_object_.empty())
			{
				SecureZeroMemory(key_object_.data(), key_object_.size());
			}
		}

		bool aes_ecb::init(const std::uint8_t* key, const std::size_t key_size, std::string* error)
		{
			BCRYPT_ALG_HANDLE algorithm = nullptr;
			if (!nt_ok(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0)))
			{
				set_error(error, "CNG: the AES provider is not available");
				return false;
			}
			algorithm_ = algorithm;
			if (!nt_ok(BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
				sizeof(BCRYPT_CHAIN_MODE_ECB), 0)))
			{
				set_error(error, "CNG: cannot select AES-ECB");
				return false;
			}
			ULONG object_size = 0, written = 0;
			if (!nt_ok(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof object_size, &written, 0)))
			{
				set_error(error, "CNG: cannot size the AES key object");
				return false;
			}
			key_object_.resize(object_size);
			BCRYPT_KEY_HANDLE handle = nullptr;
			if (!nt_ok(BCryptGenerateSymmetricKey(algorithm, &handle, key_object_.data(), object_size, const_cast<PUCHAR>(key), static_cast<ULONG>(key_size), 0)))
			{
				set_error(error, "CNG: cannot create the AES key");
				return false;
			}
			key_ = handle;
			return true;
		}

		bool aes_ecb::ctr_transform(const std::uint8_t* iv, std::uint8_t* data, const std::size_t size, std::string* error) const
		{
			// LibTomCryptCtr.cs:22-66
			if (size == 0)
			{
				return true;
			}
			if (!key_)
			{
				set_error(error, "AES key is not initialised");
				return false;
			}
			const std::size_t blocks = (size + 15) / 16;
			std::vector<std::uint8_t> counters(blocks * 16);
			std::uint8_t counter[16];
			std::memcpy(counter, iv, 16);
			for (std::size_t b = 0; b < blocks; b++)
			{
				std::memcpy(counters.data() + b * 16, counter, 16);
				for (int i = 15; i >= 0; i--)	// big endian increment
				{
					if (++counter[i] != 0)
					{
						break;
					}
				}
			}
			ULONG written = 0;
			// ECB without padding encrypts in place; a CNG key handle may be used from several threads
			if (!nt_ok(BCryptEncrypt(static_cast<BCRYPT_KEY_HANDLE>(key_), counters.data(), static_cast<ULONG>(counters.size()), nullptr, nullptr, 0,
				counters.data(), static_cast<ULONG>(counters.size()), &written, 0)) || written != counters.size())
			{
				set_error(error, "CNG: AES encryption failed");
				return false;
			}
			for (std::size_t i = 0; i < size; i++)
			{
				data[i] ^= counters[i];
			}
			return true;
		}

		bool read_file_range(const std::filesystem::path& path, const std::uint64_t offset, const std::size_t size, std::vector<std::uint8_t>& out)
		{
			std::FILE* file = _wfopen(path.c_str(), L"rb");
			if (!file)
			{
				return false;
			}
			out.resize(size);
			const bool ok = _fseeki64(file, static_cast<long long>(offset), SEEK_SET) == 0 && std::fread(out.data(), 1, size, file) == size;
			std::fclose(file);
			return ok;
		}

		bool read_whole_file(const std::filesystem::path& path, std::vector<std::uint8_t>& out)
		{
			std::FILE* file = _wfopen(path.c_str(), L"rb");
			if (!file)
			{
				return false;
			}
			bool ok = _fseeki64(file, 0, SEEK_END) == 0;
			const long long size = ok ? _ftelli64(file) : -1;
			ok = ok && size >= 0 && _fseeki64(file, 0, SEEK_SET) == 0;
			if (ok)
			{
				out.resize(static_cast<std::size_t>(size));
				ok = std::fread(out.data(), 1, out.size(), file) == out.size();
			}
			std::fclose(file);
			return ok;
		}
	}

	bool aes_ctr_transform(const iwd_key& key, const std::uint8_t* iv, std::uint8_t* data, const std::size_t size, std::string* error)
	{
		try
		{
			detail::aes_ecb aes;
			return aes.init(key.data(), key.size(), error) && aes.ctr_transform(iv, data, size, error);
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}

	bool derive_iwd_key(const std::filesystem::path& exe, iwd_key& key, std::string* error)
	{
		try
		{
			std::vector<std::uint8_t> image;
			if (!detail::read_whole_file(exe, image))
			{
				set_error(error, "cannot read " + exe.string());
				return false;
			}
			// FastFileKeyProvider.cs:68-77
			const std::size_t blob_offset = static_cast<std::size_t>(build::EXE_IWD_KEY_BLOB);
			if (image.size() < blob_offset + 256)
			{
				set_error(error, exe.string() + " is too small to contain the IWD key blob");
				return false;
			}
			BCRYPT_ALG_HANDLE algorithm = nullptr;
			if (!nt_ok(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0)))
			{
				set_error(error, "CNG: the RSA provider is not available");
				return false;
			}
			BCRYPT_KEY_HANDLE rsa = nullptr;
			bool ok = find_rsa_private_key(image, algorithm, rsa);
			if (!ok)
			{
				set_error(error, "no PKCS#1 RSA-2048 private key found in " + exe.string());
			}
			else
			{
				// FastFileKeyProvider.cs:78-93: RSAEncryptionPadding.OaepSHA256, the result must be 24 bytes
				BCRYPT_OAEP_PADDING_INFO padding{};
				padding.pszAlgId = BCRYPT_SHA256_ALGORITHM;
				std::uint8_t plain[256];
				ULONG written = 0;
				ok = nt_ok(BCryptDecrypt(rsa, image.data() + blob_offset, 256, &padding, nullptr, 0, plain, sizeof plain, &written, BCRYPT_PAD_OAEP));
				if (!ok)
				{
					set_error(error, "the IWD key blob did not decrypt; the executable is not the 1.1.1.13 client");
				}
				else if (written != key.size())
				{
					set_error(error, "the IWD key blob decrypted to " + std::to_string(written) + " bytes instead of 24");
					ok = false;
				}
				else
				{
					std::memcpy(key.data(), plain, key.size());
				}
				SecureZeroMemory(plain, sizeof plain);
				BCryptDestroyKey(rsa);
			}
			BCryptCloseAlgorithmProvider(algorithm, 0);
			return ok;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}
}
