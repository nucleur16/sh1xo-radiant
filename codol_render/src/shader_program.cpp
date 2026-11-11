#include "codol_render/shader_program.hpp"

#include <algorithm>
#include <cstdio>

namespace codol::render
{
	zone_view flat_zone_view(const byte_span data, const std::uint32_t base)
	{
		zone_view view;
		view.read = [data, base](const std::uint32_t ptr, const std::uint32_t size) -> byte_span
		{
			if (ptr == 0 || ptr < base || size == 0)
			{
				return {};
			}
			const std::uint64_t offset = static_cast<std::uint64_t>(ptr) - base;
			if (offset + size > data.size())
			{
				return {};
			}
			return data.subspan(static_cast<std::size_t>(offset), size);
		};
		view.string = [data, base](const std::uint32_t ptr, std::string& out) -> bool
		{
			if (ptr == 0 || ptr < base || static_cast<std::uint64_t>(ptr) - base >= data.size())
			{
				return false;
			}
			const std::size_t offset = ptr - base;
			const auto* begin = data.data() + offset;
			const auto* end = static_cast<const std::uint8_t*>(std::memchr(begin, 0, data.size() - offset));
			if (!end)
			{
				return false;
			}
			out.assign(reinterpret_cast<const char*>(begin), static_cast<std::size_t>(end - begin));
			return true;
		};
		return view;
	}

	std::string hex32(const std::uint32_t value)
	{
		char text[16];
		std::snprintf(text, sizeof text, "0x%08X", value);
		return text;
	}

	const shader_constant* shader_program::find_constant(const register_set set, const int register_index, int* row) const
	{
		// ShaderProgram.cs:44-58
		for (const auto& c : constants)
		{
			if (c.set == set && register_index >= c.register_index
				&& register_index < c.register_index + std::max<int>(c.register_count, 1))
			{
				if (row)
				{
					*row = register_index - c.register_index;
				}
				return &c;
			}
		}
		if (row)
		{
			*row = 0;
		}
		return nullptr;
	}

	namespace
	{
		// ShaderProgram.cs:96-120. `table` starts behind the "CTAB" tag: it is the D3DXSHADER_CONSTANTTABLE
		// { Size, Creator, Version, Constants, ConstantInfo, Flags, Target }, all offsets relative to its start.
		void read_constant_table(const byte_span table, shader_program& out)
		{
			const auto text = [&](const std::int32_t offset, std::string& value) -> bool
			{
				if (offset <= 0 || static_cast<std::size_t>(offset) >= table.size())
				{
					return false;
				}
				const auto* begin = table.data() + offset;
				const auto* end = static_cast<const std::uint8_t*>(std::memchr(begin, 0, table.size() - offset));
				const std::size_t length = end ? static_cast<std::size_t>(end - begin) : table.size() - offset;
				value.assign(reinterpret_cast<const char*>(begin), length);
				return true;
			};

			if (!text(rd_s32(table.data() + 4), out.creator))
			{
				out.creator.clear();
			}
			const std::int32_t count = rd_s32(table.data() + 12);
			const std::int32_t info = rd_s32(table.data() + 16);
			if (!text(rd_s32(table.data() + 24), out.target))
			{
				out.target.clear();
			}
			for (std::int32_t i = 0; i < count; i++)
			{
				const std::int64_t entry = static_cast<std::int64_t>(info) + 20ll * i;
				if (entry < 0 || entry + 20 > static_cast<std::int64_t>(table.size()))
				{
					break;
				}
				const auto* e = table.data() + entry;
				shader_constant constant;
				if (!text(rd_s32(e), constant.name))
				{
					constant.name = "c" + std::to_string(i);
				}
				constant.set = static_cast<register_set>(rd_u16(e + 4));
				constant.register_index = rd_u16(e + 6);
				constant.register_count = rd_u16(e + 8);
				out.constants.push_back(std::move(constant));
			}
		}
	}

	bool parse_shader_program(const byte_span code, shader_program& out, std::string* error)
	{
		try
		{
			out = {};
			// ShaderProgram.cs:62-70
			if (code.size() < 8 || code.size() % 4 != 0)
			{
				set_error(error, "shader program of " + std::to_string(code.size()) + " bytes");
				return false;
			}
			out.version = rd_u32(code.data());
			if ((out.version >> 16) != 0xFFFE && (out.version >> 16) != 0xFFFF)
			{
				set_error(error, "not a Direct3D 9 shader (version token " + hex32(out.version) + ")");
				return false;
			}
			// ShaderProgram.cs:73-92: the comment tokens that follow the version token
			std::size_t offset = 4;
			while (offset + 8 <= code.size())
			{
				const std::uint32_t token = rd_u32(code.data() + offset);
				if ((token & 0xFFFF) != 0xFFFE)
				{
					break;
				}
				const std::size_t length = static_cast<std::size_t>((token >> 16) & 0x7FFF) * 4;
				const std::size_t body = offset + 4;
				if (body + length > code.size())
				{
					break;
				}
				if (length >= 32 && std::memcmp(code.data() + body, "CTAB", 4) == 0)
				{
					read_constant_table(code.subspan(body + 4, length - 4), out);
				}
				offset = body + length;
			}
			return true;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}

	std::size_t shader_program_length(const byte_span code)
	{
		if (code.size() < 8)
		{
			return 0;
		}
		const std::uint32_t version = rd_u32(code.data());
		if ((version >> 16) != 0xFFFE && (version >> 16) != 0xFFFF)
		{
			return 0;
		}
		const std::uint32_t major = (version >> 8) & 0xFF;
		std::size_t offset = 4;
		while (offset + 4 <= code.size())
		{
			const std::uint32_t token = rd_u32(code.data() + offset);
			offset += 4;
			if (token == 0x0000FFFF)
			{
				return offset;
			}
			if ((token & 0xFFFF) == 0xFFFE)
			{
				offset += static_cast<std::size_t>((token >> 16) & 0x7FFF) * 4;	// comment
			}
			else if (major >= 2)
			{
				offset += static_cast<std::size_t>((token >> 24) & 0xF) * 4;	// instruction length without the opcode token
			}
			else
			{
				return 0;	// shader model 1 has no instruction lengths; the game only ships model 3
			}
		}
		return 0;
	}
}
