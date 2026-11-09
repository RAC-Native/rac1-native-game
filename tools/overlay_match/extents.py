#!/usr/bin/env python3
"""The extents of the engine's data objects in each level's overlay (the open engine's stage 1, docs/open-engine.md).

The overlay maps (overlay_match.py) pair the data addresses Veldin's matched code names with level N's. An address
between two mapped ones is an offset into an object, but which object, and is it the engine's at all? Today's rule
(ovl_reloc.cpp) takes the mapped address below when the one above moved by the same amount, else guesses within 0x100;
a level's own data (its own classes' tables and statics, linked between the engine's objects) is "not within 0x100 of a
mapped address". Engine arrays far past their mapped base (the class id -> index map, 0x1ec0 bytes; the moby texture
copy...) fall out of both rules.

This tool gives each block of engine data its extent:
  - a block: consecutive mapped addresses (in Veldin's order) that moved by the same displacement, contiguous in the
    level too (a lone pair whose displacement differs from both neighbours that agree with each other, and a "page" pair
    - lui + an offset under 0x100, the same number in both builds - where the data below has moved, are values both
    builds load, not addresses: left out);
  - its tail past its last mapped address: up to the next mapped address, in Veldin and in the level (whichever is
    nearer), the section's end, and the first address there that the other unit's own code names: in the level, code
    that is neither matched to Veldin's (the map's fn lines) nor one of the level's hand-checked variants
    (src/game/ovl/variants_l<N>.cpp); in Veldin, code that no level's map matches (Veldin's own); and data pointers of
    the unit's own data. An address only own code names starts an own object; engine data does not run past it.

Output (gitignored, generated from your ISO): <out>/levelN.ext, one line per block:
  ext <veldin> <levelN> <size>    the block [veldin, veldin + size) is [levelN, levelN + size) in level N
and a summary. The game reads it beside levelN.txt (ovl_reloc.cpp): an address inside a block is the engine's (translated
with the block's displacement), any other address of the level's data sections is the level's own.

Usage (repo root):
  tools/overlay_match/extents.py [--iso assets/iso/rac1-ntsc-u.iso] [--maps assets/generated/overlay_map] [--out <dir>]
                                 [--levels 1,16]
  tools/overlay_match/extents.py --levels 16 --check <RAC1_RAMDUMP's file>   (the check: what the extents call own)
"""
import argparse
import bisect
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from overlay_match import (ADDI, ADDI_TRAP, GP, MEM, ROOT, SP, VELDIN, Overlay, gpr_written, level_overlay,  # noqa: E402
                           sx16)

DATA_LO = 0x15EF00  # the overlays' first address (section 0)


def read_map(path):
    """A map's fn lines {veldin: (level, size)} and data lines {veldin: level}."""
    fns, data = {}, {}
    for line in path.read_text().splitlines():
        p = line.split()
        if len(p) >= 4 and p[0] == "fn":
            fns[int(p[1], 16)] = (int(p[2], 16), int(p[3], 16))
        elif len(p) >= 3 and p[0] == "data":
            data[int(p[1], 16)] = int(p[2], 16)
    return fns, data


def variant_rows(level):
    """The level's hand-checked variants (src/game/ovl/variants_l<N>.cpp): [(level address, size)]."""
    f = ROOT / f"src/game/ovl/variants_l{level}.cpp"
    if not f.exists():
        return []
    return [(int(l, 16), int(n, 16)) for _, l, n in re.findall(r"\{\s*0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+)\s*\}",
                                                               f.read_text())]


class Ranges:
    """Sorted [lo, hi) ranges: membership."""

    def __init__(self, ranges):
        self.r = sorted(ranges)
        self.lo = [a for a, _ in self.r]

    def __contains__(self, a):
        i = bisect.bisect_right(self.lo, a) - 1
        return i >= 0 and a < self.r[i][1]


def sections(U):
    """The data sections' [lo, hi) (bss by its size word: the section's bytes, or the gap to the next one)."""
    out = []
    secs = U.secs[:6]
    for k, (dest, typ, b) in enumerate(secs):
        hi = dest + len(b)
        if hi <= dest:  # (a section without contents: up to the next one)
            nxt = [d for d, _, _ in U.secs if d > dest]
            hi = min(nxt) if nxt else dest
        out.append((dest, hi))
    return out


def code_refs(U, own):
    """The data addresses the unit's code builds (lui + addiu / a load's or store's offset; $gp offsets) in the code
    `own` holds (a predicate on the instruction's address). A plain scan: the registers' upper halves are forgotten at a
    function's end (jr ra) and when written."""
    refs = set()
    hi = {}
    for i, w in enumerate(U.code):
        pc = U.code_base + 4 * i
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op == 0x0F:
            hi[rt] = (w & 0xFFFF) << 16
            continue
        if (op in MEM or op in ADDI or op in ADDI_TRAP) and rs != SP:
            a = None
            if rs == GP:
                a = (U.gp + sx16(w)) & 0xFFFFFFFF
            elif rs in hi:
                a = (hi[rs] + sx16(w)) & 0xFFFFFFFF
            if a is not None and DATA_LO <= a < U.code_base and own(pc):
                refs.add(a)
            if a is not None and (op in ADDI or op in ADDI_TRAP):
                hi[rt] = a
                continue
        if w == 0x03E00008:
            hi = {}
            continue
        d = gpr_written(w)
        if d is not None:
            hi.pop(d, None)
    return refs


def data_refs(U, own_data):
    """The data addresses the unit's data words point at, from the words `own_data` (a predicate) holds."""
    refs = set()
    for dest, typ, b in U.secs[:6]:
        if typ != 1:
            continue
        for off in range(0, len(b) - 3, 4):
            w = struct.unpack_from("<I", b, off)[0]
            if DATA_LO <= w < U.code_base and own_data(dest + off):
                refs.add(w)
    return refs


def blocks(data):
    """The map's data pairs as blocks [(veldin first, level first, veldin last)] of one displacement, contiguous in both."""
    # (a page value: lui + a small offset, the same number in both builds - a constant, not an address - where the data
    # around has moved)
    page = {v for v, l in data.items() if v == l and (v & 0xFFFF) < 0x100}
    ks = sorted(data)
    real = [v for v in ks if v not in page]
    for v in sorted(page):
        i = bisect.bisect_left(real, v) - 1
        if i < 0 or data[real[i]] == real[i]:
            real.insert(i + 1, v)  # (no moved data below it: kept)
    ks = real
    disp = [data[v] - v for v in ks]
    keep = []
    for i, v in enumerate(ks):  # (a lone pair between two that agree: a value both builds load)
        if 0 < i < len(ks) - 1 and disp[i - 1] == disp[i + 1] and disp[i] != disp[i - 1]:
            continue
        keep.append(v)
    out = []
    for v in keep:
        l = data[v]
        if out and l - v == out[-1][1] - out[-1][0] and l > data[out[-1][2]]:
            out[-1][2] = v
        else:
            out.append([v, l, v])
    return out


def extents(V, L, fns, data, veldin_own_fn, variants):
    """The level's blocks with their sizes [(veldin, level, size)], and the summary's counts."""
    lfn = Ranges([(l, l + n) for l, n in fns.values()] + [(l, l + n) for l, n in variants])
    lrefs = code_refs(L, lambda pc: pc not in lfn)
    vrefs = code_refs(V, lambda pc: pc in veldin_own_fn)
    bl = blocks(data)
    mapped_l = sorted(data.values())
    mapped_v = sorted(data)
    lsec, vsec = sections(L), sections(V)
    # the own data's pointers: words of the level's data outside every block's mapped span (an engine object's own
    # pointers are the engine's)
    spans = Ranges([(l, l + (vl - v) + 4) for v, l, vl in bl])
    lrefs |= data_refs(L, lambda a: a not in spans)
    lref_sorted = sorted(lrefs - set(mapped_l))
    vref_sorted = sorted(vrefs - set(mapped_v))
    out = []
    cut_l = cut_v = 0
    for v0, l0, vl in bl:
        ll = l0 + (vl - v0)
        # the tail: to the next mapped address in each unit, the section's end
        i = bisect.bisect_right(mapped_v, vl)
        tail = (mapped_v[i] if i < len(mapped_v) else V.code_base) - vl
        j = bisect.bisect_right(mapped_l, ll)
        tail = min(tail, (mapped_l[j] if j < len(mapped_l) else L.code_base) - ll)
        for lo, hi in lsec:
            if lo <= ll < hi:
                tail = min(tail, hi - ll)
        for lo, hi in vsec:
            if lo <= vl < hi:
                tail = min(tail, hi - vl)
        # an address only own code names: the end
        k = bisect.bisect_right(lref_sorted, ll)
        if k < len(lref_sorted) and lref_sorted[k] - ll < tail:
            tail = lref_sorted[k] - ll
            cut_l += 1
        k = bisect.bisect_right(vref_sorted, vl)
        if k < len(vref_sorted) and vref_sorted[k] - vl < tail:
            tail = vref_sorted[k] - vl
            cut_v += 1
        out.append((v0, l0, vl - v0 + max(tail, 1)))
    # (the engine's names: the matched code's addresses inside a block - outside every block they are values, as the page
    # pairs: lui + a small offset)
    inside = Ranges([(l, l + n) for _, l, n in out])
    names = {"own": lref_sorted, "engine": sorted(a for a in set(mapped_l) | code_refs(L, lambda pc: pc in lfn) if a in inside)}
    return out, len(lrefs), len(vrefs), cut_l, cut_v, names


def check(L, data, ext, names, dump_path, lv):
    """The data words of the level's sections that differ between its static image and a RAM dump of a frame of play
    (RAC1_RAMDUMP): those outside every block are the level's own. Counted by today's rule too (an address within 0x100
    of a mapped one is the engine's); each own run must begin inside an object the level's own code or data names (the
    nearest named address at or below it is an own name). Returns the number of runs that do not."""
    dump = Path(dump_path).read_bytes()
    mapped = sorted(data.values())
    starts = [l for _, l, _ in ext]
    own_names, eng_names = names["own"], names["engine"]

    def engine_old(a):
        i = bisect.bisect_right(mapped, a) - 1
        return i >= 0 and a - mapped[i] < 0x100

    def engine_new(a):
        i = bisect.bisect_right(starts, a) - 1
        return i >= 0 and a < ext[i][1] + ext[i][2]

    bad = 0
    for name, engine in (("today's rule", engine_old), ("the extents", engine_new)):
        own = written = 0
        runs = []
        for dest, typ, b in L.secs[:6]:
            for off in range(0, len(b) - 3, 4):
                a = dest + off
                if engine(a):
                    continue
                own += 4
                img = struct.unpack_from("<I", b, off)[0] if typ == 1 else 0
                if struct.unpack_from("<I", dump, a)[0] != img:
                    written += 4
                    if runs and runs[-1][1] == a:
                        runs[-1][1] = a + 4
                    else:
                        runs.append([a, a + 4])
        line = f"level {lv}, {name}: own data {own / 1024:.0f} KB, written at the frame {written / 1024:.1f} KB in {len(runs)} runs"
        if engine is engine_new:
            unexplained = []
            for a, e in runs:
                i = bisect.bisect_right(own_names, a) - 1
                j = bisect.bisect_right(eng_names, a) - 1
                o = own_names[i] if i >= 0 else -1
                g = eng_names[j] if j >= 0 else -1
                if g > o:
                    unexplained.append(f"{a:08x}+{e - a:#x} (named last by the engine at {g:08x})")
            bad = len(unexplained)
            line += f"; each in an object own code or data names: {len(runs) - bad}, not: {bad}"
            for u in unexplained:
                line += "\n  " + u
        else:
            big = sorted(runs, key=lambda r: r[0] - r[1])[:6]
            line += " (largest " + ", ".join(f"{a:08x}+{e - a:#x}" for a, e in big) + ")"
        print(line)
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--iso", default=str(ROOT / "assets/iso/rac1-ntsc-u.iso"))
    ap.add_argument("--maps", default=str(ROOT / "assets/generated/overlay_map"))
    ap.add_argument("--out", default=None, help="where the .ext files go (default: beside the maps)")
    ap.add_argument("--levels", default=",".join(str(i) for i in range(1, 19)))
    ap.add_argument("--check", metavar="DUMP", help="(one level) a RAM dump of a frame of that level (RAC1_RAMDUMP): its "
                    "own data written at run time, by today's rule and by the extents (nothing is written)")
    a = ap.parse_args()
    maps = Path(a.maps)
    out = Path(a.out) if a.out else maps
    levels = [int(x) for x in a.levels.split(",") if x]
    # Veldin's own code: functions no level's map matches (read from every level's map that exists)
    matched_somewhere = set()
    for n in range(1, 19):
        p = maps / f"level{n}.txt"
        if p.exists():
            matched_somewhere |= set(read_map(p)[0])
    with open(a.iso, "rb") as iso:
        V = Overlay(*level_overlay(iso, VELDIN))
        starts = V.function_starts() + [V.code_end]
        veldin_own = Ranges([(s, e) for s, e in zip(starts, starts[1:]) if s not in matched_somewhere])
        for lv in levels:
            L = Overlay(*level_overlay(iso, lv))
            fns, data = read_map(maps / f"level{lv}.txt")
            data = {v: l for v, l in data.items() if DATA_LO <= v < V.code_base and DATA_LO <= l < L.code_base}  # (the overlay's data)
            ext, nl, nv, cl, cv, names = extents(V, L, fns, data, veldin_own, variant_rows(lv))
            if a.check:
                sys.exit(1 if check(L, data, ext, names, a.check, lv) else 0)
            total = sum(n for _, _, n in ext)
            lines = [f"# level {lv}: the extents of the engine's data blocks (tools/overlay_match/extents.py)",
                     f"# {len(ext)} blocks, {total} bytes ({total / 1024:.0f} KB); the level's own code / data names {nl} "
                     f"addresses, Veldin's own code {nv}; tails cut by them: {cl} in the level, {cv} in Veldin"]
            lines += [f"ext {v:08x} {l:08x} {n:x}" for v, l, n in ext]
            out.mkdir(parents=True, exist_ok=True)
            tmp = out / f"level{lv}.ext.tmp"
            tmp.write_text("\n".join(lines) + "\n")
            tmp.replace(out / f"level{lv}.ext")
            print(lines[1])


if __name__ == "__main__":
    main()
