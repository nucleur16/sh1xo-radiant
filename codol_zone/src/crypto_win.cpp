#include "crypto_win.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <cstring>

#pragma comment(lib, "bcrypt.lib")

namespace codol::zone::crypto
{
	namespace
	{
		bool ok(const NTSTATUS status)
		{
			return status >= 0;
		}
	}

	bool sha256(const std::span<const std::uint8_t> data, std::uint8_t digest[32])
	{
		BCRYPT_ALG_HANDLE alg = nullptr;
		if (!ok(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
		{
			return false;
		}
		BCRYPT_HASH_HANDLE hash = nullptr;
		bool good = ok(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0));
		if (good)
		{
			good = ok(BCryptHashData(hash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0))
				&& ok(BCryptFinishHash(hash, digest, 32, 0));
			BCryptDestroyHash(hash);
		}
		BCryptCloseAlgorithmProvider(alg, 0);
		return good;
	}

	// ------------------------------------------------------------------------------------------

	AesCtr::~AesCtr()
	{
		if (key_)
		{
			BCryptDestroyKey(key_);
		}
		if (alg_)
		{
			BCryptCloseAlgorithmProvider(alg_, 0);
		}
	}

	bool AesCtr::init(const std::span<const std::uint8_t> key)
	{
		if (alg_ || (key.size() != 16 && key.size() != 24 && key.size() != 32))
		{
			return false;
		}
		BCRYPT_ALG_HANDLE alg = nullptr;
		if (!ok(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
		{
			return false;
		}
		alg_ = alg;
		if (!ok(BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
			sizeof(BCRYPT_CHAIN_MODE_ECB), 0)))
		{
			return false;
		}
		ULONG objectSize = 0, got = 0;
		if (!ok(BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &got, 0)))
		{
			return false;
		}
		keyObject_.resize(objectSize);
		BCRYPT_KEY_HANDLE handle = nullptr;
		if (!ok(BCryptGenerateSymmetricKey(alg, &handle, keyObject_.data(), objectSize, const_cast<PUCHAR>(key.data()),
			static_cast<ULONG>(key.size()), 0)))
		{
			return false;
		}
		key_ = handle;
		return true;
	}

	bool AesCtr::transform(const std::uint8_t iv[16], const std::span<std::uint8_t> data)
	{
		if (!key_)
		{
			return false;
		}
		if (data.empty())
		{
			return true;
		}
		const std::size_t blocks = (data.size() + 15) / 16;
		counters_.resize(blocks * 16);

		std::uint8_t counter[16];
		std::memcpy(counter, iv, 16);
		for (std::size_t b = 0; b < blocks; b++)
		{
			std::memcpy(counters_.data() + b * 16, counter, 16);
			for (int i = 0; i < 16; i++)
			{
				if (++counter[i] != 0)
				{
					break;
				}
			}
		}

		// ECB encrypt the counter blocks in place: that is the keystream
		ULONG written = 0;
		if (!ok(BCryptEncrypt(key_, counters_.data(), static_cast<ULONG>(counters_.size()), nullptr, nullptr, 0, counters_.data(),
			static_cast<ULONG>(counters_.size()), &written, 0)) || written != counters_.size())
		{
			return false;
		}
		for (std::size_t i = 0; i < data.size(); i++)
		{
			data[i] ^= counters_[i];
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------

	namespace
	{
		struct Der
		{
			const std::uint8_t* p;
			const std::uint8_t* end;

			// reads tag + length, leaves p at the content
			bool header(const std::uint8_t tag, std::size_t& length)
			{
				if (end - p < 2 || *p != tag)
				{
					return false;
				}
				p++;
				std::size_t len = *p++;
				if (len & 0x80)
				{
					const std::size_t n = len & 0x7F;
					if (n == 0 || n > 4 || static_cast<std::size_t>(end - p) < n)
					{
						return false;
					}
					len = 0;
					for (std::size_t i = 0; i < n; i++)
					{
						len = (len << 8) | *p++;
					}
				}
				if (static_cast<std::size_t>(end - p) < len)
				{
					return false;
				}
				length = len;
				return true;
			}

			// INTEGER as unsigned big endian bytes without leading zeros
			bool integer(std::vector<std::uint8_t>& value)
			{
				std::size_t len = 0;
				if (!header(0x02, len) || len == 0)
				{
					return false;
				}
				const std::uint8_t* s = p;
				p += len;
				while (len > 1 && *s == 0)
				{
					s++;
					len--;
				}
				value.assign(s, s + len);
				return true;
			}
		};
	}

	bool parse_pkcs1_private_key(const std::span<const std::uint8_t> der, RsaPrivateKey& key)
	{
		Der d{ der.data(), der.data() + der.size() };
		std::size_t seq = 0;
		if (!d.header(0x30, seq))
		{
			return false;
		}
		d.end = d.p + seq;
		key.derLength = static_cast<std::size_t>(d.end - der.data());

		std::vector<std::uint8_t> version, privateExponent;
		return d.integer(version) && version.size() == 1 && version[0] == 0
			&& d.integer(key.modulus) && d.integer(key.publicExponent) && d.integer(privateExponent)
			&& d.integer(key.prime1) && d.integer(key.prime2);
	}

	Rsa::~Rsa()
	{
		if (key_)
		{
			BCryptDestroyKey(key_);
		}
		if (alg_)
		{
			BCryptCloseAlgorithmProvider(alg_, 0);
		}
	}

	bool Rsa::import(const RsaPrivateKey& key)
	{
		if (alg_ || key.modulus.empty() || key.prime1.empty() || key.prime2.empty() || key.publicExponent.empty())
		{
			return false;
		}
		BCRYPT_ALG_HANDLE alg = nullptr;
		if (!ok(BCryptOpenAlgorithmProvider(&alg, BCRYPT_RSA_ALGORITHM, nullptr, 0)))
		{
			return false;
		}
		alg_ = alg;

		BCRYPT_RSAKEY_BLOB header{};
		header.Magic = BCRYPT_RSAPRIVATE_MAGIC;
		header.BitLength = static_cast<ULONG>(key.modulus.size() * 8);
		header.cbPublicExp = static_cast<ULONG>(key.publicExponent.size());
		header.cbModulus = static_cast<ULONG>(key.modulus.size());
		header.cbPrime1 = static_cast<ULONG>(key.prime1.size());
		header.cbPrime2 = static_cast<ULONG>(key.prime2.size());

		std::vector<std::uint8_t> blob(sizeof(header));
		std::memcpy(blob.data(), &header, sizeof(header));
		blob.insert(blob.end(), key.publicExponent.begin(), key.publicExponent.end());
		blob.insert(blob.end(), key.modulus.begin(), key.modulus.end());
		blob.insert(blob.end(), key.prime1.begin(), key.prime1.end());
		blob.insert(blob.end(), key.prime2.begin(), key.prime2.end());

		BCRYPT_KEY_HANDLE handle = nullptr;
		if (!ok(BCryptImportKeyPair(alg, nullptr, BCRYPT_RSAPRIVATE_BLOB, &handle, blob.data(), static_cast<ULONG>(blob.size()), 0)))
		{
			return false;
		}
		key_ = handle;
		return true;
	}

	bool Rsa::decrypt_oaep_sha256(const std::span<const std::uint8_t> block, std::vector<std::uint8_t>& plain) const
	{
		if (!key_)
		{
			return false;
		}
		BCRYPT_OAEP_PADDING_INFO padding{};
		padding.pszAlgId = BCRYPT_SHA256_ALGORITHM;

		plain.resize(block.size());
		ULONG written = 0;
		if (!ok(BCryptDecrypt(key_, const_cast<PUCHAR>(block.data()), static_cast<ULONG>(block.size()), &padding, nullptr, 0,
			plain.data(), static_cast<ULONG>(plain.size()), &written, BCRYPT_PAD_OAEP)))
		{
			plain.clear();
			return false;
		}
		plain.resize(written);
		return true;
	}
}
