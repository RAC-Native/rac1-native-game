#!/usr/bin/env python3
"""The ports' overlay maps for a disc of another region (docs/architecture.md, "The region layer").

The ports carry NTSC-U addresses (SCUS-97199: Veldin's overlay for OV / OVF, level N's for the
level-only ports, the boot module's for `front:`). On the NTSC-U disc tools/overlay_match matches
NTSC-U Veldin's code in every level. A player who owns only the PAL disc (SCES-50916) has no
NTSC-U code to match against. This tool closes the gap with a *region table*: for each overlay
(the boot module, levels 0..18) the NTSC-U address ranges and where the PAL build put them
(ranges of addresses and displacements, nothing of the game's bytes). With it:

  table   (both discs: the developer's side) writes the region table from the two discs:
          each overlay's NTSC-U code / data matched in the PAL overlay of the same level
          (relocations masked, tools/overlay_match's matcher), as runs of equal displacement.
  apply   (one disc + the table: the player's side) writes the maps the runtime reads, as
          tools/overlay_match / front_match write them on the NTSC-U disc:
            levelN.txt (N = 0..18)  NTSC-U Veldin -> the disc's level N: the disc's own Veldin
                                    matched in its level N (the same matcher, one disc),
                                    composed with the table's level 0 (NTSC-U Veldin -> the
                                    disc's Veldin); level 0 is the table's level 0 itself
            front_reloc.txt         NTSC-U Veldin -> the disc's boot module, the same way
            region_levelN.txt, region_boot.txt  the table's runs (NTSC-U level N / boot module ->
                                    the disc's), for the level-only and boot module ports
  check   (both discs) compares apply's maps with the direct ones (NTSC-U Veldin matched in the
          PAL overlays) on every address the ports name.
  check-core (both discs, a table with a core unit) the disc's core loaded at NTSC-U's addresses as the runtime loads it,
          against the NTSC-U core.
  check-levels (both discs) the level-only and boot module literals (L<n>_ADDR / L<n>_CASE /
          LvlConst, FR / FrConst) translated as the runtime does (region_levelN / region_boot runs)
          against the direct maps (NTSC-U level N / boot module matched in the PAL one).

A build whose core moved (the 1.02 core of NTSC-J and PAL 2.00: its data 0x100 bytes up, its code by up to 0x300)
gets a unit `core` too: the core's own code and data runs (the base ELF below the overlays, NTSC-U 0x100080..0x15ef00),
the disc's $gp (`gp`), and per unit `ptr a` lines: the disc's data words that point into its core (the runtime takes them
back to NTSC-U's addresses: src/game/ovl/region_core.cpp, which loads the disc's core there; a word that only looks like
such an address, two halfwords as 0x00130001, is not listed).

The table (committed as data/regions/<serial>.txt, or <serial>-<version>.txt for a later version of the same serial
(SCES-50916-2.00.txt); the table of a disc is the one whose `disc` line names its boot executable's sha1. Daino's
decision: addresses and
their shifts only, never bytes of code, data or text) has, per overlay, `code lo hi d` / `data lo hi d` runs
and `entry n p` lines: the entries of the functions the PAL build changed (no run covers them), so that
the NTSC-U port stands in for them until their PAL variants are ported; and `fix a d` lines for the code
generators (tools/overlay_match/region_view.py).

Usage (repo root):
  tools/overlay_match/region_maps.py table --pal ISO --ntsc ISO -o TABLE
  tools/overlay_match/region_maps.py apply --iso ISO --elf ELF --table TABLE -o OUTDIR
  tools/overlay_match/region_maps.py check --pal ISO --ntsc ISO --maps OUTDIR
  tools/overlay_match/region_maps.py check-levels --table data/regions/SCES-50916.txt
  tools/overlay_match/region_maps.py check-core --pal ISO --ntsc ISO --table data/regions/SCES-50916-2.00.txt
"""
import argparse
import bisect
import collections
import hashlib
import re
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import overlay_match as om  # noqa: E402
import front_match as fm  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SECTOR = 2048
CORE_END = 0x15EF00  # the base ELF below the boot module / overlays (NTSC-U): the same at the same addresses, unless
#                     the table has a `core` unit (the 1.02 core: Core below)


# ---- the disc ---------------------------------------------------------------------------------

def root_file(iso, name):
    """A file of the ISO's root directory (bytes), None when absent."""
    iso.seek(16 * SECTOR)
    pvd = iso.read(SECTOR)
    lba, size = struct.unpack_from("<I", pvd, 156 + 2)[0], struct.unpack_from("<I", pvd, 156 + 10)[0]
    iso.seek(lba * SECTOR)
    d = iso.read(size)
    o = 0
    while o < len(d):
        ln = d[o]
        if ln == 0:
            o = (o // SECTOR + 1) * SECTOR
            continue
        r = d[o:o + ln]
        if r[33:33 + r[32]].split(b";")[0] == name.encode():
            iso.seek(struct.unpack_from("<I", r, 2)[0] * SECTOR)
            return iso.read(struct.unpack_from("<I", r, 10)[0])
        o += ln
    return None


def disc_region(iso):
    """(serial, boot ELF name, vmode) from SYSTEM.CNF: ('SCUS-97199', 'SCUS_971.99', 'NTSC')."""
    cnf = root_file(iso, "SYSTEM.CNF")
    if cnf is None:
        raise SystemExit("[region_maps] no SYSTEM.CNF in the ISO's root: not a PS2 game disc")
    txt = cnf.decode("ascii", "replace")
    m = re.search(r"BOOT2\s*=\s*cdrom0:\\([A-Z]{4}_\d{3}\.\d{2})", txt)
    if not m:
        raise SystemExit(f"[region_maps] SYSTEM.CNF has no BOOT2 line: {txt!r}")
    elf = m.group(1)
    vm = re.search(r"VMODE\s*=\s*(\w+)", txt)
    return elf[:4] + "-" + elf[5:8] + elf[9:11], elf, vm.group(1) if vm else "NTSC"


def boot_overlay(elf):
    return om.Overlay(fm.boot_module(elf), fm.boot_entry(elf))


# ---- the core (the base ELF below the boot module), when the build moved it ---------------------

CORE_SECTIONS = (".vutext", "core.text", "core.data", "core.rdata", "core.bss", "core.lit")


def elf_sections(elf):
    """{name: (address, type, size, file offset)} of an ELF's sections."""
    shoff = struct.unpack_from("<I", elf, 0x20)[0]
    shnum, shstrndx = struct.unpack_from("<HH", elf, 0x30)
    secs = [struct.unpack_from("<IIIIII", elf, shoff + i * 40) for i in range(shnum)]
    stro = secs[shstrndx][4]
    return {elf[stro + n:elf.index(b"\0", stro + n)].decode(): (addr, typ, size, off) for n, typ, _f, addr, off, size in secs}


def elf_gp(elf):
    """The ELF's $gp (its .reginfo's ri_gp_value)."""
    _a, _t, _s, off = elf_sections(elf)[".reginfo"]
    return struct.unpack_from("<I", elf, off + 20)[0]


def elf_core_end(elf):
    """Where the ELF's core ends: its boot module's first section (.lit)."""
    return elf_sections(elf)[".lit"][0]


class Core:
    """A build's core against NTSC-U's: `code lo hi d` runs (NTSC-U functions lo..hi, end exclusive, at +d; the code
    between two runs is the build's own), `data lo hi d` runs (NTSC-U lo..hi, both included, at +d; the runs cover the
    core's data without gaps), the build's $gp and the end of its core. The identity (NTSC-U's own core) when empty."""

    def __init__(self):
        self.code, self.data = [], []
        self.gp, self.end_n, self.end_p = om.GP_VALUE, CORE_END, CORE_END

    def __bool__(self):
        return bool(self.code or self.data)

    def fwd(self, n):
        for lo, hi, d in self.code:
            if lo <= n < hi:
                return n + d
        for lo, hi, d in self.data:
            if lo <= n <= hi:
                return n + d
        return None

    def back(self, p):
        """The NTSC-U address of the build's core address p (None: the build's own code, or 8 bytes the build inserted)."""
        for lo, hi, d in self.code:
            if lo + d <= p < hi + d:
                return p - d
        found = None
        for lo, hi, d in self.data:  # (two runs claiming p: the later one, the object it starts)
            if lo + d <= p <= hi + d:
                found = p - d
        return found

    def overlay(self, ov):
        """Marks an overlay of this build: its $gp, core end and core translation (overlay_match's tie-breaks and data
        pairs)."""
        if self:
            ov.gp, ov.core_end, ov.core_back = self.gp, self.end_p, self._back
        return ov

    @property
    def _back(self):
        if not hasattr(self, "_bk"):
            self._bk = lambda a: self.back(a) if a < self.end_p else a
        return self._bk

    def lines(self):
        return (["gp %08x" % self.gp, "end %08x %08x" % (self.end_n, self.end_p)]
                + ["code %08x %08x %+x" % r for r in self.code] + ["data %08x %08x %+x" % r for r in self.data])


def core_code_runs(ne, pe):
    """The code runs of the core (.vutext, core.text): the masked words compared in order (a function the build inserted,
    removed or rewrote is a gap)."""
    import difflib
    sn, sp = elf_sections(ne), elf_sections(pe)
    runs = []
    for name in (".vutext", "core.text"):
        (an, _t, zn, on), (ap, _t2, zp, op) = sn[name], sp[name]
        wn, wp = [om.mask(w) for w in om.words(ne[on:on + zn])], [om.mask(w) for w in om.words(pe[op:op + zp])]
        for i, j, k in difflib.SequenceMatcher(None, wn, wp, autojunk=False).get_matching_blocks():
            if k < 8:  # (a few equal words inside a rewritten stretch)
                continue
            lo, hi, d = an + 4 * i, an + 4 * (i + k), (ap + 4 * j) - (an + 4 * i)
            if runs and runs[-1][2] == d and runs[-1][1] == lo:
                runs[-1][1] = hi
            else:
                runs.append([lo, hi, d])
    return [tuple(r) for r in runs]


def core_data_base(ne, pe):
    """The core's data runs from its sections alone: each section's displacement, from the first section's start to the
    core's end (the runs cover it without gaps)."""
    sn, sp = elf_sections(ne), elf_sections(pe)
    runs = []
    names = [n for n in CORE_SECTIONS[2:]]
    for k, name in enumerate(names):
        lo = sn[name][0]
        hi = (sn[names[k + 1]][0] if k + 1 < len(names) else elf_core_end(ne)) - 1
        runs.append((lo, hi, sp[name][0] - sn[name][0]))
    return runs


def core_data_refine(base, votes, units_min=2, near=0x40):
    """The core's data runs: the sections' (`base`), with the stretches whose addresses the matched code of at least
    `units_min` units builds at another displacement close to the section's (within `near`: an object the build moved a
    few bytes, such as the 1.02 game-state block 0x13d350.. at +0x108) cut out of them. `votes`: {NTSC-U address:
    {displacement: number of units}}. Other displacements are constants that only look like core addresses, or noise."""
    def base_d(a):
        return next((d for lo, hi, d in base if lo <= a <= hi), None)
    marks = []  # (address, displacement): the vote's winner, the section's or a near one
    for a in sorted(votes):
        bd = base_d(a)
        if bd is None:
            continue
        d, n = max(votes[a].items(), key=lambda x: (x[1], x[0] == bd))
        if n < units_min or d != bd and abs(d - bd) > near:
            continue
        marks.append((a, d))
    # (a mark of the section's displacement a few bytes inside a stretch of another one, both sides within 8 bytes: a
    # field the build added there, which the matched code reaches through a changed offset; not a stretch of its own)
    marks = [m for k, m in enumerate(marks)
             if not (0 < k < len(marks) - 1 and m[1] == base_d(m[0]) and marks[k - 1][1] == marks[k + 1][1] != m[1]
                     and m[0] - marks[k - 1][0] < 8 and marks[k + 1][0] - m[0] < 8)]
    # the stretches of a near displacement: from its first mark to the next mark of another displacement
    cuts = []
    for k, (a, d) in enumerate(marks):
        if d == base_d(a):
            continue
        if cuts and cuts[-1][2] == d and cuts[-1][3] == k - 1:
            cuts[-1][1], cuts[-1][3] = a, k
        else:
            cuts.append([a, a, d, k])
    out = []
    for lo, hi, d in base:
        pieces = [(lo, hi, d)]
        for clo, chi, cd, k in cuts:
            if not lo <= clo <= hi:
                continue
            nxt = marks[k + 1][0] if k + 1 < len(marks) else hi + 1  # (up to the next mark of another displacement)
            cend = min(hi, nxt - 1)
            last = pieces.pop()
            if last[0] < clo:
                pieces.append((last[0], clo - 1, last[2]))
            pieces.append((clo, cend, cd))
            if cend < last[1]:
                pieces.append((cend + 1, last[1], last[2]))
        out += pieces
    return out


def read_core(table):
    """The table's core unit (an empty Core: the core is NTSC-U's)."""
    c, inside = Core(), False
    for line in Path(table).read_text().splitlines():
        f = line.split()
        if not f:
            continue
        if f[0] == "overlay":
            inside = f[1] == "core"
        elif inside and f[0] in ("code", "data"):
            getattr(c, f[0]).append((int(f[1], 16), int(f[2], 16), int(f[3], 16)))
        elif inside and f[0] == "gp":
            c.gp = int(f[1], 16)
        elif inside and f[0] == "end":
            c.end_n, c.end_p = int(f[1], 16), int(f[2], 16)
    return c


def table_for(elf_sha1, serial=None):
    """The region table (data/regions/*.txt) whose `disc` line names this boot executable's sha1; else, by the serial,
    data/regions/<serial>.txt; None when there is none."""
    regions = ROOT / "data/regions"
    for t in sorted(regions.glob("*.txt")):
        for line in t.read_text().splitlines():
            if line.startswith("disc "):
                f = line.split()
                if "sha1" in f and f[f.index("sha1") + 1] == elf_sha1:
                    return t
                break
    if serial is not None and (regions / f"{serial}.txt").exists():
        return regions / f"{serial}.txt"
    return None


# ---- runs -------------------------------------------------------------------------------------

def code_runs(fmap, sizes):
    """[(lo, hi, delta)]: contiguous matched functions with the same displacement."""
    runs = []
    for a in sorted(fmap):
        d = fmap[a] - a
        if runs and runs[-1][2] == d and runs[-1][1] == a:
            runs[-1][1] = a + sizes[a]
        else:
            runs.append([a, a + sizes[a], d])
    return [tuple(r) for r in runs]


def data_runs(points):
    """[(lo, hi, delta)]: consecutive mapped data addresses with the same displacement (lo, hi the
    first and last address of the run; an address between two of them moves the same way)."""
    runs = []
    for a in sorted(points):
        d = points[a] - a
        if runs and runs[-1][2] == d:
            runs[-1][1] = a
        else:
            runs.append([a, a, d])
    return [tuple(r) for r in runs]


def changed_entries(V, L, fmap, sizes, pts, cr):
    """{NTSC-U entry: the disc's}: the functions the region changed (outside every code run) whose entry
    the matched code names: the jal targets of the matched functions, the code pointers it builds. The
    runtime lets the NTSC-U port stand in for them."""
    R = Region()
    R.code = cr
    seen = collections.defaultdict(collections.Counter)
    for a, b in fmap.items():
        i, j = (a - V.code_base) // 4, (b - L.code_base) // 4
        for k in range(sizes[a] // 4):
            wv, wl = V.code[i + k], L.code[j + k]
            if wv >> 26 == 3:
                tv, tl = (wv & 0x3FFFFFF) << 2, (wl & 0x3FFFFFF) << 2
                if V.in_code(tv) and R.code_fwd(tv) is None and L.in_code(tl):
                    seen[tv][tl] += 1
    for a, b in pts.items():
        if V.in_code(a) and R.code_fwd(a) is None and L.in_code(b):
            seen[a][b] += 1
    # the code pointers the data holds (a menu widget's functions, a callback table), word by word through the data runs
    for lo, hi, d in data_runs(pts):
        if hi - lo > 0x100000:
            continue  # (a matcher noise span)
        for a in range(lo & ~3, hi + 1, 4):
            x, y = V.data_word(a), L.data_word(a + d)
            if x is not None and y is not None and V.in_code(x) and R.code_fwd(x) is None and L.in_code(y):
                seen[x][y] += 1
    # the level tables (sections 3 / 4 / 5: class -> update function, camera types, sound handlers), row by row
    for k in (3, 4, 5):
        if k >= len(V.secs) or k >= len(L.secs):
            continue
        (_vd, _vt, vb), (_ld, _lt, lb) = V.secs[k], L.secs[k]
        vw, lw = om.words(vb), om.words(lb)
        for x, y in zip(vw, lw):
            if V.in_code(x) and R.code_fwd(x) is None and L.in_code(y):
                seen[x][y] += 1
    # (a word of data that only looks like a code address, such as "   " 0x00202020, moves by no displacement of the code)
    ds = [d for _lo, _hi, d in cr]
    lo_d, hi_d = min(ds) - 0x200, max(ds) + 0x40
    return {a: c.most_common(1)[0][0] for a, c in seen.items()
            if len(c) == 1 and c.most_common(1)[0][0] != a and lo_d <= c.most_common(1)[0][0] - a <= hi_d}


def gap_entries(V, L, cr, ent):
    """More entries of changed functions: between two code runs (a stretch of code the build changed), the NTSC-U
    function starts and the disc's, paired in order when there are as many of each (the build changed the functions
    there, not their number or order). The functions no matched code names (a level table's, a menu's callback) get
    their entry this way. {NTSC-U entry: the disc's}, without those `ent` has."""
    vs, ls = V.function_starts(), L.function_starts()
    out = {}
    runs = sorted(cr)
    edges = [(V.code_base, L.code_base)] + [(hi, hi + d) for _lo, hi, d in runs]
    ends = [(lo, lo + d) for lo, _hi, d in runs] + [(V.code_end, L.code_end)]
    for (n0, p0), (n1, p1) in zip(edges, ends):
        if n1 <= n0 or p1 <= p0:
            continue
        a = [x for x in vs if n0 <= x < n1]
        b = [x for x in ls if p0 <= x < p1]
        if a and len(a) == len(b):
            for x, y in zip(a, b):
                if x not in ent and y not in ent.values():
                    out[x] = y
    return out


def named_addresses():
    """Every code address the ports and the generators' inputs name (`orig:` annotations, tools/codegen/gen/mkall/*.json
    entries): the candidates gap_window_entries looks for."""
    out = subprocess.run(["grep", "-rhoE", r"(orig: (L[0-9]+:|front:)?0x[0-9a-fA-F]{6,8}|\"[0-9a-f]{6}:)",
                          str(ROOT / "src/game"), str(ROOT / "tools/codegen/gen/mkall")], capture_output=True, text=True).stdout
    rows = subprocess.run(["grep", "-rhoE", r"\{0x[0-9a-fA-F]{8}, 0x[0-9a-fA-F]{8}", *map(str, sorted((ROOT / "src/game/ovl").glob("variants_l*.cpp")))],
                          capture_output=True, text=True).stdout  # (the levels' variant rows: {Veldin, level, size})
    return {int(m.group(1), 16) for m in re.finditer(r"(?:0x|\")([0-9a-fA-F]{6,8})", out)} | \
        {int(m.group(1), 16) for m in re.finditer(r", 0x([0-9a-fA-F]{8})", rows)}


def gap_window_entries(V, L, cr, ent, named, k=8):
    """More entries of changed functions: an address inside a stretch the build changed that the ports or the generators
    name (an entry no function start heuristic finds, a callback in the middle of a function), paired with the one place
    of the disc's stretch whose k masked words are the NTSC-U address's. Written `entry n p inner`: the runtime does not
    take them as the start of a stand-in's code (ovl_reloc.cpp's region_to_ntsc)."""
    out = {}
    runs = sorted(cr)
    edges = [(V.code_base, L.code_base)] + [(hi, hi + d) for _lo, hi, d in runs]
    ends = [(lo, lo + d) for lo, _hi, d in runs] + [(V.code_end, L.code_end)]
    for (n0, p0), (n1, p1) in zip(edges, ends):
        for x in sorted(a for a in named if n0 <= a < n1 and a % 4 == 0 and a not in ent):
            i = (x - V.code_base) // 4
            w = V.masked[i:i + k]
            if len(w) < k:
                continue
            j0, j1 = (p0 - L.code_base) // 4, (p1 - L.code_base) // 4
            hits = [j for j in range(j0, j1 - k + 1) if L.masked[j:j + k] == w]
            if len(hits) == 1:
                out[x] = L.code_base + 4 * hits[0]
    return out


def literal_points(V, L, pts, lits, window=0x40):
    """{a: b}: the ports' data literals `lits` (FR / L<n>_ADDR) that the runs of `pts` leave untranslated, placed by
    their bytes: of the displacements of the runs below and above, the one under which the NTSC-U words a..a+window
    equal the disc's (all of them there, not all zero, one displacement only). The objects only the ports name (the
    title's sound lists 0x186100 / 0x1861e0 / 0x1862b0 in the boot module of the 1.02 build, whose own title code
    changed: no matched code names them) get a point, and the run they sit in then covers them."""
    R = Region()
    R.data = data_runs(pts)
    out = {}
    for x in sorted(lits):
        if x < CORE_END or x >= V.code_base:
            continue
        # (a section's start, a bss one with no bytes to compare: the disc's same section's start, when the two sections
        # are of the same kind and size, whatever the runs say: the runs' "offset into the object below" would put it
        # there. Veldin's bss 0x161e00, which every level's start clears from: the 1.02 build's own clear changed, so no
        # matched code names it, and the data run below, +0xd0, took it 0x30 short of the disc's bss)
        k = next((k for k, (dest, _t, _b) in enumerate(V.secs[:6]) if dest == x), None)
        if k is not None and k < len(L.secs) and V.secs[k][1] == L.secs[k][1] and len(V.secs[k][2]) == len(L.secs[k][2]):
            if R.data_fwd(x) != L.secs[k][0]:
                out[x] = L.secs[k][0]
            continue
        i = bisect.bisect_right(R.data, (x, 0xFFFFFFFF, 0))
        cands = {R.data[k][2] for k in (i - 1, i) if 0 <= k < len(R.data)}
        cur = R.data_fwd(x)
        if cur is not None:
            # (translated by the runs, but into a gap between two of them: kept where its bytes are there; else, of the
            # displacements from the run below's to the run above's, the one under which its bytes are the disc's. The
            # 1.02 build put 8 bytes before the screens' box templates 0x1e7a50 / 0x1e7a68, which only screen_draw names
            # (a changed function): the run below's displacement read them 8 bytes early, #165)
            if len(cands) < 2 or not (0 < i < len(R.data)) or x <= R.data[i - 1][1]:
                continue
            for w in (window, window // 2, window // 4):
                nw = [V.data_word(x + k) for k in range(0, w, 4)]
                if None in nw or sum(1 for v in nw if v) < 4:
                    break
                if [L.data_word(cur + k) for k in range(0, w, 4)] == nw:
                    break
                lo, hi = min(cands), max(cands)
                hits = [d for d in range(lo, hi + 1, 4) if [L.data_word(x + d + k) for k in range(0, w, 4)] == nw]
                if len(hits) == 1:
                    out[x] = x + hits[0]
                    break
                if hits:
                    break
            continue
        nw = [V.data_word(x + k) for k in range(0, window, 4)]
        if None in nw or not any(nw):
            continue
        hits = [d for d in sorted(cands) if [L.data_word(x + d + k) for k in range(0, window, 4)] == nw]
        if len(hits) == 1:
            out[x] = x + hits[0]
    return out


def map_points(V, L, fmap, sizes, pairs):
    """The data lines of a map as {a: b} (overlay_match's choice: the most common pairing)."""
    pts = {a: c.most_common(1)[0][0] for a, c in pairs.items()}
    for a, b in om.data_pointer_pairs(V, L, pairs).items():
        pts.setdefault(a, b)
    return pts


# ---- table ------------------------------------------------------------------------------------

def sha1(b):
    return hashlib.sha1(b).hexdigest()


def core_moved(ne, pe):
    """The build's core is not NTSC-U's at the same addresses (its sections, its $gp or its end moved)."""
    sn, sp = elf_sections(ne), elf_sections(pe)
    return any(sn[n][:3] != sp[n][:3] for n in CORE_SECTIONS) or elf_gp(ne) != elf_gp(pe) or elf_core_end(ne) != elf_core_end(pe)


def make_core(ne, pe, units):
    """The core unit of a build whose core moved: the code runs, the data runs (the sections' displacements refined by the
    data pairs of every unit's matched code), its $gp and end; `units` [(name, V, L)] get marked with it."""
    core = Core()
    core.code, core.data = core_code_runs(ne, pe), core_data_base(ne, pe)
    core.gp, core.end_p = elf_gp(pe), elf_core_end(pe)
    for _name, _V, L in units:
        core.overlay(L)
    votes = collections.defaultdict(collections.Counter)
    lo_data = core.data[0][0]
    for name, V, L in units:
        pairs = om.map_overlays(V, L)[6]
        for x, c in pairs.items():
            if lo_data <= x < CORE_END:
                votes[x][c.most_common(1)[0][0] - x] += 1
    core.data = core_data_refine(core.data, votes)
    return core


def data_back_runs(runs):
    """A function: an address of the disc's data back to NTSC-U's through a unit's data runs (region_view.Back's
    data_back: the innermost run holding it, else the nearest run below's displacement)."""
    rs = sorted((lo + d, hi + d, d) for lo, hi, d in runs if hi - lo < 0x100000)
    los = [r[0] for r in rs]

    def back(p):
        i = bisect.bisect_right(los, p) - 1
        if i < 0:
            return None
        for j in range(i, max(-1, i - 8), -1):
            lo, hi, d = rs[j]
            if lo <= p <= hi:
                return p - d
        return p - rs[i][2]
    return back


def core_pointers(core, words_p, word_p, word_n, back_pos, disps):
    """The `ptr` addresses of one unit (or of the core itself): the disc's data words (words_p: [(address, word)]; word_p
    reads one) whose value is an address of the disc's core: the NTSC-U word at the same place (word_n(back_pos(q))) is
    that address taken back. Where the unit's runs do not say where the place is (data no code names), the NTSC-U data is
    aligned on the 16 words around it, at each displacement of the unit's runs (`disps`). A word whose value only looks
    like such an address (two halfwords, 0x00130001) is not listed. Returns (pointers, counts)."""
    def alike(lw, vw):
        if lw is None or vw is None:
            return False
        if 0x100080 <= lw < core.end_p:
            return vw == lw or vw == core.back(lw)
        return vw == lw
    ptrs, n = [], collections.Counter()
    for q, v in words_p:
        if not 0x100080 <= v < core.end_p:
            continue
        b = core.back(v)
        if b is None:
            n["not pointers (no NTSC-U address)"] += 1
            continue
        pn = back_pos(q)
        u = word_n(pn) if pn is not None else None
        if u == b:
            n["pointers"] += 1
            ptrs.append(q)
            continue
        if u == v and b != v:
            n["not pointers"] += 1
            continue
        score = {d: sum(alike(word_p(q + 4 * k), word_n(q - d + 4 * k)) for k in range(-8, 9) if k) for d in disps}
        d = max(score, key=score.get)
        u = word_n(q - d) if score[d] >= 12 else None
        if u == b and b != v:
            n["pointers (aligned)"] += 1
            ptrs.append(q)
        elif u == v or u == b:
            n["not pointers (aligned)"] += 1
        else:
            n["unresolved: left as they are"] += 1
    return ptrs, n


def sec_words(secs):
    """[(address, word)] of an overlay's data sections (0..5 with contents)."""
    out = []
    for dest, typ, b in secs[:6]:
        if typ == 1:
            out += [(dest + 4 * i, w) for i, w in enumerate(om.words(b))]
    return out


def cmd_table(a):
    with open(a.ntsc, "rb") as ni, open(a.pal, "rb") as pi:
        nser, nelf, _ = disc_region(ni)
        pser, pelf, pvm = disc_region(pi)
        ne, pe = root_file(ni, nelf), root_file(pi, pelf)
        lines = [
            f"# The {pser} build's layout against {nser}'s: where each NTSC-U code / data range of the boot",
            "# module and of every level's overlay sits in the PAL build (tools/overlay_match/region_maps.py table).",
            "# Addresses and displacements only. `code lo hi d`: the NTSC-U functions lo..hi (end",
            "# exclusive) are at +d; `data lo hi d`: the NTSC-U data addresses lo..hi (both mapped) are at +d;",
            "# `entry n p`: the entry of a function the region changed; `fix a d`: the relocated field of the",
            "# NTSC-U instruction at a, moved by d where the code generators cannot take it back alone",
            "# (tools/overlay_match/region_view.py).",
            f"disc {pser} {pelf} {pvm} size {len(pe)} sha1 {sha1(pe)}",
        ]
        units = [("boot", boot_overlay(ne), boot_overlay(pe))]
        for lv in range(19):
            units.append((str(lv), om.Overlay(*om.level_overlay(ni, lv)), om.Overlay(*om.level_overlay(pi, lv))))
        core = make_core(ne, pe, units) if core_moved(ne, pe) else Core()
        named = named_addresses() if core else set()
        lits = level_literals()
        veldin = veldin_literals()
        if core:
            # the core moved (the 1.02 core): its own unit, loaded at NTSC-U's addresses by the runtime
            lines.append("core 00100080 %08x -> 00100080 %08x moved: the unit `core`, and `ptr` lines per unit" % (CORE_END, core.end_p))
            sn, sp = elf_sections(ne), elf_sections(pe)
            tn, tp = sn["core.text"], sp["core.text"]
            lines.append(f"overlay core code {tn[0]:08x} {tn[0] + tn[2]:08x} -> {tp[0]:08x} {tp[0] + tp[2]:08x}"
                         f" entry {struct.unpack_from('<I', ne, 0x18)[0]:08x} -> {struct.unpack_from('<I', pe, 0x18)[0]:08x}")
            lines += core.lines()
            lo_p, hi_p = core.data[0][0] + core.data[0][2], core.end_p
            ptrs, cnt = core_pointers(core, [(q, _elf_word(pe, q)) for q in range(lo_p, hi_p, 4) if _elf_word(pe, q) is not None],
                                      lambda q: _elf_word(pe, q), lambda n: _elf_word(ne, n), core.back,
                                      sorted({d for _lo, _hi, d in core.data}))
            lines += ["ptr %08x" % q for q in ptrs]
            print(f"[region_maps] core: {len(core.code)} code runs, {len(core.data)} data runs; its data's words in the core: "
                  + ", ".join(f"{k} {v}" for k, v in sorted(cnt.items())))
        else:
            # the core: the same bytes at the same addresses but the boot module's entry (core main)
            core_diff = [x for x in range(0x100080, CORE_END, 4) if _elf_word(ne, x) != _elf_word(pe, x)]
            lines.append("core 00100080 %08x same, but " % CORE_END + " ".join("%08x" % x for x in core_diff))
        for name, V, L in units:
            _starts, fmap, _exact, _amb, missing, sizes, pairs, _bad = om.map_overlays(V, L)
            pts = map_points(V, L, fmap, sizes, pairs)
            if core:  # (the core's addresses: the core unit's)
                pts = {x: y for x, y in pts.items() if x >= CORE_END}
            # (the data only the ports name, which no matched code reaches: placed by its bytes)
            lp = literal_points(V, L, pts, lits.get(name, set()) | (veldin if name == "0" else set()))
            pts.update(lp)
            if lp:
                print(f"[region_maps] {name}: {len(lp)} literals of the ports placed by their bytes: "
                      + ", ".join("%08x %+x" % (x, y - x) for x, y in sorted(lp.items())))
            cr, dr = code_runs(fmap, sizes), data_runs(pts)
            ent = changed_entries(V, L, fmap, sizes, pts, cr)
            inner = {}
            if core:  # (the 1.02 build changed many functions no matched code names)
                ent.update(gap_entries(V, L, cr, ent))
                inner = gap_window_entries(V, L, cr, ent, named)
                ent.update(inner)
                # (an entry that starts no function of the disc's, a `case` of a jump table the data holds: inside a
                # stand-in, whose labels are its NTSC-U entry plus the offset in the disc's function (the runtime's
                # region_to_ntsc finds them so); kept, as `inner`, when the ports or the generators name it)
                lstarts = set(L.function_starts())
                for x in [x for x, y in ent.items() if y not in lstarts and x not in inner]:
                    if x in named:
                        inner[x] = ent[x]
                    else:
                        del ent[x]
            lines.append(f"overlay {name} code {V.code_base:08x} {V.code_end:08x} -> {L.code_base:08x} {L.code_end:08x}"
                         f" entry {V.entry:08x} -> {L.entry:08x}")
            lines += ["code %08x %08x %+x" % r for r in cr]
            lines += ["data %08x %08x %+x" % r for r in dr]
            lines += [("entry %08x %08x inner" if core and e[0] in inner else "entry %08x %08x") % e for e in sorted(ent.items())]
            kmsg = ""
            if core:
                ptrs, cnt = core_pointers(core, sec_words(L.secs), L.data_word, V.data_word, data_back_runs(dr),
                                          sorted({d for _lo, _hi, d in dr} | {0}))
                lines += ["ptr %08x" % q for q in ptrs]
                kmsg = "; data words in the core: " + ", ".join(f"{k} {v}" for k, v in sorted(cnt.items()))
            print(f"[region_maps] {name}: {len(fmap)} functions matched, {len(missing)} not: {len(cr)} code runs, {len(dr)} data runs,"
                  f" {len(ent)} entries of changed functions{kmsg}")
    Path(a.o).parent.mkdir(parents=True, exist_ok=True)
    Path(a.o).write_text("\n".join(lines) + "\n")
    print(f"[region_maps] wrote {a.o}")
    import region_view  # (the code generators' corrections: `fix` lines)
    region_view.write_fixes(a.o, a.pal, a.ntsc)


def _elf_word(elf, addr):
    shoff = struct.unpack_from("<I", elf, 0x20)[0]
    for i in range(struct.unpack_from("<H", elf, 0x30)[0]):
        _n, typ, _f, sa, off, size = struct.unpack_from("<IIIIII", elf, shoff + i * 40)
        if typ == 1 and sa <= addr < sa + size:
            return struct.unpack_from("<I", elf, off + addr - sa)[0]
    return None


# ---- apply ------------------------------------------------------------------------------------

class Region:
    """One overlay's runs: NTSC-U -> the disc's, and back."""

    def __init__(self):
        self.code, self.data, self.entry, self.ptr, self.inner = [], [], {}, [], set()
        self.code_base = self.code_end = 0

    def code_fwd(self, n):
        for lo, hi, d in self.code:
            if lo <= n < hi:
                return n + d
        return self.entry.get(n)

    def code_back(self, p):
        for lo, hi, d in self.code:
            if lo <= p - d < hi:
                return p - d
        for n, q in self.entry.items():
            if q == p:
                return n
        return None

    def data_fwd(self, n):
        """The runtime's translation of an NTSC-U data address (ovl_reloc.cpp's RegionRuns::data_fwd):
        inside a run, its displacement; between two runs of the same displacement, that one; else the
        run below's when the address is less than 0x100 above its end (an offset into its last object);
        None (kept) otherwise."""
        i = bisect.bisect_right(self.data, (n, 0xFFFFFFFF, 0)) - 1
        if i < 0:
            return None
        lo, hi, d = self.data[i]
        if n <= hi:
            return n + d
        if i + 1 < len(self.data) and self.data[i + 1][2] == d:
            return n + d
        return n + d if n - hi < 0x100 else None

    def data_back(self, p):
        """Every NTSC-U address a run puts at p (a run is a range of equal displacement)."""
        return [p - d for lo, hi, d in self.data if lo <= p - d <= hi]


def read_table(path):
    """(the disc line's fields, {overlay name: Region}); each Region also has .code_base (NTSC-U)."""
    disc, units, cur = None, {}, None
    for line in Path(path).read_text().splitlines():
        f = line.split()
        if not f or f[0].startswith("#"):
            continue
        if f[0] == "disc":
            disc = f[1:]
        elif f[0] == "overlay":
            cur = units.setdefault(f[1], Region())
            cur.code_base, cur.code_end = int(f[3], 16), int(f[4], 16)
        elif f[0] in ("code", "data"):
            getattr(cur, f[0]).append((int(f[1], 16), int(f[2], 16), int(f[3], 16)))
        elif f[0] == "entry":
            cur.entry[int(f[1], 16)] = int(f[2], 16)
            if f[3:4] == ["inner"]:
                cur.inner.add(int(f[1], 16))
        elif f[0] == "ptr":
            cur.ptr.append(int(f[1], 16))
    return disc, units


def compose(T0, V, L, label, core=None):
    """NTSC-U Veldin -> L: the disc's Veldin V matched in L (one disc), its addresses taken back to
    NTSC-U Veldin's through the table's level 0 runs. (`core`: the table's core unit when the disc's core moved: its
    addresses are NTSC-U's at run time, the runtime loads it there.)"""
    core = core or Core()
    _starts, fmap, _exact, _amb, _missing, sizes, pairs, _bad = om.map_overlays(V, L)
    fn, lost = {}, 0
    for p, q in fmap.items():
        n = T0.code_back(p)
        if n is None:
            lost += 1  # a PAL-only function of Veldin (no NTSC-U equivalent)
            continue
        fn[n] = (q, sizes[p])
    data = {}
    for p, q in map_points(V, L, fmap, sizes, pairs).items():
        if p < core.end_p:
            if not core:
                data[p] = q
            elif core.back(p) is not None:
                data[core.back(p)] = core.back(p)
            continue
        for n in T0.data_back(p):
            data.setdefault(n, q)
    return fn, data, lost


def write_map(path, header, fn, data):
    lines = [f"# {h}" for h in header]
    lines += [f"fn {n:08x} {q:08x} {s:x}" for n, (q, s) in sorted(fn.items())]
    lines += [f"data {n:08x} {q:08x} 0" for n, q in sorted(data.items())]
    path.write_text("\n".join(lines) + "\n")


def cmd_apply(a):
    out = Path(a.o)
    out.mkdir(parents=True, exist_ok=True)
    disc, units = read_table(a.table)
    core = read_core(a.table)
    elf = Path(a.elf).read_bytes()
    with open(a.iso, "rb") as iso:
        ser, ename, _vm = disc_region(iso)
        if disc is None or disc[0] != ser:
            raise SystemExit(f"[region_maps] the table is for {disc[0] if disc else '?'}, the disc is {ser}")
        if "sha1" in disc and disc[disc.index("sha1") + 1] != sha1(elf):
            raise SystemExit(f"[region_maps] {ename} is not the build the table was made from (sha1 differs)")
        T0 = units["0"]
        V = core.overlay(om.Overlay(*om.level_overlay(iso, 0)))
        # level 0: the table's runs themselves (fn lines at the disc's own function starts)
        starts = V.function_starts()
        ends = starts[1:] + [V.code_end]
        fn = {}
        for p, e in zip(starts, ends):
            n = T0.code_back(p)
            if n is not None:
                fn[n] = (p, e - p)
        data = {}
        for lo, hi, d in T0.data:
            data[lo], data[hi] = lo + d, hi + d
        # (every address the disc's Veldin code builds, back to NTSC-U: more points inside the runs)
        _s, idmap, _e, _a, _m, sizes, pairs, _b = om.map_overlays(V, V)
        for p in map_points(V, V, idmap, sizes, pairs):
            if p >= core.end_p:
                for n in T0.data_back(p):
                    data.setdefault(n, p)
        write_map(out / "level0.txt", [f"level 0: NTSC-U Veldin -> {ser} Veldin (the region table's runs; tools/overlay_match/region_maps.py apply)"],
                  fn, data)
        print(f"[region_maps] level 0: {len(fn)} functions, {len(data)} data addresses")
        for lv in range(1, 19):
            L = core.overlay(om.Overlay(*om.level_overlay(iso, lv)))
            fn, data, lost = compose(T0, V, L, f"level {lv}", core)
            write_map(out / f"level{lv}.txt", [f"level {lv}: NTSC-U Veldin -> {ser} level {lv} ({ser} Veldin matched in its level {lv}, composed with the region table's level 0)"],
                      fn, data)
            print(f"[region_maps] level {lv}: {len(fn)} functions ({lost} {ser}-only of Veldin's dropped), {len(data)} data addresses")
        B = core.overlay(boot_overlay(elf))
        fn, data, lost = compose(T0, V, B, "boot", core)
        write_map(out / "front_reloc.txt", [f"level front: NTSC-U Veldin -> {ser} boot module ({ename}; composed with the region table's level 0)"],
                  fn, data)
        print(f"[region_maps] boot module: {len(fn)} functions, {len(data)} data addresses")
    for name, R in units.items():
        if name == "core":  # (the core's runs, for the runtime's load of the core at NTSC-U's addresses)
            (out / "region_core.txt").write_text("".join(l + "\n" for l in core.lines())
                                                 + "".join("ptr %08x\n" % q for q in R.ptr))
            continue
        fname = "region_boot.txt" if name == "boot" else f"region_level{name}.txt"
        (out / fname).write_text("base %08x %08x\n" % (R.code_base, R.code_end)
                                 + "".join("code %08x %08x %+x\n" % r for r in R.code)
                                 + "".join("data %08x %08x %+x\n" % r for r in R.data)
                                 + "".join(("entry %08x %08x inner\n" if e[0] in R.inner else "entry %08x %08x\n") % e
                                           for e in sorted(R.entry.items()))
                                 + "".join("ptr %08x\n" % q for q in R.ptr))
    if not core:
        (out / "region_core.txt").unlink(missing_ok=True)
    print(f"[region_maps] wrote {out}/")


# ---- check ------------------------------------------------------------------------------------

def read_map(path):
    fn, data = {}, {}
    for line in Path(path).read_text().splitlines():
        f = line.split()
        if len(f) >= 3 and f[0] == "fn":
            fn[int(f[1], 16)] = int(f[2], 16)
        elif len(f) >= 3 and f[0] == "data":
            data[int(f[1], 16)] = int(f[2], 16)
    return fn, data


def xlate_data(data, keys, a):
    """ovl_reloc.cpp's ovl_xlate_data on a map (None: kept untranslated)."""
    if a in data:
        return data[a]
    i = bisect.bisect_right(keys, a)
    if i == 0:
        return None
    lo = keys[i - 1]
    d = data[lo] - lo
    if i < len(keys) and data[keys[i]] - keys[i] == d:
        return a + d
    return a + d if a - lo < 0x100 else None


def xlate_code(fn, sizes, keys, data, a):
    if a in fn:
        return fn[a]
    if a in data:
        return data[a]
    i = bisect.bisect_right(keys, a)
    if i and a - keys[i - 1] < sizes.get(keys[i - 1], 0):
        return a + fn[keys[i - 1]] - keys[i - 1]
    return None


def port_literals():
    out = subprocess.run(["grep", "-rhoE", r"\b(OV|OVF|OvConst<)\(?0x[0-9a-fA-F]+", str(ROOT / "src/game")],
                         capture_output=True, text=True).stdout.split()
    code, data = set(), set()
    for t in out:
        v = int(t[t.index("0x"):], 16)
        (code if t.startswith("OVF") else data).add(v)
    return code, data


def cmd_check(a):
    code_lit, data_lit = port_literals()
    print(f"[region_maps] the ports' literals: {len(code_lit)} code (OVF), {len(data_lit)} data (OV / OvConst)")
    ni, pi = open(a.ntsc, "rb"), open(a.pal, "rb")
    _r, pelf0, _v = disc_region(pi)
    tab = table_for(sha1(root_file(pi, pelf0)))
    core = read_core(tab) if tab is not None else Core()
    V = om.Overlay(*om.level_overlay(ni, 0))
    for lv in [x for x in a.levels.split(",")]:
        # the direct map: NTSC-U Veldin matched in the PAL overlay (both discs), as overlay_match writes it
        if lv == "front":
            _r, pelf, _v = disc_region(pi)
            L = core.overlay(boot_overlay(root_file(pi, pelf)))
        else:
            L = core.overlay(om.Overlay(*om.level_overlay(pi, int(lv))))
        _st, fmap, _e, _a, _m, sizes, pairs, _b = om.map_overlays(V, L)
        dfn, dsz = dict(fmap), {x: sizes[x] for x in fmap}
        ddata = map_points(V, L, fmap, sizes, pairs)
        if core:  # (the core's addresses: NTSC-U's at run time, the core loaded there)
            ddata = {x: (x if x < CORE_END else y) for x, y in ddata.items()}
        mine = Path(a.maps) / ("front_reloc.txt" if lv == "front" else f"level{lv}.txt")
        mfn, mdata = read_map(mine)
        msz = {int(l.split()[1], 16): int(l.split()[3], 16) for l in mine.read_text().splitlines() if l.startswith("fn ")}
        dk, mk, dfk, mfk = sorted(ddata), sorted(mdata), sorted(dfn), sorted(mfn)
        res = collections.Counter()
        diffs = []
        for x in sorted(data_lit | code_lit):
            if x >= 0x1EAA00 and x in code_lit or x >= 0x1EAA00 and x not in data_lit:
                u, v = xlate_code(dfn, dsz, dfk, ddata, x), xlate_code(mfn, msz, mfk, mdata, x)
                kind = "code"
            else:
                u, v = xlate_data(ddata, dk, x), xlate_data(mdata, mk, x)
                kind = "data"
            if u == v:
                res[kind + (" same" if u is not None else " both untranslated")] += 1
                if u is None:
                    diffs.append(f"    {kind} {x:08x}: untranslated in both (kept)")
            else:
                res[kind + " differ"] += 1
                diffs.append(f"    {kind} {x:08x}: direct {u if u is None else '%08x' % u}, region {v if v is None else '%08x' % v}")
        fn_same = sum(1 for n in dfn if mfn.get(n) == dfn[n])
        print(f"  level {lv}: fn lines direct {len(dfn)}, region {len(mfn)}, the same {fn_same}; the ports' literals: "
              + ", ".join(f"{k} {v}" for k, v in sorted(res.items())))
        for d in diffs[: a.show]:
            print(d)


def veldin_literals():
    """{address}: the ports' literals of Veldin's overlay (`OV(0x...)`, `OvConst<0x...>`), the region table's level 0."""
    out = subprocess.run(["grep", "-rhoE", r"\b(OV\(|OvConst<)0x[0-9a-fA-F]+", str(ROOT / "src/game")],
                         capture_output=True, text=True).stdout.splitlines()
    return {int(re.search(r"0x[0-9a-fA-F]+", t).group(0), 16) for t in out}


def level_literals():
    """{level: {address}}: the level-only ports' literals (`L<n>_ADDR(0x...)`), and the boot module's
    (`FR(0x...)`) under 'boot'."""
    out = subprocess.run(["grep", "-rhoE", r"\b(L[0-9]+_(ADDR|CASE)\(|FR\(|LvlConst<[0-9]+, )0x[0-9a-fA-F]+", str(ROOT / "src/game")],
                         capture_output=True, text=True).stdout.splitlines()
    lits = collections.defaultdict(set)
    for t in out:
        m = re.match(r"(?:L([0-9]+)_(?:ADDR|CASE)\(|(FR)\(|LvlConst<([0-9]+), )(0x[0-9a-fA-F]+)", t)
        lits["boot" if m.group(2) else (m.group(1) or m.group(3))].add(int(m.group(4), 16))
    return lits


def cmd_check_levels(a):
    """The level-only and boot module literals translated through the table's runs (as the runtime does)
    against the direct maps (NTSC-U level N matched in the PAL level N, both discs)."""
    _disc, units = read_table(a.table)
    core = read_core(a.table)
    lits = level_literals()
    ni, pi = open(a.ntsc, "rb"), open(a.pal, "rb")
    bad_total = 0
    order = sorted(lits, key=lambda k: 99 if k == "boot" else int(k))
    for name in order:
        if a.levels and name not in a.levels.split(","):
            continue
        if name == "boot":
            _r, nelf, _v = disc_region(ni)
            _r, pelf, _v = disc_region(pi)
            V, L = boot_overlay(root_file(ni, nelf)), core.overlay(boot_overlay(root_file(pi, pelf)))
        else:
            V, L = om.Overlay(*om.level_overlay(ni, int(name))), core.overlay(om.Overlay(*om.level_overlay(pi, int(name))))
        _st, fmap, _e, _am, _m, sizes, pairs, _b = om.map_overlays(V, L)
        dfn, dsz = dict(fmap), {x: sizes[x] for x in fmap}
        ddata = map_points(V, L, fmap, sizes, pairs)
        dk, dfk = sorted(ddata), sorted(dfn)
        R = units[name]
        res, diffs = collections.Counter(), []
        for x in sorted(lits[name]):
            if x < CORE_END:
                res["below the overlay (never translated)"] += 1
                continue
            if x >= V.code_base:
                u, v, kind = xlate_code(dfn, dsz, dfk, ddata, x), R.code_fwd(x), "code"
            else:
                u, v, kind = xlate_data(ddata, dk, x), R.data_fwd(x), "data"
                # (a section's start: the disc's same section's start, as the table writer places it; the direct maps
                # only guess it from the object below)
                k = next((k for k, (dest, _t, _b) in enumerate(V.secs[:6]) if dest == x), None)
                if k is not None and k < len(L.secs) and V.secs[k][1] == L.secs[k][1] and v == L.secs[k][0]:
                    u = v
            if u == v:
                res[kind + (" same" if u is not None else " both untranslated")] += 1
                if u is None:
                    # (an error: the runtime keeps the NTSC-U address, right only where the disc has the same bytes
                    # there; the 1.02 boot module's title sound lists were read 0x100 early so, #137)
                    bad_total += 1
                    diffs.append(f"    {kind} {x:08x}: untranslated in both (kept: an error)")
            elif u is None:
                res[kind + " direct has none"] += 1
                diffs.append(f"    {kind} {x:08x}: direct none, region {v:08x}")
            else:
                res[kind + " differ"] += 1
                bad_total += 1
                diffs.append(f"    {kind} {x:08x}: direct {u:08x}, region {v if v is None else '%08x' % v}")
        print(f"  {name}: {len(lits[name])} literals: " + ", ".join(f"{k} {v}" for k, v in sorted(res.items())))
        for d in diffs[: a.show]:
            print(d)
    print(f"[region_maps] {bad_total} literals translated differently from the direct maps, or not at all")


def load_image(elf):
    """The ELF's PT_LOAD as memory: (base address, bytearray)."""
    phoff = struct.unpack_from("<I", elf, 0x1C)[0]
    _t, off, vaddr, _pa, filesz = struct.unpack_from("<IIIII", elf, phoff)
    return vaddr, bytearray(elf[off:off + filesz])


def normalise_core(elf, core, ptrs):
    """The disc's core at NTSC-U's addresses, as the runtime loads it (src/game/ovl/region_core.cpp): the core's data
    words the table lists (`ptr`: pointers into the disc's core) taken back, then each run copied back, the rest of the
    disc's core (end_n .. end_p) cleared. Returns (base, bytearray)."""
    base, src = load_image(elf)
    src = bytearray(src)
    for q in ptrs:
        b = core.back(struct.unpack_from("<I", src, q - base)[0])
        if b is not None:
            struct.pack_into("<I", src, q - base, b)
    mem = bytearray(src)
    for lo, hi, d in core.code:
        mem[lo - base:hi - base] = src[lo + d - base:hi + d - base]
    for lo, hi, d in core.data:
        mem[lo - base:hi + 1 - base] = src[lo + d - base:hi + 1 + d - base]
    mem[core.end_n - base:core.end_p - base] = bytes(core.end_p - core.end_n)
    return base, mem


def cmd_check_core(a):
    """(both discs) The disc's core loaded at NTSC-U's addresses (normalise_core, the runtime's load) against the NTSC-U
    core: every word of its data and code runs."""
    _disc, units = read_table(a.table)
    core = read_core(a.table)
    if not core:
        print("[region_maps] the table has no core unit: the disc's core is NTSC-U's")
        return
    with open(a.ntsc, "rb") as ni, open(a.pal, "rb") as pi:
        ne, pe = root_file(ni, disc_region(ni)[1]), root_file(pi, disc_region(pi)[1])
    nb, nm = load_image(ne)
    base, mem = normalise_core(pe, core, units["core"].ptr)
    w = lambda m, b, x: struct.unpack_from("<I", m, x - b)[0]
    bad = [x for x in range(core.data[0][0], CORE_END, 4) if w(mem, base, x) != w(nm, nb, x)]
    code_bad = sum(1 for lo, hi, _d in core.code for x in range(lo, hi, 4) if om.mask(w(mem, base, x)) != om.mask(w(nm, nb, x)))
    print(f"[region_maps] core data: {(CORE_END - core.data[0][0]) // 4} words, {len(bad)} differ from NTSC-U's; "
          f"code runs: {code_bad} words differ (masked)")
    for x in bad[: a.show]:
        print(f"    {x:08x}: NTSC-U {w(nm, nb, x):08x}, the disc's loaded {w(mem, base, x):08x}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    t = sub.add_parser("table")
    t.add_argument("--pal", default=str(ROOT / "assets/iso/Ratchet Clank.iso"))
    t.add_argument("--ntsc", default=str(ROOT / "assets/iso/rac1-ntsc-u.iso"))
    t.add_argument("-o", required=True)
    p = sub.add_parser("apply")
    p.add_argument("--iso", required=True)
    p.add_argument("--elf", required=True, help="the disc's boot ELF (SCES_509.16), extracted")
    p.add_argument("--table", required=True)
    p.add_argument("-o", required=True)
    c = sub.add_parser("check")
    c.add_argument("--maps", required=True, help="apply's output")
    c.add_argument("--pal", default=str(ROOT / "assets/iso/Ratchet Clank.iso"))
    c.add_argument("--ntsc", default=str(ROOT / "assets/iso/rac1-ntsc-u.iso"))
    c.add_argument("--levels", default=",".join(str(i) for i in range(19)) + ",front")
    c.add_argument("--show", type=int, default=8)
    k = sub.add_parser("check-levels")
    k.add_argument("--table", required=True)
    k.add_argument("--pal", default=str(ROOT / "assets/iso/Ratchet Clank.iso"))
    k.add_argument("--ntsc", default=str(ROOT / "assets/iso/rac1-ntsc-u.iso"))
    k.add_argument("--levels", default="", help="e.g. 3,5,boot (default: every level with literals)")
    k.add_argument("--show", type=int, default=8)
    o = sub.add_parser("check-core")
    o.add_argument("--table", required=True)
    o.add_argument("--pal", default=str(ROOT / "assets/iso/Ratchet Clank.iso"))
    o.add_argument("--ntsc", default=str(ROOT / "assets/iso/rac1-ntsc-u.iso"))
    o.add_argument("--show", type=int, default=8)
    a = ap.parse_args()
    {"table": cmd_table, "apply": cmd_apply, "check": cmd_check, "check-levels": cmd_check_levels,
     "check-core": cmd_check_core}[a.cmd](a)


if __name__ == "__main__":
    main()
