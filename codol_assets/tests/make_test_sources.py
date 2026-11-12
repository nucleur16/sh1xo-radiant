#!/usr/bin/env python3
"""Makes the source assets the end to end test compiles: a crate texture set ( colour, normal, specular PNG ), a
TGA and a DDS of the colour map ( the three image readers ), a crate as xmodel_export with two LODs, material
definitions as .mtl and as a CoD4 style .gdt.

    python make_test_sources.py <out dir>
"""
import math, os, struct, sys, zlib


def png(path, w, h, pixel):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(pixel(x, y))
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b''))


def height(x, y, n):
    """planks with gaps, a frame, and a raised diagonal brace"""
    u, v = x / n, y / n
    frame = 1.0 if min(u, v, 1 - u, 1 - v) < 0.09 else 0.0
    plank = 0.0 if (u * 6) % 1.0 < 0.06 else 0.6
    brace = 1.0 if abs(u - v) < 0.06 else 0.0
    return max(frame, brace, plank)


def colour(x, y, n=256):
    hgt = height(x, y, n)
    grain = 0.5 + 0.5 * math.sin(y * 0.9 + 6 * math.sin(x * 0.05))
    base = (150 + 40 * grain, 105 + 30 * grain, 60 + 20 * grain)
    k = 0.45 + 0.55 * hgt
    r, g, b = (int(c * k) for c in base)
    # a red arrow pointing to +u in the upper left quarter, so that orientation and mirroring show
    u, v = x / n, y / n
    if 0.15 < u < 0.45 and abs(v - 0.25) < 0.03 or (0.38 < u < 0.45 and abs(v - 0.25) < (0.45 - u) * 1.2 + 0.03):
        r, g, b = 200, 30, 30
    return (r, g, b, 255)


def normal(x, y, n=256):
    dx = height(x + 1, y, n) - height(x - 1, y, n)
    dy = height(x, y + 1, n) - height(x, y - 1, n)
    nx, ny, nz = -dx * 2.0, -dy * 2.0, 1.0
    l = math.sqrt(nx * nx + ny * ny + nz * nz)
    return (int((nx / l + 1) * 127.5), int((ny / l + 1) * 127.5), int((nz / l + 1) * 127.5), 255)


def specular(x, y, n=256):
    hgt = height(x, y, n)
    s = int(40 + 60 * hgt)
    return (s, s, s, int(90 + 100 * hgt))


def tga(path, w, h, pixel):
    with open(path, 'wb') as f:
        f.write(struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, 0x28))
        for y in range(h):
            for x in range(w):
                r, g, b, a = pixel(x, y)
                f.write(bytes((b, g, r, a)))


def dds(path, w, h, pixel):
    header = struct.pack('<4sIIIIIII44x', b'DDS ', 124, 0x100F, h, w, w * 4, 0, 1)
    header += struct.pack('<IIIIIIII', 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    header += struct.pack('<IIIII', 0x1000, 0, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(header)
        for y in range(h):
            for x in range(w):
                r, g, b, a = pixel(x, y)
                f.write(bytes((b, g, r, a)))


def crate(path, size, material, subdivide):
    """a box standing on z = 0, one quad ( or subdivide x subdivide quads ) per side, outward normals"""
    s = size / 2.0
    sides = [  # normal, u axis, v axis
        ((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, -1, 0), (0, 0, 1)), ((0, 1, 0), (-1, 0, 0), (0, 0, 1)),
        ((0, -1, 0), (1, 0, 0), (0, 0, 1)), ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (1, 0, 0), (0, -1, 0))]
    verts, faces = [], []
    for n, ua, va in sides:
        for j in range(subdivide):
            for i in range(subdivide):
                quad = []
                for du, dv in ((0, 0), (1, 0), (1, 1), (0, 1)):
                    u, v = (i + du) / subdivide, (j + dv) / subdivide
                    p = tuple(n[k] * s + ua[k] * (u * 2 - 1) * s + va[k] * (v * 2 - 1) * s for k in range(3))
                    verts.append((p[0], p[1], p[2] + s))
                    quad.append((len(verts) - 1, n, (u, 1 - v)))
                # counter clockwise seen from outside
                faces.append((quad[0], quad[1], quad[2]))
                faces.append((quad[0], quad[2], quad[3]))
    with open(path, 'w') as f:
        f.write('// codol_assets test crate\nMODEL\nVERSION 6\n\nNUMBONES 1\nBONE 0 -1 "tag_origin"\n\nBONE 0\nOFFSET 0.000000 0.000000 0.000000\nSCALE 1.000000 1.000000 1.000000\n')
        f.write('X 1.000000 0.000000 0.000000\nY 0.000000 1.000000 0.000000\nZ 0.000000 0.000000 1.000000\n\nNUMVERTS %d\n' % len(verts))
        for k, v in enumerate(verts):
            f.write('VERT %d\nOFFSET %.6f %.6f %.6f\nBONES 1\nBONE 0 1.000000\n\n' % (k, v[0], v[1], v[2]))
        f.write('NUMFACES %d\n' % len(faces))
        for face in faces:
            f.write('TRI 0 0 0 0\n')
            for index, n, uv in face:
                f.write('VERT %d\nNORMAL %.6f %.6f %.6f\nCOLOR 1.000000 1.000000 1.000000 1.000000\nUV 1 %.6f %.6f\n\n' % (index, n[0], n[1], n[2], uv[0], uv[1]))
        f.write('NUMOBJECTS 1\nOBJECT 0 "crate"\n\nNUMMATERIALS 1\nMATERIAL 0 "%s" "Lambert" ""\n' % material)


def main():
    out = sys.argv[1]
    os.makedirs(os.path.join(out, 'texture_assets'), exist_ok=True)
    png(os.path.join(out, 'codol_test_crate_col.png'), 256, 256, colour)
    png(os.path.join(out, 'codol_test_crate_nml.png'), 256, 256, normal)
    png(os.path.join(out, 'codol_test_crate_spc.png'), 256, 256, specular)
    tga(os.path.join(out, 'texture_assets', 'codol_test_gdt_col.tga'), 128, 128, lambda x, y: colour(x, y, 128))
    dds(os.path.join(out, 'codol_test_dds_col.dds'), 64, 64, lambda x, y: colour(x, y, 64))
    crate(os.path.join(out, 'codol_test_crate_lod0.xmodel_export'), 48.0, 'mc/codol_test_crate', 2)
    crate(os.path.join(out, 'codol_test_crate_lod1.xmodel_export'), 48.0, 'mc/codol_test_crate', 1)
    with open(os.path.join(out, 'codol_test.mtl'), 'w') as f:
        f.write('''// materials of the codol_assets end to end test
material "mc/codol_test_crate"
{
	type "model phong"
	colorMap codol_test_crate_col
	normalMap codol_test_crate_nml
	specularMap codol_test_crate_spc
	surfaceType wood
}

material "wc/codol_test_wall"
{
	type "world phong"
	colorMap codol_test_crate_col
	normalMap codol_test_crate_nml
	specularMap codol_test_crate_spc
	surfaceType wood
}

material "wc/codol_test_unlit"
{
	type "world unlit"
	colorMap codol_test_dds_col
}
''')
    with open(os.path.join(out, 'codol_test.gdt'), 'w') as f:
        f.write('''{
	"wc/codol_test_gdt" ( "material.gdf" )
	{
		"materialType" "world phong"
		"colorMap" "texture_assets\\\\codol_test_gdt_col.tga"
		"blendFunc" "Replace*"
		"alphaTest" "Always*"
		"cullFace" "Back*"
		"surfaceType" "wood"
		"tileColor" "tile both*"
		"filterColor" "mip standard (2x bilinear)*"
		"sort" "<default>*"
	}
}
''')


if __name__ == '__main__':
    main()
