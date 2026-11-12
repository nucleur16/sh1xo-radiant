#pragma once

// The Preview window: an asset before it is used.
//
//   model      a model of the game or of the project, drawn by the renderer of the 3D view ( map_renderer::add_model )
//              with an orbit camera
//   material   the colour map, and a cube drawn with the material's own technique set ( map_renderer::create_mesh )
//   image      a source image of the project ( PNG / TGA / DDS, decoded by codol_assets )
//   animation  an xanim on its model: the compiled XAnimParts and XModel are read back from the project's zone
//              ( codol_zone, codol_assets read_raw_anim / decode_anim / extract_model_lod ) - what the game gets -,
//              or, before the first build, the source files; the mesh is skinned here and drawn with the model's
//              materials. Timeline: play / pause, frame, speed, loop, note tracks.
//   effect     the particles of an effect at a moment, with a timeline: the .efx of the project, or the FxEffectDef
//              of a zone turned back into curves ( codol_assets read_fx / extract_fx ), simulated by codol_assets
//              simulate_fx and drawn as textured quads. An approximation, see fx_preview.hpp for what is left out.
//   sound      the aliases of a soundaliases .csv of the project: the .wav of a row is played ( WinMM ), an .mp3
//              through MCI; an alias of a zone: its LoadedSound is read from the zone and decoded ( IMA ADPCM )
//
// The pictures are drawn into a render target of their own by the renderer of the map that is open; the objects
// they show are hidden in the 3D view.

#include <filesystem>
#include <string>

namespace editor::preview
{
	void show_model(const std::string& name);
	void show_material(const std::string& name);
	void show_image(const std::filesystem::path& file);
	// an animation of the project; `model`: another model than the one its .anim names
	void show_anim(const std::string& name, const std::string& model = {});
	// a soundaliases .csv of the project ( its aliases are listed )
	void show_sounds(const std::filesystem::path& csv);
	// An effect over time: its sprites, tails and lines as the editor simulates them ( codol_assets simulate_fx ) -
	// not the game's effect system. `zone` empty: fx\<name>.efx of the project, else the effect out of the zone of
	// the map or a common zone; a zone name of the install or a .ff: the compiled effect of that zone.
	void show_fx(const std::string& name, const std::string& zone = {});
	// plays an alias: of the project's csv files, else of a zone ( `zone`: a .ff file or a zone name of the install;
	// empty: the project's zone )
	bool play_alias(const std::string& alias, const std::string& zone, std::string& message);
	void stop_sound();
	void close();

	// the zones of the renderer changed ( map opened / closed, the project was built ): what is shown is made again
	void renderer_changed();
	// .. or the zones it takes assets from ( the project was built ): the objects are made again from them
	void assets_changed();
	void device_lost();

	void frame();				// draws the window
	void register_commands();
}
