#!/usr/bin/env python3
"""Makes the level-only transliterations region-proof (docs/architecture.md, "The region layer").

The generators run it on every file they write that names a level's own addresses (`L<n>_ADDR`), before the
instruction comments are left out (it reads them): tools/codegen/gen_common.py region_proof.

The level-only ports (`// orig: L<n>:0x...`, tools/codegen/mips2cpp.py with --const-wrap L<n>_ADDR) name the NTSC-U
addresses of their level's own overlay through L<n>_ADDR, which on another region's disc (the PAL release) is the
region table's translation (src/game/ovl/ovl_reloc.h). Two shapes of the generated text do not survive it:

  - a jump table's dispatch, `switch (a0.w()) {  // jr a0` with `case 0x...u:` labels: the table in the disc's
    data holds the disc's code addresses. It becomes `switch (lvl_key(a0.w()))` (the pointer back to NTSC-U's).
  - a `lui` half used apart from its low half: `set32(a1, L5_ADDR(0x00160000u));  // lui` then
    `set32(a1, a1.w() + 0x71c0);` (the addiu in a call's delay slot, which the generator's own folding skips).
    A page is not an address: translating it is wrong wherever the addresses around it moved by different
    amounts. The use becomes the full address, `set32(a1, L5_ADDR(0x001671c0u))`, and the page itself
    `L<n>_PAGE(0x00160000u)`: the NTSC-U value.

The pages are followed through the transliteration's own control flow (labels, gotos, `case` gotos, likely-branch
blocks, returns); a `move` copies the page. A register whose every write in the function is the same page holds it
everywhere. A lui + addiu the generator folded into a raw number above Veldin's code end (a level's callback) becomes
`L<n>_ADDR`. On the NTSC-U disc nothing changes: the folded address is the same number. Idempotent.

Usage (repo root): tools/codegen/reloc_level.py [--check] files...
"""
import re
import sys
from pathlib import Path

LO, HI = 0x15EF00, 0x340000  # the levels' overlays (data from 0x15ef00, the longest code to ~0x332000)

PAGE = re.compile(r"set32\((\w+), L(\d+)_(?:ADDR|PAGE)\((0x(?:00)?[0-9a-f]{2}0000)u?\)\);(\s*// (?:\(delay\) )?lui )")
USE = re.compile(r"(?<![\w.])(\w+)\.w\(\) \+ (-?)(0x[0-9a-fA-F]+)\b")
LABEL = re.compile(r"^\s*(L_\w+|case [^:]+|default):")
FUNC = re.compile(r"^[A-Za-z_][\w:<>,\s\*&]*\s+\**(\w+)\((.*)\)\s*\{\s*$")
JR = re.compile(r"switch \((\w+)\.w\(\)\) \{(\s*// jr (?!\$?ra\b))")
# a raw page (the generator wrapped only data pages: a level's code page), in a function
# of a level's own (`// orig: L<n>:`): its uses folded the same way, the page itself left raw (it may be a value: 0x330000)
RAW = re.compile(r"set32\((\w+), (0x[0-9a-f]{2}0000)u?\);(\s*// (?:\(delay\) )?lui )")
ORIG = re.compile(r"//\s*orig: L(\d+):0x")
RAWADDIU = re.compile(r"(\s*set32\(\w+, )(0x[0-9a-fA-F]+)u?\);")
MOVE = re.compile(r"^\s*(\w+)\.lo = (\w+)\.lo (?:\+|\|) zero\.lo;")


def writes(code):
    """The GPRs a line writes (set32(r, ...), r.lo = / r.hi =, r = ..., ldl_/ldr_(r, ...))."""
    out = set(re.findall(r"set32\((\w+),", code)) | set(re.findall(r"\b(\w+)\.(?:lo|hi) =[^=]", code))
    out |= set(re.findall(r"(?<![\w.\]])(\w+) = (?!=)", code)) | set(re.findall(r"(?:ldl_|ldr_|mult3)\((\w+),", code))
    return out - {"c", "fr"}


def page_of(ln, lvl):
    """(reg, (level, page)) of a `lui` line holding a level page, else None."""
    m = PAGE.search(ln)
    if m:
        return m.group(1), (m.group(2), int(m.group(3), 16))
    m = RAW.search(ln)
    if m and lvl and 0x160000 <= int(m.group(2), 16) < HI:
        return m.group(1), (lvl, int(m.group(2), 16))
    return None


def const_pages(lines, lvl):
    """{reg: (level, page)}: registers whose every write in the function is the same lui page (or a move from one)."""
    w = {}
    for ln in lines:
        code = ln.partition("//")[0]
        if re.search(r"= lq\(sp\.w\(\) \+ ", code):
            continue  # (the epilogue's restore of a callee-saved register)
        m = page_of(ln, lvl)
        mv = MOVE.match(code)
        for r in writes(code):
            if m and m[0] == r:
                w.setdefault(r, []).append(m[1])
            elif mv and mv.group(1) == r:
                w.setdefault(r, []).append(("move", mv.group(2)))
            else:
                w.setdefault(r, []).append(None)
    known = {}
    for _ in range(6):
        for r, ws in w.items():
            vals = {known.get(x[1]) if x and x[0] == "move" else x for x in ws}
            if len(vals) == 1 and None not in vals:
                known[r] = vals.pop()
    known.pop("sp", None)
    return known


def transfer(state, ln, lvl):
    code = ln.partition("//")[0]
    new = dict(state)
    mv = MOVE.match(code)
    for r in writes(code):
        new.pop(r, None)
    if mv and mv.group(2) in state and mv.group(1) != mv.group(2):
        new[mv.group(1)] = state[mv.group(2)]
    po = page_of(ln, lvl)
    if po:
        new[po[0]] = po[1]
    return new


def flow_states(lines, lvl):
    """The pages every path to each line holds (a forward must data-flow over the transliteration's own control flow:
    labels, gotos, `case` gotos, likely-branch blocks, returns), as tools/codegen/hi_flow.py does over the instruction words for
    the Veldin ports; None for a function whose text has other control flow (a hand-written loop: the straight-line pass)."""
    n = len(lines)
    codes = [ln.partition("//")[0].strip() for ln in lines]
    if any(re.match(r"^(for|while|do|else)\b|^\} else", c) for c in codes):
        return None
    labels = {}
    for i, c in enumerate(codes):
        m = re.match(r"^(L_\w+):", c)
        if m:
            labels[m.group(1)] = i
    match, stack = {}, []
    for i, c in enumerate(codes):
        if c.startswith("}"):
            if not stack:
                return None
            match[stack.pop()] = i
        if c.endswith("{"):
            stack.append(i)
    succ = [[] for _ in range(n)]
    for i, c in enumerate(codes):
        for g in re.findall(r"goto (L_\w+);", c):
            if g not in labels:
                return None
            succ[i].append(labels[g])
        ends = re.match(r"^(goto L_\w+;|return\b|default: return\b)", c) is not None
        if not ends and i + 1 < n:
            succ[i].append(i + 1)
        if c.endswith("{") and c.startswith("if ") and i in match and match[i] + 1 < n:
            succ[i].append(match[i] + 1)  # (a likely branch's block, skipped)
    IN = [None] * n
    IN[0] = {}
    work = [0]
    while work:
        i = work.pop()
        out = transfer(IN[i], lines[i], lvl)
        for t in succ[i]:
            new = dict(out) if IN[t] is None else {r: v for r, v in IN[t].items() if out.get(r) == v}
            if new != IN[t]:
                IN[t] = new
                work.append(t)
    return IN


def fold_function(lines, stats, lvl):
    consts = const_pages(lines, lvl)
    flow = flow_states(lines, lvl) if lines else None
    held = dict(consts)
    stack = []  # the open blocks: True for a branch's own block (straight-line), False for a conditional one
    out = []
    for idx, ln in enumerate(lines):
        code, sep, cmt = ln.partition("//")
        if flow is not None:
            held = dict(flow[idx] or {})
        if LABEL.match(code):
            held = dict(consts)
            out.append(ln)
            continue
        stripped = code.strip()
        # the uses of a held page
        def use(m):
            r = m.group(1)
            if r not in held or r == "sp":
                return m.group(0)
            lv, pg = held[r]
            full = (pg + int(m.group(3), 16) * (-1 if m.group(2) else 1)) & 0xFFFFFFFF
            if 0x100000 <= full < LO:  # (the base ELF below the overlays: the same on every disc, a plain number)
                stats["folded"] += 1
                return f"0x{full:08x}u"
            if not LO <= full < HI:
                return m.group(0)
            stats["folded"] += 1
            return f"L{lv}_ADDR(0x{full:08x}u)"
        new = USE.sub(use, code)
        # a lui + addiu the generator folded into a raw number (it wrapped only the addresses below Veldin's code end,
        # 0x2f0cd0: the levels' code reaches 0x332000): the level's code address (a callback, a hook)
        ra = RAWADDIU.match(new)
        if ra and lvl and re.match(r"\s*(\(delay\) )?addiu ", cmt) and LO <= int(ra.group(2), 16) < HI:
            new = new[:ra.start(2)] + f"L{lvl}_ADDR(0x{int(ra.group(2), 16):08x}u)" + new[ra.end(2) + (1 if new[ra.end(2):ra.end(2) + 1] == "u" else 0):]
            stats["folded"] += 1
        # the page itself: kept as the NTSC-U value (L<n>_PAGE)
        pm = PAGE.search(ln)
        if pm:
            old = f"set32({pm.group(1)}, L{pm.group(2)}_ADDR({pm.group(3)}u));"
            if old in new:
                new = new.replace(old, f"set32({pm.group(1)}, L{pm.group(2)}_PAGE({pm.group(3)}u));")
                stats["pages"] += 1
        # what the line writes
        mv = MOVE.match(code)
        wr = writes(code)
        for r in wr:
            held.pop(r, None)
        if mv and mv.group(2) in held and mv.group(1) != mv.group(2):
            held[mv.group(1)] = held[mv.group(2)]
        po = page_of(ln, lvl)
        if po:
            held[po[0]] = po[1]
        # blocks
        if stripped.startswith("}"):
            if stack and not stack.pop():
                held = {r: v for r, v in held.items() if consts.get(r) == v}
            if stripped.endswith("{"):  # } else {
                stack.append(False)
        elif stripped.endswith("{"):
            stack.append(bool(re.match(r"^\{\s*$", stripped)) and bool(re.match(r"\s*b", cmt)))
        out.append(new + sep + cmt)
    return out


def relocate(text):
    stats = {"folded": 0, "pages": 0, "switches": 0}
    text, n = JR.subn(r"switch (lvl_key(\1.w())) {\2", text)
    stats["switches"] = n
    lines = text.split("\n")
    out, i = [], 0
    while i < len(lines):
        if FUNC.match(lines[i]):
            j = i + 1
            while j < len(lines) and lines[j] != "}":
                j += 1
            out.append(lines[i])
            orig = [ORIG.search(x) for x in lines[max(0, i - 4):i]]
            lvl = next((m.group(1) for m in reversed(orig) if m), None)
            out += fold_function(lines[i + 1:j], stats, lvl)
            i = j
            continue
        out.append(lines[i])
        i += 1
    return "\n".join(out), stats


def main():
    check = "--check" in sys.argv
    files = [Path(a) for a in sys.argv[1:] if not a.startswith("--")]
    total = {"folded": 0, "pages": 0, "switches": 0}
    changed = 0
    for p in files:
        text = p.read_text()
        new, st = relocate(text)
        if new != text:
            changed += 1
            print(f"{p}: {st['switches']} switches, {st['folded']} uses folded, {st['pages']} pages")
            for k in total:
                total[k] += st[k]
            if not check:
                p.write_text(new)
    print(f"[reloc_level] {changed} files: {total['switches']} switches, {total['folded']} uses folded, {total['pages']} pages")
    return 1 if check and changed else 0


if __name__ == "__main__":
    sys.exit(main())
