#!/usr/bin/env python3
"""The sites report (the open engine, docs/open-engine.md): every level-only address the ports name, classified as a
session's context classifies it at run time (src/game/enhance/context.cpp).

Level N's own code names its addresses as `L<N>_ADDR(0x...)` (the generated code, assets/generated/src/game/ovl/, and the
hand ports, src/game/ovl/): its sites. In a session whose loaded overlay is another level's, each site of level N goes
through N's context, by what the address is in N's overlay:
  base         below the overlays (0x15ef00): the same on every level
  engine-data  inside an engine object of N's map (its extents, overlay_map/levelN.ext): Veldin's object
  own-data     N's own data (its data sections, in no engine object): N's context image
  engine-code  the start of a function of N that N's map pairs with Veldin's: Veldin's function (unless N has a port of
               its own)
  own-code     N's own code, or a label inside a paired function (a jump table's entry): a tag
  outside      past N's code (its run-time layout): kept
The report also lists the jump tables the generated code reads (an address loaded just before a `switch (lvl_key(...))`,
with its case count): their entries are own-code words of N's data, which the context's relocation must tag.

Output (generated: never in the repository): <out>/levelNN.txt, one line per site
  site <address> <kind> <Veldin's address or -> <uses> <files>
  jump <address> <entries> <file>
and a summary. --check-rel <pool>/context/levelNN.rel (written by the game when it makes level N's context) checks that
every jump table's entries are among the relocated own-code words.

Usage (repo root):
  tools/overlay_match/sites.py [--levels 3,16] [--out assets/generated/pool/sites] [--check-rel]
"""
import argparse
import bisect
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAPS = ROOT / "assets/generated/overlay_map"
BASE = 0x15EF00
SITE = re.compile(r"\bL(\d+)_ADDR\(0x([0-9a-fA-F]+)u?\)")
LVL = re.compile(r"\bLVL_ADDR\((\d+),\s*0x([0-9a-fA-F]+)u?\)")


class Level:
    def __init__(self, n):
        self.n = n
        text = (MAPS / f"level{n}.txt").read_text()
        m = re.search(r"level \d+ code ([0-9a-f]+)\.\.([0-9a-f]+)", text)
        self.code = (int(m.group(1), 16), int(m.group(2), 16))
        fns = []
        for line in text.splitlines():
            p = line.split()
            if len(p) >= 4 and p[0] == "fn":
                fns.append((int(p[2], 16), int(p[1], 16), int(p[3], 16)))
        vf = ROOT / f"src/game/ovl/variants_l{n}.cpp"
        if vf.exists():
            for v, l, s in re.findall(r"\{\s*0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+)\s*\}", vf.read_text()):
                fns.append((int(l, 16), int(v, 16), int(s, 16)))
        self.fns = sorted(fns)
        self.ext = []
        for line in (MAPS / f"level{n}.ext").read_text().splitlines():
            p = line.split()
            if len(p) == 4 and p[0] == "ext":
                self.ext.append((int(p[2], 16), int(p[1], 16), int(p[3], 16)))
        self.ext.sort()

    @staticmethod
    def _find(tab, a):
        i = bisect.bisect_right(tab, (a, 0xFFFFFFFF, 0)) - 1
        return tab[i] if i >= 0 and a - tab[i][0] < tab[i][2] else None

    def classify(self, a):
        if a < BASE:
            return "base", a
        if self.code[0] <= a < self.code[1]:
            f = self._find(self.fns, a)  # (a paired function's start only: inside one, a label of the level's own)
            return ("engine-code", f[1]) if f and a == f[0] else ("own-code", None)
        if a < self.code[0]:
            e = self._find(self.ext, a)
            return ("engine-data", a - e[0] + e[1]) if e else ("own-data", None)
        return "outside", None


def scan(files):
    """{level: {address: [uses, set(files)]}} and the jump tables [(level, address, entries, file)]."""
    sites = defaultdict(lambda: defaultdict(lambda: [0, set()]))
    jumps = []
    for f in files:
        lines = f.read_text(errors="replace").splitlines()
        for i, line in enumerate(lines):
            for rx in (SITE, LVL):
                for lv, a in rx.findall(line):
                    s = sites[int(lv)][int(a, 16)]
                    s[0] += 1
                    s[1].add(f.name)
            m = SITE.search(line)
            if m is None:
                continue
            # a jump table: the address, then within a few lines the switch on a word loaded from it
            for j in range(i + 1, min(i + 5, len(lines))):
                if "switch (lvl_key(" in lines[j]:
                    n = 0
                    for k in range(j + 1, len(lines)):
                        if lines[k].lstrip().startswith("case "):
                            n += 1
                        elif lines[k].lstrip().startswith("default:") or lines[k].strip() == "}":
                            break
                    jumps.append((int(m.group(1)), int(m.group(2), 16), n, f.name))
                    break
    return sites, jumps


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--levels", default=",".join(str(n) for n in range(1, 19)))
    ap.add_argument("--out", default=str(ROOT / "assets/generated/pool/sites"))
    ap.add_argument("--check-rel", action="store_true", help="check the jump tables against the contexts' .rel files")
    a = ap.parse_args()
    levels = [int(x) for x in a.levels.split(",") if x]
    files = sorted((ROOT / "assets/generated/src/game/ovl").glob("*.cpp")) + sorted((ROOT / "src/game/ovl").glob("*.[ch]*"))
    sites, jumps = scan(files)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    bad = 0
    for n in levels:
        if not (MAPS / f"level{n}.ext").exists():
            print(f"level {n}: no extents (./setup.sh): skipped")
            continue
        L = Level(n)
        counts = defaultdict(lambda: [0, 0])
        body = []
        for addr in sorted(sites[n]):
            uses, fs = sites[n][addr]
            kind, to = L.classify(addr)
            counts[kind][0] += 1
            counts[kind][1] += uses
            body.append(f"site {addr:08x} {kind} {to:08x} {uses} {','.join(sorted(fs))}" if to is not None
                        else f"site {addr:08x} {kind} - {uses} {','.join(sorted(fs))}")
        jl = sorted({(addr, ent, fn) for lv, addr, ent, fn in jumps if lv == n})
        for addr, ent, fn in jl:
            body.append(f"jump {addr:08x} {ent} {fn}")
        summary = ", ".join(f"{k} {v[0]} ({v[1]} uses)" for k, v in sorted(counts.items()))
        head = [f"# level {n}: its code's level-only sites, as a session's context classifies them (tools/overlay_match/sites.py)",
                f"# {len(sites[n])} addresses: {summary}; {len(jl)} jump tables",
                "# site <address> <kind> <Veldin's address or -> <uses> <files>; jump <address> <entries> <file>"]
        (out / f"level{n:02d}.txt").write_text("\n".join(head + body) + "\n")
        line = f"level {n}: {len(sites[n])} addresses ({summary}); {len(jl)} jump tables"
        if a.check_rel:
            rel = ROOT / f"assets/generated/pool/context/level{n:02d}.rel"
            if rel.exists():
                tagged = {int(p[0], 16) for p in (l.split() for l in rel.read_text().splitlines() if not l.startswith("#")) if len(p) == 4
                          and p[2] == "own-code"}
                want = [addr + 4 * k for addr, ent, _ in jl if L.classify(addr)[0] == "own-data" for k in range(ent)]
                missing = [w for w in want if w not in tagged]
                line += f"; jump table entries relocated as own code: {len(want) - len(missing)} of {len(want)}"
                bad += len(missing)
            else:
                line += "; no context made yet (no .rel)"
        print(line)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
