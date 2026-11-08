#!/usr/bin/env python3
"""sd_level_packs.py <disc dir> <out dir> [level ...]: each level's sound banks and voices (levels/levelNN/hdr_*) in one file,
<out dir>/levels/levelNN/hdr.pack, and their index <out dir>/levels/hdr_packs.txt (every pack in the out dir), for the Wii U's SD card:
copy the out dir's levels/ over the card's assets/disc/levels/. runtime/kernel/disc.cpp reads a listed file from its level's pack.

A level's 50-440 hdr_ files are its first reads in play (a landing, after a cutscene: 40-650 ms each on the card); the Wii U keeps
them in memory from the level's load (platform::keep_in_memory). One by one their opens cost 11-28 ms each in the level's folder
(Kerwan's 441: 7.6-12.6 s); one file read in one go, ~2.5 s.

A pack: the files' bytes, each at an offset a multiple of 64. The index: "rac1-native hdr packs 1", then per pack "pack <level> <size>"
and per file in it "<level> <name> <offset> <size>" (hdr_packs.txt: one file read at the start, not a header in each pack).
"""
import os
import sys


def pack(src, dst):
    names = sorted(n for n in os.listdir(src) if n.startswith("hdr_") and os.path.isfile(os.path.join(src, n)))
    if not names:
        return 0, 0
    at = 0
    entries = []
    for n in names:
        size = os.path.getsize(os.path.join(src, n))
        entries.append((n, at, size))
        at = (at + size + 63) & ~63
    os.makedirs(dst, exist_ok=True)
    tmp = os.path.join(dst, "hdr.pack.tmp")
    with open(tmp, "wb") as f:
        for n, off, size in entries:
            f.write(b"\0" * (off - f.tell()))
            with open(os.path.join(src, n), "rb") as g:
                data = g.read()
            if len(data) != size:
                sys.exit(f"{n}: read {len(data)} of {size} bytes")
            f.write(data)
    os.replace(tmp, os.path.join(dst, "hdr.pack"))
    with open(os.path.join(dst, "hdr.pack.txt"), "w") as f:  # (this pack's lines of the index)
        f.write(f"pack {{lv}} {os.path.getsize(os.path.join(dst, 'hdr.pack'))}\n")
        for n, off, size in entries:
            f.write(f"{{lv}} {n} {off} {size}\n")
    return len(entries), at


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    disc, out = sys.argv[1], sys.argv[2]
    levels = sys.argv[3:] or sorted(d for d in os.listdir(os.path.join(disc, "levels")) if d.startswith("level"))
    for lv in levels:
        n, size = pack(os.path.join(disc, "levels", lv), os.path.join(out, "levels", lv))
        print(f"{lv}: {n} files, {size >> 20} MB")
    lines = ["rac1-native hdr packs 1\n"]
    root = os.path.join(out, "levels")
    for lv in sorted(os.listdir(root)):
        part = os.path.join(root, lv, "hdr.pack.txt")
        if os.path.isfile(part):
            lines += [l.replace("{lv}", lv) for l in open(part)]
    with open(os.path.join(root, "hdr_packs.txt"), "w") as f:
        f.writelines(lines)


if __name__ == "__main__":
    main()
