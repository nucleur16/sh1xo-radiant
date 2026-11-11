# codol_render - device independent core of the native CoDOL 1.1.1.13 viewport.
#
# Ported from CODOL-AssetExplorer ( Codol.Assets / Codol.Rendering / Codol.FastFiles ). No Direct3D device, no
# dependency on the rest of this repository or its deps/ submodules; Windows only ( CNG for AES / RSA ).
#
#   cmake -S codol_render -B build-codol-render -G "Visual Studio 17 2022" -A Win32      ( or -A x64 )
#   cmake --build build-codol-render --config Release
#   ctest --test-dir build-codol-render -C Release

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(CODOL_RENDER_STANDALONE ON)
	if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
else()
	set(CODOL_RENDER_STANDALONE OFF)
endif()

project(codol_render LANGUAGES CXX)

option(CODOL_RENDER_BUILD_TESTS "build the codol_render unit tests" ${CODOL_RENDER_STANDALONE})
option(CODOL_RENDER_BUILD_TOOLS "build the codol_render real-data check tool" ${CODOL_RENDER_STANDALONE})

add_library(codol_render STATIC
	src/shader_program.cpp
	src/technique_set.cpp
	src/vertex_format.cpp
	src/render_state.cpp
	src/material.cpp
	src/code_constants.cpp
	src/code_tables.cpp
	src/iwi.cpp
	src/texture_decode.cpp
	src/wavelet.cpp
	src/texture_upload.cpp
	src/inflate.cpp
	src/iwd.cpp
	src/crypto.cpp
)
target_include_directories(codol_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(codol_render PUBLIC cxx_std_20)
target_link_libraries(codol_render PUBLIC bcrypt)
if(MSVC)
	target_compile_options(codol_render PRIVATE /W4 /WX /permissive- /utf-8)
	target_compile_definitions(codol_render PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

if(CODOL_RENDER_BUILD_TESTS)
	enable_testing()
	add_executable(codol_render_tests
		tests/test_main.cpp
		tests/test_shader_program.cpp
		tests/test_technique_set.cpp
		tests/test_vertex_format.cpp
		tests/test_render_state.cpp
		tests/test_material.cpp
		tests/test_code_constants.cpp
		tests/test_technique_choice.cpp
		tests/test_textures.cpp
		tests/test_iwd.cpp
	)
	target_link_libraries(codol_render_tests PRIVATE codol_render)
	if(MSVC)
		target_compile_options(codol_render_tests PRIVATE /W4 /WX /permissive- /utf-8)
		target_compile_definitions(codol_render_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
	endif()
	foreach(suite shader_program technique_set vertex_format render_state material code_constants technique_choice textures iwd)
		add_test(NAME codol_render.${suite} COMMAND codol_render_tests ${suite})
	endforeach()
endif()

if(CODOL_RENDER_BUILD_TOOLS)
	add_executable(codol_render_check tools/realdata_check.cpp tools/zone_scan.cpp tools/png_wic.cpp)
	target_link_libraries(codol_render_check PRIVATE codol_render windowscodecs ole32)
	if(MSVC)
		target_compile_options(codol_render_check PRIVATE /W4 /WX /permissive- /utf-8)
		target_compile_definitions(codol_render_check PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
	endif()
endif()
