// Scalar-equivalence test for the NEON port of the VU fast-path SIMD helpers
// (runtime/vu/vu_native_fast.h, src/game/ovl/vu0_regs.h).
//
// For every SIMD function in the two headers, this compares the header's REAL
// implementation against an independent plain-scalar reference over randomized
// inputs (deterministic seed), bit-exact. The heavy dependency layers
// (vu_native.h, vu0_macro.h) are stubbed by pre-defining their include guards;
// the SIMD function bodies under test are the real ones from the headers.
//
// Why bit-exact (no tolerance): every ported op is a lane-wise single IEEE-754
// operation (add/sub/mul, round-to-nearest, no FMA) or an integer bit op, so
// the SIMD and scalar paths must agree bitwise. NaN payloads are excluded from
// the *arithmetic* op inputs only (x86 SSE and ARM NEON may propagate different
// NaN payloads through a binary op); cop/clamp/flag tests use the full pool
// including Inf/NaN/denormals because those paths are pure bit logic.
//
// FP environment assumption: default (round-to-nearest, no flush-to-zero),
// which the VU float rules require anyway (denormals are classified, not flushed).
//
// Build and run (x86_64: validates the untouched SSE path against scalar):
//   g++ -std=c++17 -O2 -ffp-contract=off -I<repo> -I<repo>/src tests/neon/vu_neon_equiv.cpp -o /tmp/vu_equiv_x86 && /tmp/vu_equiv_x86
// Build and run (aarch64: validates the NEON path against scalar):
//   aarch64-linux-gnu-g++ -std=c++17 -O2 -ffp-contract=off -I<repo> -I<repo>/src tests/neon/vu_neon_equiv.cpp -o /tmp/vu_equiv_neon && qemu-aarch64 /tmp/vu_equiv_neon
//
// Exit code 0: all functions bit-identical. Non-zero: count of failed checks.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Stub layer: faithful minimal stand-ins for the heavy dependency headers.
// The SIMD bodies under test come from the real headers below.
// ---------------------------------------------------------------------------

namespace rac1 {
struct alignas(16) VUVector {
    float v[4];
};
}  // namespace rac1

#define RAC1_RUNTIME_VU_VU_NATIVE_H  // stub out vu_native.h's heavy includes
namespace rac1::vun {
#define VUN_INLINE inline __attribute__((always_inline))
struct FlagRegs {
    uint32_t macflag = 0, statusflag = 0, clipflag = 0;
    float q = 0.0f;
};
VUN_INLINE uint32_t u_of(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
VUN_INLINE float f_of(uint32_t u) {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
// vu_native.h's operand rule, copied verbatim in scalar form.
VUN_INLINE float cop(uint32_t u) {
    const uint32_t e = u & 0x7f800000u;
    if (e == 0)
        u &= 0x80000000u;
    else if (e == 0x7f800000u)
        u = (u & 0x80000000u) | 0x7f7fffffu;
    return f_of(u);
}
VUN_INLINE float copf(float f) { return cop(u_of(f)); }
}  // namespace rac1::vun

#define RAC1_GAME_OVL_VU0_MACRO_H  // stub out vu0_macro.h's heavy includes
namespace rac1::vu0m {
using Vf = ::rac1::VUVector;
struct Regs {
    uint32_t macflag = 0, statusflag = 0, clipflag = 0;
    std::array<Vf, 32> vf{};
    Vf acc{};
    float q = 0.0f;
};
inline Regs& regs() {
    static Regs r;
    return r;
}
inline float u2f(uint32_t u) {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
inline uint32_t f2u(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
// ovl_common.h's vuc, copied verbatim in scalar form.
inline float vuc(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    const uint32_t m = u & 0x7fffffffu;
    if (__builtin_expect(m >= 0x7f800000u || (m - 1u < 0x007fffffu), 0)) {
        u = (u & 0x80000000u) | (m >= 0x7f800000u ? 0x7f7fffffu : 0u);
        std::memcpy(&f, &u, 4);
    }
    return f;
}
inline float bits(uint32_t u) { return u2f(u); }
// Declared for the header to compile; never called by this test (abort if hit).
inline float ftoi(float, float) { std::abort(); }
inline void clipw(uint32_t&, const Vf&, float) { std::abort(); }
}  // namespace rac1::vu0m

// The real SIMD function bodies under test.
#include <runtime/vu/vu_native_fast.h>
#include <game/ovl/vu0_regs.h>

// ---------------------------------------------------------------------------
// Deterministic PRNG and input pools.
// ---------------------------------------------------------------------------

static uint64_t rng_state = 0x12345678abcdef01ull;
static uint64_t rnd64() {
    uint64_t x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 0x2545f4914f6cdd1dull;
}
static uint32_t rnd32() { return static_cast<uint32_t>(rnd64() >> 32); }

// Full bit pool: zeros, denormals, normals, inf, NaN payloads.
static const uint32_t kSpecials[] = {
    0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007fffffu,
    0x00800000u, 0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u,
    0x7fc00000u, 0xffc00000u, 0x7fc00001u, 0x3f800000u, 0xbf800000u,
    0x42c80000u, 0x3dccccedu,
};
static uint32_t rnd_bits_full() {
    if ((rnd64() & 3u) == 0) return kSpecials[rnd64() % (sizeof(kSpecials) / sizeof(kSpecials[0]))];
    return rnd32();
}
// Finite-only pool for the arithmetic ops (see file header re NaN payloads).
static float rnd_finite() {
    switch (rnd64() % 8u) {
        case 0: return 0.0f;
        case 1: return -0.0f;
        case 2: return rac1::vu0m::u2f(0x00000001u);   // tiny denormal
        case 3: return rac1::vu0m::u2f(0x807fffffu);   // negative denormal
        case 4: return 1e-30f;
        case 5: return -1e30f;
        default: {
            float f;
            uint32_t u = rnd32() & 0x7f7fffffu;  // finite magnitude
            std::memcpy(&f, &u, 4);
            return (rnd64() & 1u) ? f * 1e-3f : f;
        }
    }
}

// ---------------------------------------------------------------------------
// Independent scalar references, written from the documented VU rules.
// ---------------------------------------------------------------------------

static uint32_t ref_cop_u(uint32_t u) {
    const uint32_t e = u & 0x7f800000u;
    if (e == 0) return u & 0x80000000u;
    if (e == 0x7f800000u) return (u & 0x80000000u) | 0x7f7fffffu;
    return u;
}

struct FinRef {
    uint32_t r[4];
    uint32_t mac;
    uint32_t st;
};
// The MAC-flag update of fin_m / fin<M>, lane by lane, from the VU rules:
// Z = exponent 0, S = sign, U = denormal, O = exponent 255 (clamped to max).
static FinRef ref_fin(const uint32_t v[4], int m, uint32_t status_in) {
    FinRef o;
    uint32_t zm = 0, sm = 0, um = 0, om = 0;
    for (int i = 0; i < 4; ++i) {
        const uint32_t u = v[i], e = u & 0x7f800000u, man = u & 0x007fffffu;
        uint32_t lane = 0, res = u;
        if (e == 0) {
            lane |= 1u;
            res = u & 0x80000000u;
            if (man != 0) lane |= 0x100u;
        } else if (e == 0x7f800000u) {
            lane |= 0x1000u;
            res = (u & 0x80000000u) | 0x7f7fffffu;
        }
        if ((u >> 31) != 0) lane |= 0x10u;
        o.r[i] = res;
        if ((m >> i & 1) != 0) {
            zm |= (lane & 1u) << i;
            sm |= ((lane >> 4) & 1u) << i;
            um |= ((lane >> 8) & 1u) << i;
            om |= ((lane >> 12) & 1u) << i;
        }
    }
    const uint32_t x = zm | (sm << 4) | (um << 8) | (om << 12);
    o.mac = ((x & 0x1111u) << 3) | ((x & 0x2222u) << 1) | ((x & 0x4444u) >> 1) | ((x & 0x8888u) >> 3);
    const uint32_t t = (x | (x >> 1) | (x >> 2) | (x >> 3)) & 0x1111u;
    const uint32_t st = ((t * 0x1248u) >> 12) & 15u;
    o.st = (status_in & 0xfc0) | st | (st << 6);
    return o;
}

// ---------------------------------------------------------------------------
// Test driver.
// ---------------------------------------------------------------------------

static int failures = 0;
static int checks = 0;
#define CHECK(cond)                                \
    do {                                           \
        ++checks;                                  \
        if (!(cond)) {                             \
            ++failures;                            \
            std::printf("FAIL %s:%d: %s\n", __func__, __LINE__, #cond); \
        }                                          \
    } while (0)

static bool vec_eq4(const float a[4], const float b[4]) {
    return std::memcmp(a, b, 16) == 0;
}
static void rnd_vec_u(uint32_t v[4]) {
    for (int i = 0; i < 4; ++i) v[i] = rnd_bits_full();
}
static void to_vec(const uint32_t u[4], rac1::VUVector& v) {
    std::memcpy(v.v, u, 16);
}
static void to_u(const rac1::VUVector& v, uint32_t u[4]) {
    std::memcpy(u, v.v, 16);
}

using rac1::vu0m::Vf;
using rac1::vun::I4;

static void test_vu0m_pure() {
    for (int t = 0; t < 20000; ++t) {
        uint32_t au[4], bu[4];
        rnd_vec_u(au);
        rnd_vec_u(bu);
        Vf a, b;
        to_vec(au, a);
        to_vec(bu, b);

        // ld4 round trip
        {
            I4 x = rac1::vu0m::ld4(a);
            uint32_t got[4];
            std::memcpy(got, &x, 16);
            CHECK(std::memcmp(got, au, 16) == 0);
        }
        // cop4
        {
            I4 x = rac1::vu0m::cop4(rac1::vu0m::ld4(a));
            uint32_t got[4], want[4];
            std::memcpy(got, &x, 16);
            for (int i = 0; i < 4; ++i) want[i] = ref_cop_u(au[i]);
            CHECK(std::memcmp(got, want, 16) == 0);
        }
        // vadd4 / vsub4 / vmul4 (finite inputs; see header note)
        {
            float af[4], bf[4];
            for (int i = 0; i < 4; ++i) {
                af[i] = rnd_finite();
                bf[i] = rnd_finite();
            }
            Vf fa, fb;
            std::memcpy(fa.v, af, 16);
            std::memcpy(fb.v, bf, 16);
            float got[4], want[4];
            I4 x = rac1::vu0m::vadd4(rac1::vu0m::ld4(fa), rac1::vu0m::ld4(fb));
            std::memcpy(got, &x, 16);
            for (int i = 0; i < 4; ++i) want[i] = af[i] + bf[i];
            CHECK(vec_eq4(got, want));
            x = rac1::vu0m::vsub4(rac1::vu0m::ld4(fa), rac1::vu0m::ld4(fb));
            std::memcpy(got, &x, 16);
            for (int i = 0; i < 4; ++i) want[i] = af[i] - bf[i];
            CHECK(vec_eq4(got, want));
            x = rac1::vu0m::vmul4(rac1::vu0m::ld4(fa), rac1::vu0m::ld4(fb));
            std::memcpy(got, &x, 16);
            for (int i = 0; i < 4; ++i) want[i] = af[i] * bf[i];
            CHECK(vec_eq4(got, want));
        }
        // bc4
        {
            float f = rac1::vu0m::u2f(rnd_bits_full());
            I4 x = rac1::vu0m::bc4(f);
            uint32_t got[4], w = rac1::vu0m::f2u(rac1::vu0m::vuc(f));
            std::memcpy(got, &x, 16);
            CHECK(got[0] == w && got[1] == w && got[2] == w && got[3] == w);
        }
    }
    // st4_m lane masking
    for (int t = 0; t < 4000; ++t) {
        for (int m : {0, 1, 5, 9, 15}) {
            uint32_t au[4], du[4];
            rnd_vec_u(au);
            rnd_vec_u(du);
            Vf a, d;
            to_vec(au, a);
            to_vec(du, d);
            rac1::vu0m::st4_m(d, rac1::vu0m::ld4(a), m);
            uint32_t got[4];
            to_u(d, got);
            for (int i = 0; i < 4; ++i) {
                const uint32_t want = ((m >> i) & 1) ? au[i] : du[i];
                CHECK(got[i] == want);
            }
        }
    }
}

static void test_vu0m_fin() {
    const int masks[] = {1, 3, 7, 15};
    for (int t = 0; t < 8000; ++t) {
        const int m = masks[t % 4];
        uint32_t vu[4];
        rnd_vec_u(vu);
        rac1::vu0m::Regs& R = rac1::vu0m::regs();
        R.macflag = rnd32();
        R.statusflag = rnd32();
        const uint32_t st_in = R.statusflag;
        Vf vin;
        std::memcpy(vin.v, vu, 16);
        I4 x = rac1::vu0m::ld4(vin);  // bits in, lanes out
        I4 r = rac1::vu0m::fin_m(R, x, m);
        const FinRef want = ref_fin(vu, m, st_in);
        uint32_t got[4];
        std::memcpy(got, &r, 16);
        CHECK(std::memcmp(got, want.r, 16) == 0);
        CHECK(R.macflag == want.mac);
        CHECK(R.statusflag == want.st);
    }
}

static void test_vu0m_fmac() {
    for (int t = 0; t < 4000; ++t) {
        const int m = 1 + static_cast<int>(rnd64() % 15u);
        uint32_t raw[4], du[4];
        rnd_vec_u(raw);
        rnd_vec_u(du);
        rac1::vu0m::Regs& R = rac1::vu0m::regs();
        R.macflag = rnd32();
        R.statusflag = rnd32();
        const uint32_t st_in = R.statusflag;
        // normal masked store into vf[5]
        Vf& d = R.vf[5];
        to_vec(du, d);
        I4 x;
        std::memcpy(&x, raw, 16);
        rac1::vu0m::fmac4(d, m, x);
        const FinRef want = ref_fin(raw, m, st_in);
        uint32_t got[4];
        to_u(d, got);
        for (int i = 0; i < 4; ++i) {
            const uint32_t w = ((m >> i) & 1) ? want.r[i] : du[i];
            CHECK(got[i] == w);
        }
        CHECK(R.macflag == want.mac);
        CHECK(R.statusflag == want.st);
        // ACC form
        R.macflag = rnd32();
        R.statusflag = rnd32();
        const uint32_t st2 = R.statusflag;
        uint32_t accu[4];
        rnd_vec_u(accu);
        to_vec(accu, R.acc);
        rac1::vu0m::fmac4_acc(m, x);
        const FinRef want2 = ref_fin(raw, m, st2);
        to_u(R.acc, got);
        for (int i = 0; i < 4; ++i) {
            const uint32_t w = ((m >> i) & 1) ? want2.r[i] : accu[i];
            CHECK(got[i] == w);
        }
        CHECK(R.macflag == want2.mac);
        // vf0 special case: lanes forced to (0,0,0,1)
        R.macflag = rnd32();
        R.statusflag = rnd32();
        Vf& z = R.vf[0];
        to_vec(du, z);
        rac1::vu0m::fmac4(z, m, x);
        to_u(z, got);
        CHECK(got[0] == 0u && got[1] == 0u && got[2] == 0u && got[3] == 0x3f800000u);
    }
}

static void test_vu0m_opmsub() {
    // Exercises the _mm_shuffle_epi32 swizzles (the trickiest NEON mapping).
    for (int t = 0; t < 4000; ++t) {
        uint32_t au[4], bu[4], cu[4], du[4];
        rnd_vec_u(au);
        rnd_vec_u(bu);
        rnd_vec_u(cu);
        rnd_vec_u(du);
        rac1::vu0m::Regs& R = rac1::vu0m::regs();
        R.macflag = rnd32();
        R.statusflag = rnd32();
        const uint32_t st_in = R.statusflag;
        Vf a, b, d;
        to_vec(au, a);
        to_vec(bu, b);
        to_vec(du, d);
        to_vec(cu, R.acc);
        rac1::vu0m::opmsub(d, a, b);
        // scalar reference: c - a_yzx * b_zxy on XYZ, through the flag update
        uint32_t raw[4];
        for (int i = 0; i < 4; ++i) {
            const uint32_t ayzx = ref_cop_u(i < 3 ? au[(i + 1) % 3] : au[3]);
            const uint32_t bzxy = ref_cop_u(i < 3 ? bu[(i + 2) % 3] : bu[3]);
            const float p = rac1::vu0m::u2f(ayzx) * rac1::vu0m::u2f(bzxy);
            raw[i] = rac1::vu0m::f2u(rac1::vu0m::u2f(ref_cop_u(cu[i])) - p);
        }
        const FinRef want = ref_fin(raw, 7 /*XYZ*/, st_in);
        uint32_t got[4];
        to_u(d, got);
        for (int i = 0; i < 3; ++i) CHECK(got[i] == want.r[i]);
        CHECK(got[3] == du[3]);  // w lane untouched
        CHECK(R.macflag == want.mac);
        CHECK(R.statusflag == want.st);
    }
}

static void test_vun_pure() {
    using namespace rac1::vun;
    using rac1::VUVector;
    for (int t = 0; t < 20000; ++t) {
        uint32_t au[4], bu[4];
        rnd_vec_u(au);
        rnd_vec_u(bu);
        VUVector a, b;
        to_vec(au, a);
        to_vec(bu, b);
        {  // ld4/st4 round trip
            VUVector v{};
            st4(v, ld4(a));
            uint32_t got[4];
            to_u(v, got);
            CHECK(std::memcmp(got, au, 16) == 0);
        }
        {  // cop4
            I4 x = cop4(ld4(a));
            uint32_t got[4], want[4];
            std::memcpy(got, &x, 16);
            for (int i = 0; i < 4; ++i) want[i] = ref_cop_u(au[i]);
            CHECK(std::memcmp(got, want, 16) == 0);
        }
        {  // bits4
            const uint32_t u = rnd32();
            I4 x = bits4(u);
            uint32_t got[4];
            std::memcpy(got, &x, 16);
            CHECK(got[0] == u && got[1] == u && got[2] == u && got[3] == u);
        }
        {  // vadd/vsub/vmul, finite inputs
            float af[4], bf[4];
            for (int i = 0; i < 4; ++i) {
                af[i] = rnd_finite();
                bf[i] = rnd_finite();
            }
            VUVector fa, fb;
            std::memcpy(fa.v, af, 16);
            std::memcpy(fb.v, bf, 16);
            float got[4], want[4];
            I4 x = vadd(ld4(fa), ld4(fb));
            std::memcpy(got, &x, 16);
            for (int i = 0; i < 4; ++i) want[i] = af[i] + bf[i];
            CHECK(vec_eq4(got, want));
            x = vsub(ld4(fa), ld4(fb));
            std::memcpy(got, &x, 16);
            for (int i = 0; i < 4; ++i) want[i] = af[i] - bf[i];
            CHECK(vec_eq4(got, want));
            x = vmul(ld4(fa), ld4(fb));
            std::memcpy(got, &x, 16);
            for (int i = 0; i < 4; ++i) want[i] = af[i] * bf[i];
            CHECK(vec_eq4(got, want));
        }
        {  // vq: scalar copf, broadcast
            const float f = rac1::vu0m::u2f(rnd_bits_full());
            I4 x = vq(f);
            uint32_t got[4];
            std::memcpy(got, &x, 16);
            const uint32_t w = u_of(copf(f));
            CHECK(got[0] == w && got[1] == w && got[2] == w && got[3] == w);
        }
        {  // VUN_BC lane broadcast (lanes must be compile-time constants,
           // as with the underlying shuffle immediate)
            I4 x = ld4(a);
            const int lanes[4] = {0, 1, 2, 3};
            for (int li = 0; li < 4; ++li) {
                const int l = lanes[li];
                uint32_t got[4];
                if (l == 0) {
                    I4 y = VUN_BC(x, 0);
                    std::memcpy(got, &y, 16);
                } else if (l == 1) {
                    I4 y = VUN_BC(x, 1);
                    std::memcpy(got, &y, 16);
                } else if (l == 2) {
                    I4 y = VUN_BC(x, 2);
                    std::memcpy(got, &y, 16);
                } else {
                    I4 y = VUN_BC(x, 3);
                    std::memcpy(got, &y, 16);
                }
                CHECK(got[0] == au[l] && got[1] == au[l] && got[2] == au[l] && got[3] == au[l]);
            }
        }
        {  // ldq4 round trip
            I4 x = ldq4(au);
            uint32_t got[4];
            std::memcpy(got, &x, 16);
            CHECK(std::memcmp(got, au, 16) == 0);
        }
    }
    // st4m / stq4m lane masking
    for (int t = 0; t < 4000; ++t) {
        const int m = 1 + static_cast<int>(rnd64() % 15u);
        uint32_t au[4], vu[4], qu[4];
        rnd_vec_u(au);
        rnd_vec_u(vu);
        rnd_vec_u(qu);
        VUVector a, v;
        to_vec(au, a);
        to_vec(vu, v);
        uint32_t got[4];
        if (m == 1) {
            st4m<1>(v, ld4(a));
        } else if (m <= 3) {
            st4m<3>(v, ld4(a));
        } else if (m <= 7) {
            st4m<7>(v, ld4(a));
        } else {
            st4m<15>(v, ld4(a));
        }
        const int meff = (m == 1) ? 1 : (m <= 3 ? 3 : (m <= 7 ? 7 : 15));
        to_u(v, got);
        for (int i = 0; i < 4; ++i) CHECK(got[i] == (((meff >> i) & 1) ? au[i] : vu[i]));
        uint32_t q[4];
        std::memcpy(q, qu, 16);
        if (meff == 1) {
            stq4m<1>(q, ld4(a));
        } else if (meff == 3) {
            stq4m<3>(q, ld4(a));
        } else if (meff == 7) {
            stq4m<7>(q, ld4(a));
        } else {
            stq4m<15>(q, ld4(a));
        }
        for (int i = 0; i < 4; ++i) CHECK(q[i] == (((meff >> i) & 1) ? au[i] : qu[i]));
    }
}

template <int M>
static void test_vun_fin_one() {
    using namespace rac1::vun;
    for (int t = 0; t < 6000; ++t) {
        uint32_t vu[4];
        rnd_vec_u(vu);
        FlagRegs f;
        f.macflag = rnd32();
        f.statusflag = rnd32();
        const uint32_t st_in = f.statusflag;
        I4 x;
        std::memcpy(&x, vu, 16);
        I4 r = fin<M>(f, x);
        const FinRef want = ref_fin(vu, M, st_in);
        uint32_t got[4];
        std::memcpy(got, &r, 16);
        CHECK(std::memcmp(got, want.r, 16) == 0);
        CHECK(f.macflag == want.mac);
        CHECK(f.statusflag == want.st);
    }
}

int main() {
    test_vu0m_pure();
    test_vu0m_fin();
    test_vu0m_fmac();
    test_vu0m_opmsub();
    test_vun_pure();
    test_vun_fin_one<1>();
    test_vun_fin_one<5>();
    test_vun_fin_one<15>();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
