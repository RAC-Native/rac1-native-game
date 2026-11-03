#!/usr/bin/env python3
"""Match the code of Veldin's overlay (the one every src/game/ovl port is written against) in
another level's overlay, relocations masked, and map the addresses.

Every R&C1 level ships its own overlay executable: the engine is
compiled into each of them, linked at different addresses, with the level's own code and
tables. This tool reads the overlays straight from your ISO and, for each level N:

  - finds Veldin's functions in level N's code: the instruction words compared with the
    relocated fields masked (jal / j targets, lui immediates, the 16-bit offsets of loads,
    stores and addiu whose base is not $sp); a function matches when exactly one place in
    level N's code has the same masked words;
  - from the matched pairs, maps Veldin's data addresses to level N's (lui + lo pairs, $gp
    offsets) and its function addresses (jal targets);
  - compares the level tables of sections 3 / 4 / 5 (moby class -> update function, camera
    types, the level's sound handlers);
  - with --ported, counts the ported functions (the `orig:` annotations of src/game) that have
    an equivalent in level N.

Output (gitignored, generated from your ISO; see setup.sh):
  <out>/levelN.txt   "fn <veldin> <levelN> <size>" / "data <veldin> <levelN> <uses>" lines and a summary
                     (uses 0: a data object only data points to, paired through the mapped
                     objects' pointers)

Usage (repo root):
  tools/overlay_match/overlay_match.py [--iso assets/iso/rac1-ntsc-u.iso] [--out assets/generated/overlay_map]
                                       [--levels 3,1] [--ported]
"""
import argparse
import bisect
import collections
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SECTOR = 2048
VELDIN = 0

# ---- the ISO: table of contents (LBA 1500) -> level header -> level data -> overlay ---------


def level_overlay(iso, level):
    """The level's overlay sections [(dest, type, bytes)] and its entry point, read from the ISO:
    the table of contents at LBA 1500 (8-byte rows from byte 8: the level headers' LBAs), the
    level header (id, 0x2434, data sector range), the level data's header (overlay byte range
    at +0), then the Ratchet executable's [dest, size, type, entry] blocks."""
    iso.seek(1500 * SECTOR)
    magic, toc_size = struct.unpack("<ii", iso.read(8))
    iso.seek(1500 * SECTOR)
    toc = iso.read(toc_size)
    for ofs in range(8, len(toc), 8):
        lsn = struct.unpack_from("<I", toc, ofs)[0]
        if not lsn:
            continue
        iso.seek(lsn * SECTOR)
        h = iso.read(0x10)
        lid, hsize, data_lba, data_n = struct.unpack("<iiII", h)
        if hsize != 0x2434 or lid != level:
            continue
        iso.seek(data_lba * SECTOR)
        off, size = struct.unpack("<ii", iso.read(8))
        iso.seek(data_lba * SECTOR + off)
        exe = iso.read(size)
        secs, p, entry = [], 0, None
        while p + 16 <= len(exe):
            dest, n, typ, ep = struct.unpack_from("<iiii", exe, p)
            if entry is None:
                entry = ep
            elif ep != entry:
                break
            secs.append((dest, typ, exe[p + 16:p + 16 + n]))
            p += 16 + n
        return secs, entry
    raise SystemExit(f"[overlay_match] level {level} not in the table of contents")


def words(b):
    n = len(b) // 4
    return list(struct.unpack("<%dI" % n, b[:n * 4]))


# ---- masking the relocated fields -------------------------------------------------------------

MEM = {0x1a, 0x1b, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
       0x2c, 0x2d, 0x2e, 0x31, 0x35, 0x36, 0x37, 0x39, 0x3d, 0x3e, 0x3f}  # loads / stores
ADDI = {0x09, 0x19}  # addiu, daddiu
ADDI_TRAP = {0x08, 0x18}  # addi, daddi: not masked (the same offset in both), but a base + offset address all the same
SP, GP = 29, 28
GP_VALUE = 0x00166C00  # the base ELF's $gp (its .reginfo), the same for every overlay of a build (the 1.02 core's: 0x166d00)


def mask(w):
    op = w >> 26
    if op in (2, 3):
        return w & 0xFC000000
    if op == 0x0F:
        return w & 0xFFFF0000
    if (op in MEM or op in ADDI) and ((w >> 21) & 31) != SP:
        return w & 0xFFFF0000
    return w


def sx16(v):
    return (v & 0xFFFF) - ((v & 0x8000) << 1)


class Overlay:
    # The build's $gp, the end of its core (the boot executable below the overlays) and the core's addresses back to
    # NTSC-U's (None: not the same core): the NTSC-U build's by default; another build's core (the 1.02 one, moved) is
    # set by tools/overlay_match/region_maps.py
    gp = GP_VALUE
    core_end = 0x15EF00
    core_back = staticmethod(lambda a: a)

    def __init__(self, secs, entry):
        self.secs, self.entry = secs, entry
        self.code_base, _, code = secs[6]
        self.code = words(code)
        self.masked = [mask(w) for w in self.code]
        self.code_end = self.code_base + 4 * len(self.code)

    def data_word(self, a):
        """The word at `a` of the data sections (0..5, with contents), None elsewhere."""
        for k, (dest, typ, b) in enumerate(self.secs[:6]):
            if typ == 1 and dest <= a and a + 4 <= dest + len(b) and a % 4 == 0:
                return struct.unpack_from("<I", b, a - dest)[0]
        return None

    def in_data(self, a):
        return any(dest <= a < dest + (len(b) if typ == 1 else 0) for dest, typ, b in self.secs[:6])

    def in_code(self, a):
        return self.code_base <= a < self.code_end and a % 4 == 0

    def table_rows(self, k, stride):
        """Section k's leading rows (stride words each) up to the -1 key."""
        dest, _, b = self.secs[k]
        w = words(b)
        rows = []
        for i in range(0, len(w) - stride + 1, stride):
            if w[i] == 0xFFFFFFFF:
                break
            rows.append(tuple(w[i:i + stride]))
        return rows

    def after_return(self, a):
        """A leaf's start: a `jr ra` (and its delay slot) just before, nops for alignment allowed."""
        i = (a - self.code_base) // 4 - 2
        while i >= 0 and self.code[i] == 0 and (a - self.code_base) // 4 - i < 6:
            i -= 1
        return i >= 0 and self.code[i] == 0x03E00008 or i >= 0 and self.code[i + 1] == 0x03E00008

    def function_starts(self):
        """Function entries without a symbol table: the jal targets, the overlay's entry point,
        and the code pointers of the data sections whose target starts a frame (addiu sp,sp,-n)
        or follows a return (a leaf: jr ra two words before), or that sections 3 / 4 / 5 (the
        level's tables) hold."""
        s = {self.entry}
        # a frame prologue (addiu sp,sp,-n) right after a return, 8-aligned as the compiler
        # places functions: a function nothing calls directly (an unlisted callback); without
        # it the function before would run on into it
        for i, w in enumerate(self.code):
            a = self.code_base + 4 * i
            if a % 8 == 0 and (w >> 16) == 0x27BD and (w & 0x8000) and self.after_return(a):
                s.add(self.code_base + 4 * i)
        for w in self.code:
            if w >> 26 == 3:
                t = (w & 0x3FFFFFF) << 2
                if self.in_code(t):
                    s.add(t)
        # code pointers the code builds (lui + addiu: callbacks passed as arguments)
        hi = {}
        for w in self.code:
            op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
            if op == 0x0F:
                hi[rt] = (w & 0xFFFF) << 16
                continue
            if op == 0x09 and rs in hi:
                t = (hi[rs] + sx16(w)) & 0xFFFFFFFF
                if self.in_code(t):
                    first = self.code[(t - self.code_base) // 4]
                    if (first >> 16) == 0x27BD and (first & 0x8000) or self.after_return(t):
                        s.add(t)
            d = gpr_written(w)
            if d is not None:
                hi.pop(d, None)
        for k, (dest, typ, b) in enumerate(self.secs):
            if k == 6 or typ != 1:
                continue
            for w in words(b):
                if self.in_code(w):
                    first = self.code[(w - self.code_base) // 4]
                    if k in (3, 4, 5) or (first >> 16) == 0x27BD and (first & 0x8000) or self.after_return(w):
                        s.add(w)
        return sorted(s)


BASE_END = 0x15EF00  # the boot executable's core and globals: the same addresses in every image
DROP_COLLISIONS = False  # (set by front_match.py)


def _core_ne(V, a, L, b):
    """Two addresses into the boot executable's core (or one of them) that are not the same object: a != b on the same
    core (each build's back to NTSC-U's when one core moved)."""
    return a != b if V.core_back is L.core_back else V.core_back(a) != L.core_back(b)


def same_strict(V, i, L, j, n):
    """Two bodies with the same masked words alike beyond the mask (the tie-break between copies that differ only in a
    masked field, e.g. two flag tests, or a structure's field): the offsets of the loads / stores / addiu whose base
    holds no address built by a lui (a structure's fields: not relocated) the same, and the addresses into the boot
    executable (below 0x15ef00: its globals, the game's flags, its library calls) the same."""
    if i < 0 or j < 0 or i + n > len(V.code) or j + n > len(L.code):
        return False
    hv, hl = {}, {}
    for k in range(n):
        x, y = V.code[i + k], L.code[j + k]
        if V.masked[i + k] != L.masked[j + k]:
            return False
        op, rs, rt = x >> 26, (x >> 21) & 31, (x >> 16) & 31
        if op == 3:
            tx, ty = (x & 0x3FFFFFF) << 2, (y & 0x3FFFFFF) << 2
            if (tx < V.core_end or ty < L.core_end) and _core_ne(V, tx, L, ty):
                return False
        elif op == 0x0F:
            hv[rt], hl[rt] = (x & 0xFFFF) << 16, (y & 0xFFFF) << 16
            continue
        elif (op in MEM or op in ADDI) and rs != SP:
            if rs == GP:
                ax, ay = (V.gp + sx16(x)) & 0xFFFFFFFF, (L.gp + sx16(y)) & 0xFFFFFFFF
            elif rs in hv and rs in hl:
                ax, ay = (hv[rs] + sx16(x)) & 0xFFFFFFFF, (hl[rs] + sx16(y)) & 0xFFFFFFFF
            elif (x & 0xFFFF) != (y & 0xFFFF):
                return False
            else:
                ax = ay = None
            if ax is not None and (ax < V.core_end or ay < L.core_end) and _core_ne(V, ax, L, ay):
                return False
        d = gpr_written(x)
        if d is not None:
            hv.pop(d, None)
            hl.pop(d, None)
    return True


def match(V, L, starts, K=6):
    """Veldin function -> level address, for the functions found once in level code."""
    idx = collections.defaultdict(list)
    for i in range(len(L.masked) - K + 1):
        idx[tuple(L.masked[i:i + K])].append(i)
    ends = starts[1:] + [V.code_end]
    span = {a: e - a for a, e in zip(starts, ends)}
    fmap, exact, amb, missing = {}, 0, {}, []
    for a, e in zip(starts, ends):
        i, n = (a - V.code_base) // 4, (e - a) // 4
        key = tuple(V.masked[i:i + K]) if n >= K else None
        if key is None:
            cands = []  # too short to place on its own
        else:
            cands = [j for j in idx.get(key, []) if L.masked[j:j + n] == V.masked[i:i + n]]
        if len(cands) == 1:
            fmap[a] = L.code_base + 4 * cands[0]
            if L.code[cands[0]:cands[0] + n] == V.code[i:i + n]:
                exact += 1
        elif cands:
            amb[a] = [L.code_base + 4 * j for j in cands]
        else:
            missing.append(a)
    # the calls of the uniquely matched functions: where the level's copy calls each Veldin
    # target (all its calls agreeing, the masked words the same there) -- before any guess
    # by the neighbours (an identical copy next to it, a short function)
    votes = collections.defaultdict(set)
    for a, b in fmap.items():
        i, j = (a - V.code_base) // 4, (b - L.code_base) // 4
        for k in range(span[a] // 4):
            wv = V.code[i + k]
            if wv >> 26 == 3:
                votes[(wv & 0x3FFFFFF) << 2].add((L.code[j + k] & 0x3FFFFFF) << 2)
    for a, e in zip(starts, ends):
        if a in fmap or len(votes.get(a, ())) != 1:
            continue
        t = next(iter(votes[a]))
        i, n, j = (a - V.code_base) // 4, (e - a) // 4, (t - L.code_base) // 4
        if 0 <= j and j + n <= len(L.masked) and L.masked[j:j + n] == V.masked[i:i + n]:
            fmap[a] = t
            if L.code[j:j + n] == V.code[i:i + n]:
                exact += 1
    # the identical copies: the one at the matched neighbours' displacement
    keys = sorted(fmap)
    for a, cands in amb.items():
        if a in fmap:
            continue
        k = bisect.bisect_left(keys, a)
        near = {fmap[keys[j]] - keys[j] for j in (k - 1, k) if 0 <= j < len(keys)}
        pick = [c for c in cands if c - a in near]
        if len(pick) > 1:
            i, n = (a - V.code_base) // 4, span[a] // 4
            pick = [c for c in pick if same_strict(V, i, L, (c - L.code_base) // 4, n)] or pick
        if len(pick) == 1:
            fmap[a] = pick[0]
    # the functions too short to place on their own (or not found): at a matched neighbour's
    # displacement, when the masked words there are the same
    keys = sorted(fmap)
    for a, e in zip(starts, ends):
        if a in fmap:
            continue
        i, n = (a - V.code_base) // 4, (e - a) // 4
        k = bisect.bisect_left(keys, a)
        ds = [d for d in sorted({fmap[keys[j]] - keys[j] for j in (k - 1, k) if 0 <= j < len(keys)})
              if 0 <= (a + d - L.code_base) // 4 and (a + d - L.code_base) // 4 + n <= len(L.masked)
              and L.masked[(a + d - L.code_base) // 4:(a + d - L.code_base) // 4 + n] == V.masked[i:i + n]]
        # (two displacements fit: the copy alike beyond the mask, e.g. the one testing the same flag)
        ds = [d for d in ds if same_strict(V, i, L, (a + d - L.code_base) // 4, n)] or ds
        if ds:
            j = (a + ds[0] - L.code_base) // 4
            fmap[a] = a + ds[0]
            if L.code[j:j + n] == V.code[i:i + n]:
                exact += 1
    # two functions placed on the same copy (identical masked bodies, one of them not in the level: e.g. a pair that
    # differs in a field's offset): the copy keeps the one alike beyond the mask, when only one is (the boot module's
    # map only, front_match.py: the levels' maps keep both, as the runtime's translation was proved with them)
    by = collections.defaultdict(list)
    for a, b in (fmap.items() if DROP_COLLISIONS else ()):
        by[b].append(a)
    for b, aa in by.items():
        if len(aa) < 2:
            continue
        keep = [a for a in aa if same_strict(V, (a - V.code_base) // 4, L, (b - L.code_base) // 4, span[a] // 4)]
        if len(keep) == 1:
            for a in aa:
                if a != keep[0]:
                    del fmap[a]
    missing = [a for a in missing if a not in fmap]
    sizes = {a: e - a for a, e in zip(starts, ends)}
    return fmap, exact, amb, missing, sizes


LOADS = {0x1A, 0x1B, 0x1E, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x37}  # into a GPR
IMM_ALU = {0x08, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x18}  # addi slti sltiu andi ori xori daddi


def gpr_written(w):
    """The general register an instruction writes (None: none, or not tracked)."""
    op = w >> 26
    if op in (0x00, 0x1C):
        return (w >> 11) & 31
    if op in LOADS or op in IMM_ALU or op in ADDI or op == 0x0F:
        return (w >> 16) & 31
    if op == 0x11 and ((w >> 21) & 31) in (0, 1, 2):  # mfc1 / dmfc1 / cfc1
        return (w >> 16) & 31
    if op == 0x12 and ((w >> 21) & 31) in (1, 2):  # qmfc2 / cfc2
        return (w >> 16) & 31
    if op == 0x03:
        return 31
    return None


def data_pairs(V, L, fmap, sizes):
    """Veldin data address -> level data address (with use counts), from the matched pairs: the
    addresses the code builds (lui, then addiu / the offset of a load or store on that register;
    $gp offsets); and the jal targets checked against the function map."""
    pairs = collections.defaultdict(collections.Counter)
    jal_bad = 0
    for a, b in fmap.items():
        i, j, n = (a - V.code_base) // 4, (b - L.code_base) // 4, sizes[a] // 4
        hv, hl = {}, {}  # register -> the address it holds (Veldin, level)
        # the registers at a forward branch's target: the state after the branch's delay slot
        # (a lui in the delay slot, the addiu at the target), merged with the fall-through
        pending = collections.defaultdict(list)
        branch_at, dead = None, False  # (delay slot index, target), fall-through unreachable
        for k in range(n):
            if k in pending:
                # a register is kept when the paths that hold it agree
                states = pending.pop(k) + ([] if dead else [(hv, hl)])
                merged = {}
                for sv, sl in states:
                    for r, v in sv.items():
                        merged.setdefault(r, set()).add((v, sl[r]))
                hv = {r: next(iter(s))[0] for r, s in merged.items() if len(s) == 1}
                hl = {r: next(iter(merged[r]))[1] for r in hv}
            elif dead:
                hv, hl = {}, {}
            dead = False
            wv, wl = V.code[i + k], L.code[j + k]
            op = wv >> 26
            rs, rt = (wv >> 21) & 31, (wv >> 16) & 31
            t = branch_target(wv, V.code_base + 4 * (i + k))
            if t is not None:
                branch_at = (k + 1, (t - V.code_base) // 4 - i, unconditional(wv))
            self_delay = branch_at is not None and branch_at[0] == k
            if op == 3:
                tv, tl = (wv & 0x3FFFFFF) << 2, (wl & 0x3FFFFFF) << 2
                if tv in fmap and fmap[tv] != tl:
                    jal_bad += 1
            pairs_k(pairs, wv, wl, op, rs, rt, hv, hl, V.gp, L.gp)
            if self_delay:
                _, tk, unc = branch_at
                if k < tk < n:
                    pending[tk].append((dict(hv), dict(hl)))
                dead = unc
                branch_at = None
    return pairs, jal_bad


def branch_target(w, pc):
    """A branch's or j's target (None: not one; jal / jr are not followed)."""
    op = w >> 26
    if op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or op == 1 and ((w >> 16) & 31) in (0, 1, 2, 3, 0x10, 0x11, 0x12, 0x13) \
            or op in (0x11, 0x12) and ((w >> 21) & 31) == 8:
        return (pc + 4 + (sx16(w) << 2)) & 0xFFFFFFFF
    if op == 2:
        return ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
    return None


def unconditional(w):
    """`b` (beq zero,zero) or `j`: the code after its delay slot is reached only by a jump."""
    return w >> 26 == 2 or (w >> 26 == 4 and (w >> 16) & 0x3FF == 0)


def pairs_k(pairs, wv, wl, op, rs, rt, hv, hl, gpv=GP_VALUE, gpl=GP_VALUE):
    """One instruction of a matched pair: the address it builds (recorded), the registers' state."""
    if op == 0x0F:
        hv[rt], hl[rt] = (wv & 0xFFFF) << 16, (wl & 0xFFFF) << 16
        return
    if op == 0 and (wv & 0x3F) in (0x21, 0x25, 0x2D) and 0 in (rs, rt):  # move (addu / or / daddu with zero)
        src, rd = rs or rt, (wv >> 11) & 31
        if src in hv:
            hv[rd], hl[rd] = hv[src], hl[src]
            return
    if (op in MEM or op in ADDI or op in ADDI_TRAP) and rs != SP:
        if rs == GP:
            av, al = gpv + sx16(wv), gpl + sx16(wl)
        elif rs in hv:
            av, al = (hv[rs] + sx16(wv)) & 0xFFFFFFFF, (hl[rs] + sx16(wl)) & 0xFFFFFFFF
        else:
            av = None
        if av is not None:
            pairs[av][al] += 1
            if op in ADDI or op in ADDI_TRAP:
                hv[rt], hl[rt] = av, al
                return
    d = gpr_written(wv)
    if d is not None:
        hv.pop(d, None)
        hl.pop(d, None)


def data_pointer_pairs(V, L, pairs):
    """More data pairs from the pointers the mapped data holds: objects only data
    points to (the pause menu's pages and widgets, reached through other widgets' tables) get no
    pair from the code. From each mapped pair (Veldin a, level b), the words a + k / b + k up to
    the next mapped Veldin address (the object's extent, at most 0x400 bytes) that are both
    data pointers pair their targets, followed in turn. A target paired two different ways is
    dropped. The code's pairs win."""
    known = {a: c.most_common(1)[0][0] for a, c in pairs.items()}
    keys = sorted(a for a in known if V.in_data(a))
    cand = collections.defaultdict(set)
    seen = set()
    queue = [(a, known[a]) for a in keys]
    while queue:
        a, b = queue.pop()
        if a in seen:
            continue
        seen.add(a)
        i = bisect.bisect_right(keys, a)
        end = min(keys[i] if i < len(keys) else a + 0x400, a + 0x400)
        for off in range(0, end - a, 4):
            wv, wl = V.data_word(a + off), L.data_word(b + off)
            if wv is None or wl is None:
                break
            if wv != 0 and V.in_data(wv) and L.in_data(wl) and wv not in known:
                cand[wv].add(wl)
                if len(cand[wv]) == 1:
                    queue.append((wv, wl))
    def alike(a, b):  # the two objects' first non-pointer words mostly equal (a pointer-like value is not a pointer)
        eq = tot = 0
        for k in range(0, 0x20, 4):
            x, y = V.data_word(a + k), L.data_word(b + k)
            if x is None or y is None:
                break
            if not (V.in_data(x) or V.in_code(x)):
                tot += 1
                eq += x == y
        return tot == 0 or 2 * eq >= tot
    return {a: next(iter(c)) for a, c in cand.items() if len(c) == 1 and alike(a, next(iter(c)))}


def ported_addresses():
    out = subprocess.run(["grep", "-rhoE", "orig: 0x[0-9a-fA-F]{8}", str(ROOT / "src/game")],
                         capture_output=True, text=True).stdout.split()
    return sorted({int(x, 16) for x in out if x.startswith("0x")})


REGIONS = [("base ELF (below 0x15ef00)", 0x100000, 0x15EF00), ("section 0 (0x15ef00..)", 0x15EF00, 0x161E00),
           ("section 1 bss (0x161e00..)", 0x161E00, 0x166000), ("section 2 data (0x166000..)", 0x166000, 0x1EA300)]


def map_overlays(V, L):
    """V's functions and data addresses in L: (starts, fmap, exact, amb, missing, sizes, pairs, jal_bad)."""
    starts = V.function_starts()
    fmap, exact, amb, missing, sizes = match(V, L, starts)
    pairs, jal_bad = data_pairs(V, L, fmap, sizes)
    # the level tables (sections 3 / 4 / 5: class -> update function, camera types, sound
    # handlers): their rows are walked from the section's start, so a row's address is the
    # same offset into the level's section (uses 0: from the layout, not from the code)
    for k in (3, 4, 5):
        (vd, _, vb), (ld, _, lb) = V.secs[k], L.secs[k]
        for off in range(0, min(len(vb), len(lb)), 4):
            if vd + off not in pairs:
                pairs[vd + off][ld + off] += 0
    return starts, fmap, exact, amb, missing, sizes, pairs, jal_bad


def map_lines(V, L, fmap, sizes, pairs):
    """The map file's fn / data lines (levelN.txt's format)."""
    lines = [f"fn {a:08x} {fmap[a]:08x} {sizes[a]:x}" for a in sorted(fmap)]
    for a in sorted(pairs):
        b, c = pairs[a].most_common(1)[0]
        lines.append(f"data {a:08x} {b:08x} {sum(pairs[a].values())}")
    for a, b in sorted(data_pointer_pairs(V, L, pairs).items()):  # (uses 0: reached through a data pointer)
        lines.append(f"data {a:08x} {b:08x} 0")
    return lines


def compare(V, L, level, out, ported):
    starts, fmap, exact, amb, missing, sizes, pairs, jal_bad = map_overlays(V, L)
    total = sum(sizes.values())
    mbytes = sum(sizes[a] for a in fmap)
    lines = []
    say = lines.append
    say(f"# level {level}: Veldin's overlay code matched in level {level}'s (tools/overlay_match, task 0300)")
    say(f"# Veldin code {V.code_base:08x}..{V.code_end:08x}, level {level} code {L.code_base:08x}..{L.code_end:08x}, "
        f"entry {V.entry:08x} -> {L.entry:08x} ({'matched' if fmap.get(V.entry) == L.entry else 'NOT matched'})")
    say(f"# Veldin functions {len(starts)} ({total} bytes): found once in level {level} {len(fmap)} "
        f"({mbytes} bytes, {100.0 * mbytes / total:.1f}%), byte-identical {exact}, "
        f"identical copies left ambiguous {len([a for a in amb if a not in fmap])}, not found {len(missing)}")
    covered = sum(sizes[a] for a in fmap)
    say(f"# level {level} code covered by Veldin's functions: {100.0 * covered / (4 * len(L.code)):.1f}%")
    deltas = collections.Counter(fmap[a] - a for a in fmap)
    say("# code displacements (most common): " + ", ".join(f"{d:+#x} x{c}" for d, c in deltas.most_common(6)))
    say(f"# jal targets inconsistent with the function map: {jal_bad}")
    for name, lo, hi in REGIONS:
        sel = [(a, c.most_common(1)[0][0]) for a, c in pairs.items() if lo <= a < hi]
        same = sum(1 for a, b in sel if a == b)
        d = collections.Counter(b - a for a, b in sel)
        say(f"# data {name}: {len(sel)} addresses, unchanged {same}, moved {len(sel) - same}; displacements "
            + ", ".join(f"{x:+#x} x{c}" for x, c in d.most_common(4)))
    # the level tables
    vrows, lrows = {r[0]: r for r in V.table_rows(3, 3)}, {r[0]: r for r in L.table_rows(3, 3)}
    both = set(vrows) & set(lrows)
    same_fn = sum(1 for c in both if fmap.get(vrows[c][1]) == lrows[c][1])
    say(f"# moby class table (section 3): Veldin {len(vrows)} classes, level {level} {len(lrows)}; in both {len(both)} "
        f"(update function the same code in {same_fn}); only in level {level}: {len(set(lrows) - set(vrows))}")
    lfn_mapped = sum(1 for c, r in lrows.items() if r[1] in set(fmap.values()))
    say(f"# level {level}'s class update functions with a Veldin equivalent: {lfn_mapped} of {len(lrows)}")
    if ported is not None:
        pv = [a for a in ported if V.code_base <= a < V.code_end]
        pm = [a for a in pv if a in fmap]
        say(f"# ported overlay functions (src/game orig:): {len(pv)}, with an equivalent in level {level}: {len(pm)}")
    lines += map_lines(V, L, fmap, sizes, pairs)
    out.mkdir(parents=True, exist_ok=True)
    (out / f"level{level}.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(l for l in lines if l.startswith("#")))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--iso", default=str(ROOT / "assets/iso/rac1-ntsc-u.iso"))
    ap.add_argument("--out", default=str(ROOT / "assets/generated/overlay_map"))
    ap.add_argument("--levels", default=",".join(str(i) for i in range(1, 19)))
    ap.add_argument("--ported", action="store_true", help="count the ported functions with an equivalent")
    a = ap.parse_args()
    ported = ported_addresses() if a.ported else None
    with open(a.iso, "rb") as iso:
        V = Overlay(*level_overlay(iso, VELDIN))
        for lv in [int(x) for x in a.levels.split(",") if x]:
            compare(V, Overlay(*level_overlay(iso, lv)), lv, Path(a.out), ported)


if __name__ == "__main__":
    main()
