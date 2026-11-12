// AssetLinker::add_anim: a compiled animation as the pieces of zone stream the game's XAnimParts loader reads.
//
// Stream order ( codol_asset_probe graph on shipped animations ): header, name, bone names ( script strings ),
// notifies, the delta part - its three pointers, then the translation ( 4 byte head; 28 bytes box and frame pointer,
// the frame table, the frames; or 12 bytes for a constant ), the rotation about z and the full rotation ( 4 byte
// head; frame pointer, frame table, frames; or the one value ) - then dataByte, dataShort, dataInt,
// randomDataShort, randomDataByte, randomDataInt, indices.

#include "codol_assets/link.hpp"

#include <cstring>

namespace codol::assets
{
	namespace
	{
		using zone::ZoneNewPiece;

		ZoneNewPiece& add_piece(std::vector<ZoneNewPiece>& pieces, const void* data, const std::size_t size)
		{
			ZoneNewPiece piece;
			piece.bytes.assign(static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
			pieces.push_back(std::move(piece));
			return pieces.back();
		}

		template <typename T>
		void add_stream(std::vector<ZoneNewPiece>& pieces, const std::vector<T>& values)
		{
			if (!values.empty())
			{
				add_piece(pieces, values.data(), values.size() * sizeof(T));
			}
		}

		std::vector<std::uint8_t> low_bytes(const std::vector<std::uint16_t>& values)
		{
			std::vector<std::uint8_t> out(values.size());
			for (std::size_t k = 0; k < values.size(); k++)
			{
				out[k] = static_cast<std::uint8_t>(values[k]);
			}
			return out;
		}

		// a frame table of a delta track: u8 below 256 frames, else u16
		void add_indices(std::vector<ZoneNewPiece>& pieces, const std::vector<std::uint16_t>& indices, const bool wide)
		{
			if (wide)
			{
				add_stream(pieces, indices);
			}
			else
			{
				add_stream(pieces, low_bytes(indices));
			}
		}
	}

	Status AssetLinker::add_anim(const CompiledAnim& anim)
	{
		const RawAnim& raw = anim.raw;
		if (editor().find_asset(c1::ASSET_TYPE_XANIMPARTS, raw.name) != zone::ZoneEditor::npos)
		{
			return Status::error("the zone already has xanim '" + raw.name + "'");
		}
		const bool wide = raw.header.numframes >= 256;
		std::vector<ZoneNewPiece> pieces;

		c1::XAnimParts header = raw.header;
		const auto follows = [](const bool present) { return present ? c1::ZPTR_INLINE : 0u; };
		header.name = c1::ZPTR_INLINE;
		header.names = follows(!raw.boneNames.empty());
		header.notify = follows(!raw.notifies.empty());
		header.deltaPart = follows(raw.hasDelta);
		header.dataByte = follows(!raw.dataByte.empty());
		header.dataShort = follows(!raw.dataShort.empty());
		header.dataInt = follows(!raw.dataInt.empty());
		header.randomDataShort = follows(!raw.randomDataShort.empty());
		header.randomDataByte = follows(!raw.randomDataByte.empty());
		header.randomDataInt = follows(!raw.randomDataInt.empty());
		header.indices = follows(!raw.indices.empty());
		add_piece(pieces, &header, sizeof(header));

		{
			ZoneNewPiece name;
			name.string = true;
			name.bytes.assign(raw.name.begin(), raw.name.end());
			name.bytes.push_back(0);
			pieces.push_back(std::move(name));
		}
		if (!raw.boneNames.empty())
		{
			const std::vector<std::uint16_t> zero(raw.boneNames.size(), 0);
			ZoneNewPiece& names = add_piece(pieces, zero.data(), zero.size() * 2);
			for (std::size_t k = 0; k < raw.boneNames.size(); k++)
			{
				names.scriptStrings.emplace_back(static_cast<std::uint32_t>(k * 2), raw.boneNames[k]);
			}
		}
		if (!raw.notifies.empty())
		{
			std::vector<c1::XAnimNotifyInfo> notify(raw.notifies.size());
			for (std::size_t k = 0; k < notify.size(); k++)
			{
				notify[k] = {};
				notify[k].time = raw.notifies[k].second;
			}
			ZoneNewPiece& piece = add_piece(pieces, notify.data(), notify.size() * sizeof(c1::XAnimNotifyInfo));
			for (std::size_t k = 0; k < notify.size(); k++)
			{
				piece.scriptStrings.emplace_back(static_cast<std::uint32_t>(k * sizeof(c1::XAnimNotifyInfo)), raw.notifies[k].first);
			}
		}
		if (raw.hasDelta)
		{
			const std::uint32_t part[3] = { follows(raw.deltaTrans.present), follows(raw.deltaQuat2.present), follows(raw.deltaQuat.present) };
			add_piece(pieces, part, sizeof(part));
			if (raw.deltaTrans.present)
			{
				const RawDeltaTrans& t = raw.deltaTrans;
				const std::uint8_t head[4] = { static_cast<std::uint8_t>(t.size & 0xFF), static_cast<std::uint8_t>(t.size >> 8), static_cast<std::uint8_t>(t.size && t.small ? 1 : 0), 0 };
				add_piece(pieces, head, 4);
				if (t.size == 0)
				{
					add_piece(pieces, t.frame0, 12);
				}
				else
				{
					std::uint8_t box[28];
					std::memcpy(box, t.mins, 12);
					std::memcpy(box + 12, t.frameSize, 12);
					const std::uint32_t frames = c1::ZPTR_INLINE;
					std::memcpy(box + 24, &frames, 4);
					add_piece(pieces, box, sizeof(box));
					add_indices(pieces, t.indices, wide);
					if (t.small)
					{
						add_stream(pieces, low_bytes(t.frames));
					}
					else
					{
						add_stream(pieces, t.frames);
					}
				}
			}
			for (const RawDeltaQuat* q : { &raw.deltaQuat2, &raw.deltaQuat })
			{
				if (!q->present)
				{
					continue;
				}
				const std::uint8_t head[4] = { static_cast<std::uint8_t>(q->size & 0xFF), static_cast<std::uint8_t>(q->size >> 8), 0, 0 };
				add_piece(pieces, head, 4);
				if (q->size == 0)
				{
					add_stream(pieces, q->frames);		// the one value: 2 or 4 s16
				}
				else
				{
					const std::uint32_t frames = c1::ZPTR_INLINE;
					add_piece(pieces, &frames, 4);
					add_indices(pieces, q->indices, wide);
					add_stream(pieces, q->frames);
				}
			}
		}
		add_stream(pieces, raw.dataByte);
		add_stream(pieces, raw.dataShort);
		add_stream(pieces, raw.dataInt);
		add_stream(pieces, raw.randomDataShort);
		add_stream(pieces, raw.randomDataByte);
		add_stream(pieces, raw.randomDataInt);
		add_stream(pieces, raw.indices);
		return editor().add_new_asset(c1::ASSET_TYPE_XANIMPARTS, raw.name, pieces);
	}
}
