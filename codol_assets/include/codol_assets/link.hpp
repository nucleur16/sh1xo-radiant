#pragma once

// Linking compiled assets into a zone: the job of linker_pc.
//
// How shipped zones refer to assets, and what the linker therefore does ( established with codol_asset_probe graph ):
//
//   * An asset pointer inside an asset is either 0xFFFFFFFF / 0xFFFFFFFE followed by the asset itself ( nested ), or
//     an alias: to the header slot of a top level asset in the zone's asset table, or to the pointer field through
//     which the asset was nested the first time. The linker makes every new asset a top level asset and points at
//     its slot.
//   * An asset of another zone is a nested header that holds nothing but a name starting with ',': ui_mp has 2434
//     materials and no technique set; its first material nests a MaterialTechniqueSet named ",2d". The linker
//     writes such a reference for a technique set that an always loaded zone ( code_post_gfx_mp, common_mp .. )
//     defines, and for images that are not part of the zone.
//   * Technique sets that only map zones define ( all wc_* ) cannot be referenced: which map is loaded is not
//     known. Every map zone carries its own copies, and so does the output: the technique set is copied from the
//     smallest zone that has it, with the technique sets it shares techniques with ( ZoneEditor::add_asset_closure ).
//     Vertex and pixel shaders stay what they are in the donor - mostly references to the common zones.
//   * $identitynormalmap, $white, $black, $gray: map zones carry their own 1 x 1 copies; so does the output.
//   * Order: an asset comes after everything it points at ( images, technique sets, materials, models ).

#include "codol_assets/anim.hpp"
#include "codol_assets/image.hpp"
#include "codol_assets/material.hpp"
#include "codol_assets/model.hpp"
#include "codol_assets/sound.hpp"

namespace codol::assets
{
	struct LinkOptions
	{
		std::filesystem::path install;			// the 1.1.1.13 install ( read only )
		std::filesystem::path exe;				// C1.exe; empty: <install>/C1.exe
		std::filesystem::path libraryCache;		// material library file; made when missing. Empty: scan every time
		std::filesystem::path base;				// existing zone to add to; empty: a new zone
		std::string zoneName = "codol_assets";
		bool copyAllTechsets = false;			// never reference a technique set by name: the zone draws on its own
	};

	class AssetLinker
	{
	public:
		AssetLinker();
		~AssetLinker();

		Status begin(const LinkOptions& options);
		const MaterialLibrary& library() const;
		zone::ZoneEditor& editor();
		const zone::ClientKey& key() const;

		// replaces nothing: fails when the zone has an asset of that type and name
		Status add_image(const CompiledImage& image);
		Status add_material(const CompiledMaterial& material);
		Status add_model(const CompiledModel& model);
		Status add_rawfile(std::string_view name, std::span<const std::uint8_t> contents);
		Status add_anim(const CompiledAnim& anim);			// link_anim.cpp

		// sounds ( sound.hpp ): loaded sounds and curves first, then the alias lists that name them
		Status add_loaded_sound(const CompiledLoadedSound& sound);
		Status add_snd_curve(const SndCurveDef& curve);
		Status add_sound_aliases(const CompiledAliasList& list);

		// what was assumed and done, one line each
		const std::vector<std::string>& log() const;

		// the inflated zone / the fast file ( IWffu100 )
		Status write_zone(std::vector<std::uint8_t>& zoneBytes);
		Status write(const std::filesystem::path& fastfile);

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
