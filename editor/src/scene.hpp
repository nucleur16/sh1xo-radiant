#pragma once

// The open map as the editor sees it: the native renderer with its objects, the entities of the map, the camera
// and the selection. One scene per editor.

#include <codol_view/map_renderer.hpp>

namespace editor
{
	namespace view = codol::view;
	using float3 = codol::view::float3;

	struct scene
	{
		view::map_renderer renderer;
		std::string map_name;
		double load_seconds = 0.0;

		// the entity string of the zone, parsed ( index = entity index of the renderer's objects )
		codol::mapents::entity_set entities;

		view::camera cam;
		int spawn_index = 0;

		std::vector<view::object_ref> selection;	// the last one is the primary
		bool outline_dirty = true;

		// counts every change of the scene ( placement, added / removed things )
		std::uint64_t revision = 0;

		bool load(const std::string& map, std::string& error);

		[[nodiscard]] bool is_selected(const view::object_ref& ref) const;
		void select(const view::object_ref& ref, bool add);
		void deselect();
		void hide_selection();

		[[nodiscard]] const codol::mapents::entity* entity_of(int index) const;
	};

	scene& get_scene();
}
