#!/usr/bin/env python3
# usage: ovlist.py <level | front> <addr hex> [--maxwords N]  > listing.s
#
# A function's listing straight from a level's overlay (read from the ISO by tools/overlay_match, no Ghidra
# project needed): lines `addr word mnemonic operands` in Ghidra's operand style (`addiu sp,sp,-0x60`), the
# input of tools/codegen/asm2cpp.py (gen_asm2cpp.py; the text is tools/codegen/r5900dis.py's). The function runs from `addr` to the first `jr ra` (and its delay slot)
# that no earlier branch of the function jumps beyond, capped at the next function start.
import os
import re
import struct
import sys

W = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, W + '/tools/overlay_match')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import overlay_match as om  # noqa: E402
import region_view as rv  # noqa: E402

_cache = {}
ASSETS = os.environ.get('RAC1_ASSETS') or W + '/assets'  # (another asset root, e.g. a clean one made by setup.sh --code)


FRONT = 'front'  # the boot module (SCUS_971.99's own engine build): `level` 'front'


def overlay(level, iso_path=None):
    """The level's overlay (or the boot module's) at the NTSC-U addresses the ports use: on another region's disc, its
    code rebuilt at NTSC-U's through the region table (tools/overlay_match/region_view.py)."""
    if level not in _cache:
        iso_path = iso_path or rv.disc_path(ASSETS)
        _ser, elf, table = rv.disc_of(iso_path)
        if level == FRONT:
            import front_match as fm  # noqa: E402
            b = open(os.path.join(ASSETS, 'dumps', elf), 'rb').read()
            _cache[level] = rv.boot_view(b, table) if table else om.Overlay(fm.boot_module(b), 0x1E9658)
        elif table:
            _cache[level] = rv.level_view(iso_path, level, table)
        else:
            with open(iso_path, 'rb') as iso:
                _cache[level] = om.Overlay(*om.level_overlay(iso, level))
    return _cache[level]


def map_dir():
    """The overlay maps the generators read (NTSC-U Veldin -> NTSC-U level N): the setup's on the NTSC-U disc; on another
    region's disc, those made on the rebuilt NTSC-U code (region_view.py maps), as the game's own maps there go to the
    disc's addresses."""
    other = rv.disc_of(rv.disc_path(ASSETS))[2] is not None
    return os.path.join(ASSETS, 'generated', 'overlay_map', 'codegen' if other else '')


def _known(O, a):
    if hasattr(O, 'is_known') and not O.is_known(a):
        raise SystemExit('[ovlist] 0x%08x: code the region changed (its NTSC-U code is not on this disc)' % a)


def _text(w, a):
    """The instruction's text (a comment only: tools/codegen/r5900dis.py, Ghidra's style; no capstone, whose MIPS64 mode
    printed the R5900's lq / sq / MMI / COP2 ops as MSA / DSP ones and whose text varied with its version)."""
    import r5900dis
    return r5900dis.text(w, a)


def _jump_table(O, win):
    """The targets of the jump table a `jr` reads (the compiler's switch idiom: sltiu bound; lui / addiu base; sll 2; addu; lw; jr)."""
    bound = None
    for w in reversed(win):
        if (w >> 26) == 0xb and bound is None:  # sltiu
            bound = w & 0xffff
    base = None
    hi = None
    for w in win:
        op = w >> 26
        if op == 0xf:  # lui
            hi = (((w >> 16) & 31), (w & 0xffff) << 16)
        elif op == 9 and hi and ((w >> 21) & 31) == hi[0]:  # addiu rt,<the lui register>,imm
            imm = (w & 0xffff) - ((w & 0x8000) << 1)
            base = (hi[1] + imm) & 0xffffffff
    if not bound or base is None:
        return []
    return [t for t in (O.data_word(base + 4 * k) for k in range(bound)) if t]


STANDINS = []  # (NTSC-U address, PAL address) of the stand-ins listed (another region's disc: see listing)


def listing(level, addr, maxwords=40000):
    O = overlay(level)
    if hasattr(O, 'standin') and not O.is_known(addr) and os.environ.get('RAC1_PAL_CHANGED', 'standin') == 'standin':
        # code the region changed: the disc's own build of the function, at its NTSC-U entry (a temporary stand-in,
        # labelled by the generators; RAC1_PAL_CHANGED=fail stops instead)
        S = O.standin(addr)
        if S is not None:
            STANDINS.append((addr, S.pal))
            sys.stderr.write('[ovlist] STANDIN %08x PAL %08x\n' % (addr, S.pal))
            O = S
    starts = sorted(O.function_starts())
    nxt = next((s for s in starts if s > addr), O.code_end)
    out = []
    maxt = addr
    i0 = (addr - O.code_base) // 4
    last_end = None
    k = -1
    while k + 1 < maxwords:
        k += 1
        a = addr + 4 * k
        if a >= nxt and a > maxt:  # (the matcher's function starts include a switch's cases: the cap yields to the branch / jump-table targets seen)
            break
        _known(O, a)
        w = O.code[i0 + k]
        op = w >> 26
        if op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (op == 1 and ((w >> 16) & 0x1f) in (0, 1, 2, 3)) or (op in (0x11, 0x12) and ((w >> 21) & 0x1f) == 8):
            # (not bltzal / bgezal: those are calls to routines of their own; bc1t / bc1f / ..l, bc2t / bc2f: COP rs field = 8)
            imm = (w & 0xffff) - ((w & 0x8000) << 1)
            maxt = max(maxt, a + 4 + 4 * imm)
        if op == 2:  # j: a jump inside the function (before the next function start) to code that follows a `jr ra`
            t = ((a + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2)
            if addr <= t < nxt:
                maxt = max(maxt, t)
        if (w >> 26) == 0 and (w & 0x3f) == 8 and ((w >> 21) & 31) != 31:  # jr <reg>: a jump table (its cases may lie beyond the first `jr ra`)
            for t in _jump_table(O, [O.code[i0 + j] for j in range(max(0, k - 40), k)]):
                maxt = max(maxt, t)
        mn, ops = _text(w, a)
        out.append('%08x %08x %s %s' % (a, w, mn, ops))
        if w == 0x03E00008 and a + 4 >= maxt:  # jr ra: the end unless a later branch target lies beyond
            if k + 1 < len(O.code) - i0:
                dw = O.code[i0 + k + 1]
                dmn, dops = _text(dw, a + 4)
                out.append('%08x %08x %s %s' % (a + 4, dw, dmn, dops))
            last_end = k + 2
            break
    if last_end is None:
        last_end = len(out)
    return out[:last_end]


if __name__ == '__main__':
    lv, ad = (FRONT if sys.argv[1] == FRONT else int(sys.argv[1])), int(sys.argv[2], 16)
    mw = int(sys.argv[sys.argv.index('--maxwords') + 1]) if '--maxwords' in sys.argv else 40000
    print('\n'.join(listing(lv, ad, mw)))
