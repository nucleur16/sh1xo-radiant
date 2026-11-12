#!/usr/bin/env python3
"""Makes the animation sources the end to end test compiles, as an exporter writes them ( model space ):

    <out>/xanim_export/codol_test_spin.xanim_export     61 frames at 30 / s: tag_origin turns once about z and bobs,
                                                        the note "codol_half" at frame 30. For any one bone model.
    <out>/xanim_export/codol_test_lid.xanim_export      41 frames: a box whose lid ( j_lid, hinged 24 above the
                                                        origin ) opens by 100 degrees; notes "lid_start", "lid_open"
    <out>/xmodel_export/codol_test_lidbox.xmodel_export the skeleton the lid animation is for ( two bones and one
                                                        triangle each: enough for the animation compiler, and a
                                                        model that codol_assetc --xmodel takes )
    <out>/animtrees/codol_test.atr                      the animtree a script names: #using_animtree( "codol_test" )
    <out>/animlists/codol_test.animlist                 the map's animlist ( rawfile animlists/<map>.animlist of the map
                                                        zone ): "=codol_test" and the animations the map adds to that tree

    python make_anim_sources.py <out dir>
"""
import math, os, sys


def text(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', newline='\n') as f:
        f.write(content)


def about_z(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [(c, s, 0.0), (-s, c, 0.0), (0.0, 0.0, 1.0)]


def about_y(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    return [(c, 0.0, -s), (0.0, 1.0, 0.0), (s, 0.0, c)]


def anim(parts, framerate, frames, notes):
    """frames: list of lists of ( offset, axis rows ) per part"""
    out = ['// codol_assets test source', 'ANIMATION', 'VERSION 3', '', 'NUMPARTS %d' % len(parts)]
    out += ['PART %d "%s"' % (k, name) for k, name in enumerate(parts)]
    out += ['', 'FRAMERATE %d' % framerate, 'NUMFRAMES %d' % len(frames), '']
    for f, poses in enumerate(frames):
        out.append('FRAME %d' % f)
        for k, (offset, axis) in enumerate(poses):
            out.append('PART %d' % k)
            out.append('OFFSET %.6f %.6f %.6f' % offset)
            out.append('SCALE 1.000000 1.000000 1.000000')
            for label, row in zip('XYZ', axis):
                out.append('%s %.6f %.6f %.6f' % ((label,) + row))
            out.append('')
    out += ['NOTETRACKS', '']
    for k in range(len(parts)):
        out.append('PART %d' % k)
        if k == 0 and notes:
            out += ['NUMTRACKS 1', 'NOTETRACK 0', 'NUMKEYS %d' % len(notes)]
            out += ['FRAME %d "%s"' % (frame, name) for frame, name in notes]
        else:
            out.append('NUMTRACKS 0')
        out.append('')
    return '\n'.join(out)


def main():
    out = sys.argv[1]
    identity = about_z(0)

    frames = []
    for f in range(61):
        frames.append([((0.0, 0.0, 8.0 * math.sin(math.pi * f / 60.0)), about_z(6.0 * f))])
    text(os.path.join(out, 'xanim_export', 'codol_test_spin.xanim_export'), anim(['tag_origin'], 30, frames, [(30, 'codol_half')]))

    # the lid: hinge along y at ( -16, 0, 24 ); it opens with an ease in / out
    frames = []
    for f in range(41):
        t = f / 40.0
        angle = 100.0 * (3 * t * t - 2 * t * t * t)
        frames.append([((0.0, 0.0, 0.0), identity), ((-16.0, 0.0, 24.0), about_y(-angle))])
    text(os.path.join(out, 'xanim_export', 'codol_test_lid.xanim_export'), anim(['tag_origin', 'j_lid'], 30, frames, [(0, 'lid_start'), (40, 'lid_open')]))

    model = ['// codol_assets test source: the skeleton of codol_test_lid', 'MODEL', 'VERSION 6', '', 'NUMBONES 2', 'BONE 0 -1 "tag_origin"', 'BONE 1 0 "j_lid"', '']
    for k, offset in enumerate([(0.0, 0.0, 0.0), (-16.0, 0.0, 24.0)]):
        model += ['BONE %d' % k, 'OFFSET %.6f %.6f %.6f' % offset, 'SCALE 1.000000 1.000000 1.000000', 'X 1.000000 0.000000 0.000000', 'Y 0.000000 1.000000 0.000000',
                  'Z 0.000000 0.000000 1.000000', '']
    verts = [(-16, -16, 0, 0), (16, -16, 0, 0), (0, 16, 0, 0), (-16, -16, 24, 1), (16, -16, 24, 1), (0, 16, 24, 1)]
    model.append('NUMVERTS %d' % len(verts))
    for k, (x, y, zed, bone) in enumerate(verts):
        model += ['VERT %d' % k, 'OFFSET %.6f %.6f %.6f' % (x, y, zed), 'BONES 1', 'BONE %d 1.000000' % bone, '']
    model.append('NUMFACES 2')
    for tri in ((0, 2, 1), (3, 5, 4)):
        model.append('TRI 0 0 0 0')
        for v in tri:
            model += ['VERT %d' % v, 'NORMAL 0.000000 0.000000 1.000000', 'COLOR 1.000000 1.000000 1.000000 1.000000',
                      'UV 1 %.6f %.6f' % ((verts[v][0] + 16) / 32.0, (verts[v][1] + 16) / 32.0)]
        model.append('')
    model += ['NUMOBJECTS 1', 'OBJECT 0 "codol_test_lidbox"', '', 'NUMMATERIALS 1', 'MATERIAL 0 "mc/codol_test_crate" "Phong" "codol_test_crate_col.png"',
              'COLOR 0.000000 0.000000 0.000000 1.000000', 'TRANSPARENCY 0.000000 0.000000 0.000000 1.000000', 'AMBIENTCOLOR 0.000000 0.000000 0.000000 1.000000',
              'INCANDESCENCE 0.000000 0.000000 0.000000 1.000000', 'COEFFS 0.800000 0.000000', 'GLOW 0.000000 0', 'REFRACTIVE 6 1.000000',
              'SPECULARCOLOR -1.000000 -1.000000 -1.000000 1.000000', 'REFLECTIVECOLOR -1.000000 -1.000000 -1.000000 1.000000', 'REFLECTIVE -1 -1.000000',
              'BLINN -1.000000 -1.000000', 'PHONG -1.000000', '']
    text(os.path.join(out, 'xmodel_export', 'codol_test_lidbox.xmodel_export'), '\n'.join(model))

    text(os.path.join(out, 'animtrees', 'codol_test.atr'), 'codol_test_spin\ncodol_test_lid\n')
    # what goes into the map zone as the rawfile animlists/<map>.animlist: the tree and the animations the map adds to it
    text(os.path.join(out, 'animlists', 'codol_test.animlist'), '=codol_test\ncodol_test_spin\ncodol_test_lid\n')
    print('wrote the animation sources to', out)


if __name__ == '__main__':
    main()
