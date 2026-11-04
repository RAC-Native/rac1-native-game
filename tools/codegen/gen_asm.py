#!/usr/bin/env python3
# usage: gen_asm.py list | gen <spec ...> [--out ROOT] [--no-insn-comments] [--tmp DIR]
#
# The asm2fn.py transliterations (tools/codegen/asm2fn_specs/<name>.json -> the spec's `out`, src/game/ovl/<name>_asm.cpp), generated whole
# from the ISO: each function's listing straight from its level's overlay (tools/codegen/ovlist.py), then tools/codegen/asm2fn_batch.py. The spec
# (ours: names, signatures, jump / call tables, the file's head) is the only other input. --no-insn-comments as in gen_mkall.py.
# A function's "host_ntsc" / "entry_host_ntsc" (as "host" / "entry_host", asm2fn.py) apply to the NTSC-U build's code only: another
# region's disc whose build of the function differs gets that build's stand-in (tools/codegen/ovlist.py) without them.
import glob
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
W = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
import gen_common  # noqa: E402
import ovlist  # noqa: E402

SPECS = os.path.join(HERE, 'asm2fn_specs')


def specs():
    return sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(SPECS, '*.json')))


def gen(name, out_root, strip, tmp):
    d = json.load(open(os.path.join(SPECS, name + '.json')))
    lvl = d.get('level', 0)
    lst = os.path.join(tmp, name)
    os.makedirs(lst, exist_ok=True)
    funcs = []
    for f in d['functions']:
        f = dict(f)
        funcs.append(f)
        if f.get('hand'):
            continue
        lv = f.get('level', lvl)  # (a function's own "level" overrides the spec's)
        n0 = len(ovlist.STANDINS)
        open(os.path.join(lst, 'lst_%s.s' % f['addr']), 'w').write('\n'.join(ovlist.listing(lv, int(f['addr'], 16))) + '\n')
        # "host_ntsc" / "entry_host_ntsc": host code at the NTSC-U build's addresses only (its own labels): merged into "host" /
        # "entry_host" for the disc's own build of the function, left out of another region's stand-in (that build's code)
        standin = len(ovlist.STANDINS) > n0
        hn, en = f.pop('host_ntsc', {}), f.pop('entry_host_ntsc', None)
        if not standin:
            if hn:
                f['host'] = dict(f.get('host', {}), **hn)
            if en:
                f['entry_host'] = en
    d2 = dict(d)
    d2['functions'] = funcs
    d2['listings'] = lst
    d2['out'] = os.path.join(lst, 'out.cpp')
    sp = os.path.join(lst, name + '.json')
    json.dump(d2, open(sp, 'w'), indent=1)
    env = dict(os.environ, RE_TMP=lst)
    r = subprocess.run([sys.executable, os.path.join(HERE, 'asm2fn_batch.py'), sp], capture_output=True, text=True, env=env)
    if r.returncode:
        sys.exit('%s: asm2fn_batch failed: %s' % (name, r.stderr[-800:]))
    text = open(d2['out']).read()
    if '#error' in text:
        sys.exit('%s: #error in the output' % name)
    text = gen_common.region_proof(text)
    if strip:
        text = gen_common.strip_insn_comments(text)
    p = os.path.join(out_root, d['out'])
    os.makedirs(os.path.dirname(p), exist_ok=True)
    gen_common.write_if_changed(p, text)
    return d['out']


def main():
    a = sys.argv[1:]
    if not a or a[0] == 'list':
        print('\n'.join(specs()))
        return
    opt = lambda k, dflt=None: a[a.index(k) + 1] if k in a else dflt
    vals = {opt('--out'), opt('--tmp')}
    names = [x for x in a[1:] if not x.startswith('--') and x not in vals] or specs()
    tmp = opt('--tmp') or tempfile.mkdtemp(prefix='gen_asm_')
    for n in names:
        print(gen(n, opt('--out', W), '--no-insn-comments' in a, tmp))


if __name__ == '__main__':
    main()
