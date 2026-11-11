#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include "host.hpp"
#include "png_wic.hpp"

// CoDOL Radiant: the standalone host. A plain Win32 window with a Direct3D 9 device and a docking ImGui on it.
// No CoD4 radiant, no hooks: everything shown comes from the 1.1.1.13 install through codol_zone / codol_render /
// codol_view, and from the editor's own code.

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace editor
{
	namespace
	{
		HWND g_hwnd = nullptr;
		IDirect3D9* g_d3d = nullptr;
		IDirect3DDevice9* g_device = nullptr;
		D3DPRESENT_PARAMETERS g_pp = {};
		bool g_reset_pending = false;
		bool g_device_lost = false;
		bool g_exit = false;
		bool g_minimized = false;

		struct log_line
		{
			bool error;
			std::string text;
		};

		std::mutex g_log_mutex;
		std::vector<log_line> g_log;
		bool g_log_scroll = false;

		bool g_show_console = true;
		bool g_show_demo = false;
		bool g_show_about = false;
		bool g_show_settings = false;
		std::string g_console_input;

		// ---------------------------------------------------------------------------------------------

		void reset_device()
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			ggui::codol_viewport_dialog::on_device_lost();

			const HRESULT hr = g_device->Reset(&g_pp);
			if (FAILED(hr))
			{
				g_device_lost = true;
				return;
			}

			g_device_lost = false;
			ggui::codol_viewport_dialog::on_device_reset();
			ImGui_ImplDX9_CreateDeviceObjects();
		}

		bool create_device()
		{
			g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
			if (!g_d3d)
			{
				return false;
			}

			g_pp = {};
			g_pp.Windowed = TRUE;
			g_pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
			g_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
			g_pp.EnableAutoDepthStencil = TRUE;
			g_pp.AutoDepthStencilFormat = D3DFMT_D24S8;
			g_pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
			g_pp.hDeviceWindow = g_hwnd;

			// the game's shaders are vs_3_0 / ps_3_0: hardware vertex processing is required
			const DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED;

			if (FAILED(g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd, flags, &g_pp, &g_device)))
			{
				return false;
			}

			game::glob::d3d9_device = g_device;
			return true;
		}

		LRESULT WINAPI wnd_proc(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam)
		{
			if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
			{
				return 1;
			}

			switch (msg)
			{
			case WM_SIZE:
				g_minimized = wparam == SIZE_MINIMIZED;

				if (g_device && wparam != SIZE_MINIMIZED)
				{
					const UINT w = LOWORD(lparam), h = HIWORD(lparam);
					if (w > 0 && h > 0 && (w != g_pp.BackBufferWidth || h != g_pp.BackBufferHeight))
					{
						g_pp.BackBufferWidth = w;
						g_pp.BackBufferHeight = h;
						g_reset_pending = true;
					}
				}

				return 0;

			case WM_SYSCOMMAND:
				// no ALT application menu: ALT is a modifier in the views
				if ((wparam & 0xfff0) == SC_KEYMENU)
				{
					return 0;
				}

				break;

			case WM_CLOSE:
				g_exit = true;
				return 0;

			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;

			default:
				break;
			}

			return DefWindowProcW(hwnd, msg, wparam, lparam);
		}

		// ---------------------------------------------------------------------------------------------

		void init_fonts()
		{
			using namespace components;
			ImGuiIO& io = ImGui::GetIO();

			// the same set, in the same order, as the radiant plugin ( the shared code picks fonts by index )
			io.Fonts->AddFontFromMemoryCompressedTTF(fonts::opensans_bold_compressed_data, fonts::opensans_bold_compressed_size, 18.0f);
			io.Fonts->AddFontFromMemoryCompressedTTF(fonts::opensans_bold_compressed_data, fonts::opensans_bold_compressed_size, 17.0f);
			io.Fonts->AddFontFromMemoryCompressedTTF(fonts::opensans_regular_compressed_data, fonts::opensans_regular_compressed_size, 12.0f);
			io.Fonts->AddFontFromMemoryCompressedTTF(fonts::opensans_regular_compressed_data, fonts::opensans_regular_compressed_size, 14.0f);
			io.FontDefault = io.Fonts->AddFontFromMemoryCompressedTTF(fonts::opensans_regular_compressed_data, fonts::opensans_regular_compressed_size, 18.0f);

			// monospace font of the script workbench ( index 5 ), with the glyphs of the game's GBK scripts
			if (!GetEnvironmentVariableA("CODOL_NO_MONO_FONT", nullptr, 0))
			{
				char windir[MAX_PATH] = {};
				if (GetWindowsDirectoryA(windir, sizeof(windir)))
				{
					for (const char* name : { "\\Fonts\\consola.ttf", "\\Fonts\\cour.ttf", "\\Fonts\\lucon.ttf" })
					{
						const std::string path = std::string(windir) + name;
						if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES)
						{
							io.Fonts->AddFontFromFileTTF(path.c_str(), 15.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
							break;
						}
					}
				}
			}
		}

		void init_style()
		{
			ImGui::StyleColorsDark();

			auto& style = ImGui::GetStyle();
			style.WindowRounding = 2.0f;
			style.FrameRounding = 2.0f;
			style.GrabRounding = 2.0f;
			style.TabRounding = 2.0f;
			style.PopupRounding = 2.0f;
			style.ScrollbarRounding = 2.0f;
			style.WindowBorderSize = 1.0f;
			style.FramePadding = ImVec2(8.0f, 4.0f);
			style.ItemSpacing = ImVec2(8.0f, 6.0f);
			style.WindowMenuButtonPosition = ImGuiDir_None;

			auto* c = style.Colors;
			c[ImGuiCol_WindowBg] = ImVec4(0.115f, 0.118f, 0.125f, 1.0f);
			c[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			c[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.145f, 0.155f, 0.98f);
			c[ImGuiCol_Border] = ImVec4(0.24f, 0.25f, 0.27f, 1.0f);
			c[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.092f, 0.098f, 1.0f);
			c[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.185f, 0.20f, 1.0f);
			c[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.25f, 0.27f, 1.0f);
			c[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.30f, 0.33f, 1.0f);
			c[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.092f, 0.098f, 1.0f);
			c[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.135f, 0.145f, 1.0f);
			c[ImGuiCol_Tab] = ImVec4(0.13f, 0.135f, 0.145f, 1.0f);
			c[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.42f, 0.36f, 1.0f);
			c[ImGuiCol_TabActive] = ImVec4(0.22f, 0.33f, 0.28f, 1.0f);
			c[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.115f, 0.12f, 1.0f);
			c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.17f, 0.22f, 0.20f, 1.0f);
			c[ImGuiCol_Header] = ImVec4(0.22f, 0.33f, 0.28f, 1.0f);
			c[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.40f, 0.34f, 1.0f);
			c[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.45f, 0.38f, 1.0f);
			c[ImGuiCol_Button] = ImVec4(0.20f, 0.21f, 0.23f, 1.0f);
			c[ImGuiCol_ButtonHovered] = ImVec4(0.27f, 0.40f, 0.34f, 1.0f);
			c[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.45f, 0.38f, 1.0f);
			c[ImGuiCol_CheckMark] = ImVec4(0.48f, 0.80f, 0.62f, 1.0f);
			c[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.66f, 0.52f, 1.0f);
			c[ImGuiCol_SliderGrabActive] = ImVec4(0.48f, 0.80f, 0.62f, 1.0f);
			c[ImGuiCol_Separator] = ImVec4(0.24f, 0.25f, 0.27f, 1.0f);
			c[ImGuiCol_DockingPreview] = ImVec4(0.40f, 0.66f, 0.52f, 0.6f);
			c[ImGuiCol_DockingEmptyBg] = ImVec4(0.08f, 0.082f, 0.086f, 1.0f);
			c[ImGuiCol_TextSelectedBg] = ImVec4(0.30f, 0.50f, 0.42f, 0.6f);
		}

		// ---------------------------------------------------------------------------------------------

		void draw_console()
		{
			if (!g_show_console)
			{
				return;
			}

			ImGui::SetNextWindowSize(ImVec2(900.0f, 240.0f), ImGuiCond_FirstUseEver);

			if (ImGui::Begin("Console##window", &g_show_console, ImGuiWindowFlags_NoCollapse))
			{
				const float footer = ImGui::GetFrameHeightWithSpacing();

				if (ImGui::BeginChild("##console_lines", ImVec2(0.0f, -footer), false, ImGuiWindowFlags_HorizontalScrollbar))
				{
					std::lock_guard lock(g_log_mutex);

					ImGuiListClipper clipper;
					clipper.Begin(static_cast<int>(g_log.size()));

					while (clipper.Step())
					{
						for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
						{
							const auto& line = g_log[static_cast<std::size_t>(i)];

							if (line.error)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.45f, 0.40f, 1.0f));
							}

							ImGui::TextUnformatted(line.text.c_str());

							if (line.error)
							{
								ImGui::PopStyleColor();
							}
						}
					}

					if (g_log_scroll)
					{
						ImGui::SetScrollHereY(1.0f);
						g_log_scroll = false;
					}
				}

				ImGui::EndChild();

				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::InputTextWithHint("##console_input", "command, eg. codol_viewport mp_rust", &g_console_input, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					if (!g_console_input.empty())
					{
						log("> " + g_console_input);
						command::execute(g_console_input);
						g_console_input.clear();
					}

					ImGui::SetKeyboardFocusHere(-1);
				}
			}

			ImGui::End();
		}

		void draw_menu()
		{
			if (!ImGui::BeginMainMenuBar())
			{
				return;
			}

			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open map ..", "Ctrl+O"))
				{
					GET_GUI(ggui::codol_map_ents_dialog)->open();
				}

				if (ImGui::MenuItem("Export entities to the server"))
				{
					GET_GUI(ggui::codol_map_ents_dialog)->export_current_map();
				}

				ImGui::Separator();
				ImGui::MenuItem("Settings ..", nullptr, &g_show_settings);
				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
				{
					g_exit = true;
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				const auto item = [](const char* label, ggui::ggui_module* module_)
				{
					bool open = module_->is_active();
					if (ImGui::MenuItem(label, nullptr, &open))
					{
						module_->toggle(true, open);
					}
				};

				item("Viewport", GET_GUI(ggui::codol_viewport_dialog));
				item("Maps", GET_GUI(ggui::codol_map_ents_dialog));
				item("Script Workbench", GET_GUI(ggui::script_workbench_dialog));
				item("Game", GET_GUI(ggui::game_view_dialog));
				ImGui::MenuItem("Console", nullptr, &g_show_console);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Game"))
			{
				if (ImGui::MenuItem("Reload scripts in the game"))
				{
					components::codol_game::reload_scripts("menu");
				}

				if (ImGui::MenuItem("Game window"))
				{
					GET_GUI(ggui::game_view_dialog)->open();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				ImGui::MenuItem("About", nullptr, &g_show_about);
				ImGui::MenuItem("ImGui demo", nullptr, &g_show_demo);
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		// every saved setting, generically
		void draw_settings()
		{
			if (!g_show_settings)
			{
				return;
			}

			ImGui::SetNextWindowSize(ImVec2(820.0f, 0.0f), ImGuiCond_Appearing);

			if (ImGui::Begin("Settings##window", &g_show_settings, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
			{
				ImGui::TextDisabled("Saved to codol_editor.cfg next to the editor. Hover a name for its meaning.");
				ImGui::Separator();

				for (auto* dvar : dvars::all())
				{
					// the asset tool is only used by the radiant plugin's history; nothing here converts assets
					if (!(dvar->flags & game::dvar_flags::saved) || std::string_view(dvar->name) == "codol_asset_tool")
					{
						continue;
					}

					ImGui::PushID(dvar->name);
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(dvar->name);

					if (ImGui::IsItemHovered() && dvar->description && *dvar->description)
					{
						ImGui::SetTooltip("%s", dvar->description);
					}

					ImGui::SameLine(260.0f);
					ImGui::SetNextItemWidth(-1.0f);

					switch (dvar->type)
					{
					case game::dvar_type::boolean:
					{
						bool value = dvar->current.enabled;
						if (ImGui::Checkbox("##value", &value))
						{
							dvars::set_bool(dvar, value);
						}

						break;
					}

					case game::dvar_type::integer:
					{
						int value = dvar->current.integer;
						if (ImGui::InputInt("##value", &value, 0))
						{
							dvars::set_int(dvar, value);
						}

						break;
					}

					default:
					{
						std::string value = dvar->current.string ? dvar->current.string : "";
						if (ImGui::InputText("##value", &value))
						{
							dvars::set_string(dvar, value.c_str());
						}

						break;
					}
					}

					ImGui::PopID();
				}
			}

			ImGui::End();
		}

		void draw_about()
		{
			if (!g_show_about)
			{
				return;
			}

			ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);

			if (ImGui::Begin("About##window", &g_show_about, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
			{
				ImGui::TextUnformatted("CoDOL Radiant");
				ImGui::Separator();
				ImGui::PushTextWrapPos(0.0f);
				ImGui::TextUnformatted(
					"A map and script editor for Call of Duty Online 1.1.1.13.\n\n"
					"Standalone: maps, models, materials and shaders are read from the game's own fastfiles and drawn with the "
					"game's own shaders. No CoD4 radiant, no mod tools, no converted assets.\n\n"
					"Started as a fork of iw3xo-radiant ( xoxor4d ).");
				ImGui::PopTextWrapPos();
			}

			ImGui::End();
		}

		void first_layout(const ImGuiID dockspace)
		{
			// windows that have never been placed are put into the main dock space
			static bool done = false;
			if (done)
			{
				return;
			}

			done = true;

			if (ImGui::DockBuilderGetNode(dockspace) && ImGui::DockBuilderGetNode(dockspace)->IsSplitNode())
			{
				return; // a saved layout exists
			}

			ImGui::DockBuilderRemoveNode(dockspace);
			ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);

			ImGuiID center = dockspace;
			const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.22f, nullptr, &center);
			const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
			const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.2f, nullptr, &center);

			ImGui::DockBuilderDockWindow("CoDOL Viewport##window", center);
			ImGui::DockBuilderDockWindow("Script Workbench##codol_window", center);
			ImGui::DockBuilderDockWindow("Game##codol_window", center);
			ImGui::DockBuilderDockWindow("Maps##window", left);
			ImGui::DockBuilderDockWindow("Models##window", left);
			ImGui::DockBuilderDockWindow("Materials##window", left);
			ImGui::DockBuilderDockWindow("Selection##window", right);
			ImGui::DockBuilderDockWindow("Console##window", bottom);
			ImGui::DockBuilderFinish(dockspace);
		}

		void frame()
		{
			ImGui_ImplDX9_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			draw_menu();

			const ImGuiID dockspace = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
			first_layout(dockspace);

			if (!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
			{
				GET_GUI(ggui::codol_map_ents_dialog)->open();
			}

			components::codol_game::frame();

			for (const auto& module_ : ggui::loader::get_modules())
			{
				module_->frame();
			}

			draw_console();
			draw_settings();
			draw_about();

			if (g_show_demo)
			{
				ImGui::ShowDemoWindow(&g_show_demo);
			}

			ImGui::RenderToasts();
			ImGui::EndFrame();

			g_device->SetRenderState(D3DRS_ZENABLE, FALSE);
			g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			g_device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
			g_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(20, 21, 22), 1.0f, 0);

			if (SUCCEEDED(g_device->BeginScene()))
			{
				ImGui::Render();
				ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
				components::codol_game::dev_capture_frame();
				g_device->EndScene();
			}

			if (g_device->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST)
			{
				g_device_lost = true;
			}
		}
	}

	// -------------------------------------------------------------------------------------------------

	void log(const std::string& text)
	{
		std::lock_guard lock(g_log_mutex);

		std::size_t start = 0;
		while (start <= text.size())
		{
			auto end = text.find('\n', start);
			if (end == std::string::npos)
			{
				end = text.size();
			}

			if (end > start)
			{
				std::string line = text.substr(start, end - start);
				const bool error = line.rfind("^1", 0) == 0;

				if (error)
				{
					line.erase(0, 2);
				}

				g_log.push_back({ error, std::move(line) });
			}

			start = end + 1;
		}

		if (g_log.size() > 5000)
		{
			g_log.erase(g_log.begin(), g_log.begin() + 1000);
		}

		g_log_scroll = true;
	}

	bool save_backbuffer_png(const std::string& path)
	{
		if (!g_device)
		{
			return false;
		}

		IDirect3DSurface9* target = nullptr;
		if (FAILED(g_device->GetRenderTarget(0, &target)) || !target)
		{
			return false;
		}

		D3DSURFACE_DESC desc = {};
		target->GetDesc(&desc);

		bool ok = false;
		IDirect3DSurface9* copy = nullptr;

		if (SUCCEEDED(g_device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &copy, nullptr)) && copy)
		{
			D3DLOCKED_RECT rect = {};

			if (SUCCEEDED(g_device->GetRenderTargetData(target, copy)) && SUCCEEDED(copy->LockRect(&rect, nullptr, D3DLOCK_READONLY)))
			{
				tools::image img;
				img.width = static_cast<int>(desc.Width);
				img.height = static_cast<int>(desc.Height);
				img.rgba.resize(static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height) * 4);

				for (int y = 0; y < img.height; y++)
				{
					const auto* src = static_cast<const std::uint8_t*>(rect.pBits) + static_cast<std::size_t>(y) * static_cast<std::size_t>(rect.Pitch);
					auto* dst = img.rgba.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width) * 4;

					for (int x = 0; x < img.width; x++)
					{
						dst[x * 4 + 0] = src[x * 4 + 2];
						dst[x * 4 + 1] = src[x * 4 + 1];
						dst[x * 4 + 2] = src[x * 4 + 0];
						dst[x * 4 + 3] = 255;
					}
				}

				copy->UnlockRect();

				std::string error;
				ok = tools::save_png(std::filesystem::u8path(path), img, error);
			}

			copy->Release();
		}

		target->Release();
		return ok;
	}

	HWND main_window()
	{
		return g_hwnd;
	}

	void request_exit()
	{
		g_exit = true;
	}
}

int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, int)
{
	using namespace editor;

	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = wnd_proc;
	wc.hInstance = instance;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
	wc.lpszClassName = L"CoDOLRadiant";
	RegisterClassExW(&wc);

	g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"CoDOL Radiant - Call of Duty Online 1.1.1.13", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1600, 900, nullptr, nullptr, instance, nullptr);

	if (!g_hwnd)
	{
		return 1;
	}

	if (!create_device())
	{
		MessageBoxW(g_hwnd, L"Could not create a Direct3D 9 device with hardware vertex processing.", L"CoDOL Radiant", MB_ICONERROR);
		return 1;
	}

	ShowWindow(g_hwnd, GetEnvironmentVariableA("CODOL_WINDOWED", nullptr, 0) ? SW_SHOWNORMAL : SW_SHOWMAXIMIZED);
	UpdateWindow(g_hwnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	static const std::string ini_path = utils::fs::get_basepath() + "\\codol_editor_layout.ini";

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	io.IniFilename = ini_path.c_str();

	init_fonts();
	init_style();

	ImGui_ImplWin32_Init(g_hwnd);
	ImGui_ImplDX9_Init(g_device);

	// settings, then the game bridge ( rcon, reload, console commands )
	components::codol_game::register_dvars();
	dvars::load_config();

	{
		components::codol_game game_bridge;

		log("CoDOL Radiant - standalone editor for Call of Duty Online 1.1.1.13");

		if (components::codol_game::install_path().empty())
		{
			log("^1The CoDOL install folder is not set: File > Settings ( codol_install_path ).");
			g_show_settings = true;
		}

		GET_GUI(ggui::codol_viewport_dialog)->open();
		GET_GUI(ggui::codol_map_ents_dialog)->open();

		while (!g_exit)
		{
			MSG msg;
			while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);

				if (msg.message == WM_QUIT)
				{
					g_exit = true;
				}
			}

			if (g_exit)
			{
				break;
			}

			if (g_minimized)
			{
				Sleep(50);
				continue;
			}

			if (g_device_lost)
			{
				const HRESULT hr = g_device->TestCooperativeLevel();
				if (hr == D3DERR_DEVICELOST)
				{
					Sleep(50);
					continue;
				}

				if (hr == D3DERR_DEVICENOTRESET)
				{
					g_reset_pending = true;
				}
			}

			if (g_reset_pending)
			{
				g_reset_pending = false;
				reset_device();

				if (g_device_lost)
				{
					Sleep(50);
					continue;
				}
			}

			frame();
		}

		// the game window goes back to the desktop before this window dies ( ~codol_game )
		components::codol_game::detach_game_window();
	}

	dvars::save_config();

	// windows first: they hold device objects
	ggui::loader::destroy_modules();

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	game::glob::d3d9_device = nullptr;

	if (g_device)
	{
		g_device->Release();
	}

	if (g_d3d)
	{
		g_d3d->Release();
	}

	DestroyWindow(g_hwnd);
	UnregisterClassW(wc.lpszClassName, instance);
	CoUninitialize();
	return 0;
}
