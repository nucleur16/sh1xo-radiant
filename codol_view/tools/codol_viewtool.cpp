// codol_viewtool: renders CoDOL 1.1.1.13 maps offscreen through the public API of codol_view.
//
//   codol_viewtool render    --game <install> --map <name> --out <png> [view] [options] [--stats]
//   codol_viewtool bench     --game <install> --map <name> [--frames n] [view] [options]
//   codol_viewtool statetest --game <install> --map <name> [--out <folder>] [view] [options]
//   codol_viewtool compare   <oracle.png> <ours.png> [--out <side by side png>]
//
//   view:    --size WxH ( 1280x720 )  --pos x,y,z  --angles pitch,yaw[,roll]  --spawn n  --fov degrees ( 80 )
//   options: --no-models --no-entity-models --no-brush-models --no-world --no-shadows --no-cell-culling --wireframe --lods
//            --ex ( IDirect3DDevice9Ex )  --pool managed|default  --skip-mips n  --time seconds
//
// The tool owns a hidden window and a Direct3D 9 device created the way the editor's host creates its own
// ( D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE ). It never presents.

#include "codol_view/map_renderer.hpp"
#include "png_wic.hpp"

#include <windows.h>
#include <d3d9.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>

namespace
{
	using codol::view::camera;
	using codol::view::frame_options;
	using codol::view::map_renderer;

	struct arguments
	{
		std::string command;
		std::vector<std::string> positional;
		std::map<std::string, std::string> values;

		bool flag(const char* name) const { return values.count(name) != 0; }
		std::optional<std::string> value(const char* name) const
		{
			const auto it = values.find(name);
			return it == values.end() || it->second.empty() ? std::nullopt : std::optional<std::string>(it->second);
		}
	};

	arguments parse_arguments(const int argc, char** argv)
	{
		static const char* const FLAGS[] = { "no-models", "no-entity-models", "no-brush-models", "no-world", "no-shadows", "no-cell-culling", "wireframe", "lods",
			"ex", "stats", "verbose" };
		arguments args;
		if (argc > 1)
		{
			args.command = argv[1];
		}
		for (int i = 2; i < argc; i++)
		{
			const std::string text = argv[i];
			if (text.rfind("--", 0) != 0)
			{
				args.positional.push_back(text);
				continue;
			}
			const std::string name = text.substr(2);
			const bool is_flag = std::any_of(std::begin(FLAGS), std::end(FLAGS), [&](const char* f) { return name == f; });
			if (is_flag || i + 1 >= argc)
			{
				args.values[name] = "";
			}
			else
			{
				args.values[name] = argv[++i];
			}
		}
		return args;
	}

	std::vector<float> parse_floats(const std::string& text)
	{
		std::vector<float> values;
		std::size_t at = 0;
		while (at <= text.size())
		{
			std::size_t end = text.find(',', at);
			if (end == std::string::npos)
			{
				end = text.size();
			}
			values.push_back(static_cast<float>(std::atof(text.substr(at, end - at).c_str())));
			at = end + 1;
		}
		return values;
	}

	// ---- device -------------------------------------------------------------------------------------------

	struct device_host
	{
		HWND window = nullptr;
		IDirect3D9* d3d = nullptr;
		IDirect3D9Ex* d3d_ex = nullptr;
		IDirect3DDevice9* device = nullptr;
		D3DPRESENT_PARAMETERS parameters{};
		std::string adapter;

		~device_host()
		{
			if (device) { device->Release(); }
			if (d3d_ex) { d3d_ex->Release(); }
			else if (d3d) { d3d->Release(); }
			if (window) { DestroyWindow(window); }
		}

		bool create(const bool ex, std::string& error)
		{
			WNDCLASSW wc{};
			wc.lpfnWndProc = DefWindowProcW;
			wc.hInstance = GetModuleHandleW(nullptr);
			wc.lpszClassName = L"codol_viewtool";
			RegisterClassW(&wc);
			window = CreateWindowExW(0, wc.lpszClassName, L"codol_viewtool", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
			if (!window)
			{
				error = "cannot create a window";
				return false;
			}
			parameters.Windowed = TRUE;
			parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
			parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
			parameters.BackBufferWidth = 64;
			parameters.BackBufferHeight = 64;
			parameters.BackBufferCount = 1;
			parameters.hDeviceWindow = window;
			parameters.EnableAutoDepthStencil = TRUE;
			parameters.AutoDepthStencilFormat = D3DFMT_D24S8;
			parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
			const DWORD behaviour = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE;
			HRESULT hr;
			if (ex)
			{
				if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d_ex)))
				{
					error = "Direct3DCreate9Ex failed";
					return false;
				}
				d3d = d3d_ex;
				IDirect3DDevice9Ex* device_ex = nullptr;
				hr = d3d_ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, behaviour, &parameters, nullptr, &device_ex);
				device = device_ex;
			}
			else
			{
				d3d = Direct3DCreate9(D3D_SDK_VERSION);
				if (!d3d)
				{
					error = "Direct3DCreate9 failed";
					return false;
				}
				hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, behaviour, &parameters, &device);
			}
			if (FAILED(hr) || !device)
			{
				char text[64];
				std::snprintf(text, sizeof text, "CreateDevice failed ( 0x%08lX )", static_cast<unsigned long>(hr));
				error = text;
				return false;
			}
			D3DADAPTER_IDENTIFIER9 id{};
			if (SUCCEEDED(d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id)))
			{
				adapter = id.Description;
			}
			return true;
		}

		// waits until the GPU has executed everything submitted so far
		void sync() const
		{
			IDirect3DQuery9* query = nullptr;
			if (SUCCEEDED(device->CreateQuery(D3DQUERYTYPE_EVENT, &query)) && query)
			{
				query->Issue(D3DISSUE_END);
				while (query->GetData(nullptr, 0, D3DGETDATA_FLUSH) == S_FALSE)
				{
				}
				query->Release();
			}
		}
	};

	bool read_back(IDirect3DDevice9* device, IDirect3DTexture9* texture, tools::image& out, std::string& error)
	{
		if (!texture)
		{
			error = "render() returned no texture";
			return false;
		}
		D3DSURFACE_DESC desc{};
		IDirect3DSurface9* source = nullptr;
		IDirect3DSurface9* staging = nullptr;
		bool ok = SUCCEEDED(texture->GetLevelDesc(0, &desc)) && SUCCEEDED(texture->GetSurfaceLevel(0, &source))
			&& SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &staging, nullptr))
			&& SUCCEEDED(device->GetRenderTargetData(source, staging));
		D3DLOCKED_RECT rect{};
		ok = ok && SUCCEEDED(staging->LockRect(&rect, nullptr, D3DLOCK_READONLY));
		if (ok)
		{
			out.width = static_cast<int>(desc.Width);
			out.height = static_cast<int>(desc.Height);
			out.rgba.resize(static_cast<std::size_t>(desc.Width) * desc.Height * 4);
			for (UINT y = 0; y < desc.Height; y++)
			{
				const std::uint8_t* row = static_cast<const std::uint8_t*>(rect.pBits) + static_cast<std::size_t>(y) * static_cast<std::size_t>(rect.Pitch);
				std::uint8_t* to = out.rgba.data() + static_cast<std::size_t>(y) * desc.Width * 4;
				for (UINT x = 0; x < desc.Width; x++)
				{
					to[4 * x] = row[4 * x + 2];
					to[4 * x + 1] = row[4 * x + 1];
					to[4 * x + 2] = row[4 * x];
					to[4 * x + 3] = 255;
				}
			}
			staging->UnlockRect();
		}
		else
		{
			error = "cannot read the render target back";
		}
		if (staging) { staging->Release(); }
		if (source) { source->Release(); }
		return ok;
	}

	// ---- shared set up ------------------------------------------------------------------------------------

	struct session
	{
		device_host host;
		map_renderer renderer;
		camera cam;
		frame_options frame;
		int width = 1280, height = 720;
		std::string map;
	};

	bool open_session(const arguments& args, session& s)
	{
		const auto game = args.value("game");
		const auto map = args.value("map");
		if (!game || !map)
		{
			std::fprintf(stderr, "--game and --map are required\n");
			return false;
		}
		s.map = *map;
		std::string error;
		if (!s.host.create(args.flag("ex"), error))
		{
			std::fprintf(stderr, "%s\n", error.c_str());
			return false;
		}
		codol::view::options options;
		options.game_root = *game;
		if (const auto pool = args.value("pool"))
		{
			options.pool = *pool == "default" ? codol::view::resource_pool::default_pool : codol::view::resource_pool::managed;
		}
		if (const auto skip = args.value("skip-mips"))
		{
			options.texture_skip_mips = std::atoi(skip->c_str());
		}
		options.load_model_lods = args.flag("lods");
		const auto started = std::chrono::steady_clock::now();
		if (!s.renderer.init(s.host.device, options, &error))
		{
			std::fprintf(stderr, "init: %s\n", error.c_str());
			return false;
		}
		if (!s.renderer.load_map(*map, &error))
		{
			std::fprintf(stderr, "load_map: %s\n", error.c_str());
			return false;
		}
		const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
		std::printf("loaded %s in %.1f s on %s ( %s )\n", map->c_str(), seconds, s.host.adapter.c_str(), args.flag("ex") ? "IDirect3DDevice9Ex" : "IDirect3DDevice9");

		if (const auto size = args.value("size"))
		{
			const std::size_t x = size->find_first_of("xX");
			if (x != std::string::npos)
			{
				s.width = std::atoi(size->substr(0, x).c_str());
				s.height = std::atoi(size->substr(x + 1).c_str());
			}
		}
		// RenderCommands.cs:222-232 ( DefaultCamera ): the intermission camera, else above the middle of the map
		if (!s.renderer.spawn_camera(args.value("spawn") ? std::atoi(args.value("spawn")->c_str()) : 0, s.cam))
		{
			codol::view::float3 center, half;
			s.renderer.world_bounds(center, half);
			s.cam.origin = { center.x, center.y, center.z + half.z };
			s.cam.pitch = 60;
			s.cam.yaw = 45;
		}
		if (const auto pos = args.value("pos"))
		{
			const auto p = parse_floats(*pos);
			if (p.size() >= 3)
			{
				s.cam.origin = { p[0], p[1], p[2] };
			}
		}
		if (const auto angles = args.value("angles"))
		{
			const auto a = parse_floats(*angles);
			if (a.size() >= 2)
			{
				s.cam.pitch = a[0];
				s.cam.yaw = a[1];
				s.cam.roll = a.size() > 2 ? a[2] : 0;
			}
		}
		if (const auto fov = args.value("fov"))
		{
			s.cam.fov_x = static_cast<float>(std::atof(fov->c_str()));
		}
		s.frame.draw_static_models = !args.flag("no-models");
		s.frame.draw_entity_models = !args.flag("no-models") && !args.flag("no-entity-models");
		s.frame.draw_brush_models = !args.flag("no-brush-models");
		s.frame.draw_world = !args.flag("no-world");
		s.frame.sun_shadows = !args.flag("no-shadows");
		s.frame.cell_culling = !args.flag("no-cell-culling");
		s.frame.wireframe = args.flag("wireframe");
		s.frame.model_lods = args.flag("lods");
		if (const auto time = args.value("time"))
		{
			s.frame.game_time = static_cast<float>(std::atof(time->c_str()));
		}
		std::printf("camera %.0f,%.0f,%.0f angles %.1f,%.1f fov %.0f\n", s.cam.origin.x, s.cam.origin.y, s.cam.origin.z, s.cam.pitch, s.cam.yaw, s.cam.fov_x);
		return true;
	}

	void print_memory()
	{
		PROCESS_MEMORY_COUNTERS_EX counters{};
		counters.cb = sizeof counters;
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof counters))
		{
			std::printf("memory ( %d bit process ): working set %.0f MB ( peak %.0f MB ), private %.0f MB ( peak %.0f MB )\n", static_cast<int>(sizeof(void*) * 8),
				static_cast<double>(counters.WorkingSetSize) / 1048576.0, static_cast<double>(counters.PeakWorkingSetSize) / 1048576.0,
				static_cast<double>(counters.PrivateUsage) / 1048576.0, static_cast<double>(counters.PeakPagefileUsage) / 1048576.0);
		}
	}

	void print_stats(const session& s, const bool verbose)
	{
		const auto st = s.renderer.stats();
		std::printf("frame: %d draws, %lld triangles; %d world surfaces ( %d culled ), %d of %d static + %d entity models ( %d surfaces, %d culled ), "
			"%d of %d brush models ( %d surfaces )\n", st.draw_calls, static_cast<long long>(st.triangles), st.surfaces, st.culled_surfaces, st.model_instances,
			st.static_model_count, st.entity_model_count, st.model_surfaces, st.culled_models, st.brush_models, st.brush_model_count, st.brush_model_surfaces);
		std::printf("       skipped: %d without technique, %d incomplete passes, %d hidden ( cameraRegion >= 4 ); %d shader changes, %d material changes, "
			"%d texture binds; sun shadow %d caster draws, float z %d draws; cpu %.2f ms\n", st.skipped_no_technique, st.skipped_incomplete_pass, st.skipped_hidden, st.shader_changes,
			st.material_changes, st.texture_binds, st.sun_shadow_draws, st.float_z_draws, st.cpu_ms);
		std::printf("       cells: camera in cell %d of %d, %d reached through portals ( %d cell visits )\n", st.camera_cell, st.cell_count, st.reached_cells, st.cell_visits);
		std::printf("map:   %d surfaces, %d distinct models ( %d streamed from IWDs, %d from other zones ), %d materials, %d vertex / %d pixel shaders, "
			"%d textures, %d lightmaps, %d reflection probes, %d lights\n", st.world_surface_count, st.distinct_models, st.streamed_models, st.foreign_models, st.materials,
			st.vertex_shaders, st.pixel_shaders, st.textures, st.lightmap_count, st.reflection_probe_count, st.light_count);
		std::printf("       missing textures %d, unbound code constants %d; light grid %d corner traces, %d blocked\n", st.missing_textures, st.unbound_constants,
			st.light_grid_traces, st.light_grid_blocked);
		std::printf("       GPU: textures %.1f MB, geometry %.1f MB; zones while loading: peak %.1f MB inflated; load %.0f ms\n",
			static_cast<double>(st.texture_bytes) / 1048576.0, static_cast<double>(st.geometry_bytes) / 1048576.0,
			static_cast<double>(st.zone_bytes_peak) / 1048576.0, st.load_ms);
		std::printf("       fog: %s\n", s.renderer.fog_source().c_str());
		const auto warnings = s.renderer.warnings();
		const auto failures = s.renderer.texture_failures();
		std::printf("       %zu warnings, %zu texture failures\n", warnings.size(), failures.size());
		std::size_t shown = 0;
		for (const std::string& warning : warnings)
		{
			if (!verbose && shown++ >= 25)
			{
				break;
			}
			std::printf("  warning: %s\n", warning.c_str());
		}
		shown = 0;
		for (const std::string& failure : failures)
		{
			if (!verbose && shown++ >= 10)
			{
				break;
			}
			std::printf("  texture: %s\n", failure.c_str());
		}
		if (verbose)
		{
			for (const auto& object : s.renderer.objects())
			{
				if (object.kind == codol::view::object_kind::brush_model && object.drawn)
				{
					std::printf("  brush model %s of entity %d at %.0f,%.0f,%.0f ( half size %.0f,%.0f,%.0f )\n", object.name.c_str(), object.entity, object.center.x,
						object.center.y, object.center.z, object.half_size.x, object.half_size.y, object.half_size.z);
				}
			}
		}
		print_memory();
	}

	// ---- render ---------------------------------------------------------------------------------------------

	int run_render(const arguments& args)
	{
		session s;
		if (!open_session(args, s))
		{
			return 1;
		}
		const std::string out = args.value("out").value_or(s.map + ".png");
		tools::image img;
		std::string error;
		IDirect3DTexture9* texture = s.renderer.render(s.cam, s.width, s.height, s.frame);
		if (!read_back(s.host.device, texture, img, error) || !tools::save_png(out, img, error))
		{
			std::fprintf(stderr, "%s ( %s )\n", error.c_str(), s.renderer.last_error().c_str());
			return 1;
		}
		std::printf("wrote %s ( %dx%d )\n", out.c_str(), img.width, img.height);
		if (args.flag("stats") || args.flag("verbose"))
		{
			print_stats(s, args.flag("verbose"));
		}
		return 0;
	}

	// ---- bench ----------------------------------------------------------------------------------------------

	int run_bench(const arguments& args)
	{
		session s;
		if (!open_session(args, s))
		{
			return 1;
		}
		const int frames = std::max(1, std::atoi(args.value("frames").value_or("200").c_str()));
		s.renderer.render(s.cam, s.width, s.height, s.frame);		// warm up: everything is created
		s.host.sync();
		std::vector<double> total(static_cast<std::size_t>(frames)), cpu(static_cast<std::size_t>(frames));
		for (int f = 0; f < frames; f++)
		{
			camera cam = s.cam;
			cam.yaw += static_cast<float>(f) * 0.5f;		// RenderCommands.cs:186: a slow turn
			const auto started = std::chrono::steady_clock::now();
			s.renderer.render(cam, s.width, s.height, s.frame);
			s.host.sync();
			total[static_cast<std::size_t>(f)] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
			cpu[static_cast<std::size_t>(f)] = s.renderer.stats().cpu_ms;
		}
		const auto summary = [&](std::vector<double> values, const char* what)
		{
			double sum = 0;
			for (const double v : values)
			{
				sum += v;
			}
			std::sort(values.begin(), values.end());
			std::printf("%s: mean %.2f ms, median %.2f ms, worst %.2f ms\n", what, sum / static_cast<double>(values.size()), values[values.size() / 2], values.back());
		};
		std::printf("%d frames of %dx%d, yaw turning 0.5 degrees per frame\n", frames, s.width, s.height);
		summary(total, "frame ( submission + GPU, synchronised with an event query )");
		summary(cpu, "CPU inside render()");
		s.renderer.render(s.cam, s.width, s.height, s.frame);
		print_stats(s, false);
		return 0;
	}

	// ---- compare --------------------------------------------------------------------------------------------

	struct difference
	{
		double mean[3] = {};
		double share_over_8 = 0;
		int max = 0;
	};

	difference compare_images(const tools::image& a, const tools::image& b, tools::image* side_by_side)
	{
		difference d;
		const std::size_t pixels = static_cast<std::size_t>(a.width) * static_cast<std::size_t>(a.height);
		std::size_t over = 0;
		double sum[3] = {};
		if (side_by_side)
		{
			side_by_side->width = a.width * 3;
			side_by_side->height = a.height;
			side_by_side->rgba.assign(pixels * 3 * 4, 255);
		}
		for (int y = 0; y < a.height; y++)
		{
			for (int x = 0; x < a.width; x++)
			{
				const std::size_t i = 4 * (static_cast<std::size_t>(y) * static_cast<std::size_t>(a.width) + static_cast<std::size_t>(x));
				int worst = 0;
				int delta[3];
				for (int c = 0; c < 3; c++)
				{
					delta[c] = std::abs(static_cast<int>(a.rgba[i + static_cast<std::size_t>(c)]) - static_cast<int>(b.rgba[i + static_cast<std::size_t>(c)]));
					sum[c] += delta[c];
					worst = std::max(worst, delta[c]);
				}
				d.max = std::max(d.max, worst);
				over += worst > 8 ? 1 : 0;
				if (side_by_side)
				{
					std::uint8_t* row = side_by_side->rgba.data() + 4 * (static_cast<std::size_t>(y) * static_cast<std::size_t>(a.width) * 3);
					for (int c = 0; c < 3; c++)
					{
						row[4 * static_cast<std::size_t>(x) + static_cast<std::size_t>(c)] = a.rgba[i + static_cast<std::size_t>(c)];
						row[4 * static_cast<std::size_t>(x + a.width) + static_cast<std::size_t>(c)] = b.rgba[i + static_cast<std::size_t>(c)];
						row[4 * static_cast<std::size_t>(x + 2 * a.width) + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(std::min(255, delta[c] * 4));
					}
				}
			}
		}
		for (int c = 0; c < 3; c++)
		{
			d.mean[c] = sum[c] / static_cast<double>(pixels);
		}
		d.share_over_8 = static_cast<double>(over) / static_cast<double>(pixels);
		return d;
	}

	int run_compare(const arguments& args)
	{
		if (args.positional.size() < 2)
		{
			std::fprintf(stderr, "compare <oracle.png> <ours.png> [--out <png>]\n");
			return 2;
		}
		tools::image a, b, side;
		std::string error;
		if (!tools::load_png(args.positional[0], a, error) || !tools::load_png(args.positional[1], b, error))
		{
			std::fprintf(stderr, "%s\n", error.c_str());
			return 1;
		}
		if (a.width != b.width || a.height != b.height)
		{
			std::fprintf(stderr, "sizes differ: %dx%d and %dx%d\n", a.width, a.height, b.width, b.height);
			return 1;
		}
		const auto out = args.value("out");
		const difference d = compare_images(a, b, out ? &side : nullptr);
		std::printf("mean absolute difference R %.3f G %.3f B %.3f ( of 255 ), pixels differing by more than 8: %.3f %%, largest difference %d\n",
			d.mean[0], d.mean[1], d.mean[2], 100.0 * d.share_over_8, d.max);
		if (out && !tools::save_png(*out, side, error))
		{
			std::fprintf(stderr, "%s\n", error.c_str());
			return 1;
		}
		return 0;
	}

	// ---- statetest ------------------------------------------------------------------------------------------

	// everything of the device state that can be read back
	struct device_state
	{
		std::vector<DWORD> render_states;
		std::vector<DWORD> sampler_states;
		std::vector<DWORD> texture_stage_states;
		D3DVIEWPORT9 viewport{};
		RECT scissor{};
		IDirect3DSurface9* target = nullptr;
		IDirect3DSurface9* depth = nullptr;
		IDirect3DVertexShader9* vertex_shader = nullptr;
		IDirect3DPixelShader9* pixel_shader = nullptr;
		IDirect3DVertexDeclaration9* declaration = nullptr;
		DWORD fvf = 0;
		IDirect3DVertexBuffer9* streams[2] = {};
		UINT stream_offsets[2] = {}, stream_strides[2] = {}, stream_freq[2] = {};
		IDirect3DIndexBuffer9* indices = nullptr;
		IDirect3DBaseTexture9* textures[16] = {};
		std::vector<float> vertex_constants, pixel_constants;
		D3DMATRIX transforms[3] = {};
		float clip_plane[4] = {};
	};

	device_state capture(IDirect3DDevice9* device)
	{
		device_state s;
		for (DWORD state = 7; state <= 209; state++)
		{
			DWORD value = 0xDEADBEEF;
			if (FAILED(device->GetRenderState(static_cast<D3DRENDERSTATETYPE>(state), &value)))
			{
				value = 0xDEADBEEF;
			}
			s.render_states.push_back(value);
		}
		for (DWORD sampler = 0; sampler < 16; sampler++)
		{
			for (DWORD type = 1; type <= 13; type++)
			{
				DWORD value = 0;
				device->GetSamplerState(sampler, static_cast<D3DSAMPLERSTATETYPE>(type), &value);
				s.sampler_states.push_back(value);
			}
			if (sampler < 8)
			{
				for (const D3DTEXTURESTAGESTATETYPE type : { D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2, D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2,
					D3DTSS_TEXCOORDINDEX, D3DTSS_TEXTURETRANSFORMFLAGS })
				{
					DWORD value = 0;
					device->GetTextureStageState(sampler, type, &value);
					s.texture_stage_states.push_back(value);
				}
			}
			device->GetTexture(sampler, &s.textures[sampler]);
			if (s.textures[sampler]) { s.textures[sampler]->Release(); }
		}
		device->GetViewport(&s.viewport);
		device->GetScissorRect(&s.scissor);
		device->GetRenderTarget(0, &s.target);
		if (s.target) { s.target->Release(); }
		device->GetDepthStencilSurface(&s.depth);
		if (s.depth) { s.depth->Release(); }
		device->GetVertexShader(&s.vertex_shader);
		if (s.vertex_shader) { s.vertex_shader->Release(); }
		device->GetPixelShader(&s.pixel_shader);
		if (s.pixel_shader) { s.pixel_shader->Release(); }
		device->GetVertexDeclaration(&s.declaration);
		if (s.declaration) { s.declaration->Release(); }
		device->GetFVF(&s.fvf);
		for (UINT i = 0; i < 2; i++)
		{
			device->GetStreamSource(i, &s.streams[i], &s.stream_offsets[i], &s.stream_strides[i]);
			if (s.streams[i]) { s.streams[i]->Release(); }
			device->GetStreamSourceFreq(i, &s.stream_freq[i]);
		}
		device->GetIndices(&s.indices);
		if (s.indices) { s.indices->Release(); }
		s.vertex_constants.resize(256 * 4);
		s.pixel_constants.resize(224 * 4);
		device->GetVertexShaderConstantF(0, s.vertex_constants.data(), 256);
		device->GetPixelShaderConstantF(0, s.pixel_constants.data(), 224);
		device->GetTransform(D3DTS_WORLD, &s.transforms[0]);
		device->GetTransform(D3DTS_VIEW, &s.transforms[1]);
		device->GetTransform(D3DTS_PROJECTION, &s.transforms[2]);
		device->GetClipPlane(0, s.clip_plane);
		return s;
	}

	int count_differences(const device_state& a, const device_state& b)
	{
		int differences = 0;
		const auto report = [&](const char* what, const std::size_t index)
		{
			if (differences++ < 20)
			{
				std::printf("  NOT RESTORED: %s %zu\n", what, index);
			}
		};
		for (std::size_t i = 0; i < a.render_states.size(); i++)
		{
			if (a.render_states[i] != b.render_states[i]) { report("render state", i + 7); }
		}
		for (std::size_t i = 0; i < a.sampler_states.size(); i++)
		{
			if (a.sampler_states[i] != b.sampler_states[i]) { report("sampler state ( sampler * 13 + type - 1 )", i); }
		}
		for (std::size_t i = 0; i < a.texture_stage_states.size(); i++)
		{
			if (a.texture_stage_states[i] != b.texture_stage_states[i]) { report("texture stage state", i); }
		}
		if (std::memcmp(&a.viewport, &b.viewport, sizeof a.viewport) != 0) { report("viewport", 0); }
		if (std::memcmp(&a.scissor, &b.scissor, sizeof a.scissor) != 0) { report("scissor rectangle", 0); }
		if (a.target != b.target) { report("render target", 0); }
		if (a.depth != b.depth) { report("depth stencil surface", 0); }
		if (a.vertex_shader != b.vertex_shader) { report("vertex shader", 0); }
		if (a.pixel_shader != b.pixel_shader) { report("pixel shader", 0); }
		if (a.declaration != b.declaration) { report("vertex declaration", 0); }
		if (a.fvf != b.fvf) { report("FVF", 0); }
		for (std::size_t i = 0; i < 2; i++)
		{
			if (a.streams[i] != b.streams[i] || a.stream_offsets[i] != b.stream_offsets[i] || a.stream_strides[i] != b.stream_strides[i]) { report("stream source", i); }
			if (a.stream_freq[i] != b.stream_freq[i]) { report("stream source frequency", i); }
		}
		if (a.indices != b.indices) { report("indices", 0); }
		for (std::size_t i = 0; i < 16; i++)
		{
			if (a.textures[i] != b.textures[i]) { report("texture", i); }
		}
		for (std::size_t i = 0; i < a.vertex_constants.size(); i += 4)
		{
			if (std::memcmp(&a.vertex_constants[i], &b.vertex_constants[i], 16) != 0) { report("vertex shader constant", i / 4); }
		}
		for (std::size_t i = 0; i < a.pixel_constants.size(); i += 4)
		{
			if (std::memcmp(&a.pixel_constants[i], &b.pixel_constants[i], 16) != 0) { report("pixel shader constant", i / 4); }
		}
		if (std::memcmp(a.transforms, b.transforms, sizeof a.transforms) != 0) { report("transform", 0); }
		if (std::memcmp(a.clip_plane, b.clip_plane, sizeof a.clip_plane) != 0) { report("clip plane", 0); }
		return differences;
	}

	struct host_objects
	{
		IDirect3DTexture9* texture = nullptr;
		IDirect3DTexture9* target_texture = nullptr;
		IDirect3DSurface9* target = nullptr;
		IDirect3DSurface9* depth = nullptr;
		IDirect3DVertexBuffer9* vertices = nullptr;
		IDirect3DIndexBuffer9* indices = nullptr;
		IDirect3DVertexDeclaration9* declaration = nullptr;

		void release()
		{
			for (IUnknown* object : { static_cast<IUnknown*>(target), static_cast<IUnknown*>(target_texture), static_cast<IUnknown*>(depth),
				static_cast<IUnknown*>(texture), static_cast<IUnknown*>(vertices), static_cast<IUnknown*>(indices), static_cast<IUnknown*>(declaration) })
			{
				if (object) { object->Release(); }
			}
			*this = {};
		}
	};

	// what a host that draws its own scene could leave behind - none of it is what the map shaders need
	void make_dirty(IDirect3DDevice9* device, host_objects& objects, const bool managed_objects)
	{
		const D3DPOOL pool = managed_objects ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;
		device->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, pool, &objects.texture, nullptr);
		device->CreateTexture(96, 80, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &objects.target_texture, nullptr);
		if (objects.target_texture) { objects.target_texture->GetSurfaceLevel(0, &objects.target); }
		device->CreateDepthStencilSurface(96, 80, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &objects.depth, nullptr);
		device->CreateVertexBuffer(256, D3DUSAGE_WRITEONLY, 0, pool, &objects.vertices, nullptr);
		device->CreateIndexBuffer(64, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, pool, &objects.indices, nullptr);
		const D3DVERTEXELEMENT9 elements[] = { { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 }, D3DDECL_END() };
		device->CreateVertexDeclaration(elements, &objects.declaration);

		device->SetRenderTarget(0, objects.target);
		device->SetDepthStencilSurface(objects.depth);
		const D3DVIEWPORT9 viewport{ 7, 5, 40, 30, 0.25f, 0.75f };
		device->SetViewport(&viewport);
		const RECT scissor{ 3, 3, 20, 20 };
		device->SetScissorRect(&scissor);
		const std::pair<D3DRENDERSTATETYPE, DWORD> states[] =
		{
			{ D3DRS_SCISSORTESTENABLE, TRUE }, { D3DRS_ZENABLE, D3DZB_FALSE }, { D3DRS_ZWRITEENABLE, FALSE }, { D3DRS_ZFUNC, D3DCMP_NEVER },
			{ D3DRS_ALPHABLENDENABLE, TRUE }, { D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR }, { D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR }, { D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT },
			{ D3DRS_ALPHATESTENABLE, TRUE }, { D3DRS_ALPHAREF, 250 }, { D3DRS_ALPHAFUNC, D3DCMP_LESS }, { D3DRS_CULLMODE, D3DCULL_CW },
			{ D3DRS_FILLMODE, D3DFILL_POINT }, { D3DRS_FOGENABLE, TRUE }, { D3DRS_FOGCOLOR, 0xFF00FF00 }, { D3DRS_COLORWRITEENABLE, 0x1 },
			{ D3DRS_STENCILENABLE, TRUE }, { D3DRS_STENCILFUNC, D3DCMP_NEVER }, { D3DRS_STENCILREF, 77 }, { D3DRS_STENCILMASK, 0x0F }, { D3DRS_STENCILWRITEMASK, 0xF0 },
			{ D3DRS_SRGBWRITEENABLE, TRUE }, { D3DRS_CLIPPLANEENABLE, 1 }, { D3DRS_LIGHTING, TRUE }, { D3DRS_SEPARATEALPHABLENDENABLE, FALSE },
			{ D3DRS_TWOSIDEDSTENCILMODE, FALSE }, { D3DRS_DEPTHBIAS, 0x3F000000 }, { D3DRS_SLOPESCALEDEPTHBIAS, 0x40000000 }, { D3DRS_SHADEMODE, D3DSHADE_FLAT },
			{ D3DRS_BLENDFACTOR, 0x11223344 }, { D3DRS_MULTISAMPLEMASK, 0x1 }, { D3DRS_POINTSPRITEENABLE, TRUE }, { D3DRS_COLORWRITEENABLE1, 0x2 },
			{ D3DRS_ANTIALIASEDLINEENABLE, TRUE }, { D3DRS_DITHERENABLE, TRUE }, { D3DRS_SPECULARENABLE, TRUE }, { D3DRS_CLIPPING, FALSE },
		};
		for (const auto& [state, value] : states)
		{
			device->SetRenderState(state, value);
		}
		const float plane[4] = { 0, 0, 1, -100000.0f };		// clips everything
		device->SetClipPlane(0, plane);
		for (DWORD sampler = 0; sampler < 16; sampler++)
		{
			device->SetTexture(sampler, objects.texture);
			device->SetSamplerState(sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
			device->SetSamplerState(sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
			device->SetSamplerState(sampler, D3DSAMP_ADDRESSW, D3DTADDRESS_MIRROR);
			device->SetSamplerState(sampler, D3DSAMP_BORDERCOLOR, 0xFFFF00FF);
			device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			device->SetSamplerState(sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			const float bias = 3.0f;
			DWORD bias_bits;
			std::memcpy(&bias_bits, &bias, 4);
			device->SetSamplerState(sampler, D3DSAMP_MIPMAPLODBIAS, bias_bits);
			device->SetSamplerState(sampler, D3DSAMP_MAXMIPLEVEL, 4);
			device->SetSamplerState(sampler, D3DSAMP_MAXANISOTROPY, 1);
			device->SetSamplerState(sampler, D3DSAMP_SRGBTEXTURE, TRUE);
		}
		std::vector<float> junk(256 * 4);
		for (std::size_t i = 0; i < junk.size(); i++)
		{
			junk[i] = 1.0e6f + static_cast<float>(i);
		}
		device->SetVertexShaderConstantF(0, junk.data(), 256);
		device->SetPixelShaderConstantF(0, junk.data(), 224);
		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetVertexDeclaration(objects.declaration);
		device->SetStreamSource(0, objects.vertices, 12, 12);
		device->SetStreamSource(1, objects.vertices, 24, 4);
		device->SetIndices(objects.indices);
		D3DMATRIX matrix{};
		matrix._11 = 2; matrix._22 = 3; matrix._33 = 4; matrix._44 = 1; matrix._41 = 5;
		device->SetTransform(D3DTS_WORLD, &matrix);
		device->SetTransform(D3DTS_VIEW, &matrix);
		device->SetTransform(D3DTS_PROJECTION, &matrix);
	}

	int run_statetest(const arguments& args)
	{
		session s;
		if (!open_session(args, s))
		{
			return 1;
		}
		IDirect3DDevice9* device = s.host.device;
		const std::filesystem::path folder = args.value("out").value_or(".");
		std::error_code ec;
		std::filesystem::create_directories(folder, ec);
		std::string error;
		int failures = 0;
		const auto check = [&](const bool ok, const char* what)
		{
			std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
			failures += ok ? 0 : 1;
		};

		const unsigned int fpu_before = _controlfp(0, 0);

		// 1. the reference frame on a clean device
		tools::image clean, dirty, in_scene, after_reset;
		check(read_back(device, s.renderer.render(s.cam, s.width, s.height, s.frame), clean, error), "frame on a clean device");
		tools::save_png(folder / "state_clean.png", clean, error);

		// 2. the same view after the host left the device in a state that suits nothing
		host_objects objects;
		make_dirty(device, objects, !args.flag("ex"));
		const device_state before = capture(device);
		IDirect3DTexture9* texture = s.renderer.render(s.cam, s.width, s.height, s.frame);
		const device_state after = capture(device);
		const int differences = count_differences(before, after);
		check(differences == 0, "device state after render() equals the state before ( render states, sampler states, targets, viewport, scissor, shaders, "
			"declaration, streams, indices, textures, shader constants, transforms, clip plane )");
		check(read_back(device, texture, dirty, error), "frame on the dirty device");
		tools::save_png(folder / "state_dirty.png", dirty, error);
		const difference d1 = compare_images(clean, dirty, nullptr);
		std::printf("      clean vs dirty: largest difference %d, mean %.4f %.4f %.4f\n", d1.max, d1.mean[0], d1.mean[1], d1.mean[2]);
		check(d1.max == 0, "the frame drawn on the dirty device is identical to the clean one");

		// 3. inside the host's scene
		const bool began = SUCCEEDED(device->BeginScene());
		frame_options inside = s.frame;
		inside.scene = codol::view::scene_mode::automatic;
		texture = s.renderer.render(s.cam, s.width, s.height, inside);
		const bool ended = SUCCEEDED(device->EndScene());		// fails when render() ended the host's scene
		check(began && ended, "render() inside the host's BeginScene / EndScene leaves the scene open");
		check(read_back(device, texture, in_scene, error) && compare_images(clean, in_scene, nullptr).max == 0, "the frame drawn inside the host's scene is identical");

		// back to a state the device can be reset in
		device->SetRenderTarget(0, nullptr);
		{
			IDirect3DSurface9* back = nullptr;
			if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)))
			{
				device->SetRenderTarget(0, back);
				back->Release();
			}
		}
		device->SetDepthStencilSurface(nullptr);
		for (DWORD sampler = 0; sampler < 16; sampler++)
		{
			device->SetTexture(sampler, nullptr);
		}
		device->SetStreamSource(0, nullptr, 0, 0);
		device->SetStreamSource(1, nullptr, 0, 0);
		device->SetIndices(nullptr);
		device->SetVertexDeclaration(nullptr);
		objects.release();

		// 4. device reset with another back buffer size
		s.renderer.on_device_lost();
		s.host.parameters.BackBufferWidth = 200;
		s.host.parameters.BackBufferHeight = 120;
		const HRESULT reset = device->Reset(&s.host.parameters);
		char text[160];
		std::snprintf(text, sizeof text, "IDirect3DDevice9::Reset with a 200x120 back buffer after on_device_lost() ( 0x%08lX )", static_cast<unsigned long>(reset));
		check(SUCCEEDED(reset), text);
		check(s.renderer.on_device_reset(), "on_device_reset()");
		check(read_back(device, s.renderer.render(s.cam, s.width, s.height, s.frame), after_reset, error), "frame after the reset");
		tools::save_png(folder / "state_after_reset.png", after_reset, error);
		const difference d2 = compare_images(clean, after_reset, nullptr);
		std::printf("      clean vs after reset: largest difference %d\n", d2.max);
		check(d2.max == 0, "the frame after the reset is identical to the clean one");

		const unsigned int fpu_after = _controlfp(0, 0);
		std::snprintf(text, sizeof text, "FPU control word unchanged ( 0x%08X before, 0x%08X after )", fpu_before, fpu_after);
		check(fpu_before == fpu_after, text);

		std::printf("%s: %d failure(s)\n", failures == 0 ? "state test passed" : "STATE TEST FAILED", failures);
		print_memory();
		return failures == 0 ? 0 : 1;
	}
}

int main(const int argc, char** argv)
{
	const arguments args = parse_arguments(argc, argv);
	if (args.command == "render")
	{
		return run_render(args);
	}
	if (args.command == "bench")
	{
		return run_bench(args);
	}
	if (args.command == "statetest")
	{
		return run_statetest(args);
	}
	if (args.command == "compare")
	{
		return run_compare(args);
	}
	std::fprintf(stderr,
		"codol_viewtool render    --game <install> --map <name> --out <png> [--size WxH] [--pos x,y,z --angles pitch,yaw[,roll] | --spawn n] [--fov deg]\n"
		"                         [--no-models] [--no-entity-models] [--no-brush-models] [--no-world] [--no-shadows] [--no-cell-culling] [--wireframe] [--lods]\n"
		"                         [--ex] [--pool managed|default] [--skip-mips n] [--time s] [--stats] [--verbose]\n"
		"codol_viewtool bench     --game <install> --map <name> [--frames n] [view and options as above]\n"
		"codol_viewtool statetest --game <install> --map <name> [--out <folder>] [view and options as above]\n"
		"codol_viewtool compare   <oracle.png> <ours.png> [--out <side by side png: oracle | ours | 4 x difference>]\n");
	return 2;
}
