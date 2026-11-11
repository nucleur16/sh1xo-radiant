#pragma once
// Minimal stand-in for src/std_include.hpp so the engine independent parts of the
// CoDOL code ( src/codol/*.cpp ) can be compiled into the unit tests without radiant.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "codol/script_lex.hpp"
#include "codol/anim_index.hpp"
