#!/usr/bin/env python3
"""Source assets of a modder's material project for the codol_bsp tests: PNGs made from nothing and the material
definitions that use them - a lit wall with normal and specular map, an alpha tested fence, a blended decal, a
minimap picture and a loading screen picture.

    python make_modder_assets.py <out dir>

    codol_assetc --install <game> --out <out dir>/bsp_test_assets.ff --image .. --material <out dir>/bsp_test.mtl
    ( the command line is printed )
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


N = 256


def brick_height(x, y):
    """bricks of 64 x 32 with mortar lines, every second row shifted"""
    row = (y % N) // 32
    xx = (x + (32 if row & 1 else 0)) % 64
    yy = y % 32
    return 0.0 if xx < 4 or yy < 4 else 1.0


def wall_colour(x, y):
    h = brick_height(x, y)
    row, col = (y % N) // 32, ((x + (32 if ((y % N) // 32) & 1 else 0)) % N) // 64
    tone = 0.85 + 0.15 * math.sin(row * 12.9898 + col * 78.233)
    if h:
        r, g, b = 150 * tone, 62 * tone, 48 * tone
    else:
        r, g, b = 150, 146, 138
    # a yellow stripe along the top so that the orientation shows
    if y % N < 6:
        r, g, b = 220, 200, 40
    return (int(r), int(g), int(b), 255)


def wall_normal(x, y):
    dx = brick_height(x + 2, y) - brick_height(x - 2, y)
    dy = brick_height(x, y + 2) - brick_height(x, y - 2)
    nx, ny, nz = -dx * 1.5, -dy * 1.5, 1.0
    l = math.sqrt(nx * nx + ny * ny + nz * nz)
    return (int((nx / l + 1) * 127.5), int((ny / l + 1) * 127.5), int((nz / l + 1) * 127.5), 255)


def wall_specular(x, y):
    s = int(30 + 50 * brick_height(x, y))
    return (s, s, s, int(60 + 80 * brick_height(x, y)))


def fence_colour(x, y):
    """a chain link pattern: diagonal wires, everything else transparent; a solid frame"""
    u, v = x % 32, y % 32
    wire = abs((u + v) % 32 - 16) < 2.5 or abs((u - v) % 32 - 16) < 2.5
    frame = x < 6 or x >= N - 6 or y < 6 or y >= N - 6
    if frame:
        return (70, 74, 80, 255)
    if wire:
        return (150, 156, 160, 255)
    return (150, 156, 160, 0)


def decal_colour(x, y):
    """a painted white arrow with a soft edge, transparent around it"""
    u, v = x / N - 0.5, y / N - 0.5
    shaft = abs(v) < 0.08 and -0.35 < u < 0.1
    head = 0.1 <= u < 0.4 and abs(v) < (0.4 - u) * 0.8
    ring = abs(math.hypot(u, v) - 0.46) < 0.02
    a = 230 if shaft or head or ring else 0
    return (235, 235, 225, a)


def minimap_colour(x, y):
    """stands in for a top down picture: a grid with a north mark; the real one is rendered from the map"""
    g = 60 + (40 if x % 32 < 2 or y % 32 < 2 else 0)
    return (g, g + 10, g, 255)


def loadscreen_colour(x, y):
    u, v = x / 512.0, y / 512.0
    r = int(30 + 120 * u)
    g = int(40 + 80 * v)
    b = int(90 + 100 * (1 - u))
    if abs(v - 0.5) < 0.012 or abs(u - 0.5) < 0.012:
        r, g, b = 240, 240, 240
    return (r, g, b, 255)


MTL = '''// materials of the codol_bsp modder material test
material "wc/bsp_test_wall"
{
	type "world phong"
	colorMap bsp_test_wall_col
	normalMap bsp_test_wall_nml
	specularMap bsp_test_wall_spc
	surfaceType brick
}

material "wc/bsp_test_fence"
{
	type "world phong"
	colorMap bsp_test_fence_col
	alphaTest ge128
	cullFace none
	surfaceType metal
}

material "wc/bsp_test_decal"
{
	type "world phong"
	colorMap bsp_test_decal_col
	blendFunc blend			// sort 9 ( static decal ) and the state the shipped wc_l_sm_b0c0 materials have follow from this
	surfaceType concrete
}
'''


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    png(os.path.join(out, 'bsp_test_wall_col.png'), N, N, wall_colour)
    png(os.path.join(out, 'bsp_test_wall_nml.png'), N, N, wall_normal)
    png(os.path.join(out, 'bsp_test_wall_spc.png'), N, N, wall_specular)
    png(os.path.join(out, 'bsp_test_fence_col.png'), N, N, fence_colour)
    png(os.path.join(out, 'bsp_test_decal_col.png'), N, N, decal_colour)
    png(os.path.join(out, 'bsp_test_minimap.png'), 256, 256, minimap_colour)
    png(os.path.join(out, 'bsp_test_loadscreen.png'), 512, 512, loadscreen_colour)
    with open(os.path.join(out, 'bsp_test.mtl'), 'w') as f:
        f.write(MTL)
    images = ' '.join('--image %s=%s' % (n, os.path.join(out, n + '.png')) for n in
                      ('bsp_test_wall_col', 'bsp_test_wall_nml', 'bsp_test_wall_spc', 'bsp_test_fence_col', 'bsp_test_decal_col'))
    print('codol_assetc --install <game> --out %s %s --material %s' % (os.path.join(out, 'bsp_test_assets.ff'), images, os.path.join(out, 'bsp_test.mtl')))


if __name__ == '__main__':
    main()
