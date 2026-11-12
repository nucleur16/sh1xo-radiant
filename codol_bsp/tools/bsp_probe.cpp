// codol_bsp_probe: development aid. Prints pieces of the zone stream of an asset, so that what the compiler writes can
// be compared with what the game's own map compiler wrote.
//
//   codol_bsp_probe dump <zone.ff> <C1.exe> <asset type name> [first piece] [piece count] [max bytes per piece]
//   codol_bsp_probe pieces <zone.ff> <C1.exe> <asset type name>          one line per piece: size, string, pointer count
//   codol_bsp_probe check <zone.ff> <C1.exe>                             the offline validators of codol_bsp on any map zone
//   codol_bsp_probe portals|cells|terrain|smodels <zone.ff> <C1.exe>     how a map's portals, cells, terrain, static models are stored
//   codol_bsp_probe sun <zone.ff> <C1.exe>                               what the sun and its shadows hang on ( header fields, lights, ranges )
//   codol_bsp_probe patchworld <zone.ff> <C1.exe> <out.ff> <size> <offset>=<hex> ..   a copy with GfxWorld header bytes changed ( experiments )

#include "codol_bsp/bsp.hpp"
#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

using namespace codol;

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		std::puts("codol_bsp_probe dump|pieces <zone.ff> <C1.exe> <asset type> [first piece] [piece count] [max bytes]\n"
			"codol_bsp_probe check <zone.ff> <C1.exe>");
		return 1;
	}
	const std::string mode = argv[1];
	zone::ClientKey key;
	if (const zone::Status s = key.load_from_exe(argv[3]); !s)
	{
		std::printf("%s\n", s.message.c_str());
		return 1;
	}
	if (mode == "check")
	{
		std::vector<std::uint8_t> bytes;
		if (const zone::Status s = zone::read_fastfile(argv[2], key, bytes); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		std::vector<std::string> problems;
		std::string summary;
		const bool ok = bsp::validate_zone(bytes, problems, &summary);
		std::printf("%s", summary.c_str());
		for (const std::string& p : problems)
		{
			std::printf("problem: %s\n", p.c_str());
		}
		std::printf("%s\n", ok ? "valid" : "INVALID");
		return ok ? 0 : 1;
	}
	if (mode == "portals")
	{
		// how the shipped portals are oriented: plane against the centres of the owning and the target cell,
		// winding order against the plane normal, the hull axes
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::GfxWorldView world;
		if (!zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world))
		{
			return 1;
		}
		int shown = 0;
		for (std::size_t c = 0; c < world.cells.size(); c++)
		{
			for (const c1::GfxPortal& p : world.cells[c].portals)
			{
				float plane[4], axis[6];
				std::memcpy(plane, p._unk0 + 12, 16);
				std::memcpy(axis, p._unk35 + 1, 24);
				const std::vector<float> v = z.get_array<float>(p.vertices, 3u * p.vertexCount);
				const auto side = [&](const c1::Bounds& b) { return plane[0] * b.midPoint[0] + plane[1] * b.midPoint[1] + plane[2] * b.midPoint[2] - plane[3]; };
				float n[3] = {};
				if (v.size() >= 9)
				{
					const float a[3] = { v[3] - v[0], v[4] - v[1], v[5] - v[2] }, b[3] = { v[6] - v[0], v[7] - v[1], v[8] - v[2] };
					n[0] = a[1] * b[2] - a[2] * b[1];
					n[1] = a[2] * b[0] - a[0] * b[2];
					n[2] = a[0] * b[1] - a[1] * b[0];
				}
				const float facing = n[0] * plane[0] + n[1] * plane[1] + n[2] * plane[2];
				if (shown++ < 14)
				{
					std::printf("cell %zu -> %u: plane %.3f %.3f %.3f | %.1f, own centre %+.1f, target centre %+.1f, vertex 0 on plane %+.3f, %u verts wound %s, axes ( %.2f %.2f %.2f ) ( %.2f %.2f %.2f )\n",
						c, p.cellIndex, plane[0], plane[1], plane[2], plane[3], side(world.cells[c].raw.bounds), side(world.cells[p.cellIndex].raw.bounds),
						v.size() >= 3 ? plane[0] * v[0] + plane[1] * v[1] + plane[2] * v[2] - plane[3] : 0.0f, p.vertexCount,
						facing > 0 ? "ccw seen from the normal's side" : "cw seen from the normal's side", axis[0], axis[1], axis[2], axis[3], axis[4], axis[5]);
				}
			}
		}
		return 0;
	}
	if (mode == "patchworld" && argc >= 6)
	{
		// codol_bsp_probe patchworld <zone.ff> <C1.exe> <out.ff> <file size, 0: any> <offset>=<hex bytes> ..
		// An experiment aid: a copy of a map zone with bytes of its GfxWorld header overwritten ( no pointer fields ), to
		// find out in the game which field a behaviour hangs on - e.g. a shipped map made like a compiled one field by
		// field until its sun shadows go. The output goes into a test client's folder, never into the install.
		zone::ZoneEditor editor;
		if (const zone::Status s = editor.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		std::size_t world = SIZE_MAX;
		for (std::size_t a = 0; a < editor.asset_count(); a++)
		{
			if (editor.asset(a).type == c1::ASSET_TYPE_GFXWORLD)
			{
				world = a;
			}
		}
		if (world == SIZE_MAX)
		{
			std::puts("no GfxWorld");
			return 1;
		}
		const zone::ZoneDataId header = editor.asset_data(world, 0);
		for (int k = 6; k < argc; k++)
		{
			const std::string arg = argv[k];
			const std::size_t eq = arg.find('=');
			if (eq == std::string::npos)
			{
				continue;
			}
			const std::uint32_t offset = static_cast<std::uint32_t>(std::strtoul(arg.substr(0, eq).c_str(), nullptr, 0));
			std::vector<std::uint8_t> bytes;
			for (std::size_t c = eq + 1; c + 1 < arg.size(); c += 2)
			{
				bytes.push_back(static_cast<std::uint8_t>(std::strtoul(arg.substr(c, 2).c_str(), nullptr, 16)));
			}
			if (const zone::Status s = editor.patch_data(header, offset, bytes); !s)
			{
				// a pointer field: make it plain bytes
				std::uint32_t plain = 0;
				std::memcpy(&plain, bytes.data(), std::min<std::size_t>(4, bytes.size()));
				if (bytes.size() != 4 || !editor.set_pointer(header, offset, zone::ZoneTarget{}, plain))
				{
					std::printf("offset %u: %s\n", offset, s.message.c_str());
					return 1;
				}
			}
			std::printf("offset %u: %zu bytes\n", offset, bytes.size());
		}
		std::vector<std::uint8_t> bytes;
		if (const zone::Status s = editor.write(bytes); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::FastFileWriteOptions options;
		options.exactFileSize = std::strtoull(argv[5], nullptr, 0);
		if (const zone::Status s = zone::write_fastfile(argv[4], bytes, key, options); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		std::printf("wrote %s\n", argv[4]);
		return 0;
	}
	if (mode == "sun")
	{
		// what the sun and its shadows hang on: the header's light and surface range fields, the sun flare block, how
		// surfaces and static models name their primary light, the shadow geometry lists, the primary lights
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::GfxWorldView world;
		if (!zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world))
		{
			return 1;
		}
		const c1::GfxWorld& h = world.raw;
		const auto hex = [](const std::uint8_t* p, const std::size_t n)
		{
			std::string out;
			char t[4];
			for (std::size_t k = 0; k < n; k++)
			{
				std::snprintf(t, sizeof(t), "%02X", p[k]);
				out += t;
				out += k % 4 == 3 ? " " : "";
			}
			return out;
		};
		std::printf("lastSunPrimaryLightIndex %u, primaryLightCount %u, skies %d, cells %d, surfaces %u, models %d\n", h.lastSunPrimaryLightIndex, h.primaryLightCount, h.skyCount,
			h.cellCount, h.surfaceCount, h.modelCount);
		std::printf("header 36..52 ( sort keys ): %s\nheader 56..76: %s\n", hex(h._unk36, 16).c_str(), hex(h._unk56, 20).c_str());
		std::printf("sun flare block: %s\n", hex(h.sun, 96).c_str());
		std::printf("header 416..456: %s\n", hex(h._unk416, 40).c_str());
		std::printf("light grid header: %s\n", hex(h.lightGrid, 56).c_str());
		std::printf("draw block: %s\n", hex(reinterpret_cast<const std::uint8_t*>(&h.draw), sizeof(h.draw)).c_str());
		{
			// the light grid's entries by primary light and the needsTrace byte
			std::uint32_t entry_count = 0, entries_at = 0;
			std::memcpy(&entry_count, h.lightGrid + 40, 4);
			std::memcpy(&entries_at, h.lightGrid + 44, 4);
			std::map<std::pair<int, int>, int> by_light;
			if (entries_at)
			{
				for (const std::uint32_t e : z.get_array<std::uint32_t>(entries_at, entry_count))
				{
					by_light[{ static_cast<int>((e >> 16) & 0xFF), static_cast<int>(e >> 24) }]++;
				}
			}
			for (const auto& [k, n] : by_light)
			{
				std::printf("%8d  light grid entries: primary light %d, needsTrace %d\n", n, k.first, k.second);
			}
		}
		std::printf("dpvs: %u static models, %u static surfaces ( %u without decals ); lit opaque %u..%u, lit transparent %u..%u, shadow casters %u..%u, emissive %u..%u; tail %s\n",
			h.dpvs.smodelCount, h.dpvs.staticSurfaceCount, h.dpvs.staticSurfaceCountNoDecal, h.dpvs.litOpaqueSurfsBegin, h.dpvs.litOpaqueSurfsEnd, h.dpvs.litTransSurfsBegin,
			h.dpvs.litTransSurfsEnd, h.dpvs.shadowCasterSurfsBegin, h.dpvs.shadowCasterSurfsEnd, h.dpvs.emissiveSurfsBegin, h.dpvs.emissiveSurfsEnd, hex(h.dpvs._unk104, 4).c_str());
		std::printf("dpvsDyn: %s\nhero only lights %u, header 624: %s\n", hex(h.dpvsDyn, 52).c_str(), h.heroOnlyLightCount, hex(h._unk624, 4).c_str());
		std::map<std::string, int> counts;
		for (std::size_t s = 0; s < world.surfaces.size(); s++)
		{
			const c1::GfxSurface& surf = world.surfaces[s].raw;
			const char* range = s < h.dpvs.litOpaqueSurfsEnd ? "lit opaque" : s < h.dpvs.litTransSurfsEnd ? "lit transparent" : s < h.dpvs.shadowCasterSurfsEnd ? "shadow caster"
				: s < h.dpvs.emissiveSurfsEnd ? "emissive" : "brush model";
			char line[160];
			std::snprintf(line, sizeof(line), "surfaces, %s: primary light %u flags 0x%02X lightmap %s", range, surf.primaryLightIndex, surf.flags, surf.lightmapIndex == 31 ? "none" : "yes");
			counts[line]++;
			if (s >= h.dpvs.litTransSurfsEnd && s < h.dpvs.shadowCasterSurfsEnd)
			{
				counts["shadow caster material " + std::string(world.surfaces[s].materialName)]++;
				if (s < h.dpvs.litTransSurfsEnd + 3)
				{
					// the first few: how large, and where
					const std::vector<c1::GfxWorldVertex> verts = world.vertices();
					float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
					for (std::uint32_t v = surf.firstVertex; v < surf.firstVertex + surf.vertexCount && v < verts.size(); v++)
					{
						for (int a = 0; a < 3; a++)
						{
							lo[a] = std::min(lo[a], verts[v].xyz[a]);
							hi[a] = std::max(hi[a], verts[v].xyz[a]);
						}
					}
					std::printf("shadow caster surface %zu: %u triangles, %u vertices, ( %.0f %.0f %.0f ) - ( %.0f %.0f %.0f ), lightmap %u probe %u\n", s, surf.triCount,
						surf.vertexCount, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], surf.lightmapIndex, surf.reflectionProbeIndex);
				}
			}
		}
		for (const zone::WorldStaticModel& m : world.staticModels)
		{
			char line[160];
			std::snprintf(line, sizeof(line), "static models: primary light %u flags 0x%02X", m.raw.primaryLightIndex, m.raw.flags);
			counts[line]++;
		}
		for (const auto& [what, count] : counts)
		{
			std::printf("%6d  %s\n", count, what.c_str());
		}
		// shadow geometry and light regions: one entry per primary light
		struct shadow_geometry
		{
			std::uint16_t surfaceCount, smodelCount;
			std::uint32_t sortedSurfIndex, smodelIndex;
		};
		struct light_region
		{
			std::uint32_t hullCount, hulls;
		};
		std::uint32_t geometry_at = 0, region_at = 0;
		std::memcpy(&geometry_at, h._unk416 + 32, 4);
		std::memcpy(&region_at, h._unk416 + 36, 4);
		if (geometry_at && region_at)
		{
			const std::vector<shadow_geometry> geometry = z.get_array<shadow_geometry>(geometry_at, h.primaryLightCount);
			const std::vector<light_region> regions = z.get_array<light_region>(region_at, h.primaryLightCount);
			for (std::uint32_t l = 0; l < h.primaryLightCount && l < 12; l++)
			{
				std::printf("light %u: shadow geometry %u surfaces, %u static models; region %u hulls\n", l, geometry[l].surfaceCount, geometry[l].smodelCount, regions[l].hullCount);
			}
		}
		// the primary lights of the ComWorld
		struct com_world
		{
			std::uint32_t name;
			std::int32_t isInUse;
			std::uint32_t count, lights;
		};
		struct com_light
		{
			std::uint8_t type, canUseShadowMap, exponent, unused;
			float color[3], dir[3], origin[3], radius, cosHalfFovOuter, cosHalfFovInner, cosHalfFovExpanded, rotationLimit, translationLimit;
			std::uint32_t defName;
		};
		if (const zone::zaddr at = zone::first_asset(z, c1::ASSET_TYPE_COMWORLD))
		{
			const com_world com = z.get<com_world>(at);
			const std::vector<com_light> lights = z.get_array<com_light>(com.lights, com.count);
			std::printf("ComWorld: isInUse %d, %u lights\n", com.isInUse, com.count);
			for (std::size_t l = 0; l < lights.size() && l < 12; l++)
			{
				const com_light& c = lights[l];
				std::printf("  light %zu: type %u canUseShadowMap %u exponent %u unused %u colour ( %.3f %.3f %.3f ) dir ( %.3f %.3f %.3f ) origin ( %.0f %.0f %.0f ) radius %.0f cos %.3f %.3f %.3f limits %.2f %.2f def %s\n",
					l, c.type, c.canUseShadowMap, c.exponent, c.unused, c.color[0], c.color[1], c.color[2], c.dir[0], c.dir[1], c.dir[2], c.origin[0], c.origin[1], c.origin[2], c.radius,
					c.cosHalfFovOuter, c.cosHalfFovInner, c.cosHalfFovExpanded, c.rotationLimit, c.translationLimit, c.defName ? std::string(z.c_string(c.defName)).c_str() : "-");
			}
		}
		return 0;
	}
	if (mode == "lights")
	{
		// codol_bsp_probe lights <zone.ff> <C1.exe> [out dir]: the primary lights of a map with what hangs on each -
		// the ComWorld record, the shadow geometry, the light region's hulls, the surfaces that name it and how bright
		// their lightmap texels are -, and the brightness of the lightmap pages ( written as .tga with an out dir )
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::GfxWorldView world;
		if (!zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world))
		{
			return 1;
		}
		const c1::GfxWorld& h = world.raw;
		const std::string out_dir = argc > 4 ? argv[4] : "";
		const auto save = [&](const std::string& name, const int w, const int hh, const std::uint8_t* px, const int channels)
		{
			if (out_dir.empty())
			{
				return;
			}
			if (std::FILE* f = std::fopen((out_dir + "/" + name).c_str(), "wb"))
			{
				std::uint8_t head[18] = {};
				head[2] = 2;
				head[12] = static_cast<std::uint8_t>(w);
				head[13] = static_cast<std::uint8_t>(w >> 8);
				head[14] = static_cast<std::uint8_t>(hh);
				head[15] = static_cast<std::uint8_t>(hh >> 8);
				head[16] = 24;
				head[17] = 0x20;
				std::fwrite(head, 1, 18, f);
				for (int k = 0; k < w * hh; k++)
				{
					const std::uint8_t bgr[3] = { px[k * channels], px[k * channels + (channels > 1 ? 1 : 0)], px[k * channels + (channels > 1 ? 2 : 0)] };
					std::fwrite(bgr, 1, 3, f);
				}
				std::fclose(f);
			}
		};
		struct page
		{
			zone::ImageView primary, secondary;
		};
		std::vector<page> pages;
		if (h.draw.lightmapCount > 0)
		{
			const auto pointers = z.get_array<zone::zaddr>(h.draw.lightmaps, 2u * static_cast<std::size_t>(h.draw.lightmapCount));
			for (std::size_t i = 0; i + 1 < pointers.size(); i += 2)
			{
				page p;
				zone::read_image(z, pointers[i], p.primary);
				zone::read_image(z, pointers[i + 1], p.secondary);
				const int sw = p.secondary.raw.width, sh = p.secondary.raw.height, pw = p.primary.raw.width, ph = p.primary.raw.height;
				std::printf("lightmap %zu: primary %s %d x %d format %u ( %zu bytes ), secondary %s %d x %d format %u ( %zu bytes )\n", i / 2, std::string(p.primary.name).c_str(), pw, ph,
					p.primary.loadDef.format, p.primary.pixels.size(), std::string(p.secondary.name).c_str(), sw, sh, p.secondary.loadDef.format, p.secondary.pixels.size());
				if (p.secondary.pixels.size() >= static_cast<std::size_t>(sw) * sh * 4)
				{
					for (int layer = 0; layer < 2; layer++)
					{
						double sum[4] = {};
						std::size_t lit = 0;
						std::vector<int> lum;
						const std::uint8_t* px = p.secondary.pixels.data() + static_cast<std::size_t>(layer) * sw * (sh / 2) * 4;
						for (int k = 0; k < sw * (sh / 2); k++)
						{
							if (px[k * 4] || px[k * 4 + 1] || px[k * 4 + 2])
							{
								lit++;
								for (int c = 0; c < 4; c++)
								{
									sum[c] += px[k * 4 + c];
								}
								lum.push_back((px[k * 4] + px[k * 4 + 1] + px[k * 4 + 2]) / 3);
							}
						}
						std::sort(lum.begin(), lum.end());
						const auto pct = [&](const double q) { return lum.empty() ? 0 : lum[static_cast<std::size_t>(q * static_cast<double>(lum.size() - 1))]; };
						std::printf("  secondary %s layer: %zu of %d texels not black; mean b g r a %.1f %.1f %.1f %.1f; brightness 10%% %d, 50%% %d, 90%% %d, 99%% %d\n", layer ? "lower" : "upper",
							lit, sw * (sh / 2), sum[0] / std::max<std::size_t>(lit, 1), sum[1] / std::max<std::size_t>(lit, 1), sum[2] / std::max<std::size_t>(lit, 1),
							sum[3] / std::max<std::size_t>(lit, 1), pct(0.1), pct(0.5), pct(0.9), pct(0.99));
						save("lm" + std::to_string(i / 2) + (layer ? "_lower.tga" : "_upper.tga"), sw, sh / 2, px, 4);
					}
				}
				if (p.primary.pixels.size() >= static_cast<std::size_t>(pw) * ph)
				{
					std::size_t hist[4] = {};
					for (int k = 0; k < pw * ph; k++)
					{
						const std::uint8_t v = p.primary.pixels[static_cast<std::size_t>(k)];
						hist[v == 0 ? 0 : v < 128 ? 1 : v < 255 ? 2 : 3]++;
					}
					std::printf("  primary: %zu texels 0, %zu below 128, %zu below 255, %zu 255\n", hist[0], hist[1], hist[2], hist[3]);
					save("lm" + std::to_string(i / 2) + "_primary.tga", pw, ph, p.primary.pixels.data(), 1);
				}
				pages.push_back(p);
			}
		}
		// per primary light: the surfaces that name it, and their lightmap texels at the vertices
		struct per_light
		{
			int surfaces = 0, vertices = 0;
			double upper[3] = {}, lower[3] = {}, primary = 0;
			float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
		};
		std::map<int, per_light> by_light;
		const std::vector<c1::GfxWorldVertex> verts = world.vertices();
		const std::vector<std::uint16_t> all_indices = world.indices();
		for (const zone::WorldSurface& ws : world.surfaces)
		{
			const c1::GfxSurface& surf = ws.raw;
			per_light& pl = by_light[surf.primaryLightIndex];
			pl.surfaces++;
			if (std::getenv("PROBE_SURF") && surf.primaryLightIndex == std::atoi(std::getenv("PROBE_SURF")))
			{
				std::printf("surface %zu: first vertex %u count %u tris %u lightmap %u material %s\n", static_cast<std::size_t>(&ws - world.surfaces.data()), surf.firstVertex, surf.vertexCount,
					surf.triCount, surf.lightmapIndex, std::string(ws.materialName).c_str());
			}
			if (surf.lightmapIndex >= pages.size())
			{
				continue;
			}
			const page& p = pages[surf.lightmapIndex];
			const int sw = p.secondary.raw.width, sh = p.secondary.raw.height / 2, pw = p.primary.raw.width, ph = p.primary.raw.height;
			if (p.secondary.pixels.size() < static_cast<std::size_t>(sw) * sh * 8 || p.primary.pixels.size() < static_cast<std::size_t>(pw) * ph)
			{
				continue;
			}
			// a surface's vertices are those its triangles name ( shipped surfaces share vertex blocks )
			for (std::uint32_t i = surf.baseIndex; i < surf.baseIndex + 3u * surf.triCount && i < all_indices.size(); i++)
			{
				const std::uint32_t v = surf.firstVertex + all_indices[i];
				if (v >= verts.size())
				{
					continue;
				}
				const float u = verts[v].lmapCoord[0], t = verts[v].lmapCoord[1];
				const int x = std::clamp(static_cast<int>(u * static_cast<float>(sw)), 0, sw - 1), y = std::clamp(static_cast<int>(t * static_cast<float>(sh)), 0, sh - 1);
				const std::uint8_t* up = p.secondary.pixels.data() + (static_cast<std::size_t>(y) * sw + x) * 4;
				const std::uint8_t* low = up + static_cast<std::size_t>(sw) * sh * 4;
				for (int c = 0; c < 3; c++)
				{
					pl.upper[c] += up[2 - c];
					pl.lower[c] += low[2 - c];
					pl.lo[c] = std::min(pl.lo[c], verts[v].xyz[c]);
					pl.hi[c] = std::max(pl.hi[c], verts[v].xyz[c]);
				}
				pl.primary += p.primary.pixels[static_cast<std::size_t>(std::clamp(static_cast<int>(t * static_cast<float>(ph)), 0, ph - 1)) * pw
					+ std::clamp(static_cast<int>(u * static_cast<float>(pw)), 0, pw - 1)];
				pl.vertices++;
			}
		}
		struct shadow_geometry
		{
			std::uint16_t surfaceCount, smodelCount;
			std::uint32_t sortedSurfIndex, smodelIndex;
		};
		struct light_region
		{
			std::uint32_t hullCount, hulls;
		};
		struct region_hull
		{
			float mid[9], half[9];
			std::uint32_t axisCount, axis;
		};
		struct region_axis
		{
			float dir[3], mid, half;
		};
		struct com_world
		{
			std::uint32_t name;
			std::int32_t isInUse;
			std::uint32_t count, lights;
		};
		struct com_light
		{
			std::uint8_t type, canUseShadowMap, exponent, unused;
			float color[3], dir[3], origin[3], radius, cosHalfFovOuter, cosHalfFovInner, cosHalfFovExpanded, rotationLimit, translationLimit;
			std::uint32_t defName;
		};
		std::uint32_t pointers[10];
		std::memcpy(pointers, h._unk416, sizeof(pointers));
		std::printf("header pointers 416: cellCasterBits %X cellHasSunLitSurfsBits %X sceneDynModel %X sceneDynBrush %X primaryLightEntityShadowVis %X dynEntShadowVis %X %X nonSunPrimaryLightForModelDynEnt %X shadowGeom %X lightRegion %X\n",
			pointers[0], pointers[1], pointers[2], pointers[3], pointers[4], pointers[5], pointers[6], pointers[7], pointers[8], pointers[9]);
		std::printf("lastSunPrimaryLightIndex %u primaryLightCount %u; dpvsDyn:", h.lastSunPrimaryLightIndex, h.primaryLightCount);
		for (int k = 0; k < 52; k += 4)
		{
			std::uint32_t v;
			std::memcpy(&v, h.dpvsDyn + k, 4);
			std::printf(" %X", v);
		}
		std::printf("\n");
		std::vector<shadow_geometry> geometry;
		std::vector<light_region> regions;
		if (pointers[8] && pointers[9])
		{
			geometry = z.get_array<shadow_geometry>(pointers[8], h.primaryLightCount);
			regions = z.get_array<light_region>(pointers[9], h.primaryLightCount);
		}
		std::vector<com_light> lights;
		if (const zone::zaddr at = zone::first_asset(z, c1::ASSET_TYPE_COMWORLD))
		{
			const com_world com = z.get<com_world>(at);
			lights = z.get_array<com_light>(com.lights, com.count);
		}
		std::map<int, int> models_of;
		for (const zone::WorldStaticModel& m : world.staticModels)
		{
			models_of[m.raw.primaryLightIndex]++;
		}
		const int detail = argc > 5 ? std::atoi(argv[5]) : 8;
		for (std::size_t l = 0; l < lights.size(); l++)
		{
			const com_light& c = lights[l];
			std::printf("light %zu: type %u canUseShadowMap %u exponent %u colour ( %.3f %.3f %.3f ) dir ( %.3f %.3f %.3f ) origin ( %.1f %.1f %.1f ) radius %.0f cos outer %.3f inner %.3f expanded %.3f limits %.2f %.2f def %s\n",
				l, c.type, c.canUseShadowMap, c.exponent, c.color[0], c.color[1], c.color[2], c.dir[0], c.dir[1], c.dir[2], c.origin[0], c.origin[1], c.origin[2], c.radius,
				c.cosHalfFovOuter, c.cosHalfFovInner, c.cosHalfFovExpanded, c.rotationLimit, c.translationLimit, c.defName ? std::string(z.c_string(c.defName)).c_str() : "-");
			if (const auto it = by_light.find(static_cast<int>(l)); it != by_light.end() && it->second.vertices)
			{
				const per_light& pl = it->second;
				const double n = pl.vertices;
				std::printf("   %d surfaces name it ( %d static models ), box ( %.0f %.0f %.0f ) - ( %.0f %.0f %.0f ); at their vertices: upper r g b %.0f %.0f %.0f, lower %.0f %.0f %.0f, primary %.0f\n",
					pl.surfaces, models_of[static_cast<int>(l)], pl.lo[0], pl.lo[1], pl.lo[2], pl.hi[0], pl.hi[1], pl.hi[2], pl.upper[0] / n, pl.upper[1] / n, pl.upper[2] / n,
					pl.lower[0] / n, pl.lower[1] / n, pl.lower[2] / n, pl.primary / n);
			}
			if (l < geometry.size())
			{
				const shadow_geometry& g = geometry[l];
				std::printf("   shadow geometry: %u surfaces, %u static models", g.surfaceCount, g.smodelCount);
				if (g.surfaceCount && g.sortedSurfIndex && static_cast<int>(l) < detail)
				{
					// how many of them name this light themselves, and which ranges they are in
					const std::vector<std::uint16_t> list = z.get_array<std::uint16_t>(g.sortedSurfIndex, g.surfaceCount);
					int own = 0, other = 0, none = 0, sorted = 1, opaque = 0;
					float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
					for (std::size_t k = 0; k < list.size(); k++)
					{
						sorted = sorted && (k == 0 || list[k] > list[k - 1]);
						if (list[k] >= world.surfaces.size())
						{
							continue;
						}
						const c1::GfxSurface& surf = world.surfaces[list[k]].raw;
						(surf.primaryLightIndex == l ? own : surf.primaryLightIndex == 0 ? none : other)++;
						opaque += list[k] < h.dpvs.litOpaqueSurfsEnd;
						for (std::uint32_t i = surf.baseIndex; i < surf.baseIndex + 3u * surf.triCount && i < all_indices.size(); i++)
						{
							const std::uint32_t v = surf.firstVertex + all_indices[i];
							for (int a = 0; a < 3 && v < verts.size(); a++)
							{
								lo[a] = std::min(lo[a], verts[v].xyz[a]);
								hi[a] = std::max(hi[a], verts[v].xyz[a]);
							}
						}
					}
					std::printf(" ( surfaces: %d name this light, %d none, %d another; %d lit opaque; ascending %d; box ( %.0f %.0f %.0f ) - ( %.0f %.0f %.0f ) )", own, none, other, opaque,
						sorted, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
				}
				if (g.smodelCount && g.smodelIndex && static_cast<int>(l) < detail)
				{
					const std::vector<std::uint16_t> list = z.get_array<std::uint16_t>(g.smodelIndex, g.smodelCount);
					int own = 0;
					for (const std::uint16_t m : list)
					{
						own += m < world.staticModels.size() && world.staticModels[m].raw.primaryLightIndex == l;
					}
					std::printf(" ( static models: %d of them name this light )", own);
				}
				std::printf("\n");
			}
			if (l < regions.size() && regions[l].hullCount)
			{
				const std::vector<region_hull> hulls = z.get_array<region_hull>(regions[l].hulls, regions[l].hullCount);
				for (std::size_t k = 0; k < hulls.size() && static_cast<int>(l) < detail; k++)
				{
					const region_hull& rh = hulls[k];
					std::printf("   hull %zu: kdop mid", k);
					for (int a = 0; a < 9; a++)
					{
						std::printf(" %.1f", rh.mid[a]);
					}
					std::printf(" half");
					for (int a = 0; a < 9; a++)
					{
						std::printf(" %.1f", rh.half[a]);
					}
					std::printf("; %u axes\n", rh.axisCount);
					if (rh.axisCount && rh.axis)
					{
						for (const region_axis& ax : z.get_array<region_axis>(rh.axis, rh.axisCount))
						{
							std::printf("      axis ( %.3f %.3f %.3f ) mid %.1f half %.1f\n", ax.dir[0], ax.dir[1], ax.dir[2], ax.mid, ax.half);
						}
					}
				}
				if (static_cast<int>(l) >= detail)
				{
					std::printf("   region: %u hulls\n", regions[l].hullCount);
				}
			}
		}
		// the light definitions: the falloff picture, and where its copy lies in row 0 of the secondary lightmaps
		for (const zone::AssetRecord& record : z.all_assets())
		{
			if (record.type != c1::ASSET_TYPE_LIGHT_DEF)
			{
				continue;
			}
			const std::string_view def_name = z.c_string(z.get<zone::zaddr>(record.header));
			zone::ImageView image;
			zone::read_image(z, z.get<zone::zaddr>(record.header + 4), image);
			const std::int32_t start = z.get<std::int32_t>(record.header + 12);
			std::printf("light def %s: attenuation image %s %d x %d format %u ( %zu bytes inline ), sampler state 0x%02X, lmapLookupStart %d\n", std::string(def_name).c_str(),
				std::string(image.name).c_str(), image.raw.width, image.raw.height, image.loadDef.format, image.pixels.size(), z.get<std::uint8_t>(record.header + 8), start);
			if (!image.pixels.empty())
			{
				const std::size_t bpp = image.pixels.size() / std::max<std::size_t>(1, static_cast<std::size_t>(image.raw.width) * image.raw.height);
				std::printf("   image row 0 ( first byte of each texel, %zu bytes per texel ):", bpp);
				for (int x = 0; x < image.raw.width; x++)
				{
					std::printf(" %u", image.pixels[static_cast<std::size_t>(x) * std::max<std::size_t>(bpp, 1)]);
				}
				std::printf("\n");
			}
			for (std::size_t p = 0; p < pages.size() && p < 2; p++)
			{
				const int sw = pages[p].secondary.raw.width;
				if (pages[p].secondary.pixels.size() < static_cast<std::size_t>(sw) * 8)
				{
					continue;
				}
				for (int row = 0; row < 2; row++)
				{
					std::printf("   lightmap %zu row %d from texel %d * width / 512 ( b g r a ):", p, row, start);
					for (int x = start * sw / 512; x < (start + std::max<int>(image.raw.width, 32) + 2) * sw / 512 && x < sw; x++)
					{
						const std::uint8_t* t = pages[p].secondary.pixels.data() + (static_cast<std::size_t>(row) * sw + static_cast<std::size_t>(x)) * 4;
						std::printf(" %u.%u.%u.%u", t[0], t[1], t[2], t[3]);
					}
					std::printf("\n");
				}
			}
		}
		// the light grid's entries by primary light
		{
			std::uint32_t entry_count = 0, entries_at = 0;
			std::memcpy(&entry_count, h.lightGrid + 40, 4);
			std::memcpy(&entries_at, h.lightGrid + 44, 4);
			std::map<int, int> grid_lights;
			if (entries_at)
			{
				for (const std::uint32_t e : z.get_array<std::uint32_t>(entries_at, entry_count))
				{
					grid_lights[static_cast<int>((e >> 16) & 0xFF)]++;
				}
			}
			std::printf("light grid entries by primary light ( byte 2 ):");
			for (const auto& [k, n] : grid_lights)
			{
				std::printf(" %d: %d,", k, n);
			}
			std::printf("\n");
		}
		return 0;
	}
	if (mode == "ents")
	{
		// codol_bsp_probe ents <zone.ff> <C1.exe> [text]: the entities of a map whose text holds `text` ( all without )
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::MapEntsView ents;
		if (!zone::read_map_ents(z, zone::find_map_ents(z), ents))
		{
			std::printf("no map entities\n");
			return 1;
		}
		const std::string text(ents.entityString);
		const std::string want = argc > 4 ? argv[4] : "";
		std::size_t total = 0, shown = 0;
		for (std::size_t at = text.find('{'); at != std::string::npos; at = text.find('{', at))
		{
			const std::size_t end = text.find('}', at);
			if (end == std::string::npos)
			{
				break;
			}
			const std::string entity = text.substr(at, end - at + 1);
			total++;
			if (want.empty() || entity.find(want) != std::string::npos)
			{
				shown++;
				std::printf("%s\n", entity.c_str());
			}
			at = end + 1;
		}
		std::printf("%zu of %zu entities\n", shown, total);
		return 0;
	}
	if (mode == "lightcuts")
	{
		// codol_bsp_probe lightcuts <zone.ff> <C1.exe> [edges to print]: how the surfaces of a map are cut by its primary
		// light entities. Per light entity: how far the triangles of the surfaces that name it lie from the light ( in
		// radii, and against the cone ) - and the seams: edges two coplanar triangles of one material share whose
		// surfaces name different primary lights.
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::GfxWorldView world;
		if (!zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world))
		{
			return 1;
		}
		struct com_world
		{
			std::uint32_t name;
			std::int32_t isInUse;
			std::uint32_t count, lights;
		};
		struct com_light
		{
			std::uint8_t type, canUseShadowMap, exponent, unused;
			float color[3], dir[3], origin[3], radius, cosHalfFovOuter, cosHalfFovInner, cosHalfFovExpanded, rotationLimit, translationLimit;
			std::uint32_t defName;
		};
		std::vector<com_light> lights;
		if (const zone::zaddr at = zone::first_asset(z, c1::ASSET_TYPE_COMWORLD))
		{
			const com_world com = z.get<com_world>(at);
			lights = z.get_array<com_light>(com.lights, com.count);
		}
		const std::vector<c1::GfxWorldVertex> verts = world.vertices();
		const std::vector<std::uint16_t> all_indices = world.indices();
		struct v3
		{
			double x, y, z;
		};
		const auto sub = [](const v3& a, const v3& b) { return v3{ a.x - b.x, a.y - b.y, a.z - b.z }; };
		const auto dot = [](const v3& a, const v3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
		const auto cross = [](const v3& a, const v3& b) { return v3{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
		// the point of a triangle nearest to p ( Ericson )
		const auto nearest = [&](const v3& p, const v3& a, const v3& b, const v3& c)
		{
			const v3 ab = sub(b, a), ac = sub(c, a), ap = sub(p, a);
			const double d1 = dot(ab, ap), d2 = dot(ac, ap);
			if (d1 <= 0 && d2 <= 0) return a;
			const v3 bp = sub(p, b);
			const double d3 = dot(ab, bp), d4 = dot(ac, bp);
			if (d3 >= 0 && d4 <= d3) return b;
			const double vc = d1 * d4 - d3 * d2;
			if (vc <= 0 && d1 >= 0 && d3 <= 0)
			{
				const double t = d1 / (d1 - d3);
				return v3{ a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t };
			}
			const v3 cp = sub(p, c);
			const double d5 = dot(ab, cp), d6 = dot(ac, cp);
			if (d6 >= 0 && d5 <= d6) return c;
			const double vb = d5 * d2 - d1 * d6;
			if (vb <= 0 && d2 >= 0 && d6 <= 0)
			{
				const double t = d2 / (d2 - d6);
				return v3{ a.x + ac.x * t, a.y + ac.y * t, a.z + ac.z * t };
			}
			const double va = d3 * d6 - d5 * d4;
			if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
			{
				const double t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
				return v3{ b.x + (c.x - b.x) * t, b.y + (c.y - b.y) * t, b.z + (c.z - b.z) * t };
			}
			const double den = 1.0 / (va + vb + vc), v = vb * den, w = vc * den;
			return v3{ a.x + ab.x * v + ac.x * w, a.y + ab.y * v + ac.y * w, a.z + ab.z * v + ac.z * w };
		};
		struct tri
		{
			v3 p[3];
			std::uint32_t surface;
			int light;
			v3 n;
			double w, area;
		};
		std::vector<tri> tris;
		for (std::size_t si = 0; si < world.surfaces.size(); si++)
		{
			const c1::GfxSurface& surf = world.surfaces[si].raw;
			for (std::uint32_t t = 0; t < surf.triCount; t++)
			{
				tri out{};
				bool ok = true;
				for (int k = 0; k < 3; k++)
				{
					const std::size_t i = surf.baseIndex + 3u * t + static_cast<std::uint32_t>(k);
					const std::uint32_t v = i < all_indices.size() ? surf.firstVertex + all_indices[i] : 0xFFFFFFFFu;
					if (v >= verts.size())
					{
						ok = false;
						break;
					}
					out.p[k] = v3{ verts[v].xyz[0], verts[v].xyz[1], verts[v].xyz[2] };
				}
				if (!ok)
				{
					continue;
				}
				out.surface = static_cast<std::uint32_t>(si);
				out.light = surf.primaryLightIndex;
				v3 n = cross(sub(out.p[1], out.p[0]), sub(out.p[2], out.p[0]));
				const double len = std::sqrt(dot(n, n));
				if (len < 1e-6)
				{
					continue;
				}
				out.area = len * 0.5;
				out.n = v3{ n.x / len, n.y / len, n.z / len };
				out.w = dot(out.n, out.p[0]);
				tris.push_back(out);
			}
		}
		std::printf("%zu surfaces, %zu triangles, %zu primary lights\n", world.surfaces.size(), tris.size(), lights.size());
		// per light entity: where its triangles lie
		for (std::size_t l = 2; l < lights.size(); l++)
		{
			const com_light& c = lights[l];
			const v3 o{ c.origin[0], c.origin[1], c.origin[2] };
			const v3 beam{ -c.dir[0], -c.dir[1], -c.dir[2] };
			std::size_t n = 0, beyond = 0, off_cone = 0, facing_away = 0;
			double worst = 0, area = 0, far_area = 0;
			std::size_t histogram[6] = {};
			for (const tri& t : tris)
			{
				if (t.light != static_cast<int>(l))
				{
					continue;
				}
				n++;
				area += t.area;
				const v3 q = nearest(o, t.p[0], t.p[1], t.p[2]);
				const double d = std::sqrt(dot(sub(q, o), sub(q, o))) / std::max(1.0f, c.radius);
				worst = std::max(worst, d);
				histogram[d < 0.25 ? 0 : d < 0.5 ? 1 : d < 0.75 ? 2 : d < 1.0 ? 3 : d < 1.25 ? 4 : 5]++;
				if (d > 1.0)
				{
					beyond++;
					far_area += t.area;
				}
				// against the cone: the corner with the smallest angle to the beam
				double best = -1;
				for (int k = 0; k < 3; k++)
				{
					const v3 to = sub(t.p[k], o);
					const double len = std::sqrt(dot(to, to));
					best = std::max(best, len > 1e-3 ? dot(to, beam) / len : 1.0);
				}
				off_cone += c.type == 2 && best < c.cosHalfFovOuter - 0.02;
				facing_away += dot(t.n, o) - t.w < -0.5 && dot(t.n, o) - t.w > -1e9 ? 0 : 0;
			}
			std::printf("light %zu ( type %u radius %.0f cos outer %.3f ): %zu triangles, area %.0f; nearest point in radii: <0.25 %zu, <0.5 %zu, <0.75 %zu, <1 %zu, <1.25 %zu, more %zu ( worst %.2f, area beyond the radius %.0f ); all corners outside the cone: %zu\n",
				l, c.type, c.radius, c.cosHalfFovOuter, n, area, histogram[0], histogram[1], histogram[2], histogram[3], histogram[4], histogram[5], worst, far_area, off_cone);
		}
		// seams: an edge shared by two coplanar triangles of one material whose surfaces name different lights
		struct edge_key
		{
			long long a[3], b[3];
			bool operator<(const edge_key& o) const { return std::memcmp(this, &o, sizeof(*this)) < 0; }
		};
		const auto quant = [](const v3& p, long long out[3])
		{
			out[0] = std::llround(p.x * 8);
			out[1] = std::llround(p.y * 8);
			out[2] = std::llround(p.z * 8);
		};
		std::map<edge_key, std::vector<std::uint32_t>> edges;
		for (std::size_t ti = 0; ti < tris.size(); ti++)
		{
			for (int k = 0; k < 3; k++)
			{
				edge_key e;
				quant(tris[ti].p[k], e.a);
				quant(tris[ti].p[(k + 1) % 3], e.b);
				if (std::memcmp(e.a, e.b, sizeof(e.a)) > 0)
				{
					std::swap(e.a, e.b);
				}
				edges[e].push_back(static_cast<std::uint32_t>(ti));
			}
		}
		std::map<std::pair<int, int>, int> seam_pairs;
		std::map<std::pair<int, int>, int> same_surface_pairs;
		int printed = 0;
		const int to_print = argc > 4 ? std::atoi(argv[4]) : 30;
		std::size_t shared = 0, coplanar_same_material = 0, different_surface = 0, different_light = 0;
		for (const auto& [e, list] : edges)
		{
			for (std::size_t i = 0; i < list.size(); i++)
			{
				for (std::size_t j = i + 1; j < list.size(); j++)
				{
					const tri& a = tris[list[i]];
					const tri& b = tris[list[j]];
					shared++;
					if (dot(a.n, b.n) < 0.999 || std::fabs(a.w - b.w) > 0.5 ||
						world.surfaces[a.surface].materialName != world.surfaces[b.surface].materialName)
					{
						continue;
					}
					coplanar_same_material++;
					if (a.surface == b.surface)
					{
						continue;
					}
					different_surface++;
					if (a.light == b.light)
					{
						same_surface_pairs[{ a.light, a.light }]++;
						continue;
					}
					different_light++;
					seam_pairs[{ std::min(a.light, b.light), std::max(a.light, b.light) }]++;
					if (printed < to_print && (a.light >= 2 || b.light >= 2))
					{
						printed++;
						const v3 p{ static_cast<double>(e.a[0]) / 8, static_cast<double>(e.a[1]) / 8, static_cast<double>(e.a[2]) / 8 };
						const v3 q{ static_cast<double>(e.b[0]) / 8, static_cast<double>(e.b[1]) / 8, static_cast<double>(e.b[2]) / 8 };
						std::printf("  seam ( %.1f %.1f %.1f ) - ( %.1f %.1f %.1f ) normal ( %.2f %.2f %.2f ) %s: surfaces %u ( light %d ) / %u ( light %d )", p.x, p.y, p.z, q.x, q.y, q.z, a.n.x,
							a.n.y, a.n.z, std::string(world.surfaces[a.surface].materialName).c_str(), a.surface, a.light, b.surface, b.light);
						for (const int l : { a.light, b.light })
						{
							if (l >= 2 && static_cast<std::size_t>(l) < lights.size())
							{
								const com_light& c = lights[static_cast<std::size_t>(l)];
								const v3 o{ c.origin[0], c.origin[1], c.origin[2] };
								const v3 dp = sub(p, o), dq = sub(q, o);
								std::printf("; light %d: ends at %.2f / %.2f radii", l, std::sqrt(dot(dp, dp)) / c.radius, std::sqrt(dot(dq, dq)) / c.radius);
							}
						}
						std::printf("\n");
					}
				}
			}
		}
		std::printf("%zu shared edges; %zu between coplanar triangles of one material; %zu of those between two surfaces; %zu of those between surfaces with different primary lights\n",
			shared, coplanar_same_material, different_surface, different_light);
		for (const auto& [pair, n] : seam_pairs)
		{
			std::printf("  lights %d / %d: %d seam edges\n", pair.first, pair.second, n);
		}
		return 0;
	}
	if (mode == "cells")
	{
		// every cell with its box, and every portal with its target and the box of its corners
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::GfxWorldView world;
		if (!zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world))
		{
			return 1;
		}
		for (std::size_t c = 0; c < world.cells.size(); c++)
		{
			const c1::Bounds& b = world.cells[c].raw.bounds;
			std::printf("cell %zu: ( %.0f %.0f %.0f ) - ( %.0f %.0f %.0f ), %zu portals\n", c, b.midPoint[0] - b.halfSize[0], b.midPoint[1] - b.halfSize[1],
				b.midPoint[2] - b.halfSize[2], b.midPoint[0] + b.halfSize[0], b.midPoint[1] + b.halfSize[1], b.midPoint[2] + b.halfSize[2], world.cells[c].portals.size());
			for (const c1::GfxPortal& p : world.cells[c].portals)
			{
				float plane[4];
				std::memcpy(plane, p._unk0 + 12, 16);
				const std::vector<float> v = z.get_array<float>(p.vertices, 3u * p.vertexCount);
				float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
				for (std::size_t k = 0; k < v.size(); k++)
				{
					lo[k % 3] = std::min(lo[k % 3], v[k]);
					hi[k % 3] = std::max(hi[k % 3], v[k]);
				}
				std::printf("  -> %u: normal %.0f %.0f %.0f, ( %.0f %.0f %.0f ) - ( %.0f %.0f %.0f ), %u corners\n", p.cellIndex, plane[0], plane[1], plane[2], lo[0], lo[1], lo[2], hi[0],
					hi[1], hi[2], p.vertexCount);
			}
		}
		return 0;
	}
	if (mode == "terrain")
	{
		// The terrain ( triangle ) collision of a clip map: how the game's compiler shapes partitions, borders, the
		// AABB trees and the leafs that name them.
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		const zone::zaddr at = zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP);
		if (!at)
		{
			return 1;
		}
		const c1::clipMap_t cm = z.get<c1::clipMap_t>(at);
		struct partition { std::uint8_t tris, borders, block, pad; std::uint32_t first, border; };
		struct aabb { float mid[3]; std::uint16_t material, children; float half[3]; std::uint32_t index; };
		struct border { float eq[3], zBase, zSlope, start, length; };
		struct material { zone::zaddr name; std::int32_t flags, contents; };
		const std::vector<float> verts = z.get_array<float>(cm.verts, 3u * cm.vertCount);
		const std::vector<std::uint16_t> tris = z.get_array<std::uint16_t>(cm.triIndices, 3u * static_cast<std::size_t>(cm.triCount));
		const std::vector<std::uint32_t> walk = z.get_array<std::uint32_t>(cm.triEdgeIsWalkable, (static_cast<std::size_t>(cm.triCount) * 3 + 31) / 32);
		const std::vector<partition> parts = z.get_array<partition>(cm.partitions, static_cast<std::size_t>(cm.partitionCount));
		const std::vector<aabb> trees = z.get_array<aabb>(cm.aabbTrees, static_cast<std::size_t>(cm.aabbTreeCount));
		const std::vector<border> borders = z.get_array<border>(cm.borders, static_cast<std::size_t>(cm.borderCount));
		const std::vector<c1::cLeaf_t> leafs = z.get_array<c1::cLeaf_t>(cm.leafs, cm.numLeafs);
		const std::vector<material> materials = z.get_array<material>(cm.materials, cm.numMaterials);
		std::printf("%u verts, %d tris, %d borders, %d partitions, %d aabb nodes, %u leafs, %u materials\n", cm.vertCount, cm.triCount, cm.borderCount, cm.partitionCount,
			cm.aabbTreeCount, cm.numLeafs, cm.numMaterials);
		// partitions
		int tri_hist[18] = {}, border_hist[18] = {}, max_block = 0, max_index = 0;
		std::size_t tri_sum = 0, border_at = 0;
		bool tris_in_order = true, borders_in_order = true;
		for (const partition& p : parts)
		{
			tri_hist[std::min<int>(p.tris, 17)]++;
			border_hist[std::min<int>(p.borders, 17)]++;
			max_block = std::max<int>(max_block, p.block);
			tris_in_order = tris_in_order && p.first == tri_sum;
			tri_sum += p.tris;
			if (p.borders)
			{
				borders_in_order = borders_in_order && p.border == cm.borders + 28 * border_at;
				border_at += p.borders;
			}
			for (std::uint32_t t = 0; t < 3u * p.tris; t++)
			{
				max_index = std::max<int>(max_index, tris[3 * p.first + t]);
			}
		}
		std::printf("partitions: triangles in running order %d ( sum %zu ), borders in running order %d, highest vertex block %d, highest index inside a block %d\n  tris per partition:",
			tris_in_order, tri_sum, borders_in_order, max_block, max_index);
		for (int k = 0; k < 18; k++)
		{
			std::printf(" %d", tri_hist[k]);
		}
		std::printf("\n  borders per partition:");
		for (int k = 0; k < 18; k++)
		{
			std::printf(" %d", border_hist[k]);
		}
		// the first partitions with their triangles and borders
		const int show = argc > 4 ? std::atoi(argv[4]) : 3;
		for (int k = 0; k < show && k < static_cast<int>(parts.size()); k++)
		{
			const partition& p = parts[static_cast<std::size_t>(k)];
			std::printf("\npartition %d: %u tris from %u, %u borders, block %u\n", k, p.tris, p.first, p.borders, p.block);
			for (std::uint32_t t = 0; t < p.tris; t++)
			{
				std::printf("  tri");
				for (int c = 0; c < 3; c++)
				{
					const std::size_t v = static_cast<std::size_t>(p.block) * 1024 + tris[3 * (p.first + t) + static_cast<std::uint32_t>(c)];
					std::printf(" %u( %.1f %.1f %.1f )", tris[3 * (p.first + t) + static_cast<std::uint32_t>(c)], verts[3 * v], verts[3 * v + 1], verts[3 * v + 2]);
				}
				const std::uint32_t bit = 3 * (p.first + t);
				std::printf(" walkable %u%u%u\n", (walk[bit >> 5] >> (bit & 31)) & 1, (walk[(bit + 1) >> 5] >> ((bit + 1) & 31)) & 1, (walk[(bit + 2) >> 5] >> ((bit + 2) & 31)) & 1);
			}
			const std::size_t first_border = p.borders ? (p.border - cm.borders) / 28 : 0;
			for (std::uint32_t b = 0; b < p.borders; b++)
			{
				const border& e = borders[first_border + b];
				std::printf("  border eq %.4f %.4f %.3f zBase %.3f zSlope %.4f start %.3f length %.3f\n", e.eq[0], e.eq[1], e.eq[2], e.zBase, e.zSlope, e.start, e.length);
			}
		}
		// trees: roots are what the leafs name
		std::vector<int> depth_hist(12, 0), child_hist(18, 0);
		std::size_t roots = 0, leafs_with = 0, leaf_nodes = 0;
		int same_material = 0, mixed_material = 0, contents_match = 0, contents_differ = 0;
		const auto depth_of = [&](const auto& self, const std::uint32_t n) -> int
		{
			const aabb& a = trees[n];
			if (!a.children)
			{
				leaf_nodes++;
				return 1;
			}
			child_hist[std::min<int>(a.children, 17)]++;
			int deepest = 0;
			for (std::uint32_t c = 0; c < a.children; c++)
			{
				deepest = std::max(deepest, self(self, a.index + c));
			}
			return deepest + 1;
		};
		for (const c1::cLeaf_t& leaf : leafs)
		{
			if (!leaf.collAabbCount)
			{
				continue;
			}
			leafs_with++;
			std::int32_t contents = 0;
			std::set<int> used;
			for (std::uint32_t r = 0; r < leaf.collAabbCount; r++)
			{
				const std::uint32_t n = leaf.firstCollAabbIndex + r;
				roots++;
				depth_hist[static_cast<std::size_t>(std::min(depth_of(depth_of, n), 11))]++;
				used.insert(trees[n].material);
				contents |= materials[trees[n].material].contents;
			}
			(used.size() == leaf.collAabbCount ? same_material : mixed_material)++;
			(contents == leaf.terrainContents ? contents_match : contents_differ)++;
		}
		std::printf("\ntrees: %zu leafs with terrain, %zu roots, %zu leaf nodes; one root per material in %d leafs ( several roots of one material in %d ); terrainContents == contents of the roots' materials in %d leafs ( not in %d )\n  root depth:",
			leafs_with, roots, leaf_nodes, same_material, mixed_material, contents_match, contents_differ);
		for (const int d : depth_hist)
		{
			std::printf(" %d", d);
		}
		std::printf("\n  children per inner node:");
		for (const int d : child_hist)
		{
			std::printf(" %d", d);
		}
		// a leaf node names one partition; are partitions shared between leafs?
		std::vector<int> part_uses(parts.size(), 0);
		for (const aabb& a : trees)
		{
			if (!a.children && a.index < parts.size())
			{
				part_uses[a.index]++;
			}
		}
		int unused = 0, once = 0, more = 0;
		for (const int u : part_uses)
		{
			(u == 0 ? unused : u == 1 ? once : more)++;
		}
		std::printf("\n  partitions named by no / one / several tree leafs: %d / %d / %d\n", unused, once, more);
		for (int k = 0; k < show && k < static_cast<int>(trees.size()); k++)
		{
			const aabb& a = trees[static_cast<std::size_t>(k)];
			std::printf("  node %d: mid %.1f %.1f %.1f half %.1f %.1f %.1f material %u children %u index %u\n", k, a.mid[0], a.mid[1], a.mid[2], a.half[0], a.half[1], a.half[2],
				a.material, a.children, a.index);
		}
		// ---- rules: winding, the walkable bits, which edges are borders
		{
			const auto vertex = [&](const partition& p, const std::uint32_t corner) { return static_cast<std::size_t>(p.block) * 1024 + tris[corner]; };
			struct tri_info { float n[3]; std::size_t v[3]; };
			std::vector<tri_info> info(static_cast<std::size_t>(cm.triCount));
			std::map<std::pair<std::size_t, std::size_t>, std::vector<std::pair<std::uint32_t, int>>> edges;
			for (const partition& p : parts)
			{
				for (std::uint32_t t = p.first; t < p.first + p.tris; t++)
				{
					tri_info& ti = info[t];
					for (int c = 0; c < 3; c++)
					{
						ti.v[c] = vertex(p, 3 * t + static_cast<std::uint32_t>(c));
					}
					const float* a = &verts[3 * ti.v[0]];
					const float* b = &verts[3 * ti.v[1]];
					const float* c = &verts[3 * ti.v[2]];
					const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
					ti.n[0] = e1[1] * e2[2] - e1[2] * e2[1];
					ti.n[1] = e1[2] * e2[0] - e1[0] * e2[2];
					ti.n[2] = e1[0] * e2[1] - e1[1] * e2[0];
					const float l = std::sqrt(ti.n[0] * ti.n[0] + ti.n[1] * ti.n[1] + ti.n[2] * ti.n[2]);
					for (float& x : ti.n)
					{
						x = l > 0 ? x / l : 0;
					}
					for (int c2 = 0; c2 < 3; c2++)
					{
						const std::size_t x = ti.v[c2], y = ti.v[(c2 + 1) % 3];
						edges[{ std::min(x, y), std::max(x, y) }].push_back({ t, c2 });
					}
				}
			}
			int up = 0, down = 0, steep = 0;
			// bit against: [ own triangle walkable ][ neighbour: none, walkable, not walkable ]
			int bits[2][3][2] = {};
			for (std::uint32_t t = 0; t < info.size(); t++)
			{
				(info[t].n[2] > 0.7f ? up : info[t].n[2] < -0.7f ? down : steep)++;
				for (int c = 0; c < 3; c++)
				{
					const std::size_t x = info[t].v[c], y = info[t].v[(c + 1) % 3];
					int neighbour = 0;
					for (const auto& [other, corner] : edges[{ std::min(x, y), std::max(x, y) }])
					{
						if (other != t)
						{
							neighbour = std::fabs(info[other].n[2]) > 0.7f ? 1 : std::max(neighbour, 2);
						}
					}
					const std::uint32_t bit = 3 * t + static_cast<std::uint32_t>(c);
					bits[std::fabs(info[t].n[2]) > 0.7f][neighbour][(walk[bit >> 5] >> (bit & 31)) & 1]++;
				}
			}
			std::printf("winding: cross( v1 - v0, v2 - v0 ) points up ( z > 0.7 ) for %d triangles, down for %d, steeper %d\n", up, down, steep);
			for (int own = 0; own < 2; own++)
			{
				for (int n = 0; n < 3; n++)
				{
					std::printf("walkable bit, triangle %s, neighbour over the edge %s: 0 x %d, 1 x %d\n", own ? "walkable" : "steep", n == 0 ? "none" : n == 1 ? "walkable" : "steep",
						bits[own][n][0], bits[own][n][1]);
				}
			}
			// borders against the rule "edge whose surface does not go on outwards in the xy plane"
			std::size_t rule_total = 0, matched = 0, on_edge = 0;
			std::size_t at_border = 0;
			for (const partition& p : parts)
			{
				for (std::uint32_t b = 0; b < p.borders; b++)
				{
					const border& e = borders[at_border + b];
					// is it an edge of one of the partition's triangles?
					bool found = false;
					for (std::uint32_t t = p.first; t < p.first + p.tris && !found; t++)
					{
						for (int c = 0; c < 3 && !found; c++)
						{
							const float* a = &verts[3 * info[t].v[c]];
							const float* b2 = &verts[3 * info[t].v[(c + 1) % 3]];
							const float da = e.eq[0] * a[0] + e.eq[1] * a[1] - e.eq[2], db = e.eq[0] * b2[0] + e.eq[1] * b2[1] - e.eq[2];
							const float sa = e.eq[1] * a[0] - e.eq[0] * a[1], sb = e.eq[1] * b2[0] - e.eq[0] * b2[1];
							found = std::fabs(da) < 0.05f && std::fabs(db) < 0.05f
								&& ((std::fabs(sa - e.start) < 0.05f && std::fabs(sb - e.start - e.length) < 0.05f) || (std::fabs(sb - e.start) < 0.05f && std::fabs(sa - e.start - e.length) < 0.05f));
						}
					}
					on_edge += found;
				}
				at_border += p.borders;
				// the rule
				std::set<std::pair<std::size_t, std::size_t>> mine;
				for (std::uint32_t t = p.first; t < p.first + p.tris; t++)
				{
					for (int c = 0; c < 3; c++)
					{
						const std::size_t x = info[t].v[c], y = info[t].v[(c + 1) % 3], third = info[t].v[(c + 2) % 3];
						const float* a = &verts[3 * x];
						const float* b2 = &verts[3 * y];
						const float dx = b2[0] - a[0], dy = b2[1] - a[1], l = std::sqrt(dx * dx + dy * dy);
						if (l < 0.01f)
						{
							continue;
						}
						float n[2] = { -dy / l, dx / l };
						const float d = n[0] * a[0] + n[1] * a[1];
						const float side = n[0] * verts[3 * third] + n[1] * verts[3 * third + 1] - d;
						if (std::fabs(side) < 0.01f)
						{
							continue;
						}
						const float sign = side > 0 ? -1.0f : 1.0f;		// the normal points away from the triangle
						bool goes_on = false;
						for (const auto& [other, corner] : edges[{ std::min(x, y), std::max(x, y) }])
						{
							if (other != t)
							{
								const std::size_t o3 = info[other].v[(corner + 2) % 3];
								goes_on = goes_on || sign * (n[0] * verts[3 * o3] + n[1] * verts[3 * o3 + 1] - d) > 0.01f;
							}
						}
						if (!goes_on)
						{
							mine.insert({ std::min(x, y), std::max(x, y) });
						}
					}
				}
				rule_total += mine.size();
				matched += mine.size() == p.borders;
			}
			std::printf("borders: %zu of %d lie on an edge of a triangle of their partition; the rule gives %zu borders, the same count as stored in %zu of %zu partitions\n", on_edge,
				cm.borderCount, rule_total, matched, parts.size());
			// which edges are borders: per class ( own triangle up / steep / down by its outward normal -cross, how many
			// triangles share the edge, the neighbour's class, whether the surface goes on outwards in xy ) how many
			// edges of the map there are and how many of them are stored as a border of their partition
			{
				std::map<std::string, std::pair<int, int>> classes;
				std::size_t first_border = 0;
				const auto cls = [&](const std::uint32_t t) { return -info[t].n[2] > 0.7f ? 'U' : -info[t].n[2] < -0.7f ? 'D' : 'S'; };
				for (const partition& p : parts)
				{
					std::set<std::pair<std::size_t, std::size_t>> seen;
					for (std::uint32_t t = p.first; t < p.first + p.tris; t++)
					{
						for (int c = 0; c < 3; c++)
						{
							const std::size_t x = info[t].v[c], y = info[t].v[(c + 1) % 3], third = info[t].v[(c + 2) % 3];
							const float* a = &verts[3 * x];
							const float* b2 = &verts[3 * y];
							const float dx = b2[0] - a[0], dy = b2[1] - a[1], l = std::sqrt(dx * dx + dy * dy);
							std::string kind;
							kind += cls(t);
							const auto& sharing = edges[{ std::min(x, y), std::max(x, y) }];
							kind += sharing.size() == 1 ? " alone" : sharing.size() == 2 ? " pair " : " many ";
							char other_class = '-';
							bool goes_on = false, same_partition = false;
							if (l >= 0.01f)
							{
								const float n[2] = { -dy / l, dx / l };
								const float d = n[0] * a[0] + n[1] * a[1];
								const float side = n[0] * verts[3 * third] + n[1] * verts[3 * third + 1] - d;
								const float sign = side > 0 ? -1.0f : 1.0f;
								for (const auto& [other, corner] : sharing)
								{
									if (other != t)
									{
										other_class = cls(other);
										same_partition = same_partition || (other >= p.first && other < p.first + p.tris);
										const std::size_t o3 = info[other].v[(corner + 2) % 3];
										goes_on = goes_on || sign * (n[0] * verts[3 * o3] + n[1] * verts[3 * o3 + 1] - d) > 0.01f;
									}
								}
								kind += std::fabs(side) < 0.01f ? " own-vertical" : goes_on ? " goes-on" : " ends";
							}
							else
							{
								kind += " vertical-edge";
							}
							kind += std::string(" nb ") + other_class + (same_partition ? " same-part" : "");
							// stored?
							bool stored = false;
							for (std::uint32_t b = 0; b < p.borders && !stored; b++)
							{
								const border& e = borders[first_border + b];
								const float da = e.eq[0] * a[0] + e.eq[1] * a[1] - e.eq[2], db = e.eq[0] * b2[0] + e.eq[1] * b2[1] - e.eq[2];
								const float sa = e.eq[1] * a[0] - e.eq[0] * a[1], sb = e.eq[1] * b2[0] - e.eq[0] * b2[1];
								stored = std::fabs(da) < 0.05f && std::fabs(db) < 0.05f
									&& ((std::fabs(sa - e.start) < 0.05f && std::fabs(sb - e.start - e.length) < 0.05f) || (std::fabs(sb - e.start) < 0.05f && std::fabs(sa - e.start - e.length) < 0.05f))
									&& std::fabs(e.zBase - (std::fabs(sa - e.start) < 0.05f ? a[2] : b2[2])) < 0.05f;
							}
							classes[kind].first++;
							classes[kind].second += stored;
						}
					}
					first_border += p.borders;
				}
				for (const auto& [kind, counts] : classes)
				{
					std::printf("  %-48s edges %7d borders %7d\n", kind.c_str(), counts.first, counts.second);
				}
			}
		}
		return 0;
	}
	if (mode == "smodels")
	{
		// The static models of a map: per draw instance one line for a fit of the cull distance ( csv: model, model
		// radius, scale, cullDist, flags, primary light, probe, lods and their distances ), and how the clip map's
		// entry of the same place stores its matrix ( equal to the draw axis / scale, or to its transpose ).
		zone::Zone z;
		if (const zone::Status s = z.open(argv[2], key); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return 1;
		}
		zone::GfxWorldView world;
		zone::ClipMapView clip;
		if (!zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), world) || !zone::read_clip_map(z, zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP), clip))
		{
			return 1;
		}
		const bool csv = argc > 4 && std::string(argv[4]) == "csv";
		int same = 0, transposed = 0, both = 0, neither = 0, unmatched = 0;
		for (const zone::ClipStaticModel& c : clip.staticModels)
		{
			const zone::WorldStaticModel* draw = nullptr;
			for (const zone::WorldStaticModel& d : world.staticModels)
			{
				if (std::fabs(d.raw.origin[0] - c.raw.origin[0]) < 0.01f && std::fabs(d.raw.origin[1] - c.raw.origin[1]) < 0.01f && std::fabs(d.raw.origin[2] - c.raw.origin[2]) < 0.01f)
				{
					draw = &d;
					break;
				}
			}
			if (!draw)
			{
				unmatched++;
				continue;
			}
			bool eq = true, tr = true;
			for (int a = 0; a < 3; a++)
			{
				for (int k = 0; k < 3; k++)
				{
					eq = eq && std::fabs(c.raw.invScaledAxis[a][k] - draw->raw.axis[a][k] / draw->raw.scale) < 2e-3f;
					tr = tr && std::fabs(c.raw.invScaledAxis[a][k] - draw->raw.axis[k][a] / draw->raw.scale) < 2e-3f;
				}
			}
			(eq && tr ? both : eq ? same : tr ? transposed : neither)++;
		}
		std::printf("%zu draw instances, %zu clip entries: matrix == axis / scale %d, == transpose( axis ) / scale %d, symmetric ( both ) %d, neither %d, no draw instance at the place %d\n",
			world.staticModels.size(), clip.staticModels.size(), same, transposed, both, neither, unmatched);
		if (csv)
		{
			const std::vector<c1::GfxStaticModelInst> insts = z.get_array<c1::GfxStaticModelInst>(world.raw.dpvs.smodelInsts, world.raw.dpvs.smodelCount);
			std::printf("model,radius,half_x,half_y,half_z,scale,cull,flags,light,probe,lods,lod0,lod1,lod2,lod3,inst_radius,lighting_dx,lighting_dy,lighting_dz,ground\n");
			for (std::size_t m = 0; m < world.staticModels.size(); m++)
			{
				const zone::WorldStaticModel& d = world.staticModels[m];
				float radius = 0;
				c1::Bounds box{};
				c1::XModel raw{};
				if (d.raw.model && !z.reference(d.raw.model))
				{
					raw = z.get<c1::XModel>(d.raw.model);
					radius = z.get<float>(d.raw.model + 316);
					box = z.get<c1::Bounds>(d.raw.model + 320);
				}
				const c1::Bounds& ib = insts[m].bounds;
				std::uint32_t ground;
				std::memcpy(&ground, d.raw.groundLighting, 4);
				std::printf("%.*s,%.2f,%.2f,%.2f,%.2f,%.3f,%u,%u,%u,%u,%u,%.0f,%.0f,%.0f,%.0f,%.2f,%.1f,%.1f,%.1f,%08x\n", static_cast<int>(d.modelName.size()), d.modelName.data(), radius,
					box.halfSize[0], box.halfSize[1], box.halfSize[2], d.raw.scale, d.raw.cullDist, d.raw.flags, d.raw.primaryLightIndex, d.raw.reflectionProbeIndex, raw.numLods,
					raw.lodInfo[0].dist, raw.lodInfo[1].dist, raw.lodInfo[2].dist, raw.lodInfo[3].dist,
					std::sqrt(ib.halfSize[0] * ib.halfSize[0] + ib.halfSize[1] * ib.halfSize[1] + ib.halfSize[2] * ib.halfSize[2]),
					insts[m].lightingOrigin[0] - d.raw.origin[0], insts[m].lightingOrigin[1] - d.raw.origin[1], insts[m].lightingOrigin[2] - d.raw.origin[2], ground);
			}
		}
		return 0;
	}
	if (argc < 5)
	{
		return 1;
	}
	zone::ZoneEditor editor;
	if (const zone::Status s = editor.open(argv[2], key); !s)
	{
		std::printf("%s\n", s.message.c_str());
		return 1;
	}
	const std::string type = argv[4];
	const std::uint32_t first = argc > 5 ? static_cast<std::uint32_t>(std::atoi(argv[5])) : 0;
	const std::uint32_t count = argc > 6 ? static_cast<std::uint32_t>(std::atoi(argv[6])) : 8;
	const std::uint32_t max_bytes = argc > 7 ? static_cast<std::uint32_t>(std::atoi(argv[7])) : 256;
	for (std::size_t a = 0; a < editor.asset_count(); a++)
	{
		const zone::EditorAsset asset = editor.asset(a);
		if (type != c1::XASSET_NAMES[asset.type])
		{
			continue;
		}
		std::printf("asset %zu %s '%s': %u pieces\n", a, type.c_str(), asset.name.c_str(), asset.chunks);
		for (std::uint32_t p = mode == "pieces" ? 0 : first; p < asset.chunks && (mode == "pieces" || p < first + count); p++)
		{
			const zone::ZoneDataId id = editor.asset_data(a, p);
			zone::ZoneDataInfo info;
			editor.data_info(id, info);
			const auto bytes = editor.data_bytes(id);
			if (mode == "save")
			{
				// codol_bsp_probe save <ff> <exe> <type> <piece> 1 <file>
				if (std::FILE* f = argc > 7 ? std::fopen(argv[7], "wb") : nullptr)
				{
					std::fwrite(bytes.data(), 1, bytes.size(), f);
					std::fclose(f);
					std::printf("piece %u: %zu bytes -> %s\n", p, bytes.size(), argv[7]);
				}
				continue;
			}
			std::printf("  piece %u: %u bytes%s, %u pointers, loaded at %08X\n", p, info.size, info.string ? " string" : "", info.pointers, info.address);
			if (mode == "pieces")
			{
				continue;
			}
			if (info.string)
			{
				std::printf("    \"%.*s\"\n", static_cast<int>(std::min<std::size_t>(bytes.size(), 200)), reinterpret_cast<const char*>(bytes.data()));
				continue;
			}
			for (std::uint32_t k = 0; k < bytes.size() && k < max_bytes; k += 4)
			{
				if (k % 32 == 0)
				{
					std::printf("%s    +0x%03X", k ? "\n" : "", k);
				}
				std::uint32_t word = 0;
				std::memcpy(&word, bytes.data() + k, std::min<std::size_t>(4, bytes.size() - k));
				std::printf(" %08X", word);
			}
			std::printf("\n");
		}
		break;
	}
	return 0;
}
