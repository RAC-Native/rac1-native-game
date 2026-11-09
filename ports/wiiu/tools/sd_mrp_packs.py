#!/usr/bin/env python3
"""sd_mrp_packs.py <mrp dir> <out dir> [folder ...]: each folder's MRP moby classes (assets/generated/mrp/moby/<folder>/*.mrpm) in one
file, <out dir>/<folder>/files.pack, with its index files.pack.txt, for the Wii U's SD card: copy the out dir's folders over the card's
assets/generated/mrp/moby/. platform::read_packed (ports/wiiu/game/platform_wiiu.cpp) reads a listed file from its folder's pack.

A level's ~200 classes opened one by one on the card cost 10-28 ms each (Kerwan's 195: 3.1 s); its pack (~20 MB) read whole once.
The index: "rac1-native files pack 1", then per file "<name> <offset> <size>" (offsets multiples of 64).
"""
import os
import sys


def pack(src, dst):
    names = sorted(n for n in os.listdir(src) if n.endswith(".mrpm") and os.path.isfile(os.path.join(src, n)))
    if not names:
        return 0, 0
    os.makedirs(dst, exist_ok=True)
    entries = []
    tmp = os.path.join(dst, "files.pack.tmp")
    with open(tmp, "wb") as f:
        for n in names:
            f.write(b"\0" * ((-f.tell()) % 64))
            off = f.tell()
            with open(os.path.join(src, n), "rb") as g:
                data = g.read()
            f.write(data)
            entries.append((n, off, len(data)))
        size = f.tell()
    os.replace(tmp, os.path.join(dst, "files.pack"))
    with open(os.path.join(dst, "files.pack.txt"), "w") as f:
        f.write("rac1-native files pack 1\n")
        for n, off, sz in entries:
            f.write(f"{n} {off} {sz}\n")
    return len(entries), size


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    folders = sys.argv[3:] or sorted(d for d in os.listdir(src) if os.path.isdir(os.path.join(src, d)))
    for d in folders:
        n, size = pack(os.path.join(src, d), os.path.join(out, d))
        print(f"{d}: {n} files, {size >> 20} MB")


if __name__ == "__main__":
    main()
