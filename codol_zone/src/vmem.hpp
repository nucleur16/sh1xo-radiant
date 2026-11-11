#pragma once

// Private 32 bit address space the translated zone loaders run against. Sixteen 256 MB regions, each a byte
// vector that grows on demand:
//   0        the loaders' globals ( the client's .data addresses 0x00FE'0000 .. 0x0204'0000 )
//   1 .. 8   zone memory blocks 0 .. 7
//   9        block table ( 0x9000'0000 ) and the asset pool ( 0x9001'0000 .. )
//   0xA      DB_AddXAsset entries
//   0xB      emulated stack frames
// Little endian, unaligned access. An access outside these regions throws LoadError ( caught at the API boundary ).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace codol::zone
{
	struct LoadError : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	// observer of the loaders' memory traffic ( zone capture and zone writing, zone_editor.cpp ); null for a plain load
	struct MemWatch
	{
		virtual ~MemWatch() = default;
		virtual void on_write32(std::uint32_t address, std::uint32_t value) = 0;	// before the store
		virtual void on_read32(std::uint32_t address) = 0;							// only inside the read window
	};

	class VMem
	{
	public:
		static constexpr std::uint32_t REGION_BITS = 28;
		static constexpr std::uint32_t REGION_SIZE = 1u << REGION_BITS;
		static constexpr std::uint32_t REGION_MASK = REGION_SIZE - 1;
		static constexpr std::uint32_t REGION_COUNT = 16;
		static constexpr std::uint32_t WRITABLE_REGIONS = 12;

		static constexpr std::uint32_t STACK_BASE = 0xB0000000u;
		static constexpr std::uint32_t STACK_LIMIT = 0xB8000000u;

		void reset()
		{
			for (auto& r : regions_)
			{
				std::vector<std::uint8_t>().swap(r);
			}
			stackTop_ = STACK_BASE;
		}

		// sizes a region up front ( a block gets its declared size )
		void reserve_region(const std::uint32_t region, const std::uint32_t size)
		{
			if (regions_[region].size() < size)
			{
				regions_[region].resize(size);
			}
		}

		std::uint32_t region_size(const std::uint32_t region) const
		{
			return static_cast<std::uint32_t>(regions_[region].size());
		}

		void shrink()
		{
			for (auto& r : regions_)
			{
				r.shrink_to_fit();
			}
		}

		void release_region(const std::uint32_t region)
		{
			std::vector<std::uint8_t>().swap(regions_[region]);
		}

		// writable pointer to n bytes, growing the region when needed
		std::uint8_t* at(const std::uint32_t address, const std::uint32_t n)
		{
			auto& r = regions_[address >> REGION_BITS];
			const std::size_t off = address & REGION_MASK;
			if (off + n <= r.size())
			{
				return r.data() + off;
			}
			return grow(address, n);
		}

		// read only pointer, nullptr when the range is not mapped
		const std::uint8_t* peek(const std::uint32_t address, const std::size_t n) const
		{
			const auto& r = regions_[address >> REGION_BITS];
			const std::size_t off = address & REGION_MASK;
			return off + n <= r.size() ? r.data() + off : nullptr;
		}

		std::uint8_t U8(const std::uint32_t a) { return *at(a, 1); }
		std::int8_t I8(const std::uint32_t a) { return static_cast<std::int8_t>(U8(a)); }

		std::uint16_t U16(const std::uint32_t a)
		{
			std::uint16_t v;
			std::memcpy(&v, at(a, 2), 2);
			return v;
		}
		std::int16_t I16(const std::uint32_t a) { return static_cast<std::int16_t>(U16(a)); }

		std::uint32_t U32(const std::uint32_t a)
		{
			if (watch_ && a - readWindow_ < readWindowSize_)
			{
				watch_->on_read32(a);
			}
			std::uint32_t v;
			std::memcpy(&v, at(a, 4), 4);
			return v;
		}
		std::int32_t I32(const std::uint32_t a) { return static_cast<std::int32_t>(U32(a)); }

		std::uint8_t WU8(const std::uint32_t a, const std::uint8_t v)
		{
			*at(a, 1) = v;
			return v;
		}
		std::int8_t WI8(const std::uint32_t a, const std::int8_t v)
		{
			WU8(a, static_cast<std::uint8_t>(v));
			return v;
		}
		std::uint16_t WU16(const std::uint32_t a, const std::uint16_t v)
		{
			std::memcpy(at(a, 2), &v, 2);
			return v;
		}
		std::uint32_t WU32(const std::uint32_t a, const std::uint32_t v)
		{
			if (watch_)
			{
				watch_->on_write32(a, v);
			}
			std::memcpy(at(a, 4), &v, 4);
			return v;
		}
		std::int32_t WI32(const std::uint32_t a, const std::int32_t v)
		{
			WU32(a, static_cast<std::uint32_t>(v));
			return v;
		}

		std::int32_t PreAddI32(const std::uint32_t a, const std::int32_t d)
		{
			return WI32(a, static_cast<std::int32_t>(U32(a) + static_cast<std::uint32_t>(d)));
		}

		void write(const std::uint32_t address, const std::uint8_t* data, const std::uint32_t n)
		{
			if (n)
			{
				std::memcpy(at(address, n), data, n);
			}
		}

		std::uint32_t Memcpy(const std::uint32_t destination, const std::uint32_t source, const std::uint32_t n)
		{
			if (n)
			{
				// map both ranges first: growing one region may move the other only when they are the same region
				at(destination, n);
				const std::uint8_t* src = at(source, n);
				std::memmove(at(destination, n), src, n);
			}
			return destination;
		}

		std::uint32_t Memset(const std::uint32_t destination, const std::uint32_t value, const std::uint32_t n)
		{
			if (n)
			{
				std::memset(at(destination, n), static_cast<int>(value & 0xFF), n);
			}
			return destination;
		}

		std::uint32_t StrLen(const std::uint32_t address)
		{
			std::uint32_t n = 0;
			while (U8(address + n) != 0)
			{
				n++;
			}
			return n;
		}

		void set_watch(MemWatch* watch) { watch_ = watch; }
		void set_read_window(const std::uint32_t address, const std::uint32_t size)
		{
			readWindow_ = address;
			readWindowSize_ = size;
		}

		std::uint32_t StackEnter(const std::uint32_t frameSize)
		{
			const std::uint32_t frame = stackTop_;
			stackTop_ += (frameSize + 3) & ~3u;
			if (stackTop_ >= STACK_LIMIT)
			{
				throw LoadError("emulated loader stack overflow");
			}
			Memset(frame, 0, frameSize);
			return frame;
		}

		void StackLeave(const std::uint32_t frameSize)
		{
			stackTop_ -= (frameSize + 3) & ~3u;
		}

	private:
		std::uint8_t* grow(const std::uint32_t address, const std::uint32_t n)
		{
			const std::uint32_t region = address >> REGION_BITS;
			const std::size_t need = static_cast<std::size_t>(address & REGION_MASK) + n;
			if (region >= WRITABLE_REGIONS || need > REGION_SIZE)
			{
				char text[96];
				std::snprintf(text, sizeof(text), "loader touched unmapped address 0x%08X ( %u bytes )", address, n);
				throw LoadError(text);
			}
			auto& r = regions_[region];
			std::size_t size = r.size() + (r.size() >> 1);
			if (size < need + 0x10000)
			{
				size = need + 0x10000;
			}
			if (size > REGION_SIZE)
			{
				size = REGION_SIZE;
			}
			r.resize(size);
			return r.data() + (address & REGION_MASK);
		}

		std::vector<std::uint8_t> regions_[REGION_COUNT];
		std::uint32_t stackTop_ = STACK_BASE;
		MemWatch* watch_ = nullptr;
		std::uint32_t readWindow_ = 0;
		std::uint32_t readWindowSize_ = 0;
	};

	struct StackGuard
	{
		VMem& m;
		std::uint32_t size;
		~StackGuard() { m.StackLeave(size); }
	};
}
