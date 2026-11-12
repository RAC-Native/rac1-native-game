#!/usr/bin/env python3
# usage: generate_all.py [--out ROOT] [--check] [--no-insn-comments] [--jobs N] [--only UNIT ...] [--list] [--files]
#
# The single entry point of the code generated from the game's own code: every machine-transliterated port, written whole from the
# player's ISO (and the boot ELF / overlay maps setup.sh extracts from it) plus the committed, hand-written generator
# inputs (no generated file is read). The units:
#   mkall:<group>   tools/codegen/gen_mkall.py   mips2cpp / mkall transliterations (tools/codegen/gen/mkall/<group>.json, .glue, gen/skel/)
#   asm2cpp:<name>  tools/codegen/gen_asm2cpp.py hand-written routines transliterated by asm2cpp and reviewed (tools/codegen/gen/asm2cpp/<name>.json)
#   vu:moby         tools/codegen/gen_moby_vu_fast.py   the moby pass's VU0 / VU1 programs (exact and fast)
#   vu:world        tools/codegen/gen_world_vu.py       the world passes' six VU1 programs (exact and fast)
#   vu:light        tools/codegen/gen_light_vu0.py      the static lighting's VU0 program (exact and fast)
# --out ROOT        write ROOT/src/... (default: the repository: the dev repo commits the generated files)
# --check           generate into a temporary root and compare with the repository's files, byte for byte (exit 1 on a difference)
# --no-insn-comments  without the comments that carry the original instruction (the public setup): the code is the same
# --jobs N          units run in parallel (default 4)
# --verify-strip    generate twice, with and without the instruction comments, and check that every file is the same C++ token for
#                   token once its comments are gone (g++ -fpreprocessed -E: the compiler's own comment removal; the generated code
#                   uses no __LINE__), i.e. that --no-insn-comments compiles to the same object code
# --list / --files  the units / every generated file (relative paths), nothing generated
# Environment: RAC1_ASSETS (default assets/): iso/rac1-ntsc-u.iso, dumps/SCUS_971.99, generated/overlay_map/ (setup.sh).
#   RAC1_ISO: the disc. On another region's disc (the PAL one: iso/rac1-pal.iso, dumps/SCES_509.16) the overlays are read at
#   NTSC-U addresses through the region table (tools/overlay_match/region_view.py), and the generators' maps are
#   generated/overlay_map/codegen/; a function the region changed is translated from that disc's code as a labelled
#   stand-in (RAC1_PAL_CHANGED=fail stops instead).
import concurrent.futures
import filecmp
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
W = os.path.abspath(os.path.join(HERE, '..', '..'))
OVL = 'src/game/ovl/'
VU = {
    'vu:moby': ['moby_vu0.cpp', 'moby_vu0_fast.cpp', 'moby_vu1.cpp', 'moby_vu1_fast.cpp'],
    'vu:world': ['world_vu1_%s%s.cpp' % (s, f) for s in ('tfrag_a', 'tfrag_b', 'tie_a', 'tie_b', 'shrub_a', 'shrub_b') for f in ('', '_fast')],
    'vu:light': ['world_light_vu0.cpp', 'world_light_vu0_fast.cpp'],
}


def units():
    """{unit: [generated files, relative to the repository]}"""
    out = {}
    for p in sorted(glob.glob(os.path.join(HERE, 'gen', 'mkall', '*.json'))):
        out['mkall:' + os.path.splitext(os.path.basename(p))[0]] = json.load(open(p))['files']
    for p in sorted(glob.glob(os.path.join(HERE, 'gen', 'asm2cpp', '*.json'))):
        out['asm2cpp:' + os.path.splitext(os.path.basename(p))[0]] = [json.load(open(p))['file']]
    for k, fs in VU.items():
        out[k] = [OVL + f for f in fs]
    return out


def command(unit, root, strip, tmp):
    kind, name = unit.split(':', 1)
    py = sys.executable
    if kind == 'mkall':
        return [py, os.path.join(HERE, 'gen_mkall.py'), 'gen', name, '--out', root, '--tmp', tmp] + (['--no-insn-comments'] if strip else [])
    if kind == 'asm2cpp':
        return [py, os.path.join(HERE, 'gen_asm2cpp.py'), 'gen', name, '--out', root] + (['--no-insn-comments'] if strip else [])
    out = os.path.join(root, OVL)
    if unit == 'vu:moby':
        return [py, os.path.join(HERE, 'gen_moby_vu_fast.py'), '--out', out]
    if unit == 'vu:world':
        return [py, os.path.join(HERE, 'gen_world_vu.py'), out]
    if unit == 'vu:light':
        sys.path.insert(0, HERE)
        import vu_micro
        return [py, os.path.join(HERE, 'gen_light_vu0.py'), vu_micro.ELF_DEFAULT, out]
    raise SystemExit('unknown unit ' + unit)


def run_unit(unit, root, strip, tmpbase):
    tmp = os.path.join(tmpbase, unit.replace(':', '_'))
    os.makedirs(tmp, exist_ok=True)
    os.makedirs(os.path.join(root, OVL), exist_ok=True)
    t0 = time.time()
    r = subprocess.run(command(unit, root, strip, tmp), capture_output=True, text=True, cwd=W)
    return unit, r.returncode, time.time() - t0, (r.stderr or r.stdout)[-1500:]


def _tokens(path):
    r = subprocess.run(['g++', '-fpreprocessed', '-E', '-P', '-x', 'c++', path], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('g++ -fpreprocessed failed on %s: %s' % (path, r.stderr[-400:]))
    return r.stdout.split()


def verify_strip(sel, us, jobs):
    tmp = tempfile.mkdtemp(prefix='rac1_genstrip_')
    roots = {False: os.path.join(tmp, 'full'), True: os.path.join(tmp, 'stripped')}
    for strip, root in roots.items():
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
            res = list(ex.map(lambda u: run_unit(u, root, strip, os.path.join(tmp, 'work')), sel))
        bad = [u for u, rc, dt, err in res if rc]
        if bad:
            print('generation failed:', ' '.join(bad))
            return 1
    sys.path.insert(0, HERE)
    import gen_common
    same = differ = 0
    nlines = [0, 0]
    ninsn = [0, 0]
    for u in sel:
        for f in us[u]:
            a, b = os.path.join(roots[False], f), os.path.join(roots[True], f)
            for k, p in enumerate((a, b)):
                for line in open(p):
                    nlines[k] += 1
                    c = gen_common._split_comment(line.rstrip('\n'))[1]
                    if c is not None and gen_common.is_insn_comment(c.split('//')[0]) and not c.lstrip().startswith('orig:'):
                        ninsn[k] += 1
            if _tokens(a) == _tokens(b):
                same += 1
            else:
                differ += 1
                print('TOKENS DIFFER', f)
    sz = [sum(os.path.getsize(os.path.join(roots[k], f)) for u in sel for f in us[u]) for k in (False, True)]
    print('verify-strip: %d files the same C++ without their instruction comments, %d differ; %d -> %d lines, %d -> %d bytes, '
          '%d -> %d lines with an instruction comment' % (same, differ, nlines[0], nlines[1], sz[0], sz[1], ninsn[0], ninsn[1]))
    shutil.rmtree(tmp, ignore_errors=True)
    return 1 if differ else 0


def main():
    a = sys.argv[1:]
    opt = lambda k, dflt=None: a[a.index(k) + 1] if k in a else dflt
    us = units()
    if '--list' in a:
        for u, fs in us.items():
            print(u, ' '.join(fs))
        return
    if '--files' in a:
        for fs in us.values():
            for f in fs:
                print(f)
        return
    only = []
    if '--only' in a:
        i = a.index('--only') + 1
        while i < len(a) and not a[i].startswith('--'):
            only.append(a[i])
            i += 1
    sel = [u for u in us if not only or u in only or u.split(':')[0] in only]
    if '--verify-strip' in a:
        sys.exit(verify_strip(sel, us, int(opt('--jobs', '4'))))
    check = '--check' in a
    strip = '--no-insn-comments' in a
    jobs = int(opt('--jobs', '4'))
    root = os.path.abspath(opt('--out', W))
    tmpbase = tempfile.mkdtemp(prefix='rac1_gen_')
    if check:
        root = os.path.join(tmpbase, 'root')
    t0 = time.time()
    failed = []
    crashed = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        futs = [ex.submit(run_unit, u, root, strip, tmpbase) for u in sel]
        for f in concurrent.futures.as_completed(futs):
            unit, rc, dt, err = f.result()
            crashed[unit] = rc < 0 or rc >= 0xC0000000 or 'Fatal Python error' in err
            print('%-34s %s %6.1f s' % (unit, 'ok  ' if rc == 0 else 'FAIL', dt), flush=True)
            if rc:
                failed.append(unit)
                print('   ' + err.replace('\n', '\n   '))
    # A unit whose Python crashed (an exit code from a Windows exception, 0xC0000000 and up: "Fatal Python error"
    # from CPython 3.14's interpreter, GitHub issue #72) is run once more, alone: the crash is not the generator's
    for unit in [u for u in failed if crashed.get(u)]:
        unit, rc, dt, err = run_unit(unit, root, strip, tmpbase)
        print('%-34s %s %6.1f s (run again)' % (unit, 'ok  ' if rc == 0 else 'FAIL', dt), flush=True)
        if rc == 0:
            failed.remove(unit)
        else:
            print('   ' + err.replace('\n', '\n   '))
    # the VU0 ops without their flags where a file reads none (tools/codegen/vu0_nf_pass.py)
    sys.path.insert(0, HERE)
    import vu0_nf_pass
    nf = 0
    for u in sel:
        if u in failed:
            continue
        for f in us[u]:
            path = os.path.join(root, f)
            if path.endswith('.cpp') and os.path.exists(path) and vu0_nf_pass.apply(path):
                nf += 1
    print('%d files with the VU0 ops without their flags (vu0_nf_pass.py)' % nf)
    nfiles = sum(len(us[u]) for u in sel)
    print('%d units, %d files, %.1f s%s' % (len(sel), nfiles, time.time() - t0, '' if not failed else ', FAILED: ' + ' '.join(failed)))
    rc = 1 if failed else 0
    if check:
        same = diff = missing = 0
        for u in sel:
            for f in us[u]:
                g, c = os.path.join(root, f), os.path.join(W, f)
                if not os.path.exists(g):
                    missing += 1
                    print('NOT GENERATED', f)
                elif not os.path.exists(c) or not filecmp.cmp(g, c, shallow=False):
                    diff += 1
                    print('DIFFERS', f)
                else:
                    same += 1
        print('check: %d identical, %d differ, %d not generated' % (same, diff, missing))
        if diff or missing:
            rc = 1
    shutil.rmtree(tmpbase, ignore_errors=True)
    sys.exit(rc)


if __name__ == '__main__':
    main()
