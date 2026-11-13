#pragma once

// codol_edit: prefab files on disk. A misc_prefab names its file relative to a map_source folder
// ( "model" "prefabs/crate.map" ); document::expand_prefabs takes a loader, these make one that reads files.

#include "codol_edit/document.hpp"

#include <filesystem>
#include <span>

namespace codol::edit
{
	// Where the prefabs of a map are looked for, in order: the folder of the .map and each of its parents ( up to
	// four ), and a "map_source" folder next to any of them - so a map in map_source\, in a sub folder of it, or
	// a copy of the map somewhere below the editor's folder all find map_source\prefabs\.. .
	std::vector<std::filesystem::path> prefab_roots_for(const std::filesystem::path& map_file);

	// the file of a prefab: the first `root / model` that exists ( "\\" and "/" are the same, ".map" may be
	// missing ); empty when there is none
	std::filesystem::path resolve_prefab(std::string_view model, std::span<const std::filesystem::path> roots);

	// a loader for document::expand_prefabs that reads the files below `roots`
	prefab_loader file_prefab_loader(std::vector<std::filesystem::path> roots);

	// what the compilers do right after loading a .map: every misc_prefab ( and the prefabs inside prefabs ) is
	// replaced by its contents. `extra_roots` are searched ahead of prefab_roots_for( map_file ). True when the
	// map had prefabs and at least one was expanded; `result.warnings` names those that were not.
	bool expand_prefabs_from_disk(document& doc, const std::filesystem::path& map_file, prefab_result& result,
		std::span<const std::filesystem::path> extra_roots = {});
}
