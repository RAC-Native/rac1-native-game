#ifndef RAC1_RUNTIME_EE_PS2_FPU_H
#define RAC1_RUNTIME_EE_PS2_FPU_H
// The EE's float rounding: ADD.S / SUB.S / MUL.S (and ADDA / SUBA / MULA / MADD / MSUB / MADDA /
// MSUBA) round toward zero, not to nearest: the R5900's FPU has no other mode, and PCSX2 runs it
// and the VUs so ("FPU.Roundmode = 3", "VU0.Roundmode = 3", "VU1.Roundmode = 3": chop); DIV.S /
// SQRT.S / RSQRT.S have their own setting, nearest by default ("FPUDiv.Roundmode = 0").
//
// As PCSX2 does, the host runs the game's code with its own FPU in the toward-zero mode
// (enter_ee_rounding() once, on the thread that runs the ports, the interpreter and the VU code):
// every port's float arithmetic, plain C++ or through fpu_add / fpu_sub / fpu_mul, then rounds as
// the PS2 does. A double computation truncated to a float is the float's truncation (a float is
// a double, and truncating twice truncates once). Divisions and square roots of the FPU are done
// to nearest by div_nearest / sqrt_nearest (fpu_div, the interpreter's DIV.S / SQRT.S / RSQRT.S),
// without switching the mode: from the truncated result, the exact value's side of the midpoint.
//
// Found by the Vendor's closing (0x2a5930) takes 0.1 off +0x90 while it is above
// zero; from 1.0999998 (its opening, 0.1 added eleven times toward zero) it ends at -4.2e-7 on
// the PS2, but rounded to nearest the opening stops at 1.0000001 and the closing at -0.1: the
// Vendor orb's scale (+0x90 x 2.5 x the class scale) went negative enough to turn its grid rect
// inside out (x0 > x1) and moby_grid_move (0x250b20) never left its cell loop (Nebula G34,
// Pokitaru).
#include <cfenv>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace rac1::ps2fpu {

// enter_ee_rounding() ran (the thread rounds toward zero)
inline bool g_ee_chop = false;

// The calling thread's float arithmetic rounds toward zero from now on (the EE's mode).
// Also no denormals (the PS2's FPU and VUs have none; PCSX2's "DenormalsAreZero"): the host's
// flush-to-zero and denormals-are-zero modes, so a denormal term is the zero the PS2 adds (toward
// zero, adding an opposite-sign denormal would take one step off the sum).
// Testing switch: RAC1_FPU_NEAREST=1 keeps the host's round-to-nearest (the behaviour before
// for comparison).
inline void enter_ee_rounding() {
    if (const char* e = std::getenv("RAC1_FPU_NEAREST"); e != nullptr && e[0] == '1') return;
    std::fesetround(FE_TOWARDZERO);
#if defined(__SSE__) || defined(__x86_64__)
    _mm_setcsr(_mm_getcsr() | 0x8040u);  // MXCSR FTZ (bit 15) | DAZ (bit 6)
#endif
#if defined(__aarch64__)
    // The FPCR's FZ bit (bit 24) is AArch64's flush-to-zero: a result that would be denormal
    // comes back as zero, and a denormal input is treated as zero too (AArch64 has no separate
    // DAZ bit: FZ covers both). fesetround above has just set the FPCR's rounding bits, so the
    // register is read, modified and written back to preserve them.
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1ull << 24);
    __asm__ volatile("msr fpcr, %0" ::"r"(fpcr));
#endif
    g_ee_chop = true;
}

// The calling thread rounds to nearest while this lives (the mode it had put back after): for the host's text <-> float
// conversions on a thread that runs toward zero. strtof / sscanf and printf follow the rounding mode: chopped, "0.8" reads
// 0.79999995 and "%.3f" prints it back "0.799" - a value read and written again lost one unit of its last digit (the light
// files' "emit" lines drifted so through the debug menu's Save: 0.800 -> 0.795 after five saves).
class NearestScope {
public:
    NearestScope() : prev_(std::fegetround()) {
        if (prev_ != FE_TONEAREST) std::fesetround(FE_TONEAREST);
    }
    ~NearestScope() {
        if (prev_ != FE_TONEAREST) std::fesetround(prev_);
    }
    NearestScope(const NearestScope&) = delete;
    NearestScope& operator=(const NearestScope&) = delete;

private:
    int prev_;
};

// `q`, a quotient or root truncated toward zero, rounded to nearest instead: the next float away
// from zero when the exact value is past their midpoint `m` (`past(m)`: exact in double, a
// midpoint has 25 significant bits), the even one of the two on a tie.
template <typename Past>
inline float nearest_from_chop(float q, Past past) {
    if (q == 0.0f || !std::isfinite(q) || std::fabs(q) == FLT_MAX) return q;
    const float next = std::nextafter(q, q > 0.0f ? HUGE_VALF : -HUGE_VALF);
    const double m = (static_cast<double>(q) + static_cast<double>(next)) * 0.5;
    const int side = past(std::fabs(m));  // 1: the exact value is beyond m, 0: on it, -1: short of it
    if (side > 0) return next;
    if (side < 0) return q;
    uint32_t u;
    std::memcpy(&u, &q, 4);
    return (u & 1u) == 0 ? q : next;
}

// a / b rounded to nearest (DIV.S of two ordinary numbers; the PS2's x / 0 rule is fpu_div's)
inline float div_nearest(float a, float b) {
    const float q = a / b;
    if (!g_ee_chop) return q;
    const double aa = std::fabs(static_cast<double>(a)), ab = std::fabs(static_cast<double>(b));
    return nearest_from_chop(q, [&](double m) { const double mb = m * ab; return aa > mb ? 1 : (aa == mb ? 0 : -1); });
}
// The same on PS2 operands held as doubles (exponent 255 is a number: up to 2^129), narrowed to a
// float (a quotient beyond the float range comes back as +-FLT_MAX: the callers clamp)
inline float div_nearest(double a, double b) {
    const float q = static_cast<float>(a / b);
    if (!g_ee_chop) return q;
    const double aa = std::fabs(a), ab = std::fabs(b);
    return nearest_from_chop(q, [&](double m) { const double mb = m * ab; return aa > mb ? 1 : (aa == mb ? 0 : -1); });
}
// sqrt(a) rounded to nearest (SQRT.S)
inline float sqrt_nearest(float a) {
    const float r = std::sqrt(a);
    if (!g_ee_chop) return r;
    const double da = static_cast<double>(a);
    return nearest_from_chop(r, [&](double m) { const double mm = m * m; return da > mm ? 1 : (da == mm ? 0 : -1); });
}

}  // namespace rac1::ps2fpu

#endif
