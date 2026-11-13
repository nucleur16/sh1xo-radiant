# Seventh round: a map for the colour of bounced light, made from the fifth round's room with path nodes
# ( mp_c5bots.map ): the wall at - x gets a red material, the wall at + x a blue one, a low screen stands in front of
# each, and ONE white light that is only baked hangs in the middle. The floor between a screen and its wall is in the
# light's shadow: what lights it came off the wall.
# usage: make_bounce_map.py <mp_c5bots.map> <out.map> [red material] [blue material]
import re, sys

src, out = sys.argv[1], sys.argv[2]
red = sys.argv[3] if len(sys.argv) > 3 else 'wc/abandon_floor_red'
blue = sys.argv[4] if len(sys.argv) > 4 else 'wc/abandon_floor_blue'
text = open(src).read()

def box(mins, maxs, material):
    x0, y0, z0 = mins; x1, y1, z1 = maxs
    tail = ' %s 64 64 0 0 0 0 lightmap_gray 16384 16384 0 0 0 0\n' % material
    f = lambda a, b, c: ' ( %g %g %g ) ( %g %g %g ) ( %g %g %g )' % (a + b + c) + tail
    return '{\n' + f((x1, y1, z0), (x0, y0, z0), (x1, y0, z0)) + f((x1, y0, z1), (x0, y0, z0), (x0, y0, z1)) + f((x0, y0, z1), (x0, y1, z0), (x0, y1, z1)) \
        + f((x1, y1, z1), (x1, y0, z0), (x1, y0, z1)) + f((x0, y1, z1), (x1, y1, z0), (x1, y1, z1)) + f((x1, y0, z1), (x0, y1, z1), (x1, y1, z1)) + '}\n'

# the brushes of the worldspawn: 2 is the wall at - x, 3 the wall at + x ( see the map )
def repaint(text, brush, material):
    m = re.search(r'// brush %d\n\{\n(.*?)\n\}' % brush, text, re.S)
    body = re.sub(r'\) ([A-Za-z]\S*) (-?\d+ -?\d+ )', lambda g: ') %s %s' % (material, g.group(2)), m.group(1))
    return text[:m.start(1)] + body + text[m.end(1):]

text = repaint(text, 2, red)
text = repaint(text, 3, blue)
# the screens go behind the last brush of the worldspawn
first_entity_end = text.index('\n}\n', text.rindex('// brush', 0, text.index('// entity 1'))) + 3
screens = box((-400, -320, 0), (-384, 320, 112), 'wc/me_concrete_riverwall') + box((384, -320, 0), (400, 320, 112), 'wc/me_concrete_riverwall')
text = text[:first_entity_end] + screens + text[first_entity_end:]
# no sun, no sky light, hardly any ambient light: what is not lit by the lamp is lit by what bounces
text = re.sub(r'^"(sunlight|ambient|skylight)" "[^"]*"\n', '', text, flags=re.M)
text = text.replace('"classname" "worldspawn"\n', '"classname" "worldspawn"\n"sunlight" "0"\n"ambient" "0.01"\n"skylight" "0"\n', 1)
# the lamp
a = text.index('"classname" "light"')
start, end = text.rindex('{', 0, a), text.index('}', a) + 1
text = text[:start] + '{\n"classname" "light"\n"origin" "0 0 150"\n"radius" "1400"\n"intensity" "3"\n"_color" "1 1 1"\n}' + text[end:]
open(out, 'w', newline='\n').write(text)
print('wrote %s ( %s, %s )' % (out, red, blue))
