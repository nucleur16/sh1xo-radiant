// codol_viewtool: renders CoDOL 1.1.1.13 maps offscreen through the public API of codol_view.
//
//   codol_viewtool render    --game <install> --map <name> --out <png> [view] [options] [--stats]
//   codol_viewtool bench     --game <install> --map <name> [--frames n] [view] [options]
//   codol_viewtool statetest --game <install> --map <name> [--out <folder>] [view] [options]
//   codol_viewtool compare   <oracle.png> <ours.png> [--out <side by side png>]
//   codol_viewtool edittest  --game <install> --map <name> [--out <folder>] [view] [options] [--model <xmodel>] [--material <name>]
//   codol_viewtool pickbench --game <install> --map <name> [view]
//
//   view:    --size WxH ( 1280x720 )  --pos x,y,z  --angles pitch,yaw[,roll]  --spawn n  --fov degrees ( 80 )
//   options: --no-models --no-entity-models --no-brush-models --no-world --no-shadows --no-cell-culling --wireframe --lods
//            --ex ( IDirect3DDevice9Ex )  --pool managed|default  --skip-mips n  --time seconds
//            --keep-edit-data ( options::keep_pick_geometry + keep_light_grid )  --extra-zones a,b
//            --zone-dirs "a;b" ( folders searched for <zone>.ff before the install's, e.g. the output of codol_mapc )
//   render:  --box "x,y,z,half:material;.."  --model "x,y,z[,yaw[,scale]]:xmodel;.."  --hide "kind,id;.."
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
			"ex", "stats", "verbose", "keep-edit-data" };
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

	bool open_session(const arguments& args, session& s, const bool edit = false)
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
		if (const auto dirs = args.value("zone-dirs"))
		{
			// folders searched for <zone>.ff ahead of the install's own ( a compiled map that is not in the install )
			std::size_t at = 0;
			while (at <= dirs->size())
			{
				std::size_t end = dirs->find(';', at);
				end = end == std::string::npos ? dirs->size() : end;
				if (end > at)
				{
					options.zone_directories.push_back(dirs->substr(at, end - at));
				}
				at = end + 1;
			}
			options.zone_directories.push_back(options.game_root / "client_shipRetail" / "chinese");
			options.zone_directories.push_back(options.game_root / "client_shipRetail" / "patch");
		}
		options.load_model_lods = args.flag("lods");
		options.keep_pick_geometry = options.keep_light_grid = edit || args.flag("keep-edit-data");
		if (const auto extra = args.value("extra-zones"))
		{
			std::size_t at = 0;
			while (at <= extra->size())
			{
				std::size_t end = extra->find(',', at);
				end = end == std::string::npos ? extra->size() : end;
				if (end > at)
				{
					options.extra_zones.push_back(extra->substr(at, end - at));
				}
				at = end + 1;
			}
		}
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

	void make_box(const codol::view::float3& center, const codol::view::float3& half, float texels_per_unit_u, float texels_per_unit_v,
		std::vector<codol::view::mesh_vertex>& vertices, std::vector<std::uint32_t>& indices);

	// --box "x,y,z,half:material;.."  --model "x,y,z,yaw:xmodel;.."  --hide "kind,id;.." ( kind: 0 surface, 1 static model, 2 entity model, 3 brush model )
	void apply_edit_switches(const arguments& args, session& s)
	{
		const auto each = [](const std::string& list, const auto& visit)
		{
			std::size_t at = 0;
			while (at < list.size())
			{
				std::size_t end = list.find(';', at);
				end = end == std::string::npos ? list.size() : end;
				if (end > at)
				{
					visit(list.substr(at, end - at));
				}
				at = end + 1;
			}
		};
		std::string error;
		if (const auto boxes = args.value("box"))
		{
			each(*boxes, [&](const std::string& item)
			{
				const std::size_t colon = item.find(':');
				const auto numbers = parse_floats(item.substr(0, colon));
				const std::string material = colon == std::string::npos ? std::string() : item.substr(colon + 1);
				if (numbers.size() < 4)
				{
					return;
				}
				codol::view::material_info info;
				const bool known = s.renderer.material_details(material, info);
				std::vector<codol::view::mesh_vertex> vertices;
				std::vector<std::uint32_t> indices;
				// radiant's default texture scale: 0.25 units per texel
				make_box({ numbers[0], numbers[1], numbers[2] }, { numbers[3], numbers[3], numbers[3] }, known && info.width > 0 ? 4.0f / static_cast<float>(info.width) : 1.0f / 128.0f,
					known && info.height > 0 ? 4.0f / static_cast<float>(info.height) : 1.0f / 128.0f, vertices, indices);
				codol::view::mesh_data data;
				data.vertices = vertices.data();
				data.vertex_count = vertices.size();
				data.indices = indices.data();
				data.index_count = indices.size();
				data.material = material;
				const int handle = s.renderer.create_mesh(data, &error);
				static const char* const SHADING[] = { "none", "game_material", "color_map", "flat_color" };
				std::printf("box of '%s' ( techset %s, colour map %s %dx%d ): mesh %d, drawn as %s %s\n", material.c_str(), info.technique_set.c_str(), info.color_map.c_str(),
					info.width, info.height, handle, SHADING[static_cast<int>(s.renderer.mesh_shading_of(handle))], handle ? "" : error.c_str());
			});
		}
		if (const auto models = args.value("model"))
		{
			each(*models, [&](const std::string& item)
			{
				const std::size_t colon = item.find(':');
				const auto numbers = parse_floats(item.substr(0, colon));
				if (numbers.size() < 3 || colon == std::string::npos)
				{
					return;
				}
				codol::view::transform placement;
				placement.origin = { numbers[0], numbers[1], numbers[2] };
				placement.angles.y = numbers.size() > 3 ? numbers[3] : 0.0f;
				placement.scale = numbers.size() > 4 ? numbers[4] : 1.0f;
				const int handle = s.renderer.add_model(item.substr(colon + 1), placement, &error);
				std::printf("model '%s': handle %d %s\n", item.substr(colon + 1).c_str(), handle, handle ? "" : error.c_str());
			});
		}
		if (const auto hidden = args.value("hide"))
		{
			each(*hidden, [&](const std::string& item)
			{
				const auto numbers = parse_floats(item);
				if (numbers.size() >= 2)
				{
					const codol::view::object_ref ref{ static_cast<codol::view::object_kind>(static_cast<int>(numbers[0])), static_cast<int>(numbers[1]) };
					std::printf("hide %d,%d: %s\n", static_cast<int>(numbers[0]), static_cast<int>(numbers[1]), s.renderer.set_hidden(ref, true) ? "ok" : "no such object");
				}
			});
		}
	}

	int run_render(const arguments& args)
	{
		session s;
		if (!open_session(args, s))
		{
			return 1;
		}
		apply_edit_switches(args, s);
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
			if (a.streams[i] != b.streams[i] || a.stream_offsets[i] != b.stream_offsets[i] || a.stream_strides[i] != b.stream_strides[i])
			{
				report("stream source", i);
				std::printf("    buffer %p -> %p, offset %u -> %u, stride %u -> %u\n", static_cast<void*>(a.streams[i]), static_cast<void*>(b.streams[i]),
					a.stream_offsets[i], b.stream_offsets[i], a.stream_strides[i], b.stream_strides[i]);
			}
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
	// ---- edittest ------------------------------------------------------------------------------------------
	// Exercises the editor API offscreen: picking ( with a depth cross check against what is drawn ), hiding,
	// moving, placing models, dynamic meshes, the overlay hook, the catalogs and thumbnails.

	using codol::view::float3;
	using codol::view::object_kind;
	using codol::view::object_ref;

	struct line_vertex
	{
		float x, y, z;
		DWORD color;
	};

	float3 add(const float3& a, const float3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	float3 scaled(const float3& a, const float s) { return { a.x * s, a.y * s, a.z * s }; }
	float distance_between(const float3& a, const float3& b)
	{
		const float3 d{ a.x - b.x, a.y - b.y, a.z - b.z };
		return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
	}

	const char* kind_name(const object_kind kind)
	{
		switch (kind)
		{
		case object_kind::world_surface: return "world_surface";
		case object_kind::static_model: return "static_model";
		case object_kind::entity_model: return "entity_model";
		case object_kind::brush_model: return "brush_model";
		case object_kind::placed_model: return "placed_model";
		case object_kind::dynamic_mesh: return "dynamic_mesh";
		}
		return "?";
	}

	void push_line(std::vector<line_vertex>& lines, const float3& a, const float3& b, const DWORD color)
	{
		lines.push_back({ a.x, a.y, a.z, color });
		lines.push_back({ b.x, b.y, b.z, color });
	}

	void push_box(std::vector<line_vertex>& lines, const codol::view::oriented_box& box, const DWORD color)
	{
		float3 corners[8];
		for (int c = 0; c < 8; c++)
		{
			corners[c] = add(add(add(box.center, scaled(box.axis[0], (c & 1) ? box.half_size.x : -box.half_size.x)),
				scaled(box.axis[1], (c & 2) ? box.half_size.y : -box.half_size.y)), scaled(box.axis[2], (c & 4) ? box.half_size.z : -box.half_size.z));
		}
		for (int c = 0; c < 8; c++)
		{
			for (const int bit : { 1, 2, 4 })
			{
				if (!(c & bit))
				{
					push_line(lines, corners[c], corners[c | bit], color);
				}
			}
		}
	}

	// a box of `half` around `center`, 24 vertices, uv = world units / texel size
	void make_box(const codol::view::float3& center, const codol::view::float3& half, const float texels_per_unit_u, const float texels_per_unit_v,
		std::vector<codol::view::mesh_vertex>& vertices, std::vector<std::uint32_t>& indices)
	{
		vertices.clear();
		indices.clear();
		for (int face = 0; face < 6; face++)
		{
			const int axis = face / 2;
			const float sign = (face & 1) ? 1.0f : -1.0f;
			const int u_axis = (axis + 1) % 3, v_axis = (axis + 2) % 3;
			const float h[3] = { half.x, half.y, half.z };
			const float c[3] = { center.x, center.y, center.z };
			const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
			for (int corner = 0; corner < 4; corner++)
			{
				const float su = (corner & 1) ? 1.0f : -1.0f, sv = (corner & 2) ? 1.0f : -1.0f;
				float p[3];
				p[axis] = c[axis] + sign * h[axis];
				p[u_axis] = c[u_axis] + su * h[u_axis];
				p[v_axis] = c[v_axis] + sv * h[v_axis];
				codol::view::mesh_vertex v;
				v.position = { p[0], p[1], p[2] };
				float n[3] = {};
				n[axis] = sign;
				v.normal = { n[0], n[1], n[2] };
				v.uv[0] = p[u_axis] * texels_per_unit_u;
				v.uv[1] = -p[v_axis] * texels_per_unit_v;
				vertices.push_back(v);
			}
			// the winding is left to the renderer ( it orients triangles by the normals )
			for (const std::uint32_t i : { 0u, 1u, 2u, 2u, 1u, 3u })
			{
				indices.push_back(base + i);
			}
		}
	}

	int run_edittest(const arguments& args)
	{
		session s;
		if (!open_session(args, s, true))
		{
			return 1;
		}
		map_renderer& r = s.renderer;
		IDirect3DDevice9* device = s.host.device;
		const std::filesystem::path folder = args.value("out").value_or(".");
		std::error_code ec;
		std::filesystem::create_directories(folder, ec);
		std::string error;
		int failures = 0;
		const auto check = [&](const bool ok, const std::string& what)
		{
			std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
			failures += ok ? 0 : 1;
		};
		const auto frame_of = [&](const frame_options& frame, tools::image& img) { return read_back(device, r.render(s.cam, s.width, s.height, frame), img, error); };
		const auto save = [&](const char* name, const tools::image& img)
		{
			if (!tools::save_png(folder / name, img, error))
			{
				std::fprintf(stderr, "%s\n", error.c_str());
			}
		};
		const auto pixel = [&](const tools::image& img, const int x, const int y) -> std::uint32_t
		{
			const std::uint8_t* p = img.rgba.data() + 4 * (static_cast<std::size_t>(y) * static_cast<std::size_t>(img.width) + static_cast<std::size_t>(x));
			return (static_cast<std::uint32_t>(p[0]) << 16) | (static_cast<std::uint32_t>(p[1]) << 8) | p[2];
		};

		{
			const auto st = r.stats();
			std::printf("pick geometry: %lld triangles, %.1f MB, built in %.0f ms ( part of the %.0f ms load ); light grid + sight trace kept: %.1f MB\n",
				static_cast<long long>(st.pick_triangles), static_cast<double>(st.pick_bytes) / 1048576.0, st.pick_build_ms, st.load_ms,
				static_cast<double>(st.light_grid_bytes) / 1048576.0);
		}

		// ---- 1. the plain frame
		tools::image base;
		check(frame_of(s.frame, base), "plain frame");
		save("edit_0_base.png", base);

		// ---- 2. camera math: project( screen_ray ) is the pixel again
		{
			float worst = 0;
			for (int y = 0; y < s.height; y += 37)
			{
				for (int x = 0; x < s.width; x += 41)
				{
					float3 origin, direction;
					codol::view::screen_ray(s.cam, s.width, s.height, static_cast<float>(x), static_cast<float>(y), origin, direction);
					float xy[2] = {}, depth = 0;
					if (!codol::view::project(s.cam, s.width, s.height, add(origin, scaled(direction, 500.0f)), xy, depth))
					{
						worst = 1.0e9f;
						continue;
					}
					worst = std::max({ worst, std::fabs(xy[0] - static_cast<float>(x)), std::fabs(xy[1] - static_cast<float>(y)) });
				}
			}
			char text[160];
			std::snprintf(text, sizeof text, "project( screen_ray( pixel ) ) returns the pixel ( worst error %.4f pixels )", worst);
			check(worst < 0.01f, text);
		}

		// ---- 3. picking: a grid of pixels
		struct sample
		{
			int x, y;
			bool hit;
			codol::view::pick_result result;
		};
		std::vector<sample> samples;
		const int columns = 24, rows = 14;
		double pick_total = 0, pick_worst = 0;
		for (int row = 0; row < rows; row++)
		{
			for (int column = 0; column < columns; column++)
			{
				sample smp{};
				smp.x = (2 * column + 1) * s.width / (2 * columns);
				smp.y = (2 * row + 1) * s.height / (2 * rows);
				const auto started = std::chrono::steady_clock::now();
				smp.hit = r.pick(s.cam, s.width, s.height, static_cast<float>(smp.x), static_cast<float>(smp.y), smp.result);
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
				pick_total += ms;
				pick_worst = std::max(pick_worst, ms);
				samples.push_back(std::move(smp));
			}
		}
		{
			int hits = 0, by_kind[6] = {};
			float worst_pixels = 0, worst_distance = 0;
			for (const sample& smp : samples)
			{
				if (!smp.hit)
				{
					continue;
				}
				hits++;
				by_kind[static_cast<int>(smp.result.object.kind)]++;
				float xy[2] = {}, depth = 0;
				if (codol::view::project(s.cam, s.width, s.height, smp.result.point, xy, depth))
				{
					worst_pixels = std::max({ worst_pixels, std::fabs(xy[0] - static_cast<float>(smp.x)), std::fabs(xy[1] - static_cast<float>(smp.y)) });
				}
				else
				{
					worst_pixels = 1.0e9f;
				}
				worst_distance = std::max(worst_distance, std::fabs(distance_between(smp.result.point, s.cam.origin) - smp.result.distance));
			}
			std::printf("pick: %d x %d pixels, %d hits ( %d world surfaces, %d static models, %d entity models, %d brush models ); %.3f ms mean, %.3f ms worst\n",
				columns, rows, hits, by_kind[0], by_kind[1], by_kind[2], by_kind[3], pick_total / static_cast<double>(samples.size()), pick_worst);
			char text[200];
			std::snprintf(text, sizeof text, "picked points project back to their pixels ( worst %.3f pixels ) and distance = |point - eye| ( worst %.4f units )",
				worst_pixels, worst_distance);
			check(hits > 0 && worst_pixels < 0.5f && worst_distance < 0.05f, text);
			check(pick_worst < 10.0, "every pick took less than 10 ms");
			for (std::size_t i = 0; i < samples.size(); i += 29)
			{
				const sample& smp = samples[i];
				if (smp.hit)
				{
					std::printf("  pixel %4d,%3d: %-13s id %-6d entity %-4d distance %8.1f point %.1f,%.1f,%.1f normal %.2f,%.2f,%.2f surface %d triangle %d material %s\n",
						smp.x, smp.y, kind_name(smp.result.object.kind), smp.result.object.id, smp.result.entity, smp.result.distance, smp.result.point.x,
						smp.result.point.y, smp.result.point.z, smp.result.normal.x, smp.result.normal.y, smp.result.normal.z, smp.result.surface,
						smp.result.triangle, smp.result.material.c_str());
				}
				else
				{
					std::printf("  pixel %4d,%3d: nothing\n", smp.x, smp.y);
				}
			}
		}

		// ---- 4. the depth of what is drawn against the picked distance: a marker a little in front of each
		// picked point must be visible, a marker a little behind it must be hidden by the scene's depth buffer
		{
			const auto markers = [&](const float push, tools::image& img)
			{
				frame_options frame = s.frame;
				frame.overlay = [&](const codol::view::overlay_context& context)
				{
					std::vector<line_vertex> triangles;
					// camera right / up from the view matrix ( its first two columns )
					const float3 right{ context.view[0], context.view[4], context.view[8] };
					const float3 up{ context.view[1], context.view[5], context.view[9] };
					for (const sample& smp : samples)
					{
						if (!smp.hit)
						{
							continue;
						}
						float3 origin, direction;
						codol::view::screen_ray(context.cam, context.width, context.height, static_cast<float>(smp.x), static_cast<float>(smp.y), origin, direction);
						const float t = smp.result.distance + push * std::max(1.0f, 0.01f * smp.result.distance);
						const float3 centre = add(origin, scaled(direction, t));
						const float size = 2.0f * t * std::tan(context.cam.fov_x * 3.14159265f / 360.0f) / static_cast<float>(context.width);		// about two pixels
						const float3 a = add(add(centre, scaled(right, -size)), scaled(up, -size)), b = add(add(centre, scaled(right, size)), scaled(up, -size));
						const float3 c = add(add(centre, scaled(right, size)), scaled(up, size)), d = add(add(centre, scaled(right, -size)), scaled(up, size));
						for (const float3& p : { a, b, c, a, c, d })
						{
							triangles.push_back({ p.x, p.y, p.z, 0xFFFF00FF });
						}
					}
					if (!triangles.empty())
					{
						context.device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(triangles.size() / 3), triangles.data(), sizeof(line_vertex));
					}
				};
				return frame_of(frame, img);
			};
			tools::image front, behind;
			check(markers(-1.0f, front) && markers(1.0f, behind), "frames with overlay markers");
			save("edit_1_pick_markers_in_front.png", front);
			save("edit_1_pick_markers_behind.png", behind);
			// A surface that writes no depth ( decals, blended materials ) or has alpha tested holes ( fences, foliage )
			// is picked on its triangles but does not hide what is behind it: those are counted apart
			int tested = 0, front_visible = 0, solid = 0, solid_hidden = 0, other = 0, other_hidden = 0;
			std::map<std::string, int> unexplained;
			for (const sample& smp : samples)
			{
				if (!smp.hit)
				{
					continue;
				}
				tested++;
				const bool visible = pixel(front, smp.x, smp.y) == 0xFF00FF;
				const bool hidden = pixel(behind, smp.x, smp.y) != 0xFF00FF;
				front_visible += visible ? 1 : 0;
				codol::view::material_info info;
				const bool is_solid = r.material_details(smp.result.material, info) && info.writes_depth && !info.alpha_tested;
				(is_solid ? solid : other)++;
				(is_solid ? solid_hidden : other_hidden) += hidden ? 1 : 0;
				if (!visible || (is_solid && !hidden))
				{
					unexplained[smp.result.material]++;
					std::printf("  depth mismatch at %d,%d: %s id %d material %s distance %.1f ( marker in front %s, marker behind %s )\n", smp.x, smp.y,
						kind_name(smp.result.object.kind), smp.result.object.id, smp.result.material.c_str(), smp.result.distance, visible ? "visible" : "HIDDEN",
						hidden ? "hidden" : "VISIBLE");
				}
			}
			char text[400];
			std::snprintf(text, sizeof text, "depth of the drawn scene at the %d picked pixels: a marker 1 %% in front of the picked point is visible at %d; a marker 1 %% "
				"behind it is hidden at %d of the %d hits on solid materials ( and at %d of the %d hits on materials without depth writes or with alpha tested holes )",
				tested, front_visible, solid_hidden, solid, other_hidden, other);
			check(tested > 0 && front_visible == tested && solid_hidden == solid, text);
		}

		// ---- 5. hiding: the picked object of some pixels goes, the pixel changes; show_all brings the frame back
		{
			std::vector<object_ref> hidden;
			int changed = 0, tried = 0;
			for (std::size_t i = 7; i < samples.size() && tried < 12; i += 5)
			{
				// solid materials only: a decal layer or the hole of a fence can leave the pixel as it was
				const sample& smp = samples[i];
				codol::view::material_info info;
				if (!smp.hit || std::find(hidden.begin(), hidden.end(), smp.result.object) != hidden.end() || !r.material_details(smp.result.material, info)
					|| !info.writes_depth || info.alpha_tested || info.blends)
				{
					continue;
				}
				tried++;
				r.set_hidden(smp.result.object, true);
				hidden.push_back(smp.result.object);
				tools::image img;
				frame_of(s.frame, img);
				const bool differs = pixel(img, smp.x, smp.y) != pixel(base, smp.x, smp.y);
				changed += differs ? 1 : 0;
				codol::view::pick_result again;
				const bool hit_again = r.pick(s.cam, s.width, s.height, static_cast<float>(smp.x), static_cast<float>(smp.y), again);
				if (!differs || (hit_again && again.object == smp.result.object) || (hit_again && again.distance < smp.result.distance))
				{
					std::printf("  hide %s %d ( %s ) at %d,%d: pixel %s, pick behind it %s\n", kind_name(smp.result.object.kind), smp.result.object.id,
						smp.result.material.c_str(), smp.x, smp.y, differs ? "changed" : "DID NOT CHANGE", hit_again ? kind_name(again.object.kind) : "nothing");
				}
				if (hit_again && (again.object == smp.result.object || again.distance < smp.result.distance))
				{
					failures++;
					std::printf("FAIL  a hidden object was picked again\n");
				}
			}
			tools::image img;
			frame_of(s.frame, img);
			save("edit_2_hidden.png", img);
			char text[160];
			std::snprintf(text, sizeof text, "hiding the picked object changes its pixel ( %d of %d ); %d objects hidden", changed, tried, r.stats().hidden_objects);
			check(tried > 0 && changed == tried && r.stats().hidden_objects == tried, text);
			r.show_all();
			frame_of(s.frame, img);
			check(compare_images(base, img, nullptr).max == 0 && r.stats().hidden_objects == 0, "after show_all the frame is identical to the plain one");
		}

		// ---- 5b. a brush model: pick, outline, move, put back
		{
			codol::view::scene_object brush;
			float nearest_brush = FLT_MAX;
			float brush_xy[2] = {};
			for (const auto& candidate : r.objects())
			{
				float xy[2] = {}, depth = 0;
				if (candidate.kind == object_kind::brush_model && candidate.drawn && codol::view::project(s.cam, s.width, s.height, candidate.center, xy, depth)
					&& xy[0] > 0 && xy[1] > 0 && xy[0] < static_cast<float>(s.width) && xy[1] < static_cast<float>(s.height)
					&& distance_between(candidate.center, s.cam.origin) < nearest_brush)
				{
					// one that is seen: the pick at its centre is the brush model itself
					codol::view::pick_result seen;
					if (!r.pick(s.cam, s.width, s.height, xy[0], xy[1], seen) || !(seen.object == candidate.ref()))
					{
						continue;
					}
					nearest_brush = distance_between(candidate.center, s.cam.origin);
					brush = candidate;
					brush_xy[0] = xy[0];
					brush_xy[1] = xy[1];
				}
			}
			if (nearest_brush == FLT_MAX)
			{
				std::printf("no brush model is drawn in this view ( %d in the map )\n", r.stats().brush_model_count);
			}
			else
			{
				const codol::view::pick_options only_brushes{ .world = false, .static_models = false, .entity_models = false, .placed_models = false, .meshes = false };
				codol::view::pick_result hit;
				const bool picked = r.pick(s.cam, s.width, s.height, brush_xy[0], brush_xy[1], hit, only_brushes);
				std::vector<float3> triangles;
				codol::view::oriented_box box;
				const bool outlined = r.object_box(brush.ref(), box) && r.object_triangles(brush.ref(), triangles);
				std::printf("brush model %s of entity %d: centre at pixel %.0f,%.0f, %zu triangles, box half size %.0f,%.0f,%.0f; a ray at its centre picks %s %d ( %s ) at %.1f\n",
					brush.name.c_str(), brush.entity, brush_xy[0], brush_xy[1], triangles.size() / 3, box.half_size.x, box.half_size.y, box.half_size.z,
					picked ? kind_name(hit.object.kind) : "nothing", picked ? hit.object.id : -1, picked ? hit.material.c_str() : "", picked ? hit.distance : 0.0f);
				check(outlined && !triangles.empty(), "object_box and object_triangles of a brush model");
				codol::view::transform before, t;
				check(r.get_transform(brush.ref(), before), "get_transform of a brush model");
				t = before;
				t.origin.z += 40.0f;
				t.angles.y += 25.0f;
				check(r.set_transform(brush.ref(), t), "set_transform of a brush model ( raised 40, turned 25 degrees )");
				tools::image img;
				frame_of(s.frame, img);
				save("edit_2b_brush_moved.png", img);
				codol::view::scene_object moved;
				r.object(brush.ref(), moved);
				float xy[2] = {}, depth = 0;
				const bool again = codol::view::project(s.cam, s.width, s.height, moved.center, xy, depth) && r.pick(s.cam, s.width, s.height, xy[0], xy[1], hit, only_brushes);
				check(compare_images(base, img, nullptr).max > 0 && moved.moved, "the frame with the moved brush model differs from the plain one");
				std::printf("  moved brush model: centre at pixel %.0f,%.0f, picked %s %d\n", xy[0], xy[1], again ? kind_name(hit.object.kind) : "nothing", again ? hit.object.id : -1);
				r.reset_transform(brush.ref());
				frame_of(s.frame, img);
				check(compare_images(base, img, nullptr).max == 0, "after reset_transform of the brush model the frame is identical to the plain one");
			}
		}

		// ---- 6. moving a static model and an entity model
		const sample* ground = nullptr;			// a picked point on an upward facing world surface near the screen centre
		{
			float best = FLT_MAX;
			for (const sample& smp : samples)
			{
				if (smp.hit && smp.result.object.kind == object_kind::world_surface && smp.result.normal.z > 0.9f && smp.result.distance < 1500.0f && smp.result.distance > 350.0f)
				{
					const float d = std::fabs(static_cast<float>(smp.x - s.width / 2)) + 2.0f * std::fabs(static_cast<float>(smp.y - 3 * s.height / 4));
					if (d < best)
					{
						best = d;
						ground = &smp;
					}
				}
			}
		}
		if (!ground)
		{
			std::printf("no ground under this view: the move / add / mesh steps are skipped ( choose another --pos / --spawn )\n");
			failures++;
		}
		else
		{
			std::printf("ground point: pixel %d,%d at %.1f,%.1f,%.1f ( surface %d, %s )\n", ground->x, ground->y, ground->result.point.x, ground->result.point.y,
				ground->result.point.z, ground->result.object.id, ground->result.material.c_str());
			float3 origin, forward;
			codol::view::screen_ray(s.cam, s.width, s.height, static_cast<float>(s.width) * 0.5f, static_cast<float>(s.height) * 0.5f, origin, forward);
			const float flat_length = std::max(std::sqrt(forward.x * forward.x + forward.y * forward.y), 1.0e-3f);
			const float3 flat_forward{ forward.x / flat_length, forward.y / flat_length, 0 };
			const float3 side{ -flat_forward.y, flat_forward.x, 0 };		// to the left of the view
			const float3 spot = ground->result.point;

			object_ref moved_static{ object_kind::static_model, -1 }, moved_entity{ object_kind::entity_model, -1 };
			for (const sample& smp : samples)
			{
				if (smp.hit && smp.result.object.kind == object_kind::static_model && moved_static.id < 0)
				{
					codol::view::scene_object info;
					if (r.object(smp.result.object, info) && std::max({ info.half_size.x, info.half_size.y, info.half_size.z }) < 200.0f)
					{
						moved_static = smp.result.object;
					}
				}
			}
			float nearest = FLT_MAX;
			for (const auto& object : r.objects())
			{
				if (object.kind == object_kind::entity_model && distance_between(object.center, s.cam.origin) < nearest
					&& std::max({ object.half_size.x, object.half_size.y, object.half_size.z }) < 300.0f)
				{
					nearest = distance_between(object.center, s.cam.origin);
					moved_entity = object.ref();
				}
			}
			codol::view::transform before_static, before_entity, t;
			tools::image img;
			if (moved_static.id >= 0 && r.get_transform(moved_static, before_static))
			{
				codol::view::scene_object info;
				r.object(moved_static, info);
				t = before_static;
				t.origin = add(spot, scaled(side, 260.0f));
				t.angles.y += 45.0f;
				check(r.set_transform(moved_static, t), "set_transform of static model " + std::to_string(moved_static.id) + " ( " + info.name + " )");
				codol::view::transform now;
				r.get_transform(moved_static, now);
				check(distance_between(now.origin, t.origin) < 0.01f && std::fabs(now.scale - t.scale) < 1.0e-4f, "get_transform returns the override");
			}
			else
			{
				std::printf("no small static model was picked in this view\n");
			}
			if (moved_entity.id >= 0 && r.get_transform(moved_entity, before_entity))
			{
				codol::view::scene_object info;
				r.object(moved_entity, info);
				t = before_entity;
				t.origin = add(spot, scaled(side, -260.0f));
				t.angles.y += 30.0f;
				check(r.set_transform(moved_entity, t), "set_transform of entity model " + std::to_string(moved_entity.id) + " ( " + info.name + " )");
			}
			else
			{
				std::printf("the map has no small entity model\n");
			}
			frame_of(s.frame, img);
			save("edit_3_moved.png", img);
			check(compare_images(base, img, nullptr).max > 0, "the frame with moved models differs from the plain one");
			for (const object_ref& ref : { moved_static, moved_entity })
			{
				codol::view::scene_object info;
				if (ref.id < 0 || !r.object(ref, info))
				{
					continue;
				}
				// a ray at the middle of the moved model's bounds hits it ( or something of it is in front )
				float xy[2] = {}, depth = 0;
				codol::view::pick_result hit;
				const bool projected = codol::view::project(s.cam, s.width, s.height, info.center, xy, depth);
				const bool picked = projected && r.pick(s.cam, s.width, s.height, xy[0], xy[1], hit, { .world = false });
				std::printf("  moved %s %d ( %s ): centre at pixel %.0f,%.0f, picked %s %d at distance %.1f; moved = %d\n", kind_name(ref.kind), ref.id, info.name.c_str(),
					xy[0], xy[1], picked ? kind_name(hit.object.kind) : "nothing", picked ? hit.object.id : -1, picked ? hit.distance : 0.0f, info.moved ? 1 : 0);
				check(info.moved, std::string("scene_object::moved is set for the moved ") + kind_name(ref.kind));
			}
			check(r.stats().moved_objects == (moved_static.id >= 0 ? 1 : 0) + (moved_entity.id >= 0 ? 1 : 0), "statistics::moved_objects");
			if (moved_static.id >= 0)
			{
				r.reset_transform(moved_static);
			}
			if (moved_entity.id >= 0)
			{
				r.reset_transform(moved_entity);
			}
			frame_of(s.frame, img);
			const difference restored = compare_images(base, img, nullptr);
			std::printf("      after reset_transform: largest difference to the plain frame %d\n", restored.max);
			check(restored.max == 0,"after reset_transform the frame is identical to the plain one ( placement and lighting restored )");

			// ---- 7. new model instances
			const auto& model_names = r.model_names();
			const auto& material_names = r.material_names();
			std::printf("catalog: %zu models, %zu materials\n", model_names.size(), material_names.size());
			std::string map_model, library_model;
			{
				codol::view::scene_object info;
				if (moved_static.id >= 0 && r.object(moved_static, info))
				{
					map_model = info.name;
				}
				const auto wanted = args.value("model");
				// a model no object of the map uses: from the library ( another zone )
				std::map<std::string, bool> used;
				for (const auto& object : r.objects())
				{
					if (object.kind == object_kind::static_model || object.kind == object_kind::entity_model)
					{
						used[object.name] = true;
					}
				}
				for (const std::string& name : model_names)
				{
					if (wanted ? name == *wanted : (!used.count(name) && (name.find("vehicle") != std::string::npos || name.find("com_") == 0)))
					{
						float3 mins, maxs;
						if (r.model_bounds(name, mins, maxs) && maxs.z - mins.z > 30.0f && maxs.z - mins.z < 250.0f && maxs.x - mins.x < 400.0f)
						{
							library_model = name;
							break;
						}
					}
				}
			}
			int handles[2] = {};
			const std::string names[2] = { map_model, library_model };
			for (int i = 0; i < 2; i++)
			{
				if (names[i].empty())
				{
					std::printf("no %s model to place\n", i == 0 ? "map" : "library");
					continue;
				}
				float3 mins, maxs;
				const bool bounds = r.model_bounds(names[i], mins, maxs);
				codol::view::transform placement;
				placement.origin = add(add(spot, scaled(side, i == 0 ? 130.0f : -130.0f)), scaled(flat_forward, 160.0f));
				placement.origin.z -= bounds ? std::min(mins.z, 0.0f) : 0.0f;
				placement.angles = { 0, i == 0 ? 20.0f : 200.0f, 0 };
				const auto started = std::chrono::steady_clock::now();
				handles[i] = r.add_model(names[i], placement, &error);
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
				char text[320];
				std::snprintf(text, sizeof text, "add_model( %s ) -> handle %d in %.0f ms; bounds %.0f,%.0f,%.0f .. %.0f,%.0f,%.0f%s%s", names[i].c_str(), handles[i], ms,
					mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z, handles[i] ? "" : ": ", handles[i] ? "" : error.c_str());
				check(handles[i] > 0 && bounds, text);
			}
			frame_of(s.frame, img);
			save("edit_4_added.png", img);
			check(r.stats().placed_models == (handles[0] ? 1 : 0) + (handles[1] ? 1 : 0), "the placed models were drawn ( statistics::placed_models )");
			for (const int handle : handles)
			{
				codol::view::scene_object info;
				if (!handle || !r.object({ object_kind::placed_model, handle }, info))
				{
					continue;
				}
				float xy[2] = {}, depth = 0;
				codol::view::pick_result hit;
				const bool picked = codol::view::project(s.cam, s.width, s.height, info.center, xy, depth) && r.pick(s.cam, s.width, s.height, xy[0], xy[1], hit);
				std::printf("  placed model %d ( %s ): centre at pixel %.0f,%.0f, picked %s %d ( %s )\n", handle, info.name.c_str(), xy[0], xy[1],
					picked ? kind_name(hit.object.kind) : "nothing", picked ? hit.object.id : -1, picked ? hit.material.c_str() : "");
			}
			if (handles[1])
			{
				// move the placed model, then remove the other
				codol::view::transform placement;
				r.get_transform({ object_kind::placed_model, handles[1] }, placement);
				placement.origin.z += 40.0f;
				placement.scale = 1.5f;
				check(r.set_transform({ object_kind::placed_model, handles[1] }, placement), "set_transform of a placed model ( raised, scale 1.5 )");
			}
			if (handles[0])
			{
				check(r.remove_model(handles[0]) && !r.remove_model(handles[0]), "remove_model");
			}

			// ---- 8. dynamic meshes
			codol::view::material_info info;
			// two solid materials made for brushes that the view shows: a wall first
			std::string wall_material = ground->result.material, second_material;
			for (const bool wall : { true, false })
			{
				for (const sample& smp : samples)
				{
					if (smp.hit && smp.result.object.kind == object_kind::world_surface && (!wall || std::fabs(smp.result.normal.z) < 0.3f)
						&& r.material_details(smp.result.material, info) && info.world_material && !info.blends && !info.alpha_tested && info.writes_depth)
					{
						if (wall)
						{
							wall_material = smp.result.material;
							break;
						}
						if (smp.result.material != wall_material)
						{
							second_material = smp.result.material;
							break;
						}
					}
				}
			}
			const std::string world_material = args.value("material").value_or(wall_material);
			const bool known = r.material_details(world_material, info);
			std::printf("material %s: techset %s, colour map %s %dx%d, surface type bits 0x%08X, game flags 0x%02X, sort key %d, camera region %d, world material %d, "
				"blends %d, alpha tested %d, writes depth %d, zone %s\n", world_material.c_str(), info.technique_set.c_str(), info.color_map.c_str(), info.width, info.height,
				info.surface_type_bits, info.game_flags, info.sort_key, info.camera_region, info.world_material ? 1 : 0, info.blends ? 1 : 0, info.alpha_tested ? 1 : 0,
				info.writes_depth ? 1 : 0, info.zone.c_str());
			check(known && info.width > 0 && info.height > 0, "material_details gives the colour map and its size");
			std::vector<codol::view::mesh_vertex> vertices;
			std::vector<std::uint32_t> indices;
			const float u_scale = info.width > 0 ? 4.0f / static_cast<float>(info.width) : 1.0f / 128.0f;		// radiant: 0.25 units per texel
			const float v_scale = info.height > 0 ? 4.0f / static_cast<float>(info.height) : 1.0f / 128.0f;
			std::string model_material;
			if (handles[1])
			{
				codol::view::pick_result hit;
				codol::view::scene_object placed;
				float xy[2] = {}, depth = 0;
				if (r.object({ object_kind::placed_model, handles[1] }, placed) && codol::view::project(s.cam, s.width, s.height, placed.center, xy, depth)
					&& r.pick(s.cam, s.width, s.height, xy[0], xy[1], hit, { .world = false, .static_models = false, .entity_models = false, .brush_models = false }))
				{
					model_material = hit.material;
				}
			}
			struct box_spec
			{
				const char* what;
				std::string material;
				float3 offset;
				codol::view::mesh_shading expected;
			};
			const box_spec boxes[] =
			{
				{ "world material", world_material, scaled(side, 50.0f), codol::view::mesh_shading::game_material },
				{ "another world material", second_material, add(scaled(side, 150.0f), scaled(flat_forward, -90.0f)), codol::view::mesh_shading::game_material },
				{ "missing material", "no_such_material_xyz", scaled(side, -50.0f), codol::view::mesh_shading::flat_color },
				{ "model material", model_material, add(scaled(side, 0.0f), scaled(flat_forward, -110.0f)), codol::view::mesh_shading::none },
				{ "tool material ( never drawn by the game )", args.value("tool-material").value_or("clip"), add(scaled(side, -150.0f), scaled(flat_forward, -90.0f)),
					codol::view::mesh_shading::none },
			};
			std::vector<int> mesh_handles;
			for (const box_spec& box : boxes)
			{
				if (box.material.empty())
				{
					continue;
				}
				const float3 half{ 36, 36, 36 };
				make_box(add(add(spot, box.offset), float3{ 0, 0, half.z }), half, u_scale, v_scale, vertices, indices);
				codol::view::mesh_data data;
				data.vertices = vertices.data();
				data.vertex_count = vertices.size();
				data.indices = indices.data();
				data.index_count = indices.size();
				data.material = box.material;
				data.fallback_color = 0xFFE08020;
				const int handle = r.create_mesh(data, &error);
				const auto shading = r.mesh_shading_of(handle);
				static const char* const SHADING[] = { "none", "game_material", "color_map", "flat_color" };
				check(handle > 0 && (box.expected == codol::view::mesh_shading::none || shading == box.expected),
					std::string("create_mesh, ") + box.what + " '" + box.material + "': drawn as " + SHADING[static_cast<int>(shading)]);
				if (handle)
				{
					mesh_handles.push_back(handle);
				}
			}
			frame_of(s.frame, img);
			save("edit_5_meshes.png", img);
			check(r.stats().meshes == static_cast<int>(mesh_handles.size()), "the meshes were drawn ( statistics::meshes )");
			for (const int handle : mesh_handles)
			{
				codol::view::scene_object mesh_info;
				r.object({ object_kind::dynamic_mesh, handle }, mesh_info);
				float xy[2] = {}, depth = 0;
				codol::view::pick_result hit;
				// the top face, seen from the camera
				const float3 top = add(mesh_info.center, float3{ 0, 0, mesh_info.half_size.z });
				const bool picked = codol::view::project(s.cam, s.width, s.height, top, xy, depth) && r.pick(s.cam, s.width, s.height, xy[0], xy[1], hit);
				std::printf("  mesh %d ( %s ): top at pixel %.0f,%.0f, picked %s %d, normal %.2f,%.2f,%.2f, point %.1f,%.1f,%.1f\n", handle, mesh_info.name.c_str(), xy[0], xy[1],
					picked ? kind_name(hit.object.kind) : "nothing", picked ? hit.object.id : -1, hit.normal.x, hit.normal.y, hit.normal.z, hit.point.x, hit.point.y, hit.point.z);
			}
			if (!mesh_handles.empty())
			{
				// update: the first box becomes a slab; hide the second
				make_box(add(add(spot, boxes[0].offset), float3{ 0, 0, 60 }), { 20, 70, 60 }, u_scale, v_scale, vertices, indices);
				codol::view::mesh_data data;
				data.vertices = vertices.data();
				data.vertex_count = vertices.size();
				data.indices = indices.data();
				data.index_count = indices.size();
				data.material = world_material;
				check(r.update_mesh(mesh_handles[0], data, &error), "update_mesh");
				if (mesh_handles.size() > 1)
				{
					check(r.set_mesh_visible(mesh_handles[1], false) && r.is_hidden({ object_kind::dynamic_mesh, mesh_handles[1] }), "set_mesh_visible( false )");
				}
				frame_of(s.frame, img);
				save("edit_6_mesh_updated.png", img);
				if (mesh_handles.size() > 1)
				{
					r.set_mesh_visible(mesh_handles[1], true);
				}
			}

			// ---- 9. overlay: outlines of the edited objects, the triangles of one, axes, thumbnails
			{
				std::vector<IDirect3DTexture9*> thumbnails;
				std::vector<std::string> thumbnail_names;
				const auto started = std::chrono::steady_clock::now();
				for (std::size_t i = 0; i < material_names.size() && thumbnails.size() < 12; i += std::max<std::size_t>(1, material_names.size() / 97))
				{
					if (IDirect3DTexture9* texture = r.material_thumbnail(material_names[i], 128))
					{
						thumbnails.push_back(texture);
						thumbnail_names.push_back(material_names[i]);
					}
				}
				if (IDirect3DTexture9* texture = r.material_thumbnail(world_material, 128))
				{
					thumbnails.insert(thumbnails.begin(), texture);
					thumbnail_names.insert(thumbnail_names.begin(), world_material);
				}
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
				std::printf("thumbnails: %zu loaded in %.0f ms:", thumbnails.size(), ms);
				for (std::size_t i = 0; i < thumbnails.size(); i++)
				{
					D3DSURFACE_DESC desc{};
					thumbnails[i]->GetLevelDesc(0, &desc);
					std::printf(" %s %ux%u", thumbnail_names[i].c_str(), desc.Width, desc.Height);
				}
				std::printf("\n");
				check(!thumbnails.empty() && r.material_thumbnail(thumbnail_names[0], 128) == thumbnails[0], "material_thumbnail loads and caches");

				frame_options frame = s.frame;
				frame.overlay = [&](const codol::view::overlay_context& context)
				{
					IDirect3DDevice9* dev = context.device;
					std::vector<line_vertex> lines;
					codol::view::oriented_box box;
					for (const int handle : mesh_handles)
					{
						if (r.object_box({ object_kind::dynamic_mesh, handle }, box))
						{
							push_box(lines, box, 0xFFFFFF00);
						}
					}
					if (handles[1] && r.object_box({ object_kind::placed_model, handles[1] }, box))
					{
						push_box(lines, box, 0xFF00FFFF);
					}
					if (moved_static.id >= 0 && r.object_box(moved_static, box))
					{
						push_box(lines, box, 0xFF00FF00);
					}
					if (moved_entity.id >= 0 && r.object_box(moved_entity, box))
					{
						push_box(lines, box, 0xFFFF8000);
					}
					// the triangles of the placed model and of the ground surface as wire
					std::vector<float3> triangles;
					if (handles[1])
					{
						r.object_triangles({ object_kind::placed_model, handles[1] }, triangles);
					}
					const std::size_t model_points = triangles.size();
					r.object_triangles(ground->result.object, triangles);
					for (std::size_t i = 0; i + 2 < triangles.size(); i += 3)
					{
						const DWORD color = i < model_points ? 0xFFFFFFFF : 0xFFFF4040;
						push_line(lines, triangles[i], triangles[i + 1], color);
						push_line(lines, triangles[i + 1], triangles[i + 2], color);
						push_line(lines, triangles[i + 2], triangles[i], color);
					}
					push_line(lines, spot, add(spot, float3{ 100, 0, 0 }), 0xFFFF0000);
					push_line(lines, spot, add(spot, float3{ 0, 100, 0 }), 0xFF00FF00);
					push_line(lines, spot, add(spot, float3{ 0, 0, 100 }), 0xFF0000FF);
					// depth tested, pulled towards the eye a little so that lines on a surface do not z-fight
					const float bias = -0.00002f;
					DWORD bias_bits;
					std::memcpy(&bias_bits, &bias, 4);
					dev->SetRenderState(D3DRS_DEPTHBIAS, bias_bits);
					dev->DrawPrimitiveUP(D3DPT_LINELIST, static_cast<UINT>(lines.size() / 2), lines.data(), sizeof(line_vertex));
					// a translucent flat triangle fan over the ground point, depth tested
					dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
					dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
					dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
					dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
					const line_vertex quad[] =
					{
						{ spot.x - 50, spot.y - 50, spot.z + 30, 0x8000A0FF }, { spot.x + 50, spot.y - 50, spot.z + 30, 0x8000A0FF },
						{ spot.x + 50, spot.y + 50, spot.z + 30, 0x8000A0FF }, { spot.x - 50, spot.y + 50, spot.z + 30, 0x8000A0FF },
					};
					dev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, quad, sizeof(line_vertex));
					// the thumbnails along the top edge, in pixels
					struct screen_vertex
					{
						float x, y, z, rhw;
						float u, v;
					};
					dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
					dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
					dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
					dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
					dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
					dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
					dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
					dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
					dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
					for (std::size_t i = 0; i < thumbnails.size(); i++)
					{
						const float x0 = 8.0f + 72.0f * static_cast<float>(i) - 0.5f, y0 = 8.0f - 0.5f, size = 64.0f;
						const screen_vertex v[] = { { x0, y0, 0, 1, 0, 0 }, { x0 + size, y0, 0, 1, 1, 0 }, { x0 + size, y0 + size, 0, 1, 1, 1 }, { x0, y0 + size, 0, 1, 0, 1 } };
						dev->SetTexture(0, thumbnails[i]);
						dev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, v, sizeof(screen_vertex));
					}
					// leave a mess behind: render() has to restore the host's state all the same
					dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
					dev->SetRenderState(D3DRS_FOGENABLE, TRUE);
					dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_ADD);
				};
				if (moved_static.id >= 0)
				{
					codol::view::transform placement = before_static;
					placement.origin = add(spot, scaled(side, 260.0f));
					r.set_transform(moved_static, placement);
				}
				if (moved_entity.id >= 0)
				{
					codol::view::transform placement = before_entity;
					placement.origin = add(spot, scaled(side, -260.0f));
					r.set_transform(moved_entity, placement);
				}
				host_objects objects;
				make_dirty(device, objects, !args.flag("ex"));
				const device_state before = capture(device);
				IDirect3DTexture9* texture = r.render(s.cam, s.width, s.height, frame);
				const device_state after = capture(device);
				check(count_differences(before, after) == 0, "device state after render() with an overlay hook that changes states equals the state before");
				check(read_back(device, texture, img, error), "frame with the overlay");
				save("edit_7_overlay.png", img);
				// the dirty state would make every later frame of this test draw into the host's little target
				device->SetRenderTarget(0, nullptr);
				IDirect3DSurface9* back = nullptr;
				if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)))
				{
					device->SetRenderTarget(0, back);
					back->Release();
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
				device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
				objects.release();
			}

			// ---- 10. a device reset keeps the edits ( with the default pool the map is loaded again )
			{
				tools::image before_reset, after_reset;
				frame_of(s.frame, before_reset);
				save("edit_8_before_reset.png", before_reset);
				const auto stats_before = r.stats();
				r.on_device_lost();
				const HRESULT reset = device->Reset(&s.host.parameters);
				check(SUCCEEDED(reset) && r.on_device_reset(), "device reset");
				frame_of(s.frame, after_reset);
				save("edit_8_after_reset.png", after_reset);
				const difference d = compare_images(before_reset, after_reset, nullptr);
				const auto stats_after = r.stats();
				char text[240];
				std::snprintf(text, sizeof text, "the frame after the reset is identical ( largest difference %d ); %d placed models, %d meshes, %d moved, %d hidden before, "
					"%d / %d / %d / %d after", d.max, stats_before.placed_model_count, stats_before.mesh_count, stats_before.moved_objects, stats_before.hidden_objects,
					stats_after.placed_model_count, stats_after.mesh_count, stats_after.moved_objects, stats_after.hidden_objects);
				check(d.max == 0 && stats_before.placed_model_count == stats_after.placed_model_count && stats_before.mesh_count == stats_after.mesh_count
					&& stats_before.moved_objects == stats_after.moved_objects, text);
			}

			// ---- 11. everything undone: the plain frame again
			for (const int handle : mesh_handles)
			{
				r.destroy_mesh(handle);
			}
			if (handles[1])
			{
				r.remove_model(handles[1]);
			}
			if (moved_static.id >= 0)
			{
				r.reset_transform(moved_static);
			}
			if (moved_entity.id >= 0)
			{
				r.reset_transform(moved_entity);
			}
			r.show_all();
			frame_of(s.frame, img);
			const difference undone = compare_images(base, img, nullptr);
			std::printf("      after undoing every edit: largest difference to the plain frame %d\n", undone.max);
			check(undone.max == 0, "with every edit undone the frame is identical to the plain one");
		}

		{
			const auto st = r.stats();
			std::printf("end: %d distinct models, %d materials, %d textures, GPU geometry %.1f MB, textures %.1f MB\n", st.distinct_models, st.materials, st.textures,
				static_cast<double>(st.geometry_bytes) / 1048576.0, static_cast<double>(st.texture_bytes) / 1048576.0);
			for (const std::string& warning : r.warnings())
			{
				if (warning.find("mesh") != std::string::npos || warning.find("lighting volume") != std::string::npos || warning.find("extra zone") != std::string::npos)
				{
					std::printf("  warning: %s\n", warning.c_str());
				}
			}
		}
		std::printf("%s: %d failure(s)\n", failures == 0 ? "edit test passed" : "EDIT TEST FAILED", failures);
		print_memory();
		return failures == 0 ? 0 : 1;
	}

	// ---- pickbench ------------------------------------------------------------------------------------------

	int run_pickbench(const arguments& args)
	{
		session s;
		if (!open_session(args, s, true))
		{
			return 1;
		}
		const auto st = s.renderer.stats();
		std::printf("pick geometry: %lld triangles, %.1f MB, built in %.0f ms; load %.0f ms\n", static_cast<long long>(st.pick_triangles),
			static_cast<double>(st.pick_bytes) / 1048576.0, st.pick_build_ms, st.load_ms);
		// rays from the camera through every 8th pixel, then rays from above the map straight down and along the map
		std::vector<double> times;
		int hits = 0;
		codol::view::pick_result result;
		const auto timed = [&](const float3& origin, const float3& direction)
		{
			const auto started = std::chrono::steady_clock::now();
			hits += s.renderer.pick_ray(origin, direction, result) ? 1 : 0;
			times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
		};
		for (int y = 4; y < s.height; y += 8)
		{
			for (int x = 4; x < s.width; x += 8)
			{
				float3 origin, direction;
				codol::view::screen_ray(s.cam, s.width, s.height, static_cast<float>(x), static_cast<float>(y), origin, direction);
				timed(origin, direction);
			}
		}
		const std::size_t camera_rays = times.size();
		float3 center, half;
		s.renderer.world_bounds(center, half);
		for (int i = 0; i < 64; i++)
		{
			for (int k = 0; k < 64; k++)
			{
				const float fx = (static_cast<float>(i) + 0.5f) / 32.0f - 1.0f, fy = (static_cast<float>(k) + 0.5f) / 32.0f - 1.0f;
				timed({ center.x + fx * half.x, center.y + fy * half.y, center.z + half.z + 10.0f }, { 0, 0, -1 });
				timed({ center.x - half.x - 10.0f, center.y + fx * half.y, center.z + fy * half.z }, { 1, 0.01f * fx, 0.01f * fy });
			}
		}
		const auto summary = [&](const std::size_t begin, const std::size_t end, const char* what)
		{
			std::vector<double> part(times.begin() + static_cast<std::ptrdiff_t>(begin), times.begin() + static_cast<std::ptrdiff_t>(end));
			double sum = 0;
			for (const double v : part)
			{
				sum += v;
			}
			std::sort(part.begin(), part.end());
			std::printf("%s: %zu rays, mean %.4f ms, median %.4f ms, 99 %% %.4f ms, worst %.4f ms\n", what, part.size(), sum / static_cast<double>(part.size()),
				part[part.size() / 2], part[part.size() * 99 / 100], part.back());
		};
		summary(0, camera_rays, "rays through the pixels of the view");
		summary(camera_rays, times.size(), "rays down onto and across the whole map");
		std::printf("%d of %zu rays hit something\n", hits, times.size());
		print_memory();
		return 0;
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
	if (args.command == "edittest")
	{
		return run_edittest(args);
	}
	if (args.command == "pickbench")
	{
		return run_pickbench(args);
	}
	std::fprintf(stderr,
		"codol_viewtool render    --game <install> --map <name> --out <png> [--size WxH] [--pos x,y,z --angles pitch,yaw[,roll] | --spawn n] [--fov deg]\n"
		"                         [--no-models] [--no-entity-models] [--no-brush-models] [--no-world] [--no-shadows] [--no-cell-culling] [--wireframe] [--lods]\n"
		"                         [--ex] [--pool managed|default] [--skip-mips n] [--time s] [--stats] [--verbose]\n"
		"codol_viewtool bench     --game <install> --map <name> [--frames n] [view and options as above]\n"
		"codol_viewtool statetest --game <install> --map <name> [--out <folder>] [view and options as above]\n"
		"codol_viewtool compare   <oracle.png> <ours.png> [--out <side by side png: oracle | ours | 4 x difference>]\n"
		"codol_viewtool edittest  --game <install> --map <name> [--out <folder>] [view and options as above] [--model <xmodel>] [--material <name>]\n"
		"                         picking, hiding, moving, placing models, dynamic meshes, overlay, thumbnails: checks and edit_*.png\n"
		"codol_viewtool pickbench --game <install> --map <name> [view as above]      time of map_renderer::pick_ray\n"
		"                         --keep-edit-data ( render / bench / statetest with the pick geometry and the light grid kept )  --extra-zones a,b\n");
	return 2;
}
