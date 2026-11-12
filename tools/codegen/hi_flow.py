#!/usr/bin/env python3
"""Reaching `lui` halves of the overlay's addresses (ADR 0008), for tools/codegen/mips2cpp.py.

A compiled function builds an address as `lui r,hi` + `addiu r,r,lo` (or uses r as the base of a load / store), and the
two are often far apart: a `lui` before a loop, an `addiu` after a label. The text pass of tools/codegen/reloc_asm.py only
follows straight-line code; this one follows the control flow: a forward data-flow over the instruction words (branch
delay slots, `beql` / `bnel` likelies, calls clobbering the caller-saved registers) of which registers hold a `lui` half
of an overlay address on EVERY path to an instruction. Such a use (`rs.w() + off` of an addiu / load / store) can be
rewritten to OV(hi + off). A register that holds a half on some paths only is reported (the caller turns it into an
#error to resolve by hand)."""
import re

REG = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
       's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 's8', 'ra']
CALLER = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 31]
DATA = (0x15EF00, 0x1EAA00)
CODE = (0x1EAA00, 0x2F0CD0)
NONE = 'none'
ADDR = 'addr'


def sx16(x):
    return x - 0x10000 if x & 0x8000 else x


def wrap(a):
    if DATA[0] <= a < DATA[1]:
        return f'OV(0x{a:08x}u)'
    if CODE[0] <= a < CODE[1]:
        return f'OVF(0x{a:08x}u)'
    return None


def dest_reg(w):
    """(dest GPR or None, known) of one instruction word; known False: an opcode the analysis does not know (all clobbered)"""
    op, rs, rt, rd, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, w & 63
    if w == 0:
        return None, True
    if op == 0:
        if fn in (0x08, 0x18, 0x19, 0x1a, 0x1b, 0x11, 0x13, 0x0f, 0x0d):
            return None, True  # jr, mult, multu, div, divu, mthi, mtlo, sync, break
        if fn == 0x09:
            return rd, True  # jalr
        return rd, True
    if op in (1, 2, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17):
        return None, True  # branches, j
    if op == 3:
        return 31, True
    if op in (8, 9, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x37, 0x1e, 0x1a, 0x1b):
        return rt, True
    if op in (0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x3f, 0x1f, 0x31, 0x39, 0x36, 0x3e, 0x33):
        return None, True  # stores, lwc1 / swc1, lqc2 / sqc2, pref
    if op == 0x11:  # COP1: mfc1 / cfc1 write rt
        return (rt, True) if rs in (0, 2) else (None, True)
    if op == 0x12:  # COP2: qmfc2 / cfc2 write rt
        return (rt, True) if rs in (1, 2) else (None, True)
    if op == 0x1c:  # MMI: most write rd
        return rd, True
    return None, False


def uses_base(w):
    """(base reg, offset, kind) when the instruction addresses memory / adds an immediate to rs, else None"""
    op, rs = w >> 26, (w >> 21) & 31
    imm = sx16(w & 0xffff)
    if op in (8, 9):
        return rs, imm, 'addiu'
    if op in (0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x31, 0x39, 0x36, 0x3e,
              0x37, 0x3f, 0x1e, 0x1f, 0x33, 0x1a, 0x1b):
        return rs, imm, 'mem'
    return None


def analyse(ins, tr_all, jr_targets):
    """ins: [(addr, word, ...)], tr_all: [(addr, word, mn, ops, st, br)]. Returns in_state: list (per instruction index) of
    dict reg -> frozenset of values ('hi', H) | ADDR | NONE (the union of every way the instruction is reached: directly, or as
    a branch's delay slot)."""
    n = len(ins)
    idx = {a: i for i, (a, *_r) in enumerate(ins)}
    direct = [None] * n  # reached as an instruction in its own right
    ctx = [None] * n  # reached as the delay slot of a branch (the state after that branch)
    direct[0] = {}

    def get(st, r):
        return st.get(r, frozenset([NONE]))

    def transfer(st, w):
        st = dict(st)
        op, rs, rt, rd, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, w & 63
        d, known = dest_reg(w)
        if not known:
            return {}  # unknown instruction: forget everything
        if d is None or d == 0:
            return st
        if op == 0x0f:
            imm = w & 0xffff
            st[d] = frozenset([('hi', imm << 16)]) if 0x15 <= imm < 0x30 else frozenset([NONE])
        elif op in (8, 9):
            src = get(st, rs)
            his = [v for v in src if isinstance(v, tuple)]
            if his and len(src) == 1:
                st[d] = frozenset([ADDR])
            elif his:
                st[d] = frozenset(['mixed'])
            elif src == frozenset([ADDR]) and d == rs:
                st[d] = frozenset([ADDR])
            else:
                st[d] = frozenset([NONE])
        elif op == 0 and fn in (0x20, 0x21, 0x25, 0x2c, 0x2d) and (rt == 0 or rs == 0):
            st[d] = get(st, rs if rt == 0 else rt)  # move
        else:
            st[d] = frozenset([NONE])
        return st

    def merge(arr, i, st):
        if i >= n:
            return False
        cur = arr[i]
        if cur is None:
            arr[i] = dict(st)
            return True
        changed = False
        new = {}
        for k in set(cur) | set(st):
            v = get(cur, k) | get(st, k)
            new[k] = v
            if v != get(cur, k):
                changed = True
        if changed:
            arr[i] = new
        return changed

    def clobber(st):
        st = dict(st)
        for r in CALLER:
            st[r] = frozenset([NONE])
        return st

    changed = True
    guard = 0
    while changed and guard < 300:
        changed = False
        guard += 1
        for i in range(n):
            st = direct[i]
            if st is None:
                continue
            a, w, mn, ops, s_, br = tr_all[i]
            if br is None:
                changed |= merge(direct, i + 1, transfer(st, w))
                continue
            kind, cond, tgt = br
            after = transfer(st, w)  # (the branch itself writes no GPR but a call's ra)
            changed |= merge(ctx, i + 1, after)
            dsw = tr_all[i + 1][1] if i + 1 < n else 0
            after_ds = transfer(ctx[i + 1] if i + 1 < n else after, dsw)
            if kind == 'call':
                changed |= merge(direct, i + 2, clobber(after_ds))
            elif kind == 'jr':
                for t in (jr_targets if cond != 'ra' else []):
                    if t in idx:
                        changed |= merge(direct, idx[t], after_ds)
            elif kind == 'j' or cond == 'true':
                if tgt in idx:
                    changed |= merge(direct, idx[tgt], after_ds)
            else:
                if tgt in idx and cond != 'false':
                    changed |= merge(direct, idx[tgt], after_ds)
                changed |= merge(direct, i + 2, after if kind == 'likely' else after_ds)
    out = []
    for i in range(n):
        if direct[i] is None and ctx[i] is None:
            out.append(None)
        elif direct[i] is None:
            out.append(ctx[i])
        elif ctx[i] is None:
            out.append(direct[i])
        else:
            m = dict(direct[i])
            for k in set(m) | set(ctx[i]):
                m[k] = get(direct[i], k) | get(ctx[i], k)
            out.append(m)
    return out


def rewrite(ins, tr_all, jr_targets):
    """New statements for tr_all (addr, word, mn, ops, st, br): the uses of a register with a definite lui half rewritten to
    OV / OVF; returns (tr_all', errors)"""
    in_state = analyse(ins, tr_all, jr_targets)
    out, errs = [], []
    for i, (a, w, mn, ops, st, br) in enumerate(tr_all):
        ub = uses_base(w)
        s = in_state[i]
        if ub is not None and s is not None and st and not st.startswith('#error'):
            rb, off, kind = ub
            vals = s.get(rb, frozenset([NONE]))
            his = [v for v in vals if isinstance(v, tuple)]
            if his and len(vals) == 1:
                hi = his[0][1]
                wr = wrap((hi + off) & 0xFFFFFFFF)
                if wr is not None:
                    pat = rf'\b{REG[rb]}\.w\(\) \+ (?:-?0x[0-9a-fA-F]+)'
                    new = re.sub(pat, wr, st, count=1)
                    if new != st:
                        st = new
            elif his:
                errs.append(f'0x{a:08x}: {REG[rb]} holds an overlay address half on some paths only')
        out.append((a, w, mn, ops, st, br))
    return out, errs


def gpr_reads(w):
    """the GPRs one instruction word reads (approximate: the operands of the opcodes the transliterator knows)"""
    op, rs, rt, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 63
    r = set()
    if op == 0:
        if fn == 0x08:
            r = {rs}
        elif fn in (0x00, 0x02, 0x03):
            r = {rt}
        elif fn in (0x10, 0x12):
            r = set()
        elif fn in (0x11, 0x13, 0x09):
            r = {rs}
        else:
            r = {rs, rt}
    elif op in (4, 5, 0x14, 0x15):
        r = {rs, rt}
    elif op in (1, 6, 7, 0x16, 0x17):
        r = {rs}
    elif op in (8, 9, 0xa, 0xb, 0xc, 0xd, 0xe):
        r = {rs}
    elif op in (0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x1e, 0x37, 0x31, 0x36, 0x1a, 0x1b):
        r = {rs}
    elif op in (0x39, 0x3e, 0x3d):  # (swc1 / sqc2 / sdc1: rt is an FPR / a VU register)
        r = {rs}
    elif op in (0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x1f, 0x3f):
        r = {rs, rt}
    elif op == 0x11 and rs == 4:
        r = {rt}
    elif op == 0x1c:
        r = {rs, rt}
    r.discard(0)
    return r


def fpr_reads(w):
    """the FPRs one instruction word reads"""
    op, rs, rt, rd, sa, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
    if op == 0x39:
        return {rt}
    if op == 0x11:
        if rs == 0:
            return {rd}
        if rs == 0x14:
            return {rd}
        if rs == 0x10:
            if fn in (0x00, 0x01, 0x02, 0x03, 0x34, 0x36, 0x32):
                return {rd, rt}
            if fn in (0x05, 0x06, 0x07, 0x04, 0x24, 0x0d, 0x0c, 0x0f):
                return {rd}
    return set()


def return_use(ins):
    """for each call (jal) in the listing: (callee, v0 read after it, f0 read after it) -- the registers the caller reads before writing
    them again, on the straight-line code after the delay slot (a branch ends the scan)"""
    out = []
    n = len(ins)
    for i, (pc, w, *_r) in enumerate(ins):
        if (w >> 26) != 3:
            continue
        tgt = ((pc + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2)
        v0 = f0 = False
        wr_v0 = wr_f0 = False
        j = i + 2
        while j < n and j < i + 60:
            pw = ins[j][1]
            if not wr_v0 and 2 in gpr_reads(pw):
                v0 = True
            if not wr_f0 and 0 in fpr_reads(pw):
                f0 = True
            d, _k = dest_reg(pw)
            if d == 2:
                wr_v0 = True
            pop = pw >> 26
            if pop == 0x31 and ((pw >> 16) & 31) == 0:
                wr_f0 = True  # lwc1 f0
            if pop == 0x11:
                rs_, fn_ = (pw >> 21) & 31, pw & 63
                if (rs_ == 4 and ((pw >> 11) & 31) == 0) or (rs_ == 0x10 and ((pw >> 6) & 31) == 0 and fn_ in (0, 1, 2, 3, 4, 5, 6, 7, 0x24, 0x0d)):
                    wr_f0 = True
            if pop in (1, 2, 3, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or (pop == 0 and (pw & 63) in (8, 9)):
                if j + 1 < n:  # the branch's delay slot runs too
                    dw = ins[j + 1][1]
                    if not wr_v0 and 2 in gpr_reads(dw):
                        v0 = True
                    if not wr_f0 and 0 in fpr_reads(dw):
                        f0 = True
                break
            if wr_v0 and wr_f0:
                break
            j += 1
        out.append((pc, tgt, v0, f0))
    return out


def jump_tables(ins, data_word):
    """the case targets of every `jr <reg>` (not ra) of a listing: the table base from the lui + addiu before it and its size from the
    sltiu bound; the words from data_word(address) (Veldin's overlay data)"""
    jr = []
    for i, (pc, w, mn, ops) in enumerate(ins):
        if (w >> 26) == 0 and (w & 0x3f) == 8 and ((w >> 21) & 31) != 31:
            n = None
            base = None
            hi = {}
            for pc2, w2, mn2, ops2 in ins[max(0, i - 16):i]:
                op = w2 >> 26
                if op == 0x0b and n is None:  # sltiu rt, rs, N
                    n = w2 & 0xffff
                if op == 0x0f:
                    hi[(w2 >> 16) & 31] = (w2 & 0xffff) << 16
                if op == 9 and ((w2 >> 21) & 31) in hi:
                    base = hi[(w2 >> 21) & 31] + sx16(w2 & 0xffff)
            if base is not None and n:
                for t in range(n):
                    v = data_word(base + 4 * t)
                    if v is not None and v not in jr:
                        jr.append(v)
    return jr
