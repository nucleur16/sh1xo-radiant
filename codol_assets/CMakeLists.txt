# codol_assets - asset compiler for Call of Duty Online 1.1.1.13: images, materials and models from the source
# formats a modder makes ( PNG / TGA / DDS, material definitions / GDT, xmodel_export ) to native C1 assets inside
# fast files. The role Asset Manager + converter.exe + linker_pc play in the CoD4 mod tools.
#
#   cmake -S codol_assets -B build/codol_assets_x64 -G "Visual Studio 17 2022" -A x64      ( or -A Win32 )
#   cmake --build build/codol_assets_x64 --config Release
#   ctest --test-dir build/codol_assets_x64 -C Release
#
# Builds codol_zone and codol_render with it when they are not targets already. No third party code.

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(CODOL_ASSETS_STANDALONE ON)
	if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
else()
	set(CODOL_ASSETS_STANDALONE OFF)
endif()

project(codol_assets LANGUAGES CXX)

option(CODOL_ASSETS_BUILD_TOOLS "build codol_assetc and the research / verification tools" ${CODOL_ASSETS_STANDALONE})
option(CODOL_ASSETS_BUILD_TESTS "build codol_assets_tests" ${CODOL_ASSETS_STANDALONE})
set(CODOL_ASSETS_GAME_DIR "D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13" CACHE PATH "CoDOL 1.1.1.13 install ( read only ) for the tests")

if(NOT TARGET codol_zone)
	set(CODOL_ZONE_BUILD_TOOLS ${CODOL_ASSETS_BUILD_TOOLS} CACHE BOOL "" FORCE)
	set(CODOL_ZONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_zone ${CMAKE_CURRENT_BINARY_DIR}/codol_zone)
endif()
if(NOT TARGET codol_render)
	set(CODOL_RENDER_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
	set(CODOL_RENDER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../codol_render ${CMAKE_CURRENT_BINARY_DIR}/codol_render)
endif()

function(codol_assets_options target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /std:c++latest /W4 /WX /permissive- /utf-8 /MP)
		target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
	else()
		target_compile_features(${target} PRIVATE cxx_std_20)
	endif()
endfunction()

file(GLOB CODOL_ASSETS_SOURCES CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/include/codol_assets/*.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(codol_assets STATIC ${CODOL_ASSETS_SOURCES})
target_include_directories(codol_assets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(codol_assets PUBLIC codol_zone codol_render)
set_target_properties(codol_assets PROPERTIES OUTPUT_NAME codol_assets_lib)
codol_assets_options(codol_assets)

if(CODOL_ASSETS_BUILD_TOOLS)
	add_executable(codol_assetc tools/codol_assetc.cpp)
	target_link_libraries(codol_assetc PRIVATE codol_assets)
	codol_assets_options(codol_assetc)

	# research and verification: how shipped assets are laid out, round trips against shipped assets
	add_executable(codol_asset_probe tools/asset_probe.cpp)
	target_link_libraries(codol_asset_probe PRIVATE codol_assets)
	codol_assets_options(codol_asset_probe)

	file(GLOB CODOL_ASSETS_VERIFY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tools/verify.hpp)
	add_executable(codol_asset_verify tools/asset_verify.cpp ${CODOL_ASSETS_VERIFY_SOURCES})
	target_link_libraries(codol_asset_verify PRIVATE codol_assets)
	codol_assets_options(codol_asset_verify)
endif()

if(CODOL_ASSETS_BUILD_TESTS)
	enable_testing()
	add_executable(codol_assets_tests tests/codol_assets_tests.cpp)
	target_link_libraries(codol_assets_tests PRIVATE codol_assets)
	target_compile_definitions(codol_assets_tests PRIVATE CODOL_ASSETS_GAME_DIR="${CODOL_ASSETS_GAME_DIR}")
	codol_assets_options(codol_assets_tests)
	add_test(NAME codol_assets_tests COMMAND codol_assets_tests)
endif()
