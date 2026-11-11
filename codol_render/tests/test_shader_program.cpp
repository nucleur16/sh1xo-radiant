#include "test.hpp"

#include "codol_render/shader_program.hpp"

using namespace codol::render;

namespace
{
	// A vs_3_0 / ps_3_0 program with one CTAB comment, laid out like D3DXSHADER_CONSTANTTABLE:
	//   table +0 Size 28, +4 Creator, +8 Version, +12 Constants, +16 ConstantInfo, +20 Flags, +24 Target
	//   info ( 20 bytes ): +0 Name, +4 RegisterSet u16, +6 RegisterIndex u16, +8 RegisterCount u16, +10 Reserved,
	//                      +12 TypeInfo, +16 DefaultValue
	test::bytes make_program(const std::uint32_t version)
	{
		test::bytes table;
		table.u32(28).u32(0).u32(version).u32(3).u32(28).u32(0).u32(0);
		const std::size_t info = table.size();					// 28
		table.zeros(3 * 20);
		const auto name = [&](const char* text) { const auto at = static_cast<std::uint32_t>(table.size()); table.str(text); return at; };
		const std::uint32_t n0 = name("worldViewProjectionMatrix");
		const std::uint32_t n1 = name("colorMapSampler");
		const std::uint32_t n2 = name("fogConsts");
		const std::uint32_t creator = name("Microsoft (R) HLSL Shader Compiler 9.19.949.2111");
		const std::uint32_t target = name("vs_3_0");
		table.put_u32(4, creator);
		table.put_u32(24, target);
		// worldViewProjectionMatrix: float4 c0 .. c3
		table.put_u32(info, n0); table.put_u16(info + 4, 2); table.put_u16(info + 6, 0); table.put_u16(info + 8, 4);
		// colorMapSampler: sampler s2, count 1
		table.put_u32(info + 20, n1); table.put_u16(info + 24, 3); table.put_u16(info + 26, 2); table.put_u16(info + 28, 1);
		// fogConsts: float4 c7 with a register count of 0 ( covers one register all the same )
		table.put_u32(info + 40, n2); table.put_u16(info + 44, 2); table.put_u16(info + 46, 7); table.put_u16(info + 48, 0);
		while (table.size() % 4 != 0)
		{
			table.u8(0);
		}

		test::bytes code;
		code.u32(version);
		code.u32(static_cast<std::uint32_t>(((table.size() + 4) / 4) << 16) | 0xFFFE);		// comment token, length in DWORDs
		code.raw("CTAB", 4);
		code.raw(table.data.data(), table.size());
		code.u32(0x0200001F).u32(0x80000000).u32(0x900F0000);		// dcl_position v0
		code.u32(0x0000FFFF);										// end
		return code;
	}
}

void test_shader_program()
{
	{
		const auto code = make_program(0xFFFE0300);
		shader_program program;
		std::string error;
		CHECK(parse_shader_program(code.data, program, &error));
		CHECK_EQ(program.version, 0xFFFE0300u);
		CHECK(!program.is_pixel_shader());
		CHECK_EQ(program.major(), 3u);
		CHECK_EQ(program.constants.size(), 3u);
		if (program.constants.size() == 3)
		{
			CHECK(program.constants[0].name == "worldViewProjectionMatrix");
			CHECK(program.constants[0].set == register_set::float4);
			CHECK_EQ(program.constants[0].register_index, 0);
			CHECK_EQ(program.constants[0].register_count, 4);
			CHECK(program.constants[1].name == "colorMapSampler");
			CHECK(program.constants[1].set == register_set::sampler);
			CHECK_EQ(program.constants[1].register_index, 2);
			CHECK(program.constants[2].name == "fogConsts");
			CHECK_EQ(program.constants[2].register_count, 0);
		}
		CHECK(program.creator == "Microsoft (R) HLSL Shader Compiler 9.19.949.2111");
		CHECK(program.target == "vs_3_0");

		// ShaderProgram.cs:44-58
		int row = -1;
		const auto* c = program.find_constant(register_set::float4, 3, &row);
		CHECK(c && c->name == "worldViewProjectionMatrix");
		CHECK_EQ(row, 3);
		CHECK(program.find_constant(register_set::float4, 4, &row) == nullptr);
		CHECK_EQ(row, 0);
		c = program.find_constant(register_set::float4, 7, &row);		// count 0 still covers its register
		CHECK(c && c->name == "fogConsts");
		CHECK(program.find_constant(register_set::float4, 8) == nullptr);
		c = program.find_constant(register_set::sampler, 2);
		CHECK(c && c->name == "colorMapSampler");
		CHECK(program.find_constant(register_set::sampler, 0) == nullptr);		// c0 is a float4, not a sampler

		CHECK_EQ(shader_program_length(code.data), code.size());
		// trailing bytes are not part of the program
		auto longer = code;
		longer.u32(0x12345678).u32(0);
		CHECK_EQ(shader_program_length(longer.data), code.size());
	}
	{
		const auto code = make_program(0xFFFF0300);
		shader_program program;
		CHECK(parse_shader_program(code.data, program));
		CHECK(program.is_pixel_shader());
	}
	{
		// ShaderProgram.cs:62-70
		shader_program program;
		std::string error;
		const std::uint8_t tiny[4] = { 0x00, 0x03, 0xFE, 0xFF };
		CHECK(!parse_shader_program(tiny, program, &error));
		CHECK(error == "shader program of 4 bytes");
		const std::uint8_t odd[10] = { 0x00, 0x03, 0xFE, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0 };
		CHECK(!parse_shader_program(odd, program, &error));
		const std::uint8_t not_shader[8] = { 0x44, 0x58, 0x42, 0x43, 0, 0, 0, 0 };		// "DXBC"
		CHECK(!parse_shader_program(not_shader, program, &error));
		CHECK(error == "not a Direct3D 9 shader (version token 0x43425844)");
		CHECK_EQ(shader_program_length(not_shader), 0u);

		// no comment block: no constants, still a program
		const std::uint8_t bare[8] = { 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00 };
		CHECK(parse_shader_program(bare, program, &error));
		CHECK(program.constants.empty());
		CHECK(program.creator.empty());
		CHECK_EQ(shader_program_length(bare), 8u);

		// a program without an END token has no length
		const std::uint8_t open[8] = { 0x00, 0x03, 0xFF, 0xFF, 0x1F, 0x00, 0x00, 0x02 };
		CHECK_EQ(shader_program_length(open), 0u);
	}
	{
		// a comment that claims more bytes than the program has stops the scan ( ShaderProgram.cs:83-86 )
		test::bytes code;
		code.u32(0xFFFE0300).u32((100u << 16) | 0xFFFE).raw("CTAB", 4).u32(0x0000FFFF);
		shader_program program;
		CHECK(parse_shader_program(code.data, program));
		CHECK(program.constants.empty());
	}
}
