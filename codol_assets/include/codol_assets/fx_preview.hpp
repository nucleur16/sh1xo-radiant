#pragma once

// An effect over time, for a preview: where the particles of an EditorEffect are at a moment, how large, which
// colour, which cell of their material's atlas. Input is the editor's form of an effect - what an .efx says, or
// what extract_fx makes of a shipped FxEffectDef - so a project's effect and one of the game go the same way.
//
// This is NOT the game's effect system. It follows the definitions as the effects editor explains them:
//
//   done       looping and one shot spawning with delay and life span; spawn origin ranges and the offset cylinder;
//              the local and world velocity graphs integrated over the particle's life ( both curves, blended by a
//              random number per particle where the element says so ); gravity; size ( width / height ), colour,
//              alpha and rotation graphs; atlas cell ( fixed, random, indexed, played over life or at a frame rate );
//              one visual picked per particle
//   simplified a cloud / spark cloud is one particle, not a swarm; a tail and a line are a particle with its
//              velocity ( the drawer stretches it ); an oriented sprite reports the effect's forward axis as its
//              normal; fade in / out by distance, collision, elasticity, lighting are left out
//   not done   trails ( a ribbon behind a MOVING effect: an effect that stands still has none ), spark fountains,
//              models, lights, sounds, decals, runners and the effects an element starts on impact, on death or
//              as emission: they are named in FxPreview::skipped, nothing of them is simulated
//
// The effect stands at the origin with its forward axis ( local x ) pointing up, as playfx( fx, origin ) without
// a direction plays it. All numbers that are "random" are a hash of ( seed, element, particle, which ), so a moment
// can be computed again and drawn frame after frame without state.

#include "codol_assets/fx.hpp"

namespace codol::assets
{
	enum class FxShape : std::uint8_t
	{
		billboard,		// faces the camera
		oriented,		// lies in the plane across `normal`
		rotated,		// a billboard turned by `rotation` ( as billboard here )
		tail,			// stretched back along `velocity` by its height
		line,			// from the position back along `velocity`
		cloud,			// one particle standing for a swarm
	};

	struct FxParticle
	{
		int element = 0;				// index into EditorEffect::elems
		int index = 0;					// which particle of the element
		FxShape shape = FxShape::billboard;
		std::string_view material;		// points into the effect: valid while the effect lives
		float position[3] = {};			// world, z up
		float velocity[3] = {};			// units per second, world
		float normal[3] = { 0, 0, 1 };
		float width = 0, height = 0;	// units, edge to edge
		float rotation = 0;				// degrees
		float color[4] = { 1, 1, 1, 1 };
		float age = 0, life = 0;		// seconds
		float u0 = 0, v0 = 0, u1 = 1, v1 = 1;	// the atlas cell
	};

	struct FxPreview
	{
		std::vector<FxParticle> particles;
		std::vector<std::string> skipped;	// "element 2 ( trail ): not drawn - .."
	};

	// How long one pass of the effect takes: the latest moment a one shot particle can still live, and for looping
	// elements with a count the end of their last particle; an element that loops for ever counts with four of
	// its intervals plus a life span. At least 0.5 s.
	float fx_preview_duration(const EditorEffect& effect);

	// the particles alive `seconds` after the effect started ( elements that loop for ever keep spawning )
	void simulate_fx(const EditorEffect& effect, float seconds, FxPreview& out, std::uint32_t seed = 1);

	// a curve of a graph at t ( 0 .. 1 ): linear between its keys, the first / last value outside; `dimension`
	// picks the value of a key ( a colour has three ). 0 for a curve without keys
	float fx_curve_value(const FxCurve& curve, float t, int dimension = 0);
}
