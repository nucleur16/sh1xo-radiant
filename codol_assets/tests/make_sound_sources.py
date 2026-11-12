#!/usr/bin/env python3
"""Makes the sound sources the end to end test compiles, laid out like raw/ of the mod tools:

    <out>/soundaliases/codol_test_sounds.csv     four aliases ( header row, mod tools columns )
    <out>/soundaliases/codol_test_near.vfcurve   a volume falloff curve
    <out>/soundaliases/codol_test_front.spkrmap  a speaker map
    <out>/sound/codol_test/beep.wav              16 bit PCM, mono, 22050 Hz: two short tones
    <out>/sound/codol_test/hum.wav               16 bit PCM, stereo, 44100 Hz, one second, loops without a click
                                                 ( the alias asks for ADPCM )
    <out>/sound/codol_test/click8.wav            8 bit PCM, mono, 11025 Hz ( stored as 16 bit )
    <out>/sound/codol_test/wind.wav              16 bit PCM, stereo, 22050 Hz: a streamed file ( goes into the .iwd )

    python make_sound_sources.py <out dir>
"""
import math, os, struct, sys


def wav(path, rate, channels, bits, frames):
    """frames: list of tuples of floats -1 .. 1"""
    data = bytearray()
    for frame in frames:
        for v in frame:
            v = max(-1.0, min(1.0, v))
            if bits == 8:
                data.append(int(round(v * 127)) + 128)
            else:
                data += struct.pack('<h', int(round(v * 32767)))
    align = channels * bits // 8
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, channels, rate, rate * align, align, bits)
                + b'data' + struct.pack('<I', len(data)) + bytes(data))


def text(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', newline='\n') as f:
        f.write(content)


def main():
    out = sys.argv[1]
    snd = os.path.join(out, 'sound', 'codol_test')
    alias = os.path.join(out, 'soundaliases')

    rate = 22050
    beep = []
    for n in range(int(rate * 0.45)):
        t = n / rate
        tone = 880.0 if t < 0.2 else 1320.0
        gap = 0.0 if 0.2 <= t < 0.25 else 1.0
        env = min(1.0, t * 200) * min(1.0, (0.45 - t) * 200)
        beep.append((0.6 * gap * env * math.sin(2 * math.pi * tone * t),))
    wav(os.path.join(snd, 'beep.wav'), rate, 1, 16, beep)

    rate = 44100
    hum = []
    for n in range(rate):
        t = n / rate
        left = 0.35 * math.sin(2 * math.pi * 110 * t) + 0.15 * math.sin(2 * math.pi * 220 * t)
        right = 0.35 * math.sin(2 * math.pi * 112 * t) + 0.15 * math.sin(2 * math.pi * 330 * t)
        hum.append((left, right))
    wav(os.path.join(snd, 'hum.wav'), rate, 2, 16, hum)

    rate = 11025
    wav(os.path.join(snd, 'click8.wav'), rate, 1, 8, [(0.8 * math.exp(-n / 120.0) * math.sin(2 * math.pi * 1800 * n / rate),) for n in range(rate // 5)])

    rate = 22050
    seed = 12345
    wind, low = [], [0.0, 0.0]
    for n in range(rate * 2):
        frame = []
        for c in range(2):
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
            noise = seed / 0x40000000 - 1.0
            low[c] += 0.02 * (noise - low[c])
            frame.append(3.0 * low[c] * (0.6 + 0.4 * math.sin(2 * math.pi * n / (rate * 2) + c)))
        wind.append(tuple(frame))
    wav(os.path.join(snd, 'wind.wav'), rate, 2, 16, wind)

    text(os.path.join(alias, 'codol_test_near.vfcurve'), 'SNDCURVE\n\n4\n0 1\n0.2 0.6\n0.6 0.15\n1 0\n')
    text(os.path.join(alias, 'codol_test_front.spkrmap'),
         'SPKRMAP\n// <speaker> <level of source channel 0> [<level of source channel 1>]\n'
         'map mono 2\n0 0.5\n1 0.5\n'
         'map mono 6\n0 0.4\n1 0.4\n2 0.2\n3 0\n4 0\n5 0\n'
         'map stereo 2\n0 1 0\n1 0 1\n'
         'map stereo 6\n0 0.8 0\n1 0 0.8\n2 0.2 0.2\n3 0 0\n4 0 0\n5 0 0\n')
    text(os.path.join(alias, 'codol_test_sounds.csv'),
         '# sound aliases of the codol_assets end to end test\n'
         'name,sequence,file,vol_min,vol_max,vol_mod,pitch_min,pitch_max,dist_min,dist_max,channel,type,probability,loop,masterslave,loadspec,subtitle,compression,'
         'secondaryaliasname,volumefalloffcurve,startdelay,speakermap,reverb,lfe percentage,center percentage,platform,envelop_min,envelop_max,envelop percentage\n'
         'codol_test_beep,,codol_test\\beep.wav,0.9,1,,0.95,1.05,60,1500,auto,loaded,,,,,,,,codol_test_near,,,,,,,,,\n'
         'codol_test_beep,,codol_test/click8.wav,0.8,,,,,60,1500,auto,loaded,,,,,,,,codol_test_near,,,,,,,,,\n'
         'codol_test_hum,,codol_test/hum.wav,0.7,,,,,100,900,auto,loaded,,looping,,,,adpcm,,,,,,,,,,,\n'
         'codol_test_wind,,codol_test/wind.wav,0.5,,,,,,,ambient,streamed,,looping,,,,,,,,codol_test_front,,,,,,,\n'
         'codol_test_shipped,,null.wav,1,,,,,,,auto2d,loaded,,,,,,,,,,,,,,,,,\n')
    print('wrote', out)


if __name__ == '__main__':
    main()
