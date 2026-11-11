#pragma once

// The entity string of a map ( MapEnts::entityString ): { "key" "value" .. } blocks.
// Port of Codol.Assets\Maps\MapEntityParser.cs and MapData.cs:225-250 ( MapEntity ).

#include "map_types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace codol::view
{
	struct map_entity
	{
		int index = 0;
		std::vector<std::pair<std::string, std::string>> properties;

		// MapData.cs:231-241: first property with that key, compared without case; nullptr when there is none
		const std::string* get(std::string_view key) const;
		std::string_view class_name() const;
	};

	// MapEntityParser.cs:8-84
	std::vector<map_entity> parse_entities(std::string_view text, std::vector<std::string>* warnings = nullptr);

	// GameMapRenderer.cs:475-486 ( ParseVector ): exactly three numbers separated by spaces
	std::optional<float3> parse_vector(const std::string* text);
	// float.TryParse( .., NumberStyles.Float, InvariantCulture )
	std::optional<float> parse_float(std::string_view text);

	bool equals_ignore_case(std::string_view a, std::string_view b);
	std::string to_lower(std::string_view text);
}
