#pragma once

// Shared plumbing of codol_render: byte access, the zone view the caller supplies, error reporting.
//
// The library never sees native pointers into a zone. Every zone pointer is the 32 bit value the caller's zone
// loader hands out ( an address in its memory image, a block offset - whatever it uses ), and the caller supplies
// the two callbacks of `zone_view` that turn such a value into bytes. A null pointer, a pointer the loader did
// not resolve ( e.g. a still encoded 0xFFFFFFFF / 0xFFFFFFFE ) or a range that is not loaded must come back as
// an empty span / false; that is the C# `GameMemory.IsDataPointer` / `IsLoadedRange` / `String() == null`.
//
// No function of the library throws; failures come back as false / std::nullopt with a text in `*error`.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace codol::render
{
	using byte_span = std::span<const std::uint8_t>;

	struct zone_view
	{
		// the bytes [ptr, ptr + size), or an empty span when the range is not loaded ( size > 0 )
		std::function<byte_span(std::uint32_t ptr, std::uint32_t size)> read;
		// the zero terminated string at ptr; false when ptr is null or not loaded
		std::function<bool(std::uint32_t ptr, std::string& out)> string;
	};

	// A zone_view over one flat buffer: pointer p addresses data[p - base]. Null and out of range pointers are
	// "not loaded". Used by the tests and by callers that keep a zone as one memory image.
	zone_view flat_zone_view(byte_span data, std::uint32_t base);

	inline std::uint16_t rd_u16(const std::uint8_t* p)
	{
		return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
	}

	inline std::uint32_t rd_u32(const std::uint8_t* p)
	{
		return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
			| (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
	}

	inline std::int16_t rd_s16(const std::uint8_t* p) { return static_cast<std::int16_t>(rd_u16(p)); }
	inline std::int32_t rd_s32(const std::uint8_t* p) { return static_cast<std::int32_t>(rd_u32(p)); }

	inline float rd_f32(const std::uint8_t* p)
	{
		const std::uint32_t bits = rd_u32(p);
		float f;
		std::memcpy(&f, &bits, sizeof f);
		return f;
	}

	inline std::uint32_t float_bits(const float f)
	{
		std::uint32_t bits;
		std::memcpy(&bits, &f, sizeof bits);
		return bits;
	}

	inline void set_error(std::string* error, std::string text)
	{
		if (error)
		{
			*error = std::move(text);
		}
	}

	// "0x%08X"
	std::string hex32(std::uint32_t value);

	// A name of the form ",name" refers to an asset another zone defines ( MaterialExtractor.cs:149 TrimReference )
	inline bool is_reference_name(const std::string_view name) { return !name.empty() && name.front() == ','; }
	inline std::string_view trim_reference(const std::string_view name) { return is_reference_name(name) ? name.substr(1) : name; }
}
