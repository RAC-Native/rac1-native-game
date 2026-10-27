#!/usr/bin/env python3
"""The player's disc, extracted into files (docs/setup.md, "The disc's files"): the game reads these, not the image.

The game reads its disc by sector (2048 bytes): the sector numbers come from Insomniac's own table of contents at
sector 1500 and from each level's header, not from the ISO 9660 file system, which holds only SYSTEM.CNF, the boot
executable and the IOP's modules. Everything else (the levels' data, the scenes, the dialogue, the music, the
movies) lies in sectors no ISO 9660 file names. This tool cuts the whole image into files, in order:

  files/<path>                       the ISO 9660 file system's files (SYSTEM.CNF, SCUS_971.99, IOPRP243.IMG)
  toc.bin                            the table of contents (sector 1500)
  levels/levelNN/header.bin          a level's header (the sector its row in the table of contents names)
  levels/levelNN/data.wad            its data (the overlay and the core), gameplay_ntsc.bin, gameplay_pal.bin and
                                     occlusion.bin: the four sector ranges at the header's +0x08 .. +0x24
  levels/levelNN/hdr_XXXX.<ext>      a sector named by a word of the level's header, at byte XXXX (hex): the level's
                                     scenes, sounds and music
  toc/row_XXXX.<ext>                 a sector named by a word of the table of contents, at byte XXXX (hex): the
                                     front end's data, the dialogue, the movies...
  other/<sector>[_<byte>].bin        bytes that no table names and that are not all zero (the ISO 9660 volume's
                                     own sectors: its descriptors and directories)

<ext> is what the bytes are: .vag (a sound, "VAGp"), .pss (a movie, an MPEG program stream) or .bin. Where the
size is known (an ISO 9660 file, a level's four ranges, a VAG's header) the file is exactly that; otherwise a file
runs to the next sector a table names. What no file holds is zero. So every byte of the image is in exactly one
file or is zero, and a read of any sector, across files or outside every one, gives the image's bytes back.

layout.txt, written last, lists the files: "<first byte (hex)> <size> <offset in the file> <sha1> <path>", one
line per piece, sorted by the first byte; the runtime (runtime/kernel/disc.cpp) maps a sector to its file with it.
Each file is checked after it is written: its size, and its sha1 against the image's bytes.

Python's standard library only. Used by tools/setup.py (its "disc" step); by hand:
  tools/disc_extract.py --iso <image> --out assets/disc        (extract)
  tools/disc_extract.py --iso <image> --plan                   (list what would be written, write nothing)
"""

import argparse
import bisect
import hashlib
import os
import shutil
import struct
import sys
import time
from pathlib import Path

SECTOR = 2048
TOC_LBA = 1500  # the table of contents (src/formats/iso_toc.h)
TOC_SECTORS = 6  # what the game reads of it (toc_read, src/game/boot/level_read.cpp)
LEVELS = 19
LEVEL_TABLE = 0x28C8  # the levels' header sectors, 8 bytes per level (0x13a448 - 0x137b80)
LEVEL_HEADER_BYTES = 0x2434
LEVEL_RANGES = ("data.wad", "gameplay_ntsc.bin", "gameplay_pal.bin", "occlusion.bin")  # header +0x08 .. +0x24
LAYOUT = "layout.txt"
LAYOUT_MAGIC = "rac1-native disc layout 1"
WINDOWS = os.name == "nt"
CHUNK = 8 << 20


def long_path(path):
    """On Windows, a path past the old 260-character limit only opens with the \\\\?\\ prefix."""
    p = os.path.abspath(path)
    if WINDOWS and len(p) > 240 and not p.startswith("\\\\?\\"):
        return "\\\\?\\" + p
    return p


class DiscError(Exception):
    pass


class Image:
    def __init__(self, path):
        self.path = Path(path)
        self.f = open(long_path(self.path), "rb")
        self.size = os.fstat(self.f.fileno()).st_size

    def read(self, offset, n):
        self.f.seek(offset)
        return self.f.read(n)

    def word(self, offset):
        b = self.read(offset, 4)
        return struct.unpack("<I", b)[0] if len(b) == 4 else 0

    def close(self):
        self.f.close()


def kind(img, start):
    """The file extension for the bytes at `start`: a sound, a movie, or data."""
    head = img.read(start, 4)
    if head == b"VAGp":
        return "vag"
    if head == b"\0\0\1\xba":
        return "pss"
    return "bin"


def iso9660_files(img):
    """The ISO 9660 file system: [(path, [(first byte, size), ...])] (more than one extent: a multi-extent file)."""
    pvd = img.read(16 * SECTOR, SECTOR)
    if len(pvd) < SECTOR or pvd[1:6] != b"CD001" or pvd[0] != 1:
        raise DiscError("not an ISO 9660 image (no primary volume descriptor at sector 16)")
    root = pvd[156:156 + 34]
    files, seen = [], set()

    def walk(lba, size, prefix, depth):
        if (lba, size) in seen or depth > 16:  # (a loop, or nonsense)
            return
        seen.add((lba, size))
        data = img.read(lba * SECTOR, size)
        off, pending = 0, None
        while off < len(data):
            ln = data[off]
            if ln == 0:  # (records do not cross sector boundaries)
                off = (off // SECTOR + 1) * SECTOR
                continue
            rec = data[off:off + ln]
            off += ln
            if len(rec) < 34:
                break
            name = rec[33:33 + rec[32]]
            flags = rec[25]
            elba, esize = struct.unpack_from("<I", rec, 2)[0], struct.unpack_from("<I", rec, 10)[0]
            if name in (b"\0", b"\1"):
                continue
            text = name.decode("ascii", "replace").split(";")[0].rstrip(".") or "_"
            text = "".join(c if c.isalnum() or c in "._-" else "_" for c in text)
            if flags & 2:
                walk(elba, esize, prefix + text + "/", depth + 1)
                continue
            if pending is not None and pending[0] == prefix + text:
                pending[1].append((elba * SECTOR, esize))
            else:
                if pending is not None:
                    files.append(pending)
                pending = (prefix + text, [(elba * SECTOR, esize)])
            if not flags & 0x80:  # (the last extent of a multi-extent file)
                files.append(pending)
                pending = None
        if pending is not None:
            files.append(pending)

    walk(struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0], "", 0)
    return files


class Plan:
    """The image cut into files: `pieces` [(first byte, size, offset in the file, path)], sorted, not overlapping."""

    def __init__(self, image_size, pieces):
        self.image_size = image_size
        self.pieces = pieces
        self.files = {}
        for start, size, foff, path in pieces:
            self.files[path] = max(self.files.get(path, 0), foff + size)

    @property
    def total(self):
        return sum(p[1] for p in self.pieces)


def plan_disc(img):
    """Where each file starts and (when known) ends, from the ISO 9660 file system, the table of contents and the
    levels' headers; see the module's description."""
    sectors = img.size // SECTOR
    if sectors <= TOC_LBA + TOC_SECTORS:
        raise DiscError("the image is too small: it ends before the table of contents (sector 1500)")
    exact = []  # (first byte, size, offset in the file, path): the pieces whose size is known
    starts = {}  # first byte -> (priority, path stem, extension or None for "what is there")

    def start(lba, prio, stem, ext=None):
        if TOC_LBA + TOC_SECTORS <= lba < sectors:
            old = starts.get(lba * SECTOR)
            if old is None or prio < old[0]:
                starts[lba * SECTOR] = (prio, stem, ext)

    for path, extents in iso9660_files(img):
        foff = 0
        for first, size in extents:
            if size and first + size <= img.size:
                exact.append((first, size, foff, "files/" + path))
            foff += size
    toc = img.read(TOC_LBA * SECTOR, TOC_SECTORS * SECTOR)
    magic, toc_size = struct.unpack_from("<iI", toc, 0)
    if magic != 1 or not 8 <= toc_size <= len(toc):
        raise DiscError("no Ratchet & Clank table of contents at sector 1500")
    starts[TOC_LBA * SECTOR] = (0, "toc", "bin")
    # the levels: their headers and four ranges are sized; every other word of a header naming a sector past the table
    # of contents starts a file (the scenes' lists of sectors, the music's...)
    for lv in range(LEVELS):
        hl = struct.unpack_from("<I", toc, LEVEL_TABLE + 8 * lv)[0]
        if not TOC_LBA < hl < sectors:
            continue
        head = img.read(hl * SECTOR, LEVEL_HEADER_BYTES)
        if len(head) < LEVEL_HEADER_BYTES or struct.unpack_from("<iI", head, 0) != (lv, LEVEL_HEADER_BYTES):
            continue
        d = f"levels/level{lv:02d}/"
        start(hl, 1, d + "header", "bin")
        for k, name in enumerate(LEVEL_RANGES):
            lba, n = struct.unpack_from("<II", head, 8 + 8 * k)
            if n and TOC_LBA < lba and lba + n <= sectors:
                exact.append((lba * SECTOR, n * SECTOR, 0, d + name))
        for o in range(0x28, LEVEL_HEADER_BYTES - 3, 4):
            start(struct.unpack_from("<I", head, o)[0], 2, f"{d}hdr_{o:04x}")
    # the table of contents: rows of (sector, sector count) and lists of sectors. A row's second word is a count, not
    # a sector, when the two add up to no more than the disc (two sectors of a list add up to more: the data the
    # lists name lies past the middle of the disc); a movie's row has its size in bytes, past the disc's sectors.
    for o in range(8, toc_size - 7, 8):
        if LEVEL_TABLE <= o < LEVEL_TABLE + 8 * LEVELS:
            continue  # (the levels' rows: above)
        a, b = struct.unpack_from("<II", toc, o)
        if a:
            start(a, 3, f"toc/row_{o:04x}")
            if b >= SECTOR and TOC_LBA + TOC_SECTORS <= a and a * SECTOR + b <= img.size and kind(img, a * SECTOR) == "pss":
                exact.append((a * SECTOR, b, 0, None))  # (a movie: its size in bytes)
        if b and (a == 0 or a + b > sectors):
            start(b, 3, f"toc/row_{o + 4:04x}")
    # the sounds' own sizes (a VAG's header: 48 bytes, then its data, big-endian at +12)
    for first, (prio, stem, ext) in list(starts.items()):
        if ext is None:
            ext = kind(img, first)
            starts[first] = (prio, stem, ext)
        if ext == "vag":
            size = 48 + struct.unpack(">I", img.read(first + 12, 4))[0]
            if first + size <= img.size:
                exact.append((first, size, 0, None))  # (named with its start, below)

    # the sized pieces: the ISO 9660 files and the levels' ranges, which cannot overlap on a good disc, then the sizes the
    # sounds and movies give, each kept only where it overlaps none of those (a piece inside another one is dropped:
    # the outer one holds its bytes)
    sized = []
    for named in (True, False):
        for p in sorted((p for p in exact if (p[3] is not None) == named), key=lambda p: (p[0], -p[1])):
            i = bisect.bisect_right(sized, (p[0], float("inf")))
            before = sized[i - 1] if i else None
            after = sized[i] if i < len(sized) else None
            if before is not None and before[0] + before[1] >= p[0] + p[1]:
                continue  # (inside another)
            if (before is not None and before[0] + before[1] > p[0]) or (after is not None and after[0] < p[0] + p[1]):
                if named:
                    raise DiscError(f"two files overlap at byte {p[0]:#x} ({p[3]})")
                continue
            sized.insert(i, p)
    # the boundaries: every start not inside a sized piece, and every sized piece's two ends
    bounds = {0: None, img.size: None}
    sized_at = {}
    for p in sized:
        bounds.setdefault(p[0], None)
        bounds.setdefault(p[0] + p[1], None)
        sized_at[p[0]] = p
    inside = [(p[0], p[0] + p[1]) for p in sized]
    firsts = [s for s, _ in inside]
    for first in starts:
        i = bisect.bisect_right(firsts, first) - 1
        if i >= 0 and inside[i][0] < first < inside[i][1]:
            continue  # (a sector inside a sized file: that file holds it)
        bounds[first] = None
    order = sorted(bounds)
    pieces = []
    for i in range(len(order) - 1):
        a, b = order[i], order[i + 1]
        p = sized_at.get(a)
        if p is not None:
            name = p[3]
            if name is None:
                prio, stem, ext = starts[a]
                name = f"{stem}.{ext}"
            pieces.append((a, p[1], p[2], name))
            continue
        if a in starts:
            prio, stem, ext = starts[a]
            pieces.append((a, b - a, 0, f"{stem}.{ext}"))
        else:  # (not named by any table: kept when it is not all zero)
            if not is_zero(img, a, b - a):
                pieces.append((a, b - a, 0, f"other/{a // SECTOR}" + (f"_{a % SECTOR}" if a % SECTOR else "") + ".bin"))
    # multi-extent files hold more than one piece; any other name used twice is an error
    names = {}
    for p in pieces:
        if p[3] in names and not p[3].startswith("files/"):
            raise DiscError(f"two pieces named {p[3]}")
        names[p[3]] = True
    return Plan(img.size, pieces)


def is_zero(img, start, size):
    zero = bytes(min(size, CHUNK))
    done = 0
    while done < size:
        n = min(CHUNK, size - done)
        if img.read(start + done, n) != zero[:n]:
            return False
        done += n
    return True


def disk_free(path):
    p = Path(path)
    while not p.exists():
        p = p.parent
    return shutil.disk_usage(p).free


def extract(iso, out, progress=None, log=print):
    """Extracts the image `iso` into the directory `out` (replaced). Returns the Plan. Raises DiscError."""
    out = Path(out)
    if out.is_symlink():  # (assets/disc linked elsewhere, to a larger disk: the files go there)
        out = out.resolve()
    img = Image(iso)
    try:
        plan = plan_disc(img)
        part = out.with_name(out.name + ".part")
        for old in (part, out):  # (the old files first: the space they free counts)
            if old.is_symlink() or old.is_file():
                old.unlink()
            elif old.exists():
                shutil.rmtree(long_path(old))
        need = plan.total + (256 << 20)
        free = disk_free(out.parent)
        if free < need:
            raise DiscError(f"not enough free disk space in {out.parent}: the disc's files need "
                            f"{need / 2**30:.1f} GB, {free / 2**30:.1f} GB is free")
        part.mkdir(parents=True)
        done, total = 0, plan.total * 2  # (written, then read back)
        layout = []
        open_files = {}
        try:
            for first, size, foff, path in plan.pieces:
                dest = part / path
                f = open_files.get(path)
                if f is None:
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    f = open(long_path(dest), "wb")
                    if path.startswith("files/"):
                        open_files[path] = f  # (a multi-extent file: its next piece follows)
                h = hashlib.sha1()
                f.seek(foff)
                left, at = size, first
                while left:
                    n = min(CHUNK, left)
                    data = img.read(at, n)
                    if len(data) != n:
                        raise DiscError(f"the image ends early (at byte {at + len(data):#x})")
                    h.update(data)
                    f.write(data)
                    left -= n
                    at += n
                    done += n
                    if progress:
                        progress(done, total)
                if path not in open_files:
                    f.close()
                layout.append((first, size, foff, h.hexdigest(), path))
        finally:
            for f in open_files.values():
                f.close()
        # each file read back: its size, and its bytes against the image's (their sha1)
        for first, size, foff, digest, path in layout:
            dest = part / path
            if os.path.getsize(long_path(dest)) != plan.files[path]:
                raise DiscError(f"{dest} has the wrong size after writing it")
            h = hashlib.sha1()
            with open(long_path(dest), "rb") as f:
                f.seek(foff)
                left = size
                while left:
                    data = f.read(min(CHUNK, left))
                    if not data:
                        break
                    h.update(data)
                    left -= len(data)
                    done += len(data)
                    if progress:
                        progress(done, total)
            if left or h.hexdigest() != digest:
                raise DiscError(f"{dest} does not read back as written (the disk or its file system is faulty)")
        with open(long_path(part / LAYOUT), "w", encoding="ascii", newline="\n") as f:
            f.write(f"{LAYOUT_MAGIC}\n")
            f.write(f"image {img.size}\n")
            f.write("# first byte (hex), size, offset in the file, sha1, path; bytes no line holds are zero\n")
            for first, size, foff, digest, path in layout:
                f.write(f"{first:x} {size} {foff} {digest} {path}\n")
        part.rename(out)
        return plan
    finally:
        img.close()


def read_layout(directory):
    """The layout of an extracted disc: (image size, [(first, size, foff, sha1, path)]); None when it is not there or
    not complete (the files it names missing or of the wrong size)."""
    d = Path(directory)
    try:
        lines = (d / LAYOUT).read_text(encoding="ascii").splitlines()
    except (OSError, UnicodeDecodeError):
        return None
    if not lines or lines[0] != LAYOUT_MAGIC or not lines[1].startswith("image "):
        return None
    pieces, sizes = [], {}
    for ln in lines[2:]:
        if ln.startswith("#") or not ln.strip():
            continue
        first, size, foff, digest, path = ln.split(" ", 4)
        pieces.append((int(first, 16), int(size), int(foff), digest, path))
        sizes[path] = max(sizes.get(path, 0), int(foff) + int(size))
    return int(lines[1].split()[1]), pieces, sizes


def layout_missing(directory):
    """Why the extracted disc in `directory` is not usable ("<path> is missing"), or None when it is complete."""
    d = Path(directory)
    lay = read_layout(d)
    if lay is None:
        return f"{d / LAYOUT} is missing"
    for path, size in lay[2].items():
        try:
            if os.stat(long_path(d / path)).st_size == size:
                continue
        except OSError:
            return f"{d / path} is missing"
        return f"{d / path} has the wrong size"
    return None


def main():
    ap = argparse.ArgumentParser(description="Extracts a Ratchet & Clank disc image into the files the game reads.")
    ap.add_argument("--iso", required=True, help="the disc image")
    ap.add_argument("--out", default="assets/disc", help="the directory to write (replaced; default assets/disc)")
    ap.add_argument("--plan", action="store_true", help="only list the files it would write")
    a = ap.parse_args()
    try:
        if a.plan:
            img = Image(a.iso)
            plan = plan_disc(img)
            for first, size, foff, path in plan.pieces:
                print(f"{first // SECTOR:8d} {first % SECTOR:4d} {size:11d} {path}")
            kinds = {}
            for path, size in plan.files.items():
                k = path.split("/")[0] + " ." + path.rsplit(".", 1)[-1]
                c = kinds.setdefault(k, [0, 0])
                c[0] += 1
                c[1] += size
            for k, (n, size) in sorted(kinds.items()):
                print(f"# {k}: {n} files, {size / 2**20:.1f} MB", file=sys.stderr)
            print(f"# {len(plan.files)} files, {plan.total / 2**30:.3f} GB of {img.size / 2**30:.3f} GB", file=sys.stderr)
            return
        t = time.time()
        last = [-1]

        def progress(done, total):
            pct = done * 100 // total
            if pct != last[0]:
                last[0] = pct
                print(f"\r  {pct}%", end="", flush=True)

        plan = extract(a.iso, a.out, progress)
        print(f"\r  {len(plan.files)} files, {plan.total / 2**30:.2f} GB, in {time.time() - t:.0f} s")
    except (DiscError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
