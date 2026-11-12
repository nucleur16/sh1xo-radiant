// d3d9.dll for the test client's folder ( the link based shadow copy, never the install ): passes everything on to
// the system's d3d9.dll and, when asked through a file, writes the back buffer of the frame that is about to be
// presented to a .bmp. For taking pictures of the game on a machine whose display is off: Windows then drops the
// frames of a windowed Direct3D 9 program, so a screen copy of its window is black, while the game still renders.
//
//   <exe dir>\codol_shot.req   one line: the path of the .bmp to write. Looked at before every Present ( at most 20
//                              times a second ); the picture is written to <path>.tmp, renamed, the request deleted.
//
// It changes nothing the game draws. Interception: the IDirect3D9 and the device the game gets have a private copy of
// their vtable in which CreateDevice / Present point here ( nothing in d3d9.dll or the game is patched ).
//
//   build ( x86 ):  cl /nologo /O2 /LD /MT d3d9_shot.cpp /Fe:d3d9.dll /link /DEF:d3d9_shot.def

#include <windows.h>
#include <d3d9.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
	HMODULE g_real;
	char g_request[MAX_PATH];

	typedef HRESULT(WINAPI* create_device_fn)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
	typedef HRESULT(WINAPI* present_fn)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
	create_device_fn g_create_device;
	present_fn g_present;

	HMODULE real()
	{
		if (!g_real)
		{
			char path[MAX_PATH];
			const UINT n = GetSystemDirectoryA(path, MAX_PATH - 16);
			std::strcpy(path + n, "\\d3d9.dll");
			g_real = LoadLibraryA(path);
			const DWORD m = GetModuleFileNameA(nullptr, g_request, MAX_PATH - 20);
			DWORD at = m;
			while (at && g_request[at - 1] != '\\')
			{
				at--;
			}
			std::strcpy(g_request + at, "codol_shot.req");
		}
		return g_real;
	}

	// The object gets its own copy of the vtable; returns the copy. d3d9's classes have virtual functions of their own
	// behind those of the interface and call them through the same table, so far more than the interface's entries are
	// copied ( as many as are readable ).
	void** private_vtable(void* object)
	{
		void*** slot = static_cast<void***>(object);
		SIZE_T entries = 512;
		MEMORY_BASIC_INFORMATION info{};
		if (VirtualQuery(*slot, &info, sizeof(info)))
		{
			const SIZE_T left = (static_cast<const char*>(info.BaseAddress) + info.RegionSize - reinterpret_cast<const char*>(*slot)) / sizeof(void*);
			entries = left < entries ? left : entries;
		}
		void** copy = static_cast<void**>(HeapAlloc(GetProcessHeap(), 0, sizeof(void*) * entries));
		std::memcpy(copy, *slot, sizeof(void*) * entries);
		*slot = copy;
		return copy;
	}

	bool write_bmp(const char* path, const D3DLOCKED_RECT& lock, const UINT width, const UINT height)
	{
		char tmp[MAX_PATH + 8];
		std::snprintf(tmp, sizeof(tmp), "%s.tmp", path);
		FILE* f = std::fopen(tmp, "wb");
		if (!f)
		{
			return false;
		}
		const UINT row = (width * 3 + 3) & ~3u;
		BITMAPFILEHEADER fh{};
		BITMAPINFOHEADER ih{};
		fh.bfType = 0x4D42;
		fh.bfOffBits = sizeof(fh) + sizeof(ih);
		fh.bfSize = fh.bfOffBits + row * height;
		ih.biSize = sizeof(ih);
		ih.biWidth = static_cast<LONG>(width);
		ih.biHeight = static_cast<LONG>(height);
		ih.biPlanes = 1;
		ih.biBitCount = 24;
		std::fwrite(&fh, sizeof(fh), 1, f);
		std::fwrite(&ih, sizeof(ih), 1, f);
		unsigned char* line = static_cast<unsigned char*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, row));
		for (UINT y = 0; y < height; y++)
		{
			const unsigned char* src = static_cast<const unsigned char*>(lock.pBits) + static_cast<SIZE_T>(lock.Pitch) * (height - 1 - y);
			for (UINT x = 0; x < width; x++)
			{
				line[x * 3 + 0] = src[x * 4 + 0];
				line[x * 3 + 1] = src[x * 4 + 1];
				line[x * 3 + 2] = src[x * 4 + 2];
			}
			std::fwrite(line, row, 1, f);
		}
		HeapFree(GetProcessHeap(), 0, line);
		std::fclose(f);
		DeleteFileA(path);
		return MoveFileA(tmp, path) != 0;
	}

	void capture(IDirect3DDevice9* dev, const char* path)
	{
		IDirect3DSurface9* back = nullptr;
		if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) || !back)
		{
			return;
		}
		D3DSURFACE_DESC desc{};
		back->GetDesc(&desc);
		IDirect3DSurface9* resolved = nullptr;
		IDirect3DSurface9* memory = nullptr;
		// the back buffer is multisampled as a rule: resolve it into a plain render target first
		if (SUCCEEDED(dev->CreateRenderTarget(desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &resolved, nullptr)) &&
			SUCCEEDED(dev->StretchRect(back, nullptr, resolved, nullptr, D3DTEXF_NONE)) &&
			SUCCEEDED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &memory, nullptr)) &&
			SUCCEEDED(dev->GetRenderTargetData(resolved, memory)))
		{
			D3DLOCKED_RECT lock{};
			if ((desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8) && SUCCEEDED(memory->LockRect(&lock, nullptr, D3DLOCK_READONLY)))
			{
				write_bmp(path, lock, desc.Width, desc.Height);
				memory->UnlockRect();
			}
		}
		if (memory)
		{
			memory->Release();
		}
		if (resolved)
		{
			resolved->Release();
		}
		back->Release();
	}

	// codol_shot.log next to the request file: device creation, and every 5 seconds how many frames were presented
	// and what TestCooperativeLevel says ( a lost device renders nothing )
	void log_line(const char* format, ...)
	{
		char path[MAX_PATH];
		std::strcpy(path, g_request);
		std::strcpy(path + std::strlen(path) - 3, "log");
		if (FILE* f = std::fopen(path, "ab"))
		{
			va_list args;
			va_start(args, format);
			std::vfprintf(f, format, args);
			va_end(args);
			std::fputc(10, f);
			std::fclose(f);
		}
	}

	typedef HRESULT(WINAPI* device_fn)(IDirect3DDevice9*);
	typedef HRESULT(WINAPI* chain_present_fn)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
	device_fn g_test_level, g_end_scene;
	chain_present_fn g_chain_present;
	volatile LONG g_presents, g_chain_presents, g_end_scenes, g_tests, g_last_test, g_in_present;

	// before a frame is presented: the log line every 5 seconds, a pending request
	void before_present(IDirect3DDevice9* dev)
	{
		static DWORD last, last_log;
		const DWORD now = GetTickCount();
		if (now - last_log >= 5000)
		{
			last_log = now;
			log_line("%lu: Present %ld, swap chain Present %ld, EndScene %ld, TestCooperativeLevel %ld calls, last 0x%08lX", now, g_presents, g_chain_presents, g_end_scenes,
				g_tests, static_cast<unsigned long>(g_last_test));
		}
		if (now - last < 50)
		{
			return;
		}
		last = now;
		if (GetFileAttributesA(g_request) == INVALID_FILE_ATTRIBUTES)
		{
			return;
		}
		char path[MAX_PATH] = {};
		if (FILE* f = std::fopen(g_request, "rb"))
		{
			const size_t got = std::fread(path, 1, sizeof(path) - 1, f);
			std::fclose(f);
			path[got] = 0;
			for (char* c = path; *c; c++)
			{
				if (*c == '\r' || *c == '\n')
				{
					*c = 0;
					break;
				}
			}
			DeleteFileA(g_request);
			if (path[0])
			{
				capture(dev, path);
			}
		}
	}

	HRESULT WINAPI hook_present(IDirect3DDevice9* dev, const RECT* a, const RECT* b, HWND window, const RGNDATA* region)
	{
		InterlockedIncrement(&g_presents);
		before_present(dev);
		InterlockedIncrement(&g_in_present);		// d3d9 may present through the swap chain's table
		const HRESULT hr = g_present(dev, a, b, window, region);
		InterlockedDecrement(&g_in_present);
		return hr;
	}

	HRESULT WINAPI hook_chain_present(IDirect3DSwapChain9* chain, const RECT* a, const RECT* b, HWND window, const RGNDATA* region, DWORD flags)
	{
		if (!g_in_present)
		{
			InterlockedIncrement(&g_chain_presents);
			IDirect3DDevice9* dev = nullptr;
			if (SUCCEEDED(chain->GetDevice(&dev)) && dev)
			{
				before_present(dev);
				dev->Release();
			}
		}
		return g_chain_present(chain, a, b, window, region, flags);
	}

	HRESULT WINAPI hook_end_scene(IDirect3DDevice9* dev)
	{
		InterlockedIncrement(&g_end_scenes);
		return g_end_scene(dev);
	}

	HRESULT WINAPI hook_test_level(IDirect3DDevice9* dev)
	{
		const HRESULT hr = g_test_level(dev);
		InterlockedIncrement(&g_tests);
		g_last_test = static_cast<LONG>(hr);
		return hr;
	}

	HRESULT WINAPI hook_create_device(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out)
	{
		const HRESULT hr = g_create_device(d3d, adapter, type, window, flags, pp, out);
		log_line("CreateDevice: hr 0x%08lX, flags 0x%lX, %ux%u, windowed %d, multisample %d", static_cast<unsigned long>(hr), flags, pp ? pp->BackBufferWidth : 0,
			pp ? pp->BackBufferHeight : 0, pp ? pp->Windowed : -1, pp ? static_cast<int>(pp->MultiSampleType) : -1);
		if (SUCCEEDED(hr) && out && *out)
		{
			void** vt = private_vtable(*out);
			g_test_level = static_cast<device_fn>(vt[3]);
			vt[3] = static_cast<void*>(hook_test_level);
			g_present = static_cast<present_fn>(vt[17]);
			vt[17] = static_cast<void*>(hook_present);
			g_end_scene = static_cast<device_fn>(vt[42]);
			vt[42] = static_cast<void*>(hook_end_scene);
			IDirect3DSwapChain9* chain = nullptr;
			if (SUCCEEDED((*out)->GetSwapChain(0, &chain)) && chain)
			{
				void** cvt = private_vtable(chain);
				g_chain_present = static_cast<chain_present_fn>(cvt[3]);
				cvt[3] = static_cast<void*>(hook_chain_present);
				chain->Release();
			}
		}
		return hr;
	}
}

extern "C" IDirect3D9* WINAPI shot_Direct3DCreate9(UINT sdk)
{
	typedef IDirect3D9*(WINAPI* fn)(UINT);
	const fn create = real() ? reinterpret_cast<fn>(GetProcAddress(g_real, "Direct3DCreate9")) : nullptr;
	IDirect3D9* d3d = create ? create(sdk) : nullptr;
	if (d3d)
	{
		void** vt = private_vtable(d3d);
		g_create_device = static_cast<create_device_fn>(vt[16]);
		vt[16] = static_cast<void*>(hook_create_device);
	}
	return d3d;
}

extern "C" HRESULT WINAPI shot_Direct3DCreate9Ex(UINT sdk, IDirect3D9Ex** out)
{
	typedef HRESULT(WINAPI* fn)(UINT, IDirect3D9Ex**);
	const fn create = real() ? reinterpret_cast<fn>(GetProcAddress(g_real, "Direct3DCreate9Ex")) : nullptr;
	return create ? create(sdk, out) : E_FAIL;
}
