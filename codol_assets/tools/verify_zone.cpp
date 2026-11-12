// codol_asset_verify model-extract / model-compare: a shipped model out to xmodel_export files, and two models of two
// zones ( the shipped one and the one codol_assetc built from those files ) compared as the zone reader sees them.
// This goes through everything: text, compiler, linker, the game's loaders, the reader.

#include "verify.hpp"

#include "codol_assets/model.hpp"
#include "codol_zone/assets.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace codol::assets;
using namespace codol::zone;
namespace c1 = codol::c1;
namespace fs = std::filesystem;

namespace
{
	bool find_model(const Zone& zone, const std::string& name, ModelView& view)
	{
		for (const AssetRecord& r : zone.all_assets())
		{
			if (r.type == c1::ASSET_TYPE_XMODEL && !zone.reference(r.header) && zone.asset_name(r.type, r.header) == name)
			{
				return read_model(zone, r.header, view, true) && !view.isReference;
			}
		}
		return false;
	}

	bool open(const fs::path& file, const fs::path& exe, Zone& zone)
	{
		ClientKey key;
		Status s = key.load_from_exe(exe);
		s = s ? zone.open(file, key) : s;
		if (!s)
		{
			std::fprintf(stderr, "%s: %s\n", file.string().c_str(), s.message.c_str());
		}
		return static_cast<bool>(s);
	}
}

int model_extract(const fs::path& zoneFile, const fs::path& exe, const std::string& name, const fs::path& outDir)
{
	Zone zone;
	ModelView view;
	if (!open(zoneFile, exe, zone))
	{
		return 1;
	}
	if (!find_model(zone, name, view))
	{
		std::fprintf(stderr, "no model '%s' with data in that zone\n", name.c_str());
		return 1;
	}
	std::string argument = name + "=";
	for (std::size_t l = 0; l < view.lods.size(); l++)
	{
		ExportModel mesh;
		if (!extract_model_lod(zone, view, l, mesh))
		{
			std::fprintf(stderr, "LOD %zu has its surfaces outside the zone\n", l);
			return 1;
		}
		const fs::path file = outDir / (name + "_lod" + std::to_string(l) + ".xmodel_export");
		const std::string text = write_xmodel_export(mesh, "extracted from " + zoneFile.filename().string());
		if (const Status s = write_file(file, { reinterpret_cast<const std::uint8_t*>(text.data()), text.size() }); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
		}
		char distance[32];
		std::snprintf(distance, sizeof(distance), "@%.9g", view.lods[l].dist);
		argument += (l ? "," : "") + file.generic_string() + distance;
	}
	if (view.raw.numCollSurfs > 0 && view.raw._unk298 != 255)
	{
		argument += ",collision=" + std::to_string(view.raw._unk298) + ",contents=" + std::to_string(view.raw.contents);
	}
	std::printf("%s\n", argument.c_str());		// the --xmodel argument of codol_assetc that rebuilds it
	return 0;
}

int model_compare(const fs::path& zoneA, const std::string& nameA, const fs::path& zoneB, const std::string& nameB, const fs::path& exe)
{
	Zone a, b;
	ModelView va, vb;
	if (!open(zoneA, exe, a) || !open(zoneB, exe, b))
	{
		return 1;
	}
	if (!find_model(a, nameA, va) || !find_model(b, nameB, vb))
	{
		std::fprintf(stderr, "model not found in one of the zones\n");
		return 1;
	}
	int failures = 0;
	const auto check = [&](const char* what, const bool same, const std::string& detail = {})
	{
		std::printf("%s %s %s\n", same ? "PASS" : "FAIL", what, detail.c_str());
		failures += same ? 0 : 1;
	};
	const auto near = [](const float x, const float y, const float eps) { return std::fabs(x - y) <= eps * std::max(1.0f, std::max(std::fabs(x), std::fabs(y))); };

	check("bone count, root count", va.bones.size() == vb.bones.size() && va.raw.numRootBones == vb.raw.numRootBones, std::to_string(va.bones.size()) + " bones");
	bool names = va.bones.size() == vb.bones.size(), parents = names, local = names, bind = names, classes = names;
	for (std::size_t k = 0; names && k < va.bones.size(); k++)
	{
		const ModelBone& x = va.bones[k];
		const ModelBone& y = vb.bones[k];
		names = x.name == y.name;
		parents = parents && x.parent == y.parent;
		classes = classes && x.partClassification == y.partClassification;
		// q and -q are the same rotation
		const auto same_rotation = [&](const float* p, const float* q)
		{
			bool plus = true, minus = true;
			for (int c = 0; c < 4; c++)
			{
				plus = plus && near(p[c], q[c], 3e-4f);
				minus = minus && near(p[c], -q[c], 3e-4f);
			}
			return plus || minus;
		};
		local = local && same_rotation(x.localQuat, y.localQuat);
		bind = bind && same_rotation(x.baseQuat, y.baseQuat);
		for (int c = 0; c < 3; c++)
		{
			local = local && near(x.localTrans[c], y.localTrans[c], 2e-3f);
			bind = bind && near(x.baseTrans[c], y.baseTrans[c], 1e-4f);
		}
	}
	check("bone names ( script strings )", names);
	check("bone parents", parents);
	check("bone local rotation and translation", local);
	check("bone bind pose", bind);
	check("part classification", classes, classes ? "" : "( the compiler writes 0 )");

	check("LOD count, collision LOD, contents", va.lods.size() == vb.lods.size() && va.raw._unk298 == vb.raw._unk298 && va.raw.contents == vb.raw.contents,
		std::to_string(va.lods.size()) + " LODs, collision LOD " + std::to_string(va.raw._unk298) + ", contents " + std::to_string(va.raw.contents));
	std::size_t vertices = 0, triangles = 0, surfaces = 0;
	bool dist = va.lods.size() == vb.lods.size(), counts = dist, positions = dist, uvs = dist, colors = dist, normals = dist, indices = dist, weights = dist, lists = dist,
		materials = va.materialNames == vb.materialNames, partBits = dist;
	double tangentClose = 0, tangentTotal = 0, signSame = 0;
	for (std::size_t l = 0; dist && l < va.lods.size(); l++)
	{
		dist = va.lods[l].dist == vb.lods[l].dist;
		const auto& sa = va.lods[l].surfs.surfaces;
		const auto& sb = vb.lods[l].surfs.surfaces;
		if (sa.size() != sb.size())
		{
			counts = false;
			continue;
		}
		for (std::size_t s = 0; s < sa.size(); s++)
		{
			surfaces++;
			vertices += sa[s].vertCount;
			triangles += sa[s].triCount;
			partBits = partBits && std::memcmp(sa[s].raw.partBits, sb[s].raw.partBits, sizeof(sa[s].raw.partBits)) == 0;
			if (sa[s].vertCount != sb[s].vertCount || sa[s].triCount != sb[s].triCount || sa[s].raw.deformed != sb[s].raw.deformed)
			{
				counts = false;
				continue;
			}
			const auto x = sa[s].vertices(), y = sb[s].vertices();
			const auto wx = sa[s].weights(a), wy = sb[s].weights(b);
			indices = indices && sa[s].indices() == sb[s].indices();
			const auto lx = sa[s].rigid_lists(), ly = sb[s].rigid_lists();
			lists = lists && lx.size() == ly.size();
			for (std::size_t k = 0; lists && k < lx.size(); k++)
			{
				lists = lx[k].bone == ly[k].bone && lx[k].vertCount == ly[k].vertCount && lx[k].triOffset == ly[k].triOffset && lx[k].triCount == ly[k].triCount;
			}
			for (std::size_t k = 0; k < x.size(); k++)
			{
				positions = positions && std::memcmp(x[k].position, y[k].position, 12) == 0;
				uvs = uvs && x[k].uv[0] == y[k].uv[0] && x[k].uv[1] == y[k].uv[1];
				colors = colors && std::memcmp(x[k].color, y[k].color, 4) == 0;
				float error = 0, cosine = 0, length = 0;
				for (int c = 0; c < 3; c++)
				{
					error += (x[k].normal[c] - y[k].normal[c]) * (x[k].normal[c] - y[k].normal[c]);
					cosine += x[k].tangent[c] * y[k].tangent[c];
					length += x[k].tangent[c] * x[k].tangent[c];
				}
				normals = normals && std::sqrt(error) < 0.02f;
				if (length > 0.25f)
				{
					tangentTotal++;
					tangentClose += cosine / std::sqrt(length) > 0.9659f ? 1 : 0;
					signSame += x[k].binormalSign == y[k].binormalSign ? 1 : 0;
				}
				weights = weights && k < wx.size() && k < wy.size() && wx[k].count == wy[k].count;
				for (int w = 0; weights && w < wx[k].count; w++)
				{
					weights = wx[k].bone[w] == wy[k].bone[w] && std::fabs(wx[k].weight[w] - wy[k].weight[w]) < 4e-5f;
				}
			}
		}
	}
	const std::string size = std::to_string(surfaces) + " surfaces, " + std::to_string(vertices) + " vertices, " + std::to_string(triangles) + " triangles";
	check("LOD distances", dist);
	check("surfaces: vertex and triangle counts, deformed", counts, size);
	check("materials per surface", materials);
	check("vertex positions ( bit exact )", positions);
	check("vertex uvs ( bit exact )", uvs);
	check("vertex colours", colors);
	check("vertex normals ( within 0.02 )", normals);
	check("triangle indices", indices);
	check("bone weights", weights);
	check("rigid vertex lists", lists);
	check("part bits", partBits);
	char text[200];
	std::snprintf(text, sizeof(text), "%.2f %% within 15 degrees, binormal sign equal %.2f %%", tangentTotal ? 100.0 * tangentClose / tangentTotal : 0.0,
		tangentTotal ? 100.0 * signSame / tangentTotal : 0.0);
	std::printf("INFO tangents ( rebuilt from positions and uvs ): %s\n", text);

	float ba[7], bb[7];
	std::memcpy(ba, va.raw._unk316, sizeof(ba));
	std::memcpy(bb, vb.raw._unk316, sizeof(bb));
	bool bounds = true;
	for (int k = 0; k < 7; k++)
	{
		bounds = bounds && near(ba[k], bb[k], 2e-3f);
	}
	std::snprintf(text, sizeof(text), "radius %g, centre %g %g %g, half size %g %g %g", ba[0], ba[1], ba[2], ba[3], ba[4], ba[5], ba[6]);
	check("radius and bounds", bounds, text);
	const auto ia = a.get_array<float>(va.raw.boneInfo, va.bones.size() * 7), ib = b.get_array<float>(vb.raw.boneInfo, vb.bones.size() * 7);
	bool info = ia.size() == ib.size() && !ia.empty();
	for (std::size_t k = 0; info && k < ia.size(); k++)
	{
		info = near(ia[k], ib[k], 5e-3f) || std::fabs(ia[k] - ib[k]) < 5e-3f;
	}
	check("bone info", info);
	bool collision = va.raw.numCollSurfs == vb.raw.numCollSurfs;
	std::size_t collisionTris = 0, twins = 0;
	double worst = 0;
	if (collision && va.raw.numCollSurfs > 0)
	{
		const auto ca = a.bytes(va.raw.collSurfs, static_cast<std::size_t>(va.raw.numCollSurfs) * 44), cb = b.bytes(vb.raw.collSurfs, static_cast<std::size_t>(vb.raw.numCollSurfs) * 44);
		for (int k = 0; collision && k < va.raw.numCollSurfs; k++)
		{
			std::uint32_t trisA, trisB;
			std::int32_t countA, countB;
			std::memcpy(&trisA, ca.data() + k * 44, 4);
			std::memcpy(&trisB, cb.data() + k * 44, 4);
			std::memcpy(&countA, ca.data() + k * 44 + 4, 4);
			std::memcpy(&countB, cb.data() + k * 44 + 4, 4);
			collision = countA == countB && std::memcmp(ca.data() + k * 44 + 32, cb.data() + k * 44 + 32, 4) == 0;		// bone
			const auto ta = a.get_array<float>(trisA, static_cast<std::size_t>(countA) * 12), tb = b.get_array<float>(trisB, static_cast<std::size_t>(countB) * 12);
			collisionTris += ta.size() / 12;
			for (std::size_t f = 0; collision && f < ta.size(); f++)
			{
				worst = std::max<double>(worst, std::fabs(ta[f] - tb[f]) / std::max(1.0f, std::fabs(ta[f])));
			}
			// the same records in another order?
			for (std::size_t f = 0; collision && f + 11 < ta.size(); f += 12)
			{
				bool twin = false;
				for (std::size_t g = 0; !twin && g + 11 < tb.size(); g += 12)
				{
					twin = true;
					for (std::size_t c = 0; twin && c < 12; c++)
					{
						twin = std::fabs(ta[f + c] - tb[g + c]) <= 1e-3f * std::max(1.0f, std::fabs(ta[f + c]));
					}
				}
				twins += twin ? 1 : 0;
			}
		}
	}
	std::snprintf(text, sizeof(text), "%d surfaces, %zu triangle records, largest relative difference of a plane / s / t value %.2g", va.raw.numCollSurfs, collisionTris, worst);
	check("collision surfaces ( count, bone, triangle count )", collision, text);
	// the order of the records inside a surface is not reproduced for every model; the set is what a trace sees
	check("collision triangle records ( plane, s and t vectors within 1e-3, any order; 99 % )", collision && twins * 100 >= collisionTris * 99,
		std::to_string(twins) + " of " + std::to_string(collisionTris) + " records are in the other model's surface" + (worst < 1e-3 ? ", in the same order" : ""));
	std::printf("%s\n", failures ? "DIFFERENT" : "the two models are the same");
	return failures ? 1 : 0;
}
