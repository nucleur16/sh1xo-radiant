#include "test.hpp"

// codol_render_tests [suite ..]   - runs the named suites ( all without arguments ); exit code 1 on any failure

namespace test
{
	counters& state()
	{
		static counters c;
		return c;
	}
}

int main(const int argc, char** argv)
{
	struct suite
	{
		const char* name;
		void (*run)();
	};
	const suite suites[] =
	{
		{ "shader_program", test_shader_program },
		{ "technique_set", test_technique_set },
		{ "vertex_format", test_vertex_format },
		{ "render_state", test_render_state },
		{ "material", test_material },
		{ "code_constants", test_code_constants },
		{ "technique_choice", test_technique_choice },
		{ "textures", test_textures },
		{ "iwd", test_iwd },
	};
	int ran = 0;
	for (const auto& s : suites)
	{
		bool wanted = argc < 2;
		for (int i = 1; i < argc; i++)
		{
			wanted = wanted || std::strcmp(argv[i], s.name) == 0;
		}
		if (!wanted)
		{
			continue;
		}
		const int before_checks = test::state().checks, before_failures = test::state().failures;
		s.run();
		std::printf("%-18s %4d checks, %d failed\n", s.name, test::state().checks - before_checks, test::state().failures - before_failures);
		ran++;
	}
	if (ran == 0)
	{
		std::printf("no such suite\n");
		return 2;
	}
	return test::state().failures == 0 ? 0 : 1;
}
