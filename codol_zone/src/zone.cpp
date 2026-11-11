#include "zone_impl.hpp"

namespace codol::zone
{
	namespace
	{
		const std::string EMPTY_STRING;
		const std::vector<std::string> EMPTY_STRINGS;
		const XFileHeader EMPTY_HEADER;

		// offset of the name pointer inside the asset header
		std::uint32_t name_offset(const std::int32_t type)
		{
			switch (type)
			{
			case c1::ASSET_TYPE_IMAGE: return offsetof(c1::GfxImage, name);
			case c1::ASSET_TYPE_LOCALIZE_ENTRY: return offsetof(c1::LocalizeEntry, name);
			default: return 0;
			}
		}

		std::string name_key(const std::int32_t type, const std::string_view name)
		{
			std::string key = std::to_string(type);
			key.push_back(':');
			key.append(name);
			return key;
		}
	}

	std::string asset_type_name(const std::int32_t type)
	{
		if (type >= 0 && type < c1::ASSET_TYPE_COUNT)
		{
			return c1::XASSET_NAMES[type];
		}
		return "type_" + std::to_string(type);
	}

	std::int32_t asset_type_from_name(const std::string_view name)
	{
		for (std::int32_t i = 0; i < c1::ASSET_TYPE_COUNT; i++)
		{
			if (name == c1::XASSET_NAMES[i])
			{
				return i;
			}
		}
		return -1;
	}

	Zone::Zone() = default;
	Zone::~Zone() = default;
	Zone::Zone(Zone&&) noexcept = default;
	Zone& Zone::operator=(Zone&&) noexcept = default;

	Status Zone::load(std::vector<std::uint8_t> zoneBytes, std::string name)
	{
		return load_traced(zoneBytes, std::move(name), nullptr);
	}

	Status Zone::load_traced(const std::vector<std::uint8_t>& zoneBytes, std::string name, LoadTracer* tracer)
	{
		impl_.reset();
		try
		{
			auto impl = std::make_unique<Impl>();
			impl->name = std::move(name);
			run_zone_loaders(*impl, zoneBytes, tracer);

			impl->loaded = true;
			impl_ = std::move(impl);

			// name index: assets that have their data here ( a leading ',' marks a reference to another zone )
			for (const AssetRecord& record : impl_->records)
			{
				const std::string_view raw = c_string(get<zaddr>(record.header + name_offset(record.type)));
				if (!raw.empty() && raw[0] != ',')
				{
					impl_->byName.emplace(name_key(record.type, raw), record.header);
				}
			}
			return {};
		}
		catch (const std::exception& e)
		{
			impl_.reset();
			return Status::error(e.what());
		}
	}

	Status Zone::open(const std::filesystem::path& fastfile, const ClientKey& key, FastFileInfo* info)
	{
		try
		{
			std::vector<std::uint8_t> bytes;
			if (const Status s = read_fastfile(fastfile, key, bytes, info); !s)
			{
				return s;
			}
			Status s = load(std::move(bytes), fastfile.stem().string());
			if (!s)
			{
				s.message = fastfile.filename().string() + ": " + s.message;
			}
			return s;
		}
		catch (const std::exception& e)
		{
			return Status::error(e.what());
		}
	}

	bool Zone::loaded() const { return impl_ && impl_->loaded; }
	const std::string& Zone::name() const { return impl_ ? impl_->name : EMPTY_STRING; }
	const XFileHeader& Zone::header() const { return impl_ ? impl_->header : EMPTY_HEADER; }
	std::uint64_t Zone::stream_size() const { return impl_ ? impl_->streamSize : 0; }
	const std::vector<std::string>& Zone::warnings() const { return impl_ ? impl_->warnings : EMPTY_STRINGS; }
	const std::vector<std::string>& Zone::script_strings() const { return impl_ ? impl_->scriptStrings : EMPTY_STRINGS; }

	BlockUsage Zone::block_usage(const int block) const
	{
		return impl_ && block >= 0 && block < BLOCK_COUNT ? impl_->blocks[block] : BlockUsage{};
	}

	std::string_view Zone::script_string(const std::uint32_t handle) const
	{
		if (!impl_ || handle >= impl_->scriptStringHandles.size())
		{
			return {};
		}
		return impl_->scriptStringHandles[handle];
	}

	std::span<const XAssetEntry> Zone::assets() const
	{
		return impl_ ? std::span<const XAssetEntry>(impl_->entries) : std::span<const XAssetEntry>();
	}

	std::span<const AssetRecord> Zone::all_assets() const
	{
		return impl_ ? std::span<const AssetRecord>(impl_->records) : std::span<const AssetRecord>();
	}

	const AssetReference* Zone::reference(const zaddr address) const
	{
		if (!impl_)
		{
			return nullptr;
		}
		const auto it = impl_->references.find(address);
		return it == impl_->references.end() ? nullptr : &it->second;
	}

	std::string_view Zone::asset_name(const std::int32_t type, const zaddr header) const
	{
		if (!header)
		{
			return {};
		}
		if (const AssetReference* ref = reference(header))
		{
			return ref->name;
		}
		std::string_view name = c_string(get<zaddr>(header + name_offset(type)));
		if (!name.empty() && name[0] == ',')
		{
			name.remove_prefix(1);
		}
		return name;
	}

	std::string_view Zone::pointee_name(const std::int32_t type, const zaddr address) const
	{
		return asset_name(type, address);
	}

	zaddr Zone::find_asset(const std::int32_t type, const std::string_view name) const
	{
		if (!impl_)
		{
			return 0;
		}
		const auto it = impl_->byName.find(name_key(type, name));
		return it == impl_->byName.end() ? 0 : it->second;
	}

	std::span<const std::uint8_t> Zone::bytes(const zaddr address, const std::size_t size) const
	{
		if (!impl_ || !address || !size)
		{
			return {};
		}
		const std::uint8_t* p = impl_->mem.peek(address, size);
		return p ? std::span<const std::uint8_t>(p, size) : std::span<const std::uint8_t>();
	}

	std::string_view Zone::c_string(const zaddr address) const
	{
		if (!impl_ || !address)
		{
			return {};
		}
		const std::uint32_t region = address >> VMem::REGION_BITS;
		const std::uint32_t size = impl_->mem.region_size(region);
		const std::uint32_t offset = address & VMem::REGION_MASK;
		if (offset >= size)
		{
			return {};
		}
		const std::uint8_t* p = impl_->mem.peek(address, 1);
		const void* nul = std::memchr(p, 0, size - offset);
		if (!nul)
		{
			return {};
		}
		return { reinterpret_cast<const char*>(p), static_cast<std::size_t>(static_cast<const std::uint8_t*>(nul) - p) };
	}
}
