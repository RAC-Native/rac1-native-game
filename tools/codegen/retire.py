#!/usr/bin/env python3
"""retire.py: take generated functions out of a mkall unit, so that the game and the unit's other generated code call
hand-written functions in their place.

usage: retire.py <unit> [<target>...] [--call ADDR=FUNCTION ...] [--glue 'ADDR: STATEMENT' ...] [options]
       retire.py --help              (prints this text, does nothing else)

<unit>    a mkall unit: `aridia` or `mkall:aridia` (tools/codegen/gen/mkall/<unit>.json, .glue, the skeletons of its files)
<target>  a generated function of the unit, by name (`lv2_2618a8_l2`, `shark_hook_0_l2`) or by address (`0x2618a8`, `2618a8`)

What happens to each target:
  an entry (the json's `entries`: a class update, a hook, a draw callback) leaves `entries` (and `MKALL_UNIFORM`). Register
     the hand-written function in its place in the level's registration files yourself (`*_mobys.cpp`, `*_hooks.cpp`,
     `carry_hooks_levels.cpp` ...), best before running this: the tool refuses while hand-written code names a function
     that goes (see --allow-refs).
  a helper in MKALL_FORCE leaves MKALL_FORCE: the generator then glues its call sites to the Veldin port through the
     port's `// orig: 0x...` header declaration and the level's overlay map / variants table (mkall.py's glue_for).
  any other generated function needs a glue line for its address (the generator would translate it again otherwise):
     --call 0xADDR=FUNCTION   a glue line built from FUNCTION's declaration in a src/game header, the way mkall.py builds
                              one from a port's declaration (integer, float, Vec4& / Mat4&, pointer / reference parameters)
     --glue '0xADDR: STMT'    a glue line as written (a statement on the generator's registers: a0.w(), fr[12], set32(v0, ...))
     An entry that the unit's other functions also call needs one too, unless the generator glues it by itself.
  a helper the generator now glues by itself (a declaration got its `// orig: 0x...`, a map / variants row was added)
     needs nothing: it is accepted when the run no longer produces it.
With no target at all the tool brings the unit in line with today's headers and maps: every function the run no longer
produces is dropped (after a port's declaration got its `// orig:`, every unit's copies of it go at once).

Then the tool:
  1. runs the unit's generator once (tools/codegen/gen_mkall.py's run, 20 s to a few minutes) and compares what it produces
     with what the skeletons place: every helper the run no longer produces (no entry reaches it any more) is dropped
     as well: its `//@gen` line and forward declarations leave the skeletons, its declarations leave the src/game headers,
     its address leaves MKALL_FORCE / MKALL_WIDE_GPR, and the unit's glue lines only the dropped functions used go;
     a skeleton left without any `//@gen` line is deleted and its file leaves `files` (the generated file is deleted with
     --apply; a unit left without files loses its json and glue, and its line in tools/codegen/gen/units.txt, the list of
     the units generate_all.py generates, when that file exists);
  2. writes the unit's files into a temporary root from that run, the way generate_all.py does (gen_mkall.py's fill of
     the skeletons, without the instruction comments as the setup writes them, then vu0_nf_pass.py), adds an `#include`
     to a skeleton whose new call sites call a function its includes do not declare, and writes them again if it did
     (--verify runs generate_all.py itself as well and checks that it writes the same files);
  3. prints the summary against the files in assets/generated/: per file the functions gone and the call sites changed
     (old callee -> new callee); anything else that differs (a function added or changed elsewhere than at a call site of a
     dropped function, other text than includes and the dropped functions' declarations) is an error;
  4. checks: hand-written code, oracle batches or glue files that still name a dropped function (an error unless
     --allow-refs), hook points of tools/codegen/hooks.glue inside a dropped function (an error: the enhancement would be
     lost), and the PAL builds' changed functions (`entry` lines of data/regions/SCES-50916*.txt) inside a dropped function
     (printed: the hand-written function must handle that region's code, see fixes102.h);
  5. with --apply, installs the regenerated files in assets/generated/src/game/ovl/ (a new file renamed over the old one,
     never written in place: assets/ are hard links shared with other checkouts) and deletes the generated files that
     became empty; prints the generated file count (generate_all.py --files) before and after.

On an error every edited source file is put back as it was (unless --keep-edits) and the exit code is 1.

Options:
  --apply          install the regenerated files (without it: the source edits stay, assets/generated is not touched)
  --dry-run        everything, then put every edited file back (nothing changes; the summary shows what would)
  --out DIR        the regeneration root (default: a new directory under $RAC1_RETIRE_TMP or the system's temp dir,
                   removed at the end; DIR is kept)
  --allow-refs     go on although hand-written code / batches / glue files still name a dropped function (they are listed)
  --keep-edits     on an error, leave the source edits in place (to look at them)
  --insn-comments  regenerate with the original instructions' comments (the dev tree's form; the setup leaves them out)
  -q               shorter output (no per-call-site lines)
  --verify         regenerate once more with generate_all.py itself and check that it writes the same files

Examples:
  retire.py aridia 0x2618a8 0x2e7660                       # two FORCE'd copies of Veldin ports: glued to the ports
  retire.py aridia shark_hook_0_l2 --apply                 # an entry, after aridia_hooks.cpp calls the hand-written hook
  retire.py novalis_classes 0x26de80 --call 0x26de80=doomlet_259028 --apply   # a copy the map does not pair: glued by hand
"""
import sys

if __name__ == '__main__' and (len(sys.argv) < 2 or any(a in ('-h', '--help', 'help') for a in sys.argv[1:])):
    print(__doc__)
    sys.exit(0)

import json  # noqa: E402
import os  # noqa: E402
import re  # noqa: E402
import shutil  # noqa: E402
import subprocess  # noqa: E402
import tempfile  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
W = os.path.abspath(os.path.join(HERE, '..', '..'))
SPEC = os.path.join(HERE, 'gen', 'mkall')
SKEL = os.path.join(HERE, 'gen', 'skel')
GENROOT = os.path.join(W, 'assets', 'generated')
MARK = '//@gen '
DEF = re.compile(r'^(\w+) (\w+)\(([^)]*)\) \{$')
ORIG = re.compile(r'orig: (?:L(\d+):|front:)?0x([0-9a-f]+)')
sys.path.insert(0, HERE)


class Fail(Exception):
    pass


# ---------------------------------------------------------------------------------------------------------------- edits
class Edits:
    """Every source file the tool changes, with its original text, so that an error puts them all back."""

    def __init__(self):
        self.orig = {}

    def _keep(self, path):
        if path not in self.orig:
            self.orig[path] = open(path).read() if os.path.exists(path) else None

    def write(self, path, text):
        self._keep(path)
        if os.path.exists(path) and open(path).read() == text:
            return
        open(path, 'w').write(text)

    def remove(self, path):
        self._keep(path)
        if os.path.exists(path):
            os.remove(path)

    def changed(self):
        out = []
        for p, t in self.orig.items():
            now = open(p).read() if os.path.exists(p) else None
            if now != t:
                out.append(p)
        return sorted(out)

    def restore(self):
        for p, t in self.orig.items():
            if t is None:
                if os.path.exists(p):
                    os.remove(p)
            else:
                cur = open(p).read() if os.path.exists(p) else None
                if cur != t:
                    open(p, 'w').write(t)


# ------------------------------------------------------------------------------------------------------------- helpers
def rel_w(p):
    return os.path.relpath(p, W)


def skel_path(rel):
    return os.path.join(SKEL, rel[len('src/'):] + '.skel')


def blocks(text):
    """{name: (start line, end line, address)} of a generated file's function definitions."""
    import gen_common
    L = text.split('\n')
    out = {}
    for s, e, nm in gen_common.blocks(text):
        ad = None
        for x in L[s:e]:
            m = ORIG.search(x)
            if m:
                ad = int(m.group(2), 16)
                break
        out[nm] = (s, e, ad)
    return out


def parse_addr(x):
    x = x.strip().lower()
    if x.startswith('l') and ':' in x:
        x = x.split(':', 1)[1]
    try:
        return int(x, 16) if re.fullmatch(r'(0x)?[0-9a-f]{5,8}', x) else None
    except ValueError:
        return None


def header_files():
    out = []
    for root, _dirs, files in os.walk(os.path.join(W, 'src', 'game')):
        for f in files:
            if f.endswith('.h'):
                out.append(os.path.join(root, f))
    return sorted(out)


DECL_LINE = r'^\s*(?:extern\s+)?(?:inline\s+)?([\w:<>&\* ]+?)\s+[\*&]?(%s)\s*\(([^;{]*)\)\s*;'


def find_decl(name):
    """[(header path, return type, parameters)] of a function's one-line declarations in src/game headers."""
    out = []
    rx = re.compile(DECL_LINE % re.escape(name))
    for h in header_files():
        for line in open(h):
            m = rx.match(line)
            if m:
                out.append((h, m.group(1).strip(), m.group(3).strip()))
    return out


def include_closure(text, seen=None):
    """The src/game headers a file includes, directly or through other src/game headers (`#include <game/...>`)."""
    seen = set() if seen is None else seen
    for m in re.finditer(r'^\s*#include\s*<(game/[^>]+)>', text, re.M):
        h = m.group(1)
        if h in seen:
            continue
        seen.add(h)
        p = os.path.join(W, 'src', h)
        if os.path.exists(p):
            include_closure(open(p).read(), seen)
    return seen


def jal_targets(lines):
    """Call / jump targets (jal, j) of listing lines `addr word ...`."""
    out = set()
    for l in lines:
        m = re.match(r'^([0-9a-f]{8})\s+([0-9a-f]{8})\s', l)
        if not m:
            continue
        a, w = int(m.group(1), 16), int(m.group(2), 16)
        if (w >> 26) in (2, 3):
            out.add(((a + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2))
    return out


def func_range(level, addr):
    import ovlist
    ls = ovlist.listing(level, addr)
    if not ls:
        return addr, addr + 4, []
    last = int(ls[-1].split()[0], 16)
    return addr, last + 4, ls


def region_entries(level):
    """{table name: [NTSC-U entry addresses]} of the functions the PAL builds changed on a level."""
    out = {}
    for f, nm in (('SCES-50916.txt', 'PAL 1.00'), ('SCES-50916-2.00.txt', 'PAL 2.00')):
        p = os.path.join(W, 'data', 'regions', f)
        if not os.path.exists(p):
            continue
        u = None
        lst = []
        for line in open(p):
            q = line.split()
            if not q:
                continue
            if q[0] == 'overlay':
                u = q[1]
            elif q[0] == 'entry' and u == str(level):
                lst.append(int(q[1], 16))
        out[nm] = lst
    return out


def hook_points(level):
    out = []
    p = os.path.join(HERE, 'hooks.glue')
    for line in open(p):
        m = re.match(r'^L(\d+)\s+(0x[0-9a-fA-F]+)', line)
        if m and int(m.group(1)) == level:
            out.append((int(m.group(2), 16), line.rstrip('\n')))
    return out


def references(names, skip):
    """{name: [file:line]} of the places outside the generated code that name these functions."""
    out = {}
    if not names:
        return out
    rx = re.compile(r'\b(%s)\b' % '|'.join(re.escape(n) for n in sorted(names)))
    roots = [os.path.join(W, 'src'), os.path.join(W, 'tests'), os.path.join(W, 'tools', 'codegen')]
    skip = set(skip) | {os.path.abspath(__file__)}  # (this tool's own examples)
    for r in roots:
        for root, _dirs, files in os.walk(r):
            for f in files:
                p = os.path.join(root, f)
                if p in skip or not f.endswith(('.cpp', '.h', '.glue', '.json', '.skel', '.py', '.txt', '.cmake')):
                    continue
                try:
                    lines = open(p).read().split('\n')
                except UnicodeDecodeError:
                    continue
                for k, line in enumerate(lines):
                    for m in rx.finditer(re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)):  # (not in a string: a check's label)
                        out.setdefault(m.group(1), []).append('%s:%d' % (rel_w(p), k + 1))
    return out


UNITS_LIST = os.path.join(SKEL, '..', 'units.txt')  # (tools/codegen/gen/units.txt: the units generate_all.py generates)


def units_list_update(ed, add=(), remove=()):
    """Keeps tools/codegen/gen/units.txt (one `mkall:<group>` / `asm2cpp:<name>` per line, `#` comments) in step with the
    units on disk: the lines of `remove` go, those of `add` are appended when missing. Nothing when the file does not exist
    (a tree without the list generates every json it finds). A unit split in two (a new json) adds its new name here."""
    p = os.path.normpath(UNITS_LIST)
    if not os.path.exists(p):
        return
    L = open(p).read().split('\n')
    gone = set(remove)
    out = [l for l in L if l.strip() not in gone]
    have = {l.strip() for l in out}
    for u in add:
        if u not in have:
            k = len(out)
            while k > 0 and out[k - 1].strip() == '':
                k -= 1
            out.insert(k, u)
            have.add(u)
    if out != L:
        ed.write(p, '\n'.join(out))
        print('%s: %s' % (rel_w(p), ', '.join(['- ' + u for u in remove if u not in have] + ['+ ' + u for u in add])))


def gen_files_count():
    r = subprocess.run([sys.executable, os.path.join(HERE, 'generate_all.py'), '--files'], capture_output=True, text=True, cwd=W)
    return len([l for l in r.stdout.splitlines() if l.strip()])


# ------------------------------------------------------------------------------------------------------- the glue lines
def glue_from_function(level, addr, fname, tmp):
    """A glue statement calling the hand-written FUNCTION, built from its header declaration as mkall.py builds one."""
    ds = find_decl(fname)
    if not ds:
        raise Fail('--call 0x%x=%s: no one-line declaration of %s in a src/game header' % (addr, fname, fname))
    os.environ.update({'MKALL_ISO': '1', 'MKALL_LEVEL': str(level), 'MKALL_TMP': os.path.join(tmp, 'mkall_import')})
    import mkall
    key = 0x7fff0000 + addr  # (a key no port uses: the declaration found, under the generator's own glue builder)
    mkall.DECL[key] = [(ret, fname, args, os.path.basename(h)) for h, ret, args in ds]
    g = mkall.glue_for(key)
    del mkall.DECL[key]
    if g is None:
        raise Fail('--call 0x%x=%s: its declaration (%s) is not one the generator can glue (write the statement with --glue)'
                   % (addr, fname, '; '.join('%s %s(%s)' % (r, fname, a) for _h, r, a in ds)))
    return g[1]


def read_glue(path):
    out = {}
    if os.path.exists(path):
        for line in open(path):
            m = re.match(r'\s*(0x[0-9a-fA-F]+)\s*:', line.split('//')[0])
            if m:
                out[int(m.group(1), 16)] = line.rstrip('\n')
    return out


# ------------------------------------------------------------------------------------------------------------ the diff
CALLRX = re.compile(r'(?<![\w.>:])([A-Za-z_]\w*)\s*\(')
NOT_CALLS = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'static_cast', 'reinterpret_cast', 'const_cast', 'set32', 'u2f',
             'f2u', 'lw_', 'sw_', 'lh_', 'lhu_', 'lb_', 'lbu_', 'sh_', 'sb_', 'ld_', 'sd_', 'V', 'M', 'TrRet'}


def hunks(a, b):
    import difflib
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    return [(i1, i2, j1, j2) for tag, i1, i2, j1, j2 in sm.get_opcodes() if tag != 'equal']


def compare(old_files, new_files, dropped, gone_files):
    """The summary of old -> new: (lines, errors, new callees by file). old_files / new_files: {rel: text}."""
    lines, errors = [], []
    callees_new = {}
    oldf = {r: blocks(t) for r, t in old_files.items()}
    newf = {r: blocks(t) for r, t in new_files.items()}
    old_names = {n: r for r, bs in oldf.items() for n in bs}
    new_names = {n: r for r, bs in newf.items() for n in bs}
    for n in sorted(set(new_names) - set(old_names)):
        errors.append('a function the old files do not have: %s (%s)' % (n, new_names[n]))
    for n in sorted(set(old_names) - set(new_names) - set(dropped)):
        errors.append('a function gone that is not dropped: %s (%s)' % (n, old_names[n]))
    for n in sorted(set(old_names) & set(new_names)):
        if old_names[n] != new_names[n]:
            errors.append('%s moved from %s to %s' % (n, old_names[n], new_names[n]))
    drx = re.compile(r'\b(%s)\b' % '|'.join(re.escape(x) for x in sorted(dropped))) if dropped else None
    for r in sorted(old_files):
        if r in gone_files:
            lines.append('%s: deleted (no generated function left): %s' % (r, ', '.join(sorted(oldf[r])) or '-'))
            continue
        a, b = old_files[r].split('\n'), new_files[r].split('\n')
        gone = sorted(n for n in oldf[r] if n not in newf[r])
        nsites = 0
        site_lines = []
        other = []
        incs = []

        def skeleton(L, bs):
            """The text outside the functions, each function one line `@@<name>`, and [(name, its lines)]."""
            out, fns = [], {}
            starts = {s: (e, n) for n, (s, e, _ad) in bs.items()}
            i = 0
            while i < len(L):
                if i in starts:
                    e, n = starts[i]
                    out.append('@@' + n)
                    fns[n] = (i, L[i:e])
                    i = e
                else:
                    out.append(L[i])
                    i += 1
            return out, fns
        sa, fa = skeleton(a, oldf[r])
        sb, fb = skeleton(b, newf[r])
        # the text outside the functions: the gone functions' places, includes, the dropped functions' declarations
        for i1, i2, j1, j2 in hunks(sa, sb):
            old_h = [x for x in sa[i1:i2] if x.strip() and not (x.startswith('@@') and x[2:] in gone)]
            new_h = [x for x in sb[j1:j2] if x.strip()]
            ok = True
            for x in old_h:
                if x.startswith('#include'):
                    incs.append('- ' + x)
                elif drx is not None and drx.search(x) and x.rstrip().endswith(';') and not x.startswith('@@'):
                    pass
                else:
                    ok = False
            for x in new_h:
                if x.startswith('#include'):
                    incs.append('+ ' + x)
                else:
                    ok = False
            if not ok:
                other.append(('outside the functions, near line %d' % (i1 + 1), old_h, new_h))
        # each function both have: the call sites of the dropped functions
        for n in sorted(set(fa) & set(fb)):
            la, lb = fa[n][1], fb[n][1]
            if la == lb:
                continue
            for i1, i2, j1, j2 in hunks(la, lb):
                old_h, new_h = la[i1:i2], lb[j1:j2]
                if drx is not None and any(drx.search(x) for x in old_h) and not any(drx.search(x) for x in new_h):
                    nsites += 1
                    oc = sorted({m.group(1) for x in old_h for m in drx.finditer(x)})
                    nc = sorted({c for x in new_h for c in CALLRX.findall(x) if c not in NOT_CALLS})
                    callees_new.setdefault(r, set()).update(nc)
                    site_lines.append('    %s: %s -> %s' % (n, ', '.join(oc), ', '.join(nc) or '(no call)'))
                    continue
                other.append(('%s, line %d' % (n, fa[n][0] + i1 + 1), old_h, new_h))
        lines.append('%s: %d lines -> %d; %d functions gone%s; %d call sites changed; %d include lines changed'
                     % (r, len(a), len(b), len(gone), (' (' + ', '.join(gone) + ')') if gone else '', nsites, len(incs)))
        lines.extend(sorted(set(site_lines)))
        lines.extend('    include ' + x for x in incs)
        for ln, oh, nh in other:
            errors.append('%s (%s): an unexpected difference:\n%s' % (r, ln, '\n'.join(['      - ' + x for x in oh[:6]] + ['      + ' + x for x in nh[:6]])))
    return lines, errors, callees_new


# ------------------------------------------------------------------------------------------------------------- the run
def generate(unit, out, strip, fns=None, tmp=None):
    """The unit's files under `out`: with `fns` (the generator's run, already made), in this process the way
    generate_all.py does it (gen_mkall.py's fill of the skeletons, then vu0_nf_pass.py); else generate_all.py itself."""
    os.makedirs(os.path.join(out, 'src', 'game', 'ovl'), exist_ok=True)
    if fns is not None:
        import gen_mkall
        import vu0_nf_pass
        run0 = gen_mkall.run
        gen_mkall.run = lambda name, tmp_, quiet=True: fns
        try:
            written = gen_mkall.cmd_gen(unit, out, strip, tmp)
        except SystemExit as e:
            raise Fail('the regeneration failed: %s' % e)
        finally:
            gen_mkall.run = run0
        for r in written:
            p = os.path.join(out, r)
            if p.endswith('.cpp') and os.path.exists(p):
                vu0_nf_pass.apply(p)
        return
    cmd = [sys.executable, os.path.join(HERE, 'generate_all.py'), '--out', out, '--only', 'mkall:' + unit, '--jobs', '1']
    if strip:
        cmd.append('--no-insn-comments')
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=W)
    if r.returncode or 'FAILED' in r.stdout:
        raise Fail('the regeneration failed:\n' + (r.stdout + r.stderr)[-2500:])


def main():
    a = sys.argv[1:]
    flags = {'--apply', '--dry-run', '--allow-refs', '--keep-edits', '--insn-comments', '-q', '--verify'}
    opts = {k: (k in a) for k in flags}
    calls, glues, out_dir = [], [], None
    pos = []
    i = 0
    while i < len(a):
        x = a[i]
        if x in flags:
            i += 1
        elif x in ('--call', '--glue', '--out'):
            if i + 1 >= len(a):
                sys.exit('%s needs a value' % x)
            v = a[i + 1]
            if x == '--call':
                if '=' not in v:
                    sys.exit('--call 0xADDR=FUNCTION')
                ad, fn = v.split('=', 1)
                if parse_addr(ad) is None:
                    sys.exit('--call: bad address %r' % ad)
                calls.append((parse_addr(ad), fn.strip()))
            elif x == '--glue':
                m = re.match(r'\s*(0x[0-9a-fA-F]+)\s*:\s*(.+)$', v)
                if not m:
                    sys.exit("--glue '0xADDR: statement'")
                glues.append((int(m.group(1), 16), m.group(2).strip()))
            else:
                out_dir = os.path.abspath(v)
            i += 2
        elif x.startswith('-'):
            sys.exit('unknown option %s (retire.py --help)' % x)
        else:
            pos.append(x)
            i += 1
    if len(pos) < 1:
        sys.exit('usage: retire.py <unit> <target>... (retire.py --help)')
    unit = pos[0].split(':', 1)[1] if pos[0].startswith('mkall:') else pos[0]
    targets = pos[1:]
    jp = os.path.join(SPEC, unit + '.json')
    if not os.path.exists(jp):
        sys.exit('no unit %s (%s)' % (unit, rel_w(jp)))
    tmp_root = os.environ.get('RAC1_RETIRE_TMP') or None
    work = tempfile.mkdtemp(prefix='retire_%s_' % unit, dir=tmp_root)
    out = out_dir or os.path.join(work, 'out')
    ed = Edits()
    rc = 1
    try:
        rc = run(unit, targets, calls, glues, opts, out, work, ed)
    except Fail as e:
        print('ERROR: %s' % e)
        rc = 1
    finally:
        if rc != 0 and not opts['--keep-edits']:
            ed.restore()
            if ed.orig:
                print('(every edited file put back as it was)')
        elif opts['--dry-run']:
            ed.restore()
            print('(--dry-run: every edited file put back as it was)')
        shutil.rmtree(work, ignore_errors=True)
    sys.exit(rc)


def run(unit, targets, calls, glues, opts, out, work, ed):
    import gen_mkall
    jp = os.path.join(SPEC, unit + '.json')
    gp = os.path.join(SPEC, unit + '.glue')
    jtext = open(jp).read()
    d = json.loads(jtext)
    level = d['level']
    if not isinstance(level, int):
        raise Fail('unit %s: level %r (only the levels\' units)' % (unit, level))
    files = list(d['files'])
    env = d.setdefault('env', {})
    old_files = {}
    for r in files:
        p = os.path.join(GENROOT, r)
        if not os.path.exists(p):
            raise Fail('%s is not in assets/generated (run the setup / build first)' % r)
        old_files[r] = open(p).read()
    old_bl = {n: (r, s, e, ad) for r, t in old_files.items() for n, (s, e, ad) in blocks(t).items()}
    by_addr = {}
    for n, (r, s, e, ad) in old_bl.items():
        if ad is not None:
            by_addr.setdefault(ad, n)
    entries = [(int(x.split(':')[0], 16), x.split(':')[1], x) for x in d['entries']]
    entry_by_name = {nm: (ad, x) for ad, nm, x in entries}
    entry_by_addr = {ad: (nm, x) for ad, nm, x in entries}
    force = [x for x in env.get('MKALL_FORCE', '').split(',') if x]
    force_set = {int(x, 16) for x in force}
    uniform = [x for x in env.get('MKALL_UNIFORM', '').split(',') if x]
    glue_have = read_glue(gp)

    # ---- the targets
    print('unit mkall:%s (level %d): %d entries, %d in MKALL_FORCE, %d generated functions in %d files'
          % (unit, level, len(entries), len(force), len(old_bl), len(files)))
    tg = []  # (name, address, kind)
    skipped = []
    glue_addrs = {ad for ad, _f in calls} | {ad for ad, _s in glues}
    for t in targets:
        ad = parse_addr(t)
        nm = None
        if ad is None:
            nm = t
            if nm in old_bl:
                ad = old_bl[nm][3]
            elif nm in entry_by_name:
                ad = entry_by_name[nm][0]
            else:
                skipped.append('%s: not a generated function of mkall:%s' % (t, unit))
                continue
        else:
            nm = entry_by_addr[ad][0] if ad in entry_by_addr else by_addr.get(ad)
            if nm is None:
                skipped.append('%s: no generated function of mkall:%s at 0x%x' % (t, unit, ad))
                continue
        if nm not in old_bl:
            skipped.append('%s: not in the generated files (stale entry?)' % t)
            continue
        kind = 'entry' if ad in entry_by_addr else 'force' if ad in force_set else 'glue' if ad in glue_addrs else 'auto'
        tg.append((nm, ad, kind))
    for s in skipped:
        print('SKIP ' + s)
    for ad, _f in calls + [(x, None) for x, _s in glues]:
        if ad not in {t[1] for t in tg} and ad not in by_addr and ad not in entry_by_addr:
            print('note: glue for 0x%x, which is not generated in this unit now' % ad)
    if targets and not tg and not calls and not glues:
        raise Fail('nothing left to retire')
    for nm, ad, kind in tg:
        print('target %-34s 0x%08x %s' % (nm, ad, {'entry': 'entry', 'force': 'MKALL_FORCE helper', 'glue': 'helper, glued',
                                                    'auto': 'helper the generator should glue by itself now'}[kind]))

    # ---- json and glue edits
    gone_entries = {nm for nm, ad, k in tg if k == 'entry'}
    d['entries'] = [x for ad, nm, x in entries if nm not in gone_entries]
    unforce = {ad for nm, ad, k in tg if k == 'force'}
    if 'MKALL_FORCE' in env:
        env['MKALL_FORCE'] = ','.join(x for x in force if int(x, 16) not in unforce)
    if 'MKALL_UNIFORM' in env:
        env['MKALL_UNIFORM'] = ','.join(x for x in uniform if x not in gone_entries)
    new_glue = []
    for ad, fn in calls:
        new_glue.append((ad, glue_from_function(level, ad, fn, work)))
    new_glue += glues
    glue_text = open(gp).read() if os.path.exists(gp) else ''
    for ad, st in new_glue:
        line = '0x%08x: %s' % (ad, st)
        if ad in glue_have:
            if glue_have[ad].split('//')[0].strip() == line:
                continue
            raise Fail('%s already has a glue line for 0x%x: %s' % (rel_w(gp), ad, glue_have[ad]))
        glue_text = glue_text + ('' if glue_text.endswith('\n') or not glue_text else '\n') + line + '\n'
        print('glue + ' + line)
    if glue_text or os.path.exists(gp):
        ed.write(gp, glue_text)
    ed.write(jp, json.dumps(d, indent=1) + ('\n' if jtext.endswith('\n') else ''))

    # ---- the generator's run: what it produces now
    tmp = os.path.join(work, 'run')
    os.makedirs(tmp, exist_ok=True)
    print('running the generator (mkall:%s) ...' % unit, flush=True)
    try:
        fns = gen_mkall.run(unit, tmp)
    except SystemExit as e:
        raise Fail('the generator failed: %s' % e)
    produced = set(fns)
    sk_text = {r: open(skel_path(r)).read() for r in files}
    placed = {}
    for r, t in sk_text.items():
        for l in t.split('\n'):
            if l.startswith(MARK):
                placed[l[len(MARK):].split()[0]] = r
    still = [nm for nm, ad, k in tg if nm in produced]
    if still:
        why = []
        for nm in still:
            callers = sorted(n for n, b in fns.items() if n != nm and any(re.search(r'\b%s\(' % re.escape(nm), x) for x in b))
            why.append('%s (called by %s)' % (nm, ', '.join(callers) or 'an entry'))
        raise Fail('still produced by the generator: %s. A helper of a Veldin port leaves MKALL_FORCE but the generator '
                   'found no declaration to glue it to (does the port have `// orig: 0x<Veldin address>` on a one-line header '
                   'declaration, and does the level\'s map pair the addresses?), or an entry is called by other functions of '
                   'the unit: give --call / --glue for its address' % '; '.join(why))
    extra = sorted(produced - set(placed))
    if extra:
        info = []
        for n in extra:
            ad = next((int(m.group(2), 16) for x in fns[n] for m in [ORIG.search(x)] if m), None)
            info.append('%s (0x%x)' % (n, ad or 0))
        raise Fail('the run produces functions no skeleton places: %s. An entry left `entries` but the unit\'s other '
                   'functions call it: give --call 0xADDR=<hand-written function> (or --glue) for it' % ', '.join(info))
    dropped = sorted(set(placed) - produced)
    for nm, ad, k in tg:
        if nm not in dropped:
            raise Fail('%s is not dropped by the run (?)' % nm)
    helpers_dropped = [n for n in dropped if n not in {t[0] for t in tg}]
    print('dropped: %d targets, %d other functions the run no longer produces (helpers only they reached, or copies the '
          'generator now glues to a port)%s' % (len(tg), len(helpers_dropped), (': ' + ', '.join(helpers_dropped)) if helpers_dropped else ''))
    dropped_addr = {n: old_bl[n][3] for n in dropped if n in old_bl}

    # ---- skeleton edits: the //@gen lines and forward declarations of the dropped functions
    gone_files = []
    for r in files:
        t = sk_text[r]
        L = t.split('\n')
        outl = []
        k = 0
        while k < len(L):
            l = L[k]
            if l.startswith(MARK) and l[len(MARK):].split()[0] in dropped:
                k += 1
                if k < len(L) and L[k] == '' and outl and outl[-1] == '':
                    k += 1  # (the blank line after it)
                continue
            if l.rstrip().endswith(';') and not l.startswith((' ', '\t')):  # (a forward declaration of a dropped function)
                m = re.match(r'^[\w:<>&\* ]+?\s+[\*&]?(\w+)\s*\(', l)
                if m and m.group(1) in dropped:
                    k += 1
                    continue
            outl.append(l)
            k += 1
        nt = '\n'.join(outl)
        if not any(l.startswith(MARK) for l in outl):
            gone_files.append(r)
            ed.remove(skel_path(r))
        elif nt != t:
            ed.write(skel_path(r), nt)
    if gone_files:
        d['files'] = [r for r in files if r not in gone_files]
        print('files left without a generated function: %s' % ', '.join(gone_files))
    # json: the dropped helpers' addresses out of MKALL_FORCE / MKALL_WIDE_GPR
    dad = set(dropped_addr.values())
    if env.get('MKALL_FORCE'):
        env['MKALL_FORCE'] = ','.join(x for x in env['MKALL_FORCE'].split(',') if x and int(x, 16) not in dad)
    if env.get('MKALL_WIDE_GPR'):
        env['MKALL_WIDE_GPR'] = ';'.join(x for x in env['MKALL_WIDE_GPR'].split(';') if x and int(x.split(':')[0], 16) not in dad)
    for k in ('MKALL_FORCE', 'MKALL_UNIFORM', 'MKALL_WIDE_GPR'):
        if k in env and env[k] == '' and k not in json.loads(jtext).get('env', {}):
            del env[k]
    # the unit's glue lines only the dropped functions used
    import ovlist  # noqa: F401
    lst = os.path.join(tmp, 'lst_' + unit)
    used = set()
    if os.path.isdir(lst):
        for f in os.listdir(lst):
            if f.endswith('.s'):
                used |= jal_targets(open(os.path.join(lst, f)).read().split('\n'))
    ranges = {}
    dropped_calls = set()
    for n, ad in dropped_addr.items():
        if ad is None:
            continue
        s, e, ls = func_range(level, ad)
        ranges[n] = (s, e)
        dropped_calls |= jal_targets(ls)
    glue_now = read_glue(gp)
    keep_new = {ad for ad, _s in new_glue}
    stale = sorted(ad for ad in glue_now if ad in dropped_calls and ad not in used and ad not in keep_new
                   and ad not in {x[0] for x in entries})
    if stale:
        gl = open(gp).read().split('\n')
        gl = [l for l in gl if not (re.match(r'\s*(0x[0-9a-fA-F]+)\s*:', l.split('//')[0])
                                    and int(re.match(r'\s*(0x[0-9a-fA-F]+)', l).group(1), 16) in stale)]
        ed.write(gp, '\n'.join(gl))
        print('glue lines only the dropped functions used, removed: %s' % ', '.join('0x%x' % x for x in stale))
    if d['files']:
        ed.write(jp, json.dumps(d, indent=1) + ('\n' if jtext.endswith('\n') else ''))
    else:
        print('the unit has no file left: its json and glue are deleted')
        ed.remove(jp)
        if os.path.exists(gp):
            ed.remove(gp)
        units_list_update(ed, remove=['mkall:' + unit])

    # ---- header declarations of the dropped functions
    hdr_removed = []
    for h in header_files():
        t = open(h).read()
        L = t.split('\n')
        keep = []
        for l in L:
            m = re.match(r'^\s*(?:extern\s+)?[\w:<>&\* ]+?\s+[\*&]?(\w+)\s*\([^;{]*\)\s*;', l)
            if m and m.group(1) in dropped:
                hdr_removed.append('%s: %s' % (rel_w(h), m.group(1)))
                continue
            keep.append(l)
        if len(keep) != len(L):
            ed.write(h, '\n'.join(keep))
    for x in hdr_removed:
        print('declaration removed: ' + x)

    # ---- regenerate, then the includes the new call sites need
    if not d['files']:
        new_files = {}
    else:
        print('regenerating into %s ...' % out, flush=True)
        generate(unit, out, not opts['--insn-comments'], fns, tmp)
        new_files = {r: open(os.path.join(out, r)).read() for r in d['files']}
    old_for_cmp = dict(old_files)
    nf_cmp = dict(new_files)
    for r in gone_files:
        nf_cmp[r] = ''
    lines, errors, callees_new = compare(old_for_cmp, nf_cmp, set(dropped), set(gone_files))
    added_inc = []
    for r, names in callees_new.items():
        if r in gone_files:
            continue
        sk = open(skel_path(r)).read()
        reach = include_closure(sk)
        own = set(blocks(new_files[r]))
        for nm in sorted(names):
            if nm in own or re.search(r'^[^\n]*\b%s\s*\([^\n]*;\s*$' % re.escape(nm), sk, re.M):
                continue
            ds = find_decl(nm)
            if not ds or any(rel_w(h)[len('src/'):] in reach for h, _r, _a in ds):
                continue
            h = rel_w(ds[0][0])[len('src/'):]
            incs = list(re.finditer(r'^#include .*\n', sk, re.M))
            p = incs[-1].end() if incs else 0
            sk = sk[:p] + '#include <%s>\n' % h + sk[p:]
            reach = include_closure(sk)
            added_inc.append('%s: #include <%s> (for %s)' % (os.path.basename(skel_path(r)), h, nm))
        ed.write(skel_path(r), sk)
    if added_inc:
        for x in added_inc:
            print('skeleton include added: ' + x)
        print('regenerating again (includes) ...', flush=True)
        generate(unit, out, not opts['--insn-comments'], fns, tmp)
        new_files = {r: open(os.path.join(out, r)).read() for r in d['files']}
        nf_cmp = dict(new_files)
        for r in gone_files:
            nf_cmp[r] = ''
        lines, errors, callees_new = compare(old_for_cmp, nf_cmp, set(dropped), set(gone_files))

    if opts['--verify'] and d['files']:
        print('verifying with generate_all.py ...', flush=True)
        vout = os.path.join(work, 'verify')
        generate(unit, vout, not opts['--insn-comments'])
        for r in d['files']:
            if open(os.path.join(vout, r)).read() != new_files[r]:
                raise Fail('generate_all.py writes another %s than this tool (a bug of retire.py: report it)' % r)
        print('verified: generate_all.py writes the same files')

    # ---- checks: references left, hook points, the PAL builds' changed functions
    skip = {jp, gp} | {skel_path(r) for r in files}
    refs = references(set(dropped), skip)
    ref_lines = []
    for n in sorted(refs):
        for x in refs[n]:
            ref_lines.append('    %s: %s' % (n, x))
    hooks = []
    for ad, line in hook_points(level):
        for n, (s, e) in ranges.items():
            if s <= ad < e:
                hooks.append('    %s (0x%x..0x%x): %s' % (n, s, e, line))
    regions = []
    for tab, ents in region_entries(level).items():
        for n, (s, e) in ranges.items():
            hits = [x for x in ents if s <= x < e]
            if hits:
                regions.append('    %s %s (0x%x..0x%x): %d changed-function entries (%s)'
                               % (tab, n, s, e, len(hits), ' '.join('0x%x' % x for x in hits[:6])))

    # ---- the summary
    print('---- summary (mkall:%s, against assets/generated)' % unit)
    for l in lines:
        if opts['-q'] and l.startswith('    ') and not l.startswith('    include'):
            continue
        print(l)
    tot_old = sum(t.count('\n') for t in old_files.values())
    tot_new = sum(t.count('\n') for t in new_files.values())
    print('generated lines %d -> %d (%+d), files %d -> %d' % (tot_old, tot_new, tot_new - tot_old, len(files), len(d['files'])))
    if regions:
        print('the PAL builds changed code inside dropped functions (the hand-written function must handle it: fixes102.h, '
              'region tables):')
        print('\n'.join(regions))
    if ref_lines:
        print(('references to dropped functions left (%s):' % ('allowed: --allow-refs' if opts['--allow-refs'] else 'ERROR')))
        print('\n'.join(ref_lines))
        if not opts['--allow-refs']:
            errors.append('hand-written code, batches or glue still name dropped functions (listed above): point them at the '
                          'hand-written functions first, or --allow-refs')
    if hooks:
        print('hook points of tools/codegen/hooks.glue inside dropped functions (ERROR):')
        print('\n'.join(hooks))
        errors.append('hooks.glue has hook points inside dropped functions: the enhancement would be lost')
    if errors:
        for x in errors:
            print('ERROR: ' + x)
        return 1
    print('edited: ' + (', '.join(rel_w(p) for p in ed.changed()) or 'nothing'))
    if opts['--dry-run'] or not opts['--apply']:
        if not opts['--dry-run']:
            print('(not applied: assets/generated unchanged; run again with --apply, or install the files from --out)')
        return 0
    # ---- --apply
    n0 = len(files)
    c0 = gen_files_count() + len(gone_files) if not d['files'] or gone_files else None
    for r, t in new_files.items():
        dst = os.path.join(GENROOT, r)
        if os.path.exists(dst) and open(dst).read() == t:
            continue
        tmpf = dst + '.retire-tmp'
        shutil.copyfile(os.path.join(out, r), tmpf)
        os.replace(tmpf, dst)  # (a new file: the old one may be a hard link shared with other checkouts)
        print('installed ' + rel_w(dst))
    for r in gone_files:
        p = os.path.join(GENROOT, r)
        if os.path.exists(p):
            os.remove(p)
            print('deleted ' + rel_w(p))
    c1 = gen_files_count()
    print('generated files: %s -> %d (this unit %d -> %d)' % (c0 if c0 is not None else c1, c1, n0, len(d['files'])))
    return 0


if __name__ == '__main__':
    main()
