// The driver: files, the order of the stages, the report.

#include "mapc_impl.hpp"

#include "codol_mapc/server_check.hpp"

namespace codol::mapc
{
	ZoneDataId context::data(const zaddr address, const char* what) const
	{
		std::uint32_t offset = 0;
		const ZoneDataId id = editor.data_at(address, &offset);
		if (!id || offset != 0)
		{
			throw compile_error(format("%s: no stream data at 0x%08X ( the zone is not laid out as expected )", what, address));
		}
		return id;
	}

	void context::set(const ZoneDataId id, const std::span<const std::uint8_t> bytes, const std::span<const ZonePointer> pointers, const char* what)
	{
		check(editor.set_data(id, bytes, pointers), what);
	}

	void context::refresh()
	{
		const Zone& z = editor.zone();
		gfx_address = zone::first_asset(z, c1::ASSET_TYPE_GFXWORLD);
		clip_address = zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_MP);
		if (!clip_address)
		{
			clip_address = zone::first_asset(z, c1::ASSET_TYPE_CLIPMAP_SP);
		}
		if (!gfx_address || !zone::read_gfx_world(z, gfx_address, gfx) || !clip_address || !zone::read_clip_map(z, clip_address, clip))
		{
			throw compile_error("the zone has no gfx_map / col_map ( not a map zone )");
		}
		gfx_header = editor.header_data(gfx_address);
		clip_header = editor.header_data(clip_address);
		gfx_asset = clip_asset = ZoneEditor::npos;
		const auto entries = z.assets();
		for (std::size_t i = 0; i < entries.size(); i++)
		{
			if (entries[i].header == gfx_address)
			{
				gfx_asset = i;
			}
			if (entries[i].header == clip_address)
			{
				clip_asset = i;
			}
		}
		if (!gfx_header || !clip_header || gfx_asset == ZoneEditor::npos || clip_asset == ZoneEditor::npos)
		{
			throw compile_error("gfx_map / col_map are not top level assets of the zone");
		}
	}

	void context::reload()
	{
		std::vector<std::uint8_t> bytes;
		check(editor.write(bytes), "writing the zone");
		const std::string name = editor.name();
		check(editor.load(std::move(bytes), name), "loading the written zone");
		refresh();
	}

	bool entity_string_of(const edit::document& baseline, const edit::document& edited, const std::string_view original_ents, std::string& out,
		std::vector<std::string>& warnings, std::string& error)
	{
		// the export works on a document that knows its baseline; `edited` is not to be changed
		edit::document work;
		if (!work.load_map_text(edited.save_map_text(), error))
		{
			return false;
		}
		work.set_import_baseline(baseline.save_map_text(), std::string(original_ents));
		edit::override_export exported;
		if (!work.export_entity_override(exported, error))
		{
			return false;
		}
		// three comment lines, the entity string, one line end
		std::string_view text = exported.text;
		while (text.starts_with("//"))
		{
			const std::size_t end = text.find('\n');
			text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
		}
		if (text.ends_with("\n"))
		{
			text.remove_suffix(1);
		}
		out = std::string(text);
		for (std::string& w : exported.warnings)
		{
			// static models are compiled by their own stage
			if (w.find("misc_model skipped") == std::string::npos)
			{
				warnings.push_back("entities: " + std::move(w));
			}
		}
		return true;
	}

	void stage_entities(context& ctx)
	{
		zone::MapEntsView ents;
		const zaddr address = zone::find_map_ents(ctx.zone());
		if (!address || !zone::read_map_ents(ctx.zone(), address, ents))
		{
			throw compile_error("the zone has no MapEnts");
		}
		std::string text, error;
		if (!entity_string_of(ctx.baseline, ctx.edited, ents.entityString, text, ctx.rep.warnings, error))
		{
			throw compile_error("entities: " + error);
		}
		ctx.rep.entities_before = static_cast<int>(zone::count_entities(ents.entityString));
		ctx.rep.entities_after = static_cast<int>(zone::count_entities(text));
		if (text == ents.entityString)
		{
			return;
		}
		check(ctx.editor.set_map_ents(text), "entities");
		ctx.rep.entities_changed = true;
		ctx.note(format("entities: entity string replaced, %zu -> %zu characters, %d -> %d entities", ents.entityString.size(), text.size(), ctx.rep.entities_before,
			ctx.rep.entities_after));
	}

	bool compile_zone(ZoneEditor& editor, const edit::document& baseline, const edit::document& edited, const options& opts, const zone::ClientKey& key,
		std::vector<std::uint8_t>& zone_bytes, report& rep, std::string& error)
	try
	{
		rep = {};
		zone_bytes.clear();
		if (!editor.loaded() || !editor.zone().loaded())
		{
			error = "no base zone";
			return false;
		}
		rep.zone_bytes_before = editor.zone().stream_size();
		if (!diff_documents(editor.zone(), baseline, edited, rep.diff, error))
		{
			return false;
		}
		for (const std::string& text : rep.diff.not_compiled)
		{
			rep.warnings.push_back(text);
		}
		context ctx{ editor, key, opts, rep, baseline, edited };
		ctx.refresh();

		// The collision data as a dedicated server reads it ( server_check.hpp ): when the base zone is something the
		// server takes, the compiled zone has to be as well - otherwise a stage wrote a shape the game's map compiler
		// never does, and that is this compiler's error, not the map's.
		const map_diff& d = rep.diff;
		const bool collision_edited = (opts.models && (!d.models_moved.empty() || !d.models_deleted.empty() || !d.models_added.empty()))
			|| (opts.brushes && (!d.brushes.empty() || !d.brushes_deleted.empty()));
		bool base_accepted = false;
		if (collision_edited)
		{
			std::vector<std::uint8_t> base_bytes;
			std::string why;
			check(editor.write(base_bytes), "writing the base zone");
			base_accepted = check_server_collision(base_bytes, why);
			if (!base_accepted)
			{
				ctx.warn("the collision data of the base zone is not what a dedicated server ( CoD4x_Server ) takes: " + why + ". The compiled zone is not checked against it");
			}
		}

		if (opts.entities)
		{
			stage_entities(ctx);
		}
		if (opts.models && (!d.models_moved.empty() || !d.models_deleted.empty() || !d.models_added.empty()))
		{
			if (rep.entities_changed)
			{
				ctx.reload();
			}
			stage_models(ctx);
		}
		if (opts.brushes && (!d.brushes.empty() || !d.brushes_deleted.empty()))
		{
			ctx.reload();
			stage_brushes(ctx);
			if (opts.surfaces)
			{
				ctx.reload();
				stage_surfaces(ctx);
			}
		}

		check(editor.write(zone_bytes), "writing the zone");
		rep.zone_bytes_after = zone_bytes.size();
		if (base_accepted)
		{
			std::string why;
			server_collision_info info;
			if (!check_server_collision(zone_bytes, why, &info))
			{
				throw compile_error("the compiled collision data is not what a dedicated server takes ( an error of the compiler; nothing was written ): " + why);
			}
			ctx.note(format("collision data read back the way a dedicated server reads it: %u brushes ( %u sides, %u edge bytes ), %u planes, %u leaf brush nodes, %u + %u leaf brushes, "
				"%u static models - accepted", info.brushes, info.brush_sides, info.brush_edges, info.planes, info.leaf_brush_nodes, info.leaf_brushes, info.inline_leaf_brushes,
				info.static_models));
		}
		rep.identical = !rep.entities_changed && rep.models_moved + rep.models_deleted + rep.models_added + rep.models_hidden == 0
			&& rep.brushes_replaced + rep.brushes_deleted + rep.brushes_added + rep.surfaces_added == 0;
		// leave the editor on the compiled zone, so that the caller can read the result back
		{
			std::vector<std::uint8_t> copy = zone_bytes;
			const std::string name = editor.name();
			check(editor.load(std::move(copy), name), "loading the compiled zone");
		}
		return true;
	}
	catch (const std::exception& e)
	{
		error = e.what();
		return false;
	}

	bool compile(const options& opts, report& rep, std::string& error)
	{
		rep = {};
		zone::ClientKey key;
		if (const zone::Status s = key.load_from_exe(opts.exe); !s)
		{
			error = "client key: " + s.message;
			return false;
		}
		ZoneEditor editor;
		if (const zone::Status s = editor.open(opts.base, key); !s)
		{
			error = s.message;
			return false;
		}

		edit::document edited, baseline;
		if (!edited.load_map_file(opts.map, error))
		{
			error = opts.map.string() + ": " + error;
			return false;
		}
		std::filesystem::path baseline_file = opts.baseline;
		if (baseline_file.empty())
		{
			// the sidecar the editor writes next to a saved import
			std::filesystem::path a = opts.map, b = opts.map;
			a.replace_extension(".codol_base.map");
			b += ".codol_base.map";
			std::error_code ec;
			baseline_file = std::filesystem::exists(a, ec) ? a : std::filesystem::exists(b, ec) ? b : std::filesystem::path();
		}
		if (!baseline_file.empty())
		{
			if (!baseline.load_map_file(baseline_file, error))
			{
				error = baseline_file.string() + ": " + error;
				return false;
			}
		}
		else
		{
			// no baseline on disk: the import of the base fast file is the baseline
			edit::document imported;
			const edit::entity* world = edited.find_entity(edited.worldspawn());
			const std::string map_name = world ? std::string(world->value("codol_map")) : std::string();
			if (!imported.import_fastfile(opts.base, opts.exe, error, map_name))
			{
				error = "baseline ( import of the base fast file ): " + error;
				return false;
			}
			if (!baseline.load_map_text(std::string(imported.import_baseline_map()), error))
			{
				return false;
			}
		}

		std::vector<std::uint8_t> zone_bytes;
		if (!compile_zone(editor, baseline, edited, opts, key, zone_bytes, rep, error))
		{
			return false;
		}
		if (!opts.zone_out.empty())
		{
			if (const zone::Status s = zone::write_file(opts.zone_out, zone_bytes); !s)
			{
				error = s.message;
				return false;
			}
		}
		if (!opts.out.empty())
		{
			zone::FastFileWriteOptions write;
			write.compression = opts.level;
			if (const zone::Status s = zone::write_fastfile(opts.out, zone_bytes, key, write); !s)
			{
				error = s.message;
				return false;
			}
		}
		return true;
	}

	std::string report::text() const
	{
		std::string out;
		const auto line = [&](const std::string& text) { out += text + "\n"; };
		line(format("difference: static models %zu moved, %zu deleted, %zu added; brushes %zu edited / added, %zu deleted", diff.models_moved.size(),
			diff.models_deleted.size(), diff.models_added.size(), diff.brushes.size(), diff.brushes_deleted.size()));
		if (identical)
		{
			line("nothing to compile: the zone is the base zone, byte for byte");
		}
		line(format("entities: %s ( %d -> %d )", entities_changed ? "changed" : "unchanged", entities_before, entities_after));
		line(format("static models: %d moved, %d deleted, %d added, %d kept as an undrawn holder; model assets %d removed, %d added; collision entries %d moved, %d deleted, %d added",
			models_moved, models_deleted, models_added, models_hidden, model_assets_removed, model_assets_added, clip_models_moved, clip_models_deleted, clip_models_added));
		line(format("brushes: %d replaced, %d deleted, %d added; %d sides and %d planes added; %d collision leafs / brush models touched", brushes_replaced, brushes_deleted,
			brushes_added, brush_sides_added, planes_added, leafs_touched));
		line(format("surfaces: %d added ( %d vertices, %d triangles ), %d faces not drawn", surfaces_added, vertices_added, triangles_added, faces_not_drawn));
		line(format("zone: %llu -> %llu bytes", static_cast<unsigned long long>(zone_bytes_before), static_cast<unsigned long long>(zone_bytes_after)));
		for (const std::string& n : notes)
		{
			line("  " + n);
		}
		for (const std::string& w : warnings)
		{
			line("  warning: " + w);
		}
		return out;
	}
}
