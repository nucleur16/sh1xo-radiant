#pragma once

// Index of everything animation related in a CoDOL / IW script tree:
//  - animtrees/*.atr                 ( which anims exist in which tree )
//  - mp/playeranim.script            ( state machine that drives player animations in multiplayer )
//  - *.gsc                           ( #using_animtree, %anim references, level.scr_anim scene tables, anim_* calls )
//
// The index is what powers the "Animations" views of the script workbench and its validation:
// a `%anim` that is not part of the governing animtree shifts every later index of the tree that the
// server builds, which shows up in-game as T-posing / wrong clips, so it is flagged as an error.
//
// No dependency on radiant or imgui, so it is also built into the unit tests.

namespace codol::script
{
	std::string to_lower(std::string_view s);

	// ------------------------------------------------------------------------------------------
	// animtrees

	struct atr_node
	{
		std::string name;
		std::string flags;		// "additive", "loopsync", "complete" ..
		int line = 0;			// 1 based
		int depth = 0;
		bool group = false;		// has children
		int parent = -1;		// index into animtree::nodes
	};

	struct animtree
	{
		std::string name;		// "generic_human"
		std::string path;		// absolute path of the .atr
		std::vector<atr_node> nodes;
		std::unordered_map<std::string, int> lookup; // lower case anim name -> node index ( -1: only in a merged tree )
		std::vector<std::string> merged_paths;		// same tree found in later roots, names merged into lookup
		std::vector<std::string> errors;
		std::vector<std::string> warnings;	// duplicates: tolerated by the engine ( stock trees have them )

		[[nodiscard]] bool contains(std::string_view anim) const;
	};

	bool parse_animtree(std::string_view text, animtree& out);

	// ------------------------------------------------------------------------------------------
	// playeranim.script

	struct pa_command
	{
		std::string part;		// both / legs / torso / turret
		std::string anim;
		std::string extra;		// "sound xyz" ..
		int line = 0;
	};

	struct pa_block
	{
		std::string label;		// "state combat", "idle", "playerAnimType pistol, movetype prone", "default"
		int line = 0;
		int depth = 0;			// 0 = directly below a section
		int parent = -1;		// index into playeranim::blocks
		std::string section;	// ANIMATIONS / CANNED_ANIMATIONS / STATECHANGES / EVENTS
		std::vector<pa_command> commands;
	};

	struct pa_define
	{
		std::string type;		// weaponclass / movetype / playerAnimType
		std::string name;
		std::string value;
		int line = 0;
	};

	struct playeranim
	{
		std::string path;
		std::vector<pa_define> defines;
		std::vector<pa_block> blocks;
		std::vector<std::pair<std::string, int>> sections; // name, line
		std::vector<std::string> errors;
		std::size_t command_count = 0;

		// full label path of a block: "ANIMATIONS > state combat > idle > default"
		[[nodiscard]] std::string block_path(int block) const;
	};

	bool parse_playeranim(std::string_view text, playeranim& out);

	// ------------------------------------------------------------------------------------------
	// gsc

	struct gsc_anim_ref
	{
		std::string anim;		// without '%'
		std::string tree;		// governing #using_animtree ( empty if none )
		int line = 0;
	};

	struct gsc_scene
	{
		std::string table;		// scr_anim / scr_model / scr_animtree / scr_sound / scr_face / scr_text ..
		std::string animname;	// first key  ( "foley" )
		std::string scene;		// second key ( "training_intro_begining" ), may be empty
		std::string index;		// optional third key ( loops )
		std::string value;		// right hand side as written
		std::string tree;
		int line = 0;
	};

	struct gsc_anim_call
	{
		std::string func;		// anim_single_solo ..
		std::vector<std::string> strings; // string literal arguments
		int line = 0;
	};

	struct gsc_file
	{
		std::string path;		// absolute
		std::string rel;		// relative to its root, forward slashes
		std::vector<std::pair<std::string, int>> trees; // #using_animtree( "x" ), line
		std::vector<gsc_anim_ref> refs;
		std::vector<gsc_scene> scenes;
		std::vector<gsc_anim_call> calls;
		std::vector<std::pair<std::string, int>> functions; // name, line

		[[nodiscard]] bool has_anim_content() const { return !refs.empty() || !scenes.empty() || !calls.empty() || !trees.empty(); }
	};

	void scan_gsc(std::string_view text, gsc_file& out);

	// ------------------------------------------------------------------------------------------
	// diagnostics

	enum class severity : std::uint8_t { info, warning, error };

	struct diagnostic
	{
		severity sev = severity::warning;
		std::string path;		// absolute file
		int line = 0;
		std::string message;
	};

	// ------------------------------------------------------------------------------------------
	// whole tree

	class anim_index
	{
	public:
		// roots are searched in order; a file in an earlier root shadows the same relative path in a later one.
		// animtrees are the exception: the names of the same tree in later roots are merged ( like the game does )
		void build(const std::vector<std::string>& roots);
		void clear();

		// re-parse one file after it was saved ( path must be below one of the roots )
		void update_file(const std::string& abs_path);

		[[nodiscard]] const animtree* find_tree(std::string_view name) const;
		[[nodiscard]] const gsc_file* find_gsc(std::string_view abs_path) const;

		// animations the game itself can load ( optional, from a list exported out of the zones ).
		// when set, anims that are in an animtree but not in the game are reported as warnings.
		void set_known_xanims(std::vector<std::string> names);
		[[nodiscard]] bool has_known_xanims() const { return !known_xanims_.empty(); }
		[[nodiscard]] bool is_known_xanim(std::string_view name) const;

		void validate();

		std::vector<std::string> roots;
		std::map<std::string, animtree> trees;		// lower case name
		std::map<std::string, gsc_file> gsc;		// lower case relative path
		playeranim player;
		bool has_player = false;
		std::vector<diagnostic> diagnostics;

		std::size_t total_refs = 0;
		std::size_t total_scenes = 0;

	private:
		void load_file(const std::string& root, const std::string& abs_path);
		void recount();

		std::unordered_set<std::string> known_xanims_;
	};

	// utilities shared with the workbench
	bool read_text_file(const std::string& path, std::string& out);
	std::string relative_slash_path(const std::string& root, const std::string& abs_path);
}
