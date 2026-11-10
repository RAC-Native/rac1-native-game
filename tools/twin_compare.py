#!/usr/bin/env python3
"""The twin run's comparison (the open engine, docs/open-engine.md): a class on its own level and the same class imported
into a session on another base (RAC1_SESSION_TWIN, src/game/enhance/twin.h), compared frame by frame.

Each run's log (RAC1_TWIN_LOG) holds, from the twin's start, every frame's moby record (0x100 bytes), its pvars, the hero's
place, health and state, and the mobys its update spawned. The two runs' words differ where they should: the places (each
run's origin), and the addresses (its class block, its pvars, the hero's moby, and the level's own data, engine data and
code: the native run has the level's addresses, the session its context's). A word is equal when it is the same bytes, the
same place relative to each run's origin, or the same address of the same thing. The fields the session's spawn gives its
moby (its class index and header, update function, pvars, grid rectangle, index) are not compared.

Usage (repo root):
  tools/twin_compare.py <native.twl> <session.twl> [--frames N] [--show 20] [--tolerance 1e-4]
Exit status 0 when every compared frame is equal, 1 otherwise; the first differing frame and its words are printed.
"""
import argparse
import bisect
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = 0x15EF00
KEPT = {0x22, 0x24, 0x25, 0x26, 0x27, *range(0x74, 0x7C), 0xA0, 0xA1, 0xA2, 0xA3, *range(0xA8, 0xB0),
        *range(0x9C, 0xA0), *range(0x28, 0x2C)}  # (+0x9c: the collision queries' stamp, a counter of the whole game;
#                                                     +0x28: the frame's chain of the mobys to update: bookkeeping)


def read_log(path):
    b = Path(path).read_bytes()
    assert b[:4] == b"TWL1", f"{path}: not a twin log"
    kind, level, cls, moby, pvars, pvsize, block, bsize, hero, image = struct.unpack_from("<10I", b, 4)
    origin = struct.unpack_from("<3f", b, 44)
    run = dict(kind=kind, level=level, cls=cls, moby=moby, pvars=pvars, pvsize=pvsize, block=block, bsize=bsize, hero=hero,
               image=image, origin=origin, frames=[])
    p = 56
    while p + 4 <= len(b):
        k = struct.unpack_from("<I", b, p)[0]
        p += 4
        rec = b[p:p + 0x100]
        p += 0x100
        pv = b[p:p + pvsize]
        p += pvsize
        hpos = struct.unpack_from("<3f", b, p)
        health, hstate, n = struct.unpack_from("<III", b, p + 12)
        p += 24
        sp = []
        for _ in range(n):
            c = struct.unpack_from("<I", b, p)[0]
            sp.append((c, struct.unpack_from("<3f", b, p + 4)))
            p += 16
        if len(rec) < 0x100:
            break
        run["frames"].append(dict(k=k, rec=rec, pv=pv, hpos=hpos, health=health, hstate=hstate, spawned=sp))
    return run


class LevelMap:
    """Level N's engine objects (levelN.ext) and functions paired with Veldin's (levelN.txt), both ways."""

    def __init__(self, level):
        d = ROOT / "assets/generated/overlay_map"
        self.ext = []  # (level start, veldin start, size)
        for line in (d / f"level{level}.ext").read_text().splitlines():
            p = line.split()
            if len(p) == 4 and p[0] == "ext":
                self.ext.append((int(p[2], 16), int(p[1], 16), int(p[3], 16)))
        self.ext.sort()
        self.fns = []
        for line in (d / f"level{level}.txt").read_text().splitlines():
            p = line.split()
            if len(p) >= 4 and p[0] == "fn":
                self.fns.append((int(p[2], 16), int(p[1], 16), int(p[3], 16)))
        self.fns.sort()

    def to_veldin(self, a):
        """A level address of an engine object or function: Veldin's (None: the level's own)."""
        for tab in (self.ext, self.fns):
            i = bisect.bisect_right(tab, (a, 0xFFFFFFFF, 0)) - 1
            if i >= 0 and a - tab[i][0] < tab[i][2]:
                return a - tab[i][0] + tab[i][1]
        return None


def symbol(run, w, lmap):
    """A word as what it points at, in terms both runs share (None: not an address of the twin's)."""
    for name, base, size in (("block", run["block"], run["bsize"]), ("pvars", run["pvars"], run["pvsize"]),
                             ("hero", run["hero"], 0x100)):
        if base and base <= w < base + size:
            return (name, w - base)
    if run["kind"] == 1:
        if (w & 0xC0000000) == 0x40000000 and (w >> 22) & 0xFF == run["level"]:
            return ("level", (w & 0x3FFFFF) + 0x1D0000)
        if run["image"] and run["image"] <= w < run["image"] + 0x200000:
            return ("level", BASE + (w - run["image"]))
        if BASE <= w < 0x400000:
            return ("veldin", w)
    elif BASE <= w < 0x400000:
        v = lmap.to_veldin(w)
        return ("veldin", v) if v is not None else ("level", w)
    return None


def word_equal(a, b, i, na, sb, lmap, tol):
    if a == b:
        return True
    sa, sbb = symbol(na, a, lmap), symbol(sb, b, lmap)
    if sa is not None and sa == sbb:
        return True
    fa, fb = struct.unpack("<f", struct.pack("<I", a))[0], struct.unpack("<f", struct.pack("<I", b))[0]
    if fa != fa or fb != fb:
        return False
    if abs(fa - fb) <= tol * max(1.0, abs(fa)):  # (the same value but for the float rounding of other places' sums)
        return True
    for scale in (1.0, 1024.0):  # (a place: relative to each run's origin; +0x00 is the bounding sphere's, in 1/1024 units)
        for k in range(3):
            ra, rb = fa / scale - na["origin"][k], fb / scale - sb["origin"][k]
            if abs(ra - rb) <= tol * max(1.0, abs(fa / scale)) and abs(fa) > 1e-3:
                return True
    return False


def compare_block(xa, xb, na, sb, lmap, tol, skip=()):
    out = []
    for i in range(0, min(len(xa), len(xb)) - 3, 4):
        if any(i + j in skip for j in range(4)):
            continue
        a, b = struct.unpack_from("<I", xa, i)[0], struct.unpack_from("<I", xb, i)[0]
        if not word_equal(a, b, i, na, sb, lmap, tol):
            out.append((i, a, b))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("native")
    ap.add_argument("session")
    ap.add_argument("--frames", type=int, default=0)
    ap.add_argument("--show", type=int, default=20)
    ap.add_argument("--tolerance", type=float, default=1e-4)
    ap.add_argument("--hero-tolerance", type=float, default=1e-3, help="the hero's place (the two levels' floors differ a little)")
    ap.add_argument("--list", type=int, default=0, help="the differing words' offsets of the first N differing frames")
    a = ap.parse_args()
    na, sb = read_log(a.native), read_log(a.session)
    if na["kind"] != 0 or sb["kind"] != 1 or (na["level"], na["cls"]) != (sb["level"], sb["cls"]):
        sys.exit("twin_compare: give the native run's log, then the session's, of the same class")
    lmap = LevelMap(na["level"])
    n = min(len(na["frames"]), len(sb["frames"]))
    if a.frames:
        n = min(n, a.frames)
    first, bad = None, 0
    cls_bad, cls_first, env_bad, env_first = 0, None, 0, None
    for f in range(n):
        A, B = na["frames"][f], sb["frames"][f]
        d_rec = compare_block(A["rec"], B["rec"], na, sb, lmap, a.tolerance, KEPT)
        d_pv = compare_block(A["pv"], B["pv"], na, sb, lmap, a.tolerance)
        d_hero = [k for k in range(3) if abs((A["hpos"][k] - na["origin"][k]) - (B["hpos"][k] - sb["origin"][k])) > a.hero_tolerance]
        d_misc = []
        if A["health"] != B["health"]:
            d_misc.append(f"health {A['health']} / {B['health']}")
        if A["hstate"] != B["hstate"]:
            d_misc.append(f"hero state {A['hstate']} / {B['hstate']}")
        if [c for c, _ in A["spawned"]] != [c for c, _ in B["spawned"]]:
            d_misc.append(f"spawned {[c for c, _ in A['spawned']]} / {[c for c, _ in B['spawned']]}")
        if d_rec or d_pv or (d_misc and d_misc[-1].startswith("spawned")):
            cls_bad += 1
            cls_first = A["k"] if cls_first is None else cls_first
        if d_hero or [m for m in d_misc if not m.startswith("spawned")]:
            env_bad += 1
            env_first = A["k"] if env_first is None else env_first
        if (d_rec or d_pv or d_hero or d_misc) and bad < a.list:
            print(f"  +{A['k']}: moby {[hex(i) for i, _, _ in d_rec]} pvars {[hex(i) for i, _, _ in d_pv]} hero {d_hero} {d_misc}")
        if d_rec or d_pv or d_hero or d_misc:
            bad += 1
            if first is None:
                first = f
                print(f"first difference at frame +{A['k']}:")
                for i, x, y in d_rec[:a.show]:
                    print(f"  moby +{i:#05x}: native {x:08x} session {y:08x}")
                for i, x, y in d_pv[:a.show]:
                    print(f"  pvars +{i:#05x}: native {x:08x} session {y:08x}")
                if d_hero:
                    print(f"  hero place (from the origin): native {[round(A['hpos'][k] - na['origin'][k], 3) for k in range(3)]} "
                          f"session {[round(B['hpos'][k] - sb['origin'][k], 3) for k in range(3)]}")
                for m in d_misc:
                    print(f"  {m}")
    spawned = (sum(len(fr["spawned"]) for fr in na["frames"][:n]), sum(len(fr["spawned"]) for fr in sb["frames"][:n]))
    print(f"level {na['level']} class {na['cls']}: {n} frames compared (native {len(na['frames'])}, session {len(sb['frames'])}), "
          f"{n - bad} equal, {bad} differ; spawned native {spawned[0]}, session {spawned[1]}; "
          f"health at the end native {na['frames'][n - 1]['health'] if n else '-'}, session {sb['frames'][n - 1]['health'] if n else '-'}")
    print(f"  the class (its record, pvars, spawns): {n - cls_bad} of {n} frames equal" +
          (f", the first difference at +{cls_first}" if cls_first is not None else "") +
          f"; the hero (place, health, state): {n - env_bad} equal" + (f", the first difference at +{env_first}" if env_first is not None else ""))
    sys.exit(0 if bad == 0 and n > 0 else 1)


if __name__ == "__main__":
    main()
