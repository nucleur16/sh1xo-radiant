# Sixth round light maps, made from the fifth round's ( the box room with two spots / with path nodes ):
#   mp_c6lamp.map   two primary spots over one floor: the first SCRIPTABLE ( spawnflags 6, targetname c6lamp, maxmove 300,
#                   maxturn 45 ) for the level script to dim, colour and move; the second with another falloff
#                   definition ( argv[3], default tungsten_lamp: a coloured falloff picture )
#   mp_c6omni.map   the room with path nodes and ONE primary omni light ( spawnflags 1 ) above the middle: bots under it
# usage: make_light_maps.py <mp_c5two.map> <mp_c5bots.map> <out dir> [<definition>]
import re, sys

def entities(text):
    # ( start, end ) of every top level { .. } block; brushes nest one level
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

def light_block(k):
    return '{\n' + ''.join('"%s" "%s"\n' % kv for kv in k.items()) + '}'

def rewrite(text, change):
    out, last, n = [], 0, 0
    for a, b in entities(text):
        block = text[a:b]
        k = keys(block)
        if k.get('classname') == 'light':
            block = light_block(change(n, k)); n += 1
        out.append(text[last:a]); out.append(block); last = b
    out.append(text[last:])
    return ''.join(out), n

two, bots, out = sys.argv[1], sys.argv[2], sys.argv[3]
definition = sys.argv[4] if len(sys.argv) > 4 else 'tungsten_lamp'

def lamp(n, k):
    if n == 0:
        k.update({'spawnflags': '6', 'targetname': 'c6lamp', 'maxmove': '300', 'maxturn': '45', 'intensity': '1.5'})
        k.pop('def', None)
    else:
        k.update({'spawnflags': '2', 'def': definition, '_color': '1 1 1', 'intensity': '1.5'})
        for gone in ('targetname', 'maxturn', 'maxmove'): k.pop(gone, None)
    return k

text, n = rewrite(open(two).read(), lamp)
assert n == 2, n
open(out + '/mp_c6lamp.map', 'w', newline='\n').write(text)

def omni(n, k):
    return {'classname': 'light', 'origin': '0 0 215', 'spawnflags': '1', 'radius': '900', 'intensity': '2.2', '_color': '1 0.95 0.85'}

text, n = rewrite(open(bots).read(), omni)
assert n == 1, n
open(out + '/mp_c6omni.map', 'w', newline='\n').write(text)
print('wrote mp_c6lamp.map ( second light: %s ), mp_c6omni.map' % definition)
