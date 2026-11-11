# codol_view - native Direct3D 9 map renderer for Call of Duty Online 1.1.1.13.
#
# Draws the game's own data ( GfxWorld, XModels ) with the game's own materials, technique sets and compiled SM3
# shaders, read from the 1.1.1.13 fastfiles through codol_zone and codol_render. Port of CODOL-AssetExplorer's
# Codol.Rendering\GameMapRenderer. Direct3D 9 from the Windows SDK only - no D3DX, no third party code.
#
#   cmake -S codol_view -B build-codol-view -G "Visual Studio 17 2022" -A Win32      ( or -A x64 )
#   cmake --build build-codol-view --config Release
#   ctest --test-dir build-codol-view -C Release

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(CODOL_VIEW_STANDALONE ON)
	if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
else()
	set(CODOL_VIEW_STANDALONE OFF)
endif()

project(codol_view LANGUAGES CXX)

option(CODOL_VIEW_BUILD_TESTS "build the codol_view unit tests" ${CODOL_VIEW_STANDALONE})
option(CODOL_VIEW_BUILD_TOOLS "build codol_viewtool ( offscreen renderer, bench, state test )" ${CODOL_VIEW_STANDALONE})

# the two libraries it is built on; already targets when this is part of the root project
if(NOT TARGET codol_zone)
	set(CODOL_ZONE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
	set(CODOL_ZONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_zone ${CMAKE_CURRENT_BINARY_DIR}/codol_zone)
endif()
if(NOT TARGET codol_render)
	set(CODOL_RENDER_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
	set(CODOL_RENDER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_render ${CMAKE_CURRENT_BINARY_DIR}/codol_render)
endif()

function(codol_view_options target)
	target_compile_features(${target} PUBLIC cxx_std_20)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /MP)
		target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
	endif()
endfunction()

add_library(codol_view STATIC
	include/codol_view/map_renderer.hpp
	src/map_types.hpp
	src/zone_library.hpp
	src/zone_library.cpp
	src/entities.hpp
	src/entities.cpp
	src/light_grid.hpp
	src/light_grid.cpp
	src/sight_trace.hpp
	src/sight_trace.cpp
	src/world_cells.hpp
	src/world_cells.cpp
	src/world_visibility.hpp
	src/world_visibility.cpp
	src/model_surfaces.hpp
	src/model_surfaces.cpp
	src/renderer_impl.hpp
	src/renderer_load.cpp
	src/renderer_draw.cpp
	src/renderer_shadow.cpp
	src/map_renderer.cpp
)
target_include_directories(codol_view PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(codol_view PUBLIC codol_zone codol_render d3d9)
codol_view_options(codol_view)

if(CODOL_VIEW_BUILD_TOOLS)
	add_executable(codol_viewtool tools/codol_viewtool.cpp tools/png_wic.cpp tools/png_wic.hpp)
	target_link_libraries(codol_viewtool PRIVATE codol_view windowscodecs ole32 psapi)
	codol_view_options(codol_viewtool)
	if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
		# the editor host is a 32 bit process; the tool measures the same address space budget
		target_link_options(codol_viewtool PRIVATE /LARGEADDRESSAWARE:NO)
	endif()

	add_executable(codol_lighting_check tools/lighting_check.cpp)
	target_link_libraries(codol_lighting_check PRIVATE codol_view)
	target_include_directories(codol_lighting_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	codol_view_options(codol_lighting_check)

	add_executable(codol_visibility_check tools/visibility_check.cpp)
	target_link_libraries(codol_visibility_check PRIVATE codol_view)
	target_include_directories(codol_visibility_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	codol_view_options(codol_visibility_check)
endif()

if(CODOL_VIEW_BUILD_TESTS)
	enable_testing()
	add_executable(codol_view_tests tests/codol_view_tests.cpp)
	target_link_libraries(codol_view_tests PRIVATE codol_view)
	target_include_directories(codol_view_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
	codol_view_options(codol_view_tests)
	add_test(NAME codol_view_tests COMMAND codol_view_tests)
endif()
