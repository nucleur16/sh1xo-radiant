import zlib


def arr(name, b):
    out = 'const std::uint8_t %s[] = {' % name
    for i, x in enumerate(b):
        if i % 24 == 0:
            out += '\n\t\t'
        out += '0x%02X, ' % x
    return out + '\n\t};'


def text(n):
    # same generator as make_text() in the test
    out = bytearray()
    state = 12345
    words = [b'alpha', b'bravo', b'charlie', b'delta', b'echo', b'foxtrot', b'golf', b'hotel']
    while len(out) < n:
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        out += words[(state >> 16) & 7]
        out += b' ' if (state >> 8) & 3 else b'\n'
    return bytes(out[:n])


fixed = zlib.compress(b'hello hello hello hello', 9)
stored = zlib.compress(b'stored block payload', 0)
dyn_src = text(6000)
dyn = zlib.compress(dyn_src, 9)
for name, b in (('fixed', fixed), ('stored', stored), ('dyn', dyn)):
    btype = (b[2] >> 1) & 3
    print('//', name, len(b), 'bytes, first block type', btype)
print(arr('ZLIB_FIXED', fixed))
print(arr('ZLIB_STORED', stored))
print(arr('ZLIB_DYNAMIC', dyn))
print('// adler', hex(zlib.adler32(dyn_src)), len(dyn_src))
