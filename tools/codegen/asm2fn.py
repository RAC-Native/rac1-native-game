#!/usr/bin/env python3
"""usage: asm2fn.py <listing.s> --name NAME --ret TYPE [--params "uint32_t m, float f"] [--jr 0xA,0xB] [--jalr 0xA,0xB]
                   [--stub 0xADDR,...] [--entry-note TEXT] > out.cpp

Transliterates a COMPILED function (a the translator listing, calls included) into one C++ function on the
128-bit register variables of src/game/ovl/ee_regs.h, instruction by instruction (tools/codegen/asm2cpp.py's
translator), the way asm2cpp.py does for hand-written routines -- plus what compiled code needs:

  * the function's stack frame (`addiu sp,sp,-N` at its start) taken from the EE stack (ee_stack_alloc), so
    every spill, local and stale lane lives in EE RAM at the original's offsets; `jr ra` jumps to the epilogue;
  * `jal ADDR`: the delay slot first, then the call: a PORTED callee (an `// orig: 0xADDR` annotation in
    src/game) through an adapter generated from its declaration in a header (the integer arguments from
    a0..a3 / t0..t3 in order, floats from f12..f15, references as views of EE RAM; the result into v0 / f0),
    an UNPORTED one through TODO_PORT(OVF(ADDR), ...) (logged once; v0 = 0). A callee ported but declared in
    no header (or with a signature the generator cannot map) is an `#error` to fix by a declaration;
  * `jalr reg`: a switch on the target as Veldin's address (--jalr lists the ported ones), TODO_PORT otherwise;
  * `jr reg` through a jump table (--jr lists the case labels), as asm2cpp.py;
  * DIV / MFLO / MFHI / MOVZ / BREAK / blezl / bgtzl (asm2cpp.py).

The result is a straight transliteration (the original's own control flow, constants and stale lanes), proved by
the oracle like any port; it is not readable code. Overlay addresses go through OV / OVF (reloc_asm.py).
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import asm2cpp
import reloc_asm
import reloc_rewrite
from asm2cpp import REG, parse, tr

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INT_REGS = ['a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3']
FLT_REGS = [12, 13, 14, 15, 16, 17, 18, 19]
INT_TYPES = {'uint32_t': 'u32', 'int32_t': 'i32', 'int': 'i32', 'unsigned': 'u32', 'uint16_t': 'u32', 'int16_t': 'i32', 'uint8_t': 'u32',
             'int8_t': 'i32', 'bool': 'u32', 'uint64_t': 'u64', 'int64_t': 'u64', 'size_t': 'u32'}


def norm_type(t):
    t = re.sub(r'\bconst\b', '', t).strip()
    t = re.sub(r'\bstd::', '', t)
    return re.sub(r'\s+', ' ', t)


def split_params(ps):
    out, depth, cur = [], 0, ''
    for ch in ps:
        if ch in '<(':
            depth += 1
        elif ch in '>)':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def param_type(p):
    p = re.sub(r'\bconst\b', '', p).strip()
    m = re.match(r'^(.*?)(?:\s*\b\w+)?(?:\s*=\s*[^,]+)?$', p)
    # the type is everything up to the last identifier (when there is a name)
    toks = p.rsplit(None, 1)
    if len(toks) == 2 and re.match(r'^[A-Za-z_]\w*$', toks[1]) and toks[0] not in ('unsigned',):
        return norm_type(toks[0])
    if len(toks) == 2 and toks[1].startswith(('&', '*')):
        return norm_type(p)
    return norm_type(p)


def scan_headers():
    """address -> list of (ret, name, [param types]) from the `// orig: 0xADDR` declarations of the headers"""
    table = {}
    pat = re.compile(r'([\w:<>\*&][\w:<>\*&\s]*?)\s+\*?&?(\w+)\(([^;{}]*?)\)\s*;[ \t]*//\s*orig:\s*0x([0-9a-fA-F]{8})\b', re.S)
    for dp, _, fs in os.walk(os.path.join(ROOT, 'src', 'game')):
        for f in fs:
            if not f.endswith('.h'):
                continue
            txt = open(os.path.join(dp, f)).read()
            hdr = os.path.relpath(os.path.join(dp, f), os.path.join(ROOT, 'src')).replace(os.sep, '/')
            for m in pat.finditer(txt):
                ret, name, params, addr = m.group(1).strip(), m.group(2), m.group(3), int(m.group(4), 16)
                ret = re.sub(r'^(inline|static|extern|constexpr)\s+', '', ret.split('\n')[-1]).strip()
                ret = ret.split()[-1] if ret.split() else ret
                params = [param_type(p) for p in split_params(' '.join(params.split()))] if params.strip() not in ('', 'void') else []
                table.setdefault(addr, []).append((norm_type(ret), name, params))
                HEADER_OF[(addr, name)] = hdr
            # the comment-above style: `// orig: 0xADDR ...` lines (a comment block) then the declaration
            for m in re.finditer(r'(?m)^[ \t]*//\s*orig:\s*0x([0-9a-fA-F]{8})\b[^\n]*\n((?:\s*//[^\n]*\n)*)\s*([\w:<>\*&][\w:<>\*&\s]*?)\s+\*?&?(\w+)\(([^;{}]*?)\)\s*;', txt):
                addr = int(m.group(1), 16)
                ret, name, params = m.group(3).strip(), m.group(4), m.group(5)
                ret = re.sub(r'^(inline|static|extern|constexpr)\s+', '', ret.split('\n')[-1]).strip()
                ret = ret.split()[-1] if ret.split() else ret
                params = [param_type(p) for p in split_params(' '.join(params.split()))] if params.strip() not in ('', 'void') else []
                ent = (norm_type(ret), name, params)
                if ent not in table.get(addr, []):
                    table.setdefault(addr, []).append(ent)
                HEADER_OF[(addr, name)] = hdr
    return table


def ported_set():
    # (in Python, not grep: Windows has none)
    found = set()
    for dirpath, _, files in os.walk(os.path.join(ROOT, 'src', 'game')):
        for fn in files:
            with open(os.path.join(dirpath, fn), encoding='utf-8', errors='replace') as fh:
                found.update(int(h, 16) for h in re.findall(r'orig: 0x([0-9a-fA-F]{8})', fh.read()))
    return found


def has_ref(params):
    return any('&' in p or '*' in p or 'Vec4' in p or 'Mat4' in p for p in params)


HEADER_OF = {}  # (address, name) -> the header (relative to src/) that declares it
USED_HEADERS = set()


# ports whose C++ signature is not the register ABI of the original (written by hand)
OVERRIDES = {
    0x001ff290: ['vec3_add_inplace(V(a1.w()), V(a2.w()));'],  # (a0 unused: the sum into a1)
    # rgba_lerp: the colour packed by the MMI (PPACB): its doubleword's upper half zero, not the sign of bit 31
    0x00200298: ['v0.lo = static_cast<uint64_t>(rgba_lerp(a0.w(), a1.w(), fr[12]));'],
    # sprintf: the variadic arguments from a2. (as mips2cpp's glue)
    0x00116248: ['{ const std::vector<uint64_t> va = {a2.lo, a3.lo, t0.lo, t1.lo, t2.lo, t3.lo}; set32(v0, static_cast<uint32_t>(libc_sprintf(a0.w(), a1.w(), va))); }'],
}


# ports returning a 32-bit value the original leaves ZERO-extended in v0's doubleword: `v0.lo = ...`, not set32's sign extension
# (rgba_lerp 0x200298 packs the colour with PPACH / PPACB, bits 32..127 zero; 0x25a400 is its overlay entry)., as mkall.py.
ZEXT32_RET = {0x00200298, 0x0025a400}


def adapter(addr, entries):
    """C++ statements for a call to the ported function(s) at addr, or None"""
    if addr in OVERRIDES:
        return OVERRIDES[addr]
    best = None
    for ret, name, params in entries:
        if best is None or (has_ref(best[2]) and not has_ref(params)):
            best = (ret, name, params)
    ret, name, params = best
    h = HEADER_OF.get((addr, name))
    if h:
        USED_HEADERS.add(h)
    ints, flts, args = 0, 0, []
    stack = 0  # (the 9th and later integer arguments: 8-byte slots from the caller's sp)
    for p in params:
        base = p.replace('&', '').replace('*', '').strip()
        if base in ('float',) and '&' not in p and '*' not in p:
            if flts >= len(FLT_REGS):
                return None
            args.append(f'fr[{FLT_REGS[flts]}]')
            flts += 1
            continue
        if ints >= len(INT_REGS):
            if base in INT_TYPES and INT_TYPES[base] != 'u64' and '&' not in p and '*' not in p:
                args.append(f'static_cast<{base}>(lw_(sp.w() + {8 * stack}))')
                stack += 1
                ints += 1
                continue
            return None
        r = INT_REGS[ints]
        ints += 1
        if p.endswith('&') and base in ('Vec4',):
            args.append(f'V({r}.w())')
        elif p.endswith('&') and base in ('Mat4',):
            args.append(f'M({r}.w())')
        elif p.endswith('&') and base in INT_TYPES:
            args.append(f'ee_ref<{base}>({r}.w())')
        elif p.endswith('*') or '*' in p:
            if base in ('float', 'uint32_t', 'int32_t', 'uint8_t', 'uint16_t', 'int16_t', 'int8_t', 'uint64_t') or True:
                args.append(f'static_cast<{p}>({r}.w() == 0 ? nullptr : ee_ram_ptr({r}.w()))')
        elif base in INT_TYPES:
            if INT_TYPES[base] == 'u64':
                args.append(f'static_cast<{base}>({r}.lo)')
            else:
                args.append(f'static_cast<{base}>({r}.w())')
        else:
            return None
    call = f'{name}({", ".join(args)})'
    if ret == 'void':
        return [f'{call};']
    if ret == 'float':
        return [f'fr[0] = {call};']
    base = ret
    if base in INT_TYPES:
        if INT_TYPES[base] == 'u64' or (base == 'uint32_t' and addr in ZEXT32_RET):
            return [f'v0.lo = static_cast<uint64_t>({call});']
        return [f'set32(v0, static_cast<uint32_t>({call}));']
    return None


def level_fn_map(level):
    """[(veldin_addr, level_addr, size)] of the functions the matcher found in the level's overlay"""
    out = []
    import ovlist
    for ln in open(os.path.join(ovlist.map_dir(), f'level{level}.txt')):
        t = ln.split()
        if len(t) >= 4 and t[0] == 'fn':
            out.append((int(t[1], 16), int(t[2], 16), int(t[3], 16)))
    return out


def main():
    path = sys.argv[1]
    argv = sys.argv[2:]

    def opt(name, default=None):
        return argv[argv.index(name) + 1] if name in argv else default
    name = opt('--name')
    ret_t = opt('--ret', 'void')
    params = split_params(opt('--params', ''))
    level = int(opt('--level', '0'))  # a level's own overlay (L<level>: callees mapped to Veldin's ports, no relocation)
    lmap = level_fn_map(level) if level else []
    own = set()  # level-only functions (--own a,b: the level's addresses of the other functions of this batch)
    jr_targets = [int(x, 16) for x in opt('--jr', '').split(',') if x]
    jalr_targets = [int(x, 16) for x in opt('--jalr', '').split(',') if x]
    stubs = {int(x, 16) for x in opt('--stub', '').split(',') if x}
    level_first = '--jalr-level-first' in argv  # see the jalr below
    ins = parse(path)
    first = ins[0][0]
    # the stack frame
    frame = 0
    for a, w, mn, ops in ins[:4]:
        if (w >> 16) == 0x27bd and (w & 0x8000):
            frame = 0x10000 - (w & 0xffff)
            break
    table = scan_headers()
    ported = ported_set()
    spec_path = opt('--spec')
    host = {}  # the spec entry's "host": {"<addr>": "<statement>"}: host code (an MRP note) put before that instruction
    entry_host = None  # the spec entry's "entry_host": a line of host code at the top of the body (a level's own fork)
    if spec_path:  # the other functions of the same batch: known callees (their own signatures)
        import json
        for f in json.load(open(spec_path))['functions']:
            if f['name'] == name:
                host = {int(k, 16): v for k, v in f.get('host', {}).items()}
                entry_host = f.get('entry_host')
            ad = int(f['addr'], 16)
            ps = [param_type(p) for p in split_params(f.get('params', ''))]
            table[ad] = [(f.get('ret', 'void'), f['name'], ps)]
            ported.add(ad)
    tr_all, targets = [], set(jr_targets)
    for a, w, mn, ops in ins:
        st, br = tr(a, w, mn, ops)
        tr_all.append((a, w, mn, ops, st, br))
        if br and br[0] != 'call' and br[2] is not None:
            targets.add(br[2])
    # a callee the port declares void whose original leaves a value in v0 / f0 that this function reads after the call: its register stale
    import hi_flow as _hf
    for pc_, tgt_, v0_, f0_ in _hf.return_use(ins):
        ent = table.get(tgt_) or table.get(next((vv + (tgt_ - ll) for vv, ll, sz in lmap if ll <= tgt_ < ll + sz), 0))
        if ent and tgt_ not in OVERRIDES and (v0_ or f0_):
            declared = ent[0][0]
            if (v0_ and declared == 'void') or (f0_ and declared not in ('float',) and not v0_):
                print(f'// asm2fn: 0x{pc_:08x}: the call to 0x{tgt_:08x} ({ent[0][1]}, declared {declared}) is followed by a read of '
                      f'{"v0" if v0_ else "f0"}: the port returns nothing of the kind: its register stays stale', file=sys.stderr)
    if not level:  # the overlay addresses built by a lui and used far from it: by the control flow (hi_flow.py)
        import hi_flow
        tr_all, flow_errs = hi_flow.rewrite(ins, tr_all, jr_targets)
        for e in flow_errs:
            print(f'// asm2fn: {e}', file=sys.stderr)
    out = []
    # signature
    ptxt = ', '.join(params)
    out_includes_marker = len(out)
    out.append(f'// orig: L{level}:0x{first:08x}' if level else f'// orig: 0x{first:08x}')
    out.append(f'{ret_t} {name}({ptxt}) {{')
    if entry_host:
        out.append('    ' + entry_host)
    out.append('    Q128 zero, at, v0, v1, a0, a1, a2, a3, t0, t1, t2, t3, t4, t5, t6, t7, s0, s1, s2, s3, s4, s5, s6, s7, t8, t9, gp, sp, s8, ra, lo, hi;')
    out.append('    float fr[32] = {};')
    out.append('    bool fcc = false;')
    out.append('    (void)at; (void)v1; (void)t0; (void)t1; (void)t2; (void)t3; (void)t4; (void)t5; (void)t6; (void)t7; (void)t8; (void)t9; (void)gp; (void)ra; (void)lo; (void)hi; (void)fcc; (void)s5; (void)s6; (void)s7; (void)s8;')
    ii = ff = 0
    pnames = []
    for p in params:
        pn = re.split(r'[\s\*&]+', p.strip())[-1]
        ty = p[:len(p) - len(pn)].strip()
        if ty == 'float':
            out.append(f'    fr[{FLT_REGS[ff]}] = {pn};')
            ff += 1
        elif INT_TYPES.get(norm_type(ty)) == 'u64':
            out.append(f'    {INT_REGS[ii]}.lo = {pn};')
            ii += 1
        else:
            out.append(f'    set32({INT_REGS[ii]}, static_cast<uint32_t>({pn}));')
            ii += 1
    if frame:
        out.append(f'    const uint32_t frame_base = ee_stack_alloc(0x{frame:x});')
        out.append(f'    set32(sp, frame_base + 0x{frame:x}u);')
    out.append('    (void)sp;')
    i = 0
    emitted_next = set()
    labelled = set()
    done_label = False

    def call_stmts(kind, tgt, delay_idx):
        """statements of a call"""
        if kind == 'jal' and level and tgt not in table:
            v = next((vv + (tgt - ll) for vv, ll, sz in lmap if ll <= tgt < ll + sz), None)
            if v is not None and v in table and v in ported:
                ad = adapter(v, table[v])
                return ad if ad is not None else [f'#error 0x{tgt:08x} (Veldin 0x{v:08x}): a ported callee whose signature the generator cannot map']
            if v is not None and v in ported:
                return [f'#error 0x{tgt:08x} (Veldin 0x{v:08x}) is ported but declared in no header with `// orig:`: declare it']
            return [f'TODO_PORT(0x{tgt:08x}u, "unported call (level {level})")(a0.w(), a1.w(), a2.w(), a3.w());', 'set32(v0, 0);']
        if kind == 'jal':
            if tgt in table and tgt in ported:
                ad = adapter(tgt, table[tgt])
                if ad is None:
                    return [f'#error 0x{tgt:08x}: a ported callee whose signature the generator cannot map']
                return ad
            if tgt in ported and tgt not in stubs:
                return [f'#error 0x{tgt:08x} is ported but declared in no header with `// orig:`: declare it']
            return [f'TODO_PORT(OVF(0x{tgt:08x}u), "unported call")(a0.w(), a1.w(), a2.w(), a3.w());', 'set32(v0, 0);']
        # jalr
        reg = tgt
        # --jalr-level-first: a carried class's hook (`class +0x2c` table) is the level's own function on most levels; the
        # overlay map pairs some with a Veldin hook whose constants differ (it masks them): the level's own ports are asked first
        lines = []
        if level_first:
            lines.append(f'{{ int32_t lvl_ret = 0; if (class_hook_call_level({reg}.w(), a0.w(), a1.w(), a2.w(), fr[12], lvl_ret)) {{ set32(v0, static_cast<uint32_t>(lvl_ret)); }} else {{')
        lines.append(f'switch ({reg}.w()) {{' if level else f'switch (ovl_code_to_veldin({reg}.w())) {{')
        for t_ in jalr_targets:
            ad = adapter(t_, table[t_]) if t_ in table else None
            lines.append(f'case 0x{t_:08x}u: {{ ' + (' '.join(ad) if ad else f'#error jalr target 0x{t_:x}') + ' break; }')
        lines.append(f'default: {{ int32_t hook_ret = 0; if (class_hook_call_l1({reg}.w(), a0.w(), a1.w(), a2.w(), fr[12], hook_ret)) set32(v0, static_cast<uint32_t>(hook_ret)); else {{ TODO_PORT({reg}.w(), "indirect call")(a0.w(), a1.w(), a2.w(), a3.w()); set32(v0, 0); }} break; }}')
        lines.append('}')
        if level_first:
            lines.append('} }')
        return lines

    while i < len(tr_all):
        a, w, mn, ops, st, br = tr_all[i]
        if a in targets and a not in labelled:
            out.append(f'L_{a:06x}:')
            out.append(f'    ASM_TRACE(NOT_ADDR(0x{a:08x}));')
        if a in host:  # (host code: not the game's; never in a delay slot: the loop emits a delay slot with its branch)
            out.append(f'    {host[a]}  // (host)')
        if br is None:
            if st:
                sl = st.split('\n')  # (a statement of several lines: each indented, the comment on the last)
                for x in sl[:-1]:
                    out.append(f'    {x}')
                out.append(f'    {sl[-1]}  // {mn} {ops}'.rstrip())
            i += 1
            continue
        kind, cond, tgt = br
        ds = tr_all[i + 1] if i + 1 < len(tr_all) else None
        dst = ds[4] if ds else ''
        dcomment = f'  // (delay) {ds[2]} {ds[3]}' if ds else ''
        ds_target = bool(ds and ds[0] in targets)
        if kind == 'call':
            if dst:
                out.append(f'    {dst}{dcomment}')
            for k_, c_ in enumerate(call_stmts(cond, tgt, i + 1)):
                out.append(f'    {c_}  // {mn} {ops}' if k_ == 0 else f'    {c_}')
            if ds_target:
                nxt = ds[0] + 4
                out.append(f'    goto L_{nxt:06x};')
                out.append(f'L_{ds[0]:06x}:')
                if dst:
                    out.append(f'    {dst}  // {ds[2]} {ds[3]}')
                out.append(f'L_{nxt:06x}:')
                labelled.add(nxt)
            i += 2
            continue
        if kind == 'jr' and cond != 'ra' and jr_targets:
            out.append(f'    {dst}{dcomment}' if dst else '    // (delay) nop')
            out.append(f'    switch ({cond}.w()) {{  // jr {cond}' if level else f'    switch (ovl_code_to_veldin({cond}.w())) {{  // jr {cond}')
            for t_ in jr_targets:
                out.append(f'    case 0x{t_:08x}: goto L_{t_:06x};')
            out.append('    default: TODO_PORT(0, "jr to an unknown address")();')
            out.append('    }')
            out.append('    goto L_done;')
        elif kind == 'jr':
            out.append(f'    {dst}{dcomment}' if dst else '    // (delay) nop')
            out.append(f'    goto L_done;  // jr {cond}')
            done_label = True
        elif kind == 'j' or cond == 'true':
            if dst:
                out.append(f'    {dst}{dcomment}')
            out.append(f'    goto L_{tgt:06x};  // {mn} {ops}')
        elif cond == 'false':
            if dst and kind != 'likely':  # (a likely branch never taken: its delay slot does not run)
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
            if dst:
                out.append(f'    {dst}  // {ds[2]} {ds[3]}')
            out.append(f'L_{nxt:06x}:')
            labelled.add(nxt)
        i += 2
    def reindent(ln):  # (a statement of several lines: each line at the first's indentation)
        if '\n' not in ln:
            return ln
        ind = ln[:len(ln) - len(ln.lstrip())]
        return '\n'.join([ln.split('\n')[0]] + [ind + x.lstrip() for x in ln.split('\n')[1:]])
    out = [reindent(ln) for ln in out]
    out.append('L_done:')
    if frame:
        out.append(f'    ee_stack_free(0x{frame:x});')
    if ret_t == 'void':
        out.append('    return;')
    elif ret_t == 'float':
        out.append('    return fr[0];')
    elif INT_TYPES.get(ret_t) == 'u64':
        out.append(f'    return static_cast<{ret_t}>(v0.lo);')
    else:
        out.append(f'    return static_cast<{ret_t}>(v0.w());')
    out.append('}')

    def gp_fold(m):
        return f'0x{(0x166c00 + int(m.group(1), 16)) & 0xffffffff:08x}u'
    if not level:
        out = [re.sub(r'gp\.w\(\) \+ (-?0x[0-9a-f]+)', gp_fold, ln) for ln in out]
    out[out_includes_marker:out_includes_marker] = [f'// asm2fn-include: <{h}>' for h in sorted(USED_HEADERS)]
    if level:  # the level's own addresses stay as they are (no translation)
        # $gp: the base ELF's (0x166c00 on every level: mips2cpp.py folds it so); an overlay data address the level's own (L<n>_ADDR)
        def gp_fold_level(m):
            a = (0x166c00 + int(m.group(1), 16)) & 0xffffffff
            return f'L{level}_ADDR(0x{a:08x}u)' if 0x15ef00 <= a < 0x1eaa00 else f'0x{a:08x}u'
        out = [re.sub(r'gp\.w\(\) \+ (-?0x[0-9a-f]+)', gp_fold_level, ln) for ln in out]

        # a lui's high half in a delay slot (the comment `// (delay) lui`): the level's own address half, marked as such
        # (ADR 0008's lint skips the `// lui` lines only)
        def lui_delay_level(m):
            v = int(m.group(2), 16)
            return f'set32({m.group(1)}, L{level}_ADDR(0x{v:x}u));' if 0x150000 <= v < 0x300000 else m.group(0)
        out = [re.sub(r'set32\((\w+), 0x([0-9a-f]+)u\);(?=\s*// \(delay\) lui )', lui_delay_level, ln) for ln in out]
        text = '\n'.join(out)
        if 'gp.w()' in text.replace('(void)gp', ''):
            text = '#error $gp used other than as a base in a level-specific function\n' + text
        print(text)
        return
    # (the overlay addresses built by a lui are relocated by hi_flow.py: the control-flow-aware pass; reloc_asm.py's linear one is not used)
    text, _ = reloc_rewrite.rewrite('\n'.join(out))
    print(text)


if __name__ == '__main__':
    main()
