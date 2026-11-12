#!/usr/bin/env python3
# usage: asm2cpp.py <listing.s> [--name fn] > out.cpp
#
# Transliterates a hand-written EE routine (a the translator listing) into C++ statements, one
# per instruction, on 128-bit register variables: the starting point of a port of code whose
# instruction order matters (software pipelines handing data to a running VU0 program). The
# output uses the helpers of src/game/ovl/ee_regs.h (loads / stores through EE memory with the
# hardware registers the routine touches, MMI ops, COP2 transfers and the VU0). Branches become
# gotos to address labels, delay slots are placed as the R5900 runs them (the condition read
# before the slot; likely branches run the slot only when taken). The result is reviewed and
# commented by hand; instructions it cannot decode are emitted as `#error` lines.
import re
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

REG = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
       's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 's8', 'ra']


def sx16(x):
    return x - 0x10000 if x & 0x8000 else x


LANE = 'xyzw'


def cop2_macro(w):
    """C++ for a VU0 macro-mode (COP2, bit 25 set) instruction on vu0_regs.h, or None."""
    dest, ft, fs, fd, fn = (w >> 21) & 15, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
    m = ((dest >> 3) & 1) | (((dest >> 2) & 1) << 1) | (((dest >> 1) & 1) << 2) | ((dest & 1) << 3)
    V = lambda r: f'vf()[{r}]'
    bc = fn & 3
    BC = lambda r: f'vf()[{r}].v[{bc}]'
    if fn < 0x3c:
        grp = fn >> 2
        if fn < 0x1c:
            op = ['addbc', 'subbc', 'maddbc', 'msubbc', 'max', 'mini', 'mulbc'][grp]
            if op == 'max': return f'lanes({V(fd)}, {m}, [&](int i) {{ return vmax({V(fs)}.v[i], {BC(ft)}); }});'
            if op == 'mini': return f'lanes({V(fd)}, {m}, [&](int i) {{ return vmini({V(fs)}.v[i], {BC(ft)}); }});'
            return f'{op}({V(fd)}, {V(fs)}, {BC(ft)}, {m});'
        t = {0x1c: ('mulbc', 'q()'), 0x1e: ('mulbc', 'regs().i'), 0x20: ('addbc', 'q()'), 0x21: ('maddbc', 'q()'), 0x22: ('addbc', 'regs().i'),
             0x23: ('maddbc', 'regs().i'), 0x24: ('subbc', 'q()'), 0x25: ('msubbc', 'q()'), 0x26: ('subbc', 'regs().i'), 0x27: ('msubbc', 'regs().i')}
        if fn in t: return f'{t[fn][0]}({V(fd)}, {V(fs)}, {t[fn][1]}, {m});'
        if fn == 0x1d: return f'lanes({V(fd)}, {m}, [&](int i) {{ return vmax({V(fs)}.v[i], regs().i); }});'
        if fn == 0x1f: return f'lanes({V(fd)}, {m}, [&](int i) {{ return vmini({V(fs)}.v[i], regs().i); }});'
        v = {0x28: 'add', 0x29: 'maddv', 0x2a: 'mul', 0x2c: 'sub', 0x2d: 'msubv'}
        if fn in v: return f'{v[fn]}({V(fd)}, {V(fs)}, {V(ft)}, {m});'
        if fn == 0x2b: return f'lanes({V(fd)}, {m}, [&](int i) {{ return vmax({V(fs)}.v[i], {V(ft)}.v[i]); }});'
        if fn == 0x2f: return f'lanes({V(fd)}, {m}, [&](int i) {{ return vmini({V(fs)}.v[i], {V(ft)}.v[i]); }});'
        if fn == 0x2e: return f'opmsub({V(fd)}, {V(fs)}, {V(ft)});'
        return None
    op2 = (fd << 2) | (fn & 3)
    bc = op2 & 3
    BC = lambda r: f'vf()[{r}].v[{bc}]'
    if op2 < 0x10:
        f2 = ['addabc', 'subabc', 'maddabc', 'msubabc'][op2 >> 2]
        return f'{f2}({V(fs)}, {BC(ft)}, {m});'
    if 0x10 <= op2 < 0x14: return f'itofv({V(ft)}, {V(fs)}, {m}, {["1.0f", "1.0f / 16.0f", "1.0f / 4096.0f", "1.0f / 32768.0f"][op2 & 3]});'
    if 0x14 <= op2 < 0x18: return f'ftoiv({V(ft)}, {V(fs)}, {m}, {["1.0f", "16.0f", "4096.0f", "32768.0f"][op2 & 3]});'
    if 0x18 <= op2 < 0x1c: return f'mulabc({V(fs)}, {BC(ft)}, {m});'
    t = {0x1c: ('mulabc', 'q()'), 0x1e: ('mulabc', 'regs().i'), 0x20: ('addabc', 'q()'), 0x21: ('maddabc', 'q()'), 0x22: ('addabc', 'regs().i'),
         0x23: ('maddabc', 'regs().i'), 0x24: ('subabc', 'q()'), 0x25: ('msubabc', 'q()'), 0x26: ('subabc', 'regs().i'), 0x27: ('msubabc', 'regs().i')}
    if op2 in t: return f'{t[op2][0]}({V(fs)}, {t[op2][1]}, {m});'
    if op2 == 0x1d: return f'lanes({V(ft)}, {m}, [&](int i) {{ return u2f(f2u({V(fs)}.v[i]) & 0x7fffffffu); }});'
    if op2 == 0x1f: return f'vclip({V(fs)}, {V(ft)}.v[3]);'
    v = {0x28: 'addav', 0x29: 'maddav', 0x2a: 'mulav', 0x2c: 'subav', 0x2d: 'msubav'}
    if op2 in v: return f'{v[op2]}({V(fs)}, {V(ft)}, {m});'
    if op2 == 0x2e: return f'opmula({V(fs)}, {V(ft)});'
    if op2 == 0x2f: return '// vnop'
    if op2 == 0x30: return f'{{ const Vf t = {V(fs)}; lanes({V(ft)}, {m}, [&](int i) {{ return t.v[i]; }}); }}'
    if op2 == 0x31: return f'{{ const Vf t = {V(fs)}; lanes({V(ft)}, {m}, [&](int i) {{ return t.v[(i + 1) & 3]; }}); }}'
    if op2 == 0x35: return f'vsqi({fs}, {ft & 15}, {dest});'
    fsf, ftf = (w >> 21) & 3, (w >> 23) & 3
    if op2 == 0x38: return f'q() = div({V(fs)}.v[{fsf}], {V(ft)}.v[{ftf}]);'
    if op2 == 0x39: return f'q() = vsqrt({V(ft)}.v[{ftf}]);'
    if op2 == 0x3a: return f'q() = vrsqrt({V(fs)}.v[{fsf}], {V(ft)}.v[{ftf}]);'
    if op2 == 0x3b: return '// vwaitq'
    return None


def parse(path):
    return parse_lines(open(path))


def parse_lines(lines):
    """[(addr, word, mnemonic, operands)] of a listing's lines (`addr word mnemonic operands`)."""
    out = []
    for line in lines:
        m = re.match(r'^([0-9a-f]{8})\s+([0-9a-f]{8})\s+(\S+)\s*(.*)$', line.strip())
        if m:
            out.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3).lstrip('_'), m.group(4)))
    return out


def off(o):
    return f'0x{o:x}' if o >= 0 else f'-0x{-o:x}'


def tr(a, w, mn, ops):
    """(statement, branch) for one instruction; branch = (kind, cond, target) or None."""
    op, rs, rt, rd, sa, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
    imm = w & 0xffff
    s, t, d = REG[rs], REG[rt], REG[rd]
    tgt = (a + 4 + (sx16(imm) << 2)) & 0xffffffff
    ea = f'{s}.w() + {off(sx16(imm))}'
    if w == 0:
        return '', None
    if op == 0:
        if fn == 0x00: return f'set32({d}, {t}.w() << {sa});', None
        if fn == 0x02: return f'set32({d}, {t}.w() >> {sa});', None
        if fn == 0x03: return f'set32({d}, static_cast<uint32_t>(static_cast<int32_t>({t}.w()) >> {sa}));', None
        if fn == 0x04: return f'set32({d}, {t}.w() << ({s}.w() & 31));', None
        if fn == 0x08: return '', ('jr', s, None)
        if fn == 0x0f: return '', None  # SYNC
        if fn in (0x20, 0x21): return f'set32({d}, {s}.w() + {t}.w());', None
        if fn in (0x22, 0x23): return f'set32({d}, {s}.w() - {t}.w());', None
        if fn == 0x24: return f'{d}.lo = {s}.lo & {t}.lo;', None
        if fn == 0x25: return f'{d}.lo = {s}.lo | {t}.lo;', None
        if fn == 0x26: return f'{d}.lo = {s}.lo ^ {t}.lo;', None
        if fn == 0x27: return f'{d}.lo = ~({s}.lo | {t}.lo);', None
        if fn in (0x2c, 0x2d): return f'{d}.lo = {s}.lo + {t}.lo;', None
        if fn in (0x2e, 0x2f): return f'{d}.lo = {s}.lo - {t}.lo;', None
        if fn == 0x2a: return f'{d}.lo = static_cast<int64_t>({s}.lo) < static_cast<int64_t>({t}.lo) ? 1 : 0;', None
        if fn == 0x2b: return f'{d}.lo = {s}.lo < {t}.lo ? 1 : 0;', None
        if fn == 0x18: return f'mult3({d}, {s}, {t}, lo, hi);', None
        if fn == 0x06: return f'set32({d}, {t}.w() >> ({s}.w() & 31));', None
        if fn == 0x07: return f'set32({d}, static_cast<uint32_t>(static_cast<int32_t>({t}.w()) >> ({s}.w() & 31)));', None
        if fn == 0x0a: return f'if ({t}.lo == 0) {d}.lo = {s}.lo;', None  # MOVZ
        if fn == 0x0b: return f'if ({t}.lo != 0) {d}.lo = {s}.lo;', None  # MOVN
        if fn == 0x09: return '', ('call', 'jalr', s)  # JALR (rd = ra)
        if fn == 0x0d: return f'ee_break({(w >> 6) & 0xfffff});', None  # BREAK
        if fn == 0x10: return f'set32({d}, hi.w());', None  # MFHI
        if fn == 0x12: return f'set32({d}, lo.w());', None  # MFLO
        if fn == 0x11: return f'hi = {s};', None  # MTHI
        if fn == 0x13: return f'lo = {s};', None  # MTLO
        if fn == 0x1a: return f'ee_div3({s}, {t}, lo, hi);', None  # DIV
        if fn == 0x1b: return f'ee_divu3({s}, {t}, lo, hi);', None  # DIVU
        if fn == 0x19: return f'ee_multu3({d}, {s}, {t}, lo, hi);', None  # MULTU
        if fn == 0x38: return f'{d}.lo = {t}.lo << {sa};', None  # DSLL
        if fn == 0x3b: return f'{d}.lo = static_cast<uint64_t>(static_cast<int64_t>({t}.lo) >> {sa});', None  # DSRA
        if fn == 0x14: return f'{d}.lo = {t}.lo << ({s}.lo & 63);', None  # DSLLV
        if fn == 0x16: return f'{d}.lo = {t}.lo >> ({s}.lo & 63);', None  # DSRLV
        if fn == 0x17: return f'{d}.lo = static_cast<uint64_t>(static_cast<int64_t>({t}.lo) >> ({s}.lo & 63));', None  # DSRAV
        if fn == 0x3a: return f'{d}.lo = {t}.lo >> {sa};', None
        if fn == 0x3c: return f'{d}.lo = {t}.lo << {32 + sa};', None
        if fn == 0x3e: return f'{d}.lo = {t}.lo >> {32 + sa};', None
        if fn == 0x3f: return f'{d}.lo = static_cast<uint64_t>(static_cast<int64_t>({t}.lo) >> {32 + sa});', None
    if op == 1:
        c = {0: f'neg({s})', 1: f'!neg({s})', 2: f'neg({s})', 3: f'!neg({s})'}.get(rt)
        if c: return '', ('likely' if rt in (2, 3) else 'b', c, tgt)
    if op == 2: return '', ('j', 'true', ((a + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2))
    if op == 3: return '', ('call', 'jal', ((a + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2))
    if op in (0x16, 0x17): return '', ('likely', f'static_cast<int64_t>({s}.lo) {"<=" if op == 0x16 else ">"} 0', tgt)
    if op in (4, 5, 0x14, 0x15):
        if op in (4, 0x14) and rs == 0 and rt == 0: c = 'true'
        elif op in (5, 0x15) and rs == rt: c = 'false'
        else: c = f'{s}.lo {"==" if op in (4, 0x14) else "!="} {t}.lo'
        return '', ('likely' if op >= 0x14 else 'b', c, tgt)
    if op in (6, 7): return '', ('b', f'static_cast<int64_t>({s}.lo) {"<=" if op == 6 else ">"} 0', tgt)
    if op in (8, 9): return f'set32({t}, {s}.w() + {off(sx16(imm))});', None
    if op == 0x0a: return f'{t}.lo = static_cast<int64_t>({s}.lo) < {sx16(imm)} ? 1 : 0;', None
    if op == 0x0b: return f'{t}.lo = {s}.lo < static_cast<uint64_t>(static_cast<int64_t>({sx16(imm)})) ? 1 : 0;', None
    if op == 0x33: return '', None  # PREF
    if op == 0x0c: return f'{t}.lo = {s}.lo & 0x{imm:x};', None
    if op == 0x0d: return f'{t}.lo = {s}.lo | 0x{imm:x};', None
    if op == 0x0e: return f'{t}.lo = {s}.lo ^ 0x{imm:x};', None
    if op == 0x0f: return f'set32({t}, 0x{imm << 16:x}u);', None
    if op == 0x11:  # COP1
        fs_, ft_, fd_ = rd, rt, sa
        if rs == 0x00: return f'set32({t}, f2u(fr[{fs_}]));', None                       # MFC1
        if rs == 0x04: return f'fr[{fs_}] = u2f({t}.w());', None                           # MTC1
        if rs == 0x14 and fn == 0x20: return f'fr[{fd_}] = static_cast<float>(static_cast<int32_t>(f2u(fr[{fs_}])));', None  # CVT.S.W
        if rs == 0x10:
            ops2 = {0x00: 'fpu_add', 0x01: 'fpu_sub', 0x02: 'fpu_mul', 0x03: 'fpu_div'}
            if fn in ops2: return f'fr[{fd_}] = {ops2[fn]}(fr[{fs_}], fr[{ft_}]);', None
            if fn == 0x05: return f'fr[{fd_}] = std::fabs(fr[{fs_}]);', None
            if fn == 0x06: return f'fr[{fd_}] = fr[{fs_}];', None
            if fn == 0x07: return f'fr[{fd_}] = -fr[{fs_}];', None
            if fn in (0x24, 0x0d): return f'fr[{fd_}] = cvt_w(fr[{fs_}]);', None  # CVT.W.S / TRUNC.W.S (the PS2 FPU always truncates)
            if fn == 0x04: return f'fr[{fd_}] = fpu_sqrt(fr[{fs_}]);', None
            if fn == 0x34: return f'fcc = fr[{fs_}] < fr[{ft_}];', None
            if fn == 0x36: return f'fcc = fr[{fs_}] <= fr[{ft_}];', None
            if fn == 0x32: return f'fcc = fr[{fs_}] == fr[{ft_}];', None
        if rs == 0x08:
            c = {0: '!fcc', 1: 'fcc', 2: '!fcc', 3: 'fcc'}[rt & 3]
            return '', ('likely' if rt & 2 else 'b', c, tgt)
    if op == 0x10 and rs == 0x08:  # BC0F / BC0T (/ L): CPCOND0 (the DMA controller's "all done") is 1: the host's DMA model copies at
        # once (ee_regs.h hw_sw), as the EE interpreter assumes
        return '', ('likely' if rt & 2 else 'b', 'true' if rt & 1 else 'false', tgt)
    if op == 0x21: return f'set32({t}, static_cast<uint32_t>(static_cast<int16_t>(lhu_({ea}))));', None
    if op == 0x31: return f'fr[{rt}] = u2f(lw_({ea}));', None
    if op == 0x39: return f'sw_({ea}, f2u(fr[{rt}]));', None
    if op == 0x12:  # COP2
        if rs == 0x01: return (f'vu0::drain();\n' if w & 1 else '') + (f'{t} = qmfc2({rd});' if rt else '// (qmfc2 to $zero: nothing)'), None
        if rs == 0x05: return (f'vu0::drain();\n' if w & 1 else '') + f'qmtc2({rd}, {t});', None
        if rs == 0x06: return f'ctc2({rd}, {t});', None
        if rs == 0x02: return f'set32({t}, cfc2({rd}));', None
        if rs >= 0x10:
            if fn == 0x38: return f'vu0::call(0x{(w >> 6) & 0x7fff:x});', None
            if fn == 0x39: return 'vu0::call(vu0::global().cmsar0);', None
            c = cop2_macro(w)
            if c is not None: return 'vmacro();\n' + c, None
            return f'#error COP2 macro {mn} {ops}', None
    if op == 0x1c:  # MMI
        if fn == 0x00: return f'ee_madd3({d}, {s}, {t}, lo, hi);', None  # MADD rd, rs, rt
        if fn == 0x08:
            m0 = {0x03: 'pmaxw', 0x12: 'pextlw', 0x13: 'ppacw', 0x16: 'pextlh', 0x17: 'ppach', 0x1a: 'pextlb', 0x1b: 'ppacb'}
            if sa == 0x1e: return f'{d} = pext5({t});' if rd else '', None
            if sa in m0: return f'{d} = {m0[sa]}({s}, {t});' if rd else '', None
        if fn == 0x28:
            m1 = {0x16: 'pextuh', 0x1a: 'pextub', 0x12: 'pextuw', 0x10: 'padduw', 0x03: 'pminw'}
            if sa in m1: return f'{d} = {m1[sa]}({s}, {t});' if rd else '', None
        if fn == 0x09:
            if sa == 0x0e: return f'{d} = pcpyld({s}, {t});', None
            if sa == 0x1f: return f'{d} = prot3w({t});', None
            if sa == 0x1c: return f'{d} = pmulth({s}, {t}, lo, hi);', None
        if fn == 0x29:
            if sa == 0x0e: return f'{d} = pcpyud({s}, {t});', None
            if sa == 0x1b: return f'{d} = pcpyh({t});', None
            if sa == 0x12: return f'{d} = por({s}, {t});', None
            if sa == 0x1a: return f'{d} = pexch({t});', None
        if fn == 0x30 and sa == 4: return f'{d} = pmfhl_sh(lo, hi);', None
        if fn == 0x3c: return f'{d} = psllw({t}, {sa});', None
        if fn == 0x3e: return f'{d} = psrlw({t}, {sa});', None
        if fn == 0x3f: return f'{d} = psraw({t}, {sa});', None
        if fn == 0x37: return f'{d} = psrah({t}, {sa});', None
    if op == 0x1a: return f'ldl_({ea}, {t});', None  # LDL
    if op == 0x1b: return f'ldr_({ea}, {t});', None  # LDR
    if op == 0x1e: return f'{t} = lq({ea});', None
    if op == 0x1f: return f'sq({ea}, {t});', None
    if op == 0x20: return f'set32({t}, static_cast<uint32_t>(static_cast<int8_t>(lbu_({ea}))));', None
    if op == 0x23: return f'set32({t}, lw_({ea}));', None
    if op == 0x24: return f'{t}.lo = lbu_({ea});', None
    if op == 0x25: return f'{t}.lo = lhu_({ea});', None
    if op == 0x27: return f'{t}.lo = lw_({ea});', None
    if op == 0x28: return f'sb_({ea}, {t}.w());', None
    if op == 0x29: return f'sh_({ea}, {t}.w());', None
    if op == 0x2b: return f'sw_({ea}, {t}.w());', None
    if op == 0x2c: return f'sdl({ea}, {t});', None
    if op == 0x2d: return f'sdr({ea}, {t});', None
    if op == 0x2f: return '', None  # CACHE
    if op == 0x36: return f'lqc2_({rt}, {ea});', None
    if op == 0x37: return f'{t}.lo = ld_({ea});', None
    if op == 0x3e: return f'sqc2_({ea}, {rt});', None
    if op == 0x3f: return f'sd_({ea}, {t}.lo);', None
    return f'#error {a:08x} {w:08x} {mn} {ops}', None


def translate(ins, jr_targets=(), inject=None, gp_fold=True, patches=None):
    """The C++ statements of a listing's instructions ([(addr, word, mnemonic, operands)]), as text.
    jr_targets: the code addresses a `jr <reg>` (not ra) can reach (a jump table of the routine's own): a switch on them.
    inject: {addr: statement}: host code (a note of what the routine decided) emitted before the instruction at addr.
    gp_fold: $gp is the overlay's 0x166c00 (False: a routine that uses gp as a general register, e.g. the skinning).
    patches: {addr: {"replace": [lines] | absent, "before": [lines], "after": [lines]}}: the reviewed edits of a
    transliteration, applied to the lines its instruction at addr produced (a branch's include its delay slot), after the
    relocation passes. The lines are ours (the edits of the review), keyed by address: tools/codegen/gen_asm2cpp.py's specs.
    patches='tags': [(address, line)] instead of the text (for making patches)."""
    inject = inject or {}
    targets = set(jr_targets)
    tr_all = []
    for a, w, mn, ops in ins:
        st, br = tr(a, w, mn, ops)
        if br and br[0] == 'call':
            st, br = f'#error {a:08x} {w:08x} {mn} {ops}  (calls: asm2cpp.py does not translate them)', None
        if mn.startswith('v') and not mn.startswith('vcallms') and br is None and st.startswith('#error'):
            st = f'#error COP2 macro {mn} {ops}'
        tr_all.append((a, w, mn, ops, st, br))
        if br and br[2] is not None:
            targets.add(br[2])
    out = []
    tags = []  # the address of the instruction each line of out comes from
    cur = None
    i = 0
    prev_macro = False
    emitted_next = set()
    labelled = set()
    while i < len(tr_all):
        tags.extend([cur] * (len(out) - len(tags)))
        a, w, mn, ops, st, br = tr_all[i]
        cur = a
        if a in targets and a not in labelled:
            out.append(f'L_{a:06x}:')
        if a in inject:
            out.append(f'    {inject[a]}  // (host) not in the original')
        if br is None:
            if st:
                macro = st.startswith('vmacro();\n')
                if macro:
                    st = st[len('vmacro();\n'):]
                    if not prev_macro: out.append('    vmacro();  // (the VU0 program, if one runs, ends first)')
                prev_macro = macro
                out.append(f'    {st}  // {mn} {ops}'.rstrip())
            i += 1
            continue
        prev_macro = False
        kind, cond, tgt = br
        ds = tr_all[i + 1] if i + 1 < len(tr_all) else None
        dst = ds[4] if ds else ''
        if dst.startswith('vmacro();\n'): dst = 'vmacro(); ' + dst[len('vmacro();\n'):]
        dcomment = f'  // (delay) {ds[2]} {ds[3]}' if ds else ''
        ds_target = bool(ds and ds[0] in targets)
        if kind == 'jr' and cond != 'ra' and jr_targets:
            out.append(f'    {dst}{dcomment}' if dst else f'    // (delay) nop')
            out.append(f'    switch (ovl_code_to_veldin({cond}.w())) {{  // jr {cond}')
            for t_ in jr_targets: out.append(f'    case 0x{t_:08x}: goto L_{t_:06x};')
            out.append('    default: TODO_PORT(0, "jr to an unknown address")();')
            out.append('    }')
            out.append('    return;')
        elif kind == 'jr':
            out.append(f'    {dst}{dcomment}' if dst else f'    // (delay) nop')
            out.append(f'    return;  // jr {cond}')
        elif kind == 'j' or cond == 'true':
            if dst: out.append(f'    {dst}{dcomment}')
            out.append(f'    goto L_{tgt:06x};  // {mn} {ops}')
        elif cond == 'false':
            if dst: out.append(f'    {dst}{dcomment}')
        elif kind == 'likely':
            out.append(f'    if ({cond}) {{  // {mn} {ops}')
            if dst: out.append(f'        {dst}{dcomment}')
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
        if ds_target:  # the delay slot also a jump's target: run on its own from its label
            nxt = ds[0] + 4
            if nxt not in emitted_next:
                emitted_next.add(nxt)
            out.append(f'    goto L_{nxt:06x};')
            out.append(f'L_{ds[0]:06x}:')
            if dst: out.append(f'    {dst}  // {ds[2]} {ds[3]}')
            out.append(f'L_{nxt:06x}:')
            labelled.add(nxt)
        i += 2
    tags.extend([cur] * (len(out) - len(tags)))
    tags = [t for ln, t in zip(out, tags) for _ in ln.split('\n')]  # (a statement of several lines: each tagged)
    # $gp is 0x166c00 for the whole overlay: the gp-relative accesses become plain addresses
    def gp_fold_(m):
        return f'0x{(0x166c00 + int(m.group(1), 16)) & 0xffffffff:08x}u'
    if gp_fold:
        out = [re.sub(r'gp\.w\(\) \+ (-?0x[0-9a-f]+)', gp_fold_, ln) for ln in out]
    # ADR 0008: the overlay addresses relocatable (lui halves folded, then the plain literals)
    import reloc_asm, reloc_rewrite
    text, _ = reloc_asm.relocate('\n'.join(out), one_function=True)
    text, _ = reloc_rewrite.rewrite(text)
    if not patches:
        return text
    lines = text.split('\n')
    assert len(lines) == len(tags), 'the relocation passes changed the line count'
    if patches == 'tags':
        return list(zip(tags, lines))
    res = []
    k = 0
    while k < len(lines):
        a = tags[k]
        j = k
        while j < len(lines) and tags[j] == a:
            j += 1
        p = patches.get(a) if a is not None else None
        if p is None:
            res.extend(lines[k:j])
        else:
            res.extend(p.get('before', []))
            res.extend(p['replace'] if 'replace' in p else lines[k:j])
            res.extend(p.get('after', []))
        k = j
    return '\n'.join(res)


def main():
    path = sys.argv[1]
    # --jr 0xA,0xB,..: the code addresses a `jr <reg>` (not ra) can reach (stored in memory by the routine: a jump table
    # of its own); the jump becomes a switch on the address as Veldin's overlay has it
    jr_targets = []
    if '--jr' in sys.argv:
        jr_targets = [int(x, 16) for x in sys.argv[sys.argv.index('--jr') + 1].split(',')]
    # --inject 0xADDR=<C++ statement> (repeatable): host code (a note of what the routine decided) emitted before the instruction at ADDR
    inject = {}
    for k, a in enumerate(sys.argv):
        if a == '--inject':
            ad, _, code = sys.argv[k + 1].partition('=')
            inject[int(ad, 16)] = code
    print(translate(parse(path), jr_targets, inject, '--no-gp-fold' not in sys.argv))




def live_in(path):
    """GPRs read before written, in listing order (an approximation: the inputs to give the port)."""
    written, used = {0}, []
    for a, w, mn, ops in parse(path):
        op, rs, rt, rd = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
        srcs, dsts = [], []
        if op == 0: srcs, dsts = [rs, rt], [rd]
        elif op in (1, 6, 7): srcs = [rs]
        elif op in (4, 5, 0x14, 0x15): srcs = [rs, rt]
        elif op in (8, 9, 0x0c, 0x0d, 0x0e, 0x18, 0x19, 0x20, 0x21, 0x23, 0x24, 0x25, 0x27, 0x37, 0x1e): srcs, dsts = [rs], [rt]
        elif op == 0x0f: dsts = [rt]
        elif op in (0x28, 0x29, 0x2b, 0x2c, 0x2d, 0x3f, 0x1f): srcs = [rs, rt]
        elif op in (0x31, 0x36, 0x39, 0x3e): srcs = [rs]
        elif op == 0x1c: srcs, dsts = [rs, rt], [rd]
        elif op == 0x12 and rs in (5, 6): srcs = [rt]
        elif op == 0x12 and rs in (1, 2): dsts = [rt]
        elif op == 0x11 and rs == 4: srcs = [rt]
        elif op == 0x11 and rs == 0: dsts = [rt]
        for r in srcs:
            if r not in written and REG[r] not in used: used.append(REG[r])
        written.update(dsts)
    return used


def live_in_fp(path):
    """FPRs read before written, in listing order (an approximation, like live_in: the float arguments f12..f15)."""
    written, used = set(), []
    for a, w, mn, ops in parse(path):
        op, rs, rt, rd, sa, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
        srcs, dsts = [], []
        if op == 0x31: dsts = [rt]
        elif op == 0x39: srcs = [rt]
        elif op == 0x11:
            fs_, ft_, fd_ = rd, rt, sa
            if rs == 0x00: srcs = [fs_]
            elif rs == 0x04: dsts = [fs_]
            elif rs == 0x14: srcs, dsts = [fs_], [fd_]
            elif rs == 0x10:
                if fn in (0x00, 0x01, 0x02, 0x03): srcs, dsts = [fs_, ft_], [fd_]
                elif fn in (0x05, 0x06, 0x07, 0x04, 0x24, 0x0d, 0x0c, 0x0f): srcs, dsts = [fs_], [fd_]
                elif fn in (0x34, 0x36, 0x32): srcs = [fs_, ft_]
        for r in srcs:
            if r not in written and r not in used: used.append(r)
        written.update(dsts)
    return used


if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[2] == '--live-in-fp':
        print(' '.join(f'f{r}' for r in live_in_fp(sys.argv[1]) if 12 <= r <= 19))
    elif len(sys.argv) > 2 and sys.argv[2] == '--live-in':
        print(' '.join(live_in(sys.argv[1])))
    else:
        main()
