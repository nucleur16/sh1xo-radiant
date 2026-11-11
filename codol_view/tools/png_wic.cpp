#include "png_wic.hpp"

#include <windows.h>
#include <objbase.h>
#include <wincodec.h>

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

		bool factory(com_ptr<IWICImagingFactory>& out, std::string& error)
		{
			CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&out))))
			{
				error = "WIC is not available";
				return false;
			}
			return true;
		}
	}

	bool save_png(const std::filesystem::path& path, const image& img, std::string& error)
	{
		com_ptr<IWICImagingFactory> wic;
		if (!factory(wic, error))
		{
			return false;
		}
		// WIC's PNG encoder takes BGRA
		std::vector<std::uint8_t> bgra(img.rgba.size());
		for (std::size_t i = 0; i + 3 < bgra.size(); i += 4)
		{
			bgra[i] = img.rgba[i + 2];
			bgra[i + 1] = img.rgba[i + 1];
			bgra[i + 2] = img.rgba[i];
			bgra[i + 3] = img.rgba[i + 3];
		}
		com_ptr<IWICStream> stream;
		com_ptr<IWICBitmapEncoder> encoder;
		com_ptr<IWICBitmapFrameEncode> frame;
		WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
		if (FAILED(wic->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))
			|| FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) || FAILED(encoder->Initialize(stream.p, WICBitmapEncoderNoCache))
			|| FAILED(encoder->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr))
			|| FAILED(frame->SetSize(static_cast<UINT>(img.width), static_cast<UINT>(img.height))) || FAILED(frame->SetPixelFormat(&format))
			|| !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)
			|| FAILED(frame->WritePixels(static_cast<UINT>(img.height), static_cast<UINT>(img.width) * 4, static_cast<UINT>(bgra.size()), bgra.data()))
			|| FAILED(frame->Commit()) || FAILED(encoder->Commit()))
		{
			error = "cannot write " + path.string();
			return false;
		}
		return true;
	}

	bool load_png(const std::filesystem::path& path, image& out, std::string& error)
	{
		com_ptr<IWICImagingFactory> wic;
		if (!factory(wic, error))
		{
			return false;
		}
		com_ptr<IWICBitmapDecoder> decoder;
		if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
		{
			error = "cannot decode " + path.string();
			return false;
		}
		com_ptr<IWICBitmapFrameDecode> frame;
		com_ptr<IWICFormatConverter> converter;
		UINT width = 0, height = 0;
		if (FAILED(decoder->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&converter))
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
