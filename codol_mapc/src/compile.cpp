// The driver: files, the order of the stages, the report.

#include "mapc_impl.hpp"

#include "codol_mapc/server_check.hpp"
#include "codol_edit/prefab.hpp"

#include <cstdio>
#include <set>

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

	// Models of entities ( script_model .. ) that the map zone does not have and a donor zone does: the xmodel
	// becomes a top level asset in front of the gfx_map, with what it depends on, so that the game finds it by
	// name once the map is loaded. Static models ( misc_model with "codol_static" ) are the business of
	// stage_models. A model that neither the zone nor a donor has is left alone: it may be in one of the game's
	// common zones, which the compiler does not read.
	void stage_entity_models(context& ctx)
	{
		std::vector<std::string> wanted;
		for (const edit::object_id id : ctx.edited.entities())
		{
			const edit::entity* e = ctx.edited.find_entity(id);
			if (!e || (e->classname() == "misc_model" && e->find("codol_static")))
			{
				continue;
			}
			const std::string model(e->value("model"));
			if (model.empty() || model[0] == '*' || std::find(wanted.begin(), wanted.end(), model) != wanted.end()
				|| ctx.editor.find_asset(c1::ASSET_TYPE_XMODEL, model) != ZoneEditor::npos)
			{
				continue;
			}
			wanted.push_back(model);
		}
		if (wanted.empty())
		{
			return;
		}
		std::vector<std::unique_ptr<ZoneEditor>> donors;
		for (const auto& file : ctx.opts.donors)
		{
			auto donor = std::make_unique<ZoneEditor>();
			if (const zone::Status s = donor->open(file, ctx.key); !s)
			{
				ctx.warn("donor zone: " + s.message);
				continue;
			}
			donors.push_back(std::move(donor));
		}
		bool added = false;
		for (const std::string& model : wanted)
		{
			for (const auto& donor : donors)
			{
				const std::size_t in_donor = donor->find_asset(c1::ASSET_TYPE_XMODEL, model);
				if (in_donor == ZoneEditor::npos)
				{
					continue;
				}
				const std::size_t gfx_now = ctx.editor.find_asset(c1::ASSET_TYPE_GFXWORLD, ctx.editor.asset(ctx.gfx_asset).name);
				const std::size_t before = ctx.editor.asset_count();
				if (const zone::Status s = ctx.editor.add_asset(*donor, in_donor, gfx_now, true); !s)
				{
					ctx.warn("entity model '" + model + "' could not be taken from zone '" + donor->name() + "': " + s.message
						+ ". Entities with it have no model in the game unless another loaded zone has it");
				}
				else
				{
					ctx.rep.model_assets_added += static_cast<int>(ctx.editor.asset_count() - before);
					ctx.note(format("entity models: xmodel '%s' taken from zone '%s' with %zu assets it depends on", model.c_str(), donor->name().c_str(),
						ctx.editor.asset_count() - before - 1));
					added = true;
				}
				break;
			}
		}
		if (added)
		{
			ctx.reload();		// the asset table moved
		}
	}

	// A project's assets and rawfiles ( options::assets, rawfiles, rawfiles_appended ). What ZoneEditor::add_asset_closure
	// does for a map compiled from scratch it does here: an animation, an alias list with its loaded sounds and curve, an
	// effect with its materials, a model with materials, images and technique sets become top level assets of the map
	// zone, behind what it has; an asset whose type and name the zone already has stays. The game finds them by name
	// once the map is loaded. What this does not do: a material that comes this way is not put into the GfxWorld's
	// material table - a world brush face with it still is not drawn ( stage_surfaces ).
	void stage_assets(context& ctx)
	{
		static const char* const usual[] = { "xanim", "sndcurve", "loaded_sound", "sound", "fx", "xmodel", "material" };
		bool changed = false;
		for (const options::asset_source& source : ctx.opts.assets)
		{
			ZoneEditor donor;
			if (const zone::Status s = donor.open(source.zone, ctx.key); !s)
			{
				ctx.warn("assets: " + source.zone.string() + ": " + s.message);
				continue;
			}
			std::vector<std::int32_t> types;
			if (source.types.empty())
			{
				for (const char* name : usual)
				{
					types.push_back(zone::asset_type_from_name(name));
				}
			}
			for (const std::string& name : source.types)
			{
				const std::int32_t type = zone::asset_type_from_name(name);
				if (type < 0)
				{
					ctx.warn("assets: '" + name + "' is no asset type ( " + source.zone.filename().string() + " )");
					continue;
				}
				types.push_back(type);
			}
			std::map<std::string, int> carried;
			int kept = 0, dependencies = 0, failed = 0;
			std::set<std::pair<std::int32_t, std::string>> before;
			for (std::size_t k = 0; k < ctx.editor.asset_count(); k++)
			{
				const zone::EditorAsset a = ctx.editor.asset(k);
				before.emplace(a.type, a.name);
			}
			for (std::size_t k = 0; k < donor.asset_count(); k++)
			{
				const zone::EditorAsset a = donor.asset(k);
				if (std::find(types.begin(), types.end(), a.type) == types.end())
				{
					continue;
				}
				if (ctx.editor.find_asset(a.type, a.name) != ZoneEditor::npos)
				{
					if (before.count({ a.type, a.name }))
					{
						kept++;
					}
					else
					{
						// it came along with an asset that points at it: one of the carried, not a dependency
						carried[zone::asset_type_name(a.type)]++;
						dependencies -= dependencies ? 1 : 0;
					}
					continue;
				}
				const std::size_t count = ctx.editor.asset_count();
				if (const zone::Status s = ctx.editor.add_asset_closure(donor, k); !s)
				{
					ctx.warn("assets: " + zone::asset_type_name(a.type) + " '" + a.name + "' of " + source.zone.filename().string() + " could not be carried: " + s.message);
					failed++;
					continue;
				}
				carried[zone::asset_type_name(a.type)]++;
				dependencies += static_cast<int>(ctx.editor.asset_count() - count - 1);
				changed = true;
			}
			std::string list;
			int total = 0;
			for (const auto& [type, n] : carried)
			{
				list += format("%s%d %s", list.empty() ? "" : ", ", n, type.c_str());
				total += n;
			}
			ctx.rep.assets_carried += total;
			ctx.rep.asset_dependencies += dependencies;
			ctx.note(format("assets: carried from %s: %s ( and %d assets they point at ); %d the zone already has, %d failed", source.zone.filename().string().c_str(),
				list.empty() ? "nothing" : list.c_str(), dependencies, kept, failed));
		}
		const auto read_file = [&](const std::filesystem::path& file, std::vector<std::uint8_t>& bytes)
		{
			std::FILE* f = nullptr;
#ifdef _WIN32
			_wfopen_s(&f, file.c_str(), L"rb");
#else
			f = std::fopen(file.string().c_str(), "rb");
#endif
			if (!f)
			{
				return false;
			}
			std::uint8_t block[65536];
			for (std::size_t got; (got = std::fread(block, 1, sizeof(block), f)) > 0;)
			{
				bytes.insert(bytes.end(), block, block + got);
			}
			std::fclose(f);
			return true;
		};
		for (int pass = 0; pass < 2; pass++)
		{
			for (const auto& [name, file] : pass == 0 ? ctx.opts.rawfiles : ctx.opts.rawfiles_appended)
			{
				std::vector<std::uint8_t> bytes, now;
				if (!read_file(file, bytes))
				{
					ctx.warn("rawfile '" + name + "': cannot read " + file.string());
					continue;
				}
				std::size_t had = 0;
				if (pass == 1)
				{
					// behind what the zone's rawfile holds; a line break between the two when the old text has none at its end
					zone::RawFileView view;
					const zaddr at = ctx.editor.zone().find_asset(c1::ASSET_TYPE_RAWFILE, name);
					if (at && zone::read_rawfile(ctx.editor.zone(), at, view) && zone::rawfile_contents(view, now))
					{
						while (!now.empty() && now.back() == 0)
						{
							now.pop_back();
						}
						had = now.size();
						if (!now.empty() && now.back() != '\n')
						{
							now.push_back('\n');
						}
					}
					now.insert(now.end(), bytes.begin(), bytes.end());
					bytes = std::move(now);
				}
				if (const zone::Status s = ctx.editor.set_rawfile(name, bytes); !s)
				{
					ctx.warn("rawfile '" + name + "': " + s.message);
					continue;
				}
				ctx.rep.rawfiles_set++;
				changed = true;
				ctx.note(pass == 0 ? format("rawfile %s: %zu bytes from %s", name.c_str(), bytes.size(), file.filename().string().c_str())
					: format("rawfile %s: %zu bytes of %s behind the %zu the zone had", name.c_str(), bytes.size() - had, file.filename().string().c_str(), had));
			}
		}
		if (changed)
		{
			ctx.reload();		// the asset table moved
		}
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
			if (!opts.donors.empty())
			{
				stage_entity_models(ctx);
			}
		}
		if (!opts.assets.empty() || !opts.rawfiles.empty() || !opts.rawfiles_appended.empty())
		{
			if (rep.entities_changed)
			{
				ctx.reload();
			}
			stage_assets(ctx);
		}
		if (opts.models && (!d.models_moved.empty() || !d.models_deleted.empty() || !d.models_added.empty()))
		{
			if (rep.entities_changed)
			{
				ctx.reload();
			}
			stage_models(ctx);
		}
		const bool brush_changes = !d.brushes.empty() || !d.brushes_deleted.empty();
		if (opts.brushes && brush_changes)
		{
			ctx.reload();
			stage_brushes(ctx);
		}
		// the render faces do not need the collision brushes ( --no-brushes still draws the new geometry )
		if (opts.surfaces && brush_changes)
		{
			ctx.reload();
			stage_surfaces(ctx);
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
		rep.identical = !rep.entities_changed && !rep.model_assets_added && rep.assets_carried + rep.rawfiles_set == 0
			&& rep.models_moved + rep.models_deleted + rep.models_added + rep.models_hidden == 0
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
		// misc_prefab entities of the edited map: their contents in their place, so that the brushes and entities of
		// a prefab are new brushes and entities like any other
		edit::prefab_result prefabs;
		edit::expand_prefabs_from_disk(edited, opts.map, prefabs);
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
		// a project's zone is searched for the models of new instances and entities like any donor
		options with_donors = opts;
		for (const options::asset_source& source : opts.assets)
		{
			with_donors.donors.push_back(source.zone);
		}
		if (!compile_zone(editor, baseline, edited, with_donors, key, zone_bytes, rep, error))
		{
			return false;
		}
		if (prefabs.expanded)
		{
			rep.notes.push_back("prefabs: " + std::to_string(prefabs.expanded) + " expanded into " + std::to_string(prefabs.brushes) + " brushes, "
				+ std::to_string(prefabs.patches) + " patches, " + std::to_string(prefabs.entities) + " entities");
		}
		for (const std::string& warning : prefabs.warnings)
		{
			rep.warnings.push_back("prefab: " + warning);
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
			// The client checks the length of every stock zone against a table in its exe before it loads
			// anything ( its "data is incomplete or damaged" start error ), so the file is made exactly as long
			// as the zone it replaces. A better compression level is tried when the first result is too long.
			std::error_code size_error;
			const std::uint64_t stock = std::filesystem::file_size(opts.base, size_error);
			bool written = false;
			if (!size_error && stock)
			{
				for (const int level : { opts.level, 9 })
				{
					zone::FastFileWriteOptions write;
					write.compression = level;
					write.exactFileSize = stock;
					if (zone::write_fastfile(opts.out, zone_bytes, key, write))
					{
						rep.notes.push_back(format("container: the file has the length of the base file ( %llu bytes ), which the client checks",
							static_cast<unsigned long long>(stock)));
						written = true;
						break;
					}
				}
			}
			if (!written)
			{
				zone::FastFileWriteOptions write;
				write.compression = opts.level;
				if (const zone::Status s = zone::write_fastfile(opts.out, zone_bytes, key, write); !s)
				{
					error = s.message;
					return false;
				}
				rep.warnings.push_back("container: the file could not be given the length of the base file; the client refuses a stock zone of another length at start");
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
