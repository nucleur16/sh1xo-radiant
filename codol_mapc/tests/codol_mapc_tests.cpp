// codol_mapc_tests: acceptance tests of the map compiler against the 1.1.1.13 install.
//
//   codol_mapc_tests [--game <install>] [--out <folder>] [--maps a,b] [--only identity|entities|models|brushes|all]
//
// For every map ( mp_rust, mp_killhouse_sh ) and stage an edited map is made with codol_edit from the import of the
// shipped fast file, compiled, and the result is checked by reading it back:
//   - with codol_zone: the expected contents, and that every asset the edit does not concern has the fingerprint
//     it has in the base zone;
//   - with document::import_fastfile: the import of the compiled zone equals the edited document;
//   - for collision, with the trace of codol_mapc/trace.hpp ( this library's own walk of the clip map, not the
//     game's code ).
// The edited maps, the compiled fast files and a views.txt with camera arguments for codol_viewtool are left in
// <out>/<map>/<stage>/ ; tests/run_e2e.ps1 renders them.

#include "codol_mapc/mapc.hpp"
#include "codol_mapc/trace.hpp"
#include "codol_zone/assets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>

using namespace codol;
namespace c1 = codol::c1;

static int g_failed = 0;
static int g_checks = 0;

#define CHECK(expr) do { g_checks++; if (!(expr)) { g_failed++; std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #expr); } } while (0)
#define CHECK_MSG(expr, msg) do { g_checks++; if (!(expr)) { g_failed++; std::printf("FAILED %s:%d  %s  ( %s )\n", __FILE__, __LINE__, #expr, std::string(msg).c_str()); } } while (0)

namespace
{
	struct fixture
	{
		std::string map;
		std::filesystem::path fastfile, exe, out;
		zone::ClientKey key;
		std::vector<std::uint8_t> base_zone;
		std::string baseline_text;
		std::map<std::string, std::uint64_t> fingerprints;		// "type:name" -> fingerprint in the base zone
	};

	std::string asset_key(const zone::EditorAsset& a)
	{
		return zone::asset_type_name(a.type) + ":" + a.name;
	}

	// What an asset holds, by content: its bytes and, for every zone pointer, what it leads to. Unlike
	// ZoneEditor::asset_fingerprint a pointer into *another* asset counts by that asset and the offset in the data
	// it points at, not by the position of that data in the other asset's stream order - which changes when the
	// other asset gains or loses a piece of data, although nothing changed for the asset that points there.
	std::uint64_t content_fingerprint(const zone::ZoneEditor& editor, const std::size_t asset)
	{
		std::uint64_t hash = 0xCBF29CE484222325ull;
		const auto bytes = [&](const void* data, const std::size_t size)
		{
			const auto* p = static_cast<const std::uint8_t*>(data);
			for (std::size_t k = 0; k < size; k++)
			{
				hash = (hash ^ p[k]) * 0x100000001B3ull;
			}
		};
		const auto number = [&](const std::uint64_t value) { bytes(&value, sizeof(value)); };
		const auto name_of = [&](const std::size_t a)
		{
			const std::string key = a == zone::ZoneEditor::npos ? std::string("root") : asset_key(editor.asset(a));
			bytes(key.data(), key.size());
		};
		const zone::EditorAsset info = editor.asset(asset);
		name_of(asset);
		number(info.chunks);
		std::vector<std::uint8_t> masked;
		for (std::uint32_t k = 0; k < info.chunks; k++)
		{
			const zone::ZoneDataId id = editor.asset_data(asset, k);
			const auto data = editor.data_bytes(id);
			const auto pointers = editor.data_pointers(id);
			number(data.size());
			if (pointers.empty())
			{
				bytes(data.data(), data.size());
				continue;
			}
			masked.assign(data.begin(), data.end());
			for (const zone::ZonePointer& p : pointers)
			{
				std::memset(masked.data() + p.offset, 0, 4);
				number(p.offset);
				number(static_cast<std::uint64_t>(p.target.kind));
				number(p.target.offset);
				switch (p.target.kind)
				{
				case zone::ZoneTarget::Kind::data:
				{
					zone::ZoneDataInfo target;
					editor.data_info(p.target.data, target);
					if (target.asset == asset)
					{
						number(target.index);
					}
					else
					{
						name_of(target.asset);
					}
					break;
				}
				case zone::ZoneTarget::Kind::asset:
					name_of(p.target.asset);
					break;
				case zone::ZoneTarget::Kind::allocation:
					name_of(p.target.asset);
					break;
				default:
					number(p.target.raw);
					break;
				}
			}
			bytes(masked.data(), masked.size());
		}
		return hash;
	}

	struct static_model
	{
		edit::object_id id = 0;
		std::string model;
		edit::vec3 origin, angles;
		double scale = 1.0;
	};

	std::vector<static_model> static_models(const edit::document& doc)
	{
		std::vector<static_model> out;
		for (const edit::object_id id : doc.entities())
		{
			const edit::entity* e = doc.find_entity(id);
			if (e && e->classname() == "misc_model" && e->find("codol_static"))
			{
				static_model m;
				m.id = id;
				m.model = std::string(e->value("model"));
				m.origin = e->origin();
				m.angles = e->angles();
				edit::vec3 scale;
				if (const std::string* text = e->find("modelscale"); text && edit::parse_vec3(*text, scale) && scale.x > 0)
				{
					m.scale = scale.x;
				}
				out.push_back(std::move(m));
			}
		}
		return out;
	}

	// two placements are the same instance: model, origin, orientation ( as axes: angles are not unique ), scale
	bool same_placement(const static_model& a, const static_model& b)
	{
		if (a.model != b.model || std::fabs(a.scale - b.scale) > 2e-3 * std::max(1.0, a.scale))
		{
			return false;
		}
		for (int k = 0; k < 3; k++)
		{
			if (std::fabs(a.origin[k] - b.origin[k]) > 0.02)
			{
				return false;
			}
		}
		edit::vec3 x[3], y[3];
		edit::angles_to_axes(a.angles, x);
		edit::angles_to_axes(b.angles, y);
		for (int i = 0; i < 3; i++)
		{
			for (int k = 0; k < 3; k++)
			{
				if (std::fabs(x[i][k] - y[i][k]) > 2e-3)
				{
					return false;
				}
			}
		}
		return true;
	}

	// number of placements of `a` that have no partner in `b`
	int unmatched(const std::vector<static_model>& a, const std::vector<static_model>& b)
	{
		std::multimap<std::string, const static_model*> by_model;
		for (const static_model& m : b)
		{
			by_model.emplace(m.model, &m);
		}
		int missing = 0;
		for (const static_model& m : a)
		{
			bool found = false;
			const auto range = by_model.equal_range(m.model);
			for (auto it = range.first; it != range.second; ++it)
			{
				if (same_placement(m, *it->second))
				{
					by_model.erase(it);
					found = true;
					break;
				}
			}
			missing += !found;
		}
		return missing;
	}

	// brushes of every entity that has some, as sorted plane lists ( normal, dist, material ), by sub model
	std::map<int, std::multiset<std::string>> brush_sets(const edit::document& doc)
	{
		std::map<int, std::multiset<std::string>> out;
		bool first = true;
		for (const edit::object_id id : doc.entities())
		{
			const edit::entity* e = doc.find_entity(id);
			if (!e)
			{
				continue;
			}
			int sub_model = first ? 0 : -1;
			first = false;
			if (const std::string_view model = e->value("model"); sub_model < 0 && model.size() > 1 && model[0] == '*')
			{
				sub_model = std::atoi(std::string(model.substr(1)).c_str());
			}
			for (const edit::object_id child : e->children)
			{
				const edit::brush* b = doc.find_brush(child);
				const edit::brush_geometry* geo = b ? doc.geometry(child) : nullptr;
				if (!b || b->opaque || !geo)
				{
					continue;
				}
				std::vector<std::string> planes;
				for (std::size_t f = 0; f < b->faces.size() && f < geo->planes.size(); f++)
				{
					if (geo->face_vertices[f].empty())
					{
						continue;	// a plane without a winding does not bound the brush
					}
					const edit::plane& p = geo->planes[f];
					char text[256];
					std::snprintf(text, sizeof(text), "%.2f %.2f %.2f %.1f %s", p.normal.x + 0.0, p.normal.y + 0.0, p.normal.z + 0.0, p.dist + 0.0, b->faces[f].texture.material.c_str());
					std::string s = text;
					// -0.00 and 0.00 are one number
					for (std::size_t at = s.find("-0.00"); at != std::string::npos; at = s.find("-0.00", at))
					{
						s.erase(at, 1);
					}
					if (s.find("-0.0 ") != std::string::npos)
					{
						s.erase(s.find("-0.0 "), 1);
					}
					planes.push_back(std::move(s));
				}
				std::sort(planes.begin(), planes.end());
				std::string key;
				for (const std::string& p : planes)
				{
					key += p + "|";
				}
				out[sub_model].insert(std::move(key));
			}
		}
		return out;
	}

	bool load_fixture(fixture& f, const std::filesystem::path& game, const std::filesystem::path& out, const std::string& map)
	{
		f.map = map;
		f.exe = game / "C1.exe";
		f.fastfile = game / "client_shipRetail" / "chinese" / (map + ".ff");
		f.out = out / map;
		std::error_code ec;
		std::filesystem::create_directories(f.out, ec);
		if (!std::filesystem::exists(f.fastfile, ec) || !f.key.load_from_exe(f.exe))
		{
			return false;
		}
		if (const zone::Status s = zone::read_fastfile(f.fastfile, f.key, f.base_zone); !s)
		{
			std::printf("%s\n", s.message.c_str());
			return false;
		}
		edit::document doc;
		std::string error;
		if (!doc.import_fastfile(f.fastfile, f.exe, error))
		{
			std::printf("%s\n", error.c_str());
			return false;
		}
		f.baseline_text = std::string(doc.import_baseline_map());
		zone::ZoneEditor editor;
		std::vector<std::uint8_t> copy = f.base_zone;
		if (!editor.load(std::move(copy), map))
		{
			return false;
		}
		for (std::size_t i = 0; i < editor.asset_count(); i++)
		{
			f.fingerprints[asset_key(editor.asset(i))] = content_fingerprint(editor, i);
		}
		return true;
	}

	// result of one compile, read back
	struct compiled
	{
		bool ok = false;
		mapc::report report;
		zone::ZoneEditor editor;			// holds the compiled zone
		std::vector<std::uint8_t> zone_bytes;
		std::filesystem::path folder, fastfile;
		edit::document imported;			// import of the compiled fast file
		bool imported_ok = false;
		std::set<std::string> changed;		// assets whose fingerprint differs from the base zone, "+" in front of new ones
	};

	void compile_case(const fixture& f, const std::string& stage, const edit::document& edited, compiled& out, const bool reimport = true)
	{
		out.folder = f.out / stage;
		std::error_code ec;
		std::filesystem::create_directories(out.folder, ec);
		std::string error;
		// what the command line tool gets: the edited map and the baseline next to it
		CHECK_MSG(edited.save_map_file(out.folder / "edited.map", error), error);
		std::ofstream(out.folder / "edited.codol_base.map", std::ios::binary) << f.baseline_text;

		edit::document baseline;
		CHECK_MSG(baseline.load_map_text(f.baseline_text, error), error);
		std::vector<std::uint8_t> copy = f.base_zone;
		CHECK(out.editor.load(std::move(copy), f.map));
		mapc::options opts;
		out.ok = mapc::compile_zone(out.editor, baseline, edited, opts, f.key, out.zone_bytes, out.report, error);
		CHECK_MSG(out.ok, error);
		if (!out.ok)
		{
			return;
		}
		std::ofstream(out.folder / "report.txt", std::ios::binary) << out.report.text();
		for (std::size_t i = 0; i < out.editor.asset_count(); i++)
		{
			const std::string key = asset_key(out.editor.asset(i));
			const auto it = f.fingerprints.find(key);
			if (it == f.fingerprints.end())
			{
				out.changed.insert("+" + key);
			}
			else if (it->second != content_fingerprint(out.editor, i))
			{
				out.changed.insert(key);
			}
		}
		if (reimport)
		{
			out.fastfile = out.folder / (f.map + ".ff");
			const zone::Status s = zone::write_fastfile(out.fastfile, out.zone_bytes, f.key);
			CHECK_MSG(static_cast<bool>(s), s.message);
			out.imported_ok = out.imported.import_fastfile(out.fastfile, f.exe, error);
			CHECK_MSG(out.imported_ok, error);
		}
	}

	std::string camera_line(const std::string& what, const edit::vec3& target, const edit::vec3& from)
	{
		const edit::vec3 d = target - from;
		const double yaw = std::atan2(d.y, d.x) * 180.0 / 3.14159265358979, pitch = std::atan2(-d.z, std::sqrt(d.x * d.x + d.y * d.y)) * 180.0 / 3.14159265358979;
		char text[512];
		std::snprintf(text, sizeof(text), "%s --pos %.0f,%.0f,%.0f --angles %.1f,%.1f\n", what.c_str(), from.x, from.y, from.z, pitch, yaw);
		return text;
	}

	// a place players stand at: the first spawn point of the map
	edit::vec3 spawn_origin(const edit::document& doc)
	{
		for (const edit::object_id id : doc.entities())
		{
			const edit::entity* e = doc.find_entity(id);
			if (e && e->classname().find("spawn") != std::string_view::npos && e->find("origin"))
			{
				return e->origin();
			}
		}
		return {};
	}

	// ---- an unedited map compiles to the base zone ------------------------------------------------------------

	void test_identity(const fixture& f)
	{
		std::printf("  identity\n");
		edit::document edited;
		std::string error;
		CHECK_MSG(edited.load_map_text(f.baseline_text, error), error);
		compiled c;
		compile_case(f, "identity", edited, c, false);
		CHECK(c.ok && c.report.identical);
		CHECK(c.report.diff.empty());
		CHECK(c.zone_bytes == f.base_zone);
		CHECK(c.changed.empty());
	}

	// ---- stage 1 ------------------------------------------------------------------------------------------------

	void test_entities(const fixture& f)
	{
		std::printf("  entities\n");
		edit::document edited;
		std::string error;
		CHECK_MSG(edited.load_map_text(f.baseline_text, error), error);

		// edit a key of one entity, delete another, add a new one
		std::vector<edit::object_id> points;
		for (const edit::object_id id : edited.entities())
		{
			const edit::entity* e = edited.find_entity(id);
			if (e && !e->is_worldspawn() && e->is_point() && !e->find("codol_static") && e->classname().find("spawn") != std::string_view::npos)
			{
				points.push_back(id);
			}
		}
		CHECK(points.size() >= 3);
		if (points.size() < 3)
		{
			return;
		}
		const edit::vec3 at = edited.find_entity(points[0])->origin();
		const std::string deleted_origin(edited.find_entity(points[1])->value("origin"));
		const std::string deleted_class(edited.find_entity(points[1])->classname());
		CHECK(edited.set_key(points[0], "origin", edit::format_key_vec3(at + edit::vec3(16, 8, 4))));
		CHECK(edited.set_key(points[0], "codol_mapc_test", "edited"));	// editor only keys do not reach the zone
		CHECK(edited.set_key(points[2], "script_noteworthy", "mapc_test"));
		edited.select(points[1]);
		CHECK(edited.delete_selection());
		const std::pair<std::string, std::string> pairs[] = { { "targetname", "mapc_added" }, { "angles", "0 90 0" } };
		CHECK(edited.create_point_entity("script_origin", at + edit::vec3(0, 0, 64), pairs) != edit::no_object);

		compiled c;
		compile_case(f, "entities", edited, c);
		if (!c.ok)
		{
			return;
		}
		CHECK(c.report.entities_changed && !c.report.identical);
		CHECK(c.report.entities_after == c.report.entities_before);		// one deleted, one added

		// (a) the zone holds the entity string the document exports; nothing else changed
		zone::MapEntsView ents;
		CHECK(zone::read_map_ents(c.editor.zone(), zone::find_map_ents(c.editor.zone()), ents));
		edit::document baseline;
		CHECK(baseline.load_map_text(f.baseline_text, error));
		zone::Zone base;
		{
			std::vector<std::uint8_t> copy = f.base_zone;
			CHECK(base.load(std::move(copy), f.map));
		}
		zone::MapEntsView base_ents;
		CHECK(zone::read_map_ents(base, zone::find_map_ents(base), base_ents));
		std::string expected;
		std::vector<std::string> warnings;
		CHECK_MSG(mapc::entity_string_of(baseline, edited, base_ents.entityString, expected, warnings, error), error);
		CHECK(std::string(ents.entityString) == expected);
		CHECK(expected.find("\"mapc_added\"") != std::string::npos);
		CHECK(expected.find("\"script_noteworthy\" \"mapc_test\"") != std::string::npos);
		CHECK(expected.find("codol_") == std::string::npos);
		CHECK(expected.find("\"origin\" \"" + edit::format_key_vec3(at + edit::vec3(16, 8, 4)) + "\"") != std::string::npos);
		// the deleted entity: its class with its origin is gone ( when it was the only one there )
		const std::string gone = "\"origin\" \"" + deleted_origin + "\"";
		std::size_t before = 0, after = 0;
		for (std::size_t p = base_ents.entityString.find(gone); p != std::string_view::npos; p = base_ents.entityString.find(gone, p + 1)) { before++; }
		for (std::size_t p = expected.find(gone); p != std::string::npos; p = expected.find(gone, p + 1)) { after++; }
		CHECK(after + 1 == before);
		(void)deleted_class;
		// the MapEnts is nested in the clip map: that asset and no other differs
		CHECK(c.changed.size() == 1 && c.changed.begin()->rfind("col_map_mp:", 0) == 0);

		// (b) the import of the compiled zone has the entities of the edited document; geometry untouched
		if (c.imported_ok)
		{
			std::string again;
			zone::Zone z;
			std::vector<std::uint8_t> copy = c.zone_bytes;
			CHECK(z.load(std::move(copy), f.map));
			CHECK(c.imported.import_original_ents() == expected);
			CHECK(c.imported.brush_count() == baseline.brush_count());
			CHECK(static_models(c.imported).size() == static_models(baseline).size());
			CHECK(unmatched(static_models(baseline), static_models(c.imported)) == 0);
		}
	}

	// ---- stage 2 ------------------------------------------------------------------------------------------------

	void test_models(const fixture& f)
	{
		std::printf("  static models\n");
		edit::document edited;
		std::string error;
		CHECK_MSG(edited.load_map_text(f.baseline_text, error), error);
		const std::vector<static_model> base_models = static_models(edited);
		CHECK(base_models.size() > 100);
		const edit::vec3 spawn = spawn_origin(edited);

		// the static models nearest to a spawn point, so that one camera sees every edit
		std::vector<std::size_t> near(base_models.size());
		for (std::size_t i = 0; i < near.size(); i++)
		{
			near[i] = i;
		}
		const auto distance = [&](const std::size_t i) { const edit::vec3 d = base_models[i].origin - spawn; return edit::dot(d, d); };
		std::sort(near.begin(), near.end(), [&](const std::size_t a, const std::size_t b) { return distance(a) < distance(b); });

		std::string views;
		// move three: one sideways and up, one turned, one scaled
		const static_model& m0 = base_models[near[0]];
		const static_model& m1 = base_models[near[1]];
		const static_model& m2 = base_models[near[2]];
		CHECK(edited.set_key(m0.id, "origin", edit::format_key_vec3(m0.origin + edit::vec3(48, 0, 40))));
		CHECK(edited.set_key(m1.id, "angles", edit::format_key_vec3(edit::vec3(m1.angles.x, m1.angles.y + 90.0, m1.angles.z))));
		CHECK(edited.set_key(m1.id, "origin", edit::format_key_vec3(m1.origin + edit::vec3(0, 0, 56))));
		CHECK(edited.set_key(m2.id, "modelscale", edit::format_key_number(m2.scale * 2.0)));
		// delete two near the spawn, and the very first instance of the map ( it carries its XModel )
		edited.select(base_models[near[3]].id);
		edited.select(base_models[near[4]].id, edit::select_mode::add);
		edited.select(base_models[0].id, edit::select_mode::add);
		CHECK(edited.delete_selection());
		// add one: the model of the nearest instance, above the spawn point
		const edit::vec3 added_at = spawn + edit::vec3(0, 0, 150);
		const std::pair<std::string, std::string> pairs[] = { { "model", m0.model }, { "angles", "0 45 0" }, { "codol_static", "1" } };
		CHECK(edited.create_point_entity("misc_model", added_at, pairs) != edit::no_object);

		views += camera_line("spawn", spawn + edit::vec3(0, 0, 60), spawn + edit::vec3(-420, -420, 380));
		views += camera_line("spawn2", spawn + edit::vec3(0, 0, 60), spawn + edit::vec3(380, -300, 260));
		views += "# moved up/aside: " + m0.model + " at " + edit::format_key_vec3(m0.origin) + "\n";
		views += "# turned 90, raised 56: " + m1.model + " at " + edit::format_key_vec3(m1.origin) + "\n";
		views += "# scaled x2: " + m2.model + " at " + edit::format_key_vec3(m2.origin) + "\n";
		views += "# deleted: " + base_models[near[3]].model + " at " + edit::format_key_vec3(base_models[near[3]].origin) + ", " + base_models[near[4]].model + " at "
			+ edit::format_key_vec3(base_models[near[4]].origin) + "\n";
		views += "# added: " + m0.model + " at " + edit::format_key_vec3(added_at) + "\n";

		compiled c;
		compile_case(f, "models", edited, c);
		std::ofstream(f.out / "models" / "views.txt", std::ios::binary) << views;
		if (!c.ok)
		{
			return;
		}
		const mapc::report& r = c.report;
		CHECK(r.diff.models_moved.size() == 3 && r.diff.models_deleted.size() == 3 && r.diff.models_added.size() == 1);
		CHECK(r.models_moved == 3 && r.models_added == 1 && r.models_deleted + r.models_hidden == 3);
		CHECK(!r.entities_changed);

		// (a) the zone
		zone::GfxWorldView gfx;
		zone::ClipMapView clip;
		const zone::Zone& z = c.editor.zone();
		CHECK(zone::read_gfx_world(z, zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD), gfx));
		CHECK(zone::read_clip_map(z, zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP), clip));
		CHECK(gfx.staticModels.size() == base_models.size() - 3 + 1 + static_cast<std::size_t>(r.models_hidden));
		CHECK(gfx.raw.dpvs.smodelCount == gfx.staticModels.size());
		for (const auto& sm : gfx.staticModels)
		{
			CHECK(!sm.modelName.empty());
			if (sm.modelName.empty())
			{
				break;
			}
		}
		// only the two map assets differ
		for (const std::string& key : c.changed)
		{
			CHECK_MSG(key.rfind("gfx_map:", 0) == 0 || key.rfind("col_map_mp:", 0) == 0, key);
		}
		CHECK(c.changed.size() <= 2 && !c.changed.empty());
		// every instance is listed by a cell and by the shadow geometry of its light, with valid indices
		{
			const std::uint32_t count = gfx.raw.dpvs.smodelCount;
			std::vector<int> in_cell(count, 0), in_light(count, 0);
			const std::uint32_t world = zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD);
			const std::uint32_t tree_counts = z.get<std::uint32_t>(world + 68), trees = z.get<std::uint32_t>(world + 72);
			bool valid = true;
			for (int cell = 0; cell < gfx.raw.cellCount; cell++)
			{
				const std::uint32_t tree = z.get<std::uint32_t>(trees + 4u * static_cast<std::uint32_t>(cell));
				const int nodes = z.get<std::int32_t>(tree_counts + 4u * static_cast<std::uint32_t>(cell));
				if (nodes <= 0)
				{
					continue;
				}
				const std::uint16_t root_count = z.get<std::uint16_t>(tree + 34);
				const std::uint32_t root_list = z.get<std::uint32_t>(tree + 36);
				for (const std::uint16_t m : z.get_array<std::uint16_t>(root_list, root_count))
				{
					valid = valid && m < count;
					if (m < count)
					{
						in_cell[m]++;
					}
				}
				// every node's list is a run of the root's list
				for (int n = 1; n < nodes; n++)
				{
					const std::uint16_t node_count = z.get<std::uint16_t>(tree + 44u * static_cast<std::uint32_t>(n) + 34);
					const std::uint32_t node_list = z.get<std::uint32_t>(tree + 44u * static_cast<std::uint32_t>(n) + 36);
					valid = valid && (node_count == 0 || (node_list >= root_list && node_list + 2u * node_count <= root_list + 2u * root_count));
				}
			}
			CHECK(valid);
			const std::uint32_t shadow = z.get<std::uint32_t>(world + 0x1C0);
			for (std::uint32_t l = 0; l < gfx.raw.primaryLightCount; l++)
			{
				const std::uint16_t models = z.get<std::uint16_t>(shadow + 12 * l + 2);
				for (const std::uint16_t m : z.get_array<std::uint16_t>(z.get<std::uint32_t>(shadow + 12 * l + 8), models))
				{
					CHECK(m < count && gfx.staticModels[m].raw.primaryLightIndex == l);
					if (m < count)
					{
						in_light[m]++;
					}
				}
			}
			int unlisted_cell = 0, unlisted_light = 0;
			for (std::uint32_t m = 0; m < count; m++)
			{
				unlisted_cell += in_cell[m] == 0;
				unlisted_light += in_light[m] != 1;
			}
			// the base zones have at most one instance that no cell lists
			CHECK_MSG(unlisted_cell <= 1 + r.models_hidden, std::to_string(unlisted_cell));
			CHECK_MSG(unlisted_light == r.models_hidden, std::to_string(unlisted_light));
		}
		// the edited instances as the zone has them
		const auto find_instance = [&](const std::string& model, const edit::vec3& origin) -> const zone::WorldStaticModel*
		{
			for (const auto& sm : gfx.staticModels)
			{
				if (sm.modelName == model && std::fabs(sm.raw.origin[0] - origin.x) < 0.02 && std::fabs(sm.raw.origin[1] - origin.y) < 0.02 && std::fabs(sm.raw.origin[2] - origin.z) < 0.02)
				{
					return &sm;
				}
			}
			return nullptr;
		};
		const auto find_clip = [&](const std::string& model, const edit::vec3& origin) -> const zone::ClipStaticModel*
		{
			for (const auto& sm : clip.staticModels)
			{
				if (sm.modelName == model && std::fabs(sm.raw.origin[0] - origin.x) < 0.02 && std::fabs(sm.raw.origin[1] - origin.y) < 0.02 && std::fabs(sm.raw.origin[2] - origin.z) < 0.02)
				{
					return &sm;
				}
			}
			return nullptr;
		};
		CHECK(find_instance(m0.model, m0.origin + edit::vec3(48, 0, 40)) && !find_instance(m0.model, m0.origin));
		CHECK(find_instance(m1.model, m1.origin + edit::vec3(0, 0, 56)));
		if (const auto* scaled = find_instance(m2.model, m2.origin))
		{
			CHECK(std::fabs(scaled->raw.scale - m2.scale * 2.0) < 1e-3);
		}
		else
		{
			CHECK(false);
		}
		CHECK(!find_instance(base_models[near[3]].model, base_models[near[3]].origin) || unmatched({ base_models[near[3]] }, static_models(edited)) == 0);
		CHECK(find_instance(m0.model, added_at));
		// collision follows: a model that had an entry at the old place has one at the new place
		{
			zone::Zone base;
			std::vector<std::uint8_t> copy = f.base_zone;
			CHECK(base.load(std::move(copy), f.map));
			zone::ClipMapView base_clip;
			CHECK(zone::read_clip_map(base, zone::first_asset(base, c1::ASSET_TYPE_CLIPMAP_MP), base_clip));
			const auto had = [&](const static_model& m)
			{
				for (const auto& sm : base_clip.staticModels)
				{
					if (sm.modelName == m.model && std::fabs(sm.raw.origin[0] - m.origin.x) < 0.02 && std::fabs(sm.raw.origin[1] - m.origin.y) < 0.02
						&& std::fabs(sm.raw.origin[2] - m.origin.z) < 0.02)
					{
						return true;
					}
				}
				return false;
			};
			if (had(m0))
			{
				CHECK(find_clip(m0.model, m0.origin + edit::vec3(48, 0, 40)) && !find_clip(m0.model, m0.origin));
				CHECK(find_clip(m0.model, added_at));
			}
			if (had(base_models[near[3]]))
			{
				CHECK(!find_clip(base_models[near[3]].model, base_models[near[3]].origin));
			}
			const int expected = static_cast<int>(base_clip.staticModels.size()) - r.clip_models_deleted + r.clip_models_added;
			CHECK(static_cast<int>(clip.staticModels.size()) == expected);
			std::printf("    collision entries: %zu -> %zu ( %d moved, %d deleted, %d added )\n", base_clip.staticModels.size(), clip.staticModels.size(), r.clip_models_moved,
				r.clip_models_deleted, r.clip_models_added);
			// the AABB tree names every entry exactly once and its boxes hold what is below them
			const std::uint32_t node_count = clip.raw.smodelNodeCount & 0xFFFF, model_count = clip.raw.numStaticModels;
			std::vector<int> named(model_count, 0);
			bool shape = true, boxes = true;
			for (std::uint32_t n = 0; n < node_count; n++)
			{
				const std::uint32_t node = clip.raw.smodelNodes + 28 * n;
				const c1::Bounds b = z.get<c1::Bounds>(node);
				const std::uint16_t first = z.get<std::uint16_t>(node + 24), children = z.get<std::uint16_t>(node + 26);
				for (std::uint32_t k = 0; k < children; k++)
				{
					c1::Bounds below;
					if (first >= model_count)
					{
						shape = shape && first - model_count + k < node_count;
						below = z.get<c1::Bounds>(clip.raw.smodelNodes + 28 * (first - model_count + k));
					}
					else
					{
						shape = shape && first + k < model_count;
						if (first + k < model_count)
						{
							named[first + k]++;
							below = clip.staticModels[first + k].raw.absBounds;
						}
					}
					for (int a = 0; a < 3; a++)
					{
						boxes = boxes && below.midPoint[a] - below.halfSize[a] >= b.midPoint[a] - b.halfSize[a] - 0.05f
							&& below.midPoint[a] + below.halfSize[a] <= b.midPoint[a] + b.halfSize[a] + 0.05f;
					}
				}
			}
			CHECK(shape);
			CHECK(boxes);
			CHECK(std::all_of(named.begin(), named.end(), [](const int n) { return n == 1; }));
		}

		// (b) the import of the compiled zone is the edited document
		if (c.imported_ok)
		{
			const auto want = static_models(edited), have = static_models(c.imported);
			CHECK(have.size() == want.size() + static_cast<std::size_t>(r.models_hidden));
			CHECK_MSG(unmatched(want, have) == 0, std::to_string(unmatched(want, have)));
			CHECK_MSG(unmatched(have, want) == r.models_hidden, std::to_string(unmatched(have, want)));
			CHECK(c.imported.brush_count() == edited.brush_count());
			CHECK(brush_sets(c.imported) == brush_sets(edited));
		}
		std::printf("    %s", r.text().c_str());
	}
}

int main(const int argc, char** argv)
{
	std::filesystem::path game = CODOL_MAPC_GAME_DIR, out = "mapc_test";
	std::string maps = "mp_rust,mp_killhouse_sh", only = "all";
	for (int i = 1; i + 1 < argc; i += 2)
	{
		const std::string arg = argv[i];
		if (arg == "--game") { game = argv[i + 1]; }
		else if (arg == "--out") { out = argv[i + 1]; }
		else if (arg == "--maps") { maps = argv[i + 1]; }
		else if (arg == "--only") { only = argv[i + 1]; }
	}
	std::error_code ec;
	if (!std::filesystem::exists(game / "C1.exe", ec))
	{
		std::printf("skipped: no install at %s\n", game.string().c_str());
		return 0;
	}
	std::size_t at = 0;
	while (at <= maps.size())
	{
		std::size_t end = maps.find(',', at);
		end = end == std::string::npos ? maps.size() : end;
		const std::string map = maps.substr(at, end - at);
		at = end + 1;
		if (map.empty())
		{
			continue;
		}
		std::printf("%s\n", map.c_str());
		fixture f;
		CHECK(load_fixture(f, game, out, map));
		if (f.baseline_text.empty())
		{
			continue;
		}
		if (only == "all" || only == "identity") { test_identity(f); }
		if (only == "all" || only == "entities") { test_entities(f); }
		if (only == "all" || only == "models") { test_models(f); }
	}
	std::printf("%d checks, %d failed\n", g_checks, g_failed);
	return g_failed ? 1 : 0;
}
