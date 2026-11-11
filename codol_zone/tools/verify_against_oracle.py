#!/usr/bin/env python3
"""Compare codol_zonetool with CODOL-AssetExplorer's CodolSoundDumper ( the "oracle" ) on a real 1.1.1.13 install.

    python verify_against_oracle.py --tool <codol_zonetool.exe> --oracle <CodolSoundDumper.exe>
                                    --game <install folder> --work <scratch folder>
                                    [--zone-docs <CoD4x_Server/docs/zone_assets>] [--steps a,b,c,d,e,f]

Nothing is written outside --work ( the oracle's index cache is redirected there with CODOL_CACHE_DIR ).

steps
  a  container : inflated zone byte-identical to `oracle decrypt` for a sample ( largest, smallest, patch, unsigned )
  b  traversal : `check` on every folder; XAsset table ( type, zone offset, size ) identical to `oracle assets` for
                 every zone; asset names against <zone-docs>/<zone>.txt
  c  rawfiles  : every rawfile of a few zones identical to `oracle zone-rawfiles`
  d  models    : bones / root bones / lods / lod 0 surfaces, vertices, triangles / materials against `oracle list-models --csv`
  e  maps      : world and collision counts against `oracle map-info` and `oracle list-maps --csv`
  f  entities  : MapEnts string found verbatim in the oracle's zone, behind a MapEnts header with the same length
"""
import argparse
import csv
import filecmp
import json
import os
import re
import shutil
import struct
import subprocess
import sys


def run(cmd, env=None, binary=False):
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    return p.returncode, (p.stdout if binary else p.stdout.decode('utf-8', 'replace')), p.stderr.decode('utf-8', 'replace')


class Ctx:
    def __init__(self, a):
        self.tool, self.oracle, self.game, self.work = a.tool, a.oracle, a.game, a.work
        self.exe = a.exe or os.path.join(a.game, 'C1.exe')
        self.docs = a.zone_docs
        self.env = dict(os.environ, CODOL_CACHE_DIR=os.path.join(a.work, 'cache'))
        os.makedirs(self.env['CODOL_CACHE_DIR'], exist_ok=True)
        self.folders = [os.path.join(a.game, 'client_shipRetail', 'chinese'), os.path.join(a.game, 'client_shipRetail', 'patch')]
        self.zones = []
        for folder in self.folders:
            for f in sorted(os.listdir(folder)):
                if f.endswith('.ff'):
                    self.zones.append(os.path.join(folder, f))
        self.results = []

    def mine(self, *args, binary=False):
        return run([self.tool, *args, '--exe', self.exe], binary=binary)

    def orc(self, *args):
        return run([self.oracle, *args], env=self.env)

    def report(self, step, text, ok=True):
        line = '%s [%s] %s' % ('PASS' if ok else 'FAIL', step, text)
        self.results.append((ok, line))
        print(line, flush=True)


def magic(path):
    with open(path, 'rb') as f:
        return f.read(8).decode('ascii', 'replace')


def step_a(c):
    by_size = sorted(c.zones, key=os.path.getsize, reverse=True)
    patch = [z for z in c.zones if os.sep + 'patch' + os.sep in z]
    unsigned = [z for z in by_size if magic(z) == 'IWffu100']
    sample = []
    for z in by_size[:12] + patch + by_size[-4:] + unsigned[:6] + unsigned[-2:]:
        if z not in sample:
            sample.append(z)
    out = os.path.join(c.work, 'container')
    os.makedirs(out, exist_ok=True)
    same = 0
    total_bytes = 0
    bad = []
    for z in sample:
        name = os.path.splitext(os.path.basename(z))[0]
        a, b = os.path.join(out, name + '.mine.zone'), os.path.join(out, name + '.oracle.zone')
        rc1, _, e1 = c.mine('decrypt', z, '--out', a)
        rc2, _, e2 = c.orc('decrypt', '--input', z, '--out', b, '--exe', c.exe)
        if rc1 == 0 and rc2 == 0 and filecmp.cmp(a, b, shallow=False):
            same += 1
            total_bytes += os.path.getsize(a)
        else:
            bad.append('%s ( mine rc %d %s / oracle rc %d %s )' % (name, rc1, e1.strip()[:80], rc2, e2.strip()[:80]))
        for f in (a, b):
            if os.path.exists(f):
                os.remove(f)
    n_unsigned = sum(1 for z in sample if magic(z) == 'IWffu100')
    c.report('a', 'container: %d/%d sampled zones byte-identical to the oracle ( %d signed, %d unsigned, %d patch, %.1f MB inflated )%s'
             % (same, len(sample), len(sample) - n_unsigned, n_unsigned, sum(1 for z in sample if z in patch), total_bytes / 1e6,
                ''.join('\n      ' + b for b in bad)), same == len(sample))


ORACLE_ROW = re.compile(r'^\s*(\d+)\s+(\d+)\s+(\S+)\s+0x([0-9A-Fa-f]+)\s+(\d+)\s+0x[0-9A-Fa-f]+\s*$')


def step_b(c):
    ok_all = True
    for folder in c.folders:
        rc, out, err = c.mine('check', folder, '--json')
        try:
            j = json.loads(out)
            c.report('b', 'check %s: %d zones, %d failures, %d assets ( %d registered incl. nested )'
                     % (os.path.basename(folder), j['zones_total'], j['failures'], j['assets'], j['registeredAssets']), j['failures'] == 0 and rc == 0)
            for z in j['zones']:
                if not z['ok']:
                    print('      ' + z['error'])
        except Exception as ex:
            c.report('b', 'check %s: no result ( %s %s )' % (folder, ex, err[:200]), False)

    table_ok = 0
    table_bad = []
    assets_total = 0
    type_mismatch = 0
    names_checked = names_ok = 0
    name_bad = []
    no_doc = []
    doc_rows = {'plain': [0, 0], 'flagged': [0, 0]}
    doc_examples = {'plain': [], 'flagged': []}
    doc_unmatched = {'plain': {}, 'flagged': {}}
    rebuilt = []
    for z in c.zones:
        name = os.path.splitext(os.path.basename(z))[0]
        rc, out, err = c.mine('assets', z, '--json')
        if rc != 0:
            table_bad.append(name + ': ' + err.strip()[:100])
            continue
        mine = json.loads(out)
        rc2, oout, oerr = c.orc('assets', '--input', z, '--exe', c.exe)
        orows = [m.groups() for m in map(ORACLE_ROW.match, oout.splitlines()) if m]
        o = [(int(t), int(off, 16), int(size)) for _, t, _, off, size in orows]
        onames = [n for _, _, n, _, _ in orows]
        m = [(a['zoneOffset'], a['size']) for a in mine]
        mnames = [a['type'] for a in mine]
        if rc2 == 0 and len(o) == len(m) and [(x[1], x[2]) for x in o] == m and onames == mnames:
            table_ok += 1
            assets_total += len(m)
        else:
            table_bad.append('%s: oracle %d rows, mine %d rows' % (name, len(o), len(m)))
            type_mismatch += 1

        if c.docs:
            doc = os.path.join(c.docs, name + '.txt')
            if not os.path.exists(doc):
                no_doc.append(name)
                continue
            # type<TAB>name[<TAB>flag]; the doc's own README: '~' = name guessed from a de-duplicated reference,
            # '!' = unresolved. Its names come from a heuristic that does not replay the loader.
            from collections import Counter
            rows = []
            with open(doc, encoding='latin-1') as f:
                for line in f:
                    cols = line.rstrip('\r\n').split('\t')
                    if len(cols) >= 2:
                        rows.append((cols[0], cols[1], cols[2] if len(cols) > 2 else ''))
            got = Counter((a['type'], a['name']) for a in mine)	# --json writes bytes >= 0x7F as \u00XX: latin-1 like the doc
            doc_types = Counter(r[0] for r in rows)
            my_types = Counter(a['type'] for a in mine)
            if doc_types != my_types and magic(z) == 'IWffu100':
                # an unsigned file is not the stock zone the doc was generated from
                rebuilt.append('%s ( doc %d rows, file %d )' % (name, len(rows), len(mine)))
                continue
            names_checked += 1
            if doc_types == my_types:
                names_ok += 1
            else:
                name_bad.append('%s: per type counts differ: doc %d rows, mine %d rows' % (name, len(rows), len(mine)))
            left = Counter(got)
            for typ, nm, flag in rows:
                key = 'flagged' if flag else 'plain'
                doc_rows[key][0] += 1
                if left[(typ, nm)] > 0:
                    left[(typ, nm)] -= 1
                    doc_rows[key][1] += 1
                else:
                    doc_unmatched[key][typ] = doc_unmatched[key].get(typ, 0) + 1
                    if len(doc_examples[key]) < 8:
                        doc_examples[key].append('%s: %s %r' % (name, typ, nm))
    c.report('b', 'XAsset table ( type, zone offset, data size per asset ) identical to the oracle: %d/%d zones, %d assets%s'
             % (table_ok, len(c.zones), assets_total, ''.join('\n      ' + b for b in table_bad[:10])), table_ok == len(c.zones))
    if c.docs:
        c.report('b', 'per type asset counts equal to docs/zone_assets: %d/%d zones that have a doc ( %d zones have no doc )%s'
                 % (names_ok, names_checked, len(no_doc), ''.join('\n      ' + b for b in name_bad[:40])), names_ok == names_checked)
        # informational: the doc's names are heuristic, see step g for the names against the oracle's own index
        print('INFO [b] names against docs/zone_assets: %d/%d unflagged rows and %d/%d flagged ( ~ ! ) rows have the same type + name'
              % (doc_rows['plain'][1], doc_rows['plain'][0], doc_rows['flagged'][1], doc_rows['flagged'][0]))
        for key in ('plain', 'flagged'):
            print('      doc rows without a match ( %s ) per type: %s' % (key, ', '.join('%s %d' % kv for kv in sorted(doc_unmatched[key].items(), key=lambda kv: -kv[1]))))
            for e in doc_examples[key][:8]:
                print('      doc row without a match ( %s ): %s' % (key, e))
        if rebuilt:
            print('      not compared, the file is an unsigned rebuild that no longer has the doc\'s asset list: ' + ', '.join(rebuilt))
        if no_doc:
            print('      no doc: ' + ' '.join(no_doc))


def tree(root):
    files = {}
    for d, _, fs in os.walk(root):
        for f in fs:
            p = os.path.join(d, f)
            files[os.path.relpath(p, root).replace('\\', '/').lower()] = p
    return files


def step_c(c):
    picks = ['common_mp', 'code_post_gfx_mp', 'mp_killhouse_sh', 'common_gametype_zm', 'codol_common']
    total = same = 0
    bad = []
    for pick in picks:
        z = next((x for x in c.zones if os.path.basename(x) == pick + '.ff'), None)
        if not z:
            continue
        a, b = os.path.join(c.work, 'raw', pick, 'mine'), os.path.join(c.work, 'raw', pick, 'oracle')
        for d in (a, b):
            shutil.rmtree(d, ignore_errors=True)
            os.makedirs(d)
        c.mine('dump-rawfiles', z, '--out', a)
        c.orc('zone-rawfiles', '--input', z, '--out', b, '--exe', c.exe)
        fa, fb = tree(a), tree(b)
        for k in sorted(set(fa) | set(fb)):
            total += 1
            if k in fa and k in fb and filecmp.cmp(fa[k], fb[k], shallow=False):
                same += 1
            else:
                bad.append('%s: %s ( %s )' % (pick, k, 'differs' if k in fa and k in fb else 'only mine' if k in fa else 'only oracle'))
    c.report('c', 'rawfiles: %d/%d files of %s identical to the oracle%s' % (same, total, ', '.join(picks), ''.join('\n      ' + b for b in bad[:10])),
             same == total and total > 0)


def step_d(c):
    ocsv = os.path.join(c.work, 'oracle_models.csv')
    if not os.path.exists(ocsv):
        c.orc('list-models', '--game', c.game, '--csv', ocsv)
    oracle = {}
    with open(ocsv, encoding='utf-8-sig') as f:
        for row in csv.DictReader(f):
            oracle[row['name'].lower()] = row
    mine = {}
    for z in c.zones:
        if os.sep + 'patch' + os.sep in z:
            continue
        zone = os.path.splitext(os.path.basename(z))[0]
        rc, out, _ = c.mine('models', z, '--json')
        if rc != 0:
            continue
        for m in json.loads(out):
            mine.setdefault(m['name'].lower(), {})[zone] = m

    full = skel = bad = missing = 0
    geometry_models = 0
    bad_rows = []
    for name, row in oracle.items():
        zones = row['zones'].split()
        cands = [mine[name][z] for z in zones if name in mine and z in mine[name]]
        if not cands:
            missing += 1
            bad_rows.append('%s: not found in %s' % (name, row['zones'][:60]))
            continue
        want_skel = (int(row['bones']), int(row['root_bones']), int(row['lods']))
        streamed = row['streamed'] == '1'
        want_geo = (int(row['meshes']), int(row['vertices']), int(row['faces']), int(row['materials']))
        hit = False
        for m in cands:
            if (m['bones'], m['rootBones'], m['lods']) != want_skel:
                continue
            if streamed:
                # the surfaces are not in the fast file ( models/<surfs> in an .iwd ): the oracle has them from there
                if m['lod0External'] or m['lods'] == 0:
                    hit = True
                    skel += 1
                    break
                continue
            if (m['lod0Surfaces'], m['lod0Vertices'], m['lod0Triangles'], m['materials']) == want_geo or (
                    want_geo[1] == -1 and m['lods'] == 0):
                hit = True
                full += 1
                geometry_models += 1
                break
        if not hit:
            bad += 1
            m = cands[0]
            bad_rows.append('%s: oracle bones/root/lods %s meshes/verts/faces/mats %s streamed %s; mine %s %s ext %s' % (
                name, want_skel, want_geo, row['streamed'], (m['bones'], m['rootBones'], m['lods']),
                (m['lod0Surfaces'], m['lod0Vertices'], m['lod0Triangles'], m['materials']), m['lod0External']))
    c.report('d', 'xmodels: %d oracle models; %d match on bones, root bones, lods, lod 0 surfaces, vertices, triangles and materials; '
                  '%d more have lod 0 outside the fast file ( streamed ) and match on bones, root bones and lods; %d differ, %d not found%s'
             % (len(oracle), full, skel, bad, missing, ''.join('\n      ' + b for b in bad_rows[:15])), bad == 0 and missing == 0)

    # material names of three kinds of model against the oracle's export metadata are covered by the surface table:
    for zone, model in (('mp_killhouse_sh', 'vehicle_80s_sedan1_brn_destructible_mp'), ('common_gametype_mp', 'viewmodel_m4'),
                        ('common_gametype_mp', 'body_mp_usmc_assault')):
        z = next((x for x in c.zones if os.path.basename(x) == zone + '.ff'), None)
        if not z:
            continue
        rc, out, _ = c.mine('model', z, model, '--json')
        if rc != 0:
            print('      ( %s is not in %s )' % (model, zone))
            continue
        m = json.loads(out)
        print('      %s/%s: %d bones, %d lods, lod0 %s, materials %s' % (zone, model, m['bones'], len(m['lods']),
              'external' if m['lods'] and m['lods'][0]['external'] else '%d surfaces' % len(m['lods'][0]['surfaces']) if m['lods'] else '-',
              sorted(set(x for x in m['materials']))[:6]))


def unhex(s):
    # 1.1.1.13 is the Chinese client: localized text is GBK
    raw = bytes.fromhex(s)
    try:
        return raw.decode('gbk')
    except UnicodeDecodeError:
        return raw.decode('utf-8', 'replace')


def step_g(c):
    """every zone against the oracle's own asset index ( the cache `list-models` leaves in CODOL_CACHE_DIR )"""
    import gzip
    cache = [f for f in os.listdir(c.env['CODOL_CACHE_DIR']) if f.startswith('index_') and f.endswith('.json.gz')]
    if not cache:
        c.orc('list-models', '--game', c.game, '--csv', os.path.join(c.work, 'oracle_models.csv'))
        cache = [f for f in os.listdir(c.env['CODOL_CACHE_DIR']) if f.startswith('index_') and f.endswith('.json.gz')]
    with gzip.open(os.path.join(c.env['CODOL_CACHE_DIR'], cache[0]), 'rt', encoding='utf-8') as f:
        index = {r['Record']['ZoneName']: r for r in json.load(f)}

    n = {k: [0, 0] for k in ('zones', 'asset count', 'models', 'model geometry', 'model materials', 'model address', 'anims', 'locales',
                             'inline images', 'material count', 'image count', 'material textures', 'sound alias names', 'fx names', 'weapon names',
                             'xmodel names', 'xanim names')}
    pairs = [0]
    bad = {k: [] for k in n}

    def tally(key, ok, text):
        n[key][0] += 1
        if ok:
            n[key][1] += 1
        elif len(bad[key]) < 6:
            bad[key].append(text)

    for z in c.zones:
        zone = os.path.splitext(os.path.basename(z))[0]
        o = index.get(zone)
        if o is None or os.sep + 'patch' + os.sep in z:
            continue
        rc, out, err = c.mine('index', z)
        if rc != 0:
            tally('zones', False, zone + ': ' + err[:100])
            continue
        m = json.loads(out)
        tally('zones', True, '')
        tally('asset count', o['Record']['Assets'] == m['assets'], '%s: oracle %s mine %s' % (zone, o['Record']['Assets'], m['assets']))

        mm = {}
        addresses = {}
        for x in m['models']:
            mm.setdefault(x['name'], x)
            addresses.setdefault(x['name'], set()).add(x['address'])	# a zone can hold one model name more than once
        for om in o['Models']:
            x = mm.get(om['Name'])
            who = zone + '/' + om['Name']
            if x is None:
                tally('models', False, who + ': not in my output')
                continue
            tally('models', (x['bones'], x['rootBones'], x['lods']) == (om['BoneCount'], om['RootBoneCount'], om['LodCount']),
                  '%s: oracle %s mine %s' % (who, (om['BoneCount'], om['RootBoneCount'], om['LodCount']), (x['bones'], x['rootBones'], x['lods'])))
            if not om['Streamed'] and om['LodCount'] > 0 and om['VertexCount'] >= 0:
                tally('model geometry', (x['lod0Surfaces'], x['lod0Vertices'], x['lod0Triangles']) == (om['MeshCount'], om['VertexCount'], om['FaceCount']),
                      '%s: oracle %s mine %s' % (who, (om['MeshCount'], om['VertexCount'], om['FaceCount']),
                                                 (x['lod0Surfaces'], x['lod0Vertices'], x['lod0Triangles'])))
            tally('model materials', x['materials'] == om['MaterialNames'], '%s: oracle %s mine %s' % (who, om['MaterialNames'][:3], x['materials'][:3]))
            # the oracle merges the occurrences of a model over all zones; one of them must be this zone's header
            tally('model address', any(occ['Address'] in addresses[om['Name']] for occ in om['Occurrences']),
                  '%s: oracle %s mine 0x%X' % (who, [hex(occ['Address']) for occ in om['Occurrences']][:2], x['address']))

        ma = {}
        for x in m['anims']:
            ma.setdefault(x['name'], x)
        for oa in o['Animations']:
            x = ma.get(oa['Name'])
            tally('anims', x is not None and (x['lastFrame'], x['bones'], x['notetracks']) == (oa['LastFrame'], oa['BoneCount'], oa['NotetrackCount'])
                  and abs(x['framerate'] - oa['Framerate']) < 1e-4, '%s/%s: oracle %s mine %s' % (zone, oa['Name'], oa, x))

        ml = {}
        for x in m['locales']:
            ml[unhex(x['key'])] = unhex(x['value'])
        def loose(s):
            # bytes that are not valid GBK ( a cp1252 trademark sign in one English string ): the two decoders replace
            # them differently, so compare without the replacement characters
            return ''.join(ch for ch in (s or '') if ch not in '?� ')

        for ol in o['Locales']:
            tally('locales', ml.get(ol['Key']) == ol['Value'] or loose(ml.get(ol['Key'])) == loose(ol['Value']), '%s/%s: oracle %r mine %r' % (zone, ol['Key'], ol['Value'][:30], (ml.get(ol['Key']) or '')[:30]))

        mi = {}
        for x in m['images']:
            if not x['streamed']:
                mi.setdefault(unhex(x['name']), x)
        for oi in o['EmbeddedImages']:
            x = mi.get(oi['ImageName'])
            tally('inline images', x is not None and (x['format'], x['levels'], x['width'], x['height'], x['mapType'], x['size']) ==
                  (oi['D3DFormat'], oi['LevelCount'], oi['Width'], oi['Height'], oi['MapType'], oi['DataSize']),
                  '%s/%s: oracle %s mine %s' % (zone, oi['ImageName'], oi, x))

        rec = o['Record']
        tally('material count', rec['Materials'] == len(m['materials']), '%s: oracle %s mine %s' % (zone, rec['Materials'], len(m['materials'])))
        tally('image count', rec['Images'] == len(m['images']), '%s: oracle %s mine %s' % (zone, rec['Images'], len(m['images'])))
        mine_refs = set((unhex(t), unhex(x['name'])) for x in m['materials'] for t in x['textures'])
        oracle_refs = set((t['ImageName'], t['Material']) for t in o['TextureReferences'])
        tally('material textures', mine_refs == oracle_refs, '%s: %d pairs only oracle %s, %d only mine %s' % (
            zone, len(oracle_refs - mine_refs), sorted(oracle_refs - mine_refs)[:2], len(mine_refs - oracle_refs), sorted(mine_refs - oracle_refs)[:2]))
        pairs[0] += len(oracle_refs)

        for key, okey, typ in (('sound alias names', 'Aliases', 'sound'), ('fx names', 'Effects', 'fx'), ('weapon names', 'Weapons', 'weapon'),
                               ('xmodel names', 'Models', 'xmodel'), ('xanim names', 'Animations', 'xanim')):
            mine_set = set(unhex(x) for x in m['names'].get(typ, []))
            oracle_set = set(x['Name'] for x in o[okey])
            tally(key, mine_set == oracle_set, '%s: %d only oracle %s, %d only mine %s' % (
                zone, len(oracle_set - mine_set), sorted(oracle_set - mine_set)[:2], len(mine_set - oracle_set), sorted(mine_set - oracle_set)[:2]))

    for key, (total, good) in n.items():
        extra = ' ( %d image / material pairs )' % pairs[0] if key == 'material textures' else ''
        c.report('g', 'oracle index, %s: %d/%d equal%s%s' % (key, good, total, extra, ''.join('\n      ' + b for b in bad[key])), good == total and total > 0)


MAPINFO = {
    'vertices': re.compile(r'world: ([\d,]+) vertices'),
    'triangles': re.compile(r'vertices, ([\d,]+) triangles in the index buffer'),
    'surfaces': re.compile(r'index buffer, ([\d,]+) surfaces'),
    'static_surfaces': re.compile(r'surfaces \(([\d,]+) static\)'),
    'materials': re.compile(r'static\), ([\d,]+) materials'),
    'brush_models': re.compile(r'brush models ([\d,]+)'),
    'static_models': re.compile(r'static models ([\d,]+)'),
    'distinct_models': re.compile(r'static models [\d,]+ \(([\d,]+) distinct\)'),
    'brushes': re.compile(r'collision: ([\d,]+) brushes'),
    'planes': re.compile(r'brushes, ([\d,]+) planes'),
    'clip_materials': re.compile(r'planes, ([\d,]+) clip materials'),
    'collision_triangles': re.compile(r'clip materials, ([\d,]+) triangles'),
    'partitions': re.compile(r'triangles in ([\d,]+) partitions'),
    'submodels': re.compile(r'partitions, ([\d,]+) submodels'),
    'entities': re.compile(r'entities ([\d,]+)'),
    'primary_lights': re.compile(r'primary lights ([\d,]+)'),
    'lightmaps': re.compile(r'lightmaps ([\d,]+)'),
    'reflection_probes': re.compile(r'reflection probes ([\d,]+)'),
    'cells': re.compile(r'cells ([\d,]+)'),
}


def my_map(c, z):
    rc, out, _ = c.mine('map', z, '--json')
    if rc != 0:
        return None
    j = json.loads(out)
    g, cl, e, cw = j.get('gfxWorld', {}), j.get('clipMap', {}), j.get('mapEnts', {}), j.get('comWorld', {})
    return {
        'vertices': g.get('vertices'), 'triangles': g.get('indices', 0) // 3, 'surfaces': g.get('surfaces'), 'static_surfaces': g.get('staticSurfaces'),
        'materials': g.get('materials'), 'brush_models': g.get('brushModels'), 'static_models': g.get('staticModels'),
        'distinct_models': g.get('distinctStaticModels'), 'brushes': cl.get('brushes'), 'planes': cl.get('planes'),
        'clip_materials': cl.get('materials'), 'collision_triangles': cl.get('collisionTriangles'), 'partitions': cl.get('partitions'),
        'submodels': cl.get('subModels'), 'entities': e.get('entities'), 'primary_lights': cw.get('primaryLights'),
        'lightmaps': g.get('lightmaps'), 'reflection_probes': g.get('reflectionProbes'), 'cells': g.get('cells'),
    }


def step_e(c):
    for name in ('mp_killhouse_sh', 'zm_sumpf_sh', 'mp_terminal_cls'):
        z = next((x for x in c.zones if os.path.basename(x) == name + '.ff'), None)
        rc, out, err = c.orc('map-info', '--game', c.game, '--map', name)
        want = {k: int(r.search(out).group(1).replace(',', '')) for k, r in MAPINFO.items() if r.search(out)}
        got = my_map(c, z) or {}
        diff = {k: (v, got.get(k)) for k, v in want.items() if got.get(k) != v}
        c.report('e', 'map-info %s: %d/%d counts equal ( %s )%s' % (name, len(want) - len(diff), len(want),
                 ', '.join('%s %d' % (k, want[k]) for k in ('surfaces', 'vertices', 'static_models', 'brushes') if k in want),
                 ''.join('\n      %s: oracle %s mine %s' % (k, v[0], v[1]) for k, v in diff.items())), not diff and len(want) >= 15)

    lcsv = os.path.join(c.work, 'oracle_maps.csv')
    c.orc('list-maps', '--game', c.game, '--csv', lcsv)
    if not os.path.exists(lcsv):
        c.report('e', 'list-maps: the oracle wrote no csv', False)
        return
    with open(lcsv, encoding='utf-8-sig') as f:
        rows = list(csv.DictReader(f))
    print('      list-maps columns: ' + ', '.join(rows[0].keys()) if rows else '      list-maps: empty')
    same = 0
    bad = []
    for row in rows:
        zone = row.get('zone') or row.get('name')
        z = next((x for x in c.zones if os.path.basename(x) == zone + '.ff'), None)
        got = my_map(c, z) if z else None
        if not got:
            bad.append('%s: no map in my output' % zone)
            continue
        pairs = [(k, mk) for k, mk in (('triangles', 'triangles'), ('static_models', 'static_models'), ('distinct_models', 'distinct_models'),
                                        ('brushes', 'brushes'), ('entities', 'entities'), ('materials', 'materials')) if k in row and row[k] != '']
        diff = [(k, row[k], got[mk]) for k, mk in pairs if int(row[k]) != got[mk]]
        if diff or not pairs:
            bad.append('%s: %s' % (zone, diff))
        else:
            same += 1
    c.report('e', 'list-maps: %d/%d maps equal on triangles, static models, distinct models, brushes, entities, materials%s'
             % (same, len(rows), ''.join('\n      ' + b for b in bad[:10])), same == len(rows) and rows)


def step_f(c):
    out = os.path.join(c.work, 'ents')
    os.makedirs(out, exist_ok=True)
    for name, want in (('mp_killhouse_sh', (227009, 1967, 143)), ('zm_sumpf_sh', None), ('mp_terminal_cls', None), ('mp_crash_tropical', None)):
        z = next((x for x in c.zones if os.path.basename(x) == name + '.ff'), None)
        if not z:
            continue
        ents = os.path.join(out, name + '.ents')
        zone = os.path.join(out, name + '.oracle.zone')
        rc, _, err = c.mine('dump-ents', z, '--out', ents)
        c.orc('decrypt', '--input', z, '--out', zone, '--exe', c.exe)
        with open(ents, 'rb') as f:
            text = f.read()
        with open(zone, 'rb') as f:
            data = f.read()
        os.remove(zone)
        m = re.search(r'(\d+) bytes, (\d+) entities, (\d+) trigger models', err)
        stats = tuple(int(x) for x in m.groups()) if m else None
        # independent location: the MapEnts header { name -1, entityString -1, numEntityChars } in the stream, then the
        # name string, then the entity string
        found = False
        pos = data.find(text + b'\0')
        if pos > 0 and data.count(text + b'\0') == 1:
            # the name is either an alias ( header directly in front of the text ) or inline ( name string in between )
            for header in (pos - 44, data.rfind(b'\0', 0, pos - 1) + 1 - 44):
                name_ptr, ents_ptr, chars, triggers = struct.unpack_from('<IIiI', data, header)
                if ents_ptr == 0xFFFFFFFF and chars == len(text) + 1 and (stats is None or triggers == stats[2]):
                    found = True
                    break
        ok = found and (want is None or stats == want)
        c.report('f', 'MapEnts %s: %s bytes, %s entities, %s trigger models; string %s in the oracle zone behind a MapEnts header of that length%s'
                 % ((name,) + (stats or ('?', '?', '?')) + ('found verbatim' if found else 'NOT found',
                    '' if want is None else ' ( expected %d / %d / %d )' % want)), ok)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tool', required=True)
    ap.add_argument('--oracle', required=True)
    ap.add_argument('--game', required=True)
    ap.add_argument('--work', required=True)
    ap.add_argument('--exe')
    ap.add_argument('--zone-docs')
    ap.add_argument('--steps', default='a,b,c,d,e,f')
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    c = Ctx(a)
    print('%d fast files' % len(c.zones), flush=True)
    steps = {'a': step_a, 'b': step_b, 'c': step_c, 'd': step_d, 'e': step_e, 'f': step_f, 'g': step_g}
    for s in a.steps.split(','):
        steps[s](c)
    failed = [line for ok, line in c.results if not ok]
    print('\n%d checks, %d failed' % (len(c.results), len(failed)))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
