#include "crypto_internal.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>

namespace codol::render
{
	namespace
	{
		constexpr std::uint32_t FILLER = 0x01010101;	// IwdArchive.cs:23

		void wr_u32(std::uint8_t* p, const std::uint32_t v)
		{
			p[0] = static_cast<std::uint8_t>(v);
			p[1] = static_cast<std::uint8_t>(v >> 8);
			p[2] = static_cast<std::uint8_t>(v >> 16);
			p[3] = static_cast<std::uint8_t>(v >> 24);
		}
	}

	struct iwd_archive::impl
	{
		std::FILE* file = nullptr;
		mutable std::mutex gate;
		bool encrypted = false;
		detail::aes_ecb aes;
		std::int64_t bytes_before = 0;		// bytes in front of the zip ( 0 for the game's archives )
		std::int64_t file_size = 0;

		~impl()
		{
			if (file)
			{
				std::fclose(file);
			}
		}

		bool read_at(const std::int64_t offset, std::uint8_t* out, const std::size_t size) const
		{
			return offset >= 0 && _fseeki64(file, offset, SEEK_SET) == 0 && std::fread(out, 1, size, file) == size;
		}
	};

	iwd_archive::~iwd_archive() = default;

	const iwd_entry* iwd_archive::find(const std::string_view name) const
	{
		for (const auto& entry : entries_)
		{
			if (entry.name == name)
			{
				return &entry;
			}
		}
		return nullptr;
	}

	std::unique_ptr<iwd_archive> iwd_archive::open(const std::filesystem::path& path, const iwd_key* key, std::string* error)
	{
		try
		{
			std::unique_ptr<iwd_archive> archive(new iwd_archive());
			archive->impl_ = std::make_unique<impl>();
			impl& io = *archive->impl_;
			const std::string shown = path.string();
			io.file = _wfopen(path.c_str(), L"rb");
			if (!io.file)
			{
				set_error(error, "cannot open " + shown);
				return nullptr;
			}
			if (key)
			{
				io.encrypted = true;
				if (!io.aes.init(key->data(), key->size(), error))
				{
					return nullptr;
				}
			}
			// IwdArchive.cs:54-57: the cipher is seeded with the CRC of the archive's file name
			archive->file_name_ = path.filename().string();
			archive->name_hash_ = crc32(byte_span(reinterpret_cast<const std::uint8_t*>(archive->file_name_.data()), archive->file_name_.size()));

			// IwdArchive.cs:180-241 ( ReadCentralDirectory )
			if (_fseeki64(io.file, 0, SEEK_END) != 0 || (io.file_size = _ftelli64(io.file)) < 0)
			{
				set_error(error, "cannot size " + shown);
				return nullptr;
			}
			const std::size_t tail_length = static_cast<std::size_t>(std::min<std::int64_t>(io.file_size, 0xFFFF + 22));
			std::vector<std::uint8_t> tail(tail_length);
			if (!io.read_at(io.file_size - static_cast<std::int64_t>(tail_length), tail.data(), tail_length))
			{
				set_error(error, "cannot read " + shown);
				return nullptr;
			}
			std::int64_t eocd = -1;
			for (std::int64_t i = static_cast<std::int64_t>(tail_length) - 22; i >= 0; i--)
			{
				if (tail[static_cast<std::size_t>(i)] == 0x50 && tail[static_cast<std::size_t>(i) + 1] == 0x4B && tail[static_cast<std::size_t>(i) + 2] == 0x05
					&& tail[static_cast<std::size_t>(i) + 3] == 0x06)
				{
					eocd = i;
					break;
				}
			}
			if (eocd < 0)
			{
				set_error(error, shown + ": no zip end-of-central-directory record");
				return nullptr;
			}
			const std::uint8_t* e = tail.data() + eocd;
			const int total = rd_u16(e + 10);
			const std::uint32_t cd_size = rd_u32(e + 12);
			const std::uint32_t cd_offset = rd_u32(e + 16);
			const std::int64_t eocd_pos = io.file_size - static_cast<std::int64_t>(tail_length) + eocd;
			io.bytes_before = eocd_pos - (static_cast<std::int64_t>(cd_offset) + cd_size);
			if (io.bytes_before < 0)
			{
				set_error(error, shown + ": inconsistent central directory offset");
				return nullptr;
			}

			std::vector<std::uint8_t> cd(cd_size);
			if (cd_size != 0 && !io.read_at(io.bytes_before + cd_offset, cd.data(), cd_size))
			{
				set_error(error, shown + ": cannot read the central directory");
				return nullptr;
			}
			archive->entries_.reserve(static_cast<std::size_t>(total));
			std::size_t p = 0;
			std::uint8_t iv[16];
			for (int index = 0; index < total; index++)
			{
				if (cd.size() - p < 46 || p > cd.size() || rd_u32(cd.data() + p) != 0x02014B50)
				{
					set_error(error, shown + ": bad central directory header for entry " + std::to_string(index));
					return nullptr;
				}
				const std::uint8_t* h = cd.data() + p;
				iwd_entry entry;
				entry.index = index;
				entry.flags = rd_u16(h + 8);
				entry.method = rd_u16(h + 10);
				entry.crc32 = rd_u32(h + 16);
				entry.compressed_size = rd_u32(h + 20);
				entry.uncompressed_size = rd_u32(h + 24);
				const std::size_t name_length = rd_u16(h + 28);
				const std::size_t extra_length = rd_u16(h + 30);
				const std::size_t comment_length = rd_u16(h + 32);
				entry.local_header_offset = rd_u32(h + 42);
				if (cd.size() - p - 46 < name_length)
				{
					set_error(error, shown + ": truncated name of entry " + std::to_string(index));
					return nullptr;
				}
				std::vector<std::uint8_t> name(h + 46, h + 46 + name_length);
				if (io.encrypted)
				{
					wr_u32(iv, FILLER);
					wr_u32(iv + 4, archive->name_hash_);
					wr_u32(iv + 8, static_cast<std::uint32_t>(index));
					wr_u32(iv + 12, FILLER);
					if (!io.aes.ctr_transform(iv, name.data(), name.size(), error))
					{
						return nullptr;
					}
				}
				entry.name.assign(reinterpret_cast<const char*>(name.data()), name.size());		// Latin-1 bytes as they are
				archive->entries_.push_back(std::move(entry));
				p += 46 + name_length + extra_length + comment_length;
			}
			return archive;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return nullptr;
		}
	}

	bool iwd_archive::read_stored(const iwd_entry& entry, const std::uint32_t length, std::vector<std::uint8_t>& out, std::string* error) const
	{
		// IwdArchive.cs:145-178 ( ReadDecryptedStored )
		const impl& io = *impl_;
		std::int64_t data_pos;
		{
			std::lock_guard lock(io.gate);
			std::uint8_t local[30];
			if (!io.read_at(io.bytes_before + entry.local_header_offset, local, sizeof local) || rd_u32(local) != 0x04034B50)
			{
				set_error(error, file_name_ + ": entry " + std::to_string(entry.index) + " has no local file header at " + hex32(entry.local_header_offset));
				return false;
			}
			const int name_length = rd_u16(local + 26);
			const int extra_length = rd_u16(local + 28);
			data_pos = static_cast<std::int64_t>(entry.local_header_offset) + 30 + name_length + extra_length;
			if (io.bytes_before + data_pos + length > io.file_size)
			{
				set_error(error, file_name_ + ": entry '" + entry.name + "' reaches past the end of the archive");
				return false;
			}
			out.resize(length);
			if (length != 0 && !io.read_at(io.bytes_before + data_pos, out.data(), length))
			{
				set_error(error, file_name_ + ": cannot read entry '" + entry.name + "'");
				return false;
			}
		}
		if (!io.encrypted)
		{
			return true;
		}
		// the counter restarts every DATA_CHUNK_SIZE bytes, seeded with the chunk's offset in the zip
		std::uint8_t iv[16];
		for (std::uint32_t k = 0; k < length; k += DATA_CHUNK_SIZE)
		{
			wr_u32(iv, static_cast<std::uint32_t>(data_pos + k));
			wr_u32(iv + 4, name_hash_);
			wr_u32(iv + 8, FILLER);
			wr_u32(iv + 12, FILLER);
			if (!io.aes.ctr_transform(iv, out.data() + k, std::min(DATA_CHUNK_SIZE, length - k), error))
			{
				return false;
			}
		}
		return true;
	}

	bool iwd_archive::read(const iwd_entry& entry, std::vector<std::uint8_t>& out, std::string* error, const bool verify_crc) const
	{
		try
		{
			// IwdArchive.cs:70-96
			out.clear();
			if (entry.method == 0)
			{
				if (!read_stored(entry, entry.compressed_size, out, error))
				{
					return false;
				}
			}
			else if (entry.method == 8)
			{
				std::vector<std::uint8_t> stored;
				if (!read_stored(entry, entry.compressed_size, stored, error))
				{
					return false;
				}
				std::string inflate_error;
				if (!inflate_raw(stored, entry.uncompressed_size, out, &inflate_error))
				{
					set_error(error, file_name_ + ": entry '" + entry.name + "': " + inflate_error);
					return false;
				}
			}
			else
			{
				set_error(error, file_name_ + ": entry '" + entry.name + "' uses zip method " + std::to_string(entry.method));
				return false;
			}
			if (verify_crc && crc32(out) != entry.crc32)
			{
				set_error(error, file_name_ + ": CRC32 mismatch for '" + entry.name + "' (decryption or format error)");
				return false;
			}
			return true;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}

	bool iwd_archive::read_prefix(const iwd_entry& entry, const std::uint32_t count, std::vector<std::uint8_t>& out, std::string* error) const
	{
		try
		{
			// IwdArchive.cs:98-143
			out.clear();
			const std::uint32_t length = std::min(count, entry.uncompressed_size);
			if (entry.method == 0)
			{
				return read_stored(entry, length, out, error);
			}
			if (entry.method != 8)
			{
				set_error(error, file_name_ + ": entry '" + entry.name + "' uses zip method " + std::to_string(entry.method));
				return false;
			}
			std::uint32_t take = std::min(entry.compressed_size, DATA_CHUNK_SIZE);
			for (;;)
			{
				std::vector<std::uint8_t> stored;
				if (!read_stored(entry, take, stored, error))
				{
					return false;
				}
				std::string inflate_error;
				if (inflate_raw(stored, length, out, &inflate_error, true) && out.size() == length)
				{
					return true;
				}
				if (take >= entry.compressed_size)
				{
					set_error(error, file_name_ + ": entry '" + entry.name + "' inflates to fewer than " + std::to_string(length) + " bytes");
					return false;
				}
				take = static_cast<std::uint32_t>(std::min<std::uint64_t>(entry.compressed_size, static_cast<std::uint64_t>(take) * 4));
			}
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}

	// ---------------------------------------------------------------------------------------------------
	// iwd_archive_set ( IwdArchiveSet.cs )

	namespace
	{
		std::string lower_ascii(std::string text)
		{
			for (char& c : text)
			{
				if (c >= 'A' && c <= 'Z')
				{
					c = static_cast<char>(c + ('a' - 'A'));
				}
			}
			return text;
		}

		std::string upper_ascii(std::string text)
		{
			for (char& c : text)
			{
				if (c >= 'a' && c <= 'z')
				{
					c = static_cast<char>(c - ('a' - 'A'));
				}
			}
			return text;
		}
	}

	std::string iwd_archive_set::normalize(const std::string_view path)
	{
		// IwdArchiveSet.cs:36: backslash -> '/', no leading '/', lower case
		std::string text(path);
		std::replace(text.begin(), text.end(), '\\', '/');
		std::size_t start = 0;
		while (start < text.size() && text[start] == '/')
		{
			start++;
		}
		return lower_ascii(text.substr(start));
	}

	std::unique_ptr<iwd_archive_set> iwd_archive_set::open(const std::vector<std::filesystem::path>& directories, const iwd_key* key, const std::string_view prefix,
		const std::function<bool(const std::string& file_name)>& include_archive, std::vector<std::string>* warnings)
	{
		try
		{
			auto set = std::make_unique<iwd_archive_set>();
			std::vector<std::pair<std::string, hit>> found;		// in search order
			for (const auto& directory : directories)
			{
				// IwdArchiveSet.cs:50-54: localized_* first, then by file name descending ( compared without case )
				std::vector<std::pair<std::string, std::filesystem::path>> files;
				std::error_code ec;
				for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
				{
					const std::string name = it->path().filename().string();
					if (lower_ascii(it->path().extension().string()) == ".iwd" && (!include_archive || include_archive(name)))
					{
						files.emplace_back(upper_ascii(name), it->path());		// StringComparer.OrdinalIgnoreCase compares upper case
					}
				}
				std::sort(files.begin(), files.end(), [](const auto& a, const auto& b)
				{
					const bool a_localized = a.first.rfind("LOCALIZED_", 0) == 0, b_localized = b.first.rfind("LOCALIZED_", 0) == 0;
					return a_localized != b_localized ? a_localized : a.first > b.first;
				});
				for (const auto& file : files)
				{
					std::string error;
					auto archive = iwd_archive::open(file.second, key, &error);
					if (!archive)
					{
						if (warnings)
						{
							warnings->push_back("skipping " + file.second.filename().string() + ": " + error);
						}
						continue;
					}
					// IwdArchiveSet.cs:70-98
					int matches = 0;
					for (const auto& entry : archive->entries())
					{
						std::string path = normalize(entry.name);
						if (path.compare(0, prefix.size(), prefix) != 0)
						{
							continue;
						}
						matches++;
						found.emplace_back(std::move(path), hit{ archive.get(), &entry });
					}
					if (matches != 0)
					{
						set->archives_.push_back(std::move(archive));
					}
				}
			}
			// the first archive in search order wins a path
			std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
			for (auto& item : found)
			{
				if (!set->index_.empty() && set->index_.back().first == item.first)
				{
					set->duplicates_++;
					continue;
				}
				set->index_.push_back(std::move(item));
			}
			return set;
		}
		catch (...)
		{
			return nullptr;
		}
	}

	bool iwd_archive_set::find(const std::string_view game_path, hit& out) const
	{
		const std::string path = normalize(game_path);
		const auto it = std::lower_bound(index_.begin(), index_.end(), path, [](const auto& item, const std::string& p) { return item.first < p; });
		if (it == index_.end() || it->first != path)
		{
			return false;
		}
		out = it->second;
		return true;
	}

	bool iwd_archive_set::read(const std::string_view game_path, std::vector<std::uint8_t>& out, std::string* error) const
	{
		hit h;
		if (!find(game_path, h))
		{
			set_error(error, std::string(game_path) + " is not in the archives");
			return false;
		}
		return h.archive->read(*h.entry, out, error);
	}

	void iwd_archive_set::for_each(const std::function<void(const std::string& game_path, const hit&)>& visit) const
	{
		for (const auto& item : index_)
		{
			visit(item.first, item.second);
		}
	}

	bool image_archive_sets::find(const std::string_view image_name, iwd_archive_set::hit& out) const
	{
		// TextureExtractor.cs:123 ( GamePath ), :155-172 ( TryFindIwd )
		std::string_view name = image_name;
		while (!name.empty() && name.front() == ',')
		{
			name.remove_prefix(1);
		}
		const std::string path = "images/" + std::string(name) + ".iwi";
		return (images && images->find(path, out)) || (stripped && stripped->find(path, out));
	}

	bool image_archive_sets::read(const std::string_view image_name, std::vector<std::uint8_t>& out, std::string* error) const
	{
		iwd_archive_set::hit h;
		if (!find(image_name, h))
		{
			set_error(error, "no images/" + std::string(image_name) + ".iwi in the IWD archives");
			return false;
		}
		return h.archive->read(*h.entry, out, error);
	}

	bool open_image_archives(const std::filesystem::path& game_root, const iwd_key& key, image_archive_sets& out, std::string* error, std::vector<std::string>* warnings)
	{
		// GameInstallation.cs:343-385: main, then main_shared when it exists ( FS_Startup 0x617AC0 )
		std::vector<std::filesystem::path> directories = { game_root / "main" };
		std::error_code ec;
		if (std::filesystem::is_directory(game_root / "main_shared", ec))
		{
			directories.push_back(game_root / "main_shared");
		}
		const auto is_stripped = [](const std::string& file_name) { return lower_ascii(file_name).find("imagestripped") != std::string::npos; };
		out.images = iwd_archive_set::open(directories, &key, "images/", [&](const std::string& name) { return !is_stripped(name); }, warnings);
		out.stripped = iwd_archive_set::open(directories, &key, "images/", is_stripped, warnings);
		if (!out.images || out.images->archives().empty())
		{
			set_error(error, "no image archives in " + (game_root / "main").string());
			return false;
		}
		return true;
	}
}
