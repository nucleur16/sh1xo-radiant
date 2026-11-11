#pragma once

// Windows CNG wrappers shared by crypto.cpp and iwd.cpp. Not part of the public interface.

#include "codol_render/iwd.hpp"

namespace codol::render::detail
{
	// AES in ECB mode with a fixed key; the building block of the CTR keystream ( LibTomCryptCtr.cs )
	class aes_ecb
	{
	public:
		aes_ecb() = default;
		~aes_ecb();
		aes_ecb(const aes_ecb&) = delete;
		aes_ecb& operator=(const aes_ecb&) = delete;

		bool init(const std::uint8_t* key, std::size_t key_size, std::string* error);
		// LibTomCryptCtr.cs:22-66 with bigEndianCounter: XORs `size` bytes with the keystream that starts at counter `iv`
		bool ctr_transform(const std::uint8_t* iv, std::uint8_t* data, std::size_t size, std::string* error) const;

	private:
		void* algorithm_ = nullptr;		// BCRYPT_ALG_HANDLE
		void* key_ = nullptr;			// BCRYPT_KEY_HANDLE
		std::vector<std::uint8_t> key_object_;
	};

	// Reads `size` bytes at `offset` of a file; false when the file is shorter
	bool read_file_range(const std::filesystem::path& path, std::uint64_t offset, std::size_t size, std::vector<std::uint8_t>& out);
	bool read_whole_file(const std::filesystem::path& path, std::vector<std::uint8_t>& out);
}
