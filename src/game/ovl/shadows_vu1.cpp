// Ported VU1 microcode: the shadow-mesh program (see shadows_vu1.h), instruction by instruction
// (the micro addresses in the comments; "|" joins an instruction's upper and lower halves, which
// read the registers as before it: no pair here writes what its partner reads). Its state lives
// in the VU1 registers and memory as on the hardware. Float ops follow the VU rules (operands and
// results: denormals signed zero, Inf / NaN the signed largest float). Every MULq / MULAq here
// reads the Q of the last DIV, issued at least 7 instructions before (checked on every path), so
// Q is set at the DIV. The clip flags are read by FCAND / FCOR at least 4 instructions after the
// CLIPs they test. MAC / status flags are not modelled (not read).
//
// Data memory: two input buffers (BASE 0, OFFSET 0x1d2) of a batch: +0 the GIF tag (NLOOP the
// vertex count; a triangle strip), then per vertex ST (q 1), RGBA (integers), XYZ (w 1); 0x3a4..
// 0x3af the start-up's constants (vf18..21 the screen matrix, vf22..25 the clip matrix, 0x3ac the
// clipped fans' GIF tag, w the screen scale k, 0x3ad / 0x3ae vf29 / vf30 the screen scale /
// offset, 0x3af vf7 (fog min, max)); 0x3b3 the clipper's saved vi6 / vi7 / vi15 / Q, 0x3b4 /
// 0x3cd its two polygon buffers, 0x3e6 its GIF packet (the fan's tag, then its vertices).
//
// (host, 2026-10-31) The program's instructions are in shadows_vu1_body.inc, compiled twice: with the exact lane rules (`exact`, the
// port as it always ran) and with the same rules decided on the FPU (`fast`, vu1_fast.inc: the Wii U's default, RAC1_VU1_FAST); both
// leave the same registers, flags, memory and kicks, bit for bit (RAC1_VU1_FAST=2 compares them at every start).
#include <game/ovl/shadows_vu1.h>

#include <game/ovl/ovl_common.h>
#include <runtime/vu/vu1.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace rac1 {

namespace {

constexpr int X = 1, Y = 2, Z = 4, W = 8, XY = X | Y, XYZ = X | Y | Z, ZW = Z | W, XW = X | W, YW = Y | W, YZW = Y | Z | W, XYZW = 15;

#include <game/ovl/vu1_fast.inc>

namespace exact {

// orig: (helper) a VU float operand / result by the VU rules
float op(const VUVector& v, int l) { return vuc(u2f(v.u[l])); }
float opf(float f) { return vuc(f); }
void put(VUVector& d, int l, float r) { d.u[l] = f2u(vuc(r)); }
// orig: (helper) MAXbc / MINIbc: compared as signed integers (both negative: reversed)
uint32_t fmax(uint32_t a, uint32_t b) {
    const int32_t x = static_cast<int32_t>(a), y = static_cast<int32_t>(b);
    return static_cast<uint32_t>((x < 0 && y < 0) ? std::min(x, y) : std::max(x, y));
}
uint32_t fmin(uint32_t a, uint32_t b) {
    const int32_t x = static_cast<int32_t>(a), y = static_cast<int32_t>(b);
    return static_cast<uint32_t>((x < 0 && y < 0) ? std::max(x, y) : std::min(x, y));
}
// orig: (helper) a w lane's max / min with a broadcast lane (b's lane l)
void maxw(VUVector& d, const VUVector& b, int l) { d.u[3] = fmax(d.u[3], b.u[l]); }
void miniw(VUVector& d, const VUVector& b, int l) { d.u[3] = fmin(d.u[3], b.u[l]); }
// orig: (helper) FTOI4 / FTOI0 (truncated, saturating) of all four lanes
void ftoi(VUVector& d, float scale) {
    for (int l = 0; l < 4; ++l) {
        const float f = u2f(d.u[l]) * scale;
        const uint32_t u = f2u(f);
        d.u[l] = (u & 0x7f800000u) >= 0x4f000000u ? ((u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu) : static_cast<uint32_t>(static_cast<int32_t>(f));
    }
}
// orig: (helper) CLIPw.xyz v, v.w: six outside bits shifted into the clip flags
void clipw(VUState& s, const VUVector& v) {
    const uint32_t w = v.u[3];
    const int32_t lim = (w & 0x7f800000u) != 0 ? static_cast<int32_t>(w & 0x7fffffffu) : 0x007fffff;
    uint32_t c = s.clipflag << 6;
    for (int i = 0; i < 3; ++i) {
        if (static_cast<int32_t>(v.u[i]) > lim) c |= 1u << (2 * i);
        if (static_cast<int32_t>(v.u[i] ^ 0x80000000u) > lim) c |= 2u << (2 * i);
    }
    s.clipflag = c & 0xffffff;
}
// orig: (helper) DIV Q, a.la / b.lb (by zero: the signed largest float)
void divq(VUState& s, const VUVector& av, int la, const VUVector& bv, int lb) {
    const uint32_t a = av.u[la], b = bv.u[lb];
    const float x = vuc(u2f(a)), y = vuc(u2f(b));
    if (y == 0.0f) s.q = u2f(((a ^ b) & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu);
    else s.q = vuc(x / y);
}
// orig: (helper) the w lane copied (a result of the ops: a zero or a normal number)
void cpw(VUVector& d, const VUVector& s) { d.u[3] = s.u[3]; }

#include <game/ovl/shadows_vu1_body.inc>

}  // namespace exact

namespace fast {

// orig: (helper) the same rules on the FPU (vu1_fast.inc)
float op(const VUVector& v, int l) { return fvuc(v.f[l]); }
float opf(float f) { return fvuc(f); }
void put(VUVector& d, int l, float r) { d.f[l] = fvuc(r); }
// orig: (helper) MAX / MINI on the bits: for zeros and normal numbers, not both zeros, the bits' order is the values' (both negative
// reversed: the larger value); the rest as the exact one
void maxw(VUVector& d, const VUVector& b, int l) {
    const float x = d.f[3], y = b.f[l];
    if (__builtin_expect(fnorm(x) && fnorm(y) && !(x == 0.0f && y == 0.0f), 1)) {
        if (y > x) d.f[3] = y;
        return;
    }
    exact::maxw(d, b, l);
}
void miniw(VUVector& d, const VUVector& b, int l) {
    const float x = d.f[3], y = b.f[l];
    if (__builtin_expect(fnorm(x) && fnorm(y) && !(x == 0.0f && y == 0.0f), 1)) {
        if (y < x) d.f[3] = y;
        return;
    }
    exact::miniw(d, b, l);
}
// orig: (helper) FTOI: below 2^31 in size the conversion (truncated); 2^31 and past it, infinities and NaNs saturate by the sign
void ftoi(VUVector& d, float scale) {
    for (int l = 0; l < 4; ++l) {
        const float f = d.f[l] * scale;
        if (__builtin_expect(__builtin_fabsf(f) < 2147483648.0f, 1)) d.s[l] = static_cast<int32_t>(f);
        else d.u[l] = (f2u(f) & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu;
    }
}
// orig: (helper) CLIP: for zeros and normal numbers (every lane) the bits' compares are the values' (|x| > |w|; a zero w: any
// non-zero x); the rest as the exact one
void clipw(VUState& s, const VUVector& v) {
    const float x = v.f[0], y = v.f[1], z = v.f[2], w = v.f[3];
    if (__builtin_expect(fnorm(x) && fnorm(y) && fnorm(z) && fnorm(w), 1)) {
        const float aw = __builtin_fabsf(w);
        uint32_t c = s.clipflag << 6;
        if (x > aw) c |= 1u;
        if (-x > aw) c |= 2u;
        if (y > aw) c |= 4u;
        if (-y > aw) c |= 8u;
        if (z > aw) c |= 16u;
        if (-z > aw) c |= 32u;
        s.clipflag = c & 0xffffff;
        return;
    }
    exact::clipw(s, v);
}
// orig: (helper) DIV: by zero as the exact one
void divq(VUState& s, const VUVector& av, int la, const VUVector& bv, int lb) {
    const float x = fvuc(av.f[la]), y = fvuc(bv.f[lb]);
    if (__builtin_expect(y != 0.0f, 1)) {
        s.q = fvuc(x / y);
        return;
    }
    exact::divq(s, av, la, bv, lb);
}
// orig: (helper)
void cpw(VUVector& d, const VUVector& s) { d.f[3] = s.f[3]; }

#include <game/ovl/shadows_vu1_body.inc>

}  // namespace fast

}  // namespace

// orig: 0x0010e818 (exact or fast: RAC1_VU1_FAST, vu1_fast.inc)
void shadows_vu1(VUState& s, uint32_t start, const std::function<void(uint32_t)>& kick) {
    switch (vu1_fast_mode()) {
    case 0: exact::program(s, start, kick); return;
    case 2: vu1_fast_check("shadows (program 7)", s, start, kick, exact::program, fast::program); return;
    default: fast::program(s, start, kick); return;
    }
}

// orig: (none) the port's registration with the VU1 backend: the program's 588 instructions at
// micro address 0 hash (fnv1a64) to 0x3dc739c28ad6ece5
void shadows_vu1_register() {
    vu1::register_program(0, 0x24c, 0x3dc739c28ad6ece5ull, &shadows_vu1, "shadows");
}

}  // namespace rac1
