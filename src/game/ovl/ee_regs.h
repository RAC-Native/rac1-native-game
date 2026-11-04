// The EE's 128-bit registers and the operations hand-written routines run on them, for ports that
// keep the original's instruction order: MMI (parallel) ops, COP2 transfers
// to / from the VU0 (runtime/vu/vu0.h: CTC2 lets a waiting VU0 program go on), memory accesses
// that reach the DMA channels' registers (the scratchpad transfers done at their kick).
#ifndef RAC1_GAME_OVL_EE_REGS_H
#define RAC1_GAME_OVL_EE_REGS_H

#include <game/ovl/ovl_common.h>
#include <runtime/vu/vu0.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace rac1::eer {

// orig: (helper) a 128-bit GPR (lo: the 64-bit register ordinary instructions)
struct Q128 {
    uint64_t lo = 0, hi = 0;
    uint32_t w() const { return static_cast<uint32_t>(lo); }
};
// orig: (helper) a 32-bit result as the register holds it (sign-extended, the high half kept)
RAC1_HOT_INLINE void set32(Q128& d, uint32_t v) { d.lo = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
// orig: (helper) BLTZ / BGEZ
RAC1_HOT_INLINE bool neg(const Q128& r) { return static_cast<int64_t>(r.lo) < 0; }

// orig: (helper) the DMA channel registers (0x10000000..0x1000ffff) the routine writes: a CHCR
// write with STR (0x100) runs the scratchpad transfer (fromSPR 0x1000d000, toSPR 0x1000d400;
// normal or source chain mode) at once and clears STR
// (the scratchpad channels' registers 0x1000d000..0x1000d4ff, which the culling routines write for every transfer, in
// an array; any other register in a map, as before: an unknown register reads 0)
struct HwRegs {
    std::array<uint32_t, 0x500 / 4> spr{};
    std::unordered_map<uint32_t, uint32_t> other;
    uint32_t& operator[](uint32_t a) {
        if (a - 0x1000d000u < 0x500u && (a & 3u) == 0) return spr[(a - 0x1000d000u) >> 2];
        return other[a];
    }
};
inline HwRegs& hwregs() {
    static HwRegs r;
    return r;
}
RAC1_HOT_INLINE bool is_hw(uint32_t a) { return (a >> 16) == 0x1000u || (a >> 16) == 0xb000u; }  // (kseg0 / kseg1)
RAC1_COLD_PATH inline void hw_sw(uint32_t a, uint32_t v) {
    a &= 0x1fffffffu;
    hwregs()[a] = v;
    if ((a == 0x1000d000u || a == 0x1000d400u) && (v & 0x100u) != 0) {
        auto& r = hwregs();
        const uint32_t madr = r[a + 0x10], qwc = r[a + 0x20], sadr = r[a + 0x80];
        const uint32_t mode = (v >> 2) & 3;
        if (a == 0x1000d400u) {
            if (mode == 1) spr_dma_to_chain(sadr, r[a + 0x30], v);
            else spr_dma_to(sadr, madr, qwc);
        } else {
            spr_dma_from(madr, sadr, qwc);
        }
        r[a] = v & ~0x100u;
    }
}
RAC1_COLD_PATH inline uint32_t hw_lw(uint32_t a) { return hwregs()[a & 0x1fffffffu]; }

// orig: (helper) loads / stores (a hardware register address goes to the registers above)
RAC1_HOT_INLINE uint32_t lw_(uint32_t a) { return is_hw(a) ? hw_lw(a) : lwu(a); }
RAC1_HOT_INLINE uint32_t lbu_(uint32_t a) { return lbu(a); }
RAC1_HOT_INLINE uint32_t lhu_(uint32_t a) { return lhu(a); }
RAC1_HOT_INLINE uint64_t ld_(uint32_t a) { return ld(a); }
RAC1_HOT_INLINE void sw_(uint32_t a, uint32_t v) {
    if (is_hw(a)) hw_sw(a, v);
    else sw(a, v);
}
RAC1_HOT_INLINE void sh_(uint32_t a, uint32_t v) { sh(a, v); }
RAC1_HOT_INLINE void sb_(uint32_t a, uint32_t v) { sb(a, v); }
RAC1_HOT_INLINE void sd_(uint32_t a, uint64_t v) { sd(a, v); }
RAC1_HOT_INLINE Q128 lq(uint32_t a) {
    Q128 r;
    r.lo = ld(a & ~15u);
    r.hi = ld((a & ~15u) + 8);
    return r;
}
RAC1_HOT_INLINE void sq(uint32_t a, const Q128& v) {
    sd(a & ~15u, v.lo);
    sd((a & ~15u) + 8, v.hi);
}
// orig: (helper) SDL / SDR (little-endian: the doubleword's high / low part at an unaligned address)
inline void sdl(uint32_t a, const Q128& r) {
    const uint32_t s = (a & 7) * 8;
    uint64_t m = ld(a & ~7u);
    m = (s == 56) ? r.lo : ((m & ~(~0ull >> (56 - s))) | (r.lo >> (56 - s)));
    sd(a & ~7u, m);
}
inline void sdr(uint32_t a, const Q128& r) {
    const uint32_t s = (a & 7) * 8;
    uint64_t m = ld(a & ~7u);
    m = (s == 0) ? r.lo : ((m & ((1ull << s) - 1)) | (r.lo << s));
    sd(a & ~7u, m);
}

// orig: (helper) LDL / LDR (little-endian): the bytes up to / from an unaligned address of its doubleword into the
// high / low part of the register, the rest kept
inline void ldl_(Q128& r, uint32_t a) {
    const uint32_t k = (a & 7) + 1;
    const uint64_t m = ld(a & ~7u) << ((8 - k) * 8);
    const uint64_t keep = k == 8 ? 0 : (~0ull >> (k * 8));
    r.lo = (r.lo & keep) | m;
}
inline void ldr_(Q128& r, uint32_t a) {
    const uint32_t n = 8 - (a & 7);
    const uint64_t m = ld(a & ~7u) >> ((a & 7) * 8);
    const uint64_t keep = n == 8 ? 0 : (~0ull << (n * 8));
    r.lo = (r.lo & keep) | m;
}
// orig: (helper) the MMI lanes
RAC1_HOT_INLINE uint32_t wd(const Q128& v, int i) { return static_cast<uint32_t>((i < 2 ? v.lo : v.hi) >> ((i & 1) * 32)); }
RAC1_HOT_INLINE uint16_t hw(const Q128& v, int i) { return static_cast<uint16_t>((i < 4 ? v.lo : v.hi) >> ((i & 3) * 16)); }
RAC1_HOT_INLINE uint8_t by(const Q128& v, int i) { return static_cast<uint8_t>((i < 8 ? v.lo : v.hi) >> ((i & 7) * 8)); }
RAC1_HOT_INLINE Q128 mkw(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    Q128 r;
    r.lo = (static_cast<uint64_t>(b) << 32) | a;
    r.hi = (static_cast<uint64_t>(d) << 32) | c;
    return r;
}
inline Q128 mkh(const uint16_t h[8]) {
    Q128 r;
    for (int i = 0; i < 8; ++i) (i < 4 ? r.lo : r.hi) |= static_cast<uint64_t>(h[i]) << ((i & 3) * 16);
    return r;
}
inline Q128 mkb(const uint8_t b[16]) {
    Q128 r;
    for (int i = 0; i < 16; ++i) (i < 8 ? r.lo : r.hi) |= static_cast<uint64_t>(b[i]) << ((i & 7) * 8);
    return r;
}
// orig: (helper) PEXTLB / PEXTUB: the low / high 8 bytes of t and s interleaved
inline Q128 pextlb(const Q128& s, const Q128& t) {
    uint8_t b[16];
    for (int i = 0; i < 8; ++i) b[2 * i] = by(t, i), b[2 * i + 1] = by(s, i);
    return mkb(b);
}
inline Q128 pextub(const Q128& s, const Q128& t) {
    uint8_t b[16];
    for (int i = 0; i < 8; ++i) b[2 * i] = by(t, i + 8), b[2 * i + 1] = by(s, i + 8);
    return mkb(b);
}
// orig: (helper) PEXTLH / PEXTUH: the low / high 4 halfwords interleaved
inline Q128 pextlh(const Q128& s, const Q128& t) {
    uint16_t h[8];
    for (int i = 0; i < 4; ++i) h[2 * i] = hw(t, i), h[2 * i + 1] = hw(s, i);
    return mkh(h);
}
inline Q128 pextuh(const Q128& s, const Q128& t) {
    uint16_t h[8];
    for (int i = 0; i < 4; ++i) h[2 * i] = hw(t, i + 4), h[2 * i + 1] = hw(s, i + 4);
    return mkh(h);
}
// orig: (helper) PEXTLW
RAC1_HOT_INLINE Q128 pextlw(const Q128& s, const Q128& t) { return mkw(wd(t, 0), wd(s, 0), wd(t, 1), wd(s, 1)); }
// orig: (helper) PPACW: the even words of t, then of s
inline Q128 ppacw(const Q128& s, const Q128& t) { return mkw(wd(t, 0), wd(t, 2), wd(s, 0), wd(s, 2)); }
// orig: (helper) PPACH / PPACB: the even halfwords / bytes of t, then of s
inline Q128 ppach(const Q128& s, const Q128& t) {
    uint16_t h[8];
    for (int i = 0; i < 4; ++i) h[i] = hw(t, 2 * i), h[i + 4] = hw(s, 2 * i);
    return mkh(h);
}
inline Q128 ppacb(const Q128& s, const Q128& t) {
    uint8_t b[16];
    for (int i = 0; i < 8; ++i) b[i] = by(t, 2 * i), b[i + 8] = by(s, 2 * i);
    return mkb(b);
}
// orig: (helper) PEXT5: each word's low 1:5:5:5 halfword spread to 8:8:8:8 (R<<3, G<<11, B<<19, A<<31)
inline Q128 pext5(const Q128& t) {
    uint32_t r[4];
    for (int i = 0; i < 4; ++i) {
        const uint32_t w = wd(t, i);
        r[i] = ((w & 0x1fu) << 3) | ((w & 0x3e0u) << 6) | ((w & 0x7c00u) << 9) | ((w & 0x8000u) << 16);
    }
    return mkw(r[0], r[1], r[2], r[3]);
}
// orig: (helper) PADDUW: the four words added, unsigned saturating
inline Q128 padduw(const Q128& s, const Q128& t) {
    uint32_t r[4];
    for (int i = 0; i < 4; ++i) {
        const uint64_t v = static_cast<uint64_t>(wd(s, i)) + wd(t, i);
        r[i] = v > 0xffffffffull ? 0xffffffffu : static_cast<uint32_t>(v);
    }
    return mkw(r[0], r[1], r[2], r[3]);
}
// orig: (helper) PMAXW
RAC1_HOT_INLINE Q128 pmaxw(const Q128& s, const Q128& t) {
    uint32_t r[4];
    for (int i = 0; i < 4; ++i) {
        const int32_t a = static_cast<int32_t>(wd(s, i)), b = static_cast<int32_t>(wd(t, i));
        r[i] = static_cast<uint32_t>(a > b ? a : b);
    }
    return mkw(r[0], r[1], r[2], r[3]);
}
// orig: (helper) PMINW
RAC1_HOT_INLINE Q128 pminw(const Q128& s, const Q128& t) {
    uint32_t r[4];
    for (int i = 0; i < 4; ++i) {
        const int32_t a = static_cast<int32_t>(wd(s, i)), b = static_cast<int32_t>(wd(t, i));
        r[i] = static_cast<uint32_t>(a < b ? a : b);
    }
    return mkw(r[0], r[1], r[2], r[3]);
}
// orig: (helper) PROT3W: the three low words rotated (w1, w2, w0), w3 kept
RAC1_HOT_INLINE Q128 prot3w(const Q128& t) { return mkw(wd(t, 1), wd(t, 2), wd(t, 0), wd(t, 3)); }
// orig: (helper) PCPYLD / PCPYUD / PCPYH / POR
RAC1_HOT_INLINE Q128 pcpyld(const Q128& s, const Q128& t) {
    Q128 r;
    r.lo = t.lo;
    r.hi = s.lo;
    return r;
}
RAC1_HOT_INLINE Q128 pcpyud(const Q128& s, const Q128& t) {
    Q128 r;
    r.lo = s.hi;
    r.hi = t.hi;
    return r;
}
inline Q128 pcpyh(const Q128& t) {
    uint16_t h[8];
    for (int i = 0; i < 4; ++i) h[i] = hw(t, 0), h[i + 4] = hw(t, 4);
    return mkh(h);
}
RAC1_HOT_INLINE Q128 por(const Q128& s, const Q128& t) {
    Q128 r;
    r.lo = s.lo | t.lo;
    r.hi = s.hi | t.hi;
    return r;
}
// orig: (helper) PSLLW / PSRLW / PSRAW / PSRAH
RAC1_HOT_INLINE Q128 psllw(const Q128& t, int n) { return mkw(wd(t, 0) << n, wd(t, 1) << n, wd(t, 2) << n, wd(t, 3) << n); }
RAC1_HOT_INLINE Q128 psrlw(const Q128& t, int n) { return mkw(wd(t, 0) >> n, wd(t, 1) >> n, wd(t, 2) >> n, wd(t, 3) >> n); }
RAC1_HOT_INLINE Q128 psraw(const Q128& t, int n) {
    uint32_t r[4];
    for (int i = 0; i < 4; ++i) r[i] = static_cast<uint32_t>(static_cast<int32_t>(wd(t, i)) >> n);
    return mkw(r[0], r[1], r[2], r[3]);
}
inline Q128 psrah(const Q128& t, int n) {
    uint16_t h[8];
    for (int i = 0; i < 8; ++i) h[i] = static_cast<uint16_t>(static_cast<int16_t>(hw(t, i)) >> n);
    return mkh(h);
}
// orig: (helper) CVT.W.S: truncated toward zero, saturating
inline float cvt_w(float f) {
    const uint32_t u = f2u(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) return u2f((u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu);
    return u2f(static_cast<uint32_t>(static_cast<int32_t>(f)));
}
// orig: (helper) PEXCH / PEXTUW
inline Q128 pexch(const Q128& t) {
    const uint16_t h[8] = {hw(t, 0), hw(t, 2), hw(t, 1), hw(t, 3), hw(t, 4), hw(t, 6), hw(t, 5), hw(t, 7)};
    return mkh(h);
}
inline Q128 pextuw(const Q128& s, const Q128& t) { return mkw(wd(t, 2), wd(s, 2), wd(t, 3), wd(s, 3)); }
// orig: (helper) MULT rd, rs, rt (the R5900's three-operand form): LO / HI and rd
inline void mult3(Q128& d, const Q128& s, const Q128& t, Q128& lo, Q128& hi) {
    const int64_t p = static_cast<int64_t>(static_cast<int32_t>(s.w())) * static_cast<int32_t>(t.w());
    set32(lo, static_cast<uint32_t>(p));
    set32(hi, static_cast<uint32_t>(static_cast<uint64_t>(p) >> 32));
    d.lo = lo.lo;
}
// orig: (helper) DIV / DIVU rs, rt: LO the quotient, HI the remainder (x / 0 and INT_MIN / -1 as the R5900); MULTU rd, rs, rt
inline void ee_div3(const Q128& s, const Q128& t, Q128& lo, Q128& hi) {
    set32(lo, static_cast<uint32_t>(ee_div(static_cast<int32_t>(s.w()), static_cast<int32_t>(t.w()))));
    set32(hi, static_cast<uint32_t>(ee_rem(static_cast<int32_t>(s.w()), static_cast<int32_t>(t.w()))));
}
inline void ee_divu3(const Q128& s, const Q128& t, Q128& lo, Q128& hi) {
    set32(lo, t.w() == 0 ? 0xffffffffu : s.w() / t.w());
    set32(hi, t.w() == 0 ? s.w() : s.w() % t.w());
}
inline void ee_multu3(Q128& d, const Q128& s, const Q128& t, Q128& lo, Q128& hi) {
    const uint64_t p = static_cast<uint64_t>(s.w()) * t.w();
    set32(lo, static_cast<uint32_t>(p));
    set32(hi, static_cast<uint32_t>(p >> 32));
    d.lo = lo.lo;
}
// orig: (helper) BREAK: the original traps (a divide-by-zero check, never reached in the game's data); logged
inline void ee_break(uint32_t code) { std::fprintf(stderr, "[ee] BREAK %u reached\n", code); }
// orig: (helper) SQRT.S: the square root of the magnitude (a negative operand sets a flag, no trap)
inline float fpu_sqrt(float f) { return fpu_out(std::sqrt(std::fabs(fpu_in(f)))); }
// orig: (helper) MADD rd, rs, rt (the R5900's three-operand form): (HI:LO) += rs * rt (signed), LO / HI sign-extended words, rd = LO
inline void ee_madd3(Q128& d, const Q128& s, const Q128& t, Q128& lo, Q128& hi) {
    const int64_t acc = static_cast<int64_t>((static_cast<uint64_t>(hi.w()) << 32) | lo.w()) + static_cast<int64_t>(static_cast<int32_t>(s.w())) * static_cast<int32_t>(t.w());
    set32(lo, static_cast<uint32_t>(acc));
    set32(hi, static_cast<uint32_t>(static_cast<uint64_t>(acc) >> 32));
    d.lo = lo.lo;
}
// orig: (helper) LDL / LDR (little-endian): the doubleword's bytes up to / from the address into the register's high / low part
inline void ldl_(uint32_t a, Q128& r) {
    const uint32_t s = (a & 7) * 8;
    const uint64_t m = ld(a & ~7u);
    const uint64_t part = s == 56 ? m : (m & ((1ull << (s + 8)) - 1));
    r.lo = s == 56 ? m : ((r.lo & ((1ull << (56 - s)) - 1)) | (part << (56 - s)));
}
inline void ldr_(uint32_t a, Q128& r) {
    const uint32_t s = (a & 7) * 8;
    const uint64_t m = ld(a & ~7u);
    r.lo = s == 0 ? m : ((r.lo & ~(~0ull >> s)) | (m >> s));
}
// orig: (helper) PMULTH: the eight halfword products; rd the even ones, LO / HI all of them
inline Q128 pmulth(const Q128& s, const Q128& t, Q128& lo, Q128& hi) {
    uint32_t p[8];
    for (int i = 0; i < 8; ++i)
        p[i] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(hw(s, i))) * static_cast<int32_t>(static_cast<int16_t>(hw(t, i))));
    lo = mkw(p[0], p[1], p[4], p[5]);
    hi = mkw(p[2], p[3], p[6], p[7]);
    return mkw(p[0], p[2], p[4], p[6]);
}
// orig: (helper) PMFHL.SH: LO / HI words saturated to halfwords, in order
inline Q128 pmfhl_sh(const Q128& lo, const Q128& hi) {
    auto s16 = [](uint32_t v) {
        const int32_t x = static_cast<int32_t>(v);
        return static_cast<uint16_t>(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
    };
    const uint16_t h[8] = {s16(wd(lo, 0)), s16(wd(lo, 1)), s16(wd(hi, 0)), s16(wd(hi, 1)), s16(wd(lo, 2)), s16(wd(lo, 3)), s16(wd(hi, 2)), s16(wd(hi, 3))};
    return mkh(h);
}

// orig: (host) debugging of the transliterations: build with -DRAC1_ASM_TRACE and set ASM_TRACE=1: each label (a block's start) printed
#ifdef RAC1_ASM_TRACE
inline void asm_trace(uint32_t a) {
    static const bool on = std::getenv("ASM_TRACE") != nullptr;
    if (on) std::printf("  [asm %08x]\n", a);
}
#define ASM_TRACE(a) ::rac1::eer::asm_trace(a)
#else
#define ASM_TRACE(a) ((void)0)
#endif

// orig: (helper) a macro-mode VU0 op waits for a running VU0 program to end
RAC1_HOT_INLINE void vmacro() {
    if (__builtin_expect(vu0::running(), 0)) vu0::drain();
}
// orig: (helper) COP2 transfers on the VU0 registers
RAC1_HOT_INLINE Q128 qmfc2(int r) {
    const VUVector& v = vu0::global().st.vf[static_cast<size_t>(r)];
    return mkw(v.u[0], v.u[1], v.u[2], v.u[3]);
}
RAC1_HOT_INLINE void qmtc2(int r, const Q128& x) {
    if (r == 0) return;
    VUVector& v = vu0::global().st.vf[static_cast<size_t>(r)];
    for (int i = 0; i < 4; ++i) v.u[i] = wd(x, i);
}
// orig: (helper) CTC2 (vi0..vi15, CMSAR0 27): a VU0 program waiting on the EE goes on
inline void ctc2(int r, const Q128& x) {
    vu0::Vu0& v = vu0::global();
    if (r == 27) v.cmsar0 = x.w() & 0xffff;
    else if (r > 0 && r < 16) v.st.vi[static_cast<size_t>(r)] = static_cast<int16_t>(x.w() & 0xffff);
    if (vu0::running()) vu0::run_until_blocked();
}
RAC1_HOT_INLINE uint32_t cfc2(int r) {
    vu0::Vu0& v = vu0::global();
    if (r == 27) return v.cmsar0;
    if (r < 16) return static_cast<uint32_t>(static_cast<int32_t>(v.st.vi[static_cast<size_t>(r)]));
    switch (r) {
    case 16: return v.st.statusflag;
    case 17: return v.st.macflag;
    case 18: return v.st.clipflag;
    case 20: return f2u(v.st.r);
    case 21: return f2u(v.st.i);
    case 22: return f2u(v.st.q);
    case 23: return f2u(v.st.p);
    default: return 0;
    }
}
// orig: (helper) LQC2 / SQC2
RAC1_HOT_INLINE void lqc2_(int r, uint32_t a) { qmtc2(r, lq(a)); }
RAC1_HOT_INLINE void sqc2_(uint32_t a, int r) { sq(a, qmfc2(r)); }
// orig: (helper) VSQI (macro): a VU0 register to its data memory at vi[it], vi[it] + 1
inline void vsqi(int fs, int it, int dest) {
    vu0::drain();
    vu0::Vu0& v = vu0::global();
    const uint32_t a = (static_cast<uint32_t>(static_cast<uint16_t>(v.st.vi[static_cast<size_t>(it)])) * 16) & 0x3fff;
    for (int i = 0; i < 4; ++i)
        if ((dest >> (3 - i) & 1) != 0) std::memcpy(&v.st.mem[a + 4 * static_cast<uint32_t>(i)], &v.st.vf[static_cast<size_t>(fs)].u[i], 4);
    if (it != 0) v.st.vi[static_cast<size_t>(it)] = static_cast<int16_t>(v.st.vi[static_cast<size_t>(it)] + 1);
}

}  // namespace rac1::eer

#endif
