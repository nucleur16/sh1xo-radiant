#pragma once

// Geometry editing in the 3D view ( edit3d.cpp ): the tools the left mouse button drives there. The 3D view
// ( viewport.cpp ) hands over the button, asks whether a tool took it, and draws the tool's overlay; everything a
// tool does goes to the document ( scene.hpp ), so the 2D views follow and every drag is one undo step.
//
//   Select    click selects; drag on the selection moves it, drag next to a selected brush moves the faces on that
//             side ( Radiant ), the squares on the selection's box resize it, a selected face is dragged along its
//             normal; Shift drag selects what is in the rectangle; Alt click goes through what is behind
//   Vertices  handles on the corners of the selected brushes and the control points of the selected patches
//   Edges     handles on the edges of the selected brushes
//   Clipper   two or three points on surfaces; the cut is previewed
//   Create    drag a rectangle on the surface under the mouse: a box, cylinder, cone, wedge or terrain patch
//
// Mouse positions are pixels of the 3D view ( its top left corner is 0 0 ). press / drag / release are what the
// mouse calls and what the console command `ed3d` calls: an unattended run goes through the same code.

#include "scene.hpp"

namespace editor::edit3d
{
	enum class tool : int
	{
		object,
		vertex,
		edge,
		clip,
		create,
	};

	enum class primitive : int
	{
		box,
		cylinder,
		cone,
		wedge,
		terrain,
	};

	// the tool that is on ( vertex mode and the clipper are shared with the 2D views: ui::state )
	[[nodiscard]] tool current();
	void set_tool(tool t);

	// The left button went down in the view. True: a tool took the press - the view then neither drives the camera
	// nor selects on release; drag() / release() follow. False: the view does what it always did.
	bool press(float px, float py, bool shift, bool ctrl, bool alt);
	// `force`: also when the mouse has not moved the few pixels that tell a drag from a click ( console )
	void drag(float px, float py, bool force);
	void release();
	// a press is being followed
	[[nodiscard]] bool busy();
	// .. and it came from the console ( ed3d press ): the mouse of the view leaves it alone until `ed3d release`
	[[nodiscard]] bool scripted();
	// takes back what the drag in progress did
	void cancel();

	// ---- once per frame, from the 3D view
	// second row of the view's toolbar
	void toolbar();
	// handles, previews, the rectangle of a selection drag. `origin`: screen position of the view's top left corner.
	void draw(ImDrawList* dl, const ImVec2& origin, int width, int height, bool hovered);
	// vertex, edge and clip tool: the gizmo of the selection is not shown
	[[nodiscard]] bool replaces_gizmo();
	// the move gizmo on the selected handles; true while it has the mouse
	bool handle_gizmo(ImDrawList* dl, const ImVec2& origin, int width, int height);
	// two lines for the bottom of the view: what the mouse and the keys do in the tool that is on
	[[nodiscard]] const char* hint();

	// keys of the tools while the mouse is over the 3D view ( from ui::process_shortcuts ); true: the key was used
	// the keys of the tools as actions of the registry ( hotkeys.cpp )
	void register_actions();

	// a material dropped on the view: on the face under the pixel ( `whole_brush`: every face of that brush ), or on
	// the patch there
	bool drop_material(const std::string& material, float px, float py, bool whole_brush);

	// console: ed3d ..
	void register_commands();
}
