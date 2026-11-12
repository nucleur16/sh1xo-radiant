// codol_asset_verify model-roundtrip: every shipped model whose surfaces are in its zone -> xmodel_export text per
// LOD ( the library's own extractor ) -> compile -> compare with the shipped XModel.
// codol_asset_verify model-file: one export file ( e.g. written by CODOL-AssetExplorer ) against the shipped model.

#include "verify.hpp"

#include "codol_assets/model.hpp"
#include "codol_zone/assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>

using namespace codol::assets;
using namespace codol::zone;
namespace c1 = codol::c1;
namespace fs = std::filesystem;

namespace
{
	struct CollSurf
	{
		std::uint32_t tris;
		std::int32_t count;
		c1::Bounds bounds;
		std::int32_t bone, contents, surfaceFlags;
	};

	bool close(const float a, const float b, const float eps) { return std::fabs(a - b) <= eps * std::max(1.0f, std::max(std::fabs(a), std::fabs(b))); }

	template <typename A, typename B>
	bool close_all(const A& a, const B& b, const std::size_t count, const float eps)
	{
		for (std::size_t k = 0; k < count; k++)
		{
			if (!close(a[k], b[k], eps))
			{
				return false;
			}
		}
		return true;
	}

	struct Totals
	{
		Rate boneNames, parents, quats, trans, baseMat, lodCount, lodDist, surfaceCount, surfaceShape, positions, uvs, colors, normals, indices, rigid, blend, partBits,
			bounds, radius, boneInfo, collisionLod, contents, collisionCount, collisionTris, whole;
		std::uint64_t vertices = 0, tangentClose = 0, tangentSign = 0, tangentDegenerate = 0, facesAgainst = 0, faces = 0;
		double normalErrorMax = 0;
		std::uint32_t models = 0, skippedExternal = 0, failed = 0;
		std::uint64_t listSurfaces = 0, listShort = 0, listShortByDegenerate = 0, listShortByOne = 0, listShortTrailing = 0;
	};

	// compares a compiled model with the shipped one; false when anything differs
	bool compare(const Zone& zone, const ModelView& shipped, const CompiledModel& mine, Totals& t, std::string* why)
	{
		bool all = true;
		std::string detail;
		const auto check = [&](Rate& rate, const bool same, const char* what)
		{
			rate.add(same);
			if (!same && all && why)
			{
				*why = what + detail;
			}
			all = all && same;
		};
		const std::size_t bones = shipped.bones.size(), roots = shipped.raw.numRootBones;
		bool names = mine.boneNames.size() == bones, parents = mine.rootBones == roots && mine.parentList.size() == bones - roots, quats = parents, trans = parents, base = names;
		for (std::size_t k = 0; names && k < bones; k++)
		{
			names = mine.boneNames[k] == shipped.bones[k].name;
			const float flipped[4] = { -mine.baseMat[k * 8], -mine.baseMat[k * 8 + 1], -mine.baseMat[k * 8 + 2], -mine.baseMat[k * 8 + 3] };
			const bool sameBase = (close_all(&mine.baseMat[k * 8], shipped.bones[k].baseQuat, 4, 2e-4f) || close_all(flipped, shipped.bones[k].baseQuat, 4, 2e-4f)) && close_all(&mine.baseMat[k * 8 + 4], shipped.bones[k].baseTrans, 3, 1e-4f)
				&& close(mine.baseMat[k * 8 + 7], shipped.bones[k].transWeight, 2e-4f);		// shipped: 2 / | quat |^2 of a quat that went through s16
			if (!sameBase && base)
			{
				char text[300];
				std::snprintf(text, sizeof(text), " [ bone %zu: mine %g %g %g %g | %g %g %g | %g, shipped %g %g %g %g | %g %g %g | %g ]", k, mine.baseMat[k * 8], mine.baseMat[k * 8 + 1],
					mine.baseMat[k * 8 + 2], mine.baseMat[k * 8 + 3], mine.baseMat[k * 8 + 4], mine.baseMat[k * 8 + 5], mine.baseMat[k * 8 + 6], mine.baseMat[k * 8 + 7],
					shipped.bones[k].baseQuat[0], shipped.bones[k].baseQuat[1], shipped.bones[k].baseQuat[2], shipped.bones[k].baseQuat[3], shipped.bones[k].baseTrans[0],
					shipped.bones[k].baseTrans[1], shipped.bones[k].baseTrans[2], shipped.bones[k].transWeight);
				detail = text;
			}
			base = base && sameBase;
			if (parents && k >= roots)
			{
				const std::size_t c = k - roots;
				parents = static_cast<int>(k) - mine.parentList[c] == shipped.bones[k].parent;
				float q[4], negative[4];
				for (int i = 0; i < 4; i++)
				{
					q[i] = static_cast<float>(mine.quats[c * 4 + static_cast<std::size_t>(i)]) / 32767.0f;
					negative[i] = -q[i];
				}
				quats = quats && (close_all(q, shipped.bones[k].localQuat, 4, 3e-4f) || close_all(negative, shipped.bones[k].localQuat, 4, 3e-4f));
				trans = trans && close_all(&mine.trans[c * 3], shipped.bones[k].localTrans, 3, 2e-3f);
			}
		}
		check(t.boneNames, names, "bone names");
		check(t.parents, parents, "bone parents");
		check(t.quats, quats, "bone local rotations");
		check(t.trans, trans, "bone local translations");
		check(t.baseMat, base, "bone bind pose");

		check(t.lodCount, mine.lods.size() == shipped.lods.size(), "LOD count");
		bool dist = mine.lods.size() == shipped.lods.size(), count = dist, shape = dist, positions = dist, uvs = dist, colors = dist, normals = dist, indices = dist, rigid = dist,
			blend = dist, bits = dist;
		for (std::size_t l = 0; dist && l < mine.lods.size(); l++)
		{
			const ModelLod& theirs = shipped.lods[l];
			const CompiledLod& lod = mine.lods[l];
			dist = dist && lod.distance == theirs.dist;
			if (lod.surfaces.size() != theirs.surfs.surfaces.size())
			{
				count = shape = positions = uvs = colors = normals = indices = rigid = blend = bits = false;
				continue;
			}
			bits = bits && std::memcmp(lod.partBits, theirs.surfs.raw.partBits, sizeof(lod.partBits)) == 0;
			for (std::size_t s = 0; s < lod.surfaces.size(); s++)
			{
				const CompiledSurface& a = lod.surfaces[s];
				const SurfaceView& b = theirs.surfs.surfaces[s];
				const bool sameShape = a.header.vertCount == b.raw.vertCount && a.header.triCount == b.raw.triCount && a.header.deformed == b.raw.deformed
					&& a.header.vertListCount == b.raw.vertListCount && a.header.flags == (b.raw.flags & 0x20) && a.header.baseTriIndex == b.raw.baseTriIndex
					&& a.header.baseVertIndex == b.raw.baseVertIndex && std::memcmp(a.header.vertInfoCount, b.raw.vertInfoCount, 8) == 0;
				shape = shape && sameShape;
				bits = bits && std::memcmp(a.header.partBits, b.raw.partBits, sizeof(a.header.partBits)) == 0;
				if (a.header.vertCount != b.raw.vertCount || a.header.triCount != b.raw.triCount)
				{
					positions = uvs = colors = normals = indices = rigid = blend = false;
					continue;
				}
				const std::vector<SurfaceVertex> theirVertices = b.vertices();
				const std::vector<std::uint16_t> theirIndices = b.indices();
				if (a.indices != theirIndices && indices && detail.empty())
				{
					std::size_t at = 0;
					while (at < a.indices.size() && at < theirIndices.size() && a.indices[at] == theirIndices[at])
					{
						at++;
					}
					char text[200];
					std::snprintf(text, sizeof(text), " [ lod %zu surface %zu: first index difference at %zu of %zu, %s, %u lists ]", l, s, at, a.indices.size(),
						a.header.deformed ? "deformed" : "rigid", a.header.vertListCount);
					detail = text;
				}
				indices = indices && a.indices == theirIndices;
				// how many of the shipped triangles are degenerate ( an index twice ), and whether they are the last ones
				if (!a.rigid.empty() && b.rigidBytes.size() >= sizeof(c1::XRigidVertList))
				{
					std::size_t degenerate = 0, trailing = 0;
					for (std::size_t k = 0; k + 2 < theirIndices.size(); k += 3)
					{
						const bool d = theirIndices[k] == theirIndices[k + 1] || theirIndices[k] == theirIndices[k + 2] || theirIndices[k + 1] == theirIndices[k + 2];
						degenerate += d ? 1 : 0;
						trailing = d ? trailing + 1 : 0;
					}
					std::size_t listed = 0;
					for (std::size_t k = 0; k * sizeof(c1::XRigidVertList) < b.rigidBytes.size(); k++)
					{
						c1::XRigidVertList theirList;
						std::memcpy(&theirList, b.rigidBytes.data() + k * sizeof(c1::XRigidVertList), sizeof(theirList));
						listed += theirList.triCount;
					}
					t.listSurfaces++;
					t.listShort += listed != b.triCount ? 1 : 0;
					t.listShortByDegenerate += listed != b.triCount && b.triCount - listed == degenerate ? 1 : 0;
					t.listShortByOne += listed + 1 == b.triCount ? 1 : 0;
					t.listShortTrailing += listed != b.triCount && b.triCount - listed == trailing ? 1 : 0;
				}
				rigid = rigid && a.rigid.size() * sizeof(c1::XRigidVertList) == b.rigidBytes.size();
				for (std::size_t k = 0; rigid && k < a.rigid.size(); k++)
				{
					rigid = std::memcmp(&a.rigid[k], b.rigidBytes.data() + k * sizeof(c1::XRigidVertList), 8) == 0;		// without the collision tree pointer
					if (!rigid && detail.empty())
					{
						c1::XRigidVertList theirList;
						std::memcpy(&theirList, b.rigidBytes.data() + k * sizeof(c1::XRigidVertList), sizeof(theirList));
						char text[200];
						std::snprintf(text, sizeof(text), " [ lod %zu surface %zu list %zu of %zu: mine bone %u verts %u tris %u+%u, shipped bone %u verts %u tris %u+%u; surface %u verts %u tris ]",
							l, s, k, a.rigid.size(), a.rigid[k].boneOffset / 64, a.rigid[k].vertCount, a.rigid[k].triOffset, a.rigid[k].triCount, theirList.boneOffset / 64,
							theirList.vertCount, theirList.triOffset, theirList.triCount, a.header.vertCount, a.header.triCount);
						detail = text;
					}
				}
				if (!a.blend.empty() || b.raw.vertsBlend)
				{
					const auto theirBlend = zone.bytes(b.raw.vertsBlend, a.blend.size() * 2);
					bool same = theirBlend.size() == a.blend.size() * 2;
					for (std::size_t k = 0; same && k < a.blend.size(); k++)
					{
						std::uint16_t v;
						std::memcpy(&v, theirBlend.data() + k * 2, 2);
						same = std::abs(static_cast<int>(v) - static_cast<int>(a.blend[k])) <= 1;		// weights go through six decimals of text
					}
					blend = blend && same;
				}
				for (std::size_t k = 0; k < a.vertices.size(); k++)
				{
					const c1::GfxPackedVertex& v = a.vertices[k];
					const SurfaceVertex& w = theirVertices[k];
					positions = positions && std::memcmp(v.xyz, w.position, 12) == 0;
					uvs = uvs && half_to_float(v.texCoordU) == w.uv[0] && half_to_float(v.texCoordV) == w.uv[1];
					const std::uint32_t theirColor = (static_cast<std::uint32_t>(w.color[3]) << 24) | (static_cast<std::uint32_t>(w.color[0]) << 16)
						| (static_cast<std::uint32_t>(w.color[1]) << 8) | w.color[2];
					colors = colors && v.color == theirColor;
					float n[3], tangent[3];
					unpack_unit_vector(v.normal, n);
					unpack_unit_vector(v.tangent, tangent);
					const float error = std::sqrt((n[0] - w.normal[0]) * (n[0] - w.normal[0]) + (n[1] - w.normal[1]) * (n[1] - w.normal[1]) + (n[2] - w.normal[2]) * (n[2] - w.normal[2]));
					t.normalErrorMax = std::max<double>(t.normalErrorMax, error);
					normals = normals && error < 0.02f;
					// tangents are derived data: how close the rebuilt frame is to the shipped one
					t.vertices++;
					const float tl = std::sqrt(w.tangent[0] * w.tangent[0] + w.tangent[1] * w.tangent[1] + w.tangent[2] * w.tangent[2]);
					if (tl < 0.5f)
					{
						t.tangentDegenerate++;
					}
					else
					{
						const float cosine = (tangent[0] * w.tangent[0] + tangent[1] * w.tangent[1] + tangent[2] * w.tangent[2]) / tl;
						t.tangentClose += cosine > 0.9659f ? 1 : 0;		// within 15 degrees
						t.tangentSign += v.binormalSign == w.binormalSign ? 1 : 0;
					}
				}
				// winding of the shipped triangles against their vertex normals
				for (std::size_t k = 0; k + 2 < theirIndices.size(); k += 3)
				{
					const SurfaceVertex& p0 = theirVertices[theirIndices[k]];
					const SurfaceVertex& p1 = theirVertices[theirIndices[k + 1]];
					const SurfaceVertex& p2 = theirVertices[theirIndices[k + 2]];
					const float e1[3] = { p1.position[0] - p0.position[0], p1.position[1] - p0.position[1], p1.position[2] - p0.position[2] };
					const float e2[3] = { p2.position[0] - p0.position[0], p2.position[1] - p0.position[1], p2.position[2] - p0.position[2] };
					const float g[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
					const float d = g[0] * (p0.normal[0] + p1.normal[0] + p2.normal[0]) + g[1] * (p0.normal[1] + p1.normal[1] + p2.normal[1])
						+ g[2] * (p0.normal[2] + p1.normal[2] + p2.normal[2]);
					if (d != 0)
					{
						t.faces++;
						t.facesAgainst += d < 0 ? 1 : 0;
					}
				}
			}
		}
		check(t.lodDist, dist, "LOD distances");
		check(t.surfaceCount, count, "surfaces per LOD");
		check(t.surfaceShape, shape, "surface header ( counts, deformed, lists, base indices )");
		check(t.positions, positions, "vertex positions");
		check(t.uvs, uvs, "vertex uvs");
		check(t.colors, colors, "vertex colours");
		check(t.normals, normals, "vertex normals");
		check(t.indices, indices, "triangle indices");
		check(t.rigid, rigid, "rigid vertex lists");
		check(t.blend, blend, "blend weight stream");
		check(t.partBits, bits, "part bits");

		float theirBounds[7];
		std::memcpy(theirBounds, shipped.raw._unk316, sizeof(theirBounds));
		check(t.radius, close(mine.radius, theirBounds[0], 2e-3f), "radius");
		check(t.bounds, close_all(&mine.bounds.midPoint[0], &theirBounds[1], 6, 2e-3f), "bounds");
		const std::vector<float> info = zone.get_array<float>(shipped.raw.boneInfo, bones * 7);
		bool sameInfo = info.size() == mine.boneInfo.size();
		for (std::size_t k = 0; sameInfo && k < info.size(); k++)
		{
			sameInfo = close(info[k], mine.boneInfo[k], k % 7 == 6 ? 5e-3f : 3e-3f) || std::fabs(info[k] - mine.boneInfo[k]) < 5e-3f;
		}
		if (!sameInfo && info.size() == mine.boneInfo.size())
		{
			for (std::size_t k = 0; k < info.size(); k += 7)
			{
				bool same = true;
				for (std::size_t c = 0; c < 7; c++)
				{
					same = same && (close(info[k + c], mine.boneInfo[k + c], 5e-3f) || std::fabs(info[k + c] - mine.boneInfo[k + c]) < 5e-3f);
				}
				if (!same)
				{
					char text[300];
					std::snprintf(text, sizeof(text), " [ bone %zu of %zu: mine %g %g %g | %g %g %g | %g, shipped %g %g %g | %g %g %g | %g ]", k / 7, bones, mine.boneInfo[k], mine.boneInfo[k + 1],
						mine.boneInfo[k + 2], mine.boneInfo[k + 3], mine.boneInfo[k + 4], mine.boneInfo[k + 5], mine.boneInfo[k + 6], info[k], info[k + 1], info[k + 2], info[k + 3],
						info[k + 4], info[k + 5], info[k + 6]);
					detail = text;
					break;
				}
			}
		}
		check(t.boneInfo, sameInfo, "bone info ( per bone bounds )");
		check(t.collisionLod, (mine.collisionLod < 0 ? 255 : mine.collisionLod) == shipped.raw._unk298 || shipped.raw.numCollSurfs == 0, "collision LOD");
		check(t.contents, mine.contents == shipped.raw.contents || shipped.raw.numCollSurfs == 0, "contents");
		if (shipped.raw.numCollSurfs > 0)
		{
			const std::vector<CollSurf> theirs = zone.get_array<CollSurf>(shipped.raw.collSurfs, static_cast<std::size_t>(shipped.raw.numCollSurfs));
			std::size_t theirTris = 0, myTris = 0;
			for (const CollSurf& c : theirs)
			{
				theirTris += static_cast<std::size_t>(c.count);
			}
			for (const CompiledCollisionSurface& c : mine.collision)
			{
				myTris += c.triangles.size();
			}
			if (theirs.size() != mine.collision.size())
			{
				detail = " [ shipped " + std::to_string(theirs.size()) + ":";
				for (const CollSurf& c : theirs)
				{
					char text[80];
					std::snprintf(text, sizeof(text), " bone %d contents 0x%X flags 0x%X tris %d;", c.bone, c.contents, c.surfaceFlags, c.count);
					detail += text;
				}
				detail += " mine " + std::to_string(mine.collision.size()) + " ]";
			}
			check(t.collisionCount, theirs.size() == mine.collision.size(), "collision surface count");
			check(t.collisionTris, theirTris == myTris, "collision triangle count");
		}
		t.whole.add(all);
		return all;
	}

	void report(const Totals& t)
	{
		std::printf("%u models compiled and compared ( %u more have surfaces outside their zone, %u failed to compile )\n", t.models, t.skippedExternal, t.failed);
		print_rate("bone names", t.boneNames);
		print_rate("bone parents, root count", t.parents);
		print_rate("bone local rotations ( s16 )", t.quats);
		print_rate("bone local translations", t.trans);
		print_rate("bone bind pose", t.baseMat);
		print_rate("LOD count", t.lodCount);
		print_rate("LOD distances", t.lodDist);
		print_rate("surfaces per LOD", t.surfaceCount);
		print_rate("surface headers", t.surfaceShape);
		print_rate("vertex positions ( bit exact )", t.positions);
		print_rate("vertex uvs ( bit exact )", t.uvs);
		print_rate("vertex colours ( exact )", t.colors);
		print_rate("vertex normals ( < 0.02 )", t.normals);
		print_rate("triangle indices ( exact )", t.indices);
		print_rate("rigid vertex lists", t.rigid);
		print_rate("blend weight streams ( +- 1 )", t.blend);
		print_rate("part bits", t.partBits);
		print_rate("model radius", t.radius);
		print_rate("model bounds", t.bounds);
		print_rate("bone info", t.boneInfo);
		print_rate("collision LOD", t.collisionLod);
		print_rate("contents", t.contents);
		print_rate("collision surface count", t.collisionCount);
		print_rate("collision triangle count", t.collisionTris);
		print_rate("all of the above", t.whole);
		const std::uint64_t framed = t.vertices - t.tangentDegenerate;
		std::printf("  largest normal error %.5f; tangents ( rebuilt from positions and uvs, %llu vertices ): %.2f %% within 15 degrees of the shipped tangent, binormal sign equal %.2f %%\n",
			t.normalErrorMax, static_cast<unsigned long long>(framed), framed ? 100.0 * static_cast<double>(t.tangentClose) / static_cast<double>(framed) : 0.0,
			framed ? 100.0 * static_cast<double>(t.tangentSign) / static_cast<double>(framed) : 0.0);
		std::printf("  rigid surfaces: %llu; lists cover fewer triangles than the surface has: %llu ( by one: %llu, by the number of degenerate triangles: %llu, of trailing degenerate ones: %llu )\n",
			static_cast<unsigned long long>(t.listSurfaces), static_cast<unsigned long long>(t.listShort), static_cast<unsigned long long>(t.listShortByOne),
			static_cast<unsigned long long>(t.listShortByDegenerate), static_cast<unsigned long long>(t.listShortTrailing));
		std::printf("  shipped triangles whose cross( v1 - v0, v2 - v0 ) points against the vertex normals: %.3f %% of %llu\n",
			t.faces ? 100.0 * static_cast<double>(t.facesAgainst) / static_cast<double>(t.faces) : 0.0, static_cast<unsigned long long>(t.faces));
	}

	// the collision LOD of a shipped model: the one whose triangle count the collision surfaces have ( the header names it )
	ModelOptions options_of(const ModelView& view)
	{
		ModelOptions options;
		options.name = std::string(view.name);
		options.reverseWinding = 0;
		if (view.raw.numCollSurfs > 0 && view.raw._unk298 != 255)
		{
			options.collisionLod = view.raw._unk298;
			options.contents = view.raw.contents;
		}
		return options;
	}
}

int model_roundtrip(const fs::path& install, const fs::path& exe, const int every, const std::string& filter)
{
	ClientKey key;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	Totals totals;
	std::set<std::string> seen;
	std::uint32_t index = 0, shown = 0;
	for (const fs::path& file : install_zones(install))
	{
		if (!filter.empty() && file.stem().string().find(filter) == std::string::npos)
		{
			continue;
		}
		Zone zone;
		if (!zone.open(file, key))
		{
			continue;
		}
		for (const AssetRecord& r : zone.all_assets())
		{
			if (r.type != c1::ASSET_TYPE_XMODEL || zone.reference(r.header))
			{
				continue;
			}
			ModelView view;
			if (!read_model(zone, r.header, view, true) || view.isReference || !seen.insert(std::string(view.name)).second)
			{
				continue;
			}
			bool external = view.lods.empty();
			for (const ModelLod& lod : view.lods)
			{
				external = external || lod.surfs.external;
			}
			if (external)
			{
				totals.skippedExternal++;
				continue;
			}
			if ((index++ % static_cast<std::uint32_t>(every)) != 0)
			{
				continue;
			}
			ModelOptions options = options_of(view);
			bool ok = true;
			for (std::size_t l = 0; ok && l < view.lods.size(); l++)
			{
				ExportModel extracted;
				ModelLodSource lod;
				lod.distance = view.lods[l].dist;
				ok = extract_model_lod(zone, view, l, extracted);
				// through the text, as a modder would have it
				ok = ok && static_cast<bool>(parse_xmodel_export(write_xmodel_export(extracted), lod.mesh));
				options.lods.push_back(std::move(lod));
			}
			CompiledModel compiled;
			Status status;
			if (!ok || !(status = compile_model(options, compiled)))
			{
				totals.failed++;
				if (shown++ < 10)
				{
					std::fprintf(stderr, "%s: %s\n", options.name.c_str(), ok ? status.message.c_str() : "cannot be extracted");
				}
				continue;
			}
			totals.models++;
			std::string why;
			if (!compare(zone, view, compiled, totals, &why) && shown++ < 25)
			{
				std::fprintf(stderr, "%s ( %s ): first difference: %s\n", options.name.c_str(), file.stem().string().c_str(), why.c_str());
			}
		}
	}
	report(totals);
	return 0;
}

int model_file(const fs::path& zoneFile, const fs::path& exe, const std::string& name, const std::vector<fs::path>& exports)
{
	ClientKey key;
	Zone zone;
	if (const Status s = key.load_from_exe(exe); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	if (const Status s = zone.open(zoneFile, key); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	ModelView view;
	zaddr header = 0;
	for (const AssetRecord& r : zone.all_assets())
	{
		if (r.type == c1::ASSET_TYPE_XMODEL && !zone.reference(r.header) && zone.asset_name(r.type, r.header) == name)
		{
			header = r.header;
			break;
		}
	}
	if (!header || !read_model(zone, header, view, true))
	{
		std::fprintf(stderr, "no model '%s' with data in that zone\n", name.c_str());
		return 1;
	}
	ModelOptions options = options_of(view);
	options.reverseWinding = -1;		// a foreign exporter: let the compiler find the winding
	for (std::size_t l = 0; l < exports.size(); l++)
	{
		ModelLodSource lod;
		lod.distance = l < view.lods.size() ? view.lods[l].dist : 1000000.0f;
		if (const Status s = load_xmodel_export(exports[l], lod.mesh); !s)
		{
			std::fprintf(stderr, "%s\n", s.message.c_str());
			return 1;
		}
		std::printf("%s: %zu bones, %zu vertices, %zu triangles, %zu materials\n", exports[l].filename().string().c_str(), lod.mesh.bones.size(), lod.mesh.vertices.size(),
			lod.mesh.faces.size(), lod.mesh.materials.size());
		options.lods.push_back(std::move(lod));
	}
	// compare only the LODs that were given
	view.lods.resize(std::min(view.lods.size(), exports.size()));
	CompiledModel compiled;
	if (const Status s = compile_model(options, compiled); !s)
	{
		std::fprintf(stderr, "%s\n", s.message.c_str());
		return 1;
	}
	for (const std::string& note : compiled.notes)
	{
		std::printf("note: %s\n", note.c_str());
	}
	Totals totals;
	totals.models = 1;
	std::string why;
	const bool same = compare(zone, view, compiled, totals, &why);
	report(totals);
	if (!same)
	{
		std::printf("first difference: %s\n", why.c_str());
	}
	return 0;
}
