#!/usr/bin/env python3
# usage: gen_asm2cpp.py list | gen <name> [--out ROOT] [--no-insn-comments] | freeze <src path> [--level N]
#
# The hand-written EE / VU0 macro-mode routines transliterated with tools/codegen/asm2cpp.py and then reviewed (the moby skinning, the drop
# shadows, the world passes' culling and lighting), generated whole from the ISO and the committed inputs:
#   tools/codegen/gen/asm2cpp/<name>.json   the file, the level, its fragments: each a routine (`addr`), its `jr` targets (a jump table of
#                                      its own), `gp_fold` (false: the routine uses $gp as a general register) and `patches`, the
#                                      review's edits keyed by instruction address ({addr: {"replace": [lines], "before": [...],
#                                      "after": [...]}}: asm2cpp.translate) -- ours: host notes, spin loops the port does not need, ...
#   tools/codegen/gen/skel/<path>.skel      the file with each fragment's statements replaced by a `//@asm <fragment>` line: the
#                                      hand-written C++ around them (the wrappers, the compiled functions ported by hand)
# `freeze` writes both from a file that carries its instruction comments (generated without --no-insn-comments): every function whose body carries instruction comments and an
# `// orig: 0x...` is a fragment; what the review changed becomes its patches (the instruction comments left out of them).
import difflib
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
W = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
import asm2cpp  # noqa: E402
import gen_common  # noqa: E402
import ovlist  # noqa: E402

SPEC = os.path.join(HERE, 'gen', 'asm2cpp')
SKEL = os.path.join(HERE, 'gen', 'skel')
MARK = '//@asm '


def names():
    return sorted(os.path.splitext(f)[0] for f in os.listdir(SPEC) if f.endswith('.json'))


def skel_path(rel):
    return os.path.join(SKEL, rel[len('src/'):] + '.skel')


def listing(level, addr):
    return asm2cpp.parse_lines(ovlist.listing(level, addr))


def fragment(d, f):
    ins = listing(f.get('level', d.get('level', 0)), int(f['addr'], 16))
    patches = {int(k, 16): v for k, v in f.get('patches', {}).items()}
    return asm2cpp.translate(ins, [int(x, 16) for x in f.get('jr', [])], None, f.get('gp_fold', True), patches or None)


def cmd_gen(name, out_root, strip):
    d = json.load(open(os.path.join(SPEC, name + '.json')))
    frags = {f['name']: f for f in d['fragments']}
    out = []
    used = set()
    for l in open(skel_path(d['file'])).read().split('\n'):
        if l.startswith(MARK):
            nm = l[len(MARK):].strip()
            out.append(fragment(d, frags[nm]))
            used.add(nm)
        else:
            out.append(l)
    missing = set(frags) - used
    if missing:
        sys.exit('%s: fragments the skeleton does not place: %s' % (name, ' '.join(sorted(missing))))
    text = '\n'.join(out)
    if strip:
        text = gen_common.strip_insn_comments(text)
    p = os.path.join(out_root, d['file'])
    os.makedirs(os.path.dirname(p), exist_ok=True)
    gen_common.write_if_changed(p, text)
    return d['file']


def _code(l):
    return re.sub(r'\s*//[^"]*$', '', l).rstrip()


def freeze(rel, level):
    t = open(os.path.join(W, rel)).read()
    L = t.split('\n')
    skel = []
    frags = []
    pos = 0
    for s, e, nm in gen_common.blocks(t):
        body = L[s:e]
        ninsn = sum(1 for x in body if '//' in x and gen_common.is_insn_comment(x.split('//', 1)[1].split('//')[0]))
        mo = next((re.match(r'^// orig: (?:L\d+:)?0x([0-9a-f]+)', x) for x in body if x.startswith('// orig: ')), None)
        if ninsn < 10 or not mo:
            continue
        ad = int(mo.group(1), 16)
        jr = []
        for x in body:
            m = re.match(r'^\s+case 0x([0-9a-f]{8}): goto L_', x)
            if m and m.group(1) not in jr:
                jr.append(m.group(1))
        gp_fold = not any(re.search(r'\bgp\.w\(\) \+ ', x) for x in body)
        ins = listing(level, ad)
        tl = asm2cpp.translate(ins, [int(x, 16) for x in jr], None, gp_fold, 'tags')
        G = [x for _, x in tl]
        tags = [a for a, _ in tl]
        # the body's lines (from the line after the signature to the line before the closing brace) against the fragment
        sig = next(k for k in range(s, e) if gen_common.DEF.match(L[k]))
        C = L[sig + 1:e - 1]
        sm = difflib.SequenceMatcher(None, [_code(x) for x in C], [_code(x) for x in G], autojunk=False)
        ops = sm.get_opcodes()
        eqs = [o for o in ops if o[0] == 'equal']
        if not eqs:
            continue
        c0, c1 = eqs[0][1], eqs[-1][2]  # the committed range of the fragment (its first / last matching line)
        g0, g1 = eqs[0][3], eqs[-1][4]
        cmap = {}
        for tag, i1, i2, j1, j2 in eqs:
            for k in range(j2 - j1):
                cmap[j1 + k] = i1 + k
        dirty = set()
        for j in list(range(0, g0)) + list(range(g1, len(G))):
            dirty.add(tags[j])  # (generated lines before / after the fragment's matched range: the review dropped them)
        for tag, i1, i2, j1, j2 in ops:
            if tag == 'equal' or i2 <= c0 and j2 <= g0 or i1 >= c1 and j1 >= g1:
                continue
            for j in range(j1, j2):
                dirty.add(tags[j])
            if j1 == j2:  # an insertion: the instruction after it (or before, at the end)
                dirty.add(tags[j1] if j1 < len(G) else tags[-1])
        patches = {}
        j = 0
        while j < len(G):
            if tags[j] not in dirty:
                j += 1
                continue
            j0 = j
            while j < len(G) and tags[j] in dirty:
                j += 1
            # the committed lines between the clean neighbours (the lines outside the dirty runs are the equal blocks' own)
            lo = cmap[j0 - 1] + 1 if j0 > 0 else c0
            hi = cmap[j] if j < len(G) else c1
            # (the instruction comments left out; the review's own comment lines kept, and a block's `{  // <branch>`: the lint takes a
            # line followed by a lone `{` for a function definition)
            rep = [x if x.strip().startswith('//') or x.split('//')[0].strip() == '{' else gen_common.strip_insn_comments(x) for x in C[lo:hi]]
            run_tags = []
            for k in range(j0, j):
                if tags[k] not in run_tags:
                    run_tags.append(tags[k])
            patches['%x' % run_tags[0]] = {'replace': rep}
            for a in run_tags[1:]:
                patches['%x' % a] = {'replace': []}
        f = {'name': nm, 'addr': '%x' % ad}
        if jr:
            f['jr'] = jr
        if not gp_fold:
            f['gp_fold'] = False
        if patches:
            f['patches'] = patches
        frags.append(f)
        skel.extend(L[pos:sig + 1 + c0])
        skel.append(MARK + nm)
        pos = sig + 1 + c1
    skel.extend(L[pos:])
    name = os.path.splitext(os.path.basename(rel))[0]
    os.makedirs(SPEC, exist_ok=True)
    json.dump({'file': rel, 'level': level, 'fragments': frags}, open(os.path.join(SPEC, name + '.json'), 'w'), indent=1)
    p = skel_path(rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, 'w').write('\n'.join(skel))
    print('froze %s: %d fragments, %d patched instructions' % (rel, len(frags), sum(len(f.get('patches', {})) for f in frags)))


def main():
    a = sys.argv[1:]
    if not a or a[0] == 'list':
        print('\n'.join(names()))
        return
    opt = lambda k, dflt=None: a[a.index(k) + 1] if k in a else dflt
    if a[0] == 'gen':
        print(cmd_gen(a[1], opt('--out', W), '--no-insn-comments' in a))
    elif a[0] == 'freeze':
        freeze(a[1], int(opt('--level', '0')))


if __name__ == '__main__':
    main()
