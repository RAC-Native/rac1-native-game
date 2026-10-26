// VU0 macro-mode operations as the hand-written overlay routines use them (COP2 from the EE),
// computed with the VU float rules (operands and results: denormals signed zero, Inf / NaN the
// signed largest float), on a register held as four lanes.
#ifndef RAC1_GAME_OVL_VU0_MACRO_H
#define RAC1_GAME_OVL_VU0_MACRO_H

#include <game/ovl/ovl_common.h>
#include <runtime/vu/vu0.h>

#include <climits>
#include <cmath>
#include <cstdint>

namespace rac1 {
RAC1_EE_WORDS_ONLY(VUVector);  // (four 32-bit lanes: laid out as EE memory)
}  // namespace rac1

namespace rac1::vu0m {

// orig: (helper) a VF register's four lanes (the hardware register's type)
using Vf = VUVector;
// orig: (helper) LQC2 / SQC2
inline Vf lqc2(uint32_t a) {
    if (__builtin_expect((a & 0xfffu) <= 0xff0u, 1)) return ee_read<Vf>(a);
    Vf r;
    for (int i = 0; i < 4; ++i) r.v[i] = lwc1(a + 4 * static_cast<uint32_t>(i));
    return r;
}
// orig: (helper) SQC2
inline void sqc2(uint32_t a, const Vf& r) {
    if (__builtin_expect((a & 0xfffu) <= 0xff0u, 1)) {
        ee_write<Vf>(a, r);
        return;
    }
    for (int i = 0; i < 4; ++i) swc1(a + 4 * static_cast<uint32_t>(i), r.v[i]);
}
// orig: (helper) the lane ops: a op b by the VU rules
inline float mul(float a, float b) { return vuc(vuc(a) * vuc(b)); }
// orig: (helper) VADD lane
inline float add(float a, float b) { return vuc(vuc(a) + vuc(b)); }
// orig: (helper) VSUB lane
inline float sub(float a, float b) { return vuc(vuc(a) - vuc(b)); }
// orig: (helper) VMADD lane: acc + a * b
inline float madd(float acc, float a, float b) { return vuc(vuc(acc) + vuc(a) * vuc(b)); }
// orig: (helper) VMSUB lane: acc - a * b
inline float msub(float acc, float a, float b) { return vuc(vuc(acc) - vuc(a) * vuc(b)); }
// orig: (helper) VFTOIn: x 2^n truncated, saturating (the operand not clamped)
inline uint32_t ftoi(float f, float scale) {
    f *= scale;
    const uint32_t u = f2u(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) return (u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu;
    return static_cast<uint32_t>(static_cast<int32_t>(f));
}
// orig: (helper) a lane holding an integer (VFTOI results) as its bits
inline float bits(uint32_t u) { return u2f(u); }
// orig: (helper) VCLIPW.xyz: the six outside bits of p against |w| shifted into the clip flags
inline void clipw(uint32_t& clip, const Vf& p, float w) {
    const uint32_t wu = f2u(w);
    const int32_t lim = (wu & 0x7f800000u) != 0 ? static_cast<int32_t>(wu & 0x7fffffffu) : 0x007fffff;
    clip <<= 6;
    for (int i = 0; i < 3; ++i) {
        const uint32_t u = f2u(p.v[i]);
        if (static_cast<int32_t>(u) > lim) clip |= 1u << (2 * i);
        if (static_cast<int32_t>(u ^ 0x80000000u) > lim) clip |= 2u << (2 * i);
    }
    clip &= 0xffffff;
}
// orig: (helper) VDIV Q = s / t (by zero: the signed largest float)
inline float div(float s, float t) {
    const float a = vuc(s), b = vuc(t);
    if (b == 0.0f) return u2f(((f2u(s) ^ f2u(t)) & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu);
    return vuc(a / b);
}
// orig: (helper) a row-vector transform: r0 x + r1 y + r2 z + r3 w (VMULAx / VMADDAy / VMADDAz / VMADDw)
inline Vf transform(const Vf r[4], float x, float y, float z, float w) {
    Vf o;
    for (int i = 0; i < 4; ++i) o.v[i] = madd(madd(madd(mul(r[0].v[i], x), r[1].v[i], y), r[2].v[i], z), r[3].v[i], w);
    return o;
}

// orig: (host) the VU0 registers -- the hardware's (runtime/vu/vu0.h), shared with its micro
// programs: a routine reading a lane it did not write sees the last writer's value
using Regs = VUState;
inline Regs& regs() { return vu0::global().st; }

}  // namespace rac1::vu0m

#endif
