// zlib ( RFC 1950 ) / deflate ( RFC 1951 ) decoder, written for codol_zone. No third party code.

#include "codol_zone/fastfile.hpp"
#include "crypto_win.hpp"

#include <cstring>

namespace codol::zone
{
	namespace
	{
		constexpr int FAST_BITS = 10;
		constexpr int MAX_BITS = 15;

		struct Huffman
		{
			std::uint16_t fast[1 << FAST_BITS];	// ( symbol << 4 ) | length, 0: code is longer than FAST_BITS
			std::uint16_t count[MAX_BITS + 1];
			std::uint16_t symbol[288];
		};

		// canonical huffman code from code lengths; false when the lengths over-subscribe the code space
		bool build(Huffman& h, const std::uint8_t* lengths, const int n)
		{
			std::memset(h.fast, 0, sizeof(h.fast));
			std::memset(h.count, 0, sizeof(h.count));
			for (int i = 0; i < n; i++)
			{
				h.count[lengths[i]]++;
			}
			h.count[0] = 0;

			int left = 1;
			for (int len = 1; len <= MAX_BITS; len++)
			{
				left <<= 1;
				left -= h.count[len];
				if (left < 0)
				{
					return false;
				}
			}

			std::uint16_t offs[MAX_BITS + 2];
			offs[1] = 0;
			for (int len = 1; len <= MAX_BITS; len++)
			{
				offs[len + 1] = static_cast<std::uint16_t>(offs[len] + h.count[len]);
			}

			std::uint16_t next[MAX_BITS + 1];
			{
				std::uint32_t code = 0;
				for (int len = 1; len <= MAX_BITS; len++)
				{
					code = (code + h.count[len - 1]) << 1;
					next[len] = static_cast<std::uint16_t>(code);
				}
			}

			for (int i = 0; i < n; i++)
			{
				const int len = lengths[i];
				if (!len)
				{
					continue;
				}
				h.symbol[offs[len]++] = static_cast<std::uint16_t>(i);

				const std::uint32_t code = next[len]++;
				if (len <= FAST_BITS)
				{
					// the stream stores codes most significant bit first, the bit reader is LSB first
					std::uint32_t rev = 0;
					for (int b = 0; b < len; b++)
					{
						rev |= ((code >> b) & 1u) << (len - 1 - b);
					}
					const std::uint16_t entry = static_cast<std::uint16_t>((i << 4) | len);
					for (std::uint32_t fill = rev; fill < (1u << FAST_BITS); fill += (1u << len))
					{
						h.fast[fill] = entry;
					}
				}
			}
			return true;
		}

		struct BitReader
		{
			const std::uint8_t* p;
			const std::uint8_t* end;
			std::uint64_t buf = 0;
			int bits = 0;
			bool overrun = false;

			void refill()
			{
				while (bits <= 56)
				{
					if (p < end)
					{
						buf |= static_cast<std::uint64_t>(*p++) << bits;
					}
					else
					{
						// zero bits stand in for missing input; a valid stream never needs more than one refill of
						// them because the adler32 trailer follows the deflate data
						bits_past_end_ += 8;
						if (bits_past_end_ > 64)
						{
							overrun = true;
						}
					}
					bits += 8;
				}
			}

			std::uint32_t get(const int n)
			{
				if (bits < n)
				{
					refill();
				}
				const std::uint32_t v = static_cast<std::uint32_t>(buf & ((1ull << n) - 1));
				buf >>= n;
				bits -= n;
				return v;
			}

			// number of whole input bytes that were pulled into the buffer but not used
			std::size_t unused_bytes() const
			{
				const int real = bits - bits_past_end_;
				return real > 0 ? static_cast<std::size_t>(real / 8) : 0;
			}

			int bits_past_end_ = 0;
		};

		int decode(BitReader& br, const Huffman& h)
		{
			if (br.bits < MAX_BITS)
			{
				br.refill();
			}
			const std::uint16_t e = h.fast[br.buf & ((1u << FAST_BITS) - 1)];
			if (e)
			{
				br.buf >>= (e & 15);
				br.bits -= (e & 15);
				return e >> 4;
			}

			int code = 0, first = 0, index = 0;
			std::uint64_t b = br.buf;
			for (int len = 1; len <= MAX_BITS; len++)
			{
				code |= static_cast<int>(b & 1);
				b >>= 1;
				const int count = h.count[len];
				if (code - count < first)
				{
					br.buf >>= len;
					br.bits -= len;
					return h.symbol[index + (code - first)];
				}
				index += count;
				first += count;
				first <<= 1;
				code <<= 1;
			}
			return -1;
		}

		constexpr std::uint16_t LENGTH_BASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
		constexpr std::uint8_t LENGTH_EXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
		constexpr std::uint16_t DIST_BASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
		constexpr std::uint8_t DIST_EXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

		std::uint32_t adler32(const std::uint8_t* data, std::size_t len)
		{
			std::uint32_t a = 1, b = 0;
			while (len)
			{
				std::size_t n = len < 5552 ? len : 5552;
				len -= n;
				while (n--)
				{
					a += *data++;
					b += a;
				}
				a %= 65521;
				b %= 65521;
			}
			return (b << 16) | a;
		}
	}

	namespace
	{
		Status inflate_impl(const std::span<const std::uint8_t> compressed, std::vector<std::uint8_t>& out, const std::size_t sizeHint,
			std::size_t* consumed, const bool zoneHeader)
		{
			out.clear();
			if (compressed.size() < 2)
			{
				return Status::error("zlib stream is shorter than its header");
			}
			const std::uint32_t cmf = compressed[0], flg = compressed[1];
			if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 || ((cmf << 8) | flg) % 31 != 0 || (flg & 0x20))
			{
				return Status::error("not a zlib stream ( bad CMF / FLG header )");
			}

			BitReader br{ compressed.data() + 2, compressed.data() + compressed.size() };

			// a zone starts with XFile.size: begin small and size the buffer exactly once those four bytes are out
			std::size_t cap = zoneHeader ? 0x10000 : sizeHint ? sizeHint : compressed.size() * 4 + 4096;
			out.resize(cap);
			std::size_t pos = 0;
			bool probed = !zoneHeader;

			const auto ensure = [&](const std::size_t extra)
			{
				if (pos + extra <= cap)
				{
					return;
				}
				if (!probed && pos >= 4)
				{
					probed = true;
					std::uint32_t size;
					std::memcpy(&size, out.data(), 4);
					const std::size_t declared = static_cast<std::size_t>(size) + 40 + 512;
					if (declared > cap)
					{
						cap = declared;
					}
				}
				if (pos + extra > cap)
				{
					cap = (cap + extra) + (cap >> 1);
				}
				out.resize(cap);
			};

			// heap: the tables are ~4 KB each and this may run on a small thread stack
			struct Tables
			{
				Huffman litlen, dist;
				std::uint8_t lengths[320];
			};
			const auto tables = std::make_unique<Tables>();
			Huffman& litlen = tables->litlen;
			Huffman& dist = tables->dist;
			std::uint8_t* lengths = tables->lengths;

			bool last = false;
			while (!last)
			{
				last = br.get(1) != 0;
				const std::uint32_t type = br.get(2);

				if (type == 0)
				{
					// stored: drop the bits up to the byte boundary, then LEN / NLEN
					const int drop = br.bits & 7;
					br.buf >>= drop;
					br.bits -= drop;
					const std::uint32_t len = br.get(16);
					const std::uint32_t nlen = br.get(16);
					if ((len ^ 0xFFFFu) != nlen)
					{
						return Status::error("deflate: stored block length check failed");
					}
					ensure(len);
					std::uint32_t left = len;
					while (left && br.bits >= 8)
					{
						out[pos++] = static_cast<std::uint8_t>(br.buf & 0xFF);
						br.buf >>= 8;
						br.bits -= 8;
						left--;
					}
					if (left)
					{
						if (static_cast<std::size_t>(br.end - br.p) < left)
						{
							return Status::error("deflate: stored block runs past the end of the input");
						}
						std::memcpy(out.data() + pos, br.p, left);
						br.p += left;
						pos += left;
					}
					continue;
				}

				if (type == 1)
				{
					int i = 0;
					for (; i < 144; i++) lengths[i] = 8;
					for (; i < 256; i++) lengths[i] = 9;
					for (; i < 280; i++) lengths[i] = 7;
					for (; i < 288; i++) lengths[i] = 8;
					build(litlen, lengths, 288);
					for (i = 0; i < 30; i++) lengths[i] = 5;
					build(dist, lengths, 30);
				}
				else if (type == 2)
				{
					const int nlen = static_cast<int>(br.get(5)) + 257;
					const int ndist = static_cast<int>(br.get(5)) + 1;
					const int ncode = static_cast<int>(br.get(4)) + 4;
					if (nlen > 286 || ndist > 30)
					{
						return Status::error("deflate: bad dynamic block counts");
					}
					static constexpr std::uint8_t ORDER[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
					std::memset(lengths, 0, 19);
					for (int i = 0; i < ncode; i++)
					{
						lengths[ORDER[i]] = static_cast<std::uint8_t>(br.get(3));
					}
					if (!build(litlen, lengths, 19))
					{
						return Status::error("deflate: bad code length code");
					}

					int index = 0;
					while (index < nlen + ndist)
					{
						const int sym = decode(br, litlen);
						if (sym < 0)
						{
							return Status::error("deflate: bad code length symbol");
						}
						if (sym < 16)
						{
							lengths[index++] = static_cast<std::uint8_t>(sym);
							continue;
						}
						std::uint8_t value = 0;
						int repeat;
						if (sym == 16)
						{
							if (!index)
							{
								return Status::error("deflate: repeat without a previous length");
							}
							value = lengths[index - 1];
							repeat = 3 + static_cast<int>(br.get(2));
						}
						else if (sym == 17)
						{
							repeat = 3 + static_cast<int>(br.get(3));
						}
						else
						{
							repeat = 11 + static_cast<int>(br.get(7));
						}
						if (index + repeat > nlen + ndist)
						{
							return Status::error("deflate: code lengths overflow");
						}
						while (repeat--)
						{
							lengths[index++] = value;
						}
					}
					if (!lengths[256])
					{
						return Status::error("deflate: block has no end code");
					}
					std::uint8_t distLengths[32];
					std::memcpy(distLengths, lengths + nlen, static_cast<std::size_t>(ndist));
					if (!build(litlen, lengths, nlen) || !build(dist, distLengths, ndist))
					{
						return Status::error("deflate: over-subscribed huffman code");
					}
				}
				else
				{
					return Status::error("deflate: reserved block type");
				}

				for (;;)
				{
					if (br.overrun)
					{
						return Status::error("deflate: stream is truncated");
					}
					int sym = decode(br, litlen);
					if (sym < 0)
					{
						return Status::error("deflate: bad literal / length code");
					}
					if (sym < 256)
					{
						if (pos >= cap)
						{
							ensure(1);
						}
						out[pos++] = static_cast<std::uint8_t>(sym);
						continue;
					}
					if (sym == 256)
					{
						break;
					}
					sym -= 257;
					if (sym >= 29)
					{
						return Status::error("deflate: bad length symbol");
					}
					const std::uint32_t len = LENGTH_BASE[sym] + br.get(LENGTH_EXTRA[sym]);

					const int dsym = decode(br, dist);
					if (dsym < 0 || dsym >= 30)
					{
						return Status::error("deflate: bad distance code");
					}
					const std::size_t distance = DIST_BASE[dsym] + br.get(DIST_EXTRA[dsym]);
					if (distance > pos)
					{
						return Status::error("deflate: distance reaches before the start of the output");
					}

					ensure(len);
					std::uint8_t* dst = out.data() + pos;
					const std::uint8_t* src = dst - distance;
					if (distance >= len)
					{
						std::memcpy(dst, src, len);
					}
					else
					{
						for (std::uint32_t i = 0; i < len; i++)
						{
							dst[i] = src[i];
						}
					}
					pos += len;
				}
			}

			if (br.overrun)
			{
				return Status::error("deflate: stream is truncated");
			}
			out.resize(pos);

			// trailer: adler32 of the output, big endian, byte aligned
			const int drop = br.bits & 7;
			br.buf >>= drop;
			br.bits -= drop;
			const std::size_t unused = br.unused_bytes();
			const std::size_t used = static_cast<std::size_t>(br.p - compressed.data()) - unused;
			if (compressed.size() - used < 4)
			{
				return Status::error("zlib stream has no adler32 trailer");
			}
			const std::uint8_t* t = compressed.data() + used;
			const std::uint32_t expected = (static_cast<std::uint32_t>(t[0]) << 24) | (static_cast<std::uint32_t>(t[1]) << 16)
				| (static_cast<std::uint32_t>(t[2]) << 8) | t[3];
			if (adler32(out.data(), out.size()) != expected)
			{
				return Status::error("zlib adler32 mismatch: the inflated data is corrupt");
			}
			if (consumed)
			{
				*consumed = used + 4;
			}
			return {};
		}
	}

	Status zlib_inflate(const std::span<const std::uint8_t> compressed, std::vector<std::uint8_t>& out, const std::size_t sizeHint,
		std::size_t* consumed)
	{
		try
		{
			return inflate_impl(compressed, out, sizeHint, consumed, false);
		}
		catch (const std::exception& e)
		{
			out.clear();
			return Status::error(std::string("inflate failed: ") + e.what());
		}
	}

	namespace detail
	{
		Status zlib_inflate_zone(const std::span<const std::uint8_t> compressed, std::vector<std::uint8_t>& zone)
		{
			return inflate_impl(compressed, zone, 0, nullptr, true);
		}
	}
}
