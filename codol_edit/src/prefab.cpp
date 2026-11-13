// document: prefabs. A misc_prefab entity stands for the contents of another .map, placed by its origin and
// angles. Expanding one is: paste the file's objects, turn and move them, delete the entity - with the operations
// the editor has anyway, so textures stay locked, brush entities keep their origin and entities their angles.

#include "doc_impl.hpp"
#include "codol_edit/prefab.hpp"

#include <fstream>

namespace codol::edit
{
	namespace
	{
		std::string prefab_key(const std::string_view model)
		{
			std::string key(model);
			for (char& c : key)
			{
				c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}

			return key;
		}

		// `top`: the document is the map itself, not a prefab that is being expanded for it ( what comes out of a
		// prefab is counted once, when it arrives in the map )
		bool expand(document& doc, const prefab_loader& load, prefab_result& result, const bool selected_only, const int depth,
			std::vector<std::string>& chain, const bool top = true)
		{
			std::vector<object_id> prefabs;

			if (selected_only)
			{
				for (const object_id id : doc.selected_entities())
				{
					if (document::is_prefab(*doc.find_entity(id)))
					{
						prefabs.push_back(id);
					}
				}
			}
			else
			{
				for (const object_id id : doc.entities())
				{
					if (document::is_prefab(*doc.find_entity(id)))
					{
						prefabs.push_back(id);
					}
				}
			}

			if (prefabs.empty())
			{
				return false;
			}

			const bool lock = doc.texture_lock();
			doc.set_texture_lock(true);
			doc.begin_transaction(selected_only ? "Stamp prefab" : "Expand prefabs");

			std::vector<object_id> created;
			int done = 0;

			for (const object_id id : prefabs)
			{
				const entity* e = doc.find_entity(id);
				const std::string model(e->value("model"));
				const vec3 origin = e->origin(), angles = e->angles();
				const std::string key = prefab_key(model);
				const std::string where = "misc_prefab \"" + model + "\" at ( " + format_key_vec3(origin) + " )";

				if (model.empty())
				{
					result.warnings.push_back(where + ": no \"model\" key; it stays an entity");
					continue;
				}

				if (std::find(chain.begin(), chain.end(), key) != chain.end())
				{
					result.warnings.push_back(where + ": the prefab contains itself; it stays an entity");
					continue;
				}

				std::string text, error;
				if (!load || !load(model, text, error))
				{
					result.warnings.push_back(where + ": " + (error.empty() ? std::string("the file was not found") : error) + "; it stays an entity");
					continue;
				}

				if (!selected_only)
				{
					// the prefabs inside it first
					document inner;
					if (!inner.load_map_text(text, error))
					{
						result.warnings.push_back(where + ": " + error + "; it stays an entity");
						continue;
					}

					if (depth <= 0)
					{
						result.warnings.push_back(where + ": prefabs are nested too deep; the ones inside are left out");
					}
					else
					{
						chain.push_back(key);
						if (expand(inner, load, result, false, depth - 1, chain, false))
						{
							text = inner.save_map_text();
						}
						chain.pop_back();
					}
				}

				if (!doc.paste_map_text(text, nullptr, 0.0, "Prefab"))
				{
					result.warnings.push_back(where + ": " + doc.last_error() + "; it stays an entity");
					continue;
				}

				const transform place = transform::concat(transform::rotation_angles({}, angles), transform::translation(origin));
				if (!(origin == vec3{}) || !(angles == vec3{}))
				{
					doc.transform_selection(place, "Place prefab");
				}

				for (const object_id made : doc.selection())
				{
					switch (top ? doc.kind(made) : object_kind::none)
					{
					case object_kind::brush: result.brushes++; break;
					case object_kind::patch: result.patches++; break;
					case object_kind::entity: result.entities++; break;
					default: break;
					}

					created.push_back(made);
				}

				doc.select(id);
				doc.delete_selection();
				result.expanded++;
				done++;
			}

			if (selected_only && !created.empty())
			{
				doc.select(std::span<const object_id>(created));
			}
			else
			{
				doc.deselect_all();
			}

			doc.end_transaction();
			doc.set_texture_lock(lock);
			return done > 0;
		}
	}

	bool document::expand_prefabs(const prefab_loader& load, prefab_result& result, const bool selected_only, const int max_depth)
	{
		std::vector<std::string> chain;

		if (!expand(*this, load, result, selected_only, max_depth, chain))
		{
			return d->fail(result.warnings.empty() ? (selected_only ? "Stamp prefab: no misc_prefab selected" : "The map has no prefabs") : result.warnings.front());
		}

		return true;
	}

	object_id document::convert_selection_to_prefab(const std::string_view model, std::string& prefab_text, const vec3* origin)
	{
		if (d->sel.objects.empty())
		{
			d->fail("Convert to prefab: nothing selected");
			return no_object;
		}

		if (model.empty())
		{
			d->fail("Convert to prefab: no file name");
			return no_object;
		}

		const bounds box = selection_bounds();
		const vec3 centre = box.center();
		const vec3 at = origin ? *origin : vec3{ std::round(centre.x), std::round(centre.y), std::round(box.mins.z) };

		const bool lock = d->texture_lock;
		d->texture_lock = true;
		begin_transaction("Convert to prefab");

		if (!(at == vec3{}))
		{
			translate(at * -1.0);
		}

		prefab_text = selection_map_text(false);
		delete_selection();

		const std::pair<std::string, std::string> pairs[] = { { "model", std::string(model) } };
		const object_id id = create_point_entity("misc_prefab", at, pairs);

		d->texture_lock = lock;

		if (id == no_object || prefab_text.empty())
		{
			const std::string why = d->last_error;
			cancel_transaction();
			d->fail(why.empty() ? std::string("Convert to prefab: failed") : why);
			return no_object;
		}

		end_transaction();
		return id;
	}

	// ---- files --------------------------------------------------------------------------------------

	std::vector<std::filesystem::path> prefab_roots_for(const std::filesystem::path& map_file)
	{
		std::vector<std::filesystem::path> roots;
		std::error_code ec;
		std::filesystem::path dir = std::filesystem::absolute(map_file, ec).parent_path();

		for (int level = 0; level < 5 && !dir.empty(); level++)
		{
			roots.push_back(dir);
			roots.push_back(dir / "map_source");

			const auto parent = dir.parent_path();
			if (parent == dir)
			{
				break;
			}

			dir = parent;
		}

		return roots;
	}

	std::filesystem::path resolve_prefab(const std::string_view model, const std::span<const std::filesystem::path> roots)
	{
		std::string name(model);
		for (char& c : name)
		{
			if (c == '\\') { c = '/'; }
		}

		while (!name.empty() && name.front() == '/')
		{
			name.erase(name.begin());
		}

		if (name.empty())
		{
			return {};
		}

		std::error_code ec;
		const std::filesystem::path relative(name);

		for (const auto& root : roots)
		{
			for (const auto& candidate : { root / relative, root / std::filesystem::path(name + ".map") })
			{
				if (std::filesystem::is_regular_file(candidate, ec))
				{
					return candidate;
				}
			}
		}

		// an absolute path is taken as it is
		if (relative.is_absolute() && std::filesystem::is_regular_file(relative, ec))
		{
			return relative;
		}

		return {};
	}

	prefab_loader file_prefab_loader(std::vector<std::filesystem::path> roots)
	{
		return [roots = std::move(roots)](const std::string_view model, std::string& text, std::string& error)
		{
			const auto file = resolve_prefab(model, roots);
			if (file.empty())
			{
				error = "the file was not found below a map_source folder";
				return false;
			}

			std::ifstream in(file, std::ios::binary);
			if (!in.is_open())
			{
				error = "the file cannot be read";
				return false;
			}

			text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			return true;
		};
	}

	bool expand_prefabs_from_disk(document& doc, const std::filesystem::path& map_file, prefab_result& result,
		const std::span<const std::filesystem::path> extra_roots)
	{
		std::vector<std::filesystem::path> roots(extra_roots.begin(), extra_roots.end());
		const auto more = prefab_roots_for(map_file);
		roots.insert(roots.end(), more.begin(), more.end());

		const bool ok = doc.expand_prefabs(file_prefab_loader(std::move(roots)), result, false);
		// the compilers work on the map as it is now: nothing of this is something to undo
		doc.clear_undo();
		return ok;
	}
}
