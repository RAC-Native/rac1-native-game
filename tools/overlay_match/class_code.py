#!/usr/bin/env python3
"""A class id's code on two levels compared (the open engine, docs/open-engine.md: "A world's objects"): is a class a world's
section places as the base level's (the same id) the planet's own? The pool says whether their blocks are equal (pool.idx's
hashes); this compares their code on the disc's own MIPS: each level's overlay read from the ISO (overlay_match.py's reader),
the class's update function (its pool row) and every function it calls or names, followed: the instruction words with their
relocated fields masked must be equal, and each relocated field must name the same thing (the base executable's address, the
engine's data or function - Veldin's, through the overlay map and its extents -, the same label of the function, the same
function of the level's own - compared in turn -, own data: words that differ and are not code addresses listed). An engine
function one level's map pairs with Veldin's and the other's does not (a function the build compiled otherwise) is compared
as code too.

Usage (repo root, after the setup and the pool: `rac1-native --setup --pool`):
  tools/overlay_match/class_code.py <levelA> <levelB> [class...]   (no class: every class id of both, a summary)
  [--iso assets/iso/rac1-ntsc-u.iso]
"""
import bisect
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import overlay_match as om  # noqa: E402

MAPS = ROOT / "assets/generated/overlay_map"
POOL = ROOT / "assets/generated/pool"
BASE = 0x15EF00


class Lv:
    def __init__(self, iso, n):
        self.n = n
        self.o = om.Overlay(*om.level_overlay(iso, n))
        self.starts = self.o.function_starts()
        self.fmap, self.ext = {}, []
        for l in (MAPS / f"level{n}.txt").read_text().splitlines():
            p = l.split()
            if p and p[0] == "fn":
                self.fmap[int(p[2], 16)] = int(p[1], 16)
        for l in (MAPS / f"level{n}.ext").read_text().splitlines():
            p = l.split()
            if p and p[0] == "ext":
                self.ext.append((int(p[2], 16), int(p[1], 16), int(p[3], 16)))

    def end(self, a):
        i = bisect.bisect_right(self.starts, a)
        return self.starts[i] if i < len(self.starts) else self.o.code_end

    def word(self, a):
        return self.o.code[(a - self.o.code_base) // 4]

    def classify(self, a, fn):
        """what address `a` is, seen from function `fn`"""
        if a < BASE:
            return ("base", a)
        for l, v, s in self.ext:
            if l <= a < l + s:
                return ("eng", v + a - l)
        if a in self.fmap:
            return ("engfn", self.fmap[a])
        if self.o.in_code(a):
            if fn <= a < self.end(fn):
                return ("label", a - fn)
            if a in self.starts:
                return ("ownfn", a)
            return ("owncode", a)
        return ("owndata", a)


def compare(A, B, a, b, seen, out, depth=0):
    if (a, b) in seen:
        return True
    seen.add((a, b))
    pad = "  " * depth
    if A.fmap.get(a) is not None and A.fmap.get(a) == B.fmap.get(b):
        out.append(f"{pad}same: the engine's function L{A.n}:{a:x} / L{B.n}:{b:x} -> Veldin {A.fmap[a]:x}")
        return True
    if A.fmap.get(a) is not None or B.fmap.get(b) is not None:
        out.append(f"{pad}(L{A.n}:{a:x} -> Veldin {A.fmap.get(a, 0):x} / L{B.n}:{b:x} -> Veldin {B.fmap.get(b, 0):x}: one not paired, their code compared)")
    na, nb = (A.end(a) - a) // 4, (B.end(b) - b) // 4
    while na > 1 and A.word(a + 4 * (na - 1)) == 0 and A.word(a + 4 * (na - 2)) == 0:  # (the alignment's nops after jr ra's slot)
        na -= 1
    while nb > 1 and B.word(b + 4 * (nb - 1)) == 0 and B.word(b + 4 * (nb - 2)) == 0:
        nb -= 1
    if na != nb:
        # (a function start one level's heuristics missed: equal up to the shorter one's end, a jr ra and its slot)
        n = min(na, nb)
        short = A if na < nb else B
        st = a if na < nb else b
        if n < 2 or short.word(st + 4 * (n - 2)) != 0x03E00008:
            out.append(f"{pad}DIFFERENT L{A.n}:{a:x} ({na} words) / L{B.n}:{b:x} ({nb} words): other sizes")
            return False
        na = nb = n
    sub = []
    hia, hib = {}, {}
    owns = []
    for i in range(na):
        wa, wb = A.word(a + 4 * i), B.word(b + 4 * i)
        if om.mask(wa) != om.mask(wb):
            out.append(f"{pad}DIFFERENT L{A.n}:{a:x} / L{B.n}:{b:x}: word {i} {wa:08x} / {wb:08x}")
            return False
        op, rs, rt = wa >> 26, (wa >> 21) & 31, (wa >> 16) & 31
        if op == 3 or op == 2:
            ta, tb = (wa & 0x3FFFFFF) << 2, (wb & 0x3FFFFFF) << 2
            ca, cb = A.classify(ta, a), B.classify(tb, b)
            if ca[0] in ("ownfn", "engfn") and cb[0] in ("ownfn", "engfn") and ca != cb:
                sub.append((ta, tb))
            elif ca != cb:
                out.append(f"{pad}DIFFERENT L{A.n}:{a:x} / L{B.n}:{b:x}: word {i} {'jal' if op == 3 else 'j'} {ca} / {cb}")
                return False
            continue
        if op == 0x0F:
            hia[rt], hib[rt] = (wa & 0xFFFF) << 16, (wb & 0xFFFF) << 16
            continue
        full = None
        if (op in om.MEM or op in om.ADDI) and rs != om.SP:
            if rs == om.GP:
                full = ((om.GP_VALUE + om.sx16(wa)) & 0xFFFFFFFF, (om.GP_VALUE + om.sx16(wb)) & 0xFFFFFFFF)
            elif rs in hia and rs in hib:
                full = ((hia[rs] + om.sx16(wa)) & 0xFFFFFFFF, (hib[rs] + om.sx16(wb)) & 0xFFFFFFFF)
            elif om.sx16(wa) != om.sx16(wb):
                out.append(f"{pad}DIFFERENT L{A.n}:{a:x} / L{B.n}:{b:x}: word {i} offsets {wa & 0xffff:x} / {wb & 0xffff:x}")
                return False
        if full is not None:
            ca, cb = A.classify(full[0], a), B.classify(full[1], b)
            if ca[0] in ("ownfn",) and cb[0] in ("ownfn",):
                sub.append((full[0], full[1]))
            elif ca[0] == "owndata" and cb[0] == "owndata":
                owns.append((full[0], full[1]))
            elif ca != cb:
                out.append(f"{pad}DIFFERENT L{A.n}:{a:x} / L{B.n}:{b:x}: word {i} address {ca[0]} {ca[1]:x} / {cb[0]} {cb[1]:x}")
                return False
        d = om.gpr_written(wa)
        if d is not None and op != 0x0F:
            hia.pop(d, None)
            hib.pop(d, None)
    out.append(f"{pad}same L{A.n}:{a:x} / L{B.n}:{b:x} ({na} words){', own data ' + str(len(set(owns))) if owns else ''}")
    for x, y in sorted(set(owns)):
        va, vb = A.o.data_word(x & ~3), B.o.data_word(y & ~3)
        if va is not None and vb is not None and va != vb and not (A.o.in_code(va) and B.o.in_code(vb)):
            out.append(f"{pad}  own data L{A.n}:{x:x} = {va:08x} / L{B.n}:{y:x} = {vb:08x}")
    ok = True
    for x, y in sub:
        ok = compare(A, B, x, y, seen, out, depth + 1) and ok
    return ok


def rows():
    r = {}
    for l in (POOL / "pool.idx").read_text().splitlines():
        p = l.split()
        if p and p[0] == "class":
            r[(int(p[1]), int(p[2]))] = p
    return r


def main():
    args = sys.argv[1:]
    iso_path = ROOT / "assets/iso/rac1-ntsc-u.iso"
    if "--iso" in args:
        k = args.index("--iso")
        iso_path = Path(args[k + 1])
        del args[k:k + 2]
    if len(args) < 2:
        raise SystemExit(__doc__)
    la, lb = int(args[0]), int(args[1])
    with open(iso_path, "rb") as iso:
        A, B = Lv(iso, la), Lv(iso, lb)
    R = rows()
    cls = [int(c) for c in args[2:]] or sorted(c for (l, c) in R if l == la and (lb, c) in R)
    verbose = len(args) > 2
    same, diff, none = [], [], []
    for c in cls:
        if (la, c) not in R or (lb, c) not in R:
            print(f"class {c}: not a class of both levels")
            continue
        ra, rb = R[(la, c)], R[(lb, c)]
        ua, ub = int(ra[6], 16), int(rb[6], 16)
        blk = "same block" if ra[5] == rb[5] else "OTHER BLOCKS"
        if not ua or not ub:
            none.append(c)
            continue
        out = []
        ok = compare(A, B, ua, ub, set(), out)
        (same if ok else diff).append(c)
        if verbose or not ok:
            print(f"class {c}: {blk}; code {'SAME' if ok else 'DIFFERENT'}")
            for l in out[:40 if verbose else 6]:
                print("  " + l)
    print(f"L{la} vs L{lb}: {len(cls)} classes; code the same {len(same)}, different {len(diff)} {diff}, no update function {len(none)}")


main()
