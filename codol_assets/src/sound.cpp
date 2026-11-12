// Sounds: soundaliases .csv + .wav -> snd_alias_list_t / LoadedSound / SndCurve of 1.1.1.13, and back.

#include "codol_assets/sound.hpp"

#include "codol_zone/assets.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>

namespace codol::assets
{
	namespace
	{
		using zone::ZoneEditor;
		using zone::ZoneNewPiece;
		using zone::ZoneNewPointer;

		// ---- little helpers ------------------------------------------------------------------------------
		std::uint16_t rd16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
		std::uint32_t rd32(const std::uint8_t* p) { return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
		void wr16(std::vector<std::uint8_t>& out, const std::uint32_t v)
		{
			out.push_back(static_cast<std::uint8_t>(v));
			out.push_back(static_cast<std::uint8_t>(v >> 8));
		}
		void wr32(std::vector<std::uint8_t>& out, const std::uint32_t v)
		{
			wr16(out, v & 0xFFFF);
			wr16(out, v >> 16);
		}
		void wrtag(std::vector<std::uint8_t>& out, const char* tag) { out.insert(out.end(), tag, tag + 4); }

		std::string lower(std::string_view text)
		{
			std::string out(text);
			for (char& c : out)
			{
				c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
			}
			return out;
		}

		std::string trim(std::string_view text)
		{
			std::size_t a = 0, b = text.size();
			while (a < b && (text[a] == ' ' || text[a] == '\t' || text[a] == '\r'))
			{
				a++;
			}
			while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t' || text[b - 1] == '\r'))
			{
				b--;
			}
			return std::string(text.substr(a, b - a));
		}

		bool to_float(const std::string& text, float& out)
		{
			const char* first = text.data();
			const char* last = text.data() + text.size();
			if (first != last && *first == '+')
			{
				first++;
			}
			const auto r = std::from_chars(first, last, out);
			return r.ec == std::errc() && r.ptr == last;
		}

		bool to_int(const std::string& text, std::int32_t& out)
		{
			float f = 0;
			if (!to_float(text, f))
			{
				return false;
			}
			out = static_cast<std::int32_t>(f);
			return true;
		}

		// shortest text that reads back as the same float
		std::string float_text(const float value)
		{
			char buffer[48];
			const auto r = std::to_chars(buffer, buffer + sizeof buffer, value);
			return std::string(buffer, r.ptr);
		}

		// ---- IMA ADPCM -----------------------------------------------------------------------------------
		constexpr int IMA_INDEX[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
		constexpr int IMA_STEP[89] = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
			157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
			2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
			24623, 27086, 29794, 32767 };

		struct ImaState
		{
			int predictor = 0;
			int index = 0;

			int decode(const int nibble)
			{
				const int step = IMA_STEP[index];
				int diff = step >> 3;
				if (nibble & 1) diff += step >> 2;
				if (nibble & 2) diff += step >> 1;
				if (nibble & 4) diff += step;
				predictor = std::clamp(predictor + ((nibble & 8) ? -diff : diff), -32768, 32767);
				index = std::clamp(index + IMA_INDEX[nibble], 0, 88);
				return predictor;
			}

			int encode(const int sample)
			{
				const int step = IMA_STEP[index];
				int delta = sample - predictor;
				int nibble = 0;
				if (delta < 0)
				{
					nibble = 8;
					delta = -delta;
				}
				if (delta >= step) { nibble |= 4; delta -= step; }
				if (delta >= (step >> 1)) { nibble |= 2; delta -= step >> 1; }
				if (delta >= (step >> 2)) { nibble |= 1; }
				decode(nibble);
				return nibble;
			}
		};

		std::uint32_t ima_frames_per_block(const std::uint32_t blockAlign, const int channels)
		{
			const std::uint32_t header = 4u * static_cast<std::uint32_t>(channels);
			return blockAlign > header ? (blockAlign - header) * 8 / (4u * static_cast<std::uint32_t>(channels)) + 1 : 0;
		}

		std::int16_t pcm_at(const WaveData& pcm, const std::uint32_t frame, const int channel)
		{
			const std::uint32_t frames = pcm.frames();
			const std::uint32_t f = frames ? std::min(frame, frames - 1) : 0;
			const std::size_t at = (static_cast<std::size_t>(f) * static_cast<std::size_t>(pcm.channels) + static_cast<std::size_t>(channel)) * 2;
			return at + 1 < pcm.data.size() ? static_cast<std::int16_t>(rd16(pcm.data.data() + at)) : std::int16_t{ 0 };
		}

		// ---- pieces of zone stream ( as link.cpp ) -----------------------------------------------------------
		struct Pieces
		{
			std::vector<ZoneNewPiece> list;

			std::uint32_t add(const void* data, const std::size_t size)
			{
				ZoneNewPiece piece;
				piece.bytes.assign(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
				list.push_back(std::move(piece));
				return static_cast<std::uint32_t>(list.size() - 1);
			}
			template <typename T>
			std::uint32_t add(const T& value) { return add(&value, sizeof(T)); }
			std::uint32_t add_string(const std::string_view text)
			{
				ZoneNewPiece piece;
				piece.string = true;
				piece.bytes.assign(text.begin(), text.end());
				piece.bytes.push_back(0);
				list.push_back(std::move(piece));
				return static_cast<std::uint32_t>(list.size() - 1);
			}
			void to_piece(const std::uint32_t piece, const std::uint32_t offset, const std::uint32_t target, const std::uint32_t targetOffset = 0)
			{
				ZoneNewPointer p;
				p.offset = offset;
				p.piece = target;
				p.pieceOffset = targetOffset;
				list[piece].pointers.push_back(p);
			}
			void to_asset(const std::uint32_t piece, const std::uint32_t offset, const std::size_t asset)
			{
				ZoneNewPointer p;
				p.offset = offset;
				p.asset = asset;
				list[piece].pointers.push_back(p);
			}
			void put32(const std::uint32_t piece, const std::uint32_t offset, const std::uint32_t value) { std::memcpy(list[piece].bytes.data() + offset, &value, 4); }
		};

		bool same_map(const SpeakerMapDef& a, const SpeakerMapDef& b)
		{
			return a.name == b.name && a.isDefault == b.isDefault && std::memcmp(a.channelMaps, b.channelMaps, sizeof a.channelMaps) == 0;
		}

		// soundaliases/channels.def ( rawfile of code_pre_gfx_mp ): the order is the channel index
		constexpr std::string_view CHANNELS[] = { "physics", "ambdist1", "ambdist2", "auto", "auto2", "autodog", "bulletimpact", "bulletwhizby", "explosiveimpact",
			"element", "auto2d", "vehicle", "vehiclelimited", "menu", "body", "body2d", "reload", "reload2d", "item", "effects1", "effects2", "weapon", "weapon2d",
			"nonshock", "voice", "local", "local2", "local3", "ambient", "hurt", "player1", "player2", "music", "musicnopause", "mission", "announcer", "shellshock" };

		std::vector<std::string> csv_fields(const std::string_view line)
		{
			std::vector<std::string> fields;
			std::string field;
			bool quoted = false;
			for (std::size_t i = 0; i < line.size(); i++)
			{
				const char c = line[i];
				if (quoted)
				{
					if (c == '"' && i + 1 < line.size() && line[i + 1] == '"')
					{
						field.push_back('"');
						i++;
					}
					else if (c == '"')
					{
						quoted = false;
					}
					else
					{
						field.push_back(c);
					}
				}
				else if (c == '"' && trim(field).empty())
				{
					field.clear();
					quoted = true;
				}
				else if (c == ',')
				{
					fields.push_back(std::move(field));
					field.clear();
				}
				else
				{
					field.push_back(c);
				}
			}
			fields.push_back(std::move(field));
			return fields;
		}

		std::string csv_quote(const std::string& value)
		{
			if (value.find_first_of(",\"") == std::string::npos && (value.empty() || (value.front() != ' ' && value.back() != ' ' && value.front() != '#')))
			{
				return value;
			}
			std::string out = "\"";
			for (const char c : value)
			{
				out.push_back(c);
				if (c == '"')
				{
					out.push_back('"');
				}
			}
			out.push_back('"');
			return out;
		}

		constexpr std::string_view COD4_COLUMNS[] = { "name", "sequence", "file", "vol_min", "vol_max", "vol_mod", "pitch_min", "pitch_max", "dist_min", "dist_max",
			"channel", "type", "probability", "loop", "masterslave", "loadspec", "subtitle", "compression", "secondaryaliasname", "volumefalloffcurve", "startdelay",
			"speakermap", "reverb", "lfe percentage", "center percentage", "platform", "envelop_min", "envelop_max", "envelop percentage" };

		constexpr std::string_view OUT_COLUMNS[] = { "name", "sequence", "file", "vol_min", "vol_max", "vol_mod", "pitch_min", "pitch_max", "dist_min", "dist_max",
			"velocity_min", "channel", "type", "probability", "loop", "masterslave", "loadspec", "subtitle", "compression", "secondaryaliasname", "chainaliasname",
			"volumefalloffcurve", "startdelay", "speakermap", "reverb", "lfe percentage", "center percentage", "platform", "envelop_min", "envelop_max",
			"envelop percentage", "masterpriority", "masterpercentage", "slavepercentage", "flags" };
	}

	// =====================================================================================================
	// wave files

	std::uint32_t WaveData::frames() const
	{
		if (channels <= 0)
		{
			return 0;
		}
		if (format == WAVE_IMA_ADPCM)
		{
			return blockAlign ? static_cast<std::uint32_t>(data.size() / blockAlign) * ima_frames_per_block(blockAlign, channels) : 0;
		}
		return static_cast<std::uint32_t>(data.size() / (2u * static_cast<std::uint32_t>(channels)));
	}

	Status parse_wav(const std::span<const std::uint8_t> file, WaveData& out)
	{
		out = WaveData{};
		if (file.size() < 12 || std::memcmp(file.data(), "RIFF", 4) != 0 || std::memcmp(file.data() + 8, "WAVE", 4) != 0)
		{
			return Status::error("not a RIFF WAVE file");
		}
		bool haveFormat = false, haveData = false;
		int tag = 0, bits = 0;
		std::span<const std::uint8_t> data;
		for (std::size_t at = 12; at + 8 <= file.size();)
		{
			const std::size_t size = rd32(file.data() + at + 4);
			const std::uint8_t* body = file.data() + at + 8;
			const std::size_t have = std::min(size, file.size() - at - 8);
			if (std::memcmp(file.data() + at, "fmt ", 4) == 0 && have >= 16)
			{
				tag = rd16(body);
				out.channels = rd16(body + 2);
				out.rate = rd32(body + 4);
				out.blockAlign = rd16(body + 12);
				bits = rd16(body + 14);
				if (tag == 0xFFFE && have >= 26)
				{
					tag = rd16(body + 24);		// WAVE_FORMAT_EXTENSIBLE: the first two bytes of the sub format
				}
				haveFormat = true;
			}
			else if (std::memcmp(file.data() + at, "data", 4) == 0)
			{
				data = { body, have };			// a size beyond the file ( streamed writers ): what is there
				haveData = true;
			}
			at += 8 + size + (size & 1);
		}
		if (!haveFormat || !haveData)
		{
			return Status::error("wave file without a fmt or data chunk");
		}
		if (out.channels < 1 || out.channels > 2)
		{
			return Status::error("wave file with " + std::to_string(out.channels) + " channels: mono and stereo only");
		}
		if (out.rate < 4000 || out.rate > 96000)
		{
			return Status::error("wave file with a sample rate of " + std::to_string(out.rate));
		}
		const auto channels = static_cast<std::size_t>(out.channels);
		if (tag == WAVE_IMA_ADPCM)
		{
			if (bits != 4 || out.blockAlign <= 4 * channels || ima_frames_per_block(out.blockAlign, out.channels) < 2)
			{
				return Status::error("IMA ADPCM wave file with " + std::to_string(bits) + " bits or a block of " + std::to_string(out.blockAlign));
			}
			out.format = WAVE_IMA_ADPCM;
			out.bits = 4;
			out.data.assign(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(data.size() - data.size() % out.blockAlign));
			if (data.size() % out.blockAlign)
			{
				out.notes.push_back("incomplete last ADPCM block dropped");
			}
			return {};
		}
		if (tag != WAVE_PCM || (bits != 8 && bits != 16 && bits != 24))
		{
			return Status::error("wave format " + std::to_string(tag) + " with " + std::to_string(bits) + " bits: PCM 8 / 16 / 24 bit and IMA ADPCM only");
		}
		const std::size_t bytes = static_cast<std::size_t>(bits) / 8;
		const std::size_t samples = data.size() / bytes / channels * channels;
		out.format = WAVE_PCM;
		out.bits = 16;
		out.blockAlign = static_cast<std::uint32_t>(2 * channels);
		out.data.resize(samples * 2);
		for (std::size_t k = 0; k < samples; k++)
		{
			const std::uint8_t* s = data.data() + k * bytes;
			const int value = bits == 8 ? (static_cast<int>(s[0]) - 128) * 256 : bits == 16 ? static_cast<std::int16_t>(rd16(s)) : static_cast<std::int16_t>(rd16(s + 1));
			out.data[k * 2] = static_cast<std::uint8_t>(value);
			out.data[k * 2 + 1] = static_cast<std::uint8_t>(value >> 8);
		}
		if (bits != 16)
		{
			out.notes.push_back(std::to_string(bits) + " bit samples stored as 16 bit");
		}
		return {};
	}

	Status load_wav(const std::filesystem::path& file, WaveData& out)
	{
		std::vector<std::uint8_t> bytes;
		if (const Status s = zone::read_file(file, bytes); !s)
		{
			return s;
		}
		if (const Status s = parse_wav(bytes, out); !s)
		{
			return Status::error(file.string() + ": " + s.message);
		}
		return {};
	}

	std::vector<std::uint8_t> write_wav(const WaveData& wave)
	{
		const bool adpcm = wave.format == WAVE_IMA_ADPCM;
		std::vector<std::uint8_t> out;
		wrtag(out, "RIFF");
		wr32(out, 0);
		wrtag(out, "WAVE");
		wrtag(out, "fmt ");
		wr32(out, adpcm ? 20u : 16u);
		wr16(out, static_cast<std::uint32_t>(wave.format));
		wr16(out, static_cast<std::uint32_t>(wave.channels));
		wr32(out, wave.rate);
		const std::uint32_t perBlock = adpcm ? ima_frames_per_block(wave.blockAlign, wave.channels) : 1;
		wr32(out, adpcm ? (perBlock ? static_cast<std::uint32_t>(static_cast<std::uint64_t>(wave.rate) * wave.blockAlign / perBlock) : 0) : wave.rate * wave.blockAlign);
		wr16(out, wave.blockAlign);
		wr16(out, static_cast<std::uint32_t>(wave.bits));
		if (adpcm)
		{
			wr16(out, 2);
			wr16(out, perBlock);
			wrtag(out, "fact");
			wr32(out, 4);
			wr32(out, wave.frames());
		}
		wrtag(out, "data");
		wr32(out, static_cast<std::uint32_t>(wave.data.size()));
		out.insert(out.end(), wave.data.begin(), wave.data.end());
		if (out.size() & 1)
		{
			out.push_back(0);
		}
		const auto riff = static_cast<std::uint32_t>(out.size() - 8);
		std::memcpy(out.data() + 4, &riff, 4);
		return out;
	}

	Status ima_adpcm_encode(const WaveData& pcm, std::uint32_t blockAlign, WaveData& out)
	{
		if (pcm.format != WAVE_PCM || pcm.bits != 16 || pcm.channels < 1 || pcm.channels > 2)
		{
			return Status::error("ima_adpcm_encode wants 16 bit PCM, mono or stereo");
		}
		const auto channels = static_cast<std::uint32_t>(pcm.channels);
		if (blockAlign == 0)
		{
			// 256 bytes per channel and 11025 Hz: 1024 at 44100 and 48000, 768 at 32000, 512 at 22050 ( shipped mono sounds )
			blockAlign = 256 * channels * std::max<std::uint32_t>(1, (pcm.rate + 5512) / 11025);
		}
		const std::uint32_t perBlock = ima_frames_per_block(blockAlign, pcm.channels);
		if (perBlock < 2 || (blockAlign - 4 * channels) % (4 * channels) != 0)
		{
			return Status::error("ADPCM block size " + std::to_string(blockAlign) + " does not fit " + std::to_string(channels) + " channel(s)");
		}
		const std::uint32_t frames = pcm.frames();
		const std::uint32_t blocks = (frames + perBlock - 1) / perBlock;
		out = WaveData{};
		out.format = WAVE_IMA_ADPCM;
		out.channels = pcm.channels;
		out.rate = pcm.rate;
		out.bits = 4;
		out.blockAlign = blockAlign;
		out.data.assign(static_cast<std::size_t>(blocks) * blockAlign, 0);
		ImaState state[2];
		for (std::uint32_t b = 0; b < blocks; b++)
		{
			std::uint8_t* block = out.data.data() + static_cast<std::size_t>(b) * blockAlign;
			const std::uint32_t first = b * perBlock;
			for (std::uint32_t c = 0; c < channels; c++)
			{
				const std::int16_t s = pcm_at(pcm, first, static_cast<int>(c));
				state[c].predictor = s;
				if (b == 0)
				{
					// a first step size near the first difference instead of the smallest one
					const int diff = std::abs(pcm_at(pcm, first + 1, static_cast<int>(c)) - s);
					int index = 0;
					while (index < 88 && IMA_STEP[index] < diff)
					{
						index++;
					}
					state[c].index = index;
				}
				block[c * 4] = static_cast<std::uint8_t>(s);
				block[c * 4 + 1] = static_cast<std::uint8_t>(s >> 8);
				block[c * 4 + 2] = static_cast<std::uint8_t>(state[c].index);
			}
			// groups of 8 frames: 4 bytes per channel, low nibble first
			std::uint8_t* at = block + 4 * channels;
			for (std::uint32_t f = 1; f < perBlock; f += 8)
			{
				for (std::uint32_t c = 0; c < channels; c++)
				{
					for (std::uint32_t k = 0; k < 8; k++)
					{
						const int nibble = state[c].encode(pcm_at(pcm, first + f + k, static_cast<int>(c)));
						at[k / 2] = static_cast<std::uint8_t>(at[k / 2] | (nibble << ((k & 1) * 4)));
					}
					at += 4;
				}
			}
		}
		return {};
	}

	Status ima_adpcm_decode(const WaveData& adpcm, WaveData& out)
	{
		if (adpcm.format != WAVE_IMA_ADPCM || adpcm.channels < 1 || adpcm.channels > 2)
		{
			return Status::error("ima_adpcm_decode wants IMA ADPCM, mono or stereo");
		}
		const auto channels = static_cast<std::uint32_t>(adpcm.channels);
		const std::uint32_t perBlock = ima_frames_per_block(adpcm.blockAlign, adpcm.channels);
		if (perBlock < 2)
		{
			return Status::error("ADPCM block size " + std::to_string(adpcm.blockAlign));
		}
		const std::size_t blocks = adpcm.data.size() / adpcm.blockAlign;
		out = WaveData{};
		out.channels = adpcm.channels;
		out.rate = adpcm.rate;
		out.blockAlign = 2 * channels;
		out.data.assign(blocks * perBlock * channels * 2, 0);
		const auto put = [&](const std::size_t frame, const std::uint32_t c, const int value)
		{
			const std::size_t at = (frame * channels + c) * 2;
			out.data[at] = static_cast<std::uint8_t>(value);
			out.data[at + 1] = static_cast<std::uint8_t>(value >> 8);
		};
		for (std::size_t b = 0; b < blocks; b++)
		{
			const std::uint8_t* block = adpcm.data.data() + b * adpcm.blockAlign;
			ImaState state[2];
			for (std::uint32_t c = 0; c < channels; c++)
			{
				state[c].predictor = static_cast<std::int16_t>(rd16(block + c * 4));
				state[c].index = std::min<int>(block[c * 4 + 2], 88);
				put(b * perBlock, c, state[c].predictor);
			}
			const std::uint8_t* at = block + 4 * channels;
			for (std::uint32_t f = 1; f < perBlock; f += 8)
			{
				for (std::uint32_t c = 0; c < channels; c++)
				{
					for (std::uint32_t k = 0; k < 8 && f + k < perBlock; k++)
					{
						put(b * perBlock + f + k, c, state[c].decode((at[k / 2] >> ((k & 1) * 4)) & 15));
					}
					at += 4;
				}
			}
		}
		return {};
	}

	// =====================================================================================================
	// definitions

	int sound_channel_index(const std::string_view name)
	{
		const std::string n = lower(name);
		for (std::size_t k = 0; k < std::size(CHANNELS); k++)
		{
			if (CHANNELS[k] == n)
			{
				return static_cast<int>(k);
			}
		}
		return -1;
	}

	std::string_view sound_channel_name(const int index)
	{
		return index >= 0 && static_cast<std::size_t>(index) < std::size(CHANNELS) ? CHANNELS[index] : std::string_view();
	}

	std::string sound_file_name(const std::string_view file)
	{
		std::string out = trim(file);
		std::replace(out.begin(), out.end(), '\\', '/');
		while (!out.empty() && out.front() == '/')
		{
			out.erase(out.begin());
		}
		if (lower(out).rfind("sound/", 0) == 0)
		{
			out.erase(0, 6);
		}
		return out;
	}

	Status parse_sound_aliases(const std::string_view csv, std::vector<SoundAliasDef>& out, std::vector<std::string>* notes)
	{
		std::vector<std::string> columns(std::begin(COD4_COLUMNS), std::end(COD4_COLUMNS));
		std::size_t lineNumber = 0;
		bool first = true;
		for (std::size_t at = 0; at < csv.size();)
		{
			std::size_t end = csv.find('\n', at);
			end = end == std::string_view::npos ? csv.size() : end;
			std::string_view line = csv.substr(at, end - at);
			at = end + 1;
			lineNumber++;
			if (lineNumber == 1 && line.size() >= 3 && line.substr(0, 3) == "\xEF\xBB\xBF")
			{
				line.remove_prefix(3);
			}
			const std::string head = trim(line);
			if (head.empty() || head.front() == '#')
			{
				continue;
			}
			std::vector<std::string> fields = csv_fields(line);
			for (std::string& f : fields)
			{
				f = trim(f);
			}
			if (first)
			{
				first = false;
				if (lower(fields[0]) == "name")
				{
					columns.clear();
					for (const std::string& f : fields)
					{
						columns.push_back(lower(f));
					}
					continue;
				}
				if (notes)
				{
					notes->push_back("no header row: the column order of the CoD4 mod tools is assumed");
				}
			}
			if (fields[0].empty())
			{
				continue;
			}
			SoundAliasDef def;
			def.name = fields[0];
			const std::string where = "line " + std::to_string(lineNumber) + " ( " + def.name + " ): ";
			bool volMax = false, pitchMax = false, distMax = false, slaveGiven = false, typeGiven = false;
			for (std::size_t k = 1; k < fields.size() && k < columns.size(); k++)
			{
				const std::string& column = columns[k];
				const std::string& value = fields[k];
				if (value.empty())
				{
					continue;
				}
				const std::string low = lower(value);
				bool ok = true;
				if (column == "sequence") ok = to_int(value, def.sequence);
				else if (column == "file") def.file = sound_file_name(value);
				else if (column == "vol_min") ok = to_float(value, def.volMin);
				else if (column == "vol_max") { ok = to_float(value, def.volMax); volMax = true; }
				else if (column == "vol_mod") def.volMod = value;
				else if (column == "pitch_min") ok = to_float(value, def.pitchMin);
				else if (column == "pitch_max") { ok = to_float(value, def.pitchMax); pitchMax = true; }
				else if (column == "dist_min") ok = to_float(value, def.distMin);
				else if (column == "dist_max") { ok = to_float(value, def.distMax); distMax = true; }
				else if (column == "velocity_min") ok = to_float(value, def.velocityMin);
				else if (column == "channel") { def.channel = low; ok = sound_channel_index(low) >= 0; }
				else if (column == "type") { ok = low == "streamed" || low == "loaded"; def.streamed = low == "streamed"; typeGiven = true; }
				else if (column == "probability") ok = to_float(value, def.probability);
				else if (column == "loop")
				{
					ok = low == "looping" || low == "rlooping" || low == "nonlooping";
					def.looping = low == "looping" || low == "rlooping";
					def.rlooping = low == "rlooping";
				}
				else if (column == "masterslave")
				{
					float f = 0;
					if (to_float(value, f))
					{
						if (!slaveGiven)
						{
							def.slavePercentage = f;
						}
					}
					else if (low != "master")
					{
						ok = false;
					}
					else if (notes)
					{
						notes->push_back(where + "masterslave 'master' has no field of its own in this build: use masterpriority / masterpercentage");
					}
				}
				else if (column == "subtitle") def.subtitle = value;
				else if (column == "compression") def.compression = low;
				else if (column == "secondaryaliasname") def.secondaryAliasName = value;
				else if (column == "chainaliasname") def.chainAliasName = value;
				else if (column == "volumefalloffcurve") def.volumeFalloffCurve = value;
				else if (column == "startdelay") ok = to_int(value, def.startDelay);
				else if (column == "speakermap") def.speakerMap = value;
				else if (column == "reverb")
				{
					def.fullDryLevel = low.find("fulldrylevel") != std::string::npos;
					def.noWetLevel = low.find("nowetlevel") != std::string::npos;
					ok = def.fullDryLevel || def.noWetLevel;
				}
				else if (column == "lfe percentage") ok = to_float(value, def.lfePercentage);
				else if (column == "center percentage") ok = to_float(value, def.centerPercentage);
				else if (column == "envelop_min") ok = to_float(value, def.envelopMin);
				else if (column == "envelop_max") ok = to_float(value, def.envelopMax);
				else if (column == "envelop percentage") ok = to_float(value, def.envelopPercentage);
				else if (column == "masterpriority")
				{
					std::int32_t n = 0;
					ok = to_int(value, n) && n >= 0 && n <= 255;
					def.masterPriority = static_cast<std::uint8_t>(n);
				}
				else if (column == "masterpercentage") ok = to_float(value, def.masterPercentage);
				else if (column == "slavepercentage") { ok = to_float(value, def.slavePercentage); slaveGiven = true; }
				else if (column == "flags")
				{
					const char* b = low.data() + (low.rfind("0x", 0) == 0 ? 2 : 0);
					const auto r = std::from_chars(b, low.data() + low.size(), def.extraFlags, 16);
					ok = r.ec == std::errc() && r.ptr == low.data() + low.size() && (def.extraFlags & ~0x66u) == 0;
				}
				else if (column == "loadspec" || column == "platform" || column == "conversion_quality")
				{
					// build time selections of the original tools; every row is compiled
				}
				else if (notes)
				{
					notes->push_back(where + "column '" + column + "' is not known and ignored");
				}
				if (!ok)
				{
					return Status::error(where + "column " + column + ": cannot use '" + value + "'");
				}
			}
			def.volMax = volMax ? def.volMax : def.volMin;
			def.pitchMax = pitchMax ? def.pitchMax : def.pitchMin;
			def.distMax = distMax ? def.distMax : std::max(def.distMax, def.distMin);
			if (!typeGiven && !def.file.empty())
			{
				const std::string f = lower(def.file);
				def.streamed = f.size() > 4 && f.compare(f.size() - 4, 4, ".mp3") == 0;
			}
			out.push_back(std::move(def));
		}
		return {};
	}

	std::string write_sound_aliases(const std::span<const SoundAliasDef> aliases)
	{
		std::string out;
		for (std::size_t k = 0; k < std::size(OUT_COLUMNS); k++)
		{
			out += (k ? "," : "");
			out += OUT_COLUMNS[k];
		}
		out += "\n";
		for (const SoundAliasDef& d : aliases)
		{
			std::string reverb = d.fullDryLevel ? "fulldrylevel" : "";
			if (d.noWetLevel)
			{
				reverb += reverb.empty() ? "nowetlevel" : " nowetlevel";
			}
			char flags[16] = "";
			if (d.extraFlags)
			{
				std::snprintf(flags, sizeof flags, "0x%X", d.extraFlags);
			}
			const std::string fields[] = {
				d.name, d.sequence ? std::to_string(d.sequence) : "", d.file, float_text(d.volMin), float_text(d.volMax), d.volMod, float_text(d.pitchMin),
				float_text(d.pitchMax), float_text(d.distMin), float_text(d.distMax), d.velocityMin != 0 ? float_text(d.velocityMin) : "", d.channel,
				d.streamed ? "streamed" : "loaded", float_text(d.probability), d.rlooping ? "rlooping" : d.looping ? "looping" : "", "", "", d.subtitle, d.compression,
				d.secondaryAliasName, d.chainAliasName, d.volumeFalloffCurve, d.startDelay ? std::to_string(d.startDelay) : "", d.speakerMap, reverb,
				d.lfePercentage != 0 ? float_text(d.lfePercentage) : "", d.centerPercentage != 0 ? float_text(d.centerPercentage) : "", "",
				d.envelopMin != 0 ? float_text(d.envelopMin) : "", d.envelopMax != 0 ? float_text(d.envelopMax) : "",
				d.envelopPercentage != 0 ? float_text(d.envelopPercentage) : "", std::to_string(d.masterPriority), float_text(d.masterPercentage),
				float_text(d.slavePercentage), flags };
			for (std::size_t k = 0; k < std::size(fields); k++)
			{
				out += (k ? "," : "");
				out += csv_quote(fields[k]);
			}
			out += "\n";
		}
		return out;
	}

	Status parse_snd_curve(const std::string_view text, SndCurveDef& out)
	{
		std::vector<float> numbers;
		bool head = false;
		for (std::size_t at = 0; at < text.size();)
		{
			while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n' || text[at] == ','))
			{
				at++;
			}
			if (at + 1 < text.size() && text[at] == '/' && text[at + 1] == '/')
			{
				at = text.find('\n', at);
				at = at == std::string_view::npos ? text.size() : at;
				continue;
			}
			std::size_t end = at;
			while (end < text.size() && text[end] != ' ' && text[end] != '\t' && text[end] != '\r' && text[end] != '\n' && text[end] != ',')
			{
				end++;
			}
			if (end == at)
			{
				break;
			}
			const std::string token(text.substr(at, end - at));
			at = end;
			float f = 0;
			if (!head && numbers.empty() && lower(token) == "sndcurve")
			{
				head = true;
			}
			else if (to_float(token, f))
			{
				numbers.push_back(f);
			}
			else
			{
				return Status::error("sound curve: cannot use '" + token + "'");
			}
		}
		// the knot count in front of the pairs is optional
		if (numbers.size() % 2 == 1 && numbers[0] == static_cast<float>((numbers.size() - 1) / 2))
		{
			numbers.erase(numbers.begin());
		}
		if (numbers.size() % 2 != 0 || numbers.size() < 4 || numbers.size() > 32)
		{
			return Status::error("sound curve: 2 to 16 knots of x y are needed");
		}
		out.knots.clear();
		for (std::size_t k = 0; k < numbers.size(); k += 2)
		{
			out.knots.emplace_back(numbers[k], numbers[k + 1]);
		}
		return {};
	}

	std::string write_snd_curve(const SndCurveDef& curve)
	{
		std::string out = "SNDCURVE\n\n" + std::to_string(curve.knots.size()) + "\n";
		for (const auto& [x, y] : curve.knots)
		{
			out += float_text(x) + " " + float_text(y) + "\n";
		}
		return out;
	}

	SpeakerMapDef default_speaker_map()
	{
		// what every shipped default map holds: a mono source at half level in the two front speakers, a stereo
		// source left to left and right to right, nothing in the other four speakers of a 5.1 set
		SpeakerMapDef map;
		map.isDefault = true;
		for (int source = 0; source < 2; source++)
		{
			for (int set = 0; set < 2; set++)
			{
				c1::MSSChannelMap& m = map.channelMaps[source][set];
				m.speakerCount = set ? 6 : 2;
				for (int k = 0; k < m.speakerCount; k++)
				{
					m.speakers[k].speaker = k;
					m.speakers[k].numLevels = source + 1;
				}
				if (source == 0)
				{
					m.speakers[0].levels[0] = m.speakers[1].levels[0] = 0.5f;
				}
				else
				{
					m.speakers[0].levels[0] = m.speakers[1].levels[1] = 1.0f;
				}
			}
		}
		return map;
	}

	Status parse_speaker_map(const std::string_view text, SpeakerMapDef& out)
	{
		const std::string name = out.name;
		out = SpeakerMapDef{};
		out.name = name;
		c1::MSSChannelMap* current = nullptr;
		int levels = 0, blocks = 0;
		for (std::size_t at = 0; at < text.size();)
		{
			std::size_t end = text.find('\n', at);
			end = end == std::string_view::npos ? text.size() : end;
			std::string line(text.substr(at, end - at));
			at = end + 1;
			if (const std::size_t comment = line.find("//"); comment != std::string::npos)
			{
				line.erase(comment);
			}
			std::vector<std::string> tokens;
			for (std::size_t k = 0; k < line.size();)
			{
				while (k < line.size() && (line[k] == ' ' || line[k] == '\t' || line[k] == '\r'))
				{
					k++;
				}
				std::size_t e = k;
				while (e < line.size() && line[e] != ' ' && line[e] != '\t' && line[e] != '\r')
				{
					e++;
				}
				if (e > k)
				{
					tokens.push_back(lower(line.substr(k, e - k)));
				}
				k = e;
			}
			if (tokens.empty() || tokens[0] == "spkrmap")
			{
				continue;
			}
			if (tokens[0] == "default")
			{
				out.isDefault = true;
				continue;
			}
			if (tokens[0] == "map")
			{
				if (tokens.size() != 3 || (tokens[1] != "mono" && tokens[1] != "stereo") || (tokens[2] != "2" && tokens[2] != "6"))
				{
					return Status::error("speaker map: 'map <mono|stereo> <2|6>' expected");
				}
				levels = tokens[1] == "mono" ? 1 : 2;
				current = &out.channelMaps[levels - 1][tokens[2] == "6" ? 1 : 0];
				*current = c1::MSSChannelMap{};
				blocks++;
				continue;
			}
			std::int32_t speaker = 0;
			if (!current || tokens.size() != static_cast<std::size_t>(levels) + 1 || !to_int(tokens[0], speaker) || current->speakerCount >= 6)
			{
				return Status::error("speaker map: '<speaker> <level> [<level>]' inside a map block expected");
			}
			auto& s = current->speakers[current->speakerCount++];
			s.speaker = speaker;
			s.numLevels = levels;
			for (int k = 0; k < levels; k++)
			{
				if (!to_float(tokens[static_cast<std::size_t>(k) + 1], s.levels[k]))
				{
					return Status::error("speaker map: level '" + tokens[static_cast<std::size_t>(k) + 1] + "'");
				}
			}
		}
		return blocks == 4 ? Status{} : Status::error("speaker map: the four blocks map mono 2, mono 6, stereo 2, stereo 6 are needed");
	}

	std::string write_speaker_map(const SpeakerMapDef& map)
	{
		std::string out = "SPKRMAP\n";
		if (map.isDefault)
		{
			out += "default\n";
		}
		for (int source = 0; source < 2; source++)
		{
			for (int set = 0; set < 2; set++)
			{
				const c1::MSSChannelMap& m = map.channelMaps[source][set];
				out += std::string("map ") + (source ? "stereo " : "mono ") + (set ? "6" : "2") + "\n";
				for (int k = 0; k < m.speakerCount && k < 6; k++)
				{
					out += std::to_string(m.speakers[k].speaker);
					for (int l = 0; l <= source; l++)
					{
						out += " " + float_text(m.speakers[k].levels[l]);
					}
					out += "\n";
				}
			}
		}
		return out;
	}

	// =====================================================================================================
	// compile

	Status compile_loaded_sound(const std::string_view name, const WaveData& wave, CompiledLoadedSound& out)
	{
		if (wave.data.empty() || wave.channels < 1 || wave.channels > 2 || (wave.format != WAVE_PCM && wave.format != WAVE_IMA_ADPCM))
		{
			return Status::error("loaded sound '" + std::string(name) + "': no samples, or not PCM / IMA ADPCM, mono / stereo");
		}
		out = CompiledLoadedSound{};
		out.name = sound_file_name(name);
		out.data = wave.data;
		out.info.format = wave.format;
		out.info.dataLen = static_cast<std::uint32_t>(wave.data.size());
		out.info.rate = wave.rate;
		out.info.bits = wave.bits;
		out.info.channels = wave.channels;
		out.info.channelMask = 0xFFFFFFFFu;
		// PCM: every sample of every channel; ADPCM: frames of the whole blocks ( as the shipped sounds count )
		out.info.samples = wave.format == WAVE_PCM ? static_cast<std::uint32_t>(wave.data.size() / 2) : wave.frames();
		out.info.blockSize = wave.blockAlign;
		return {};
	}

	WaveData loaded_sound_wave(const CompiledLoadedSound& sound)
	{
		WaveData wave;
		wave.format = sound.info.format;
		wave.channels = sound.info.channels;
		wave.rate = sound.info.rate;
		wave.bits = sound.info.bits;
		wave.blockAlign = sound.info.blockSize;
		wave.data = sound.data;
		return wave;
	}

	Status compile_sound_aliases(const std::span<const SoundAliasDef> defs, const SoundLibrary& library, std::vector<CompiledAliasList>& out, std::vector<std::string>* notes)
	{
		std::map<std::string, std::size_t> index;
		for (const SoundAliasDef& d : defs)
		{
			const std::string where = "sound alias '" + d.name + "': ";
			if (d.name.empty())
			{
				return Status::error("sound alias without a name");
			}
			CompiledAlias a;
			a.aliasName = d.name;
			a.subtitle = d.subtitle;
			a.secondaryAliasName = d.secondaryAliasName;
			a.chainAliasName = d.chainAliasName;
			a.mixerGroup = d.volMod;
			a.hasFile = !d.noFile && !d.file.empty();
			a.streamed = d.streamed;
			const std::string file = sound_file_name(d.file);
			if (!a.hasFile && !d.noFile && notes)
			{
				notes->push_back(where + "no file: the alias plays nothing ( two shipped aliases are like this )");
			}
			if (a.streamed)
			{
				const std::size_t slash = file.rfind('/');
				a.streamDir = slash == std::string::npos ? "" : file.substr(0, slash);
				a.streamName = slash == std::string::npos ? file : file.substr(slash + 1);
			}
			else
			{
				a.loadedName = file;
			}
			const int channel = sound_channel_index(d.channel);
			if (channel < 0)
			{
				return Status::error(where + "channel '" + d.channel + "' is not in soundaliases/channels.def");
			}
			a.raw.sequence = d.sequence;
			a.raw.volMin = d.volMin;
			a.raw.volMax = d.volMax;
			a.raw.pitchMin = d.pitchMin;
			a.raw.pitchMax = d.pitchMax;
			a.raw.distMin = d.distMin;
			a.raw.distMax = d.distMax;
			a.raw.velocityMin = d.velocityMin;
			a.raw.flags = (d.looping || d.rlooping ? 1u : 0u) | (d.fullDryLevel ? 8u : 0u) | (d.noWetLevel ? 16u : 0u) | (d.rlooping ? 32u : 0u) | (d.extraFlags & 0x66u)
				| ((d.streamed ? c1::SAT_STREAMED : c1::SAT_LOADED) << 7) | (static_cast<std::uint32_t>(channel) << 9);
			a.raw.masterPriority = d.masterPriority;
			a.raw.masterPercentage = d.masterPercentage;
			a.raw.slavePercentage = d.slavePercentage;
			a.raw.probability = d.probability;
			a.raw.lfePercentage = d.lfePercentage;
			a.raw.centerPercentage = d.centerPercentage;
			a.raw.startDelay = d.startDelay;
			a.raw.envelopMin = d.envelopMin;
			a.raw.envelopMax = d.envelopMax;
			a.raw.envelopPercentage = d.envelopPercentage;
			a.curve = d.volumeFalloffCurve.empty() ? "$default" : d.volumeFalloffCurve;
			if (d.speakerMap.empty())
			{
				a.speakerMap = default_speaker_map();
			}
			else if (const auto it = library.speakerMaps.find(lower(d.speakerMap)); it != library.speakerMaps.end())
			{
				a.speakerMap = it->second;
				a.speakerMap.name = d.speakerMap;
			}
			else
			{
				return Status::error(where + "speaker map '" + d.speakerMap + "' has no definition ( soundaliases/" + d.speakerMap + ".spkrmap )");
			}
			if (d.volMin > d.volMax || d.pitchMin > d.pitchMax || d.distMin > d.distMax)
			{
				if (notes)
				{
					notes->push_back(where + "a minimum above its maximum ( volume, pitch or distance )");
				}
			}
			const std::string key = lower(d.name);
			auto [it, added] = index.emplace(key, out.size());
			if (added)
			{
				out.emplace_back();
				out.back().name = key;
			}
			out[it->second].aliases.push_back(std::move(a));
		}
		return {};
	}

	// =====================================================================================================
	// shipped assets

	bool extract_loaded_sound(const zone::ZoneEditor& editor, const zone::zaddr header, CompiledLoadedSound& out)
	{
		// The wave data is read into the temporary block ( the game hands it to Miles and frees it ), so the loaded
		// image does not keep it: it is taken from what the editor recorded of the zone stream.
		const zone::Zone& zone = editor.zone();
		c1::LoadedSound raw{};
		if (!zone.get(header, raw))
		{
			return false;
		}
		out = CompiledLoadedSound{};
		out.name = std::string(zone.c_string(raw.name));
		if (out.name.empty() || out.name.front() == ',')
		{
			return false;		// a reference: the data is in another zone
		}
		// in the stream: the header, the name when the header says it follows, the data
		const zone::ZoneDataId headerId = editor.header_data(header);
		zone::ZoneDataInfo info;
		c1::LoadedSound stream{};
		const std::span<const std::uint8_t> headerBytes = editor.data_bytes(headerId);
		if (!headerId || !editor.data_info(headerId, info) || headerBytes.size() != sizeof stream)
		{
			return false;
		}
		std::memcpy(&stream, headerBytes.data(), sizeof stream);
		if (!c1::zptr_is_inline(stream.sound.data))
		{
			return false;
		}
		const std::span<const std::uint8_t> data = editor.data_bytes(editor.asset_data(info.asset, info.index + (stream.name == c1::ZPTR_INLINE ? 2 : 1)));
		if (data.size() != raw.sound.dataLen)
		{
			return false;
		}
		out.info = raw.sound;
		out.info.data = out.info.dataPtr = out.info.initialPtr = 0;
		out.data.assign(data.begin(), data.end());
		return true;
	}

	bool extract_snd_curve(const zone::Zone& zone, const zone::zaddr header, SndCurveDef& out)
	{
		c1::SndCurve raw{};
		if (!zone.get(header, raw))
		{
			return false;
		}
		out.name = std::string(zone.c_string(raw.filename));
		out.knots.clear();
		if (!out.name.empty() && out.name.front() == ',')
		{
			out.name.erase(out.name.begin());
			return false;
		}
		for (int k = 0; k < raw.knotCount && k < 16; k++)
		{
			out.knots.emplace_back(raw.knots[k][0], raw.knots[k][1]);
		}
		return true;
	}

	bool extract_sound_alias_list(const zone::Zone& zone, const zone::zaddr header, CompiledAliasList& out)
	{
		c1::snd_alias_list_t raw{};
		if (!zone.get(header, raw) || raw.count < 0 || raw.count > 4096)
		{
			return false;
		}
		out = CompiledAliasList{};
		out.name = std::string(zone.c_string(raw.aliasName));
		if (!out.name.empty() && out.name.front() == ',')
		{
			return false;
		}
		const auto items = zone.get_array<c1::snd_alias_t>(raw.head, static_cast<std::size_t>(raw.count));
		if (items.size() != static_cast<std::size_t>(raw.count))
		{
			return false;
		}
		const auto name_of = [&](const zone::zaddr at)
		{
			std::string n(zone.c_string(zone.get<c1::zptr>(at)));		// name pointer at offset 0 of the three asset types
			if (!n.empty() && n.front() == ',')
			{
				n.erase(n.begin());
			}
			return n;
		};
		for (const c1::snd_alias_t& item : items)
		{
			CompiledAlias a;
			a.raw = item;
			a.aliasName = std::string(zone.c_string(item.aliasName));
			a.subtitle = std::string(zone.c_string(item.subtitle));
			a.secondaryAliasName = std::string(zone.c_string(item.secondaryAliasName));
			a.chainAliasName = std::string(zone.c_string(item.chainAliasName));
			a.mixerGroup = std::string(zone.c_string(item.mixerGroup));
			a.hasFile = item.soundFile != 0;
			a.streamed = ((item.flags >> 7) & 3) == c1::SAT_STREAMED;
			if (a.hasFile)
			{
				const auto file = zone.get<c1::SoundFile>(item.soundFile);
				a.streamed = file.type == c1::SAT_STREAMED;
				if (a.streamed)
				{
					a.streamDir = std::string(zone.c_string(file.loadSndOrDir));
					a.streamName = std::string(zone.c_string(file.streamName));
				}
				else
				{
					a.loadedName = name_of(file.loadSndOrDir);
				}
			}
			a.curve = item.volumeFalloffCurve ? name_of(item.volumeFalloffCurve) : std::string();
			if (item.speakerMap)
			{
				const auto map = zone.get<c1::SpeakerMap>(item.speakerMap);
				a.speakerMap.name = std::string(zone.c_string(map.name));
				a.speakerMap.isDefault = map.isDefault != 0;
				std::memcpy(a.speakerMap.channelMaps, map.channelMaps, sizeof map.channelMaps);
			}
			a.raw.aliasName = a.raw.subtitle = a.raw.secondaryAliasName = a.raw.chainAliasName = a.raw.mixerGroup = a.raw.soundFile = 0;
			a.raw.volumeFalloffCurve = a.raw.speakerMap = 0;
			out.aliases.push_back(std::move(a));
		}
		return true;
	}

	std::vector<SoundAliasDef> alias_list_defs(const CompiledAliasList& list)
	{
		std::vector<SoundAliasDef> out;
		for (const CompiledAlias& a : list.aliases)
		{
			SoundAliasDef d;
			d.name = a.aliasName.empty() ? list.name : a.aliasName;
			d.sequence = a.raw.sequence;
			d.streamed = a.streamed;
			d.noFile = !a.hasFile;
			d.file = !a.hasFile ? std::string() : a.streamed ? (a.streamDir.empty() ? a.streamName : a.streamDir + "/" + a.streamName) : a.loadedName;
			d.volMin = a.raw.volMin;
			d.volMax = a.raw.volMax;
			d.volMod = a.mixerGroup;
			d.pitchMin = a.raw.pitchMin;
			d.pitchMax = a.raw.pitchMax;
			d.distMin = a.raw.distMin;
			d.distMax = a.raw.distMax;
			d.velocityMin = a.raw.velocityMin;
			d.channel = std::string(sound_channel_name(static_cast<int>((a.raw.flags >> 9) & 0x3F)));
			d.probability = a.raw.probability;
			d.looping = (a.raw.flags & 1) != 0;
			d.rlooping = (a.raw.flags & 0x21) == 0x21;
			d.fullDryLevel = (a.raw.flags & 8) != 0;
			d.noWetLevel = (a.raw.flags & 16) != 0;
			d.extraFlags = (a.raw.flags & 0x46u) | ((a.raw.flags & 0x21u) == 0x20u ? 0x20u : 0u);
			d.masterPriority = a.raw.masterPriority;
			d.masterPercentage = a.raw.masterPercentage;
			d.slavePercentage = a.raw.slavePercentage;
			d.subtitle = a.subtitle;
			d.secondaryAliasName = a.secondaryAliasName;
			d.chainAliasName = a.chainAliasName;
			d.volumeFalloffCurve = a.curve == "$default" ? std::string() : a.curve;
			d.startDelay = a.raw.startDelay;
			d.speakerMap = a.speakerMap.name;
			d.lfePercentage = a.raw.lfePercentage;
			d.centerPercentage = a.raw.centerPercentage;
			d.envelopMin = a.raw.envelopMin;
			d.envelopMax = a.raw.envelopMax;
			d.envelopPercentage = a.raw.envelopPercentage;
			out.push_back(std::move(d));
		}
		return out;
	}

	// =====================================================================================================
	// linking

	Status link_loaded_sound(ZoneEditor& editor, const CompiledLoadedSound& sound)
	{
		if (editor.find_asset(c1::ASSET_TYPE_LOADED_SOUND, sound.name) != ZoneEditor::npos)
		{
			return Status::error("the zone already has loaded_sound '" + sound.name + "'");
		}
		if (sound.data.empty() || sound.data.size() != sound.info.dataLen)
		{
			return Status::error("loaded sound '" + sound.name + "': no data");
		}
		Pieces pieces;
		c1::LoadedSound header{};
		header.name = c1::ZPTR_INLINE;
		header.sound = sound.info;
		header.sound.dataPtr = header.sound.initialPtr = 0;
		header.sound.data = c1::ZPTR_INLINE;
		pieces.add(header);
		pieces.add_string(sound.name);
		pieces.add(sound.data.data(), sound.data.size());
		return editor.add_new_asset(c1::ASSET_TYPE_LOADED_SOUND, sound.name, pieces.list);
	}

	Status link_snd_curve(ZoneEditor& editor, const SndCurveDef& curve)
	{
		if (editor.find_asset(c1::ASSET_TYPE_SOUND_CURVE, curve.name) != ZoneEditor::npos)
		{
			return Status::error("the zone already has sndcurve '" + curve.name + "'");
		}
		if (curve.knots.size() < 2 || curve.knots.size() > 16)
		{
			return Status::error("sound curve '" + curve.name + "': 2 to 16 knots");
		}
		Pieces pieces;
		c1::SndCurve header{};
		header.filename = c1::ZPTR_INLINE;
		header.knotCount = static_cast<std::uint16_t>(curve.knots.size());
		for (std::size_t k = 0; k < curve.knots.size(); k++)
		{
			header.knots[k][0] = curve.knots[k].first;
			header.knots[k][1] = curve.knots[k].second;
		}
		pieces.add(header);
		pieces.add_string(curve.name);
		return editor.add_new_asset(c1::ASSET_TYPE_SOUND_CURVE, curve.name, pieces.list);
	}

	Status link_sound_alias_list(ZoneEditor& editor, const CompiledAliasList& list, std::vector<std::string>* log)
	{
		if (editor.find_asset(c1::ASSET_TYPE_SOUND, list.name) != ZoneEditor::npos)
		{
			return Status::error("the zone already has sound '" + list.name + "'");
		}
		if (list.aliases.empty() || list.name.empty())
		{
			return Status::error("sound alias list '" + list.name + "' is empty");
		}
		Pieces pieces;
		c1::snd_alias_list_t header{};
		header.aliasName = c1::ZPTR_INLINE;
		header.head = c1::ZPTR_INLINE;
		header.count = static_cast<std::int32_t>(list.aliases.size());
		pieces.add(header);
		pieces.add_string(list.name);
		std::vector<c1::snd_alias_t> items;
		for (const CompiledAlias& a : list.aliases)
		{
			c1::snd_alias_t raw = a.raw;
			raw.aliasName = c1::ZPTR_INLINE;
			raw.subtitle = a.subtitle.empty() ? 0 : c1::ZPTR_INLINE;
			raw.secondaryAliasName = a.secondaryAliasName.empty() ? 0 : c1::ZPTR_INLINE;
			raw.chainAliasName = a.chainAliasName.empty() ? 0 : c1::ZPTR_INLINE;
			raw.mixerGroup = a.mixerGroup.empty() ? 0 : c1::ZPTR_INLINE;
			raw.soundFile = a.hasFile ? c1::ZPTR_INLINE : 0;
			raw.volumeFalloffCurve = c1::ZPTR_INLINE;
			raw.speakerMap = c1::ZPTR_INLINE;
			items.push_back(raw);
		}
		const std::uint32_t array = pieces.add(items.data(), items.size() * sizeof(c1::snd_alias_t));
		std::vector<std::pair<const SpeakerMapDef*, std::uint32_t>> maps;		// maps written so far and their piece
		for (std::size_t k = 0; k < list.aliases.size(); k++)
		{
			const CompiledAlias& a = list.aliases[k];
			const auto base = static_cast<std::uint32_t>(k * sizeof(c1::snd_alias_t));
			pieces.add_string(a.aliasName);
			for (const std::string* text : { &a.subtitle, &a.secondaryAliasName, &a.chainAliasName, &a.mixerGroup })
			{
				if (!text->empty())
				{
					pieces.add_string(*text);
				}
			}
			if (a.hasFile)
			{
				c1::SoundFile file{};
				file.type = a.streamed ? c1::SAT_STREAMED : c1::SAT_LOADED;
				file.exists = 1;
				file.loadSndOrDir = c1::ZPTR_INLINE;
				file.streamName = a.streamed ? c1::ZPTR_INLINE : 0;
				const std::uint32_t filePiece = pieces.add(file);
				if (a.streamed)
				{
					pieces.add_string(a.streamDir);
					pieces.add_string(a.streamName);
				}
				else if (const std::size_t asset = editor.find_asset(c1::ASSET_TYPE_LOADED_SOUND, a.loadedName); asset != ZoneEditor::npos)
				{
					pieces.to_asset(filePiece, offsetof(c1::SoundFile, loadSndOrDir), asset);
				}
				else
				{
					c1::LoadedSound reference{};
					reference.name = c1::ZPTR_INLINE;
					pieces.add(reference);
					pieces.add_string("," + a.loadedName);
					if (log)
					{
						log->push_back("sound " + list.name + ": loaded sound " + a.loadedName + " is not in the zone, referenced by name");
					}
				}
			}
			if (const std::size_t asset = editor.find_asset(c1::ASSET_TYPE_SOUND_CURVE, a.curve); asset != ZoneEditor::npos)
			{
				pieces.to_asset(array, base + static_cast<std::uint32_t>(offsetof(c1::snd_alias_t, volumeFalloffCurve)), asset);
			}
			else
			{
				c1::SndCurve reference{};
				reference.filename = c1::ZPTR_INLINE;
				pieces.add(reference);
				pieces.add_string("," + a.curve);
			}
			const auto known = std::find_if(maps.begin(), maps.end(), [&](const auto& m) { return same_map(*m.first, a.speakerMap); });
			if (known != maps.end())
			{
				pieces.to_piece(array, base + static_cast<std::uint32_t>(offsetof(c1::snd_alias_t, speakerMap)), known->second);
			}
			else
			{
				c1::SpeakerMap map{};
				map.isDefault = a.speakerMap.isDefault ? 1 : 0;
				map.name = c1::ZPTR_INLINE;
				std::memcpy(map.channelMaps, a.speakerMap.channelMaps, sizeof map.channelMaps);
				maps.emplace_back(&a.speakerMap, pieces.add(map));
				pieces.add_string(a.speakerMap.name);
			}
		}
		return editor.add_new_asset(c1::ASSET_TYPE_SOUND, list.name, pieces.list);
	}

	// =====================================================================================================
	// a whole alias file

	std::filesystem::path sound_source_root(const std::filesystem::path& csvFile)
	{
		const std::filesystem::path folder = csvFile.parent_path();
		return lower(folder.filename().string()) == "soundaliases" ? folder.parent_path() : folder;
	}

	Status build_sound_aliases(const std::filesystem::path& csvFile, const std::filesystem::path& root, ZoneEditor& editor, SoundBuild& out)
	{
		namespace fs = std::filesystem;
		std::vector<std::uint8_t> bytes;
		if (const Status s = zone::read_file(csvFile, bytes); !s)
		{
			return s;
		}
		std::vector<SoundAliasDef> defs;
		const std::string file = csvFile.filename().string();
		const std::size_t notesFrom = out.log.size();
		if (const Status s = parse_sound_aliases({ reinterpret_cast<const char*>(bytes.data()), bytes.size() }, defs, &out.log); !s)
		{
			return Status::error(file + ": " + s.message);
		}
		std::error_code ec;
		SoundLibrary library;
		std::map<std::string, bool> adpcm;		// loaded file -> a row asks for ADPCM
		for (const SoundAliasDef& d : defs)
		{
			const std::string key = lower(d.speakerMap);
			if (!key.empty() && !library.speakerMaps.count(key))
			{
				const fs::path source = root / "soundaliases" / (d.speakerMap + ".spkrmap");
				SpeakerMapDef map;
				if (!zone::read_file(source, bytes))
				{
					return Status::error(file + ": alias '" + d.name + "' names the speaker map '" + d.speakerMap + "': " + source.string() + " is needed");
				}
				if (const Status s = parse_speaker_map({ reinterpret_cast<const char*>(bytes.data()), bytes.size() }, map); !s)
				{
					return Status::error(source.string() + ": " + s.message);
				}
				map.name = d.speakerMap;
				library.speakerMaps.emplace(key, std::move(map));
			}
			if (!d.streamed && !d.file.empty())
			{
				if (d.compression != "" && d.compression != "adpcm" && d.compression != "pcm")
				{
					out.log.push_back(file + ": alias '" + d.name + "': compression '" + d.compression + "' is not known ( adpcm, pcm ); the file's own format is kept");
				}
				adpcm[sound_file_name(d.file)] |= d.compression == "adpcm";
			}
		}
		std::vector<CompiledAliasList> lists;
		if (const Status s = compile_sound_aliases(defs, library, lists, &out.log); !s)
		{
			return Status::error(file + ": " + s.message);
		}
		for (std::size_t k = notesFrom; k < out.log.size(); k++)
		{
			if (out.log[k].rfind(file, 0) != 0)
			{
				out.log[k] = file + ": " + out.log[k];
			}
		}
		for (const CompiledAliasList& list : lists)
		{
			for (const CompiledAlias& a : list.aliases)
			{
				if (a.hasFile && a.streamed)
				{
					const std::string relative = a.streamDir.empty() ? a.streamName : a.streamDir + "/" + a.streamName;
					const bool known = std::any_of(out.streamedFiles.begin(), out.streamedFiles.end(), [&](const auto& f) { return f.first == "sound/" + relative; });
					if (!known && zone::read_file(root / "sound" / fs::path(relative), bytes))
					{
						out.streamedFiles.emplace_back("sound/" + relative, bytes);
					}
					else if (!known)
					{
						out.log.push_back("sound " + list.name + ": streamed file sound/" + relative + " is not in the source tree; the game looks for it in the archives it has");
					}
				}
				else if (a.hasFile && editor.find_asset(c1::ASSET_TYPE_LOADED_SOUND, a.loadedName) == ZoneEditor::npos)
				{
					const fs::path source = root / "sound" / fs::path(a.loadedName);
					if (fs::exists(source, ec))
					{
						WaveData wave;
						if (const Status s = load_wav(source, wave); !s)
						{
							return s;
						}
						if (adpcm[a.loadedName] && wave.format == WAVE_PCM)
						{
							WaveData coded;
							if (const Status s = ima_adpcm_encode(wave, 0, coded); !s)
							{
								return Status::error(source.string() + ": " + s.message);
							}
							coded.notes = wave.notes;
							wave = std::move(coded);
						}
						CompiledLoadedSound sound;
						Status s = compile_loaded_sound(a.loadedName, wave, sound);
						s = s ? link_loaded_sound(editor, sound) : s;
						if (!s)
						{
							return Status::error(source.string() + ": " + s.message);
						}
						out.loadedSounds++;
						std::string line = "loaded sound " + sound.name + ": " + (wave.format == WAVE_PCM ? "16 bit PCM" : "IMA ADPCM, block " + std::to_string(wave.blockAlign)) + ", "
							+ std::to_string(wave.channels) + " channel(s), " + std::to_string(wave.rate) + " Hz, " + std::to_string(wave.frames()) + " frames, "
							+ std::to_string(wave.data.size()) + " bytes";
						for (const std::string& note : wave.notes)
						{
							line += "; " + note;
						}
						out.log.push_back(std::move(line));
					}
					// else: link_sound_alias_list references it by name and says so
				}
				if (editor.find_asset(c1::ASSET_TYPE_SOUND_CURVE, a.curve) == ZoneEditor::npos && zone::read_file(root / "soundaliases" / (a.curve + ".vfcurve"), bytes))
				{
					SndCurveDef curve;
					Status s = parse_snd_curve({ reinterpret_cast<const char*>(bytes.data()), bytes.size() }, curve);
					curve.name = a.curve;
					s = s ? link_snd_curve(editor, curve) : s;
					if (!s)
					{
						return Status::error("soundaliases/" + a.curve + ".vfcurve: " + s.message);
					}
					out.curves++;
					out.log.push_back("sound curve " + a.curve + ": " + std::to_string(curve.knots.size()) + " knots");
				}
			}
			if (const Status s = link_sound_alias_list(editor, list, &out.log); !s)
			{
				return Status::error(file + ": " + s.message);
			}
			out.lists++;
			out.aliases += static_cast<std::uint32_t>(list.aliases.size());
		}
		return {};
	}
}
