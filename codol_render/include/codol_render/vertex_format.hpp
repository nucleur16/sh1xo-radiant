#pragma once

// Vertex formats: MaterialVertexDeclaration routing + the vertex format of the geometry -> D3DVERTEXELEMENT9.
// Port of Codol.Assets\Rendering\GfxStateTables.cs:4-85 ( VertexFormats ).
//
// Format numbering ( GfxStateTables.cs:6-9 ):
//   0             generic 32 byte vertex ( 2D quads, post effects: GameMapRenderer.PostEffects.cs:193 )
//   1             GfxPackedVertex, 32 bytes ( XModel surfaces and effects: GameMapRenderer.cs:1238 )
//   2 + wvf       GfxWorldVertex, 44 bytes, wvf = MaterialTechniqueSet::worldVertFormat ( 0 .. 11 );
//                 formats 3 .. 13 add a second stream ( the world's vertex layer data ) for more texture layers
//   14, 15        the last two rows of the game's table ( 20 and 32 byte vertices; the C# renderer never uses them )
//
// Sources ( MaterialVertexDeclaration routing source ): 0 position, 1 color, 2 texcoord0, 3 normal, 4 tangent,
// 5 texcoord1, 6 texcoord2, 7 normalTransform0, 8 normalTransform1.
// Destinations: 0 position, 1 normal, 2 color0, 3 color1, 4 depth, 5 .. 12 texcoord0 .. 7.

#include "d3d9_constants.hpp"
#include "technique_set.hpp"

#include <optional>
#include <span>
#include <vector>

namespace codol::render
{
	namespace vertex_format
	{
		inline constexpr int GENERIC = 0;
		inline constexpr int PACKED = 1;
		inline constexpr int WORLD = 2;			// + MaterialTechniqueSet::worldVertFormat
		inline constexpr int COUNT = 16;
		inline constexpr int SOURCE_COUNT = 9;
		inline constexpr int DEST_COUNT = 14;
	}

	// One cell of the [16 formats][9 sources] table: where a vertex member lives. stream 0xFF: the format has no such member.
	struct vertex_source_info
	{
		std::uint8_t stream;
		std::uint8_t offset;
		std::uint8_t type;		// D3DDECLTYPE
	};

	// GfxStateTables.cs:11-29
	vertex_source_info vertex_source(int format, int source);

	// GfxStateTables.cs:31: { D3DDECLUSAGE, usage index } of a destination
	struct vertex_dest_info
	{
		std::uint8_t usage;
		std::uint8_t usage_index;
	};
	vertex_dest_info vertex_dest(int dest);

	// GfxStateTables.cs:33-52 ( Build ). The elements in routing order, stably sorted by stream; with `terminate`
	// a D3DDECL_END() is appended so that the result can go to IDirect3DDevice9::CreateVertexDeclaration directly.
	// std::nullopt when a source or destination is out of range or the format lacks one of the sources
	// ( the pass cannot be drawn with that geometry ).
	std::optional<std::vector<d3d::vertex_element>> build_vertex_elements(std::span<const vertex_routing> routing, int format, bool terminate = true);

	// GfxStateTables.cs:54-59: bytes per vertex of stream 0
	int stream0_stride(int format);

	// GfxStateTables.cs:61-73: bytes per vertex of stream 1 ( 0: the format has no second stream )
	int stream1_stride(int format);

	// GfxStateTables.cs:75-84: size of a D3DDECLTYPE
	int decl_type_size(std::uint8_t type);
}
