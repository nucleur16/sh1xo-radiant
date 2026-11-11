#include "test.hpp"

#include "codol_render/vertex_format.hpp"

using namespace codol::render;

void test_vertex_format()
{
	using d3d::vertex_element;

	// GfxStateTables.cs:11-29, row 2 ( world vertex, worldVertFormat 0 ): position FLOAT4 @0, color D3DCOLOR @16,
	// texcoord0 FLOAT4 @20, normal UBYTE4 @36, tangent UBYTE4 @40; no second stream
	{
		const vertex_routing routing[] = { { 0, 0 }, { 1, 2 }, { 2, 5 }, { 3, 1 }, { 4, 6 } };
		const auto elements = build_vertex_elements(routing, vertex_format::WORLD);
		CHECK(elements.has_value());
		if (elements)
		{
			const std::vector<vertex_element> expected =
			{
				{ 0, 0, 3, 0, 0, 0 },		// POSITION0
				{ 0, 16, 4, 0, 10, 0 },		// COLOR0
				{ 0, 20, 3, 0, 5, 0 },		// TEXCOORD0
				{ 0, 36, 5, 0, 3, 0 },		// NORMAL0
				{ 0, 40, 5, 0, 5, 1 },		// TEXCOORD1 <- tangent
				{ 0xFF, 0, 17, 0, 0, 0 },	// D3DDECL_END
			};
			CHECK(*elements == expected);
		}
		const auto bare = build_vertex_elements(routing, vertex_format::WORLD, false);
		CHECK(bare && bare->size() == 5);
	}

	// row 1 ( GfxPackedVertex ): position FLOAT4 @0, color @16, texcoord0 UBYTE4 @20, normal UBYTE4 @24, tangent UBYTE4 @28
	{
		const vertex_routing routing[] = { { 0, 0 }, { 3, 1 }, { 2, 5 }, { 4, 7 }, { 1, 3 } };
		const auto elements = build_vertex_elements(routing, vertex_format::PACKED, false);
		CHECK(elements.has_value());
		if (elements)
		{
			const std::vector<vertex_element> expected =
			{
				{ 0, 0, 3, 0, 0, 0 }, { 0, 24, 5, 0, 3, 0 }, { 0, 20, 5, 0, 5, 0 }, { 0, 28, 5, 0, 5, 2 }, { 0, 16, 4, 0, 10, 1 },
			};
			CHECK(*elements == expected);
		}
	}

	// row 0 ( generic ): position FLOAT4 @0, color @16, texcoord0 FLOAT2 @20, normal UBYTE4 @28; no tangent
	{
		const vertex_routing routing[] = { { 0, 0 }, { 2, 5 } };
		const auto elements = build_vertex_elements(routing, vertex_format::GENERIC, false);
		CHECK(elements.has_value());
		if (elements)
		{
			const std::vector<vertex_element> expected = { { 0, 0, 3, 0, 0, 0 }, { 0, 20, 1, 0, 5, 0 } };
			CHECK(*elements == expected);
		}
		const vertex_routing tangent[] = { { 0, 0 }, { 4, 6 } };
		CHECK(!build_vertex_elements(tangent, vertex_format::GENERIC).has_value());
	}

	// row 4 ( worldVertFormat 2 ): texcoord1 FLOAT2 on stream 1 @0, normalTransform0 D3DCOLOR on stream 1 @8.
	// The elements are stably sorted by stream whatever the routing order ( GfxStateTables.cs:44-49 ).
	{
		const vertex_routing routing[] = { { 5, 6 }, { 0, 0 }, { 7, 8 }, { 2, 5 } };
		const auto elements = build_vertex_elements(routing, vertex_format::WORLD + 2, false);
		CHECK(elements.has_value());
		if (elements)
		{
			const std::vector<vertex_element> expected =
			{
				{ 0, 0, 3, 0, 0, 0 }, { 0, 20, 3, 0, 5, 0 }, { 1, 0, 1, 0, 5, 1 }, { 1, 8, 4, 0, 5, 3 },
			};
			CHECK(*elements == expected);
		}
	}

	// out of range routing / format
	{
		const vertex_routing bad_source[] = { { 9, 0 } };
		const vertex_routing bad_dest[] = { { 0, 14 } };
		const vertex_routing depth[] = { { 0, 4 }, { 0, 13 } };
		CHECK(!build_vertex_elements(bad_source, 2).has_value());
		CHECK(!build_vertex_elements(bad_dest, 2).has_value());
		CHECK(!build_vertex_elements(depth, 16).has_value());
		CHECK(!build_vertex_elements(depth, -1).has_value());
		const auto elements = build_vertex_elements(depth, 2, false);
		CHECK(elements.has_value());
		if (elements)
		{
			const std::vector<vertex_element> expected = { { 0, 0, 3, 0, 12, 0 }, { 0, 0, 3, 0, 0, 0 } };
			CHECK(*elements == expected);
		}
		CHECK(build_vertex_elements({}, 2, false).has_value());		// an empty routing is an empty declaration
	}

	// GfxStateTables.cs:54-73
	CHECK_EQ(stream0_stride(0), 32);
	CHECK_EQ(stream0_stride(1), 32);
	CHECK_EQ(stream0_stride(2), 44);
	CHECK_EQ(stream0_stride(13), 44);
	CHECK_EQ(stream0_stride(14), 20);
	CHECK_EQ(stream0_stride(15), 32);
	const int stream1[16] = { 0, 0, 0, 8, 12, 16, 20, 24, 24, 28, 32, 32, 36, 40, 0, 0 };
	for (int format = 0; format < 16; format++)
	{
		CHECK_EQ(stream1_stride(format), stream1[format]);
	}
	CHECK_EQ(decl_type_size(0), 4);
	CHECK_EQ(decl_type_size(3), 16);
	CHECK_EQ(decl_type_size(4), 4);
	CHECK_EQ(decl_type_size(16), 8);
	CHECK_EQ(decl_type_size(17), 0);

	CHECK_EQ(vertex_source(2, 0).stream, 0);
	CHECK_EQ(vertex_source(2, 5).stream, 255);
	CHECK_EQ(vertex_source(13, 8).offset, 36);
	CHECK_EQ(vertex_dest(12).usage, 5);
	CHECK_EQ(vertex_dest(12).usage_index, 7);
	CHECK_EQ(vertex_dest(4).usage, 12);
}
