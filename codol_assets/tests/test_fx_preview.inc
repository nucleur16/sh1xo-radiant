// The effect preview ( simulate_fx ); included by codol_assets_tests.cpp ( uses its CHECK ). An effect whose numbers
// can be computed by hand: where its particles are, how large, which colour, which atlas cell at chosen moments.

#include "codol_assets/fx_preview.hpp"

namespace
{
	FxCurve fx_keys(std::initializer_list<float> keys, const int dimensions = 1)
	{
		FxCurve curve;
		curve.dimensions = dimensions;
		curve.keys.assign(keys);
		return curve;
	}

	bool fx_near(const float a, const float b, const float tolerance = 0.02f) { return std::fabs(a - b) <= tolerance; }

	void test_fx_preview()
	{
		// a curve: linear between its keys, the ends outside, one value of a colour
		{
			const FxCurve curve = fx_keys({ 0.0f, 1.0f, 0.5f, 3.0f, 1.0f, 2.0f });
			CHECK(fx_curve_value(curve, 0.0f) == 1.0f && fx_near(fx_curve_value(curve, 0.25f), 2.0f) && fx_near(fx_curve_value(curve, 0.75f), 2.5f) && fx_curve_value(curve, 2.0f) == 2.0f);
			const FxCurve colour = fx_keys({ 0.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f }, 3);
			CHECK(colour.key_count() == 2 && fx_near(fx_curve_value(colour, 0.5f, 0), 0.5f) && fx_near(fx_curve_value(colour, 0.5f, 2), 0.5f) && fx_curve_value(colour, 0.5f, 3) == 0.0f);
			CHECK(fx_curve_value(FxCurve{}, 0.5f) == 0.0f);
		}

		// five particles, one every 100 ms, each living 500 ms, flying 100 units a second along the effect's forward
		// axis ( up ), growing from 10 to 30 units, fading out, playing a 2 x 2 atlas over their life
		EditorEffect effect;
		EditorElem loop;
		loop.name = "loop";
		loop.editorFlags = { "looping" };
		loop.spawnLooping[0] = 100;
		loop.spawnLooping[1] = 5;
		loop.lifeSpanMsec[0] = 500;
		loop.lifeSpanMsec[1] = 0;
		loop.velGraph[0][0].scale = 100.0f;
		loop.velGraph[0][0].curve[0] = fx_keys({ 0.0f, 1.0f, 1.0f, 1.0f });
		loop.sizeGraph[0].scale = 10.0f;
		loop.sizeGraph[0].curve[0] = fx_keys({ 0.0f, 1.0f, 1.0f, 3.0f });
		loop.alphaGraph.curve[0] = fx_keys({ 0.0f, 1.0f, 1.0f, 0.0f });
		loop.colorGraph.curve[0] = fx_keys({ 0.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f }, 3);
		loop.atlasBehavior = "startFixed playOverLife";
		loop.atlasColIndexBits = 1;
		loop.atlasRowIndexBits = 1;
		loop.visualType = "billboardSprite";
		loop.visuals = { "some_material" };
		effect.elems.push_back(loop);
		CHECK(fx_near(fx_preview_duration(effect), 0.9f));

		FxPreview at;
		simulate_fx(effect, 0.05f, at);
		CHECK(at.particles.size() == 1 && at.skipped.empty());
		if (at.particles.size() == 1)
		{
			const FxParticle& p = at.particles[0];
			CHECK(p.index == 0 && p.material == "some_material" && p.shape == FxShape::billboard && fx_near(p.age, 0.05f) && fx_near(p.life, 0.5f));
			CHECK(fx_near(p.position[2], 5.0f) && fx_near(p.position[0], 0.0f) && fx_near(p.position[1], 0.0f) && fx_near(p.velocity[2], 100.0f, 0.5f));
			CHECK(fx_near(p.width, 12.0f, 0.05f) && p.height == p.width && fx_near(p.color[3], 0.9f) && fx_near(p.color[0], 0.9f) && fx_near(p.color[1], 0.5f) && fx_near(p.color[2], 0.1f));
			CHECK(p.u0 == 0.0f && p.v0 == 0.0f && p.u1 == 0.5f && p.v1 == 0.5f);
		}

		simulate_fx(effect, 0.46f, at);
		CHECK(at.particles.size() == 5);
		if (at.particles.size() == 5)
		{
			// the oldest first: 46 units up, nearly gone, in the last cell; the youngest 5 units up
			CHECK(fx_near(at.particles[0].position[2], 46.0f, 0.1f) && fx_near(at.particles[4].position[2], 6.0f, 0.1f) && fx_near(at.particles[0].width, 28.4f, 0.1f));
			CHECK(fx_near(at.particles[0].color[3], 0.08f) && at.particles[0].u0 == 0.5f && at.particles[0].v0 == 0.5f && at.particles[2].u0 == 0.0f && at.particles[2].v0 == 0.5f);
		}

		simulate_fx(effect, 0.65f, at);
		CHECK(at.particles.size() == 3 && at.particles[0].index == 2);		// the first two have lived their 500 ms
		simulate_fx(effect, 0.95f, at);
		CHECK(at.particles.empty());										// a counted loop ends

		// gravity: 100 percent is 800 units / s^2 downwards, whatever the effect's axes are
		effect.elems[0].gravity[0] = 100.0f;
		simulate_fx(effect, 0.25f, at);
		CHECK(!at.particles.empty() && fx_near(at.particles[0].position[2], 25.0f - 0.5f * 800.0f * 0.25f * 0.25f, 0.1f) && fx_near(at.particles[0].velocity[2], 100.0f - 800.0f * 0.25f, 1.0f));
		effect.elems[0].gravity[0] = 0.0f;

		// a loop without a count goes on: after ten seconds there still are the five youngest
		effect.elems[0].spawnLooping[1] = 0;
		simulate_fx(effect, 10.02f, at);
		CHECK(at.particles.size() == 5 && at.particles.back().index == 100);
		CHECK(fx_near(fx_preview_duration(effect), 0.4f + 0.5f));

		// a one shot element of three tails beside it, with a random life: the same call gives the same particles
		EditorElem burst;
		burst.name = "burst";
		burst.spawnOneShot[0] = 3;
		burst.lifeSpanMsec[0] = 400;
		burst.lifeSpanMsec[1] = 400;
		burst.spawnOrigin[1][0] = 50.0f;
		burst.velGraph[1][2].scale = 60.0f;			// the second graph set, in the world's frame: up
		burst.velGraph[1][2].curve[0] = fx_keys({ 0.0f, 1.0f, 1.0f, 1.0f });
		burst.editorFlags = { "absVel1" };
		burst.sizeGraph[0].scale = 2.0f;
		burst.sizeGraph[0].curve[0] = fx_keys({ 0.0f, 1.0f, 1.0f, 1.0f });
		burst.visualType = "tail";
		burst.visuals = { "a", "b" };
		effect.elems.push_back(burst);
		FxPreview first, second;
		simulate_fx(effect, 0.2f, first, 7);
		simulate_fx(effect, 0.2f, second, 7);
		std::size_t tails = 0;
		bool same = first.particles.size() == second.particles.size();
		for (std::size_t k = 0; same && k < first.particles.size(); k++)
		{
			const FxParticle& p = first.particles[k];
			same = std::memcmp(p.position, second.particles[k].position, sizeof(p.position)) == 0 && p.life == second.particles[k].life && p.material == second.particles[k].material;
			if (p.element == 1)
			{
				tails++;
				CHECK(p.shape == FxShape::tail && fx_near(p.position[1], 50.0f) && fx_near(p.position[2], 12.0f, 0.1f) && p.life >= 0.4f && p.life <= 0.8f && (p.material == "a" || p.material == "b"));
			}
		}
		CHECK(same && tails == 3);
		simulate_fx(effect, 0.2f, second, 8);
		CHECK(second.particles.size() == first.particles.size());

		// what is not drawn is named, and nothing of it is simulated
		EditorElem trail;
		trail.name = "ribbon";
		trail.visualType = "trail";
		trail.visuals = { "x" };
		trail.spawnOneShot[0] = 1;
		EditorElem off = loop;
		off.editorFlags.push_back("disabled");
		EditorEffect others;
		others.elems = { trail, off };
		simulate_fx(others, 0.1f, at);
		CHECK(at.particles.empty() && at.skipped.size() == 2 && at.skipped[0].find("trail") != std::string::npos && at.skipped[0].find("ribbon") != std::string::npos &&
			at.skipped[1].find("disabled") != std::string::npos);

		// the test effect of the library: its looping smoke and its sparks are there, its light is named
		EditorEffect spark;
		if (load_efx(fs::path(__FILE__).parent_path() / "fx" / "codol_test" / "spark_burst.efx", spark))
		{
			simulate_fx(spark, 0.3f, at);
			std::size_t smoke = 0, sparks = 0;
			for (const FxParticle& p : at.particles)
			{
				smoke += p.material == "gfx_smk_white_atlas";
				sparks += p.material == "gfx_spark_atlas";
				CHECK(p.age >= 0.0f && p.age < p.life && p.width > 0.0f && p.color[3] >= 0.0f && p.color[3] <= 1.0f);
			}
			CHECK(smoke >= 1 && smoke <= 2 && sparks > 4 && at.skipped.size() == 1 && at.skipped[0].find("light") != std::string::npos);
			CHECK(fx_preview_duration(spark) > 2.0f && fx_preview_duration(spark) < 6.0f);
		}
		else
		{
			CHECK(false);
		}
	}
}
