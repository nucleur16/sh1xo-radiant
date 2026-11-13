#pragma once

// Cross zone registry of the render assets a map needs: shaders, vertex declarations, technique sets, materials
// and the zone a model is defined in, all by name. Port of Codol.Assets\Rendering\RenderAssetLibrary.cs
// ( AddLoadedZone / Freeze / Find* ) and RenderLayouts.cs:85-121 ( SharedZones, LevelZones, Load*Library ).
//
// A map zone refers to most of its materials, technique sets, shaders and models by name ( ",name" ): they are
// defined in shared zones ( common_mp, code_post_gfx_mp, .. ) and level zones ( common_gametype_mp, patch zones ).
// Like the C# library this one loads such a zone once, keeps what the draw loop needs of it in compact form
// ( `library_zone`: "frozen" ) and releases the zone memory again, so no more than the map zone and one other zone
// are in memory at a time. Model geometry and inline image pixels of a frozen zone are read by loading that zone
// again on demand ( RenderAssetLibrary.FindModelFile ).

#include "codol_render/material.hpp"
#include "codol_render/technique_set.hpp"
#include "codol_zone/assets.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace codol::view
{
	using zone::zaddr;

	// codol_render reads zone memory through this adapter. The zone must outlive the view.
	render::zone_view make_zone_view(const zone::Zone& zone);

	struct name_hash
	{
		std::size_t operator()(const std::string& s) const noexcept;
	};
	struct name_equal
	{
		bool operator()(const std::string& a, const std::string& b) const noexcept;
	};
	template <typename T>
	using name_map = std::unordered_map<std::string, T, name_hash, name_equal>;		// StringComparer.OrdinalIgnoreCase
	using name_set = std::unordered_set<std::string, name_hash, name_equal>;

	struct shader_blob
	{
		std::string name;
		bool pixel = false;
		std::vector<std::uint8_t> code;			// the D3D9 byte code as stored in the zone
	};

	// the programs of one pass that the techset's own zone carries; null: stored as a ",name" reference
	struct library_pass
	{
		std::shared_ptr<const shader_blob> vertex_shader;
		std::shared_ptr<const shader_blob> pixel_shader;
	};

	struct library_techset
	{
		render::render_technique_set set;
		std::vector<std::vector<library_pass>> passes;	// [ index into set.techniques ][ pass ]
	};

	struct library_material
	{
		render::material asset;
		std::string zone_name;								// the zone that defines it: inline image pixels are read from it
		std::shared_ptr<const library_techset> techset;		// set when the same zone defines the techset, else by name
	};

	// What stays of a zone after its memory is released ( RenderAssetLibrary.Freeze )
	struct library_zone
	{
		std::string name;
		name_map<std::shared_ptr<const shader_blob>> vertex_shaders;
		name_map<std::shared_ptr<const shader_blob>> pixel_shaders;
		name_map<render::vertex_declaration> declarations;
		name_map<std::shared_ptr<const library_techset>> techsets;
		name_map<std::shared_ptr<const library_material>> materials;
		name_map<std::string> models;						// XModels defined here ( not ",name" references ): the name as stored
		std::size_t shader_bytes = 0;
	};

	// RenderLayouts.cs:85-87
	const std::vector<std::string>& shared_zone_names();
	// RenderLayouts.cs:89-96
	std::vector<std::string> level_zone_names(const std::string& map_name);

	// The zones a lookup goes through, first hit wins: the map zone, its level zones, the shared zones
	// ( RenderAssetLibrary layers: each layer asks its parent when it has no entry )
	struct library_scope
	{
		std::vector<std::shared_ptr<const library_zone>> zones;

		std::shared_ptr<const library_material> find_material(const std::string& name) const;
		std::shared_ptr<const library_techset> find_techset(const std::string& name) const;
		std::shared_ptr<const shader_blob> find_shader(const std::string& name, bool pixel) const;
		const render::vertex_declaration* find_declaration(const std::string& name) const;
		// name of the zone that defines the model; empty when none does
		std::string find_model_zone(const std::string& name, std::string* stored_name = nullptr) const;
	};

	class zone_library
	{
	public:
		// `zone_directories`: where <name>.ff is looked up, in order
		bool init(const std::filesystem::path& client_exe, std::vector<std::filesystem::path> zone_directories, std::string* error);

		std::filesystem::path find_zone_file(const std::string& name) const;	// empty: no such zone

		// loads the zone; nothing is cached
		std::shared_ptr<const zone::Zone> load_zone(const std::string& name, std::string* error) const;

		// The frozen form of a zone; loaded on first use and kept until clear(). nullptr when the zone does not
		// exist or cannot be loaded ( with a text in *error when it exists ).
		std::shared_ptr<const library_zone> frozen(const std::string& name, std::string* error);

		// A zone outside the zone directories: <name>.ff is `file` from now on ( an empty path takes that back ).
		// What is cached of the zone is dropped, so the next frozen() reads the file as it is then.
		void set_zone_file(const std::string& name, const std::filesystem::path& file);

		// RenderAssetLibrary.AddLoadedZone + Freeze for a zone that is already in memory ( not cached )
		static std::shared_ptr<library_zone> freeze(const zone::Zone& zone, std::vector<std::string>* warnings = nullptr);

		void clear();
		std::size_t cached_zone_count() const { return frozen_.size(); }

	private:
		zone::ClientKey key_;
		std::vector<std::filesystem::path> directories_;
		name_map<std::filesystem::path> files_;			// set_zone_file
		name_map<std::shared_ptr<const library_zone>> frozen_;
	};
}
