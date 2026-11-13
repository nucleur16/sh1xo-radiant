// MaterialLibrary ( what the shipped materials teach about each technique set ), compile_material, extract_material.

#include "material_state.hpp"

#include "codol_zone/assets.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>

namespace codol::assets
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr int REF_SLOTS[] = { 10, 5, 4 };		// lit, emissive, unlit ( codol_render renderer_technique )

		template <typename K>
		K most_common(const std::map<K, std::uint32_t>& counts, const K& fallback)
		{
			K best = fallback;
			std::uint32_t most = 0;
			for (const auto& [key, count] : counts)
			{
				if (count > most)
				{
					most = count;
					best = key;
				}
			}
			return best;
		}

		// zones every client has loaded whatever map or mode runs
		bool always_loaded(const std::string& stem)
		{
			for (const char* prefix : { "code_", "common_mp", "patch_code_", "patch_common_mp", "codol_common", "localized_code_", "localized_common_mp", "ui_mp",
				"patch_ui_mp", "patch_localized_code_", "patch_localized_common_mp" })
			{
				if (stem.rfind(prefix, 0) == 0)
				{
					return true;
				}
			}
			return false;
		}

		std::string hex_of(const std::uint8_t* data, const std::size_t size)
		{
			std::string out;
			char t[4];
			for (std::size_t k = 0; k < size; k++)
			{
				std::snprintf(t, sizeof(t), "%02X", data[k]);
				out += t;
			}
			return out;
		}

		std::string counts_text(const std::map<std::uint8_t, std::uint32_t>& counts)
		{
			std::string out;
			for (const auto& [key, count] : counts)
			{
				out += (out.empty() ? "" : ",") + std::to_string(key) + ":" + std::to_string(count);
			}
			return out.empty() ? "-" : out;
		}

		template <typename K>
		void parse_counts(const std::string& text, std::map<K, std::uint32_t>& counts)
		{
			if (text == "-")
			{
				return;
			}
			std::stringstream in(text);
			std::string item;
			while (std::getline(in, item, ','))
			{
				const std::size_t colon = item.find(':');
				if (colon != std::string::npos)
				{
					counts[static_cast<K>(std::stoul(item.substr(0, colon)))] = static_cast<std::uint32_t>(std::stoul(item.substr(colon + 1)));
				}
			}
		}
	}

	std::uint32_t material_name_hash(const std::string_view name)
	{
		// "anything#XXXXXXXX": a name that is only known as its hash
		if (const std::size_t mark = name.find('#'); mark != std::string_view::npos && name.size() - mark == 9)
		{
			return static_cast<std::uint32_t>(std::strtoul(std::string(name.substr(mark + 1)).c_str(), nullptr, 16));
		}
		std::uint32_t hash = 0;
		for (const char c : name)
		{
			const auto u = static_cast<unsigned char>(c);
			hash = static_cast<std::uint32_t>(u >= 'A' && u <= 'Z' ? u + 32 : u) ^ (hash * 33u);
		}
		return hash;
	}

	int LibraryState::ref_slot() const
	{
		for (const int slot : REF_SLOTS)
		{
			if (entries[static_cast<std::size_t>(slot)] < table.size())
			{
				return slot;
			}
		}
		for (int slot = 0; slot < 48; slot++)
		{
			if (entries[static_cast<std::size_t>(slot)] < table.size())
			{
				return slot;
			}
		}
		return -1;
	}

	StateBits LibraryState::ref() const
	{
		const int slot = ref_slot();
		return slot < 0 ? 0 : table[entries[static_cast<std::size_t>(slot)]];
	}

	// ---- library ----------------------------------------------------------------------------------------------

	struct MaterialLibrary::Raw
	{
		struct Techset
		{
			std::set<std::string> seen;
			std::map<std::string, std::uint32_t> textureShapes;
			std::map<std::uint32_t, std::map<std::uint8_t, std::uint32_t>> samplerStates;
			std::map<std::uint32_t, std::map<std::string, std::uint32_t>> defaultImages;
			std::map<std::string, std::uint32_t> constantShapes;
			std::map<std::uint32_t, std::map<std::array<float, 4>, std::uint32_t>> constantValues;
			std::map<std::string, LibraryState> states;
			std::string commonZone, donorZone;
			std::uint64_t donorSize = 0;
		};
		std::map<std::string, Techset> techsets;
	};

	void MaterialLibrary::add(const zone::Zone& zone, const zone::zaddr material, const bool)
	{
		zone::MaterialView view;
		if (!zone::read_material(zone, material, view) || view.isReference || view.techsetName.empty())
		{
			return;
		}
		if (!raw_)
		{
			raw_ = std::make_shared<Raw>();
		}
		Raw::Techset& t = raw_->techsets[std::string(view.techsetName)];
		if (!t.seen.insert(std::string(view.name)).second)
		{
			return;		// the same material in another zone
		}
		std::string shape;
		for (const zone::MaterialTexture& texture : view.textures)
		{
			shape.append(reinterpret_cast<const char*>(&texture.raw.nameHash), 4);
			shape += texture.raw.nameStart;
			shape += texture.raw.nameEnd;
			shape += static_cast<char>(texture.raw.semantic);
			t.samplerStates[texture.raw.nameHash][texture.raw.samplerState]++;
			if (!texture.imageName.empty() && texture.imageName.front() == '$')
			{
				t.defaultImages[texture.raw.nameHash][std::string(texture.imageName)]++;
			}
		}
		t.textureShapes[shape]++;
		shape.clear();
		for (const c1::MaterialConstantDef& c : view.constants)
		{
			shape.append(reinterpret_cast<const char*>(&c.nameHash), 4);
			shape.append(c.name, 12);
			t.constantValues[c.nameHash][{ c.literal[0], c.literal[1], c.literal[2], c.literal[3] }]++;
		}
		t.constantShapes[shape]++;

		LibraryState s;
		std::memcpy(s.entries.data(), view.raw.stateBitsEntry, 48);
		for (const auto& bits : view.stateBits)
		{
			s.table.push_back(static_cast<StateBits>(bits[0]) | (static_cast<StateBits>(bits[1]) << 32));
		}
		s.stateFlags = view.raw._unk75;
		std::string key = hex_of(s.entries.data(), 48) + hex_of(reinterpret_cast<const std::uint8_t*>(s.table.data()), s.table.size() * 8) + hex_of(&s.stateFlags, 1);
		LibraryState& state = t.states.emplace(std::move(key), std::move(s)).first->second;
		state.count++;
		state.sortKeys[view.raw.sortKey]++;
		state.gameFlags[view.raw.gameFlags]++;
		state.cameraRegions[view.raw.cameraRegion]++;
		state.sortToRegion[static_cast<std::uint16_t>((view.raw.sortKey << 8) | view.raw.cameraRegion)]++;
		sortRegions_[view.raw.sortKey][view.raw.cameraRegion]++;
	}

	void MaterialLibrary::finish()
	{
		if (!raw_)
		{
			return;
		}
		for (auto& [name, raw] : raw_->techsets)
		{
			LibraryTechset t;
			t.name = name;
			t.commonZone = raw.commonZone;
			t.donorZone = raw.donorZone;
			t.donorSize = raw.donorSize;
			t.materials = static_cast<std::uint32_t>(raw.seen.size());
			const std::string textures = most_common(raw.textureShapes, std::string());
			for (std::size_t k = 0; k + 7 <= textures.size(); k += 7)
			{
				LibrarySampler s;
				std::memcpy(&s.nameHash, textures.data() + k, 4);
				s.nameStart = textures[k + 4];
				s.nameEnd = textures[k + 5];
				s.semantic = static_cast<std::uint8_t>(textures[k + 6]);
				s.samplerState = most_common(raw.samplerStates[s.nameHash], std::uint8_t(0x0B));
				s.defaultImage = most_common(raw.defaultImages[s.nameHash], std::string());
				t.samplers.push_back(s);
			}
			const std::string constants = most_common(raw.constantShapes, std::string());
			for (std::size_t k = 0; k + 16 <= constants.size(); k += 16)
			{
				LibraryConstant c;
				std::memcpy(&c.nameHash, constants.data() + k, 4);
				std::memcpy(c.name, constants.data() + k + 4, 12);
				const std::array<float, 4> v = most_common(raw.constantValues[c.nameHash], std::array<float, 4>{});
				std::memcpy(c.value, v.data(), sizeof(c.value));
				t.constants.push_back(c);
			}
			for (auto& [key, state] : raw.states)
			{
				t.states.push_back(state);
			}
			std::stable_sort(t.states.begin(), t.states.end(), [](const LibraryState& a, const LibraryState& b) { return a.count > b.count; });
			techsets_[name] = std::move(t);
		}
		raw_.reset();
	}

	Status MaterialLibrary::scan(const fs::path& install, const zone::ClientKey& key)
	try
	{
		techsets_.clear();
		sortRegions_.clear();
		commonImages_.clear();
		raw_ = std::make_shared<Raw>();
		std::vector<fs::path> files;
		for (const char* sub : { "client_shipRetail/chinese", "client_shipRetail/patch" })
		{
			std::error_code ec;
			for (const auto& e : fs::directory_iterator(install / sub, ec))
			{
				if (e.is_regular_file() && e.path().extension() == ".ff")
				{
					files.push_back(e.path());
				}
			}
		}
		std::sort(files.begin(), files.end());
		if (files.empty())
		{
			return Status::error("no fast files below " + (install / "client_shipRetail").string());
		}
		for (const fs::path& file : files)
		{
			zone::Zone z;
			if (!z.open(file, key))
			{
				continue;
			}
			const std::string stem = file.stem().string();
			const bool common = always_loaded(stem);
			std::error_code ec;
			const std::uint64_t size = fs::file_size(file, ec);
			for (const zone::AssetRecord& r : z.all_assets())
			{
				if (r.type == c1::ASSET_TYPE_MATERIAL && !z.reference(r.header))
				{
					add(z, r.header, common);
				}
				else if (r.type == c1::ASSET_TYPE_IMAGE && common && !z.reference(r.header))
				{
					zone::ImageView image;
					if (zone::read_image(z, r.header, image) && !image.isReference && !image.name.empty())
					{
						commonImages_.emplace(std::string(image.name), stem);
					}
				}
			}
			for (const zone::XAssetEntry& e : z.assets())
			{
				zone::TechniqueSetView view;
				if (e.type != c1::ASSET_TYPE_TECHNIQUE_SET || !zone::read_techset(z, e.header, view) || view.isReference)
				{
					continue;
				}
				Raw::Techset& t = raw_->techsets[std::string(view.name)];
				if (common && t.commonZone.empty())
				{
					t.commonZone = stem;
				}
				if (t.donorZone.empty() || size < t.donorSize)
				{
					t.donorZone = fs::relative(file, install, ec).generic_string();
					t.donorSize = size;
				}
			}
		}
		install_ = install.generic_string();
		finish();
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status MaterialLibrary::save(const fs::path& file) const
	try
	{
		std::ofstream out(file, std::ios::binary);
		if (!out)
		{
			return Status::error("cannot write " + file.string());
		}
		char line[512];
		out << "codol_matlib 2\n" << install_ << "\n";
		for (const auto& [sort, regions] : sortRegions_)
		{
			for (const auto& [region, count] : regions)
			{
				out << "R " << sort << " " << region << " " << count << "\n";
			}
		}
		for (const auto& [name, zoneName] : commonImages_)
		{
			out << "I " << name << " " << zoneName << "\n";
		}
		for (const auto& [name, t] : techsets_)
		{
			out << "T " << name << " " << t.materials << " " << (t.commonZone.empty() ? "-" : t.commonZone) << " " << (t.donorZone.empty() ? "-" : t.donorZone) << " "
				<< t.donorSize << "\n";
			for (const LibrarySampler& s : t.samplers)
			{
				std::snprintf(line, sizeof(line), "S %08X %d %d %u %u %s\n", s.nameHash, s.nameStart, s.nameEnd, s.semantic, s.samplerState,
					s.defaultImage.empty() ? "-" : s.defaultImage.c_str());
				out << line;
			}
			for (const LibraryConstant& c : t.constants)
			{
				std::snprintf(line, sizeof(line), "C %08X %s %.9g %.9g %.9g %.9g\n", c.nameHash, hex_of(reinterpret_cast<const std::uint8_t*>(c.name), 12).c_str(),
					c.value[0], c.value[1], c.value[2], c.value[3]);
				out << line;
			}
			for (const LibraryState& s : t.states)
			{
				std::string sortRegion;
				for (const auto& [pair, count] : s.sortToRegion)
				{
					sortRegion += (sortRegion.empty() ? "" : ",") + std::to_string(pair) + ":" + std::to_string(count);
				}
				out << "V " << static_cast<int>(s.stateFlags) << " " << s.count << " " << hex_of(s.entries.data(), 48) << " "
					<< (s.table.empty() ? std::string("-") : hex_of(reinterpret_cast<const std::uint8_t*>(s.table.data()), s.table.size() * 8)) << " "
					<< counts_text(s.sortKeys) << " " << counts_text(s.gameFlags) << " " << counts_text(s.cameraRegions) << " " << (sortRegion.empty() ? "-" : sortRegion) << "\n";
			}
		}
		return out.good() ? Status{} : Status::error("cannot write " + file.string());
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status MaterialLibrary::load(const fs::path& file)
	try
	{
		std::ifstream in(file, std::ios::binary);
		std::string line;
		if (!in || !std::getline(in, line) || line != "codol_matlib 2" || !std::getline(in, install_))
		{
			return Status::error(file.string() + " is not a material library of this version");
		}
		techsets_.clear();
		sortRegions_.clear();
		commonImages_.clear();
		LibraryTechset* current = nullptr;
		const auto unhex = [](const std::string& text, std::uint8_t* out, const std::size_t size)
		{
			for (std::size_t k = 0; k < size && k * 2 + 1 < text.size(); k++)
			{
				out[k] = static_cast<std::uint8_t>(std::stoul(text.substr(k * 2, 2), nullptr, 16));
			}
		};
		while (std::getline(in, line))
		{
			std::stringstream s(line);
			std::string tag;
			s >> tag;
			if (tag == "R")
			{
				int sort = 0, region = 0;
				std::uint32_t count = 0;
				s >> sort >> region >> count;
				sortRegions_[sort][region] = count;
			}
			else if (tag == "I")
			{
				std::string name, zoneName;
				s >> name >> zoneName;
				commonImages_[name] = zoneName;
			}
			else if (tag == "T")
			{
				LibraryTechset t;
				s >> t.name >> t.materials >> t.commonZone >> t.donorZone >> t.donorSize;
				t.commonZone = t.commonZone == "-" ? "" : t.commonZone;
				t.donorZone = t.donorZone == "-" ? "" : t.donorZone;
				current = &(techsets_[t.name] = std::move(t));
			}
			else if (tag == "S" && current)
			{
				LibrarySampler sampler;
				std::string hash;
				int start = 0, end = 0, semantic = 0, samplerState = 0;
				s >> hash >> start >> end >> semantic >> samplerState >> sampler.defaultImage;
				sampler.nameHash = static_cast<std::uint32_t>(std::stoul(hash, nullptr, 16));
				sampler.nameStart = static_cast<char>(start);
				sampler.nameEnd = static_cast<char>(end);
				sampler.semantic = static_cast<std::uint8_t>(semantic);
				sampler.samplerState = static_cast<std::uint8_t>(samplerState);
				sampler.defaultImage = sampler.defaultImage == "-" ? "" : sampler.defaultImage;
				current->samplers.push_back(sampler);
			}
			else if (tag == "C" && current)
			{
				LibraryConstant c;
				std::string hash, name;
				s >> hash >> name >> c.value[0] >> c.value[1] >> c.value[2] >> c.value[3];
				c.nameHash = static_cast<std::uint32_t>(std::stoul(hash, nullptr, 16));
				unhex(name, reinterpret_cast<std::uint8_t*>(c.name), 12);
				current->constants.push_back(c);
			}
			else if (tag == "V" && current)
			{
				LibraryState state;
				int flags = 0;
				std::string entries, table, sorts, game, regions, sortRegion;
				s >> flags >> state.count >> entries >> table >> sorts >> game >> regions >> sortRegion;
				state.stateFlags = static_cast<std::uint8_t>(flags);
				unhex(entries, state.entries.data(), 48);
				if (table != "-")
				{
					state.table.resize(table.size() / 16);
					unhex(table, reinterpret_cast<std::uint8_t*>(state.table.data()), state.table.size() * 8);
				}
				parse_counts(sorts, state.sortKeys);
				parse_counts(game, state.gameFlags);
				parse_counts(regions, state.cameraRegions);
				parse_counts(sortRegion, state.sortToRegion);
				current->states.push_back(std::move(state));
			}
		}
		return techsets_.empty() ? Status::error(file.string() + " holds no technique sets") : Status{};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status MaterialLibrary::open(const fs::path& install, const zone::ClientKey& key, const fs::path& cache)
	{
		if (!cache.empty() && load(cache) && install_ == install.generic_string())
		{
			return {};
		}
		if (const Status s = scan(install, key); !s)
		{
			return s;
		}
		return cache.empty() ? Status{} : save(cache);
	}

	const LibraryTechset* MaterialLibrary::find(const std::string_view techset) const
	{
		const auto it = techsets_.find(techset);
		return it == techsets_.end() ? nullptr : &it->second;
	}

	int MaterialLibrary::region_of_sort(const int sortKey) const
	{
		const auto it = sortRegions_.find(sortKey);
		if (it == sortRegions_.end())
		{
			return 0;
		}
		int best = 0;
		std::uint32_t most = 0;
		for (const auto& [region, count] : it->second)
		{
			if (count > most)
			{
				most = count;
				best = region;
			}
		}
		return best;
	}

	std::string MaterialLibrary::image_zone(const std::string_view image) const
	{
		const auto it = commonImages_.find(image);
		return it == commonImages_.end() ? std::string() : it->second;
	}

	// ---- compile ----------------------------------------------------------------------------------------------

	Status compile_material(const MaterialDef& def, const MaterialLibrary& library, CompiledMaterial& out, const LibraryState* excludeOne)
	try
	{
		out = {};
		out.name = def.name;
		out.techset = techset_for(def);
		if (def.name.empty())
		{
			return Status::error("a material needs a name");
		}
		const LibraryTechset* t = library.find(out.techset);
		if (!t)
		{
			std::string near;
			const std::string prefix = out.techset.substr(0, std::min<std::size_t>(out.techset.size(), 9));
			for (const auto& [name, other] : library.techsets())
			{
				if (name.rfind(prefix, 0) == 0 && near.size() < 400)
				{
					near += " " + name;
				}
			}
			return Status::error("material '" + def.name + "': no shipped material uses technique set '" + out.techset + "'"
				+ (near.empty() ? std::string() : "; similar:" + near) + ". Name one with `techset`.");
		}

		// ---- state
		const auto usable = [&](const LibraryState& s) { return !(&s == excludeOne && s.count <= 1) && s.ref_slot() >= 0; };
		// Several shipped states can agree in the material's own slot and differ in others ( shadow caster, depth
		// prepass .. ). Those go with the game flags and the sort key, so a state that shipped materials with the
		// definition's flags / sort key have is preferred; then the most common.
		const LibraryState* chosen = nullptr;
		int chosenScore = -1;
		for (const LibraryState& s : t->states)		// most common first
		{
			if (usable(s) && (s.ref() & def.stateMask) == (def.stateValue & def.stateMask))
			{
				const int score = (def.gameFlags >= 0 && s.gameFlags.count(static_cast<std::uint8_t>(def.gameFlags)) ? 2 : 0)
					+ (def.sortKey >= 0 && s.sortKeys.count(static_cast<std::uint8_t>(def.sortKey)) ? 1 : 0);
				if (score > chosenScore)
				{
					chosenScore = score;
					chosen = &s;
				}
			}
		}
		LibraryState derived;
		out.stateShipped = chosen != nullptr;
		if (!chosen)
		{
			// the shipped state that agrees in the most fields; the others are changed wherever a slot has the value
			// the material's own slot has
			const LibraryState* base = nullptr;
			int bestScore = -1;
			for (const LibraryState& s : t->states)
			{
				if (!usable(s))
				{
					continue;
				}
				int score = 0;
				for (const StateBits field : state::FIELDS)
				{
					const StateBits m = def.stateMask & field;
					score += (s.ref() & m) == (def.stateValue & m) ? 1 : 0;
				}
				if (score > bestScore)
				{
					bestScore = score;
					base = &s;
				}
			}
			if (!base)
			{
				// a technique set without state bits ( or the only state was left out ): nothing to derive from
				if (t->states.empty() || excludeOne)
				{
					return Status::error("material '" + def.name + "': no shipped state of technique set '" + out.techset + "' to derive from");
				}
				base = &t->states.front();
			}
			derived = *base;
			const StateBits ref = base->ref();
			for (const StateBits field : state::FIELDS)
			{
				const StateBits m = def.stateMask & field;
				if (!m || (ref & m) == (def.stateValue & m))
				{
					continue;
				}
				for (StateBits& bits : derived.table)
				{
					if ((bits & m) == (ref & m))
					{
						bits = (bits & ~m) | (def.stateValue & m);
					}
				}
			}
			// stateFlags bit 0: the material culls ( all shipped states with cull none have it clear )
			if ((def.stateMask & state::CULL) == state::CULL)
			{
				derived.stateFlags = static_cast<std::uint8_t>((derived.stateFlags & ~1) | ((def.stateValue & state::CULL) == 0x4000 ? 0 : 1));
			}
			out.notes.push_back("no shipped material of " + out.techset + " has this state; derived from the nearest ( " + std::to_string(base->count) + " materials )");
			chosen = &derived;
		}

		c1::Material& h = out.header;
		h.name = c1::ZPTR_INLINE;
		const int sortKey = def.sortKey >= 0 ? def.sortKey : most_common(chosen->sortKeys, std::uint8_t(0));
		h.sortKey = static_cast<std::uint8_t>(sortKey);
		h.gameFlags = static_cast<std::uint8_t>(def.gameFlags >= 0 ? def.gameFlags : most_common(chosen->gameFlags, std::uint8_t(0)));
		h._unk6[0] = static_cast<std::uint8_t>(def.atlasRows);			// texture atlas rows, columns; the 8 bytes after them are zero in every shipped material
		h._unk6[1] = static_cast<std::uint8_t>(def.atlasColumns);
		h.surfaceTypeBits = def.surfaceTypeBits;
		std::memcpy(h.stateBitsEntry, chosen->entries.data(), 48);
		h._unk75 = chosen->stateFlags;
		if (def.cameraRegion >= 0)
		{
			h.cameraRegion = static_cast<std::uint8_t>(def.cameraRegion);
		}
		else
		{
			// the region goes with the sort key: of this state when a shipped material has the pair, else overall
			int region = -1;
			std::uint32_t most = 0;
			for (const auto& [pair, count] : chosen->sortToRegion)
			{
				if ((pair >> 8) == sortKey && count > most)
				{
					most = count;
					region = pair & 0xFF;
				}
			}
			h.cameraRegion = static_cast<std::uint8_t>(region >= 0 ? region : library.region_of_sort(sortKey));
		}
		out.stateBits = chosen->table;

		// ---- textures: the technique set's slots, in its order
		std::vector<bool> used(def.textures.size(), false);
		for (const LibrarySampler& slot : t->samplers)
		{
			c1::MaterialTextureDef texture{};
			texture.nameHash = slot.nameHash;
			texture.nameStart = slot.nameStart;
			texture.nameEnd = slot.nameEnd;
			texture.samplerState = slot.samplerState;
			texture.semantic = slot.semantic;
			std::string image = slot.defaultImage;
			bool own = false;
			for (std::size_t k = 0; k < def.textures.size(); k++)
			{
				if (material_name_hash(def.textures[k].sampler) == slot.nameHash)
				{
					used[k] = true;
					own = true;
					image = def.textures[k].image;
					if (def.textures[k].samplerState >= 0)
					{
						texture.samplerState = static_cast<std::uint8_t>(def.textures[k].samplerState);
					}
					if (def.textures[k].semantic >= 0)
					{
						texture.semantic = static_cast<std::uint8_t>(def.textures[k].semantic);
					}
				}
			}
			if (image.empty())
			{
				return Status::error("material '" + def.name + "': technique set '" + out.techset + "' needs an image for sampler " + std::string(1, slot.nameStart)
					+ ".." + std::string(1, slot.nameEnd) + " and shipped materials have no stand-in for it");
			}
			if (!own)
			{
				out.notes.push_back("sampler " + std::string(1, slot.nameStart) + ".." + std::string(1, slot.nameEnd) + " bound to " + image + " as shipped materials do");
			}
			out.textures.push_back(texture);
			out.images.push_back(image);
		}
		for (std::size_t k = 0; k < def.textures.size(); k++)
		{
			if (!used[k])
			{
				out.notes.push_back(def.textures[k].sampler + " is not a sampler of " + out.techset + " and was left out");
			}
		}
		// ---- constants
		for (const LibraryConstant& slot : t->constants)
		{
			c1::MaterialConstantDef c{};
			c.nameHash = slot.nameHash;
			std::memcpy(c.name, slot.name, 12);
			std::memcpy(c.literal, slot.value, sizeof(c.literal));
			for (const MaterialConstant& mine : def.constants)
			{
				// the table keeps 12 characters of the name, the hash is of the whole name
				if (material_name_hash(mine.name) == slot.nameHash || (mine.name.size() == 12 && std::strncmp(mine.name.c_str(), slot.name, 12) == 0))
				{
					std::memcpy(c.literal, mine.value, sizeof(c.literal));
				}
			}
			out.constants.push_back(c);
		}
		h.textureCount = static_cast<std::uint8_t>(out.textures.size());
		h.constantCount = static_cast<std::uint8_t>(out.constants.size());
		h.stateBitsCount = static_cast<std::uint8_t>(out.stateBits.size());
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	bool extract_material(const zone::Zone& zone, const zone::zaddr material, MaterialDef& out)
	{
		// the sampler names the shipped technique sets use; any other is written as a hash and read back as one
		static const char* const SAMPLERS[] = { "colorMap", "normalMap", "specularMap", "detailMap", "colorMap00", "colorMap01", "colorMap02", "colorMap03",
			"attenuationMap", "distortionMap", "colorMap1", "colorMap2", "detailMap1", "occlusionMap", "parallaxMap", "cubeMap", "lookupMap", "skyMapSampler",
			"normalMap00", "normalMap1", "specularMap1", "colorMapPostSun", "colorMapPostSunSampler", "colorMapSampler", "normalMapSampler" };
		out = {};
		zone::MaterialView view;
		if (!zone::read_material(zone, material, view) || view.isReference)
		{
			return false;
		}
		out.name = std::string(view.name);
		out.techset = std::string(view.techsetName);
		out.sortKey = view.raw.sortKey;
		out.gameFlags = view.raw.gameFlags;
		out.surfaceTypeBits = view.raw.surfaceTypeBits;
		out.atlasRows = view.raw._unk6[0];
		out.atlasColumns = view.raw._unk6[1];
		LibraryState s;
		std::memcpy(s.entries.data(), view.raw.stateBitsEntry, 48);
		for (const auto& bits : view.stateBits)
		{
			s.table.push_back(static_cast<StateBits>(bits[0]) | (static_cast<StateBits>(bits[1]) << 32));
		}
		if (s.ref_slot() >= 0)
		{
			out.stateMask = state::ALL_FIELDS;
			out.stateValue = s.ref() & state::ALL_FIELDS;
			const unsigned op = static_cast<unsigned>((out.stateValue >> 8) & 7), src = static_cast<unsigned>(out.stateValue & 15), dst = static_cast<unsigned>((out.stateValue >> 4) & 15);
			out.blendFunc = op == 0 ? "replace" : dst == 6 ? "blend" : dst == 3 ? "multiply" : src == 10 ? "screen" : "add";
		}
		for (const zone::MaterialTexture& texture : view.textures)
		{
			MaterialTextureRef ref;
			for (const char* name : SAMPLERS)
			{
				if (material_name_hash(name) == texture.raw.nameHash)
				{
					ref.sampler = name;
				}
			}
			if (ref.sampler.empty())
			{
				char text[32];
				std::snprintf(text, sizeof(text), "Map#%08X", texture.raw.nameHash);
				ref.sampler = text;
			}
			ref.image = std::string(texture.imageName);
			ref.samplerState = texture.raw.samplerState;
			ref.semantic = texture.raw.semantic;
			out.textures.push_back(ref);
		}
		for (const c1::MaterialConstantDef& c : view.constants)
		{
			MaterialConstant mine;
			mine.name.assign(c.name, strnlen(c.name, 12));
			std::memcpy(mine.value, c.literal, sizeof(mine.value));
			out.constants.push_back(mine);
		}
		return true;
	}
}
