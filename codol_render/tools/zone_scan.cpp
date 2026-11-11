#include "tools.hpp"

#include "codol_render/build_1_1_1_13.hpp"
#include "codol_render/material.hpp"
#include "codol_render/shader_program.hpp"

#include <algorithm>
#include <cstdio>

// Stream layout of the structures the scanner follows, as the 1.1.1.13 loaders read them ( the same order as the
// IW4 loaders; established on the stock zones: every walked techset / material agrees with the owner's zone
// loader, see `codol_render_check zone` ):
//
//   MaterialTechniqueSet   0xCC header | name | for each of the 48 slots that is inline: MaterialTechnique
//   MaterialTechnique      8 byte header + passCount * 20 byte passes | per pass: [vertex declaration] [vertex
//                          shader] [pixel shader] [arguments] | name
//   vertex declaration     0x64 header | name
//   shader                 16 byte header | name | programSize * 4 bytes of byte code
//   arguments              count * 8 bytes | 16 bytes for every literal argument ( type 1 / 7 ) whose value is inline
//   Material               0x60 header | name | [technique set] | [texture table: count * 12 bytes, then per def its
//                          GfxImage ( water_t for semantic 11: not followed )] | [constant table: count * 32]
//                          | [state bits table: count * 8]
//   GfxImage               0x24 header | [load def: 16 bytes + resourceSize] | name
//
// "[x]" exists in the stream only when the pointer to it is inline ( 0xFFFFFFFF / 0xFFFFFFFE ).

namespace tools
{
	using namespace codol::render;

	namespace
	{
		bool is_inline(const std::uint32_t p) { return p == 0xFFFFFFFFu || p == 0xFFFFFFFEu; }

		// null, inline, or a reference into one of the loader's blocks
		bool plausible_pointer(const std::uint32_t p)
		{
			return p == 0 || is_inline(p) || (((p - 1) >> 28) <= 8 && p > 0x100);
		}

		struct walker
		{
			const std::vector<std::uint8_t>& src;	// the zone as stored
			std::vector<std::uint8_t>& dst;			// the copy that gets patched
			std::vector<std::pair<std::uint32_t, std::uint32_t>> patches;	// field offset, value: applied when the whole walk succeeds
			bool failed = false;
			bool stopped_at_water = false;
			std::string why;

			std::uint32_t u32(const std::size_t o) { return o + 4 <= src.size() ? rd_u32(src.data() + o) : (fail("read past the end"), 0u); }
			std::uint16_t u16(const std::size_t o) { return o + 2 <= src.size() ? rd_u16(src.data() + o) : (fail("read past the end"), std::uint16_t{ 0 }); }
			std::uint8_t u8(const std::size_t o) { return o < src.size() ? src[o] : (fail("read past the end"), std::uint8_t{ 0 }); }

			void fail(const char* text)
			{
				if (!failed)
				{
					failed = true;
					why = text;
				}
			}

			void patch(const std::size_t field, const std::size_t target)
			{
				patches.emplace_back(static_cast<std::uint32_t>(field), ZONE_BASE + static_cast<std::uint32_t>(target));
			}

			// a printable, zero terminated string at `at`; returns the offset behind its terminator
			std::size_t string(const std::size_t at, const char* what, std::string* out = nullptr)
			{
				std::size_t end = at;
				while (end < src.size() && end - at < 256 && src[end] >= 0x20 && src[end] < 0x7F)
				{
					end++;
				}
				if (end >= src.size() || src[end] != 0 || end == at)
				{
					fail(what);
					return at;
				}
				if (out)
				{
					out->assign(reinterpret_cast<const char*>(src.data() + at), end - at);
				}
				return end + 1;
			}

			// a pointer field to a string: patched when inline
			std::size_t string_field(const std::size_t field, std::size_t cursor, const char* what)
			{
				if (is_inline(u32(field)))
				{
					const std::size_t next = string(cursor, what);
					patch(field, cursor);
					return next;
				}
				return cursor;
			}

			std::size_t shader(std::size_t cursor)
			{
				const std::size_t header = cursor;
				cursor += build::SHADER_SIZE;
				cursor = string_field(header + build::SHADER_NAME, cursor, "shader name");
				if (is_inline(u32(header + build::SHADER_PROGRAM)))
				{
					const std::size_t bytes = static_cast<std::size_t>(u16(header + build::SHADER_PROGRAM_SIZE)) * 4;
					if (cursor + bytes > src.size() || shader_program_length(byte_span(src.data() + cursor, src.size() - cursor)) != bytes)
					{
						fail("shader program does not match its size");
						return cursor;
					}
					patch(header + build::SHADER_PROGRAM, cursor);
					cursor += bytes;
				}
				return cursor;
			}

			std::size_t technique(std::size_t cursor)
			{
				const std::size_t header = cursor;
				const std::uint32_t name = u32(header + build::TECHNIQUE_NAME);
				const std::size_t pass_count = u16(header + build::TECHNIQUE_PASS_COUNT);
				if (name == 0 || !plausible_pointer(name) || pass_count > 16)
				{
					fail("technique header");
					return cursor;
				}
				const std::size_t passes = header + build::TECHNIQUE_PASSES;
				cursor = passes + pass_count * build::PASS_SIZE;
				for (std::size_t p = 0; p < pass_count && !failed; p++)
				{
					const std::size_t pass = passes + p * build::PASS_SIZE;
					for (const std::uint32_t field : { build::PASS_VERTEX_DECL, build::PASS_VERTEX_SHADER, build::PASS_PIXEL_SHADER, build::PASS_ARGS })
					{
						if (!plausible_pointer(u32(pass + field)))
						{
							fail("pass pointer");
							return cursor;
						}
					}
					if (is_inline(u32(pass + build::PASS_VERTEX_DECL)))
					{
						patch(pass + build::PASS_VERTEX_DECL, cursor);
						const std::size_t decl = cursor;
						cursor += build::VERTEX_DECL_SIZE;
						cursor = string_field(decl + build::VERTEX_DECL_NAME, cursor, "vertex declaration name");
					}
					if (is_inline(u32(pass + build::PASS_VERTEX_SHADER)))
					{
						patch(pass + build::PASS_VERTEX_SHADER, cursor);
						cursor = shader(cursor);
					}
					if (is_inline(u32(pass + build::PASS_PIXEL_SHADER)))
					{
						patch(pass + build::PASS_PIXEL_SHADER, cursor);
						cursor = shader(cursor);
					}
					if (is_inline(u32(pass + build::PASS_ARGS)))
					{
						const std::size_t count = static_cast<std::size_t>(u8(pass + build::PASS_PER_PRIM_ARG_COUNT)) + u8(pass + build::PASS_PER_OBJ_ARG_COUNT)
							+ u8(pass + build::PASS_STABLE_ARG_COUNT);
						const std::size_t args = cursor;
						patch(pass + build::PASS_ARGS, args);
						cursor += count * build::ARG_SIZE;
						for (std::size_t i = 0; i < count && !failed; i++)
						{
							const std::size_t arg = args + i * build::ARG_SIZE;
							const std::uint16_t type = u16(arg + build::ARG_TYPE);
							if (type > 7)
							{
								fail("argument type");
								return cursor;
							}
							if ((type == 1 || type == 7) && is_inline(u32(arg + build::ARG_VALUE)))
							{
								patch(arg + build::ARG_VALUE, cursor);
								cursor += 16;
							}
						}
					}
				}
				if (failed)
				{
					return cursor;
				}
				return string_field(header + build::TECHNIQUE_NAME, cursor, "technique name");
			}

			std::size_t technique_set(const std::size_t header)
			{
				std::size_t cursor = header + build::TECHSET_SIZE;
				cursor = string_field(header + build::TECHSET_NAME, cursor, "technique set name");
				for (int t = 0; t < build::TECHNIQUE_COUNT && !failed; t++)
				{
					const std::size_t field = header + build::TECHSET_TECHNIQUES + 4 * static_cast<std::size_t>(t);
					if (is_inline(u32(field)))
					{
						patch(field, cursor);
						cursor = technique(cursor);
					}
				}
				return cursor;
			}

			std::size_t image(const std::size_t header)
			{
				std::size_t cursor = header + build::IMAGE_SIZE;
				const std::uint32_t load_def = u32(header + build::IMAGE_LOAD_DEF);
				if (!plausible_pointer(load_def) || !plausible_pointer(u32(header + build::IMAGE_NAME)))
				{
					fail("image pointer");
					return cursor;
				}
				if (is_inline(load_def))
				{
					patch(header + build::IMAGE_LOAD_DEF, cursor);
					const std::size_t size = u32(cursor + build::LOAD_DEF_RESOURCE_SIZE);
					cursor += build::LOAD_DEF_HEADER_SIZE + size;
					if (cursor > src.size())
					{
						fail("image data runs past the end");
						return cursor;
					}
				}
				return string_field(header + build::IMAGE_NAME, cursor, "image name");
			}

			std::size_t material(const std::size_t header)
			{
				std::size_t cursor = header + build::MATERIAL_SIZE;
				cursor = string_field(header + build::MATERIAL_NAME, cursor, "material name");
				if (is_inline(u32(header + build::MATERIAL_TECHNIQUE_SET)))
				{
					patch(header + build::MATERIAL_TECHNIQUE_SET, cursor);
					if (!is_inline(u32(cursor + build::TECHSET_NAME)))
					{
						fail("inline technique set without a name");
						return cursor;
					}
					cursor = technique_set(cursor);
				}
				if (failed)
				{
					return cursor;
				}
				const std::size_t texture_count = u8(header + build::MATERIAL_TEXTURE_COUNT);
				if (is_inline(u32(header + build::MATERIAL_TEXTURE_TABLE)))
				{
					const std::size_t table = cursor;
					patch(header + build::MATERIAL_TEXTURE_TABLE, table);
					cursor += texture_count * build::TEXTURE_DEF_SIZE;
					for (std::size_t i = 0; i < texture_count && !failed; i++)
					{
						const std::size_t def = table + i * build::TEXTURE_DEF_SIZE;
						if (!is_inline(u32(def + build::TEXTURE_DEF_IMAGE)))
						{
							continue;
						}
						if (u8(def + build::TEXTURE_DEF_SEMANTIC) == texture_semantic::WATER_MAP)
						{
							stopped_at_water = true;	// a water_t follows: its layout is not ported, the rest cannot be located
							return cursor;
						}
						patch(def + build::TEXTURE_DEF_IMAGE, cursor);
						cursor = image(cursor);
					}
				}
				if (failed)
				{
					return cursor;
				}
				if (is_inline(u32(header + build::MATERIAL_CONSTANT_TABLE)))
				{
					patch(header + build::MATERIAL_CONSTANT_TABLE, cursor);
					cursor += static_cast<std::size_t>(u8(header + build::MATERIAL_CONSTANT_COUNT)) * build::CONSTANT_DEF_SIZE;
				}
				if (is_inline(u32(header + build::MATERIAL_STATE_BITS_TABLE)))
				{
					patch(header + build::MATERIAL_STATE_BITS_TABLE, cursor);
					cursor += static_cast<std::size_t>(u8(header + build::MATERIAL_STATE_BITS_COUNT)) * 8;
				}
				if (cursor > src.size())
				{
					fail("material tables run past the end");
				}
				return cursor;
			}

			void apply()
			{
				for (const auto& [field, value] : patches)
				{
					for (int i = 0; i < 4; i++)
					{
						dst[field + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (8 * i));
					}
				}
			}
		};

		bool name_like(const std::vector<std::uint8_t>& d, const std::size_t at, std::string& name)
		{
			std::size_t end = at;
			while (end < d.size() && end - at < 128)
			{
				const std::uint8_t c = d[end];
				const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$' || c == ','
					|| c == '.' || c == '/' || c == '+' || c == '-' || c == '~' || c == '#' || c == '@' || c == '*' || c == ' ' || c == '&' || c == '!';
				if (!ok)
				{
					break;
				}
				end++;
			}
			if (end >= d.size() || d[end] != 0 || end - at < 2)
			{
				return false;
			}
			name.assign(reinterpret_cast<const char*>(d.data() + at), end - at);
			return true;
		}

		bool techset_shaped(const std::vector<std::uint8_t>& d, const std::size_t o, std::string& name)
		{
			if (o + build::TECHSET_SIZE + 2 > d.size() || rd_u32(d.data() + o) != 0xFFFFFFFFu || d[o + 4] > 11)
			{
				return false;
			}
			for (int i = 5; i < 12; i++)
			{
				if (d[o + static_cast<std::size_t>(i)] != 0)
				{
					return false;
				}
			}
			bool any = false;
			for (int t = 0; t < build::TECHNIQUE_COUNT; t++)
			{
				const std::uint32_t p = rd_u32(d.data() + o + build::TECHSET_TECHNIQUES + 4 * static_cast<std::size_t>(t));
				if (!plausible_pointer(p))
				{
					return false;
				}
				any = any || p != 0;
			}
			// a techset without any technique is the stub of a ",name" reference; such a pattern alone is too weak
			return name_like(d, o + build::TECHSET_SIZE, name) && (any || name[0] == ',');
		}

		bool material_shaped(const std::vector<std::uint8_t>& d, const std::size_t o, std::string& name)
		{
			if (o + build::MATERIAL_SIZE + 2 > d.size() || rd_u32(d.data() + o) != 0xFFFFFFFFu)
			{
				return false;
			}
			const std::uint8_t* m = d.data() + o;
			const std::uint32_t techset = rd_u32(m + build::MATERIAL_TECHNIQUE_SET), textures = rd_u32(m + build::MATERIAL_TEXTURE_TABLE),
				constants = rd_u32(m + build::MATERIAL_CONSTANT_TABLE), states = rd_u32(m + build::MATERIAL_STATE_BITS_TABLE);
			if (!plausible_pointer(techset) || !plausible_pointer(textures) || !plausible_pointer(constants) || !plausible_pointer(states))
			{
				return false;
			}
			const int texture_count = m[build::MATERIAL_TEXTURE_COUNT], constant_count = m[build::MATERIAL_CONSTANT_COUNT],
				state_count = m[build::MATERIAL_STATE_BITS_COUNT];
			if ((texture_count != 0) != (textures != 0) || (constant_count != 0) != (constants != 0) || (state_count != 0) != (states != 0))
			{
				return false;
			}
			if (texture_count > 32 || constant_count > 64 || state_count > 96 || m[build::MATERIAL_CAMERA_REGION] > 16
				|| m[77] != 0 || m[78] != 0 || m[79] != 0)
			{
				return false;
			}
			// a material that is defined here has a techset and state bits; every state bits entry is an index into
			// its table or 0xFF. A ",name" reference is a header of zeros.
			if (!name_like(d, o + build::MATERIAL_SIZE, name))
			{
				return false;
			}
			if (name[0] == ',')
			{
				return techset == 0 && textures == 0 && states == 0;
			}
			if (techset == 0 || state_count == 0)
			{
				return false;
			}
			for (int t = 0; t < build::TECHNIQUE_COUNT; t++)
			{
				const std::uint8_t entry = m[build::MATERIAL_STATE_BITS_ENTRY + static_cast<std::size_t>(t)];
				if (entry != 0xFF && entry >= state_count)
				{
					return false;
				}
			}
			return true;
		}

		void find_shaders(scanned_zone& out)
		{
			const auto& d = out.image;
			for (std::size_t o = 0; o + 16 <= d.size(); o++)
			{
				// 0x0300 / 0x0200 / .. version token of a vertex ( FFFE ) or pixel ( FFFF ) shader, followed by the
				// CTAB comment every compiled shader of the game starts with
				if (d[o + 2] < 0xFE || d[o + 3] != 0xFF || d[o + 1] < 2 || d[o + 1] > 3 || d[o + 4] != 0xFE || d[o + 5] != 0xFF
					|| std::memcmp(d.data() + o + 8, "CTAB", 4) != 0)
				{
					continue;
				}
				const std::size_t length = shader_program_length(byte_span(d.data() + o, d.size() - o));
				if (length == 0)
				{
					continue;
				}
				scanned_shader shader;
				shader.is_pixel_shader = d[o + 2] == 0xFF;
				shader.program_offset = static_cast<std::uint32_t>(o);
				shader.program_size = static_cast<std::uint32_t>(length);
				// the MaterialShader header: 16 bytes | name | program, or 16 bytes | program when the name is a reference
				const auto header_ok = [&](const std::size_t header)
				{
					return is_inline(rd_u32(d.data() + header + build::SHADER_PROGRAM))
						&& static_cast<std::size_t>(rd_u16(d.data() + header + build::SHADER_PROGRAM_SIZE)) * 4 == length;
				};
				if (o >= 18 && d[o - 1] == 0)
				{
					std::size_t start = o - 1;
					while (start > 0 && d[start - 1] >= 0x20 && d[start - 1] < 0x7F && o - start < 200)
					{
						start--;
					}
					// the name may begin later than the first printable byte ( the header's last bytes can be printable ):
					// try every start until a header fits
					for (std::size_t s = start; s + 1 < o && !shader.header_matches; s++)
					{
						if (s >= build::SHADER_SIZE && is_inline(rd_u32(d.data() + s - build::SHADER_SIZE)) && header_ok(s - build::SHADER_SIZE))
						{
							shader.header_matches = true;
							shader.name.assign(reinterpret_cast<const char*>(d.data() + s), o - 1 - s);
						}
					}
				}
				if (!shader.header_matches && o >= build::SHADER_SIZE && !is_inline(rd_u32(d.data() + o - build::SHADER_SIZE)) && header_ok(o - build::SHADER_SIZE))
				{
					shader.header_matches = true;	// name stored as a reference
				}
				out.shaders.push_back(std::move(shader));
				o += length - 1;
			}
		}
	}

	bool scan_zone(const std::filesystem::path& path, scanned_zone& out, std::string& error)
	{
		std::vector<std::uint8_t> source;
		if (!read_file(path, source))
		{
			error = "cannot read " + path.string();
			return false;
		}
		if (source.size() >= 0x30000000)
		{
			error = "zone too large for the scanner's address space";
			return false;
		}
		out = {};
		out.image = source;

		// Technique sets first ( a material's inline techset is found here too, and again through its material ).
		// Everything a successful walk covered is skipped: its bytes cannot hold another header.
		std::vector<std::pair<std::size_t, std::size_t>> walked_techsets;	// [begin, end)
		std::string name;
		for (std::size_t o = 0; o + build::TECHSET_SIZE < source.size(); o++)
		{
			if (source[o] != 0xFF || !techset_shaped(source, o, name))
			{
				continue;
			}
			out.techset_candidates++;
			walker w{ source, out.image };
			const std::size_t end = w.technique_set(o);
			if (w.failed)
			{
				out.techset_rejected++;
				char text[64];
				std::snprintf(text, sizeof text, "techset candidate at 0x%zX '", o);
				out.rejected.push_back(text + name + "': " + w.why);
				continue;
			}
			w.apply();
			out.techsets.push_back(ZONE_BASE + static_cast<std::uint32_t>(o));
			walked_techsets.emplace_back(o, end);
			o = end - 1;
		}

		for (std::size_t o = 0; o + build::MATERIAL_SIZE < source.size(); o++)
		{
			if (source[o] != 0xFF || !material_shaped(source, o, name))
			{
				continue;
			}
			out.material_candidates++;
			walker w{ source, out.image };
			const std::size_t end = w.material(o);
			if (w.failed)
			{
				out.material_rejected++;
				char text[64];
				std::snprintf(text, sizeof text, "material candidate at 0x%zX '", o);
				out.rejected.push_back(text + name + "': " + w.why);
				continue;
			}
			w.apply();
			out.materials.push_back(ZONE_BASE + static_cast<std::uint32_t>(o));
			if (w.stopped_at_water)
			{
				out.partial_materials.push_back(ZONE_BASE + static_cast<std::uint32_t>(o));
			}
			else
			{
				o = end - 1;
			}
		}

		find_shaders(out);
		return true;
	}
}
