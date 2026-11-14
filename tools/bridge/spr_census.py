#!/usr/bin/env python3
"""The scratchpad's data flow (RAC1_BRIDGE_GUARD=spr[:<file>], src/game/bridge/guard.cpp), symbolized and summed.

  spr_census.py <flow files...> [--binary build/rac1-native] [--out-dir <dir>]

A flow is a read of a scratchpad word outside the call that wrote it: the word's last write was made inside a call of a
marked function (SprScope: the collision routines, wad_decompress, armor_load, gameplay_load) that has returned, or by code
outside every marked call. Writes:
  flows.tsv        one row per (writer scope, writer function, reader scope, reader function, kind): the words as offset
                   ranges from 0x70000000, the count, the frames (same / next / later) and the tours
  scopes.tsv       per marked function: the flows out of its calls (the answer to "is any word it writes read outside its
                   call?"), by reader and offsets
  writes.tsv       per marked function and writing function (inside its calls: its own code, its callees, the DMA): the
                   words written
  unseen.tsv       reads of a word that code the guard does not see (a host pointer, the scratchpad's direct users) changed
  summary.txt      the per-function answer and the counts
Kinds: R a read; P a host pointer taken (V(), M(): read or written through it). "self" marks a flow whose writer and
reader are the same unmarked function (its calls cannot be told apart: one call's own data, or the last call's).
"""
import argparse
import bisect
import collections
import os
import re
import struct
import subprocess
import sys

ANCHOR = "rac1_bridge_guard_anchor"
F_POINTER, F_NEXT, F_LATER, F_UNSEEN, F_WRITE = 1, 2, 4, 8, 16
UNSEEN, RESTORED = 1, 2


class Symbols:
    def __init__(self, binary):
        out = subprocess.run(["nm", "-n", "-C", "--defined-only", binary], capture_output=True, text=True, check=True).stdout
        self.addrs, self.names, self.anchor = [], [], None
        for line in out.splitlines():
            p = line.split(" ", 2)
            if len(p) < 3 or p[1] not in "tTwW":
                continue
            self.addrs.append(int(p[0], 16))
            self.names.append(p[2])
            if p[2] == ANCHOR:
                self.anchor = int(p[0], 16)
        if self.anchor is None:
            sys.exit(f"{binary}: no symbol {ANCHOR}")
        self.cache = {}

    def name(self, rel):
        if rel == UNSEEN:
            return "(unseen writer)"
        if rel == RESTORED:
            return "(restored)"
        if rel in self.cache:
            return self.cache[rel]
        r = rel - (1 << 48) if rel & (1 << 47) else rel
        i = bisect.bisect_right(self.addrs, self.anchor + r) - 1
        n = short(self.names[i]) if i >= 0 else f"0x{rel:x}"
        self.cache[rel] = n
        return n


def short(name):
    n = re.sub(r" \[clone [^\]]*\]", "", name).replace("(anonymous namespace)::", "")
    depth, cut = 0, len(n)
    for i, c in enumerate(n):
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == "(" and depth == 0:
            cut = i
            break
    return n[:cut].replace("rac1::", "")


def read(path):
    data = open(path, "rb").read()
    if data[:8] != b"R1BGSPR1":
        sys.exit(f"{path}: not a scratchpad flow file (RAC1_BRIDGE_GUARD=spr)")
    e = "<" if struct.unpack_from("<I", data, 8)[0] == 0x01020304 else ">"
    _bom, _ver, header, slots, nscopes, restore = struct.unpack_from(e + "6I", data, 8)
    dropped = struct.unpack_from(e + "Q", data, 0x28)[0]
    scopes = [data[0x400 + i * 48:0x400 + i * 48 + 48].split(b"\0")[0].decode() for i in range(nscopes)]
    calls = [struct.unpack_from(e + "Q", data, 0x380 + 8 * i)[0] for i in range(min(nscopes, 16))]
    rows = []
    for i in range(slots):
        w, r, meta, count = struct.unpack_from(e + "4Q", data, header + i * 32)
        if meta == 0 or meta == (1 << 64) - 1 or count == 0:
            continue
        rows.append((w, r, meta & 0xfff, (meta >> 16) & 0xff, (meta >> 24) & 0xff, (meta >> 32) & 0xff, count))
    return rows, scopes, calls, dropped, restore


def ranges(words):
    """word indices -> 'lo..hi' offset ranges (bytes from 0x70000000, hi exclusive)"""
    out, ws = [], sorted(words)
    i = 0
    while i < len(ws):
        j = i
        while j + 1 < len(ws) and ws[j + 1] == ws[j] + 1:
            j += 1
        out.append(f"+0x{ws[i] * 4:04x}..+0x{(ws[j] + 1) * 4:04x}")
        i = j + 1
    return ", ".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+")
    ap.add_argument("--binary", default="build/rac1-native")
    ap.add_argument("--out-dir", default=".")
    a = ap.parse_args()
    syms = Symbols(a.binary)
    os.makedirs(a.out_dir, exist_ok=True)
    # (writer scope, writer fn, reader scope, reader fn, kind) -> words, count, frames, tours
    agg = collections.defaultdict(lambda: {"words": collections.Counter(), "count": 0, "frames": collections.Counter(), "tours": set()})
    unseen = collections.defaultdict(lambda: {"words": set(), "count": 0, "tours": set()})
    writes = collections.defaultdict(lambda: {"words": collections.Counter(), "count": 0, "tours": set()})
    scope_names, calls_total = None, collections.Counter()
    for path in a.files:
        rows, scopes, calls, dropped, restore = read(path)
        scope_names = scopes
        tag = os.path.basename(path).rsplit(".", 1)[0]
        for i, c in enumerate(calls):
            calls_total[i] += c
        if dropped:
            print(f"{path}: {dropped} flows dropped (the table was full)", file=sys.stderr)
        for w, r, word, flags, ws, rs, count in rows:
            wn = syms.name(w)
            if flags & F_WRITE:
                g = writes[(ws, wn)]
                g["words"][word] += count
                g["count"] += count
                g["tours"].add(tag)
                continue
            rn = syms.name(r)
            if flags & F_UNSEEN:
                u = unseen[(rs, rn)]
                u["words"].add(word)
                u["count"] += count
                u["tours"].add(tag)
            kind = "P" if flags & F_POINTER else "R"
            if ws == 0 and rs == 0 and wn == rn and w not in (UNSEEN, RESTORED):
                kind += " self"
            g = agg[(ws, wn, rs, rn, kind)]
            g["words"][word] += count
            g["count"] += count
            g["frames"]["same" if not flags & (F_NEXT | F_LATER) else "next" if flags & F_NEXT else "later"] += count
            g["tours"].add(tag)
    sn = lambda k: scope_names[k] if k else "-"
    with open(os.path.join(a.out_dir, "flows.tsv"), "w") as f:
        f.write("writer_scope\twriter_fn\treader_scope\treader_fn\tkind\tcount\tsame/next/later frame\ttours\toffsets\n")
        for (ws, wn, rs, rn, kind), g in sorted(agg.items(), key=lambda x: -x[1]["count"]):
            fr = g["frames"]
            f.write(f"{sn(ws)}\t{wn}\t{sn(rs)}\t{rn}\t{kind}\t{g['count']}\t{fr['same']}/{fr['next']}/{fr['later']}\t{len(g['tours'])}\t"
                    f"{ranges(g['words'])}\n")
    with open(os.path.join(a.out_dir, "unseen.tsv"), "w") as f:
        f.write("reader_scope\treader_fn\tcount\ttours\toffsets\n")
        for (rs, rn), u in sorted(unseen.items(), key=lambda x: -x[1]["count"]):
            f.write(f"{sn(rs)}\t{rn}\t{u['count']}\t{len(u['tours'])}\t{ranges(u['words'])}\n")
    with open(os.path.join(a.out_dir, "writes.tsv"), "w") as f:
        f.write("scope\twriter_fn\tcount\ttours\toffsets\n")
        for (ws, wn), g in sorted(writes.items(), key=lambda x: (x[0][0], -x[1]["count"])):
            f.write(f"{sn(ws)}\t{wn}\t{g['count']}\t{len(g['tours'])}\t{ranges(g['words'])}\n")
    lines = []
    with open(os.path.join(a.out_dir, "scopes.tsv"), "w") as f:
        f.write("scope\tcalls\twriter_fn (inside the call)\treader_scope\treader_fn\tkind\tcount\tsame/next/later frame\toffsets\n")
        for k in range(1, len(scope_names)):
            out = [(key, g) for key, g in agg.items() if key[0] == k]
            words = set()
            for _, g in out:
                words |= set(g["words"])
            lines.append(f"{scope_names[k]}: {calls_total[k]} calls; "
                         + (f"{sum(g['count'] for _, g in out)} reads of its words outside its calls, by "
                            f"{len(set((key[2], key[3]) for key, _ in out))} readers, at {ranges(words)}" if out else "no word it wrote was read outside its call"))
            for (ws, wn, rs, rn, kind), g in sorted(out, key=lambda x: -x[1]["count"]):
                fr = g["frames"]
                f.write(f"{scope_names[k]}\t{calls_total[k]}\t{wn}\t{sn(rs)}\t{rn}\t{kind}\t{g['count']}\t{fr['same']}/{fr['next']}/{fr['later']}\t"
                        f"{ranges(g['words'])}\n")
    with open(os.path.join(a.out_dir, "summary.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
