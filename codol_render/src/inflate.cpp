// Raw deflate decoder ( RFC 1951 ), written for codol_render: no third party code.
// Canonical Huffman decoding with a 10 bit lookup table and a bit by bit path for longer codes.

#include "codol_render/iwd.hpp"

#include <algorithm>

namespace codol::render
{
	namespace
	{
		constexpr int MAX_BITS = 15;
		constexpr int FAST_BITS = 10;

		constexpr std::uint16_t LENGTH_BASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
		constexpr std::uint8_t LENGTH_EXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
		constexpr std::uint16_t DIST_BASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
		constexpr std::uint8_t DIST_EXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
		constexpr std::uint8_t CODE_LENGTH_ORDER[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

		struct bit_stream
		{
			const std::uint8_t* in;
			const std::uint8_t* end;
			std::uint64_t bits = 0;
			int count = 0;
			bool overrun = false;	// a read went past the end of the input

			void refill()
			{
				while (count <= 56 && in < end)
				{
					bits |= static_cast<std::uint64_t>(*in++) << count;
					count += 8;
				}
			}

			// up to 32 bits; past the end of the input the missing bits read as 0 and `overrun` is set
			std::uint32_t take(const int n)
			{
				if (count < n)
				{
					refill();
					if (count < n)
					{
						overrun = true;
						count = n;
					}
				}
				const auto value = static_cast<std::uint32_t>(bits & ((1ull << n) - 1));
				bits >>= n;
				count -= n;
				return value;
			}
		};

		struct huffman
		{
			std::uint16_t fast[1 << FAST_BITS];		// ( symbol << 4 ) | length, 0: longer than FAST_BITS or unused
			std::uint16_t counts[MAX_BITS + 1];
			std::uint16_t symbols[288];

			// false when the lengths over-subscribe the code space
			bool build(const std::uint8_t* lengths, const int n)
			{
				std::fill(std::begin(counts), std::end(counts), std::uint16_t{ 0 });
				for (int i = 0; i < n; i++)
				{
					counts[lengths[i]]++;
				}
				counts[0] = 0;
				int left = 1;
				for (int len = 1; len <= MAX_BITS; len++)
				{
					left = (left << 1) - counts[len];
					if (left < 0)
					{
						return false;
					}
				}
				std::uint16_t offsets[MAX_BITS + 2];
				offsets[1] = 0;
				for (int len = 1; len <= MAX_BITS; len++)
				{
					offsets[len + 1] = static_cast<std::uint16_t>(offsets[len] + counts[len]);
				}
				for (int i = 0; i < n; i++)
				{
					if (lengths[i] != 0)
					{
						symbols[offsets[lengths[i]]++] = static_cast<std::uint16_t>(i);
					}
				}

				// the lookup table: every FAST_BITS pattern whose low bits are a ( bit reversed ) code of <= FAST_BITS
				std::fill(std::begin(fast), std::end(fast), std::uint16_t{ 0 });
				int code = 0, index = 0;
				for (int len = 1; len <= FAST_BITS; len++)
				{
					for (int k = 0; k < counts[len]; k++, code++, index++)
					{
						int reversed = 0;
						for (int b = 0; b < len; b++)
						{
							reversed |= ((code >> b) & 1) << (len - 1 - b);
						}
						const auto entry = static_cast<std::uint16_t>((symbols[index] << 4) | len);
						for (int fill = reversed; fill < (1 << FAST_BITS); fill += 1 << len)
						{
							fast[fill] = entry;
						}
					}
					code <<= 1;
				}
				return true;
			}

			// -1: invalid code
			int decode(bit_stream& s) const
			{
				if (s.count < MAX_BITS)
				{
					s.refill();
				}
				const std::uint16_t entry = fast[s.bits & ((1 << FAST_BITS) - 1)];
				if (entry != 0)
				{
					const int len = entry & 0xF;
					if (s.count < len)
					{
						s.overrun = true;
						s.count = len;
					}
					s.bits >>= len;
					s.count -= len;
					return entry >> 4;
				}
				// canonical decode, one bit at a time
				int code = 0, first = 0, index = 0;
				for (int len = 1; len <= MAX_BITS; len++)
				{
					code |= static_cast<int>(s.take(1));
					const int count = counts[len];
					if (code - count < first)
					{
						return symbols[index + (code - first)];
					}
					index += count;
					first += count;
					first <<= 1;
					code <<= 1;
				}
				return -1;
			}
		};

		const char* inflate_block(bit_stream& s, const huffman& literals, const huffman& distances, std::vector<std::uint8_t>& out,
			const std::size_t expected, bool& done)
		{
			for (;;)
			{
				const int symbol = literals.decode(s);
				if (symbol < 0)
				{
					return "invalid literal/length code";
				}
				if (s.overrun)
				{
					return "deflate stream ends early";
				}
				if (symbol < 256)
				{
					if (out.size() >= expected)
					{
						return "deflate stream is longer than the entry";
					}
					out.push_back(static_cast<std::uint8_t>(symbol));
					continue;
				}
				if (symbol == 256)
				{
					return nullptr;
				}
				if (symbol > 285)
				{
					return "invalid length symbol";
				}
				const std::size_t length = LENGTH_BASE[symbol - 257] + s.take(LENGTH_EXTRA[symbol - 257]);
				const int dsym = distances.decode(s);
				if (dsym < 0 || dsym > 29)
				{
					return "invalid distance code";
				}
				const std::size_t distance = DIST_BASE[dsym] + s.take(DIST_EXTRA[dsym]);
				if (s.overrun)
				{
					return "deflate stream ends early";
				}
				if (distance > out.size())
				{
					return "distance reaches before the start of the data";
				}
				if (out.size() + length > expected)
				{
					if (!done)	// `done` on entry: the caller accepts a cut ( see inflate_raw )
					{
						return "deflate stream is longer than the entry";
					}
				}
				const std::size_t take = std::min(length, expected - out.size());
				std::size_t from = out.size() - distance;
				for (std::size_t i = 0; i < take; i++)
				{
					out.push_back(out[from++]);
				}
				if (take < length)
				{
					return nullptr;
				}
			}
		}
	}

	bool inflate_raw(const byte_span compressed, const std::size_t expected_size, std::vector<std::uint8_t>& out, std::string* error,
		const bool allow_truncated_input)
	{
		try
		{
			out.clear();
			out.reserve(expected_size);
			bit_stream s{ compressed.data(), compressed.data() + compressed.size() };
			auto literals = std::make_unique<huffman>();
			auto distances = std::make_unique<huffman>();
			const auto fail = [&](const char* text)
			{
				// a cut input is fine for a prefix read once enough bytes exist
				if (allow_truncated_input && out.size() >= expected_size)
				{
					return true;
				}
				set_error(error, std::string("inflate: ") + text);
				return false;
			};

			for (;;)
			{
				if (allow_truncated_input && out.size() >= expected_size)
				{
					return true;
				}
				const std::uint32_t last = s.take(1);
				const std::uint32_t type = s.take(2);
				if (s.overrun)
				{
					return fail("deflate stream ends early");
				}
				if (type == 0)
				{
					// stored: skip to the byte boundary, LEN, NLEN
					s.bits >>= s.count & 7;
					s.count -= s.count & 7;
					const std::uint32_t len = s.take(16);
					const std::uint32_t nlen = s.take(16);
					if (s.overrun)
					{
						return fail("deflate stream ends early");
					}
					if ((len ^ 0xFFFF) != nlen)
					{
						return fail("stored block length check failed");
					}
					for (std::uint32_t i = 0; i < len; i++)
					{
						if (out.size() >= expected_size)
						{
							return allow_truncated_input ? true : fail("deflate stream is longer than the entry");
						}
						const std::uint32_t byte = s.take(8);
						if (s.overrun)
						{
							return fail("deflate stream ends early");
						}
						out.push_back(static_cast<std::uint8_t>(byte));
					}
				}
				else if (type == 1 || type == 2)
				{
					std::uint8_t lengths[320] = {};
					int nlen, ndist;
					if (type == 1)
					{
						nlen = 288;
						ndist = 30;
						std::fill(lengths, lengths + 144, std::uint8_t{ 8 });
						std::fill(lengths + 144, lengths + 256, std::uint8_t{ 9 });
						std::fill(lengths + 256, lengths + 280, std::uint8_t{ 7 });
						std::fill(lengths + 280, lengths + 288, std::uint8_t{ 8 });
						std::fill(lengths + 288, lengths + 318, std::uint8_t{ 5 });
					}
					else
					{
						nlen = static_cast<int>(s.take(5)) + 257;
						ndist = static_cast<int>(s.take(5)) + 1;
						const int ncode = static_cast<int>(s.take(4)) + 4;
						if (nlen > 286 || ndist > 30)
						{
							return fail("too many length or distance codes");
						}
						std::uint8_t code_lengths[19] = {};
						for (int i = 0; i < ncode; i++)
						{
							code_lengths[CODE_LENGTH_ORDER[i]] = static_cast<std::uint8_t>(s.take(3));
						}
						if (!literals->build(code_lengths, 19))
						{
							return fail("invalid code length code");
						}
						int index = 0;
						while (index < nlen + ndist)
						{
							const int symbol = literals->decode(s);
							if (symbol < 0 || s.overrun)
							{
								return fail(s.overrun ? "deflate stream ends early" : "invalid code length symbol");
							}
							if (symbol < 16)
							{
								lengths[index++] = static_cast<std::uint8_t>(symbol);
								continue;
							}
							std::uint8_t value = 0;
							int repeat;
							if (symbol == 16)
							{
								if (index == 0)
								{
									return fail("repeat without a previous length");
								}
								value = lengths[index - 1];
								repeat = 3 + static_cast<int>(s.take(2));
							}
							else if (symbol == 17)
							{
								repeat = 3 + static_cast<int>(s.take(3));
							}
							else
							{
								repeat = 11 + static_cast<int>(s.take(7));
							}
							if (index + repeat > nlen + ndist)
							{
								return fail("too many code lengths");
							}
							while (repeat-- > 0)
							{
								lengths[index++] = value;
							}
						}
						if (lengths[256] == 0)
						{
							return fail("no end of block code");
						}
					}
					if (!literals->build(lengths, nlen) || !distances->build(lengths + nlen, ndist))
					{
						return fail("invalid Huffman code lengths");
					}
					bool cut_ok = allow_truncated_input;
					if (const char* text = inflate_block(s, *literals, *distances, out, expected_size, cut_ok))
					{
						return fail(text);
					}
				}
				else
				{
					return fail("invalid block type");
				}
				if (last)
				{
					break;
				}
			}
			if (out.size() != expected_size)
			{
				set_error(error, "inflate: the stream holds " + std::to_string(out.size()) + " bytes, expected " + std::to_string(expected_size));
				return false;
			}
			return true;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return false;
		}
	}
}
