#!/usr/bin/env python3
# usage: mkall.py <out.cpp> "<title>" <addr hex>:<cpp name> [...] [--glue extra.glue] [--ov-dir DIR]
#
# Generates one source file with the transliterations (tools/codegen/mips2cpp.py) of the given entry functions of a
# level's overlay (the level whose `overlay.elf` / section files are in --ov-dir, level 1 by default: its
# `assets/generated/overlay_map/level1.txt` gives the Veldin counterparts) and of every function they call that
# has no port: callees with a Veldin port get a glue line built from the port's declaration (integer / float /
# pointer parameters only; the others are listed as "NO GLUE" for a hand-written line in --glue), the others are
# transliterated too (uniform ABI), leaves first. Jump tables of `jr rX` are found from the code (sltiu bound,
# lui / addiu table address) and read from the overlay's sections.
import os
import re
import struct
import subprocess
import sys

W = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
TR = os.path.dirname(os.path.abspath(__file__)) + '/mips2cpp.py'
LEVEL = os.environ.get('MKALL_LEVEL') or os.environ.get('MKALL_LVL') or '1'  # 0 Veldin, 1 Novalis, 2 Aridia, 3 Kerwan, 4 Eudora ... (MKALL_LVL: the old name)
# MKALL_ISO=1 (or MKALL_LVL set): the level's own overlay read straight from the ISO (tools/codegen/ovlist.py, no Ghidra project):
# listings, jump-table words and the level's variants table (src/game/ovl/variants_l<n>.cpp) as the callee map
ISO_MODE = bool(os.environ.get('MKALL_ISO') or os.environ.get('MKALL_LVL'))
LVL = int(LEVEL) if ISO_MODE else None  # (0 = Veldin's overlay, also from the ISO since)
if LVL is not None:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import ovlist
VELDIN = LEVEL == '0'
EXTRA_INC = ''.join('#include <%s>\n' % h for h in os.environ.get('MKALL_INCLUDE', '').split(',') if h)  # the file's own header
PROOF = os.environ.get('MKALL_PROOF', 'batch 103' if LEVEL == '1' else 'batch 109')  # the oracle batch named in the file's header
SCR = os.environ.get('MKALL_TMP', '/tmp/mkall' + ('_v' if VELDIN else ''))
os.makedirs(SCR, exist_ok=True)
# (without MKALL_ISO / MKALL_LVL: a Ghidra project's listings; the setup never runs that way)
GH = os.environ.get('MKALL_GH')  # runs Listing.java / Dis2.java on the level's project
OV = os.environ.get('MKALL_OV')  # the overlay's sections, with their manifest.csv
MAPF = os.path.join(ovlist.map_dir(), 'level%s.txt' % LEVEL) if LVL is not None else \
    (os.environ.get('RAC1_ASSETS') or W + '/assets') + '/generated/overlay_map/level%s.txt' % LEVEL  # (RAC1_ASSETS)
# the enhancements' hook points in the levels' own code (tools/codegen/mips2cpp.py load_hooks)
os.environ.setdefault('MIPS2CPP_HOOKS', W + '/tools/codegen/hooks.glue')


def sections():
    out = []
    if LVL is not None:
        return out
    if not (GH and OV):
        sys.exit('mkall: without MKALL_ISO / MKALL_LVL, set MKALL_GH and MKALL_OV (a Ghidra project)')
    for l in open(OV + '/manifest.csv').read().splitlines()[1:]:
        p = l.split(',')
        out.append((int(p[2], 16), int(p[3]), open(OV + '/' + p[0], 'rb').read()))
    return out


SEC = sections()
ENTRY_SIG = {}
FMAX = 15 if LEVEL in ('0', '1') else 19  # the uniform ABI's last float argument register (levels 0 / 1 keep their generated files' f12..f15)
FARGS = ', '.join('fr[%d]' % k for k in range(12, FMAX + 1))
FDECL = ', '.join('float f%d' % k for k in range(12, FMAX + 1))
os.environ['UNIFORM_FMAX'] = str(FMAX)
BASE_END = 0x15ef00  # the base ELF ends here (the levels' overlay code starts above it: 0x1dd300 on Eudora, 0x1e3880 on Novalis, ...)
# the helpers' names: `ovl_<addr>_tr` (Veldin), `nov_<addr>_l1`, `lv<n>_<addr>_l<n>`; MKALL_HELPER_PREFIX replaces the prefix (`eud_`, `ril_`, `rih_` for the hero run)
HELPER_FMT = (os.environ.get('MKALL_HELPER_PREFIX') or ('ovl_' if VELDIN else 'nov_' if LEVEL == '1' else 'lv' + LEVEL + '_')) + '%06x_' + ('tr' if VELDIN else 'l' + LEVEL)
UNIFORM_NAMES = {x for x in os.environ.get('MKALL_UNIFORM', '').split(',') if x}  # (entries of the uniform ABI whose names do not end in `_u`)
UNIFORM_ENTRY = lambda nm: nm.endswith('_u') or nm in UNIFORM_NAMES  # an entry in the uniform ABI (TrRet, every argument register): its glue is preset
FORCE = {int(x, 16) for x in os.environ.get('MKALL_FORCE', '').split(',') if x}


def standin_hooks(ad):
    """A hooks file for the stand-in at `ad` (tools/codegen/ovlist.py): the `L<n> PAL:0x...` lines of hooks.glue inside the PAL
    function it is made from, their addresses and `L_PAL_<addr>` labels moved to the stand-in's (its NTSC-U entry + the
    offset in the PAL function). The `PAL:` addresses are the PAL disc 1.00's (data/regions/SCES-50916.txt): on another
    build (PAL 2.00) its stand-ins get none."""
    pal, end, n = ovlist.overlay(LVL).standin(ad).local
    move = lambda a: n + (a - pal)
    out = []
    table = ovlist.rv.disc_of(ovlist.rv.disc_path(ovlist.ASSETS))[2]
    for line in open(os.environ['MIPS2CPP_HOOKS']) if table is not None and os.path.basename(table) == 'SCES-50916.txt' else []:
        m = re.match(r'^L(\d+)\s+PAL:(0x[0-9a-fA-F]+)(.*)$', line.rstrip('\n'))
        if m and int(m.group(1)) == LVL and pal <= int(m.group(2), 16) < end:
            rest = re.sub(r'L_PAL_([0-9a-f]+)', lambda x: 'L_%x' % move(int(x.group(1), 16)), m.group(3))
            out.append('L%d 0x%08x%s' % (LVL, move(int(m.group(2), 16)), rest))
    path = os.path.join(SCR, 'standin_hooks_%x.glue' % ad)
    open(path, 'w').write('\n'.join(out) + '\n')
    print('    %x: a stand-in (PAL:0x%x), its %d hook point(s) from the PAL build' % (ad, pal, len(out)), file=sys.stderr)
    return path


def word(a):
    if LVL is not None:
        w = ovlist.overlay(LVL).data_word(a)
        return w if w is not None else 0
    for d, n, b in SEC:
        if d <= a < d + n:
            return struct.unpack_from('<I', b, a - d)[0]
    return 0


fnmap = {}
for l in ([] if VELDIN else open(MAPF)):
    p = l.split()
    if p and p[0] == 'fn':
        fnmap[int(p[2], 16)] = int(p[1], 16)
if LVL:  # the level's hand-checked variants (a Veldin function with another build here)
    vf = W + '/src/game/ovl/variants_l%d.cpp' % LVL
    if os.path.exists(vf):
        for m in re.finditer(r'\{0x([0-9a-f]+), 0x([0-9a-f]+), 0x([0-9a-f]+)\}', open(vf).read()):
            fnmap[int(m.group(2), 16)] = int(m.group(1), 16)

# rows of the level's variants table that are the same code as Veldin's (MKALL_EXTRAMAP: lines `<level hex> <Veldin hex>`)
if os.environ.get('MKALL_EXTRAMAP'):
    for l in open(os.environ['MKALL_EXTRAMAP']):
        p = l.split()
        if len(p) == 2:
            fnmap[int(p[0], 16)] = int(p[1], 16)

# ---- the ports' declarations by Veldin address
DECL = {}
for d in ('src/game/ovl', 'src/game/math', 'src/game/lib', 'src/game'):
    for f in sorted(os.listdir(W + '/' + d)):
        if not f.endswith('.h'):
            continue
        for line in open(W + '/' + d + '/' + f):
            m = re.match(r'^\s*([\w:<>&\* ]+?)\s+(\w+)\(([^;{]*)\)\s*;\s*//.*?orig: 0x0*([0-9a-f]+)', line)
            if m:
                DECL.setdefault(int(m.group(4), 16), []).append((m.group(1).strip(), m.group(2), m.group(3).strip(), f))

INT = {'uint32_t', 'int32_t', 'uint16_t', 'int16_t', 'uint8_t', 'int8_t', 'bool', 'int', 'uint64_t', 'int64_t'}

# ports returning a 32-bit value the original leaves ZERO-extended in v0's doubleword (not sign-extended as a MIPS `lw` / `addu`
# result would be): their glue is `v0.lo = ...`, so a caller storing v0 with `sd` (a GS register) gets the original's upper word.
# 0x200298 rgba_lerp ends `ppach v0,zero,v0; ppacb v0,zero,v0` (the colour's 4 bytes, bits 32..127 zero); 0x25a400 is its
# overlay entry (jal 0x200298, v0 untouched). (Found with an alpha >= 0x80.)
ZEXT32_RET = {0x00200298, 0x0025a400}


def glue_for(v):
    """(glue statement, note) for the Veldin port at v, or None."""
    best = None
    for ret, name, args, f in DECL.get(v, []):
        params = [x.strip() for x in args.split(',')] if args and args != 'void' else []
        ok = True
        parts = []
        pre = []
        post = []
        ni = nf = 0
        iregs = ['a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3']
        for k, prm in enumerate(params):
            t = re.sub(r'\s*\w+$', '', prm).strip() if re.search(r'\w+$', prm) and ' ' in prm.strip() else prm
            t = t.replace('const ', '').strip()
            if t == 'float':
                parts.append(f'fr[{12 + nf}]')
                nf += 1
                continue
            if ni >= 8:
                ok = False
                break
            r = iregs[ni]
            ni += 1
            cast = {'int32_t': 'static_cast<int32_t>({}.w())', 'int': 'static_cast<int32_t>({}.w())', 'bool': '({}.w() != 0)',
                    'int16_t': 'static_cast<int16_t>({}.w())', 'int8_t': 'static_cast<int8_t>({}.w())',
                    'int64_t': 'static_cast<int64_t>({}.lo)', 'uint64_t': '{}.lo'}.get(t, '{}.w()')
            if t in INT:
                parts.append(cast.format(r))
            elif t == 'Vec4&':
                parts.append(f'V({r}.w())')
            elif t == 'Mat4&':
                parts.append(f'M({r}.w())')
            elif t in ('int32_t*', 'uint32_t*', 'float*', 'int32_t&', 'uint32_t&', 'float&'):
                base = t[:-1]
                isf = base == 'float'
                pre.append(f'{base} t{k} = {r}.w() != 0 ? ' + (f'u2f(lw_({r}.w()))' if isf else f'static_cast<{base}>(lw_({r}.w()))') + ' : 0;')
                post.append(f'if ({r}.w() != 0) sw_({r}.w(), ' + (f'f2u(t{k})' if isf else f'static_cast<uint32_t>(t{k})') + ');')
                parts.append(f't{k}' if t.endswith('&') else f'{r}.w() != 0 ? &t{k} : nullptr')
            else:
                ok = False
                break
        if not ok:
            continue
        call = f'{name}({", ".join(parts)})'
        r = ret.replace('const ', '').strip()
        if r == 'void':
            stmt = call + ';'
        elif r == 'float':
            stmt = f'fr[0] = {call};'
        elif r in ('uint64_t', 'int64_t') or (r == 'uint32_t' and v in ZEXT32_RET):
            # a doubleword result (a GS register: the effect textures' TEX0): all 64 bits; a zero-extended 32-bit one
            stmt = f'v0.lo = static_cast<uint64_t>({call});'
        elif r in INT:
            stmt = f'set32(v0, static_cast<uint32_t>({call}));'
        else:
            continue
        if pre or post:
            stmt = '{ ' + ' '.join(pre) + ' ' + stmt + ' ' + ' '.join(post) + ' }'
        score = 0 if all(('&' not in p and '*' not in p) for p in params) else 1
        if best is None or score < best[0]:
            best = (score, stmt, f'{name} ({f})')
    return best


# ---- listings
def run_gh(script, *args):
    r = subprocess.run([GH, script] + list(args), capture_output=True, text=True)
    return r.stdout.splitlines()


def listing(addr):
    f = '%s/%08x.s' % (SCR, addr)
    if os.path.exists(f) and os.path.getsize(f) > 0:
        return f
    if LVL is not None:
        open(f, 'w').write('\n'.join(ovlist.listing(LVL, addr)) + '\n')
        return f
    lines = run_gh('Listing.java', SCR, '0x%x' % addr)
    del lines
    ls = []
    p = '%s/%08x.s' % (SCR, addr)
    if os.path.exists(p):
        ls = [l for l in open(p).read().splitlines() if re.match(r'^[0-9a-f]{8}\s', l)]

    def holes(ls):
        a = [int(l.split()[0], 16) for l in ls]
        return any(b - c != 4 for c, b in zip(a, a[1:]))

    def complete(ls):
        return len(ls) > 2 and re.search(r'\sjr ra', ls[-2]) is not None

    if not ls or holes(ls) or not complete(ls):
        # a Ghidra body with holes / without its end: the whole range from the start to the last `jr ra` + delay
        n = 1400
        raw = [l for l in run_gh('Dis2.java', '%x' % addr, str(n)) if re.match(r'^[0-9a-f]{8}\s', l)]
        out = []
        last_end = None
        stack_depth_ret = None
        for i, l in enumerate(raw):
            out.append(l)
            if re.search(r'\sjr ra', l) and i + 1 < len(raw) and re.search(r'addiu sp,sp,0x', raw[i + 1]):
                out.append(raw[i + 1])
                last_end = len(out)
                break
        if last_end is None:  # frameless: up to the first jr ra + delay slot past the last branch target
            maxt = addr
            for i, l in enumerate(raw):
                m = re.search(r'\s(?:b|beq|bne|beql|bnel|bgez|bltz|bgtz|blez|bc1[tf]l?|bgezl|bltzl)\w*\s.*0x([0-9a-f]{8})$', l)
                if m:
                    maxt = max(maxt, int(m.group(1), 16))
                out.append(l) if l not in out else None
                if re.search(r'\sjr ra', l):
                    pc = int(l.split()[0], 16)
                    if pc + 4 >= maxt:
                        if i + 1 < len(raw):
                            out.append(raw[i + 1]) if raw[i + 1] not in out else None
                        break
            out = raw[:raw.index(out[-1]) + 1] if out else raw
        ls = out
    open(f, 'w').write('\n'.join(ls) + '\n')
    return f


LOCALS = {}  # function -> [(start, end)] of the routines it reaches by bal (shared registers: appended to its listing)


def combined(ad):
    """The function's listing, with the routines it calls by bgezal / bltzal appended (hand-written asm that takes its inputs from the caller's
    registers and leaves its results there: they cannot be separate functions with the uniform ABI)."""
    f = listing(ad)
    txt = open(f).read()
    tg = sorted(set((a + 4 + 4 * ((w & 0xffff) - ((w & 0x8000) << 1))) & 0xffffffff for a, w in _words(f)
                    if (w >> 26) == 1 and ((w >> 16) & 31) in (0x10, 0x11) and (w >> 21) & 31))  # bltzal / bgezal (not bal: a call)
    if not tg:
        return f
    parts = [txt]
    LOCALS[ad] = []
    for t in tg:
        g = open(listing(t)).read()
        ls = [l for l in g.splitlines() if re.match(r'^[0-9a-f]{8}\s', l)]
        LOCALS[ad].append((int(ls[0].split()[0], 16), int(ls[-1].split()[0], 16)))
        parts.append(g)
    out = f + '.cmb'
    open(out, 'w').write(''.join(parts))
    return out


def _words(f):
    """[(address, word)] of a listing's instruction lines (the code is decided on the words; the text is a comment)."""
    out = []
    for l in open(f).read().splitlines():
        m = re.match(r'^([0-9a-f]{8})\s+([0-9a-f]{8})\s', l)
        if m:
            out.append((int(m.group(1), 16), int(m.group(2), 16)))
    return out


def jumptables(f):
    """{jr address: [targets]} for the indirect jumps of a listing (the idiom: sltiu bound; lui / addiu base; sll 2; addu; lw; jr)."""
    ls = _words(f)
    res = {}
    for i, (a, w) in enumerate(ls):
        if not ((w >> 26) == 0 and (w & 0x3f) == 8 and ((w >> 21) & 31) != 31):  # jr <reg> (not ra)
            continue
        win = ls[max(0, i - 40):i]
        bound = None
        base = None
        hi = None
        for _a, w2 in reversed(win):
            if (w2 >> 26) == 0x0b and bound is None:  # sltiu
                bound = w2 & 0xffff
        for _a, w2 in win:
            op = w2 >> 26
            if op == 0x0f:  # lui
                hi = ((w2 >> 16) & 31, (w2 & 0xffff) << 16)
            if op == 0x09 and hi and ((w2 >> 21) & 31) == hi[0]:  # addiu rt,<the lui register>,imm
                v = (w2 & 0xffff) - ((w2 & 0x8000) << 1)
                base = (hi[1] + v) & 0xffffffff
        if bound and base:
            res[a] = [word(base + 4 * k) for k in range(bound)]
    return res


def callees(f):
    t = set()
    for a, w in _words(f):
        if (w >> 26) == 3:  # jal
            t.add(((a + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2))
    return t


def main():
    a = sys.argv[1:]
    out_cpp, title = a[0], a[1]
    entries = []
    extra = None
    i = 2
    while i < len(a):
        if a[i] == '--glue':
            extra = a[i + 1]
            i += 2
        else:
            ad, nm, *sig = a[i].split(':')  # addr:name[:ret[:a0,a1,..]]  (ret void / v0 / f0; default void, a0)
            entries.append((int(ad, 16), nm))
            ENTRY_SIG[int(ad, 16)] = (sig[0] if sig else 'void', sig[1] if len(sig) > 1 else 'a0')
            i += 1
    glue = {}
    if extra:
        for l in open(extra):
            l = l.split('//')[0].strip()
            m = re.match(r'(0x[0-9a-fA-F]+)\s*:\s*(.*)$', l)
            if m:
                glue[int(m.group(1), 16)] = m.group(2)
    common = os.environ.get('MKALL_COMMON')
    if common:
        for l in open(common):
            l = l.split('//')[0].strip()
            m = re.match(r'(0x[0-9a-fA-F]+)\s*:\s*(.*)$', l)
            if m and int(m.group(1), 16) not in glue:
                glue[int(m.group(1), 16)] = m.group(2)
    vcommon = os.environ.get('MKALL_VCOMMON')  # glue keyed by the callee's Veldin address (resolved through the level's map)
    vglue = {}
    if vcommon:
        for l in open(vcommon):
            l = l.split('//')[0].strip()
            m = re.match(r'(0x[0-9a-fA-F]+)\s*:\s*(.*)$', l)
            if m:
                vglue[int(m.group(1), 16)] = m.group(2)
    entry_names = dict(entries)
    for _ad, _nm in entries:  # the entries in the uniform ABI call each other (the hero functions): their glue is known before they are generated
        if UNIFORM_ENTRY(_nm):
            glue[_ad] = '{ const TrRet r = %s(a0.w(), a1.w(), a2.w(), a3.w(), t0.w(), t1.w(), t2.w(), t3.w(), %s); set32(v0, r.v0); fr[0] = r.f0; }' % (_nm, FARGS)
    # the closure of the functions without glue
    order = []
    seen = set()
    nogl = []

    def visit(ad, stack):
        if ad in seen:
            return
        seen.add(ad)
        f = combined(ad)
        for t in sorted(callees(f)):
            if t in glue or t in entry_names:
                continue
            v = fnmap.get(t) or (t if (t < BASE_END or VELDIN) else None)
            if v in vglue and t not in FORCE:
                glue[t] = vglue[v]
                continue
            g = glue_for(v) if v and t not in FORCE else None  # (MKALL_FORCE: addresses transliterated although a Veldin port exists)
            if g:
                glue[t] = g[1]
                continue
            if t < BASE_END:  # a base ELF function (the kernel's, libc's) without a port: a TODO_PORT stub, as the ports do
                glue[t] = 'TODO_PORT(0x%08x, "base ELF function (not ported)")();' % t
                continue
            if v and v in DECL and t not in FORCE:
                nogl.append((t, v, 'declaration not convertible'))
                continue
            if os.environ.get('MKALL_STUB_UNPORTED') and v and t >= BASE_END and t not in FORCE:
                # (the hero variants) a Veldin function without a port stays a TODO_PORT stub, as in the Veldin ports: the hero batches no-op it on the original's side too
                glue[t] = '{ TODO_PORT(0x%08x, "Veldin function 0x%x without a port")(); set32(v0, 0); }' % (t, v)
                continue
            visit(t, stack + [ad])
        order.append(ad)

    for ad, nm in entries:
        visit(ad, [])
    if nogl:
        print('NO GLUE (hand-write in --glue):', ' '.join('0x%x(V:%x)' % (t, v) for t, v, _ in nogl), file=sys.stderr)
    # generate leaves first
    gl_file = SCR + '/mkall.glue'
    # MKALL_WIDE_GPR=<addr hex>:<reg>[,<reg>..][;...]: a uniform-ABI helper whose argument registers are passed whole (64
    # bits: the helper uses their upper word, e.g. 0x1fa418's sd of t1): its parameters uint64_t, its callers pass the register's .lo
    wide_gpr = {}
    for item in os.environ.get('MKALL_WIDE_GPR', '').split(';'):
        if ':' in item:
            a_, regs_ = item.split(':')
            wide_gpr[int(a_, 16)] = [r_ for r_ in regs_.split(',') if r_]
    def uni_args(ad_):
        return ', '.join('%s.%s' % (r_, 'lo' if r_ in wide_gpr.get(ad_, []) else 'w()') for r_ in ('a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3'))
    names = {}
    gen = []
    wide = {}
    for ad in order:
        nm = entry_names.get(ad, HELPER_FMT % ad)
        names[ad] = nm
    for ad in order:
        nm = names[ad]
        # glue of the functions generated so far
        with open(gl_file, 'w') as gf:
            for t, s in glue.items():
                gf.write('0x%08x: %s\n' % (t, s))
        f = combined(ad)
        jts = jumptables(f)
        cmd = ['python3', TR, f, '--name', nm, '--callees', gl_file] + (['--const-wrap', 'OV/OVF', '--veldin'] if VELDIN else ['--const-wrap', 'L%s_ADDR' % LEVEL])
        if ad in entry_names and not UNIFORM_ENTRY(entry_names[ad]):
            cmd += ['--args', ENTRY_SIG[ad][1], '--ret', ENTRY_SIG[ad][0]]
        else:
            cmd += ['--args', 'a0', '--ret', 'void', '--uniform']
        for lo, hi in LOCALS.get(ad, []):
            cmd += ['--local', '%x:%x' % (lo, hi)]
        if ad in wide_gpr:
            cmd += ['--wide-gpr', ','.join(wide_gpr[ad])]
        for j, tl in jts.items():
            cmd += ['--jt', '%x=%s' % (j, ','.join('%x' % x for x in tl))]
        env = None
        if LVL is not None and ad in dict(ovlist.STANDINS):  # (another region's disc: the disc's own build of a function the
            env = dict(os.environ, MIPS2CPP_HOOKS=standin_hooks(ad))  # region changed: its own hook points, `PAL:` lines)
        r = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if r.returncode != 0:
            print('FAILED', '%x' % ad, r.stderr[-500:], file=sys.stderr)
            continue
        code = r.stdout
        for e in r.stderr.splitlines():  # (a register holding an overlay page on some paths only: resolve by hand)
            if '(hi_flow)' in e:
                print('    ', e, file=sys.stderr)
        nerr = code.count('#error')
        print('%08x %-28s %5d lines, %d errors' % (ad, nm, code.count('\n'), nerr), file=sys.stderr)
        for e in re.findall(r'#error.*', code)[:6]:
            print('    ', e, file=sys.stderr)
        gen.append(code)
        wide[ad] = FMAX < 19 and 'float f19_in' in code.split('{', 1)[0]  # (it takes f16..f19 too: mips2cpp's reads_f16_f19)
        if ad not in entry_names or UNIFORM_ENTRY(entry_names[ad]):
            glue[ad] = '{ const TrRet r = %s(%s, %s%s); set32(v0, r.v0); fr[0] = r.f0; }' % (
                nm, uni_args(ad), FARGS, ''.join(', fr[%d]' % k for k in range(FMAX + 1, 20)) if wide[ad] else '')
    # (Veldin has no variants table of its own: a Veldin-address file includes level 1's, as the files generated so far do)
    head = f'''// {title}
// Transliterated instruction by instruction from {'Veldin' if VELDIN else {'1': 'Novalis', '2': 'Aridia'}.get(LEVEL, 'level ' + LEVEL)}'s own overlay with tools/codegen/mips2cpp.py (tools/codegen/mkall.py:
// the original's control flow and PS2 float rules by construction), reviewed in the oracle ({PROOF}), not rewritten
// by hand: the names are the entries', the helpers `{'ovl_<addr>_tr' if VELDIN else 'nov_<addr>_l1' if LEVEL == '1' else 'lv' + LEVEL + '_<addr>_l' + LEVEL}` (uniform ABI: TrRet).
#include <game/lib/libc.h>
{'#include <game/lib/libgcc_fp.h>' + chr(10) if any('libgcc_' in c for c in gen) else ''}{EXTRA_INC}#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/movie_mode.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l{LEVEL if LEVEL != '0' else '1'}.h>
#include <game/ovl/vu0_regs.h>
#include <game/ovl/walker.h>
{'#include <game/enhance/advanced.h>' + chr(10) if any('advanced_' in c for c in gen) else ''}#include <game/port_stubs.h>

namespace rac1 {{

using namespace eer;
using namespace vu0m;

'''
    decls = []
    for ad in order:
        if ad in entry_names and not UNIFORM_ENTRY(entry_names[ad]):
            rt, ar = ENTRY_SIG[ad]
            decls.append('%s %s(%s);  // %s0x%08x' % ({'void': 'void', 'v0': 'uint32_t', 'f0': 'float'}[rt], names[ad], ', '.join(('float ' if x.startswith('f') else 'uint32_t ') + x for x in ar.split(',')), '' if VELDIN else 'L%s:' % LEVEL, ad))
        else:
            decls.append('TrRet %s(%s, %s%s);  // %s0x%08x' % (
                names[ad], ', '.join(('uint64_t ' if r_ in wide_gpr.get(ad, []) else 'uint32_t ') + r_ for r_ in ('a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3')), FDECL, ''.join(', float f%d' % k for k in range(FMAX + 1, 20)) if wide.get(ad) else '', '' if VELDIN else 'L%s:' % LEVEL, ad))
    open(out_cpp, 'w').write(head + '\n'.join(gen) + '\n}  // namespace rac1\n')
    open(out_cpp + '.decls', 'w').write('\n'.join(decls) + '\n')
    hdr = os.environ.get('MKALL_HEADER')  # the file's own header: the declarations (MKALL_HEADER_NOTE: the comment on top)
    if hdr:
        guard = 'RAC1_GAME_OVL_' + os.path.basename(hdr).upper().replace('.', '_').replace('-', '_')
        note = os.environ.get('MKALL_HEADER_NOTE', title)
        open(hdr, 'w').write('// ' + note.replace('\n', '\n// ') + '\n#ifndef ' + guard + '\n#define ' + guard + '\n\n#include <cstdint>\n\n'
                             '#include <game/ovl/novalis_mobys2.h>\n\nnamespace rac1 {\n\n' + '\n'.join(decls) + '\n\n}  // namespace rac1\n\n#endif\n')
    print('wrote', out_cpp, len(gen), 'functions; declarations in', out_cpp + '.decls', file=sys.stderr)


if __name__ == '__main__':
    main()
