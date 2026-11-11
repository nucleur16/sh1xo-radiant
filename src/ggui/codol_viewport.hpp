#pragma once

namespace ggui
{
	// The native CoDOL viewport: the map as the game draws it. The render world, models and brush models are read
	// straight from the 1.1.1.13 fastfiles ( codol_zone ) and drawn with the game's own materials, technique sets and
	// compiled shaders ( codol_render + codol_view ) on radiant's Direct3D 9 device. Nothing is converted.
	class codol_viewport_dialog final : public ggui::ggui_module
	{
	public:
		codol_viewport_dialog();
		~codol_viewport_dialog() override;

		bool gui() override;

		// load a map by zone name ( "mp_killhouse_sh" ); blocks for a few seconds
		bool load_map(const std::string& map_name);
		void unload();

		// radiant is about to reset / has reset the device
		static void on_device_lost();
		static void on_device_reset();

	private:
		struct impl;
		std::unique_ptr<impl> m;
	};
}
