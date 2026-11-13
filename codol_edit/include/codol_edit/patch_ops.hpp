#pragma once

// codol_edit: brush primitives and patch ( curve / terrain ) operations - what the Brush and Patch menus of
// radiant do, on the document's own model. Free functions over a document; each is one undo step ( or part of the
// transaction the caller has open ) and returns false / no_object with document::last_error() when there was
// nothing to do. Unless a patch id is given they work on the selected patches.
//
// A patch is a grid of `width` columns x `height` rows, vertices[ column * height + row ]:
//   mesh    terrain: the control points are the vertices, one quad per cell; "f 1" on the first vertex of a quad
//           turns its diagonal
//   curve   quadratic bezier: odd width and height, every second control point lies on the surface
// Texture coordinates of a vertex are texels ( s t ) and luxels ( lightmap s t ).
//
// Rows / columns: "column" operations change `width`, "row" operations `height`.

#include "codol_edit/document.hpp"

namespace codol::edit
{
	inline constexpr int PATCH_MAX_LINES = 31;		// columns / rows a patch may have after an operation

	// quads per bezier segment a curve is drawn and compiled with: half of the fourth number of its size line
	// ( "3 3 16 8" -> 4 ), 1 .. 16. Inc / Dec Subdivision change that number. 1 for a terrain mesh.
	[[nodiscard]] int patch_subdivisions(const patch& p);

	// ---- brush primitives -----------------------------------------------------------------------
	// New brushes go to the worldspawn and become the selection, like document::create_box.

	enum class brush_shape
	{
		box,
		wedge,		// box cut along the diagonal
		prism,		// "arbitrary sided cylinder": `sides` faces around `axis`, touching the box like radiant's do
		cone,		// the same base, tapering to a point at the maxs end of `axis`
		sphere,		// `sides` slices around `axis`, sides / 2 bands
	};

	// sides: prism / cone 3 .. 64, sphere 4 .. 16
	object_id create_brush_shape(document& doc, brush_shape shape, const vec3& mins, const vec3& maxs, int sides = 8, int axis = 2,
		std::string_view material = {});
	// The selected brushes become one brush of that shape filling their bounds ( what radiant's Brush menu does ):
	// the first one keeps its id, entity, layer and contents and gets the new faces with the material of its first
	// face; the others are deleted.
	object_id brush_shape_from_selection(document& doc, brush_shape shape, int sides = 8, int axis = 2);

	// ---- creating patches -----------------------------------------------------------------------

	// flat curve patch in the plane perpendicular to `axis`; columns / rows odd, 3 .. 31
	object_id create_curve_patch(document& doc, const vec3& mins, const vec3& maxs, int columns, int rows, int axis = 2,
		std::string_view material = {});

	enum class patch_shape
	{
		bevel,					// quarter round, 3 x 3
		square_bevel,			// the same corner, sharp, 5 x 3
		endcap,					// half round, 5 x 3
		square_endcap,			// 7 x 3
		cylinder,				// 9 x 3
		dense_cylinder,			// 9 x 5
		very_dense_cylinder,	// 9 x 9
		square_cylinder,		// 9 x 3, sharp corners
		cone,					// 9 x 3, the last row is the tip
	};

	// curve patch filling the box, its rows along `axis`
	object_id create_patch_shape(document& doc, patch_shape shape, const vec3& mins, const vec3& maxs, int axis = 2,
		std::string_view material = {});

	// every selected curve becomes a terrain mesh with the vertices of its tessellation
	bool curve_to_terrain(document& doc);
	// One terrain mesh per face: of the selected faces, else of every face of the selected brushes. Faces with 3 or 4
	// corners only; the texture alignment of the face is kept. The new patches become the selection.
	bool faces_to_terrain(document& doc, bool delete_brushes = false);

	// ---- rows and columns -----------------------------------------------------------------------

	enum class patch_line
	{
		column,
		row,
	};

	// Two more lines. Curve: the first / last bezier segment ( or the one that holds control line `index` ) is split
	// in two, the surface stays as it is. Terrain: the first / last cell ( or the one after `index` ) is cut in three.
	bool patch_insert(document& doc, patch_line line, bool at_end, int index = -1);
	// terrain: one more line halfway between line `after` and the next ( -1: in the last cell )
	bool patch_insert_terrain(document& doc, patch_line line, int after = -1);
	// terrain: one more line beyond the first / last one, as far out as the cell before it is wide
	bool patch_extrude_terrain(document& doc, patch_line line, bool at_end = true);
	// removes the first / last two lines
	bool patch_delete(document& doc, patch_line line, bool at_end);
	// terrain: removes one line ( -1: the last )
	bool patch_delete_terrain(document& doc, patch_line line, int index = -1);

	// rows become columns ( the surface then faces the other way )
	bool patch_transpose(document& doc);
	// the surface faces the other way, the shape stays
	bool patch_invert(document& doc);

	enum class patch_disperse
	{
		vertices,	// even spacing along the lines the control points form now, both ways
		columns,	// the columns evenly on the straight line from the first to the last one, per row
		rows,
	};

	bool patch_redisperse(document& doc, patch_disperse mode);

	// two patches: the control line `index` ( -1: the middle ) ends the first and starts the second. Curve: an even
	// index. Both are selected afterwards.
	bool patch_split(document& doc, patch_line line, int index = -1);

	// curves: `steps` > 0 finer, < 0 coarser
	bool patch_change_subdivision(document& doc, int steps);

	// ---- welding --------------------------------------------------------------------------------

	// vertices of the selected patches that are closer than `tolerance` to a vertex of an earlier one ( or an earlier
	// vertex of the same patch ) move onto it
	bool patch_weld(document& doc, double tolerance = 0.1);
	// vertices of the selected patches move onto the corner of a visible brush that is closer than `tolerance`
	bool patch_weld_to_brushes(document& doc, double tolerance = 0.1);
	// document::move_patch_vertices, and every vertex of the selected patches ( and of `id` ) that sits where one of
	// the moved vertices was ( within `weld_tolerance`; < 0: none ) moves along. `locked` vertices of `id` stay.
	bool move_patch_vertices_welded(document& doc, object_id id, std::span<const int> vertices, const vec3& delta,
		double weld_tolerance, std::span<const int> locked = {});

	// ---- thicken / cap --------------------------------------------------------------------------

	// A second surface `thickness` units behind each selected patch, facing the other way ( negative: in front,
	// facing the same way ). seams: four ( two for a closed tube ) strips that close the gap along the edges.
	// The new patches are added to the selection.
	bool patch_thicken(document& doc, double thickness, bool seams);

	enum class patch_cap_type
	{
		automatic,			// cylinder for a closed outline, else bevel ( 3 wide ) / endcap
		bevel,				// between the outline and the straight line from its first to its last point
		endcap,				// the same for a 5 wide half round
		inverted_bevel,		// between the outline and its corners
		inverted_endcap,
		cylinder,			// a closed outline to its centre
	};

	// caps for the open ends ( first and last row ) of the selected curves, facing away from the patch. The caps are
	// added to the selection.
	bool patch_cap(document& doc, patch_cap_type type = patch_cap_type::automatic, bool first_end = true, bool last_end = true);

	// ---- terrain edges --------------------------------------------------------------------------

	// turns the diagonal of every quad of the selected terrain meshes
	bool patch_turn_edges(document& doc);
	bool patch_turn_edge(document& doc, object_id id, int column, int row);
	// the quad of a terrain mesh a ray hits
	bool patch_pick_quad(const document& doc, object_id id, const ray& r, int& column, int& row, double& distance);

	// ---- textures -------------------------------------------------------------------------------

	// flat projection along a world axis ( -1: the one the patch faces most ), 4 texels per unit
	bool patch_cap_texture(document& doc, int axis = -1);
	// the next projection axis after the one the texture is projected along now
	bool patch_cycle_cap_texture(document& doc);
	// follows the surface: texels by the distance along the columns / rows; `scale` world units per texel
	bool patch_natural_texture(document& doc, double scale = 0.25);
	// the texture repeats `repeat_x` x `repeat_y` times over the patch
	bool patch_fit_texture(document& doc, double repeat_x = 1.0, double repeat_y = 1.0);

	// ---- vertices -------------------------------------------------------------------------------
	// `vertices`: indices into patch::vertices; empty: all of them.

	bool patch_set_color(document& doc, object_id id, std::span<const int> vertices, const std::uint8_t rgba[4]);
	// random offset of up to +-scale per axis
	bool patch_add_noise(document& doc, object_id id, std::span<const int> vertices, const vec3& scale, std::uint32_t seed);
	bool patch_set_vertex(document& doc, object_id id, int index, const patch_vertex& v);
	// anything else: `fn` gets a copy of the patch to change; it is stored when fn returns true and the grid is
	// consistent ( width * height vertices )
	bool edit_patch(document& doc, object_id id, std::string_view undo_name, const std::function<bool(patch&)>& fn);
}
