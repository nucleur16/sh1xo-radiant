// .map reader / writer ( IW radiant format, "iwmap 4" ) and the small helpers of model.hpp.
//
// The reader keeps, for every object, the exact text it was read from ( source_text: a range of the file
// buffer, which all objects share ). The writer emits that text for objects that were not edited and
// radiant's own formatting ( taken from the format strings of CoD4Radiant ) for the rest:
//
//   iwmap 4
//   "<layer>" flags[ <words>]
//   // entity N
//   {
//   "key" "value"
//   // brush N
//   {
//   layer "<layer>"
//     contents detail;
//    ( x y z ) ( x y z ) ( x y z ) material sx sy ox oy rot 0 lightmap_gray 16384 16384 0 0 0 0
//   }
//   // brush N
//    {
//     mesh                       ( or curve )
//     {
//     contents ..; toolFlags ..;
//      material
//      lightmap_gray
//      smoothing smoothing_hard
//      W H 16 8
//      (
//   \tv x y z c r g b a t s t ls lt f 1
//      )
//     }
//    }
//   }

#include "doc_impl.hpp"

#include <charconv>
#include <cstdio>
#include <fstream>

namespace codol::edit
{
	// ---- model.hpp ----------------------------------------------------------------------------------

	bool object_header::has_content(const std::string_view word) const
	{
		return std::find(contents.begin(), contents.end(), word) != contents.end();
	}

	const std::string* entity::find(const std::string_view key) const
	{
		for (const auto& [k, v] : pairs)
		{
			if (k == key)
			{
				return &v;
			}
		}

		return nullptr;
	}

	std::string_view entity::value(const std::string_view key, const std::string_view fallback) const
	{
		const auto* v = find(key);
		return v ? std::string_view(*v) : fallback;
	}

	vec3 entity::origin() const
	{
		vec3 o;
		if (const auto* v = find("origin"))
		{
			parse_vec3(*v, o);
		}

		return o;
	}

	vec3 entity::angles() const
	{
		vec3 a;
		if (const auto* v = find("angles"))
		{
			parse_vec3(*v, a);
		}
		else if (const auto* yaw = find("angle"))
		{
			vec3 tmp;
			if (parse_vec3(*yaw, tmp))
			{
				a.y = tmp.x;
			}
		}

		return a;
	}

	namespace
	{
		bool parse_double(std::string_view text, double& out)
		{
			if (!text.empty() && text.front() == '+')
			{
				text.remove_prefix(1);
			}

			if (text.empty())
			{
				return false;
			}

			const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
			return result.ec == std::errc() && result.ptr == text.data() + text.size();
		}
	}

	bool parse_vec3(const std::string_view text, vec3& out)
	{
		int count = 0;
		std::size_t pos = 0;

		while (count < 3 && pos < text.size())
		{
			while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == ','))
			{
				pos++;
			}

			std::size_t end = pos;
			while (end < text.size() && text[end] != ' ' && text[end] != '\t' && text[end] != ',')
			{
				end++;
			}

			if (end == pos)
			{
				break;
			}

			double v = 0.0;
			if (!parse_double(text.substr(pos, end - pos), v))
			{
				break;
			}

			out[count++] = v;
			pos = end;
		}

		return count > 0;
	}

	namespace
	{
		// "%.8g" the way the C runtime of CoD4Radiant prints it: an exponent has at least three digits ( 1.3e-005 )
		int print_number(char buf[48], const double value)
		{
			int n = std::snprintf(buf, 40, "%.8g", value);

			for (int i = 0; i < n; i++)
			{
				if (buf[i] == 'e' && n - i == 4)	// e+NN / e-NN
				{
					buf[n + 1] = '\0';
					buf[n] = buf[n - 1];
					buf[n - 1] = buf[n - 2];
					buf[n - 2] = '0';
					return n + 1;
				}
			}

			return n;
		}
	}

	std::string format_number(const double value)
	{
		char buf[48];
		const int n = print_number(buf, value);
		return std::string(buf, static_cast<std::size_t>(n));
	}

	std::string format_key_number(double value)
	{
		const double rounded = std::round(value * 1e6) / 1e6;
		value = rounded;

		if (value == 0.0)
		{
			return "0";
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

	std::string format_key_vec3(const vec3& v)
	{
		return format_key_number(v.x) + " " + format_key_number(v.y) + " " + format_key_number(v.z);
	}

	// ---- reader -------------------------------------------------------------------------------------

	namespace
	{
		struct token
		{
			char type = 0;	// 'w' word, 's' quoted string, 'p' one of { } ( ) ;  0: end of file
			std::size_t begin = 0, end = 0;
		};

		struct parse_failure
		{
			std::string message;
			std::size_t pos;
		};

		struct reader
		{
			std::shared_ptr<const std::string> buffer;
			std::string_view s;
			std::size_t pos = 0;
			document::impl& d;

			reader(std::shared_ptr<const std::string> text, document::impl& out) : buffer(std::move(text)), s(*buffer), d(out) {}

			[[nodiscard]] source_text range(const std::size_t begin, const std::size_t end) const
			{
				return { buffer, begin, end > begin ? end - begin : 0 };
			}

			[[nodiscard]] std::string_view text(const token& t) const
			{
				return t.type == 's' ? s.substr(t.begin + 1, t.end - t.begin - 2) : s.substr(t.begin, t.end - t.begin);
			}

			[[nodiscard]] int line_of(const std::size_t p) const
			{
				return 1 + static_cast<int>(std::count(s.begin(), s.begin() + static_cast<std::ptrdiff_t>(std::min(p, s.size())), '\n'));
			}

			[[noreturn]] void fail(const std::string& message, const std::size_t at) const
			{
				throw parse_failure{ message, at };
			}

			void skip_space()
			{
				while (pos < s.size())
				{
					const char c = s[pos];
					if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
					{
						pos++;
					}
					else if (c == '/' && pos + 1 < s.size() && s[pos + 1] == '/')
					{
						while (pos < s.size() && s[pos] != '\n')
						{
							pos++;
						}
					}
					else
					{
						break;
					}
				}
			}

			token next()
			{
				skip_space();

				token t;
				t.begin = t.end = pos;

				if (pos >= s.size())
				{
					return t;
				}

				const char c = s[pos];

				if (c == '"')
				{
					std::size_t e = pos + 1;
					while (e < s.size() && s[e] != '"' && s[e] != '\n')
					{
						e++;
					}

					if (e >= s.size() || s[e] != '"')
					{
						fail("unterminated string", pos);
					}

					t.type = 's';
					t.end = pos = e + 1;
					return t;
				}

				if (c == '{' || c == '}' || c == '(' || c == ')' || c == ';')
				{
					t.type = 'p';
					t.end = ++pos;
					return t;
				}

				std::size_t e = pos;
				while (e < s.size())
				{
					const char ch = s[e];
					if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '{' || ch == '}' || ch == '(' || ch == ')' || ch == ';' || ch == '"')
					{
						break;
					}

					e++;
				}

				t.type = 'w';
				t.end = pos = e;
				return t;
			}

			token peek()
			{
				const std::size_t save = pos;
				const token t = next();
				pos = save;
				return t;
			}

			[[nodiscard]] bool is(const token& t, const char punct) const { return t.type == 'p' && s[t.begin] == punct; }
			[[nodiscard]] bool is_word(const token& t, const std::string_view word) const { return t.type == 'w' && text(t) == word; }

			void expect(const char punct)
			{
				const token t = next();
				if (!is(t, punct))
				{
					fail(std::string("expected '") + punct + "'", t.begin);
				}
			}

			double number()
			{
				const token t = next();
				double v = 0.0;
				if (t.type != 'w' || !parse_double(text(t), v))
				{
					fail("expected a number", t.begin);
				}

				return v;
			}

			[[nodiscard]] bool is_number(const token& t) const
			{
				double v;
				return t.type == 'w' && parse_double(text(t), v);
			}

			// index of the '\n' that ends the line of `p` ( or the end of the text )
			[[nodiscard]] std::size_t newline_at(const std::size_t p) const
			{
				const auto nl = s.find('\n', p);
				return nl == std::string_view::npos ? s.size() : nl;
			}

			// end of a structural element whose last token ends at `p`: through the line end when nothing but blanks
			// follows on the line, else `p`
			[[nodiscard]] std::size_t element_end(const std::size_t p) const
			{
				std::size_t e = p;
				while (e < s.size() && (s[e] == ' ' || s[e] == '\t' || s[e] == '\r'))
				{
					e++;
				}

				if (e >= s.size())
				{
					return s.size();
				}

				return s[e] == '\n' ? e + 1 : p;
			}

			// start of an element whose '{' is at `brace`: the start of its line when only blanks precede it
			[[nodiscard]] std::size_t element_begin(const std::size_t brace, const std::size_t not_before) const
			{
				std::size_t b = brace;
				while (b > not_before && (s[b - 1] == ' ' || s[b - 1] == '\t'))
				{
					b--;
				}

				return (b == not_before || s[b - 1] == '\n') ? b : brace;
			}

			// "// <what> N" + line end, exactly
			[[nodiscard]] int canonical_number(const std::string_view lead, const std::string_view what) const
			{
				std::string prefix = "// ";
				prefix += what;
				prefix += ' ';

				if (!lead.starts_with(prefix) || !lead.ends_with(d.eol))
				{
					return -1;
				}

				const auto digits = lead.substr(prefix.size(), lead.size() - prefix.size() - d.eol.size());
				int n = 0;

				if (digits.empty() || digits.size() > 9)
				{
					return -1;
				}

				for (const char c : digits)
				{
					if (c < '0' || c > '9')
					{
						return -1;
					}

					n = n * 10 + (c - '0');
				}

				return n;
			}

			// rest of the current line from `from`, without the line end and surrounding blanks
			[[nodiscard]] std::string_view rest_of_line(const std::size_t from, std::size_t* end = nullptr) const
			{
				const std::size_t nl = newline_at(from);
				if (end)
				{
					*end = nl;
				}

				auto r = s.substr(from, nl - from);
				while (!r.empty() && (r.back() == '\r' || r.back() == ' ' || r.back() == '\t'))
				{
					r.remove_suffix(1);
				}

				while (!r.empty() && (r.front() == ' ' || r.front() == '\t'))
				{
					r.remove_prefix(1);
				}

				return r;
			}

			// position after the '}' that closes the block whose '{' was just read
			std::size_t skip_block()
			{
				int level = 1;
				for (;;)
				{
					const token t = next();
					if (t.type == 0)
					{
						fail("unexpected end of file inside a block", s.size());
					}

					if (is(t, '{'))
					{
						level++;
					}
					else if (is(t, '}') && --level == 0)
					{
						return t.end;
					}
				}
			}

			void words_until_semicolon(std::vector<std::string>& out)
			{
				for (;;)
				{
					const token t = next();
					if (is(t, ';'))
					{
						return;
					}

					if (t.type != 'w')
					{
						fail("expected ';'", t.begin);
					}

					out.emplace_back(text(t));
				}
			}

			// layer / contents / toolFlags; true when the token was one of them
			bool header_statement(const token& t, object_header& h)
			{
				if (is_word(t, "layer"))
				{
					next();
					const token name = next();
					if (name.type != 's')
					{
						fail("expected a layer name", name.begin);
					}

					h.layer = std::string(text(name));
					return true;
				}

				if (is_word(t, "contents"))
				{
					next();
					words_until_semicolon(h.contents);
					return true;
				}

				if (is_word(t, "toolFlags"))
				{
					next();
					words_until_semicolon(h.tool_flags);
					return true;
				}

				return false;
			}

			void parse_tex(tex_def& tex, const token& name)
			{
				tex.material = std::string(text(name));
				tex.size_x = number();
				tex.size_y = number();
				tex.shift_x = number();
				tex.shift_y = number();
				tex.rotation = number();
				tex.extra = number();
			}

			// after the first '('
			void parse_face(brush& b)
			{
				face f;

				for (int i = 0; i < 3; i++)
				{
					if (i > 0)
					{
						expect('(');
					}

					f.points[i].x = number();
					f.points[i].y = number();
					f.points[i].z = number();
					expect(')');
				}

				const std::size_t line_end = newline_at(pos);

				token t = next();
				if (t.type != 'w' || t.begin >= line_end)
				{
					fail("expected a material", t.begin);
				}

				parse_tex(f.texture, t);

				t = peek();
				if (t.type == 'w' && t.begin < line_end && !is_number(t))
				{
					next();
					parse_tex(f.lightmap, t);

					// whatever else is on the line
					std::size_t stop = line_end;
					const auto close = s.substr(pos, line_end - pos).find_first_of("{}(");
					if (close != std::string_view::npos)
					{
						stop = pos + close;
					}

					auto rest = s.substr(pos, stop - pos);
					while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' ' || rest.back() == '\t'))
					{
						rest.remove_suffix(1);
					}

					while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t'))
					{
						rest.remove_prefix(1);
					}

					if (rest.starts_with("//"))
					{
						fail("comment on a face line", pos);	// kept verbatim through the opaque path
					}

					f.trailing = std::string(rest);
					pos = stop;
				}
				else
				{
					fail("face without lightmap fields", t.begin);
				}

				b.faces.push_back(std::move(f));
			}

			void parse_brush_body(brush& b)
			{
				for (;;)
				{
					const token t = peek();

					if (t.type == 0)
					{
						fail("unexpected end of file inside a brush", s.size());
					}

					if (is(t, '}'))
					{
						next();
						return;
					}

					if (is(t, '('))
					{
						next();
						parse_face(b);
					}
					else if (header_statement(t, b))
					{
					}
					else if (is(t, '{'))
					{
						next();
						const std::size_t end = skip_block();
						b.extra.emplace_back(s.substr(t.begin, end - t.begin));
					}
					else if (t.type == 'w')
					{
						// a statement that is not known: up to the end of the line; a block may follow ( physics_box { .. } )
						std::size_t end = 0;
						const auto line = rest_of_line(t.begin, &end);
						if (line.find_first_of("{}") != std::string_view::npos)
						{
							fail("unknown statement", t.begin);
						}

						b.extra.emplace_back(line);
						pos = end;
					}
					else
					{
						fail("unexpected token in a brush", t.begin);
					}
				}
			}

			// after "mesh {" / "curve {"
			void parse_patch_body(patch& p)
			{
				std::vector<std::string> names;

				// header statements, the material lines, then "W H 16 8"
				for (;;)
				{
					const token t = peek();

					if (header_statement(t, p))
					{
						continue;
					}

					if (is_word(t, "smoothing"))
					{
						next();
						const token w = next();
						if (w.type != 'w')
						{
							fail("expected a smoothing mode", w.begin);
						}

						p.smoothing = std::string(text(w));
						continue;
					}

					if (t.type != 'w')
					{
						fail("unexpected token in a patch", t.begin);
					}

					if (is_number(t))
					{
						break;
					}

					next();
					names.emplace_back(text(t));
				}

				if (names.empty() || names.size() > 2)
				{
					fail("patch: expected a material and a lightmap", pos);
				}

				p.material = names[0];
				p.lightmap = names.size() > 1 ? names[1] : std::string();

				{
					const std::size_t line_end = newline_at(peek().begin);
					std::vector<double> numbers;

					for (;;)
					{
						const token t = peek();
						if (t.begin >= line_end || !is_number(t))
						{
							break;
						}

						numbers.push_back(number());
					}

					if (numbers.size() < 2 || numbers.size() > 4)
					{
						fail("patch: bad size line", pos);
					}

					p.width = static_cast<int>(numbers[0]);
					p.height = static_cast<int>(numbers[1]);
					p.has_header_scale = numbers.size() > 2;
					p.has_header_int = numbers.size() > 3;
					p.header_scale = numbers.size() > 2 ? numbers[2] : 16.0;
					p.header_int = numbers.size() > 3 ? static_cast<int>(numbers[3]) : 8;

					if (numbers[0] != p.width || numbers[1] != p.height || p.width < 1 || p.height < 1 || p.width > 4096 || p.height > 4096
						|| (numbers.size() > 3 && numbers[3] != p.header_int))
					{
						fail("patch: bad size line", pos);
					}
				}

				p.vertices.reserve(static_cast<std::size_t>(p.width) * p.height);

				for (int column = 0; column < p.width; column++)
				{
					expect('(');

					for (int row = 0; row < p.height; row++)
					{
						token t = next();
						if (!is_word(t, "v"))
						{
							fail("patch: expected 'v'", t.begin);
						}

						patch_vertex v;
						v.xyz.x = number();
						v.xyz.y = number();
						v.xyz.z = number();

						for (;;)
						{
							t = peek();

							if (is_word(t, "c") && !v.has_color && v.tex.empty() && !v.has_flag)
							{
								next();
								v.has_color = true;

								for (auto& channel : v.color)
								{
									const double value = number();
									if (value < 0.0 || value > 255.0 || value != std::floor(value))
									{
										fail("patch: bad colour", pos);
									}

									channel = static_cast<std::uint8_t>(value);
								}
							}
							else if (is_word(t, "t") && v.tex.empty() && !v.has_flag)
							{
								next();
								while (is_number(peek()))
								{
									v.tex.push_back(number());
								}

								if (v.tex.empty())
								{
									fail("patch: 't' without numbers", pos);
								}
							}
							else if (is_word(t, "f") && !v.has_flag)
							{
								next();
								const double value = number();
								if (value != std::floor(value))
								{
									fail("patch: bad flag", pos);
								}

								v.has_flag = true;
								v.flag = static_cast<int>(value);
							}
							else
							{
								break;
							}
						}

						p.vertices.push_back(std::move(v));
					}

					expect(')');
				}

				expect('}');
				expect('}');
			}

			// `brace`: the '{' token of the child, already consumed
			void parse_child(entity& owner, std::size_t& cursor, const token& brace)
			{
				const std::size_t body_begin = element_begin(brace.begin, cursor);
				const source_text lead = range(cursor, body_begin);
				const int number = canonical_number(lead.view(), "brush");
				const std::size_t after_brace = pos;

				const auto id = static_cast<object_id>(d.slots.size());
				std::shared_ptr<object_header> made;
				object_kind kind = object_kind::brush;

				try
				{
					const token first = peek();

					if (is_word(first, "mesh") || is_word(first, "curve"))
					{
						next();
						expect('{');

						auto p = std::make_shared<patch>();
						p->type = is_word(first, "curve") ? patch_type::curve : patch_type::mesh;
						parse_patch_body(*p);
						made = p;
						kind = object_kind::patch;
					}
					else
					{
						auto b = std::make_shared<brush>();
						parse_brush_body(*b);
						b->opaque = b->faces.empty();	// only blocks that are not modelled ( physics_box .. ): nothing to edit
						made = b;
					}
				}
				catch (const parse_failure& f)
				{
					// keep the block as it is: an opaque brush
					pos = after_brace;
					skip_block();

					auto b = std::make_shared<brush>();
					b->opaque = true;
					made = b;
					kind = object_kind::brush;

					d.warnings.push_back("line " + std::to_string(line_of(f.pos)) + ": " + f.message + " - the block is kept as it is and can not be edited");
				}

				const std::size_t end = element_end(pos);

				made->id = id;
				made->owner = owner.id;
				made->revision = ++d.next_revision;
				made->source_lead = lead;
				made->source_body = range(body_begin, end);
				made->source_number = (number == static_cast<int>(owner.children.size())) ? number : -1;

				owner.children.push_back(id);

				slot sl;
				sl.kind = kind;
				sl.ptr = kind == object_kind::patch ? std::shared_ptr<const void>(std::static_pointer_cast<patch>(made))
					: std::shared_ptr<const void>(std::static_pointer_cast<brush>(made));
				d.slots.push_back(std::move(sl));
				(kind == object_kind::patch ? d.n_patches : d.n_brushes)++;

				cursor = end;
				pos = end;
			}

			void parse_entity(std::size_t& cursor, const token& brace)
			{
				auto e = std::make_shared<entity>();
				e->id = static_cast<object_id>(d.slots.size());
				e->revision = ++d.next_revision;

				// reserve the slot now: the children get the following ids
				d.slots.push_back({ object_kind::entity, nullptr });

				const std::size_t body_begin = element_begin(brace.begin, cursor);
				e->source_lead = range(cursor, body_begin);

				const int number = canonical_number(e->source_lead.view(), "entity");
				e->source_number = (number == static_cast<int>(d.order.size())) ? number : -1;

				std::size_t last_head_token = brace.end;
				bool head_open = true;

				const auto close_head = [&](const std::size_t next_token)
				{
					if (head_open)
					{
						head_open = false;
						cursor = std::min(element_end(last_head_token), next_token);
						cursor = std::max(cursor, last_head_token);
						e->source_head = range(body_begin, cursor);
					}
				};

				for (;;)
				{
					const token t = next();

					if (t.type == 0)
					{
						fail("unexpected end of file inside an entity", s.size());
					}

					if (is(t, '}'))
					{
						close_head(t.begin);
						const std::size_t end = element_end(t.end);
						e->source_tail = range(cursor, end);
						cursor = end;
						pos = end;
						break;
					}

					if (is(t, '{'))
					{
						close_head(element_begin(t.begin, last_head_token));
						parse_child(*e, cursor, t);
						continue;
					}

					if (t.type == 's')
					{
						const token value = next();
						if (value.type != 's')
						{
							fail("key without a value", t.begin);
						}

						e->pairs.emplace_back(std::string(text(t)), std::string(text(value)));
						last_head_token = value.end;
					}
					else if (is_word(t, "layer"))
					{
						const token name = next();
						if (name.type != 's')
						{
							fail("expected a layer name", name.begin);
						}

						e->layer = std::string(text(name));
						last_head_token = name.end;
					}
					else if (t.type == 'w')
					{
						std::size_t end = 0;
						const auto line = rest_of_line(t.begin, &end);
						if (line.find_first_of("{}") != std::string_view::npos)
						{
							fail("unknown statement in an entity", t.begin);
						}

						e->extra.emplace_back(line);
						pos = end;
						last_head_token = t.begin + line.size();
					}
					else
					{
						fail("unexpected token in an entity", t.begin);
					}

					if (!head_open)
					{
						e->source_late_pairs = true;
					}
				}

				d.order.push_back(e->id);
				d.slots[e->id].ptr = e;
			}

			void parse_file()
			{
				// line end of the file
				{
					const auto nl = s.find('\n');
					d.eol = (nl != std::string_view::npos && nl > 0 && s[nl - 1] == '\r') ? "\r\n" : "\n";
				}

				token t = next();
				if (!is_word(t, "iwmap"))
				{
					fail("not a radiant map: the file does not start with \"iwmap\"", t.begin);
				}

				d.version = static_cast<int>(number());
				if (d.version != 4)
				{
					d.warnings.push_back("map version " + std::to_string(d.version) + " ( expected 4 )");
				}

				std::size_t cursor = element_end(pos);
				if (cursor == pos && pos < s.size())
				{
					fail("unexpected text after the version", pos);
				}

				pos = cursor;

				// layer table: lines that start with a string
				for (;;)
				{
					const std::size_t line_begin = pos;
					t = peek();

					if (t.type != 's' || t.begin != line_begin)
					{
						break;
					}

					next();

					layer l;
					l.name = std::string(text(t));

					const std::size_t nl = newline_at(pos);
					auto rest = s.substr(pos, nl - pos);
					if (!rest.empty() && rest.back() == '\r')
					{
						rest.remove_suffix(1);
					}

					std::size_t skip = 0;
					while (skip < rest.size() && (rest[skip] == ' ' || rest[skip] == '\t'))
					{
						skip++;
					}

					if (!rest.substr(skip).starts_with("flags"))
					{
						fail("layer line without \"flags\"", t.begin);
					}

					l.flags = std::string(rest.substr(skip + 5));
					l.active = (" " + l.flags + " ").find(" active ") != std::string::npos;
					d.layers.push_back(std::move(l));

					pos = cursor = (nl < s.size() ? nl + 1 : nl);
				}

				d.header_source = range(0, cursor);
				d.header_dirty = false;

				for (;;)
				{
					t = next();
					if (t.type == 0)
					{
						break;
					}

					if (!is(t, '{'))
					{
						fail("expected '{'", t.begin);
					}

					parse_entity(cursor, t);
				}

				d.trailer = range(cursor, s.size());

				if (d.order.empty())
				{
					fail("the map has no entities", s.size());
				}
			}
		};
	}

	bool document::impl::parse(std::shared_ptr<const std::string> text, impl& out, std::string& error)
	{
		reader r(text, out);
		out.source = text;

		try
		{
			r.parse_file();
		}
		catch (const parse_failure& f)
		{
			error = "line " + std::to_string(r.line_of(f.pos)) + ": " + f.message;
			return false;
		}

		const auto* world = out.get_entity(out.world());
		if (!world || !world->is_worldspawn())
		{
			out.warnings.emplace_back("the first entity is not the worldspawn");
		}

		return true;
	}

	// ---- writer -------------------------------------------------------------------------------------

	namespace
	{
		void put_number(std::string& out, const double value)
		{
			char buf[48];
			const int n = print_number(buf, value);
			out.append(buf, static_cast<std::size_t>(n));
		}

		void put_tex(std::string& out, const tex_def& t)
		{
			out += ' ';
			out += t.material;

			for (const double v : { t.size_x, t.size_y, t.shift_x, t.shift_y, t.rotation, t.extra })
			{
				out += ' ';
				put_number(out, v);
			}
		}

		void put_header_statements(std::string& out, const object_header& h, const std::string& eol)
		{
			if (!h.layer.empty())
			{
				out += "layer \"";
				out += h.layer;
				out += "\"";
				out += eol;
			}

			if (!h.contents.empty())
			{
				out += "  contents";
				for (const auto& w : h.contents)
				{
					out += ' ';
					out += w;
				}

				out += ';';
				out += eol;
			}

			if (!h.tool_flags.empty())
			{
				out += "  toolFlags";
				for (const auto& w : h.tool_flags)
				{
					out += ' ';
					out += w;
				}

				out += ';';
				out += eol;
			}
		}

		void put_brush(std::string& out, const brush& b, const std::string& eol)
		{
			out += '{';
			out += eol;
			put_header_statements(out, b, eol);

			for (const face& f : b.faces)
			{
				for (const vec3& p : f.points)
				{
					out += " (";
					for (int i = 0; i < 3; i++)
					{
						out += ' ';
						put_number(out, p[i]);
					}

					out += " )";
				}

				put_tex(out, f.texture);
				put_tex(out, f.lightmap);

				if (!f.trailing.empty())
				{
					out += ' ';
					out += f.trailing;
				}

				out += eol;
			}

			for (const auto& extra : b.extra)
			{
				out += extra;
				out += eol;
			}

			out += '}';
			out += eol;
		}

		void put_patch(std::string& out, const patch& p, const std::string& eol)
		{
			out += " {";
			out += eol;
			out += p.type == patch_type::curve ? "  curve" : "  mesh";
			out += eol;
			out += "  {";
			out += eol;
			put_header_statements(out, p, eol);

			out += "   ";
			out += p.material;
			out += eol;

			if (!p.lightmap.empty())
			{
				out += "   ";
				out += p.lightmap;
				out += eol;
			}

			if (!p.smoothing.empty())
			{
				out += "   smoothing ";
				out += p.smoothing;
				out += eol;
			}

			out += "   " + std::to_string(p.width) + " " + std::to_string(p.height);
			if (p.has_header_scale)
			{
				out += ' ';
				char buf[48];
				std::snprintf(buf, sizeof(buf), "%g", p.header_scale);
				out += buf;

				if (p.has_header_int)
				{
					out += ' ';
					out += std::to_string(p.header_int);
				}
			}

			out += eol;

			for (int column = 0; column < p.width; column++)
			{
				out += "   (";
				out += eol;

				for (int row = 0; row < p.height; row++)
				{
					const patch_vertex& v = p.at(column, row);

					out += "\tv";
					for (int i = 0; i < 3; i++)
					{
						out += ' ';
						put_number(out, v.xyz[i]);
					}

					if (v.has_color)
					{
						out += " c";
						for (const auto channel : v.color)
						{
							out += ' ';
							out += std::to_string(channel);
						}
					}

					if (!v.tex.empty())
					{
						out += " t";
						for (const double t : v.tex)
						{
							out += ' ';
							put_number(out, t);
						}
					}

					if (v.has_flag)
					{
						out += " f ";
						out += std::to_string(v.flag);
					}

					out += eol;
				}

				out += "   )";
				out += eol;
			}

			for (const auto& extra : p.extra)
			{
				out += extra;
				out += eol;
			}

			out += "  }";
			out += eol;
			out += " }";
			out += eol;
		}
	}

	std::string document::impl::write(const bool canonical) const
	{
		std::string out;
		out.reserve((source ? source->size() : 0) + 4096);

		if (!canonical && !header_dirty && header_source.buffer)
		{
			out += header_source.view();
		}
		else
		{
			out += "iwmap " + std::to_string(version) + eol;

			for (const layer& l : layers)
			{
				out += '"';
				out += l.name;
				out += "\" flags";
				out += l.flags;
				out += eol;
			}
		}

		for (std::size_t i = 0; i < order.size(); i++)
		{
			const entity* e = get_entity(order[i]);
			if (!e)
			{
				continue;
			}

			const bool head_new = canonical || !e->source_head.buffer;
			const bool rewrite_all = canonical || (head_new && e->source_late_pairs);

			if (rewrite_all || e->source_number >= 0 || !e->source_lead.buffer)
			{
				out += "// entity " + std::to_string(i) + eol;
			}
			else
			{
				out += e->source_lead.view();
			}

			if (!head_new)
			{
				out += e->source_head.view();
			}
			else
			{
				out += '{';
				out += eol;

				if (!e->layer.empty())
				{
					out += "layer \"" + e->layer + "\"" + eol;
				}

				for (const auto& extra : e->extra)
				{
					out += extra;
					out += eol;
				}

				for (const auto& [k, v] : e->pairs)
				{
					out += '"';
					out += k;
					out += "\" \"";
					out += v;
					out += '"';
					out += eol;
				}
			}

			for (std::size_t c = 0; c < e->children.size(); c++)
			{
				const object_id child = e->children[c];
				const brush* b = get_brush(child);
				const patch* p = b ? nullptr : get_patch(child);
				const object_header* h = b ? static_cast<const object_header*>(b) : p;

				if (!h)
				{
					continue;
				}

				const bool opaque = b && b->opaque;

				if (rewrite_all || h->source_number >= 0 || !h->source_lead.buffer)
				{
					out += "// brush " + std::to_string(c) + eol;
				}
				else
				{
					out += h->source_lead.view();
				}

				if (h->source_body.buffer && (!canonical || opaque))
				{
					out += h->source_body.view();
				}
				else if (b)
				{
					put_brush(out, *b, eol);
				}
				else
				{
					put_patch(out, *p, eol);
				}
			}

			if (!rewrite_all && e->source_tail.buffer)
			{
				out += e->source_tail.view();
			}
			else
			{
				out += '}';
				out += eol;
			}
		}

		if (trailer.buffer)
		{
			out += trailer.view();
		}

		return out;
	}

	// ---- document: files ----------------------------------------------------------------------------

	bool document::load_map_text(std::string text, std::string& error)
	{
		auto fresh = std::make_unique<impl>();
		auto buffer = std::make_shared<const std::string>(std::move(text));

		if (!impl::parse(buffer, *fresh, error))
		{
			return false;
		}

		impl::carry_over(*d, *fresh);
		d = std::move(fresh);
		return true;
	}

	bool document::load_map_file(const std::filesystem::path& file, std::string& error)
	{
		std::ifstream in(file, std::ios::binary);
		if (!in.is_open())
		{
			error = "can not open " + file.string();
			return false;
		}

		std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return load_map_text(std::move(text), error);
	}

	std::string document::save_map_text() const
	{
		return d->write(false);
	}

	std::string document::save_map_text_canonical() const
	{
		return d->write(true);
	}

	bool document::save_map_file(const std::filesystem::path& file, std::string& error) const
	{
		const std::string text = d->write(false);

		std::ofstream out(file, std::ios::binary | std::ios::trunc);
		if (!out.is_open())
		{
			error = "can not write " + file.string();
			return false;
		}

		out.write(text.data(), static_cast<std::streamsize>(text.size()));
		out.close();

		if (!out)
		{
			error = "write error on " + file.string();
			return false;
		}

		return true;
	}
}
