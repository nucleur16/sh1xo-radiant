#pragma once

// codol_edit: the objects of a map document.
//
// A map is a list of entities ( the worldspawn first ). An entity owns key / value pairs and any number of
// children: brushes and patches. Every entity, brush and patch has an object_id that is unique in its document,
// is never reused and survives undo / redo.
//
// Objects are immutable from the outside: document hands out const pointers and replaces the whole object when
// it is edited ( copy on write ). A pointer therefore stays valid, and unchanged, until the object is edited,
// deleted or the edit is undone - do not keep pointers across calls that modify the document; keep ids.
// `revision` changes with every edit of the object ( and goes back to the old value on undo ), so
// ( id, revision ) identifies the exact contents: cache GPU meshes under that key.

#include "codol_edit/math.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace codol::edit
{
	using object_id = std::uint32_t;
	inline constexpr object_id no_object = 0;

	enum class object_kind : std::uint8_t
	{
		none,
		entity,
		brush,
		patch,
	};

	// A piece of the file an object was read from. While an object is unedited it is written back as exactly this
	// text. Internal to the reader / writer; the UI has no use for it.
	struct source_text
	{
		std::shared_ptr<const std::string> buffer;
		std::size_t offset = 0;
		std::size_t length = 0;

		[[nodiscard]] bool empty() const { return !buffer || length == 0; }
		[[nodiscard]] std::string_view view() const
		{
			return buffer ? std::string_view(*buffer).substr(offset, length) : std::string_view();
		}
	};

	// "<material> <a> <b> <c> <d> <e> <f>" of a face line.
	//   size_x / size_y  world units one repeat of the texture covers ( texture size * scale; radiant writes 64 for a
	//                    256 texel texture at scale 0.25 ). The lightmap uses 16384.
	//   shift_x / _y     offset in texels
	//   rotation         degrees
	//   extra            sixth number, always 0 in the files seen; preserved
	struct tex_def
	{
		std::string material;
		double size_x = 64.0;
		double size_y = 64.0;
		double shift_x = 0.0;
		double shift_y = 0.0;
		double rotation = 0.0;
		double extra = 0.0;

		bool operator==(const tex_def&) const = default;
	};

	struct face
	{
		vec3 points[3];			// as written in the file; cross( p2 - p0, p1 - p0 ) points out of the brush
		tex_def texture;
		tex_def lightmap;		// "lightmap_gray 16384 16384 0 0 0 0"
		std::string trailing;	// whatever follows the lightmap fields on the line ( smoothing .. ), verbatim
	};

	// what brushes and patches have in common
	struct object_header
	{
		object_id id = no_object;
		object_id owner = no_object;			// entity
		std::uint64_t revision = 0;
		std::string layer;						// "" = no layer line ( the object belongs to the layer of its entity / the root )
		std::vector<std::string> contents;		// words of the "contents ..;" line: detail, nonColliding, weaponClip
		std::vector<std::string> tool_flags;	// words of the "toolFlags ..;" line: splitGeo
		std::vector<std::string> extra;			// statements / blocks that are not modelled ( physics_box .. ), verbatim, one per entry

		// reader / writer state
		source_text source_lead;				// comment lines in front of the object ( "// brush 12" )
		source_text source_body;				// "{ .. }" while the object is unedited
		int source_number = -1;					// N of a canonical "// brush N" lead; -1: the lead is written verbatim ( or generated for a new object )

		[[nodiscard]] bool has_content(std::string_view word) const;
		[[nodiscard]] bool is_detail() const { return has_content("detail"); }
	};

	struct brush : object_header
	{
		std::vector<face> faces;
		// The block has no faces the editor understands ( it could not be parsed, or it only holds a physics_box /
		// physics_cylinder block ): it is kept and written back verbatim and no operation touches it.
		bool opaque = false;
	};

	struct patch_vertex
	{
		vec3 xyz;
		bool has_color = false;
		std::uint8_t color[4] = { 255, 255, 255, 255 };	// "c r g b a"
		std::vector<double> tex;						// numbers after "t": s t [ lightmap s t [ .. ] ]
		bool has_flag = false;
		int flag = 0;									// "f N" ( turned edge of a terrain quad )

		[[nodiscard]] double s() const { return tex.size() > 0 ? tex[0] : 0.0; }
		[[nodiscard]] double t() const { return tex.size() > 1 ? tex[1] : 0.0; }
	};

	enum class patch_type : std::uint8_t
	{
		mesh,		// terrain: the control points are the vertices
		curve,		// quadratic bezier patch, 3x3 control points per segment
	};

	struct patch : object_header
	{
		patch_type type = patch_type::mesh;
		std::string material;
		std::string lightmap;				// second material line, "lightmap_gray"
		std::string smoothing;				// "smoothing <word>" line, "" when absent
		int width = 0;						// number of "( .. )" groups
		int height = 0;						// vertices per group
		double header_scale = 16.0;			// third number of the "W H 16 8" line ( meaning not verified )
		int header_int = 8;					// fourth number
		bool has_header_scale = true;
		bool has_header_int = true;
		std::vector<patch_vertex> vertices;	// width * height, vertices[ column * height + row ]

		[[nodiscard]] const patch_vertex& at(const int column, const int row) const { return vertices[static_cast<std::size_t>(column) * height + row]; }
		[[nodiscard]] patch_vertex& at(const int column, const int row) { return vertices[static_cast<std::size_t>(column) * height + row]; }
	};

	struct entity
	{
		object_id id = no_object;
		std::uint64_t revision = 0;
		std::vector<std::pair<std::string, std::string>> pairs;	// in file order
		std::vector<object_id> children;							// brushes and patches, in file order
		std::string layer;											// `layer "x"` line of the entity, "" when absent
		std::vector<std::string> extra;								// unknown statements between the keys, verbatim

		// reader / writer state
		source_text source_lead;		// "// entity 3"
		source_text source_head;		// "{" and the key lines while the keys are unedited
		source_text source_tail;		// "}"
		int source_number = -1;
		bool source_late_pairs = false;	// the file had keys after the first brush: leads / tail are rewritten once the keys are edited

		[[nodiscard]] const std::string* find(std::string_view key) const;
		[[nodiscard]] std::string_view value(std::string_view key, std::string_view fallback = {}) const;
		[[nodiscard]] std::string_view classname() const { return value("classname"); }
		[[nodiscard]] bool is_worldspawn() const { return classname() == "worldspawn"; }
		[[nodiscard]] bool is_point() const { return children.empty(); }
		// "origin" ( 0 0 0 when absent or malformed )
		[[nodiscard]] vec3 origin() const;
		// "angles" ( pitch yaw roll ), else "angle" as yaw, else 0 0 0
		[[nodiscard]] vec3 angles() const;
	};

	// One line of the layer table at the top of the file: "<name>" flags [ words ]. Layers form a tree through '/'
	// in their names ( "000_Global/codol_tools" is a child of "000_Global" ).
	struct layer
	{
		std::string name;
		bool active = false;		// the layer new objects go to
		std::string flags;			// text after `flags` as read ( " active", "" .. ), regenerated when `active` changes
		bool hidden = false;		// view state, not saved, not part of undo
	};

	struct face_ref
	{
		object_id brush = no_object;
		int face = -1;

		bool operator==(const face_ref&) const = default;
	};

	// "x y z" with up to three numbers; false when the text holds none
	bool parse_vec3(std::string_view text, vec3& out);
	// number as radiant writes it into a .map: printf "%.8g" of its C runtime ( three digit exponents: 1.3e-005 )
	std::string format_number(double value);
	// "x y z" for an entity key: at most 6 decimals, no trailing zeros, no "-0"
	std::string format_key_vec3(const vec3& v);
	std::string format_key_number(double value);
}
