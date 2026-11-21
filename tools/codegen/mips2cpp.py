#!/usr/bin/env python3
# usage: mips2cpp.py <listing.s> --name <fn> [--args a0,a1] [--ret v0|f0|void] [--callees glue.txt]
#                    [--jt <jr addr>=<target>,<target>...] [--const-wrap L1_ADDR] > out.cpp
#
# Transliterates one EE function (a the translator / Ghidra `Listing.java` listing, or a raw `Dis2.java` dump:
# lines `addr  word  mnemonic operands`, decoded from the word) into C++ on the 128-bit registers of
# src/game/ovl/ee_regs.h, instruction by instruction: the scalar / FPU / COP1 / load-store / branch / call
# subset (tools/codegen/asm2cpp.py is the VU0 macro-mode one). The result is the original's control flow and
# float / integer semantics by construction (PS2 FPU rules through fpu_*), proved by the EE-interpreter
# oracle like every port; it is reviewed and commented by hand, not "readable modern C++".
#
#  - branches become gotos to address labels, delay slots are placed as the R5900 runs them;
#  - `jal` calls go through a glue table (--callees): `0xADDR: <C++ statement>` using the registers
#    (a0.w(), fr[12], ...) and writing the results (set32(v0, ...), fr[0] = ...); an unknown callee is `#error`;
#  - `jr ra` returns (freeing the frame allocated for the function's `addiu sp,sp,-N`); an indirect `jr rX`
#    is a switch over the targets given with --jt;
#  - the stack frame is a real EE stack block (ee_stack_alloc), so a callee's own frames never overlap it;
#  - constant addresses (lui + addiu / ori / the loads' offsets, $gp = 0x166c00) are folded into
#    `L1_ADDR(0x...)` literals when they fall in the overlay's range (the function runs on Novalis's own
#    overlay only; a value that is not an address is unaffected: L1_ADDR is the identity).
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from asm2cpp import REG, sx16, parse  # noqa: E402

GP = 0x166C00
LVL = 'L' + os.environ.get('MKALL_LEVEL', '1')  # the level's own overlay (annotations `L2:0x...`)
if os.environ.get('MKALL_LEVEL') == 'front':  # the boot module's own code (`front:0x...`, tools/codegen/ovlist.py front)
    LVL = 'front'
OVL_LO, OVL_HI = 0x15EF00, int(os.environ.get('MIPS2CPP_OVL_HI', '0x2F0CD0'), 0)  # Aridia's code runs to 0x2ff930 (MIPS2CPP_OVL_HI=0x300000)


def off(o):
    return f'0x{o:x}' if o >= 0 else f'-0x{-o:x}'


def lit(v, wrap):
    v &= 0xFFFFFFFF
    if OVL_LO <= v < OVL_HI:
        if wrap == 'none':  # no relocation
            return f'0x{v:08x}u'
        w = wrap.split('/')
        w = w[0] if len(w) == 1 or v < 0x1EAA00 else w[1]  # "OV/OVF": data / code
        return f'{w}(0x{v:08x}u)'
    return f'0x{v:x}u'


class Ctx:
    def __init__(self, wrap):
        self.consts = {}
        self.wrap = wrap

    def reset(self, regs=None):
        if regs is None:
            self.consts = {}
        else:
            for r in regs:
                self.consts.pop(r, None)

    def ea(self, base, imm):
        """C++ expression of the address base + imm (folded when the base is a known constant)."""
        o = sx16(imm)
        if base == 'gp':
            return lit(GP + o, self.wrap)
        if base in self.consts:
            return lit(self.consts[base] + o, self.wrap)
        return f'{base}.w() + {off(o)}'


def tr(a, w, mn, ops, cx):
    """(statement, branch) for one instruction; branch = (kind, cond, target)."""
    op, rs, rt, rd, sa, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
    imm = w & 0xffff
    s, t, d = REG[rs], REG[rt], REG[rd]
    tgt = (a + 4 + (sx16(imm) << 2)) & 0xffffffff
    ea = cx.ea(s, imm)
    c = cx.consts

    def kill(*rr):
        for r in rr:
            c.pop(r, None)

    if w == 0:
        return '', None
    if op == 0:
        if fn == 0x00:
            kill(d)
            return f'set32({d}, {t}.w() << {sa});', None
        if fn == 0x02:
            kill(d)
            return f'set32({d}, {t}.w() >> {sa});', None
        if fn == 0x03:
            kill(d)
            return f'set32({d}, static_cast<uint32_t>(static_cast<int32_t>({t}.w()) >> {sa}));', None
        if fn == 0x04:
            kill(d)
            return f'set32({d}, {t}.w() << ({s}.w() & 31));', None
        if fn == 0x06:
            kill(d)
            return f'set32({d}, {t}.w() >> ({s}.w() & 31));', None
        if fn == 0x07:
            kill(d)
            return f'set32({d}, static_cast<uint32_t>(static_cast<int32_t>({t}.w()) >> ({s}.w() & 31)));', None
        if fn == 0x08:
            return '', ('jr', s, None)
        if fn == 0x09:
            kill('v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7', 't8', 't9')
            return '', ('jalr', s, None)
        if fn == 0x0a:  # (movz / movn write d on one path only: its known constant is gone either way)
            kill(d)
            return f'if ({t}.lo == 0) {d}.lo = {s}.lo;', None
        if fn == 0x0b:
            kill(d)
            return f'if ({t}.lo != 0) {d}.lo = {s}.lo;', None
        if fn == 0x0d:
            return '// break (a divide by zero trap: not taken)', None
        if fn == 0x0f:
            return '', None
        if fn == 0x10:
            kill(d)
            return f'{d}.lo = hi.lo;', None
        if fn == 0x11:
            return f'hi.lo = {s}.lo;', None
        if fn == 0x13:
            return f'lo.lo = {s}.lo;', None
        if fn == 0x12:
            kill(d)
            return f'{d}.lo = lo.lo;', None
        if fn == 0x18:
            kill(d)
            return f'{{ const int64_t p = static_cast<int64_t>(static_cast<int32_t>({s}.w())) * static_cast<int32_t>({t}.w()); set32(lo, static_cast<uint32_t>(p)); set32(hi, static_cast<uint32_t>(static_cast<uint64_t>(p) >> 32)); }}' + (f' {d}.lo = lo.lo;' if rd else ''), None
        if fn == 0x19:
            kill(d)
            return f'{{ const uint64_t p = static_cast<uint64_t>({s}.w()) * {t}.w(); set32(lo, static_cast<uint32_t>(p)); set32(hi, static_cast<uint32_t>(p >> 32)); }}' + (f' {d}.lo = lo.lo;' if rd else ''), None
        if fn == 0x1a:
            return f'{{ const int32_t x = static_cast<int32_t>({s}.w()), y = static_cast<int32_t>({t}.w()); set32(lo, static_cast<uint32_t>(ee_div(x, y))); set32(hi, static_cast<uint32_t>(ee_rem(x, y))); }}', None
        if fn == 0x1b:
            return f'{{ const uint32_t x = {s}.w(), y = {t}.w(); set32(lo, y == 0 ? 0xffffffffu : x / y); set32(hi, y == 0 ? x : x % y); }}', None
        if fn in (0x20, 0x21):
            kill(d)
            return f'set32({d}, {s}.w() + {t}.w());', None
        if fn in (0x22, 0x23):
            kill(d)
            return f'set32({d}, {s}.w() - {t}.w());', None
        if fn == 0x24:
            kill(d)
            return f'{d}.lo = {s}.lo & {t}.lo;', None
        if fn == 0x25:
            if rt == 0 and rs in c:
                c[d] = c[s]
            else:
                kill(d)
            return f'{d}.lo = {s}.lo | {t}.lo;', None
        if fn == 0x26:
            kill(d)
            return f'{d}.lo = {s}.lo ^ {t}.lo;', None
        if fn == 0x27:
            kill(d)
            return f'{d}.lo = ~({s}.lo | {t}.lo);', None
        if fn == 0x2a:
            kill(d)
            return f'{d}.lo = static_cast<int64_t>({s}.lo) < static_cast<int64_t>({t}.lo) ? 1 : 0;', None
        if fn == 0x2b:
            kill(d)
            return f'{d}.lo = {s}.lo < {t}.lo ? 1 : 0;', None
        if fn == 0x2d:
            if rt == 0 and rs in c:
                c[d] = c[s]
            else:
                kill(d)
            return f'{d}.lo = {s}.lo + {t}.lo;', None
        if fn == 0x2f:
            kill(d)
            return f'{d}.lo = {s}.lo - {t}.lo;', None
        if fn == 0x3a:
            kill(d)
            return f'{d}.lo = {t}.lo >> {sa};', None
        if fn == 0x38:
            kill(d)
            return f'{d}.lo = {t}.lo << {sa};', None
        if fn == 0x3b:
            kill(d)
            return f'{d}.lo = static_cast<uint64_t>(static_cast<int64_t>({t}.lo) >> {sa});', None
        if fn == 0x3c:
            kill(d)
            return f'{d}.lo = {t}.lo << {32 + sa};', None
        if fn == 0x3e:
            kill(d)
            return f'{d}.lo = {t}.lo >> {32 + sa};', None
        if fn == 0x3f:
            kill(d)
            return f'{d}.lo = static_cast<uint64_t>(static_cast<int64_t>({t}.lo) >> {32 + sa});', None
    if op == 1:
        cc = {0: f'neg({s})', 1: f'!neg({s})', 2: f'neg({s})', 3: f'!neg({s})'}.get(rt)
        if cc:
            return '', ('likely' if rt in (2, 3) else 'b', cc, tgt)
        if rt in (0x10, 0x11):  # BLTZAL / BGEZAL: a conditional call (ra is written whether or not it is taken)
            kill('v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7', 't8', 't9')
            return '', ('bal', f'neg({s})' if rt == 0x10 else f'!neg({s})', tgt)
    if op == 3:
        kill('v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7', 't8', 't9')
        return '', ('jal', None, (w & 0x3ffffff) << 2)
    if op == 2:
        return '', ('j', 'true', ((a + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2))
    if op in (4, 5, 0x14, 0x15):
        if op in (4, 0x14) and rs == 0 and rt == 0:
            cc = 'true'
        elif op in (5, 0x15) and rs == rt:
            cc = 'false'
        else:
            cc = f'{s}.lo {"==" if op in (4, 0x14) else "!="} {t}.lo'
        return '', ('likely' if op >= 0x14 else 'b', cc, tgt)
    if op in (6, 7, 0x16, 0x17):
        return '', ('likely' if op >= 0x16 else 'b', f'static_cast<int64_t>({s}.lo) {"<=" if op in (6, 0x16) else ">"} 0', tgt)
    if op in (8, 9):
        if s == 'zero':
            c[t] = sx16(imm) & 0xffffffff
        elif s == 'gp':
            c[t] = (GP + sx16(imm)) & 0xffffffff
        elif s in c:
            c[t] = (c[s] + sx16(imm)) & 0xffffffff
        else:
            kill(t)
        if t in c and (s in ('zero', 'gp') or s in c) and op == 9 and s != 'zero':
            return f'set32({t}, {lit(c[t], cx.wrap)});', None
        return f'set32({t}, {s}.w() + {off(sx16(imm))});', None
    if op == 0x0a:
        kill(t)
        return f'{t}.lo = static_cast<int64_t>({s}.lo) < {sx16(imm)} ? 1 : 0;', None
    if op == 0x0b:
        kill(t)
        return f'{t}.lo = {s}.lo < static_cast<uint64_t>(static_cast<int64_t>({sx16(imm)})) ? 1 : 0;', None
    if op == 0x0c:
        kill(t)
        return f'{t}.lo = {s}.lo & 0x{imm:x};', None
    if op == 0x0d:
        if s in c:
            c[t] = c[s] | imm
            v = c[t] & 0xFFFFFFFF  # (lui + ori builds a value, not an address: a colour, a mask; never translated)
            return f'set32({t}, {"NOT_ADDR(0x%xu)" % v if OVL_LO <= v < OVL_HI else "0x%xu" % v});', None
        kill(t)
        return f'{t}.lo = {s}.lo | 0x{imm:x};', None
    if op == 0x0e:
        kill(t)
        return f'{t}.lo = {s}.lo ^ 0x{imm:x};', None
    if op == 0x0f:
        c[t] = (imm << 16) & 0xffffffff
        return f'set32({t}, {lit(imm << 16, cx.wrap)});', None
    if op == 0x10 and rs == 0x08:  # BC0F / BC0T / BC0FL / BC0TL: a poll of CPCOND0 (the DMA channels the D_PCR names done).
        # The host's DMA completes when kicked, so CPCOND0 is 1 (as the EE interpreter assumes): BC0F never jumps, BC0T always.
        return '', ('likely' if rt & 2 else 'b', 'true' if rt & 1 else 'false', tgt)
    if op == 0x11:  # COP1
        fs_, ft_, fd_ = rd, rt, sa
        if rs == 0x00:
            kill(t)
            return f'set32({t}, f2u(fr[{fs_}]));', None
        if rs == 0x04:
            return f'fr[{fs_}] = u2f({t}.w());', None
        if rs == 0x14 and fn == 0x20:
            return f'fr[{fd_}] = static_cast<float>(static_cast<int32_t>(f2u(fr[{fs_}])));', None
        if rs == 0x10:
            ops2 = {0x00: 'fpu_add', 0x01: 'fpu_sub', 0x02: 'fpu_mul', 0x03: 'fpu_div'}
            if fn in ops2:
                return f'fr[{fd_}] = {ops2[fn]}(fr[{fs_}], fr[{ft_}]);', None
            if fn == 0x04:
                return f'fr[{fd_}] = fpu_sqrt(fr[{fs_}]);', None
            if fn == 0x18:
                return f'acc = fpu_add(fr[{fs_}], fr[{ft_}]);', None
            if fn == 0x19:
                return f'acc = fpu_sub(fr[{fs_}], fr[{ft_}]);', None
            if fn == 0x1a:
                return f'acc = fpu_mul(fr[{fs_}], fr[{ft_}]);', None
            if fn == 0x1c:
                return f'fr[{fd_}] = fpu_add(acc, fpu_mul(fr[{fs_}], fr[{ft_}]));', None
            if fn == 0x1d:
                return f'fr[{fd_}] = fpu_sub(acc, fpu_mul(fr[{fs_}], fr[{ft_}]));', None
            if fn == 0x1e:
                return f'acc = fpu_add(acc, fpu_mul(fr[{fs_}], fr[{ft_}]));', None
            if fn == 0x1f:
                return f'acc = fpu_sub(acc, fpu_mul(fr[{fs_}], fr[{ft_}]));', None
            if fn == 0x05:
                return f'fr[{fd_}] = std::fabs(fr[{fs_}]);', None
            if fn == 0x06:
                return f'fr[{fd_}] = fr[{fs_}];', None
            if fn == 0x07:
                return f'fr[{fd_}] = -fr[{fs_}];', None
            if fn == 0x24:
                return f'fr[{fd_}] = cvt_w(fr[{fs_}]);', None
            if fn == 0x34:
                return f'fcc = fr[{fs_}] < fr[{ft_}];', None
            if fn == 0x36:
                return f'fcc = fr[{fs_}] <= fr[{ft_}];', None
            if fn == 0x32:
                return f'fcc = fr[{fs_}] == fr[{ft_}];', None
        if rs == 0x08:
            cc = {0: '!fcc', 1: 'fcc', 2: '!fcc', 3: 'fcc'}[rt & 3]
            return '', ('likely' if rt & 2 else 'b', cc, tgt)
    if op == 0x1c:  # MMI
        if fn == 0x29 and sa == 0x12:  # POR (clear rd = por rd, zero, zero)
            kill(d)
            return f'{{ {d}.lo = {s}.lo | {t}.lo; {d}.hi = {s}.hi | {t}.hi; }}', None
        if fn == 0x00:
            kill(d)
            return f'{{ int64_t acc = static_cast<int64_t>((static_cast<uint64_t>(hi.w()) << 32) | lo.w()) + static_cast<int64_t>(static_cast<int32_t>({s}.w())) * static_cast<int32_t>({t}.w()); set32(lo, static_cast<uint32_t>(acc)); set32(hi, static_cast<uint32_t>(static_cast<uint64_t>(acc) >> 32)); {d}.lo = lo.lo; }}', None
        if fn == 0x18:
            kill(d)
            return f'mult3({d}, {s}, {t}, lo, hi);', None
    if op == 0x1a:
        kill(t)
        return f'ldl_({t}, {ea});', None
    if op == 0x1b:
        kill(t)
        return f'ldr_({t}, {ea});', None
    if op == 0x1e:
        kill(t)
        return f'{t} = lq({ea});', None
    if op == 0x1f:
        return f'sq({ea}, {t});', None
    if op == 0x20:
        kill(t)
        return f'set32({t}, static_cast<uint32_t>(static_cast<int8_t>(lbu_({ea}))));', None
    if op == 0x21:
        kill(t)
        return f'set32({t}, static_cast<uint32_t>(static_cast<int16_t>(lhu_({ea}))));', None
    if op == 0x23:
        kill(t)
        return f'set32({t}, lw_({ea}));', None
    if op == 0x24:
        kill(t)
        return f'{t}.lo = lbu_({ea});', None
    if op == 0x25:
        kill(t)
        return f'{t}.lo = lhu_({ea});', None
    if op == 0x27:
        kill(t)
        return f'{t}.lo = lw_({ea});', None
    if op == 0x28:
        return f'sb_({ea}, {t}.w());', None
    if op == 0x29:
        return f'sh_({ea}, {t}.w());', None
    if op == 0x2b:
        return f'sw_({ea}, {t}.w());', None
    if op == 0x2c:
        return f'sdl({ea}, {t});', None
    if op == 0x2d:
        return f'sdr({ea}, {t});', None
    if op == 0x2f:
        return '', None
    if op == 0x31:
        return f'fr[{rt}] = u2f(lw_({ea}));', None
    if op == 0x36:  # lqc2 (the address folds $gp / constants like every other access)
        return f'lqc2_({rt}, {ea});', None
    if op == 0x3e:  # sqc2
        return f'sqc2_({ea}, {rt});', None
    if op == 0x37:
        kill(t)
        return f'{t}.lo = ld_({ea});', None
    if op == 0x39:
        return f'sw_({ea}, f2u(fr[{rt}]));', None
    if op == 0x3f:
        return f'sd_({ea}, {t}.lo);', None
    import asm2cpp
    st, br = asm2cpp.tr(a, w, mn, ops)
    # the GPR the instruction asm2cpp handles writes loses its known constant (lq / ldl / lwl / MMI / qmfc2 ...)
    if op in (0x1a, 0x1b, 0x1e, 0x22, 0x26, 0x10) or (op == 0x12 and rs in (1, 2)):
        kill(t)
    elif op in (0x00, 0x1c):
        kill(d)
    return st, br


def fpu_regs(w):
    """(the FPU registers the instruction word reads, the one it writes or None): from the word, not the listing's text."""
    op, rs, ft, fs, fd, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
    if op == 0x31:  # lwc1
        return [], ft
    if op == 0x39:  # swc1
        return [ft], None
    if op != 0x11:
        return [], None
    if rs == 0:  # mfc1
        return [fs], None
    if rs == 4:  # mtc1
        return [], fs
    if rs == 0x14 and fn == 0x20:  # cvt.s.w
        return [fs], fd
    if rs != 0x10:
        return [], None
    if fn in (0x00, 0x01, 0x02, 0x03, 0x16, 0x1c, 0x1d, 0x28, 0x29):  # add sub mul div rsqrt madd msub max min
        return [fs, ft], fd
    if fn == 0x04:  # sqrt
        return [ft], fd
    if fn in (0x05, 0x06, 0x07, 0x24):  # abs mov neg cvt.w.s
        return [fs], fd
    return [fs, ft], None  # adda suba mula madda msuba, the compares


def reads_f16_f19(ins):
    """Whether the function reads one of f16..f19 before writing it (in address order): a float argument there. Decided on the
    instruction words (the listing's text is only a comment; the text-based test read `clear f16` / `cvt.s.w` wrongly)."""
    written = set()
    for _a, w, _mn, _ops in ins:
        srcs, dst = fpu_regs(w)
        for r in srcs:
            if 16 <= r <= 19 and r not in written:
                return True
        if dst is not None:
            written.add(dst)
    return False


def load_glue(path):
    g = {}
    if path:
        for line in open(path):
            line = line.split('//')[0].strip()
            if not line:
                continue
            m = re.match(r'(0x[0-9a-fA-F]+)\s*:\s*(.*)$', line)
            if m:
                g[int(m.group(1), 16)] = m.group(2)
    return g


def load_hooks(path, lvl):
    """ The statements inserted at instruction addresses of a level's own code: the hook points of the optional
    enhancements (src/game/enhance/advanced.h) in the levels' transliterated copies of the hero functions, at the points the
    hand ports call them. The file (tools/codegen/hooks.glue, MIPS2CPP_HOOKS) has lines
        L<n> 0xADDR: <C++ statement>   the statement emitted before the instruction at ADDR (after its label), on level n
        L<n> 0xADDR label              ADDR made a label (a branch target) for a statement's `goto L_<addr>`
    ({addr: [statement]}, {label addr}) of the level `lvl` ('L5')."""
    hooks, labels = {}, set()
    if not path or not os.path.exists(path):
        return hooks, labels
    for line in open(path):
        if not line.strip() or line.lstrip().startswith('#') or re.match(r'^L\d+\s+PAL:', line):
            continue  # (`PAL:` points: the PAL build's code, moved onto a stand-in by mkall.py)
        m = re.match(r'^(L\d+)\s+(0x[0-9a-fA-F]+)\s*(?::\s*(.*?)|\s+(label))\s*$', line.rstrip('\n'))
        if not m:
            print('mips2cpp: bad hook line: ' + line.rstrip(), file=sys.stderr)
            continue
        if m.group(1) != lvl:
            continue
        if m.group(4):
            labels.add(int(m.group(2), 16))
        else:
            hooks.setdefault(int(m.group(2), 16), []).append(m.group(3))
    return hooks, labels


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('listing')
    ap.add_argument('--name', default='fn')
    ap.add_argument('--args', default='')
    ap.add_argument('--ret', default='void')
    ap.add_argument('--callees', default=None)
    ap.add_argument('--jt', action='append', default=[])
    ap.add_argument('--const-wrap', default='L1_ADDR', help='L1_ADDR, OV/OVF (data / code) for a port at Veldin addresses, FR for the boot module\'s own')
    ap.add_argument('--veldin', action='store_true', help='a Veldin-address port: orig comment `0x...`, the jump tables switch on ovl_code_to_veldin')
    ap.add_argument('--ptr-args', default='')
    ap.add_argument('--local', action='append', default=[], help='start:end (hex): a routine reached by bal that shares the caller\'s registers (hand-written asm with its own register contract), appended to the listing: its jr ra returns to the bal sites')
    ap.add_argument('--uniform', action='store_true', help='the uniform helper ABI: (a0..a3, t0..t3, f12..f15) -> TrRet {v0, f0}')
    ap.add_argument('--emit-glue', default=None, help='append the glue line calling this function (uniform ABI) to this file')
    ap.add_argument('--wide-gpr', default='', help='uniform ABI: the argument registers (comma separated) passed whole, 64 bits (uint64_t: the '
                    'function uses their upper word, e.g. sd of an argument; task 0366)')
    o = ap.parse_args()
    wide_gpr = [x for x in o.wide_gpr.split(',') if x]
    glue = load_glue(o.callees)
    jts = {}
    for j in o.jt:
        k, v = j.split('=')
        jts[int(k, 16)] = [int(x, 16) for x in v.split(',')]
    ins = parse(o.listing)
    locs = [tuple(int(x, 16) for x in l.split(':')) for l in o.local]
    in_local = lambda a: any(lo <= a <= hi for lo, hi in locs)
    cx = Ctx(o.const_wrap)
    targets = set()
    for tl in jts.values():
        targets.update(tl)
    # the enhancements' hook points in this function (none on Veldin's own code: its hand ports call them)
    hooks, hook_labels = load_hooks(None if o.veldin else os.environ.get('MIPS2CPP_HOOKS'), LVL)
    fn_lo, fn_hi = ins[0][0], ins[-1][0]
    hooks = {a: s for a, s in hooks.items() if fn_lo <= a <= fn_hi}
    targets.update(a for a in hook_labels if fn_lo <= a <= fn_hi)
    hooks_done = set()
    # first pass: branch targets
    local_rets = {}  # a local routine's start -> the addresses after its bal sites
    tmp = Ctx(o.const_wrap)
    for a, w, mn, ops in ins:
        _, br = tr(a, w, mn, ops, tmp)
        if br and br[0] in ('b', 'likely', 'j') and br[2] is not None:
            targets.add(br[2])
        if br and br[0] == 'bal' and any(lo == br[2] for lo, _ in locs):
            targets.add(br[2])
            targets.add(a + 8)
            local_rets.setdefault(br[2], []).append(a + 8)
    frame = 0
    for _a, first, _m, _o in ins[:16]:  # the prologue's addiu sp,sp,-N (not always the first instruction: the scheduler moves loads before it)
        if (first >> 26) == 9 and ((first >> 21) & 31) == 29 and ((first >> 16) & 31) == 29 and sx16(first & 0xffff) < 0:
            frame = -sx16(first & 0xffff)
            break
    args = [x for x in o.args.split(',') if x]
    if o.uniform:
        fmax = int(os.environ.get('UNIFORM_FMAX', '15'))  # (Aridia and later: f12..f19 for every helper)
        args = ['a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3'] + ['f%d' % k for k in range(12, fmax + 1)]
        wide = fmax < 19 and reads_f16_f19(ins)
        if wide:  # (a function taking floats in f16..f19 too: the EE ABI's float arguments are f12..f19)
            args += ['f%d' % k for k in range(fmax + 1, 20)]
        o.ret = 'uniform'
        if o.emit_glue:
            with open(o.emit_glue, 'a') as gf:
                gf.write('0x%08x: { const TrRet r = %s(a0.w(), a1.w(), a2.w(), a3.w(), t0.w(), t1.w(), t2.w(), t3.w(), %s); set32(v0, r.v0); fr[0] = r.f0; }\n'
                         % (ins[0][0], o.name, ', '.join('fr[%d]' % int(x[1:]) for x in args if x.startswith('f'))))
    out = []
    ptype = lambda n: 'float' if n.startswith('f') else ('uint64_t' if n in wide_gpr else 'uint32_t')
    out.append(f'// orig: {"" if o.veldin else LVL + ":"}0x{ins[0][0]:08x}')
    out.append(f'{"TrRet" if o.ret == "uniform" else ("float" if o.ret == "f0" else ("uint32_t" if o.ret == "v0" else "void"))} {o.name}(' +
               ', '.join(f'{ptype(x)} {x}_in' for x in args) + ') {')
    out.append('    Q128 zero, at, v0, v1, a0, a1, a2, a3, t0, t1, t2, t3, t4, t5, t6, t7, s0, s1, s2, s3, s4, s5, s6, s7, t8, t9, k0, k1, gp, sp, s8, ra, lo, hi;')
    out.append('    float fr[32] = {};')
    out.append('    float acc = 0.0f;')
    out.append('    bool fcc = false;')
    out.append('    (void)at; (void)v1; (void)a1; (void)a2; (void)a3; (void)t0; (void)t1; (void)t2; (void)t3; (void)t4; (void)t5; (void)t6; (void)t7; (void)s0; (void)s1; (void)s2; (void)s3; (void)s4; (void)s5; (void)s6; (void)s7; (void)t8; (void)t9; (void)k0; (void)k1; (void)gp; (void)s8; (void)ra; (void)lo; (void)hi; (void)fcc; (void)zero; (void)acc;')
    for x in args:
        if x.startswith('f'):
            out.append(f'    fr[{x[1:]}] = {x}_in;')
        elif x in wide_gpr:
            out.append(f'    {x}.lo = {x}_in;  // (the whole register: its upper word is used)')
        else:
            out.append(f'    set32({x}, {x}_in);')
    # a dynamic allocation (`subu sp,sp,rX`, alloca / a variable-length array): the callees' own frames (ee_stack_alloc of the ports) must be
    # allocated below it, so the host's stack pointer follows the register (class 794's helper copies a hit list into such an array)
    is_alloca = lambda w: (w >> 26) == 0 and (w & 0x3f) == 0x23 and ((w >> 11) & 31) == 29 and ((w >> 21) & 31) == 29
    has_alloca = bool(frame) and any(is_alloca(w) for _a, w, _m, _o in ins)
    if frame:
        out.append(f'    set32(sp, ee_stack_alloc({frame}) + {frame});  // the frame of the prologue\'s addiu sp,sp,-{frame}')
        if has_alloca:
            out.append('    const uint32_t frame_sp_ = ee_stack_ptr();  // (the host stack pointer at the frame: restored before the frame is freed)')
    retexpr = {'v0': 'v0.w()', 'f0': 'fr[0]', 'void': '', 'uniform': 'TrRet{v0.w(), fr[0]}'}[o.ret]

    def ret_stmt():
        r = (('ee_stack_ptr() = frame_sp_; ' if has_alloca else '') + f'ee_stack_free({frame}); ' if frame else '')
        return r + (f'return {retexpr};' if retexpr else 'return;')

    def conv(st):
        return st

    items = []
    saved = None  # the constants before a branch-likely's delay slot (it runs only when the branch is taken)
    for a, w, mn, ops in ins:
        if a in targets:
            cx.reset()
        st, br = tr(a, w, mn, ops, cx)
        if saved is not None:
            cx.consts = saved
            saved = None
        if br is not None and br[0] == 'likely':
            saved = dict(cx.consts)
        if has_alloca and is_alloca(w):
            st = (st + ' ee_stack_ptr() = sp.w();').strip()
        items.append((a, w, mn, ops, st, br))
    if o.veldin and not os.environ.get('MIPS2CPP_NO_HI_FLOW'):  # (the switch: to list what the pass changes in a file generated before it)
        # the overlay addresses whose `lui` half reaches the use on EVERY path (tools/codegen/hi_flow.py, a data-flow over the
        # control flow): the text passes below only follow straight-line code and miss a page copied by
        # a `move` into a register rewritten on other paths (the vendor's draw 0x2a02b0: `move s2,v0` after `lui v0,0x1d`,
        # its 0x1ca4c0 block read at Veldin's address on every other level)
        import hi_flow
        flow = [(a, w, mn, ops, st, (('call',) + tuple(br[1:])) if br is not None and br[0] in ('jal', 'jalr', 'bal') else br)
                for a, w, mn, ops, st, br in items]
        jr_all = sorted({t for tl in jts.values() for t in tl})
        new, errs = hi_flow.rewrite(flow, flow, jr_all)
        items = [(a, w, mn, ops, n[4], br) for (a, w, mn, ops, st, br), n in zip(items, new)]
        for e in errs:
            print(f'mips2cpp {o.name}: {e} (hi_flow)', file=sys.stderr)
    i = 0
    emitted_labels = set()
    while i < len(items):
        a, w, mn, ops, st, br = items[i]
        if a in targets and a not in emitted_labels:
            emitted_labels.add(a)
            out.append(f'L_{a:06x}:')
        for hs in hooks.get(a, []):  # an enhancement's hook: before the instruction, on every path that reaches it
            for lab in re.findall(r'\bgoto L_([0-9a-f]{6})\b', hs):
                if int(lab, 16) not in targets:
                    out.append(f'#error hook at {a:08x}: L_{lab} is not a label (add `{LVL} 0x{int(lab, 16):08x} label`)')
            out.append(f'    {hs}')
            hooks_done.add(a)
        if br is None:
            if st:
                out.append(f'    {st}  // {mn} {ops}'.rstrip())
            i += 1
            continue
        kind, cond, tgt = br
        ds = items[i + 1] if i + 1 < len(items) else None
        dst = ds[4] if ds else ''
        dcomment = f'  // (delay) {ds[2]} {ds[3]}' if ds else ''
        ds_target = bool(ds and ds[0] in targets)
        if kind == 'jr' and cond == 'ra' and in_local(a):  # the local routine returns to the bal site that called it
            rl = [r for lo, hi in locs if lo <= a <= hi for r in local_rets.get(lo, [])]
            if dst:
                out.append(f'    {dst}{dcomment}')
            out.append('    switch (ra.w()) {  // jr ra (a routine reached by bal: back to its call sites)')
            for r in dict.fromkeys(rl):
                out.append(f'    case 0x{r:x}u: goto L_{r:06x};')
            out.append('    default: ' + ret_stmt() + '  // (not a bal site)')
            out.append('    }')
        elif kind == 'jr':
            if cond == 'ra':
                if dst:
                    out.append(f'    {dst}{dcomment}')
                out.append(f'    {ret_stmt()}  // jr ra')
            else:
                tl = jts.get(a)
                if tl is None:
                    out.append(f'#error jr {cond} at {a:08x}: give --jt {a:x}=<targets>')
                else:
                    if dst:
                        out.append(f'    {dst}{dcomment}')
                    out.append(f'    switch ({"ovl_code_to_veldin(" + cond + ".w())" if o.veldin else cond + ".w()"}) {{  // jr {cond}')
                    for tv in dict.fromkeys(tl):
                        out.append(f'    case 0x{tv:x}u: goto L_{tv:06x};')
                    out.append('    default: return' + ({'void': '', 'uniform': ' TrRet{0, 0.0f}'}.get(o.ret, ' 0')) + ';  // (a target outside the table)')
                    out.append('    }')
        elif kind == 'jalr':  # an indirect call: a class / moby hook through the level's dispatcher (a0 = the moby)
            if dst:
                out.append(f'    {dst}{dcomment}')
            out.append(f'    set32(ra, NOT_ADDR(0x{a + 8:x}u));')
            out.append(f'    ovl_call_moby_fn({cond}.w(), a0.w());  // jalr {cond}')
        elif kind == 'bal' and any(lo == tgt for lo, _ in locs):  # a call to a routine that shares the registers: a goto, it returns to a + 8
            out.append(f'    {{  // {mn} {ops}')
            out.append(f'        const bool c = {cond};')
            if dst:
                out.append(f'        {dst}{dcomment}')
            out.append(f'        set32(ra, NOT_ADDR(0x{a + 8:x}u));')
            out.append(f'        if (c) goto L_{tgt:06x};  // call 0x{tgt:08x}')
            out.append('    }')
        elif kind == 'bal':  # bltzal / bgezal: the delay slot, ra, then the call when the condition (read before the delay slot) holds
            g = glue.get(tgt)
            out.append(f'    {{  // {mn} {ops}')
            out.append(f'        const bool c = {cond};')
            if dst:
                out.append(f'        {dst}{dcomment}')
            out.append(f'        set32(ra, NOT_ADDR(0x{a + 8:x}u));')
            if g is None:
                out.append(f'#error bal 0x{tgt:08x}: no glue for the callee')
            else:
                out.append(f'        if (c) {{ {g} }}  // call 0x{tgt:08x}')
            out.append('    }')
        elif kind == 'jal':
            if dst:
                out.append(f'    {dst}{dcomment}')
            g = glue.get(tgt)
            out.append(f'    set32(ra, NOT_ADDR(0x{a + 8:x}u));')
            if g is None:
                out.append(f'#error jal 0x{tgt:08x}: no glue for the callee')
            else:
                out.append(f'    {g}  // jal 0x{tgt:08x}')
        elif kind == 'j' or cond == 'true':
            if dst:
                out.append(f'    {dst}{dcomment}')
            out.append(f'    goto L_{tgt:06x};  // {mn} {ops}')
        elif cond == 'false':
            if dst and kind != 'likely':  # (a branch-likely never taken nullifies its delay slot)
                out.append(f'    {dst}{dcomment}')
        elif kind == 'likely':
            out.append(f'    if ({cond}) {{  // {mn} {ops}')
            if dst:
                out.append(f'        {dst}{dcomment}')
            out.append(f'        goto L_{tgt:06x};')
            out.append('    }')
        else:
            if dst:
                out.append(f'    {{  // {mn} {ops}')
                out.append(f'        const bool c = {cond};')
                out.append(f'        {dst}{dcomment}')
                out.append(f'        if (c) goto L_{tgt:06x};')
                out.append('    }')
            else:
                out.append(f'    if ({cond}) goto L_{tgt:06x};  // {mn} {ops}')
        if ds_target:
            nxt = ds[0] + 4
            out.append(f'    goto L_{nxt:06x};')
            out.append(f'L_{ds[0]:06x}:')
            emitted_labels.add(ds[0])
            if dst:
                out.append(f'    {dst}  // {ds[2]} {ds[3]}')
            out.append(f'L_{nxt:06x}:')
            emitted_labels.add(nxt)
        i += 2
    for a in sorted(set(hooks) - hooks_done):  # (a delay slot is emitted with its branch: no hook there)
        out.append(f'#error hook at {a:08x}: not the start of an instruction outside a delay slot')
    out.append('}')
    if o.veldin:
        # a `lui` page kept across a label is re-added by an unfolded addiu/ori: the page must be raw and the sum wrapped
        import re as _re
        page = {}
        # a branch-likely's block (`if (c) {` its delay slot, `goto` `}`) runs only when the branch is taken: the code after it
        # sees the pages from before it (the Vendor's start 0x29a884: `bnel v1,s2` with `lui v0,0x17` in its slot, then
        # `lw a1,0xd0(v0)` on the fall-through path read 0x1700d0 instead of the centre item's id, issue #27)
        blocks = []
        for i, ln in enumerate(out):
            if _re.match(r'\s*if \(.*\) \{  //', ln):
                blocks.append(dict(page))
                continue
            if _re.match(r'\s*\{  //', ln):
                blocks.append(None)
                continue
            if _re.match(r'\s*\}\s*$', ln) and blocks:
                b = blocks.pop()
                if b is not None:
                    page = b
                continue
            m = _re.match(r'(\s*)set32\((\w+), (?:OVF?|L1_ADDR)\(0x([0-9a-f]{4})0000u\)\);(.*lui )', ln)
            if m:
                page[m.group(2)] = int(m.group(3), 16) << 16
                out[i] = f'{m.group(1)}set32({m.group(2)}, NOT_ADDR(0x{page[m.group(2)]:x}u));{m.group(4)}' + ln[m.end():]
                continue
            m = _re.match(r'(\s*)set32\((\w+), (\w+)\.w\(\) \+ (-?)(0x[0-9a-f]+)\);(.*)', ln)  # addiu rd, rs, k with rs a page (rd may be another register)
            if m and m.group(3) in page and m.group(3) != 'sp':
                v = (page[m.group(3)] + int(m.group(5), 16) * (-1 if m.group(4) else 1)) & 0xFFFFFFFF
                out[i] = f'{m.group(1)}set32({m.group(2)}, {lit(v, o.const_wrap)});{m.group(6)}'
                page.pop(m.group(2), None)
                continue
            m = _re.match(r'\s*(\w+)\.lo = (\w+)\.lo (?:\+|\|) zero\.lo;', ln)  # move rd, rs: the page is copied
            if m and m.group(2) in page:
                page[m.group(1)] = page[m.group(2)]
                continue
            # a load / store through a page register (`lw v1,0x20a0(a1)` after a label:, particle type 8's table
            # 0x1b20a0 read through a `lui a1,0x1b` before a beql's target): the translated address
            code, sep, cmt = ln.partition('//')
            sub = _re.sub(r'\b((?:l[bhwdq]u?_?|lwc1_?|s[bhwdq]_?|lqc2|sqc2)\()(\w+)\.w\(\) \+ (-?)(0x[0-9a-f]+)',
                          lambda mm: mm.group(1) + lit((page[mm.group(2)] + int(mm.group(4), 16) * (-1 if mm.group(3) else 1)) & 0xFFFFFFFF, o.const_wrap)
                          if mm.group(2) in page and mm.group(2) != 'sp' else mm.group(0), code)
            if sub != code:
                out[i] = ln = sub + sep + cmt
            w = _re.match(r'\s*(?:set32\((\w+),|(\w+)\.lo =|(\w+) = )', ln)
            if w:
                page.pop(w.group(1) or w.group(2) or w.group(3), None)  # the register is rewritten: its page is gone
        # a register whose every write is the same `lui` of an overlay page (a base kept for the whole function, used after labels
        # where the straight-line folding above lost it) holds that page at every use: its `r.w() + k` uses are the translated address
        writes = {}  # register -> [the page a lui puts in it | ('move', source register) | None (any other write)]
        for ln in out:
            code = ln.partition('//')[0]
            if _re.search(r'= lq\(sp\.w\(\) \+ ', code):  # (the epilogue's restore of a callee-saved register: not a write of the function's own)
                continue
            mv = _re.match(r'\s*(\w+)\.lo = (\w+)\.lo (?:\+|\|) zero\.lo;', code)  # move rd, rs
            for r in _re.findall(r'set32\((\w+),', code) + _re.findall(r'\b(\w+)\.(?:lo|hi) =[^=]', code) + _re.findall(r'(?<![\w.])(\w+) = ', code) + \
                    _re.findall(r'(?:ldl_|ldr_|mult3)\((\w+),', code):
                mm = _re.match(r'\s*set32\(\w+, NOT_ADDR\(0x([0-9a-f]+)u\)\);\s*// lui ', ln)
                if mm and mm.group(0).count(f'set32({r},'):
                    writes.setdefault(r, []).append(int(mm.group(1), 16))
                elif mv and mv.group(1) == r:
                    writes.setdefault(r, []).append(('move', mv.group(2)))
                else:
                    writes.setdefault(r, []).append(None)
        const_page = {}
        for _ in range(6):  # a move from a register that holds the page holds it too
            for r, ws in writes.items():
                vals = {const_page.get(w[1]) if isinstance(w, tuple) else w for w in ws}
                if len(vals) == 1 and None not in vals:
                    pg = vals.pop()
                    if OVL_LO <= pg + 0xffff and pg < OVL_HI:
                        const_page[r] = pg
        for i, ln in enumerate(out):
            if '// lui ' in ln and 'NOT_ADDR' in ln:
                continue
            for r, pg in const_page.items():
                out[i] = ln = _re.sub(rf'(?<![\w.]){r}\.w\(\) \+ (-?)(0x[0-9a-f]+)', lambda m: lit((pg + int(m.group(2), 16) * (-1 if m.group(1) else 1)) & 0xFFFFFFFF, o.const_wrap), ln)
    print('\n'.join(out))


if __name__ == '__main__':
    main()
