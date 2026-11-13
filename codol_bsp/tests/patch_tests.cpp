// codol_bsp_patch_tests: a room with every kind of patch and brush primitive the editor's Brush and Patch menus
// make ( codol_edit patch_ops.hpp ), compiled, and what the zone holds for them: collision triangles, drawn
// triangles, lightmaps.
//
//   codol_bsp_patch_tests [--out <dir>] [--game <install>]
//
// Needs the 1.1.1.13 install ( materials come from its zones ); without it the map is only written.

#include "test_maps.hpp"

#include "codol_bsp/bsp.hpp"
#include "codol_edit/patch_ops.hpp"
#include "codol_mapc/server_check.hpp"
#include "codol_zone/assets.hpp"
#include "codol_zone/zone_editor.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace
{
	using namespace codol;
	namespace fs = std::filesystem;
	using edit::vec3;

	int checks = 0, failed = 0;

	void check(const bool ok, const char* what, const char* file, const int line)
	{
		checks++;
		if (!ok)
		{
			failed++;
			std::printf("FAILED %s ( %s:%d )\n", what, file, line);
		}
	}
#define CHECK(x) check((x), #x, __FILE__, __LINE__)

	struct made
	{
		std::vector<edit::object_id> patches, brushes;
	};

	// the sky room of the other tests with, on its floor: a capped patch cylinder, an arch ( a curve ), a bumpy
	// terrain mesh, a capped bevel, a thickened curve with seams, a cone patch, and one brush of every primitive
	made make_map(edit::document& doc, const bool with_patches)
	{
		const bsp::test::materials m;
		bsp::test::make_box_room(doc, m, true);
		made out;

		const auto keep = [&](std::vector<edit::object_id>& list)
		{
			for (const edit::object_id id : doc.selection())
			{
				if (std::find(list.begin(), list.end(), id) == list.end())
				{
					list.push_back(id);
				}
			}
		};

		if (with_patches)
		{
			// a cylinder with both caps
			edit::create_patch_shape(doc, edit::patch_shape::cylinder, { 40, 140, 0 }, { 88, 188, 96 }, 2, m.other);
			edit::patch_cap(doc);
			keep(out.patches);

			// an arch over the floor: a flat 5 x 3 curve whose middle columns are lifted
			const edit::object_id arch = edit::create_curve_patch(doc, { -180, -40, 1 }, { -100, 40, 1 }, 5, 3, 2, m.crate);
			const int middle[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11 };
			doc.move_patch_vertices(arch, middle, { 0, 0, 48 });
			edit::patch_change_subdivision(doc, 2);		// two steps finer: the subdivision number goes down until the tessellation changed twice
			keep(out.patches);

			// terrain with noise and a turned edge
			const edit::object_id hill = doc.create_terrain_patch({ 150, -60, 8 }, { 250, 40, 8 }, 5, 5, 2, m.floor);
			edit::patch_add_noise(doc, hill, {}, { 0, 0, 6 }, 3);
			edit::patch_turn_edge(doc, hill, 1, 1);
			keep(out.patches);

			// a bevel with caps
			edit::create_patch_shape(doc, edit::patch_shape::bevel, { -60, 140, 0 }, { -12, 188, 64 }, 2, m.other);
			edit::patch_cap(doc);
			keep(out.patches);

			// a slab: a flat curve, thickened with seams
			edit::create_curve_patch(doc, { -180, -200, 40 }, { -100, -140, 40 }, 3, 3, 2, m.crate);
			edit::patch_thicken(doc, 8.0, true);
			keep(out.patches);

			// a cone and an end cap
			edit::create_patch_shape(doc, edit::patch_shape::cone, { 330, 140, 0 }, { 378, 188, 80 }, 2, m.other);
			keep(out.patches);
			edit::create_patch_shape(doc, edit::patch_shape::endcap, { 330, -60, 0 }, { 378, -12, 64 }, 2, m.other);
			keep(out.patches);
		}

		out.brushes.push_back(edit::create_brush_shape(doc, edit::brush_shape::prism, { 230, 40, 0 }, { 294, 104, 64 }, 8, 2, m.crate));
		out.brushes.push_back(edit::create_brush_shape(doc, edit::brush_shape::cone, { 230, -180, 0 }, { 294, -116, 80 }, 8, 2, m.crate));
		out.brushes.push_back(edit::create_brush_shape(doc, edit::brush_shape::sphere, { -60, 230, 0 }, { 4, 294, 64 }, 8, 2, m.crate));
		out.brushes.push_back(edit::create_brush_shape(doc, edit::brush_shape::wedge, { -340, -180, 0 }, { -280, -120, 48 }, 0, 0, m.crate));
		out.brushes.push_back(edit::create_brush_shape(doc, edit::brush_shape::prism, { -340, 230, 0 }, { -280, 290, 96 }, 5, 2, m.crate));

		doc.select(out.brushes);
		doc.set_detail(true);
		doc.deselect_all();
		return out;
	}

	struct compiled
	{
		bool ok = false;
		bsp::report rep;
		fs::path file;
	};

	compiled compile(const fs::path& out, const fs::path& game, const char* map)
	{
		compiled c;
		bsp::options opts;
		opts.map = out / "maps" / (std::string(map) + ".map");
		opts.exe = game / "C1.exe";
		opts.install = game;
		c.file = out / "ff" / ("mp_" + std::string(map) + ".ff");
		opts.out = c.file;
		opts.cache = out;
		opts.light = true;
		opts.quality = 0;

		std::string error;
		c.ok = bsp::compile(opts, c.rep, error);
		if (!c.ok)
		{
			std::printf("%s: %s\n", map, error.c_str());
		}

		return c;
	}

	// triangles the tessellation of the patches has, as the compiler counts them for the collision ( it drops
	// triangles without area: the fan of a cap around its centre )
	std::size_t collision_triangles(const edit::document& doc, const std::vector<edit::object_id>& patches, std::size_t* all = nullptr)
	{
		std::size_t count = 0, total = 0;

		for (const edit::object_id id : patches)
		{
			edit::mesh mesh;
			doc.build_mesh(id, mesh, edit::patch_subdivisions(*doc.find_patch(id)));

			for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
			{
				const auto pos = [&](const std::size_t k)
				{
					const edit::mesh_vertex& v = mesh.vertices[mesh.indices[t + k]];
					return vec3{ v.position[0], v.position[1], v.position[2] };
				};

				total++;
				count += length(cross(pos(1) - pos(0), pos(2) - pos(0))) >= 0.01;
			}
		}

		if (all)
		{
			*all = total;
		}

		return count;
	}
	// "light: N faces in P lightmap page( s ), L luxels of S units; .." of a report: L
	std::size_t luxels_of(const bsp::report& rep)
	{
		for (const std::string& note : rep.notes)
		{
			const auto at = note.find(" luxels of ");
			if (note.rfind("light: ", 0) == 0 && at != std::string::npos)
			{
				std::size_t start = note.rfind(' ', at - 1);
				return static_cast<std::size_t>(std::strtoull(note.c_str() + start + 1, nullptr, 10));
			}
		}

		return 0;
	}
}

int main(int argc, char** argv)
{
	fs::path out = fs::temp_directory_path() / "codol_bsp_patch_tests";
	fs::path game = CODOL_BSP_GAME_DIR;

	for (int i = 1; i + 1 < argc; i++)
	{
		if (std::string(argv[i]) == "--out") { out = argv[i + 1]; }
		if (std::string(argv[i]) == "--game") { game = argv[i + 1]; }
	}

	std::error_code ec;
	fs::create_directories(out / "maps", ec);
	fs::create_directories(out / "ff", ec);

	// ---- the maps: with and without the patches
	edit::document doc, plain;
	const made with = make_map(doc, true);
	make_map(plain, false);

	std::string error;
	CHECK(doc.save_map_file(out / "maps" / "patches.map", error));
	CHECK(plain.save_map_file(out / "maps" / "patches_none.map", error));

	// the same room with the lightmap layer of every face at a quarter of radiant's 16384 ( Texture Lock > Lightmaps
	// keeps such a layer on the brush ): codol_bsp makes the luxels of those faces a quarter as large each way
	edit::document fine;
	make_map(fine, false);
	fine.select_all();
	{
		edit::tex_def lm;
		lm.size_x = lm.size_y = 4096.0;
		CHECK(fine.set_lightmap(lm));
	}
	fine.deselect_all();
	CHECK(fine.save_map_file(out / "maps" / "patches_fine.map", error));

	// what was written reads back as the same patches
	{
		edit::document again;
		CHECK(again.load_map_file(out / "maps" / "patches.map", error) && again.load_warnings().empty());
		CHECK(again.patch_count() == doc.patch_count() && again.brush_count() == doc.brush_count());
		CHECK(again.save_map_text() == doc.save_map_text());
	}

	// 3 ( cylinder, 2 caps ) + 1 arch + 1 hill + 3 ( bevel, 2 caps ) + 6 ( curve, back, 4 seams ) + cone + endcap
	CHECK(with.patches.size() == 16 && doc.patch_count() == 16);

	std::size_t all_triangles = 0;
	const std::size_t solid_triangles = collision_triangles(doc, with.patches, &all_triangles);
	std::printf("patches: %zu, %zu triangles in their tessellation, %zu with an area\n", with.patches.size(), all_triangles, solid_triangles);

	if (!fs::exists(game / "C1.exe", ec))
	{
		std::printf("codol_bsp_patch_tests: no install at %s - the maps were written to %s, nothing was compiled\n", game.string().c_str(), out.string().c_str());
		return failed ? 1 : 0;
	}

	const compiled c = compile(out, game, "patches");
	const compiled none = compile(out, game, "patches_none");
	const compiled finer = compile(out, game, "patches_fine");
	CHECK(finer.ok);
	if (none.ok && finer.ok)
	{
		const std::size_t a = luxels_of(none.rep), b = luxels_of(finer.rep);
		std::printf("lightmap layer 16384: %zu luxels, 4096: %zu luxels\n", a, b);
		// four times as many luxels each way, minus what the padding and the cap of the pages take
		CHECK(a > 0 && b > a * 4);
	}
	CHECK(c.ok && none.ok);

	if (c.ok && none.ok)
	{
		std::printf("%s", c.rep.text().c_str());

		CHECK(c.rep.patches == 16 && none.rep.patches == 0);
		CHECK(!c.rep.leaked);
		// the brush primitives are brushes of the clip map in both
		CHECK(c.rep.clip_brushes == none.rep.clip_brushes && c.rep.brushes == none.rep.brushes);

		for (const std::string& w : c.rep.warnings)
		{
			CHECK(w.find("patch") == std::string::npos);
		}

		// ---- drawn: every triangle of every patch is a triangle of the world, with a lightmap
		CHECK(c.rep.triangles >= none.rep.triangles + solid_triangles);
		CHECK(c.rep.triangles <= none.rep.triangles + all_triangles + all_triangles / 4);		// + what fixing T-junctions adds
		CHECK(c.rep.surfaces > none.rep.surfaces);
		CHECK(c.rep.lightmaps >= 1 && c.rep.lightmaps >= none.rep.lightmaps);

		// ---- collision: the triangles are the clip map's terrain
		zone::ClientKey key;
		std::vector<std::uint8_t> bytes;

		if (key.load_from_exe(game / "C1.exe") && zone::read_fastfile(c.file, key, bytes))
		{
			mapc::server_collision_info info;
			CHECK(mapc::check_server_collision(bytes, error, &info));
			CHECK(static_cast<std::size_t>(info.terrain_triangles) == solid_triangles);
			CHECK(info.terrain_partitions > 0 && info.terrain_tree_nodes >= info.terrain_partitions);

			zone::Zone z;
			CHECK(static_cast<bool>(z.load(bytes, "patches")));
			const c1::clipMap_t cm = z.get<c1::clipMap_t>(zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP));
			const std::vector<float> verts = z.get_array<float>(cm.verts, 3u * cm.vertCount);
			const std::vector<std::uint16_t> tris = z.get_array<std::uint16_t>(cm.triIndices, 3u * static_cast<std::size_t>(cm.triCount));

			// the top cap of the cylinder: triangles at z 96 inside its outline, facing up ( stored clockwise seen
			// from above ); its wall: vertices between the inscribed circle and the box
			int cap = 0, cap_up = 0, wall = 0, arch_top = 0;
			for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
			{
				const float* a = &verts[3u * tris[t]];
				const float* b = &verts[3u * tris[t + 1]];
				const float* d = &verts[3u * tris[t + 2]];

				const auto in_box = [](const float* p, const float x0, const float y0, const float x1, const float y1)
				{
					return p[0] >= x0 - 0.01f && p[0] <= x1 + 0.01f && p[1] >= y0 - 0.01f && p[1] <= y1 + 0.01f;
				};

				if (in_box(a, 40, 140, 88, 188) && in_box(b, 40, 140, 88, 188) && in_box(d, 40, 140, 88, 188))
				{
					if (std::fabs(a[2] - 96) < 0.01f && std::fabs(b[2] - 96) < 0.01f && std::fabs(d[2] - 96) < 0.01f)
					{
						cap++;
						cap_up += (b[0] - a[0]) * (d[1] - a[1]) - (b[1] - a[1]) * (d[0] - a[0]) < 0;
					}
					else if (a[2] > 0.5f && a[2] < 95.5f)
					{
						const double r = std::hypot(a[0] - 64.0, a[1] - 164.0);
						wall += r > 23.0 && r < 34.0;
					}
				}

				// the crown of the arch: 49 above the floor at x -140
				arch_top += in_box(a, -180, -40, -100, 40) && a[2] > 36.0f;
			}

			CHECK(cap >= 16 && cap_up == cap);
			CHECK(wall > 0);
			CHECK(arch_top > 0);
		}
		else
		{
			CHECK(!"the compiled zone could not be read back");
		}
	}

	std::printf("\ncodol_bsp_patch_tests: %d checks, %d failed\n", checks, failed);
	return failed ? 1 : 0;
}
