#include "entities.hpp"

#include <charconv>
#include <cstdlib>

namespace codol::view
{
	namespace
	{
		char lower(const char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; }

		// char.IsWhiteSpace for the characters an entity string can hold
		bool is_space(const char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }
	}

	bool equals_ignore_case(const std::string_view a, const std::string_view b)
	{
		if (a.size() != b.size())
		{
			return false;
		}
		for (std::size_t i = 0; i < a.size(); i++)
		{
			if (lower(a[i]) != lower(b[i]))
			{
				return false;
			}
		}
		return true;
	}

	std::string to_lower(const std::string_view text)
	{
		std::string out(text);
		for (char& c : out)
		{
			c = lower(c);
		}
		return out;
	}

	const std::string* map_entity::get(const std::string_view key) const
	{
		for (const auto& [k, v] : properties)
		{
			if (equals_ignore_case(k, key))
			{
				return &v;
			}
		}
		return nullptr;
	}

	std::string_view map_entity::class_name() const
	{
		const std::string* name = get("classname");
		return name ? std::string_view(*name) : std::string_view();
	}

	std::vector<map_entity> parse_entities(const std::string_view text, std::vector<std::string>* warnings)
	{
		// MapEntityParser.cs:8-84
		std::vector<map_entity> entities;
		const auto warn = [&](std::string message)
		{
			if (warnings)
			{
				warnings->push_back(std::move(message));
			}
		};
		std::size_t i = 0;
		bool open = false;
		map_entity current;
		std::optional<std::string> pending_key;
		while (i < text.size())
		{
			const char c = text[i];
			if (is_space(c) || c == '\0')
			{
				i++;
				continue;
			}
			if (c == '{')
			{
				if (open)
				{
					warn("entity string: '{' at " + std::to_string(i) + " inside an entity");
				}
				current = {};
				open = true;
				pending_key.reset();
				i++;
				continue;
			}
			if (c == '}')
			{
				if (!open)
				{
					warn("entity string: '}' at " + std::to_string(i) + " without an entity");
				}
				else
				{
					if (pending_key)
					{
						warn("entity string: key '" + *pending_key + "' without a value in entity " + std::to_string(entities.size()));
					}
					current.index = static_cast<int>(entities.size());
					entities.push_back(std::move(current));
				}
				current = {};
				open = false;
				pending_key.reset();
				i++;
				continue;
			}
			if (c == '"')
			{
				const std::size_t end = text.find('"', i + 1);
				if (end == std::string_view::npos)
				{
					warn("entity string: unterminated string at " + std::to_string(i));
					break;
				}
				std::string token(text.substr(i + 1, end - i - 1));
				i = end + 1;
				if (!open)
				{
					warn("entity string: string \"" + token + "\" outside an entity");
					continue;
				}
				if (!pending_key)
				{
					pending_key = std::move(token);
				}
				else
				{
					current.properties.emplace_back(std::move(*pending_key), std::move(token));
					pending_key.reset();
				}
				continue;
			}
			warn(std::string("entity string: unexpected character '") + c + "' at " + std::to_string(i));
			i++;
		}
		if (open)
		{
			warn("entity string: the last entity is not closed");
		}
		return entities;
	}

	std::optional<float> parse_float(std::string_view text)
	{
		// NumberStyles.Float: leading / trailing white space, leading sign, decimal point, exponent
		while (!text.empty() && is_space(text.front()))
		{
			text.remove_prefix(1);
		}
		while (!text.empty() && is_space(text.back()))
		{
			text.remove_suffix(1);
		}
		if (!text.empty() && text.front() == '+')
		{
			text.remove_prefix(1);
			if (!text.empty() && (text.front() == '+' || text.front() == '-'))
			{
				return std::nullopt;
			}
		}
		if (text.empty())
		{
			return std::nullopt;
		}
		float value = 0;
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
		if (result.ec != std::errc() || result.ptr != text.data() + text.size())
		{
			return std::nullopt;
		}
		return value;
	}

	std::optional<float3> parse_vector(const std::string* text)
	{
		// GameMapRenderer.cs:475-486: Split( ' ', RemoveEmptyEntries ) must give exactly three numbers
		if (!text)
		{
			return std::nullopt;
		}
		float values[3] = {};
		int count = 0;
		std::size_t i = 0;
		const std::string_view s(*text);
		while (i < s.size())
		{
			while (i < s.size() && s[i] == ' ')
			{
				i++;
			}
			if (i >= s.size())
			{
				break;
			}
			std::size_t end = s.find(' ', i);
			if (end == std::string_view::npos)
			{
				end = s.size();
			}
			if (count == 3)
			{
				return std::nullopt;
			}
			const auto value = parse_float(s.substr(i, end - i));
			if (!value)
			{
				return std::nullopt;
			}
			values[count++] = *value;
			i = end;
		}
		if (count != 3)
		{
			return std::nullopt;
		}
		return float3{ values[0], values[1], values[2] };
	}
}
