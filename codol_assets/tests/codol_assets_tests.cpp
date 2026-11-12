// codol_assets unit tests. The first part needs nothing but the library; the second builds a zone against a real
// 1.1.1.13 install ( CODOL_ASSETS_GAME_DIR, or the environment variable of that name ) and is skipped without one.

#include "codol_assets/link.hpp"

#include "codol_render/iwd.hpp"
#include "codol_render/iwi.hpp"
#include "codol_zone/assets.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace codol::assets;
namespace fs = std::filesystem;
namespace c1 = codol::c1;
namespace z = codol::zone;

namespace
{
	int checks = 0, failures = 0;

	void check(const bool ok, const char* what, const char* file, const int line)
	{
		checks++;
		if (!ok)
		{
			failures++;
			std::printf("FAIL %s ( %s:%d )\n", what, file, line);
		}
	}
#define CHECK(x) check(static_cast<bool>(x), #x, __FILE__, __LINE__)
}

#include "anim_tests.hpp"

namespace
{

	Bitmap test_image(const int w, const int h, const bool alpha)
	{
		Bitmap b;
		b.resize(w, h);
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				std::uint8_t* p = b.at(x, y);
				p[0] = static_cast<std::uint8_t>(40 + 150 * x / w);
				p[1] = static_cast<std::uint8_t>(60 + 120 * y / h);
				p[2] = static_cast<std::uint8_t>(128 + 60 * std::sin(x * 0.2) * std::cos(y * 0.15));
				p[3] = alpha ? static_cast<std::uint8_t>(255 * ((x / 8 + y / 8) & 1)) : 255;
			}
		}
		return b;
	}

	void test_image_files()
	{
		const Bitmap source = test_image(37, 21, true);
		std::vector<std::uint8_t> png;
		CHECK(encode_png(source, png));
		Bitmap back;
		bool hasAlpha = false;
		CHECK(decode_png(png, back, &hasAlpha));
		CHECK(hasAlpha && back.width == 37 && back.height == 21 && back.rgba == source.rgba);
		CHECK(psnr(source, back) == 99);

		// TGA: 32 bit, bottom up, run length coded: two pixels
		const std::uint8_t tga[] = { 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 32, 8, 0x81, 10, 20, 30, 255 };
		Bitmap t;
		CHECK(decode_tga(tga, t, &hasAlpha));
		CHECK(t.width == 2 && t.height == 1 && t.rgba[0] == 30 && t.rgba[1] == 20 && t.rgba[2] == 10 && t.rgba[3] == 255 && t.rgba[4] == 30);

		// DDS: uncompressed 2 x 1, B G R A masks
		std::vector<std::uint8_t> dds(128 + 8, 0);
		std::memcpy(dds.data(), "DDS ", 4);
		const std::uint32_t header[] = { 124, 0x100F, 1, 2, 8, 0, 1 };
		std::memcpy(dds.data() + 4, header, sizeof(header));
		const std::uint32_t format[] = { 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 };
		std::memcpy(dds.data() + 76, format, sizeof(format));
		const std::uint8_t pixels[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		std::memcpy(dds.data() + 128, pixels, 8);
		SourceImage d;
		CHECK(decode_dds(dds, d));
		CHECK(d.faces.size() == 1 && d.hasAlpha && d.faces[0].rgba[0] == 3 && d.faces[0].rgba[1] == 2 && d.faces[0].rgba[2] == 1 && d.faces[0].rgba[3] == 4);
		CHECK(!decode_png(tga, t));
	}

	void test_image_compiler()
	{
		SourceImage source;
		source.faces.push_back(test_image(64, 32, false));
		CompiledImage image;
		ImageOptions options;
		options.streamed = true;
		CHECK(compile_image("test_col", source, options, image));
		CHECK(image.iwiFormat == 11 && image.semantic == 2 && image.flags == 0x340 && image.levelCount == 7 && image.surfaces.size() == 7);
		const std::vector<std::uint8_t> file = write_iwi(image);
		IwiLevels back;
		CHECK(read_iwi_levels(file, {}, back));
		CHECK(back.levelCount == 7 && back.width == 64 && back.height == 32 && back.format == 11 && back.flags == 0x340);
		CHECK(back.fileSizeForPicmip[0] == static_cast<std::int32_t>(file.size()));
		CHECK(back.fileSizeForPicmip[1] == static_cast<std::int32_t>(file.size() - image.surfaces[0].size()));
		CHECK(psnr(back.levels[0], image.reference[0], 7) > 38);
		std::printf("DXT1 of the test gradient: level 0 %.1f dB, level 1 %.1f dB, level 3 %.1f dB\n", psnr(back.levels[0], image.reference[0], 7),
			psnr(back.levels[1], image.reference[1], 7), psnr(back.levels[3], image.reference[3], 7));
		CHECK(psnr(back.levels[1], image.reference[1], 7) > 30);
		// the small levels of this image are a steep three channel gradient in a single block: the worst case of DXT
		CHECK(psnr(back.levels[3], image.reference[3], 7) > 22);

		// a flat block is reproduced exactly up to 565
		Bitmap flat;
		flat.resize(4, 4);
		for (std::size_t k = 0; k < flat.rgba.size(); k += 4)
		{
			flat.rgba[k] = 0x84;
			flat.rgba[k + 1] = 0x82;
			flat.rgba[k + 2] = 0x84;
			flat.rgba[k + 3] = 77;
		}
		const std::vector<std::uint8_t> block = compress_dxt(flat, ImageFormat::dxt5);
		CHECK(block.size() == 16 && block[0] == 77);
		SourceImage flatSource;
		flatSource.faces.push_back(flat);
		options.format = ImageFormat::dxt5;
		options.mips = 0;
		CHECK(compile_image("flat", flatSource, options, image));
		CHECK(read_iwi_levels(write_iwi(image), {}, back));
		CHECK(back.levels[0].rgba[0] == 0x84 && back.levels[0].rgba[1] == 0x82 && back.levels[0].rgba[3] == 77);

		// alpha in DXT5, punch through in DXT1
		SourceImage cut;
		cut.faces.push_back(test_image(32, 32, true));
		options = {};
		options.streamed = true;
		CHECK(compile_image("cut", cut, options, image));
		CHECK(image.iwiFormat == 13);
		CHECK(read_iwi_levels(write_iwi(image), {}, back));
		CHECK(psnr(back.levels[0], image.reference[0], 8) > 45);
		options.format = ImageFormat::dxt1;
		CHECK(compile_image("cut1", cut, options, image));
		CHECK(read_iwi_levels(write_iwi(image), {}, back));
		CHECK(back.levels[0].at(0, 0)[3] == 0 && back.levels[0].at(8, 0)[3] == 255);

		// normal map: x in alpha, y in the colour channels; mips stay unit vectors
		SourceImage normal;
		normal.faces.emplace_back();
		normal.faces[0].resize(16, 16);
		for (int k = 0; k < 256; k++)
		{
			std::uint8_t* p = normal.faces[0].rgba.data() + k * 4;
			p[0] = static_cast<std::uint8_t>(k % 2 ? 200 : 60);
			p[1] = 128;
			p[2] = 220;
			p[3] = 255;
		}
		options = {};
		options.use = ImageUse::normal;
		CHECK(compile_image("test_nml", normal, options, image));
		CHECK(image.semantic == 5 && image.flags == 0x40000 && image.iwiFormat == 13);
		CHECK(image.reference[0].rgba[3] == 60 && image.reference[0].rgba[0] == 128 && image.reference[0].rgba[2] == 128);
		const Bitmap half = downsample(normal.faces[0], false, true);
		const float nx = half.rgba[0] / 127.5f - 1, ny = half.rgba[1] / 127.5f - 1, nz = half.rgba[2] / 127.5f - 1;
		CHECK(std::fabs(std::sqrt(nx * nx + ny * ny + nz * nz) - 1.0f) < 0.02f);
		// colour mips are averaged in linear light: black and white give more than 127
		Bitmap checker;
		checker.resize(2, 2);
		for (int k = 0; k < 4; k++)
		{
			std::memset(checker.rgba.data() + k * 4, (k == 0 || k == 3) ? 255 : 0, 3);
			checker.rgba[static_cast<std::size_t>(k) * 4 + 3] = 255;
		}
		CHECK(downsample(checker, true, false).rgba[0] > 180 && downsample(checker, false, false).rgba[0] == 128);

		// sizes: mips need powers of two
		SourceImage odd;
		odd.faces.push_back(test_image(30, 20, false));
		options = {};
		CHECK(!compile_image("odd", odd, options, image));
		options.use = ImageUse::ui;
		CHECK(compile_image("odd", odd, options, image));
		CHECK(image.levelCount == 1 && image.flags == 0x173 && image.iwiFormat == 1 && image.surfaces[0].size() == 30 * 20 * 4);

		// GfxImage pieces
		const auto streamed = image_pieces([&] { CompiledImage i = image; i.streamed = true; return i; }());
		CHECK(streamed.size() == 3 && streamed[0].bytes.size() == sizeof(c1::GfxImage) && streamed[1].string && streamed[2].bytes.size() == 16);
		const auto inlined = image_pieces(image);
		CHECK(inlined.size() == 4 && inlined[3].bytes.size() == 30 * 20 * 4);
		c1::GfxImage header;
		std::memcpy(&header, inlined[0].bytes.data(), sizeof(header));
		CHECK(header.width == 30 && header.height == 20 && header.category == 1 && header.semantic == 0 && header.mapType == 3);
	}

	void test_iwd(const fs::path& scratch)
	{
		const std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files = { { "images/a.iwi", { 1, 2, 3, 4, 5 } }, { "images/b.iwi", std::vector<std::uint8_t>(70000, 7) } };
		const fs::path file = scratch / "codol_assets_test.iwd";
		CHECK(write_iwd(file, files));
		std::string error;
		const auto archive = codol::render::iwd_archive::open(file, nullptr, &error);
		CHECK(archive && archive->entries().size() == 2);
		if (archive)
		{
			std::vector<std::uint8_t> data;
			const auto* entry = archive->find("images/b.iwi");
			CHECK(entry && archive->read(*entry, data, &error) && data == files[1].second);
		}
		std::error_code ec;
		fs::remove(file, ec);
	}

	void test_materials()
	{
		CHECK(material_name_hash("colorMap") == 0xA0AB1041u);
		CHECK(material_name_hash("x#12345678") == 0x12345678u);
		CHECK(surface_type_bits("wood") == 0x100000 && surface_type_bits("Metal") == 0x1000 && surface_type_bits("nothing") == 0);
		std::vector<MaterialDef> defs;
		CHECK(parse_material_text("// c\nmaterial \"mc/a\"\n{\n\ttype \"model phong\"\n\tcolorMap a_col clamp nomip\n\tnormalMap a_nml\n\tblendFunc blend\n\talphaTest gt0\n"
			"\tcullFace none\n\tsort 9\n\tsurfaceType wood\n\tconstant envMapParms 1 2 3 4\n}\nmaterial b { techset 2d\n colorMap b }\n", defs));
		CHECK(defs.size() == 2 && defs[0].name == "mc/a" && defs[0].textures.size() == 2 && defs[0].textures[0].samplerState == 0xE3);
		CHECK(defs[0].sortKey == 9 && defs[0].surfaceTypeBits == 0x100000 && defs[0].constants.size() == 1 && defs[0].constants[0].value[2] == 3);
		CHECK((defs[0].stateMask & 0xC000) == 0xC000 && (defs[0].stateValue & 0xC000) == 0x4000 && (defs[0].stateValue & 0x3800) == 0x1000);
		CHECK(techset_for(defs[0]) == "mc_l_sm_b0c0n0" && techset_for(defs[1]) == "2d");
		// text -> definition -> text -> definition
		std::vector<MaterialDef> again;
		CHECK(parse_material_text(write_material_text(defs[0]), again));
		CHECK(again.size() == 1 && again[0].stateMask == defs[0].stateMask && again[0].stateValue == defs[0].stateValue && again[0].textures.size() == 2
			&& again[0].textures[0].samplerState == 0xE3 && again[0].sortKey == 9);
		CHECK(!parse_material_text("material x { cullFace sideways }", again));

		std::vector<MaterialDef> gdt;
		CHECK(parse_gdt_materials("{\n\t\"wc/g\" ( \"material.gdf\" )\n\t{\n\t\t\"materialType\" \"world phong\"\n\t\t\"colorMap\" \"texture_assets\\\\G_Col.tga\"\n"
			"\t\t\"normalMap\" \"texture_assets\\\\g_nml.tga\"\n\t\t\"blendFunc\" \"Replace*\"\n\t\t\"alphaTest\" \"GE128\"\n\t\t\"surfaceType\" \"brick\"\n\t\t\"tileColor\" \"no tile\"\n\t}\n"
			"\t\"other\" ( \"xmodel.gdf\" )\n\t{\n\t\t\"filename\" \"x\"\n\t}\n}\n", gdt));
		CHECK(gdt.size() == 1 && gdt[0].name == "wc/g" && gdt[0].textures.size() == 2 && gdt[0].textures[0].image == "g_col");
		CHECK(gdt[0].imageFiles.at("colorMap") == "texture_assets\\G_Col.tga" && gdt[0].surfaceTypeBits == 2 && (gdt[0].textures[0].samplerState & 0xE0) == 0xE0);
		CHECK(techset_for(gdt[0]) == "wc_l_sm_t0c0n0");
	}

	ExportModel box_export(const bool flipped)
	{
		ExportModel m;
		m.bones.resize(1);
		m.bones[0].name = "tag_origin";
		m.objects.push_back("box");
		m.materials.push_back("mc/test");
		const float sides[6][3][3] = { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } }, { { -1, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1 } }, { { 0, 1, 0 }, { -1, 0, 0 }, { 0, 0, 1 } },
			{ { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } }, { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } }, { { 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 } } };
		for (const auto& side : sides)
		{
			const int base = static_cast<int>(m.vertices.size());
			const float uvs[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
			for (const auto& uv : uvs)
			{
				ExportVertex v;
				for (int k = 0; k < 3; k++)
				{
					v.position[k] = 16 * (side[0][k] + side[1][k] * (uv[0] * 2 - 1) + side[2][k] * (uv[1] * 2 - 1));
				}
				v.weights.emplace_back(0, 1.0f);
				m.vertices.push_back(v);
			}
			const int order[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
			for (const auto& tri : order)
			{
				ExportFace f;
				for (int c = 0; c < 3; c++)
				{
					const int corner = tri[flipped ? 2 - c : c];
					f.corners[c].vertex = base + corner;
					std::memcpy(f.corners[c].normal, side[0], sizeof(side[0]));
					f.corners[c].uv[0] = uvs[corner][0];
					f.corners[c].uv[1] = 1 - uvs[corner][1];
				}
				m.faces.push_back(f);
			}
		}
		return m;
	}

	void test_models()
	{
		float n[3] = { 0.267f, -0.535f, 0.802f }, back[3];
		z::unpack_unit_vector(pack_unit_vector(n), back);
		CHECK(std::fabs(back[0] - n[0]) < 0.006f && std::fabs(back[1] - n[1]) < 0.006f && std::fabs(back[2] - n[2]) < 0.006f);
		for (const float v : { 0.0f, 1.0f, -1.0f, 0.5f, 0.333251953125f, 123.5f, -0.000244140625f })
		{
			CHECK(z::half_to_float(float_to_half(v)) == v);
		}
		CHECK(std::fabs(z::half_to_float(float_to_half(0.1f)) - 0.1f) < 1e-4f);

		const ExportModel box = box_export(false);
		ExportModel parsed;
		CHECK(parse_xmodel_export(write_xmodel_export(box), parsed));
		CHECK(parsed.bones.size() == 1 && parsed.vertices.size() == 24 && parsed.faces.size() == 12 && parsed.materials[0] == "mc/test" && parsed.bones[0].name == "tag_origin");
		CHECK(parsed.faces[5].corners[1].vertex == box.faces[5].corners[1].vertex && parsed.vertices[7].position[2] == box.vertices[7].position[2]);

		ModelOptions options;
		options.name = "test_box";
		options.lods.push_back({ parsed, 500.0f });
		options.collisionLod = 0;
		CompiledModel a, b;
		CHECK(compile_model(options, a));
		CHECK(a.lods.size() == 1 && a.lods[0].surfaces.size() == 1 && a.lods[0].surfaces[0].vertices.size() == 24 && a.lods[0].surfaces[0].indices.size() == 36);
		CHECK(a.lods[0].surfaces[0].rigid.size() == 1 && a.lods[0].surfaces[0].rigid[0].triCount == 12 && a.lods[0].surfaces[0].header.flags == 0x20);
		CHECK(a.lods[0].partBits[0] == 0x80000000u && a.rootBones == 1 && a.parentList.empty());
		CHECK(std::fabs(a.radius - 16 * std::sqrt(3.0f)) < 1e-3f && a.bounds.halfSize[0] == 16 && a.bounds.midPoint[2] == 0);
		CHECK(a.collision.size() == 1 && a.collision[0].triangles.size() == 12 && a.contents == 1 && a.collisionLod == 0);
		// every collision record: the plane holds the triangle, the normal points outward
		for (const auto& t : a.collision[0].triangles)
		{
			CHECK(std::fabs(t[3] - 16.0f) < 1e-3f);
		}
		// the collision tree of the rigid list ( LOD 0 of a model with collision ): the rules read off the shipped trees
		// with codol_asset_probe trees - every triangle in exactly one leaf entry, a leaf node holds up to 16 entries and
		// its box holds their triangles, children lie behind their parent and inside its box, the root spans tree space
		const auto check_tree = [&](const CompiledSurface& s, const std::size_t list)
		{
			CHECK(s.trees.size() == s.rigid.size());
			if (s.trees.size() != s.rigid.size())
			{
				return;
			}
			const CompiledCollisionTree& tree = s.trees[list];
			const c1::XRigidVertList& rigid = s.rigid[list];
			CHECK(!tree.nodes.empty() && tree.nodes[0].mins[0] == 0 && tree.nodes[0].maxs[0] == 65535 && tree.nodes[0].maxs[2] == 65535);
			std::vector<int> covered(rigid.triCount, 0), parents(tree.nodes.size(), 0);
			bool boxes = true, order = true, leaf_size = true;
			std::size_t entries = 0;
			for (std::size_t n = 0; n < tree.nodes.size(); n++)
			{
				const CompiledCollisionTree::Node& node = tree.nodes[n];
				if (!(node.childCount & 0x8000))
				{
					order = order && node.childBegin > n && node.childCount >= 2 && node.childBegin + node.childCount <= tree.nodes.size();
					for (std::size_t c = node.childBegin; c < static_cast<std::size_t>(node.childBegin) + node.childCount && c < tree.nodes.size(); c++)
					{
						parents[c]++;
						for (int a = 0; a < 3; a++)
						{
							boxes = boxes && tree.nodes[c].mins[a] >= node.mins[a] && tree.nodes[c].maxs[a] <= node.maxs[a];
						}
					}
					continue;
				}
				const std::size_t count = node.childCount & 0x7FFFu;
				leaf_size = leaf_size && count >= 1 && count <= 16 && node.childBegin + count <= tree.leafs.size();
				entries += count;
				for (std::size_t e = node.childBegin; e < node.childBegin + count && e < tree.leafs.size(); e++)
				{
					const std::uint32_t at = tree.leafs[e] & 0x7FFFu;
					for (std::uint32_t t = at; t < at + (tree.leafs[e] & 0x8000 ? 2u : 1u); t++)
					{
						if (t < rigid.triOffset || t >= static_cast<std::uint32_t>(rigid.triOffset) + rigid.triCount)
						{
							leaf_size = false;
							continue;
						}
						covered[t - rigid.triOffset]++;
						for (int c = 0; c < 3; c++)
						{
							const float* p = s.vertices[s.indices[t * 3 + static_cast<std::uint32_t>(c)]].xyz;
							for (int a = 0; a < 3; a++)
							{
								const float unit = (p[a] + tree.trans[a]) * tree.scale[a];
								boxes = boxes && unit >= static_cast<float>(node.mins[a]) - 1.0f && unit <= static_cast<float>(node.maxs[a]) + 1.0f && unit > -1.0f && unit < 65536.0f;
							}
						}
					}
				}
			}
			CHECK(boxes && order && leaf_size && entries == tree.leafs.size());
			CHECK(std::all_of(covered.begin(), covered.end(), [](const int c) { return c == 1; }));
			CHECK(std::all_of(parents.begin() + 1, parents.end(), [](const int c) { return c == 1; }));
		};
		check_tree(a.lods[0].surfaces[0], 0);
		CHECK(a.lods[0].surfaces[0].trees[0].nodes.size() == 1 && a.lods[0].surfaces[0].trees[0].leafs.size() == 12);
		{
			// a larger mesh: a strip of 200 triangles gets inner nodes
			std::vector<c1::GfxPackedVertex> strip(202);
			std::vector<std::uint16_t> strip_indices;
			for (std::size_t k = 0; k < strip.size(); k++)
			{
				strip[k].xyz[0] = static_cast<float>(k / 2) * 4.0f;
				strip[k].xyz[1] = k % 2 ? 8.0f : -8.0f;
				strip[k].xyz[2] = static_cast<float>((k / 2) % 7);
			}
			for (std::uint16_t k = 0; k < 200; k++)
			{
				strip_indices.insert(strip_indices.end(), { k, static_cast<std::uint16_t>(k + 1), static_cast<std::uint16_t>(k + 2) });
			}
			CompiledSurface big;
			big.vertices = strip;
			big.indices = strip_indices;
			c1::XRigidVertList whole{};
			whole.vertCount = 202;
			whole.triCount = 200;
			big.rigid.push_back(whole);
			big.trees.push_back(build_collision_tree(big.vertices, big.indices, 0, 200));
			check_tree(big, 0);
			CHECK(big.trees[0].nodes.size() > 16 && big.trees[0].leafs.size() == 200);
			CHECK(build_collision_tree(big.vertices, big.indices, 0, 0).nodes.empty());
		}

		// the engine's winding whatever the file has
		options.lods[0].mesh = box_export(true);
		CHECK(compile_model(options, b));
		CHECK(b.lods[0].surfaces[0].indices == a.lods[0].surfaces[0].indices || b.lods[0].surfaces[0].vertices.size() == 24);
		const auto& sv = a.lods[0].surfaces[0];
		const auto& sw = b.lods[0].surfaces[0];
		const auto winding = [](const CompiledSurface& s)
		{
			const float* p0 = s.vertices[s.indices[0]].xyz;
			const float* p1 = s.vertices[s.indices[1]].xyz;
			const float* p2 = s.vertices[s.indices[2]].xyz;
			const float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] }, e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
			float normal[3];
			z::unpack_unit_vector(s.vertices[s.indices[0]].normal, normal);
			return (e1[1] * e2[2] - e1[2] * e2[1]) * normal[0] + (e1[2] * e2[0] - e1[0] * e2[2]) * normal[1] + (e1[0] * e2[1] - e1[1] * e2[0]) * normal[2];
		};
		CHECK(winding(sv) < 0 && winding(sw) < 0);

		// an odd surface gets the filler triangle, the rigid list does not count it
		options.lods[0].mesh = box_export(false);
		options.lods[0].mesh.faces.pop_back();
		CHECK(compile_model(options, b));
		CHECK(b.lods[0].surfaces[0].header.triCount == 12 && b.lods[0].surfaces[0].rigid[0].triCount == 11);
		CHECK(b.lods[0].surfaces[0].indices[33] == b.lods[0].surfaces[0].indices[35] && b.lods[0].surfaces[0].indices[33] == b.lods[0].surfaces[0].indices[32]);

		// two bones: the second 10 units up, turned 90 degrees about z; half of the vertices follow it
		ExportModel two = box_export(false);
		two.bones.resize(2);
		two.bones[1].name = "j_lid";
		two.bones[1].parent = 0;
		two.bones[1].offset[2] = 10;
		const float turned[3][3] = { { 0, 1, 0 }, { -1, 0, 0 }, { 0, 0, 1 } };
		std::memcpy(two.bones[1].axis, turned, sizeof(turned));
		for (std::size_t k = 12; k < two.vertices.size(); k++)
		{
			two.vertices[k].weights[0].first = 1;
		}
		options.lods[0].mesh = two;
		options.collisionLod = -1;
		CHECK(compile_model(options, b));
		CHECK(b.boneNames.size() == 2 && b.parentList.size() == 1 && b.parentList[0] == 1 && b.quats.size() == 4 && b.trans.size() == 3);
		CHECK(std::abs(b.quats[2] - 23170) <= 1 && std::abs(b.quats[3] - 23170) <= 1 && b.trans[2] == 10);
		CHECK(b.lods[0].surfaces[0].rigid.size() == 2 && b.lods[0].surfaces[0].rigid[1].boneOffset == 64 && b.lods[0].surfaces[0].rigid[0].vertCount == 12);
		CHECK(b.lods[0].partBits[0] == 0xC0000000u && b.contents == 0 && b.collision.empty());
		CHECK(b.lods[0].surfaces[0].trees.empty());		// no collision: bullets pass, no tree ( as the shipped models )
		options.collisionLod = 0;
		CHECK(compile_model(options, b));
		check_tree(b.lods[0].surfaces[0], 0);
		check_tree(b.lods[0].surfaces[0], 1);			// the second list's triangles count from the surface's first
		options.collisionLod = -1;
		// weighted vertices make the surface deformed
		two.vertices[0].weights = { { 0, 0.75f }, { 1, 0.25f } };
		options.lods[0].mesh = two;
		CHECK(compile_model(options, b));
		const CompiledSurface& skinned = b.lods[0].surfaces[0];
		CHECK(skinned.header.deformed == 1 && skinned.rigid.empty() && skinned.header.vertInfoCount[0] == 23 && skinned.header.vertInfoCount[1] == 1);
		CHECK(skinned.blend.size() == 23 + 3 && skinned.blend[23] == 0 && skinned.blend[24] == 64 && skinned.blend[25] == 16384);

		ExportModel broken = box;
		broken.faces[0].corners[0].vertex = 99;
		CHECK(!parse_xmodel_export(write_xmodel_export(broken), parsed));
	}

	void test_install(const fs::path& install, const fs::path& scratch)
	{
		LinkOptions link;
		link.install = install;
		link.libraryCache = scratch / "codol_assets_test_matlib.txt";
		link.zoneName = "codol_assets_test";
		AssetLinker linker;
		const Status begun = linker.begin(link);
		CHECK(begun);
		if (!begun)
		{
			std::printf("%s\n", begun.message.c_str());
			return;
		}
		const LibraryTechset* world = linker.library().find("wc_l_sm_r0c0n0s0");
		CHECK(world && world->samplers.size() == 3 && world->constants.size() == 2 && !world->states.empty() && world->commonZone.empty() && !world->donorZone.empty());
		const LibraryTechset* model = linker.library().find("mc_l_sm_r0c0n0s0");
		CHECK(model && !model->commonZone.empty());

		SourceImage source;
		source.faces.push_back(test_image(64, 64, false));
		CompiledImage image;
		CHECK(compile_image("codol_unit_col", source, {}, image));
		CHECK(linker.add_image(image));
		CHECK(!linker.add_image(image));

		std::vector<MaterialDef> defs;
		CHECK(parse_material_text("material \"wc/codol_unit\" { type \"world phong\"\n colorMap codol_unit_col\n surfaceType concrete }\n"
			"material \"mc/codol_unit\" { type \"model phong\"\n colorMap codol_unit_col\n normalMap $identitynormalmap\n specularMap $white }\n"
			"material \"wc/codol_unit_twosided\" { type \"world phong\"\n colorMap codol_unit_col\n cullFace none\n blendFunc blend\n depthWrite on }\n", defs));
		for (const MaterialDef& def : defs)
		{
			CompiledMaterial material;
			const Status s = compile_material(def, linker.library(), material);
			CHECK(s);
			if (!s)
			{
				std::printf("%s\n", s.message.c_str());
				continue;
			}
			CHECK(material.header.stateBitsCount == material.stateBits.size() && material.textures.size() == material.images.size());
			const Status added = linker.add_material(material);
			CHECK(added);
			if (!added)
			{
				std::printf("%s\n", added.message.c_str());
			}
		}
		MaterialDef bad;
		bad.name = "x";
		bad.techset = "no_such_techset";
		CompiledMaterial none;
		CHECK(!compile_material(bad, linker.library(), none));

		ModelOptions options;
		options.name = "codol_unit_box";
		options.lods.push_back({ box_export(false), 800.0f });
		options.lods[0].mesh.materials[0] = "mc/codol_unit";
		options.collisionLod = 0;
		CompiledModel compiled;
		CHECK(compile_model(options, compiled));
		CHECK(linker.add_model(compiled));
		std::vector<RawAnim> anims;
		anim_tests::test_anim_zone(linker, anims);

		// the game's loaders take it, and the reader finds what went in
		std::vector<std::uint8_t> bytes;
		const Status written = linker.write_zone(bytes);
		CHECK(written);
		if (!written)
		{
			std::printf("%s\n", written.message.c_str());
			return;
		}
		z::Zone zone;
		CHECK(zone.load(bytes, "codol_assets_test"));
		CHECK(zone.warnings().empty());
		anim_tests::check_anim_zone(zone, anims);
		z::MaterialView material;
		CHECK(z::read_material(zone, zone.find_asset(c1::ASSET_TYPE_MATERIAL, "wc/codol_unit"), material));
		CHECK(material.techsetName == "wc_l_sm_r0c0" && material.textures.size() == 2 && material.raw.surfaceTypeBits == 0x10 && material.raw.sortKey == 1);
		CHECK(zone.find_asset(c1::ASSET_TYPE_TECHNIQUE_SET, "wc_l_sm_r0c0") != 0);
		bool colour = false, identity = false;
		for (const z::MaterialTexture& t : material.textures)
		{
			colour = colour || t.imageName == "codol_unit_col";
			identity = identity || t.imageName == "$identitynormalmap";
		}
		CHECK(colour && identity);
		CHECK(z::read_material(zone, zone.find_asset(c1::ASSET_TYPE_MATERIAL, "mc/codol_unit"), material));
		CHECK(material.techsetName == "mc_l_sm_r0c0n0s0" && material.textures.size() == 3);
		CHECK(z::read_material(zone, zone.find_asset(c1::ASSET_TYPE_MATERIAL, "wc/codol_unit_twosided"), material));
		CHECK(material.techsetName == "wc_l_sm_b0c0" && !material.stateBits.empty());
		z::ImageView view;
		CHECK(z::read_image(zone, zone.find_asset(c1::ASSET_TYPE_IMAGE, "codol_unit_col"), view));
		CHECK(!view.streamed && view.raw.width == 64 && view.loadDef.levelCount == 7 && view.pixels.size() == image.data_bytes());
		z::ModelView box;
		CHECK(z::read_model(zone, zone.find_asset(c1::ASSET_TYPE_XMODEL, "codol_unit_box"), box));
		CHECK(box.bones.size() == 1 && box.bones[0].name == "tag_origin" && box.lods.size() == 1 && box.lods[0].dist == 800.0f);
		CHECK(box.materialNames.size() == 1 && box.materialNames[0] == "mc/codol_unit" && box.raw.numCollSurfs == 1 && box.raw.contents == 1);
		if (box.lods.size() == 1 && box.lods[0].surfs.surfaces.size() == 1)
		{
			const z::SurfaceView& s = box.lods[0].surfs.surfaces[0];
			const auto vertices = s.vertices();
			CHECK(s.vertCount == 24 && s.triCount == 12 && vertices.size() == 24 && std::fabs(std::fabs(vertices[0].position[0]) - 16) < 1e-6f);
			CHECK(s.rigid_lists().size() == 1 && s.rigid_lists()[0].vertCount == 24);
		}
		else
		{
			CHECK(false);
		}
		// and a second time through the editor: identical
		z::ZoneEditor editor;
		std::vector<std::uint8_t> again;
		CHECK(editor.load(bytes, "codol_assets_test") && editor.write(again) && again == bytes);

		// carry: the animations and the model of that zone, as a fast file, into another zone - with the material and
		// the image the model points at; a second time nothing is taken
		const fs::path file = scratch / "codol_assets_test_carry.ff";
		CHECK(z::write_fastfile(file, bytes, linker.key()));
		link.zoneName = "codol_assets_carry";
		AssetLinker target;
		CHECK(target.begin(link));
		const std::int32_t types[] = { c1::ASSET_TYPE_XANIMPARTS, c1::ASSET_TYPE_XMODEL };
		AssetLinker::Carried carried;
		const Status taken = target.carry(file, types, carried);
		CHECK(taken);
		if (!taken)
		{
			std::printf("%s\n", taken.message.c_str());
		}
		for (const std::string& line : carried.failed)
		{
			std::printf("not carried: %s\n", line.c_str());
		}
		CHECK(carried.failed.empty() && carried.kept.empty() && carried.copied[c1::ASSET_TYPE_XANIMPARTS] == anims.size() && carried.copied[c1::ASSET_TYPE_XMODEL] == 1);
		CHECK(carried.dependencies >= 2);		// mc/codol_unit and codol_unit_col at least
		CHECK(target.editor().find_asset(c1::ASSET_TYPE_MATERIAL, "mc/codol_unit") != z::ZoneEditor::npos);
		CHECK(target.editor().find_asset(c1::ASSET_TYPE_MATERIAL, "wc/codol_unit") == z::ZoneEditor::npos);
		const std::size_t count = target.editor().asset_count();
		AssetLinker::Carried second;
		CHECK(target.carry(file, types, second) && second.copied.empty() && second.kept.size() == anims.size() + 1 && target.editor().asset_count() == count);
		CHECK(!target.carry(scratch / "codol_assets_no_such_zone.ff", types, second));
		std::vector<std::uint8_t> carriedBytes;
		const Status carriedWritten = target.write_zone(carriedBytes);
		CHECK(carriedWritten);
		if (carriedWritten)
		{
			z::Zone other;
			CHECK(other.load(carriedBytes, "codol_assets_carry") && other.warnings().empty());
			anim_tests::check_anim_zone(other, anims);
			z::ModelView moved;
			CHECK(z::read_model(other, other.find_asset(c1::ASSET_TYPE_XMODEL, "codol_unit_box"), moved));
			CHECK(moved.materialNames.size() == 1 && moved.materialNames[0] == "mc/codol_unit" && moved.lods.size() == 1 && moved.raw.numCollSurfs == 1);
		}
		else
		{
			std::printf("%s\n", carriedWritten.message.c_str());
		}
		std::error_code ec;
		fs::remove(file, ec);
	}
}

#include "test_sounds.inc"
#include "test_fx.inc"

int main()
{
	std::error_code ec;
	const fs::path scratch = fs::temp_directory_path(ec);
	test_image_files();
	test_image_compiler();
	test_iwd(scratch);
	test_materials();
	test_models();
	anim_tests::test_anims();
	test_sound_files();
	test_sound_zone(scratch);
	test_fx(scratch);
	const char* env = std::getenv("CODOL_ASSETS_GAME_DIR");
	const fs::path install = env && *env ? env : CODOL_ASSETS_GAME_DIR;
	if (fs::exists(install / "C1.exe", ec))
	{
		test_install(install, scratch);
		test_sounds_install(install);
		test_fx_install(install);
	}
	else
	{
		std::printf("no install at %s: the zone tests are skipped\n", install.string().c_str());
	}
	std::printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
