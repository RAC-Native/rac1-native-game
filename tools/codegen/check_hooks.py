#!/usr/bin/env python3
"""check_hooks.py: every enhancement hook of the code generation must land in generated code.

usage: check_hooks.py [--gen-root DIR] [--hooks FILE] [--level N] [--strict] [--list]
       check_hooks.py --help            (prints this text, does nothing else)

The project's own additions to the game's generated code (the 1.02 forks of src/game/enhance/fixes102.h, the HUD width,
the performance switches, the mods' hooks) are statements the generator places into generated functions:
  - tools/codegen/hooks.glue: `L<n> 0xADDR: <statement>` before the instruction at ADDR of level n's code, `L<n> 0xADDR
    label`, `L<n> PAL:0xADDR: ...` (the PAL 1.00 disc's stand-ins of a function that region changed);
  - a unit's glue (tools/codegen/gen/mkall/<unit>.glue, tools/codegen/*.glue): `0xCALLEE: <statement>` at every call of
    CALLEE in the unit's code; a line that calls the enhancement API (src/game/enhance/*.h, render_view_hud_*,
    palette_blend_loop) is a hook;
  - a skeleton's own lines (tools/codegen/gen/skel/**/*.skel) that call the enhancement API.
When the function such a statement sits in becomes hand-written, the generator silently stops placing it: the setting
then does nothing, and neither the tours (default settings) nor the oracle (the original has no hooks) can see it.

What is checked (an ERROR makes the exit code 1):
  hooks.glue  the address lies inside a function of the level's code (the overlay's function starts, the function's
              listing: tools/codegen/ovlist.py, as retire.py measures a dropped function); that function is GENERATED
              (a function of the generated files whose `// orig:` line names L<n>:0x<entry>); the statement is IN its
              body (whitespace aside), a label's `L_<addr>:` too. A `PAL:` line: the NTSC-U function the region table
              (data/regions/SCES-50916.txt `entry` rows) pairs with the PAL function holding the address is generated.
              When the function is hand-written (a `// orig:` definition in src/game) the error says so, and whether the
              port calls the hook's enhancement functions (then the hook line is dead: replace it by a note).
  unit glue   an enhancement line's statement appears in at least one of the unit's generated files (otherwise every
              call site it served is hand-written now: the hand-written callers must call the enhancement themselves).
              (A text search: a glue statement that is also written elsewhere in the unit's files, as a hooks.glue
              statement for instance, is not told apart from it.)
  skeletons   an enhancement line of a skeleton appears in its generated file.
A WARNING (an ERROR with --strict): a hooked function that is still generated but already has a hand-written port (a
shared family port whose `// orig:` line names the level's copy, not yet glued in) that does not call the hook's
enhancement functions: the fork is lost the day that level is glued to the port.

Options:
  --gen-root DIR  the generated sources (default assets/generated/src; a `generate_all.py --out DIR` root: DIR/src)
  --hooks FILE    another hooks file than tools/codegen/hooks.glue (e.g. an older version's, to see where its hooks went)
  --level N       only level N's hooks.glue lines
  --strict        warnings are errors
  --list          print every hooks.glue hook with its generated function, file and line (the proof that it applies);
                  without it the output is the problems and a summary line
"""
import sys

if __name__ == '__main__' and any(a in ('-h', '--help', 'help') for a in sys.argv[1:]):
    print(__doc__)
    sys.exit(0)

import bisect  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402
import re  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
W = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)

ORIG = re.compile(r'orig: ((?:L\d+:|front:)?0x[0-9a-f]+)')
HOOK = re.compile(r'^L(\d+)\s+(PAL:)?(0x[0-9a-fA-F]+)\s*(?::\s*(.*?)|\s+(label))\s*$')


def norm(stmt):
    """A statement's code without its comment and whitespace (the generator copies it verbatim)."""
    return re.sub(r'\s+', '', stmt.split('  //')[0])


def enhancement_names():
    """The enhancement API: the functions declared in src/game/enhance/*.h, the HUD width's and the palette blend's."""
    out = {'render_view_hud_begin', 'render_view_hud_end', 'palette_blend_loop'}
    rx = re.compile(r'^\s*(?:inline\s+|static\s+|extern\s+)*[\w:<>&\*]+(?:\s+[\w:<>&\*]+)*?\s+[\*&]?(\w+)\s*\(', re.M)
    d = os.path.join(W, 'src', 'game', 'enhance')
    for f in sorted(os.listdir(d)):
        if f.endswith('.h'):
            out |= {m.group(1) for m in rx.finditer(open(os.path.join(d, f)).read())}
    return out - {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'operator'}


def generated_index(gen_root):
    """{(level, entry): (file, first line, last line, lines, name)}; level 0 = a Veldin-address function."""
    import gen_common
    d = os.path.join(gen_root, 'game', 'ovl')
    out = {}
    for f in sorted(os.listdir(d)):
        if not f.endswith('.cpp'):
            continue
        t = open(os.path.join(d, f)).read()
        L = t.split('\n')
        for s, e, nm in gen_common.blocks(t):
            for x in L[s:e]:
                m = ORIG.search(x)
                if m:
                    o = m.group(1)
                    if o.startswith('front:'):
                        break
                    lv = int(o[1:o.index(':')]) if o.startswith('L') else 0
                    out[(lv, int(o.split('0x')[1], 16))] = (f, s, e, L, nm)
                    break
    return out


def handwritten_index():
    """{(level, entry): [file:line]} of the `// orig:` lines of src/game's definitions (every L<n>:0x.. of the line)."""
    out = {}
    for root, _d, fs in os.walk(os.path.join(W, 'src', 'game')):
        for f in fs:
            if not f.endswith('.cpp'):
                continue
            p = os.path.join(root, f)
            for k, x in enumerate(open(p, errors='ignore').read().split('\n')):
                if x.startswith('// orig:'):
                    for lv, a in re.findall(r'(?:L(\d+):)?0x([0-9a-f]{6,8})', x[8:].split('(')[0]):
                        out.setdefault((int(lv) if lv else 0, int(a, 16)), []).append('%s:%d' % (os.path.relpath(p, W), k + 1))
    return out


def port_body(ref):
    """The text of the hand-written definition at file:line (to its closing brace at column 0)."""
    f, ln = ref.rsplit(':', 1)
    L = open(os.path.join(W, f), errors='ignore').read().split('\n')
    k = int(ln)
    out = []
    while k < len(L):
        out.append(L[k])
        if L[k] == '}':
            break
        k += 1
    return '\n'.join(out)


_starts = {}
_maps = {}


def level_map(level):
    """{level address: Veldin address}: the overlay map's `fn` rows and the level's variants table (src/game/ovl/
    variants_l<n>.cpp), the pairs by which the generator glues a level's copy to a Veldin port (mkall.py's fnmap)."""
    if level not in _maps:
        import ovlist
        out = {}
        mp = os.path.join(ovlist.map_dir(), 'level%d.txt' % level)
        for l in open(mp) if os.path.exists(mp) else []:
            p = l.split()
            if p and p[0] == 'fn':
                out[int(p[2], 16)] = int(p[1], 16)
        vf = os.path.join(W, 'src', 'game', 'ovl', 'variants_l%d.cpp' % level)
        if os.path.exists(vf):
            for m in re.finditer(r'\{0x([0-9a-f]+), 0x([0-9a-f]+), 0x([0-9a-f]+)\}', open(vf).read()):
                out[int(m.group(2), 16)] = int(m.group(1), 16)
        _maps[level] = out
    return _maps[level]


def owner(level, addr):
    """(entry, end) of the function of level `level` holding `addr`, or None."""
    import ovlist
    from retire import func_range  # (the dropped functions' extents of retire.py's hook check)
    if level not in _starts:
        _starts[level] = sorted(ovlist.overlay(level).function_starts())
    s = _starts[level]
    i = bisect.bisect_right(s, addr) - 1
    if i < 0:
        return None
    e, end, _ls = func_range(level, s[i])
    return (e, end) if e <= addr < end else None


def pal_entries(level):
    """[(PAL address, NTSC-U entry)] of the functions the PAL 1.00 build changed on a level (`entry` rows)."""
    p = os.path.join(W, 'data', 'regions', 'SCES-50916.txt')
    out, u = [], None
    for line in open(p) if os.path.exists(p) else []:
        q = line.split()
        if not q:
            continue
        if q[0] == 'overlay':
            u = q[1]
        elif q[0] == 'entry' and u == str(level):
            out.append((int(q[2], 16), int(q[1], 16)))
    return sorted(out)


def unit_files():
    """{glue file: [generated files]} of the units generate_all.py generates (mkall json `files`), plus the top-level glue
    files (every generated file)."""
    spec = os.path.join(HERE, 'gen', 'mkall')
    listed = None
    ul = os.path.join(HERE, 'gen', 'units.txt')
    if os.path.exists(ul):
        listed = {l.strip() for l in open(ul) if l.strip() and not l.startswith('#')}
    out = {}
    for f in sorted(os.listdir(spec)):
        if f.endswith('.json') and (listed is None or 'mkall:' + f[:-5] in listed):
            g = os.path.join(spec, f[:-5] + '.glue')
            if os.path.exists(g):
                out[g] = [r[len('src/'):] for r in json.load(open(os.path.join(spec, f))).get('files', [])]
    for f in sorted(os.listdir(HERE)):
        if f.endswith('.glue') and f != 'hooks.glue':
            out[os.path.join(HERE, f)] = None
    return out


def main():
    a = sys.argv[1:]
    gen_root = os.path.join(W, 'assets', 'generated', 'src')
    if '--gen-root' in a:
        gen_root = os.path.abspath(a[a.index('--gen-root') + 1])
        if not os.path.isdir(os.path.join(gen_root, 'game')) and os.path.isdir(os.path.join(gen_root, 'src', 'game')):
            gen_root = os.path.join(gen_root, 'src')
    only = int(a[a.index('--level') + 1]) if '--level' in a else None
    hooks_file = os.path.abspath(a[a.index('--hooks') + 1]) if '--hooks' in a else os.path.join(HERE, 'hooks.glue')
    strict, listing = '--strict' in a, '--list' in a

    names = enhancement_names()
    called = re.compile(r'\b(%s)\s*\(' % '|'.join(sorted(names, key=len, reverse=True)))
    gen = generated_index(gen_root)
    hand = handwritten_index()
    errors, warnings, ok = [], [], []

    # ---- hooks.glue
    section = ''
    for n, line in enumerate(open(hooks_file), 1):
        line = line.rstrip('\n')
        if line.startswith('# ----'):
            section = line[6:].strip()
        m = HOOK.match(line)
        if not m:
            if line.strip() and not line.lstrip().startswith('#'):
                errors.append('hooks.glue:%d: not a hook line: %s' % (n, line))
            continue
        lv, pal, ad, stmt, is_label = int(m.group(1)), bool(m.group(2)), int(m.group(3), 16), m.group(4), bool(m.group(5))
        if only is not None and lv != only:
            continue
        where = 'hooks.glue:%d L%d %s0x%x' % (n, lv, 'PAL:' if pal else '', ad)
        if pal:
            ents = pal_entries(lv)
            i = bisect.bisect_right([p for p, _e in ents], ad) - 1
            if i < 0:
                errors.append('%s: no PAL 1.00 changed function holds it (data/regions/SCES-50916.txt)' % where)
                continue
            entry = ents[i][1]
        else:
            o = owner(lv, ad)
            if o is None:
                errors.append('%s: not inside a function of level %d\'s code' % (where, lv))
                continue
            entry = o[0]
        g = gen.get((lv, entry))
        fn_calls = set(called.findall(stmt or ''))
        if g is None:
            ports, via = hand.get((lv, entry), []), ''
            if not ports and lv:
                v = level_map(lv).get(entry)
                if v is not None and hand.get((0, v)):
                    ports, via = hand[(0, v)], ' (the level\'s copy runs the Veldin port 0x%x: overlay map / variants table)' % v
            if ports and is_label:
                errors.append('%s: DANGLING: a label in L%d:0x%x, hand-written (%s%s): remove it with its hook lines' % (
                    where, lv, entry, ports[0], via))
            elif ports:
                bodies = '\n'.join(port_body(x) for x in ports)
                has = bool(fn_calls) and all(re.search(r'\b%s\s*\(' % re.escape(c), bodies) for c in fn_calls)
                errors.append('%s: DANGLING: L%d:0x%x is hand-written (%s%s), not generated: the hook is never placed; %s' % (
                    where, lv, entry, ports[0], via, ('the port calls %s: replace the line by a note of where the fork '
                                                      'lives' % ', '.join(sorted(fn_calls))) if has else
                    'the port\'s definitions do not call %s: carry the fork into the port (or show it is in a callee)' %
                    (', '.join(sorted(fn_calls)) or 'the hook\'s code')))
            else:
                errors.append('%s: DANGLING: L%d:0x%x is not a generated function (and no hand-written one names it)' % (
                    where, lv, entry))
            continue
        f, s, e, L, nm = g
        if pal:
            ok.append((lv, ad, entry, nm, f, None, stmt, section, True))
            continue
        want = norm(stmt) if stmt else 'L_%x:' % ad
        hit = next((k + 1 for k in range(s, e) if want in norm(L[k])), None)
        if hit is None:
            errors.append('%s: NOT APPLIED: %s (%s) is generated but %s is not in its body' % (
                where, nm, f, 'the statement' if stmt else 'the label'))
            continue
        ok.append((lv, ad, entry, nm, f, hit, stmt, section, False))
        ports = hand.get((lv, entry), [])
        if ports and stmt and fn_calls:
            body = port_body(ports[0])
            miss = sorted(c for c in fn_calls if not re.search(r'\b%s\s*\(' % re.escape(c), body))
            if miss:
                warnings.append('%s: %s is still generated (the hook applies), but its hand-written port %s does not call '
                                '%s: the fork is lost when level %d is glued to the port' % (where, nm, ports[0],
                                                                                           ', '.join(miss), lv))

    # ---- the units' glue
    if only is None:
        gdir = os.path.join(gen_root, 'game', 'ovl')
        texts = {}

        def text(r):
            if r not in texts:
                p = os.path.join(gen_root, r)
                texts[r] = re.sub(r'\s+', '', open(p).read()) if os.path.exists(p) else ''
            return texts[r]
        for g, files in unit_files().items():
            files = files if files is not None else ['game/ovl/' + x for x in os.listdir(gdir) if x.endswith('.cpp')]
            for n, line in enumerate(open(g), 1):
                m = re.match(r'^(0x[0-9a-fA-F]+):\s*(.*)$', line.rstrip('\n'))
                if not m or not called.search(m.group(2)):
                    continue
                want = norm(m.group(2))
                if not any(want in text(r) for r in files):
                    errors.append('%s:%d: DEAD: the enhancement glue of callee %s is in none of the unit\'s generated files '
                                  '(its call sites are hand-written now: they must call %s themselves)' % (
                                      os.path.relpath(g, W), n, m.group(1), ', '.join(sorted(set(called.findall(m.group(2)))))))
        # ---- the skeletons' own enhancement lines
        sk = os.path.join(HERE, 'gen', 'skel')
        for root, _d, fs in os.walk(sk):
            for f in fs:
                if not f.endswith('.skel'):
                    continue
                p = os.path.join(root, f)
                r = os.path.relpath(p, sk)[:-len('.skel')]
                for n, line in enumerate(open(p), 1):
                    if '#include' in line or line.lstrip().startswith('//') or not called.search(line):
                        continue
                    if norm(line) not in text(r):
                        errors.append('%s:%d: the skeleton\'s enhancement line is not in %s' % (os.path.relpath(p, W), n, r))

    # ---- report
    if listing:
        lv0 = None
        for lv, ad, entry, nm, f, hit, stmt, section, pal in sorted(ok, key=lambda x: (x[0], x[2], x[1])):
            if lv != lv0:
                print('== L%d' % lv)
                lv0 = lv
            print('  %s0x%06x  %s (0x%06x)  %s%s  [%s]  %s' % ('PAL:' if pal else '', ad, nm, entry, f,
                                                              ':%d' % hit if hit else ' (PAL 1.00 stand-in)',
                                                              section[:60], (stmt or 'label')[:90]))
    for w in warnings:
        print(('ERROR: ' if strict else 'WARNING: ') + w)
    for x in errors:
        print('ERROR: ' + x)
    print('check_hooks: %d hooks.glue lines placed in generated code, %d warning(s), %d error(s)' % (
        len(ok), len(warnings), len(errors) + (len(warnings) if strict else 0)))
    return 1 if errors or (strict and warnings) else 0


if __name__ == '__main__':
    sys.exit(main())
