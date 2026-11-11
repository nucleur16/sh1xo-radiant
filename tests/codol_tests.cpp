// Unit tests for the engine independent CoDOL script code.
// Usage: codol_tests [path-to-a-rawfiles-tree]
// With a path, the whole tree is indexed as a smoke test and a summary is printed.

#include "std_include.hpp"

#include <chrono>
#include <cstdio>

using namespace codol::script;

static int g_failed = 0;
static int g_checks = 0;

#define CHECK(expr) do { g_checks++; if (!(expr)) { g_failed++; std::printf("FAILED %s:%d  %s\n", __FILE__, __LINE__, #expr); } } while (0)

static std::vector<span> lex(lang l, std::string_view line, std::uint8_t* state_io = nullptr)
{
	std::uint8_t state = state_io ? *state_io : static_cast<std::uint8_t>(LS_NONE);
	std::vector<span> out;
	lex_line(l, line, state, out);

	if (state_io)
	{
		*state_io = state;
	}

	return out;
}

static bool has(const std::vector<span>& spans, std::string_view line, tok kind, std::string_view text)
{
	for (const auto& s : spans)
	{
		if (s.kind == kind && line.substr(s.start, s.len) == text)
		{
			return true;
		}
	}

	return false;
}

static void test_lexer()
{
	{
		const std::string_view line = "\tlevel.scr_anim[ \"foley\" ][ \"intro\" ] = %training_intro; // hi";
		const auto s = lex(lang::gsc, line);
		CHECK(has(s, line, tok::builtin, "level"));
		CHECK(has(s, line, tok::string, "\"foley\""));
		CHECK(has(s, line, tok::anim, "%training_intro"));
		CHECK(has(s, line, tok::comment, "// hi"));
	}

	{
		// modulo is not an anim reference
		const std::string_view line = "x = a %b; y = ( c ) %d; z = 10 %e; return %f;";
		const auto s = lex(lang::gsc, line);
		CHECK(!has(s, line, tok::anim, "%b"));
		CHECK(!has(s, line, tok::anim, "%d"));
		CHECK(!has(s, line, tok::anim, "%e"));
		CHECK(has(s, line, tok::anim, "%f"));
	}

	{
		const std::string_view line = "#using_animtree( \"generic_human\" ); self thread maps\\mp\\_utility::foo( ::bar, &\"MP_STR\" );";
		const auto s = lex(lang::gsc, line);
		CHECK(has(s, line, tok::preproc, "#using_animtree"));
		CHECK(has(s, line, tok::keyword, "thread"));
		CHECK(has(s, line, tok::funcref, "maps\\mp\\_utility::foo"));
		CHECK(has(s, line, tok::funcref, "::bar"));
		CHECK(has(s, line, tok::localized, "&\"MP_STR\""));
	}

	{
		// block comments span lines, "/*/" does not close itself
		std::uint8_t state = LS_NONE;
		auto s = lex(lang::gsc, "a = 1; /*/ still comment", &state);
		CHECK(state & LS_BLOCK_COMMENT);
		const std::string_view l2 = "more */ wait 0.05;";
		s = lex(lang::gsc, l2, &state);
		CHECK(!(state & LS_BLOCK_COMMENT));
		CHECK(has(s, l2, tok::comment, "more */"));
		CHECK(has(s, l2, tok::keyword, "wait"));
		CHECK(has(s, l2, tok::number, "0.05"));
	}

	{
		std::uint8_t state = LS_NONE;
		lex(lang::gsc, "/#", &state);
		CHECK(state & LS_DEV_BLOCK);
		lex(lang::gsc, "#/", &state);
		CHECK(state == LS_NONE);
	}

	{
		const std::string_view line = "\tplayerAnimType pistol, movetype prone";
		const auto s = lex(lang::playeranim, line);
		CHECK(has(s, line, tok::keyword, "playerAnimType"));
		CHECK(has(s, line, tok::keyword, "movetype"));
		CHECK(has(lex(lang::playeranim, "ANIMATIONS"), "ANIMATIONS", tok::section, "ANIMATIONS"));
	}

	CHECK(lang_from_path("maps/mp/_load.GSC") == lang::gsc);
	CHECK(lang_from_path("animtrees/multiplayer.atr") == lang::atr);
	CHECK(lang_from_path("mp/playeranim.script") == lang::playeranim);
	CHECK(lang_from_path("mp/statstable.csv") == lang::csv);
	CHECK(lang_from_path("README") == lang::plain);
}

static void test_animtree()
{
	animtree tree;
	const bool ok = parse_animtree(
		"// comment\n"
		"code\n"
		"{\n"
		"\tmain\n"
		"\t{\n"
		"\t\ttorso { pt_stand_shoot /* inline */ pt_crouch_shoot }\n"
		"\t\tpb_stand_alert // trailing\n"
		"\t}\n"
		"}\n", tree);

	CHECK(ok);
	CHECK(tree.nodes.size() == 6);
	CHECK(tree.contains("PT_STAND_SHOOT"));
	CHECK(tree.contains("pb_stand_alert"));
	CHECK(!tree.contains("inline"));
	CHECK(tree.nodes[0].group && tree.nodes[0].name == "code" && tree.nodes[0].depth == 0);
	CHECK(tree.nodes[3].name == "pt_stand_shoot" && tree.nodes[3].depth == 3 && !tree.nodes[3].group && tree.nodes[3].line == 6);
	CHECK(tree.nodes[5].name == "pb_stand_alert" && tree.nodes[5].depth == 2 && tree.nodes[tree.nodes[5].parent].name == "main");

	animtree flagged;
	CHECK(parse_animtree("look_2 : additive\n{\n look_down\n}\nrun : complete nonloopsync { run_a }\n", flagged));
	CHECK(flagged.nodes.size() == 4 && flagged.nodes[0].flags == "additive" && flagged.nodes[0].group && flagged.nodes[2].flags == "complete nonloopsync");
	CHECK(!flagged.contains(":") && !flagged.contains("additive"));

	animtree bad;
	CHECK(!parse_animtree("a { b b }\n}\n", bad));
	CHECK(bad.errors.size() == 1 && bad.warnings.size() == 1); // unmatched brace + duplicate
}

static void test_playeranim()
{
	playeranim pa;
	const bool ok = parse_playeranim(
		"DEFINES\n"
		"set movetype crouching = idlecr AND runcr\n"
		"\n"
		"ANIMATIONS\n"
		"state combat\n"
		"{\n"
		"\tidle\n"
		"\t{\n"
		"\t\tplayerAnimType pistol, movetype crouching\n"
		"\t\t{\n"
		"\t\t\tboth pb_crouch_alert_pistol\n"
		"\t\t\ttorso pt_stand_shoot sound weap_fire // c\n"
		"\t\t}\n"
		"\t\tdefault { legs pl_stand }\n"
		"\t}\n"
		"}\n"
		"EVENTS\n"
		"fireweapon\n"
		"{\n"
		"\tdefault\n"
		"\t{\n"
		"\t\ttorso pt_stand_shoot\n"
		"\t}\n"
		"}\n", pa);

	CHECK(ok);
	CHECK(pa.sections.size() == 3);
	CHECK(pa.defines.size() == 1 && pa.defines[0].type == "movetype" && pa.defines[0].name == "crouching" && pa.defines[0].value == "idlecr AND runcr");
	CHECK(pa.blocks.size() == 6);
	CHECK(pa.command_count == 4);

	if (pa.blocks.size() == 6)
	{
		CHECK(pa.blocks[0].label == "state combat" && pa.blocks[0].section == "ANIMATIONS" && pa.blocks[0].line == 5);
		CHECK(pa.blocks[2].label == "playerAnimType pistol, movetype crouching" && pa.blocks[2].depth == 2);
		CHECK(pa.blocks[2].commands.size() == 2);
		CHECK(pa.blocks[2].commands[0].part == "both" && pa.blocks[2].commands[0].anim == "pb_crouch_alert_pistol" && pa.blocks[2].commands[0].line == 11);
		CHECK(pa.blocks[2].commands[1].extra == "sound weap_fire");
		CHECK(pa.blocks[3].label == "default" && pa.blocks[3].commands.size() == 1 && pa.blocks[3].commands[0].anim == "pl_stand");
		CHECK(pa.blocks[4].section == "EVENTS" && pa.blocks[4].label == "fireweapon");
		CHECK(pa.block_path(2) == "ANIMATIONS > state combat > idle > playerAnimType pistol, movetype crouching");
	}

	playeranim bad;
	CHECK(!parse_playeranim("ANIMATIONS\nstate x\n{\n idle\n {\n", bad));
}

static void test_gsc_scan()
{
	gsc_file f;
	scan_gsc(
		"#include maps\\_utility;\n"
		"#using_animtree( \"generic_human\" );\n"
		"main()\n"
		"{\n"
		"\tlevel.scr_anim[ \"foley\" ][ \"intro\" ] = %training_intro_foley; // %not_an_anim\n"
		"\tlevel.scr_anim[ \"generic\" ][ \"idle\" ][ 0 ] = %patrol_idle;\n"
		"\tlevel.scr_animtree[ \"foley\" ] = #animtree;\n"
		"\tx = 10 % count; s = \"100%sure\";\n"
		"\tif ( level.scr_anim[ \"a\" ][ \"b\" ] == %cmp_anim ) return;\n"
		"\tnode thread anim_loop_solo( guy, \"idle\", \"stop_idle\" );\n"
		"}\n"
		"#using_animtree( \"vehicles\" );\n"
		"other( a, b )\n"
		"{\n"
		"\tself setanim( %heli_rotors, 1 );\n"
		"}\n", f);

	CHECK(f.trees.size() == 2 && f.trees[0].first == "generic_human" && f.trees[1].first == "vehicles");
	CHECK(f.functions.size() == 2 && f.functions[0].first == "main" && f.functions[1].first == "other" && f.functions[1].second == 13);
	CHECK(f.refs.size() == 4);

	if (f.refs.size() == 4)
	{
		CHECK(f.refs[0].anim == "training_intro_foley" && f.refs[0].tree == "generic_human" && f.refs[0].line == 5);
		CHECK(f.refs[2].anim == "cmp_anim");
		CHECK(f.refs[3].anim == "heli_rotors" && f.refs[3].tree == "vehicles" && f.refs[3].line == 15);
	}

	CHECK(f.scenes.size() == 3); // the == comparison is not an assignment

	if (f.scenes.size() == 3)
	{
		CHECK(f.scenes[0].table == "scr_anim" && f.scenes[0].animname == "foley" && f.scenes[0].scene == "intro" && f.scenes[0].value == "%training_intro_foley");
		CHECK(f.scenes[1].index == "0" && f.scenes[1].value == "%patrol_idle");
		CHECK(f.scenes[2].table == "scr_animtree" && f.scenes[2].value == "#animtree");
	}

	CHECK(f.calls.size() == 2);

	if (f.calls.size() == 2)
	{
		CHECK(f.calls[0].func == "anim_loop_solo" && f.calls[0].strings.size() == 2 && f.calls[0].strings[0] == "idle" && f.calls[0].line == 10);
		CHECK(f.calls[1].func == "setanim");
	}
}

static void test_index(const std::string& tmp_root)
{
	namespace fs = std::filesystem;
	fs::remove_all(tmp_root);
	fs::create_directories(tmp_root + "/animtrees");
	fs::create_directories(tmp_root + "/maps");
	fs::create_directories(tmp_root + "/mp");
	fs::create_directories(tmp_root + "/.git/maps");

	const auto write = [&](const std::string& rel, const std::string& text)
	{
		std::ofstream(tmp_root + "/" + rel, std::ios::binary) << text;
	};

	write("animtrees/generic_human.atr", "root { run_fast walk_slow }\n");
	write("animtrees/multiplayer.atr", "code { main { pb_stand pt_shoot } }\n");
	write("mp/playeranim.script", "ANIMATIONS\nstate combat\n{\n idle\n {\n  default\n  {\n   both pb_stand\n   torso pt_missing\n  }\n }\n}\n");
	write("maps/level_anim.gsc", "#using_animtree( \"generic_human\" );\nmain()\n{\n a = %run_fast;\n b = %not_in_tree;\n}\n");
	write("maps/no_tree.gsc", "main()\n{\n a = %walk_slow;\n}\n");
	write("maps/missing_tree.gsc", "#using_animtree( \"nope\" );\nmain()\n{\n a = %x;\n b = %y;\n}\n");
	write(".git/maps/ignored.gsc", "main()\n{\n a = %ignored;\n}\n");

	anim_index index;
	index.build({ tmp_root });

	CHECK(index.trees.size() == 2);
	CHECK(index.gsc.size() == 3);
	CHECK(index.has_player);
	CHECK(index.total_refs == 5);
	CHECK(index.find_tree("GENERIC_HUMAN") != nullptr);

	const auto count = [&](severity sev, std::string_view needle)
	{
		int n = 0;
		for (const auto& d : index.diagnostics)
		{
			n += (d.sev == sev && d.message.find(needle) != std::string::npos) ? 1 : 0;
		}

		return n;
	};

	CHECK(count(severity::error, "%not_in_tree is not part of animtree 'generic_human'") == 1);
	CHECK(count(severity::error, "%walk_slow is used before any #using_animtree") == 1);
	CHECK(count(severity::warning, "animtree 'nope'") == 1); // reported once per file
	CHECK(count(severity::warning, "player animation 'pt_missing'") == 1);
	CHECK(index.diagnostics.size() == 4);

	// fixing the file and updating only that file clears the error
	write("maps/level_anim.gsc", "#using_animtree( \"generic_human\" );\nmain()\n{\n a = %run_fast;\n b = %walk_slow;\n}\n");
	index.update_file((fs::path(tmp_root) / "maps" / "level_anim.gsc").string());
	CHECK(count(severity::error, "not_in_tree") == 0);
	CHECK(index.diagnostics.size() == 3);

	// a second root: gsc files are shadowed by the first root, animtree names are merged
	{
		const auto second = tmp_root + "_ref";
		fs::remove_all(second);
		fs::create_directories(second + "/animtrees");
		fs::create_directories(second + "/maps");
		std::ofstream(second + "/animtrees/generic_human.atr", std::ios::binary) << "root { zone_only_anim }\n";
		std::ofstream(second + "/maps/level_anim.gsc", std::ios::binary) << "main()\n{\n a = %shadowed;\n}\n";
		std::ofstream(second + "/maps/uses_zone_anim.gsc", std::ios::binary) << "#using_animtree( \"generic_human\" );\nmain()\n{\n a = %zone_only_anim;\n}\n";

		anim_index two;
		two.build({ tmp_root, second });
		CHECK(two.gsc.size() == 4);
		CHECK(two.find_tree("generic_human")->contains("zone_only_anim"));
		CHECK(two.find_tree("generic_human")->merged_paths.size() == 1);
		CHECK(two.diagnostics.size() == 3); // same findings as before, nothing from the second root
		fs::remove_all(second);
	}

	// known xanims: anims that only exist in the tree are reported
	index.set_known_xanims({ "run_fast", "pb_stand", "pt_shoot" });
	index.validate();
	CHECK(count(severity::warning, "%walk_slow is in animtree 'generic_human' but is not an xanim of the game") == 1);
	CHECK(count(severity::warning, "%run_fast") == 0);

	fs::remove_all(tmp_root);
}

static void smoke_real_tree(const std::string& root)
{
	const auto t0 = std::chrono::steady_clock::now();

	anim_index index;
	index.build({ root });

	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

	std::size_t errors = 0, warnings = 0, calls = 0, anim_files = 0, nodes = 0;
	for (const auto& d : index.diagnostics)
	{
		(d.sev == severity::error ? errors : warnings)++;
	}

	for (const auto& [rel, file] : index.gsc)
	{
		calls += file.calls.size();
		anim_files += file.has_anim_content() ? 1 : 0;
	}

	for (const auto& [name, tree] : index.trees)
	{
		nodes += tree.nodes.size();
	}

	std::printf("\n-- %s\n", root.c_str());
	std::printf("   indexed in %lld ms\n", static_cast<long long>(ms));
	std::printf("   animtrees: %zu ( %zu nodes )\n", index.trees.size(), nodes);
	std::printf("   gsc files: %zu ( %zu with animation content )\n", index.gsc.size(), anim_files);
	std::printf("   %%anim refs: %zu, scr_* table entries: %zu, anim calls: %zu\n", index.total_refs, index.total_scenes, calls);

	if (index.has_player)
	{
		std::printf("   playeranim.script: %zu defines, %zu blocks, %zu commands, %zu parse errors\n",
			index.player.defines.size(), index.player.blocks.size(), index.player.command_count, index.player.errors.size());
	}

	std::printf("   diagnostics: %zu errors, %zu warnings\n", errors, warnings);

	int shown = 0;
	for (const auto& d : index.diagnostics)
	{
		if (shown++ >= (std::getenv("CODOL_TESTS_ALL") ? 100000 : 12))
		{
			break;
		}

		std::printf("     [%s] %s:%d  %s\n", d.sev == severity::error ? "E" : "W",
			relative_slash_path(root, d.path).c_str(), d.line, d.message.c_str());
	}

	CHECK(!index.trees.empty());
	CHECK(!index.gsc.empty());
}

static void test_map_ents(const char* real_ents_file, const char* real_zone_file)
{
	using namespace codol::mapents;

	// same vector as the server ( zlib crc32 )
	CHECK(crc32("123456789") == 0xCBF43926u);

	const std::string zone_text =
		"{\n\"classname\" \"worldspawn\"\n\"sunlight\" \"1.1\"\n}\n"
		"{\n\"classname\" \"mp_dm_spawn\"\n\"origin\" \"10 20 30\"\n\"angles\" \"0 90 0\"\n}\n"
		"{\n\"classname\" \"trigger_multiple\"\n\"model\" \"?3\"\n\"targetname\" \"flag\"\n}\n"
		"{\n\"classname\" \"script_brushmodel\"\n\"model\" \"*1\"\n}\n"
		"{\n\"classname\" \"script_model\"\n\"model\" \"com_barrel\"\n\"origin\" \"1 2 3\"\n}";

	entity_set set;
	std::string error;
	CHECK(parse_entity_string(zone_text, set, error));
	CHECK(set.ents.size() == 5);
	CHECK(set.has_crc && set.base_crc == crc32(zone_text));
	CHECK(serialize_entity_string(set.ents) == zone_text); // byte exact round trip

	// zone -> entity string
	std::string blob = std::string("\x01\x02garbage{", 10) + std::string(1, '\0') + zone_text + std::string(1, '\0') + "tail";
	std::string extracted;
	CHECK(extract_from_zone(blob, extracted));
	CHECK(extracted == zone_text);

	// -> radiant map
	const auto map = to_radiant_map(set, "mp_test");
	CHECK(map.starts_with("iwmap 4\n// entity 0\n{\n\"classname\" \"worldspawn\"\n"));
	CHECK(map.find("\"codol_base_crc32\"") != std::string::npos);
	CHECK(map.find("\"classname\" \"codol_bmodel\"\n\"model\" \"?3\"") != std::string::npos);
	CHECK(map.find("\"codol_classname\" \"trigger_multiple\"") != std::string::npos);
	CHECK(map.find("\"classname\" \"script_model\"\n\"model\" \"com_barrel\"") != std::string::npos); // xmodels stay as they are

	// unedited map -> override: same entities as the zone
	export_result exp;
	CHECK(from_radiant_map(map, exp, error));
	CHECK(exp.map_name == "mp_test" && exp.base_crc == set.base_crc && exp.exported == 5 && exp.warnings.empty());

	entity_set back;
	CHECK(parse_entity_string(exp.text, back, error));
	CHECK(back.has_crc && back.base_crc == set.base_crc);
	CHECK(serialize_entity_string(back.ents) == zone_text);

	// edited in radiant: layer header, a moved spawn, a new spawn, a drawn brush entity, worldspawn brushes
	std::string edited = map;
	edited.replace(edited.find("\"origin\" \"10 20 30\""), 19, "\"origin\" \"64 64 8\"");
	edited.insert(7, "\n\"000_Global\" flags  active\n\"The Map\" flags ");
	edited.insert(edited.find("// entity 1"), "// brush 0\n{\n ( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) caulk 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0\n}\n");
	// ^ lands after the worldspawn's closing brace: make it a real nested brush instead
	edited = map;
	{
		const auto world_close = edited.find("}\n// entity 1");
		edited.insert(world_close, "// brush 0\n{\n ( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) caulk 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0\n}\n");
		edited.replace(edited.find("\"origin\" \"10 20 30\""), 19, "\"origin\" \"64 64 8\"");
		edited.insert(7, "\n\"000_Global\" flags  active\n\"The Map\" flags ");
		edited += "// entity 5\n{\n\"classname\" \"mp_tdm_spawn\"\n\"origin\" \"5 5 5\"\n}\n";
		edited += "// entity 6\n{\n\"classname\" \"trigger_multiple\"\n// brush 0\n{\n ( 0 0 0 ) ( 1 0 0 ) ( 0 1 0 ) trigger 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0\n}\n}\n";
		edited += "// entity 7\n{\n\"classname\" \"misc_model\"\n\"model\" \"tree\"\n}\n";
	}

	CHECK(from_radiant_map(edited, exp, error));
	CHECK(exp.exported == 6);
	CHECK(exp.warnings.size() == 2); // drawn brush entity + misc_model
	CHECK(exp.text.find("\"origin\" \"64 64 8\"") != std::string::npos);
	CHECK(exp.text.find("\"classname\" \"mp_tdm_spawn\"") != std::string::npos);
	CHECK(exp.text.find("codol_") == std::string::npos || exp.text.find("codol_") < exp.text.find("{")); // only in the header comment
	CHECK(exp.text.find("caulk") == std::string::npos);
	CHECK(exp.text.find("\"classname\" \"trigger_multiple\"\n\"model\" \"?3\"") != std::string::npos);
	CHECK(parse_entity_string(exp.text, back, error) && back.ents.size() == 6);

	// ---- merge against a baseline: radiant rewrites untouched entities when it saves
	{
		const auto replace_all = [](std::string s, const std::string& from, const std::string& to)
		{
			for (auto pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size()))
			{
				s.replace(pos, from.size(), to);
			}

			return s;
		};

		// what a radiant save does to the freshly imported map
		std::string baseline = map;
		baseline = replace_all(baseline, "\"origin\" \"10 20 30\"", "\"origin\" \"10.0 20.0 30.0\"");	// reformatted
		baseline = replace_all(baseline, "\"origin\" \"1 2 3\"", "\"origin\" \"1.0 2.0 3.0\"");
		baseline = replace_all(baseline, "\"model\" \"com_barrel\"", "\"model\" \"radiant_forced_model\"");	// known class
		baseline = replace_all(baseline, "\"model\" \"?3\"\n", "\"model\" \"?3\"\n\"origin\" \"0.0 0.0 0.0\"\n");	// point entity
		baseline = replace_all(baseline, "\"model\" \"*1\"\n", "\"model\" \"*1\"\n\"origin\" \"0.0 0.0 0.0\"\n");
		CHECK(baseline != map);

		// nothing edited: the override is the zone's entity string, byte for byte
		export_result merged;
		CHECK(merge_radiant_map(baseline, baseline, zone_text, merged, error));
		CHECK(merged.exported == 5 && merged.warnings.empty() && merged.base_crc == set.base_crc && merged.map_name == "mp_test");
		CHECK(parse_entity_string(merged.text, back, error));
		CHECK(serialize_entity_string(back.ents) == zone_text);

		// the plain exporter would have taken radiant's rewrites
		CHECK(from_radiant_map(baseline, exp, error));
		CHECK(exp.text.find("radiant_forced_model") != std::string::npos);

		// edits
		std::string edited_map = baseline;
		edited_map = replace_all(edited_map, "\"origin\" \"10.0 20.0 30.0\"", "\"origin\" \"64.0 64.0 8.0\"");			// moved a spawn
		edited_map = replace_all(edited_map, "\"model\" \"?3\"\n\"origin\" \"0.0 0.0 0.0\"", "\"model\" \"?3\"\n\"origin\" \"5.0 0.0 0.0\"");	// moved a trigger hull
		edited_map = replace_all(edited_map, "\"sunlight\" \"1.1\"", "\"sunlight\" \"2\"");							// worldspawn key
		edited_map = replace_all(edited_map, "\"targetname\" \"flag\"\n", "\"targetname\" \"flag\"\n\"script_label\" \"_a\"\n");	// new key

		// delete the script_model ( entity 4 ), add a spawn and a copy of the trigger hull that was moved
		edited_map.erase(edited_map.find("// entity 4"));
		edited_map += "// entity 4\n{\n\"classname\" \"mp_tdm_spawn\"\n\"origin\" \"7.0 7.0 7.0\"\n}\n";
		edited_map += "// entity 5\n{\n\"classname\" \"codol_bmodel\"\n\"model\" \"?3\"\n\"origin\" \"100.0 0.0 0.0\"\n\"targetname\" \"flag\"\n"
			"\"codol_classname\" \"trigger_multiple\"\n\"codol_id\" \"2\"\n}\n";

		CHECK(merge_radiant_map(edited_map, baseline, zone_text, merged, error));
		CHECK(merged.exported == 6 && merged.warnings.empty());
		CHECK(parse_entity_string(merged.text, back, error) && back.ents.size() == 6);

		if (back.ents.size() == 6)
		{
			CHECK(*back.ents[0].find("sunlight") == "2" && back.ents[0].pairs.size() == 2);
			CHECK(*back.ents[1].find("origin") == "64.0 64.0 8.0" && *back.ents[1].find("angles") == "0 90 0");
			CHECK(*back.ents[2].find("classname") == "trigger_multiple" && *back.ents[2].find("origin") == "5.0 0.0 0.0"
				&& *back.ents[2].find("script_label") == "_a" && *back.ents[2].find("model") == "?3");
			CHECK(*back.ents[3].find("classname") == "script_brushmodel" && !back.ents[3].find("origin")); // radiant's origin is not exported
			CHECK(*back.ents[4].find("classname") == "mp_tdm_spawn");
			CHECK(*back.ents[5].find("classname") == "trigger_multiple" && *back.ents[5].find("origin") == "100.0 0.0 0.0"
				&& *back.ents[5].find("model") == "?3" && !back.ents[5].find("script_label"));
		}

		CHECK(merged.text.find("codol_") == std::string::npos && merged.text.find("radiant_forced_model") == std::string::npos);

		// a key deleted by the user stays deleted, a class change is kept
		std::string edited2 = replace_all(baseline, "\"angles\" \"0 90 0\"\n", "");
		edited2 = replace_all(edited2, "\"classname\" \"mp_dm_spawn\"", "\"classname\" \"mp_tdm_spawn\"");
		CHECK(merge_radiant_map(edited2, baseline, zone_text, merged, error));
		CHECK(parse_entity_string(merged.text, back, error) && back.ents.size() == 5);
		CHECK(back.ents.size() == 5 && !back.ents[1].find("angles") && *back.ents[1].find("classname") == "mp_tdm_spawn"
			&& *back.ents[1].find("origin") == "10 20 30");

		CHECK(!merge_radiant_map(edited_map, "garbage", zone_text, merged, error));
	}

	// ---- the MapEnts asset inside a zone ( c1::MapEnts header in front of the entity string )
	{
		using namespace codol;

		c1::MapEnts header = {};
		header.name = 0x302A6785u;					// reference to the name of the render world
		header.entityString = c1::ZPTR_INLINE;
		header.numEntityChars = static_cast<std::int32_t>(zone_text.size() + 1);
		header.trigger = { 2, c1::ZPTR_INLINE, 4, c1::ZPTR_INLINE, 7, c1::ZPTR_INLINE };
		header.stages = c1::ZPTR_INLINE;
		header.stageCount = 3;

		std::string zone(64, '\x7f');
		zone += "{\n\"classname\" \"worldspawn\"\n}";	// a look-alike without a header in front of it
		zone += std::string(1, '\0') + std::string(40, '\x11');

		const auto header_at = zone.size();
		zone.append(reinterpret_cast<const char*>(&header), sizeof(header));
		zone += zone_text + std::string(1, '\0') + "trailing";

		zone_map_ents info;
		CHECK(find_map_ents(zone, info));
		CHECK(info.header_offset == header_at && info.text_offset == header_at + sizeof(header) && info.text_length == zone_text.size());
		CHECK(info.header.trigger.count == 2 && info.header.stageCount == 3 && c1::zptr_block(info.header.name) == 3);

		int trigger_models = 0;
		CHECK(extract_from_zone(zone, extracted, &trigger_models) && extracted == zone_text && trigger_models == 2);

		// name stored inline between the header and the entity string
		header.name = c1::ZPTR_INLINE;
		std::string zone2(32, '\x7f');
		zone2.append(reinterpret_cast<const char*>(&header), sizeof(header));
		zone2 += "maps/mp/mp_test.d3dbsp" + std::string(1, '\0') + zone_text + std::string(1, '\0');
		CHECK(find_map_ents(zone2, info) && info.header_offset == 32 && info.text_length == zone_text.size());

		// a wrong length is not accepted as the asset
		header.numEntityChars += 1;
		std::string zone3(32, '\x7f');
		zone3.append(reinterpret_cast<const char*>(&header), sizeof(header));
		zone3 += "maps/mp/mp_test.d3dbsp" + std::string(1, '\0') + zone_text + std::string(1, '\0');
		CHECK(!find_map_ents(zone3, info));

		// trigger hull references are checked against the zone on export ( the map has 2 hulls: ?0 and ?1 )
		entity_set limited = set;
		limited.trigger_models = 2;
		export_result res;
		CHECK(from_radiant_map(to_radiant_map(limited, "mp_test"), res, error));
		CHECK(res.exported == 4 && res.warnings.size() == 1 && res.warnings[0].find("\"?3\" is not one of the 2 trigger hulls") != std::string::npos);
		CHECK(res.text.find("codol_trigger_models") == std::string::npos);

		limited.trigger_models = 4;
		CHECK(from_radiant_map(to_radiant_map(limited, "mp_test"), res, error) && res.exported == 5 && res.warnings.empty());
	}

	// a normal radiant map is refused
	CHECK(!from_radiant_map("iwmap 4\n// entity 0\n{\n\"classname\" \"worldspawn\"\n}\n", exp, error));
	CHECK(!parse_entity_string("{\n\"classname\" \"info_null\"\n}", set, error));
	CHECK(!parse_entity_string("{\n\"classname\" \"worldspawn\"\n", set, error));

	// a real override written by the server tooling: parse -> map -> export keeps every entity
	if (real_ents_file)
	{
		std::string text;
		CHECK(codol::script::read_text_file(real_ents_file, text));
		CHECK(parse_entity_string(text, set, error));
		CHECK(set.has_crc);

		const auto real_map = to_radiant_map(set, "real_map");
		CHECK(from_radiant_map(real_map, exp, error));
		CHECK(exp.exported == static_cast<int>(set.ents.size()) && exp.warnings.empty() && exp.base_crc == set.base_crc);
		CHECK(parse_entity_string(exp.text, back, error));
		CHECK(serialize_entity_string(back.ents) == serialize_entity_string(set.ents));

		std::printf("\n-- %s\n   %zu entities, base_crc32 %08x, round trip through a radiant .map is lossless: %s\n", real_ents_file,
			set.ents.size(), set.base_crc, serialize_entity_string(back.ents) == serialize_entity_string(set.ents) ? "yes" : "NO");
	}
	// a real decrypted zone: the MapEnts header has to validate against its own entity string
	if (real_zone_file)
	{
		std::string zone;
		{
			std::ifstream file(real_zone_file, std::ios::binary);
			zone.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		}

		zone_map_ents info;
		CHECK(!zone.empty());
		CHECK(find_map_ents(zone, info));
		CHECK(info.header.numEntityChars == static_cast<std::int32_t>(info.text_length + 1));
		CHECK(codol::c1::zptr_is_inline(info.header.entityString));

		std::string text;
		int trigger_models = -1;
		CHECK(extract_from_zone(zone, text, &trigger_models) && text.size() == info.text_length);
		CHECK(parse_entity_string(text, set, error));

		// every "?N" of the map names one of its trigger hulls
		int hull_refs = 0, bad_refs = 0;
		for (const auto& ent : set.ents)
		{
			if (const auto* model = ent.find("model"); model && model->size() > 1 && (*model)[0] == '?')
			{
				hull_refs++;
				bad_refs += std::atoi(model->c_str() + 1) >= trigger_models ? 1 : 0;
			}
		}

		CHECK(bad_refs == 0);

		std::printf("\n-- %s\n   MapEnts header at 0x%zx: %d entity chars, %u trigger models, %u hulls, %u slabs, %u stages; "
			"%zu entities, crc32 %08x, %d \"?N\" references ( %d out of range )\n", real_zone_file, info.header_offset,
			info.header.numEntityChars, info.header.trigger.count, info.header.trigger.hullCount, info.header.trigger.slabCount,
			static_cast<unsigned>(info.header.stageCount), set.ents.size(), set.base_crc, hull_refs, bad_refs);
	}
}
int main(int argc, char** argv)
{
	test_lexer();
	test_animtree();
	test_playeranim();
	test_gsc_scan();
	test_index((std::filesystem::temp_directory_path() / "codol_tests_tree").string());

	// arguments: script trees to index, and/or a .d3dbsp.ents file for the map entity round trip
	const char* ents_file = nullptr;
	const char* zone_file = nullptr;
	for (int i = 1; i < argc; i++)
	{
		if (std::string_view(argv[i]).ends_with(".ents"))
		{
			ents_file = argv[i];
		}
		else if (std::string_view(argv[i]).ends_with(".zone"))
		{
			zone_file = argv[i];
		}
		else
		{
			smoke_real_tree(argv[i]);
		}
	}

	test_map_ents(ents_file, zone_file);

	std::printf("\n%d checks, %d failed\n", g_checks, g_failed);
	return g_failed ? 1 : 0;
}
