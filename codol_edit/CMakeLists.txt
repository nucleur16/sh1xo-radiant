# codol_edit - UI independent core of the standalone CoDOL 1.1.1.13 map editor:
# document model, .map reader / writer, geometry, picking, editing operations with undo, the C1 entity class
# catalog, fastfile import and the entity override export. No ImGui, no Direct3D, no radiant.
#
#   cmake -S codol_edit -B build-codol-edit -G "Visual Studio 17 2022" -A x64      ( or -A Win32 )
#   cmake --build build-codol-edit --config Release
#   ctest --test-dir build-codol-edit -C Release
#
# Or from another project: add_subdirectory( codol_edit ) and target_link_libraries( <target> PRIVATE codol_edit ).

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(CODOL_EDIT_STANDALONE ON)
	if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
else()
	set(CODOL_EDIT_STANDALONE OFF)
endif()

project(codol_edit LANGUAGES CXX)

option(CODOL_EDIT_BUILD_TESTS "build codol_edit_tests" ${CODOL_EDIT_STANDALONE})
option(CODOL_EDIT_BUILD_TOOLS "build codol_classgen ( generates src/class_catalog_data.inc from the install )" ${CODOL_EDIT_STANDALONE})

# the repository this folder is part of: the fastfile import reuses src/codol/map_*.cpp from there
set(CODOL_EDIT_REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "root of the iw3xo-radiant-codol checkout")
# rapidjson headers ( a submodule of the repository; only src/codol/map_geometry.cpp needs them )
set(CODOL_EDIT_RAPIDJSON_INCLUDE "${CODOL_EDIT_REPO_DIR}/deps/rapidjson/include" CACHE PATH "rapidjson include folder")
# test data ( all optional: tests that need a missing folder are skipped and say so )
set(CODOL_EDIT_GAME_DIR "D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13" CACHE PATH "CoDOL 1.1.1.13 install ( read only )")
set(CODOL_EDIT_MAP_SOURCE_DIR "D:/CoD/iw3xo-radiant-codol/modtools/map_source" CACHE PATH "folder with radiant .map files for the round trip test")

if(NOT EXISTS "${CODOL_EDIT_RAPIDJSON_INCLUDE}/rapidjson/document.h")
	message(FATAL_ERROR "codol_edit: rapidjson not found in ${CODOL_EDIT_RAPIDJSON_INCLUDE} - run `git submodule update --init deps/rapidjson` or set CODOL_EDIT_RAPIDJSON_INCLUDE")
endif()

if(NOT TARGET codol_zone)
	set(CODOL_ZONE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
	set(CODOL_ZONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_zone ${CMAKE_CURRENT_BINARY_DIR}/codol_zone)
endif()

function(codol_edit_options target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /std:c++latest /W4 /WX /permissive- /utf-8 /MP)
		target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
	else()
		target_compile_features(${target} PRIVATE cxx_std_20)
	endif()
endfunction()

# the engine independent code of the radiant fork, compiled from where it is ( the same files are part of
# iw3r, codol_tests and codol_mapconv in the root project )
add_library(codol_edit_mapconv OBJECT
	${CODOL_EDIT_REPO_DIR}/src/codol/map_ents.cpp
	${CODOL_EDIT_REPO_DIR}/src/codol/map_geometry.cpp
	${CODOL_EDIT_REPO_DIR}/src/codol/map_zone.cpp
)
target_include_directories(codol_edit_mapconv PRIVATE
	${CODOL_EDIT_REPO_DIR}/tests/stub
	${CODOL_EDIT_REPO_DIR}/src
	${CODOL_EDIT_RAPIDJSON_INCLUDE}
	$<TARGET_PROPERTY:codol_zone,INTERFACE_INCLUDE_DIRECTORIES>)
if(MSVC)
	target_compile_options(codol_edit_mapconv PRIVATE /std:c++latest /W4 /wd4996 /utf-8 /MP)
endif()

add_library(codol_edit STATIC
	include/codol_edit/codol_edit.hpp
	include/codol_edit/math.hpp
	include/codol_edit/model.hpp
	include/codol_edit/geometry.hpp
	include/codol_edit/document.hpp
	include/codol_edit/class_catalog.hpp
	src/doc_impl.hpp
	src/geometry.cpp
	src/map_io.cpp
	src/document.cpp
	src/edit_ops.cpp
	src/select_ops.cpp
	src/prefab.cpp
	include/codol_edit/select_types.hpp
	include/codol_edit/prefab.hpp
	include/codol_edit/document_select.inc
	src/import.cpp
	src/class_catalog.cpp
	src/class_catalog_data.inc
	$<TARGET_OBJECTS:codol_edit_mapconv>
)
target_include_directories(codol_edit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(codol_edit PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CODOL_EDIT_REPO_DIR}/tests/stub
	${CODOL_EDIT_REPO_DIR}/src
	${CODOL_EDIT_RAPIDJSON_INCLUDE})
target_link_libraries(codol_edit PUBLIC codol_zone)
codol_edit_options(codol_edit)

if(CODOL_EDIT_BUILD_TOOLS)
	add_executable(codol_classgen tools/codol_classgen.cpp)
	target_link_libraries(codol_classgen PRIVATE codol_zone)
	codol_edit_options(codol_classgen)
	if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
		target_link_options(codol_classgen PRIVATE /LARGEADDRESSAWARE)
	endif()
endif()

if(CODOL_EDIT_BUILD_TESTS)
	enable_testing()
	add_executable(codol_edit_tests tests/codol_edit_tests.cpp)
	target_link_libraries(codol_edit_tests PRIVATE codol_edit)
	target_compile_definitions(codol_edit_tests PRIVATE
		CODOL_EDIT_GAME_DIR="${CODOL_EDIT_GAME_DIR}"
		CODOL_EDIT_MAP_SOURCE_DIR="${CODOL_EDIT_MAP_SOURCE_DIR}")
	codol_edit_options(codol_edit_tests)
	if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
		target_link_options(codol_edit_tests PRIVATE /LARGEADDRESSAWARE)
	endif()
	add_test(NAME codol_edit_tests COMMAND codol_edit_tests)

	# selection menu, clipboard, filters, regions, prefabs
	add_executable(codol_edit_select_tests tests/codol_edit_select_tests.cpp)
	target_link_libraries(codol_edit_select_tests PRIVATE codol_edit)
	codol_edit_options(codol_edit_select_tests)
	add_test(NAME codol_edit_select_tests COMMAND codol_edit_select_tests)
endif()
