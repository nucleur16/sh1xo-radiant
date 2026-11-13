# codol_bsp - map compiler for Call of Duty Online 1.1.1.13: a .map in, a complete map fast file out
# ( the place cod4map + cod4rad + linker_pc have in the CoD4 mod tools ). Nothing is taken from a shipped map zone.
#
#   cmake -S codol_bsp -B build/codol_bsp -A x64 -DCODOL_EDIT_RAPIDJSON_INCLUDE=<deps/rapidjson/include>
#   cmake --build build/codol_bsp --config Release
#   ctest --test-dir build/codol_bsp -C Release
#
# Builds codol_zone, codol_edit, codol_render, codol_view and codol_mapc ( server_check, trace ) with it when they
# are not targets already.

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(CODOL_BSP_STANDALONE ON)
	if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
else()
	set(CODOL_BSP_STANDALONE OFF)
endif()

project(codol_bsp LANGUAGES CXX)

option(CODOL_BSP_BUILD_TOOLS "build the codol_bsp command line tool" ${CODOL_BSP_STANDALONE})
option(CODOL_BSP_BUILD_TESTS "build codol_bsp_tests" ${CODOL_BSP_STANDALONE})
set(CODOL_BSP_GAME_DIR "D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13" CACHE PATH "CoDOL 1.1.1.13 install ( read only ) for the tests")

if(NOT TARGET codol_mapc)
	set(CODOL_MAPC_BUILD_TOOLS ${CODOL_BSP_BUILD_TOOLS} CACHE BOOL "" FORCE)
	set(CODOL_MAPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(CODOL_MAPC_BUILD_VIEWTOOL ${CODOL_BSP_BUILD_TOOLS} CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_mapc ${CMAKE_CURRENT_BINARY_DIR}/codol_mapc)
endif()

function(codol_bsp_options target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /std:c++latest /W4 /WX /permissive- /utf-8 /MP)
		target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
	else()
		target_compile_features(${target} PRIVATE cxx_std_20)
	endif()
endfunction()

file(GLOB CODOL_BSP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/codol_bsp/*.hpp)
add_library(codol_bsp STATIC ${CODOL_BSP_SOURCES})
target_include_directories(codol_bsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(codol_bsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(codol_bsp PUBLIC codol_zone codol_edit codol_mapc PRIVATE codol_render)
codol_bsp_options(codol_bsp)

if(CODOL_BSP_BUILD_TOOLS)
	# see codol_mapc/CMakeLists.txt: the library must not share the tool's name
	set_target_properties(codol_bsp PROPERTIES OUTPUT_NAME codol_bsp_lib)
	add_executable(codol_bsp_tool tools/codol_bsp.cpp)
	set_target_properties(codol_bsp_tool PROPERTIES OUTPUT_NAME codol_bsp)
	target_link_libraries(codol_bsp_tool PRIVATE codol_bsp)
	codol_bsp_options(codol_bsp_tool)

	# development aid: prints the stream layout of a zone's map assets
	add_executable(codol_bsp_probe tools/bsp_probe.cpp)
	target_link_libraries(codol_bsp_probe PRIVATE codol_bsp)
	codol_bsp_options(codol_bsp_probe)
endif()

if(CODOL_BSP_BUILD_TESTS)
	enable_testing()
	add_executable(codol_bsp_tests tests/codol_bsp_tests.cpp)
	target_link_libraries(codol_bsp_tests PRIVATE codol_bsp)
	target_compile_definitions(codol_bsp_tests PRIVATE CODOL_BSP_GAME_DIR="${CODOL_BSP_GAME_DIR}")
	codol_bsp_options(codol_bsp_tests)
	add_test(NAME codol_bsp_tests COMMAND codol_bsp_tests)

	# the patches and brush primitives of the editor's Brush / Patch menus, compiled
	add_executable(codol_bsp_patch_tests tests/patch_tests.cpp)
	target_link_libraries(codol_bsp_patch_tests PRIVATE codol_bsp)
	target_compile_definitions(codol_bsp_patch_tests PRIVATE CODOL_BSP_GAME_DIR="${CODOL_BSP_GAME_DIR}")
	codol_bsp_options(codol_bsp_patch_tests)
	add_test(NAME codol_bsp_patch_tests COMMAND codol_bsp_patch_tests)
endif()
