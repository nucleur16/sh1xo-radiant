#include "test.hpp"

#include "codol_render/technique_choice.hpp"

using namespace codol::render;

namespace
{
	std::uint64_t slots(std::initializer_list<int> types)
	{
		std::uint64_t mask = 0;
		for (const int t : types)
		{
			mask |= 1ull << t;
		}
		return mask;
	}
}

void test_technique_choice()
{
	// GameMapRenderer.cs:1391-1427, every branch. Slot numbers: unlit 4, emissive 5, lit 10 ( 9 without fog ),
	// sun 12 ( 11 ), sun shadow 14, spot 16 ( 15 ), spot shadow 18, omni 20 ( 19 ), omni shadow 22.
	const std::uint64_t all = slots({ 4, 5, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 });
	technique_choice c;
	c.slots = all;
	c.last_sun_primary_light_index = 1;

	// no primary light -> lit
	CHECK_EQ(choose_technique(c), 10);
	// emissive range -> emissive, or unlit when the set has no emissive technique
	c.emissive = true;
	CHECK_EQ(choose_technique(c), 5);
	c.slots = all & ~slots({ 5 });
	CHECK_EQ(choose_technique(c), 4);
	c.slots = 0;
	CHECK_EQ(choose_technique(c), 4);		// unlit is returned even when absent: the caller checks the slot
	c.emissive = false;
	c.slots = all;

	// the sun ( index <= lastSunPrimaryLightIndex ): shadowed only while a sun shadow map exists and the set has slot 14
	c.primary_light_index = 1;
	c.light_type = 1;
	CHECK_EQ(choose_technique(c), 12);
	c.sun_shadows_active = true;
	CHECK_EQ(choose_technique(c), 14);
	c.slots = all & ~slots({ 14 });
	CHECK_EQ(choose_technique(c), 12);
	c.slots = all & ~slots({ 14, 12 });
	CHECK_EQ(choose_technique(c), 11);		// lit - 1: the same technique without distance fog
	c.slots = all & ~slots({ 14, 12, 11 });
	CHECK_EQ(choose_technique(c), 10);		// then plain lit
	c.slots = slots({ 4, 5 });
	CHECK_EQ(choose_technique(c), 5);		// then emissive
	c.slots = slots({ 4 });
	CHECK_EQ(choose_technique(c), 4);
	c.slots = all;
	// the light type does not matter for sun indices
	c.light_type = 2;
	c.sun_shadows_active = false;
	CHECK_EQ(choose_technique(c), 12);

	// spot light
	c.primary_light_index = 3;
	c.light_type = 2;
	CHECK_EQ(choose_technique(c), 16);
	c.light_shadowed = true;
	CHECK_EQ(choose_technique(c), 18);
	c.slots = all & ~slots({ 18 });
	CHECK_EQ(choose_technique(c), 16);		// shadowed but no shadow technique: the unshadowed one
	c.slots = all & ~slots({ 18, 16 });
	CHECK_EQ(choose_technique(c), 15);
	c.slots = all & ~slots({ 18, 16, 15 });
	CHECK_EQ(choose_technique(c), 10);
	c.slots = all & ~slots({ 16 });			// slot 18 exists: taken as is
	CHECK_EQ(choose_technique(c), 18);
	c.slots = all;
	c.light_shadowed = false;
	c.slots = all & ~slots({ 16 });
	CHECK_EQ(choose_technique(c), 15);

	// omni light
	c.slots = all;
	c.light_type = 3;
	CHECK_EQ(choose_technique(c), 20);
	c.light_shadowed = true;
	CHECK_EQ(choose_technique(c), 22);
	c.slots = all & ~slots({ 22 });
	CHECK_EQ(choose_technique(c), 20);
	c.slots = all & ~slots({ 22, 20 });
	CHECK_EQ(choose_technique(c), 19);
	c.light_shadowed = false;

	// a light of another type keeps lit; "shadowed" with a missing lit slot then falls to the omni technique,
	// exactly as the C# expression does ( GameMapRenderer.cs:1407-1410 )
	c.slots = all;
	c.light_type = 1;
	CHECK_EQ(choose_technique(c), 10);
	c.light_shadowed = true;
	CHECK_EQ(choose_technique(c), 10);
	c.slots = all & ~slots({ 10 });
	CHECK_EQ(choose_technique(c), 20);
	c.light_shadowed = false;
	CHECK_EQ(choose_technique(c), 9);

	// an index that is not a light of the map ( light_type 0 ) -> lit
	c.slots = all;
	c.light_type = 0;
	c.primary_light_index = 200;
	c.light_shadowed = true;
	CHECK_EQ(choose_technique(c), 10);

	// constexpr
	static_assert(choose_technique(technique_choice{ true, 0, 0, 0, false, false, 1ull << 5 }) == 5);

	// with a parsed technique set
	render_technique_set set;
	set.techniques.resize(1);
	set.slots[10] = 0;
	set.slots[12] = 0;
	technique_choice from_set;
	from_set.slots = set.slot_mask();
	from_set.primary_light_index = 1;
	from_set.last_sun_primary_light_index = 1;
	CHECK_EQ(choose_technique(from_set), 12);
}
