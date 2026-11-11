// The difference between the import of a zone and the edited document.
//
// The document that import_fastfile makes lists the objects of the zone in a fixed order ( src/codol/map_zone.cpp,
// map_ents.cpp to_radiant_map ): the collision brushes of sub model 0 in the worldspawn, those of sub model N in
// the entity with "model" "*N", each in cbrush_t order, and one misc_model with "codol_static" per GfxWorld static
// model, in dpvs.smodelDrawInsts order. That order maps the objects of the baseline to zone indices. An object of
// the edited document that is not an exact copy of a baseline object is an edit.

#include "mapc_impl.hpp"

#include <functional>
#include <unordered_map>

namespace codol::mapc
{
	bool is_tool_material(const std::string_view name)
	{
		// src/codol/map_geometry.cpp is_tool_material
		static constexpr std::string_view words[] =
		{
			"clip", "hint", "mantle", "nosight", "portal", "ladder", "trigger", "caulk", "nodraw", "skip", "lightgrid", "aitrig",
		};
		for (const auto& word : words)
		{
			if (name.find(word) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	std::uint32_t vis_data_count(const std::uint32_t stored, const std::uint32_t count, const std::uint32_t new_count)
	{
		if (stored == count)
		{
			return new_count;
		}
		for (const std::uint32_t align : { 1u, 2u, 4u, 8u, 16u, 32u })
		{
			const auto words = [align](const std::uint32_t n) { return ((n + 31) / 32 + align - 1) / align * align; };
			if (stored == words(count))
			{
				return words(new_count);
			}
		}
		return stored * 32ull >= new_count ? stored : new_count;
	}

	std::map<int, std::vector<int>> imported_brush_order(const Zone& zone, const zone::ClipMapView& clip)
	{
		// src/codol/map_zone.cpp read_zone_map: the owner of a brush is the last sub model > 0 whose leaf brush tree lists it
		std::vector<int> owner(clip.brushes.size(), 0);
		const auto nodes = zone.get_array<c1::cLeafBrushNode_s>(clip.raw.leafbrushNodes, clip.raw.leafbrushNodesCount);
		const std::function<void(std::size_t, std::vector<std::uint16_t>&, int)> collect = [&](const std::size_t index, std::vector<std::uint16_t>& out, const int depth)
		{
			if (index >= nodes.size() || depth > 64)
			{
				return;
			}
			const auto& node = nodes[index];
			if (node.leafBrushCount > 0)
			{
				std::uint32_t address = 0;
				std::memcpy(&address, node.data, 4);
				const auto brushes = zone.get_array<std::uint16_t>(address, static_cast<std::size_t>(node.leafBrushCount));
				out.insert(out.end(), brushes.begin(), brushes.end());
				return;
			}
			for (int k = 1; k <= -node.leafBrushCount; k++)
			{
				collect(index + static_cast<std::size_t>(k), out, depth + 1);
			}
			std::uint16_t child[2] = {};
			std::memcpy(child, node.data + 8, 4);
			for (const auto offset : child)
			{
				if (offset != 0)
				{
					collect(index + offset, out, depth + 1);
				}
			}
		};
		for (std::size_t m = 1; m < clip.subModels.size(); m++)
		{
			const auto node = clip.subModels[m].leaf.leafBrushNode;
			if (node <= 0)
			{
				continue;
			}
			std::vector<std::uint16_t> indices;
			collect(static_cast<std::size_t>(node), indices, 0);
			for (const auto index : indices)
			{
				if (index < owner.size())
				{
					owner[index] = static_cast<int>(m);
				}
			}
		}

		std::map<int, std::vector<int>> order;
		for (std::size_t i = 0; i < clip.brushes.size(); i++)
		{
			// sides that are the same plane twice count once; a brush needs four
			struct side { double n[3], d; };
			std::vector<side> sides;
			for (const auto& s : clip.brushes[i].sides)
			{
				const side now{ { s.plane.normal[0], s.plane.normal[1], s.plane.normal[2] }, s.plane.dist };
				bool duplicate = false;
				for (const auto& have : sides)
				{
					if (std::fabs(have.d - now.d) < 0.01 && std::fabs(have.n[0] - now.n[0]) < 1e-4 && std::fabs(have.n[1] - now.n[1]) < 1e-4
						&& std::fabs(have.n[2] - now.n[2]) < 1e-4)
					{
						duplicate = true;
						break;
					}
				}
				if (!duplicate)
				{
					sides.push_back(now);
				}
			}
			if (sides.size() >= 4)
			{
				order[owner[i]].push_back(static_cast<int>(i));
			}
		}
		return order;
	}

	namespace
	{
		std::string placement_key(const edit::entity& e)
		{
			std::string key(e.value("model"));
			for (const char* name : { "origin", "angles", "modelscale" })
			{
				key += '|';
				key += e.value(name);
			}
			return key;
		}

		model_placement placement_of(const edit::entity& e)
		{
			model_placement p;
			p.model = std::string(e.value("model"));
			p.origin = e.origin();
			p.angles = e.angles();
			edit::vec3 scale;
			if (const std::string* text = e.find("modelscale"); text && edit::parse_vec3(*text, scale) && scale.x > 0)
			{
				p.scale = scale.x;
			}
			return p;
		}

		bool is_static_model(const edit::entity& e)
		{
			return e.classname() == "misc_model" && e.find("codol_static") != nullptr;
		}

		// exact contents of a brush: plane points and material of every face
		std::string brush_key(const edit::brush& b)
		{
			std::string key;
			char text[256];
			for (const auto& f : b.faces)
			{
				std::snprintf(text, sizeof(text), "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g ", f.points[0].x, f.points[0].y, f.points[0].z, f.points[1].x, f.points[1].y,
					f.points[1].z, f.points[2].x, f.points[2].y, f.points[2].z);
				key += text;
				key += f.texture.material;
				key += ';';
			}
			return key;
		}

		// sub model of an entity: 0 worldspawn, N for "model" "*N", -1 otherwise
		int sub_model_of(const edit::entity& e)
		{
			if (e.is_worldspawn())
			{
				return 0;
			}
			const std::string_view model = e.value("model");
			if (model.size() >= 2 && model[0] == '*')
			{
				const int n = std::atoi(std::string(model.substr(1)).c_str());
				return n > 0 ? n : -1;
			}
			return -1;
		}

		struct brush_lists
		{
			std::map<int, std::vector<edit::object_id>> by_model;		// brushes per sub model, document order
			std::vector<std::string> other;								// what has no place in the zone
		};

		brush_lists brushes_of(const edit::document& doc)
		{
			brush_lists out;
			bool first = true;
			for (const edit::object_id id : doc.entities())
			{
				const edit::entity* e = doc.find_entity(id);
				if (!e)
				{
					continue;
				}
				// the first entity is the world whatever its keys say
				const int sub_model = first ? 0 : sub_model_of(*e);
				first = false;
				int patches = 0, brushes = 0, opaque = 0;
				for (const edit::object_id child : e->children)
				{
					if (const edit::brush* b = doc.find_brush(child))
					{
						if (b->opaque)
						{
							opaque++;
						}
						else if (sub_model >= 0)
						{
							out.by_model[sub_model].push_back(child);
						}
						else
						{
							brushes++;
						}
					}
					else if (doc.find_patch(child))
					{
						patches++;
					}
				}
				const std::string who = "entity '" + std::string(e->classname()) + "'";
				if (brushes)
				{
					out.other.push_back(format("%s has %d brushes but no compiled brush model ( \"model\" \"*N\" ): a new brush model cannot be compiled, the brushes are left out",
						who.c_str(), brushes));
				}
				if (patches)
				{
					out.other.push_back(format("%s has %d patches: terrain and curve patches are not compiled", who.c_str(), patches));
				}
				if (opaque)
				{
					out.other.push_back(format("%s has %d blocks the editor could not read as brushes ( physics_box .. ): left out", who.c_str(), opaque));
				}
			}
			return out;
		}
	}

	bool diff_documents(const Zone& zone, const edit::document& baseline, const edit::document& edited, map_diff& out, std::string& error)
	{
		out = {};
		zone::GfxWorldView gfx;
		zone::ClipMapView clip;
		const zaddr gfx_address = zone::first_asset(zone, c1::ASSET_TYPE_GFXWORLD);
		zaddr clip_address = zone::first_asset(zone, c1::ASSET_TYPE_CLIPMAP_MP);
		if (!clip_address)
		{
			clip_address = zone::first_asset(zone, c1::ASSET_TYPE_CLIPMAP_SP);
		}
		if (!gfx_address || !zone::read_gfx_world(zone, gfx_address, gfx) || !clip_address || !zone::read_clip_map(zone, clip_address, clip))
		{
			error = "the zone has no gfx_map / col_map ( not a map zone )";
			return false;
		}

		// ---- static models
		std::vector<int> zone_models;		// zone index of the n-th static model of the baseline
		for (std::size_t i = 0; i < gfx.staticModels.size(); i++)
		{
			if (!gfx.staticModels[i].modelName.empty())
			{
				zone_models.push_back(static_cast<int>(i));
			}
		}
		struct placed
		{
			const edit::entity* entity;
			std::string key;
			int zone_index = -1;
			bool matched = false;
		};
		const auto statics_of = [](const edit::document& doc)
		{
			std::vector<placed> list;
			for (const edit::object_id id : doc.entities())
			{
				const edit::entity* e = doc.find_entity(id);
				if (e && is_static_model(*e))
				{
					list.push_back({ e, placement_key(*e) });
				}
			}
			return list;
		};
		std::vector<placed> base = statics_of(baseline), now = statics_of(edited);
		if (base.size() != zone_models.size())
		{
			error = format("the baseline has %zu static models, the zone %zu: the baseline is not the import of this zone", base.size(), zone_models.size());
			return false;
		}
		{
			std::unordered_map<std::string, std::vector<std::size_t>> by_key;
			for (std::size_t i = 0; i < base.size(); i++)
			{
				base[i].zone_index = zone_models[i];
				by_key[base[i].key].push_back(i);
			}
			for (auto& [key, list] : by_key)
			{
				std::reverse(list.begin(), list.end());		// pop_back gives them in order
			}
			for (placed& p : now)
			{
				const auto it = by_key.find(p.key);
				if (it != by_key.end() && !it->second.empty())
				{
					base[it->second.back()].matched = true;
					it->second.pop_back();
					p.matched = true;
				}
			}
			// what is left: the same model on both sides is a moved model, in order
			std::unordered_map<std::string, std::vector<std::size_t>> left;
			for (std::size_t i = base.size(); i-- > 0;)
			{
				if (!base[i].matched)
				{
					left[std::string(base[i].entity->value("model"))].push_back(i);
				}
			}
			for (placed& p : now)
			{
				if (p.matched)
				{
					continue;
				}
				model_placement to = placement_of(*p.entity);
				if (to.model.empty())
				{
					out.not_compiled.push_back("a static misc_model without a \"model\" key is left out");
					continue;
				}
				const auto it = left.find(to.model);
				if (it != left.end() && !it->second.empty())
				{
					placed& from = base[it->second.back()];
					it->second.pop_back();
					from.matched = true;
					to.angles_changed = from.entity->value("angles") != p.entity->value("angles") || from.entity->value("angle") != p.entity->value("angle");
					to.scale_changed = from.entity->value("modelscale") != p.entity->value("modelscale");
					out.models_moved.push_back({ from.zone_index, std::move(to) });
				}
				else
				{
					out.models_added.push_back(std::move(to));
				}
			}
			for (const placed& p : base)
			{
				if (!p.matched)
				{
					out.models_deleted.push_back(p.zone_index);
				}
			}
		}

		// ---- brushes
		const auto zone_order = imported_brush_order(zone, clip);
		const brush_lists base_brushes = brushes_of(baseline), now_brushes = brushes_of(edited);
		out.not_compiled = now_brushes.other;
		for (const auto& [sub_model, list] : base_brushes.by_model)
		{
			const auto it = zone_order.find(sub_model);
			const std::size_t in_zone = it == zone_order.end() ? 0 : it->second.size();
			if (list.size() != in_zone)
			{
				error = format("the baseline has %zu brushes in sub model %d, the zone %zu: the baseline is not the import of this zone", list.size(), sub_model, in_zone);
				return false;
			}
		}
		std::vector<int> sub_models;
		for (const auto& [sub_model, list] : base_brushes.by_model)
		{
			sub_models.push_back(sub_model);
		}
		for (const auto& [sub_model, list] : now_brushes.by_model)
		{
			if (!base_brushes.by_model.count(sub_model))
			{
				sub_models.push_back(sub_model);
			}
		}
		for (const int sub_model : sub_models)
		{
			static const std::vector<edit::object_id> none;
			const auto b = base_brushes.by_model.find(sub_model);
			const auto n = now_brushes.by_model.find(sub_model);
			const auto& base_list = b == base_brushes.by_model.end() ? none : b->second;
			const auto& now_list = n == now_brushes.by_model.end() ? none : n->second;
			if (sub_model > 0 && static_cast<std::size_t>(sub_model) >= clip.subModels.size())
			{
				out.not_compiled.push_back(format("brush model *%d does not exist in the zone: its brushes are left out", sub_model));
				continue;
			}
			std::unordered_map<std::string, std::vector<std::size_t>> by_key;
			for (std::size_t i = base_list.size(); i-- > 0;)
			{
				by_key[brush_key(*baseline.find_brush(base_list[i]))].push_back(i);
			}
			std::vector<std::uint8_t> matched(base_list.size(), 0);
			std::vector<edit::object_id> changed;
			for (const edit::object_id id : now_list)
			{
				const auto it = by_key.find(brush_key(*edited.find_brush(id)));
				if (it != by_key.end() && !it->second.empty())
				{
					matched[it->second.back()] = 1;
					it->second.pop_back();
				}
				else
				{
					changed.push_back(id);
				}
			}
			// an edited brush takes the place of a brush that is gone, in order; the rest is new / deleted
			const auto& zone_list = zone_order.count(sub_model) ? zone_order.at(sub_model) : std::vector<int>();
			std::size_t next = 0;
			for (std::size_t i = 0; i < base_list.size(); i++)
			{
				if (matched[i])
				{
					continue;
				}
				if (next < changed.size())
				{
					out.brushes.push_back({ changed[next++], sub_model, zone_list[i] });
				}
				else
				{
					out.brushes_deleted.push_back(zone_list[i]);
				}
			}
			for (; next < changed.size(); next++)
			{
				out.brushes.push_back({ changed[next], sub_model, -1 });
			}
		}
		std::sort(out.brushes_deleted.begin(), out.brushes_deleted.end());
		return true;
	}
}
