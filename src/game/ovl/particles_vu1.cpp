// Ported VU1 microcode: the particle program (see particles_vu1.h), instruction by instruction
// (the micro addresses in the comments). Its state lives in the VU1 registers and memory as on
// the hardware: vf16..vf31, vi8, vi14 and vi15 (the double buffer) are kept from one start to the
// next. Float ops follow the VU rules (operands and results: denormals signed zero, Inf / NaN the
// signed largest float); a DIV's Q is used 7 instructions later, by the vertex it was issued
// for. MAC / status flags are not modelled (the program does not read them).
//
// Input chunk at the double buffer vi15 (qwords): per particle +0 the position relative to the
// camera (w its type: 0 a sprite turned by the angle, 1 a sprite of FPU-turned size, -1 a line,
// -2 a streak; w of the first: the chunk's quadword count), then for a sprite +1 the colour
// words (alpha the fade), +2 (sin, cos) x size / depth and the GS register word, +3 TEX0; a line
// +1 its colour, +2 the other end, +3 that end's colour; a streak +1 the other end, +2 the half
// width across, +3 / +4 the ends' colours, +5 TEX0 and the GS register word. Output from vi15 +
// 0x80: per particle a GIF tag (vf16 sprite, vf17 line, vf18 streak) and its data, the last tag
// made EOP, kicked from vi15 + 0x80.
//
// (host, 2026-10-31) The program's instructions are in particles_vu1_body.inc, compiled twice: with the exact lane rules (`exact`, the
// port as it always ran) and with the same rules decided on the FPU (`fast`, vu1_fast.inc: the Wii U's default, RAC1_VU1_FAST); both
// leave the same registers, memory and kicks, bit for bit (RAC1_VU1_FAST=2 compares them at every start).
#include <cstdio>
#include <game/ovl/particles_vu1.h>

#include <game/ovl/ovl_common.h>
#include <runtime/vu/vu1.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace rac1 {

namespace {

constexpr int X = 1, Y = 2, Z = 4, W = 8, XY = X | Y, XYZ = X | Y | Z, XYZW = 15;

#include <game/ovl/vu1_fast.inc>

namespace exact {

// orig: (helper) a VU float operand / result by the VU rules
float op(const VUVector& v, int l) { return vuc(u2f(v.u[l])); }
void put(VUVector& d, int l, float r) { d.u[l] = f2u(vuc(r)); }
// orig: (helper) FTOI4 of all four lanes (x 16, truncated, saturating)
void ftoi4(VUVector& d) {
    for (int l = 0; l < 4; ++l) {
        const float f = u2f(d.u[l]) * 16.0f;
        const uint32_t u = f2u(f);
        d.u[l] = (u & 0x7f800000u) >= 0x4f000000u ? ((u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu) : static_cast<uint32_t>(static_cast<int32_t>(f));
    }
}
// orig: (helper) DIV Q = vf25.x / w (by zero: the signed largest float)
float divq(const VUState& s, const VUVector& d) {
    const float a = op(s.vf[25], 0), b = op(d, 3);
    if (b == 0.0f) return u2f(((s.vf[25].u[0] ^ d.u[3]) & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu);
    return vuc(a / b);
}

#include <game/ovl/particles_vu1_body.inc>

}  // namespace exact

namespace fast {

// orig: (helper) the same rules on the FPU (vu1_fast.inc)
float op(const VUVector& v, int l) { return fvuc(v.f[l]); }
void put(VUVector& d, int l, float r) { d.f[l] = fvuc(r); }
// orig: (helper) FTOI4: below 2^31 in size the conversion (truncated); 2^31 and past it, infinities and NaNs saturate by the sign
void ftoi4(VUVector& d) {
    for (int l = 0; l < 4; ++l) {
        const float f = d.f[l] * 16.0f;
        if (__builtin_expect(__builtin_fabsf(f) < 2147483648.0f, 1)) d.s[l] = static_cast<int32_t>(f);
        else d.u[l] = (f2u(f) & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu;
    }
}
// orig: (helper) DIV: by zero as the exact one
float divq(const VUState& s, const VUVector& d) {
    const float a = fvuc(s.vf[25].f[0]), b = fvuc(d.f[3]);
    if (__builtin_expect(b != 0.0f, 1)) return fvuc(a / b);
    return exact::divq(s, d);
}

#include <game/ovl/particles_vu1_body.inc>

}  // namespace fast

}  // namespace

// orig: 0x00101088 (exact or fast: RAC1_VU1_FAST, vu1_fast.inc)
void particles_vu1(VUState& s, uint32_t start, const std::function<void(uint32_t)>& kick) {
    switch (vu1_fast_mode()) {
    case 0: exact::program(s, start, kick); return;
    case 2: vu1_fast_check("particles", s, start, kick, exact::program, fast::program); return;
    default: fast::program(s, start, kick); return;
    }
}

// orig: (none) the port's registration with the VU1 backend: the program's 217 instructions at
// micro address 0 hash (fnv1a64) to 0x901435981696d0d3
void particles_vu1_register() {
    vu1::register_program(0, 217, 0x901435981696d0d3ull, &particles_vu1, "particles");
}

}  // namespace rac1
