#include "std_include.hpp"

// windows.h macros
#undef min
#undef max

#include <cmath>

namespace codol::mapgeo
{
	namespace
	{
		constexpr double PI = 3.14159265358979323846;

		const rapidjson::Value* member(const rapidjson::Value& obj, const char* name)
		{
			if (!obj.IsObject())
			{
				return nullptr;
			}

			const auto it = obj.FindMember(name);
			return it == obj.MemberEnd() ? nullptr : &it->value;
		}

		bool read_matrix(const rapidjson::Value& obj, double m[16])
		{
			const auto* t = member(obj, "worldTransform");
			if (!t || !t->IsArray() || t->Size() != 16)
			{
				return false;
			}

			for (rapidjson::SizeType i = 0; i < 16; i++)
			{
				if (!(*t)[i].IsNumber())
				{
					return false;
				}

				m[i] = (*t)[i].GetDouble();
			}

			return true;
		}

		bool parse_document(const std::string& text, rapidjson::Document& doc, std::string& error)
		{
			doc.Parse(text.data(), text.size());

			if (doc.HasParseError() || !doc.IsObject())
			{
				error = "not a json object ( offset " + std::to_string(doc.GetErrorOffset()) + " )";
				return false;
			}

			const auto* objects = member(doc, "objects");
			if (!objects || !objects->IsArray())
			{
				error = "no \"objects\" array: not a map export of the asset tool";
				return false;
			}

			return true;
		}

		void normalize(double v[3])
		{
			const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
			if (len > 1e-12)
			{
				v[0] /= len; v[1] /= len; v[2] /= len;
			}
		}

		void cross(const double a[3], const double b[3], double out[3])
		{
			out[0] = a[1] * b[2] - a[2] * b[1];
			out[1] = a[2] * b[0] - a[0] * b[2];
			out[2] = a[0] * b[1] - a[1] * b[0];
		}
	}

	std::string format_number(double value)
	{
		// snap what is an integer / a clean fraction in the game data apart from float noise
		const double rounded = std::round(value * 1024.0) / 1024.0;
		if (std::fabs(rounded - value) < 1e-5)
		{
			value = rounded;
		}

		if (value == 0.0)
		{
			return "0"; // also turns -0 into 0
		}

		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.6f", value);

		std::string s = buf;
		while (!s.empty() && s.back() == '0')
		{
			s.pop_back();
		}

		if (!s.empty() && s.back() == '.')
		{
			s.pop_back();
		}

		return s == "-0" ? "0" : s;
	}

	void angles_from_axes(const double m[16], double angles[3], double* uniform_scale)
	{
		double fwd[3] = { m[0], m[1], m[2] };
		double left[3] = { m[4], m[5], m[6] };
		double up[3] = { m[8], m[9], m[10] };

		if (uniform_scale)
		{
			*uniform_scale = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
		}

		normalize(fwd);
		normalize(left);
		normalize(up);

		// AxisToAngles
		const double flat = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1]);
		double pitch, yaw, roll;

		if (flat > 1e-6)
		{
			yaw = std::atan2(fwd[1], fwd[0]) * 180.0 / PI;
			pitch = std::atan2(-fwd[2], flat) * 180.0 / PI;
			roll = std::atan2(left[2], up[2]) * 180.0 / PI;
		}
		else
		{
			// looking straight up or down: yaw and roll are the same rotation, put it all into yaw
			pitch = fwd[2] > 0.0 ? -90.0 : 90.0;
			yaw = std::atan2(-left[0], left[1]) * 180.0 / PI;
			roll = 0.0;
		}

		const auto wrap = [](double a)
		{
			a = std::fmod(a, 360.0);
			if (a < 0.0)
			{
				a += 360.0;
			}

			return (std::fabs(a - 360.0) < 1e-4 || std::fabs(a) < 1e-4) ? 0.0 : a;
		};

		angles[0] = wrap(pitch);
		angles[1] = wrap(yaw);
		angles[2] = wrap(roll);
	}

	bool read_brushes_json(const std::string& text, map_geometry& out, std::string& error)
	{
		rapidjson::Document doc;
		if (!parse_document(text, doc, error))
		{
			return false;
		}

		if (const auto* map = member(doc, "map"); map && map->IsString())
		{
			out.map = map->GetString();
		}

		for (const auto& obj : (*member(doc, "objects")).GetArray())
		{
			const auto* type = member(obj, "sourceType");
			const auto* faces = member(obj, "faces");

			if (!type || !type->IsString() || std::strcmp(type->GetString(), "Brush") != 0 || !faces || !faces->IsArray())
			{
				continue;
			}

			double m[16];
			if (!read_matrix(obj, m))
			{
				error = "a brush has no worldTransform";
				return false;
			}

			brush b;

			if (const auto* index = member(obj, "sourceIndex"); index && index->IsInt())
			{
				b.index = index->GetInt();
			}

			if (const auto* sub = member(obj, "subModel"); sub && sub->IsInt())
			{
				b.sub_model = sub->GetInt();
			}

			if (const auto* contents = member(obj, "contents"); contents && contents->IsString())
			{
				b.contents = static_cast<std::uint32_t>(std::strtoul(contents->GetString(), nullptr, 16));
			}

			for (const auto& face : faces->GetArray())
			{
				const auto* plane = member(face, "plane");
				if (!plane || !plane->IsArray() || plane->Size() != 4)
				{
					continue;
				}

				const double n[3] = { (*plane)[0].GetDouble(), (*plane)[1].GetDouble(), (*plane)[2].GetDouble() };
				const double d = (*plane)[3].GetDouble();

				// the plane is in the space of the brush: p_world = p * R + t ( rows of R are the axes )
				brush_side side;
				for (int k = 0; k < 3; k++)
				{
					side.normal[k] = n[0] * m[0 + k] + n[1] * m[4 + k] + n[2] * m[8 + k];
				}

				const double scale_sq = side.normal[0] * side.normal[0] + side.normal[1] * side.normal[1] + side.normal[2] * side.normal[2];
				if (scale_sq < 1e-12)
				{
					continue;
				}

				// a point on the local plane, moved to the world
				const double local[3] = { n[0] * d, n[1] * d, n[2] * d };
				double world[3];
				for (int k = 0; k < 3; k++)
				{
					world[k] = local[0] * m[0 + k] + local[1] * m[4 + k] + local[2] * m[8 + k] + m[12 + k];
				}

				normalize(side.normal);
				side.dist = side.normal[0] * world[0] + side.normal[1] * world[1] + side.normal[2] * world[2];

				if (const auto* mat = member(face, "clipMaterial"); mat && mat->IsString())
				{
					side.material = mat->GetString();
				}

				if (side.material.empty())
				{
					side.material = "caulk";
				}

				b.sides.push_back(std::move(side));
			}

			if (b.sides.size() >= 4)
			{
				out.brushes.push_back(std::move(b));
			}
		}

		return true;
	}

	bool read_static_models_json(const std::string& text, map_geometry& out, std::string& error)
	{
		rapidjson::Document doc;
		if (!parse_document(text, doc, error))
		{
			return false;
		}

		if (const auto* map = member(doc, "map"); map && map->IsString())
		{
			out.map = map->GetString();
		}

		for (const auto& obj : (*member(doc, "objects")).GetArray())
		{
			const auto* type = member(obj, "sourceType");
			const auto* asset = member(obj, "asset");

			if (!type || !type->IsString() || std::strcmp(type->GetString(), "StaticModel") != 0 || !asset || !asset->IsString())
			{
				continue;
			}

			double m[16];
			if (!read_matrix(obj, m))
			{
				error = "a static model has no worldTransform";
				return false;
			}

			static_model sm;
			sm.model = asset->GetString();
			sm.origin[0] = m[12];
			sm.origin[1] = m[13];
			sm.origin[2] = m[14];
			angles_from_axes(m, sm.angles, &sm.scale);

			// the exporter also writes the scale the game stores; prefer it over the one measured from the axes
			if (const auto* scale = member(obj, "scale"); scale && scale->IsNumber() && scale->GetDouble() > 0.0)
			{
				sm.scale = scale->GetDouble();
			}

			out.models.push_back(std::move(sm));
		}

		return true;
	}

	std::string side_to_map_line(const brush_side& side)
	{
		const double* n = side.normal;

		// two directions in the plane, starting from the world axis the normal is least aligned with
		double axis[3] = { 0.0, 0.0, 0.0 };
		const double ax = std::fabs(n[0]), ay = std::fabs(n[1]), az = std::fabs(n[2]);

		if (az >= ax && az >= ay)
		{
			axis[0] = 1.0;
		}
		else
		{
			axis[2] = 1.0;
		}

		double u[3], v[3];
		cross(n, axis, u);
		normalize(u);
		cross(n, u, v);

		// three points with cross( p2 - p0, p1 - p0 ) pointing out of the brush, which is what the map format expects
		constexpr double SPAN = 256.0;
		double p0[3], p1[3], p2[3];

		for (int k = 0; k < 3; k++)
		{
			p0[k] = n[k] * side.dist;
			p1[k] = p0[k] + v[k] * SPAN;
			p2[k] = p0[k] + u[k] * SPAN;
		}

		const auto point = [](const double p[3])
		{
			return "( " + format_number(p[0]) + " " + format_number(p[1]) + " " + format_number(p[2]) + " )";
		};

		return " " + point(p0) + " " + point(p1) + " " + point(p2) + " " + side.material
			+ " 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0";
	}

	bool is_tool_material(std::string_view name)
	{
		static constexpr std::string_view words[] =
		{
			"clip", "hint", "mantle", "nosight", "portal", "ladder", "trigger", "caulk", "nodraw", "skip", "lightgrid", "aitrig",
		};

		for (const auto& word : words)
		{
			if (name.find(word) != std::string_view::npos)
			{
				return true;
			}
		}

		return false;
	}

	bool is_tool_brush(const brush& b)
	{
		for (const auto& side : b.sides)
		{
			if (!is_tool_material(side.material))
			{
				return false;
			}
		}

		return !b.sides.empty();
	}

	void append_brush(std::string& out, const brush& b, const int number, const char* layer)
	{
		out += "// brush " + std::to_string(number) + "\n{\n";

		if (layer)
		{
			out += "layer \"";
			out += layer;
			out += "\"\n";
		}

		for (const auto& side : b.sides)
		{
			out += side_to_map_line(side);
			out += '\n';
		}

		out += "}\n";
	}
}
