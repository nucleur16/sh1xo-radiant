#include "model_surfaces.hpp"

#include <algorithm>

namespace codol::view
{
	using render::rd_u16;
	using render::rd_u32;

	bool streamed_surfaces::map(const std::string& name, std::vector<std::uint8_t> file, std::string* error)
	{
		// StreamedSurfaceLoader.cs:19-48 ( ParseHeader )
		const auto fail = [&](std::string text)
		{
			render::set_error(error, name + ": " + text);
			return false;
		};
		if (file.size() < static_cast<std::size_t>(HEADER_SIZE))
		{
			return fail("streamed surfaces file is " + std::to_string(file.size()) + " bytes, shorter than the 88-byte header");
		}
		if (rd_u32(file.data()) != SUPPORTED_VERSION)
		{
			return fail("streamed surfaces file version " + std::to_string(rd_u32(file.data())) + ", expected 1");
		}
		struct block
		{
			std::uint32_t size, file_offset, fixup_start, fixup_count;
		};
		block blocks[4];
		for (int i = 0; i < 4; i++)
		{
			const std::uint8_t* d = file.data() + 8 + 20 * i;
			blocks[i] = { rd_u32(d), rd_u32(d + 4), rd_u32(d + 8), rd_u32(d + 12) };
			if (static_cast<std::uint64_t>(blocks[i].file_offset) + blocks[i].size > file.size())
			{
				return fail("block " + std::to_string(i) + " extends past the end of the file");
			}
		}
		if (blocks[3].size % 4 != 0 || blocks[3].size >= 0x4000)
		{
			return fail("fixup table size is not a multiple of 4 below 0x4000");
		}

		// StreamedSurfaceLoader.cs:102-137 ( Map ): three blocks at addresses of their own, then the relocations.
		// The game allocates the blocks; here each gets a fixed base in a private address space.
		for (int i = 0; i < 3; i++)
		{
			bases_[i] = 0x10000000u * static_cast<std::uint32_t>(i + 1);
			sizes_[i] = blocks[i].size;
			offsets_[i] = blocks[i].file_offset;
		}
		const std::uint8_t* fixups = file.data() + blocks[3].file_offset;
		const std::uint32_t fixup_count = blocks[3].size / 4;
		for (const int patched : { 0, 2, 1 })
		{
			const block& d = blocks[patched];
			if (static_cast<std::uint64_t>(d.fixup_start) + d.fixup_count > fixup_count)
			{
				return fail("block " + std::to_string(patched) + " fixup range exceeds the fixup table");
			}
			for (std::uint32_t k = d.fixup_start; k < d.fixup_start + d.fixup_count; k++)
			{
				const std::int32_t entry = static_cast<std::int32_t>(rd_u32(fixups + 4 * k));
				const std::int32_t target = static_cast<std::int32_t>(static_cast<std::uint32_t>(entry) << 29) >> 29;
				const std::int32_t offset = entry >> 3;
				if (target < 0 || target > 2 || offset < 0 || static_cast<std::int64_t>(offset) + 4 > d.size)
				{
					return fail("fixup " + std::to_string(k) + " of block " + std::to_string(patched) + " is out of range");
				}
				std::uint8_t* location = file.data() + d.file_offset + offset;
				const std::uint32_t value = rd_u32(location) + bases_[target];
				location[0] = static_cast<std::uint8_t>(value);
				location[1] = static_cast<std::uint8_t>(value >> 8);
				location[2] = static_cast<std::uint8_t>(value >> 16);
				location[3] = static_cast<std::uint8_t>(value >> 24);
			}
		}
		file_ = std::move(file);
		return true;
	}

	std::span<const std::uint8_t> streamed_surfaces::bytes(const std::uint32_t address, const std::size_t size) const
	{
		for (int i = 0; i < 3; i++)
		{
			if (size && address >= bases_[i] && static_cast<std::uint64_t>(address - bases_[i]) + size <= sizes_[i])
			{
				return std::span<const std::uint8_t>(file_.data() + offsets_[i] + (address - bases_[i]), size);
			}
		}
		return {};
	}

	std::vector<model_render_surface> read_render_surfaces(const zone::Zone& zone, const zone::zaddr model, const int lod,
		const render::iwd_archive_set* model_archives, std::vector<std::string>& warnings, bool* streamed)
	{
		// ModelExtractor.cs:140-202
		std::vector<model_render_surface> result;
		if (streamed)
		{
			*streamed = false;
		}
		c1::XModel header;
		if (!zone.get(model, header))
		{
			return result;
		}
		const int lod_count = std::clamp(static_cast<int>(static_cast<std::int8_t>(header.numLods)), 0, 4);
		if (lod < 0 || lod >= lod_count)
		{
			return result;
		}
		const c1::XModelLodInfo& info = header.lodInfo[lod];
		if (info.modelSurfs == 0 || zone.bytes(info.modelSurfs, sizeof(c1::XModelSurfs)).empty())
		{
			return result;
		}
		const c1::XModelSurfs surfs = zone.get<c1::XModelSurfs>(info.modelSurfs);
		std::uint32_t surfaces = surfs.surfs;
		int count = surfs.numsurfs;

		streamed_surfaces file;
		bool from_file = false;
		if (surfaces == 0)
		{
			// ModelExtractor.cs:204-221 ( MapStreamedSurfaces )
			const std::string surfs_name(zone.c_string(info.surfsName));
			if (!model_archives)
			{
				warnings.push_back("surfaces '" + surfs_name + "' are streamed but no IWD folder is available");
				return result;
			}
			const std::string path = "models/" + surfs_name;
			std::vector<std::uint8_t> data;
			std::string error;
			render::iwd_archive_set::hit hit;
			if (!model_archives->find(path, hit))
			{
				warnings.push_back("streamed surfaces '" + path + "' were not found in any IWD archive");
				return result;
			}
			if (!model_archives->read(path, data, &error) || !file.map(path, std::move(data), &error))
			{
				warnings.push_back("streamed surfaces '" + path + "': " + error);
				return result;
			}
			const auto root = file.bytes(file.root(), sizeof(c1::XModelSurfs));
			if (root.empty())
			{
				warnings.push_back("streamed surfaces '" + path + "': no XModelSurfs in block 0");
				return result;
			}
			surfaces = rd_u32(root.data() + offsetof(c1::XModelSurfs, surfs));
			count = rd_u16(root.data() + offsetof(c1::XModelSurfs, numsurfs));
			from_file = true;
			if (streamed)
			{
				*streamed = true;
			}
		}
		const auto read = [&](const std::uint32_t address, const std::size_t size) -> std::span<const std::uint8_t>
		{
			return from_file ? file.bytes(address, size) : (address && size ? zone.bytes(address, size) : std::span<const std::uint8_t>());
		};
		if (read(surfaces, 1).empty())
		{
			return result;
		}
		const int material_count = header.numsurfs;
		const zone::zaddr material_array = header.materialHandles;
		result.reserve(static_cast<std::size_t>(count));
		for (int s = 0; s < count; s++)
		{
			const auto raw = read(surfaces + static_cast<std::uint32_t>(s) * sizeof(c1::XSurface), sizeof(c1::XSurface));
			if (raw.empty())
			{
				continue;
			}
			c1::XSurface surface;
			std::memcpy(&surface, raw.data(), sizeof surface);
			const int vert_count = surface.vertCount;
			const int tri_count = surface.triCount;
			const auto verts = read(surface.verts0, static_cast<std::size_t>(vert_count) * sizeof(c1::GfxPackedVertex));
			const auto tris = read(surface.triIndices, static_cast<std::size_t>(tri_count) * 6);
			if (vert_count == 0 || tri_count == 0 || verts.empty() || tris.empty())
			{
				continue;
			}
			model_render_surface out;
			out.index = s;
			const int slot = info.surfIndex + s;
			// the material table is always in the model's zone, also for streamed surfaces
			out.material = slot < material_count && material_array != 0 ? zone.get<zone::zaddr>(material_array + 4u * static_cast<std::uint32_t>(slot)) : 0;
			out.vertices.assign(verts.begin(), verts.end());
			out.vertex_count = vert_count;
			out.indices.assign(tris.begin(), tris.end());
			out.triangle_count = tri_count;
			if (surface.vertListCount > 0 && surface.vertListCount < 256)
			{
				const auto lists = read(surface.vertList, surface.vertListCount * sizeof(c1::XRigidVertList));
				for (std::uint32_t k = 0; !lists.empty() && k < surface.vertListCount; k++)
				{
					out.rigid_bones.push_back(rd_u16(lists.data() + k * sizeof(c1::XRigidVertList)) / 64);
				}
			}
			result.push_back(std::move(out));
		}
		return result;
	}
}
