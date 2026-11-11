// zlib ( RFC 1950 / 1951 ) compressor: LZ77 over a 32 KiB window with hash chains and one step of lazy matching,
// dynamic Huffman blocks. Written for the fast file writer; the game inflates the result with stock zlib.

#include "codol_zone/fastfile.hpp"

#include <algorithm>
#include <cstring>

namespace codol::zone
{
	namespace
	{
		constexpr std::uint32_t WINDOW = 32768;
		constexpr std::uint32_t HASH_BITS = 16;
		constexpr std::uint32_t MIN_MATCH = 3;
		constexpr std::uint32_t MAX_MATCH = 258;
		constexpr std::uint32_t BLOCK_TOKENS = 1u << 16;
		constexpr std::uint32_t NO_POS = 0xFFFFFFFFu;

		constexpr std::uint16_t LENGTH_BASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
		constexpr std::uint8_t LENGTH_EXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
		constexpr std::uint16_t DIST_BASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
			8193, 12289, 16385, 24577 };
		constexpr std::uint8_t DIST_EXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
		constexpr std::uint8_t CODE_LENGTH_ORDER[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

		struct BitWriter
		{
			std::vector<std::uint8_t>& out;
			std::uint64_t acc = 0;
			int bits = 0;

			void put(const std::uint32_t value, const int count)
			{
				acc |= static_cast<std::uint64_t>(value) << bits;
				bits += count;
				while (bits >= 8)
				{
					out.push_back(static_cast<std::uint8_t>(acc));
					acc >>= 8;
					bits -= 8;
				}
			}

			void align()
			{
				if (bits > 0)
				{
					out.push_back(static_cast<std::uint8_t>(acc));
					acc = 0;
					bits = 0;
				}
			}
		};

		// code lengths of a Huffman code over `count` symbols, no length above `limit`
		void huffman_lengths(const std::uint32_t* frequency, const int count, const int limit, std::uint8_t* lengths)
		{
			std::uint32_t freq[288];
			for (int i = 0; i < count; i++)
			{
				freq[i] = frequency[i];
			}
			for (;;)
			{
				// nodes: leaves first, then internal nodes
				std::uint64_t weight[2 * 288];
				int parent[2 * 288];
				int alive[288];
				int aliveCount = 0;
				int nodes = 0;
				int leafOf[288];
				for (int i = 0; i < count; i++)
				{
					lengths[i] = 0;
					if (freq[i])
					{
						weight[nodes] = freq[i];
						parent[nodes] = -1;
						leafOf[nodes] = i;
						alive[aliveCount++] = nodes++;
					}
				}
				const int leaves = nodes;
				if (leaves == 0)
				{
					return;
				}
				if (leaves == 1)
				{
					lengths[leafOf[0]] = 1;
					return;
				}
				while (aliveCount > 1)
				{
					// the two lightest nodes
					int a = 0, b = 1;
					if (weight[alive[b]] < weight[alive[a]])
					{
						std::swap(a, b);
					}
					for (int i = 2; i < aliveCount; i++)
					{
						if (weight[alive[i]] < weight[alive[a]])
						{
							b = a;
							a = i;
						}
						else if (weight[alive[i]] < weight[alive[b]])
						{
							b = i;
						}
					}
					weight[nodes] = weight[alive[a]] + weight[alive[b]];
					parent[nodes] = -1;
					parent[alive[a]] = nodes;
					parent[alive[b]] = nodes;
					alive[std::min(a, b)] = nodes++;
					alive[std::max(a, b)] = alive[--aliveCount];
				}
				int longest = 0;
				for (int i = 0; i < leaves; i++)
				{
					int depth = 0;
					for (int n = i; parent[n] >= 0; n = parent[n])
					{
						depth++;
					}
					lengths[leafOf[i]] = static_cast<std::uint8_t>(std::min(depth, 255));
					longest = std::max(longest, depth);
				}
				if (longest <= limit)
				{
					return;
				}
				// flatten the distribution and try again
				for (int i = 0; i < count; i++)
				{
					if (freq[i])
					{
						freq[i] = (freq[i] + 1) / 2 + 1;
					}
				}
			}
		}

		// canonical codes, bit reversed ( deflate sends Huffman codes most significant bit first )
		void huffman_codes(const std::uint8_t* lengths, const int count, std::uint16_t* codes)
		{
			int perLength[16] = {};
			for (int i = 0; i < count; i++)
			{
				perLength[lengths[i]]++;
			}
			perLength[0] = 0;
			std::uint32_t next[16] = {};
			std::uint32_t code = 0;
			for (int bits = 1; bits < 16; bits++)
			{
				code = (code + static_cast<std::uint32_t>(perLength[bits - 1])) << 1;
				next[bits] = code;
			}
			for (int i = 0; i < count; i++)
			{
				const int len = lengths[i];
				std::uint32_t c = len ? next[len]++ : 0;
				std::uint32_t reversed = 0;
				for (int b = 0; b < len; b++)
				{
					reversed = (reversed << 1) | (c & 1);
					c >>= 1;
				}
				codes[i] = static_cast<std::uint16_t>(reversed);
			}
		}

		struct Tables
		{
			std::uint8_t lengthSymbol[MAX_MATCH + 1];
			std::uint8_t distSymbol[512];		// distances 1 .. 256 directly, above that by ( distance - 1 ) >> 7

			Tables()
			{
				for (int s = 0; s < 29; s++)
				{
					const int top = s == 28 ? 258 : LENGTH_BASE[s] + (1 << LENGTH_EXTRA[s]) - 1;
					for (int l = LENGTH_BASE[s]; l <= top && l <= 258; l++)
					{
						lengthSymbol[l] = static_cast<std::uint8_t>(s);
					}
				}
				lengthSymbol[258] = 28;
				for (int s = 0; s < 30; s++)
				{
					const int top = DIST_BASE[s] + (1 << DIST_EXTRA[s]) - 1;
					for (int d = DIST_BASE[s]; d <= top; d++)
					{
						if (d <= 256)
						{
							distSymbol[d - 1] = static_cast<std::uint8_t>(s);
						}
						else
						{
							distSymbol[256 + ((d - 1) >> 7)] = static_cast<std::uint8_t>(s);
						}
					}
				}
			}

			int dist_symbol(const std::uint32_t distance) const
			{
				return distance <= 256 ? distSymbol[distance - 1] : distSymbol[256 + ((distance - 1) >> 7)];
			}
		};

		const Tables& tables()
		{
			static const Tables t;
			return t;
		}

		// token: literal byte, or 0x80000000 | ( length - 3 ) << 16 | ( distance - 1 )
		void write_block(BitWriter& w, const std::vector<std::uint32_t>& tokens, const bool last)
		{
			const Tables& t = tables();
			std::uint32_t litFreq[286] = {};
			std::uint32_t distFreq[30] = {};
			for (const std::uint32_t token : tokens)
			{
				if (token & 0x80000000u)
				{
					litFreq[257 + t.lengthSymbol[((token >> 16) & 0xFF) + 3]]++;
					distFreq[t.dist_symbol((token & 0x7FFF) + 1)]++;
				}
				else
				{
					litFreq[token]++;
				}
			}
			litFreq[256] = 1;

			std::uint8_t litLen[286], distLen[30];
			std::uint16_t litCode[286], distCode[30];
			huffman_lengths(litFreq, 286, 15, litLen);
			huffman_lengths(distFreq, 30, 15, distLen);
			huffman_codes(litLen, 286, litCode);
			huffman_codes(distLen, 30, distCode);

			int hlit = 286;
			while (hlit > 257 && !litLen[hlit - 1])
			{
				hlit--;
			}
			int hdist = 30;
			while (hdist > 1 && !distLen[hdist - 1])
			{
				hdist--;
			}

			// run length code the two length tables as one sequence
			std::uint8_t all[316];
			std::memcpy(all, litLen, static_cast<std::size_t>(hlit));
			std::memcpy(all + hlit, distLen, static_cast<std::size_t>(hdist));
			const int total = hlit + hdist;
			std::uint8_t symbols[316], extra[316];
			int symbolCount = 0;
			std::uint32_t clFreq[19] = {};
			for (int i = 0; i < total;)
			{
				const std::uint8_t value = all[i];
				int run = 1;
				while (i + run < total && all[i + run] == value)
				{
					run++;
				}
				i += run;
				if (value == 0)
				{
					while (run >= 3)
					{
						const int n = std::min(run, 138);
						symbols[symbolCount] = n >= 11 ? 18 : 17;
						extra[symbolCount++] = static_cast<std::uint8_t>(n >= 11 ? n - 11 : n - 3);
						clFreq[n >= 11 ? 18 : 17]++;
						run -= n;
					}
				}
				else
				{
					symbols[symbolCount] = value;
					extra[symbolCount++] = 0;
					clFreq[value]++;
					run--;
					while (run >= 3)
					{
						const int n = std::min(run, 6);
						symbols[symbolCount] = 16;
						extra[symbolCount++] = static_cast<std::uint8_t>(n - 3);
						clFreq[16]++;
						run -= n;
					}
				}
				while (run-- > 0)
				{
					symbols[symbolCount] = value;
					extra[symbolCount++] = 0;
					clFreq[value]++;
				}
			}

			std::uint8_t clLen[19];
			std::uint16_t clCode[19];
			huffman_lengths(clFreq, 19, 7, clLen);
			huffman_codes(clLen, 19, clCode);
			int hclen = 19;
			while (hclen > 4 && !clLen[CODE_LENGTH_ORDER[hclen - 1]])
			{
				hclen--;
			}

			w.put(last ? 1u : 0u, 1);
			w.put(2, 2);
			w.put(static_cast<std::uint32_t>(hlit - 257), 5);
			w.put(static_cast<std::uint32_t>(hdist - 1), 5);
			w.put(static_cast<std::uint32_t>(hclen - 4), 4);
			for (int i = 0; i < hclen; i++)
			{
				w.put(clLen[CODE_LENGTH_ORDER[i]], 3);
			}
			for (int i = 0; i < symbolCount; i++)
			{
				const int s = symbols[i];
				w.put(clCode[s], clLen[s]);
				if (s == 16)
				{
					w.put(extra[i], 2);
				}
				else if (s == 17)
				{
					w.put(extra[i], 3);
				}
				else if (s == 18)
				{
					w.put(extra[i], 7);
				}
			}

			for (const std::uint32_t token : tokens)
			{
				if (token & 0x80000000u)
				{
					const std::uint32_t length = ((token >> 16) & 0xFF) + 3;
					const std::uint32_t distance = (token & 0x7FFF) + 1;
					const int ls = t.lengthSymbol[length];
					w.put(litCode[257 + ls], litLen[257 + ls]);
					if (LENGTH_EXTRA[ls])
					{
						w.put(length - LENGTH_BASE[ls], LENGTH_EXTRA[ls]);
					}
					const int ds = t.dist_symbol(distance);
					w.put(distCode[ds], distLen[ds]);
					if (DIST_EXTRA[ds])
					{
						w.put(distance - DIST_BASE[ds], DIST_EXTRA[ds]);
					}
				}
				else
				{
					w.put(litCode[token], litLen[token]);
				}
			}
			w.put(litCode[256], litLen[256]);
		}

		std::uint32_t adler32(const std::span<const std::uint8_t> data)
		{
			std::uint32_t a = 1, b = 0;
			std::size_t i = 0;
			while (i < data.size())
			{
				const std::size_t n = std::min<std::size_t>(data.size() - i, 5552);
				for (std::size_t k = 0; k < n; k++)
				{
					a += data[i + k];
					b += a;
				}
				a %= 65521;
				b %= 65521;
				i += n;
			}
			return (b << 16) | a;
		}

		inline std::uint32_t hash3(const std::uint8_t* p)
		{
			const std::uint32_t v = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16);
			return (v * 0x9E3779B1u) >> (32 - HASH_BITS);
		}

		void deflate_stored(const std::span<const std::uint8_t> data, std::vector<std::uint8_t>& out)
		{
			std::size_t i = 0;
			do
			{
				const std::size_t n = std::min<std::size_t>(data.size() - i, 0xFFFF);
				out.push_back(i + n == data.size() ? 1 : 0);
				out.push_back(static_cast<std::uint8_t>(n));
				out.push_back(static_cast<std::uint8_t>(n >> 8));
				out.push_back(static_cast<std::uint8_t>(~n));
				out.push_back(static_cast<std::uint8_t>(~n >> 8));
				out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(i), data.begin() + static_cast<std::ptrdiff_t>(i + n));
				i += n;
			} while (i < data.size());
		}

		void deflate_compressed(const std::span<const std::uint8_t> data, std::vector<std::uint8_t>& out, const int level)
		{
			const std::uint32_t maxChain = level >= 9 ? 256u : level >= 6 ? 32u : level >= 3 ? 8u : 2u;
			const std::uint32_t niceLength = level >= 6 ? 128u : 32u;
			const bool lazy = level >= 4;

			const std::uint8_t* const p = data.data();
			const std::size_t size = data.size();
			std::vector<std::uint32_t> head(1u << HASH_BITS, NO_POS);
			std::vector<std::uint32_t> prev(WINDOW, NO_POS);
			std::vector<std::uint32_t> tokens;
			tokens.reserve(BLOCK_TOKENS);
			BitWriter w{ out };

			const auto insert = [&](const std::size_t at)
			{
				if (at + MIN_MATCH <= size)
				{
					const std::uint32_t h = hash3(p + at);
					prev[at & (WINDOW - 1)] = head[h];
					head[h] = static_cast<std::uint32_t>(at);
				}
			};

			const auto find = [&](const std::size_t at, std::uint32_t& distance) -> std::uint32_t
			{
				if (at + MIN_MATCH > size)
				{
					return 0;
				}
				const std::uint32_t limit = static_cast<std::uint32_t>(std::min<std::size_t>(MAX_MATCH, size - at));
				std::uint32_t best = MIN_MATCH - 1;
				std::uint32_t candidate = head[hash3(p + at)];
				for (std::uint32_t chain = maxChain; chain && candidate != NO_POS; chain--)
				{
					const std::size_t gap = at - candidate;
					if (gap > WINDOW || gap == 0)
					{
						break;
					}
					const std::uint8_t* a = p + at;
					const std::uint8_t* b = p + candidate;
					if (b[best] == a[best] && b[0] == a[0])
					{
						std::uint32_t n = 0;
						while (n < limit && a[n] == b[n])
						{
							n++;
						}
						if (n > best)
						{
							best = n;
							distance = static_cast<std::uint32_t>(gap);
							if (n >= niceLength || n == limit)
							{
								break;
							}
						}
					}
					const std::uint32_t next = prev[candidate & (WINDOW - 1)];
					if (next != NO_POS && next >= candidate)
					{
						break;		// slot reused by a newer position
					}
					candidate = next;
				}
				return best >= MIN_MATCH ? best : 0;
			};

			std::size_t i = 0;
			while (i < size)
			{
				std::uint32_t distance = 0;
				std::uint32_t length = find(i, distance);
				if (length && lazy && length < niceLength && i + 1 < size)
				{
					insert(i);
					std::uint32_t distance2 = 0;
					const std::uint32_t length2 = find(i + 1, distance2);
					if (length2 > length)
					{
						tokens.push_back(p[i]);
						i++;
						length = length2;
						distance = distance2;
					}
					else
					{
						// i is already in the table
						tokens.push_back(0x80000000u | ((length - 3) << 16) | (distance - 1));
						for (std::size_t k = i + 1; k < i + length; k++)
						{
							insert(k);
						}
						i += length;
						length = 0;
						distance = 0;
						if (tokens.size() >= BLOCK_TOKENS)
						{
							write_block(w, tokens, false);
							tokens.clear();
						}
						continue;
					}
				}
				if (length)
				{
					tokens.push_back(0x80000000u | ((length - 3) << 16) | (distance - 1));
					for (std::size_t k = i; k < i + length; k++)
					{
						insert(k);
					}
					i += length;
				}
				else
				{
					tokens.push_back(p[i]);
					insert(i);
					i++;
				}
				if (tokens.size() >= BLOCK_TOKENS)
				{
					write_block(w, tokens, false);
					tokens.clear();
				}
			}
			write_block(w, tokens, true);
			w.align();
		}
	}

	Status zlib_deflate(const std::span<const std::uint8_t> data, std::vector<std::uint8_t>& out, const int level)
	try
	{
		if (data.size() > 0xFFFFFFF0ull)
		{
			return Status::error("zlib_deflate: input above 4 GiB");
		}
		out.clear();
		out.reserve(data.size() / 3 + 64);
		out.push_back(0x78);
		out.push_back(level <= 0 ? 0x01 : level >= 6 ? 0xDA : 0x9C);
		if (level <= 0)
		{
			deflate_stored(data, out);
		}
		else
		{
			deflate_compressed(data, out, level);
		}
		const std::uint32_t checksum = adler32(data);
		out.push_back(static_cast<std::uint8_t>(checksum >> 24));
		out.push_back(static_cast<std::uint8_t>(checksum >> 16));
		out.push_back(static_cast<std::uint8_t>(checksum >> 8));
		out.push_back(static_cast<std::uint8_t>(checksum));
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}
}
