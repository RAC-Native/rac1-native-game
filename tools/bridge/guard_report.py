#!/usr/bin/env python3
"""The bridge guard's census files (RAC1_BRIDGE_GUARD=census[:<file>], src/game/bridge/guard.h), symbolized and summed.

  guard_report.py census <census files...> [--binary build/rac1-native] [--tsv <out.tsv>] [--static <by_function.tsv>]
      One row per (file, range, kind, function): the counts. Kinds: R read, W write, P a host pointer taken (V(), M(), a
      buffer); lower case inside a BridgeScope. Without --tsv the summary only: per range, the functions and their
      reads / writes / pointers summed over the files. --static compares with the stage-5 scan's by_function.tsv (one row
      per group and function) and lists the functions the census saw that the scan did not name for that group.
  guard_report.py trap <log> [--binary build/rac1-native]
      A trap's log (RAC1_BRIDGE_GUARD=trap) with its addresses named: the function the access counts for, and the
      backtrace's lines.

The functions are named from the binary's symbol table (`nm`): the census holds each caller as an offset from the anchor
function `rac1_bridge_guard_anchor`, whose address in the binary the symbol table gives. The binary must be the one the
census was made with.
"""
import argparse
import bisect
import collections
import os
import re
import struct
import subprocess
import sys

KINDS = {1: "R", 2: "W", 3: "P"}
ANCHOR = "rac1_bridge_guard_anchor"


class Symbols:
    def __init__(self, binary):
        out = subprocess.run(["nm", "-n", "-C", "--defined-only", binary], capture_output=True, text=True, check=True).stdout
        self.addrs, self.names = [], []
        self.anchor = None
        for line in out.splitlines():
            parts = line.split(" ", 2)
            if len(parts) < 3 or parts[1] not in "tTwW":
                continue
            a = int(parts[0], 16)
            self.addrs.append(a)
            self.names.append(parts[2])
            if parts[2] == ANCHOR:
                self.anchor = a
        if self.anchor is None:
            sys.exit(f"{binary}: no symbol {ANCHOR} (a binary built without the guard?)")

    def name(self, addr):
        i = bisect.bisect_right(self.addrs, addr) - 1
        return self.names[i] if i >= 0 else f"0x{addr:x}"


def short(name):
    """rac1::seg::segment_collide_v(unsigned int, ...) [clone .x] -> segment_collide_v"""
    n = re.sub(r" \[clone [^\]]*\]", "", name).replace("(anonymous namespace)::", "")
    depth, cut = 0, len(n)
    for i, c in enumerate(n):  # the parameter list: the last top-level '('
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == "(" and depth == 0:
            cut = i
    n = n[:cut]
    return n.split("::")[-1] if "<" not in n else re.sub(r"^.*::(?=[^:<]*<)", "", n)


def read_census(path):
    data = open(path, "rb").read()
    if data[:8] != b"R1BGUARD":
        sys.exit(f"{path}: not a census file")
    e = "<" if struct.unpack_from("<I", data, 8)[0] == 0x01020304 else ">"
    _bom, version, header, slots, nranges, _flags = struct.unpack_from(e + "6I", data, 8)
    anchor_rt, dropped = struct.unpack_from(e + "2Q", data, 0x20)
    unwinds, hits = struct.unpack_from(e + "2Q", data, 0x3f0)
    print(f"{path}: {hits} accesses counted, {unwinds} unwindings", file=sys.stderr)
    names = [data[0x400 + i * 48:0x400 + i * 48 + 48].split(b"\0")[0].decode() for i in range(nranges)]
    rows = []
    for i in range(slots):
        key, count = struct.unpack_from(e + "2Q", data, header + i * 16)
        if key == 0:
            continue
        kind = KINDS.get(key & 3, "?")
        if key & 4:
            kind = kind.lower()
        rid = (key >> 3) & 0x1fff
        rel = key >> 16
        if rel & (1 << 47):
            rel -= 1 << 48
        rows.append((names[rid] if rid < len(names) else f"#{rid}", kind, rel, count))
    return rows, dropped


def group_of(rng):
    """the census's range name as the scan's group name"""
    if rng.startswith("spr_"):
        return "spr"
    if rng in ("zones_masks", "zones_data"):
        return "zones"
    if rng == "cameras_data":
        return "cameras_ld"
    if rng.endswith("_data"):
        return rng[:-5]
    return rng


def cmd_census(args):
    syms = Symbols(args.binary)
    table = collections.defaultdict(lambda: collections.Counter())  # (range, function) -> kind -> count
    files_of = collections.defaultdict(set)
    out = open(args.tsv, "w") if args.tsv else None
    if out:
        out.write("file\trange\tkind\tfunction\tcount\n")
    for path in args.files:
        rows, dropped = read_census(path)
        if dropped:
            print(f"{path}: {dropped} hits dropped (the table was full)", file=sys.stderr)
        tag = os.path.basename(path)
        per = collections.Counter()
        for rng, kind, rel, count in rows:
            fn = syms.name(syms.anchor + rel)
            per[(rng, kind, fn)] += count
        for (rng, kind, fn), count in sorted(per.items()):
            table[(rng, fn)][kind] += count
            files_of[(rng, fn)].add(tag)
            if out:
                out.write(f"{tag}\t{rng}\t{kind}\t{fn}\t{count}\n")
    if out:
        out.close()
    # the summary: per range, the functions by count
    by_range = collections.defaultdict(list)
    for (rng, fn), c in table.items():
        by_range[rng].append((fn, c))
    print("range\tfunction\tR\tW\tP\tr\tw\tp\tfiles")
    for rng in sorted(by_range):
        for fn, c in sorted(by_range[rng], key=lambda x: -sum(x[1].values())):
            print(f"{rng}\t{fn}\t" + "\t".join(str(c.get(k, 0)) for k in "RWPrwp") + f"\t{len(files_of[(rng, fn)])}")
    if args.static:
        static = collections.defaultdict(set)
        for line in open(args.static):
            f = line.rstrip("\n").split("\t")
            if len(f) > 3 and f[0] != "group":
                static[f[0]].add(f[3])
        print("\n# functions the census saw that the static scan did not name for that group")
        print("group\trange\tfunction\tR\tW\tP\tfiles")
        for (rng, fn), c in sorted(table.items()):
            g = group_of(rng)
            if short(fn) not in static.get(g, ()):
                print(f"{g}\t{rng}\t{fn}\t" + "\t".join(str(c.get(k, 0) + c.get(k.lower(), 0)) for k in "RWP") + f"\t{len(files_of[(rng, fn)])}")


def cmd_trap(args):
    syms = Symbols(args.binary)
    text = open(args.log, errors="replace").read()
    for m in re.finditer(r"anchor([+-]\d+)", text):
        rel = int(m.group(1))
        print(f"anchor{m.group(1)}: {syms.name(syms.anchor + rel)}")
    # the backtrace's lines: <binary>(+0xoffset) (a position-independent binary's offsets are its symbol addresses)
    for m in re.finditer(r"\(\+0x([0-9a-f]+)\)", text):
        print(f"+0x{m.group(1)}: {syms.name(int(m.group(1), 16))}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("census")
    c.add_argument("files", nargs="+")
    c.add_argument("--binary", default="build/rac1-native")
    c.add_argument("--tsv")
    c.add_argument("--static")
    t = sub.add_parser("trap")
    t.add_argument("log")
    t.add_argument("--binary", default="build/rac1-native")
    args = ap.parse_args()
    cmd_census(args) if args.cmd == "census" else cmd_trap(args)


if __name__ == "__main__":
    main()
