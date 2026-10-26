// Oracle batch 470 (perf/cpu: the CPU's frame time): the VU0 macro ops' results and flags (src/game/ovl/vu0_regs.h) as the
// port computed them before the work, on the host alone (no original, no level data: a unit batch).
//   fin_m        the FMAC result's clamp and its MAC / status flags, now built on the lanes reversed and packed into one
//                movemask, against the earlier lane-by-lane form (kept here as the reference), on random words of every
//                class (zero, denormal, the largest exponent, normal; both signs) and every lane mask, from random flags
//   local_flags  random sequences of the macro ops the hand-written routines use (add / sub / mul and their broadcast,
//                ACC and multiply-add forms, rotate / transform, the cross product's opmula / opmsub), run once on the
//                VU0's own flags (each op storing them) and once on a LocalFlags (the flags in locals, written back at the
//                scope's end: moby_joints_pose): the registers, ACC, the MAC and the status flags compared after
//   joints_simd  moby_joints_pose's (0x250fe0) four hot loops (the listed scales / translations lerped, the rotations lerped and
//                normalised, the joints' matrices under their parents, the bind matrices) in their SIMD form (the registers in
//                locals, RAC1_VU0_SIMD) against their per-op form, on random registers, flags, scratchpad records, parents,
//                lists and keys: the registers, ACC, Q, the flags, the scratchpad and the work buffer compared after; the
//                matrices' AVX2 form too when the CPU has AVX2
// Run: build/tests/ovl_oracle --batch 470 (any level: it reads nothing of one).

#include "ovl_oracle.h"

#include <game/ovl/vu0_regs.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <vector>

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

}  // namespace rac1::oracle
namespace rac1 {
void moby_joints_pose_loop(int loop, int mode, uint32_t a, uint32_t b, int32_t n, bool plain);  // (moby_joints.cpp, testing)
}  // namespace rac1
namespace rac1::oracle {

namespace {
constexpr uint32_t kSpr = 0x70000000u, kWork = 0x01f80000u, kKeys = 0x01f90000u;
// a word of the pose's data: an op_float's bits, now and then any word
uint32_t pose_word() { return ri(0, 31) == 0 ? any_word() : std::bit_cast<uint32_t>(op_float()); }
// The scratchpad and the work buffer's window the loops touch, the VU0's registers and flags (Q too)
struct PoseState {
    Snap regs;
    float q = 0;
    std::vector<uint8_t> spr, work;
};
PoseState pose_state() {
    PoseState s{snap(), regs().q, std::vector<uint8_t>(0x4000), std::vector<uint8_t>(0x2000)};
    std::memcpy(s.spr.data(), ee_ram_ptr(kSpr), 0x4000);
    std::memcpy(s.work.data(), ee_ram_ptr(kWork - 0x1000), 0x2000);
    return s;
}
void pose_restore(const PoseState& s) {
    Regs& V = regs();
    V.vf = s.regs.vf;
    V.acc = s.regs.acc;
    V.macflag = s.regs.macflag;
    V.statusflag = s.regs.statusflag;
    V.q = s.q;
    std::memcpy(ee_ram_ptr(kSpr), s.spr.data(), 0x4000);
    std::memcpy(ee_ram_ptr(kWork - 0x1000), s.work.data(), 0x2000);
}
bool pose_same(const PoseState& x, const PoseState& y) {
    return same(x.regs, y.regs) && std::memcmp(&x.q, &y.q, 4) == 0 && x.spr == y.spr && x.work == y.work;
}
// a list of `k` distinct joint records (0x40 apart from the scratchpad's 0, of `n`) linked through +`link`, its head at `head`
void pose_list(uint32_t head, uint32_t link, int k, int n) {
    std::vector<uint32_t> js(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) js[static_cast<size_t>(i)] = kSpr + static_cast<uint32_t>(i) * 0x40;
    for (int i = n - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
    uint32_t at = head;
    for (int i = 0; i < std::min(k, n); ++i) {
        sw(at, js[static_cast<size_t>(i)]);
        at = js[static_cast<size_t>(i)] + link;
    }
    sw(at, 0);
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
    {  // moby_joints_pose's loops: the SIMD form against the per-op form
        static const char* const names[4] = {"lists", "rotations", "joint matrices", "bind matrices"};
        int bad = 0, cases = 0, wide = 0;
        int per[4] = {};
        const bool avx2 = __builtin_cpu_supports("avx2");
        for (int c = 0; c < 8000; ++c) {
            const int loop = c % 4;
            const int32_t n = ri(1, 64);
            for (int r = 1; r < 32; ++r)
                for (int k = 0; k < 4; ++k) V.vf[static_cast<size_t>(r)].v[k] = op_float();
            V.vf[0] = VUVector{};
            V.vf[0].v[3] = 1.0f;
            for (int k = 0; k < 4; ++k) V.acc.v[k] = op_float();
            if (ri(0, 1) == 0) {  // (as the pose sets them: vf4 = 1 1 1 0, vf5.x = -1, vf6 / vf7 = 1 - t / t)
                V.vf[4] = VUVector{};
                for (int k = 0; k < 3; ++k) V.vf[4].v[k] = 1.0f;
                V.vf[5].v[0] = -1.0f;
                const float t = rf(0.0f, 1.0f);
                V.vf[7].v[0] = t;
                V.vf[6].v[0] = 1.0f - t;
            }
            V.q = op_float();
            V.macflag = R() & 0xffff;
            V.statusflag = R() & 0xfff;
            for (uint32_t o = 0; o < 0x4000; o += 4) sw(kSpr + o, pose_word());
            for (uint32_t o = 0; o < 0x2000; o += 4) sw(kWork - 0x1000 + o, pose_word());
            for (uint32_t o = 0; o < 0x400; o += 4) sw(kKeys + o, R());
            for (int32_t j = 0; j < n; ++j) {  // (the records' scaled word and parent)
                const uint32_t jr = kSpr + static_cast<uint32_t>(j) * 0x40;
                sw(jr + 0x1c, ri(0, 2) == 0 ? R() | 1u : 0u);
                sw(jr + 0x2c, ri(0, 3) == 0 ? 0u : kSpr + static_cast<uint32_t>(ri(0, n - 1)) * 0x40);
            }
            if (loop == 0) {
                pose_list(kWork - 0x10, 0xc, ri(0, n), n);
                pose_list(kWork - 0xc, 0x3c, ri(0, n), n);
            }
            const bool plain = coin();
            const PoseState start = pose_state();
            moby_joints_pose_loop(loop, 0, loop == 0 ? kWork : kKeys, kKeys + 0x200, n, plain);
            const PoseState ref = pose_state();
            pose_restore(start);
            moby_joints_pose_loop(loop, 1, loop == 0 ? kWork : kKeys, kKeys + 0x200, n, plain);
            PoseState got = pose_state();
            if (pose_same(ref, got) && loop >= 2 && avx2) {  // (the matrices' AVX2 form as well)
                pose_restore(start);
                moby_joints_pose_loop(loop, 2, kKeys, kKeys + 0x200, n, plain);
                got = pose_state();
                ++wide;
            }
            ++cases;
            if (!pose_same(ref, got)) {
                ++per[loop];
                if (bad++ < 5) {
                    int r = -1;
                    for (int i = 0; i < 32 && r < 0; ++i)
                        if (std::memcmp(&ref.regs.vf[static_cast<size_t>(i)], &got.regs.vf[static_cast<size_t>(i)], 16) != 0) r = i;
                    std::printf("  joints_simd: case %d (%s, %d joints%s): first register differing vf%d, mac %04x / %04x, status %03x / %03x, scratchpad %s, "
                                "work %s\n",
                                c, names[loop], n, plain ? ", plain" : "", r, ref.regs.macflag, got.regs.macflag, ref.regs.statusflag, got.regs.statusflag,
                                ref.spr == got.spr ? "same" : "differs", ref.work == got.work ? "same" : "differs");
                }
            }
        }
        std::printf("%-26s 0x%08x  %s (%d/%d, %d with AVX2 too; differing: lists %d, rotations %d, matrices %d, bind %d)\n", "joints_simd", 0x250fe0u,
                    bad == 0 ? "ok" : "FAIL", cases - bad, cases, wide, per[0], per[1], per[2], per[3]);
        if (bad != 0) ++g_fail;
    }
    V = saved;
}

}  // namespace rac1::oracle
