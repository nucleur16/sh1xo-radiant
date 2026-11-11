#pragma once

// Thin wrappers over Windows CNG ( bcrypt.dll ): AES-192 block encryption for the CTR keystream, SHA-256 and
// RSA-OAEP( SHA-256 ) with a PKCS#1 private key. No third party crypto code.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace codol::zone::crypto
{
	bool sha256(std::span<const std::uint8_t> data, std::uint8_t digest[32]);

	// system random number generator
	bool random_bytes(std::span<std::uint8_t> out);

	// libtomcrypt style CTR with a little endian counter ( ctr_setiv 0x00A6A900, ctr_encrypt 0x00A6BD60 )
	class AesCtr
	{
	public:
		AesCtr() = default;
		~AesCtr();
		AesCtr(const AesCtr&) = delete;
		AesCtr& operator=(const AesCtr&) = delete;

		bool init(std::span<const std::uint8_t> key);

		// XORs the keystream that starts at `iv` into data ( encryption and decryption are the same operation )
		bool transform(const std::uint8_t iv[16], std::span<std::uint8_t> data);

	private:
		void* alg_ = nullptr;
		void* key_ = nullptr;
		std::vector<std::uint8_t> keyObject_;
		std::vector<std::uint8_t> counters_;
	};

	// PKCS#1 RSAPrivateKey ( DER ) -> the big endian integers CNG wants
	struct RsaPrivateKey
	{
		std::vector<std::uint8_t> modulus, publicExponent, prime1, prime2;
		std::size_t derLength = 0;
	};

	bool parse_pkcs1_private_key(std::span<const std::uint8_t> der, RsaPrivateKey& key);

	class Rsa
	{
	public:
		Rsa() = default;
		~Rsa();
		Rsa(const Rsa&) = delete;
		Rsa& operator=(const Rsa&) = delete;

		bool import(const RsaPrivateKey& key);
		bool valid() const { return key_ != nullptr; }
		bool decrypt_oaep_sha256(std::span<const std::uint8_t> block, std::vector<std::uint8_t>& plain) const;

		// public key operation: plain -> one block of the modulus size
		bool encrypt_oaep_sha256(std::span<const std::uint8_t> plain, std::vector<std::uint8_t>& block) const;

	private:
		void* alg_ = nullptr;
		void* key_ = nullptr;
	};
}

namespace codol::zone
{
	struct Status;

	namespace detail
	{
		// zlib stream of a zone: the output buffer is sized from XFile.size ( its first four bytes ). May throw std::bad_alloc.
		Status zlib_inflate_zone(std::span<const std::uint8_t> compressed, std::vector<std::uint8_t>& zone);
	}
}
