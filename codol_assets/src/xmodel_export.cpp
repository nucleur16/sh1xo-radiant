// xmodel_export text: reader, writer, and the extractor that turns a LOD of a loaded model into one.

#include "codol_assets/model.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace codol::assets
{
	namespace
	{
		void quat_to_axis(const float q[4], float axis[3][3])
		{
			const float x = q[0], y = q[1], z = q[2], w = q[3];
			axis[0][0] = 1 - 2 * (y * y + z * z);
			axis[0][1] = 2 * (x * y + z * w);
			axis[0][2] = 2 * (x * z - y * w);
			axis[1][0] = 2 * (x * y - z * w);
			axis[1][1] = 1 - 2 * (x * x + z * z);
			axis[1][2] = 2 * (y * z + x * w);
			axis[2][0] = 2 * (x * z + y * w);
			axis[2][1] = 2 * (y * z - x * w);
			axis[2][2] = 1 - 2 * (x * x + y * y);
		}
	}

	Status parse_xmodel_export(const std::string_view text, ExportModel& out)
	try
	{
		out = {};
		enum class Section { bones, vertices, faces, objects, materials } section = Section::bones;
		int currentBone = -1;
		ExportVertex* vertex = nullptr;
		ExportCorner* corner = nullptr;
		int cornerIndex = 0, lineNumber = 0;
		std::size_t at = 0;
		while (at < text.size())
		{
			std::size_t end = text.find('\n', at);
			end = end == std::string_view::npos ? text.size() : end;
			std::string line(text.substr(at, end - at));
			at = end + 1;
			lineNumber++;
			if (const std::size_t comment = line.find("//"); comment != std::string::npos)
			{
				line.erase(comment);
			}
			// words; one quoted string per line at most matters ( names )
			std::vector<std::string> words;
			for (std::size_t k = 0; k < line.size();)
			{
				if (line[k] == ' ' || line[k] == '\t' || line[k] == '\r' || line[k] == ',')
				{
					k++;
				}
				else if (line[k] == '"')
				{
					const std::size_t close = line.find('"', k + 1);
					words.push_back(line.substr(k + 1, (close == std::string::npos ? line.size() : close) - k - 1));
					k = close == std::string::npos ? line.size() : close + 1;
				}
				else
				{
					std::size_t e = k;
					while (e < line.size() && !std::strchr(" \t\r,", line[e]))
					{
						e++;
					}
					words.push_back(line.substr(k, e - k));
					k = e;
				}
			}
			if (words.empty())
			{
				continue;
			}
			const std::string& key = words[0];
			const auto number = [&](const std::size_t k) { return k < words.size() ? static_cast<float>(std::atof(words[k].c_str())) : 0.0f; };
			const auto integer = [&](const std::size_t k) { return k < words.size() ? std::atoi(words[k].c_str()) : 0; };
			const auto fail = [&](const char* what) { return Status::error("xmodel_export line " + std::to_string(lineNumber) + ": " + what); };
			if (key == "NUMVERTS")
			{
				section = Section::vertices;
				out.vertices.reserve(static_cast<std::size_t>(std::max(0, integer(1))));
			}
			else if (key == "NUMFACES")
			{
				section = Section::faces;
				out.faces.reserve(static_cast<std::size_t>(std::max(0, integer(1))));
			}
			else if (key == "NUMOBJECTS")
			{
				section = Section::objects;
			}
			else if (key == "NUMMATERIALS")
			{
				section = Section::materials;
			}
			else if (key == "NUMBONES")
			{
				out.bones.resize(static_cast<std::size_t>(std::max(0, integer(1))));
			}
			else if (section == Section::bones && key == "BONE")
			{
				const int index = integer(1);
				if (index < 0 || static_cast<std::size_t>(index) >= out.bones.size())
				{
					return fail("bone index outside NUMBONES");
				}
				currentBone = index;
				if (words.size() >= 4)
				{
					out.bones[static_cast<std::size_t>(index)].parent = integer(2);
					out.bones[static_cast<std::size_t>(index)].name = words[3];
				}
			}
			else if (section == Section::bones && currentBone >= 0 && (key == "OFFSET" || key == "X" || key == "Y" || key == "Z"))
			{
				ExportBone& bone = out.bones[static_cast<std::size_t>(currentBone)];
				float* target = key == "OFFSET" ? bone.offset : bone.axis[key == "X" ? 0 : key == "Y" ? 1 : 2];
				for (std::size_t k = 0; k < 3; k++)
				{
					target[k] = number(k + 1);
				}
			}
			else if (section == Section::vertices && key == "VERT")
			{
				out.vertices.emplace_back();
				vertex = &out.vertices.back();
			}
			else if (section == Section::vertices && vertex && key == "OFFSET")
			{
				for (std::size_t k = 0; k < 3; k++)
				{
					vertex->position[k] = number(k + 1);
				}
			}
			else if (section == Section::vertices && vertex && key == "BONE")
			{
				vertex->weights.emplace_back(integer(1), number(2));
			}
			else if (section == Section::faces && key == "TRI")
			{
				out.faces.emplace_back();
				out.faces.back().object = integer(1);
				out.faces.back().material = integer(2);
				cornerIndex = 0;
				corner = nullptr;
			}
			else if (section == Section::faces && key == "VERT")
			{
				if (out.faces.empty() || cornerIndex >= 3)
				{
					return fail("a triangle has three corners");
				}
				corner = &out.faces.back().corners[cornerIndex++];
				corner->vertex = integer(1);
			}
			else if (section == Section::faces && corner && key == "NORMAL")
			{
				for (std::size_t k = 0; k < 3; k++)
				{
					corner->normal[k] = number(k + 1);
				}
			}
			else if (section == Section::faces && corner && key == "COLOR")
			{
				for (std::size_t k = 0; k < 4; k++)
				{
					corner->color[k] = number(k + 1);
				}
			}
			else if (section == Section::faces && corner && key == "UV")
			{
				// UV <sets> u v
				corner->uv[0] = number(words.size() >= 4 ? 2 : 1);
				corner->uv[1] = number(words.size() >= 4 ? 3 : 2);
			}
			else if (section == Section::objects && key == "OBJECT")
			{
				out.objects.push_back(words.size() > 2 ? words[2] : std::string());
			}
			else if (section == Section::materials && key == "MATERIAL")
			{
				out.materials.push_back(words.size() > 2 ? words[2] : std::string());
			}
		}
		if (out.bones.empty())
		{
			return Status::error("xmodel_export: no bones ( not a MODEL file? )");
		}
		for (const ExportFace& face : out.faces)
		{
			for (const ExportCorner& c : face.corners)
			{
				if (c.vertex < 0 || static_cast<std::size_t>(c.vertex) >= out.vertices.size())
				{
					return Status::error("xmodel_export: a triangle names a vertex outside NUMVERTS");
				}
			}
			if (face.material < 0 || (!out.materials.empty() && static_cast<std::size_t>(face.material) >= out.materials.size()))
			{
				return Status::error("xmodel_export: a triangle names a material outside NUMMATERIALS");
			}
		}
		for (const ExportVertex& v : out.vertices)
		{
			for (const auto& [bone, weight] : v.weights)
			{
				if (bone < 0 || static_cast<std::size_t>(bone) >= out.bones.size())
				{
					return Status::error("xmodel_export: a vertex names a bone outside NUMBONES");
				}
			}
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(e.what());
	}

	Status load_xmodel_export(const std::filesystem::path& file, ExportModel& out)
	{
		std::vector<std::uint8_t> bytes;
		if (const Status s = zone::read_file(file, bytes); !s)
		{
			return s;
		}
		const Status s = parse_xmodel_export({ reinterpret_cast<const char*>(bytes.data()), bytes.size() }, out);
		return s ? s : Status::error(file.string() + ": " + s.message);
	}

	std::string write_xmodel_export(const ExportModel& model, const std::string_view comment)
	{
		std::string out;
		char line[256];
		const auto add = [&](const char* format, auto... args)
		{
			std::snprintf(line, sizeof(line), format, args...);
			out += line;
		};
		out += "// " + std::string(comment.empty() ? "codol_assets" : comment) + "\nMODEL\nVERSION 6\n\n";
		add("NUMBONES %zu\n", model.bones.size());
		for (std::size_t k = 0; k < model.bones.size(); k++)
		{
			add("BONE %zu %d \"%s\"\n", k, model.bones[k].parent, model.bones[k].name.c_str());
		}
		out += "\n";
		for (std::size_t k = 0; k < model.bones.size(); k++)
		{
			const ExportBone& b = model.bones[k];
			add("BONE %zu\n", k);
			add("OFFSET %.9g %.9g %.9g\n", b.offset[0], b.offset[1], b.offset[2]);
			out += "SCALE 1.000000 1.000000 1.000000\n";
			add("X %.9g %.9g %.9g\n", b.axis[0][0], b.axis[0][1], b.axis[0][2]);
			add("Y %.9g %.9g %.9g\n", b.axis[1][0], b.axis[1][1], b.axis[1][2]);
			add("Z %.9g %.9g %.9g\n\n", b.axis[2][0], b.axis[2][1], b.axis[2][2]);
		}
		add("NUMVERTS %zu\n", model.vertices.size());
		for (std::size_t k = 0; k < model.vertices.size(); k++)
		{
			const ExportVertex& v = model.vertices[k];
			add("VERT %zu\n", k);
			add("OFFSET %.9g %.9g %.9g\n", v.position[0], v.position[1], v.position[2]);
			add("BONES %zu\n", v.weights.size());
			for (const auto& [bone, weight] : v.weights)
			{
				add("BONE %d %.9g\n", bone, weight);
			}
			out += "\n";
		}
		add("NUMFACES %zu\n", model.faces.size());
		for (const ExportFace& f : model.faces)
		{
			add("TRI %d %d 0 0\n", f.object, f.material);
			for (const ExportCorner& c : f.corners)
			{
				add("VERT %d\n", c.vertex);
				add("NORMAL %.9g %.9g %.9g\n", c.normal[0], c.normal[1], c.normal[2]);
				add("COLOR %.9g %.9g %.9g %.9g\n", c.color[0], c.color[1], c.color[2], c.color[3]);
				add("UV 1 %.9g %.9g\n\n", c.uv[0], c.uv[1]);
			}
		}
		add("NUMOBJECTS %zu\n", model.objects.size());
		for (std::size_t k = 0; k < model.objects.size(); k++)
		{
			add("OBJECT %zu \"%s\"\n", k, model.objects[k].c_str());
		}
		add("\nNUMMATERIALS %zu\n", model.materials.size());
		for (std::size_t k = 0; k < model.materials.size(); k++)
		{
			add("MATERIAL %zu \"%s\" \"Lambert\" \"\"\n", k, model.materials[k].c_str());
			out += "COLOR 0.000000 0.000000 0.000000 1.000000\nTRANSPARENCY 0.000000 0.000000 0.000000 1.000000\nAMBIENTCOLOR 0.000000 0.000000 0.000000 1.000000\n"
				"INCANDESCENCE 0.000000 0.000000 0.000000 1.000000\nCOEFFS 0.800000 0.000000\nGLOW 0.000000 0\nREFRACTIVE 6 1.000000\n"
				"SPECULARCOLOR -1.000000 -1.000000 -1.000000 1.000000\nREFLECTIVECOLOR -1.000000 -1.000000 -1.000000 1.000000\nREFLECTIVE -1 -1.000000\n"
				"BLINN -1.000000 -1.000000\nPHONG -1.000000\n\n";
		}
		return out;
	}

	bool extract_model_lod(const zone::Zone& zone, const zone::ModelView& model, const std::size_t lod, ExportModel& out)
	{
		out = {};
		if (lod >= model.lods.size() || model.lods[lod].surfs.external)
		{
			return false;
		}
		for (const zone::ModelBone& bone : model.bones)
		{
			ExportBone b;
			b.name = std::string(bone.name);
			b.parent = bone.parent;
			std::memcpy(b.offset, bone.baseTrans, sizeof(b.offset));
			quat_to_axis(bone.baseQuat, b.axis);
			out.bones.push_back(b);
		}
		const zone::ModelLod& l = model.lods[lod];
		int base = 0;
		for (std::size_t s = 0; s < l.surfs.surfaces.size(); s++)
		{
			const zone::SurfaceView& surface = l.surfs.surfaces[s];
			const std::vector<zone::SurfaceVertex> vertices = surface.vertices();
			const std::vector<zone::VertexWeights> weights = surface.weights(zone);
			const std::vector<std::uint16_t> indices = surface.indices();
			const std::size_t materialIndex = static_cast<std::size_t>(l.surfIndex) + s;
			out.objects.push_back(std::string(l.surfsName) + "_" + std::to_string(s));
			out.materials.push_back(materialIndex < model.materialNames.size() ? std::string(model.materialNames[materialIndex]) : std::string());
			for (std::size_t v = 0; v < vertices.size(); v++)
			{
				ExportVertex e;
				std::memcpy(e.position, vertices[v].position, sizeof(e.position));
				if (v < weights.size())
				{
					for (int k = 0; k < weights[v].count; k++)
					{
						e.weights.emplace_back(weights[v].bone[k], weights[v].weight[k]);
					}
				}
				if (e.weights.empty())
				{
					e.weights.emplace_back(0, 1.0f);
				}
				out.vertices.push_back(std::move(e));
			}
			for (std::size_t t = 0; t + 2 < indices.size(); t += 3)
			{
				ExportFace f;
				f.object = f.material = static_cast<int>(s);
				for (int c = 0; c < 3; c++)
				{
					const std::uint16_t index = indices[t + static_cast<std::size_t>(c)];
					if (index >= vertices.size())
					{
						return false;
					}
					const zone::SurfaceVertex& v = vertices[index];
					f.corners[c].vertex = base + index;
					std::memcpy(f.corners[c].normal, v.normal, sizeof(v.normal));
					for (int k = 0; k < 4; k++)
					{
						f.corners[c].color[k] = static_cast<float>(v.color[k]) / 255.0f;
					}
					f.corners[c].uv[0] = v.uv[0];
					f.corners[c].uv[1] = v.uv[1];
				}
				out.faces.push_back(f);
			}
			base += static_cast<int>(vertices.size());
		}
		return true;
	}
}
