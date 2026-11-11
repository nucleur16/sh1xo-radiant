// document: fastfile import and the entity override export. Built on the engine independent code of the
// radiant fork ( src/codol/map_zone, map_ents, map_geometry ), which is compiled into this library from where
// it is, so the result is the one codol_mapconv produces.

#include "std_include.hpp"	// tests/stub: the standard headers + the declarations of src/codol

#undef min
#undef max

#include "codol/map_zone.hpp"

#include "doc_impl.hpp"

namespace codol::edit
{
	bool document::import_fastfile(const std::filesystem::path& fastfile, const std::filesystem::path& client_exe,
		std::string& error, const std::string_view map_name)
	{
		mapgeo::zone_map zm;
		if (!mapgeo::read_zone_map(fastfile, client_exe, zm, error))
		{
			return false;
		}

		mapents::entity_set set;
		if (!mapents::parse_entity_string(zm.entity_text, set, error))
		{
			error = "entities: " + error;
			return false;
		}

		if (!set.has_crc)
		{
			error = "entities: no base crc";
			return false;
		}

		set.trigger_models = zm.trigger_models;

		const std::string name = map_name.empty() ? fastfile.stem().string() : std::string(map_name);
		std::string text = mapents::to_radiant_map(set, name, zm.geometry.empty() ? nullptr : &zm.geometry);

		if (!load_map_text(std::move(text), error))
		{
			return false;
		}

		d->baseline_map = d->source;	// the document was just read from exactly this text
		d->original_ents = std::move(zm.entity_text);
		return true;
	}

	bool document::export_entity_override(override_export& out, std::string& error) const
	{
		out = {};

		const std::string current = d->write(false);
		mapents::export_result result;

		const bool ok = d->baseline_map
			? mapents::merge_radiant_map(current, *d->baseline_map, d->original_ents, result, error)
			: mapents::from_radiant_map(current, result, error);

		if (!ok)
		{
			return false;
		}

		out.text = std::move(result.text);
		out.map_name = std::move(result.map_name);
		out.base_crc = result.base_crc;
		out.exported = result.exported;
		out.static_models = result.static_models;
		out.warnings = std::move(result.warnings);
		return true;
	}

	bool document::has_import_baseline() const
	{
		return d->baseline_map != nullptr;
	}

	std::string_view document::import_baseline_map() const
	{
		return d->baseline_map ? std::string_view(*d->baseline_map) : std::string_view();
	}

	std::string_view document::import_original_ents() const
	{
		return d->original_ents;
	}

	void document::set_import_baseline(std::string baseline_map, std::string original_ents)
	{
		d->baseline_map = std::make_shared<const std::string>(std::move(baseline_map));
		d->original_ents = std::move(original_ents);
	}
}
