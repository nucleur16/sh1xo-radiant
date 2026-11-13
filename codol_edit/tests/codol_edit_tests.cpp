// Tests of codol_edit. Plain checks and counters, no framework.
//
//   codol_edit_tests [folder with .map files] ...
//   codol_edit_tests --check <file.map> [n]                              describe the first n invalid brushes of a map
//   codol_edit_tests --canonical <in.map> <out.map>                      rewrite every object in radiant's formatting
//   codol_edit_tests --import <map.ff> <client exe> <out.map> [out.ents] fastfile import ( and override export )
//
// Uses, when they exist ( otherwise the tests that need them are skipped and say so ):
//   CODOL_EDIT_MAP_SOURCE_DIR   radiant .map files: byte identical load -> save round trip
//   CODOL_EDIT_GAME_DIR         CoDOL 1.1.1.13 install ( read only ): fastfile import, geometry invariants, override export
// Extra folders on the command line are added to the round trip test.

#include <codol_edit/codol_edit.hpp>

#include <chrono>
#include <cstdio>
#include <fstream>

using namespace codol::edit;

// texture_tools_tests.cpp: texture layers, flips, find / replace, rope, noise; returns the failed checks
int test_texture_tools(int& checks);

static int g_failed = 0;
static int g_checks = 0;
static int g_skipped = 0;

#define CHECK(expr) do { g_checks++; if (!(expr)) { g_failed++; std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #expr); } } while (0)
#define CHECK_MSG(expr, msg) do { g_checks++; if (!(expr)) { g_failed++; std::printf("FAILED %s:%d  %s  ( %s )\n", __FILE__, __LINE__, #expr, std::string(msg).c_str()); } } while (0)

namespace
{
	bool read_file(const std::filesystem::path& path, std::string& out)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in.is_open())
		{
			return false;
		}

		out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		return true;
	}

	bool near(const double a, const double b, const double eps = 1e-6)
	{
		return std::fabs(a - b) <= eps;
	}

	bool near(const vec3& a, const vec3& b, const double eps = 1e-6)
	{
		return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
	}

	double volume(const brush_geometry& geo)
	{
		double v = 0.0;
		for (const auto& indices : geo.face_vertices)
		{
			for (std::size_t k = 2; k < indices.size(); k++)
			{
				v += dot(geo.vertices[indices[0]], cross(geo.vertices[indices[k - 1]], geo.vertices[indices[k]]));
			}
		}

		return v / 6.0;
	}

	double total_volume(const document& doc)
	{
		double v = 0.0;
		for (const object_id ent : doc.entities())
		{
			for (const object_id child : doc.find_entity(ent)->children)
			{
				if (const auto* geo = doc.geometry(child))
				{
					v += volume(*geo);
				}
			}
		}

		return v;
	}

	std::vector<object_id> world_brushes(const document& doc)
	{
		std::vector<object_id> out;
		for (const object_id child : doc.find_entity(doc.worldspawn())->children)
		{
			if (doc.kind(child) == object_kind::brush)
			{
				out.push_back(child);
			}
		}

		return out;
	}

	// a map as radiant writes it: layers, a detail brush on a layer, a terrain mesh, a curve, a brush entity,
	// point entities, a block the reader does not know
	std::string sample_map(const std::string& eol)
	{
		const char* lines[] =
		{
			"iwmap 4",
			"\"000_Global\" flags  active",
			"\"000_Global/sub\" flags ",
			"\"The Map\" flags ",
			"// entity 0",
			"{",
			"\"classname\" \"worldspawn\"",
			"\"ambient\" \".01\"",
			"// brush 0",
			"{",
			"layer \"000_Global/sub\"",
			"  contents detail;",
			" ( 64 0 0 ) ( 64 0 -64 ) ( 64 -64 0 ) wall_a 128 32 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 0 0 0 ) ( 0 0 -64 ) ( 0 64 0 ) wall_a 128 32 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 0 64 0 ) ( 0 64 -64 ) ( 64 64 0 ) wall_a 128 32 8 -4 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 0 0 0 ) ( 0 0 -64 ) ( -64 0 0 ) wall_a 128 32 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 0 0 64 ) ( -64 0 64 ) ( 0 64 64 ) floor_b 64 64 0 0 45 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 0 0 0 ) ( -64 0 0 ) ( 0 -64 0 ) caulk 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			"}",
			"// brush 1",
			" {",
			"  mesh",
			"  {",
			"  contents nonColliding;",
			"  toolFlags splitGeo;",
			"   terrain_mat",
			"   lightmap_gray",
			"   smoothing smoothing_hard",
			"   3 2 16 8",
			"   (",
			"\tv 128 0 0 c 255 255 255 255 t 0 0 0 0 f 1",
			"\tv 128 64 0 c 255 128 0 255 t 0 256 0 4",
			"   )",
			"   (",
			"\tv 192 0 8 c 255 255 255 255 t 256 0 4 0",
			"\tv 192 64 8.5 c 255 255 255 255 t 256 256 4 4",
			"   )",
			"   (",
			"\tv 256 0 0 c 255 255 255 255 t 512 0 8 0",
			"\tv 256 64 0 c 255 255 255 255 t 512 256 8 4",
			"   )",
			"  }",
			" }",
			"// brush 2",
			" {",
			"  curve",
			"  {",
			"   curve_mat",
			"   lightmap_gray",
			"   3 3 16 8",
			"   (",
			"\tv 0 128 0 t 0 0 0 0",
			"\tv 0 128 32 t 0 128 0 2",
			"\tv 0 128 64 t 0 256 0 4",
			"   )",
			"   (",
			"\tv 32 160 0 t 128 0 2 0",
			"\tv 32 160 32 t 128 128 2 2",
			"\tv 32 160 64 t 128 256 2 4",
			"   )",
			"   (",
			"\tv 64 128 0 t 256 0 4 0",
			"\tv 64 128 32 t 256 128 4 2",
			"\tv 64 128 64 t 256 256 4 4",
			"   )",
			"  }",
			" }",
			"}",
			"// entity 1",
			"{",
			"\"targetname\" \"door\"",
			"\"classname\" \"script_brushmodel\"",
			"// brush 0",
			"{",
			" ( 364 300 300 ) ( 364 300 236 ) ( 364 236 300 ) door_c 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 300 300 300 ) ( 300 300 236 ) ( 300 364 300 ) door_c 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 300 364 300 ) ( 300 364 236 ) ( 364 364 300 ) door_c 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 300 300 300 ) ( 300 300 236 ) ( 236 300 300 ) door_c 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 300 300 364 ) ( 236 300 364 ) ( 300 364 364 ) door_c 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			" ( 300 300 300 ) ( 236 300 300 ) ( 300 236 300 ) door_c 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0",
			"}",
			"}",
			"// entity 2",
			"{",
			"\"classname\" \"mp_dm_spawn\"",
			"\"origin\" \"500 500 16\"",
			"\"angles\" \"0 90 0\"",
			"}",
			"// entity 3",
			"{",
			"\"origin\" \"-200 40 8\"",
			"\"classname\" \"script_origin\"",
			"}",
		};

		std::string out;
		for (const char* line : lines)
		{
			out += line;
			out += eol;
		}

		return out;
	}

	// ---- reader / writer ------------------------------------------------------------------------

	void test_reader_writer()
	{
		for (const char* eol : { "\n", "\r\n" })
		{
			const std::string text = sample_map(eol);

			document doc;
			std::string error;
			CHECK_MSG(doc.load_map_text(text, error), error);
			CHECK(doc.load_warnings().empty());
			CHECK(doc.save_map_text() == text);
			CHECK(doc.save_map_text_canonical() == text);	// the sample is in radiant's own formatting
			CHECK(doc.line_end() == eol);
			CHECK(!doc.modified());

			CHECK(doc.entities().size() == 4);
			CHECK(doc.layers().size() == 3);
			CHECK(doc.active_layer() == "000_Global");
			CHECK(doc.brush_count() == 2);
			CHECK(doc.patch_count() == 2);

			const entity* world = doc.find_entity(doc.worldspawn());
			CHECK(world && world->is_worldspawn() && world->children.size() == 3);
			CHECK(world->value("ambient") == ".01");

			const brush* b = doc.find_brush(world->children[0]);
			CHECK(b && b->faces.size() == 6 && b->is_detail() && b->layer == "000_Global/sub");
			CHECK(b && b->faces[2].texture.material == "wall_a" && b->faces[2].texture.size_x == 128 && b->faces[2].texture.shift_x == 8
				&& b->faces[2].texture.shift_y == -4 && b->faces[4].texture.rotation == 45 && b->faces[0].lightmap.size_x == 16384);

			const patch* mesh_patch = doc.find_patch(world->children[1]);
			CHECK(mesh_patch && mesh_patch->type == patch_type::mesh && mesh_patch->width == 3 && mesh_patch->height == 2);
			CHECK(mesh_patch && mesh_patch->material == "terrain_mat" && mesh_patch->lightmap == "lightmap_gray" && mesh_patch->smoothing == "smoothing_hard");
			CHECK(mesh_patch && mesh_patch->has_content("nonColliding") && mesh_patch->tool_flags.size() == 1 && mesh_patch->tool_flags[0] == "splitGeo");
			CHECK(mesh_patch && mesh_patch->at(0, 0).has_flag && mesh_patch->at(0, 0).flag == 1 && mesh_patch->at(0, 1).color[1] == 128);
			CHECK(mesh_patch && mesh_patch->at(1, 1).xyz.z == 8.5 && mesh_patch->at(2, 1).tex.size() == 4 && mesh_patch->at(2, 1).tex[0] == 512);

			const patch* curve = doc.find_patch(world->children[2]);
			CHECK(curve && curve->type == patch_type::curve && curve->width == 3 && curve->height == 3 && !curve->at(0, 0).has_color && curve->smoothing.empty());

			const entity* door = doc.find_entity(doc.entities()[1]);
			CHECK(door && door->classname() == "script_brushmodel" && door->children.size() == 1 && !door->is_point());

			const entity* spawn = doc.find_entity(doc.entities()[2]);
			CHECK(spawn && spawn->is_point() && near(spawn->origin(), { 500, 500, 16 }) && near(spawn->angles(), { 0, 90, 0 }));
		}

		// things radiant does not write: comments, blank lines, odd spacing, an unknown block, a face the reader can not parse
		{
			const std::string text =
				"iwmap 4\n"
				"// a comment before the first entity\n"
				"\n"
				"{\n"
				"\t\"classname\"   \"worldspawn\"\n"
				"  // comment between keys\n"
				"\"k\" \"v\"\n"
				"\n"
				"// brush 7\n"
				"{\n"
				"( 64 0 0 ) ( 64 0 -64 ) ( 64 -64 0 ) wall_a 128 32 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0\n"
				"( 0 0 0 ) ( 0 0 -64 ) ( 0 64 0 ) wall_a 128 32 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0 smoothing soft\n"
				"( 0 64 0 ) ( 0 64 -64 ) ( 64 64 0 ) wall_a 128 32 8 -4 0 0 lightmap_gray 16384 16384 0 0 0 0\n"
				"( 0 0 0 ) ( 0 0 -64 ) ( -64 0 0 ) wall_a 128 32 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0\n"
				"( 0 0 64 ) ( -64 0 64 ) ( 0 64 64 ) floor_b 64 64 0 0 45 0 lightmap_gray 16384 16384 0 0 0 0\n"
				"( 0 0 0 ) ( -64 0 0 ) ( 0 -64 0 ) caulk 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0\n"
				"  {\n    physics_box\n    {\n      1 2 3\n    }\n  }\n"
				"}\n"
				"{\n"
				" something_else 1 2 3 { nested }\n"
				"}\n"
				"} // trailing comment\n"
				"{ \"classname\" \"info_null\" \"origin\" \"1 2 3\" }\n"
				"// the end\n";

			document doc;
			std::string error;
			CHECK_MSG(doc.load_map_text(text, error), error);
			CHECK(doc.save_map_text() == text);
			CHECK(doc.load_warnings().size() == 1);
			CHECK(doc.entities().size() == 2);

			const entity* world = doc.find_entity(doc.worldspawn());
			CHECK(world && world->children.size() == 2);

			const brush* b = world ? doc.find_brush(world->children[0]) : nullptr;
			CHECK(b && !b->opaque && b->faces.size() == 6 && b->extra.size() == 1 && b->faces[1].trailing == "smoothing soft");
			CHECK(b && doc.geometry(b->id)->valid);

			const brush* unknown = world ? doc.find_brush(world->children[1]) : nullptr;
			CHECK(unknown && unknown->opaque);

			// a physics shape as the collision map tools write it
			{
				const std::string phys = "iwmap 4\r\n// entity 0\r\n{\r\n\"classname\" \"worldspawn\"\r\n// brush 0\r\n  {\r\n    physics_cylinder\r\n    {\r\n"
					"      0.000000 0.000000 1.000000 -0.000000 -0.000000 0.000000 10.000000 11.947636\r\n    }\r\n  }\r\n}\r\n";
				document p;
				CHECK(p.load_map_text(phys, error) && p.load_warnings().empty());
				CHECK(p.save_map_text() == phys && p.save_map_text_canonical() == phys);
				CHECK(p.brush_count() == 1 && p.find_brush(p.find_entity(p.worldspawn())->children[0])->opaque);
			}

			// editing the first brush keeps everything else, including the block that is not understood
			doc.select(b->id);
			CHECK(doc.translate({ 8, 0, 0 }));
			const std::string edited = doc.save_map_text();
			CHECK(edited != text);
			CHECK(edited.find("physics_box") != std::string::npos);
			CHECK(edited.find(" something_else 1 2 3 { nested }\n") != std::string::npos);
			CHECK(edited.find("smoothing soft") != std::string::npos);
			CHECK(edited.find("// a comment before the first entity\n") != std::string::npos);
			CHECK(edited.find("} // trailing comment\n") != std::string::npos);
			CHECK(edited.ends_with("// the end\n"));

			// the unknown block is not touched by operations
			doc.select(unknown->id);
			CHECK(doc.translate({ 8, 0, 0 }));		// selected, but there is nothing to move
			CHECK(doc.save_map_text() == edited);

			CHECK(doc.undo());
			CHECK(doc.save_map_text() == text);

			document again;
			CHECK(again.load_map_text(edited, error));
			CHECK(again.save_map_text() == edited);
		}

		// numbers
		CHECK(format_number(0.1) == "0.1" && format_number(-64) == "-64" && format_number(6977.7998046875) == "6977.7998");
		CHECK(format_number(0.000013) == "1.3e-005" && format_number(-3.9999999e-05) == "-3.9999999e-005" && format_number(1e20) == "1e+020");
		CHECK(format_key_number(0.5) == "0.5" && format_key_number(-0.0000001) == "0" && format_key_vec3({ 1, -2.25, 1e-9 }) == "1 -2.25 0");

		{
			vec3 v;
			CHECK(parse_vec3("1 -2.5 3e2", v) && near(v, { 1, -2.5, 300 }));
			CHECK(parse_vec3(" 90", v) && v.x == 90);
			CHECK(!parse_vec3("abc", v) && !parse_vec3("", v));
		}

		// errors
		{
			document doc;
			std::string error;
			CHECK(!doc.load_map_text("not a map", error) && !error.empty());
			CHECK(!doc.load_map_text("iwmap 4\n{\n\"classname\" \"worldspawn\"\n", error));
			CHECK(!doc.load_map_text("iwmap 4\n", error));
			CHECK(doc.entities().size() == 1);	// unchanged: still the empty map
		}

		// a new map
		{
			document doc;
			CHECK(doc.save_map_text() == "iwmap 4\n// entity 0\n{\n\"classname\" \"worldspawn\"\n}\n");

			std::string error;
			document again;
			CHECK(again.load_map_text(doc.save_map_text(), error));
			CHECK(again.find_entity(again.worldspawn())->is_worldspawn());
		}
	}

	struct round_trip_stats
	{
		int files = 0, identical = 0, canonical = 0, failed_to_load = 0;
		std::size_t bytes = 0, brushes = 0, patches = 0, entities = 0, opaque = 0;
	};

	void round_trip_folder(const std::filesystem::path& folder, round_trip_stats& stats, const bool required)
	{
		std::error_code ec;
		if (!std::filesystem::is_directory(folder, ec))
		{
			std::printf("   %s: not found, skipped\n", folder.string().c_str());
			g_skipped++;
			return;
		}

		std::vector<std::filesystem::path> files;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, ec))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".map")
			{
				files.push_back(entry.path());
			}
		}

		std::sort(files.begin(), files.end());

		for (const auto& file : files)
		{
			std::string text;
			if (!read_file(file, text))
			{
				continue;
			}

			stats.files++;
			stats.bytes += text.size();

			document doc;
			std::string error;

			if (!doc.load_map_text(text, error))
			{
				stats.failed_to_load++;
				std::printf("   NOT LOADED %s: %s\n", file.string().c_str(), error.c_str());

				if (required)
				{
					CHECK_MSG(false, file.string());
				}

				continue;
			}

			const bool same = doc.save_map_text() == text;
			stats.identical += same ? 1 : 0;

			if (!same)
			{
				std::printf("   NOT IDENTICAL %s\n", file.string().c_str());
			}

			CHECK_MSG(same, file.string());

			if (doc.save_map_text_canonical() == text)
			{
				stats.canonical++;
			}

			stats.brushes += doc.brush_count();
			stats.patches += doc.patch_count();
			stats.entities += doc.entities().size();
			stats.opaque += doc.load_warnings().size();

			// an edit of one object leaves every other byte alone
			const auto brushes = world_brushes(doc);
			if (!brushes.empty())
			{
				const brush* b = doc.find_brush(brushes[brushes.size() / 2]);
				if (!b->opaque)
				{
					const std::size_t offset = b->source_body.offset, length = b->source_body.length;

					doc.select(b->id);
					CHECK(doc.translate({ 1, 2, 3 }));

					const std::string edited = doc.save_map_text();
					const std::size_t new_length = edited.size() - (text.size() - length);

					CHECK(edited != text);
					CHECK(edited.size() >= text.size() - length);
					CHECK(edited.compare(0, offset, text, 0, offset) == 0);
					CHECK(edited.compare(offset + new_length, std::string::npos, text, offset + length, std::string::npos) == 0);

					CHECK(doc.undo());
					CHECK(doc.save_map_text() == text);
				}
			}
		}
	}

	void test_round_trip(const std::vector<std::string>& extra_folders)
	{
		std::printf("-- round trip\n");

		round_trip_stats stats;
		round_trip_folder(CODOL_EDIT_MAP_SOURCE_DIR, stats, true);

		std::printf("   %s: %d files ( %.1f MB ), %d byte identical, %d not loaded; %zu entities, %zu brushes, %zu patches, %zu blocks kept verbatim; "
			"%d files also identical when every object is rewritten\n",
			CODOL_EDIT_MAP_SOURCE_DIR, stats.files, static_cast<double>(stats.bytes) / 1048576.0, stats.identical, stats.failed_to_load, stats.entities, stats.brushes,
			stats.patches, stats.opaque, stats.canonical);

		for (const auto& folder : extra_folders)
		{
			round_trip_stats extra;
			round_trip_folder(folder, extra, false);

			std::printf("   %s: %d files ( %.1f MB ), %d byte identical, %d not loaded; %zu entities, %zu brushes, %zu patches, %zu blocks kept verbatim; "
				"%d files also identical when every object is rewritten\n",
				folder.c_str(), extra.files, static_cast<double>(extra.bytes) / 1048576.0, extra.identical, extra.failed_to_load, extra.entities, extra.brushes,
				extra.patches, extra.opaque, extra.canonical);
		}
	}

	// ---- geometry -------------------------------------------------------------------------------

	void test_geometry()
	{
		document doc;
		std::string error;
		CHECK(doc.load_map_text(sample_map("\n"), error));

		const entity* world = doc.find_entity(doc.worldspawn());
		const object_id box = world->children[0];

		const brush_geometry* geo = doc.geometry(box);
		CHECK(geo && geo->valid && geo->closed && geo->vertices.size() == 8 && geo->face_count_with_winding == 6);
		CHECK(geo && near(geo->box.mins, { 0, 0, 0 }) && near(geo->box.maxs, { 64, 64, 64 }));
		CHECK(geo && near(volume(*geo), 64.0 * 64.0 * 64.0));
		CHECK(geo && near(geo->planes[0].normal, { 1, 0, 0 }) && near(geo->planes[0].dist, 64) && near(geo->planes[5].normal, { 0, 0, -1 }));

		for (const auto& indices : geo->face_vertices)
		{
			CHECK(indices.size() == 4);
		}

		// the windings turn counter clockwise seen from outside
		for (std::size_t f = 0; f < geo->face_vertices.size(); f++)
		{
			const winding w = geo->face_winding(static_cast<int>(f));
			CHECK(dot(cross(w[1] - w[0], w[2] - w[0]), geo->planes[f].normal) > 0.0);
		}

		// uv: 128 units per repeat along x, shift 8 texels of 256
		{
			const brush* b = doc.find_brush(box);
			const vec2 uv0 = face_uv({ 0, 64, 0 }, geo->planes[2].normal, b->faces[2].texture, 256, 256);
			const vec2 uv1 = face_uv({ 64, 64, 0 }, geo->planes[2].normal, b->faces[2].texture, 256, 256);
			const vec2 uv2 = face_uv({ 0, 64, 32 }, geo->planes[2].normal, b->faces[2].texture, 256, 256);
			CHECK(near(uv1.x - uv0.x, 0.5) && near(uv0.x, 8.0 / 256.0) && near(uv1.y, uv0.y));
			CHECK(near(uv2.y - uv0.y, -1.0) && near(uv0.y, -4.0 / 256.0));	// v runs down, 32 units per repeat
		}

		// mesh
		{
			mesh m;
			CHECK(doc.build_mesh(box, m));
			CHECK(m.vertices.size() == 24 && m.indices.size() == 36 && m.parts.size() == 6 && m.revision == doc.revision(box));
			CHECK(m.parts[4].material == "floor_b" && m.parts[4].face == 4 && m.parts[4].index_count == 6);
			CHECK(near(m.box.maxs, { 64, 64, 64 }));

			// a texture size from the caller changes the shift part of the uv only
			doc.set_texture_size_callback([](std::string_view, int& w, int& h) { w = 512; h = 128; return true; });

			mesh m2;
			CHECK(doc.build_mesh(box, m2));
			bool differs = false;
			for (std::size_t i = 0; i < m.vertices.size(); i++)
			{
				differs |= m.vertices[i].uv[0] != m2.vertices[i].uv[0];
			}

			CHECK(differs);
			doc.set_texture_size_callback(nullptr);
		}

		// terrain mesh: the control points, 2 triangles per cell
		{
			mesh m;
			CHECK(doc.build_mesh(world->children[1], m, 4));
			CHECK(m.vertices.size() == 6 && m.indices.size() == 12 && m.parts.size() == 1 && m.parts[0].material == "terrain_mat");
			CHECK(m.vertices[1].color[1] == 128);
			CHECK(near(m.vertices[5].uv[0], 2.0) && near(m.vertices[5].uv[1], 1.0));	// texels / 256
			CHECK(m.vertices[0].normal[2] > 0.9f);
		}

		// curve: 3x3 control points, one bezier segment each way
		{
			mesh m;
			CHECK(doc.build_mesh(world->children[2], m, 4));
			CHECK(m.vertices.size() == 25 && m.indices.size() == 4 * 4 * 6);

			mesh fine;
			CHECK(doc.build_mesh(world->children[2], fine, 8));
			CHECK(fine.vertices.size() == 81);

			// the surface passes through the corners and bulges half way towards the middle control point
			std::vector<vec3> points;
			int columns = 0, rows = 0;
			tessellate_patch(*doc.find_patch(world->children[2]), 2, points, columns, rows);
			CHECK(columns == 3 && rows == 3);
			CHECK(near(points[0], { 0, 128, 0 }) && near(points[8], { 64, 128, 64 }) && near(points[4], { 32, 144, 32 }));
		}

		CHECK(near(doc.object_bounds(world->children[1]).maxs, { 256, 64, 8.5 }));
		CHECK(near(doc.object_bounds(doc.entities()[1]).mins, { 300, 300, 300 }));
		CHECK(near(doc.object_bounds(doc.entities()[2]).mins, { 484, 484, 16 }) && near(doc.object_bounds(doc.entities()[2]).maxs, { 516, 516, 88 }));

		// primitives
		{
			plane p;
			CHECK(plane_from_points({ 0, 0, 0 }, { -64, 0, 0 }, { 0, -64, 0 }, p) && near(p.normal, { 0, 0, -1 }));
			CHECK(!plane_from_points({ 0, 0, 0 }, { 1, 1, 1 }, { 2, 2, 2 }, p));

			winding w = base_winding({ { 0, 0, 1 }, 10 });
			clip_winding(w, { { 1, 0, 0 }, 5 });
			clip_winding(w, { { -1, 0, 0 }, 5 });
			clip_winding(w, { { 0, 1, 0 }, 5 });
			clip_winding(w, { { 0, -1, 0 }, 5 });
			CHECK(w.size() == 4 && near(winding_area(w), 100.0));

			// large coordinates
			winding far = base_winding(plane{ normalize(vec3{ 1, 1, 1 }), 131072.0 });
			clip_winding(far, { { 1, 0, 0 }, 131072.0 });
			clip_winding(far, { { 0, 1, 0 }, 131072.0 });
			clip_winding(far, { { 0, 0, 1 }, 131072.0 });
			CHECK(far.size() == 3);
			for (const vec3& v : far)
			{
				CHECK(std::fabs(dot(v, normalize(vec3{ 1, 1, 1 })) - 131072.0) < 1e-6);
			}

			std::vector<plane> planes;
			std::vector<std::vector<int>> faces;
			const vec3 tetra[4] = { { 0, 0, 0 }, { 10, 0, 0 }, { 0, 10, 0 }, { 0, 0, 10 } };
			CHECK(convex_hull(tetra, planes, faces) && planes.size() == 4);
			const vec3 flat[4] = { { 0, 0, 0 }, { 10, 0, 0 }, { 0, 10, 0 }, { 10, 10, 0 } };
			CHECK(!convex_hull(flat, planes, faces));

			vec3 axes[3];
			angles_to_axes({ 10, 20, 30 }, axes);
			CHECK(near(axes_to_angles(axes), { 10, 20, 30 }, 1e-9));
			CHECK(near(dot(cross(axes[0], axes[1]), axes[2]), 1.0, 1e-12));

			const transform r = transform::rotation({ 10, 0, 0 }, { 0, 0, 1 }, 90);
			CHECK(near(r.apply({ 11, 0, 0 }), { 10, 1, 0 }) && r.is_rotation());
			CHECK(near(transform::concat(transform::translation({ 1, 0, 0 }), transform::scaling({}, { 2, 2, 2 })).apply({ 1, 1, 1 }), { 4, 2, 2 }));
		}
	}

	// ---- operations -----------------------------------------------------------------------------

	std::size_t invalid_brushes(const document& doc)
	{
		std::size_t n = 0;
		for (const object_id ent : doc.entities())
		{
			for (const object_id child : doc.find_entity(ent)->children)
			{
				const auto* geo = doc.geometry(child);
				n += (geo && !geo->valid) ? 1 : 0;
			}
		}

		return n;
	}

	// runs `op`, then checks: it is exactly one undo step, undo gives the text and the selection of before, redo the
	// ones of after, and the result loads again and saves to the same text
	template <typename Fn>
	void check_op(document& doc, const char* name, Fn&& op, const bool expect_text_change = true)
	{
		const std::string before = doc.save_map_text();
		const std::vector<object_id> selection_before(doc.selection().begin(), doc.selection().end());
		const std::size_t steps = doc.undo_count();
		const std::size_t invalid_before = invalid_brushes(doc);

		const bool ok = op();
		CHECK_MSG(ok, std::string(name) + ": " + doc.last_error());

		if (!ok)
		{
			return;
		}

		const std::string after = doc.save_map_text();
		const std::vector<object_id> selection_after(doc.selection().begin(), doc.selection().end());

		CHECK_MSG(doc.undo_count() == steps + 1, name);
		CHECK_MSG(doc.undo_name() == name, std::string(name) + " / " + std::string(doc.undo_name()));
		CHECK_MSG(!expect_text_change || after != before, name);
		CHECK_MSG(doc.modified(), name);

		CHECK_MSG(doc.undo(), name);
		CHECK_MSG(doc.save_map_text() == before, name);
		CHECK_MSG(std::equal(doc.selection().begin(), doc.selection().end(), selection_before.begin(), selection_before.end()), name);

		CHECK_MSG(doc.redo(), name);
		CHECK_MSG(doc.save_map_text() == after, name);
		CHECK_MSG(std::equal(doc.selection().begin(), doc.selection().end(), selection_after.begin(), selection_after.end()), name);

		document again;
		std::string error;
		CHECK_MSG(again.load_map_text(after, error), std::string(name) + ": " + error);
		CHECK_MSG(again.save_map_text() == after, name);
		CHECK_MSG(again.load_warnings().empty(), name);

		// the operation did not leave a broken brush behind
		CHECK_MSG(invalid_brushes(doc) <= invalid_before, name);
	}

	// texture coordinates of the three plane points of a face
	void face_uvs(const document& doc, const face_ref f, vec2 out[3])
	{
		const brush* b = doc.find_brush(f.brush);
		plane p;
		plane_from_points(b->faces[f.face].points[0], b->faces[f.face].points[1], b->faces[f.face].points[2], p);

		for (int i = 0; i < 3; i++)
		{
			out[i] = face_uv(b->faces[f.face].points[i], p.normal, b->faces[f.face].texture, 256, 256);
		}
	}

	bool same_mod_1(const double a, const double b)
	{
		const double d = a - b;
		return std::fabs(d - std::round(d)) < 1e-6;
	}

	void test_operations()
	{
		document doc;
		std::string error;
		const std::string original = sample_map("\n");
		CHECK(doc.load_map_text(original, error));

		const object_id world = doc.worldspawn();
		const object_id box = doc.find_entity(world)->children[0];
		const object_id terrain = doc.find_entity(world)->children[1];
		const object_id curve = doc.find_entity(world)->children[2];
		const object_id door = doc.entities()[1];
		const object_id door_brush = doc.find_entity(door)->children[0];
		const object_id spawn = doc.entities()[2];
		const object_id origin_ent = doc.entities()[3];

		// ---- no selection: nothing happens, nothing on the undo stack
		CHECK(!doc.translate({ 1, 0, 0 }) && !doc.delete_selection() && !doc.clone_selection() && !doc.set_material("x") && !doc.csg_hollow(4));
		CHECK(!doc.last_error().empty());
		CHECK(doc.undo_count() == 0 && !doc.can_undo() && !doc.modified());

		// ---- create
		object_id made = no_object;
		check_op(doc, "Create box", [&] { made = doc.create_box({ -128, -128, 0 }, { -64, -96, 32 }, "new_mat"); return made != no_object; });
		CHECK(doc.is_selected(made) && doc.selection().size() == 1);
		CHECK(doc.owner_of(made) == world);
		{
			const brush_geometry* geo = doc.geometry(made);
			CHECK(geo && geo->valid && geo->vertices.size() == 8 && near(volume(*geo), 64.0 * 32.0 * 32.0));
			CHECK(near(geo->box.mins, { -128, -128, 0 }) && near(geo->box.maxs, { -64, -96, 32 }));
			CHECK(doc.find_brush(made)->faces[0].texture.material == "new_mat" && doc.find_brush(made)->faces[0].texture.size_x == 64);
			CHECK(doc.find_brush(made)->layer.empty());		// the active layer is the root layer
		}

		CHECK(doc.create_box({ 0, 0, 0 }, { 0, 10, 10 }) == no_object);
		CHECK(doc.create_box({ 0, 0, 0 }, { 10, 10, 200000 }) == no_object);

		object_id cylinder = no_object;
		check_op(doc, "Create cylinder", [&] { cylinder = doc.create_cylinder({ -400, -400, 0 }, { -336, -336, 128 }, 8); return cylinder != no_object; });
		CHECK(doc.geometry(cylinder)->vertices.size() == 16 && doc.geometry(cylinder)->face_count_with_winding == 10);
		CHECK(near(doc.geometry(cylinder)->box.mins.z, 0) && near(doc.geometry(cylinder)->box.maxs.z, 128));

		object_id cone = no_object;
		check_op(doc, "Create cone", [&] { cone = doc.create_cone({ -400, -200, 0 }, { -336, -136, 64 }, 6); return cone != no_object; });
		CHECK(doc.geometry(cone)->vertices.size() == 7 && doc.geometry(cone)->face_count_with_winding == 7);

		object_id wedge = no_object;
		check_op(doc, "Create wedge", [&] { wedge = doc.create_wedge({ -400, 0, 0 }, { -336, 64, 64 }); return wedge != no_object; });
		CHECK(doc.geometry(wedge)->vertices.size() == 6 && near(volume(*doc.geometry(wedge)), 64.0 * 64.0 * 64.0 / 2.0));

		object_id new_terrain = no_object;
		check_op(doc, "Create terrain", [&] { new_terrain = doc.create_terrain_patch({ 600, 0, 0 }, { 856, 256, 0 }, 5, 5); return new_terrain != no_object; });
		CHECK(doc.find_patch(new_terrain)->vertices.size() == 25 && near(doc.find_patch(new_terrain)->at(4, 4).xyz, { 856, 256, 0 }));

		// ---- translate / rotate / scale / flip, with texture lock
		doc.select(box);
		{
			vec2 before[3], after[3];
			const face_ref f{ box, 2 };
			face_uvs(doc, f, before);
			const std::uint64_t revision = doc.revision(box);

			check_op(doc, "Move", [&] { return doc.translate({ 16, 8, -24 }); });
			CHECK(near(doc.geometry(box)->box.mins, { 16, 8, -24 }) && near(doc.geometry(box)->box.maxs, { 80, 72, 40 }));
			CHECK(doc.revision(box) != revision);

			face_uvs(doc, f, after);
			for (int i = 0; i < 3; i++)
			{
				CHECK(same_mod_1(before[i].x, after[i].x) && same_mod_1(before[i].y, after[i].y));
			}

			CHECK(doc.find_brush(box)->faces[2].texture.size_x == 128 && doc.find_brush(box)->faces[2].texture.rotation == 0);

			// revisions follow undo
			CHECK(doc.undo());
			CHECK(doc.revision(box) == revision);
			CHECK(doc.redo());
			CHECK(doc.revision(box) != revision);
		}

		// Quarter turns about the world axes keep every face axial: the lock is exact. ( On a face that is not
		// axial the format can not express the sheared projection, the lock is then the closest alignment. )
		struct turn { vec3 axis; double degrees; bool exact_all; };
		for (const turn& t : { turn{ { 0, 0, 1 }, 90, true }, turn{ { 1, 0, 0 }, 90, true }, turn{ { 0, 1, 0 }, -90, true }, turn{ { 0, 0, 1 }, 180, true },
			turn{ { 0, 0, 1 }, 30, false }, turn{ { 1, 2, 3 }, 47, false } })
		{
			vec2 before[6][3], after[6][3];
			plane planes_before[6];
			for (int f = 0; f < 6; f++)
			{
				face_uvs(doc, { box, f }, before[f]);
				planes_before[f] = doc.geometry(box)->planes[f];
			}

			const double vol = volume(*doc.geometry(box));
			const vec3 center = doc.selection_bounds().center();

			check_op(doc, "Rotate", [&] { return doc.rotate(center, t.axis, t.degrees); });
			CHECK(near(volume(*doc.geometry(box)), vol, 1e-3));

			for (int f = 0; f < 6; f++)
			{
				// a face whose normal is the rotation axis stays in its plane: exact whatever the angle
				const bool exact = t.exact_all || std::fabs(dot(planes_before[f].normal, normalize(t.axis))) > 0.999999;
				if (!exact)
				{
					continue;
				}

				face_uvs(doc, { box, f }, after[f]);
				for (int i = 0; i < 3; i++)
				{
					CHECK(same_mod_1(before[f][i].x, after[f][i].x) && same_mod_1(before[f][i].y, after[f][i].y));
				}
			}
		}

		// back to an axial box for what follows
		while (doc.undo_name() == "Rotate")
		{
			CHECK(doc.undo());
		}

		{
			const double vol = volume(*doc.geometry(box));
			check_op(doc, "Scale", [&] { return doc.scale(doc.selection_bounds().center(), { 2, 1, 0.5 }); });
			CHECK(near(volume(*doc.geometry(box)), vol, 1e-3));

			vec2 before[3], after[3];
			face_uvs(doc, { box, 0 }, before);
			check_op(doc, "Flip", [&] { return doc.flip(doc.selection_bounds().center(), 0); });
			face_uvs(doc, { box, 0 }, after);
			CHECK(near(volume(*doc.geometry(box)), vol, 1e-3));		// still facing outwards

			// the plane points swap places when a face is mirrored
			CHECK(same_mod_1(before[0].x, after[0].x) && same_mod_1(before[1].x, after[2].x) && same_mod_1(before[2].y, after[1].y));
		}

		// without texture lock the alignment is left alone
		{
			doc.select(made);
			doc.set_texture_lock(false);
			const tex_def before = doc.find_brush(made)->faces[0].texture;
			check_op(doc, "Move", [&] { return doc.translate({ 5, 5, 5 }); });
			CHECK(doc.find_brush(made)->faces[0].texture == before);
			doc.set_texture_lock(true);
		}

		// ---- patches and entities move too
		doc.select(terrain);
		check_op(doc, "Move", [&] { return doc.translate({ 0, 0, 16 }); });
		CHECK(near(doc.find_patch(terrain)->at(1, 1).xyz, { 192, 64, 24.5 }));

		doc.select(curve);
		check_op(doc, "Rotate", [&] { return doc.rotate({ 0, 128, 0 }, { 0, 0, 1 }, 90); });
		CHECK(near(doc.find_patch(curve)->at(2, 0).xyz, { 0, 192, 0 }));
		check_op(doc, "Flip", [&] { return doc.flip({ 0, 128, 0 }, 2); });

		doc.select(spawn);
		check_op(doc, "Move", [&] { return doc.translate({ 12, 0, 0 }); });
		CHECK(doc.find_entity(spawn)->value("origin") == "512 500 16");
		check_op(doc, "Rotate", [&] { return doc.rotate({ 0, 0, 0 }, { 0, 0, 1 }, 90); });
		CHECK(doc.find_entity(spawn)->value("origin") == "-500 512 16");
		CHECK(doc.find_entity(spawn)->value("angles") == "0 180 0");

		// a brush entity selected as a whole
		doc.select(door);
		CHECK(doc.selection().size() == 1 && doc.selection()[0] == door_brush);
		CHECK(doc.selected_entities().size() == 1 && doc.selected_entities()[0] == door);
		check_op(doc, "Move", [&] { return doc.translate({ 0, 0, 64 }); });
		CHECK(near(doc.object_bounds(door).mins, { 300, 300, 364 }));

		// ---- drags: many calls, one step
		{
			doc.select(origin_ent);
			const std::string before = doc.save_map_text();
			const std::size_t steps = doc.undo_count();

			doc.begin_transaction("Drag");
			for (int i = 0; i < 10; i++)
			{
				CHECK(doc.translate({ 1, 0, 0 }));
			}

			CHECK(doc.in_transaction());
			CHECK(doc.end_transaction());
			CHECK(doc.undo_count() == steps + 1 && doc.undo_name() == "Drag");
			CHECK(doc.find_entity(origin_ent)->value("origin") == "-190 40 8");
			CHECK(doc.undo());
			CHECK(doc.save_map_text() == before);
			CHECK(doc.redo());

			// merge key
			for (int i = 0; i < 5; i++)
			{
				doc.begin_transaction("Nudge", 77);
				CHECK(doc.translate({ 0, 1, 0 }));
				CHECK(doc.end_transaction());
			}

			CHECK(doc.undo_count() == steps + 2 && doc.undo_name() == "Nudge");
			CHECK(doc.find_entity(origin_ent)->value("origin") == "-190 45 8");

			doc.begin_transaction("Nudge", 78);
			CHECK(doc.translate({ 0, 1, 0 }));
			CHECK(doc.end_transaction());
			CHECK(doc.undo_count() == steps + 3);

			CHECK(doc.undo() && doc.undo());
			CHECK(doc.find_entity(origin_ent)->value("origin") == "-190 40 8");

			// a step after an undo never merges into an older one
			doc.begin_transaction("Drag", 0);
			doc.end_transaction();
			CHECK(doc.undo_count() == steps + 1);		// an empty transaction is not a step

			// cancel
			const std::string mid = doc.save_map_text();
			doc.begin_transaction("Cancelled");
			CHECK(doc.translate({ 100, 0, 0 }));
			CHECK(doc.delete_selection());
			CHECK(doc.kind(origin_ent) == object_kind::none);
			doc.cancel_transaction();
			CHECK(doc.save_map_text() == mid && doc.kind(origin_ent) == object_kind::entity && doc.is_selected(origin_ent));
			CHECK(doc.undo_count() == steps + 1 && !doc.in_transaction());
		}

		// ---- snap
		doc.select(box);
		check_op(doc, "Snap to grid", [&] { return doc.snap_to_grid(32); });
		for (const face& f : doc.find_brush(box)->faces)
		{
			for (const vec3& p : f.points)
			{
				CHECK(p == snap_point(p, 32));
			}
		}

		// ---- a fresh box for the shape tools
		object_id work = doc.create_box({ 1000, 1000, 0 }, { 1128, 1064, 64 }, "work");
		CHECK(work != no_object);

		check_op(doc, "Move face", [&] { return doc.move_face({ work, 0 }, 16); });
		CHECK(near(volume(*doc.geometry(work)), 144.0 * 64.0 * 64.0) || near(volume(*doc.geometry(work)), 128.0 * 80.0 * 64.0)
			|| near(volume(*doc.geometry(work)), 128.0 * 64.0 * 80.0));
		CHECK(!doc.move_face({ work, 0 }, -1000));		// would turn the brush inside out
		CHECK(!doc.move_face({ work, 17 }, 1));
		CHECK(doc.undo());

		check_op(doc, "Resize", [&] { return doc.resize_selection({ 0, 0, 32 }); });
		CHECK(near(doc.geometry(work)->box.maxs.z, 96) && near(doc.geometry(work)->box.mins.z, 0));
		check_op(doc, "Resize", [&] { return doc.resize_selection({ -8, 0, 0 }); });
		CHECK(near(doc.geometry(work)->box.mins.x, 992));
		CHECK(!doc.resize_selection({ 0, 0, 0 }));
		CHECK(doc.undo() && doc.undo());

		// corners: one corner up -> a 7 faced brush; an edge ( two corners ) -> a slope
		{
			const vec3 corner{ 1128, 1064, 64 };
			check_op(doc, "Move corners", [&] { return doc.move_brush_corners(work, std::span<const vec3>(&corner, 1), { 0, 0, 32 }); });
			CHECK(doc.geometry(work)->vertices.size() == 8 && doc.find_brush(work)->faces.size() >= 7);
			CHECK(near(doc.geometry(work)->box.maxs.z, 96));
			CHECK(doc.undo());

			const vec3 edge[2] = { { 1128, 1000, 64 }, { 1128, 1064, 64 } };
			const std::string bottom_before = doc.save_map_text();
			check_op(doc, "Move corners", [&] { return doc.move_brush_corners(work, edge, { 0, 0, -32 }); });
			CHECK(doc.find_brush(work)->faces.size() == 6 && near(volume(*doc.geometry(work)), 128.0 * 64.0 * 48.0));

			// faces that did not move keep their plane points
			int kept = 0;
			for (const face& f : doc.find_brush(work)->faces)
			{
				plane p;
				plane_from_points(f.points[0], f.points[1], f.points[2], p);
				if (near(p.normal, { 0, 0, -1 }) || near(p.normal, { -1, 0, 0 }) || near(p.normal, { 0, 1, 0 }) || near(p.normal, { 0, -1, 0 }) || near(p.normal, { 1, 0, 0 }))
				{
					kept++;
				}
			}

			CHECK(kept == 5);

			const vec3 nothing{ 5, 5, 5 };
			CHECK(!doc.move_brush_corners(work, std::span<const vec3>(&nothing, 1), { 1, 0, 0 }));

			// squashing the brush flat is refused
			const vec3 top[4] = { { 1000, 1000, 64 }, { 1000, 1064, 64 }, { 1128, 1000, 32 }, { 1128, 1064, 32 } };
			CHECK(!doc.move_brush_corners(work, std::span<const vec3>(top, 2), { 0, 0, -64 }) || doc.geometry(work)->valid);
			while (doc.undo_name() == "Move corners")
			{
				doc.undo();
			}
		}

		{
			const int indices[2] = { 2, 3 };
			check_op(doc, "Move vertices", [&] { return doc.move_patch_vertices(terrain, indices, { 0, 0, 10 }); });
			CHECK(!doc.move_patch_vertices(terrain, std::span<const int>(indices, 0), { 0, 0, 10 }));
			const int bad[1] = { 99 };
			CHECK(!doc.move_patch_vertices(terrain, bad, { 0, 0, 10 }));
		}

		// ---- clip
		doc.select(work);
		{
			const double vol = volume(*doc.geometry(work));

			check_op(doc, "Clip", [&] { return doc.clip(plane{ { 1, 0, 0 }, 1064 }, clip_keep::back); });
			CHECK(near(volume(*doc.geometry(work)), vol / 2) && near(doc.geometry(work)->box.maxs.x, 1064));
			CHECK(doc.undo());

			check_op(doc, "Clip", [&] { return doc.clip(plane{ { 1, 0, 0 }, 1064 }, clip_keep::front, "clip_mat"); });
			CHECK(near(doc.geometry(work)->box.mins.x, 1064) && doc.find_brush(work)->faces.size() == 6);
			CHECK(std::any_of(doc.find_brush(work)->faces.begin(), doc.find_brush(work)->faces.end(), [](const face& f) { return f.texture.material == "clip_mat"; }));
			CHECK(doc.undo());

			const std::size_t brushes = doc.brush_count();
			check_op(doc, "Clip", [&] { return doc.clip({ 1000, 1000, 0 }, { 1128, 1064, 0 }, { 1000, 1000, 64 }, clip_keep::both); });
			CHECK(doc.brush_count() == brushes + 1 && doc.selection().size() == 2);
			CHECK(near(volume(*doc.geometry(doc.selection()[0])) + volume(*doc.geometry(doc.selection()[1])), vol));
			CHECK(doc.undo());
			CHECK(doc.brush_count() == brushes);

			// entirely on the removed side: deleted; on the kept side: untouched ( and then it is not a step )
			check_op(doc, "Clip", [&] { return doc.clip(plane{ { 1, 0, 0 }, 5000 }, clip_keep::front); });
			CHECK(doc.kind(work) == object_kind::none && doc.selection().empty());
			CHECK(doc.undo());
			CHECK(doc.kind(work) == object_kind::brush && doc.is_selected(work));		// the same id is back

			const std::size_t steps = doc.undo_count();
			CHECK(doc.clip(plane{ { 1, 0, 0 }, 5000 }, clip_keep::back));
			CHECK(doc.undo_count() == steps);
		}

		// ---- csg
		{
			const double vol = volume(*doc.geometry(work));
			const std::size_t brushes = doc.brush_count();

			const object_id cutter = doc.create_box({ 1032, 1016, 16 }, { 1096, 1048, 48 }, "cut");		// inside `work`
			CHECK(doc.is_selected(cutter));

			const double before = total_volume(doc);
			check_op(doc, "CSG subtract", [&] { return doc.csg_subtract(); });
			CHECK(doc.brush_count() == brushes + 1 + 5);		// 6 pieces around the hole
			CHECK(near(total_volume(doc), before - 64.0 * 32.0 * 32.0, 1e-3));
			CHECK(doc.is_selected(cutter));

			// cutting again changes nothing: the pieces only touch the cutter
			CHECK(!doc.csg_subtract());
			CHECK(doc.undo());
			CHECK(near(volume(*doc.geometry(work)), vol));

			CHECK(doc.delete_selection());
			doc.select(work);

			check_op(doc, "Hollow", [&] { return doc.csg_hollow(8); });
			CHECK(doc.selection().size() == 6);
			CHECK(!doc.csg_hollow(1000));
			CHECK(doc.undo());

			CHECK(doc.delete_selection());
		}

		// ---- textures
		doc.select(box);
		check_op(doc, "Set material", [&] { return doc.set_material("everything"); });
		CHECK(doc.find_brush(box)->faces[5].texture.material == "everything");
		CHECK(!doc.set_material("bad name"));

		doc.deselect_all();
		doc.select_face({ box, 4 });
		doc.select_face({ box, 5 }, select_mode::add);
		CHECK(doc.selected_faces().size() == 2 && doc.is_face_selected({ box, 5 }) && doc.selection().empty());

		check_op(doc, "Set material", [&] { return doc.set_material("two_faces"); });
		CHECK(doc.find_brush(box)->faces[4].texture.material == "two_faces" && doc.find_brush(box)->faces[0].texture.material == "everything");

		{
			const tex_def before = doc.find_brush(box)->faces[4].texture;

			check_op(doc, "Shift texture", [&] { return doc.shift_texture(16, -8); });
			CHECK(near(doc.find_brush(box)->faces[4].texture.shift_x, before.shift_x + 16) && near(doc.find_brush(box)->faces[4].texture.shift_y, before.shift_y - 8));

			check_op(doc, "Scale texture", [&] { return doc.scale_texture(2, 0.5); });
			CHECK(near(doc.find_brush(box)->faces[4].texture.size_x, before.size_x * 2) && near(doc.find_brush(box)->faces[4].texture.size_y, before.size_y * 0.5));

			check_op(doc, "Rotate texture", [&] { return doc.rotate_texture(-30); });
			CHECK(near(std::fmod(doc.find_brush(box)->faces[4].texture.rotation - before.rotation + 720.0, 360.0), 330));

			tex_def absolute;
			absolute.size_x = 32;
			absolute.size_y = 48;
			absolute.rotation = 90;
			check_op(doc, "Set texture", [&] { return doc.set_texture(absolute); });
			CHECK(doc.find_brush(box)->faces[5].texture.size_y == 48 && doc.find_brush(box)->faces[5].texture.material == "two_faces");

			check_op(doc, "Fit texture", [&] { return doc.fit_texture(2, 1); });
			{
				// two repeats across the face in u, one in v
				const brush_geometry* geo = doc.geometry(box);
				const tex_def& tex = doc.find_brush(box)->faces[4].texture;
				double lo_u = 1e30, hi_u = -1e30, lo_v = 1e30, hi_v = -1e30;

				for (const int index : geo->face_vertices[4])
				{
					const vec2 uv = face_uv(geo->vertices[index], geo->planes[4].normal, tex, 256, 256);
					lo_u = std::min(lo_u, uv.x); hi_u = std::max(hi_u, uv.x);
					lo_v = std::min(lo_v, uv.y); hi_v = std::max(hi_v, uv.y);
				}

				CHECK(near(hi_u - lo_u, 2.0) && near(hi_v - lo_v, 1.0) && same_mod_1(lo_u, 0.0) && same_mod_1(lo_v, 0.0));
			}
		}

		doc.deselect_all();
		CHECK(!doc.shift_texture(1, 1));

		// ---- delete / clone
		doc.select(made);
		check_op(doc, "Delete", [&] { return doc.delete_selection(); });
		CHECK(doc.kind(made) == object_kind::none && doc.find_brush(made) == nullptr && doc.selection().empty());
		CHECK(doc.undo());
		CHECK(doc.kind(made) == object_kind::brush && doc.is_selected(made));

		{
			const std::size_t brushes = doc.brush_count(), ents = doc.entities().size();

			check_op(doc, "Clone", [&] { return doc.clone_selection({ 0, 0, 64 }); });
			CHECK(doc.brush_count() == brushes + 1 && doc.selection().size() == 1 && doc.selection()[0] != made);
			CHECK(near(doc.geometry(doc.selection()[0])->box.mins.z, doc.geometry(made)->box.mins.z + 64));
			CHECK(doc.undo());

			// a whole brush entity: a new entity with the same keys
			doc.select(door);
			check_op(doc, "Clone", [&] { return doc.clone_selection({ 128, 0, 0 }); });
			CHECK(doc.entities().size() == ents + 1);
			const object_id copy = doc.owner_of(doc.selection()[0]);
			CHECK(copy != door && doc.find_entity(copy)->value("targetname") == "door" && doc.find_entity(copy)->children.size() == 1);
			CHECK(doc.undo());

			doc.select(spawn);
			check_op(doc, "Clone", [&] { return doc.clone_selection({ 0, 64, 0 }); });
			CHECK(doc.entities().size() == ents + 1 && doc.find_entity(doc.selection()[0])->classname() == "mp_dm_spawn");
			CHECK(doc.undo());

			// deleting the last brush of a brush entity deletes the entity
			doc.select(door_brush);
			check_op(doc, "Delete", [&] { return doc.delete_selection(); });
			CHECK(doc.kind(door) == object_kind::none && doc.entities().size() == ents - 1);
			CHECK(doc.undo());
			CHECK(doc.kind(door) == object_kind::entity && doc.find_entity(door)->children.size() == 1 && doc.entities()[1] == door);
		}

		// ---- entities
		{
			const std::size_t ents = doc.entities().size();

			object_id light = no_object;
			const std::pair<std::string, std::string> extra[] = { { "radius", "300" }, { "_color", "1 0.9 0.8" } };
			check_op(doc, "Create entity", [&] { light = doc.create_point_entity("light", { 10.5, 20, 30 }, extra); return light != no_object; });
			CHECK(doc.entities().size() == ents + 1 && doc.entities().back() == light && doc.is_selected(light));
			CHECK(doc.find_entity(light)->value("origin") == "10.5 20 30" && doc.find_entity(light)->value("radius") == "300");
			CHECK(doc.create_point_entity("", {}) == no_object);

			check_op(doc, "Set key", [&] { return doc.set_key(light, "intensity", "2"); });
			check_op(doc, "Set key", [&] { return doc.set_key(light, "radius", "512"); });
			CHECK(doc.find_entity(light)->value("radius") == "512" && doc.find_entity(light)->pairs.size() == 5);
			CHECK(!doc.set_key(light, "", "x") && !doc.set_key(light, "a", "quote\"inside") && !doc.set_key(box, "a", "b"));

			check_op(doc, "Rename key", [&] { return doc.rename_key(light, "intensity", "brightness"); });
			CHECK(doc.find_entity(light)->find("intensity") == nullptr && doc.find_entity(light)->value("brightness") == "2");
			CHECK(!doc.rename_key(light, "brightness", "radius") && !doc.rename_key(light, "nope", "x") && !doc.rename_key(light, "classname", "x"));

			check_op(doc, "Remove key", [&] { return doc.remove_key(light, "brightness"); });
			CHECK(!doc.remove_key(light, "brightness") && !doc.remove_key(light, "classname"));

			// connect: a new name, then the existing ones are reused
			check_op(doc, "Connect entities", [&] { return doc.connect_entities(light, spawn); });
			CHECK(doc.find_entity(light)->value("target") == "auto1" && doc.find_entity(spawn)->value("targetname") == "auto1");
			check_op(doc, "Connect entities", [&] { return doc.connect_entities(origin_ent, door); });
			CHECK(doc.find_entity(origin_ent)->value("target") == "door");
			check_op(doc, "Connect entities", [&] { return doc.connect_entities(light, origin_ent); });
			CHECK(doc.find_entity(origin_ent)->value("targetname") == "auto1");
			CHECK(doc.targets_of(light).size() == 2 && doc.targeting(spawn).size() == 1 && doc.targeting(spawn)[0] == light);
			CHECK(!doc.connect_entities(light, light) && !doc.connect_entities(light, world));

			// brush entity from brushes, and back
			doc.select(made);
			doc.select(cone, select_mode::add);
			object_id group = no_object;
			check_op(doc, "Create entity", [&] { group = doc.create_brush_entity("script_brushmodel"); return group != no_object; });
			CHECK(doc.find_entity(group)->children.size() == 2 && doc.owner_of(made) == group && doc.owner_of(cone) == group);
			CHECK(doc.is_selected(made) && doc.is_selected(cone));

			doc.select(door_brush);
			check_op(doc, "Move to worldspawn", [&] { return doc.move_selection_to_worldspawn(); });
			CHECK(doc.owner_of(door_brush) == world && doc.kind(door) == object_kind::none);
			CHECK(doc.undo());
			CHECK(doc.owner_of(door_brush) == door && doc.kind(door) == object_kind::entity);

			doc.select(box);
			CHECK(!doc.move_selection_to_worldspawn());

			doc.deselect_all();
			CHECK(doc.create_brush_entity("x") == no_object);
		}

		// ---- contents
		doc.select(made);
		check_op(doc, "Make detail", [&] { return doc.set_detail(true); });
		CHECK(doc.find_brush(made)->is_detail());
		check_op(doc, "Make structural", [&] { return doc.set_detail(false); });
		{
			const std::string words[] = { "detail", "weaponClip" };
			check_op(doc, "Contents", [&] { return doc.set_contents(words); });
			CHECK(doc.save_map_text().find("  contents detail weaponClip;\n") != std::string::npos);
		}

		// ---- layers
		{
			check_op(doc, "Add layer", [&] { return doc.add_layer("The Map/props/small"); });
			CHECK(doc.layers().size() == 5 && doc.layers()[3].name == "The Map/props");
			CHECK(!doc.add_layer("The Map") && !doc.add_layer(""));

			doc.select(made);
			check_op(doc, "Move to layer", [&] { return doc.move_selection_to_layer("The Map/props/small"); });
			CHECK(doc.layer_of(made) == "The Map/props/small");
			CHECK(!doc.move_selection_to_layer("nope"));

			check_op(doc, "Set active layer", [&] { return doc.set_active_layer("The Map/props"); });
			CHECK(doc.active_layer() == "The Map/props");
			CHECK(doc.save_map_text().find("\"000_Global\" flags \n") != std::string::npos);
			CHECK(doc.save_map_text().find("\"The Map/props\" flags  active\n") != std::string::npos);

			const object_id on_layer = doc.create_box({ 2000, 0, 0 }, { 2064, 64, 64 });
			CHECK(doc.find_brush(on_layer)->layer == "The Map/props");

			check_op(doc, "Rename layer", [&] { return doc.rename_layer("The Map/props", "The Map/things"); });
			CHECK(doc.layer_of(made) == "The Map/things/small" && doc.layer_of(on_layer) == "The Map/things" && doc.active_layer() == "The Map/things");

			// hidden layers ( view state )
			CHECK(doc.is_visible(made));
			doc.set_layer_hidden("The Map/things", true);
			CHECK(!doc.is_visible(made) && !doc.is_visible(on_layer) && doc.is_visible(box));
			doc.set_layer_hidden("The Map/things", false);
			CHECK(doc.is_visible(made));

			check_op(doc, "Remove layer", [&] { return doc.remove_layer("The Map/things"); });
			CHECK(doc.layers().size() == 3 && doc.layer_of(made) == "The Map" && doc.layer_of(on_layer) == "The Map" && doc.active_layer() == "The Map");
			CHECK(!doc.remove_layer("nope"));
		}

		// ---- all the way back: the file as it was read
		{
			const std::string end = doc.save_map_text();
			std::size_t steps = 0;

			while (doc.undo())
			{
				steps++;
			}

			CHECK(steps > 40);
			CHECK(doc.save_map_text() == original);
			CHECK(!doc.modified());

			while (doc.redo())
			{
			}

			CHECK(doc.save_map_text() == end);
			CHECK(doc.modified());
			doc.mark_saved();
			CHECK(!doc.modified());
			CHECK(doc.undo() && doc.modified() && doc.redo() && !doc.modified());

			doc.set_undo_limit(4);
			doc.select(box);
			for (int i = 0; i < 10; i++)
			{
				CHECK(doc.translate({ 1, 0, 0 }));
			}

			CHECK(doc.undo_count() == 4);
			doc.clear_undo();
			CHECK(!doc.can_undo() && !doc.can_redo());
		}
	}

	// moving an unedited brush to another entity does not rewrite it
	void test_regroup_keeps_text()
	{
		document doc;
		std::string error;
		const std::string original = sample_map("\n");
		CHECK(doc.load_map_text(original, error));

		const object_id door = doc.entities()[1];
		const object_id door_brush = doc.find_entity(door)->children[0];
		const std::string body(doc.find_brush(door_brush)->source_body.view());
		CHECK(body.starts_with("{\n ( 364 300 300 )") && body.ends_with("}\n"));

		doc.select(door_brush);
		CHECK(doc.move_selection_to_worldspawn());
		CHECK(doc.find_brush(door_brush)->source_body.buffer != nullptr);

		const std::string moved = doc.save_map_text();
		CHECK(moved.find("// brush 3\n" + body + "}\n// entity 1\n{\n\"classname\" \"mp_dm_spawn\"") != std::string::npos);
		CHECK(moved.find("script_brushmodel") == std::string::npos);

		const object_id box = doc.find_entity(doc.worldspawn())->children[0];
		const object_id pair[2] = { box, door_brush };
		doc.select(pair);
		const object_id group = doc.create_brush_entity("func_group_test");
		CHECK(group != no_object && doc.find_entity(group)->children.size() == 2);

		const std::string grouped = doc.save_map_text();
		CHECK(grouped.find("\"classname\" \"func_group_test\"\n// brush 0\n{\nlayer \"000_Global/sub\"\n  contents detail;\n") != std::string::npos);
		CHECK(grouped.find("// brush 1\n" + body + "}\n") != std::string::npos);

		// the world now starts with the terrain: renumbered, text untouched
		CHECK(grouped.find("\"ambient\" \".01\"\n// brush 0\n {\n  mesh\n") != std::string::npos);

		CHECK(doc.undo() && doc.undo());
		CHECK(doc.save_map_text() == original);
	}

	// ---- selection, picking, queries ------------------------------------------------------------

	void test_picking()
	{
		document doc;
		std::string error;
		CHECK(doc.load_map_text(sample_map("\n"), error));

		const object_id world = doc.worldspawn();
		const object_id box = doc.find_entity(world)->children[0];
		const object_id terrain = doc.find_entity(world)->children[1];
		const object_id curve = doc.find_entity(world)->children[2];
		const object_id door = doc.entities()[1];
		const object_id door_brush = doc.find_entity(door)->children[0];
		const object_id spawn = doc.entities()[2];

		pick_result hit;

		// straight down onto the box
		CHECK(doc.pick({ { 32, 32, 1000 }, { 0, 0, -1 } }, hit));
		CHECK(hit.object == box && hit.kind == object_kind::brush && hit.entity == world && hit.face == 4);
		CHECK(near(hit.distance, 936) && near(hit.point, { 32, 32, 64 }));

		// from the side: the +x face; the direction does not have to be a unit vector
		CHECK(doc.pick({ { 200, 32, 32 }, { -2, 0, 0 } }, hit) && hit.object == box && hit.face == 0 && near(hit.distance, 68) && near(hit.point, { 64, 32, 32 }));

		// a miss, a ray that starts inside, a ray pointing away
		CHECK(!doc.pick({ { 32, 32, 1000 }, { 0, 0, 1 } }, hit));
		CHECK(!doc.pick({ { -500, -500, 1000 }, { 0, 0, -1 } }, hit));
		CHECK(!doc.pick({ { 32, 32, 32 }, { 0, 0, -1 } }, hit));

		// the terrain, the curve, the brush of the entity, the point entity ( its box )
		CHECK(doc.pick({ { 160, 32, 1000 }, { 0, 0, -1 } }, hit) && hit.object == terrain && hit.kind == object_kind::patch && near(hit.point.z, 4, 0.01));
		CHECK(doc.pick({ { 32, 1000, 32 }, { 0, -1, 0 } }, hit) && hit.object == curve && near(hit.point.y, 144, 0.5));
		CHECK(doc.pick({ { 332, 332, 1000 }, { 0, 0, -1 } }, hit) && hit.object == door_brush && hit.entity == door && near(hit.point.z, 364));
		CHECK(doc.pick({ { 500, 500, 1000 }, { 0, 0, -1 } }, hit) && hit.object == spawn && hit.kind == object_kind::entity && near(hit.point.z, 88));

		// the closest of several; all of them in order
		CHECK(doc.create_box({ 0, 0, 200 }, { 64, 64, 264 }) != no_object);
		const object_id upper = doc.selection()[0];
		CHECK(doc.pick({ { 32, 32, 1000 }, { 0, 0, -1 } }, hit) && hit.object == upper);
		const auto all = doc.pick_all({ { 32, 32, 1000 }, { 0, 0, -1 } });
		CHECK(all.size() == 2 && all[0].object == upper && all[1].object == box && all[0].distance < all[1].distance);

		// filters
		pick_options no_brushes;
		no_brushes.brushes = false;
		CHECK(!doc.pick({ { 32, 32, 1000 }, { 0, 0, -1 } }, hit, no_brushes));

		pick_options near_only;
		near_only.max_distance = 100;
		CHECK(!doc.pick({ { 32, 32, 1000 }, { 0, 0, -1 } }, hit, near_only));

		doc.set_hidden(upper, true);
		CHECK(doc.is_hidden(upper) && !doc.is_visible(upper));
		CHECK(doc.pick({ { 32, 32, 1000 }, { 0, 0, -1 } }, hit) && hit.object == box);

		pick_options with_hidden;
		with_hidden.include_hidden = true;
		CHECK(doc.pick({ { 32, 32, 1000 }, { 0, 0, -1 } }, hit, with_hidden) && hit.object == upper);

		doc.set_layer_hidden("000_Global", true);		// with its children: the box is on 000_Global/sub
		CHECK(!doc.is_visible(box));
		CHECK(!doc.pick({ { 32, 32, 1000 }, { 0, 0, -1 } }, hit));
		doc.set_layer_hidden("000_Global", false);
		doc.show_all();
		CHECK(doc.is_visible(upper) && doc.is_visible(box));

		doc.set_hidden(door, true);
		CHECK(doc.is_hidden(door_brush) && !doc.pick({ { 332, 332, 1000 }, { 0, 0, -1 } }, hit));
		doc.set_hidden(door, false);
		CHECK(doc.pick({ { 332, 332, 1000 }, { 0, 0, -1 } }, hit));

		// a box from the caller
		doc.set_entity_box_callback([](const entity& e, bounds& b)
		{
			if (e.classname() != "mp_dm_spawn")
			{
				return false;
			}

			b.mins = { -100, -100, 0 };
			b.maxs = { 100, 100, 10 };
			return true;
		});
		CHECK(doc.pick({ { 590, 590, 1000 }, { 0, 0, -1 } }, hit) && hit.object == spawn && near(hit.point.z, 26));
		doc.set_entity_box_callback(nullptr);
		CHECK(!doc.pick({ { 590, 590, 1000 }, { 0, 0, -1 } }, hit));

		// rectangle in the top view
		{
			auto ids = doc.query_rect(2, { -10, -10 }, { 70, 70 }, true);
			CHECK(ids.size() == 2 && std::find(ids.begin(), ids.end(), box) != ids.end() && std::find(ids.begin(), ids.end(), upper) != ids.end());

			ids = doc.query_rect(2, { 30, 30 }, { 40, 40 }, true);
			CHECK(ids.empty());
			ids = doc.query_rect(2, { 30, 30 }, { 40, 40 }, false);
			CHECK(ids.size() == 2);

			ids = doc.query_rect(2, { 100, -10 }, { 600, 600 }, false);
			CHECK(std::find(ids.begin(), ids.end(), terrain) != ids.end() && std::find(ids.begin(), ids.end(), spawn) != ids.end()
				&& std::find(ids.begin(), ids.end(), door_brush) != ids.end() && std::find(ids.begin(), ids.end(), box) == ids.end());

			// side view along x: y / z
			ids = doc.query_rect(0, { -10, 190 }, { 70, 270 }, true);
			CHECK(ids.size() == 1 && ids[0] == upper);
			// front view along y: x / z
			ids = doc.query_rect(1, { -10, -10 }, { 70, 70 }, true);
			CHECK(std::find(ids.begin(), ids.end(), box) != ids.end() && std::find(ids.begin(), ids.end(), upper) == ids.end());

			// a volume
			const plane frustum[2] = { { { 0, 0, 1 }, 100 }, { { 0, 0, -1 }, 10 } };
			ids = doc.query_volume(frustum, true);
			CHECK(std::find(ids.begin(), ids.end(), box) != ids.end() && std::find(ids.begin(), ids.end(), upper) == ids.end()
				&& std::find(ids.begin(), ids.end(), door_brush) == ids.end());
		}

		// selection
		{
			const std::uint64_t counter = doc.selection_counter();
			doc.select(box);
			CHECK(doc.selection_counter() != counter && doc.is_selected(box) && doc.selection().size() == 1);

			doc.select(spawn, select_mode::add);
			doc.select(box, select_mode::add);		// twice: once
			CHECK(doc.selection().size() == 2);
			doc.select(box, select_mode::toggle);
			CHECK(doc.selection().size() == 1 && doc.is_selected(spawn));
			doc.select(spawn, select_mode::remove);
			CHECK(doc.selection().empty());

			doc.select(world);						// the worldspawn is never selected as a whole
			CHECK(doc.selection().empty());
			doc.select(static_cast<object_id>(99999));
			CHECK(doc.selection().empty());

			doc.select_all();
			const std::size_t everything = doc.selection().size();
			CHECK(everything == 7);					// 3 world brushes, 2 patches, 1 entity brush, 2 point entities - 1
			doc.invert_selection();
			CHECK(doc.selection().empty());
			doc.invert_selection();
			CHECK(doc.selection().size() == everything);

			const bounds all_bounds = doc.selection_bounds();
			CHECK(all_bounds.valid() && all_bounds.maxs.x >= 516 && all_bounds.mins.x <= -208);

			doc.hide_selected();
			CHECK(doc.selection().empty());
			doc.select_all();
			CHECK(doc.selection().empty());
			doc.show_all();

			doc.select(door_brush);
			doc.select_whole_entities();
			CHECK(doc.selection().size() == 1);

			doc.select(box);
			doc.hide_unselected();
			CHECK(doc.is_visible(box) && !doc.is_visible(upper) && !doc.is_visible(spawn));
			doc.show_all();
		}

		// ids are stable and never reused
		{
			doc.select(upper);
			CHECK(doc.delete_selection());
			const object_id next = doc.create_box({ 0, 0, 300 }, { 8, 8, 308 });
			CHECK(next != upper && doc.kind(upper) == object_kind::none);
			CHECK(doc.undo() && doc.undo());
			CHECK(doc.kind(upper) == object_kind::brush && doc.kind(next) == object_kind::none);
			CHECK(doc.redo() && doc.redo());
			CHECK(doc.kind(next) == object_kind::brush);
		}
	}

	// ---- class catalog --------------------------------------------------------------------------

	void test_class_catalog()
	{
		const class_catalog& catalog = class_catalog::c1();

		std::printf("-- class catalog\n   %zu classes from %d map zones\n", catalog.classes().size(), catalog.map_count());

		CHECK(catalog.map_count() >= 20);
		CHECK(catalog.classes().size() >= 20);
		CHECK(std::is_sorted(catalog.classes().begin(), catalog.classes().end(), [](const class_info& a, const class_info& b) { return a.name < b.name; }));

		const class_info* world = catalog.find("worldspawn");
		CHECK(world && world->count == world->map_count && world->count == catalog.map_count() && world->is_point());
		CHECK(world && !world->keys.empty());

		const class_info* spawn = catalog.find("mp_dm_spawn");
		CHECK(spawn && spawn->count > 100 && spawn->is_point() && spawn->has_box && spawn->maxs[2] == 72.0f);
		CHECK(spawn && spawn->find_key("origin") && spawn->find_key("origin")->count == spawn->count && !spawn->find_key("origin")->examples.empty());
		CHECK(spawn && spawn->find_key("classname") == nullptr);

		int brush_classes = 0, total = 0, keys = 0;
		for (const class_info& c : catalog.classes())
		{
			CHECK(c.count == c.point_count + c.brush_count && c.count > 0 && c.map_count > 0 && !c.name.empty());
			brush_classes += c.is_point() ? 0 : 1;
			total += c.count;
			keys += static_cast<int>(c.keys.size());

			for (std::size_t k = 1; k < c.keys.size(); k++)
			{
				CHECK(c.keys[k - 1].count >= c.keys[k].count);
			}
		}

		std::printf("   %d entities, %d keys, %d classes that carry a compiled brush model\n", total, keys, brush_classes);
		CHECK(brush_classes >= 1);
		CHECK(catalog.find("no_such_class") == nullptr);

		float mins[3], maxs[3];
		catalog.box("no_such_class", mins, maxs);
		CHECK(mins[0] == -8.0f && maxs[2] == 8.0f);
		catalog.box("mp_made_up_spawn_point", mins, maxs);
		CHECK(mins[2] == 0.0f && maxs[2] == 72.0f);
	}

	// A map that was imported, saved by radiant and is opened again: with the two files the radiant fork keeps next
	// to it ( <map>.codol_base.map: the map as first saved, <map>.codol_orig.ents: the entity string of the zone )
	// the override of the unedited map is the original entity string.
	void test_saved_import(const char* map)
	{
		const std::filesystem::path folder = std::filesystem::path(CODOL_EDIT_MAP_SOURCE_DIR) / "codol";
		std::string text, baseline, original;

		if (!read_file(folder / (std::string(map) + ".map"), text) || !read_file(folder / (std::string(map) + ".codol_base.map"), baseline)
			|| !read_file(folder / (std::string(map) + ".codol_orig.ents"), original))
		{
			std::printf("-- %s: no saved import in %s, skipped\n", map, folder.string().c_str());
			g_skipped++;
			return;
		}

		document doc;
		std::string error;
		CHECK_MSG(doc.load_map_text(text, error), error);
		CHECK(!doc.has_import_baseline());
		doc.set_import_baseline(baseline, original);
		CHECK(doc.has_import_baseline() && doc.import_original_ents() == original);

		override_export exported;
		CHECK_MSG(doc.export_entity_override(exported, error), error);
		CHECK(exported.map_name == map);

		// both without their comment lines
		const auto body = [](std::string_view s)
		{
			while (s.starts_with("//"))
			{
				s.remove_prefix(std::min(s.find('\n') + 1, s.size()));
			}

			while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == '\0'))
			{
				s.remove_suffix(1);
			}

			return s;
		};

		CHECK(body(exported.text) == body(original));
		CHECK(exported.text.size() > 1000);

		std::printf("-- %s ( saved by radiant ): override of %d entities equals the original entity string: %s\n", map, exported.exported,
			body(exported.text) == body(original) ? "yes" : "NO");
	}

	// ---- fastfile import ------------------------------------------------------------------------

	void test_import(const char* map)
	{
		const std::filesystem::path game = CODOL_EDIT_GAME_DIR;
		const std::filesystem::path ff = game / "client_shipRetail" / "chinese" / (std::string(map) + ".ff");
		const std::filesystem::path exe = game / "C1.exe";

		std::error_code ec;
		if (!std::filesystem::exists(ff, ec) || !std::filesystem::exists(exe, ec))
		{
			std::printf("-- %s: %s not found, import tests skipped\n", map, ff.string().c_str());
			g_skipped++;
			return;
		}

		const auto t0 = std::chrono::steady_clock::now();

		document doc;
		std::string error;
		CHECK_MSG(doc.import_fastfile(ff, exe, error), error);

		if (doc.entities().size() < 2)
		{
			return;
		}

		const auto t1 = std::chrono::steady_clock::now();

		CHECK(doc.load_warnings().empty());
		CHECK(doc.has_import_baseline());
		CHECK(!doc.modified());

		const entity* world = doc.find_entity(doc.worldspawn());
		CHECK(world->is_worldspawn() && world->value("codol_map") == map && world->find("codol_base_crc32") && world->value("codol_id") == "0");

		// the document is the map codol_mapconv writes
		const std::string text = doc.save_map_text();
		CHECK(text == doc.import_baseline_map());

		// geometry: every brush gives closed windings
		std::size_t brushes = 0, valid = 0, closed = 0, faces = 0, faces_with_winding = 0, vertices = 0, triangles = 0, entity_brushes = 0, static_models = 0, point_entities = 0;
		bounds world_box;

		for (const object_id ent : doc.entities())
		{
			const entity* e = doc.find_entity(ent);

			if (e->children.empty())
			{
				point_entities++;
				static_models += e->find("codol_static") ? 1 : 0;
			}

			for (const object_id child : e->children)
			{
				const brush_geometry* geo = doc.geometry(child);
				if (!geo)
				{
					continue;
				}

				brushes++;
				entity_brushes += ent == doc.worldspawn() ? 0 : 1;
				valid += geo->valid ? 1 : 0;
				closed += geo->closed ? 1 : 0;
				faces += geo->planes.size();
				faces_with_winding += static_cast<std::size_t>(geo->face_count_with_winding);
				vertices += geo->vertices.size();
				world_box.add(geo->box);

				if (!geo->valid && brushes - valid <= 5)
				{
					std::printf("   invalid brush %u: %zu faces, %d with a winding, closed %d\n", child, geo->planes.size(), geo->face_count_with_winding, geo->closed ? 1 : 0);
				}

				// euler: V - E + F = 2 for a closed convex polyhedron
				if (geo->valid)
				{
					std::size_t edges = 0;
					for (const auto& indices : geo->face_vertices)
					{
						edges += indices.size();
						triangles += indices.size() >= 3 ? indices.size() - 2 : 0;
					}

					g_checks++;
					if (geo->vertices.size() + static_cast<std::size_t>(geo->face_count_with_winding) != edges / 2 + 2 || volume(*geo) <= 0.0)
					{
						g_failed++;
						std::printf("FAILED brush %u of %s: V %zu F %d E %zu volume %g\n", child, map, geo->vertices.size(), geo->face_count_with_winding, edges / 2, volume(*geo));
					}
				}
			}
		}

		const auto t2 = std::chrono::steady_clock::now();

		CHECK(brushes == doc.brush_count() && brushes > 100);
		CHECK_MSG(valid == brushes, std::to_string(brushes - valid) + " invalid brushes");
		CHECK(closed == brushes);
		CHECK(world_box.valid() && world_box.maxs.x < WORLD_EXTENT && world_box.mins.x > -WORLD_EXTENT);

		// meshes of all brushes
		std::size_t mesh_vertices = 0, mesh_indices = 0;
		{
			mesh m;
			for (const object_id child : world->children)
			{
				if (doc.build_mesh(child, m))
				{
					mesh_vertices += m.vertices.size();
					mesh_indices += m.indices.size();
				}
			}
		}

		CHECK(mesh_indices > 0 && mesh_indices % 3 == 0);

		const auto t3 = std::chrono::steady_clock::now();

		// override export of the unedited import: the original entity string
		override_export exported;
		CHECK_MSG(doc.export_entity_override(exported, error), error);
		CHECK(exported.map_name == map);
		CHECK(exported.static_models == static_cast<int>(static_models));

		const std::string_view original = doc.import_original_ents();
		{
			std::string_view body = exported.text;
			for (int i = 0; i < 3; i++)
			{
				CHECK(body.starts_with("//"));
				body.remove_prefix(std::min(body.find('\n') + 1, body.size()));
			}

			CHECK(body.size() == original.size() + 1 && body.substr(0, original.size()) == original && body.back() == '\n');

			char crc[32];
			std::snprintf(crc, sizeof(crc), "// base_crc32 %08x\n", exported.base_crc);
			CHECK(exported.text.find(crc) != std::string::npos);
			CHECK(world->value("codol_base_crc32") == std::string_view(crc + 14, 8));
		}

		const auto t4 = std::chrono::steady_clock::now();

		// an edit shows up in the export, and only there; undo takes it back
		{
			object_id target = no_object;
			for (const object_id ent : doc.entities())
			{
				const entity* e = doc.find_entity(ent);
				if (e->is_point() && e->find("origin") && !e->find("codol_static") && e->classname().starts_with("mp_"))
				{
					target = ent;
					break;
				}
			}

			CHECK(target != no_object);

			if (target != no_object)
			{
				doc.select(target);
				CHECK(doc.translate({ 1000.5, 0, 0 }));
				CHECK(doc.set_key(target, "test_key", "hello"));

				override_export edited;
				CHECK(doc.export_entity_override(edited, error));
				CHECK(edited.text != exported.text);
				CHECK(edited.text.find("\"test_key\" \"hello\"") != std::string::npos);
				CHECK(edited.exported == exported.exported);

				// exactly one entity differs
				std::size_t differing = 0, pos_a = 0, pos_b = 0;
				while (pos_a < exported.text.size() && pos_b < edited.text.size())
				{
					const auto end_a = exported.text.find("\n}\n", pos_a), end_b = edited.text.find("\n}\n", pos_b);
					if (end_a == std::string::npos || end_b == std::string::npos)
					{
						break;
					}

					differing += exported.text.compare(pos_a, end_a - pos_a, edited.text, pos_b, end_b - pos_b) != 0 ? 1 : 0;
					pos_a = end_a + 3;
					pos_b = end_b + 3;
				}

				CHECK(differing == 1);

				CHECK(doc.undo() && doc.undo());
				override_export back;
				CHECK(doc.export_entity_override(back, error));
				CHECK(back.text == exported.text);
				CHECK(doc.save_map_text() == text);
			}
		}

		// world brushes: edit, undo, redo on the real thing
		{
			const auto ids = world_brushes(doc);
			const object_id a = ids[ids.size() / 3], b = ids[ids.size() / 2];

			const object_id pair[2] = { a, b };
			doc.select(pair);
			check_op(doc, "Move", [&] { return doc.translate({ 64, 0, 0 }); });
			check_op(doc, "Rotate", [&] { return doc.rotate(doc.selection_bounds().center(), { 0, 0, 1 }, 45); });
			check_op(doc, "Hollow", [&] { return doc.csg_hollow(1); }, true);
			check_op(doc, "Delete", [&] { return doc.delete_selection(); });

			while (doc.undo())
			{
			}

			CHECK(doc.save_map_text() == text);

			// a saved and reloaded map exports the same override once it has its baseline again
			document again;
			CHECK(again.load_map_text(text, error));
			CHECK(!again.has_import_baseline());
			again.set_import_baseline(std::string(doc.import_baseline_map()), std::string(doc.import_original_ents()));
			override_export second;
			CHECK(again.export_entity_override(second, error) && second.text == exported.text);
		}

		// picking: straight down through the middle of the map hits something
		{
			pick_result hit;
			const vec3 center = world_box.center();
			const auto p0 = std::chrono::steady_clock::now();
			const bool any = doc.pick({ { center.x, center.y, world_box.maxs.z + 100 }, { 0, 0, -1 } }, hit);
			const auto p1 = std::chrono::steady_clock::now();
			CHECK(any && hit.distance >= 100 && doc.kind(hit.object) != object_kind::none);

			std::printf("-- %s\n   %zu entities ( %zu point, %zu static models ), %zu brushes ( %zu in brush entities ): %zu valid, %zu closed; "
				"%zu faces, %zu with a winding, %zu corners, %zu triangles\n   mesh: %zu vertices, %zu indices; override export %d entities, crc %08x\n"
				"   import %lld ms, geometry %lld ms, meshes %lld ms, export %lld ms, pick %lld us; map text %.1f MB\n",
				map, doc.entities().size(), point_entities, static_models, brushes, entity_brushes, valid, closed, faces, faces_with_winding, vertices, triangles,
				mesh_vertices, mesh_indices, exported.exported, exported.base_crc,
				static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()),
				static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()),
				static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()),
				static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()),
				static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(p1 - p0).count()),
				static_cast<double>(text.size()) / 1048576.0);
		}
	}
}

// codol_edit_tests --check <file.map> [n]: load a map and describe the first n brushes that are not valid
static int check_map(const char* file, const int limit)
{
	document doc;
	std::string error;
	if (!doc.load_map_file(file, error))
	{
		std::printf("%s: %s\n", file, error.c_str());
		return 1;
	}

	std::size_t brushes = 0, invalid = 0;
	for (const object_id ent : doc.entities())
	{
		const auto& children = doc.find_entity(ent)->children;
		for (std::size_t c = 0; c < children.size(); c++)
		{
			const brush_geometry* geo = doc.geometry(children[c]);
			if (!geo)
			{
				continue;
			}

			brushes++;
			if (geo->valid)
			{
				continue;
			}

			if (static_cast<int>(invalid++) >= limit)
			{
				continue;
			}

			std::printf("entity %u brush %zu: %zu faces, %d with a winding, closed %d\n", ent, c, geo->planes.size(), geo->face_count_with_winding, geo->closed ? 1 : 0);
			for (std::size_t v = 0; v < geo->vertices.size(); v++)
			{
				std::printf("   v%zu  %.6f %.6f %.6f\n", v, geo->vertices[v].x, geo->vertices[v].y, geo->vertices[v].z);
			}

			std::vector<std::pair<int, int>> edges;
			for (std::size_t f = 0; f < geo->face_vertices.size(); f++)
			{
				const auto& indices = geo->face_vertices[f];
				std::printf("   f%zu  n %.6f %.6f %.6f d %.6f :", f, geo->planes[f].normal.x, geo->planes[f].normal.y, geo->planes[f].normal.z, geo->planes[f].dist);
				for (std::size_t k = 0; k < indices.size(); k++)
				{
					std::printf(" %d", indices[k]);
					edges.emplace_back(indices[k], indices[(k + 1) % indices.size()]);
				}

				std::printf("\n");
			}

			for (const auto& e : edges)
			{
				if (std::count(edges.begin(), edges.end(), std::pair<int, int>(e.second, e.first)) != 1 || std::count(edges.begin(), edges.end(), e) != 1)
				{
					std::printf("   open edge %d -> %d\n", e.first, e.second);
				}
			}
		}
	}

	std::printf("%s: %zu brushes, %zu not valid\n", file, brushes, invalid);
	return 0;
}

int main(const int argc, char** argv)
{
	if (argc >= 3 && std::string_view(argv[1]) == "--check")
	{
		return check_map(argv[2], argc >= 4 ? std::atoi(argv[3]) : 5);
	}

	// codol_edit_tests --canonical <in.map> <out.map>: rewrite every object ( to compare with what radiant wrote )
	if (argc >= 4 && std::string_view(argv[1]) == "--canonical")
	{
		document doc;
		std::string error;
		if (!doc.load_map_file(argv[2], error))
		{
			std::printf("%s: %s\n", argv[2], error.c_str());
			return 1;
		}

		std::ofstream out(argv[3], std::ios::binary | std::ios::trunc);
		const std::string text = doc.save_map_text_canonical();
		out.write(text.data(), static_cast<std::streamsize>(text.size()));
		return 0;
	}

	// codol_edit_tests --import <map.ff> <client exe> <out.map> [out.d3dbsp.ents]: what document::import_fastfile gives
	// ( to compare with codol_mapconv --ff ), and the override export of the unedited import
	if (argc >= 5 && std::string_view(argv[1]) == "--import")
	{
		document doc;
		std::string error;
		if (!doc.import_fastfile(argv[2], argv[3], error) || !doc.save_map_file(argv[4], error))
		{
			std::printf("%s: %s\n", argv[2], error.c_str());
			return 1;
		}

		if (argc >= 6)
		{
			override_export exported;
			if (!doc.export_entity_override(exported, error))
			{
				std::printf("%s: %s\n", argv[2], error.c_str());
				return 1;
			}

			std::ofstream out(argv[5], std::ios::binary | std::ios::trunc);
			out.write(exported.text.data(), static_cast<std::streamsize>(exported.text.size()));
		}

		std::printf("%s: %zu entities, %zu brushes\n", argv[2], doc.entities().size(), doc.brush_count());
		return 0;
	}

	std::vector<std::string> extra;
	for (int i = 1; i < argc; i++)
	{
		extra.emplace_back(argv[i]);
	}

	test_reader_writer();
	test_geometry();
	test_operations();
	test_regroup_keeps_text();
	test_picking();
	test_class_catalog();
	test_round_trip(extra);
	test_saved_import("mp_rust");
	test_saved_import("mp_killhouse_sh");
	test_import("mp_rust");
	test_import("mp_killhouse_sh");
	g_failed += test_texture_tools(g_checks);		// texture_tools_tests.cpp

	std::printf("\ncodol_edit_tests: %d checks, %d failed, %d skipped ( missing test data )\n", g_checks, g_failed, g_skipped);
	return g_failed == 0 ? 0 : 1;
}
