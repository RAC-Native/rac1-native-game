// Equivalence test for the float helpers' raw forms (RAC1_PERF_FPU_RAW, RAC1_PERF_VU0_RAW: runtime/ee/ps2_fpu.h).
//
// Every helper that takes a raw form - the EE FPU's fpu_add / fpu_sub / fpu_mul (src/game/ovl/ovl_common.h), the VU0 lane ops
// (vu0_macro.h), the VU0 macro ops with their flags on the VU0's state and on a LocalFlags (vu0_regs.h), without them
// (vu0_nf.h), and on a routine's local copy with and without flags (vu0_local.h) - is run twice on the same operands and the
// same VU0 state, with the raw forms off (the rule-by-rule form the helper had before) and on, and the results compared bit for
// bit: the returned float, or the VU0's 32 registers, ACC, Q and the MAC / status / clip flags. The helpers are the real ones,
// from the real headers; only the VU0's instance is the test's own.
//
// Operands: random words mixed with the edges - exponents 0, 1..3, 252..255 and around 127, mantissas zero, one, one bit, all
// ones -, pairs made to cancel or to end near the range's two ends, every lane mask (0..15), every destination (vf0 too) and
// operands that are the destination.
//
// The test runs in the game's float mode (rac1::ps2fpu::enter_ee_rounding(): toward zero, no denormals), where the raw forms
// hold; with RAC1_FPU_NEAREST=1 the game keeps the rule-by-rule forms and so does the test (it then compares them with
// themselves).
//
// Build and run (x86-64 or AArch64; the count scales the cases, 1 = about 900 million lane results):
//   g++ -std=c++20 -O2 -frounding-math -ffp-contract=off -I<repo> -I<repo>/src tests/fp/fp_raw_equiv.cpp -o fp_raw_equiv && ./fp_raw_equiv [count]
//   cmake -S . -B build -DRAC1_FP_TEST=ON && cmake --build build --target fp_raw_equiv && ctest --test-dir build -R fp_raw_equiv
//
// Exit code 0: no difference. Non-zero: differences were found (the first few are printed).

#include <game/ovl/vu0_local.h>
#include <game/ovl/vu0_nf.h>
#include <game/ovl/vu0_regs.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// The VU0 the helpers work on (the runtime's instance is not linked here).
rac1::vu0::Vu0 rac1::vu0::detail::g_instance;

namespace {

using rac1::f2u;
using rac1::u2f;
using rac1::vu0m::Vf;

uint64_t g_rng = 0x9e3779b97f4a7c15ull;
uint64_t rnd64() {
    uint64_t x = g_rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_rng = x;
    return x * 0x2545f4914f6cdd1dull;
}

// A float's words: an exponent from the edges, around 1.0 or any; a mantissa from the edges or any.
uint32_t rnd_word() {
    const uint64_t r = rnd64();
    static const uint32_t edge[8] = {0, 1, 2, 3, 252, 253, 254, 255};
    uint32_t e, m;
    switch ((r >> 1) & 7) {
    case 0:
    case 1: e = edge[(r >> 4) & 7]; break;
    case 2: e = 124 + static_cast<uint32_t>((r >> 4) % 7); break;
    default: e = static_cast<uint32_t>(r >> 8) & 0xffu; break;
    }
    switch ((r >> 16) & 7) {
    case 0: m = 0; break;
    case 1: m = 0x7fffff; break;
    case 2: m = 1; break;
    case 3: m = 1u << ((r >> 20) % 23); break;
    case 4: m = 0x7fffffu ^ (1u << ((r >> 20) % 23)); break;
    default: m = static_cast<uint32_t>(r >> 32) & 0x7fffffu; break;
    }
    return static_cast<uint32_t>(r & 1) << 31 | e << 23 | m;
}
// A second operand for `a`: any word, or one that cancels it (the same magnitude a few units of the last place away, either
// sign), or one whose exponent puts their product at one end of the range.
uint32_t rnd_partner(uint32_t a) {
    const uint64_t r = rnd64();
    switch (r & 7) {
    case 0: return (a ^ 0x80000000u) + static_cast<uint32_t>((r >> 8) % 5) - 2;
    case 1: return a + static_cast<uint32_t>((r >> 8) % 5) - 2;
    case 2: {  // (exponents summing to the smallest or the largest a float has, give or take)
        const int ea = static_cast<int>(a >> 23 & 0xff);
        const int want = ((r >> 8) & 1) != 0 ? 127 + 126 + static_cast<int>((r >> 12) % 7) - 3 : 127 - 126 + static_cast<int>((r >> 12) % 7) - 3;
        const int eb = want - ea + 127;
        if (eb < 0 || eb > 255) return rnd_word();
        return (rnd_word() & 0x807fffffu) | static_cast<uint32_t>(eb) << 23;
    }
    default: return rnd_word();
    }
}

uint64_t g_cases = 0, g_bad = 0;
void fail(const char* what, const char* detail) {
    if (++g_bad <= 20) std::printf("DIFF %s: %s\n", what, detail);
}

// ---- the scalar helpers ----
template <class F>
void scalar2(const char* name, int& mode, uint64_t n, F f) {
    const uint64_t bad0 = g_bad;
    for (uint64_t i = 0; i < n; ++i) {
        const uint32_t a = rnd_word(), b = rnd_partner(a);
        mode = 0;
        const uint32_t r0 = f2u(f(u2f(a), u2f(b)));
        mode = 1;
        const uint32_t r1 = f2u(f(u2f(a), u2f(b)));
        ++g_cases;
        if (r0 != r1) {
            char d[128];
            std::snprintf(d, sizeof(d), "%08x, %08x: rules %08x, raw %08x", a, b, r0, r1);
            fail(name, d);
        }
    }
    std::printf("%-28s %12llu cases, %llu differing\n", name, static_cast<unsigned long long>(n), static_cast<unsigned long long>(g_bad - bad0));
}
template <class F>
void scalar3(const char* name, int& mode, uint64_t n, F f) {
    const uint64_t bad0 = g_bad;
    for (uint64_t i = 0; i < n; ++i) {
        const uint32_t a = rnd_word(), b = rnd_partner(a);
        // (ACC: any word, or the product's opposite within a few units, so the sum cancels)
        uint32_t c = rnd_word();
        if ((rnd64() & 3) == 0) c = (f2u(u2f(a) * u2f(b)) ^ 0x80000000u) + static_cast<uint32_t>(rnd64() % 5) - 2;
        mode = 0;
        const uint32_t r0 = f2u(f(u2f(c), u2f(a), u2f(b)));
        mode = 1;
        const uint32_t r1 = f2u(f(u2f(c), u2f(a), u2f(b)));
        ++g_cases;
        if (r0 != r1) {
            char d[128];
            std::snprintf(d, sizeof(d), "%08x +- %08x * %08x: rules %08x, raw %08x", c, a, b, r0, r1);
            fail(name, d);
        }
    }
    std::printf("%-28s %12llu cases, %llu differing\n", name, static_cast<unsigned long long>(n), static_cast<unsigned long long>(g_bad - bad0));
}

// ---- the VU0 macro ops: what an op may change of the VU0 ----
struct Snap {
    Vf vf[32];
    Vf acc;
    float q;
    uint32_t mac, status, clip;
};
void take(Snap& s) {
    const rac1::VUState& st = rac1::vu0::global().st;
    for (int i = 0; i < 32; ++i) s.vf[i] = st.vf[static_cast<size_t>(i)];
    s.acc = st.acc;
    s.q = st.q;
    s.mac = st.macflag, s.status = st.statusflag, s.clip = st.clipflag;
}
void put(const Snap& s) {
    rac1::VUState& st = rac1::vu0::global().st;
    for (int i = 0; i < 32; ++i) st.vf[static_cast<size_t>(i)] = s.vf[i];
    st.acc = s.acc;
    st.q = s.q;
    st.macflag = s.mac, st.statusflag = s.status, st.clipflag = s.clip;
}
bool same(const Snap& a, const Snap& b) {
    return std::memcmp(a.vf, b.vf, sizeof(a.vf)) == 0 && std::memcmp(&a.acc, &b.acc, sizeof(Vf)) == 0 && f2u(a.q) == f2u(b.q) && a.mac == b.mac &&
           a.status == b.status && a.clip == b.clip;
}
// A VU0 state: registers 1..7 and ACC from the operand pools (register 2 the partner of 1, ACC near the opposite of their
// product on some lanes), vf0 its constant, the flags any.
void rnd_state(Snap& s) {
    for (int r = 1; r < 8; ++r)
        for (int l = 0; l < 4; ++l) s.vf[r].v[l] = u2f(rnd_word());
    for (int l = 0; l < 4; ++l) {
        s.vf[2].v[l] = u2f(rnd_partner(f2u(s.vf[1].v[l])));
        s.acc.v[l] = u2f(rnd_word());
        if ((rnd64() & 3) == 0) s.acc.v[l] = u2f((f2u(s.vf[1].v[l] * s.vf[2].v[l]) ^ 0x80000000u) + static_cast<uint32_t>(rnd64() % 5) - 2);
    }
    s.vf[0].v[0] = 0.0f, s.vf[0].v[1] = 0.0f, s.vf[0].v[2] = 0.0f, s.vf[0].v[3] = 1.0f;
    s.q = u2f(rnd_word());
    s.mac = static_cast<uint32_t>(rnd64()) & 0xffffu;
    s.status = static_cast<uint32_t>(rnd64()) & 0xfffu;
    s.clip = static_cast<uint32_t>(rnd64()) & 0xffffffu;
}
// One op's operands: the destination d (0..7: vf0 one time in eight), the sources a and b (1..7, often d itself), the broadcast
// lane's value (a register's lane as it is), the lane mask.
struct Args {
    int d, a, b, m;
    float bc;
};
Args rnd_args(const Snap& s) {
    const uint64_t r = rnd64();
    Args x;
    x.d = static_cast<int>(r & 7);
    x.a = (r >> 3 & 3) == 0 && x.d != 0 ? x.d : 1 + static_cast<int>((r >> 5) % 7);
    x.b = (r >> 8 & 3) == 0 && x.d != 0 ? x.d : 1 + static_cast<int>((r >> 10) % 7);
    if ((r >> 13 & 1) != 0) x.a = 1, x.b = 2;  // (the partnered pair)
    x.m = static_cast<int>(r >> 16 & 15);
    x.bc = s.vf[1 + (r >> 20) % 7].v[r >> 24 & 3];
    return x;
}
template <class F>
void vec(const char* name, uint64_t n, F f) {
    const uint64_t bad0 = g_bad;
    Snap s0, r0, r1;
    std::memset(&s0, 0, sizeof(s0));
    for (uint64_t i = 0; i < n; ++i) {
        if ((i & 3) == 0) rnd_state(s0);  // (a state serves a few ops)
        const Args x = rnd_args(s0);
        rac1::ps2fpu::g_vu0_raw = 0;
        put(s0);
        f(x);
        take(r0);
        rac1::ps2fpu::g_vu0_raw = 1;
        put(s0);
        f(x);
        take(r1);
        g_cases += 4;
        if (!same(r0, r1)) {
            char d[200];
            std::snprintf(d, sizeof(d), "d %d a %d b %d m %x bc %08x: mac %04x / %04x status %03x / %03x", x.d, x.a, x.b, x.m, f2u(x.bc), r0.mac, r1.mac, r0.status, r1.status);
            fail(name, d);
        }
    }
    std::printf("%-28s %12llu ops, %llu differing\n", name, static_cast<unsigned long long>(n), static_cast<unsigned long long>(g_bad - bad0));
}

}  // namespace

int main(int argc, char** argv) {
    rac1::ps2fpu::enter_ee_rounding();
    const double scale = argc > 1 ? std::atof(argv[1]) : 1.0;
    const auto N = [scale](double millions) { return static_cast<uint64_t>(millions * 1e6 * scale); };
    std::printf("float mode: %s\n", rac1::ps2fpu::thread_chops() ? "toward zero, no denormals (the EE's)" : "the host's (RAC1_FPU_NEAREST): the raw forms stay off");
    const bool chop = rac1::ps2fpu::g_ee_chop;
    // (without the EE's mode the game never turns the raw forms on: both runs take the rules then)
    int off = 0;
    int& fpu = chop ? rac1::ps2fpu::g_fpu_raw : off;
    int& vu = chop ? rac1::ps2fpu::g_vu0_raw : off;

    // the EE FPU
    scalar2("fpu_add", fpu, N(60), [](float a, float b) { return rac1::fpu_add(a, b); });
    scalar2("fpu_sub", fpu, N(60), [](float a, float b) { return rac1::fpu_sub(a, b); });
    scalar2("fpu_mul", fpu, N(60), [](float a, float b) { return rac1::fpu_mul(a, b); });
    // the VU0 lane ops
    scalar2("vu0m::add (lane)", vu, N(60), [](float a, float b) { return rac1::vu0m::add(a, b); });
    scalar2("vu0m::sub (lane)", vu, N(60), [](float a, float b) { return rac1::vu0m::sub(a, b); });
    scalar2("vu0m::mul (lane)", vu, N(60), [](float a, float b) { return rac1::vu0m::mul(a, b); });
    scalar3("vu0m::madd (lane)", vu, N(60), [](float c, float a, float b) { return rac1::vu0m::madd(c, a, b); });
    scalar3("vu0m::msub (lane)", vu, N(60), [](float c, float a, float b) { return rac1::vu0m::msub(c, a, b); });
    if (!chop) {
        std::printf("%llu cases, %llu differing\n", static_cast<unsigned long long>(g_cases), static_cast<unsigned long long>(g_bad));
        return g_bad != 0 ? 1 : 0;
    }

    // the macro ops with their flags, on the VU0's state (vu0_regs.h)
    {
        using namespace rac1::vu0m;
        const uint64_t n = N(2);
        vec("vu0m add", n, [](const Args& x) { add(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m sub", n, [](const Args& x) { sub(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m mul", n, [](const Args& x) { mul(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m addbc", n, [](const Args& x) { addbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0m subbc", n, [](const Args& x) { subbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0m mulbc", n, [](const Args& x) { mulbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0m maddbc", n, [](const Args& x) { maddbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0m msubbc", n, [](const Args& x) { msubbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0m maddv", n, [](const Args& x) { maddv(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m msubv", n, [](const Args& x) { msubv(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m addabc", n, [](const Args& x) { addabc(vf()[x.a], x.bc, x.m); });
        vec("vu0m subabc", n, [](const Args& x) { subabc(vf()[x.a], x.bc, x.m); });
        vec("vu0m mulabc", n, [](const Args& x) { mulabc(vf()[x.a], x.bc, x.m); });
        vec("vu0m maddabc", n, [](const Args& x) { maddabc(vf()[x.a], x.bc, x.m); });
        vec("vu0m msubabc", n, [](const Args& x) { msubabc(vf()[x.a], x.bc, x.m); });
        vec("vu0m addav", n, [](const Args& x) { addav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m subav", n, [](const Args& x) { subav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m mulav", n, [](const Args& x) { mulav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m maddav", n, [](const Args& x) { maddav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m msubav", n, [](const Args& x) { msubav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0m opmula opmsub", n, [](const Args& x) {
            opmula(vf()[x.a], vf()[x.b]);
            opmsub(vf()[x.d], vf()[x.b], vf()[x.a]);
        });
        vec("vu0m rotate", n, [](const Args& x) { rotate(vf()[x.d], 3, vf()[x.a], x.m); });
        vec("vu0m transform", n, [](const Args& x) { transform(vf()[x.d], 3, vf()[x.a], x.bc); });
        // the same with the flags in a LocalFlags (written back at the scope's end), a few ops in one scope
        vec("LocalFlags add mul sub", n, [](const Args& x) {
            LocalFlags fl(regs());
            add(fl, vf()[x.d], vf()[x.a], vf()[x.b], x.m);
            mul(fl, vf()[x.b], vf()[x.d], vf()[x.a], x.m ^ 5);
            sub(fl, vf()[x.a], vf()[x.b], vf()[x.d], 15);
        });
        vec("LocalFlags bc forms", n, [](const Args& x) {
            LocalFlags fl(regs());
            addbc(fl, vf()[x.d], vf()[x.a], x.bc, x.m);
            mulbc(fl, vf()[x.b], vf()[x.d], x.bc, 15);
            subbc(fl, vf()[x.a], vf()[x.b], x.bc, x.m ^ 9);
        });
        vec("LocalFlags ACC forms", n, [](const Args& x) {
            LocalFlags fl(regs());
            mulabc(fl, vf()[x.a], x.bc, x.m);
            maddabc(fl, vf()[x.b], x.bc, 15);
            addabc(fl, vf()[x.a], x.bc, x.m ^ 3);
            maddbc(fl, vf()[x.d], vf()[x.b], x.bc, x.m);
        });
        vec("LocalFlags rotate transform", n, [](const Args& x) {
            LocalFlags fl(regs());
            rotate(fl, vf()[x.d], 3, vf()[x.a], x.m);
            transform(fl, vf()[x.b], 3, vf()[x.d], x.bc);
            opmula(fl, vf()[x.a], vf()[x.b]);
            opmsub(fl, vf()[x.d], vf()[x.b], vf()[x.a]);
        });
    }
    // the macro ops without their flags (vu0_nf.h; into vf0: with them)
    {
        using namespace rac1::vu0nf;
        const uint64_t n = N(2);
        vec("vu0nf add", n, [](const Args& x) { add(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf sub", n, [](const Args& x) { sub(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf mul", n, [](const Args& x) { mul(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf addbc", n, [](const Args& x) { addbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0nf subbc", n, [](const Args& x) { subbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0nf mulbc", n, [](const Args& x) { mulbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0nf maddbc", n, [](const Args& x) { maddbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0nf msubbc", n, [](const Args& x) { msubbc(vf()[x.d], vf()[x.a], x.bc, x.m); });
        vec("vu0nf maddv", n, [](const Args& x) { maddv(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf msubv", n, [](const Args& x) { msubv(vf()[x.d], vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf addabc", n, [](const Args& x) { addabc(vf()[x.a], x.bc, x.m); });
        vec("vu0nf subabc", n, [](const Args& x) { subabc(vf()[x.a], x.bc, x.m); });
        vec("vu0nf mulabc", n, [](const Args& x) { mulabc(vf()[x.a], x.bc, x.m); });
        vec("vu0nf maddabc", n, [](const Args& x) { maddabc(vf()[x.a], x.bc, x.m); });
        vec("vu0nf msubabc", n, [](const Args& x) { msubabc(vf()[x.a], x.bc, x.m); });
        vec("vu0nf addav", n, [](const Args& x) { addav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf subav", n, [](const Args& x) { subav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf mulav", n, [](const Args& x) { mulav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf maddav", n, [](const Args& x) { maddav(vf()[x.a], vf()[x.b], x.m); });
        vec("vu0nf msubav", n, [](const Args& x) { msubav(vf()[x.a], vf()[x.b], x.m); });
    }
    // the ops on a routine's local copy (vu0_local.h), without and with the flags: loaded, a few ops, stored
    const auto local = [&](const char* name, auto make) {
        vec(name, N(2), [&](const Args& x) {
            auto v = make();
            v.load();
            v.add(x.d, x.a, x.b, x.m);
            v.sub(x.a, x.b, x.d, x.m ^ 6);
            v.mul(x.b, x.d, x.a, 15);
            v.addbc(x.d, x.a, x.bc, x.m);
            v.subbc(x.a, x.b, x.bc, 15);
            v.mulbc(x.b, x.d, x.bc, x.m ^ 9);
            v.store();
        });
        vec(name, N(2), [&](const Args& x) {
            auto v = make();
            v.load();
            v.mulabc(x.a, x.bc, x.m);
            v.maddabc(x.b, x.bc, 15);
            v.addabc(x.a, x.bc, x.m ^ 3);
            v.maddbc(x.d, x.b, x.bc, x.m);
            v.addav(x.a, x.b, x.m);
            v.mulav(x.b, x.a, 15);
            v.maddv(x.d, x.a, x.b, x.m ^ 12);
            v.subbc_flags(x.a, x.bc, x.m);
            v.store();
        });
        vec(name, N(2), [&](const Args& x) {
            auto v = make();
            v.load();
            v.transform(x.d, 3, 4, 5, 6, x.a, x.bc);
            v.transform3(x.b, 3, 4, 5, 6, x.d, x.bc);
            v.rotate(x.a, 4, 5, 6, x.b, x.m);
            v.opmula(x.a, x.b);
            v.opmsub(x.d, x.b, x.a);
            v.store();
        });
    };
    local("vu0l Vu (no flags)", [] { return rac1::vu0l::Vu(); });
    local("vu0l VuF (flags)", [] { return rac1::vu0l::VuF(); });

    std::printf("%llu cases (lane results), %llu ops differing\n", static_cast<unsigned long long>(g_cases), static_cast<unsigned long long>(g_bad));
    return g_bad != 0 ? 1 : 0;
}
