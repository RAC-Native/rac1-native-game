#!/usr/bin/env python3
"""The inventory of the port (issue #42): which of the game's functions are ported, which the game reaches and the port
does not run yet, and which nothing reaches (cut content), counted per copy and per unique function.

It reads the player's disc through the code generators' helpers (tools/overlay_match, tools/codegen/ovlist.py): the core
of the boot executable (its libraries, below 0x15ef00), the boot module (the title and its menus) and the 19 level
overlays, each at its NTSC-U addresses. Nothing of the game is written to the repository: the per-function tables go to
assets/generated/inventory/ (gitignored); the committed document (docs/port-inventory.md, `doc`) holds addresses,
sizes, counts and our own descriptions only.

For every image it finds the functions (the matcher's starts: jal targets, code pointers, prologues after a return; the
ports' and the maps' addresses; prologues after nop padding; the code pointers the code builds, unless built ahead in
the same function as a continuation; not a jump table's targets; then each function's end as tools/codegen/ovlist.py's
listings find it, and the unreferenced code after it as functions of their own) and the call graph:
  - direct calls (jal) and tail calls (j out of the function), into the image or into the core;
  - code pointers the code builds (lui + addiu, through register copies: callbacks, the tables the code fills);
  - code pointers in the data: the level tables (overlay section 3: moby class -> update function, the class hooks;
    4: camera types; 5: sound handlers), the particle update table (Veldin's 0x1b1e80, mapped per level), and any other
    data word that is a function's start (menus' widgets, draw callbacks, state tables).
Reach: `entry` (called from the image's entry), then `class` / `camera` / `sound` / `particle` (from that table) or
`pointer` (a code pointer the reached code builds), then `data` (only another data word points to it); not connected:
`none:noref` (no reference found anywhere) / `none:dead` (referenced only by other unconnected code).

Ported, per copy (each level carries its own relinked copy of the engine):
  - an `// orig:` annotation of src/game or of the generated code (assets/generated/src): `0x...` (the core or Veldin's
    overlay; a bare address in src/game/boot that starts a boot module function is the boot module's), `Ln:0x...`
    (level n's own address), `front:0x...` (the boot module), and the addresses a port covers inline (`(through ...)`);
  - a registration with a literal address (ovl_register_moby_fn, ovl_register_level_moby_fn, snd_register_callback), a
    `case` of a dispatch switch on a code pointer, the levels' variants rows (src/game/ovl/variants_l<n>.cpp), the sky
    dispatchers (variants_sky.cpp);
  - a copy the runtime's translation (src/game/ovl/ovl_reloc.cpp, ovl_code_to_veldin: the overlay maps, the variants
    rows, an address inside a mapped function, the code pointer pairs; for the boot module front_reloc.txt) or the
    call sites of the matched callers pair with a ported Veldin function of the same body (`matched`, `translated`);
  - a body ported for another level, only called directly by ported code (`shared`).
Not ported:
  - `standin`: the level's own build of a ported Veldin function whose body differs: the Veldin port runs in its place,
    unverified;
  - `unwired`: the same body is ported for another level, but not wired for this one (a registration, a variants row);
  - `empty`: a bare `jr ra` (or `jr ra` and one instruction): no behaviour;
  - `todo`: no port at all.

Unique functions: the copies are grouped by their body: the same instructions with the relocated fields masked, the
addresses they build compared through the overlay maps (an address of the image's own matches anything; two different
words of one image never go together). A `family` also joins a function's builds that differ.

Runtime evidence (`tour`): headless runs of every level (walking, the wrench, firing, the pause menu's pages) and of the
title's menus with RAC1_MOBY_LIST (the live mobys and the particle types with no port) and the TODO_PORT log; `scan
--runtime DIR` marks the copies the runs met. tools/inventory/notes.txt holds our descriptions (`doc` reads it).

Usage (repo root):
  tools/inventory/port_inventory.py scan [--runtime DIR] [--census CSV]   -> assets/generated/inventory/*.csv, summary.json
  tools/inventory/port_inventory.py tour [--out DIR] [--levels 0,1,...]   headless runs, one at a time (build/rac1-native)
  tools/inventory/port_inventory.py doc                                   -> docs/port-inventory.md and docs/catalog/
  tools/inventory/port_inventory.py catalog                               -> docs/catalog/ only (port_catalog.py)
"""
import argparse
import bisect
import collections
import csv
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/overlay_match"))
sys.path.insert(0, str(ROOT / "tools/codegen"))
import overlay_match as om  # noqa: E402
import ovlist  # noqa: E402
import region_view as rv  # noqa: E402

ASSETS = Path(os.environ.get("RAC1_ASSETS") or ROOT / "assets")
OUT = ASSETS / "generated/inventory"
DOC = ROOT / "docs/port-inventory.md"
CATALOG = ROOT / "docs/catalog"
LEVELS = list(range(19))
CORE, FRONT = "core", "front"
CORE_END = 0x15EF00
JR_RA = 0x03E00008
PARTICLE_TABLE, PARTICLE_N = 0x001B1E80, 0x60


def img_name(k):
    return k if isinstance(k, str) else "L%d" % k


def nk(k, a):
    """A function copy's key: `L3:002dc310`, `core:00112380`, `front:001e9658`."""
    return "%s:%08x" % (img_name(k), a)


# ---- the images ---------------------------------------------------------------------------------------------------


def core_overlay(elf):
    """The core of the boot executable (core.text and its data) in om.Overlay's layout (code last)."""
    shoff = struct.unpack_from("<I", elf, 0x20)[0]
    shnum = struct.unpack_from("<H", elf, 0x30)[0]
    shstrndx = struct.unpack_from("<H", elf, 0x32)[0]
    secs = [struct.unpack_from("<IIIIII", elf, shoff + i * 40) for i in range(shnum)]
    stro = secs[shstrndx][4]

    def name(n):
        return elf[stro + n:elf.index(b"\0", stro + n)].decode()
    by = {name(s[0]): s for s in secs}
    out = []
    for want in ("core.data", "core.rdata", "core.lit"):
        _n, _t, _f, addr, off, size = by[want]
        out.append((addr, 1, elf[off:off + size]))
    out += [(0, 1, b"")] * 3
    _n, _t, _f, addr, off, size = by["core.text"]
    out.append((addr, 1, elf[off:off + size]))
    entry = struct.unpack_from("<I", elf, 0x18)[0]
    return om.Overlay(out, entry)


class Func:
    __slots__ = ("img", "start", "end", "orphan", "calls", "ptrs", "callers", "roots", "reach", "status", "how",
                 "body", "uid", "fam", "veldin", "name", "role", "file", "partial", "runtime", "standin_of", "ptr_in")

    def __init__(self, img, start):
        self.img, self.start, self.end, self.orphan = img, start, start, False
        self.calls, self.ptrs, self.callers, self.roots = set(), set(), set(), set()
        self.reach = self.status = self.how = self.body = self.uid = self.fam = None
        self.veldin = self.name = self.role = self.file = self.standin_of = None
        self.partial, self.runtime, self.ptr_in = False, set(), False

    @property
    def size(self):
        return self.end - self.start

    @property
    def node(self):
        return (self.img, self.start)


class Image:
    def __init__(self, key, O):
        self.key, self.O = key, O
        self.funcs = {}
        self.starts = []

    def word(self, a):
        return self.O.code[(a - self.O.code_base) // 4]

    def containing(self, a):
        i = bisect.bisect_right(self.starts, a) - 1
        if i < 0:
            return None
        f = self.funcs[self.starts[i]]
        return f if a < max(f.end, f.start + 4) else None

    def discover(self, extra=()):
        """The functions: the starts, each one's end, the unreferenced code between (a function of its own)."""
        O = self.O
        jals = {(w & 0x3FFFFFF) << 2 for w in O.code if w >> 26 == 3}
        extra = {a for a in extra if O.in_code(a)}
        # a jump table's targets are case blocks, not functions (the matcher counts a data pointer after a return)
        cases = self.jump_table_targets() - jals - extra
        known = (set(O.function_starts()) - cases) | extra
        # a code pointer built ahead in the same function is a continuation (a return address it jumps through), not a
        # function: no known start between where it is built and where it points
        ks = sorted(known)
        for p, t in self.code_built_pointers():
            if t in cases or t in known:
                continue
            i = bisect.bisect_right(ks, p)
            if p < t and (i >= len(ks) or ks[i] > t):
                continue
            known.add(t)
        starts = set(known) | (self.prologues_after_padding() - cases)
        while True:
            srt = sorted(starts)
            ends, new = {}, set()
            for i, a in enumerate(srt):
                lim = srt[i + 1] if i + 1 < len(srt) else O.code_end
                ends[a] = self.find_end(a, lim)
                p = ends[a]
                while p < lim and self.word(p) == 0:
                    p += 4
                if p < lim:
                    new.add(p)
            if not new - starts:
                break
            starts |= new
        self.funcs = {}
        for a in sorted(starts):
            f = Func(self.key, a)
            f.end, f.orphan = ends[a], a not in known
            self.funcs[a] = f
        self.starts = sorted(self.funcs)

    def code_built_pointers(self):
        """(where, what) of the code addresses the code builds (lui, then addiu on that register or a copy of it):
        callbacks, the tables the code fills (the particle updates), continuations."""
        out, hi, O = set(), {}, self.O
        for k, w in enumerate(O.code):
            op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
            if op == 0x0F:
                hi[rt] = (w & 0xFFFF) << 16
                continue
            if op == 0 and (w & 0x3F) in (0x21, 0x25, 0x2D) and 0 in (rs, rt) and (rs or rt) in hi:
                hi[(w >> 11) & 31] = hi[rs or rt]
                continue
            if op == 0x09 and rs in hi:
                t = (hi[rs] + om.sx16(w)) & 0xFFFFFFFF
                if O.in_code(t):
                    out.add((O.code_base + 4 * k, t))
            d = om.gpr_written(w)
            if d is not None:
                hi.pop(d, None)
        return out

    def jump_table_targets(self):
        """The targets of every jump table the code reads (tools/codegen/ovlist.py's idiom before a `jr <reg>`)."""
        out, code = set(), self.O.code
        for i, w in enumerate(code):
            if w >> 26 == 0 and (w & 0x3F) == 8 and ((w >> 21) & 31) != 31:
                out.update(t for t in ovlist._jump_table(self.O, code[max(0, i - 40):i]) if self.O.in_code(t))
        return out

    def prologues_after_padding(self):
        """A frame prologue (addiu sp,sp,-n) after a `jr ra`, its delay slot and nop padding (the matcher's rule allows
        no padding): a function nothing calls directly."""
        out, code = set(), self.O.code
        for i in range(2, len(code)):
            w = code[i]
            if (w >> 16) != 0x27BD or not (w & 0x8000):
                continue
            j = i - 1
            while j >= 0 and code[j] == 0 and i - j <= 4:
                j -= 1
            if j >= 1 and (code[j - 1] == JR_RA or code[j] == JR_RA and code[j + 1] == 0):
                out.add(self.O.code_base + 4 * i)
        return out

    def find_end(self, a, lim):
        """The end of the function at a, as the code generators' listings find it (tools/codegen/ovlist.py): after the
        `jr ra` (and its delay slot) that no branch, j or jump-table target of the function goes past; `lim` (the next
        start) when there is none."""
        far = a
        p = a
        while p < lim:
            w = self.word(p)
            t = om.branch_target(w, p)
            if t is not None and a <= t < lim:
                far = max(far, t)
            if w >> 26 == 0 and (w & 0x3F) == 8 and ((w >> 21) & 31) != 31:  # jr <reg>: a jump table's cases
                win = [self.word(q) for q in range(max(a, p - 160), p, 4)]
                for t in ovlist._jump_table(self.O, win):
                    if a <= t < lim:
                        far = max(far, t)
            if w == JR_RA and far <= p + 4:
                return min(p + 8, lim)
            p += 4
        return lim


def load_images():
    iso = rv.disc_path(str(ASSETS))
    _ser, elfname, table = rv.disc_of(iso)
    imgs = {}
    for lv in LEVELS:
        imgs[lv] = Image(lv, ovlist.overlay(lv))
    imgs[FRONT] = Image(FRONT, ovlist.overlay(ovlist.FRONT))
    elf = open(ASSETS / "dumps" / elfname, "rb").read()
    imgs[CORE] = Image(CORE, core_overlay(elf))
    return imgs, table is not None


# ---- the repository: what is ported -------------------------------------------------------------------------------

ADDR_TOK = re.compile(r"^(?:(L\d+|front):)?0x([0-9a-fA-F]{6,8})[,;.]?$")
ORIG = re.compile(r"//\s*orig:\s*(.*)$")
DEFN = re.compile(r"([A-Za-z_]\w*)\s*\(")
PARTIAL = re.compile(r"\((?:[^)]*\b(?:blocks?|tails?|inlined|part|steps?|case|half)\b|0x)")  # a piece of the function


def scope_of(prefix, a):
    if prefix is None:
        return CORE if a < CORE_END else 0
    if prefix == "front":
        return FRONT
    return int(prefix[1:])


def read_annotations():
    """{(scope, addr): {"files": set, "name": str, "partial": bool}} from src/game and the generated code."""
    ann = {}
    roots = [ROOT / "src/game", ASSETS / "generated/src/game"]
    for r in roots:
        if not r.exists():
            continue
        for p in r.rglob("*"):
            if p.suffix not in (".cpp", ".h", ".inc") or "render" in p.parts or "enhance" in p.parts:
                continue
            lines = p.read_text(errors="replace").split("\n")
            rel = str(p.relative_to(ROOT if str(p).startswith(str(ROOT / "src")) else ASSETS / "generated"))
            for i, line in enumerate(lines):
                m = ORIG.search(line)
                if not m:
                    continue
                text = m.group(1).strip()
                toks = text.replace(" / ", " ").split()
                got = []
                for t in toks:
                    mt = ADDR_TOK.match(t)
                    if not mt:
                        break
                    a = int(mt.group(2), 16)
                    got.append((scope_of(mt.group(1), a), a))
                if not got:
                    continue
                partial = bool(PARTIAL.search(text))
                desc = port_desc(text, lines, i)
                name = None
                head = line[:m.start()].strip()
                if head:  # a trailing annotation: the declaration on its line
                    md = DEFN.search(head.split("=")[0])
                    name = md.group(1) if md else None
                for j in range(i + 1, min(i + 6, len(lines)) if not head else 0):
                    s = lines[j].strip()
                    if s.startswith("//") or not s:
                        continue
                    md = DEFN.search(s.split("=")[0])
                    if md and md.group(1) not in ("if", "for", "while", "switch", "return", "static_assert"):
                        name = md.group(1)
                    break
                # the functions a port covers inline: `(through 0x..., 0x...)`, `(inlined ... 0x...)`
                gen = not str(p).startswith(str(ROOT / "src"))
                for par in re.findall(r"\(([^)]*\b(?:through|inlined?|inline)\b[^)]*)\)", text):
                    for x in re.findall(r"0x([0-9a-fA-F]{6,8})", par):
                        e = ann.setdefault((got[0][0], int(x, 16)), {"files": set(), "name": None, "partial": True})
                        e["files"].add(rel)
                        e["inline"] = True
                        e.setdefault("sites", []).append((rel, i + 1, int(gen), "", 1, "covered inline by %s (0x%x)" % (
                            name or "its caller", got[0][1])))
                for k in got:
                    e = ann.setdefault(k, {"files": set(), "name": None, "partial": True})
                    e["files"].add(rel)
                    e.setdefault("sites", []).append((rel, i + 1, int(gen), name or "", int(partial), desc))
                    if name and (e["name"] is None or e["partial"] and not partial):
                        e["name"] = name  # (a whole function's name before a block's)
                    e["partial"] = e["partial"] and partial
    return ann


QUOTED = re.compile(r'"[^"]*"')
INSN = re.compile(r"\b(?:[ls][bhwdq]u?|l[wd]c1|s[wd]c1|lqc2|sqc2|lui|d?addiu?|d?addu|d?subu|dsll(?:32)?|dsrl(?:32)?|sll|srl|"
                  r"sra|jalr?|jr|beql?|bnel?|or|ori|and|andi|xor|slti?u?|mfc1|mtc1|v[a-z]+\.[xyzw]+)\s+\$?[a-z][a-z0-9]?\b,")


def port_desc(text, lines, i):
    """Our own description of a port, for the catalog: the words after the addresses on its `// orig:` line, else the
    comment block just above it (its first sentence; quoted text left out)."""
    rest = re.sub(r"^(?:(?:L\d+|front):)?0x[0-9a-fA-F]{6,8}[,;.]?\s*", "", text.replace(" / ", " "))
    while True:
        r2 = re.sub(r"^(?:(?:L\d+|front):)?0x[0-9a-fA-F]{6,8}[,;.]?\s*", "", rest)
        if r2 == rest:
            break
        rest = r2
    rest = rest.lstrip("-\u2014: ").strip()
    if not rest:
        block = []
        j = i - 1
        while j >= 0 and lines[j].strip().startswith("//") and "orig:" not in lines[j] and len(block) < 4:
            block.insert(0, lines[j].strip()[2:].strip())
            j -= 1
        rest = " ".join(block)
    if len(INSN.findall(rest)) >= 2:  # (a comment that spells out the original's instructions: not for the catalog)
        return ""
    rest = QUOTED.sub("\u2026", rest)
    m = re.search(r"\.(\s|$)", rest)
    if m:
        rest = rest[:m.start()]
    return rest[:160].strip()


REG_V = re.compile(r"(?:ovl_register_moby_fn|snd_register_callback)\(\s*(?:OVF\()?0x([0-9a-fA-F]+)")
REG_L = re.compile(r"ovl_register_level_moby_fn\(\s*(\w+)\s*,\s*(?:L\d+_ADDR\(|LvlConst<\d+,\s*)?0x([0-9a-fA-F]+)")
CONST = re.compile(r"constexpr int (\w+) = (\d+);")
VROW = re.compile(r"\{\s*0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+),\s*0x([0-9a-fA-F]+)\s*\},?\s*(?://\s*(.*))?")
SKYROW = re.compile(r"\{\s*(\d+),\s*0x([0-9a-fA-F]+),\s*(\w+)\s*\}")
TODO = re.compile(r"TODO_PORT\(\s*0x([0-9a-fA-F]+)\s*,\s*\"([^\"]*)\"")
SWITCH = re.compile(r"\bswitch\s*\((.*)")
CASE = re.compile(r"\bcase\s+(?:L(\d+)_CASE\()?0x([0-9a-fA-F]{6,8})")
FILE_LEVEL = {"novalis": 1, "aridia": 2, "kerwan": 3, "eudora": 4, "rilgar": 5, "blarg": 6, "umbris": 7, "batalia": 8,
              "gaspar": 9, "orxon": 10, "pokitaru": 11, "hoven": 12, "gemlik": 13, "oltanis": 14, "quartu": 15,
              "kalebo": 16, "veldin_orbit": 17, "veldin_finale": 18}


def dispatch_cases(path, text):
    """The code addresses a dispatch switch handles (`case 0x...:` under a switch on a code pointer: the ports a
    jalr reaches, some of them written inline in the case): {(scope, addr)}."""
    out, kind = set(), None
    name = path.name
    lvl = next((n for pre, n in sorted(FILE_LEVEL.items(), key=lambda kv: -len(kv[0])) if name.startswith(pre)), None)
    for line in text.split("\n"):
        m = SWITCH.search(line)
        if m:
            arg = m.group(1)
            kind = "front" if "OVL_FRONT" in arg else "veldin" if "ovl_code_to_veldin" in arg or "veldin" in arg \
                else "level" if "lvl_key" in arg else "fn" if re.match(r"\s*(fn|vf|addr)\s*\)", arg) else None
            continue
        m = CASE.search(line)
        if not m or kind is None:
            continue
        a = int(m.group(2), 16)
        if a < 0x1E8000:
            continue
        if m.group(1):
            out.add((int(m.group(1)), a))
        elif kind == "front":
            out.add((FRONT, a))
        elif kind == "fn" and lvl is not None:
            out.add((lvl, a))
        else:
            out.add((0, a))
    return out


def read_registrations():
    """The literal-address registrations, the variants rows, the sky rows and the TODO_PORT stubs."""
    names = {}
    for p in (ROOT / "src/game/ovl").glob("variants_l*.h"):
        for m in CONST.finditer(p.read_text()):
            names[m.group(1)] = int(m.group(2))
    reg, cases, rows, todo = set(), set(), collections.defaultdict(list), collections.defaultdict(set)
    where = collections.defaultdict(set)  # (scope, addr) -> the files that register it / handle it in a dispatch case
    for r in (ROOT / "src/game", ASSETS / "generated/src/game"):
        if not r.exists():
            continue
        for p in r.rglob("*.cpp"):
            t = p.read_text(errors="replace")
            rel = str(p.relative_to(ROOT)) if str(p).startswith(str(ROOT / "src")) else \
                "gen:" + str(p.relative_to(ASSETS / "generated"))
            for m in REG_V.finditer(t):
                a = int(m.group(1), 16)
                reg.add((CORE if a < CORE_END else 0, a))
                where[(CORE if a < CORE_END else 0, a)].add(rel)
            for m in REG_L.finditer(t):
                lv = names.get(m.group(1), int(m.group(1)) if m.group(1).isdigit() else None)
                if lv is not None:
                    reg.add((lv, int(m.group(2), 16)))
                    where[(lv, int(m.group(2), 16))].add(rel)
            for m in TODO.finditer(t):
                todo[int(m.group(1), 16)].add(m.group(2))
            dc = dispatch_cases(p, t)
            for k in dc:
                where[k].add(rel)
            cases |= dc
    for p in (ROOT / "src/game/ovl").glob("variants_l*.cpp"):
        lv = int(re.search(r"variants_l(\d+)", p.name).group(1))
        for line in p.read_text().split("\n"):
            m = VROW.search(line)
            if m:
                rows[lv].append((int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16), (m.group(4) or "").strip()))
    sky = set()
    sp = ROOT / "src/game/ovl/variants_sky.cpp"
    if sp.exists():
        for m in SKYROW.finditer(sp.read_text()):
            sky.add((int(m.group(1)), int(m.group(2), 16)))
    return reg, cases, rows, sky, todo, where


class Maps:
    """The overlay maps (assets/generated/overlay_map): per image (a level, or the boot module through front_reloc.txt),
    Veldin function -> its address there (fn), the matcher's Veldin function size (size: the runtime's translation
    reads a level address inside it at the same offset), Veldin data -> data (data); and the code pointer pairs the
    runtime adds (data lines whose Veldin side is code: a level's variant called through a pointer, Veldin's port
    standing in)."""

    def __init__(self):
        md = Path(ovlist.map_dir())
        self.fn, self.size, self.data = {}, {}, {}
        files = {lv: md / f"level{lv}.txt" for lv in LEVELS[1:]}
        files[FRONT] = ASSETS / "generated/overlay_map/front_reloc.txt"
        for k, path in files.items():
            fn, size, data = {}, {}, {}
            for line in path.read_text().split("\n"):
                p = line.split()
                if len(p) >= 4 and p[0] == "fn":
                    fn[int(p[1], 16)], size[int(p[1], 16)] = int(p[2], 16), int(p[3], 16)
                elif len(p) >= 3 and p[0] == "data":
                    data[int(p[1], 16)] = int(p[2], 16)
            self.fn[k], self.size[k], self.data[k] = fn, size, data
        # the boot module's own matching (front.txt: boot function -> Veldin's, boot data -> Veldin's)
        self.front_bv, self.front_data_bv = {}, {}
        for line in (ASSETS / "generated/overlay_map/front.txt").read_text().split("\n"):
            p = line.split()
            if len(p) >= 3 and p[0] == "fn" and not p[2].startswith("-"):
                self.front_bv[int(p[1], 16)] = int(p[2], 16)
            elif len(p) >= 3 and p[0] == "data":
                self.front_data_bv[int(p[1], 16)] = int(p[2], 16)
        self._x = {}

    def xlate_index(self, k, rows):
        """The runtime's translation of image k's code addresses back to Veldin's (src/game/ovl/ovl_reloc.cpp,
        ovl_code_to_veldin): the map's functions, the variants rows, an address inside a mapped function at the same
        offset, the code pointer pairs."""
        if k not in self._x:
            rev = {b: v for v, b in self.fn[k].items()}
            if k != FRONT:
                rev.update({b: v for v, b, _s, _c in rows.get(k, ())})
            spans = sorted((b, v, self.size[k].get(v, 0)) for v, b in self.fn[k].items())
            ptr = {b: v for v, b in self.data[k].items() if 0x1EAA00 <= v < 0x2F0CD0 and v not in self.fn[k]}
            self._x[k] = (rev, spans, [s[0] for s in spans], ptr)
        return self._x[k]

    def xlate(self, k, b, rows):
        rev, spans, keys, ptr = self.xlate_index(k, rows)
        if b in rev:
            return rev[b]
        i = bisect.bisect_right(keys, b) - 1
        if i >= 0 and b - spans[i][0] < spans[i][2]:
            return spans[i][1] + b - spans[i][0]
        return ptr.get(b)


def pal_changed():
    """{unit: number of functions the PAL build changed} from the region table (data/regions/SCES-50916.txt)."""
    p = ROOT / "data/regions/SCES-50916.txt"
    out, unit = collections.Counter(), None
    if not p.exists():
        return out
    for line in p.read_text().split("\n"):
        f = line.split()
        if not f:
            continue
        if f[0] == "overlay":
            unit = FRONT if f[1] == "boot" else int(f[1])
        elif f[0] == "entry" and unit is not None:
            out[unit] += 1
    return out


# ---- the graph ----------------------------------------------------------------------------------------------------


def canonizer(key, M, rows):
    """An address of image `key` as Veldin's (the overlay maps, both directions), the core's as itself, `?` for an
    address of the image's own (no Veldin equivalent): what makes two copies' relocated fields compare."""
    if key == 0:
        return lambda x: ("c%x" if x < CORE_END else "v%x") % x
    if key == CORE:
        return lambda x: "c%x" % x
    if key == FRONT:
        fn, dt = dict(M.front_bv), dict(M.front_data_bv)
    else:
        fn = {b: v for v, b in M.fn[key].items()}
        fn.update({b: v for v, b, _s, _c in rows.get(key, ())})
        dt = {b: v for v, b in M.data[key].items()}

    def canon(x):
        if x < CORE_END:
            return "c%x" % x
        v = fn.get(x, dt.get(x))
        return "v%x" % v if v is not None else "?"
    return canon


def body_key(img, f, canon):
    """The body (the trailing padding left out), as (its shape, the addresses it builds, its exact words): the shape is
    the masked words with the offsets of the loads / stores whose base is not an address (a structure's fields: not
    relocated) and the lui halves not used as an address (constants); the addresses (jal targets, lui + low halves,
    $gp offsets) go through `canon` (Veldin's, or `?` for an address of the image's own)."""
    O = img.O
    i, j = (f.start - O.code_base) // 4, (f.end - O.code_base) // 4
    w = O.code[i:j]
    while w and w[-1] == 0:
        w.pop()
    parts, addrs, hi, pend = [], [], {}, {}
    for x in w:
        op, rs, rt = x >> 26, (x >> 21) & 31, (x >> 16) & 31
        if op == 3:
            addrs.append(canon((x & 0x3FFFFFF) << 2))
        elif op == 0x0F:
            hi[rt] = (x & 0xFFFF) << 16
            pend[rt] = len(parts)
            parts.append("lui%x" % (x & 0xFFFF))
            continue
        elif (op in om.MEM or op in om.ADDI) and rs != om.SP:
            if rs == om.GP:
                addrs.append(canon((om.GP_VALUE + om.sx16(x)) & 0xFFFFFFFF))
            elif rs in hi:
                addrs.append(canon((hi[rs] + om.sx16(x)) & 0xFFFFFFFF))
                if rs in pend:
                    parts[pend.pop(rs)] = "lui"
            else:
                parts.append("o%x" % (x & 0xFFFF))
        d = om.gpr_written(x)
        if d is not None:
            hi.pop(d, None)
            pend.pop(d, None)
    m = O.masked[i:i + len(w)]
    h = hashlib.blake2b(struct.pack("<%dI" % len(m), *m), digest_size=8)
    h.update(" ".join(parts).encode())
    raw = hashlib.blake2b(struct.pack("<%dI" % len(w), *w), digest_size=8).hexdigest()
    return h.hexdigest() + ":%x" % (4 * len(w)), addrs, raw


def cluster_bodies(keys, anchors):
    """The bodies: the copies of one shape whose addresses agree wherever both are known (`?` matches anything), two
    different words of one image never together (two functions of one image that differ in an address of its own).
    Veldin's copies first, then the copies the matcher placed (`anchors`: copy -> its Veldin function), then the others
    by how much of their addresses is known, each into the cluster agreeing on the most known addresses."""
    groups = collections.defaultdict(list)
    for n, (shape, addrs, raw) in keys.items():
        groups[shape].append(n)
    body = {}
    for shape, ns in groups.items():
        def rank(n):
            if n[0] == 0:
                return (0, 0, str(n[0]), n[1])
            if n in anchors:
                return (1, 0, str(n[0]), n[1])
            return (2, keys[n][1].count("?"), str(n[0]), n[1])
        ns.sort(key=rank)
        clusters = []  # [merged addresses, {image: raw}, id]
        home = {}
        for n in ns:
            _s, addrs, raw = keys[n]
            best, score = None, -1
            cands = [home[anchors[n]]] if n in anchors and anchors[n] in home else clusters
            for cl in cands:
                vec, raws, _id = cl
                if raws.get(n[0], raw) != raw:
                    continue
                if any(a != b and a != "?" and b != "?" for a, b in zip(addrs, vec)):
                    continue
                sc = sum(1 for a, b in zip(addrs, vec) if a == b and a != "?")
                if sc > score:
                    best, score = cl, sc
            if best is None:
                best = [list(addrs), {}, "%s.%d" % (shape, len(clusters))]
                clusters.append(best)
            best[0] = [b if b != "?" else a for a, b in zip(addrs, best[0])]
            best[1][n[0]] = raw
            body[n] = best[2]
            if n[0] == 0:
                home[n] = best
    return body


def is_empty(img, f):
    """A bare `jr ra` (its delay slot a nop, or one instruction: a constant returned): no behaviour of its own."""
    w = [img.word(p) for p in range(f.start, f.end, 4)]
    while w and w[-1] == 0:
        w.pop()
    return w == [JR_RA] or len(w) == 2 and w[0] == JR_RA


def build_edges(imgs):
    """Calls, tail calls and code-built pointers, within an image and into the core."""
    core = imgs[CORE]
    for img in imgs.values():
        O = img.O
        for f in img.funcs.values():
            hi = {}
            for p in range(f.start, f.end, 4):
                w = img.word(p)
                op = w >> 26
                if op in (2, 3):
                    t = ((p + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
                    if op == 2 and f.start <= t < f.end:
                        continue
                    tgt = img if O.in_code(t) else core if core.O.in_code(t) else None
                    g = tgt.containing(t) if tgt else None
                    if g is not None and g is not f:
                        (f.calls if op == 3 or t == g.start else f.ptrs).add(g.node)
                    continue
                if op == 0x0F:
                    hi[(w >> 16) & 31] = (w & 0xFFFF) << 16
                    continue
                rs, rt = (w >> 21) & 31, (w >> 16) & 31
                if op == 0 and (w & 0x3F) in (0x21, 0x25, 0x2D) and 0 in (rs, rt) and (rs or rt) in hi:
                    hi[(w >> 11) & 31] = hi[rs or rt]  # (a move of the lui's register)
                    continue
                if op == 0x09 and ((w >> 21) & 31) in hi:
                    t = (hi[(w >> 21) & 31] + om.sx16(w)) & 0xFFFFFFFF
                    tgt = img if O.in_code(t) else core if core.O.in_code(t) else None
                    g = tgt.containing(t) if tgt else None
                    if g is not None and g is not f and t == g.start:
                        f.ptrs.add(g.node)
                d = om.gpr_written(w)
                if d is not None:
                    hi.pop(d, None)


def data_roots(imgs, M):
    """{node: set of root kinds} from the tables and the data pointers of each image."""
    roots = collections.defaultdict(set)
    core = imgs[CORE]
    for key, img in imgs.items():
        O = img.O
        if O.in_code(O.entry):
            roots[(key, O.entry)].add("entry")
        ptab = None
        if isinstance(key, int):
            ptab = PARTICLE_TABLE if key == 0 else M.data[key].get(PARTICLE_TABLE)
        elif key == FRONT:
            ptab = M.data[FRONT].get(PARTICLE_TABLE)
        cls = {}
        if key != CORE:
            for row in O.table_rows(3, 3):
                cls.setdefault(row[1], []).append(row[0])
        for k, (dest, typ, b) in enumerate(O.secs[:6]):
            if typ != 1:
                continue
            ws = om.words(b)
            for i, w in enumerate(ws):
                a = dest + 4 * i
                tgt = img if O.in_code(w) else core if core.O.in_code(w) else None
                if tgt is None or w not in tgt.funcs:
                    continue
                if key != CORE and k == 3:
                    kind = "class"
                elif key != CORE and k == 4:
                    kind = "camera"
                elif key != CORE and k == 5:
                    kind = "sound"
                elif ptab is not None and ptab <= a < ptab + 4 * PARTICLE_N:
                    kind = "particle"
                else:
                    kind = "data"
                roots[(tgt.key, w)].add(kind)
        for fn, cs in cls.items():
            if fn in img.funcs:
                img.funcs[fn].role = "class " + "/".join(str(c) for c in sorted(set(cs)))
    return roots


def reach(imgs, roots):
    """Each function's reach: `entry` (called from the entry, calls only), then `pointer` (a code pointer the reached
    code builds: a callback, a table the code fills) and the tables, then `data` (any other data pointer)."""
    allf = {f.node: f for img in imgs.values() for f in img.funcs.values()}
    for f in allf.values():
        for n in f.calls | f.ptrs:
            if n in allf:
                allf[n].callers.add(f.node)
        for n in f.ptrs - f.calls:
            if n in allf:
                allf[n].ptr_in = True
    for n, ks in roots.items():
        if n in allf:
            allf[n].roots = ks

    def spread(seeds, follow_ptrs):
        queue = list(seeds)
        while queue:
            n = queue.pop()
            f = allf[n]
            for m in (f.calls | f.ptrs) if follow_ptrs else f.calls:
                g = allf.get(m)
                if g is not None and g.reach is None:
                    g.reach = f.reach
                    queue.append(m)

    seeds = []
    for n, ks in roots.items():
        if n in allf and "entry" in ks:
            allf[n].reach = "entry"
            seeds.append(n)
    spread(seeds, False)
    seeds = []
    for n, ks in roots.items():
        if n in allf and allf[n].reach is None:
            k = next((t for t in ("class", "camera", "sound", "particle") if t in ks), None)
            if k:
                allf[n].reach = k
                seeds.append(n)
    for f in list(allf.values()):
        if f.reach is not None:
            for m in f.ptrs:
                g = allf.get(m)
                if g is not None and g.reach is None:
                    g.reach = "pointer"
                    seeds.append(m)
    spread(seeds + [n for n, f in allf.items() if f.reach == "entry"], True)
    seeds = []
    for n, ks in roots.items():
        if n in allf and allf[n].reach is None and "data" in ks:
            allf[n].reach = "data"
            seeds.append(n)
    spread(seeds, True)
    for f in allf.values():
        if f.reach is None:
            f.reach = "none:dead" if f.callers else "none:noref"
    return allf


# ---- classification -----------------------------------------------------------------------------------------------


class UF:
    def __init__(self):
        self.p = {}

    def find(self, x):
        self.p.setdefault(x, x)
        while self.p[x] != x:
            self.p[x] = self.p[self.p[x]]
            x = self.p[x]
        return x

    def union(self, a, b):
        a, b = self.find(a), self.find(b)
        if a != b:
            self.p[max(a, b)] = min(a, b)


def call_votes(V, L, fmap):
    """Veldin callee -> the level's callee at the same call sites of the matched callers (unanimous)."""
    votes = collections.defaultdict(set)
    for a, b in fmap.items():
        if a not in V.funcs or b not in L.funcs:
            continue
        n = min(V.funcs[a].size, L.funcs[b].size)
        for k in range(0, n, 4):
            wv, wl = V.word(a + k), L.word(b + k)
            if wv >> 26 == 3 and wl >> 26 == 3:
                votes[(wv & 0x3FFFFFF) << 2].add((wl & 0x3FFFFFF) << 2)
    return {v: next(iter(s)) for v, s in votes.items() if len(s) == 1}


def classify(imgs, allf, ann, reg, rows, sky, M):
    """Each copy's status (see the module's text)."""
    V = imgs[0]
    ported_v = {a for (s, a) in list(ann) + list(reg) if s == 0} | {0}  # (0: no function)
    ported_v.discard(0)
    ported_core = {a for (s, a) in list(ann) + list(reg) if s == CORE}
    # the bodies
    canons = {k: canonizer(k, M, rows) for k in imgs}
    keys = {f.node: body_key(imgs[f.img], f, canons[f.img]) for f in allf.values()}
    anchors = {(k, b): (0, v) for k in LEVELS[1:] + [FRONT] for v, b in M.fn[k].items()}
    bodies = cluster_bodies(keys, anchors)
    for f in allf.values():
        f.body = bodies[f.node]
    # each copy's Veldin counterpart: the runtime's translation, else the call sites of the matched callers
    for f in V.funcs.values():
        f.veldin = f.start
    for k in LEVELS[1:] + [FRONT]:
        img = imgs[k]
        for b, f in img.funcs.items():
            v = M.xlate(k, b, rows)
            if v is not None and v in V.funcs:
                f.veldin, f.how = v, "x"
        for v, t in call_votes(V, img, M.fn[k]).items():
            g = img.funcs.get(t)
            if g is not None and g.veldin is None and v in V.funcs:
                g.veldin, g.how = v, "c"
    variant_rows = {(lv, b) for lv in rows for _v, b, _s, _c in rows[lv]}
    for f in allf.values():
        k, a = f.img, f.start
        via, f.how = f.how, None
        if k == CORE:
            f.status = "ported" if a in ported_core else None
            f.how = "annotated" if f.status else None
        elif (k, a) in ann or (k, a) in reg:
            f.status, f.how = "ported", "annotated" if (k, a) in ann else "registered"
        elif isinstance(k, int) and (k, a) in sky:
            f.status, f.how = "ported", "sky row"
        elif k != 0 and f.veldin is not None and f.veldin in ported_v:
            same = f.body == V.funcs[f.veldin].body or M.fn[k].get(f.veldin) == a
            if same:
                f.status = "ported"
                f.how = "variants row" if (k, a) in variant_rows else "matched" if M.fn[k].get(f.veldin) == a \
                    else "translated"
            else:
                f.status, f.standin_of = "standin", f.veldin
                f.how = "pointer" if via == "x" else "call"
                O, VO = imgs[k].O, V.O
                i, j = (a - O.code_base) // 4, (f.veldin - VO.code_base) // 4
                n = max(f.size, V.funcs[f.veldin].size) // 4
                if O.masked[i:i + n] == VO.masked[j:j + n]:
                    f.how += ", masked-identical"
        if f.status == "ported":
            e = ann.get((k, a)) or (ann.get((0, f.veldin)) if f.veldin is not None else None)
            if e:
                f.name, f.file, f.partial = e["name"], sorted(e["files"])[0], e["partial"]
        elif f.veldin is not None and (0, f.veldin) in ann:
            f.name = ann[(0, f.veldin)]["name"]
    # the rest: the same body ported elsewhere (another level's own port, not wired here), empty, or nothing
    ported_bodies = collections.defaultdict(set)
    for f in allf.values():
        if f.status == "ported":
            ported_bodies[f.body].add(f.img)
    for f in allf.values():
        if f.status == "standin" and is_empty(imgs[f.img], f):
            f.status, f.how = None, None  # (a stub: below)
        elif f.status == "standin" and (f.img, f.start) in variant_rows and ported_bodies.get(f.body):
            # a level sharing another level's build (the Novalis family's hero): the Veldin function dispatches to it
            f.status, f.how, f.standin_of = "ported", "variants row, shared build", None
    for f in allf.values():
        if f.status is not None:
            continue
        if is_empty(imgs[f.img], f):
            f.status = "empty"
        elif ported_bodies.get(f.body) and not f.roots and not f.ptr_in and f.callers:
            f.status, f.how = "pending", "shared"  # (decided below, from its callers)
        elif ported_bodies.get(f.body):
            f.status = "unwired"
        else:
            f.status = "todo"
    # the same body as a port, only called directly: the callers' ports call that port, whatever its address
    changed = True
    while changed:
        changed = False
        for f in allf.values():
            if f.status == "pending":
                st = {allf[c].status for c in f.callers if c in allf}
                if st <= {"ported"}:
                    f.status, changed = "ported", True
                elif "pending" not in st:
                    f.status, f.how, changed = "unwired", None, True
    for f in allf.values():
        if f.status == "pending":
            f.status, f.how = "unwired", None
    # the copies with no Veldin counterpart yet whose shape (the masked instructions) only one Veldin function has: that
    # function's build (for the descriptions and the families; the status above does not change)
    vshape = collections.defaultdict(list)
    for f in V.funcs.values():
        vshape[keys[f.node][0]].append(f.start)
    for f in allf.values():
        if f.veldin is None and f.img not in (0, CORE) and f.size >= 24:
            c = vshape.get(keys[f.node][0], [])
            if len(c) == 1:
                f.veldin = c[0]
    # unique bodies and families (a family: a function's builds, its copies paired with Veldin's)
    uf = UF()
    for f in allf.values():
        uf.union("b:" + f.body, nk(f.img, f.start))
    for k in LEVELS[1:] + [FRONT]:
        for v, b in M.fn[k].items():
            if v in V.funcs and b in imgs[k].funcs:
                uf.union(nk(0, v), nk(k, b))
    for f in allf.values():
        f.uid = uf.find(nk(f.img, f.start))
    fam = UF()
    for f in allf.values():
        fam.union(f.uid, f.uid)
        if f.veldin is not None and f.img not in (0, CORE) and f.veldin in V.funcs:
            fam.union(f.uid, V.funcs[f.veldin].uid)
    for f in allf.values():
        f.fam = fam.find(f.uid)


def file_hints(imgs, allf):
    """The subsystem of an unported function: the source file of the nearest ported function (Veldin's addresses
    for a copy with a Veldin equivalent)."""
    per = collections.defaultdict(list)
    for f in allf.values():
        if f.file:
            per[f.img].append((f.start, f.file))
    for k in per:
        per[k].sort()
    out = {}
    for f in allf.values():
        if f.file:
            out[f.node] = f.file
            continue
        key, a = f.img, f.start
        if f.veldin is not None and f.img not in (0, CORE):
            key, a = 0, f.veldin
        lst = per.get(key, [])
        i = bisect.bisect_right(lst, (a, "~")) - 1
        cand = []
        if i >= 0:
            cand.append((a - lst[i][0], lst[i][1]))
        if i + 1 < len(lst):
            cand.append((lst[i + 1][0] - a, lst[i + 1][1]))
        out[f.node] = "~" + min(cand)[1] if cand else ""
    return out


# ---- runtime evidence ---------------------------------------------------------------------------------------------

RT_LEVEL = re.compile(r"^\[orig\] level (\d+) started")
RT_MOBY = re.compile(r"^\[moby\] \w+ class (\d+) fn ([0-9a-f]+)")
RT_PART = re.compile(r"^\[particle\] type ([0-9a-f]+) update ([0-9a-f]+) not ported")
RT_TODO = re.compile(r"^\[TODO_PORT\] Unported call to 0x([0-9a-f]+) \(([^)]*)\) from caller: (\S+)")


def read_runtime(d, imgs, M):
    """{node: set of evidence strings}; [(image, addr, name, caller)] of the TODO_PORT lines. A run's image is the level
    its file names (L<nn>_...) until a `[orig] level N started` line, the boot module for boot_*.log."""
    ev = collections.defaultdict(set)
    todos = set()
    for p in sorted(Path(d).glob("*.log")):
        m0 = re.match(r"L(\d+)_", p.name)
        level = int(m0.group(1)) if m0 else FRONT if p.name.startswith("boot_") else None
        for line in p.read_text(errors="replace").split("\n"):
            m = RT_LEVEL.match(line)
            if m:
                level = int(m.group(1))
                continue
            if level is None:
                continue
            L = imgs[level]
            m = RT_MOBY.match(line)
            if m:
                g = L.containing(int(m.group(2), 16))
                if g:
                    ev[g.node].add("live moby of class " + m.group(1))
                continue
            m = RT_PART.match(line)
            if m:
                g = L.containing(int(m.group(2), 16))
                if g:
                    ev[g.node].add("particle type 0x" + m.group(1))
                continue
            m = RT_TODO.match(line)
            if m:
                a = int(m.group(1), 16)
                todos.add((img_name(level), a, m.group(2), m.group(3)))
                g = None
                if a < CORE_END:
                    g = imgs[CORE].containing(a)
                elif level != 0 and a in M.fn[level] and not ("callback" in m.group(2) or "function" in m.group(2)):
                    g = L.containing(M.fn[level][a])  # (a port's literal: Veldin's address)
                else:
                    g = L.containing(a)  # (a code pointer the game holds: the image's own address)
                if g:
                    ev[g.node].add("TODO_PORT from " + m.group(3))
    return ev, sorted(todos, key=str)


# ---- scan ---------------------------------------------------------------------------------------------------------


def cmd_scan(a):
    imgs, other_region = load_images()
    ann = read_annotations()
    # the boot module's own functions annotated with a bare address in src/game/boot (its addresses overlap Veldin's)
    fstarts = set(imgs[FRONT].O.function_starts())
    for (s, x), e in list(ann.items()):
        if s == 0 and x in fstarts and all(f.startswith("src/game/boot/") for f in e["files"]):
            d = ann.setdefault((FRONT, x), {"files": set(), "name": None, "partial": True})
            d["files"] |= e["files"]
            d.setdefault("sites", []).extend(e.get("sites", ()))
            d["name"], d["partial"] = d["name"] or e["name"], d["partial"] and e["partial"]
            del ann[(s, x)]
    reg, cases, rows, sky, todo, reg_where = read_registrations()
    vstarts = set(imgs[0].O.function_starts())
    reg = {(FRONT if s == 0 and x not in vstarts and x in fstarts else s, x) for s, x in reg}
    reg_where = {(FRONT if s == 0 and x not in vstarts and x in fstarts else s, x): v for (s, x), v in reg_where.items()}
    M = Maps()
    # the extra starts: the ports' addresses, the maps' (both sides), the variants rows, the core's callers
    extra = collections.defaultdict(set)
    for (s, x) in [k for k, e in ann.items() if not e.get("inline")] + list(reg):
        extra[s].add(x)  # (an address a port covers inline may be a block: not a start)
    for lv in LEVELS[1:]:
        extra[0] |= set(M.fn[lv])
        extra[lv] |= set(M.fn[lv].values())
        extra[lv] |= {b for _v, b, _s, _c in rows[lv]}
        extra[0] |= {v for v, _b, _s, _c in rows[lv]}
    extra[FRONT] |= set(M.front_bv) | set(M.fn[FRONT].values())
    for lv, b in sky:
        extra[lv].add(b)
    for key, img in imgs.items():
        if key == CORE:
            continue
        img.discover(extra[key])
        for f in img.funcs.values():  # the core functions the image calls
            for p in range(f.start, f.end, 4):
                w = img.word(p)
                if w >> 26 == 3:
                    t = (w & 0x3FFFFFF) << 2
                    if imgs[CORE].O.in_code(t):
                        extra[CORE].add(t)
        for d, typ, b in img.O.secs[:6]:
            if typ == 1:
                extra[CORE] |= {w for w in om.words(b) if imgs[CORE].O.in_code(w)
                                and imgs[CORE].O.code[(w - imgs[CORE].O.code_base) // 4] >> 16 == 0x27BD}
    imgs[CORE].discover(extra[CORE])
    build_edges(imgs)
    roots = data_roots(imgs, M)
    allf = reach(imgs, roots)
    classify(imgs, allf, ann, reg | cases, rows, sky, M)  # (a case is a port only where a function starts)
    hints = file_hints(imgs, allf)
    ev, todos = ({}, [])
    if a.runtime:
        ev, todos = read_runtime(a.runtime, imgs, M)
        for n, s in ev.items():
            if n in allf:
                allf[n].runtime = s
    for f in allf.values():  # the TODO_PORT stubs' names
        if f.name is None:
            v = f.veldin if f.veldin is not None else f.start
            if v in todo:
                f.name = sorted(todo[v])[0]
    census = read_census(a.census) if a.census else None
    write_scan(imgs, allf, hints, todos, todo, census, other_region, ann, reg)
    write_catalog_inputs(imgs, ann, reg_where, census)


def read_census(path):
    """{level: {class: instances}} (docs/reversing/moby_class_census.csv of the development repository: counts only)."""
    out = collections.defaultdict(dict)
    with open(path) as fh:
        for r in csv.DictReader(fh):
            c = int(r["class_id"])
            for lv in LEVELS:
                n = int(r.get("lvl%02d" % lv) or 0)
                out[lv][c] = n
    return out


def write_scan(imgs, allf, hints, todos, todo_lits, census, other_region, ann, reg):
    OUT.mkdir(parents=True, exist_ok=True)
    order = [CORE, FRONT] + LEVELS
    with open(OUT / "functions.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["image", "addr", "size", "status", "how", "reach", "roots", "orphan", "veldin", "standin_of", "uid",
                    "family", "callers", "callees", "name", "role", "file", "placed", "runtime"])
        for k in order:
            for a in imgs[k].starts:
                f = imgs[k].funcs[a]
                placed = ""
                if census is not None and f.role and isinstance(k, int):
                    placed = "/".join(str(census[k].get(int(c), 0)) for c in f.role.split()[1].split("/"))
                w.writerow([img_name(k), "%08x" % a, f.size, f.status, f.how or "", f.reach, "/".join(sorted(f.roots)),
                            int(f.orphan), "%08x" % f.veldin if f.veldin is not None else "",
                            "%08x" % f.standin_of if f.standin_of is not None else "", "%s" % (f.uid,), "%s" % (f.fam,),
                            len(f.callers), len(f.calls | f.ptrs), f.name or "", f.role or "", hints.get(f.node, ""),
                            placed, "; ".join(sorted(f.runtime))])
    with open(OUT / "edges.csv", "w", newline="") as fh:  # caller -> callee (call: jal / j; pointer: a code-built pointer)
        w = csv.writer(fh)
        w.writerow(["from", "to", "kind"])
        for f in allf.values():
            for n in sorted(f.calls, key=str):
                w.writerow([nk(*f.node), nk(*n), "call"])
            for n in sorted(f.ptrs - f.calls, key=str):
                w.writerow([nk(*f.node), nk(*n), "pointer"])
    with open(OUT / "runtime_todo.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["level", "addr", "name", "caller"])
        for lv, x, n, c in todos:
            w.writerow([lv, "%08x" % x, n, c])
    print("[port_inventory] wrote %s (%d functions)" % (OUT / "functions.csv", len(allf)))
    summary = {"other_region": other_region, "pal_changed": {str(k): v for k, v in pal_changed().items()},
               "annotations": len(ann), "registrations": len(reg), "census": census is not None,
               "todo_literals": {"%08x" % k: sorted(v) for k, v in todo_lits.items()}}
    (OUT / "summary.json").write_text(json.dumps(summary, indent=1))


def write_catalog_inputs(imgs, ann, reg_where, census):
    """The catalog's inputs (tools/inventory/port_catalog.py): ports.csv (every `// orig:` site and registration: the
    function, the file, the line, our description) and classes.csv (each image's moby class table: class, update
    function, the hook table's functions, the placements of the census)."""
    with open(OUT / "ports.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["image", "addr", "file", "line", "generated", "kind", "name", "partial", "desc"])
        out = set()
        for (k, a), e in ann.items():
            for rel, line, gen, name, partial, desc in e.get("sites", ()):
                kind = "inline" if desc.startswith("covered inline by") else "orig"
                out.add((img_name(k), "%08x" % a, rel, line, gen, kind, name, partial, desc))
        for (k, a), files in reg_where.items():
            if a not in imgs[k].funcs:
                continue  # (a dispatch `case` that is a block of a function, not its start)
            for rel in files:
                gen = rel.startswith("gen:")
                out.add((img_name(k), "%08x" % a, rel[4:] if gen else rel, 0, int(gen), "registered", "", 0, ""))
        for row in sorted(out, key=lambda x: (x[0], x[1], x[2], x[3], x[5])):
            w.writerow(row)
    with open(OUT / "classes.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["image", "class", "update", "hooks", "placed"])
        for k in [FRONT] + LEVELS:
            img = imgs[k]
            for c, fn, hooks in img.O.table_rows(3, 3):
                hk = []
                for i in range(6):  # (the hook table: six words, the next table 0x18 bytes on)
                    x = img.O.data_word(hooks + 4 * i) if hooks else None
                    if x is not None and x in img.funcs:
                        hk.append("%08x" % x)
                placed = census[k].get(c, 0) if census is not None and isinstance(k, int) else ""
                w.writerow([img_name(k), c, "%08x" % fn if fn in img.funcs else "", ";".join(hk), placed])
    print("[port_inventory] wrote %s, %s" % (OUT / "ports.csv", OUT / "classes.csv"))


# ---- tour ---------------------------------------------------------------------------------------------------------

TOUR = [  # (scenario, extra environment, frames): a walk forward, the wrench, the fire button, the pause menu's pages
    ("walk", {"RAC1_SIM_INPUT": "forward"}, 1800),
    ("attack", {"RAC1_SIM_INPUT": "attack"}, 1500),
    ("fire", {"RAC1_SIM_INPUT": "fire", "RAC1_GIVE_ITEMS": "0x10,0x12,0xd", "RAC1_EQUIP": "0x10"}, 1500),
    ("menu", {"RAC1_SIM_INPUT": "menu:" + "xrrddoo.d" * 8, "RAC1_MENU_AT": "900"}, 3300),
    # the Metal Detector owned, the hero put by a buried-treasure zone marker (class 605) where the level has one
    ("detector", {"RAC1_GIVE_ITEMS": "0x1b", "RAC1_HERO_AT": "605@800,0,0,1"}, 1300),
]
BOOT_TOUR = [  # the boot module: the title's menus (no level)
    ("title", {"RAC1_BOOT": "1", "RAC1_NO_MOVIES": "1", "RAC1_TITLE_WAIT": "300",
               "RAC1_SIM_INPUT": "title:" + "dxrrddoo" * 5}, 2200),
]


def cmd_tour(a):
    """Headless runs (one at a time) with RAC1_MOBY_LIST: logs <out>/L<nn>_<scenario>.log, boot_<scenario>.log."""
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    binp = Path(a.build) / "rac1-native"
    runs = [("L%02d_%s" % (lv, name), dict(extra, RAC1_LEVEL=str(lv)), frames)
            for lv in [int(x) for x in a.levels.split(",") if x] for name, extra, frames in TOUR
            if not a.only or name in a.only.split(",")]
    if a.boot:
        runs += [("boot_" + name, extra, frames) for name, extra, frames in BOOT_TOUR
                 if not a.only or name in a.only.split(",")]
    for tag, extra, frames in runs:
        log = out / f"{tag}.log"
        if log.exists() and not a.force:
            continue
        env = dict(os.environ, RAC1_HEADLESS="1", RAC1_FRAMES=str(a.frames or frames), RAC1_MOBY_LIST="1", **extra)
        with open(log, "w") as fh:
            try:
                r = subprocess.run([str(binp)], cwd=str(ROOT), env=env, stdout=fh, stderr=subprocess.STDOUT,
                                   timeout=1800).returncode
            except subprocess.TimeoutExpired:
                r = "timeout"
        print("[tour] %s: exit %s" % (tag, r), flush=True)
        for ppm in ROOT.glob("*.ppm"):  # (the headless run's last frame)
            ppm.unlink()


# ---- doc ----------------------------------------------------------------------------------------------------------


def cmd_doc(a):
    import port_inventory_doc  # noqa: E402  (the document's text)
    port_inventory_doc.write(OUT, Path(a.out), a)
    if a.catalog:
        cmd_catalog(a)


def cmd_catalog(a):
    import port_catalog  # noqa: E402  (docs/catalog/)
    port_catalog.write(OUT, Path(a.catalog_out))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("scan")
    s.add_argument("--runtime", help="the tour's logs (tour --out)")
    s.add_argument("--census", help="per-level class placement counts (CSV: class_id, lvl00..lvl18)")
    t = sub.add_parser("tour")
    t.add_argument("--out", default=str(OUT / "tour"))
    t.add_argument("--build", default=str(ROOT / "build"))
    t.add_argument("--levels", default=",".join(str(x) for x in LEVELS))
    t.add_argument("--frames", type=int, default=0)
    t.add_argument("--force", action="store_true", help="run again the scenarios whose log exists")
    t.add_argument("--only", help="these scenarios only (walk,attack,fire,menu,detector,title)")
    t.add_argument("--no-boot", dest="boot", action="store_false", help="leave out the boot module's title run")
    d = sub.add_parser("doc")
    d.add_argument("--out", default=str(DOC))
    d.add_argument("--catalog-out", default=str(CATALOG))
    d.add_argument("--no-catalog", dest="catalog", action="store_false", help="this page only, not docs/catalog/")
    c = sub.add_parser("catalog")
    c.add_argument("--catalog-out", default=str(CATALOG))
    a = ap.parse_args()
    {"scan": cmd_scan, "tour": cmd_tour, "doc": cmd_doc, "catalog": cmd_catalog}[a.cmd](a)


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    main()
