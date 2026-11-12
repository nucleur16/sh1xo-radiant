# Seventh round: maps for the omni light's shadows, made from the fifth round's room with path nodes ( mp_c5bots.map ).
# Every map has ONE primary light above the middle of the room, 215 units up; what differs is the light:
#   mp_c7omni.map    primary omni ( spawnflags 1 ), the compiler's default cone
#   mp_c7spotd.map   primary SPOT straight down with the cone an omni light gets ( 120 / 60 degrees ): the control
#   mp_c7omnin.map   primary omni with a cone of its own ( fov_outer 90, fov_inner 60 ), as shipped omni lights have
# usage: make_omni_maps.py <mp_c5bots.map> <out dir> [intensity] [radius]
import re, sys

def entities(text):
    out, depth, start = [], 0, 0
    for i, c in enumerate(text):
        if c == '{':
            if depth == 0: start = i
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: out.append((start, i + 1))
    return out

def keys(block):
    return dict(re.findall(r'^"([^"]+)" "([^"]*)"\s*$', block, re.M))

def rewrite(text, light):
    out, last, n = [], 0, 0
    for a, b in entities(text):
        block = text[a:b]
        if keys(block).get('classname') == 'light':
            block = '{\n' + ''.join('"%s" "%s"\n' % kv for kv in light.items()) + '}'; n += 1
        out.append(text[last:a]); out.append(block); last = b
    out.append(text[last:])
    assert n == 1, n
    return ''.join(out)

src, out = sys.argv[1], sys.argv[2]
intensity = sys.argv[3] if len(sys.argv) > 3 else '1.2'
radius = sys.argv[4] if len(sys.argv) > 4 else '700'
base = {'classname': 'light', 'origin': '0 0 215', 'radius': radius, 'intensity': intensity, '_color': '1 0.95 0.85'}
text = open(src).read()
maps = {
    'mp_c7omni': dict(base, spawnflags='1'),
    'mp_c7spotd': dict(base, spawnflags='2', angles='90 0 0', fov_outer='120', fov_inner='60'),
    'mp_c7omnin': dict(base, spawnflags='1', fov_outer='90', fov_inner='60'),
}
for name, light in maps.items():
    open('%s/%s.map' % (out, name), 'w', newline='\n').write(rewrite(text, light))
print('wrote ' + ', '.join(maps))
