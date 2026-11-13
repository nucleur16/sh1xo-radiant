#include "codol_edit/class_catalog.hpp"

#include <algorithm>
#include <vector>

namespace codol::edit
{
	namespace
	{
		struct raw_key
		{
			std::string_view name;
			int count;
			int first_example;
			int example_count;
		};

		struct raw_flag
		{
			int value;
			int count;
		};

		struct raw_class
		{
			std::string_view name;
			int count, maps, point, brush;
			int first_key, key_count;
			int first_flag, flag_count;
		};

#include "class_catalog_data.inc"

		// ---- hand written: only what is certain ----------------------------------------------------
		// Boxes: a player spawn is the player hull ( 32 x 32 x 72, origin at the feet ), as in every IW engine
		// game. The colours are display colours of this editor, they are not game data.

		struct known_box
		{
			std::string_view name;		// exact classname, or a pattern: prefix*suffix / prefix*middle*
			float mins[3], maxs[3];
			float color[3];
			std::string_view description;
		};

		constexpr known_box known[] =
		{
			{ "info_player_start", { -16, -16, 0 }, { 16, 16, 72 }, { 0.2f, 1.0f, 0.2f }, "Player start." },
			{ "mp_global_intermission", { -16, -16, -16 }, { 16, 16, 16 }, { 0.0f, 0.5f, 1.0f }, "Camera position of the intermission / of spectators before the first spawn." },
			{ "mp_*spawn*", { -16, -16, 0 }, { 16, 16, 72 }, { 0.2f, 0.6f, 1.0f }, "Multiplayer spawn point ( player hull, origin at the feet )." },
			{ "light", { -8, -8, -8 }, { 8, 8, 8 }, { 1.0f, 1.0f, 0.3f }, "Light source: radius, intensity, _color. Baked into the map; with spawnflags 1 ( PRIMARY_OMNI ) or 2 ( PRIMARY_SPOT ) a primary light, lit at run time. In a shipped zone the entity is only a marker." },
			{ "script_origin", { -8, -8, -8 }, { 8, 8, 8 }, { 0.6f, 0.6f, 0.6f }, "Position for scripts." },
			{ "script_struct", { -8, -8, -8 }, { 8, 8, 8 }, { 0.9f, 0.3f, 0.9f }, "Position / data for scripts." },
			{ "script_model", { -8, -8, -8 }, { 8, 8, 8 }, { 0.3f, 0.9f, 0.9f }, "Model placed for scripts; the box should come from the model ( entity box callback )." },
			{ "misc_model", { -8, -8, -8 }, { 8, 8, 8 }, { 0.3f, 0.9f, 0.6f }, "Static model; the box should come from the model ( entity box callback )." },
		};

		bool matches(const std::string_view pattern, const std::string_view name)
		{
			const auto star = pattern.find('*');
			if (star == std::string_view::npos)
			{
				return pattern == name;
			}

			const auto prefix = pattern.substr(0, star);
			auto rest = pattern.substr(star + 1);

			if (!name.starts_with(prefix))
			{
				return false;
			}

			if (!rest.empty() && rest.back() == '*')
			{
				rest.remove_suffix(1);
				return name.find(rest, prefix.size()) != std::string_view::npos;
			}

			return name.size() >= prefix.size() + rest.size() && name.ends_with(rest);
		}

		struct built
		{
			std::vector<class_info> classes;
			std::vector<class_key> keys;
			std::vector<class_spawnflags> flags;

			built()
			{
				keys.reserve(std::size(catalog_keys));
				for (const raw_key& k : catalog_keys)
				{
					class_key out;
					out.name = k.name;
					out.count = k.count;
					out.examples = std::span<const std::string_view>(catalog_examples + k.first_example, static_cast<std::size_t>(k.example_count));
					keys.push_back(out);
				}

				flags.reserve(std::size(catalog_flags));
				for (const raw_flag& f : catalog_flags)
				{
					flags.push_back({ f.value, f.count });
				}

				classes.reserve(std::size(catalog_classes));
				for (const raw_class& c : catalog_classes)
				{
					if (c.name.empty())
					{
						continue;	// placeholder row of an empty table
					}

					class_info info;
					info.name = c.name;
					info.count = c.count;
					info.map_count = c.maps;
					info.point_count = c.point;
					info.brush_count = c.brush;
					info.keys = std::span<const class_key>(keys.data() + c.first_key, static_cast<std::size_t>(c.key_count));
					info.spawnflags = std::span<const class_spawnflags>(flags.data() + c.first_flag, static_cast<std::size_t>(c.flag_count));

					for (const known_box& k : known)
					{
						if (matches(k.name, info.name))
						{
							info.has_box = true;
							std::copy(std::begin(k.mins), std::end(k.mins), info.mins);
							std::copy(std::begin(k.maxs), std::end(k.maxs), info.maxs);
							std::copy(std::begin(k.color), std::end(k.color), info.color);
							info.description = k.description;
							break;
						}
					}

					classes.push_back(info);
				}

				std::sort(classes.begin(), classes.end(), [](const class_info& a, const class_info& b) { return a.name < b.name; });
			}
		};
	}

	const class_key* class_info::find_key(const std::string_view key) const
	{
		for (const class_key& k : keys)
		{
			if (k.name == key)
			{
				return &k;
			}
		}

		return nullptr;
	}

	const class_catalog& class_catalog::c1()
	{
		static const built data;
		static const class_catalog catalog = []
		{
			class_catalog c;
			c.classes_ = data.classes;
			c.map_count_ = catalog_map_count;
			return c;
		}();

		return catalog;
	}

	const class_info* class_catalog::find(const std::string_view classname) const
	{
		const auto it = std::lower_bound(classes_.begin(), classes_.end(), classname, [](const class_info& c, const std::string_view name) { return c.name < name; });
		return (it != classes_.end() && it->name == classname) ? &*it : nullptr;
	}

	void class_catalog::box(const std::string_view classname, float mins[3], float maxs[3]) const
	{
		if (const class_info* info = find(classname))
		{
			std::copy(std::begin(info->mins), std::end(info->mins), mins);
			std::copy(std::begin(info->maxs), std::end(info->maxs), maxs);
			return;
		}

		// a class the shipped maps do not use may still be a known one
		for (const known_box& k : known)
		{
			if (matches(k.name, classname))
			{
				std::copy(std::begin(k.mins), std::end(k.mins), mins);
				std::copy(std::begin(k.maxs), std::end(k.maxs), maxs);
				return;
			}
		}

		for (int i = 0; i < 3; i++)
		{
			mins[i] = -8.0f;
			maxs[i] = 8.0f;
		}
	}
}
