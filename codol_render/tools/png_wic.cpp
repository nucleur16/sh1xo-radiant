#include "tools.hpp"

#include <windows.h>
#include <objbase.h>
#include <wincodec.h>

#include <cstdio>

namespace tools
{
	namespace
	{
		template <typename T>
		struct com_ptr
		{
			T* p = nullptr;
			~com_ptr() { if (p) { p->Release(); } }
			T** operator&() { return &p; }
			T* operator->() const { return p; }
		};
	}

	bool read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& out)
	{
		std::FILE* file = _wfopen(path.c_str(), L"rb");
		if (!file)
		{
			return false;
		}
		bool ok = _fseeki64(file, 0, SEEK_END) == 0;
		const long long size = ok ? _ftelli64(file) : -1;
		ok = ok && size >= 0 && _fseeki64(file, 0, SEEK_SET) == 0;
		if (ok)
		{
			out.resize(static_cast<std::size_t>(size));
			ok = std::fread(out.data(), 1, out.size(), file) == out.size();
		}
		std::fclose(file);
		return ok;
	}

	bool load_png(const std::filesystem::path& path, png_image& out, std::string& error)
	{
		static const bool initialised = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
		(void)initialised;

		com_ptr<IWICImagingFactory> factory;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
		{
			error = "WIC is not available";
			return false;
		}
		com_ptr<IWICBitmapDecoder> decoder;
		if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
		{
			error = "cannot decode " + path.string();
			return false;
		}
		com_ptr<IWICBitmapFrameDecode> frame;
		com_ptr<IWICFormatConverter> converter;
		UINT width = 0, height = 0;
		if (FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter))
			|| FAILED(converter->Initialize(frame.p, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))
			|| FAILED(converter->GetSize(&width, &height)))
		{
			error = "cannot convert " + path.string();
			return false;
		}
		out.width = static_cast<int>(width);
		out.height = static_cast<int>(height);
		out.rgba.resize(static_cast<std::size_t>(width) * height * 4);
		if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(out.rgba.size()), out.rgba.data())))
		{
			error = "cannot read the pixels of " + path.string();
			return false;
		}
		return true;
	}
}
