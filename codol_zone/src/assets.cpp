#include "codol_zone/assets.hpp"

#include <algorithm>

// Layout knowledge beyond c1_layouts.hpp follows CODOL-AssetExplorer ( Codol.Assets: ModelExtractor.cs,
// MeshExtractor.cs, SkeletonExtractor.cs, SkinningExtractor.cs, MaterialExtractor.cs, MapExtractor.cs,
// BrushGeometry.cs, AnimationExtractor.cs, RawFileCollector.cs ).

namespace codol::zone
{
	namespace
	{
		constexpr std::size_t DOBJ_ANIM_MAT_SIZE = 32;		// quat[4], trans[3], transWeight
		constexpr std::size_t COM_PRIMARY_LIGHT_SIZE = 68;
		constexpr std::uint8_t TEXTURE_SEMANTIC_WATER_MAP = 11;
		constexpr float BONE_QUAT_SCALE = 1.0f / 32767.0f;
		constexpr float BLEND_WEIGHT_SCALE = 1.0f / 65536.0f;

		std::uint16_t rd16(const std::uint8_t* p)
		{
			std::uint16_t v;
			std::memcpy(&v, p, 2);
			return v;
		}

		float rdf(const std::uint8_t* p)
		{
			float v;
			std::memcpy(&v, p, 4);
			return v;
		}

		// header bytes of an asset or of the placeholder of a by-name reference
		template <typename T>
		bool read_header(const Zone& zone, const zaddr address, T& raw)
		{
			return address && zone.get(address, raw);
		}

		bool is_reference_name(const std::string_view rawName)
		{
			return !rawName.empty() && rawName[0] == ',';
		}

		// name + reference state of an asset header whose name pointer is at nameOffset
		void resolve_name(const Zone& zone, const std::int32_t type, const zaddr address, const zaddr namePtr, std::string_view& name, bool& isReference)
		{
			if (const AssetReference* ref = zone.reference(address))
			{
				name = ref->name;
				isReference = true;
				return;
			}
			const std::string_view rawName = zone.c_string(namePtr);
			isReference = is_reference_name(rawName);
			name = zone.asset_name(type, address);
		}
	}

	void unpack_unit_vector(const std::uint32_t packed, float out[3])
	{
		const float scale = (static_cast<float>(packed >> 24) + 192.0f) / 32385.0f;
		out[0] = (static_cast<float>(packed & 0xFF) - 127.0f) * scale;
		out[1] = (static_cast<float>((packed >> 8) & 0xFF) - 127.0f) * scale;
		out[2] = (static_cast<float>((packed >> 16) & 0xFF) - 127.0f) * scale;
	}

	float half_to_float(const std::uint16_t half)
	{
		const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16;
		std::uint32_t exponent = (half >> 10) & 0x1F;
		std::uint32_t mantissa = half & 0x3FF;
		std::uint32_t bits;
		if (exponent == 0)
		{
			if (mantissa == 0)
			{
				bits = sign;
			}
			else
			{
				// subnormal: normalise
				exponent = 1;
				while (!(mantissa & 0x400))
				{
					mantissa <<= 1;
					exponent--;
				}
				mantissa &= 0x3FF;
				bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
			}
		}
		else if (exponent == 31)
		{
			bits = sign | 0x7F800000u | (mantissa << 13);
		}
		else
		{
			bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
		}
		float f;
		std::memcpy(&f, &bits, 4);
		return f;
	}

	zaddr first_asset(const Zone& zone, const std::int32_t type)
	{
		for (const AssetRecord& record : zone.all_assets())
		{
			if (record.type == type)
			{
				return record.header;
			}
		}
		return 0;
	}

	// ------------------------------------------------------------------------------------------
	// xmodel

	std::vector<SurfaceVertex> SurfaceView::vertices() const
	try
	{
		std::vector<SurfaceVertex> out(vertexBytes.size() / sizeof(c1::GfxPackedVertex));
		for (std::size_t i = 0; i < out.size(); i++)
		{
			c1::GfxPackedVertex v;
			std::memcpy(&v, vertexBytes.data() + i * sizeof(v), sizeof(v));
			SurfaceVertex& o = out[i];
			o.position[0] = v.xyz[0];
			o.position[1] = v.xyz[1];
			o.position[2] = v.xyz[2];
			o.binormalSign = v.binormalSign;
			// stored b g r a
			o.color[0] = static_cast<std::uint8_t>(v.color >> 16);
			o.color[1] = static_cast<std::uint8_t>(v.color >> 8);
			o.color[2] = static_cast<std::uint8_t>(v.color);
			o.color[3] = static_cast<std::uint8_t>(v.color >> 24);
			o.uv[0] = half_to_float(v.texCoordU);
			o.uv[1] = half_to_float(v.texCoordV);
			unpack_unit_vector(v.normal, o.normal);
			unpack_unit_vector(v.tangent, o.tangent);
		}
		return out;
	}
	catch (const std::exception&)
	{
		return {};
	}

	std::vector<std::uint16_t> SurfaceView::indices() const
	try
	{
		std::vector<std::uint16_t> out(indexBytes.size() / 2);
		if (!out.empty())
		{
			std::memcpy(out.data(), indexBytes.data(), out.size() * 2);
		}
		return out;
	}
	catch (const std::exception&)
	{
		return {};
	}

	std::vector<RigidVertList> SurfaceView::rigid_lists() const
	try
	{
		std::vector<RigidVertList> out(rigidBytes.size() / sizeof(c1::XRigidVertList));
		for (std::size_t i = 0; i < out.size(); i++)
		{
			c1::XRigidVertList l;
			std::memcpy(&l, rigidBytes.data() + i * sizeof(l), sizeof(l));
			out[i].bone = static_cast<std::uint16_t>(l.boneOffset >> 6);
			out[i].vertCount = l.vertCount;
			out[i].triOffset = l.triOffset;
			out[i].triCount = l.triCount;
		}
		return out;
	}
	catch (const std::exception&)
	{
		return {};
	}

	std::vector<VertexWeights> SurfaceView::weights(const Zone& zone) const
	try
	{
		std::vector<VertexWeights> out(vertCount);
		for (VertexWeights& w : out)
		{
			w.count = 1;
			w.weight[0] = 1.0f;
		}

		if (raw.vertsBlend)
		{
			// per vertex with n + 1 influences: u16 bone, then n times ( u16 bone, u16 weight ); bone values are index * 64
			std::size_t total = 0, verts = 0;
			for (int g = 0; g < 4; g++)
			{
				const std::size_t n = raw.vertInfoCount[g] > 0 ? static_cast<std::size_t>(raw.vertInfoCount[g]) : 0;
				total += n * 2 * (2 * static_cast<std::size_t>(g) + 1);
				verts += n;
			}
			const auto blend = zone.bytes(raw.vertsBlend, total);
			if (verts != vertCount || blend.size() != total)
			{
				return out;
			}
			const std::uint8_t* p = blend.data();
			std::size_t v = 0;
			for (int g = 0; g < 4; g++)
			{
				for (int n = 0; n < raw.vertInfoCount[g]; n++, v++)
				{
					VertexWeights& w = out[v];
					w.count = static_cast<std::uint8_t>(g + 1);
					w.bone[0] = static_cast<std::uint16_t>(rd16(p) >> 6);
					float rest = 0;
					for (int k = 1; k <= g; k++)
					{
						w.bone[k] = static_cast<std::uint16_t>(rd16(p + 4 * k - 2) >> 6);
						w.weight[k] = static_cast<float>(rd16(p + 4 * k)) * BLEND_WEIGHT_SCALE;
						rest += w.weight[k];
					}
					w.weight[0] = 1.0f - rest;
					p += 2 * (2 * g + 1);
				}
			}
			return out;
		}

		std::size_t v = 0;
		for (const RigidVertList& list : rigid_lists())
		{
			for (std::uint32_t k = 0; k < list.vertCount && v < out.size(); k++, v++)
			{
				out[v].bone[0] = list.bone;
			}
		}
		return out;
	}
	catch (const std::exception&)
	{
		return {};
	}

	bool read_surface(const Zone& zone, const zaddr surface, SurfaceView& view)
	try
	{
		view = {};
		if (!read_header(zone, surface, view.raw))
		{
			return false;
		}
		view.address = surface;
		view.vertCount = view.raw.vertCount;
		view.triCount = view.raw.triCount;
		view.vertexBytes = zone.bytes(view.raw.verts0, static_cast<std::size_t>(view.vertCount) * sizeof(c1::GfxPackedVertex));
		view.indexBytes = zone.bytes(view.raw.triIndices, static_cast<std::size_t>(view.triCount) * 6);
		if (view.raw.vertListCount && view.raw.vertListCount < 0x10000)
		{
			view.rigidBytes = zone.bytes(view.raw.vertList, static_cast<std::size_t>(view.raw.vertListCount) * sizeof(c1::XRigidVertList));
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	bool read_model_surfs(const Zone& zone, const zaddr modelSurfs, ModelSurfsView& view)
	try
	{
		view = {};
		if (!read_header(zone, modelSurfs, view.raw))
		{
			return false;
		}
		view.address = modelSurfs;
		bool reference = false;
		resolve_name(zone, c1::ASSET_TYPE_XMODEL_SURFS, modelSurfs, view.raw.name, view.name, reference);
		// a ",name" XModelSurfs can still carry its surfaces ( common_gametype_mp: ",flag2_lod000" ), so only the
		// surface pointer decides
		view.external = zone.reference(modelSurfs) != nullptr || !view.raw.surfs;
		if (view.external)
		{
			return true;
		}
		view.surfaces.resize(view.raw.numsurfs);
		for (std::uint32_t i = 0; i < view.raw.numsurfs; i++)
		{
			if (!read_surface(zone, view.raw.surfs + i * static_cast<std::uint32_t>(sizeof(c1::XSurface)), view.surfaces[i]))
			{
				view.surfaces.resize(i);
				break;
			}
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	bool read_model(const Zone& zone, const zaddr model, ModelView& view, const bool withGeometry)
	try
	{
		view = {};
		if (!read_header(zone, model, view.raw))
		{
			return false;
		}
		view.address = model;
		resolve_name(zone, c1::ASSET_TYPE_XMODEL, model, view.raw.name, view.name, view.isReference);
		if (view.isReference)
		{
			return true;
		}
		const c1::XModel& x = view.raw;

		// bones
		const std::size_t numBones = x.numBones;
		const std::size_t numRoot = std::min<std::size_t>(x.numRootBones, numBones);
		const auto names = zone.bytes(x.boneNames, numBones * 2);
		const auto baseMat = zone.bytes(x.baseMat, numBones * DOBJ_ANIM_MAT_SIZE);
		const auto parents = zone.bytes(x.parentList, numBones - numRoot);
		const auto quats = zone.bytes(x.quats, (numBones - numRoot) * 8);
		const auto trans = zone.bytes(x.trans, (numBones - numRoot) * 12);
		const auto classification = zone.bytes(x.partClassification, numBones);
		view.bones.resize(numBones);
		for (std::size_t i = 0; i < numBones; i++)
		{
			ModelBone& bone = view.bones[i];
			if (!names.empty())
			{
				bone.name = zone.script_string(rd16(names.data() + 2 * i));
			}
			if (!baseMat.empty())
			{
				const std::uint8_t* bm = baseMat.data() + DOBJ_ANIM_MAT_SIZE * i;
				for (int k = 0; k < 4; k++)
				{
					bone.baseQuat[k] = rdf(bm + 4 * k);
				}
				for (int k = 0; k < 3; k++)
				{
					bone.baseTrans[k] = rdf(bm + 16 + 4 * k);
				}
				bone.transWeight = rdf(bm + 28);
			}
			if (!classification.empty())
			{
				bone.partClassification = classification[i];
			}
			if (i < numRoot)
			{
				std::copy(std::begin(bone.baseQuat), std::end(bone.baseQuat), bone.localQuat);
				std::copy(std::begin(bone.baseTrans), std::end(bone.baseTrans), bone.localTrans);
				continue;
			}
			const std::size_t k = i - numRoot;
			if (!parents.empty())
			{
				bone.parent = static_cast<int>(i) - parents[k];
			}
			if (!quats.empty())
			{
				for (int c = 0; c < 4; c++)
				{
					bone.localQuat[c] = static_cast<float>(static_cast<std::int16_t>(rd16(quats.data() + 8 * k + 2 * c))) * BONE_QUAT_SCALE;
				}
			}
			if (!trans.empty())
			{
				for (int c = 0; c < 3; c++)
				{
					bone.localTrans[c] = rdf(trans.data() + 12 * k + 4 * c);
				}
			}
		}

		// materials: one per surface
		view.materials = zone.get_array<zaddr>(x.materialHandles, x.numsurfs);
		view.materialNames.reserve(view.materials.size());
		for (const zaddr material : view.materials)
		{
			view.materialNames.push_back(zone.asset_name(c1::ASSET_TYPE_MATERIAL, material));
		}

		// lods
		const int numLods = std::clamp(static_cast<int>(static_cast<std::int8_t>(x.numLods)), 0, 4);
		view.lods.resize(static_cast<std::size_t>(numLods));
		for (int l = 0; l < numLods; l++)
		{
			const c1::XModelLodInfo& info = x.lodInfo[l];
			ModelLod& lod = view.lods[static_cast<std::size_t>(l)];
			lod.dist = info.dist;
			lod.numSurfs = info.numsurfs;
			lod.surfIndex = info.surfIndex;
			lod.surfsName = zone.c_string(info.surfsName);
			if (withGeometry)
			{
				read_model_surfs(zone, info.modelSurfs, lod.surfs);
			}
			else if (info.modelSurfs)
			{
				c1::XModelSurfs raw{};
				if (zone.get(info.modelSurfs, raw))
				{
					lod.surfs.address = info.modelSurfs;
					lod.surfs.raw = raw;
					bool reference = false;
					resolve_name(zone, c1::ASSET_TYPE_XMODEL_SURFS, info.modelSurfs, raw.name, lod.surfs.name, reference);
					lod.surfs.external = zone.reference(info.modelSurfs) != nullptr || !raw.surfs;
				}
			}
			if (lod.surfsName.empty())
			{
				lod.surfsName = lod.surfs.name;
			}
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	// ------------------------------------------------------------------------------------------
	// material / techset / image

	bool read_material(const Zone& zone, const zaddr material, MaterialView& view)
	try
	{
		view = {};
		if (!read_header(zone, material, view.raw))
		{
			return false;
		}
		view.address = material;
		resolve_name(zone, c1::ASSET_TYPE_MATERIAL, material, view.raw.name, view.name, view.isReference);
		if (view.isReference)
		{
			return true;
		}
		const c1::Material& m = view.raw;
		view.techsetName = zone.asset_name(c1::ASSET_TYPE_TECHNIQUE_SET, m.techniqueSet);

		const auto defs = zone.get_array<c1::MaterialTextureDef>(m.textureTable, m.textureCount);
		view.textures.reserve(defs.size());
		for (const c1::MaterialTextureDef& def : defs)
		{
			MaterialTexture texture;
			texture.raw = def;
			if (def.semantic != TEXTURE_SEMANTIC_WATER_MAP && def.image)
			{
				texture.image = def.image;
				texture.imageName = zone.asset_name(c1::ASSET_TYPE_IMAGE, def.image);
			}
			view.textures.push_back(texture);
		}

		view.constants = zone.get_array<c1::MaterialConstantDef>(m.constantTable, m.constantCount);
		view.stateBits = zone.get_array<std::array<std::uint32_t, 2>>(m.stateBitsTable, m.stateBitsCount);
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	bool read_techset(const Zone& zone, const zaddr techset, TechniqueSetView& view)
	try
	{
		view = {};
		if (!read_header(zone, techset, view.raw))
		{
			return false;
		}
		view.address = techset;
		resolve_name(zone, c1::ASSET_TYPE_TECHNIQUE_SET, techset, view.raw.name, view.name, view.isReference);
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	bool read_image(const Zone& zone, const zaddr image, ImageView& view)
	try
	{
		view = {};
		if (!read_header(zone, image, view.raw))
		{
			return false;
		}
		view.address = image;
		resolve_name(zone, c1::ASSET_TYPE_IMAGE, image, view.raw.name, view.name, view.isReference);
		if (view.isReference)
		{
			return true;
		}
		if (view.raw.loadDef && zone.get(view.raw.loadDef, view.loadDef) && view.loadDef.resourceSize)
		{
			view.pixels = zone.bytes(view.raw.loadDef + static_cast<zaddr>(sizeof(c1::GfxImageLoadDef)), view.loadDef.resourceSize);
			view.streamed = view.pixels.empty();
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	// ------------------------------------------------------------------------------------------
	// small assets

	bool read_rawfile(const Zone& zone, const zaddr rawfile, RawFileView& view)
	try
	{
		view = {};
		if (!read_header(zone, rawfile, view.raw))
		{
			return false;
		}
		view.address = rawfile;
		view.name = zone.asset_name(c1::ASSET_TYPE_RAWFILE, rawfile);
		const std::int32_t stored = view.raw.compressedLen ? view.raw.compressedLen : view.raw.len;
		if (stored > 0)
		{
			view.stored = zone.bytes(view.raw.buffer, static_cast<std::size_t>(stored));
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	Status rawfile_contents(const RawFileView& view, std::vector<std::uint8_t>& contents)
	{
		contents.clear();
		if (view.raw.len < 0 || view.raw.compressedLen < 0)
		{
			return Status::error("rawfile has a negative length");
		}
		if (view.raw.len == 0)
		{
			return {};
		}
		if (view.stored.empty())
		{
			return Status::error("rawfile buffer is not in the zone");
		}
		try
		{
			if (!view.raw.compressedLen)
			{
				contents.assign(view.stored.begin(), view.stored.end());
				return {};
			}
			if (const Status s = zlib_inflate(view.stored, contents, static_cast<std::size_t>(view.raw.len)); !s)
			{
				return s;
			}
			if (contents.size() != static_cast<std::size_t>(view.raw.len))
			{
				return Status::error("rawfile inflated to " + std::to_string(contents.size()) + " bytes, header says " + std::to_string(view.raw.len));
			}
			return {};
		}
		catch (const std::exception& e)
		{
			contents.clear();
			return Status::error(e.what());
		}
	}

	bool read_stringtable(const Zone& zone, const zaddr table, StringTableView& view)
	try
	{
		view = {};
		if (!read_header(zone, table, view.raw))
		{
			return false;
		}
		view.address = table;
		view.name = zone.asset_name(c1::ASSET_TYPE_STRINGTABLE, table);
		if (view.raw.columnCount > 0 && view.raw.rowCount > 0)
		{
			const std::size_t count = static_cast<std::size_t>(view.raw.columnCount) * static_cast<std::size_t>(view.raw.rowCount);
			const auto cells = zone.get_array<c1::StringTableCell>(view.raw.values, count);
			view.cells.reserve(cells.size());
			for (const c1::StringTableCell& cell : cells)
			{
				view.cells.push_back(zone.c_string(cell.string));
			}
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	bool read_localize(const Zone& zone, const zaddr entry, LocalizeView& view)
	try
	{
		view = {};
		c1::LocalizeEntry raw{};
		if (!read_header(zone, entry, raw))
		{
			return false;
		}
		view.address = entry;
		view.name = zone.c_string(raw.name);
		view.value = zone.c_string(raw.value);
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	bool read_anim(const Zone& zone, const zaddr anim, AnimView& view)
	try
	{
		view = {};
		if (!read_header(zone, anim, view.raw))
		{
			return false;
		}
		view.address = anim;
		resolve_name(zone, c1::ASSET_TYPE_XANIMPARTS, anim, view.raw.name, view.name, view.isReference);
		if (view.isReference)
		{
			return true;
		}
		view.frameCount = static_cast<std::uint32_t>(view.raw.numframes) + 1;

		const std::size_t parts = view.raw.boneCount[9];
		const auto names = zone.bytes(view.raw.names, parts * 2);
		view.boneNames.reserve(parts);
		for (std::size_t i = 0; i < parts && !names.empty(); i++)
		{
			view.boneNames.push_back(zone.script_string(rd16(names.data() + 2 * i)));
		}

		const auto notifies = zone.get_array<c1::XAnimNotifyInfo>(view.raw.notify, view.raw.notifyCount);
		view.notetracks.reserve(notifies.size());
		for (const c1::XAnimNotifyInfo& n : notifies)
		{
			view.notetracks.emplace_back(zone.script_string(n.name), n.time);
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	// ------------------------------------------------------------------------------------------
	// map

	bool read_map_ents(const Zone& zone, const zaddr mapEnts, MapEntsView& view)
	try
	{
		view = {};
		if (!read_header(zone, mapEnts, view.raw))
		{
			return false;
		}
		view.address = mapEnts;
		view.name = zone.asset_name(c1::ASSET_TYPE_MAP_ENTS, mapEnts);
		if (view.raw.numEntityChars > 1)
		{
			const auto text = zone.bytes(view.raw.entityString, static_cast<std::size_t>(view.raw.numEntityChars) - 1);
			view.entityString = { reinterpret_cast<const char*>(text.data()), text.size() };
		}
		view.triggerModelCount = view.raw.trigger.count;
		view.triggerHullCount = view.raw.trigger.hullCount;
		view.triggerSlabCount = view.raw.trigger.slabCount;
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	zaddr find_map_ents(const Zone& zone)
	{
		for (const AssetRecord& record : zone.all_assets())
		{
			if (record.type == c1::ASSET_TYPE_MAP_ENTS && !zone.reference(record.header))
			{
				const std::string_view rawName = zone.c_string(zone.get<zaddr>(record.header));
				if (!is_reference_name(rawName))
				{
					return record.header;
				}
			}
		}
		return 0;
	}

	std::uint32_t count_entities(const std::string_view entityString)
	{
		// "{" opens an entity only outside of a quoted key / value
		std::uint32_t count = 0;
		bool quoted = false;
		for (const char c : entityString)
		{
			if (c == '"')
			{
				quoted = !quoted;
			}
			else if (c == '{' && !quoted)
			{
				count++;
			}
		}
		return count;
	}

	bool read_com_world(const Zone& zone, const zaddr comWorld, ComWorldView& view)
	try
	{
		view = {};
		if (!read_header(zone, comWorld, view.raw))
		{
			return false;
		}
		view.address = comWorld;
		view.name = zone.asset_name(c1::ASSET_TYPE_COMWORLD, comWorld);
		view.primaryLightStride = COM_PRIMARY_LIGHT_SIZE;
		view.primaryLightBytes = zone.bytes(view.raw.primaryLights, static_cast<std::size_t>(view.raw.primaryLightCount) * COM_PRIMARY_LIGHT_SIZE);
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	bool read_clip_map(const Zone& zone, const zaddr clipMap, ClipMapView& view)
	try
	{
		view = {};
		if (!read_header(zone, clipMap, view.raw))
		{
			return false;
		}
		view.address = clipMap;
		const c1::clipMap_t& c = view.raw;
		view.name = zone.asset_name(c1::ASSET_TYPE_CLIPMAP_MP, clipMap);
		view.mapEnts = c.mapEnts;

		view.planes = zone.get_array<c1::cplane_s>(c.planes, c.planeCount > 0 ? static_cast<std::size_t>(c.planeCount) : 0);

		for (const c1::ClipMaterial& material : zone.get_array<c1::ClipMaterial>(c.materials, c.numMaterials))
		{
			view.materials.push_back(ClipMaterialView{ zone.c_string(material.name), material.surfaceFlags, material.contents });
		}

		const auto brushes = zone.get_array<c1::cbrush_t>(c.brushes, c.numBrushes);
		const auto bounds = zone.get_array<c1::Bounds>(c.brushBounds, c.numBrushes);
		const auto contents = zone.get_array<std::int32_t>(c.brushContents, c.numBrushes);
		view.brushes.resize(brushes.size());
		for (std::size_t i = 0; i < brushes.size(); i++)
		{
			Brush& brush = view.brushes[i];
			brush.raw = brushes[i];
			if (i < bounds.size())
			{
				brush.bounds = bounds[i];
			}
			if (i < contents.size())
			{
				brush.contents = contents[i];
			}

			// axial sides from the bounds: -x -y -z +x +y +z
			brush.sides.resize(6);
			for (int k = 0; k < 6; k++)
			{
				const int axis = k % 3;
				const bool positive = k >= 3;
				BrushSide& side = brush.sides[static_cast<std::size_t>(k)];
				side.plane.normal[axis] = positive ? 1.0f : -1.0f;
				side.plane.dist = positive ? brush.bounds.midPoint[axis] + brush.bounds.halfSize[axis] : -(brush.bounds.midPoint[axis] - brush.bounds.halfSize[axis]);
				side.plane.type = static_cast<std::uint8_t>(axis);
				side.materialNum = static_cast<std::uint16_t>(brush.raw.axialMaterialNum[positive ? 1 : 0][axis]);
			}

			for (const c1::cbrushside_t& stored : zone.get_array<c1::cbrushside_t>(brush.raw.sides, brush.raw.numsides))
			{
				BrushSide side;
				side.materialNum = stored.materialNum;
				if (stored.plane >= c.planes && (stored.plane - c.planes) % sizeof(c1::cplane_s) == 0
					&& (stored.plane - c.planes) / sizeof(c1::cplane_s) < view.planes.size())
				{
					side.planeIndex = static_cast<std::int32_t>((stored.plane - c.planes) / sizeof(c1::cplane_s));
					side.plane = view.planes[static_cast<std::size_t>(side.planeIndex)];
				}
				else
				{
					zone.get(stored.plane, side.plane);
				}
				brush.sides.push_back(side);
			}
		}

		view.subModels = zone.get_array<c1::cmodel_t>(c.cmodels, c.numSubModels);

		for (const c1::cStaticModel_s& model : zone.get_array<c1::cStaticModel_s>(c.staticModelList, c.numStaticModels))
		{
			view.staticModels.push_back(ClipStaticModel{ model, zone.asset_name(c1::ASSET_TYPE_XMODEL, model.xmodel) });
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}

	std::vector<c1::GfxWorldVertex> GfxWorldView::vertices() const
	try
	{
		std::vector<c1::GfxWorldVertex> out(vertexBytes.size() / sizeof(c1::GfxWorldVertex));
		if (!out.empty())
		{
			std::memcpy(out.data(), vertexBytes.data(), out.size() * sizeof(c1::GfxWorldVertex));
		}
		return out;
	}
	catch (const std::exception&)
	{
		return {};
	}

	std::vector<std::uint16_t> GfxWorldView::indices() const
	try
	{
		std::vector<std::uint16_t> out(indexBytes.size() / 2);
		if (!out.empty())
		{
			std::memcpy(out.data(), indexBytes.data(), out.size() * 2);
		}
		return out;
	}
	catch (const std::exception&)
	{
		return {};
	}

	bool read_gfx_world(const Zone& zone, const zaddr gfxWorld, GfxWorldView& view)
	try
	{
		view = {};
		if (!read_header(zone, gfxWorld, view.raw))
		{
			return false;
		}
		view.address = gfxWorld;
		const c1::GfxWorld& w = view.raw;
		view.name = zone.asset_name(c1::ASSET_TYPE_GFXWORLD, gfxWorld);
		view.baseName = zone.c_string(w.baseName);
		view.vertexBytes = zone.bytes(w.draw.vertices, static_cast<std::size_t>(w.draw.vertexCount) * sizeof(c1::GfxWorldVertex));
		view.indexBytes = zone.bytes(w.draw.indices, static_cast<std::size_t>(w.draw.indexCount) * 2);

		for (const c1::GfxSurface& surface : zone.get_array<c1::GfxSurface>(w.dpvs.surfaces, w.surfaceCount))
		{
			view.surfaces.push_back(WorldSurface{ surface, zone.asset_name(c1::ASSET_TYPE_MATERIAL, surface.material) });
		}
		for (const c1::GfxStaticModelDrawInst& inst : zone.get_array<c1::GfxStaticModelDrawInst>(w.dpvs.smodelDrawInsts, w.dpvs.smodelCount))
		{
			view.staticModels.push_back(WorldStaticModel{ inst, zone.asset_name(c1::ASSET_TYPE_XMODEL, inst.model) });
		}
		view.brushModels = zone.get_array<c1::GfxBrushModel>(w.models, w.modelCount > 0 ? static_cast<std::size_t>(w.modelCount) : 0);

		for (const c1::GfxCell& cell : zone.get_array<c1::GfxCell>(w.cells, w.cellCount > 0 ? static_cast<std::size_t>(w.cellCount) : 0))
		{
			WorldCell out;
			out.raw = cell;
			out.portals = zone.get_array<c1::GfxPortal>(cell.portals, cell.portalCount > 0 ? static_cast<std::size_t>(cell.portalCount) : 0);
			view.cells.push_back(std::move(out));
		}
		return true;
	}
	catch (const std::exception&)
	{
		view = {};
		return false;
	}
}
