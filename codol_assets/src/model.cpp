// Model compiler: xmodel_export LODs -> bones, packed surfaces, bounds, collision surfaces.

#include "codol_assets/model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numeric>

namespace codol::assets
{
	namespace
	{
		struct Quat
		{
			float x = 0, y = 0, z = 0, w = 1;
		};

		Quat multiply(const Quat& a, const Quat& b)
		{
			return { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
				a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
		}

		Quat conjugate(const Quat& q) { return { -q.x, -q.y, -q.z, q.w }; }

		void rotate(const Quat& q, const float v[3], float out[3])
		{
			const Quat p{ v[0], v[1], v[2], 0 };
			const Quat r = multiply(multiply(q, p), conjugate(q));
			out[0] = r.x;
			out[1] = r.y;
			out[2] = r.z;
		}

		// axis[i]: image of unit vector i ( the rows of an export's X / Y / Z lines )
		Quat quat_of_axis(const float axis[3][3])
		{
			const float m00 = axis[0][0], m10 = axis[0][1], m20 = axis[0][2];
			const float m01 = axis[1][0], m11 = axis[1][1], m21 = axis[1][2];
			const float m02 = axis[2][0], m12 = axis[2][1], m22 = axis[2][2];
			Quat q;
			const float trace = m00 + m11 + m22;
			if (trace > 0)
			{
				const float s = std::sqrt(trace + 1.0f) * 2;
				q = { (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s };
			}
			else if (m00 > m11 && m00 > m22)
			{
				const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2;
				q = { 0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s };
			}
			else if (m11 > m22)
			{
				const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2;
				q = { (m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s };
			}
			else
			{
				const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2;
				q = { (m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m10 - m01) / s };
			}
			const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
			if (length < 1e-9f)
			{
				return {};
			}
			return { q.x / length, q.y / length, q.z / length, q.w / length };
		}

		void cross(const float a[3], const float b[3], float out[3])
		{
			out[0] = a[1] * b[2] - a[2] * b[1];
			out[1] = a[2] * b[0] - a[0] * b[2];
			out[2] = a[0] * b[1] - a[1] * b[0];
		}

		float dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

		bool normalize(float v[3])
		{
			const float length = std::sqrt(dot(v, v));
			if (length < 1e-12f)
			{
				return false;
			}
			v[0] /= length;
			v[1] /= length;
			v[2] /= length;
			return true;
		}

		struct BoundsBuilder
		{
			float mins[3] = { 1e30f, 1e30f, 1e30f }, maxs[3] = { -1e30f, -1e30f, -1e30f };
			bool any = false;
			void add(const float p[3])
			{
				any = true;
				for (int k = 0; k < 3; k++)
				{
					mins[k] = std::min(mins[k], p[k]);
					maxs[k] = std::max(maxs[k], p[k]);
				}
			}
			c1::Bounds bounds() const
			{
				c1::Bounds b{};
				for (int k = 0; any && k < 3; k++)
				{
					b.midPoint[k] = (mins[k] + maxs[k]) * 0.5f;
					b.halfSize[k] = (maxs[k] - mins[k]) * 0.5f;
				}
				return b;
			}
		};

		void set_part_bit(std::uint32_t bits[8], const int bone)
		{
			bits[(bone >> 5) & 7] |= 0x80000000u >> (bone & 31);
		}

		// The engine's front faces: with the corners in stored order, cross( v1 - v0, v2 - v0 ) points against the
		// surface normal ( measured on the shipped models, see codol_asset_verify model-roundtrip ).
		constexpr bool STORED_NORMAL_AGAINST_CROSS = true;
	}

	std::uint32_t pack_unit_vector(const float v[3])
	{
		// decoded as ( byte - 127 ) * ( scale byte + 192 ) / 32385: try every scale, keep the closest
		std::uint32_t best = 0;
		float bestError = 1e30f;
		for (int scaleByte = 0; scaleByte < 256; scaleByte++)
		{
			const float scale = (static_cast<float>(scaleByte) + 192.0f) / 32385.0f;
			int b[3];
			float error = 0;
			for (int k = 0; k < 3; k++)
			{
				b[k] = std::clamp(static_cast<int>(std::lround(v[k] / scale + 127.0f)), 0, 255);
				const float d = (static_cast<float>(b[k]) - 127.0f) * scale - v[k];
				error += d * d;
			}
			if (error < bestError)
			{
				bestError = error;
				best = static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) | (static_cast<std::uint32_t>(b[2]) << 16)
					| (static_cast<std::uint32_t>(scaleByte) << 24);
			}
		}
		return best;
	}

	std::uint16_t float_to_half(const float value)
	{
		std::uint32_t bits;
		std::memcpy(&bits, &value, 4);
		const std::uint32_t sign = (bits >> 16) & 0x8000u;
		const int exponent = static_cast<int>((bits >> 23) & 0xFF) - 127 + 15;
		std::uint32_t mantissa = bits & 0x7FFFFFu;
		if (exponent >= 31)
		{
			return static_cast<std::uint16_t>(sign | 0x7BFFu);		// clamp to the largest half
		}
		if (exponent <= 0)
		{
			if (exponent < -10)
			{
				return static_cast<std::uint16_t>(sign);
			}
			mantissa |= 0x800000u;
			const int shift = 14 - exponent;
			std::uint32_t half = mantissa >> shift;
			if ((mantissa >> (shift - 1)) & 1)
			{
				half++;
			}
			return static_cast<std::uint16_t>(sign | half);
		}
		std::uint32_t half = (static_cast<std::uint32_t>(exponent) << 10) | (mantissa >> 13);
		if (mantissa & 0x1000u)
		{
			half++;		// round to nearest; a carry runs into the exponent, which is correct
		}
		return static_cast<std::uint16_t>(sign | half);
	}

	std::vector<std::string> CompiledModel::materials() const
	{
		std::vector<std::string> out;
		for (const CompiledLod& lod : lods)
		{
			for (const CompiledSurface& s : lod.surfaces)
			{
				out.push_back(s.material);
			}
		}
		return out;
	}

	Status compile_model(const ModelOptions& options, CompiledModel& out)
	try
	{
		out = {};
		out.name = options.name;
		if (options.name.empty() || options.lods.empty() || options.lods.size() > 4)
		{
			return Status::error("a model needs a name and one to four LODs");
		}
		const ExportModel& first = options.lods[0].mesh;
		const std::size_t boneCount = first.bones.size();
		if (boneCount == 0 || boneCount > 255)
		{
			return Status::error("model '" + options.name + "': 1 .. 255 bones");
		}
		for (const ModelLodSource& lod : options.lods)
		{
			if (lod.mesh.bones.size() != boneCount)
			{
				return Status::error("model '" + options.name + "': every LOD needs the bones of the first ( " + std::to_string(boneCount) + " )");
			}
			for (std::size_t k = 0; k < boneCount; k++)
			{
				if (lod.mesh.bones[k].name != first.bones[k].name)
				{
					return Status::error("model '" + options.name + "': bone " + std::to_string(k) + " differs between the LODs");
				}
			}
		}

		// ---- bones: roots first, every parent in front of its children
		std::vector<int> order;		// new index -> export index
		{
			std::vector<bool> placed(boneCount, false);
			for (std::size_t k = 0; k < boneCount; k++)
			{
				if (first.bones[k].parent < 0)
				{
					order.push_back(static_cast<int>(k));
					placed[k] = true;
				}
			}
			out.rootBones = static_cast<std::uint8_t>(order.size());
			for (bool progress = true; progress && order.size() < boneCount;)
			{
				progress = false;
				for (std::size_t k = 0; k < boneCount; k++)
				{
					const int parent = first.bones[k].parent;
					if (!placed[k] && parent >= 0 && static_cast<std::size_t>(parent) < boneCount && placed[static_cast<std::size_t>(parent)])
					{
						order.push_back(static_cast<int>(k));
						placed[k] = true;
						progress = true;
					}
				}
			}
			if (order.size() != boneCount || out.rootBones == 0)
			{
				return Status::error("model '" + options.name + "': the bone hierarchy has a loop or no root");
			}
		}
		std::vector<int> remap(boneCount);		// export index -> new index
		for (std::size_t k = 0; k < boneCount; k++)
		{
			remap[static_cast<std::size_t>(order[k])] = static_cast<int>(k);
		}
		std::vector<Quat> worldQuat(boneCount);
		std::vector<std::array<float, 3>> worldTrans(boneCount);
		for (std::size_t k = 0; k < boneCount; k++)
		{
			const ExportBone& bone = first.bones[static_cast<std::size_t>(order[k])];
			out.boneNames.push_back(bone.name);
			worldQuat[k] = quat_of_axis(bone.axis);
			worldTrans[k] = { bone.offset[0], bone.offset[1], bone.offset[2] };
			out.baseMat.insert(out.baseMat.end(), { worldQuat[k].x, worldQuat[k].y, worldQuat[k].z, worldQuat[k].w, bone.offset[0], bone.offset[1], bone.offset[2], 2.0f });
			out.partClassification.push_back(0);
			if (k >= out.rootBones)
			{
				const int parent = remap[static_cast<std::size_t>(bone.parent)];
				if (static_cast<int>(k) - parent > 255)
				{
					return Status::error("model '" + options.name + "': bone '" + bone.name + "' is more than 255 bones behind its parent");
				}
				out.parentList.push_back(static_cast<std::uint8_t>(static_cast<int>(k) - parent));
				const Quat inverse = conjugate(worldQuat[static_cast<std::size_t>(parent)]);
				Quat local = multiply(inverse, worldQuat[k]);
				if (local.w < 0)
				{
					local = { -local.x, -local.y, -local.z, -local.w };
				}
				for (const float c : { local.x, local.y, local.z, local.w })
				{
					out.quats.push_back(static_cast<std::int16_t>(std::clamp(std::lround(c * 32767.0f), -32767l, 32767l)));
				}
				const float delta[3] = { bone.offset[0] - worldTrans[static_cast<std::size_t>(parent)][0], bone.offset[1] - worldTrans[static_cast<std::size_t>(parent)][1],
					bone.offset[2] - worldTrans[static_cast<std::size_t>(parent)][2] };
				float localTrans[3];
				rotate(inverse, delta, localTrans);
				out.trans.insert(out.trans.end(), localTrans, localTrans + 3);
			}
		}

		// ---- LODs
		std::vector<BoundsBuilder> boneBounds(boneCount);
		BoundsBuilder modelBounds;
		float radiusSquared = 0;
		for (std::size_t l = 0; l < options.lods.size(); l++)
		{
			const ExportModel& mesh = options.lods[l].mesh;
			CompiledLod lod;
			lod.distance = options.lods[l].distance;
			lod.surfsName = options.name + "_lod" + std::to_string(l);

			// winding: the engine's convention, whatever the exporter wrote
			bool reverse = options.reverseWinding == 1;
			if (options.reverseWinding < 0)
			{
				std::size_t against = 0, counted = 0;
				for (const ExportFace& f : mesh.faces)
				{
					const float* p0 = mesh.vertices[static_cast<std::size_t>(f.corners[0].vertex)].position;
					const float* p1 = mesh.vertices[static_cast<std::size_t>(f.corners[1].vertex)].position;
					const float* p2 = mesh.vertices[static_cast<std::size_t>(f.corners[2].vertex)].position;
					const float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] }, e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
					float g[3];
					cross(e1, e2, g);
					const float n[3] = { f.corners[0].normal[0] + f.corners[1].normal[0] + f.corners[2].normal[0],
						f.corners[0].normal[1] + f.corners[1].normal[1] + f.corners[2].normal[1], f.corners[0].normal[2] + f.corners[1].normal[2] + f.corners[2].normal[2] };
					const float d = dot(g, n);
					if (d != 0)
					{
						counted++;
						against += d < 0 ? 1 : 0;
					}
				}
				const bool fileAgainst = against * 2 > counted;
				reverse = counted != 0 && fileAgainst != STORED_NORMAL_AGAINST_CROSS;
				if (reverse)
				{
					out.notes.push_back("LOD " + std::to_string(l) + ": triangle corner order reversed to the engine's winding");
				}
			}

			// one surface per ( material, object ), in order of appearance
			std::vector<std::pair<int, int>> groups;
			std::vector<std::vector<std::size_t>> groupFaces;
			for (std::size_t f = 0; f < mesh.faces.size(); f++)
			{
				const std::pair<int, int> key{ mesh.faces[f].material, mesh.faces[f].object };
				std::size_t g = static_cast<std::size_t>(std::find(groups.begin(), groups.end(), key) - groups.begin());
				if (g == groups.size())
				{
					groups.push_back(key);
					groupFaces.emplace_back();
				}
				groupFaces[g].push_back(f);
			}
			std::uint32_t baseTri = 0, baseVert = 0;
			for (std::size_t g = 0; g < groups.size(); g++)
			{
				CompiledSurface surface;
				const std::string exportName = static_cast<std::size_t>(groups[g].first) < mesh.materials.size() ? mesh.materials[static_cast<std::size_t>(groups[g].first)] : std::string();
				const auto renamed = options.materialNames.find(exportName);
				surface.material = renamed != options.materialNames.end() ? renamed->second : exportName;
				if (surface.material.empty())
				{
					return Status::error("model '" + options.name + "': a surface has no material name");
				}

				// distinct corners
				struct Corner
				{
					int vertex;
					float normal[3];
					std::uint32_t color;
					std::uint16_t u, v;
					float uv[2];
					bool operator<(const Corner& o) const
					{
						return std::tie(vertex, normal[0], normal[1], normal[2], color, u, v) < std::tie(o.vertex, o.normal[0], o.normal[1], o.normal[2], o.color, o.u, o.v);
					}
				};
				std::map<Corner, std::uint32_t> lookup;
				std::vector<Corner> corners;
				std::vector<std::array<std::uint32_t, 3>> triangles;
				bool weighted = false;
				for (const std::size_t f : groupFaces[g])
				{
					std::array<std::uint32_t, 3> tri{};
					for (int c = 0; c < 3; c++)
					{
						const ExportCorner& e = mesh.faces[f].corners[reverse ? 2 - c : c];
						Corner corner{};
						corner.vertex = e.vertex;
						std::memcpy(corner.normal, e.normal, sizeof(corner.normal));
						const auto channel = [](const float v) { return static_cast<std::uint32_t>(std::clamp(std::lround(v * 255.0f), 0l, 255l)); };
						corner.color = (channel(e.color[3]) << 24) | (channel(e.color[0]) << 16) | (channel(e.color[1]) << 8) | channel(e.color[2]);
						corner.u = float_to_half(e.uv[0]);
						corner.v = float_to_half(e.uv[1]);
						corner.uv[0] = e.uv[0];
						corner.uv[1] = e.uv[1];
						const auto [it, added] = lookup.emplace(corner, static_cast<std::uint32_t>(corners.size()));
						if (added)
						{
							corners.push_back(corner);
							weighted = weighted || mesh.vertices[static_cast<std::size_t>(e.vertex)].weights.size() > 1;
						}
						tri[static_cast<std::size_t>(c)] = it->second;
					}
					triangles.push_back(tri);
				}
				if (corners.size() > 65535 || triangles.size() > 65535)
				{
					return Status::error("model '" + options.name + "': surface '" + surface.material + "' has more than 65535 vertices or triangles; split it");
				}
				const auto primary_bone = [&](const int vertex)
				{
					const ExportVertex& v = mesh.vertices[static_cast<std::size_t>(vertex)];
					return v.weights.empty() ? 0 : remap[static_cast<std::size_t>(v.weights[0].first)];
				};
				const auto influence_count = [&](const int vertex)
				{
					return std::clamp<std::size_t>(mesh.vertices[static_cast<std::size_t>(vertex)].weights.size(), 1, 4);
				};

				// vertex order: rigid by bone, deformed by number of influences; then as the export has them
				std::vector<std::uint32_t> sorted(corners.size());
				std::iota(sorted.begin(), sorted.end(), 0u);
				std::stable_sort(sorted.begin(), sorted.end(), [&](const std::uint32_t a, const std::uint32_t b)
				{
					const auto ka = weighted ? static_cast<int>(influence_count(corners[a].vertex)) : primary_bone(corners[a].vertex);
					const auto kb = weighted ? static_cast<int>(influence_count(corners[b].vertex)) : primary_bone(corners[b].vertex);
					return ka != kb ? ka < kb : corners[a].vertex < corners[b].vertex;
				});
				std::vector<std::uint32_t> place(corners.size());
				for (std::uint32_t k = 0; k < sorted.size(); k++)
				{
					place[sorted[k]] = k;
				}
				for (auto& tri : triangles)
				{
					for (std::uint32_t& index : tri)
					{
						index = place[index];
					}
				}
				// triangle order of a rigid surface: by the bone of the first corner, so that every list has its run
				const auto triangle_bone = [&](const std::array<std::uint32_t, 3>& tri) { return primary_bone(corners[sorted[tri[0]]].vertex); };
				if (!weighted)
				{
					std::stable_sort(triangles.begin(), triangles.end(), [&](const auto& a, const auto& b) { return triangle_bone(a) < triangle_bone(b); });
				}

				// tangent frames
				std::vector<std::array<float, 3>> tangent(corners.size(), { 0, 0, 0 }), binormal(corners.size(), { 0, 0, 0 });
				for (const auto& tri : triangles)
				{
					const Corner* c[3] = { &corners[sorted[tri[0]]], &corners[sorted[tri[1]]], &corners[sorted[tri[2]]] };
					const float* p[3] = { mesh.vertices[static_cast<std::size_t>(c[0]->vertex)].position, mesh.vertices[static_cast<std::size_t>(c[1]->vertex)].position,
						mesh.vertices[static_cast<std::size_t>(c[2]->vertex)].position };
					const float e1[3] = { p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2] }, e2[3] = { p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2] };
					const float du1 = c[1]->uv[0] - c[0]->uv[0], dv1 = c[1]->uv[1] - c[0]->uv[1], du2 = c[2]->uv[0] - c[0]->uv[0], dv2 = c[2]->uv[1] - c[0]->uv[1];
					const float det = du1 * dv2 - du2 * dv1;
					if (std::fabs(det) < 1e-20f)
					{
						continue;
					}
					for (int k = 0; k < 3; k++)
					{
						const float t = (e1[k] * dv2 - e2[k] * dv1) / det, b = (e2[k] * du1 - e1[k] * du2) / det;
						for (const std::uint32_t index : tri)
						{
							tangent[index][static_cast<std::size_t>(k)] += t;
							binormal[index][static_cast<std::size_t>(k)] += b;
						}
					}
				}

				surface.vertices.resize(corners.size());
				for (std::uint32_t k = 0; k < sorted.size(); k++)
				{
					const Corner& corner = corners[sorted[k]];
					const ExportVertex& source = mesh.vertices[static_cast<std::size_t>(corner.vertex)];
					c1::GfxPackedVertex& v = surface.vertices[k];
					std::memcpy(v.xyz, source.position, sizeof(v.xyz));
					float n[3] = { corner.normal[0], corner.normal[1], corner.normal[2] };
					if (!normalize(n))
					{
						n[0] = n[1] = 0;
						n[2] = 1;
					}
					// tangent: the u direction, made perpendicular to the normal
					float t[3] = { tangent[k][0], tangent[k][1], tangent[k][2] };
					const float along = dot(t, n);
					for (int c = 0; c < 3; c++)
					{
						t[c] -= n[c] * along;
					}
					if (!normalize(t))
					{
						// no usable uv gradient: any direction perpendicular to the normal
						const float helper[3] = { std::fabs(n[0]) < 0.9f ? 1.0f : 0.0f, std::fabs(n[0]) < 0.9f ? 0.0f : 1.0f, 0.0f };
						cross(helper, n, t);
						normalize(t);
					}
					float b[3];
					cross(n, t, b);
					const float bn[3] = { binormal[k][0], binormal[k][1], binormal[k][2] };
					v.binormalSign = dot(b, bn) < 0 ? -1.0f : 1.0f;
					v.color = corner.color;
					v.texCoordU = corner.u;
					v.texCoordV = corner.v;
					v.normal = pack_unit_vector(n);
					v.tangent = pack_unit_vector(t);

					// bounds
					modelBounds.add(source.position);
					radiusSquared = std::max(radiusSquared, dot(source.position, source.position));
					const int bone = primary_bone(corner.vertex);
					const float delta[3] = { source.position[0] - worldTrans[static_cast<std::size_t>(bone)][0], source.position[1] - worldTrans[static_cast<std::size_t>(bone)][1],
						source.position[2] - worldTrans[static_cast<std::size_t>(bone)][2] };
					float local[3];
					rotate(conjugate(worldQuat[static_cast<std::size_t>(bone)]), delta, local);
					boneBounds[static_cast<std::size_t>(bone)].add(local);
				}
				for (const auto& tri : triangles)
				{
					for (const std::uint32_t index : tri)
					{
						surface.indices.push_back(static_cast<std::uint16_t>(index));
					}
				}

				c1::XSurface& h = surface.header;
				h.deformed = weighted ? 1 : 0;
				h.flags = 0x20;
				h.vertCount = static_cast<std::uint16_t>(surface.vertices.size());
				h.triCount = static_cast<std::uint16_t>(triangles.size());
				h.baseTriIndex = baseTri;
				h.baseVertIndex = baseVert;
				baseTri += h.triCount;
				baseVert += h.vertCount;
				if (weighted)
				{
					for (std::uint32_t k = 0; k < sorted.size(); k++)
					{
						const ExportVertex& source = mesh.vertices[static_cast<std::size_t>(corners[sorted[k]].vertex)];
						const std::size_t count = influence_count(corners[sorted[k]].vertex);
						h.vertInfoCount[count - 1]++;
						float total = 0;
						for (std::size_t w = 0; w < count; w++)
						{
							total += source.weights[w].second;
						}
						total = total > 0 ? total : 1.0f;
						for (std::size_t w = 0; w < count; w++)
						{
							const int bone = source.weights.empty() ? 0 : remap[static_cast<std::size_t>(source.weights[w].first)];
							set_part_bit(h.partBits, bone);
							surface.blend.push_back(static_cast<std::uint16_t>(bone * 64));
							if (w > 0)
							{
								surface.blend.push_back(static_cast<std::uint16_t>(std::clamp(std::lround(source.weights[w].second / total * 65536.0f), 0l, 65535l)));
							}
						}
					}
				}
				else
				{
					std::uint32_t vertexAt = 0, triangleAt = 0;
					while (vertexAt < sorted.size())
					{
						const int bone = primary_bone(corners[sorted[vertexAt]].vertex);
						std::uint32_t vertexEnd = vertexAt, triangleEnd = triangleAt;
						while (vertexEnd < sorted.size() && primary_bone(corners[sorted[vertexEnd]].vertex) == bone)
						{
							vertexEnd++;
						}
						while (triangleEnd < triangles.size() && triangle_bone(triangles[triangleEnd]) == bone)
						{
							triangleEnd++;
						}
						c1::XRigidVertList list{};
						list.boneOffset = static_cast<std::uint16_t>(bone * 64);
						list.vertCount = static_cast<std::uint16_t>(vertexEnd - vertexAt);
						list.triOffset = static_cast<std::uint16_t>(triangleAt);
						list.triCount = static_cast<std::uint16_t>(triangleEnd - triangleAt);
						surface.rigid.push_back(list);
						set_part_bit(h.partBits, bone);
						vertexAt = vertexEnd;
						triangleAt = triangleEnd;
					}
					h.vertListCount = static_cast<std::uint32_t>(surface.rigid.size());
				}
				for (int k = 0; k < 8; k++)
				{
					lod.partBits[k] |= h.partBits[k];
				}
				lod.surfaces.push_back(std::move(surface));
			}
			if (lod.surfaces.empty())
			{
				return Status::error("model '" + options.name + "': LOD " + std::to_string(l) + " has no triangles");
			}
			out.lods.push_back(std::move(lod));

			// ---- collision surfaces from this LOD
			if (static_cast<int>(l) == options.collisionLod)
			{
				std::map<int, CompiledCollisionSurface> perBone;
				std::map<int, BoundsBuilder> perBoneBounds;
				for (const ExportFace& f : mesh.faces)
				{
					const float* a = mesh.vertices[static_cast<std::size_t>(f.corners[0].vertex)].position;
					const float* b = mesh.vertices[static_cast<std::size_t>(f.corners[1].vertex)].position;
					const float* c = mesh.vertices[static_cast<std::size_t>(f.corners[2].vertex)].position;
					const float ab[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, ac[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
					float g[3];
					cross(ab, ac, g);
					const float sum[3] = { f.corners[0].normal[0] + f.corners[1].normal[0] + f.corners[2].normal[0],
						f.corners[0].normal[1] + f.corners[1].normal[1] + f.corners[2].normal[1], f.corners[0].normal[2] + f.corners[1].normal[2] + f.corners[2].normal[2] };
					// record: plane with the outward normal n = cross( e2, e1 ) / | |; s = dot( svec, p ) - svec.w runs 0 -> 1 along e1, t along e2
					const bool outwardIsG = dot(g, sum) >= 0;
					const float* e1 = outwardIsG ? ac : ab;
					const float* e2 = outwardIsG ? ab : ac;
					float n[3];
					cross(e2, e1, n);
					if (!normalize(n))
					{
						continue;		// degenerate
					}
					float sv[3], tv[3];
					cross(e2, n, sv);
					cross(n, e1, tv);
					const float sd = dot(e1, sv), td = dot(e2, tv);
					if (std::fabs(sd) < 1e-12f || std::fabs(td) < 1e-12f)
					{
						continue;
					}
					for (int k = 0; k < 3; k++)
					{
						sv[k] /= sd;
						tv[k] /= td;
					}
					const ExportVertex& va = mesh.vertices[static_cast<std::size_t>(f.corners[0].vertex)];
					const int bone = va.weights.empty() ? 0 : remap[static_cast<std::size_t>(va.weights[0].first)];
					perBone[bone].triangles.push_back({ n[0], n[1], n[2], dot(n, a), sv[0], sv[1], sv[2], dot(sv, a), tv[0], tv[1], tv[2], dot(tv, a) });
					perBoneBounds[bone].add(a);
					perBoneBounds[bone].add(b);
					perBoneBounds[bone].add(c);
				}
				for (auto& [bone, surface] : perBone)
				{
					surface.bone = bone;
					surface.contents = options.contents;
					surface.surfaceFlags = options.collisionSurfaceFlags;
					surface.bounds = perBoneBounds[bone].bounds();
					out.collision.push_back(std::move(surface));
				}
				if (!out.collision.empty())
				{
					out.collisionLod = static_cast<int>(l);
					out.contents = options.contents;
				}
			}
			if (l == 0)
			{
				out.bounds = modelBounds.bounds();
				out.radius = std::sqrt(radiusSquared);
			}
		}
		for (std::size_t k = 0; k < boneCount; k++)
		{
			const c1::Bounds b = boneBounds[k].bounds();
			out.boneInfo.insert(out.boneInfo.end(), { b.midPoint[0], b.midPoint[1], b.midPoint[2], b.halfSize[0], b.halfSize[1], b.halfSize[2],
				b.halfSize[0] * b.halfSize[0] + b.halfSize[1] * b.halfSize[1] + b.halfSize[2] * b.halfSize[2] });
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}
}
