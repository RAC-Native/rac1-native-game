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
#include <atomic>
#include <cfenv>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace rac1::ps2fpu {

// enter_ee_rounding() ran (the thread rounds toward zero)
inline bool g_ee_chop = false;

// The helpers' raw forms (docs/performance.md, "The float helpers' raw forms"). In the thread's mode below an add, a subtract or
// a multiply of two floats already is the PS2's: a denormal operand reads as the signed zero (DAZ), a denormal result comes
// back as the signed zero (FTZ), a result beyond the range as the signed largest float (toward zero). Only an operand with
// exponent 255 differs - a number on the PS2, an infinity or a NaN on the host - and the host's result then has exponent 255
// too, whatever the other operand. So the EE FPU helpers (ovl_common.h's fpu_add / fpu_sub / fpu_mul) and the VU0 macro ops
// (vu0_macro.h, vu0_regs.h, vu0_nf.h, vu0_local.h) do the plain operation, and take their rule-by-rule form only when a result
// they would write has exponent 255. 0: the rule-by-rule forms always (as before; and whenever the thread does not chop:
// RAC1_FPU_NEAREST); 1: the raw forms; 2: the rule-by-rule forms, each compared with what the raw form would give (the
// account at the end of the run: raw_report).
inline int g_fpu_raw = 0;  // RAC1_PERF_FPU_RAW (set by enter_ee_rounding)
inline int g_vu0_raw = 0;  // RAC1_PERF_VU0_RAW

// Whether the calling thread's float mode is the EE's (toward zero, no denormals): the raw forms hold only there.
inline bool thread_chops() {
#if defined(__SSE__) || defined(__x86_64__)
    return (_mm_getcsr() & 0xe040u) == 0xe040u;  // RC = toward zero, FTZ, DAZ
#elif defined(__aarch64__)
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    return (fpcr & (7ull << 22)) == (7ull << 22);  // RMode = toward zero, FZ
#else
    return std::fegetround() == FE_TOWARDZERO;
#endif
}

// The =2 modes' account (one for the FPU helpers, one for the VU0 ops): the ops compared, those whose raw form would have
// differed, those the raw form hands to the rules (a result with exponent 255), those run on a thread that does not chop.
struct RawCheck {
    std::atomic<uint64_t> compared{0}, differing{0}, by_rules{0}, off_thread{0};
    uint32_t note[4][8] = {};  // the first differences: kind, lanes, the operands' words
    std::atomic<int> notes{0};
    void differ(uint32_t kind, uint32_t lanes, uint32_t x, uint32_t y, uint32_t c, uint32_t raw, uint32_t rules) {
        differing.fetch_add(1, std::memory_order_relaxed);
        const int n = notes.fetch_add(1, std::memory_order_relaxed);
        if (n < 4) {
            const uint32_t v[8] = {kind, lanes, x, y, c, raw, rules, 0};
            std::memcpy(note[n], v, sizeof(v));
        }
    }
    void report(const char* what) const {
        std::printf("[perf] %s check: %llu ops compared, %llu differing; %llu left to the rules (exponent 255), %llu on a thread that does not chop\n", what,
                    static_cast<unsigned long long>(compared.load()), static_cast<unsigned long long>(differing.load()),
                    static_cast<unsigned long long>(by_rules.load()), static_cast<unsigned long long>(off_thread.load()));
        for (int i = 0; i < notes.load() && i < 4; ++i)
            std::printf("[perf] %s check: op %u lanes %x: %08x, %08x, %08x: raw %08x, rules %08x\n", what, note[i][0], note[i][1], note[i][2], note[i][3], note[i][4],
                        note[i][5], note[i][6]);
    }
};
inline RawCheck g_fpu_raw_check, g_vu0_raw_check;
// The account's lines, when a =2 mode ran (the end of a run; the oracle's end).
inline void raw_report() {
    if (g_fpu_raw == 2) g_fpu_raw_check.report("fpu raw");
    if (g_vu0_raw == 2) g_vu0_raw_check.report("vu0 raw");
}

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
    // (the raw forms: only on a thread that chops; on by default)
    const auto mode = [](const char* name) {
        const char* e = std::getenv(name);
        return e == nullptr || *e == '\0' ? 1 : std::atoi(e);
    };
    g_fpu_raw = mode("RAC1_PERF_FPU_RAW");
    g_vu0_raw = mode("RAC1_PERF_VU0_RAW");
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
