# rects.py <a.png> <b.png> <name=x0,y0,x1,y1> ..: the mean brightness ( 0 .. 255 ) of rectangles of two pictures taken
# from the same place, and b / a - how dark a shadow is that only picture b has. Coordinates are those of the
# pictures given ( the 1600 wide _s.png ).
import sys, zlib, struct

def load(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    i, idat, w, h, depth, ctype = 8, b'', 0, 0, 0, 0
    while i < len(d):
        n, kind = struct.unpack('>I4s', d[i:i + 8])
        body = d[i + 8:i + 8 + n]
        if kind == b'IHDR': w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
        elif kind == b'IDAT': idat += body
        i += 12 + n
    bpp = {2: 3, 6: 4}[ctype]
    raw = zlib.decompress(idat)
    rows, prev, stride = [], bytearray(w * bpp), w * bpp
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line); prev = line
    return w, h, bpp, rows

def mean(img, r):
    w, h, bpp, rows = img
    x0, y0, x1, y1 = r
    s = n = 0
    for y in range(max(y0, 0), min(y1, h)):
        row = rows[y]
        for x in range(max(x0, 0), min(x1, w)):
            s += row[x * bpp] + row[x * bpp + 1] + row[x * bpp + 2]; n += 3
    return s / max(n, 1)

a, b = load(sys.argv[1]), load(sys.argv[2])
for spec in sys.argv[3:]:
    name, r = spec.split('=')
    r = tuple(int(v) for v in r.split(','))
    ma, mb = mean(a, r), mean(b, r)
    print('%-10s %6.1f -> %6.1f   ratio %.2f' % (name, ma, mb, mb / max(ma, 1e-6)))
