#pragma once

// The surfaces of one LOD of an XModel as the renderer uploads them: packed 32 byte vertices and 16 bit indices,
// unchanged. Port of Codol.Assets\Models\ModelExtractor.cs:140-222 ( ReadRenderSurfaces, MapStreamedSurfaces )
// and Codol.Assets\Models\StreamedSurfaceLoader.cs ( surfaces streamed from models/<name> in the encrypted IWDs ).

#include "codol_render/iwd.hpp"
#include "codol_zone/assets.hpp"

#include <string>
#include <vector>

namespace codol::view
{
	// ModelExtractor.cs:224 ( ModelRenderSurface )
	struct model_render_surface
	{
		int index = 0;
		zone::zaddr material = 0;					// Material header in the model's zone, 0 when the model has none for the slot
		std::vector<std::uint8_t> vertices;			// vertex_count * 32: c1::GfxPackedVertex
		int vertex_count = 0;
		std::vector<std::uint8_t> indices;			// triangle_count * 3 u16
		int triangle_count = 0;
		std::vector<int> rigid_bones;
	};

	// StreamedSurfaceLoader.cs: a models/<surfs> file - four blocks ( XModelSurfs + XSurfaces, vertex data, index
	// data, fixups ) that the game maps into memory and relocates. Parsed into a private address space.
	class streamed_surfaces
	{
	public:
		static constexpr int HEADER_SIZE = 88;
		static constexpr std::uint32_t SUPPORTED_VERSION = 1;

		// consumes the file; false with a text in *error when it is malformed
		bool map(const std::string& name, std::vector<std::uint8_t> file, std::string* error);

		std::uint32_t root() const { return bases_[0]; }	// the XModelSurfs
		// `size` bytes at a relocated address; empty when the range is in no block
		std::span<const std::uint8_t> bytes(std::uint32_t address, std::size_t size) const;

	private:
		std::vector<std::uint8_t> file_;
		std::uint32_t bases_[3] = {};
		std::uint32_t sizes_[3] = {};
		std::uint32_t offsets_[3] = {};
	};

	// `model_archives`: the "models/" entries of the IWDs, nullptr when the install has none.
	// `lod_count` receives XModel::numLods clamped to 0 .. 4.
	std::vector<model_render_surface> read_render_surfaces(const zone::Zone& zone, zone::zaddr model, int lod,
		const render::iwd_archive_set* model_archives, std::vector<std::string>& warnings, bool* streamed = nullptr);
}
