// Oracle batch 470 (perf/cpu: the CPU's frame time): the VU0 macro ops' results and flags (src/game/ovl/vu0_regs.h) as the
// port computed them before the work, on the host alone (no original, no level data: a unit batch).
//   fin_m        the FMAC result's clamp and its MAC / status flags, now built on the lanes reversed and packed into one
//                movemask, against the earlier lane-by-lane form (kept here as the reference), on random words of every
//                class (zero, denormal, the largest exponent, normal; both signs) and every lane mask, from random flags
//   local_flags  random sequences of the macro ops the hand-written routines use (add / sub / mul and their broadcast,
//                ACC and multiply-add forms, rotate / transform, the cross product's opmula / opmsub), run once on the
//                VU0's own flags (each op storing them) and once on a LocalFlags (the flags in locals, written back at the
//                scope's end: moby_joints_pose): the registers, ACC, the MAC and the status flags compared after
// Run: build/tests/ovl_oracle --batch 470 (any level: it reads nothing of one).

#include "ovl_oracle.h"

#include <game/ovl/vu0_regs.h>

#include <cstring>

namespace rac1::oracle {

namespace {

using namespace vu0m;

// The FMAC result's clamp and flags as vu0_regs.h's fin_m computed them before (four movemasks, the nibbles reversed)
__m128i fin_ref(uint32_t& macflag, uint32_t& statusflag, __m128i v, int m) {
    const __m128i expm = _mm_set1_epi32(0x7f800000);
    const __m128i e = _mm_and_si128(v, expm);
    const __m128i isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    const __m128i isi = _mm_cmpeq_epi32(e, expm);
    const __m128i sgn = _mm_and_si128(v, _mm_set1_epi32(static_cast<int>(0x80000000u)));
    __m128i r = _mm_andnot_si128(_mm_and_si128(isz, _mm_set1_epi32(0x7fffffff)), v);
    r = _mm_or_si128(_mm_andnot_si128(isi, r), _mm_and_si128(isi, _mm_or_si128(sgn, _mm_set1_epi32(0x7f7fffff))));
    const __m128i nz = _mm_cmpeq_epi32(_mm_and_si128(v, _mm_set1_epi32(0x007fffff)), _mm_setzero_si128());
    const uint32_t mu = static_cast<uint32_t>(m);
    const uint32_t zm = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(isz))) & mu;
    const uint32_t sm = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(v))) & mu;
    const uint32_t um = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(_mm_andnot_si128(nz, isz)))) & mu;
    const uint32_t om = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(isi))) & mu;
    const uint32_t x = zm | (sm << 4) | (um << 8) | (om << 12);
    macflag = ((x & 0x1111u) << 3) | ((x & 0x2222u) << 1) | ((x & 0x4444u) >> 1) | ((x & 0x8888u) >> 3);
    const uint32_t t = (x | (x >> 1) | (x >> 2) | (x >> 3)) & 0x1111u;
    const uint32_t st = ((t * 0x1248u) >> 12) & 15u;
    statusflag = (statusflag & 0xfc0) | st | (st << 6);
    return r;
}

// a word of a random class: zero, denormal, the largest exponent (Inf / NaN), small, normal; either sign
uint32_t any_word() {
    const uint32_t s = coin() ? 0x80000000u : 0u;
    const uint32_t mant = R() & 0x007fffffu;
    switch (ri(0, 5)) {
        case 0: return s;
        case 1: return s | (mant | 1u);
        case 2: return s | 0x7f800000u | (coin() ? 0u : mant);
        case 3: return s | (static_cast<uint32_t>(ri(1, 4)) << 23) | mant;
        default: return s | (static_cast<uint32_t>(ri(100, 154)) << 23) | mant;
    }
}
float any_float() { return std::bit_cast<float>(any_word()); }
// a value the ops' sums and products keep finite most of the time (some overflow, some underflow: their flags)
float op_float() {
    if (ri(0, 15) == 0) return any_float();
    const float v = rf(-4.0f, 4.0f);
    return ri(0, 7) == 0 ? v * 1e-38f : ri(0, 7) == 0 ? v * 1e38f : v;
}

// The fixed lane masks of the ops (template arguments in the routines; here a runtime choice of each)
template <class F>
void op_once(F& fl, int kind, int d, int a, int b, int m, int lane) {
    Vf* const v = vf();
    const float bc = v[b].v[lane];
    switch (kind) {
        case 0: add(fl, v[d], v[a], v[b], m); break;
        case 1: sub(fl, v[d], v[a], v[b], m); break;
        case 2: mul(fl, v[d], v[a], v[b], m); break;
        case 3: addbc(fl, v[d], v[a], bc, m); break;
        case 4: subbc(fl, v[d], v[a], bc, m); break;
        case 5: mulbc(fl, v[d], v[a], bc, m); break;
        case 6: addabc(fl, v[a], bc, m); break;
        case 7: mulabc(fl, v[a], bc, m); break;
        case 8: maddabc(fl, v[a], bc, m); break;
        case 9: maddbc(fl, v[d], v[a], bc, m); break;
        case 10: rotate(fl, v[d], 8 + (a & 3) * 4, v[b], m); break;
        case 11: transform(fl, v[d], 8 + (a & 3) * 4, v[b], bc); break;
        case 12: opmula(fl, v[a], v[b]); break;
        default: opmsub(fl, v[d], v[a], v[b]); break;
    }
}

struct Op {
    int kind, d, a, b, m, lane;
};

struct Snap {
    std::array<VUVector, 32> vf;
    VUVector acc;
    uint32_t macflag, statusflag;
};
Snap snap() {
    const Regs& R = regs();
    return Snap{R.vf, R.acc, R.macflag, R.statusflag};
}
bool same(const Snap& x, const Snap& y) {
    return std::memcmp(x.vf.data(), y.vf.data(), sizeof(VUVector) * 32) == 0 && std::memcmp(&x.acc, &y.acc, sizeof x.acc) == 0 &&
           x.macflag == y.macflag && x.statusflag == y.statusflag;
}

}  // namespace

void batch_470() {
    if (g_only_batch != 470) return;
    g_batch = 470;
    Regs& V = regs();
    const Regs saved = V;
    {  // fin_m against the reference
        int bad = 0, cases = 0;
        for (int c = 0; c < 200000; ++c) {
            alignas(16) uint32_t w[4];
            for (uint32_t& x : w) x = any_word();
            const __m128i v = _mm_load_si128(reinterpret_cast<const __m128i*>(w));
            const int m = ri(0, 15);
            const uint32_t mac0 = R() & 0xffff, st0 = R() & 0xfff;
            uint32_t mac_ref = mac0, st_ref = st0;
            const __m128i want = fin_ref(mac_ref, st_ref, v, m);
            V.macflag = mac0;
            V.statusflag = st0;
            const __m128i got = fin_m(V, v, m);
            // the LocalFlags form: its flags after a sync
            LocalFlags fl(V);
            fl.macflag = mac0;
            fl.statusflag = st0;
            const __m128i got_l = fin_m(fl, v, m);
            fl.sync();
            ++cases;
            const bool ok = _mm_movemask_epi8(_mm_cmpeq_epi32(want, got)) == 0xffff && _mm_movemask_epi8(_mm_cmpeq_epi32(want, got_l)) == 0xffff &&
                            V.macflag == mac_ref && V.statusflag == st_ref && fl.macflag == mac_ref && fl.statusflag == st_ref;
            if (!ok && bad++ < 5)
                std::printf("  fin_m: words %08x %08x %08x %08x mask %x: mac %04x / %04x / %04x, status %03x / %03x / %03x\n", w[0], w[1], w[2], w[3], m, mac_ref,
                            V.macflag, fl.macflag, st_ref, V.statusflag, fl.statusflag);
        }
        std::printf("%-26s 0x%08x  %s (%d/%d)\n", "fin_m", 0u, bad == 0 ? "ok" : "FAIL", cases - bad, cases);
        if (bad != 0) ++g_fail;
    }
    {  // op sequences: the VU0's flags against a LocalFlags
        int bad = 0;
        const int n = 20000;
        for (int c = 0; c < n; ++c) {
            for (int r = 1; r < 32; ++r)
                for (int k = 0; k < 4; ++k) V.vf[static_cast<size_t>(r)].v[k] = op_float();
            V.vf[0] = VUVector{};
            V.vf[0].v[3] = 1.0f;
            for (int k = 0; k < 4; ++k) V.acc.v[k] = op_float();
            V.macflag = R() & 0xffff;
            V.statusflag = R() & 0xfff;
            std::vector<Op> ops(static_cast<size_t>(ri(1, 40)));
            for (Op& o : ops) o = Op{ri(0, 13), ri(0, 31), ri(0, 31), ri(0, 31), ri(0, 15), ri(0, 3)};
            const Snap start = snap();
            for (const Op& o : ops) op_once(V, o.kind, o.d, o.a, o.b, o.m, o.lane);
            const Snap eager = snap();
            V.vf = start.vf;
            V.acc = start.acc;
            V.macflag = start.macflag;
            V.statusflag = start.statusflag;
            {
                LocalFlags fl(V);
                for (const Op& o : ops) op_once(fl, o.kind, o.d, o.a, o.b, o.m, o.lane);
            }
            const Snap local = snap();
            if (!same(eager, local) && bad++ < 5)
                std::printf("  local_flags: case %d (%zu ops): mac %04x / %04x, status %03x / %03x\n", c, ops.size(), eager.macflag, local.macflag, eager.statusflag,
                            local.statusflag);
        }
        std::printf("%-26s 0x%08x  %s (%d/%d)\n", "local_flags", 0u, bad == 0 ? "ok" : "FAIL", n - bad, n);
        if (bad != 0) ++g_fail;
    }
    V = saved;
}

}  // namespace rac1::oracle
