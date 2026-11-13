#pragma once

// Line based lexer for the text formats a CoDOL / IW script tree is made of.
// No dependency on radiant or imgui, so it is also built into the unit tests.

namespace codol::script
{
	enum class lang : std::uint8_t
	{
		plain,
		gsc,		// .gsc / .csc
		atr,		// animtrees/*.atr
		playeranim,	// mp/playeranim.script
		csv,		// stringtables
		cfg,		// .cfg / .vision / .sun / .shock ( key value pairs )
	};

	enum class tok : std::uint8_t
	{
		text,
		comment,
		string,
		number,
		keyword,		// language keywords
		builtin,		// engine constants ( self, level, true, undefined .. )
		preproc,		// #include, #using_animtree, #animtree
		anim,			// %anim_name
		funcref,		// ::func and file\path::func
		localized,		// &"STRING_REF"
		section,		// playeranim sections / animtree group names
		punct,
		count
	};

	struct span
	{
		std::uint32_t start;
		std::uint32_t len;
		tok kind;
	};

	// lexer state carried from one line to the next
	enum line_state : std::uint8_t
	{
		LS_NONE = 0,
		LS_BLOCK_COMMENT = 1,	// inside /* */
		LS_DEV_BLOCK = 2,		// inside /# #/ ( still lexed as code )
	};

	lang lang_from_path(std::string_view path);
	const char* lang_name(lang l);

	// lex a single line ( without the trailing newline ). `state` is updated for the next line.
	void lex_line(lang l, std::string_view line, std::uint8_t& state, std::vector<span>& out);

	// true if the '%' at `pos` starts an anim reference ( and is not the modulo operator )
	bool is_anim_percent(std::string_view line, std::size_t pos);

	inline bool is_ident_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
	inline bool is_ident_char(char c) { return is_ident_start(c) || (c >= '0' && c <= '9'); }
}
