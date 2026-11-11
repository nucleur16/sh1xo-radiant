#pragma once

// Direct3D 9 shader byte code: version token check and the constant table ( CTAB comment ).
// Port of Codol.Assets\Rendering\ShaderProgram.cs.

#include "common.hpp"

#include <vector>

namespace codol::render
{
	// D3DXREGISTER_SET ( ShaderProgram.cs:6-12 )
	enum class register_set : std::uint16_t
	{
		boolean = 0,
		int4 = 1,
		float4 = 2,
		sampler = 3,
	};

	// ShaderProgram.cs:14
	struct shader_constant
	{
		std::string name;
		register_set set = register_set::boolean;
		std::uint16_t register_index = 0;
		std::uint16_t register_count = 0;
	};

	struct shader_program
	{
		std::uint32_t version = 0;				// first token: 0xFFFF0300 ps_3_0, 0xFFFE0300 vs_3_0
		std::vector<shader_constant> constants;
		std::string creator;					// empty when the table has none
		std::string target;

		bool is_pixel_shader() const { return (version >> 16) == 0xFFFF; }		// ShaderProgram.cs:36
		std::uint32_t major() const { return (version >> 8) & 0xFF; }
		std::uint32_t minor() const { return version & 0xFF; }

		// The constant that covers `register_index` of `set` ( ShaderProgram.cs:44-58 ): first match in table
		// order, a count of 0 covers one register. `row` receives register_index - constant.register_index.
		const shader_constant* find_constant(register_set set, int register_index, int* row = nullptr) const;
	};

	// ShaderProgram.cs:60-94. `code` is the whole program ( 4 * programSize bytes of a MaterialShader ).
	// Fails when the size is < 8 or not a multiple of 4, or the version token is not a D3D9 vertex / pixel shader.
	// Only the comment tokens that directly follow the version token are read, as in the C# source.
	bool parse_shader_program(byte_span code, shader_program& out, std::string* error = nullptr);

	// Number of bytes of the program that starts at code[0], found by walking its tokens up to and including the
	// END token ( 0x0000FFFF ); 0 when code does not start with a shader or ends before END. Not part of the C#
	// source ( the zone stores the size ): used to delimit programs when scanning raw data.
	std::size_t shader_program_length(byte_span code);
}
