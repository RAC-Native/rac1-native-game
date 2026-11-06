#!/usr/bin/env python3
"""Another region's overlay seen at NTSC-U addresses, for the code generators (docs/architecture.md, "The region layer").

The generators (tools/codegen/) translate the game's code at the NTSC-U addresses (SCUS-97199) the ports and the
runtime's region layer use. A player with only the PAL disc (SCES-50916) has the same code relinked elsewhere. This
module rebuilds, from the PAL overlay and the region table (data/regions/<serial>.txt), the NTSC-U layout of the
code: each code run is moved back to its NTSC-U address, and the fields the linker relocated are taken back through
the table:
  - jal / j targets;
  - a `lui` half and the offsets that complete it (addiu, loads, stores on that register), followed over the
    function's control flow (branch delay slots, likelies, calls clobbering the caller-saved registers);
  - $gp offsets;
  - the code pointers the generators read from the data (jump tables).
Nothing of the NTSC-U build is read: only the PAL disc and the table's addresses and shifts. On a disc whose core moved
(the 1.02 core of PAL 2.00: the table's unit `core`) the core's addresses are taken back too (the jal targets into it, the
addresses built into its data, the $gp offsets from the disc's own $gp).

A few fields the analysis cannot take back alone (a `lui` half built in another function, an address two runs claim) are
corrected by the table's `fix` lines; its `start` / `nostart` lines align the function starts with the NTSC-U build's,
and its `pair` lines pair the functions the PAL build rewrote, for the generators' maps. All of them are addresses and
shifts, written from both discs by `fixes` (region_maps.py table runs it).

The functions the PAL build changed (the table's gaps between code runs) have no NTSC-U code here. A listing of one
(tools/codegen/ovlist.py) gets the disc's own build of it instead, placed at its NTSC-U entry (StandIn): a temporary
stand-in, labelled in the generated code.

Usage (repo root):
  tools/overlay_match/region_view.py check --pal ISO --ntsc ISO [--table T] [--units boot,0,1,...]
      (both discs, a developer's check) the rebuilt NTSC-U code, jump tables and function starts against the NTSC-U disc's
  tools/overlay_match/region_view.py fixes --pal ISO --ntsc ISO --table T
      (both discs) rewrites the table's fix / start / nostart / pair lines
  tools/overlay_match/region_view.py maps --iso ISO [--table T] -o DIR
      (one disc + the table) the code generators' maps, DIR/level1..18.txt (setup.sh: overlay_map/codegen/)
"""
import argparse
import bisect
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import overlay_match as om  # noqa: E402
import front_match as fm  # noqa: E402
import region_maps as rm  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
NTSC_ELF = "SCUS_971.99"
CORE_END = 0x15EF00
GP = om.GP_VALUE
CALLER = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31)


# Where the game finds the disc (runtime/kernel/disc.cpp): $RAC1_ISO, else the setup's link, one name per region
ISO_LINKS = ("iso/rac1-ntsc-u.iso", "iso/rac1-pal.iso")


def table_path(serial, elf_sha1=None):
    """The region table of a disc: the one naming its boot executable's sha1, else data/regions/<serial>.txt."""
    t = rm.table_for(elf_sha1, serial) if elf_sha1 else None
    return t if t is not None else ROOT / "data/regions" / f"{serial}.txt"


def disc_path(assets):
    """The disc image the game and the generators read: $RAC1_ISO, else the first of the setup's links that exists."""
    if os.environ.get("RAC1_ISO"):
        return os.environ["RAC1_ISO"]
    for name in ISO_LINKS:
        p = os.path.join(assets, name)
        if os.path.exists(p):
            return p
    return os.path.join(assets, ISO_LINKS[0])


_disc_cache = {}


def disc_of(iso_path):
    """(serial, boot ELF name, the region table's path or None) of a disc image: None for the NTSC-U disc the ports are
    written against (and when the image cannot be read: the NTSC-U defaults)."""
    if iso_path not in _disc_cache:
        try:
            ser, elf = disc_info(iso_path)
        except (OSError, SystemExit):
            ser, elf = "SCUS-97199", NTSC_ELF
        tab = None
        if elf != NTSC_ELF:
            tab = table_path(ser, elf_sha1(iso_path, elf))
            if not tab.exists():
                raise SystemExit(f"[region_view] {iso_path} is {ser}: no region table {tab} for it")
            tab = str(tab)
        _disc_cache[iso_path] = (ser, elf, tab)
    return _disc_cache[iso_path]


def boot_elf_path(assets):
    """The boot ELF the setup extracted from the disc: assets/dumps/SCUS_971.99, or the PAL disc's SCES_509.16."""
    return os.path.join(assets, "dumps", disc_of(disc_path(assets))[1])


def elf_sha1(iso_path, elf):
    """The sha1 of the disc's boot executable (None when unreadable)."""
    try:
        with open(iso_path, "rb") as iso:
            b = rm.root_file(iso, elf)
        return rm.sha1(b) if b is not None else None
    except OSError:
        return None


def disc_info(iso_path):
    """(serial, boot ELF name) of the disc image, from its SYSTEM.CNF."""
    with open(iso_path, "rb") as iso:
        ser, elf, _vm = rm.disc_region(iso)
    return ser, elf


def field_value(w):
    """The relocatable field of an instruction word, as an address quantity: a jal / j target, a lui's page (<< 16),
    another instruction's 16-bit offset."""
    op = w >> 26
    if op in (2, 3):
        return (w & 0x3FFFFFF) << 2
    if op == 0x0F:
        return (w & 0xFFFF) << 16
    return _sx(w)


def apply_fix(w, d):
    """The word with its relocatable field moved by d (field_value's units)."""
    op = w >> 26
    if op in (2, 3):
        return (w & 0xFC000000) | ((((w & 0x3FFFFFF) << 2) + d) >> 2 & 0x3FFFFFF)
    if op == 0x0F:
        return (w & 0xFFFF0000) | ((((w & 0xFFFF) << 16) + d) >> 16 & 0xFFFF)
    return (w & 0xFFFF0000) | ((w + d) & 0xFFFF)


def read_fixes(path):
    """{unit: ({NTSC-U address: delta}, {start}, {nostart}, {Veldin: level or None})}: the table's `fix`, `start`, `nostart`
    and `pair` lines."""
    out, cur = {}, None
    for line in Path(path).read_text().splitlines():
        f = line.split()
        if f and f[0] == "overlay":
            cur = out.setdefault(f[1], ({}, set(), set(), {}))
        elif f and f[0] == "fix":
            cur[0][int(f[1], 16)] = int(f[2], 16)
        elif f and f[0] == "start":
            cur[1].add(int(f[1], 16))
        elif f and f[0] == "nostart":
            cur[2].add(int(f[1], 16))
        elif f and f[0] == "pair":
            cur[3][int(f[1], 16)] = int(f[2], 16) if f[2] != "-" else None
    return out


def read_units(path):
    """{unit: (Region, ntsc code base, ntsc code end, ntsc entry)} of the table ('boot', '0'..'18')."""
    _disc, units = rm.read_table(path)
    meta = {}
    for line in Path(path).read_text().splitlines():
        f = line.split()
        if f and f[0] == "overlay":
            meta[f[1]] = int(f[9], 16)  # "overlay N code lo hi -> lo hi entry E -> E'"
    fixes = read_fixes(path)
    core = rm.read_core(path)
    for k, r in units.items():
        r.fix, r.start, r.nostart, r.pair = fixes.get(k, ({}, set(), set(), {}))
        r.core = core
    return {k: (r, r.code_base, r.code_end, meta[k]) for k, r in units.items()}


def _sx(v):
    return (v & 0xFFFF) - ((v & 0x8000) << 1)


def _is_branch(w):
    op = w >> 26
    return op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or op == 1 and ((w >> 16) & 31) in (0, 1, 2, 3, 0x10, 0x11, 0x12, 0x13) \
        or op in (0x11, 0x12) and ((w >> 21) & 31) == 8


def _likely(w):
    op = w >> 26
    return op in (0x14, 0x15, 0x16, 0x17) or op == 1 and ((w >> 16) & 31) in (2, 3, 0x12, 0x13) \
        or op in (0x11, 0x12) and ((w >> 21) & 31) == 8 and ((w >> 16) & 31) in (2, 3)


def _uncond(w):
    return w >> 26 == 4 and (w >> 16) & 0x3FF == 0


def _base_use(w):
    """(base register) of an instruction whose 16-bit immediate is an offset on rs: addiu / daddiu / addi / daddi, loads,
    stores (overlay_match's masked set); None otherwise."""
    op = w >> 26
    if op in om.MEM or op in om.ADDI:  # (not addi / daddi: the same offset in both builds, never relocated)
        return (w >> 21) & 31
    return None


def _jump_targets(P, win):
    """The case addresses of the jump table a `jr` reads (tools/codegen/ovlist.py's idiom: sltiu bound; lui / addiu base),
    from P's own data."""
    tb = _jump_table(win)
    return [t for t in (P.data_word(tb[0] + 4 * k) for k in range(tb[1])) if t] if tb else []


def _jump_table(win):
    """(base, bound) of the jump table a `jr` reads, from the words before it; None when not the idiom."""
    bound = base = hi = None
    for w in reversed(win):
        if (w >> 26) == 0xB and bound is None:
            bound = w & 0xFFFF
    for w in win:
        op = w >> 26
        if op == 0xF:
            hi = (((w >> 16) & 31), (w & 0xFFFF) << 16)
        elif op == 9 and hi and ((w >> 21) & 31) == hi[0]:
            base = (hi[1] + _sx(w)) & 0xFFFFFFFF
    if not bound or base is None:
        return None
    return base, bound


class Back:
    """A PAL address of one unit back to its NTSC-U address, through the table's runs."""

    def __init__(self, region, pal):
        self.R, self.P = region, pal
        self.core = getattr(region, "core", None) or rm.Core()  # (the disc's core when it moved; else NTSC-U's)
        self.code = sorted((lo + d, hi + d, d) for lo, hi, d in region.code)
        self.code_lo = [c[0] for c in self.code]
        self.entry_rev = {p: n for n, p in region.entry.items()}
        # the data runs by their PAL address
        self.data = sorted((lo + d, hi + d, d) for lo, hi, d in region.data if hi - lo < 0x100000)
        self.data_lo = [x[0] for x in self.data]
        self.local = None  # (PAL lo, PAL hi, NTSC-U address of lo): a stand-in's own code
        self.tables = {}  # NTSC-U jump table address -> (PAL address, words)
        self.points = {}  # NTSC-U data address -> PAL: the addresses the code builds (lui + offset), as rebuilt

    def code_back(self, p):
        if self.local is not None and self.local[0] <= p < self.local[1]:  # (a stand-in's own code: see StandIn)
            return p - self.local[0] + self.local[2]
        i = bisect.bisect_right(self.code_lo, p) - 1
        if i >= 0 and p < self.code[i][1]:
            return p - self.code[i][2]
        return self.entry_rev.get(p)

    def data_back(self, p):
        """The NTSC-U data address at PAL p: inside a run (the nearest run starting at or below p wins), else the run
        below's displacement (an offset into its last object, or between two runs)."""
        i = bisect.bisect_right(self.data_lo, p) - 1
        if i < 0:
            return p
        # the innermost run holding p, else the nearest below
        for j in range(i, max(-1, i - 8), -1):
            lo, hi, d = self.data[j]
            if lo <= p <= hi:
                return p - d
        return p - self.data[i][2]

    def back(self, p):
        if p < self.core.end_p:
            return self.core.back(p) if self.core else p
        if self.P.code_base <= p < self.P.code_end:
            return self.code_back(p)
        n = self.data_back(p)
        if self.local is not None and self.R.data_fwd(n) != p:
            # (a stand-in: the NTSC-U address whose runtime translation, ovl_reloc.cpp's data_fwd, is p, the nearest one)
            cands = [m for m in range(p - 0x400, p + 0x400, 1) if self.R.data_fwd(m) == p]
            n = min(cands, key=lambda m: abs(m - n)) if cands else None
        return n


def unrelocate(P, back, lo_i, n):
    """The NTSC-U words of P's code words lo_i .. lo_i + n (one code run, PAL), its relocated fields taken back.
    Returns (words, unresolved count). The run is one control-flow graph: a switch's cases (which the matcher may count
    as function starts) get the state of their jump."""
    W = P.code[lo_i:lo_i + n]
    out = list(W)
    bad = _unreloc_fn(W, out, 0, n, P.code_base + 4 * lo_i, back, P)
    return out, bad


def _unreloc_fn(W, out, s, e, base_p, back, P):
    """One function's words W[s:e] (PAL addresses base_p + 4 i): the lui halves followed over its control flow."""
    n = e - s
    # successors (instruction index -> list)
    succ = [[] for _ in range(n)]
    delay_of = {}
    for i in range(n):
        w = W[s + i]
        pc = base_p + 4 * (s + i)
        if _is_branch(w) or w >> 26 == 2:
            t = om.branch_target(w, pc)
            ti = (t - base_p) // 4 - s if t is not None else None
            if i + 1 < n:
                delay_of[i + 1] = (ti, _uncond(w) or w >> 26 == 2)
                if _likely(w):
                    succ[i].append(i + 1)  # taken: the delay slot, then the target
                    if i + 2 < n:
                        succ[i].append(-(i + 2))  # not taken: the delay slot skipped (a marker: no delay transfer)
                else:
                    succ[i].append(i + 1)
            continue
        if w >> 26 == 0 and (w & 0x3F) == 8:  # jr: its delay slot, then away (a jump table: its cases)
            if i + 1 < n:
                delay_of[i + 1] = (None, True)
                succ[i].append(i + 1)
                if (w >> 21) & 31 != 31:
                    tb = _jump_table(W[max(s, s + i - 40):s + i])
                    if tb and not P.code_base <= tb[0] < P.code_end:
                        nb = back.back(tb[0])
                        if nb is not None:
                            back.tables[nb] = (tb[0], tb[1])
                    for tg in _jump_targets(P, W[max(s, s + i - 40):s + i]):
                        ti = (tg - base_p) // 4 - s
                        if 0 <= ti < n:
                            succ[i + 1].append(ti)
            continue
        if i in delay_of:
            ti, unc = delay_of[i]
            if ti is None and unc:
                continue
            if ti is not None and 0 <= ti < n:
                succ[i].append(ti)
            if not unc and i + 1 < n:
                succ[i].append(i + 1)
            continue
        if i + 1 < n:
            succ[i].append(i + 1)
    # the forward analysis: register -> frozenset of the lui instructions (indices) that may hold it; the register is
    # usable at a use when every path brings a lui (no None in the set)
    IN = [None] * n
    IN[0] = {}
    work = [0]
    after_call = set()
    for i in range(n):
        w = W[s + i]
        if (w >> 26 == 3 or w >> 26 == 0 and (w & 0x3F) == 9) and i + 1 < n:
            after_call.add(i + 1)  # the call's delay slot: the callee runs after it
    NONE = frozenset([None])

    def transfer(st, i):
        w = W[s + i]
        st = dict(st)
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op == 0x0F:
            st[rt] = frozenset([i])
        else:
            if op == 0 and (w & 0x3F) in (0x21, 0x25, 0x2D) and 0 in (rs, rt) and ((w >> 11) & 31) != 0:
                src = rs or rt
                st[(w >> 11) & 31] = st.get(src, NONE)
            else:
                d = om.gpr_written(w)
                if d is not None:  # (an address built from a lui and kept in a register is not followed: rare)
                    st[d] = NONE
        if i in after_call:
            for r in CALLER:
                st[r] = NONE
        return st

    def propagate(work, only_new=False):
        while work:
            i = work.pop()
            out_st = transfer(IN[i], i)
            for t in succ[i]:
                if t < 0:  # a likely not taken: from the branch's own state (the delay slot skipped)
                    t = -t
                    cand = dict(IN[i])
                else:
                    cand = out_st
                if IN[t] is None:
                    IN[t] = dict(cand)
                    work.append(t)
                elif not only_new:
                    keys = set(IN[t]) | set(cand)
                    new = {r: IN[t].get(r, NONE) | cand.get(r, NONE) for r in keys}
                    if new != IN[t]:
                        IN[t] = new
                        work.append(t)

    propagate(work)
    for i in range(n):  # code the entry does not reach (padding, an unrecognised jump table's cases): from nothing
        if IN[i] is None:  # known, without merging into the code the entry reaches
            IN[i] = {}
            propagate([i], only_new=True)
    # the uses: each lui's NTSC-U value from its uses, each use's offset
    lui_vals = {}  # lui index -> [NTSC-U hi candidates]
    bad = 0
    uses = []
    for i in range(n):
        if IN[i] is None:
            continue
        w = W[s + i]
        pc = base_p + 4 * (s + i)
        op = w >> 26
        if op in (2, 3):
            t = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            if t >= back.core.end_p:
                nt = back.code_back(t) if P.code_base <= t < P.code_end else None
                if nt is None:
                    bad += 1
                else:
                    out[s + i] = (w & 0xFC000000) | ((nt >> 2) & 0x3FFFFFF)
            elif back.core:  # (a call into the disc's core, which moved: its NTSC-U address)
                nt = back.core.back(t)
                if nt is None:
                    bad += 1
                else:
                    out[s + i] = (w & 0xFC000000) | ((nt >> 2) & 0x3FFFFFF)
            continue
        rb = _base_use(w)
        if rb is None or rb == 29:
            continue
        if rb == 28:
            p = (back.core.gp + _sx(w)) & 0xFFFFFFFF
            nn = back.back(p)
            if nn is None or not -0x8000 <= nn - GP < 0x8000:
                bad += 1
                continue
            out[s + i] = (w & 0xFFFF0000) | ((nn - GP) & 0xFFFF)
            continue
        src = IN[i].get(rb, NONE)
        if None in src or not src:
            continue
        his = {W[s + j] & 0xFFFF for j in src}
        if len(his) != 1:
            bad += 1
            continue
        h = his.pop()
        p = ((h << 16) + _sx(w)) & 0xFFFFFFFF
        if not (0x00100080 if back.core else CORE_END) <= p < 0x01000000:
            continue
        nn = back.back(p)
        if nn is None:
            bad += 1
            continue
        uses.append((i, src, nn))
        if not P.code_base <= p < P.code_end:
            back.points.setdefault(nn, p)
        for j in src:
            lui_vals.setdefault(j, []).append(nn)
    newhi = {}
    for j, ns in lui_vals.items():
        cands = {((x + 0x8000) >> 16) & 0xFFFF for x in ns}
        ok = [c for c in cands if all(-0x8000 <= x - (c << 16) < 0x8000 for x in ns)]
        if not ok:
            bad += 1
            continue
        old = W[s + j] & 0xFFFF
        newhi[j] = old if old in ok else min(ok, key=lambda c: abs(c - old))
        out[s + j] = (W[s + j] & 0xFFFF0000) | newhi[j]
    for i, src, nn in uses:
        hs = {newhi.get(j) for j in src}
        if None in hs or len(hs) != 1:
            bad += 1
            continue
        out[s + i] = (W[s + i] & 0xFFFF0000) | ((nn - (hs.pop() << 16)) & 0xFFFF)
    return bad


class RegionView(om.Overlay):
    """PAL overlay P of one unit at NTSC-U addresses (an om.Overlay for tools/codegen/ovlist.py and the matcher)."""

    def __init__(self, P, unit_meta):
        region, ncb, nce, nentry = unit_meta
        self.P, self.R = P, region
        self.back_ = Back(region, P)
        self.entry = nentry
        self.code_base, self.code_end = ncb, nce
        self.code = [0] * ((nce - ncb) // 4)
        self.known = bytearray(len(self.code))
        P._starts = P.function_starts()
        self.unresolved = 0
        for lo, hi, d in region.code:
            i, j = (lo + d - P.code_base) // 4, (lo - ncb) // 4
            n = (hi - lo) // 4
            words, bad = unrelocate(P, self.back_, i, n)
            self.code[j:j + n] = words
            self.known[j:j + n] = b"\1" * n
            self.unresolved += bad
        for a, dd in region.fix.items():  # the table's corrections: fields the flow above cannot take back
            i = (a - ncb) // 4
            if 0 <= i < len(self.code) and self.known[i]:
                self.code[i] = apply_fix(self.code[i], dd)
        self.masked = [om.mask(w) for w in self.code]
        self.secs = P.secs  # (PAL data: data_word / in_data translate)
        self.pts = sorted(self.back_.points.items())
        self.pts_lo = [x[0] for x in self.pts]
        self._standins = {}
        self.tbl = sorted(self.back_.tables.items())
        self.tbl_lo = [x[0] for x in self.tbl]

    def is_known(self, a):
        return self.code_base <= a < self.code_end and self.known[(a - self.code_base) // 4] == 1

    def standin(self, n):
        """The changed function whose NTSC-U entry is n, as this disc's own build of it (StandIn), or None."""
        if n in self.known_entries():
            if n not in self._standins:
                self._standins[n] = StandIn(self, n)
            return self._standins[n]
        return None

    def known_entries(self):
        return {n for n in self.R.entry if self.in_code(n) and not self.is_known(n)}

    def _fwd(self, a):
        if a < CORE_END:
            return (self.back_.core.fwd(a) or a) if self.back_.core else a
        if self.code_base <= a < self.code_end:
            return self.R.code_fwd(a)
        return self.R.data_fwd(a)

    def data_fwd(self, a):
        """PAL address of NTSC-U data address a: from the nearest address the code builds at or below it (a jump
        table's base and its words), else the table's runs."""
        if len(self.tbl) != len(self.back_.tables):  # (a stand-in's jump tables, found after the view was built)
            self.tbl = sorted(self.back_.tables.items())
            self.tbl_lo = [x[0] for x in self.tbl]
        k = bisect.bisect_right(self.tbl_lo, a) - 1
        if k >= 0 and a < self.tbl[k][0] + 4 * self.tbl[k][1][1]:
            return self.tbl[k][1][0] + (a - self.tbl[k][0])
        k = bisect.bisect_right(self.pts_lo, a) - 1
        if k >= 0 and a - self.pts[k][0] < 0x400:
            return self.pts[k][1] + (a - self.pts[k][0])
        return self._fwd(a)

    def data_word(self, a):
        p = self.data_fwd(a) if not self.code_base <= a < self.code_end else self._fwd(a)
        if p is None:
            return None
        w = self.P.data_word(p)
        if w is not None and self.P.in_code(w):
            for s in self._standins.values():  # (a jump table into a stand-in's own code)
                if s.local[0] <= w < s.local[1]:
                    return w - s.local[0] + s.local[2]
            nw = self.back_.code_back(w)
            if nw is not None:
                return nw
        return w

    def in_data(self, a):
        p = self._fwd(a)
        return p is not None and self.P.in_data(p)

    def table_rows(self, k, stride):
        return [tuple(self.back_.code_back(x) if self.P.in_code(x) and self.back_.code_back(x) else x for x in r)
                for r in self.P.table_rows(k, stride)]

    def function_starts(self):
        """The NTSC-U function starts (overlay_match's heuristic, which reads the code and the data): the disc's own
        starts taken back, the changed functions' entries, the code runs' edges, and the table's `start` / `nostart`
        corrections (a start only the NTSC-U data or code implies)."""
        pst = set(self.P._starts)
        s = {self.entry} | {n for n, p in self.R.entry.items() if self.in_code(n) and p in pst}
        s |= {n for n in (self.back_.code_back(x) for x in pst) if n is not None and self.in_code(n)}
        s |= {x for lo, hi, _d in self.R.code for x in (lo, hi) if self.in_code(x)}
        return sorted((s | self.R.start) - self.R.nostart)


class StandIn(om.Overlay):
    """A function the region changed, as the disc's own build of it (PAL), placed at its NTSC-U entry: its relocated fields
    taken back like the rest, its own addresses moved with it (NTSC-U entry + the offset in the PAL function). A temporary
    stand-in for the NTSC-U code this disc does not hold (the generators label it: tools/codegen/gen_mkall.py)."""

    def __init__(self, view, n):
        P, R = view.P, view.R
        p = R.entry[n]
        # its PAL extent: up to the next code run, or the next changed function's entry
        pst = set(P._starts)
        nxt = [lo + d for lo, _hi, d in R.code if lo + d > p] + [q for q in R.entry.values() if q > p and q in pst] + [P.code_end]
        end = min(nxt)
        self.view, self.pal, self.entry = view, p, n
        i0, cnt = (p - P.code_base) // 4, (end - p) // 4
        back = view.back_
        back.local = (p, end, n)
        try:
            words, self.unresolved = unrelocate(P, back, i0, cnt)
        finally:
            back.local = None
        self.code, self.code_base, self.code_end = words, n, n + 4 * cnt
        self.masked = [om.mask(w) for w in words]
        self.secs = P.secs
        self.local = (p, end, n)

    def is_known(self, a):
        return self.code_base <= a < self.code_end

    def function_starts(self):
        return [self.entry]

    def data_word(self, a):
        return self.view.data_word(a)


_units_cache = {}


def view(P, unit, table):
    """PAL overlay P (unit 'boot' or '0'..'18') at NTSC-U addresses, through the table."""
    if table not in _units_cache:
        _units_cache[table] = read_units(table)
    return RegionView(P, _units_cache[table][unit])


def level_view(iso_path, level, table):
    with open(iso_path, "rb") as iso:
        return view(om.Overlay(*om.level_overlay(iso, level)), str(level), table)


def boot_view(elf_bytes, table):
    return view(om.Overlay(fm.boot_module(elf_bytes), fm.boot_entry(elf_bytes)), "boot", table)


# ---- check (both discs) -----------------------------------------------------------------------

def cmd_check(a):
    table = a.table or str(table_path(disc_info(a.pal)[0], elf_sha1(a.pal, disc_info(a.pal)[1])))
    units = read_units(table)
    names = a.units.split(",") if a.units else ["boot"] + [str(i) for i in range(19)]
    total_bad = 0
    with open(a.ntsc, "rb") as ni, open(a.pal, "rb") as pi:
        for u in names:
            if u == "boot":
                ne, pe = rm.root_file(ni, rm.disc_region(ni)[1]), rm.root_file(pi, rm.disc_region(pi)[1])
                N = om.Overlay(fm.boot_module(ne), fm.boot_entry(ne))
                V = view(om.Overlay(fm.boot_module(pe), fm.boot_entry(pe)), "boot", table)
            else:
                N = om.Overlay(*om.level_overlay(ni, int(u)))
                V = view(om.Overlay(*om.level_overlay(pi, int(u))), u, table)
            if (N.code_base, N.code_end, N.entry) != (V.code_base, V.code_end, V.entry):
                print(f"  {u}: layout differs")
            known = sum(V.known)
            diff = [i for i in range(len(N.code)) if V.known[i] and N.code[i] != V.code[i]]
            total_bad += len(diff)
            print(f"  {u}: {known} of {len(N.code)} words rebuilt, {len(diff)} differ from the NTSC-U disc's, "
                  f"{V.unresolved} fields unresolved")
            for i in diff[: a.show]:
                ad = N.code_base + 4 * i
                print(f"      {ad:08x}: ntsc {N.code[i]:08x} rebuilt {V.code[i]:08x} (pal {V.P.code[(V.R.code_fwd(ad) - V.P.code_base) // 4]:08x})")
            # the jump tables the generators read (tools/codegen/ovlist.py's idiom), through the view
            jt_bad = jt_n = 0
            for i, w in enumerate(N.code):
                if w >> 26 == 0 and (w & 0x3F) == 8 and (w >> 21) & 31 != 31 and V.known[i]:
                    win = N.code[max(0, i - 40):i]
                    want = _jump_targets(N, win)
                    if want:
                        jt_n += 1
                        got = _jump_targets(V, V.code[max(0, i - 40):i])
                        if got != want:
                            jt_bad += 1
                            if jt_bad <= a.show:
                                print(f"      jump table at {N.code_base + 4 * i:08x} read differently")
            total_bad += jt_bad
            print(f"      jump tables: {jt_n}, read differently: {jt_bad}")
            st = set(N.function_starts()) ^ set(V.function_starts())
            total_bad += len(st)
            print(f"      function starts that differ: {len(st)}")
            # every code pointer of the NTSC-U data, through the view
            if a.data:
                bad_d = 0
                for k, (dest, typ, b) in enumerate(N.secs[:6]):
                    if typ != 1:
                        continue
                    for off, w in enumerate(om.words(b)):
                        if N.in_code(w) and V.is_known(w):
                            got = V.data_word(dest + 4 * off)
                            if got != w:
                                bad_d += 1
                print(f"      code pointers in the data read differently: {bad_d}")
    print(f"[region_view] {total_bad} rebuilt words differ")
    return 1 if total_bad else 0


def _pair(ni, pi, u, table):
    if u == "boot":
        ne, pe = rm.root_file(ni, rm.disc_region(ni)[1]), rm.root_file(pi, rm.disc_region(pi)[1])
        return om.Overlay(fm.boot_module(ne), fm.boot_entry(ne)), view(om.Overlay(fm.boot_module(pe), fm.boot_entry(pe)), "boot", table)
    return om.Overlay(*om.level_overlay(ni, int(u))), view(om.Overlay(*om.level_overlay(pi, int(u))), u, table)


def cmd_fixes(a):
    write_fixes(a.table, a.pal, a.ntsc)


def write_fixes(table, pal, ntsc):
    """(both discs) The table's `fix` lines: the relocated fields the rebuilt NTSC-U code still gets wrong (a `lui` half
    built in another function, a $gp offset two runs claim, a call to a function the region merged), as the NTSC-U
    address of the instruction and the shift of its field. Rewrites the table's fix lines."""
    lines = [l for l in Path(table).read_text().splitlines() if l.split()[:1] not in (["fix"], ["start"], ["nostart"], ["pair"])]
    tmp = Path(table).with_suffix(".nofix.tmp")
    tmp.write_text("\n".join(lines) + "\n")
    out, cur, fixes = [], None, {}
    with open(ntsc, "rb") as ni, open(pal, "rb") as pi:
        for u in ["boot"] + [str(i) for i in range(19)]:
            N, V = _pair(ni, pi, u, str(tmp))
            fx = []
            for i in range(len(N.code)):
                if V.known[i] and N.code[i] != V.code[i]:
                    if om.mask(N.code[i]) != om.mask(V.code[i]):
                        raise SystemExit(f"[region_view] {u} {N.code_base + 4 * i:08x}: not a relocated field")
                    fx.append((N.code_base + 4 * i, field_value(N.code[i]) - field_value(V.code[i])))
            a, b = set(N.function_starts()), set(V.function_starts())
            lines_u = ["fix %08x %+x" % x for x in fx]
            lines_u += ["start %08x" % x for x in sorted(a - b)]
            lines_u += ["nostart %08x" % x for x in sorted(b - a)]
            if u not in ("boot", "0"):
                N0, V0 = _pair(ni, pi, "0", str(tmp))
                _st, nf, _e, _a, _m, _s, _p, _b = om.map_overlays(N0, N)
                vf = codegen_fnmap(V0, V, with_table=False)[0]
                lines_u += ["pair %08x %08x" % (x, nf[x]) for x in sorted(nf) if vf.get(x) != nf[x]]
                lines_u += ["pair %08x -" % x for x in sorted(vf) if x not in nf]
            fixes[u] = lines_u
            print(f"  {u}: {len(lines_u)} lines")
    tmp.unlink()
    for l in lines:
        f = l.split()
        if f and f[0] == "overlay":
            if cur is not None:
                out += fixes.get(cur, [])
            cur = f[1]
        out.append(l)
    out += fixes.get(cur, [])
    Path(table).write_text("\n".join(out) + "\n")
    print(f"[region_view] wrote {sum(len(v) for v in fixes.values())} fix / start / nostart lines into {table}")


def codegen_fnmap(V, L, with_table=True):
    """NTSC-U Veldin's functions in NTSC-U level L, both rebuilt (views): (fmap, sizes, data pairs). No pairing of code the
    views do not hold; the functions the region changed paired as the disc's own builds of them pair, through the table's
    entries; then the table's `pair` lines (functions the disc cannot pair: the PAL build rewrote them)."""
    _st, fmap, _e, _a, _m, sizes, pairs, _b = om.map_overlays(V, L)
    fmap = {x: y for x, y in fmap.items() if V.is_known(x) and V.is_known(x + sizes[x] - 4)
            and L.is_known(y) and L.is_known(y + sizes[x] - 4)}
    pf = om.match(V.P, L.P, V.P._starts)[0]
    for p0, pn in pf.items():
        n0, nn = V.back_.entry_rev.get(p0), L.back_.entry_rev.get(pn)
        if n0 is not None and nn is not None and n0 in sizes:
            fmap.setdefault(n0, nn)
    if with_table:
        for x, y in L.R.pair.items():
            if y is None:
                fmap.pop(x, None)
            else:
                fmap[x] = y
    return fmap, sizes, pairs


def cmd_maps(a):
    """(one disc + the table) The code generators' overlay maps: NTSC-U Veldin's code matched in each NTSC-U level's,
    on the rebuilt NTSC-U code (levelN.txt, overlay_match's format, N = 1..18)."""
    out = Path(a.o)
    out.mkdir(parents=True, exist_ok=True)
    table = a.table or str(table_path(disc_info(a.iso)[0], elf_sha1(a.iso, disc_info(a.iso)[1])))
    V = level_view(a.iso, 0, table)
    for lv in range(1, 19):
        L = level_view(a.iso, lv, table)
        fmap, sizes, pairs = codegen_fnmap(V, L)
        lines = [f"# level {lv}: NTSC-U Veldin matched in NTSC-U level {lv}, both rebuilt from the {disc_info(a.iso)[0]} disc"
                 " (tools/overlay_match/region_view.py maps)"]
        lines += om.map_lines(V, L, fmap, sizes, pairs)
        (out / f"level{lv}.txt").write_text("\n".join(lines) + "\n")
    print(f"[region_view] wrote {out}/level1..18.txt")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("check")
    c.add_argument("--pal", default=str(ROOT / "assets/iso/Ratchet Clank.iso"))
    c.add_argument("--ntsc", default=str(ROOT / "assets/iso/rac1-ntsc-u.iso"))
    c.add_argument("--table")
    c.add_argument("--units", default="")
    c.add_argument("--show", type=int, default=6)
    c.add_argument("--data", action="store_true")
    f = sub.add_parser("fixes")
    f.add_argument("--pal", default=str(ROOT / "assets/iso/Ratchet Clank.iso"))
    f.add_argument("--ntsc", default=str(ROOT / "assets/iso/rac1-ntsc-u.iso"))
    f.add_argument("--table", required=True)
    m = sub.add_parser("maps")
    m.add_argument("--iso", required=True)
    m.add_argument("--table")
    m.add_argument("-o", required=True)
    a = ap.parse_args()
    if a.cmd == "maps":
        cmd_maps(a)
        return
    if a.cmd == "fixes":
        cmd_fixes(a)
        return
    sys.exit(cmd_check(a))


if __name__ == "__main__":
    main()
