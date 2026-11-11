cmake_minimum_required(VERSION 3.20)

# codol_zone: native reader for Call of Duty Online 1.1.1.13 fast files ( version 334 ).
# Standalone: no dependency on the rest of the repository, its submodules or its precompiled header.
#
#   cmake -S codol_zone -B build/codol_zone_x86 -G "Visual Studio 17 2022" -A Win32
#   cmake -S codol_zone -B build/codol_zone_x64 -G "Visual Studio 17 2022" -A x64
#   cmake --build build/codol_zone_x86 --config Release
#   ctest --test-dir build/codol_zone_x86 -C Release

project(codol_zone LANGUAGES CXX)

option(CODOL_ZONE_STATIC_CRT "Link the static C runtime ( /MT ), as the radiant DLL does" ON)
option(CODOL_ZONE_BUILD_TOOLS "Build the codol_zonetool command line tool" ON)
option(CODOL_ZONE_BUILD_TESTS "Build the unit tests" ON)

# Folder of a Call of Duty Online 1.1.1.13 install. When it exists the tests also run against real zones.
set(CODOL_ZONE_GAME_DIR "D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13" CACHE PATH "CoDOL 1.1.1.13 install used by the integration tests ( optional )")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CODOL_ZONE_STATIC_CRT)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(codol_zone STATIC
	include/codol_zone/c1_layouts.hpp
	include/codol_zone/fastfile.hpp
	include/codol_zone/zone.hpp
	include/codol_zone/assets.hpp
	src/crypto_win.cpp
	src/crypto_win.hpp
	src/fastfile.cpp
	src/inflate.cpp
	src/vmem.hpp
	src/zone.cpp
	src/zone_impl.hpp
	src/zone_load.cpp
	src/loaders_1_1_1_13.inc
	src/assets.cpp
)
target_include_directories(codol_zone PUBLIC include)
target_link_libraries(codol_zone PUBLIC bcrypt)

function(codol_zone_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /MP)
		target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
	endif()
endfunction()

codol_zone_warnings(codol_zone)
if(MSVC)
	# the translated loaders are one 13000 line struct
	set_source_files_properties(src/zone_load.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
endif()

if(CODOL_ZONE_BUILD_TOOLS)
	add_executable(codol_zonetool tools/codol_zonetool.cpp)
	target_link_libraries(codol_zonetool PRIVATE codol_zone)
	codol_zone_warnings(codol_zonetool)
endif()

if(CODOL_ZONE_BUILD_TESTS)
	enable_testing()
	add_executable(codol_zone_tests tests/codol_zone_tests.cpp)
	target_link_libraries(codol_zone_tests PRIVATE codol_zone)
	target_compile_definitions(codol_zone_tests PRIVATE CODOL_ZONE_GAME_DIR="${CODOL_ZONE_GAME_DIR}")
	codol_zone_warnings(codol_zone_tests)
	add_test(NAME codol_zone_tests COMMAND codol_zone_tests)
endif()
