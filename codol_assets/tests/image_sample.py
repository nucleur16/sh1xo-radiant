#!/usr/bin/env python3
"""Runs codol_asset_verify image-roundtrip over a sample of the shipped images of each use and prints the mean PSNR
per level. Needs the surveys of codol_asset_probe ( images.tsv, iwis.tsv ).

    python image_sample.py <codol_asset_verify.exe> <install> <images.tsv> <iwis.tsv> [<per use>]
"""
import collections, csv, subprocess, sys

verify, install, images_tsv, iwis_tsv = sys.argv[1:5]
per_use = int(sys.argv[5]) if len(sys.argv) > 5 else 40
csv.field_size_limit(10 ** 9)
iwi = {}
for r in csv.DictReader(open(iwis_tsv, encoding='latin1'), delimiter='\t'):
    iwi.setdefault(r['path'].split('/')[-1][:-4], r)
uses = {'2': 'color', '5': 'normal', '8': 'specular'}
names = collections.defaultdict(list)
seen = set()
for r in csv.DictReader(open(images_tsv, encoding='latin1'), delimiter='\t'):
    n = r['name']
    if r['streamed'] != '1' or n in seen or r['semantic'] not in uses or n not in iwi or r['mapType'] != '3':
        continue
    seen.add(n)
    f = iwi[n]
    if f['format'] in ('11', '12', '13') and int(f['width']) >= 256 and int(f['height']) >= 256 and not int(f['flags'], 16) & 2:
        names[uses[r['semantic']]].append(n)
for use, all_names in names.items():
    step = max(1, len(all_names) // per_use)
    sample = all_names[::step][:per_use]
    out = subprocess.run([verify, 'image-roundtrip', install, install + '/C1.exe', use] + sample, capture_output=True, text=True, errors='replace').stdout
    source, shipped = collections.defaultdict(list), collections.defaultdict(list)
    headers = equal = 0
    for line in out.splitlines():
        p = line.split()
        if 'header + level table' in line:
            headers += 1
            equal += 'equal to the shipped file' in line
        elif len(p) == 6 and p[0].isdigit() and p[2] == 'x':
            source[int(p[0])].append(float(p[4]))
            shipped[int(p[0])].append(float(p[5]))
    print('%s: %d images ( of %d ), IWI header and level table equal to the shipped file: %d' % (use, headers, len(all_names), equal))
    print('  level   images   vs source: mean  min     vs shipped: mean  min')
    for level in sorted(source)[:6]:
        a, b = source[level], shipped[level]
        print('  %5d   %6d   %14.2f %6.2f   %14.2f %6.2f' % (level, len(a), sum(a) / len(a), min(a), sum(b) / len(b), min(b)))
