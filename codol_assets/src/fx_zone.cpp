// The stored FxEffectDef: reading a shipped one, its size, writing one into a zone.

#include "codol_assets/fx.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>

namespace codol::assets
{
	namespace
	{
		using zone::Zone;
		using zone::zaddr;

		// what the visuals of an element type are: an asset type, -1 a sound alias name, -2 nothing
		std::int32_t visual_asset_type(const std::uint8_t elemType)
		{
			switch (elemType)
			{
			case c1::FX_ELEM_TYPE_MODEL: return c1::ASSET_TYPE_XMODEL;
			case c1::FX_ELEM_TYPE_RUNNER: return c1::ASSET_TYPE_FX;
			case c1::FX_ELEM_TYPE_SOUND: return -1;
			case c1::FX_ELEM_TYPE_OMNI_LIGHT:
			case c1::FX_ELEM_TYPE_SPOT_LIGHT: return -2;
			default: return c1::ASSET_TYPE_MATERIAL;
			}
		}
	}

	// ---- FxAtlasLibrary ----
	zone::Status FxAtlasLibrary::scan(const std::filesystem::path& install, const zone::ClientKey& key)
	{
		atlas_.clear();
		install_ = install.generic_string();
		std::size_t zones = 0;
		for (const char* folder : { "client_shipRetail/chinese", "client_shipRetail/patch" })
		{
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(install / folder, ec))
			{
				if (entry.path().extension() != ".ff")
				{
					continue;
				}
				Zone zone;
				if (!zone.open(entry.path(), key))
				{
					continue;
				}
				zones++;
				for (const zone::AssetRecord& r : zone.all_assets())
				{
					c1::Material material;
					if (r.type == c1::ASSET_TYPE_MATERIAL && !zone.reference(r.header) && zone.get(r.header, material))
					{
						// MaterialInfo: texture atlas row count, column count behind the sort key
						set(zone.asset_name(r.type, r.header), material._unk6[1], material._unk6[0]);
					}
				}
			}
		}
		return zones ? zone::Status{} : zone::Status::error("no fast files below " + install.string() + "/client_shipRetail");
	}

	void FxAtlasLibrary::set(const std::string_view material, const int columns, const int rows)
	{
		if (columns * rows > 1 && !material.empty() && material.front() != ',')
		{
			auto& entry = atlas_[std::string(material)];
			if (columns * rows > entry.first * entry.second)
			{
				entry = { columns, rows };
			}
		}
	}

	bool FxAtlasLibrary::find(const std::string_view material, int& columns, int& rows) const
	{
		const auto it = atlas_.find(material);
		if (it == atlas_.end())
		{
			return false;
		}
		columns = it->second.first;
		rows = it->second.second;
		return true;
	}

	zone::Status FxAtlasLibrary::save(const std::filesystem::path& file) const
	{
		std::ofstream out(file, std::ios::binary);
		out << "codol_fxatlas 1 " << install_ << "\n";
		for (const auto& [name, cells] : atlas_)
		{
			out << name << " " << cells.first << " " << cells.second << "\n";
		}
		return out.good() ? zone::Status{} : zone::Status::error("cannot write " + file.string());
	}

	zone::Status FxAtlasLibrary::load(const std::filesystem::path& file)
	{
		std::ifstream in(file, std::ios::binary);
		std::string magic, line;
		int version = 0;
		if (!(in >> magic >> version) || magic != "codol_fxatlas" || version != 1)
		{
			return zone::Status::error(file.string() + " is not an atlas table");
		}
		std::getline(in, line);
		install_ = line.empty() ? line : line.substr(1);
		atlas_.clear();
		std::string name;
		int columns = 0, rows = 0;
		while (in >> name >> columns >> rows)
		{
			atlas_[name] = { columns, rows };
		}
		return {};
	}

	zone::Status FxAtlasLibrary::open(const std::filesystem::path& install, const zone::ClientKey& key, const std::filesystem::path& cache)
	{
		std::error_code ec;
		if (!cache.empty() && std::filesystem::exists(cache, ec) && load(cache) && install_ == install.generic_string())
		{
			return {};
		}
		if (const zone::Status s = scan(install, key); !s)
		{
			return s;
		}
		return cache.empty() ? zone::Status{} : save(cache);
	}

	FxCompileOptions fx_compile_options(const zone::ZoneEditor& editor, const FxAtlasLibrary& atlas, const std::filesystem::path& fxRoot)
	{
		FxCompileOptions options;
		options.materialAtlas = [&editor, &atlas](const std::string_view material, int& columns, int& rows)
		{
			const std::size_t index = editor.find_asset(c1::ASSET_TYPE_MATERIAL, material);
			if (index != zone::ZoneEditor::npos)
			{
				const auto bytes = editor.data_bytes(editor.asset_data(index, 0));
				if (bytes.size() >= sizeof(c1::Material))
				{
					c1::Material header;
					std::memcpy(&header, bytes.data(), sizeof(header));
					columns = std::max<int>(header._unk6[1], 1);
					rows = std::max<int>(header._unk6[0], 1);
					return true;
				}
			}
			return atlas.find(material, columns, rows);
		};
		options.loadEffect = [fxRoot](const std::string_view name, EditorEffect& effect)
		{
			return static_cast<bool>(load_efx(fxRoot / (std::string(name) + ".efx"), effect));
		};
		return options;
	}

	std::filesystem::path fx_source_root(const std::filesystem::path& file, const std::string_view name)
	{
		const std::string path = file.generic_string();
		const std::string tail = "/" + std::string(name) + ".efx";
		if (path.size() > tail.size() && path.compare(path.size() - tail.size(), tail.size(), tail) == 0)
		{
			return path.substr(0, path.size() - tail.size());
		}
		return file.parent_path();
	}

	// The stream of an effect, in the order the loader ( 0x004BD5E0 ) reads it: the header, the name, the element
	// array, then per element the velocity samples, the visual samples, the visuals ( a handle array when there are
	// several, each handle followed by what it nests ), the names of the effects on impact / death / emitted, the
	// trail or spark fountain definition, the string at +256.
	zone::Status link_fx(zone::ZoneEditor& editor, const CompiledFx& fx, std::vector<std::string>* notes)
	{
		using zone::Status;
		using zone::ZoneEditor;
		using zone::ZoneNewPiece;
		using zone::ZoneNewPointer;
		if (editor.find_asset(c1::ASSET_TYPE_FX, fx.name) != ZoneEditor::npos)
		{
			return Status::error("the zone already has fx '" + fx.name + "'");
		}
		if (fx.elems.empty() || fx.elems.size() != static_cast<std::size_t>(fx.countLooping) + static_cast<std::size_t>(fx.countOneShot) + static_cast<std::size_t>(fx.countEmission))
		{
			return Status::error("fx '" + fx.name + "': no elements, or the section counts do not add up");
		}
		std::vector<ZoneNewPiece> pieces;
		const auto add = [&](const void* data, const std::size_t size)
		{
			ZoneNewPiece piece;
			piece.bytes.assign(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
			pieces.push_back(std::move(piece));
			return static_cast<std::uint32_t>(pieces.size() - 1);
		};
		const auto add_string = [&](const std::string_view text)
		{
			ZoneNewPiece piece;
			piece.string = true;
			piece.bytes.assign(text.begin(), text.end());
			piece.bytes.push_back(0);
			pieces.push_back(std::move(piece));
		};
		const auto put32 = [&](const std::uint32_t piece, const std::uint32_t offset, const std::uint32_t value)
		{
			std::memcpy(pieces[piece].bytes.data() + offset, &value, 4);
		};

		c1::FxEffectDef header{};
		header.name = c1::ZPTR_INLINE;
		header.flags = fx.flags;
		header.totalSize = fx.totalSize;
		header.msecLoopingLife = fx.msecLoopingLife;
		header.elemDefCountLooping = fx.countLooping;
		header.elemDefCountOneShot = fx.countOneShot;
		header.elemDefCountEmission = fx.countEmission;
		header.elemDefs = c1::ZPTR_INLINE;
		add(&header, sizeof(header));
		add_string(fx.name);

		std::vector<c1::FxElemDef> defs;
		for (const FxElem& e : fx.elems)
		{
			c1::FxElemDef d = e.def;
			d.velSamples = e.velSamples.empty() ? 0 : c1::ZPTR_INLINE;
			d.visSamples = e.visSamples.empty() ? 0 : c1::ZPTR_INLINE;
			d.visuals = 0;
			d.effectOnImpact = e.effectOnImpact.empty() ? 0 : c1::ZPTR_INLINE;
			d.effectOnDeath = e.effectOnDeath.empty() ? 0 : c1::ZPTR_INLINE;
			d.effectEmitted = e.effectEmitted.empty() ? 0 : c1::ZPTR_INLINE;
			d.extended = e.hasExtended ? c1::ZPTR_INLINE : 0;
			d._string256 = e.hasString256 ? c1::ZPTR_INLINE : 0;
			if (d.velSamples && e.velSamples.size() != static_cast<std::size_t>(d.velIntervalCount) + 1)
			{
				return Status::error("fx '" + fx.name + "': velocity samples do not match the interval count");
			}
			if (d.visSamples && e.visSamples.size() != static_cast<std::size_t>(d.visStateIntervalCount) + 1)
			{
				return Status::error("fx '" + fx.name + "': visual samples do not match the interval count");
			}
			defs.push_back(d);
		}
		const std::uint32_t array = add(defs.data(), defs.size() * sizeof(c1::FxElemDef));

		// a material or model handle: the asset of the zone, the field that named it before, or a reference by name
		std::map<std::pair<std::int32_t, std::string>, std::pair<std::uint32_t, std::uint32_t>> named;
		const auto handle = [&](const std::uint32_t piece, const std::uint32_t offset, const std::int32_t type, const std::string& name)
		{
			if (name.empty())
			{
				put32(piece, offset, 0);
				return;
			}
			ZoneNewPointer pointer;
			pointer.offset = offset;
			const std::size_t index = editor.find_asset(type, name);
			const auto before = named.find({ type, name });
			if (index != ZoneEditor::npos)
			{
				pointer.asset = index;
				pieces[piece].pointers.push_back(pointer);
			}
			else if (before != named.end())
			{
				pointer.piece = before->second.first;
				pointer.pieceOffset = before->second.second;
				pieces[piece].pointers.push_back(pointer);
			}
			else
			{
				put32(piece, offset, c1::ZPTR_INLINE);
				if (type == c1::ASSET_TYPE_XMODEL)
				{
					c1::XModel reference{};
					reference.name = c1::ZPTR_INLINE;
					add(&reference, sizeof(reference));
				}
				else
				{
					c1::Material reference{};
					reference.name = c1::ZPTR_INLINE;
					add(&reference, sizeof(reference));
				}
				add_string("," + name);
				named[{ type, name }] = { piece, offset };
				if (notes)
				{
					notes->push_back("fx " + fx.name + ": " + zone::asset_type_name(type) + " " + name + " is not in the zone, referenced by name");
				}
			}
		};

		for (std::size_t i = 0; i < fx.elems.size(); i++)
		{
			const FxElem& e = fx.elems[i];
			const c1::FxElemDef& d = e.def;
			const std::uint32_t base = static_cast<std::uint32_t>(i * sizeof(c1::FxElemDef));
			if (!e.velSamples.empty())
			{
				add(e.velSamples.data(), e.velSamples.size() * sizeof(c1::FxElemVelStateSample));
			}
			if (!e.visSamples.empty())
			{
				add(e.visSamples.data(), e.visSamples.size() * sizeof(c1::FxElemVisStateSample));
			}
			const std::int32_t type = visual_asset_type(d.elemType);
			const auto visual = [&](const std::uint32_t piece, const std::uint32_t offset, const std::string& name)
			{
				if (type == -1 || type == c1::ASSET_TYPE_FX)
				{
					// a sound alias or another effect: its name
					put32(piece, offset, name.empty() ? 0 : c1::ZPTR_INLINE);
					if (!name.empty())
					{
						add_string(name);
					}
				}
				else
				{
					handle(piece, offset, type, name);
				}
			};
			const std::size_t expected = d.elemType == c1::FX_ELEM_TYPE_DECAL ? static_cast<std::size_t>(d.visualCount) * 2 : type == -2 ? 0 : d.visualCount;
			if (e.visuals.size() != expected)
			{
				return Status::error("fx '" + fx.name + "': element " + std::to_string(i) + ": the visuals do not match the visual count");
			}
			if ((d.elemType == c1::FX_ELEM_TYPE_DECAL && !e.visuals.empty()) || e.visuals.size() > 1)
			{
				put32(array, base + static_cast<std::uint32_t>(offsetof(c1::FxElemDef, visuals)), c1::ZPTR_INLINE);
				const std::vector<std::uint32_t> handles(e.visuals.size(), 0);
				const std::uint32_t list = add(handles.data(), handles.size() * 4);
				for (std::size_t k = 0; k < e.visuals.size(); k++)
				{
					visual(list, static_cast<std::uint32_t>(k * 4), e.visuals[k]);
				}
			}
			else if (e.visuals.size() == 1)
			{
				visual(array, base + static_cast<std::uint32_t>(offsetof(c1::FxElemDef, visuals)), e.visuals[0]);
			}
			for (const std::string* name : { &e.effectOnImpact, &e.effectOnDeath, &e.effectEmitted })
			{
				if (!name->empty())
				{
					add_string(*name);
				}
			}
			if (e.hasExtended)
			{
				if (d.elemType == c1::FX_ELEM_TYPE_TRAIL)
				{
					c1::FxTrailDef trail = e.trail;
					trail.vertCount = static_cast<std::int32_t>(e.trailVerts.size());
					trail.indCount = static_cast<std::int32_t>(e.trailInds.size());
					trail.verts = e.trailVerts.empty() ? 0 : c1::ZPTR_INLINE;
					trail.inds = e.trailInds.empty() ? 0 : c1::ZPTR_INLINE;
					add(&trail, sizeof(trail));
					if (!e.trailVerts.empty())
					{
						add(e.trailVerts.data(), e.trailVerts.size() * sizeof(c1::FxTrailVertex));
					}
					if (!e.trailInds.empty())
					{
						add(e.trailInds.data(), e.trailInds.size() * 2);
					}
				}
				else if (d.elemType == c1::FX_ELEM_TYPE_SPARK_FOUNTAIN)
				{
					add(&e.spark, sizeof(e.spark));
				}
				else
				{
					add(&e.extendedByte, 1);
				}
			}
			if (e.hasString256)
			{
				add_string(e.string256);
			}
		}
		return editor.add_new_asset(c1::ASSET_TYPE_FX, fx.name, pieces);
	}

	bool read_fx(const Zone& zone, const zaddr header, CompiledFx& out)
	{
		c1::FxEffectDef def;
		if (!zone.get(header, def) || zone.reference(header))
		{
			return false;
		}
		out = {};
		out.name = std::string(zone.c_string(def.name));
		out.flags = def.flags;
		out.totalSize = def.totalSize;
		out.msecLoopingLife = def.msecLoopingLife;
		out.countLooping = def.elemDefCountLooping;
		out.countOneShot = def.elemDefCountOneShot;
		out.countEmission = def.elemDefCountEmission;
		const std::int64_t count = static_cast<std::int64_t>(def.elemDefCountLooping) + def.elemDefCountOneShot + def.elemDefCountEmission;
		if (count < 0 || count > 256)
		{
			return false;
		}
		const std::vector<c1::FxElemDef> defs = zone.get_array<c1::FxElemDef>(def.elemDefs, static_cast<std::size_t>(count));
		if (defs.size() != static_cast<std::size_t>(count))
		{
			return count == 0;
		}
		for (std::size_t i = 0; i < defs.size(); i++)
		{
			FxElem e;
			e.def = defs[i];
			const c1::FxElemDef& d = e.def;
			if (d.velSamples)
			{
				e.velSamples = zone.get_array<c1::FxElemVelStateSample>(d.velSamples, static_cast<std::size_t>(d.velIntervalCount) + 1);
			}
			if (d.visSamples)
			{
				e.visSamples = zone.get_array<c1::FxElemVisStateSample>(d.visSamples, static_cast<std::size_t>(d.visStateIntervalCount) + 1);
			}
			const std::int32_t type = visual_asset_type(d.elemType);
			const auto visual_name = [&](const zaddr handle) -> std::string
			{
				if (!handle)
				{
					return {};
				}
				if (type == -1)
				{
					return std::string(zone.c_string(handle));
				}
				return std::string(zone.pointee_name(type, handle));
			};
			if (d.elemType == c1::FX_ELEM_TYPE_DECAL)
			{
				for (const std::uint32_t handle : zone.get_array<std::uint32_t>(d.visuals, static_cast<std::size_t>(d.visualCount) * 2))
				{
					e.visuals.push_back(visual_name(handle));
				}
			}
			else if (type != -2)
			{
				if (d.visualCount > 1)
				{
					for (const std::uint32_t handle : zone.get_array<std::uint32_t>(d.visuals, d.visualCount))
					{
						e.visuals.push_back(visual_name(handle));
					}
				}
				else if (d.visualCount == 1)
				{
					e.visuals.push_back(visual_name(d.visuals));
				}
			}
			e.effectOnImpact = std::string(zone.pointee_name(c1::ASSET_TYPE_FX, d.effectOnImpact));
			e.effectOnDeath = std::string(zone.pointee_name(c1::ASSET_TYPE_FX, d.effectOnDeath));
			e.effectEmitted = std::string(zone.pointee_name(c1::ASSET_TYPE_FX, d.effectEmitted));
			e.hasExtended = d.extended != 0;
			if (d.extended)
			{
				if (d.elemType == c1::FX_ELEM_TYPE_TRAIL)
				{
					zone.get(d.extended, e.trail);
					e.trailVerts = zone.get_array<c1::FxTrailVertex>(e.trail.verts, static_cast<std::size_t>(std::max(0, e.trail.vertCount)));
					e.trailInds = zone.get_array<std::uint16_t>(e.trail.inds, static_cast<std::size_t>(std::max(0, e.trail.indCount)));
				}
				else if (d.elemType == c1::FX_ELEM_TYPE_SPARK_FOUNTAIN)
				{
					zone.get(d.extended, e.spark);
				}
				else
				{
					zone.get(d.extended, e.extendedByte);
				}
			}
			e.hasString256 = d._string256 != 0;
			e.string256 = std::string(zone.c_string(d._string256));
			out.elems.push_back(std::move(e));
		}
		return true;
	}
}
