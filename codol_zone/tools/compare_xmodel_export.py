#!/usr/bin/env python3
"""Compare the decoded geometry of one xmodel ( codol_zonetool model <ff> <name> --json --geometry ) with the
.xmodel_export CODOL-AssetExplorer writes for the same model ( CodolSoundDumper export-models --format xmodel_export --lod0 ).

    python compare_xmodel_export.py <model.json> <model.xmodel_export>

Checks bone names, parents and positions, vertex count, positions and bone weights, triangle count, and per
triangle corner the vertex index, normal, colour and uv.
"""
import json
import sys


def parse_export(path):
    bones, verts, tris = [], [], []
    bone_offsets = {}
    cur = None
    with open(path, encoding='utf-8', errors='replace') as f:
        lines = [l.strip() for l in f]
    i = 0
    section = 'bones'
    corner = None
    current_bone = None
    while i < len(lines):
        t = lines[i].split()
        i += 1
        if not t:
            continue
        if t[0] == 'BONE' and section == 'bones' and len(t) >= 4:
            bones.append((int(t[2]), lines[i - 1].split('"')[1]))
        elif t[0] == 'BONE' and section == 'bones' and len(t) == 2:
            current_bone = int(t[1])
        elif t[0] == 'OFFSET' and section == 'bones':
            bone_offsets[current_bone] = tuple(float(x) for x in t[1:4])
        elif t[0] == 'NUMVERTS':
            section = 'verts'
        elif t[0] == 'NUMFACES':
            section = 'faces'
        elif t[0] == 'NUMOBJECTS':
            section = 'objects'
        elif section == 'verts' and t[0] == 'VERT':
            cur = {'bones': []}
            verts.append(cur)
        elif section == 'verts' and t[0] == 'OFFSET':
            cur['pos'] = tuple(float(x) for x in t[1:4])
        elif section == 'verts' and t[0] == 'BONE':
            cur['bones'].append((int(t[1]), float(t[2])))
        elif section == 'faces' and t[0] == 'TRI':
            tris.append({'object': int(t[1]), 'corners': []})
        elif section == 'faces' and t[0] == 'VERT':
            corner = {'index': int(t[1])}
            tris[-1]['corners'].append(corner)
        elif section == 'faces' and t[0] == 'NORMAL':
            corner['normal'] = tuple(float(x) for x in t[1:4])
        elif section == 'faces' and t[0] == 'COLOR':
            corner['color'] = tuple(float(x) for x in t[1:5])
        elif section == 'faces' and t[0] == 'UV':
            corner['uv'] = tuple(float(x) for x in t[2:4])
    return bones, bone_offsets, verts, tris


def close(a, b, eps):
    return all(abs(x - y) <= eps for x, y in zip(a, b))


def main():
    mine = json.load(open(sys.argv[1], encoding='utf-8'))
    bones, bone_offsets, verts, tris = parse_export(sys.argv[2])
    ok = True

    def check(name, cond, detail=''):
        nonlocal ok
        ok = ok and cond
        print('%s %s %s' % ('PASS' if cond else 'FAIL', name, detail))

    my_bones = mine['boneList']
    check('bone count', len(my_bones) == len(bones), '%d / %d' % (len(my_bones), len(bones)))
    named = [(b['parent'], b['name'] or 'bone_%d' % i) for i, b in enumerate(my_bones)]
    check('bone names and parents', named == bones)
    check('bone positions', all(close(b['trans'], bone_offsets.get(i, (9e9,) * 3), 2e-4) for i, b in enumerate(my_bones)))

    surfaces = mine['lods'][0]['surfaces']
    pos, nrm, uv, col, bone, idx = [], [], [], [], [], []
    base = 0
    for s in surfaces:
        n = s['vertices']
        pos += [tuple(s['positions'][3 * k:3 * k + 3]) for k in range(n)]
        nrm += [tuple(s['normals'][3 * k:3 * k + 3]) for k in range(n)]
        uv += [tuple(s['uvs'][2 * k:2 * k + 2]) for k in range(n)]
        col += [tuple(c / 255.0 for c in s['colors'][4 * k:4 * k + 4]) for k in range(n)]
        bone += [sorted((w[2 * k], w[2 * k + 1]) for k in range(len(w) // 2) if w[2 * k + 1] > 0) for w in s['weights']]
        idx += [base + k for k in s['indices']]
        base += n
    check('vertex count', len(pos) == len(verts), '%d / %d' % (len(pos), len(verts)))
    check('triangle count', len(idx) // 3 == len(tris), '%d / %d' % (len(idx) // 3, len(tris)))

    # bone influences: same bones, weights equal to the export's six decimals
    def same_weights(mine_w, theirs):
        theirs = sorted(theirs)
        return len(mine_w) == len(theirs) and all(a[0] == b[0] and abs(a[1] - b[1]) < 2e-6 for a, b in zip(mine_w, theirs))

    check('vertex bone weights', all(same_weights(b, v['bones']) for b, v in zip(bone, verts)),
          '( up to %d influences )' % max(len(v['bones']) for v in verts))
    # vertices are stored in model space ( bind pose ), also those of rigid and skinned surfaces
    check('vertex positions', all(close(a, v['pos'], 1e-4) for a, v in zip(pos, verts)))

    # the export may flip the winding: accept the corners in either order, but the same order for every triangle
    orders = []
    for order in ((0, 1, 2), (0, 2, 1), (2, 1, 0), (1, 0, 2)):
        if all([c['index'] for c in t['corners']] == [idx[3 * k + o] for o in order] for k, t in enumerate(tris)):
            orders.append(order)
    check('triangle indices', bool(orders), 'corner order %s' % (orders[0],) if orders else '')
    corners = [c for t in tris for c in t['corners']]
    check('corner normals', all(close(c['normal'], nrm[c['index']], 2e-5) for c in corners))
    check('corner uvs', all(close(c['uv'], uv[c['index']], 2e-6) for c in corners))
    check('corner colours', all(close(c['color'], col[c['index']], 2e-6) for c in corners))
    print('%d vertices, %d triangles, %d bones' % (len(pos), len(tris), len(bones)))
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
