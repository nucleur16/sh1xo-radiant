// Effect tests; included by codol_assets_tests.cpp ( uses its CHECK ). test_fx needs nothing but the library - the
// zone it builds goes through the game's loaders, which are part of codol_zone; test_fx_install compares with a
// shipped effect.

#include "codol_assets/fx.hpp"

namespace
{
	bool fx_close(const float a, const float b) { return std::fabs(a - b) <= 1e-6f * std::max(1.0f, std::max(std::fabs(a), std::fabs(b))); }

	// what the loaders make of a zone that holds `fx`: read back and compared field by field
	bool fx_same(const CompiledFx& a, const CompiledFx& b)
	{
		bool same = a.name == b.name && a.flags == b.flags && a.totalSize == b.totalSize && a.msecLoopingLife == b.msecLoopingLife && a.countLooping == b.countLooping &&
			a.countOneShot == b.countOneShot && a.countEmission == b.countEmission && a.elems.size() == b.elems.size();
		for (std::size_t i = 0; same && i < a.elems.size(); i++)
		{
			const FxElem& x = a.elems[i];
			const FxElem& y = b.elems[i];
			// everything in front of the pointers, and the plain fields between them
			same = std::memcmp(&x.def, &y.def, offsetof(c1::FxElemDef, velSamples)) == 0 && std::memcmp(&x.def.collBounds, &y.def.collBounds, sizeof(c1::Bounds)) == 0 &&
				std::memcmp(&x.def.emitDist, &y.def.emitDist, 16) == 0 && std::memcmp(&x.def.sortOrder, &y.def.sortOrder, 8) == 0 && x.visuals == y.visuals &&
				x.effectOnImpact == y.effectOnImpact && x.effectOnDeath == y.effectOnDeath && x.effectEmitted == y.effectEmitted && x.velSamples.size() == y.velSamples.size() &&
				x.visSamples.size() == y.visSamples.size() && x.hasExtended == y.hasExtended && x.trailInds == y.trailInds && x.trailVerts.size() == y.trailVerts.size() &&
				x.hasString256 == y.hasString256 && x.string256 == y.string256;
			same = same && (x.velSamples.empty() || std::memcmp(x.velSamples.data(), y.velSamples.data(), x.velSamples.size() * sizeof(c1::FxElemVelStateSample)) == 0);
			same = same && (x.visSamples.empty() || std::memcmp(x.visSamples.data(), y.visSamples.data(), x.visSamples.size() * sizeof(c1::FxElemVisStateSample)) == 0);
			same = same && (x.trailVerts.empty() || std::memcmp(x.trailVerts.data(), y.trailVerts.data(), x.trailVerts.size() * sizeof(c1::FxTrailVertex)) == 0);
		}
		return same;
	}

	void test_fx(const fs::path& scratch)
	{
		// ---- the reader: statements, graphs, visuals, comments, what it does not know ----
		EditorEffect small;
		CHECK(!parse_efx("iwfx 3\n{ }\n", small));
		CHECK(!parse_efx("iwfx 2\n{\n\tname \"a\";\n\tsizeGraph0 1 { { 0 1 1 } { 0 1 1 1 } };\n}\n", small));
		CHECK(parse_efx("iwfx 2\n// a comment\n{\n\tname \"one\";\n\teditorFlags looping;\n\tflags spawnRelative runRelToSpawn;\n\tspawnLooping 100 5;\n\tlifeSpanMsec 500 0;\n"
						"\tspawnOrgX 10 -4;\n\tspawnAnglePitch 90 -180;\n\tsomethingNew 1 2 { 3 };\n\t//#CODOL fadeInfo 2\n"
						"\tsizeGraph0 10\n\t{\n\t\t{\n\t\t\t0 1\n\t\t\t1 1\n\t\t}\n\t\t{\n\t\t\t0 1\n\t\t\t1 1\n\t\t}\n\t};\n\tbillboardSprite\n\t{\n\t\t\"m1\"\n\t\t\"m2\"\n\t};\n}\n", small));
		CHECK(small.elems.size() == 1 && small.elems[0].name == "one" && small.elems[0].has_editor_flag("looping") && small.elems[0].has_flag("runRelToSpawn"));
		CHECK(small.elems[0].visualType == "billboardSprite" && small.elems[0].visuals.size() == 2 && small.elems[0].sizeGraph[0].scale == 10.0f);
		CHECK(small.elems[0].find_extra("fadeInfo") && small.elems[0].find_extra("statement") && *small.elems[0].find_extra("statement") == "somethingNew 1 2 { 3 }");
		{
			CompiledFx fx;
			CHECK(compile_fx("test/one", small, fx));
			CHECK(fx.countLooping == 1 && fx.elems.size() == 1 && fx.msecLoopingLife == 400 && fx.flags == 0);
			const c1::FxElemDef& d = fx.elems[0].def;
			// a range with a negative amplitude: positions from the other end, angles as written
			CHECK(d.spawnOrigin[0].base == 6.0f && d.spawnOrigin[0].amplitude == 4.0f && fx_close(d.spawnAngles[0].base, 1.5707964f) && fx_close(d.spawnAngles[0].amplitude, -3.1415927f));
			CHECK(d.flags == 0x42 && d.spawn[0] == 100 && d.spawn[1] == 5 && d.visualCount == 2 && d.fadeInfo == 2 && d.sortOrder == 5 && d.atlas.loopCount == 1 && d.atlas.entryCount == 1);
			CHECK(fx.elems[0].visSamples.size() == 2 && fx.elems[0].visSamples[0].base.size[0] == 5.0f && fx.elems[0].visSamples[0].base.size[1] == 5.0f);
			CHECK(fx.totalSize == fx_total_size(fx) && fx.totalSize == 32 + 9 + 260 + 2 * 96 + 2 * 48 + 8);
			// for ever: no count
			small.elems[0].spawnLooping[1] = 0;
			CHECK(compile_fx("test/one", small, fx) && fx.elems[0].def.spawn[1] == INT32_MAX && fx.msecLoopingLife == INT32_MAX);
			// the text again says the same
			EditorEffect again;
			CompiledFx second;
			CHECK(parse_efx(write_efx(small), again) && compile_fx("test/one", again, second) && fx_same(fx, second));
		}

		// ---- the test effect of the tree: tests/fx/codol_test/spark_burst.efx ----
		EditorEffect effect;
		const fs::path file = fs::path(__FILE__).parent_path() / "fx" / "codol_test" / "spark_burst.efx";
		CompiledFx fx;
		const Status loaded = load_efx(file, effect);
		if (!loaded)
		{
			std::printf("%s: the tests of the test effect are skipped\n", loaded.message.c_str());
			return;
		}
		CHECK(effect.elems.size() == 3 && fx_source_root(file, "codol_test/spark_burst") == fs::path(__FILE__).parent_path() / "fx");
		FxCompileOptions options;
		options.materialAtlas = [](const std::string_view material, int& columns, int& rows)
		{
			columns = 8;
			rows = 4;
			return material == "gfx_smk_white_atlas";
		};
		CHECK(compile_fx("codol_test/spark_burst", effect, fx, options));
		CHECK(fx.notes.empty() && fx.countLooping == 1 && fx.countOneShot == 2 && fx.countEmission == 0 && fx.flags == 1 && fx.msecLoopingLife == 1750);
		CHECK(fx.totalSize == 1843 && fx.totalSize == fx_total_size(fx));
		if (fx.elems.size() == 3)
		{
			// smoke: a looping billboard
			const FxElem& smoke = fx.elems[0];
			const c1::FxElemDef& d = smoke.def;
			CHECK(d.elemType == c1::FX_ELEM_TYPE_SPRITE_BILLBOARD && static_cast<std::uint32_t>(d.flags) == 0x050000E2u && d.spawn[0] == 250 && d.spawn[1] == 8);
			CHECK(d.lifeSpanMsec.base == 2000 && d.lifeSpanMsec.amplitude == 500 && fx_close(d.gravity.base, -0.02f) && fx_close(d.initialRotation.amplitude, 6.2831855f));
			CHECK(d.atlas.behavior == 1 && d.atlas.loopCount == 1 && d.atlas.colIndexBits == 3 && d.atlas.rowIndexBits == 2 && d.atlas.entryCount == 32 && d.lightingFrac == 127);
			// straight velocity graphs: one interval; the alpha graph has a key at a quarter: four
			CHECK(d.velIntervalCount == 1 && smoke.velSamples.size() == 2 && d.visStateIntervalCount == 4 && smoke.visSamples.size() == 5);
			if (smoke.velSamples.size() == 2 && smoke.visSamples.size() == 5)
			{
				const c1::FxElemVelStateInFrame& v0 = smoke.velSamples[0].local;
				const c1::FxElemVelStateInFrame& v1 = smoke.velSamples[1].local;
				CHECK(fx_close(v0.velocity.base[0], -0.01f) && fx_close(v0.velocity.amplitude[0], 0.02f) && fx_close(v0.velocity.base[2], 0.04f) && fx_close(v0.velocity.amplitude[2], -0.02f));
				CHECK(fx_close(v1.velocity.base[2], 0.01f) && v1.velocity.amplitude[2] == 0.0f && fx_close(v1.totalDelta.base[2], 0.025f) && fx_close(v1.totalDelta.amplitude[2], -0.01f));
				CHECK(smoke.velSamples[1].world.velocity.base[2] == 0.0f);
				const c1::FxElemVisStateSample& s0 = smoke.visSamples[0];
				const c1::FxElemVisStateSample& s1 = smoke.visSamples[1];
				CHECK(fx_close(s0.base.size[0], 7.5f) && fx_close(s0.amplitude.size[0], 7.5f) && s0.base.size[1] == s0.base.size[0] && s0.base.scale == 0.0f);
				CHECK(s0.base.color[0] == 153 && s0.base.color[2] == 153 && s0.base.color[3] == 0 && s1.base.color[3] == 128 && s1.amplitude.color[3] == 128);
				CHECK(fx_close(s0.base.rotationDelta, -15.0f * 0.017453292f / 4000.0f) && fx_close(s0.amplitude.rotationDelta, 30.0f * 0.017453292f / 4000.0f));
				CHECK(fx_close(smoke.visSamples[4].base.rotationTotal, -15.0f * 0.017453292f / 1000.0f));
			}
			// sparks: a one shot tail, two sizes, colour multiplied by alpha
			const FxElem& sparks = fx.elems[1];
			CHECK(sparks.def.elemType == c1::FX_ELEM_TYPE_TAIL && static_cast<std::uint32_t>(sparks.def.flags) == 0x15000042u && sparks.def.spawn[0] == 12 && sparks.def.spawn[1] == 6);
			CHECK(sparks.visSamples.size() == 2 && sparks.visuals.size() == 1 && sparks.visuals[0] == "gfx_spark_atlas" && sparks.def.atlas.entryCount == 1);
			if (sparks.visSamples.size() == 2)
			{
				CHECK(sparks.visSamples[0].base.color[0] == 153 && sparks.visSamples[0].base.color[1] == 230 && sparks.visSamples[0].base.color[2] == 255 && sparks.visSamples[0].base.color[3] == 255);
				CHECK(sparks.visSamples[1].base.color[2] == 0 && sparks.visSamples[1].base.color[3] == 255 && fx_close(sparks.visSamples[0].base.size[1], 12.0f) &&
					  fx_close(sparks.visSamples[1].base.size[1], 3.0f) && fx_close(sparks.visSamples[1].base.size[0], 0.5f));
			}
			// flash: a light has no visual and no atlas
			const FxElem& flash = fx.elems[2];
			CHECK(flash.def.elemType == c1::FX_ELEM_TYPE_OMNI_LIGHT && flash.def.visualCount == 1 && flash.visuals.empty() && flash.def.flags == 0x42 && flash.def.atlas.loopCount == 0);
			CHECK(flash.visSamples.size() == 2 && fx_close(flash.visSamples[0].base.size[0], 150.0f) && flash.visSamples[0].base.scale == 1.5f && flash.visSamples[0].base.color[1] == 204);
		}

		// ---- the definition of a compiled effect compiles to it again ----
		{
			EditorEffect extracted, parsed;
			CompiledFx again;
			CHECK(extract_fx(fx, extracted, true) && parse_efx(write_efx(extracted), parsed) && compile_fx(fx.name, parsed, again));
			CHECK(fx_same(fx, again));
			// without the stored counts the compiler decides the same here
			CHECK(extract_fx(fx, extracted, false) && parse_efx(write_efx(extracted), parsed) && compile_fx(fx.name, parsed, again));
			CHECK(again.elems.size() == 3 && again.elems[0].visSamples.size() == 5 && again.elems[0].velSamples.size() == 2);
		}

		// ---- trails, decals, effects of effects ----
		{
			EditorEffect trail;
			EditorElem e;
			e.name = "ribbon";
			e.editorFlags = { "looping", "playOnDeath", "playOnRun" };
			e.flags = { "spawnRelative" };
			e.visualType = "trail";
			e.visuals = { "gfx_smoke_trail01" };
			e.trailSplitDist = 250;
			e.trailRepeatDist = 256;
			e.trailScrollTime = -25;
			// a line drawn there and back: the two sides get vertices of their own
			e.trailVerts = { { { 1, 0 }, 0 }, { { -1, 0 }, 1 } };
			e.trailInds = { 0, 1, 1, 0 };
			e.fxOnDeath = "codol_test/after";
			e.fxOnImpact = "codol_test/not_used";
			e.emission = "codol_test/child";
			trail.elems.push_back(e);
			EditorElem decal;
			decal.name = "mark";
			decal.visualType = "decal";
			decal.visuals = { "gfx_crater_dirt" };
			trail.elems.push_back(decal);
			EditorElem runner;
			runner.name = "more";
			runner.visualType = "runner";
			runner.visuals = { "codol_test/spark_burst", "codol_test/other" };
			trail.elems.push_back(runner);
			EditorElem model;
			model.name = "chunk";
			model.visualType = "model";
			model.visuals = { "fx_rock_small" };
			trail.elems.push_back(model);
			EditorElem sound;
			sound.name = "pop";
			sound.visualType = "sound";
			sound.visuals = { "codol_test_pop" };
			trail.elems.push_back(sound);

			FxCompileOptions withChild;
			withChild.loadEffect = [&](const std::string_view name, EditorEffect& out)
			{
				out = small;
				return name == "codol_test/child";
			};
			CompiledFx compiled;
			CHECK(compile_fx("codol_test/trail", trail, compiled, withChild));
			// the element of the emitted effect is the emission section
			CHECK(compiled.countLooping == 1 && compiled.countOneShot == 4 && compiled.countEmission == 1 && compiled.elems.size() == 6);
			if (compiled.elems.size() == 6)
			{
				const FxElem& t = compiled.elems[0];
				CHECK(t.hasExtended && t.trailVerts.size() == 4 && t.trailInds == std::vector<std::uint16_t>({ 0, 1, 2, 3 }) && t.trail.scrollTimeMsec == -25000 && t.trail.repeatDist == 256);
				CHECK(fx_close(t.trail.invSplitDist, 0.004f) && t.trailVerts[0].normal[1] == -1.0f && t.trailVerts[2].normal[1] == 1.0f && t.trailVerts[2].pos[0] == -1.0f);
				CHECK(t.effectOnDeath == "codol_test/after" && t.effectEmitted == "codol_test/child" && t.effectOnImpact.empty() && t.visSamples[0].base.rotationDelta == 0.0f);
				CHECK(compiled.elems[1].def.elemType == c1::FX_ELEM_TYPE_DECAL && compiled.elems[1].def.visualCount == 1 &&
					  compiled.elems[1].visuals == std::vector<std::string>({ "mc/gfx_crater_dirt", "wc/gfx_crater_dirt" }));
				CHECK(compiled.elems[2].def.elemType == c1::FX_ELEM_TYPE_RUNNER && compiled.elems[2].visSamples.empty() && compiled.elems[2].def.visualCount == 2);
				CHECK(compiled.elems[3].def.elemType == c1::FX_ELEM_TYPE_MODEL && compiled.elems[3].def.atlas.loopCount == 0 && compiled.elems[4].def.elemType == c1::FX_ELEM_TYPE_SOUND);
				CHECK(compiled.elems[5].visuals.size() == 2 && compiled.elems[5].visuals[0] == "m1");
			}
			// a closed outline shares its vertices: averaged normals
			trail.elems[0].trailVerts = { { { 1, 0 }, 0 }, { { 0, 1 }, 0.5f }, { { -1, 0 }, 1 }, { { 0, -1 }, 1.5f } };
			trail.elems[0].trailInds = { 0, 1, 1, 2, 2, 3, 3, 0 };
			CompiledFx closed;
			CHECK(compile_fx("codol_test/trail", trail, closed, withChild) && closed.elems[0].trailVerts.size() == 4 && closed.elems[0].trailInds.size() == 8);
			CHECK(fx_close(closed.elems[0].trailVerts[1].normal[1], -1.0f) && fx_close(closed.elems[0].trailVerts[0].normal[0], -1.0f));
			CHECK(closed.totalSize == fx_total_size(closed));

			// ---- a zone with both effects, through the game's loaders ----
			z::ZoneEditor editor;
			CHECK(editor.create("codol_fx_test"));
			std::vector<std::string> notes;
			CHECK(link_fx(editor, fx, &notes) && link_fx(editor, compiled, &notes) && !link_fx(editor, fx));
			// gfx_smk_white_atlas, gfx_spark_atlas, gfx_smoke_trail01, the two decal materials, fx_rock_small, m1, m2: by name
			CHECK(notes.size() == 8);
			std::vector<std::uint8_t> bytes;
			const Status written = editor.write(bytes);
			CHECK(written);
			if (!written)
			{
				std::printf("%s\n", written.message.c_str());
				return;
			}
			z::Zone zone;
			CHECK(zone.load(bytes, "codol_fx_test") && zone.assets().size() == 2 && zone.warnings().empty());
			CompiledFx back;
			CHECK(read_fx(zone, zone.find_asset(c1::ASSET_TYPE_FX, "codol_test/spark_burst"), back) && fx_same(fx, back));
			CHECK(read_fx(zone, zone.find_asset(c1::ASSET_TYPE_FX, "codol_test/trail"), back) && fx_same(compiled, back));
			CHECK(back.elems.size() == 6 && back.elems[3].visuals.size() == 1 && back.elems[3].visuals[0] == "fx_rock_small" && back.elems[4].visuals[0] == "codol_test_pop");
			// and a second time through the editor: identical
			z::ZoneEditor reloaded;
			std::vector<std::uint8_t> again;
			CHECK(reloaded.load(bytes, "codol_fx_test") && reloaded.write(again) && again == bytes);

			// the atlas table as a file
			FxAtlasLibrary atlas, read;
			atlas.set("gfx_a", 4, 2);
			atlas.set("gfx_plain", 1, 1);
			int columns = 0, rows = 0;
			CHECK(atlas.size() == 1 && atlas.save(scratch / "codol_fx_test_atlas.txt") && read.load(scratch / "codol_fx_test_atlas.txt") && read.find("gfx_a", columns, rows) &&
				  columns == 4 && rows == 2 && !read.find("gfx_plain", columns, rows));
		}
	}

	// a shipped effect -> definition text -> compiled: the same effect
	void test_fx_install(const fs::path& install)
	{
		z::ClientKey key;
		z::Zone zone;
		CHECK(key.load_from_exe(install / "C1.exe"));
		if (!zone.open(install / "client_shipRetail" / "chinese" / "common_mp.ff", key))
		{
			std::printf("common_mp.ff does not open: the shipped effect test is skipped\n");
			return;
		}
		std::uint32_t effects = 0, same = 0;
		for (const z::AssetRecord& r : zone.all_assets())
		{
			CompiledFx shipped, compiled;
			EditorEffect extracted, parsed;
			if (r.type != c1::ASSET_TYPE_FX || !read_fx(zone, r.header, shipped) || shipped.elems.empty() || shipped.name.starts_with(","))
			{
				continue;
			}
			effects++;
			if (extract_fx(shipped, extracted, true) && parse_efx(write_efx(extracted), parsed) && compile_fx(shipped.name, parsed, compiled) &&
				compiled.elems.size() == shipped.elems.size() && compiled.totalSize == shipped.totalSize && compiled.flags == shipped.flags &&
				compiled.msecLoopingLife == shipped.msecLoopingLife)
			{
				bool equal = true;
				for (std::size_t i = 0; equal && i < shipped.elems.size(); i++)
				{
					const FxElem& a = shipped.elems[i];
					const FxElem& b = compiled.elems[i];
					equal = a.def.flags == b.def.flags && a.def.elemType == b.def.elemType && std::memcmp(&a.def.atlas, &b.def.atlas, 8) == 0 && a.visuals == b.visuals &&
						a.velSamples.size() == b.velSamples.size() && a.visSamples.size() == b.visSamples.size() && a.def.sortOrder == b.def.sortOrder &&
						a.def.lightingFrac == b.def.lightingFrac && a.effectEmitted == b.effectEmitted;
					for (std::size_t k = 0; equal && k < a.visSamples.size(); k++)
					{
						equal = std::memcmp(a.visSamples[k].base.color, b.visSamples[k].base.color, 4) == 0 && fx_close(a.visSamples[k].base.size[0], b.visSamples[k].base.size[0]);
					}
					for (std::size_t k = 0; equal && k < a.velSamples.size(); k++)
					{
						equal = fx_close(a.velSamples[k].local.velocity.base[0], b.velSamples[k].local.velocity.base[0]) &&
							fx_close(a.velSamples[k].world.velocity.base[2], b.velSamples[k].world.velocity.base[2]);
					}
				}
				same += equal ? 1 : 0;
			}
		}
		std::printf("common_mp: %u effects, %u the same after definition text -> compile\n", effects, same);
		CHECK(effects > 200 && same == effects);
	}
}
