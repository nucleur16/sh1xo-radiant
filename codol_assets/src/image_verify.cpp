// Reading images back through codol_render, the reader the renderer uses: the check that what the compiler wrote
// is what the game side decodes.

#include "codol_assets/image.hpp"

#include "codol_render/texture.hpp"

namespace codol::assets
{
	Status read_iwi_levels(const std::span<const std::uint8_t> file, const std::filesystem::path& exe, IwiLevels& out)
	try
	{
		namespace r = codol::render;
		out = {};
		r::iwi_file iwi;
		std::string error;
		if (!r::parse_iwi(file, iwi, &error))
		{
			return Status::error("iwi: " + error);
		}
		std::shared_ptr<const r::wavelet_tables> tables;
		if (iwi.header.is_wavelet())
		{
			tables = r::wavelet_tables::load_from_executable(exe, &error);
			if (!tables)
			{
				return Status::error("iwi: wavelet tables: " + error);
			}
		}
		r::texture_upload upload;
		if (!r::build_texture_upload(iwi, tables.get(), upload, &error))
		{
			return Status::error("iwi: " + error);
		}
		out.flags = iwi.header.flags;
		out.format = static_cast<std::uint8_t>(iwi.header.format);
		out.width = iwi.header.width;
		out.height = iwi.header.height;
		out.faces = upload.faces;
		out.levelCount = upload.levels;
		out.fileSizeForPicmip = iwi.header.file_size_for_picmip;
		out.levels.resize(static_cast<std::size_t>(upload.faces) * upload.levels);
		for (const r::texture_surface& s : upload.surfaces)
		{
			r::rgba_image image;
			if (!r::upload_surface_to_rgba(upload, s, image, &error))
			{
				return Status::error("iwi: " + error);
			}
			Bitmap& b = out.levels[static_cast<std::size_t>(s.level) * upload.faces + s.face];
			b.width = image.width;
			b.height = image.height;
			b.rgba = std::move(image.pixels);
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}
}
