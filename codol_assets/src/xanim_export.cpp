// xanim_export text: reader and writer.
//
//   ANIMATION
//   VERSION 3
//   NUMPARTS 2
//   PART 0 "tag_origin"
//   PART 1 "j_door"
//   FRAMERATE 30
//   NUMFRAMES 31
//   FRAME 0
//   PART 0
//   OFFSET 0.000000 0.000000 0.000000
//   SCALE 1.000000 1.000000 1.000000        ( read and ignored: the game has no bone scale )
//   X 1.000000 0.000000 0.000000
//   Y 0.000000 1.000000 0.000000
//   Z 0.000000 0.000000 1.000000
//   ..
//   NOTETRACKS
//   PART 0
//   NUMTRACKS 1
//   NOTETRACK 0
//   NUMKEYS 1
//   FRAME 12 "door_open"
//
// A part that a frame does not list keeps its pose of the frame before.

#include "codol_assets/anim.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace codol::assets
{
	Status parse_xanim_export(const std::string_view text, ExportAnim& out)
	try
	{
		out = {};
		bool inNotes = false, sawAnimation = false, firstFrameSeen = false;
		int declaredFrames = -1, lineNumber = 0;
		std::vector<ExportPose>* frame = nullptr;
		ExportPose* pose = nullptr;
		std::size_t at = 0;
		while (at < text.size())
		{
			std::size_t end = text.find('\n', at);
			end = end == std::string_view::npos ? text.size() : end;
			std::string line(text.substr(at, end - at));
			at = end + 1;
			lineNumber++;
			if (line.rfind("//#LOCAL", 0) == 0)
			{
				out.local = true;
				continue;
			}
			if (line.rfind("//#NOTRANS ", 0) == 0)
			{
				out.noTranslation.push_back(std::atoi(line.c_str() + 11));
				continue;
			}
			if (inNotes)
			{
				// FRAME <number> "<name>": the name runs to the last quote of the line, it may have quotes of its own
				// ( shipped: anim_movement = "run" )
				const std::size_t start = line.find_first_not_of(" \t");
				if (start != std::string::npos && line.compare(start, 6, "FRAME ") == 0)
				{
					const std::size_t open = line.find('"', start), close = line.rfind('"');
					if (open == std::string::npos || close <= open)
					{
						return Status::error("xanim_export line " + std::to_string(lineNumber) + ": a note needs a frame and a name in quotes");
					}
					out.notes.push_back({ line.substr(open + 1, close - open - 1), static_cast<float>(std::atof(line.c_str() + start + 6)) });
					continue;
				}
			}
			// a comment starts at "//" outside a quoted name
			bool quoted = false;
			for (std::size_t k = 0; k + 1 < line.size(); k++)
			{
				if (line[k] == '"')
				{
					quoted = !quoted;
				}
				else if (!quoted && line[k] == '/' && line[k + 1] == '/')
				{
					line.erase(k);
					break;
				}
			}
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
			const auto fail = [&](const std::string& what) { return Status::error("xanim_export line " + std::to_string(lineNumber) + ": " + what); };
			if (key == "ANIMATION")
			{
				sawAnimation = true;
			}
			else if (key == "VERSION")
			{
				if (integer(1) != 3)
				{
					return fail("VERSION " + std::to_string(integer(1)) + ", expected 3");
				}
			}
			else if (key == "NUMPARTS")
			{
				if (integer(1) < 0 || integer(1) > 255)
				{
					return fail("an animation has at most 255 parts");
				}
				out.parts.assign(static_cast<std::size_t>(integer(1)), std::string());
			}
			else if (key == "FRAMERATE")
			{
				out.framerate = number(1);
			}
			else if (key == "NUMFRAMES")
			{
				declaredFrames = integer(1);
			}
			else if (key == "NOTETRACKS")
			{
				inNotes = true;
				frame = nullptr;
				pose = nullptr;
			}
			else if (key == "FRAME")
			{
				if (inNotes)
				{
					if (words.size() < 3)
					{
						return fail("a note needs a frame and a name");
					}
					out.notes.push_back({ words[2], number(1) });
				}
				else
				{
					if (!firstFrameSeen)
					{
						out.firstFrame = integer(1);
						firstFrameSeen = true;
					}
					// starts as a copy of the frame before, so that parts a frame leaves out keep their pose
					out.frames.push_back(out.frames.empty() ? std::vector<ExportPose>(out.parts.size()) : out.frames.back());
					frame = &out.frames.back();
					pose = nullptr;
				}
			}
			else if (key == "PART")
			{
				const int index = integer(1);
				if (index < 0 || static_cast<std::size_t>(index) >= out.parts.size())
				{
					return fail("PART " + std::to_string(index) + " of " + std::to_string(out.parts.size()));
				}
				if (frame)
				{
					pose = &(*frame)[static_cast<std::size_t>(index)];
				}
				else if (!inNotes)
				{
					if (words.size() < 3)
					{
						return fail("a part needs a name");
					}
					out.parts[static_cast<std::size_t>(index)] = words[2];
				}
			}
			else if (key == "OFFSET" || key == "X" || key == "Y" || key == "Z")
			{
				if (!pose)
				{
					return fail(key + " outside a part of a frame");
				}
				if (words.size() < 4)
				{
					return fail(key + " needs three numbers");
				}
				float* target = key == "OFFSET" ? pose->offset : pose->axis[key == "X" ? 0 : key == "Y" ? 1 : 2];
				for (std::size_t c = 0; c < 3; c++)
				{
					target[c] = number(1 + c);
				}
			}
			// SCALE, NUMTRACKS, NOTETRACK, NUMKEYS: nothing to keep
		}
		if (!sawAnimation)
		{
			return Status::error("xanim_export: no ANIMATION line");
		}
		if (out.frames.empty())
		{
			return Status::error("xanim_export: no frames");
		}
		if (declaredFrames >= 0 && static_cast<std::size_t>(declaredFrames) != out.frames.size())
		{
			return Status::error("xanim_export: NUMFRAMES " + std::to_string(declaredFrames) + ", but " + std::to_string(out.frames.size()) + " frames");
		}
		if (out.frames.size() > 65536)
		{
			return Status::error("xanim_export: more than 65536 frames");
		}
		for (std::size_t k = 0; k < out.parts.size(); k++)
		{
			if (out.parts[k].empty())
			{
				return Status::error("xanim_export: part " + std::to_string(k) + " has no name");
			}
		}
		if (!(out.framerate > 0))
		{
			return Status::error("xanim_export: FRAMERATE must be positive");
		}
		return {};
	}
	catch (const std::exception& e)
	{
		return Status::error(std::string("xanim_export: ") + e.what());
	}

	Status load_xanim_export(const std::filesystem::path& file, ExportAnim& out)
	{
		std::ifstream in(file, std::ios::binary);
		if (!in)
		{
			return Status::error("cannot open " + file.string());
		}
		std::stringstream buffer;
		buffer << in.rdbuf();
		return parse_xanim_export(buffer.str(), out);
	}

	std::string write_xanim_export(const ExportAnim& anim, const std::string_view comment)
	{
		std::string out;
		char line[256];
		if (!comment.empty())
		{
			out += "// ";
			out += comment;
			out += "\n";
		}
		if (anim.local)
		{
			out += "//#LOCAL\n";
		}
		for (const int part : anim.noTranslation)
		{
			std::snprintf(line, sizeof(line), "//#NOTRANS %d\n", part);
			out += line;
		}
		out += "ANIMATION\nVERSION 3\n\n";
		std::snprintf(line, sizeof(line), "NUMPARTS %zu\n", anim.parts.size());
		out += line;
		for (std::size_t k = 0; k < anim.parts.size(); k++)
		{
			std::snprintf(line, sizeof(line), "PART %zu \"%s\"\n", k, anim.parts[k].c_str());
			out += line;
		}
		std::snprintf(line, sizeof(line), "\nFRAMERATE %g\nNUMFRAMES %zu\n", anim.framerate, anim.frames.size());
		out += line;
		for (std::size_t f = 0; f < anim.frames.size(); f++)
		{
			std::snprintf(line, sizeof(line), "\nFRAME %d\n", anim.firstFrame + static_cast<int>(f));
			out += line;
			for (std::size_t k = 0; k < anim.frames[f].size(); k++)
			{
				const ExportPose& p = anim.frames[f][k];
				std::snprintf(line, sizeof(line), "PART %zu\nOFFSET %.6f %.6f %.6f\nSCALE 1.000000 1.000000 1.000000\n", k, p.offset[0], p.offset[1], p.offset[2]);
				out += line;
				std::snprintf(line, sizeof(line), "X %.6f %.6f %.6f\nY %.6f %.6f %.6f\nZ %.6f %.6f %.6f\n\n", p.axis[0][0], p.axis[0][1], p.axis[0][2], p.axis[1][0], p.axis[1][1],
					p.axis[1][2], p.axis[2][0], p.axis[2][1], p.axis[2][2]);
				out += line;
			}
		}
		out += "NOTETRACKS\n\n";
		for (std::size_t k = 0; k < anim.parts.size(); k++)
		{
			std::snprintf(line, sizeof(line), "PART %zu\n", k);
			out += line;
			if (k == 0 && !anim.notes.empty())
			{
				std::snprintf(line, sizeof(line), "NUMTRACKS 1\nNOTETRACK 0\nNUMKEYS %zu\n", anim.notes.size());
				out += line;
				for (const ExportNote& note : anim.notes)
				{
					std::snprintf(line, sizeof(line), "FRAME %g \"%s\"\n", note.frame, note.name.c_str());
					out += line;
				}
				out += "\n";
			}
			else
			{
				out += "NUMTRACKS 0\n\n";
			}
		}
		if (anim.parts.empty() && !anim.notes.empty())
		{
			// an animation without parts ( shipped: void_loop ) still has notes
			std::snprintf(line, sizeof(line), "NUMTRACKS 1\nNOTETRACK 0\nNUMKEYS %zu\n", anim.notes.size());
			out += line;
			for (const ExportNote& note : anim.notes)
			{
				std::snprintf(line, sizeof(line), "FRAME %g \"%s\"\n", note.frame, note.name.c_str());
				out += line;
			}
		}
		return out;
	}
}
