// .efx text: reader and writer.

#include "codol_assets/fx.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace codol::assets
{
	namespace
	{
		using zone::Status;

		struct Token
		{
			std::string text;
			bool quoted = false;
			int line = 0;
		};

		// tokens: "strings", { } ;, words. A comment line "//#CODOL key values" becomes the three tokens #CODOL, key, "values".
		bool tokenize(const std::string_view text, std::vector<Token>& out, std::string& error)
		{
			int line = 1;
			std::size_t i = 0;
			while (i < text.size())
			{
				const char c = text[i];
				if (c == '\n')
				{
					line++;
					i++;
				}
				else if (c == ' ' || c == '\t' || c == '\r')
				{
					i++;
				}
				else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/')
				{
					std::size_t end = text.find('\n', i);
					end = end == std::string_view::npos ? text.size() : end;
					std::string_view comment = text.substr(i + 2, end - i - 2);
					while (!comment.empty() && (comment.back() == '\r' || comment.back() == ' ' || comment.back() == '\t'))
					{
						comment.remove_suffix(1);
					}
					if (comment.starts_with("#CODOL "))
					{
						comment.remove_prefix(7);
						const std::size_t space = comment.find(' ');
						out.push_back({ "#CODOL", false, line });
						out.push_back({ std::string(comment.substr(0, space)), false, line });
						out.push_back({ space == std::string_view::npos ? std::string() : std::string(comment.substr(space + 1)), true, line });
					}
					i = end;
				}
				else if (c == '"')
				{
					const std::size_t end = text.find('"', i + 1);
					if (end == std::string_view::npos)
					{
						error = "line " + std::to_string(line) + ": unterminated string";
						return false;
					}
					out.push_back({ std::string(text.substr(i + 1, end - i - 1)), true, line });
					i = end + 1;
				}
				else if (c == '{' || c == '}' || c == ';')
				{
					out.push_back({ std::string(1, c), false, line });
					i++;
				}
				else
				{
					std::size_t end = i;
					while (end < text.size() && !std::strchr(" \t\r\n{};\"", text[end]))
					{
						end++;
					}
					out.push_back({ std::string(text.substr(i, end - i)), false, line });
					i = end;
				}
			}
			return true;
		}

		struct Parser
		{
			std::vector<Token> tokens;
			std::size_t at = 0;
			std::string error;

			bool fail(const std::string& what)
			{
				if (error.empty())
				{
					error = "line " + std::to_string(at < tokens.size() ? tokens[at].line : (tokens.empty() ? 0 : tokens.back().line)) + ": " + what;
				}
				return false;
			}
			bool done() const { return at >= tokens.size(); }
			bool is(const char* punct) const { return !done() && !tokens[at].quoted && tokens[at].text == punct; }
			bool expect(const char* punct)
			{
				if (!is(punct))
				{
					return fail(std::string("'") + punct + "' expected" + (done() ? "" : ", found '" + tokens[at].text + "'"));
				}
				at++;
				return true;
			}
			bool number(float& value)
			{
				if (done() || tokens[at].quoted)
				{
					return fail("number expected");
				}
				const std::string& t = tokens[at].text;
				const auto r = std::from_chars(t.data(), t.data() + t.size(), value);
				if (r.ec != std::errc() || r.ptr != t.data() + t.size())
				{
					return fail("number expected, found '" + t + "'");
				}
				at++;
				return true;
			}
			bool integer(std::int32_t& value)
			{
				float f = 0;
				if (!number(f))
				{
					return false;
				}
				value = static_cast<std::int32_t>(f);
				return true;
			}
			bool floats(float* values, const int count)
			{
				for (int k = 0; k < count; k++)
				{
					if (!number(values[k]))
					{
						return false;
					}
				}
				return expect(";");
			}
			bool ints(std::int32_t* values, const int count)
			{
				for (int k = 0; k < count; k++)
				{
					if (!integer(values[k]))
					{
						return false;
					}
				}
				return expect(";");
			}
			bool string(std::string& value)
			{
				if (done() || is(";"))
				{
					return fail("string expected");
				}
				value = tokens[at++].text;
				return expect(";");
			}
			bool words(std::vector<std::string>& values)
			{
				while (!done() && !is(";"))
				{
					values.push_back(tokens[at++].text);
				}
				return expect(";");
			}
			bool curve(FxCurve& c, const int dimensions)
			{
				c.dimensions = dimensions;
				c.keys.clear();
				if (!expect("{"))
				{
					return false;
				}
				while (!is("}"))
				{
					float v = 0;
					if (!number(v))
					{
						return false;
					}
					c.keys.push_back(v);
				}
				at++;
				if (c.keys.empty() || c.keys.size() % static_cast<std::size_t>(1 + dimensions))
				{
					return fail("a curve needs keys of " + std::to_string(1 + dimensions) + " numbers");
				}
				return true;
			}
			bool graph(FxGraph& g, const int dimensions)
			{
				return number(g.scale) && expect("{") && curve(g.curve[0], dimensions) && curve(g.curve[1], dimensions) && expect("}") && expect(";");
			}
		};

		constexpr const char* VISUAL_TYPES[] = { "billboardSprite", "orientedSprite", "rotatedSprite", "tail", "line", "trail", "cloud", "sparkCloud",
			"sparkFountain", "model", "light", "spotLight", "sound", "decal", "runner" };

		bool parse_elem(Parser& p, EditorElem& e)
		{
			e.colorGraph.curve[0].dimensions = e.colorGraph.curve[1].dimensions = 3;
			while (!p.is("}"))
			{
				if (p.done())
				{
					return p.fail("'}' expected");
				}
				const std::string key = p.tokens[p.at++].text;
				bool ok = true;
				if (key == "#CODOL")
				{
					e.extra.emplace_back(p.tokens[p.at].text, p.tokens[p.at + 1].text);
					p.at += 2;
				}
				else if (key == "name") ok = p.string(e.name);
				else if (key == "editorFlags") ok = p.words(e.editorFlags);
				else if (key == "flags") ok = p.words(e.flags);
				else if (key == "spawnRange") ok = p.floats(e.spawnRange, 2);
				else if (key == "fadeInRange") ok = p.floats(e.fadeInRange, 2);
				else if (key == "fadeOutRange") ok = p.floats(e.fadeOutRange, 2);
				else if (key == "spawnFrustumCullRadius") ok = p.floats(&e.spawnFrustumCullRadius, 1);
				else if (key == "spawnLooping") ok = p.ints(e.spawnLooping, 2);
				else if (key == "spawnOneShot") ok = p.ints(e.spawnOneShot, 2);
				else if (key == "spawnDelayMsec") ok = p.ints(e.spawnDelayMsec, 2);
				else if (key == "lifeSpanMsec") ok = p.ints(e.lifeSpanMsec, 2);
				else if (key == "spawnOrgX") ok = p.floats(e.spawnOrigin[0], 2);
				else if (key == "spawnOrgY") ok = p.floats(e.spawnOrigin[1], 2);
				else if (key == "spawnOrgZ") ok = p.floats(e.spawnOrigin[2], 2);
				else if (key == "spawnOffsetRadius") ok = p.floats(e.spawnOffsetRadius, 2);
				else if (key == "spawnOffsetHeight") ok = p.floats(e.spawnOffsetHeight, 2);
				else if (key == "spawnAnglePitch") ok = p.floats(e.spawnAngles[0], 2);
				else if (key == "spawnAngleYaw") ok = p.floats(e.spawnAngles[1], 2);
				else if (key == "spawnAngleRoll") ok = p.floats(e.spawnAngles[2], 2);
				else if (key == "angleVelPitch") ok = p.floats(e.angularVelocity[0], 2);
				else if (key == "angleVelYaw") ok = p.floats(e.angularVelocity[1], 2);
				else if (key == "angleVelRoll") ok = p.floats(e.angularVelocity[2], 2);
				else if (key == "initialRot") ok = p.floats(e.initialRotation, 2);
				else if (key == "gravity") ok = p.floats(e.gravity, 2);
				else if (key == "elasticity") ok = p.floats(e.elasticity, 2);
				else if (key == "atlasBehavior")
				{
					std::vector<std::string> w;
					ok = p.words(w);
					e.atlasBehavior.clear();
					for (const std::string& s : w)
					{
						e.atlasBehavior += (e.atlasBehavior.empty() ? "" : " ") + s;
					}
				}
				else if (key == "atlasIndex") ok = p.ints(&e.atlasIndex, 1);
				else if (key == "atlasFps") ok = p.ints(&e.atlasFps, 1);
				else if (key == "atlasLoopCount") ok = p.ints(&e.atlasLoopCount, 1);
				else if (key == "atlasColIndexBits") ok = p.ints(&e.atlasColIndexBits, 1);
				else if (key == "atlasRowIndexBits") ok = p.ints(&e.atlasRowIndexBits, 1);
				else if (key == "atlasEntryCount") ok = p.ints(&e.atlasEntryCount, 1);
				else if (key.starts_with("velGraph") && key.size() == 10 && (key[8] == '0' || key[8] == '1') && key[9] >= 'X' && key[9] <= 'Z')
				{
					ok = p.graph(e.velGraph[key[8] - '0'][key[9] - 'X'], 1);
				}
				else if (key == "rotGraph") ok = p.graph(e.rotGraph, 1);
				else if (key == "sizeGraph0") ok = p.graph(e.sizeGraph[0], 1);
				else if (key == "sizeGraph1") ok = p.graph(e.sizeGraph[1], 1);
				else if (key == "scaleGraph") ok = p.graph(e.scaleGraph, 1);
				else if (key == "colorGraph") ok = p.graph(e.colorGraph, 3);
				else if (key == "alphaGraph") ok = p.graph(e.alphaGraph, 1);
				else if (key == "lightingFrac") ok = p.floats(&e.lightingFrac, 1);
				else if (key == "collOffset") ok = p.floats(e.collOffset, 3);
				else if (key == "collRadius") ok = p.floats(&e.collRadius, 1);
				else if (key == "fxOnImpact") ok = p.string(e.fxOnImpact);
				else if (key == "fxOnDeath") ok = p.string(e.fxOnDeath);
				else if (key == "emission") ok = p.string(e.emission);
				else if (key == "sortOrder")
				{
					ok = p.ints(&e.sortOrder, 1);
					e.hasSortOrder = true;
				}
				else if (key == "emitDist") ok = p.floats(e.emitDist, 2);
				else if (key == "emitDistVariance") ok = p.floats(e.emitDistVariance, 2);
				else if (key == "trailRepeatTime") ok = p.ints(&e.trailRepeatTime, 1);
				else if (key == "trailSplitDist") ok = p.floats(&e.trailSplitDist, 1);
				else if (key == "trailSplitArcDist") ok = p.floats(&e.trailSplitArcDist, 1);
				else if (key == "trailSplitTime") ok = p.floats(&e.trailSplitTime, 1);
				else if (key == "trailScrollTime") ok = p.floats(&e.trailScrollTime, 1);
				else if (key == "trailRepeatDist") ok = p.ints(&e.trailRepeatDist, 1);
				else if (key == "trailDef")
				{
					ok = p.expect("{");
					while (ok && !p.is("}"))
					{
						EditorTrailVertex v;
						ok = p.number(v.pos[0]) && p.number(v.pos[1]) && p.number(v.texCoord);
						e.trailVerts.push_back(v);
					}
					ok = ok && p.expect("}") && p.expect("{");
					while (ok && !p.is("}"))
					{
						std::int32_t index = 0;
						ok = p.integer(index);
						e.trailInds.push_back(static_cast<std::uint16_t>(index));
					}
					ok = ok && p.expect("}") && p.expect(";");
				}
				else if (std::find_if(std::begin(VISUAL_TYPES), std::end(VISUAL_TYPES), [&](const char* t) { return key == t; }) != std::end(VISUAL_TYPES))
				{
					e.visualType = key;
					if (p.is("{"))
					{
						p.at++;
						while (!p.done() && !p.is("}"))
						{
							e.visuals.push_back(p.tokens[p.at++].text);
						}
						ok = p.expect("}");
					}
					ok = ok && p.expect(";");
				}
				else
				{
					// a statement this reader has no field for: kept, written back, not compiled
					std::string value;
					int depth = 0;
					while (!p.done() && (depth > 0 || !p.is(";")))
					{
						depth += p.is("{") ? 1 : p.is("}") ? -1 : 0;
						if (depth < 0)
						{
							return p.fail("statement '" + key + "' is not terminated");
						}
						const Token& t = p.tokens[p.at++];
						value += (value.empty() ? "" : " ") + (t.quoted ? "\"" + t.text + "\"" : t.text);
					}
					ok = p.expect(";");
					e.extra.emplace_back("statement", key + " " + value);
				}
				if (!ok)
				{
					return false;
				}
			}
			p.at++;
			return true;
		}

		// ---- writer ----
		void number(std::string& out, const float value)
		{
			char text[40];
			std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(value == 0 ? 0.0f : value));
			out += text;
		}

		void range(std::string& out, const char* key, const float* v, const int count = 2)
		{
			out += "\t";
			out += key;
			for (int k = 0; k < count; k++)
			{
				out += " ";
				number(out, v[k]);
			}
			out += ";\n";
		}

		void irange(std::string& out, const char* key, const std::int32_t* v, const int count = 2)
		{
			out += "\t";
			out += key;
			for (int k = 0; k < count; k++)
			{
				out += " " + std::to_string(v[k]);
			}
			out += ";\n";
		}

		void graph(std::string& out, const std::string& key, const FxGraph& g)
		{
			out += "\t" + key + " ";
			number(out, g.scale);
			out += "\n\t{\n";
			for (const FxCurve& c : g.curve)
			{
				out += "\t\t{\n";
				const std::size_t stride = static_cast<std::size_t>(1 + c.dimensions);
				for (std::size_t k = 0; k < c.keys.size(); k += stride)
				{
					out += "\t\t\t";
					for (std::size_t d = 0; d < stride; d++)
					{
						out += d ? " " : "";
						number(out, c.keys[k + d]);
					}
					out += "\n";
				}
				out += "\t\t}\n";
			}
			out += "\t};\n";
		}

		void words(std::string& out, const char* key, const std::vector<std::string>& values)
		{
			out += "\t";
			out += key;
			for (const std::string& v : values)
			{
				out += " " + v;
			}
			out += ";\n";
		}
	}

	bool EditorElem::has_editor_flag(const std::string_view flag) const
	{
		return std::find(editorFlags.begin(), editorFlags.end(), flag) != editorFlags.end();
	}

	bool EditorElem::has_flag(const std::string_view flag) const
	{
		return std::find(flags.begin(), flags.end(), flag) != flags.end();
	}

	const std::string* EditorElem::find_extra(const std::string_view key) const
	{
		for (const auto& [k, v] : extra)
		{
			if (k == key)
			{
				return &v;
			}
		}
		return nullptr;
	}

	Status parse_efx(const std::string_view text, EditorEffect& out)
	{
		out = {};
		Parser p;
		if (!tokenize(text, p.tokens, p.error))
		{
			return Status::error(p.error);
		}
		if (p.tokens.size() < 2 || p.tokens[0].text != "iwfx" || (p.tokens[1].text != "1" && p.tokens[1].text != "2"))
		{
			return Status::error("not an effect file: 'iwfx 2' expected at the start");
		}
		out.version = p.tokens[1].text[0] - '0';
		p.at = 2;
		while (!p.done())
		{
			EditorElem e;
			if (!p.expect("{") || !parse_elem(p, e))
			{
				return Status::error(p.error);
			}
			for (FxGraph* g : { &e.velGraph[0][0], &e.velGraph[0][1], &e.velGraph[0][2], &e.velGraph[1][0], &e.velGraph[1][1], &e.velGraph[1][2], &e.rotGraph,
					 &e.sizeGraph[0], &e.sizeGraph[1], &e.scaleGraph, &e.colorGraph, &e.alphaGraph })
			{
				for (FxCurve& c : g->curve)
				{
					if (c.keys.empty())		// a graph the file does not have: constant 0
					{
						c.dimensions = g == &e.colorGraph ? 3 : 1;
						c.keys.assign(static_cast<std::size_t>(2 * (1 + c.dimensions)), 0.0f);
						c.keys[static_cast<std::size_t>(1 + c.dimensions)] = 1.0f;
					}
				}
			}
			out.elems.push_back(std::move(e));
		}
		return {};
	}

	Status load_efx(const std::filesystem::path& file, EditorEffect& out)
	{
		std::ifstream in(file, std::ios::binary);
		if (!in)
		{
			return Status::error("cannot read " + file.string());
		}
		std::ostringstream text;
		text << in.rdbuf();
		const Status s = parse_efx(text.str(), out);
		return s ? s : Status::error(file.string() + ": " + s.message);
	}

	std::string write_efx(const EditorEffect& effect)
	{
		std::string out = "iwfx 2\n\n";
		for (const EditorElem& e : effect.elems)
		{
			out += "{\n\tname \"" + e.name + "\";\n";
			words(out, "editorFlags", e.editorFlags);
			words(out, "flags", e.flags);
			range(out, "spawnRange", e.spawnRange);
			range(out, "fadeInRange", e.fadeInRange);
			range(out, "fadeOutRange", e.fadeOutRange);
			range(out, "spawnFrustumCullRadius", &e.spawnFrustumCullRadius, 1);
			irange(out, "spawnLooping", e.spawnLooping);
			irange(out, "spawnOneShot", e.spawnOneShot);
			irange(out, "spawnDelayMsec", e.spawnDelayMsec);
			irange(out, "lifeSpanMsec", e.lifeSpanMsec);
			range(out, "spawnOrgX", e.spawnOrigin[0]);
			range(out, "spawnOrgY", e.spawnOrigin[1]);
			range(out, "spawnOrgZ", e.spawnOrigin[2]);
			range(out, "spawnOffsetRadius", e.spawnOffsetRadius);
			range(out, "spawnOffsetHeight", e.spawnOffsetHeight);
			range(out, "spawnAnglePitch", e.spawnAngles[0]);
			range(out, "spawnAngleYaw", e.spawnAngles[1]);
			range(out, "spawnAngleRoll", e.spawnAngles[2]);
			range(out, "angleVelPitch", e.angularVelocity[0]);
			range(out, "angleVelYaw", e.angularVelocity[1]);
			range(out, "angleVelRoll", e.angularVelocity[2]);
			range(out, "initialRot", e.initialRotation);
			range(out, "gravity", e.gravity);
			range(out, "elasticity", e.elasticity);
			out += "\tatlasBehavior " + e.atlasBehavior + ";\n";
			irange(out, "atlasIndex", &e.atlasIndex, 1);
			irange(out, "atlasFps", &e.atlasFps, 1);
			irange(out, "atlasLoopCount", &e.atlasLoopCount, 1);
			irange(out, "atlasColIndexBits", &e.atlasColIndexBits, 1);
			irange(out, "atlasRowIndexBits", &e.atlasRowIndexBits, 1);
			irange(out, "atlasEntryCount", &e.atlasEntryCount, 1);
			for (int frame = 0; frame < 2; frame++)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					graph(out, std::string("velGraph") + static_cast<char>('0' + frame) + static_cast<char>('X' + axis), e.velGraph[frame][axis]);
				}
			}
			graph(out, "rotGraph", e.rotGraph);
			graph(out, "sizeGraph0", e.sizeGraph[0]);
			graph(out, "sizeGraph1", e.sizeGraph[1]);
			graph(out, "scaleGraph", e.scaleGraph);
			graph(out, "colorGraph", e.colorGraph);
			graph(out, "alphaGraph", e.alphaGraph);
			range(out, "lightingFrac", &e.lightingFrac, 1);
			range(out, "collOffset", e.collOffset, 3);
			range(out, "collRadius", &e.collRadius, 1);
			out += "\tfxOnImpact \"" + e.fxOnImpact + "\";\n";
			out += "\tfxOnDeath \"" + e.fxOnDeath + "\";\n";
			irange(out, "sortOrder", &e.sortOrder, 1);
			out += "\temission \"" + e.emission + "\";\n";
			range(out, "emitDist", e.emitDist);
			range(out, "emitDistVariance", e.emitDistVariance);
			if (e.visualType == "trail" || !e.trailVerts.empty() || e.trailSplitDist != 0 || e.trailRepeatDist != 0 || e.trailScrollTime != 0)
			{
				irange(out, "trailRepeatTime", &e.trailRepeatTime, 1);
				range(out, "trailSplitDist", &e.trailSplitDist, 1);
				range(out, "trailSplitArcDist", &e.trailSplitArcDist, 1);
				range(out, "trailSplitTime", &e.trailSplitTime, 1);
				range(out, "trailScrollTime", &e.trailScrollTime, 1);
				irange(out, "trailRepeatDist", &e.trailRepeatDist, 1);
			}
			if (!e.trailVerts.empty())
			{
				out += "\ttrailDef\n\t{\n";
				for (const EditorTrailVertex& v : e.trailVerts)
				{
					out += "\t\t";
					number(out, v.pos[0]);
					out += " ";
					number(out, v.pos[1]);
					out += " ";
					number(out, v.texCoord);
					out += "\n";
				}
				out += "\t} {\n";
				for (const std::uint16_t index : e.trailInds)
				{
					out += "\t\t" + std::to_string(index) + "\n";
				}
				out += "\t};\n";
			}
			for (const auto& [key, value] : e.extra)
			{
				if (key == "statement")
				{
					out += "\t" + value + ";\n";
				}
				else
				{
					out += "\t//#CODOL " + key + (value.empty() ? "" : " " + value) + "\n";
				}
			}
			if (!e.visualType.empty())
			{
				out += "\t" + e.visualType;
				if (e.visualType != "light" && e.visualType != "spotLight")
				{
					out += "\n\t{\n";
					for (const std::string& v : e.visuals)
					{
						out += "\t\t\"" + v + "\"\n";
					}
					out += "\t}";
				}
				out += ";\n";
			}
			out += "}\n";
		}
		return out;
	}
}
