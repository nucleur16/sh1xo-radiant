#pragma once

// Services of the standalone editor's host ( editor/src/main.cpp )

#include <filesystem>
#include <string>

namespace editor
{
	// a path as utf-8 text
	inline std::string to_utf8(const std::filesystem::path& path)
	{
		const auto text = path.u8string();
		return std::string(text.begin(), text.end());
	}

	// a line for the console window; "^1" at the start marks an error. Callable from any thread.
	void log(const std::string& text);

	// the frame that is on the back buffer right now, as a PNG ( development aid: CODOL_AUTOSHOT )
	bool save_backbuffer_png(const std::string& path);

	HWND main_window();

	// ask the host to close ( File > Exit )
	void request_exit();
}
