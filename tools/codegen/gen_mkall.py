#!/usr/bin/env python3
# usage: gen_mkall.py list | gen <group> [--out ROOT] [--no-insn-comments] [--tmp DIR]
#
# The machine-transliterated ports of tools/codegen/mkall.py + mips2cpp.py, generated whole from the ISO and the group's committed,
# hand-written inputs (nothing is read from the generated files themselves):
#   tools/codegen/gen/mkall/<group>.json   the run: level, generator (mkall.py / mkall_l<n>.py), environment (helper prefix, the 64-bit argument
#                                     registers, MKALL_FORCE / MKALL_UNIFORM), the entries (address:name[:ret:args]), the glue files
#   tools/codegen/gen/mkall/<group>.glue   the group's own call-site glue (callee address: statement): the calls the generator cannot build
#                                     from a port's declaration (hand-written statements, the entries / helpers of a sibling file, ...)
#   tools/codegen/gen/skel/<path>.skel     each output file with every generated function replaced by a `//@gen <name>` line: the file's
#                                     head (comment, includes), its split into parts, its function order and any hand-written text
# `gen` runs the generator on the level's overlay read from the ISO (tools/codegen/ovlist.py) and fills the skeletons: every `//@gen` line
# must be produced by the run and every function of the run placed (else it fails, naming them). --no-insn-comments: the trailing
# comments carrying the original instruction (`// lw a1,0xf0(sp)`, `// jal 0x...`) are left out (tools/codegen/gen_common.py); the
# `// orig:` lines and the code stay, so the result compiles to the same object code.
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
W = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
import gen_common  # noqa: E402

SPEC = os.path.join(HERE, 'gen', 'mkall')
SKEL = os.path.join(HERE, 'gen', 'skel')
DEF = re.compile(r'^(\w+) (\w+)\(([^)]*)\) \{$')
MARK = '//@gen '


def groups():
    return sorted(os.path.splitext(f)[0] for f in os.listdir(SPEC) if f.endswith('.json'))


def load(name):
    return json.load(open(os.path.join(SPEC, name + '.json')))


def skel_path(rel):
    return os.path.join(SKEL, rel[len('src/'):] + '.skel')


def run(name, tmp, quiet=True):
    """Runs the group's generator; returns {function name: [lines]} in the run's order."""
    d = load(name)
    os.makedirs(tmp, exist_ok=True)
    lst = os.path.join(tmp, 'lst_' + name)
    shutil.rmtree(lst, ignore_errors=True)  # (mkall caches listings by address: always from the ISO)
    env = dict(os.environ)
    for k in list(env):
        if k.startswith('MKALL_') or k.startswith('MIPS2CPP_'):
            del env[k]
    env.update({'MKALL_ISO': '1', 'MKALL_LEVEL': str(d['level']), 'MKALL_TMP': lst,
                'MKALL_VCOMMON': os.path.join(HERE, 'common_v.glue')})
    env.update(d.get('env', {}))
    glue = os.path.join(tmp, name + '.allglue')
    with open(glue, 'w') as gf:
        for g in d.get('glue_files', []) + ['tools/codegen/gen/mkall/%s.glue' % name]:
            gf.write(open(os.path.join(W, g)).read() + '\n')
    out = os.path.join(tmp, name + '.gen.cpp')
    if d.get('tool') == 'mips2cpp.py':  # one function straight from its listing (no closure: its callees are all glued), e.g. the boot module's
        import ovlist
        lv = d['level'] if d['level'] == 'front' else int(d['level'])
        parts = []
        for e in d['entries']:
            ad, nm = e.split(':')[:2]
            os.makedirs(lst, exist_ok=True)
            lf = os.path.join(lst, ad + '.s')
            open(lf, 'w').write('\n'.join(ovlist.listing(lv, int(ad, 16))) + '\n')
            r = subprocess.run([sys.executable, os.path.join(HERE, 'mips2cpp.py'), lf, '--name', nm, '--callees', glue] + d.get('args', []),
                               env=env, capture_output=True, text=True)
            if r.returncode:
                sys.exit('%s: mips2cpp failed on %s: %s' % (name, ad, r.stderr[-800:]))
            parts.append(r.stdout)
        open(out, 'w').write('\n'.join(parts))
        r = subprocess.CompletedProcess([], 0, '', '')
    else:
        cmd = [sys.executable, os.path.join(HERE, d.get('tool', 'mkall.py')), out, 'gen ' + name] + d['entries'] + ['--glue', glue]
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
    open(os.path.join(tmp, name + '.log'), 'w').write(r.stderr)
    bad = [l for l in r.stderr.splitlines() if 'FAILED' in l or 'NO GLUE' in l]
    if r.returncode or not os.path.exists(out):
        sys.exit('%s: the generator failed (rc %d): %s' % (name, r.returncode, r.stderr[-800:]))
    if bad:
        sys.exit('%s: %s' % (name, ' / '.join(bad[:5])))
    t = open(out).read()
    L = t.split('\n')
    res = {}
    for s, e, nm in gen_common.blocks(t):
        res[nm] = L[s:e]
    # another region's disc: the functions the region changed, transliterated from that disc's own build (tools/codegen/ovlist.py
    # listing, a stand-in): labelled
    standins = {int(m.group(1), 16): int(m.group(2), 16) for m in re.finditer(r'\[ovlist\] STANDIN ([0-9a-f]+) PAL ([0-9a-f]+)', r.stderr)}
    if d.get('tool') == 'mips2cpp.py':
        import ovlist
        standins.update(dict(ovlist.STANDINS))
    for nm, body in res.items():
        m = next((re.search(r'orig: (?:L\d+:|front:)?0x([0-9a-f]+)', x) for x in body if 'orig:' in x), None)
        if m and int(m.group(1), 16) in standins:
            a = int(m.group(1), 16)
            res[nm] = ['// STAND-IN (another region\'s disc): the code the region changed here is not on this disc; this is the disc\'s own',
                       '// build of the function (PAL:0x%08x), transliterated at the NTSC-U entry 0x%08x, with the enhancements\' hook'
                       % (standins[a], a),
                       '// points of that build (tools/codegen/hooks.glue, `PAL:` lines). Temporary: not proved by the oracle.'] + body
            print('%s: %s is a stand-in (PAL:0x%08x)' % (name, nm, standins[a]), file=sys.stderr)
    return res


def cmd_gen(name, out_root, strip, tmp):
    d = load(name)
    fns = run(name, tmp)
    # another region's disc whose build rewrote functions here (stand-ins): the run may call functions the NTSC-U code
    # does not (placed, declared first, at the first function of the unit's first file) and no longer reach some of the
    # NTSC-U code's (left out); on the NTSC-U disc both stop the generation
    other = any(any('STAND-IN' in x for x in b[:1]) for b in fns.values())
    placed = {l[len(MARK):].strip() for rel in d['files'] for l in open(skel_path(rel)).read().split('\n') if l.startswith(MARK)}
    extra_fns = [nm for nm in fns if nm not in placed] if other else []
    used = set()
    written = []
    for rel in d['files']:
        sk = open(skel_path(rel)).read().split('\n')
        out = []
        for l in sk:
            if l.startswith(MARK):
                nm = l[len(MARK):].strip()
                if extra_fns:  # (the stand-ins' own callees: their declarations, then their bodies)
                    out.append('// (the functions the stand-ins below call and the NTSC-U code does not: tools/codegen/gen_mkall.py)')
                    for x in extra_fns:
                        head = next((y for y in fns[x] if y.rstrip().endswith('{') and x + '(' in y), None)
                        if head is not None:
                            out.append(head.rstrip()[:-1].rstrip() + ';')
                    for x in extra_fns:
                        out.extend(fns[x])
                        used.add(x)
                    extra_fns = []
                if nm not in fns:
                    if other:
                        print('%s: %s: %s is not reached from this disc\'s build (left out)' % (name, rel, nm), file=sys.stderr)
                        continue
                    sys.exit('%s: %s: the run does not produce %s (remove it from the skeleton or fix the run)' % (name, rel, nm))
                out.extend(fns[nm])
                used.add(nm)
            else:
                out.append(l)
        text = '\n'.join(out)
        text = gen_common.region_proof(text)
        if strip:
            text = gen_common.strip_insn_comments(text)
        p = os.path.join(out_root, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        gen_common.write_if_changed(p, text)
        written.append(rel)
    extra = [nm for nm in fns if nm not in used]
    if extra:
        sys.exit('%s: the run produces functions no skeleton places: %s (add `%s<name>` lines to a skeleton)' % (name, ' '.join(extra[:12]), MARK))
    return written


def main():
    a = sys.argv[1:]
    if not a or a[0] == 'list':
        print('\n'.join(groups()))
        return
    opt = lambda k, dflt=None: a[a.index(k) + 1] if k in a else dflt
    if a[0] == 'gen':
        tmp = opt('--tmp') or tempfile.mkdtemp(prefix='gen_mkall_')
        for f in cmd_gen(a[1], opt('--out', W), '--no-insn-comments' in a, tmp):
            print(f)


if __name__ == '__main__':
    main()
