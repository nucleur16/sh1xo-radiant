#include "codol_render/technique_set.hpp"

#include <algorithm>
#include <cstdio>

namespace codol::render
{
	namespace
	{
		const char* const TECHNIQUE_NAMES[TECHNIQUE_COUNT] =
		{
			"depth prepass", "build floatz", "build shadowmap depth", "build shadowmap color", "unlit", "emissive",
			"emissive dfog", "emissive shadow", "emissive shadow dfog", "lit", "lit dfog", "lit sun", "lit sun dfog",
			"lit sun shadow", "lit sun shadow dfog", "lit spot", "lit spot dfog", "lit spot shadow", "lit spot shadow dfog",
			"lit omni", "lit omni dfog", "lit omni shadow", "lit omni shadow dfog", "lit instanced", "lit instanced dfog",
			"lit instanced sun", "lit instanced sun dfog", "lit instanced sun shadow", "lit instanced sun shadow dfog",
			"lit instanced spot", "lit instanced spot dfog", "lit instanced spot shadow", "lit instanced spot shadow dfog",
			"lit instanced omni", "lit instanced omni dfog", "lit instanced omni shadow", "lit instanced omni shadow dfog",
			"light spot", "light omni", "light spot shadow", "fakelight normal", "fakelight view", "sunlight preview",
			"case texture", "solid wireframe", "shaded wireframe", "debug bumpmap", "debug bumpmap instanced",
		};

		struct token
		{
			const char* text;
			std::uint8_t value;
		};

		// TechniqueSet.cs:33-37. The order matters: longer tokens first, as in the C# arrays.
		constexpr token SOURCES[] = { {"t0", 2}, {"t1", 5}, {"t2", 6}, {"n0", 7}, {"n1", 8}, {"p", 0}, {"c", 1}, {"n", 3}, {"t", 4} };
		constexpr token DESTINATIONS[] =
		{
			{"c0", 2}, {"c1", 3}, {"t0", 5}, {"t1", 6}, {"t2", 7}, {"t3", 8}, {"t4", 9}, {"t5", 10}, {"t6", 11}, {"t7", 12},
			{"p", 0}, {"n", 1}, {"d", 4},
		};

		bool starts_with_at(const std::string_view name, const std::size_t at, const char* tok, std::size_t& length)
		{
			length = std::strlen(tok);
			return name.compare(at, length, tok) == 0 && at + length <= name.size();
		}

		// TechniqueSet.cs:45-72 ( backtracking )
		bool parse_routing(const std::string_view name, const std::size_t at, std::vector<vertex_routing>& routing)
		{
			if (at == name.size())
			{
				return !routing.empty();
			}
			for (const auto& source : SOURCES)
			{
				std::size_t source_length;
				if (!starts_with_at(name, at, source.text, source_length))
				{
					continue;
				}
				for (const auto& dest : DESTINATIONS)
				{
					const std::size_t next = at + source_length;
					std::size_t dest_length;
					if (next > name.size() || !starts_with_at(name, next, dest.text, dest_length))
					{
						continue;
					}
					routing.push_back({ source.value, dest.value });
					if (parse_routing(name, next + dest_length, routing))
					{
						return true;
					}
					routing.pop_back();
				}
			}
			return false;
		}

		std::string zone_string(const zone_view& zone, const std::uint32_t pointer, bool& found)
		{
			std::string text;
			found = pointer != 0 && zone.string && zone.string(pointer, text);
			if (!found)
			{
				text.clear();
			}
			return text;
		}

		std::string fallback_name(const char* prefix, const std::uint32_t pointer)
		{
			char text[32];
			std::snprintf(text, sizeof text, "%s_%08X", prefix, pointer);
			return text;
		}
	}

	const char* technique_type_name(const int type)
	{
		return type >= 0 && type < TECHNIQUE_COUNT ? TECHNIQUE_NAMES[type] : "";
	}

	std::string vertex_declaration::key() const
	{
		std::string key;
		for (const auto& r : routing)
		{
			if (!key.empty())
			{
				key += ',';
			}
			key += std::to_string(r.source);
			key += ':';
			key += std::to_string(r.dest);
		}
		return key;
	}

	std::optional<std::vector<vertex_routing>> parse_vertex_declaration_name(std::string_view name)
	{
		try
		{
			// TechniqueSet.cs:39-43: name.TrimStart(',')
			while (!name.empty() && name.front() == ',')
			{
				name.remove_prefix(1);
			}
			std::vector<vertex_routing> routing;
			if (!parse_routing(name, 0, routing))
			{
				return std::nullopt;
			}
			return routing;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::optional<vertex_declaration> read_vertex_declaration(const zone_view& zone, const std::uint32_t pointer)
	{
		try
		{
			// RenderAssetLibrary.cs:524-550
			if (pointer == 0)
			{
				return std::nullopt;
			}
			const byte_span data = zone.read(pointer, build::VERTEX_DECL_ROUTING + 2 * build::VERTEX_DECL_MAX_STREAMS);
			if (data.empty())
			{
				return std::nullopt;
			}
			bool named;
			std::string name = zone_string(zone, rd_u32(data.data() + build::VERTEX_DECL_NAME), named);
			if (!named)
			{
				name = fallback_name("decl", pointer);
			}

			vertex_declaration decl;
			decl.pointer = pointer;
			if (is_reference_name(name))
			{
				// defined in another zone: the name spells the routing ( RenderAssetLibrary.cs:536-549 )
				auto routing = parse_vertex_declaration_name(name);
				if (!routing)
				{
					return std::nullopt;
				}
				decl.name = name.substr(1);
				decl.is_reference = true;
				decl.routing = std::move(*routing);
				return decl;
			}

			// RenderAssetLibrary.cs:424-435
			decl.name = std::move(name);
			const int streams = std::min<int>(data[build::VERTEX_DECL_STREAM_COUNT], build::VERTEX_DECL_MAX_STREAMS);
			decl.routing.resize(static_cast<std::size_t>(streams));
			for (int i = 0; i < streams; i++)
			{
				const auto* r = data.data() + build::VERTEX_DECL_ROUTING + 2 * i;
				decl.routing[static_cast<std::size_t>(i)] = { r[0], r[1] };
			}
			decl.has_optional_source = data[build::VERTEX_DECL_HAS_OPTIONAL_SOURCE] != 0;
			return decl;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	std::optional<shader_ref> read_shader(const zone_view& zone, const std::uint32_t pointer, const bool pixel_shader)
	{
		try
		{
			// RenderAssetLibrary.cs:513-522 ( ResolveShader ), :371-398 ( AddShader )
			if (pointer == 0)
			{
				return std::nullopt;
			}
			const byte_span data = zone.read(pointer, build::SHADER_SIZE);
			if (data.empty())
			{
				return std::nullopt;
			}
			shader_ref shader;
			shader.pointer = pointer;
			shader.is_pixel_shader = pixel_shader;
			bool named;
			std::string name = zone_string(zone, rd_u32(data.data() + build::SHADER_NAME), named);
			if (!named)
			{
				name = fallback_name("shader", pointer);
			}
			shader.has_name = named;
			shader.is_reference = is_reference_name(name);
			// RenderAssetLibrary.cs:520: TrimStart(',')
			std::size_t commas = 0;
			while (commas < name.size() && name[commas] == ',')
			{
				commas++;
			}
			shader.name = name.substr(commas);
			if (!shader.is_reference)
			{
				const std::uint32_t program = rd_u32(data.data() + build::SHADER_PROGRAM);
				const std::uint32_t tokens = rd_u16(data.data() + build::SHADER_PROGRAM_SIZE);
				// RenderAssetLibrary.cs:381: the program counts only when its whole range is loaded
				if (named && !name.empty() && program != 0 && tokens != 0 && !zone.read(program, 4 * tokens).empty())
				{
					shader.program = program;
					shader.program_size = 4 * tokens;
				}
			}
			return shader;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	byte_span shader_code(const zone_view& zone, const shader_ref& shader)
	{
		return shader.has_program() ? zone.read(shader.program, shader.program_size) : byte_span{};
	}

	std::uint64_t render_technique_set::slot_mask() const
	{
		std::uint64_t mask = 0;
		for (int t = 0; t < build::TECHNIQUE_COUNT; t++)
		{
			if (slots[static_cast<std::size_t>(t)] >= 0)
			{
				mask |= 1ull << t;
			}
		}
		return mask;
	}

	bool read_technique(const zone_view& zone, const std::uint32_t pointer, render_technique& out, std::string* error)
	{
		try
		{
			// RenderAssetLibrary.cs:467-511
			out = {};
			const byte_span header = pointer == 0 ? byte_span{} : zone.read(pointer, build::TECHNIQUE_PASSES);
			if (header.empty())
			{
				set_error(error, "technique " + hex32(pointer) + " is not loaded");
				return false;
			}
			out.pointer = pointer;
			const int pass_count = rd_u16(header.data() + build::TECHNIQUE_PASS_COUNT);
			out.flags = rd_u16(header.data() + build::TECHNIQUE_FLAGS);
			bool named;
			out.name = zone_string(zone, rd_u32(header.data() + build::TECHNIQUE_NAME), named);
			if (!named)
			{
				out.name = fallback_name("technique", pointer);
			}
			if (pass_count == 0)
			{
				return true;
			}
			const byte_span passes = zone.read(pointer + build::TECHNIQUE_PASSES, static_cast<std::uint32_t>(pass_count) * build::PASS_SIZE);
			if (passes.empty())
			{
				set_error(error, "technique " + out.name + ": its " + std::to_string(pass_count) + " passes are not loaded");
				return false;
			}
			out.passes.reserve(static_cast<std::size_t>(pass_count));
			for (int p = 0; p < pass_count; p++)
			{
				const auto* pass = passes.data() + static_cast<std::size_t>(p) * build::PASS_SIZE;
				render_pass result;
				result.per_prim_arg_count = pass[build::PASS_PER_PRIM_ARG_COUNT];
				result.per_obj_arg_count = pass[build::PASS_PER_OBJ_ARG_COUNT];
				result.stable_arg_count = pass[build::PASS_STABLE_ARG_COUNT];
				result.custom_sampler_flags = pass[build::PASS_CUSTOM_SAMPLER_FLAGS];
				const std::uint32_t arg_count = static_cast<std::uint32_t>(result.per_prim_arg_count) + result.per_obj_arg_count + result.stable_arg_count;
				const std::uint32_t args = rd_u32(pass + build::PASS_ARGS);
				if (args != 0 && arg_count != 0)
				{
					const byte_span arg_data = zone.read(args, arg_count * build::ARG_SIZE);
					if (arg_data.empty())
					{
						set_error(error, "technique " + out.name + " pass " + std::to_string(p) + ": arguments at " + hex32(args) + " are not loaded");
						return false;
					}
					result.arguments.resize(arg_count);
					for (std::uint32_t i = 0; i < arg_count; i++)
					{
						const auto* a = arg_data.data() + i * build::ARG_SIZE;
						auto& arg = result.arguments[i];
						arg.type = static_cast<shader_argument_type>(rd_u16(a + build::ARG_TYPE));
						arg.dest = rd_u16(a + build::ARG_DEST);
						arg.value = rd_u32(a + build::ARG_VALUE);
						if (arg.is_literal() && arg.value != 0)
						{
							const byte_span literal = zone.read(arg.value, 16);
							if (!literal.empty())
							{
								for (int k = 0; k < 4; k++)
								{
									arg.literal[k] = rd_f32(literal.data() + 4 * k);
								}
							}
						}
					}
				}
				result.declaration = read_vertex_declaration(zone, rd_u32(pass + build::PASS_VERTEX_DECL));
				result.vertex_shader = read_shader(zone, rd_u32(pass + build::PASS_VERTEX_SHADER), false);
				result.pixel_shader = read_shader(zone, rd_u32(pass + build::PASS_PIXEL_SHADER), true);
				out.passes.push_back(std::move(result));
			}
			return true;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}

	bool read_technique_set(const zone_view& zone, const std::uint32_t pointer, render_technique_set& out, std::string* error)
	{
		try
		{
			// RenderAssetLibrary.cs:437-465
			out = {};
			const byte_span header = pointer == 0 ? byte_span{} : zone.read(pointer, build::TECHSET_SIZE);
			if (header.empty())
			{
				set_error(error, "technique set " + hex32(pointer) + " is not loaded");
				return false;
			}
			out.pointer = pointer;
			bool named;
			out.name = zone_string(zone, rd_u32(header.data() + build::TECHSET_NAME), named);
			if (!named)
			{
				out.name = fallback_name("techset", pointer);
			}
			out.world_vert_format = header[build::TECHSET_WORLD_VERT_FORMAT];
			if (is_reference_name(out.name))
			{
				return true;	// RenderAssetLibrary.cs:269-283: resolved by name in the defining zone
			}
			std::array<std::uint32_t, build::TECHNIQUE_COUNT> pointers{};
			for (int t = 0; t < build::TECHNIQUE_COUNT; t++)
			{
				const std::uint32_t technique = rd_u32(header.data() + build::TECHSET_TECHNIQUES + 4 * t);
				pointers[static_cast<std::size_t>(t)] = technique;
				if (technique == 0 || zone.read(technique, build::TECHNIQUE_PASSES).empty())
				{
					continue;
				}
				for (int earlier = 0; earlier < t; earlier++)
				{
					if (pointers[static_cast<std::size_t>(earlier)] == technique)
					{
						out.slots[static_cast<std::size_t>(t)] = out.slots[static_cast<std::size_t>(earlier)];
						break;
					}
				}
				if (out.slots[static_cast<std::size_t>(t)] >= 0)
				{
					continue;
				}
				render_technique technique_data;
				std::string technique_error;
				if (!read_technique(zone, technique, technique_data, &technique_error))
				{
					set_error(error, out.name + " [" + std::to_string(t) + "]: " + technique_error);
					return false;
				}
				out.slots[static_cast<std::size_t>(t)] = static_cast<std::int8_t>(out.techniques.size());
				out.techniques.push_back(std::move(technique_data));
			}
			return true;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}
}
