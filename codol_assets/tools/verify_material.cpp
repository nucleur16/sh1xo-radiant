// codol_asset_verify material-roundtrip: every shipped material -> definition text -> compile -> compare with the
// shipped Material, field by field.

#include "verify.hpp"

#include "codol_assets/material.hpp"
#include "codol_zone/assets.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

using namespace codol::assets;
using namespace codol::zone;
namespace c1 = codol::c1;
namespace fs = std::filesystem;

std::vector<fs::path> install_zones(const fs::path& install)
{
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
	return files;
}

void print_rate(const char* name, const Rate& r)
{
	std::printf("  %-38s %6u / %-6u  %7.3f %%\n", name, r.equal, r.total, r.total ? 100.0 * r.equal / r.total : 0.0);
}

int material_roundtrip(const fs::path& install, const fs::path& exe, const fs::path& cache, const int every, const fs::path& dump)
{
	ClientKey key;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	MaterialLibrary library;
	if (const Status s = library.open(install, key, cache); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	std::size_t states = 0;
	for (const auto& [name, t] : library.techsets())
	{
		states += t.states.size();
	}
	std::printf("library: %zu technique sets, %zu distinct states\n", library.techsets().size(), states);

	// A: the definition as extracted. B: the same, the library not knowing the material's own state unless another
	// material has it too. C: sort key, game flags, camera region, sampler states and semantics left to the compiler.
	Rate techset, gameFlags, sortKey, atlas, surface, entries, table, stateFlags, region, textures, images, constants, header, whole, shippedState;
	Rate bEntries, bTable, bFlags, bWhole, bUnique, bCompiled;
	Rate cSort, cGame, cRegion, cSampler, cSemantic;
	std::set<std::string> seen;
	std::uint32_t index = 0, failed = 0, dumped = 0;
	for (const fs::path& file : install_zones(install))
	{
		Zone zone;
		if (!zone.open(file, key))
		{
			continue;
		}
		for (const AssetRecord& r : zone.all_assets())
		{
			if (r.type != c1::ASSET_TYPE_MATERIAL || zone.reference(r.header))
			{
				continue;
			}
			MaterialView view;
			if (!read_material(zone, r.header, view) || view.isReference || !seen.insert(std::string(view.name)).second || (index++ % static_cast<std::uint32_t>(every)) != 0)
			{
				continue;
			}
			MaterialDef extracted;
			if (!extract_material(zone, r.header, extracted))
			{
				continue;
			}
			// through the text format, as a modder would have it
			const std::string text = write_material_text(extracted);
			std::vector<MaterialDef> parsed;
			if (const Status s = parse_material_text(text, parsed); !s || parsed.size() != 1)
			{
				std::fprintf(stderr, "%s: definition does not read back: %s\n%s", extracted.name.c_str(), s.message.c_str(), text.c_str());
				failed++;
				continue;
			}
			if (!dump.empty() && dumped < 60)
			{
				dumped++;
				std::string safe = extracted.name;
				for (char& ch : safe)
				{
					ch = ch == '/' || ch == '*' || ch == '\\' || ch == ':' ? '+' : ch;
				}
				write_file(dump / (safe + ".mtl"), { reinterpret_cast<const std::uint8_t*>(text.data()), text.size() });
			}
			const MaterialDef& def = parsed[0];
			std::vector<StateBits> shippedTable;
			for (const auto& bits : view.stateBits)
			{
				shippedTable.push_back(static_cast<StateBits>(bits[0]) | (static_cast<StateBits>(bits[1]) << 32));
			}
			const auto same_state = [&](const CompiledMaterial& m, const int what)
			{
				const bool e = std::memcmp(m.header.stateBitsEntry, view.raw.stateBitsEntry, 48) == 0;
				const bool t = m.stateBits == shippedTable;
				const bool f = m.header._unk75 == view.raw._unk75;
				return what == 0 ? e : what == 1 ? t : what == 2 ? f : e && t && f;
			};

			CompiledMaterial a;
			if (const Status s = compile_material(def, library, a); !s)
			{
				std::fprintf(stderr, "%s\n", s.message.c_str());
				failed++;
				continue;
			}
			techset.add(a.techset == view.techsetName);
			gameFlags.add(a.header.gameFlags == view.raw.gameFlags);
			sortKey.add(a.header.sortKey == view.raw.sortKey);
			atlas.add(std::memcmp(a.header._unk6, view.raw._unk6, 10) == 0);
			surface.add(a.header.surfaceTypeBits == view.raw.surfaceTypeBits);
			entries.add(same_state(a, 0));
			table.add(same_state(a, 1));
			stateFlags.add(same_state(a, 2));
			region.add(a.header.cameraRegion == view.raw.cameraRegion);
			shippedState.add(a.stateShipped);
			bool sameTextures = a.textures.size() == view.textures.size(), sameImages = sameTextures;
			for (std::size_t k = 0; sameTextures && k < a.textures.size(); k++)
			{
				sameTextures = std::memcmp(&a.textures[k], &view.textures[k].raw, 8) == 0;
				sameImages = sameImages && a.images[k] == view.textures[k].imageName;
			}
			textures.add(sameTextures);
			images.add(sameImages);
			const bool sameConstants = a.constants.size() == view.constants.size()
				&& (a.constants.empty() || std::memcmp(a.constants.data(), view.constants.data(), a.constants.size() * sizeof(c1::MaterialConstantDef)) == 0);
			constants.add(sameConstants);
			// the 96 byte header without its five pointers
			c1::Material mine = a.header, theirs = view.raw;
			mine.name = theirs.name = mine.techniqueSet = theirs.techniqueSet = mine.textureTable = theirs.textureTable = 0;
			mine.constantTable = theirs.constantTable = mine.stateBitsTable = theirs.stateBitsTable = 0;
			const bool sameHeader = std::memcmp(&mine, &theirs, sizeof(mine)) == 0;
			header.add(sameHeader);
			whole.add(sameHeader && sameTextures && sameImages && sameConstants && same_state(a, 1));

			// B: leave one out
			const LibraryTechset* t = library.find(a.techset);
			const LibraryState* own = nullptr;
			for (const LibraryState& s : t->states)
			{
				if (s.table == shippedTable && std::memcmp(s.entries.data(), view.raw.stateBitsEntry, 48) == 0 && s.stateFlags == view.raw._unk75)
				{
					own = &s;
				}
			}
			const bool unique = own && own->count <= 1;
			CompiledMaterial b;
			const bool compiled = static_cast<bool>(compile_material(def, library, b, own));
			bCompiled.add(compiled);
			bEntries.add(compiled && same_state(b, 0));
			bTable.add(compiled && same_state(b, 1));
			bFlags.add(compiled && same_state(b, 2));
			bWhole.add(compiled && same_state(b, 3));
			if (unique)
			{
				bUnique.add(compiled && same_state(b, 3));
			}

			// C: defaults
			MaterialDef bare = def;
			bare.sortKey = bare.gameFlags = bare.cameraRegion = -1;
			for (MaterialTextureRef& ref : bare.textures)
			{
				ref.samplerState = ref.semantic = -1;
			}
			CompiledMaterial c;
			if (compile_material(bare, library, c))
			{
				cSort.add(c.header.sortKey == view.raw.sortKey);
				cGame.add(c.header.gameFlags == view.raw.gameFlags);
				cRegion.add(c.header.cameraRegion == view.raw.cameraRegion);
				for (std::size_t k = 0; k < c.textures.size() && k < view.textures.size(); k++)
				{
					cSampler.add(c.textures[k].samplerState == view.textures[k].raw.samplerState);
					cSemantic.add(c.textures[k].semantic == view.textures[k].raw.semantic);
				}
			}
		}
	}
	std::printf("%u distinct shipped materials, every %d. sampled; %u could not be compiled\n", index, every, failed);
	std::printf("A. recompiled from its own definition ( text written, parsed, compiled ):\n");
	print_rate("technique set", techset);
	print_rate("gameFlags", gameFlags);
	print_rate("sortKey", sortKey);
	print_rate("atlas rows, columns, drawSurf", atlas);
	print_rate("surfaceTypeBits", surface);
	print_rate("stateBitsEntry[48]", entries);
	print_rate("state bits table", table);
	print_rate("stateFlags", stateFlags);
	print_rate("cameraRegion ( derived )", region);
	print_rate("texture table ( hash, chars, state )", textures);
	print_rate("texture images", images);
	print_rate("constant table", constants);
	print_rate("header, all 76 non pointer bytes", header);
	print_rate("whole material", whole);
	print_rate("( state found among shipped ones )", shippedState);
	std::printf("B. the same, without the material's own state unless another shipped material has it:\n");
	print_rate("compiled", bCompiled);
	print_rate("stateBitsEntry[48]", bEntries);
	print_rate("state bits table", bTable);
	print_rate("stateFlags", bFlags);
	print_rate("entries + table + flags", bWhole);
	print_rate(".. of the materials with a unique state", bUnique);
	std::printf("C. sort, gameFlags, cameraRegion, sampler state, semantic not given ( the compiler's defaults ):\n");
	print_rate("sortKey", cSort);
	print_rate("gameFlags", cGame);
	print_rate("cameraRegion", cRegion);
	print_rate("sampler state ( per texture )", cSampler);
	print_rate("semantic ( per texture )", cSemantic);
	return 0;
}
