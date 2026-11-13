#include "codol_render/texture.hpp"

#include <algorithm>
#include <cstdio>

namespace codol::render
{
	namespace
	{
		constexpr std::size_t TABLE_BYTES = wavelet_tables::ENTRIES * 4;

		// WaveletDecoder.cs:136-154 ( ReadTable )
		bool read_table(const byte_span bytes, std::vector<std::int16_t>& table, const char* which, std::string* error)
		{
			if (bytes.size() < TABLE_BYTES)
			{
				set_error(error, std::string("the ") + which + " wavelet table is truncated");
				return false;
			}
			table.resize(wavelet_tables::ENTRIES * 2);
			for (std::size_t i = 0; i < table.size(); i++)
			{
				table[i] = rd_s16(bytes.data() + 2 * i);
			}
			for (int i = 0; i < wavelet_tables::ENTRIES; i++)
			{
				const int bits = static_cast<std::uint16_t>(table[static_cast<std::size_t>(2 * i + 1)]);
				if (bits < 1 || bits > 12)
				{
					set_error(error, std::string("the ") + which + " wavelet Huffman table entry " + std::to_string(i) + " has bit length "
						+ std::to_string(bits) + "; this is not the 1.1.1.13 executable");
					return false;
				}
			}
			return true;
		}

		// WaveletDecoder.cs:343-412 ( BitReader ): LSB first, a 16 bit look ahead window
		class bit_reader
		{
		public:
			bit_reader(const byte_span data, const std::size_t start) : data_(data), position_(static_cast<std::int64_t>(start)) {}

			std::uint16_t value = 0;
			bool failed = false;

			std::uint8_t read_raw_byte()
			{
				if (position_ >= static_cast<std::int64_t>(data_.size()))
				{
					failed = true;	// "wavelet data ends early"
					return 0;
				}
				return data_[static_cast<std::size_t>(position_++)];
			}

			void ensure_initialized()
			{
				if (initialized_)
				{
					return;
				}
				value = static_cast<std::uint16_t>(byte_at(position_) | (byte_at(position_ + 1) << 8));
				bit_ = 0;
				position_ += 2;
				initialized_ = true;
			}

			void consume(const int count)
			{
				const std::uint32_t window = static_cast<std::uint32_t>(byte_at(position_)) | (static_cast<std::uint32_t>(byte_at(position_ + 1)) << 8)
					| (static_cast<std::uint32_t>(byte_at(position_ + 2)) << 16) | (static_cast<std::uint32_t>(byte_at(position_ + 3)) << 24);
				value = static_cast<std::uint16_t>((value >> count) | static_cast<std::uint16_t>((window >> bit_) << (16 - count)));
				bit_ += count;
				position_ += bit_ >> 3;
				bit_ &= 7;
				if (position_ > static_cast<std::int64_t>(data_.size()) + 2)
				{
					failed = true;	// "wavelet bit stream runs past the end of the file"
				}
			}

			int read_huffman(const std::int16_t* table, const int escape_bits, const int bias)
			{
				const int index = value & 0xFFF;
				const int length = static_cast<std::uint16_t>(table[2 * index + 1]);
				int result = table[2 * index];
				consume(length);
				if (result == -32768)
				{
					result = (value & ((1 << escape_bits) - 1)) - bias;
					consume(escape_bits);
				}
				return result;
			}

			int read_bit()
			{
				const int bit = value & 1;
				consume(1);
				return bit;
			}

			void align_to_next_face()
			{
				position_ -= bit_ != 0 ? 1 : 2;
				bit_ = 0;
				initialized_ = false;
			}

		private:
			int byte_at(const std::int64_t index) const
			{
				return index >= 0 && index < static_cast<std::int64_t>(data_.size()) ? data_[static_cast<std::size_t>(index)] : 0;
			}

			byte_span data_;
			std::int64_t position_;
			int bit_ = 0;
			bool initialized_ = false;
		};

		// WaveletDecoder.cs:320-327
		void reconstruct(std::uint8_t* dst, const std::uint8_t low, const int channel_offset, const int tl, const int tr, const int bl, const int br,
			const int lsb, const int d1, const int d2, const int d3)
		{
			const int p2 = 2 * low;
			dst[tl + channel_offset] = static_cast<std::uint8_t>(lsb + ((p2 + d1 + d2 + d3) >> 1));
			dst[tr + channel_offset] = static_cast<std::uint8_t>((p2 + d1 - d3 - d2) >> 1);
			dst[bl + channel_offset] = static_cast<std::uint8_t>((p2 + d2 - d3 - d1) >> 1);
			dst[br + channel_offset] = static_cast<std::uint8_t>((p2 + d3 - d2 - d1) >> 1);
		}

		// WaveletDecoder.cs:234-318 ( DecodeLevel )
		bool decode_level(bit_reader& reader, const wavelet_tables& tables, const std::vector<std::uint8_t>& lower, const int previous_width,
			std::vector<std::uint8_t>& dst, const int w, const int h, const int stride, const int channels, const int bpp, const int* offsets)
		{
			reader.ensure_initialized();
			const bool refine = reader.read_bit() != 0;
			std::vector<std::uint8_t> refined;
			if (refine)
			{
				// the lower level is corrected first, on a copy ( the level already handed out stays as decoded )
				refined = lower;
				auto& previous = refined;
				const int count = w * h / 4;
				for (int p = 0; p < count; p++)
				{
					for (int c = 0; c < channels; c++)
					{
						const int delta = reader.read_huffman(tables.alpha(), 9, 255);
						const std::size_t index = static_cast<std::size_t>(p) * bpp + offsets[c];
						if (index >= previous.size())
						{
							return false;
						}
						previous[index] = static_cast<std::uint8_t>(previous[index] + delta);
					}
				}
			}

			const std::vector<std::uint8_t>& previous = refine ? refined : lower;
			const int row_pairs = (h - 1) / 2 + 1;
			const int column_pairs = (w - 1) / 2 + 1;
			for (int ry = 0; ry < row_pairs; ry++)
			{
				for (int rx = 0; rx < column_pairs; rx++)
				{
					const int prev_index = (ry * previous_width + rx) * bpp;
					const int tl = (2 * ry * stride + 2 * rx) * bpp;
					const int tr = tl + bpp;
					const int bl = tl + stride * bpp;
					const int br = bl + bpp;
					if (static_cast<std::size_t>(prev_index) + bpp > previous.size() || static_cast<std::size_t>(br) + bpp > dst.size())
					{
						return false;	// "wavelet low-pass index out of range"
					}

					if (channels != 1)
					{
						const int lsb = reader.read_bit();
						const int d1 = reader.read_huffman(tables.primary(), 9, 255);
						const int d2 = reader.read_huffman(tables.primary(), 9, 255);
						const int d3 = reader.read_huffman(tables.primary(), 9, 255);
						reconstruct(dst.data(), previous[static_cast<std::size_t>(prev_index + offsets[0])], offsets[0], tl, tr, bl, br, lsb, d1, d2, d3);
						if (channels >= 3)
						{
							for (int c = 1; c <= 2; c++)
							{
								const int bit = reader.read_bit();
								const int e1 = d1 + reader.read_huffman(tables.secondary(), 10, 510);
								const int e2 = d2 + reader.read_huffman(tables.secondary(), 10, 510);
								const int e3 = d3 + reader.read_huffman(tables.secondary(), 10, 510);
								reconstruct(dst.data(), previous[static_cast<std::size_t>(prev_index + offsets[c])], offsets[c], tl, tr, bl, br, bit, e1, e2, e3);
							}
						}
					}

					if (channels == 3)
					{
						if (bpp != 3)
						{
							dst[static_cast<std::size_t>(tl + offsets[3])] = 255;
							dst[static_cast<std::size_t>(tr + offsets[3])] = 255;
							dst[static_cast<std::size_t>(bl + offsets[3])] = 255;
							dst[static_cast<std::size_t>(br + offsets[3])] = 255;
						}
					}
					else
					{
						const int lsb = reader.read_bit();
						const int g1 = reader.read_huffman(tables.alpha(), 9, 255);
						const int g2 = reader.read_huffman(tables.alpha(), 9, 255);
						const int g3 = reader.read_huffman(tables.alpha(), 9, 255);
						const int offset = offsets[channels - 1];
						reconstruct(dst.data(), previous[static_cast<std::size_t>(prev_index + offset)], offset, tl, tr, bl, br, lsb, g1, g2, g3);
					}
					if (reader.failed)
					{
						return false;
					}
				}
			}
			return !reader.failed;
		}
	}

	std::shared_ptr<const wavelet_tables> wavelet_tables::from_memory(const byte_span primary, const byte_span secondary, const byte_span alpha, std::string* error)
	{
		try
		{
			auto tables = std::make_shared<wavelet_tables>();
			if (!read_table(primary, tables->primary_, "primary", error) || !read_table(secondary, tables->secondary_, "secondary", error)
				|| !read_table(alpha, tables->alpha_, "alpha", error))
			{
				return nullptr;
			}
			return tables;
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return nullptr;
		}
	}

	std::shared_ptr<const wavelet_tables> wavelet_tables::load_from_executable(const std::filesystem::path& exe, std::string* error)
	{
		try
		{
			// WaveletDecoder.cs:116-134 ( LoadFromExecutable ), the 1.1.1.13 candidate only
			std::FILE* file = _wfopen(exe.c_str(), L"rb");
			if (!file)
			{
				set_error(error, "cannot open " + exe.string());
				return nullptr;
			}
			std::vector<std::uint8_t> buffers[3];
			const std::uint64_t offsets[3] = { build::EXE_WAVELET_PRIMARY, build::EXE_WAVELET_SECONDARY, build::EXE_WAVELET_ALPHA };
			bool ok = true;
			for (int i = 0; i < 3 && ok; i++)
			{
				buffers[i].resize(TABLE_BYTES);
				ok = _fseeki64(file, static_cast<long long>(offsets[i]), SEEK_SET) == 0
					&& std::fread(buffers[i].data(), 1, TABLE_BYTES, file) == TABLE_BYTES;
			}
			std::fclose(file);
			if (!ok)
			{
				set_error(error, exe.string() + " is too short for the wavelet tables of 1.1.1.13");
				return nullptr;
			}
			return from_memory(buffers[0], buffers[1], buffers[2], error);
		}
		catch (...)
		{
			set_error(error, "out of memory");
			return nullptr;
		}
	}

	bool decode_wavelet(const iwi_file& file, const wavelet_tables& tables, std::vector<wavelet_level>& out, std::string* error)
	{
		try
		{
			// WaveletDecoder.cs:169-232
			out.clear();
			int channels;
			switch (file.header.format)
			{
			case iwi_format::wavelet_rgba: channels = 4; break;
			case iwi_format::wavelet_rgb: channels = 3; break;
			case iwi_format::wavelet_luminance_alpha: channels = 2; break;
			case iwi_format::wavelet_luminance:
			case iwi_format::wavelet_alpha: channels = 1; break;
			default:
				set_error(error, std::string(iwi_format_name(file.header.format)) + " is not a wavelet format");
				return false;
			}
			const int bpp = channels == 3 ? 4 : channels;
			static constexpr int OFFSETS_1[4] = { 0, 0, 0, 0 };
			static constexpr int OFFSETS_2[4] = { 0, 1, 0, 0 };
			static constexpr int OFFSETS_4[4] = { 0, 1, 2, 3 };
			const int* offsets = bpp == 1 ? OFFSETS_1 : bpp == 2 ? OFFSETS_2 : OFFSETS_4;

			bit_reader reader(file.data, build::IWI_HEADER_SIZE);
			const int mip_count = file.header.mip_count();
			out.reserve(static_cast<std::size_t>(mip_count) * file.header.face_count());	// `previous` points into it
			const int width = file.header.width, height = file.header.height;
			for (int face = 0; face < file.header.face_count(); face++)
			{
				const std::vector<std::uint8_t>* previous = nullptr;
				for (int level = mip_count - 1; level >= 0; level--)
				{
					int w = width >> level;
					int h = height >> level;
					wavelet_level current;
					current.face = face;
					current.level = level;
					if (w > 1 && h > 1)
					{
						if (!previous)
						{
							set_error(error, "wavelet level " + std::to_string(level) + " has no lower level to reconstruct from");
							return false;
						}
						const int padded_w = (w + 1) & ~1;
						const int padded_h = (h + 1) & ~1;
						std::vector<std::uint8_t> padded(static_cast<std::size_t>(padded_w) * padded_h * bpp);
						if (!decode_level(reader, tables, *previous, std::max(1, width >> (level + 1)), padded, w, h, padded_w, channels, bpp, offsets))
						{
							set_error(error, reader.failed ? "wavelet bit stream runs past the end of the file" : "wavelet low-pass index out of range");
							return false;
						}
						// WaveletDecoder.cs:329-341 ( Crop )
						if (padded_w == w && padded.size() == static_cast<std::size_t>(w) * h * bpp)
						{
							current.data = std::move(padded);
						}
						else
						{
							current.data.resize(static_cast<std::size_t>(w) * h * bpp);
							for (int y = 0; y < h; y++)
							{
								std::memcpy(current.data.data() + static_cast<std::size_t>(y) * w * bpp, padded.data() + static_cast<std::size_t>(y) * padded_w * bpp,
									static_cast<std::size_t>(w) * bpp);
							}
						}
					}
					else
					{
						// levels that are 1 pixel wide or high are stored raw
						w = std::max(w, 1);
						h = std::max(h, 1);
						current.data.resize(static_cast<std::size_t>(w) * h * bpp);
						for (int p = 0; p < w * h; p++)
						{
							for (int c = 0; c < channels; c++)
							{
								current.data[static_cast<std::size_t>(p * bpp + offsets[c])] = reader.read_raw_byte();
							}
							if (bpp != channels)
							{
								current.data[static_cast<std::size_t>(p * bpp + offsets[3])] = 255;
							}
						}
						if (reader.failed)
						{
							set_error(error, "wavelet data ends early");
							return false;
						}
					}
					current.width = w;
					current.height = h;
					out.push_back(std::move(current));
					previous = &out.back().data;
				}
				reader.align_to_next_face();
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
