# codol_mapc - incremental map compiler for Call of Duty Online 1.1.1.13: takes a map edited with codol_edit and
# the fast file it was imported from, applies the difference to the zone and writes a fast file.
#
#   cmake -S codol_mapc -B build/codol_mapc -A x64 -DCODOL_EDIT_RAPIDJSON_INCLUDE=<deps/rapidjson/include>
#   cmake --build build/codol_mapc --config Release
#   ctest --test-dir build/codol_mapc -C Release
#
# Builds codol_zone, codol_edit, codol_render and codol_view ( light grid sampling ) with it when they are not
# targets already.

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(CODOL_MAPC_STANDALONE ON)
	if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
else()
	set(CODOL_MAPC_STANDALONE OFF)
endif()

project(codol_mapc LANGUAGES CXX)

option(CODOL_MAPC_BUILD_TOOLS "build the codol_mapc command line tool" ${CODOL_MAPC_STANDALONE})
option(CODOL_MAPC_BUILD_TESTS "build codol_mapc_tests" ${CODOL_MAPC_STANDALONE})
option(CODOL_MAPC_BUILD_VIEWTOOL "also build codol_viewtool ( renders a compiled zone to a PNG )" ${CODOL_MAPC_STANDALONE})
set(CODOL_MAPC_GAME_DIR "D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13" CACHE PATH "CoDOL 1.1.1.13 install ( read only ) for the tests")

if(NOT TARGET codol_zone)
	# codol_zonetool ( decrypt ) and codol_linker ( --verify ) are what tests/run_e2e.ps1 checks the output with
	set(CODOL_ZONE_BUILD_TOOLS ${CODOL_MAPC_BUILD_TOOLS} CACHE BOOL "" FORCE)
	set(CODOL_ZONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_zone ${CMAKE_CURRENT_BINARY_DIR}/codol_zone)
endif()
if(NOT TARGET codol_edit)
	set(CODOL_EDIT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
	set(CODOL_EDIT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_edit ${CMAKE_CURRENT_BINARY_DIR}/codol_edit)
endif()
if(NOT TARGET codol_render)
	set(CODOL_RENDER_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
	set(CODOL_RENDER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_render ${CMAKE_CURRENT_BINARY_DIR}/codol_render)
endif()
if(NOT TARGET codol_view)
	set(CODOL_VIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(CODOL_VIEW_BUILD_TOOLS ${CODOL_MAPC_BUILD_VIEWTOOL} CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_view ${CMAKE_CURRENT_BINARY_DIR}/codol_view)
endif()

function(codol_mapc_options target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /std:c++latest /W4 /WX /permissive- /utf-8 /MP)
		target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
	else()
		target_compile_features(${target} PRIVATE cxx_std_20)
	endif()
endfunction()

add_library(codol_mapc STATIC
	include/codol_mapc/mapc.hpp
	include/codol_mapc/trace.hpp
	src/mapc_impl.hpp
	src/diff.cpp
	src/compile.cpp
	src/stage_models.cpp
	src/stage_brushes.cpp
	src/stage_surfaces.cpp
	src/trace.cpp
)
target_include_directories(codol_mapc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# light_grid.hpp / sight_trace.hpp: the renderer's model lighting, so that a compiled model is lit the way load_map lights it
target_include_directories(codol_mapc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/../codol_view/src)
target_link_libraries(codol_mapc PUBLIC codol_zone codol_edit PRIVATE codol_view)
codol_mapc_options(codol_mapc)

if(CODOL_MAPC_BUILD_TOOLS)
	add_executable(codol_mapc_tool tools/codol_mapc.cpp)
	set_target_properties(codol_mapc_tool PROPERTIES OUTPUT_NAME codol_mapc)
	target_link_libraries(codol_mapc_tool PRIVATE codol_mapc)
	codol_mapc_options(codol_mapc_tool)

	# development aid: prints how a zone's map assets are laid out ( see README, "How the layouts were established" )
	add_executable(codol_mapc_probe tools/mapc_probe.cpp)
	target_link_libraries(codol_mapc_probe PRIVATE codol_mapc)
	codol_mapc_options(codol_mapc_probe)
endif()

if(CODOL_MAPC_BUILD_TESTS)
	enable_testing()
	add_executable(codol_mapc_tests tests/codol_mapc_tests.cpp)
	target_link_libraries(codol_mapc_tests PRIVATE codol_mapc)
	target_compile_definitions(codol_mapc_tests PRIVATE CODOL_MAPC_GAME_DIR="${CODOL_MAPC_GAME_DIR}")
	codol_mapc_options(codol_mapc_tests)
	add_test(NAME codol_mapc_tests COMMAND codol_mapc_tests)
endif()
