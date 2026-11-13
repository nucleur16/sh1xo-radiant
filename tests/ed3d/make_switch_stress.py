# Writes switch_stress.cmds: map after map, opened the ways a user opens one - and from the places in a frame
# where that happens. See the header it writes.
#
#   python tests\ed3d\make_switch_stress.py [rounds]

import os
import sys

ROUNDS = int(sys.argv[1]) if len(sys.argv) > 1 else 3

# rows of the Maps window in the layout the editor is started with ( Maps docked on the left, "Maps only" on, the
# list not scrolled ) and the console's input line: window pixels
ROW = {"mp_abandon_sh_mk": (79, 166), "mp_hunger": (51, 622), "mp_killhouse_sh": (66, 646), "mp_rust": (41, 790)}
CONSOLE = (700, 1349)
DISCARD = (1278, 713)

out = []
count = 0


def add(*lines):
    out.extend(lines)


def typed(command):
    # into the Console window: its commands run where the window is drawn, at the end of the frame
    add("ui_mouse %d %d" % CONSOLE, "wait 0.2", "ui_mouse down", "ui_mouse up", "wait 0.3",
        'ui_text "%s"' % command, "wait 0.2", "ui_key enter", "wait 2")


def double_click(name):
    add("ed_focus_window Maps##window", "wait 0.4", "ui_mouse %d %d" % ROW[name], "wait 0.3",
        "ui_mouse down", "ui_mouse up", "ui_mouse down", "ui_mouse up", "wait 2")


def check(name, shot=False):
    global count
    count += 1
    # a frame that loaded a map took seconds, which a first `wait` sees as gone by: the second one really waits, so
    # that the numbers are those of the map at rest
    add("wait 1", "sel_info", "expect %s:" % name, "ed_mem %02d_%s" % (count, name), "expect ed_mem %02d_%s:" % (count, name))
    if shot:
        add("ed_shot $OUT\\switch_stress_%02d_%s.png" % (count, name), "wait 1")


add("# Map after map ( %d switches ), opened from every place in a frame a user opens one from:" % 0,
    "#   typed into the Console window        - the command runs where that window is drawn: at the end of the frame",
    "#   a double click in the Maps window    - in the middle of the frame",
    "#   \"Discard\" of the unsaved changes     - at the end of the frame ( the crash of 2026-11-12 )",
    "#   map_new / map_close / map_load        - a map of your own in the stage, no map at all",
    "# with a selection and its gizmo, the Materials window ( thumbnails ) and the Preview window in the frame.",
    "# `ed_mem` after every switch: working set and private bytes must level off, not climb.",
    "# Written by make_switch_stress.py; the pixels belong to the layout the editor is started with.",
    "wait 2")

# 1, 2: the console window
typed("map_open mp_abandon_sh_mk")
check("mp_abandon_sh_mk", True)
typed("map_open mp_hunger")
check("mp_hunger", True)

# 3: the Maps window
double_click("mp_killhouse_sh")
check("mp_killhouse_sh", True)

# 4: unsaved changes -> Discard
add("ed_box 0 0 0 64 64 64", "wait 0.5")
double_click("mp_rust")
add("ui_mouse %d %d" % DISCARD, "wait 0.4", "ui_mouse down", "wait 0.1", "ui_mouse up", "wait 2")
check("mp_rust", True)

# 5: a selection with its gizmo, the Materials window and the Preview window in the frame that closes the map
add("sel_class mp_dm_spawn 0", "ed_focus", "ed_windows set", "wait 0.5", "ed_focus_window Materials##window", "wait 1",
    "asset_preview material mc/gfx_impact_asphalt01", "wait 1", "ed_shot $OUT\\switch_stress_materials.png", "wait 1")
typed("map_open mp_abandon_sh_mk")
check("mp_abandon_sh_mk", True)

# 6: a map of your own ( the stage ), with a brush, vertex handles shown
typed("map_new $MAP")
add("ed_box 0 0 0 128 128 64", "ed_camera -150 -150 400 55 45", "ed3d tool vertex", "ed3d wclick 0 0 64", "wait 0.5", "map_save", "wait 0.5")
check("$MAP", True)

# 7: from the map of your own to a shipped one while a handle is selected, and back by its file
typed("map_open mp_hunger")
check("mp_hunger")
typed("map_load map_source/$MAP.map")
check("$MAP", True)

# 8: close, then open
typed("map_close")
add("ed_mem closed", "wait 0.5")
typed("map_open mp_killhouse_sh")
check("mp_killhouse_sh")

# the rest: round and round, alternating the three ways
names = ["mp_rust", "mp_abandon_sh_mk", "mp_hunger", "mp_killhouse_sh"]
for r in range(ROUNDS):
    for i, name in enumerate(names):
        way = (r + i) % 3
        if way == 0:
            typed("map_open " + name)
        elif way == 1:
            double_click(name)
        else:
            add("sel_class misc_model 0", "ed_move 8 0 0", "wait 0.3")
            double_click(name)
            add("ui_mouse %d %d" % DISCARD, "wait 0.4", "ui_mouse down", "wait 0.1", "ui_mouse up", "wait 2")
        check(name, r == ROUNDS - 1 and i == len(names) - 1)

add("ui_mouse off", "wait 1", "quit")
out[0] = out[0].replace("( 0 switches )", "( %d switches )" % count)

path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "switch_stress.cmds")
with open(path, "w", newline="\n") as f:
    f.write("\n".join(out) + "\n")

print("%s: %d switches, %d commands" % (path, count, len(out)))
