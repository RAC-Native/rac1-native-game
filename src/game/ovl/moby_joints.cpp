// Ported overlay code: the moby joints (see moby_joints.h). Hand-written assembly kept in its
// control flow: VU0 macro ops on the shared VU0 register file (vu0_regs.h), the scratchpad DMA
// transfers done at their kick (so the original's waits on a sentinel word the transfer
// overwrites are already over), MMI unpacking of the s16 keys.
//
// Scratchpad while the pose is built: per joint 0x40 bytes at 0 (+0 the rotation quaternion, +0x10
// the scale (+0x1c set: scaled), +0x20 the translation (+0x2c the parent's record or 0), +0x30
// the second key's translation, +0x3c a list link), the keyframes from 0x2000, the bind matrices
// at 0x1c00. The RAM buffer `work`: +0 the lists' heads, +0x10 per joint 0x20 bytes (the scale
// keys); +0x1fc0 the transfer's DMA chain.
#include <game/ovl/moby_joints.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/vu0_regs.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(__x86_64__)
#include <immintrin.h>
#endif

namespace rac1 {

namespace {

constexpr uint32_t SPR = 0x70000000u;
using namespace vu0m;

// orig: (helper) a 32-bit op's result as the 64-bit register holds it
uint64_t sx(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
// orig: (helper) BLTZ / BGEZ on a 64-bit register
bool neg(uint64_t v) { return static_cast<int64_t>(v) < 0; }
// orig: (helper) QMTC2 of four words
void set_words(Vf& d, const uint32_t w[4]) {
    for (int i = 0; i < 4; ++i) d.v[i] = u2f(w[i]);
}
// orig: (helper) PEXTLH with zero then PSRLW 13: the four halfwords << 3, unsigned
void unpack_u3(Vf& d, uint64_t x) {
    uint32_t w[4];
    for (int i = 0; i < 4; ++i) w[i] = (static_cast<uint32_t>((x >> (16 * i)) & 0xffff) << 16) >> 13;
    set_words(d, w);
}
// orig: (helper) PEXTLH / PEXTUH with zero then PSRAW 16: the four halfwords sign-extended
void unpack_s16(Vf& d, uint64_t x) {
    uint32_t w[4];
    for (int i = 0; i < 4; ++i) w[i] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>((x >> (16 * i)) & 0xffff)));
    set_words(d, w);
}
// orig: (helper) VITOFn on the masked lanes
void itof_lanes(Vf& d, const Vf& s, int m, float scale) {
    lanes(d, m, [&](int i) { return itof(s.v[i], scale); });
}
// orig: (helper) VMR32.xyzw
void mr32(Vf& d, const Vf& s) {
    const Vf t = s;
    d.v[0] = t.v[1];
    d.v[1] = t.v[2];
    d.v[2] = t.v[3];
    d.v[3] = t.v[0];
}
// orig: (helper) the quaternion dot product into y (VADDAx.y / VMADDAz.y / VMADDw.y with vf4 = 1)
void dot_y(LocalFlags& fl, Vf& d) {
    Vf* const v = vf();
    addabc(fl, d, d.v[0], Y);
    maddabc(fl, v[4], d.v[2], Y);
    maddbc(fl, d, v[4], d.v[3], Y);
}
// orig: (helper) the four lanes summed into x (VADDAy.x / VMADDAz.x / VMADDw.x with vf4 = 1)
void sum_x(LocalFlags& fl, Vf& d) {
    Vf* const v = vf();
    addabc(fl, d, d.v[1], X);
    maddabc(fl, v[4], d.v[2], X);
    maddbc(fl, d, v[4], d.v[3], X);
}
// orig: (helper) two weighted quaternions summed (or, of opposite sense, subtracted) and
// normalised: vf16 the sum, vf17.x its squared length, vf18.y the dot
void blend_quats(LocalFlags& fl, int a, int b) {
    Vf* const v = vf();
    add(fl, v[16], v[a], v[b], XYZW);
    mul(fl, v[18], v[a], v[b], XYZW);
}
// orig: (helper) (the second half: the length, the shorter arc, Q = 1 / length)
void blend_quats_norm(LocalFlags& fl, int a, int b) {
    Vf* const v = vf();
    mul(fl, v[17], v[16], v[16], XYZW);
    dot_y(fl, v[18]);
    sum_x(fl, v[17]);
    if (neg(qlo(v[18]))) {
        sub(fl, v[16], v[a], v[b], XYZW);
        mul(fl, v[17], v[16], v[16], XYZW);
        sum_x(fl, v[17]);
    }
    q() = vrsqrt(v[0].v[3], v[17].v[0]);
}


// ---- The pose's hot loops with the VU0 registers held in SIMD registers (RAC1_VU0_SIMD, on by default) ----
// The same ops in the same order on the same lanes: each op's operands clamped as the VU0 reads them (cop4; a lane an earlier op
// of the loop wrote is clamped already, fin_m's), its result clamped and its flags kept by fin_m (LocalFlags), its written lanes
// blended into the register; only the loads of the registers from the VU0's state and their stores back between ops are gone
// (the registers live in locals through a loop; ACC too), and an operand clamped once for all its uses. The registers, ACC, Q and
// the flags are written back where the loop ends: the VU0's state is the per-op form's, word for word (oracle batch 470,
// joints_simd; batches 79 / 93 on the original). No FMA, no reassociation: each multiply and add rounds as one VU0 op.
const bool g_pose_simd = [] {
    const char* e = std::getenv("RAC1_VU0_SIMD");
    return e == nullptr || std::atoi(e) != 0;
}();
namespace simd {
using I = __m128i;
// orig: (helper) a register's four lanes / back into the register file
inline I ldv(const Vf& r) { return ld4(r); }
inline void stv(Vf& d, I x) { _mm_store_si128(reinterpret_cast<__m128i*>(&d), x); }
// orig: (helper) LQC2 / SQC2 as lanes
inline I lqv(uint32_t a) {
    const Vf t = lqc2(a);
    return ld4(t);
}
inline void sqv(uint32_t a, I x) {
    alignas(16) Vf t;
    stv(t, x);
    sqc2(a, t);
}
// orig: (helper) a lane broadcast (of clamped lanes: bc4's vuc)
template <int L>
inline I bc(I x) {
    return _mm_shuffle_epi32(x, _MM_SHUFFLE(L, L, L, L));
}
// orig: (helper) the lanes M of `nw`, the others `old`'s
template <int M>
inline I blend(I old, I nw) {
    if constexpr (M == 15) {
        (void)old;
        return nw;
    } else {
        const I k = _mm_set_epi32((M & 8) != 0 ? -1 : 0, (M & 4) != 0 ? -1 : 0, (M & 2) != 0 ? -1 : 0, (M & 1) != 0 ? -1 : 0);
        return _mm_or_si128(_mm_and_si128(k, nw), _mm_andnot_si128(k, old));
    }
}
// orig: (helper) an FMAC op writing the lanes M of `d` (its flags those lanes')
template <int M>
[[gnu::always_inline]] inline void op(LocalFlags& fl, I& d, I raw) {
    d = blend<M>(d, fin_m(fl, raw, M));
}
// orig: (helper) the same into a register whose lanes outside M nothing reads before they are written whole
template <int M>
[[gnu::always_inline]] inline I opt(LocalFlags& fl, I raw) {
    return fin_m(fl, raw, M);
}
inline I add(I a, I b) { return vadd4(a, b); }
inline I sub(I a, I b) { return vsub4(a, b); }
inline I mul(I a, I b) { return vmul4(a, b); }
// orig: (helper) PEXTLH / PSRAW 16 then VITOFn.xyzw: four halfwords, sign-extended, to floats times `scale` (cvtdq2ps and mulps
// round as the scalar cvtsi2ss / mulss in the thread's mode; the halfwords are exact, the scale a power of two)
inline I itof_s16(uint64_t x, float scale) {
    const I h = _mm_cvtsi64_si128(static_cast<long long>(x));
    const I w = _mm_srai_epi32(_mm_unpacklo_epi16(h, h), 16);
    return _mm_castps_si128(_mm_mul_ps(_mm_cvtepi32_ps(w), _mm_set1_ps(scale)));
}
inline float lane0(I x) { return u2f(static_cast<uint32_t>(_mm_cvtsi128_si32(x))); }
// orig: (helper) dot_y / sum_x on registers (vf4 = 1: its clamped lanes `c4`; `a` ACC)
inline void dot_y(LocalFlags& fl, I& d, I& a, I c4) {
    const I c = cop4(d);
    op<Y>(fl, a, add(c, bc<0>(c)));
    op<Y>(fl, a, add(a, mul(c4, bc<2>(c))));
    op<Y>(fl, d, add(a, mul(c4, bc<3>(c))));
}
inline void sum_x(LocalFlags& fl, I& d, I& a, I c4) {
    const I c = cop4(d);
    op<X>(fl, a, add(c, bc<1>(c)));
    op<X>(fl, a, add(a, mul(c4, bc<2>(c))));
    op<X>(fl, d, add(a, mul(c4, bc<3>(c))));
}
// orig: (helper) rotate / transform (vu0_regs.h) on clamped rows r0..r3 and point `s` (clamped)
[[gnu::always_inline]] inline I rotate(LocalFlags& fl, I& a, I r0, I r1, I r2, I s) {
    a = opt<XYZW>(fl, mul(r0, bc<0>(s)));
    a = opt<XYZW>(fl, add(a, mul(r1, bc<1>(s))));
    return opt<XYZW>(fl, add(a, mul(r2, bc<2>(s))));
}
[[gnu::always_inline]] inline I transform(LocalFlags& fl, I& a, I r0, I r1, I r2, I r3, I s, I w) {
    a = opt<XYZW>(fl, mul(r0, bc<0>(s)));
    a = opt<XYZW>(fl, add(a, mul(r1, bc<1>(s))));
    a = opt<XYZW>(fl, add(a, mul(r2, bc<2>(s))));
    return opt<XYZW>(fl, add(a, mul(r3, w)));
}
// orig: (helper) a joint's matrix under its parent's: rotate x3 (the rows `r`, the points p0..p2) and transform (p3, w): o0..o3
[[gnu::always_inline]] inline void compose_joint(LocalFlags& fl, I& a, const I r[4], const I p[4], I w, I o[4]) {
    o[0] = rotate(fl, a, r[0], r[1], r[2], p[0]);
    o[1] = rotate(fl, a, r[0], r[1], r[2], p[1]);
    o[2] = rotate(fl, a, r[0], r[1], r[2], p[2]);
    o[3] = transform(fl, a, r[0], r[1], r[2], r[3], p[3], w);
}
// orig: (helper) a joint's matrix times its bind matrix: transform (p3, w) first, then rotate x3 (p0..p2): o0..o3
[[gnu::always_inline]] inline void compose_bind(LocalFlags& fl, I& a, const I r[4], const I p[4], I w, I o[4]) {
    o[3] = transform(fl, a, r[0], r[1], r[2], r[3], p[3], w);
    o[0] = rotate(fl, a, r[0], r[1], r[2], p[0]);
    o[1] = rotate(fl, a, r[0], r[1], r[2], p[1]);
    o[2] = rotate(fl, a, r[0], r[1], r[2], p[2]);
}
}  // namespace simd

// The same two with AVX2 (on a CPU that has it, any build: the functions are compiled for it alone, chosen at the start): the four
// chains are independent but for ACC, which each one's first op writes, and the flags, whose sticky bits are an OR and whose last
// are the last op's - so two chains run side by side in the halves of a 256-bit register, each half the 128-bit form's
// arithmetic, ACC and the flags taken from the half and the op the sequential order ends with.
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#define RAC1_POSE_AVX2 1
namespace simd2 {
using simd::I;
using J = __m256i;
#define RAC1_T_AVX2 __attribute__((target("avx2")))
RAC1_T_AVX2 inline J two(I lo, I hi) { return _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1); }
RAC1_T_AVX2 inline I lo(J x) { return _mm256_castsi256_si128(x); }
RAC1_T_AVX2 inline I hi(J x) { return _mm256_extracti128_si256(x, 1); }
template <int L>
RAC1_T_AVX2 inline J bc(J x) {
    return _mm256_shuffle_epi32(x, _MM_SHUFFLE(L, L, L, L));
}
RAC1_T_AVX2 inline J add(J a, J b) { return _mm256_castps_si256(_mm256_add_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b))); }
RAC1_T_AVX2 inline J mul(J a, J b) { return _mm256_castps_si256(_mm256_mul_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b))); }
// orig: (helper) fin_m (vu0_regs.h) of two XYZW ops at once: each half clamped, its flag bytes ORed into the sticky ones; the
// half `last_hi` (or the low one) the last op's
// (the flag bytes of the ops of one call: the sticky ones' OR, the last op's; the caller merges them into its LocalFlags)
struct Fl {
    I sticky = _mm_setzero_si128(), last = _mm_setzero_si128();
};
RAC1_T_AVX2 inline J fin2(Fl& fl, J v, bool last_hi) {
    const J vr = _mm256_shuffle_epi32(v, _MM_SHUFFLE(0, 1, 2, 3));
    const J expm = _mm256_set1_epi32(0x7f800000);
    const J e = _mm256_and_si256(vr, expm);
    const J isz = _mm256_cmpeq_epi32(e, _mm256_setzero_si256());
    const J isi = _mm256_cmpeq_epi32(e, expm);
    const J sgn = _mm256_srai_epi32(vr, 31);
    J r = _mm256_or_si256(vr, _mm256_and_si256(isi, _mm256_set1_epi32(0x007fffff)));
    r = _mm256_add_epi32(r, _mm256_slli_epi32(isi, 23));
    r = _mm256_andnot_si256(_mm256_and_si256(isz, _mm256_set1_epi32(0x7fffffff)), r);
    const J den = _mm256_andnot_si256(_mm256_cmpeq_epi32(_mm256_and_si256(vr, _mm256_set1_epi32(0x007fffff)), _mm256_setzero_si256()), isz);
    const J b = _mm256_packs_epi16(_mm256_packs_epi32(isz, sgn), _mm256_packs_epi32(den, isi));
    const I bl = lo(b), bh = hi(b);
    fl.last = last_hi ? bh : bl;
    fl.sticky = _mm_or_si128(fl.sticky, _mm_or_si128(bl, bh));
    return _mm256_shuffle_epi32(r, _MM_SHUFFLE(0, 1, 2, 3));
}
// orig: (helper) fin_m of one XYZW op into an Fl (as fin_m's LocalFlags form)
RAC1_T_AVX2 inline I fin1(Fl& fl, I v) {
    const I vr = _mm_shuffle_epi32(v, _MM_SHUFFLE(0, 1, 2, 3));
    const I expm = _mm_set1_epi32(0x7f800000);
    const I e = _mm_and_si128(vr, expm);
    const I isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    const I isi = _mm_cmpeq_epi32(e, expm);
    const I sgn = _mm_srai_epi32(vr, 31);
    I r = _mm_or_si128(vr, _mm_and_si128(isi, _mm_set1_epi32(0x007fffff)));
    r = _mm_add_epi32(r, _mm_slli_epi32(isi, 23));
    r = _mm_andnot_si128(_mm_and_si128(isz, _mm_set1_epi32(0x7fffffff)), r);
    const I den = _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(vr, _mm_set1_epi32(0x007fffff)), _mm_setzero_si128()), isz);
    const I b = _mm_packs_epi16(_mm_packs_epi32(isz, sgn), _mm_packs_epi32(den, isi));
    fl.last = b;
    fl.sticky = _mm_or_si128(fl.sticky, b);
    return _mm_shuffle_epi32(r, _MM_SHUFFLE(0, 1, 2, 3));
}
// orig: (helper) the three ops of two rotates (or a rotate and a transform's first three) side by side: rows `r` the same, points p
RAC1_T_AVX2 inline J rotate2(Fl& fl, J& a, J r0, J r1, J r2, J p, bool last_hi) {
    a = fin2(fl, mul(r0, bc<0>(p)), last_hi);
    a = fin2(fl, add(a, mul(r1, bc<1>(p))), last_hi);
    return fin2(fl, add(a, mul(r2, bc<2>(p))), last_hi);
}
// orig: (helper) compose_joint with AVX2: rotates 0 and 1 together, rotate 2 with the transform's first three ops, its last alone
RAC1_T_AVX2 __attribute__((noinline)) Fl compose_joint(I& a, const I r[4], const I p[4], I w, I o[4]) {
    Fl fl;
    const J r0 = two(r[0], r[0]), r1 = two(r[1], r[1]), r2 = two(r[2], r[2]);
    J acc2;
    const J d01 = rotate2(fl, acc2, r0, r1, r2, two(p[0], p[1]), true);
    const J d23 = rotate2(fl, acc2, r0, r1, r2, two(p[2], p[3]), true);
    o[0] = lo(d01);
    o[1] = hi(d01);
    o[2] = lo(d23);
    a = hi(d23);  // (the transform's third op: ACC)
    o[3] = fin1(fl, simd::add(a, simd::mul(r[3], w)));
    return fl;
}
// orig: (helper) compose_bind with AVX2: the transform's first three ops with rotate 0, its last, then rotates 1 and 2 together
RAC1_T_AVX2 __attribute__((noinline)) Fl compose_bind(I& a, const I r[4], const I p[4], I w, I o[4]) {
    Fl fl;
    const J r0 = two(r[0], r[0]), r1 = two(r[1], r[1]), r2 = two(r[2], r[2]);
    J acc2;
    const J d30 = rotate2(fl, acc2, r0, r1, r2, two(p[3], p[0]), false);
    o[0] = hi(d30);
    o[3] = fin1(fl, simd::add(lo(d30), simd::mul(r[3], w)));
    const J d12 = rotate2(fl, acc2, r0, r1, r2, two(p[1], p[2]), true);
    o[1] = lo(d12);
    o[2] = hi(d12);
    a = hi(acc2);  // (rotate 2's second op: ACC)
    return fl;
}
#undef RAC1_T_AVX2
}  // namespace simd2
#endif
// (the pose's matrices: 0 the 128-bit form, 1 AVX2 - chosen at the start on a CPU that has it, whatever the build's target;
// RAC1_VU0_SIMD=1 keeps the 128-bit form)
int g_pose_wide = [] {
#ifdef RAC1_POSE_AVX2
    __builtin_cpu_init();  // (a static's initialiser: before the CPU's features may have been read)
    const char* e = std::getenv("RAC1_VU0_SIMD");
    return (e == nullptr || std::atoi(e) >= 2) && __builtin_cpu_supports("avx2") ? 1 : 0;
#else
    return 0;
#endif
}();

// orig: (helper) the listed scales and translations lerped by t (1 - t in vf6.x, t in vf7.x): ACC = a (1 - t), vf10 = ACC + b t on
// x y z (vf10.w the 1 set before); the scales' list from *(t5 - 0x10) (+0 / +0x10, the link +0xc, the result into +0x10), then
// the translations' from *(t5 - 0xc) (a joint +0x20 / +0x30, the link +0x3c, the result into +0x20, its +0x2c kept)
[[gnu::always_inline]] inline void lerp_lists_simd(LocalFlags& fl, uint32_t t5) {
    using namespace simd;
    Vf* const v = vf();
    const I b6 = bc<0>(cop4(ldv(v[6]))), b7 = bc<0>(cop4(ldv(v[7])));
    I a = ldv(acc()), r8 = ldv(v[8]), r9 = ldv(v[9]), r10 = ldv(v[10]);
    for (uint32_t p = lwu(t5 - 0x10); p != 0;) {
        r8 = lqv(p);
        r9 = lqv(p + 0x10);
        op<XYZ>(fl, a, mul(cop4(r8), b6));
        op<XYZ>(fl, r10, add(a, mul(cop4(r9), b7)));
        const uint32_t nx = lwu(p + 0xc);
        sqv(p + 0x10, r10);
        p = nx;
    }
    for (uint32_t p = lwu(t5 - 0xc); p != 0;) {
        r8 = lqv(p + 0x20);
        r9 = lqv(p + 0x30);
        op<XYZ>(fl, a, mul(cop4(r8), b6));
        op<XYZ>(fl, r10, add(a, mul(cop4(r9), b7)));
        const uint32_t keep = lwu(p + 0x2c);
        sqv(p + 0x20, r10);
        sw(p + 0x2c, keep);
        p = lwu(p + 0x3c);
    }
    stv(acc(), a);
    stv(v[8], r8);
    stv(v[9], r9);
    stv(v[10], r10);
}

// orig: (helper) the rotations lerped two joints at a time (the first key's at `at`, the second's at `k2`), along the shorter arc
// and normalised unless `plain`: the scratchpad's joint records from 0 (+0, every 0x40)
[[gnu::always_inline]] inline void lerp_rotations_simd(LocalFlags& fl, int32_t n, uint32_t at, uint32_t k2, bool plain) {
    using namespace simd;
    Vf* const v = vf();
    const I b6 = bc<0>(cop4(ldv(v[6]))), b7 = bc<0>(cop4(ldv(v[7]))), b5 = bc<0>(cop4(ldv(v[5])));
    const I c4 = cop4(ldv(v[4]));
    const float one = v[0].v[3];
    I a = ldv(acc());
    I r8 = ldv(v[8]), r9 = ldv(v[9]), r10 = ldv(v[10]), r11 = ldv(v[11]), r12 = ldv(v[12]), r13 = ldv(v[13]), r14 = ldv(v[14]), r15 = ldv(v[15]);
    float qv = q();
    uint32_t p = SPR - 0x80;
    for (int32_t c = n; c > 0; c -= 2) {
        const uint64_t qa_lo = ld(at), qa_hi = ld(at + 8), qb_lo = ld(k2), qb_hi = ld(k2 + 8);
        r8 = itof_s16(qa_lo, 1.0f / 32768.0f);
        r9 = itof_s16(qb_lo, 1.0f / 32768.0f);
        r10 = itof_s16(qa_hi, 1.0f / 32768.0f);
        r11 = itof_s16(qb_hi, 1.0f / 32768.0f);
        p += 0x80;
        at += 0x10;
        k2 += 0x10;
        const I c8 = cop4(r8), c9 = cop4(r9), c10 = cop4(r10), c11 = cop4(r11);
        if (plain) {
            a = opt<XYZW>(fl, mul(c8, b6));
            r8 = opt<XYZW>(fl, add(a, mul(c9, b7)));
            a = opt<XYZW>(fl, mul(c10, b6));
            r10 = opt<XYZW>(fl, add(a, mul(c11, b7)));
        } else {
            r12 = opt<XYZW>(fl, mul(c8, c9));
            r13 = opt<XYZW>(fl, mul(c10, c11));
            r14 = opt<XYZW>(fl, mul(c9, b5));
            r15 = opt<XYZW>(fl, mul(c11, b5));
            a = opt<XYZW>(fl, mul(c8, b6));
            r8 = opt<XYZW>(fl, add(a, mul(c9, b7)));
            r9 = opt<XYZW>(fl, add(a, mul(r14, b7)));
            dot_y(fl, r12, a, c4);
            dot_y(fl, r13, a, c4);
            a = opt<XYZW>(fl, mul(c10, b6));
            const bool na = _mm_cvtsi128_si32(_mm_srli_si128(r12, 4)) < 0;  // (qlo(vf12) < 0: the y lane's sign)
            r10 = opt<XYZW>(fl, add(a, mul(c11, b7)));
            r11 = opt<XYZW>(fl, add(a, mul(r15, b7)));
            const bool nb = _mm_cvtsi128_si32(_mm_srli_si128(r13, 4)) < 0;
            if (!na) {
                r9 = opt<XYZW>(fl, mul(r8, r8));
            } else {
                r8 = r9;
                r9 = opt<XYZW>(fl, mul(r9, r9));
            }
            if (!nb) {
                r11 = opt<XYZW>(fl, mul(r10, r10));
            } else {
                r10 = r11;
                r11 = opt<XYZW>(fl, mul(r11, r11));
            }
            sum_x(fl, r9, a, c4);
            sum_x(fl, r11, a, c4);
            qv = vrsqrt(one, lane0(r9));
            r8 = opt<XYZW>(fl, mul(r8, _mm_set1_epi32(static_cast<int>(f2u(vuc(qv))))));
            qv = vrsqrt(one, lane0(r11));
            r10 = opt<XYZW>(fl, mul(r10, _mm_set1_epi32(static_cast<int>(f2u(vuc(qv))))));
        }
        sqv(p, r8);
        sqv(p + 0x40, r10);
    }
    stv(acc(), a);
    stv(v[8], r8);
    stv(v[9], r9);
    stv(v[10], r10);
    stv(v[11], r11);
    stv(v[12], r12);
    stv(v[13], r13);
    stv(v[14], r14);
    stv(v[15], r15);
    q() = qv;
}

// orig: (helper) the joints' matrices from their quaternion (+0), scale (+0x10, +0x1c set: scaled) and translation (+0x20) under
// their parent's (+0x2c), the scratchpad's joint records from 0 to `last`
[[gnu::always_inline]] inline void joint_matrices_simd(LocalFlags& fl, uint32_t last) {
    using namespace simd;
    Vf* const v = vf();
    const I V0 = ldv(v[0]), V1 = ldv(v[1]), V2 = ldv(v[2]), V3 = ldv(v[3]);
    const I C0 = cop4(V0), w1 = bc<3>(C0);
    I a = ldv(acc());
    I r8 = ldv(v[8]), r9 = ldv(v[9]), r10 = ldv(v[10]), r11 = ldv(v[11]), r12 = ldv(v[12]), r13 = ldv(v[13]), r14 = ldv(v[14]), r15 = ldv(v[15]);
    I r16 = ldv(v[16]), r17 = ldv(v[17]), r18 = ldv(v[18]), r19 = ldv(v[19]), r20 = ldv(v[20]), r21 = ldv(v[21]), r22 = ldv(v[22]), r23 = ldv(v[23]);
    for (uint32_t a3 = SPR;;) {
        r10 = lqv(a3);
        const I c10 = cop4(r10);
        r16 = V1;
        r11 = opt<XYZW>(fl, add(c10, c10));
        r17 = V2;
        r18 = V3;
        r19 = V0;
        // (vf12 .. vf15's lanes these ops leave unwritten are never read: the four are loaded whole below)
        const I t12 = opt<XYZ>(fl, mul(r11, bc<3>(c10)));
        const I t13 = opt<XYZ>(fl, mul(r11, bc<0>(c10)));
        const I t14 = opt<YZ>(fl, mul(r11, bc<1>(c10)));
        const I t15 = opt<Z>(fl, mul(r11, bc<2>(c10)));
        op<X>(fl, r17, add(C0, bc<2>(t12)));
        op<X>(fl, r18, sub(C0, bc<1>(t12)));
        op<Y>(fl, r18, add(C0, bc<0>(t12)));
        op<X>(fl, r16, sub(cop4(r16), bc<1>(t14)));
        op<Y>(fl, r17, sub(cop4(r17), bc<0>(t13)));
        op<Z>(fl, r18, sub(cop4(r18), bc<0>(t13)));
        op<Y>(fl, r16, sub(t13, bc<2>(t12)));
        op<Z>(fl, r16, add(t13, bc<1>(t12)));
        op<Z>(fl, r17, sub(t14, bc<0>(t12)));
        op<X>(fl, r17, add(cop4(r17), bc<1>(t13)));
        op<X>(fl, r18, add(cop4(r18), bc<2>(t13)));
        op<Y>(fl, r18, add(cop4(r18), bc<2>(t14)));
        a3 += 0x40;
        op<X>(fl, r16, sub(cop4(r16), bc<2>(t15)));
        const uint32_t scaled = lwu(a3 - 0x24);
        op<Y>(fl, r17, sub(cop4(r17), bc<2>(t15)));
        const uint32_t parent = lwu(a3 - 0x14);
        op<Z>(fl, r18, sub(cop4(r18), bc<1>(t14)));
        r8 = lqv(a3 - 0x30);
        r9 = lqv(a3 - 0x20);
        if (scaled != 0) {
            const I c8 = cop4(r8);
            op<XYZ>(fl, r16, mul(cop4(r16), bc<0>(c8)));
            op<XYZ>(fl, r17, mul(cop4(r17), bc<1>(c8)));
            op<XYZ>(fl, r18, mul(cop4(r18), bc<2>(c8)));
        }
        r12 = V1;
        if (parent == 0) {
            r13 = V2;
            r14 = V3;
            r15 = V0;
        } else {
            r12 = lqv(parent);
            r13 = lqv(parent + 0x10);
            r14 = lqv(parent + 0x20);
            r15 = lqv(parent + 0x30);
        }
        op<XYZ>(fl, r19, add(cop4(r19), cop4(r9)));
        const I rows[4] = {cop4(r12), cop4(r13), cop4(r14), cop4(r15)};
        const I pts[4] = {cop4(r16), cop4(r17), cop4(r18), cop4(r19)};
        I o[4];
#ifdef RAC1_POSE_AVX2
        if (g_pose_wide != 0) {
            const simd2::Fl f = simd2::compose_joint(a, rows, pts, w1, o);
            fl.sticky = _mm_or_si128(fl.sticky, f.sticky);
            fl.last = f.last;
            fl.pending = true;
        } else
#endif
        {
            compose_joint(fl, a, rows, pts, w1, o);
        }
        r20 = o[0];
        r21 = o[1];
        r22 = o[2];
        r23 = o[3];
        sqv(a3 - 0x40, r20);
        sqv(a3 - 0x30, r21);
        sqv(a3 - 0x20, r22);
        sqv(a3 - 0x10, r23);
        if (a3 == last) break;
    }
    stv(acc(), a);
    const I* const out[] = {&r8, &r9, &r10, &r11, &r12, &r13, &r14, &r15, &r16, &r17, &r18, &r19, &r20, &r21, &r22, &r23};
    for (int i = 0; i < 16; ++i) stv(v[8 + i], *out[i]);
}

// orig: (helper) each joint's matrix (from 0) times its bind matrix (from 0x1c00), to `last`
[[gnu::always_inline]] inline void bind_matrices_simd(LocalFlags& fl, uint32_t last) {
    using namespace simd;
    Vf* const v = vf();
    const I w1 = bc<3>(cop4(ldv(v[0])));
    I a = ldv(acc());
    I r12 = ldv(v[12]), r13 = ldv(v[13]), r14 = ldv(v[14]), r15 = ldv(v[15]), r16 = ldv(v[16]), r17 = ldv(v[17]), r18 = ldv(v[18]), r19 = ldv(v[19]);
    I r20 = ldv(v[20]), r21 = ldv(v[21]), r22 = ldv(v[22]), r23 = ldv(v[23]);
    uint32_t b = SPR + 0x1c00, m = SPR;
    do {
        r15 = lqv(b + 0x30);
        r14 = lqv(b + 0x20);
        r13 = lqv(b + 0x10);
        r12 = lqv(b);
        r20 = lqv(m);
        r21 = lqv(m + 0x10);
        r22 = lqv(m + 0x20);
        r23 = lqv(m + 0x30);
        const I rows[4] = {cop4(r20), cop4(r21), cop4(r22), cop4(r23)};
        const I pts[4] = {cop4(r12), cop4(r13), cop4(r14), cop4(r15)};
        I o[4];
#ifdef RAC1_POSE_AVX2
        if (g_pose_wide != 0) {
            const simd2::Fl f = simd2::compose_bind(a, rows, pts, w1, o);
            fl.sticky = _mm_or_si128(fl.sticky, f.sticky);
            fl.last = f.last;
            fl.pending = true;
        } else
#endif
        {
            compose_bind(fl, a, rows, pts, w1, o);
        }
        r16 = o[0];
        r17 = o[1];
        r18 = o[2];
        r19 = o[3];
        sqv(m, r16);
        sqv(m + 0x10, r17);
        sqv(m + 0x20, r18);
        b += 0x40;
        sqv(m + 0x30, r19);
        m += 0x40;
    } while (m != last);
    stv(acc(), a);
    const I* const out[] = {&r12, &r13, &r14, &r15, &r16, &r17, &r18, &r19, &r20, &r21, &r22, &r23};
    for (int i = 0; i < 12; ++i) stv(v[12 + i], *out[i]);
}

// orig: (helper) the per-op form of lerp_lists_simd (the VU0's registers in its state between ops)
[[gnu::always_inline]] inline void lerp_lists_ref(LocalFlags& fl, uint32_t t5) {
    Vf* const v = vf();
    for (uint32_t p = lwu(t5 - 0x10); p != 0;) {
        v[8] = lqc2(p);
        v[9] = lqc2(p + 0x10);
        mulabc(fl, v[8], v[6].v[0], XYZ);
        maddbc(fl, v[10], v[9], v[7].v[0], XYZ);
        const uint32_t nx = lwu(p + 0xc);
        sqc2(p + 0x10, v[10]);
        p = nx;
    }
    for (uint32_t p = lwu(t5 - 0xc); p != 0;) {
        v[8] = lqc2(p + 0x20);
        v[9] = lqc2(p + 0x30);
        mulabc(fl, v[8], v[6].v[0], XYZ);
        maddbc(fl, v[10], v[9], v[7].v[0], XYZ);
        const uint32_t keep = lwu(p + 0x2c);
        sqc2(p + 0x20, v[10]);
        sw(p + 0x2c, keep);
        p = lwu(p + 0x3c);
    }
}

// orig: (helper) the per-op form of lerp_rotations_simd
[[gnu::always_inline]] inline void lerp_rotations_ref(LocalFlags& fl, int32_t n, uint32_t at, uint32_t k2, bool plain) {
    Vf* const v = vf();
    uint32_t p = SPR - 0x80;
    for (int32_t c = n; c > 0; c -= 2) {
        const uint64_t qa_lo = ld(at), qa_hi = ld(at + 8), qb_lo = ld(k2), qb_hi = ld(k2 + 8);
        unpack_s16(v[8], qa_lo);
        unpack_s16(v[9], qb_lo);
        unpack_s16(v[10], qa_hi);
        unpack_s16(v[11], qb_hi);
        itof_lanes(v[8], v[8], XYZW, 1.0f / 32768.0f);
        itof_lanes(v[9], v[9], XYZW, 1.0f / 32768.0f);
        itof_lanes(v[10], v[10], XYZW, 1.0f / 32768.0f);
        itof_lanes(v[11], v[11], XYZW, 1.0f / 32768.0f);
        p += 0x80;
        at += 0x10;
        k2 += 0x10;
        if (plain) {
            mulabc(fl, v[8], v[6].v[0], XYZW);
            maddbc(fl, v[8], v[9], v[7].v[0], XYZW);
            mulabc(fl, v[10], v[6].v[0], XYZW);
            maddbc(fl, v[10], v[11], v[7].v[0], XYZW);
        } else {
            mul(fl, v[12], v[8], v[9], XYZW);
            mul(fl, v[13], v[10], v[11], XYZW);
            mulbc(fl, v[14], v[9], v[5].v[0], XYZW);
            mulbc(fl, v[15], v[11], v[5].v[0], XYZW);
            mulabc(fl, v[8], v[6].v[0], XYZW);
            maddbc(fl, v[8], v[9], v[7].v[0], XYZW);
            maddbc(fl, v[9], v[14], v[7].v[0], XYZW);
            dot_y(fl, v[12]);
            dot_y(fl, v[13]);
            mulabc(fl, v[10], v[6].v[0], XYZW);
            const uint64_t da = qlo(v[12]);
            maddbc(fl, v[10], v[11], v[7].v[0], XYZW);
            maddbc(fl, v[11], v[15], v[7].v[0], XYZW);
            const uint64_t db = qlo(v[13]);
            if (!neg(da)) {
                mul(fl, v[9], v[8], v[8], XYZW);
            } else {
                v[8] = v[9];
                mul(fl, v[9], v[9], v[9], XYZW);
            }
            if (!neg(db)) {
                mul(fl, v[11], v[10], v[10], XYZW);
            } else {
                v[10] = v[11];
                mul(fl, v[11], v[11], v[11], XYZW);
            }
            sum_x(fl, v[9]);
            sum_x(fl, v[11]);
            q() = vrsqrt(v[0].v[3], v[9].v[0]);
            mulbc(fl, v[8], v[8], q(), XYZW);
            q() = vrsqrt(v[0].v[3], v[11].v[0]);
            mulbc(fl, v[10], v[10], q(), XYZW);
        }
        sqc2(p, v[8]);
        sqc2(p + 0x40, v[10]);
    }
}

// orig: (helper) the per-op form of joint_matrices_simd
[[gnu::always_inline]] inline void joint_matrices_ref(LocalFlags& fl, uint32_t last) {
    Vf* const v = vf();
    for (uint32_t a3 = SPR;;) {
        v[10] = lqc2(a3);
        v[16] = v[1];
        add(fl, v[11], v[10], v[10], XYZW);
        v[17] = v[2];
        v[18] = v[3];
        v[19] = v[0];
        mulbc(fl, v[12], v[11], v[10].v[3], XYZ);
        mulbc(fl, v[13], v[11], v[10].v[0], XYZ);
        mulbc(fl, v[14], v[11], v[10].v[1], YZ);
        mulbc(fl, v[15], v[11], v[10].v[2], Z);
        addbc(fl, v[17], v[0], v[12].v[2], X);
        subbc(fl, v[18], v[0], v[12].v[1], X);
        addbc(fl, v[18], v[0], v[12].v[0], Y);
        subbc(fl, v[16], v[16], v[14].v[1], X);
        subbc(fl, v[17], v[17], v[13].v[0], Y);
        subbc(fl, v[18], v[18], v[13].v[0], Z);
        subbc(fl, v[16], v[13], v[12].v[2], Y);
        addbc(fl, v[16], v[13], v[12].v[1], Z);
        subbc(fl, v[17], v[14], v[12].v[0], Z);
        addbc(fl, v[17], v[17], v[13].v[1], X);
        addbc(fl, v[18], v[18], v[13].v[2], X);
        addbc(fl, v[18], v[18], v[14].v[2], Y);
        a3 += 0x40;
        subbc(fl, v[16], v[16], v[15].v[2], X);
        const uint32_t scaled = lwu(a3 - 0x24);
        subbc(fl, v[17], v[17], v[15].v[2], Y);
        const uint32_t parent = lwu(a3 - 0x14);
        subbc(fl, v[18], v[18], v[14].v[1], Z);
        v[8] = lqc2(a3 - 0x30);
        v[9] = lqc2(a3 - 0x20);
        if (scaled != 0) {
            mulbc(fl, v[16], v[16], v[8].v[0], XYZ);
            mulbc(fl, v[17], v[17], v[8].v[1], XYZ);
            mulbc(fl, v[18], v[18], v[8].v[2], XYZ);
        }
        v[12] = v[1];
        if (parent == 0) {
            v[13] = v[2];
            v[14] = v[3];
            v[15] = v[0];
        } else {
            v[12] = lqc2(parent);
            v[13] = lqc2(parent + 0x10);
            v[14] = lqc2(parent + 0x20);
            v[15] = lqc2(parent + 0x30);
        }
        add(fl, v[19], v[19], v[9], XYZ);
        rotate(fl, v[20], 12, v[16], XYZW);
        rotate(fl, v[21], 12, v[17], XYZW);
        rotate(fl, v[22], 12, v[18], XYZW);
        transform(fl, v[23], 12, v[19], v[0].v[3]);
        sqc2(a3 - 0x40, v[20]);
        sqc2(a3 - 0x30, v[21]);
        sqc2(a3 - 0x20, v[22]);
        sqc2(a3 - 0x10, v[23]);
        if (a3 == last) break;
    }
}

// orig: (helper) the per-op form of bind_matrices_simd
[[gnu::always_inline]] inline void bind_matrices_ref(LocalFlags& fl, uint32_t last) {
    Vf* const v = vf();
    uint32_t b = SPR + 0x1c00, m = SPR;
    do {
        v[15] = lqc2(b + 0x30);
        v[14] = lqc2(b + 0x20);
        v[13] = lqc2(b + 0x10);
        v[12] = lqc2(b);
        v[20] = lqc2(m);
        v[21] = lqc2(m + 0x10);
        v[22] = lqc2(m + 0x20);
        v[23] = lqc2(m + 0x30);
        transform(fl, v[19], 20, v[15], v[0].v[3]);
        rotate(fl, v[16], 20, v[12], XYZW);
        rotate(fl, v[17], 20, v[13], XYZW);
        rotate(fl, v[18], 20, v[14], XYZW);
        sqc2(m, v[16]);
        sqc2(m + 0x10, v[17]);
        sqc2(m + 0x20, v[18]);
        b += 0x40;
        sqc2(m + 0x30, v[19]);
        m += 0x40;
    } while (m != last);
}

// ---- The pose's hot loops in plain floats (RAC1_WIIU_FAST_POSE: the Wii U's default, 2026-10-28) ----
// The same arithmetic on the pose's data without the VU0's rules: no operand / result clamps (the pose's numbers are never
// denormal, infinite or NaN), no MAC / status flags, the scratchpad reached through its host pointer, the s16 keys made floats
// without an integer conversion (slow on the 750). Mode 1: the multiply-adds of the matrices, the lerps and the lengths fused (one
// rounding: fmadds on the Wii U), 1 / sqrt from the CPU's estimate and Newton steps (no fsqrt on the 750: sqrtf is software);
// the matrices then differ from the VU0's by rounding alone (the check's numbers: PERFORMANCE.md). Mode 2 ("exact"): each op
// rounded as the VU0's and its sqrt - the PC's check finds every matrix the VU0 path's, bit for bit. Either way the quaternions'
// dot (whose sign picks the shorter arc) and the rotation matrices' terms are the VU0's own ops.
// The VU0's registers these loops use as scratch (vf8 .. vf23, ACC, Q) and its flags are left as they were: nothing reads them
// after (the pose's later ops load or set theirs first, the skinning loads its own; the game never reads the status flag -
// no CFC2 of it, the VU0 programs test none - and its MAC reads each follow an op of their own). The matrices are read by the
// skinning, the MRP's pose observer and the strafe's hook only: all drawing, no game logic.
// orig: (host)
}  // namespace
int g_pose_fast = [] {
    const char* e = std::getenv("RAC1_WIIU_FAST_POSE");
#if defined(__WIIU__)
    return e == nullptr ? 1 : std::atoi(e);
#else
    return e == nullptr ? 0 : std::atoi(e);  // (the PC: the VU0's path unless asked)
#endif
}();
namespace {
namespace fast {
// orig: (helper) a multiply-add a b + c: fused (the Wii U: fmadds; the PC: fmaf, the same value), or (X, exact) two roundings
template <bool X>
inline float madd(float a, float b, float c) {
    if constexpr (X) return a * b + c;
    else return __builtin_fmaf(a, b, c);
}
// orig: (helper) 1 / sqrt(x) of a quaternion's squared length (~0.5 .. 1 here): the estimate (the 750's frsqrte; on the PC a stand-in
// as coarse as the architecture lets it be, 5 bits) and three Newton steps (5 -> 10 -> 20 -> 24 bits: with two the PC's check
// found the rotation rows up to 1e-5 off, with three 1e-6 - the fused ops' own rounding); a zero quaternion's 0: the largest
// float, the VU0's. X: 1 / sqrtf, the VU0 path's VRSQRT
template <bool X>
inline float rsqrt(float x) {
    if (!(x > 0.0f)) return u2f(0x7f7fffffu);
    if constexpr (X) return 1.0f / std::sqrt(x);
#if defined(__WIIU__)
    double e;
    __asm__("frsqrte %0,%1" : "=f"(e) : "f"(static_cast<double>(x)));
    float y = static_cast<float>(e);
#else
    float y = u2f(f2u(1.0f / std::sqrt(x)) & 0xfffc0000u);
#endif
    const float h = 0.5f * x;
    y = y * madd<false>(-h * y, y, 1.5f);
    y = y * madd<false>(-h * y, y, 1.5f);
    return y * madd<false>(-h * y, y, 1.5f);
}
// orig: (helper) a halfword (the low 16 bits of `h`) sign-extended, times 2^-15 (PSRAW 16, VITOF15): exactly, by the float
// whose mantissa holds it (256 + (h ^ 0x8000) 2^-15, then less 257) - no integer conversion (slow on the 750)
inline float s16f(uint32_t h) { return u2f(0x43800000u | ((h ^ 0x8000u) & 0xffffu)) - 257.0f; }
// orig: (helper) a quadword of EE memory (the scratchpad, a RAM buffer) as four host floats; a word of it
inline float* q4(uint32_t a) { return static_cast<float*>(ee_ram_ptr(a)); }
inline uint32_t word(const float* p) {
    uint32_t u;
    std::memcpy(&u, p, 4);
    return u;
}
// orig: (helper) a row (four lanes) times the point p: r0 p.x + r1 p.y + r2 p.z (VMULAx / VMADDAy / VMADDz, vu0_regs.h's rotate),
// into o
template <bool X>
[[gnu::always_inline]] inline void row(const float* r, float x, float y, float z, float* o) {
    o[0] = madd<X>(r[8], z, madd<X>(r[4], y, r[0] * x));
    o[1] = madd<X>(r[9], z, madd<X>(r[5], y, r[1] * x));
    o[2] = madd<X>(r[10], z, madd<X>(r[6], y, r[2] * x));
    o[3] = madd<X>(r[11], z, madd<X>(r[7], y, r[3] * x));
}
// orig: (helper) the rows r (16 floats) times the points p (16): out row i = r0 p_i.x + r1 p_i.y + r2 p_i.z, row 3 + r3 (w = 1:
// rotate x3 and transform); `o` written after r and p are read whole (it may be r)
template <bool X>
[[gnu::always_inline]] inline void compose(const float* r, const float* p, float* o) {
    float t[16];
    row<X>(r, p[0], p[1], p[2], t);
    row<X>(r, p[4], p[5], p[6], t + 4);
    row<X>(r, p[8], p[9], p[10], t + 8);
    row<X>(r, p[12], p[13], p[14], t + 12);
    t[12] += r[12];
    t[13] += r[13];
    t[14] += r[14];
    t[15] += r[15];
#pragma GCC unroll 16
    for (int i = 0; i < 16; ++i) o[i] = t[i];
}
const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}  // namespace fast

// orig: (helper) lerp_lists_simd in plain floats (the result's w lane, in the scales' list, vf10.w: 1)
template <bool X>
[[gnu::noinline]] void lerp_lists_fast(uint32_t t5) {
    Vf* const v = vf();
    const float t0 = v[6].v[0], t1 = v[7].v[0], w = v[10].v[3];
    for (uint32_t p = lwu(t5 - 0x10); p != 0;) {
        float* const s = fast::q4(p);
        s[4] = fast::madd<X>(s[4], t1, s[0] * t0);
        s[5] = fast::madd<X>(s[5], t1, s[1] * t0);
        s[6] = fast::madd<X>(s[6], t1, s[2] * t0);
        s[7] = w;
        p = fast::word(s + 3);
    }
    for (uint32_t p = lwu(t5 - 0xc); p != 0;) {
        float* const s = fast::q4(p);
        s[8] = fast::madd<X>(s[12], t1, s[8] * t0);
        s[9] = fast::madd<X>(s[13], t1, s[9] * t0);
        s[10] = fast::madd<X>(s[14], t1, s[10] * t0);
        p = fast::word(s + 15);
    }
}

// orig: (helper) lerp_rotations_simd in plain floats: per joint the keys' quaternions (s16), their dot (the VU0's products and sums:
// its sign picks the arc), lerped, normalised unless `plain`
template <bool X>
[[gnu::noinline]] void lerp_rotations_fast(int32_t n, uint32_t at, uint32_t k2, bool plain) {
    Vf* const v = vf();
    const float t0 = v[6].v[0], t1 = v[7].v[0];
    float* o = fast::q4(SPR);
    const float* ka = fast::q4(at);
    const float* kb = fast::q4(k2);
    for (int32_t c = (n + 1) & ~1; c > 0; --c, o += 16, ka += 2, kb += 2) {  // (the original's two joints a step: an odd count's pair whole)
        const uint32_t wa0 = fast::word(ka), wa1 = fast::word(ka + 1), wb0 = fast::word(kb), wb1 = fast::word(kb + 1);
        const float a0 = fast::s16f(wa0), a1 = fast::s16f(wa0 >> 16), a2 = fast::s16f(wa1), a3 = fast::s16f(wa1 >> 16);
        const float b0 = fast::s16f(wb0), b1 = fast::s16f(wb0 >> 16), b2 = fast::s16f(wb1), b3 = fast::s16f(wb1 >> 16);
        float tb = t1;
        if (!plain) {
            const float d = ((a1 * b1 + a0 * b0) + a2 * b2) + a3 * b3;
            if ((f2u(d) >> 31) != 0) tb = -t1;
        }
        float r0 = fast::madd<X>(b0, tb, a0 * t0), r1 = fast::madd<X>(b1, tb, a1 * t0), r2 = fast::madd<X>(b2, tb, a2 * t0), r3 = fast::madd<X>(b3, tb, a3 * t0);
        if (!plain) {
            const float q = fast::rsqrt<X>(fast::madd<X>(r3, r3, fast::madd<X>(r2, r2, fast::madd<X>(r1, r1, r0 * r0))));
            r0 *= q;
            r1 *= q;
            r2 *= q;
            r3 *= q;
        }
        o[0] = r0;
        o[1] = r1;
        o[2] = r2;
        o[3] = r3;
    }
}

// orig: (helper) the one keyframe's rotations (s16, one quadword's half a joint) into the joints' records, in plain floats
[[gnu::noinline]] void single_key_fast(int32_t n, uint32_t at) {
    float* o = fast::q4(SPR);
    const float* k = fast::q4(at);
    for (int32_t c = n; c > 0; --c, o += 16, k += 2) {
        const uint32_t w0 = fast::word(k), w1 = fast::word(k + 1);
        o[0] = fast::s16f(w0);
        o[1] = fast::s16f(w0 >> 16);
        o[2] = fast::s16f(w1);
        o[3] = fast::s16f(w1 >> 16);
    }
}

// orig: (helper) a blend channel's loop (the pose's, below) in plain floats: per joint (+0x2c of the channel's record: 3 quadwords)
// its rotation, scale and translation blended into the joint's by the weights `w0` (1 - the weight) and `w` - the quaternions'
// shorter arc by the VU0's dot - the rotation normalised, the scale's word +0x1c taken from the channel when the joint's is 0
template <bool X>
[[gnu::noinline]] void blend_channel_fast(uint32_t r, uint32_t end, float w0, float w) {
    for (; r != end; r += 0x30) {
        const float* const c = fast::q4(r);
        float* const j = fast::q4(fast::word(c + 11));
        const float a0 = j[0] * w0, a1 = j[1] * w0, a2 = j[2] * w0, a3 = j[3] * w0;
        const float b0 = c[0] * w, b1 = c[1] * w, b2 = c[2] * w, b3 = c[3] * w;
        j[4] = fast::madd<X>(c[4], w, j[4] * w0);
        j[5] = fast::madd<X>(c[5], w, j[5] * w0);
        j[6] = fast::madd<X>(c[6], w, j[6] * w0);
        j[8] = fast::madd<X>(c[8], w, j[8] * w0);
        j[9] = fast::madd<X>(c[9], w, j[9] * w0);
        j[10] = fast::madd<X>(c[10], w, j[10] * w0);
        const float d = ((a1 * b1 + a0 * b0) + a2 * b2) + a3 * b3;
        float q0, q1, q2, q3;
        if ((f2u(d) >> 31) == 0) {
            q0 = a0 + b0, q1 = a1 + b1, q2 = a2 + b2, q3 = a3 + b3;
        } else {
            q0 = a0 - b0, q1 = a1 - b1, q2 = a2 - b2, q3 = a3 - b3;
        }
        const float q = fast::rsqrt<X>(fast::madd<X>(q3, q3, fast::madd<X>(q2, q2, fast::madd<X>(q1, q1, q0 * q0))));
        if (fast::word(j + 7) == 0) std::memcpy(j + 7, c + 7, 4);
        j[0] = q0 * q;
        j[1] = q1 * q;
        j[2] = q2 * q;
        j[3] = q3 * q;
    }
}

// orig: (helper) joint_matrices_simd in plain floats: the rotation's rows from the quaternion (the VU0's terms), scaled, the
// translation, under the parent's matrix (or the identity)
template <bool X>
[[gnu::noinline]] void joint_matrices_fast(uint32_t last) {
    float* j = fast::q4(SPR);
    for (uint32_t a3 = SPR; a3 != last; a3 += 0x40, j += 16) {
        const float x = j[0], y = j[1], z = j[2], w = j[3];
        const float x2 = x + x, y2 = y + y, z2 = z + z;
        const float xw = x2 * w, yw = y2 * w, zw = z2 * w, xx = x2 * x, xy = y2 * x, xz = z2 * x, yy = y2 * y, yz = z2 * y, zz = z2 * z;
        float p[16];
        p[0] = (1.0f - yy) - zz, p[1] = xy - zw, p[2] = xz + yw;
        p[4] = zw + xy, p[5] = (1.0f - xx) - zz, p[6] = yz - xw;
        p[8] = xz - yw, p[9] = xw + yz, p[10] = (1.0f - xx) - yy;
        p[12] = j[8], p[13] = j[9], p[14] = j[10];
        if (fast::word(j + 7) != 0) {
            const float sx = j[4], sy = j[5], sz = j[6];
            p[0] *= sx, p[1] *= sx, p[2] *= sx;
            p[4] *= sy, p[5] *= sy, p[6] *= sy;
            p[8] *= sz, p[9] *= sz, p[10] *= sz;
        }
        const uint32_t parent = fast::word(j + 11);
        fast::compose<X>(parent != 0 ? fast::q4(parent) : fast::kIdentity, p, j);  // (p's w lanes: compose reads none)
    }
}

// orig: (helper) bind_matrices_simd in plain floats
template <bool X>
[[gnu::noinline]] void bind_matrices_fast(uint32_t last) {
    float* o = fast::q4(SPR);
    const float* b = fast::q4(SPR + 0x1c00);
    for (uint32_t m = SPR; m != last; m += 0x40, o += 16, b += 16) {
        float r[16];
#pragma GCC unroll 16
        for (int i = 0; i < 16; ++i) r[i] = o[i];
        fast::compose<X>(r, b, o);
    }
}

}  // namespace

// orig: (host) testing (oracle batch 470, joints_simd): one of the pose's loops - 0 the listed scales and translations (`a` the
// pose's t5), 1 the rotations (`n` joints, the keys at `a` / `b`, `plain`), 2 the joints' matrices, 3 the bind matrices (`n`
// joints) - in its per-op form (`mode` 0), its SIMD form (1) or with AVX2 (2: the matrices'; the caller checks the CPU has it),
// on the VU0's state and memory as they are; its flags kept as the pose keeps them
void moby_joints_pose_loop(int loop, int mode, uint32_t a, uint32_t b, int32_t n, bool plain) {
    const bool simd = mode != 0;
    const int wide = g_pose_wide;
    g_pose_wide = mode == 2 ? 1 : 0;
    LocalFlags fl(regs());
    const uint32_t last = (static_cast<uint32_t>(n) << 6) + SPR;
    switch (loop) {
        case 0: simd ? lerp_lists_simd(fl, a) : lerp_lists_ref(fl, a); break;
        case 1: simd ? lerp_rotations_simd(fl, n, a, b, plain) : lerp_rotations_ref(fl, n, a, b, plain); break;
        case 2: simd ? joint_matrices_simd(fl, last) : joint_matrices_ref(fl, last); break;
        default: simd ? bind_matrices_simd(fl, last) : bind_matrices_ref(fl, last); break;
    }
    g_pose_wide = wide;
}

namespace {
// orig: 0x00250fe0 (`fast`: the hot loops in plain floats, g_pose_fast's modes; 0 the VU0's ops)
void pose(uint32_t a0, uint32_t a1, int fast) {
    const bool ex = fast == 2;
    LocalFlags fl(regs());  // (host: the ops' flags in locals, the VU0's written back at the return: the same values)
    Vf* const v = vf();
    qset_xy(v[28], sx(a1));  // (the caller's s0..s2 parked in vf29..vf31: no such registers here)
    const int32_t n = lh(a0 + 0x10);
    if (n == 0) {  // no joints: the identity
        mulbc(fl, v[1], v[0], v[0].v[0], XYZW);
        mulbc(fl, v[2], v[0], v[0].v[0], XYZW);
        mr32(v[3], v[0]);
        addbc(fl, v[1], v[1], v[0].v[3], X);
        addbc(fl, v[2], v[2], v[0].v[3], Y);
        sqc2(SPR, v[1]);
        sqc2(SPR + 0x10, v[2]);
        sqc2(SPR + 0x20, v[3]);
        sqc2(SPR + 0x30, v[0]);
        return;
    }
    const uint32_t t7 = a0;
    const uint32_t un = static_cast<uint32_t>(n);
    // the transfer's chain at work + 0x1fc0: the joints' rest data, the first keyframe, the second
    // (the chain ended after the first when there is none)
    const uint32_t chain = a1 + 0x1fc0;
    sw(chain, 0x30000000u | un);
    sw(chain + 4, lwu(t7 + 0x18));
    const uint32_t key2 = lwu(t7 + 4);
    sw(chain + 0x10, (0x30000000u | lwu(t7 + 0x24)) & (key2 == 0 ? 0xffffu : 0xffffffffu));
    sw(chain + 0x14, lwu(t7 + 0x20));
    const int32_t flags = lh(t7 + 0x12);
    mulbc(fl, v[1], v[0], v[0].v[0], XYZW);
    mulbc(fl, v[2], v[0], v[0].v[0], XYZW);
    mr32(v[3], v[0]);
    addbc(fl, v[4], v[0], v[0].v[3], XYZ);
    sub(fl, v[4], v[0], v[0], W);
    sw(chain + 0x20, lwu(t7 + 0x34));
    sw(chain + 0x24, lwu(t7 + 0x30));
    addbc(fl, v[1], v[1], v[0].v[3], X);
    addbc(fl, v[2], v[2], v[0].v[3], Y);
    v[7] = lqc2(t7);
    uint32_t at = (un << 4) + SPR + 0x2000;  // the first key: quaternions, then its lists
    uint32_t k2 = (lwu(t7 + 0x24) << 4) + at;  // the second key
    uint32_t v0 = lhu(t7 + 0x28) + at, v1 = lhu(t7 + 0x2c) + at;
    uint32_t l1 = lhu(t7 + 0x38) + k2, l2 = lhu(t7 + 0x3c) + k2;
    subbc(fl, v[6], v[4], v[7].v[1], X);  // 1 - t
    mr32(v[7], v[7]);                 // t
    sw(at - 4, 0xffffffffu);
    sw(k2 - 4, 0xffffffffu);
    const uint32_t s1 = (lwu(t7 + 0x34) << 4) + k2;
    sw(s1 - 4, 0xffffffffu);
    const uint32_t t5 = static_cast<uint32_t>(qlo(v[28])) + 0x10;
    subbc(fl, v[5], v[0], v[0].v[3], X);
    sd(t5 - 0x10, 0);
    sd(t5 - 8, 0);
    spr_dma_to_chain(0x2000, chain, 0x104);
    uint32_t t4 = 0;
    if (key2 != 0) {
        {
            // the scales 1 (joints and records)
            int32_t c = n;
            uint32_t p = SPR - 0x40, r = t5 - 0x20;
            do {
                p += 0x40;
                sqc2(p, v[4]);
                r += 0x20;
                sqc2(p + 0x10, v[4]);
                --c;
                sqc2(r, v[4]);
                sqc2(r + 0x10, v[4]);
            } while (c > 0);
        }
        {
            // the rest translations, both keys' slots
            int32_t c = n;
            uint32_t src = SPR + 0x2000, dst = SPR - 0x100;
            do {
                v[16] = lqc2(src);
                dst += 0x100;
                v[17] = lqc2(src + 0x10);
                c -= 4;
                v[18] = lqc2(src + 0x20);
                v[19] = lqc2(src + 0x30);
                sqc2(dst + 0x20, v[16]);
                sqc2(dst + 0x60, v[17]);
                sqc2(dst + 0xa0, v[18]);
                sqc2(dst + 0xe0, v[19]);
                sqc2(dst + 0x30, v[16]);
                sqc2(dst + 0x70, v[17]);
                sqc2(dst + 0xb0, v[18]);
                sqc2(dst + 0xf0, v[19]);
                sw(dst + 0x3c, 0);
                sw(dst + 0x7c, 0);
                src += 0x40;
                sw(dst + 0xbc, 0);
                sw(dst + 0xfc, 0);
            } while (c > 0);
        }
        // the first key's scales (u3.15): the joint's (negative) or a record's, listed
        uint32_t s0 = t5 - 0x10, t3 = t5 - 0x1c;
        for (;;) {
            const uint64_t d = ld(v0);
            if (v0 == v1) break;
            uint32_t j = lbu(v0 + 6);
            v0 += 8;
            unpack_u3(v[8], d);
            const uint32_t jp = (j << 6) + SPR;
            j <<= 5;
            itof_lanes(v[8], v[8], XYZ, 1.0f / 32768.0f);
            if (neg(d)) {
                sw(t3 + 0xc, jp);
                t3 = jp;
                sqc2(jp, v[8]);
            } else {
                j += t5;
                sw(s0 + 0xc, j);
                s0 = j;
                sqc2(j, v[8]);
            }
        }
        // the first key's translations (s16), listed
        int32_t c2 = lh(t7 + 0x2e);
        t4 = t5 - 0x48;
        for (;;) {
            const uint64_t d = ld(v1);
            if (c2 == 0) break;
            const int32_t j = static_cast<int8_t>(lbu(v1 + 6));
            --c2;
            unpack_s16(v[8], d);
            uint32_t jp = static_cast<uint32_t>(j) << 6;
            v1 += 8;
            itof_lanes(v[8], v[8], XYZ, 1.0f);
            jp += SPR;
            const uint32_t keep = lwu(jp + 0x2c);
            sw(t4 + 0x3c, jp);
            t4 = jp;
            sw(jp + 0x3c, 0);
            sqc2(jp + 0x20, v[8]);
            sw(jp + 0x2c, keep);
        }
        // the second key's scales: into the +0x10 slots, listed when not yet
        for (;;) {
            const uint64_t d = ld(l1);
            if (l1 == l2) break;
            uint32_t j = lbu(l1 + 6);
            l1 += 8;
            unpack_u3(v[8], d);
            const uint32_t jp = (j << 6) + SPR;
            j <<= 5;
            itof_lanes(v[8], v[8], XYZ, 1.0f / 32768.0f);
            if (neg(d)) {
                const uint32_t linked = lwu(jp + 0xc);
                sqc2(jp + 0x10, v[8]);
                if (linked == 0) {
                    sw(t3 + 0xc, jp);
                    t3 = jp;
                }
            } else {
                j += t5;
                const uint32_t linked = lwu(j + 0x1c);
                sqc2(j + 0x10, v[8]);
                if (linked == 0) {
                    sw(s0 + 0xc, j);
                    s0 = j;
                }
            }
        }
        sw(t3 + 0xc, 0);
        sw(s0 + 0xc, 0);
        // the second key's translations: into +0x30, listed when not yet
        int32_t c3 = lh(t7 + 0x3e);
        uint32_t a2 = l2;
        for (;;) {
            const uint64_t d = ld(a2);
            if (c3 == 0) break;
            const int32_t j = static_cast<int8_t>(lbu(a2 + 6));
            --c3;
            unpack_s16(v[8], d);
            uint32_t jp = static_cast<uint32_t>(j) << 6;
            a2 += 8;
            itof_lanes(v[8], v[8], XYZ, 1.0f);
            jp += SPR;
            const uint32_t linked = lwu(jp + 0x3c);
            sqc2(jp + 0x30, v[8]);
            sw(jp + 0x3c, linked);
            if (linked == 0) {
                sw(t4 + 0x3c, jp);
                t4 = jp;
                sw(jp + 0x3c, 0);
            }
        }
        // the listed scales and translations lerped by t
        v[10].v[3] = v[0].v[3];
        if (fast) ex ? lerp_lists_fast<true>(t5) : lerp_lists_fast<false>(t5);
        else if (g_pose_simd) lerp_lists_simd(fl, t5);
        else lerp_lists_ref(fl, t5);
        // the rotations, two joints at a time: lerped along the shorter arc and normalised
        // (flag 0x400: lerped only)
        const bool plain = (flags & 0x400) != 0;
        if (fast) ex ? lerp_rotations_fast<true>(n, at, k2, plain) : lerp_rotations_fast<false>(n, at, k2, plain);
        else if (g_pose_simd) lerp_rotations_simd(fl, n, at, k2, plain);
        else lerp_rotations_ref(fl, n, at, k2, plain);
    } else {  // one keyframe
        {
            int32_t c = n;
            uint32_t p = SPR - 0x40, r = t5 - 0x20;
            do {
                p += 0x40;
                r += 0x20;
                sqc2(p + 0x10, v[4]);
                --c;
                sqc2(r, v[4]);
            } while (c > 0);
        }
        {
            int32_t c = n;
            uint32_t src = SPR + 0x2000, dst = SPR - 0x100;
            do {
                v[16] = lqc2(src);
                dst += 0x100;
                v[17] = lqc2(src + 0x10);
                c -= 4;
                v[18] = lqc2(src + 0x20);
                v[19] = lqc2(src + 0x30);
                sqc2(dst + 0x20, v[16]);
                sqc2(dst + 0x60, v[17]);
                sqc2(dst + 0xa0, v[18]);
                src += 0x40;
                sqc2(dst + 0xe0, v[19]);
            } while (c > 0);
        }
        uint32_t s0 = t5 - 0x10;
        for (;;) {
            const uint64_t d = ld(v0);
            if (v0 == v1) break;
            uint32_t j = lbu(v0 + 6);
            v0 += 8;
            unpack_u3(v[8], d);
            const uint32_t jp = (j << 6) + SPR;
            j <<= 5;
            itof_lanes(v[8], v[8], XYZ, 1.0f / 32768.0f);
            if (neg(d)) {
                sqc2(jp + 0x10, v[8]);
            } else {
                j += t5;
                sw(s0 + 0xc, j);
                s0 = j;
                sqc2(j, v[8]);
            }
        }
        sw(s0 + 0xc, 0);
        int32_t c2 = lh(t7 + 0x2e);
        for (;;) {
            const uint64_t d = ld(v1);
            if (c2 == 0) break;
            const int32_t j = static_cast<int8_t>(lbu(v1 + 6));
            --c2;
            unpack_s16(v[8], d);
            uint32_t jp = static_cast<uint32_t>(j) << 6;
            v1 += 8;
            itof_lanes(v[8], v[8], XYZ, 1.0f);
            jp += SPR;
            const uint32_t keep = lwu(jp + 0x2c);
            sqc2(jp + 0x20, v[8]);
            sw(jp + 0x2c, keep);
        }
        if (fast) {
            single_key_fast(n, at);
        } else {
            int32_t c = n;
            uint32_t p = SPR - 0x40;
            unpack_s16(v[8], ld(at));
            at += 8;
            do {
                const uint64_t d = ld(at);
                itof_lanes(v[9], v[8], XYZW, 1.0f / 32768.0f);
                --c;
                at += 8;
                unpack_s16(v[8], d);
                p += 0x40;
                sqc2(p, v[9]);
            } while (c > 0);
        }
    }
    // the blend channels: per channel (+8 its weight, +0x10 its keys, 3 quadwords per joint,
    // +0x2c the joint's record) blended into the joints by weight
    for (uint32_t t9 = lwu(t7 + 8); t9 != 0;) {
        const uint32_t qwc = static_cast<uint32_t>(lhu(t9 + 2)) * 3;
        spr_dma_to(0x2000, lwu(t9 + 0x10), qwc);
        const uint32_t end = (qwc << 4) + SPR + 0x2000;
        qset_xy(v[21], sx(lwu(t9 + 8)));
        t9 = lwu(t9 + 0x1c);
        sub(fl, v[20], v[4], v[21], X);
        if (fast) {
            (ex ? blend_channel_fast<true> : blend_channel_fast<false>)(SPR + 0x2000, end, v[20].v[0], v[21].v[0]);
            continue;
        }
        uint32_t r = SPR + 0x2000;
        do {
            const uint32_t j = lwu(r + 0x2c);
            r += 0x30;
            v[13] = lqc2(r - 0x30);
            v[14] = lqc2(r - 0x20);
            v[15] = lqc2(r - 0x10);
            v[10] = lqc2(j);
            v[11] = lqc2(j + 0x10);
            v[12] = lqc2(j + 0x20);
            mulbc(fl, v[10], v[10], v[20].v[0], XYZW);
            mulbc(fl, v[13], v[13], v[21].v[0], XYZW);
            mulbc(fl, v[11], v[11], v[20].v[0], XYZ);
            mulbc(fl, v[14], v[14], v[21].v[0], XYZ);
            mulbc(fl, v[12], v[12], v[20].v[0], XYZ);
            mulbc(fl, v[15], v[15], v[21].v[0], XYZ);
            blend_quats(fl, 10, 13);
            add(fl, v[11], v[11], v[14], XYZ);
            add(fl, v[12], v[12], v[15], XYZ);
            blend_quats_norm(fl, 10, 13);
            sqc2(j + 0x10, v[11]);
            sqc2(j + 0x20, v[12]);
            if (lwu(j + 0x1c) == 0) sw(j + 0x1c, lwu(r - 0x14));
            mulbc(fl, v[10], v[16], q(), XYZW);
            sqc2(j, v[10]);
        } while (r != end);
    }
    // the additive layers (+3: 0 composed on the pose, else blended by +0xc)
    v[19] = v[0];
    for (uint32_t t8 = lwu(t7 + 0xc); t8 != 0;) {
        const uint32_t r = t8;
        const uint32_t j = lwu(r + 4);
        const uint32_t mode = lbu(r + 3);
        v[8] = lqc2(j);
        v[10] = lqc2(j + 0x10);
        v[12] = lqc2(j + 0x20);
        v[9] = lqc2(r + 0x10);
        v[11] = lqc2(r + 0x20);
        v[13] = lqc2(r + 0x30);
        t8 = lwu(r + 8);
        if (mode == 0) {
            mul(fl, v[14], v[9], v[8], W);
            mul(fl, v[15], v[9], v[8], XYZ);
            mulbc(fl, v[16], v[9], v[8].v[3], XYZ);
            mulbc(fl, v[17], v[8], v[9].v[3], XYZ);
            opmula(fl, v[8], v[9]);  // the cross product
            opmsub(fl, v[18], v[9], v[8]);
            addabc(fl, v[15], v[15].v[1], X);
            maddbc(fl, v[15], v[4], v[15].v[2], X);
            add(fl, v[8], v[16], v[17], XYZ);
            add(fl, v[8], v[8], v[18], XYZ);
            subbc(fl, v[8], v[14], v[15].v[0], W);
            mul(fl, v[19], v[10], v[11], XYZ);
            add(fl, v[12], v[12], v[13], XYZ);
        } else {
            qset_xy(v[20], sx(lwu(r + 0xc)));
            subbc(fl, v[20], v[0], v[20].v[0], W);
            mulbc(fl, v[8], v[8], v[20].v[3], XYZW);
            mulbc(fl, v[9], v[9], v[20].v[0], XYZW);
            mulbc(fl, v[10], v[10], v[20].v[3], XYZ);
            mulbc(fl, v[11], v[11], v[20].v[0], XYZ);
            mulbc(fl, v[12], v[12], v[20].v[3], XYZ);
            mulbc(fl, v[13], v[13], v[20].v[0], XYZ);
            blend_quats(fl, 8, 9);
            add(fl, v[19], v[10], v[11], XYZ);
            add(fl, v[12], v[12], v[13], XYZ);
            blend_quats_norm(fl, 8, 9);
            mulbc(fl, v[8], v[16], q(), XYZW);
        }
        sqc2(j, v[8]);
        sqc2(j + 0x10, v[19]);
        sqc2(j + 0x20, v[12]);
    }
    // the bind matrices (+0x1c) to the scratchpad's 0x1c00; the joints' matrices from their
    // quaternion, scale and translation under their parent's
    sw((un << 6) + SPR + 0x1bfc, 0xffffffffu);  // (the transfer's last word: the original waits on it)
    spr_dma_to(0x1c00, lwu(t7 + 0x1c), un << 2);
    const uint32_t last = (un << 6) + SPR;
    if (fast) ex ? joint_matrices_fast<true>(last) : joint_matrices_fast<false>(last);
    else if (g_pose_simd) joint_matrices_simd(fl, last);
    else joint_matrices_ref(fl, last);
    // the record scales (lerped by t with a second key) on the joints' rows
    for (uint32_t r = lwu(t5 - 4); r != 0;) {
        const uint32_t m = ((r - t5) << 1) + SPR;
        v[1] = lqc2(r);
        if (key2 != 0) {
            v[8] = lqc2(r + 0x10);
            mulabc(fl, v[1], v[6].v[0], XYZ);
            maddbc(fl, v[1], v[8], v[7].v[0], XYZ);
        }
        v[2] = lqc2(m);
        v[3] = lqc2(m + 0x10);
        v[4] = lqc2(m + 0x20);
        mulbc(fl, v[2], v[2], v[1].v[0], XYZ);
        r = lwu(r + 0xc);
        mulbc(fl, v[3], v[3], v[1].v[1], XYZ);
        mulbc(fl, v[4], v[4], v[1].v[2], XYZ);
        sqc2(m, v[2]);
        sqc2(m + 0x10, v[3]);
        sqc2(m + 0x20, v[4]);
    }
    // each joint's matrix times its bind matrix
    if (fast) ex ? bind_matrices_fast<true>(last) : bind_matrices_fast<false>(last);
    else if (g_pose_simd) bind_matrices_simd(fl, last);
    else bind_matrices_ref(fl, last);
}

// orig: (host) RAC1_WIIU_FAST_POSE_CHECK=1: each pose built twice from the same state - the VU0's way, then the fast loops' (kept;
// g_pose_fast's mode, 1 when 0) -
// and their matrices compared: the largest difference in the rotation / scale rows (0 .. 2), in the translation row (3, also
// relative to the row's size) and in units in the last place (of the values of 1e-3 or more), printed every 2000 poses
struct PoseCheck {
    uint64_t poses = 0, joints = 0, differing = 0;
    float rows = 0, trans = 0, trans_rel = 0;
    uint32_t ulps = 0;
};
void pose_checked(uint32_t a0, uint32_t a1) {
    static PoseCheck c;
    alignas(16) static uint8_t spr0[0x4000], work0[0x2000], ref[0x1c00];
    Regs& R = regs();
    const Regs saved = R;
    uint8_t* const spr = static_cast<uint8_t*>(ee_ram_ptr(SPR));
    uint8_t* const work = static_cast<uint8_t*>(ee_ram_ptr(a1));
    std::memcpy(spr0, spr, sizeof spr0);
    std::memcpy(work0, work, sizeof work0);
    pose(a0, a1, 0);
    const uint32_t n = std::min<uint32_t>(std::max<int32_t>(lh(a0 + 0x10), 1), 0x70);
    std::memcpy(ref, spr, n * 0x40);
    std::memcpy(spr, spr0, sizeof spr0);
    std::memcpy(work, work0, sizeof work0);
    R = saved;
    pose(a0, a1, g_pose_fast != 0 ? g_pose_fast : 1);
    ++c.poses;
    c.joints += n;
    for (uint32_t i = 0; i < n * 16; ++i) {
        float a, b;
        std::memcpy(&a, ref + 4 * i, 4);
        std::memcpy(&b, spr + 4 * i, 4);
        if (f2u(a) == f2u(b)) continue;
        ++c.differing;
        const float d = std::fabs(a - b);
        const int32_t ia = static_cast<int32_t>(f2u(a)), ib = static_cast<int32_t>(f2u(b));
        if ((ia < 0) == (ib < 0) && std::fabs(a) >= 1e-3f) c.ulps = std::max(c.ulps, static_cast<uint32_t>(std::abs(ia - ib)));  // (near 0: |d| alone)
        if ((i & 15) < 12) {
            c.rows = std::max(c.rows, d);
        } else {
            c.trans = std::max(c.trans, d);
            const float* const t = reinterpret_cast<const float*>(ref) + (i & ~3u);  // (the row's size: its largest lane, at least 1)
            c.trans_rel = std::max(c.trans_rel, d / std::max({std::fabs(t[0]), std::fabs(t[1]), std::fabs(t[2]), 1.0f}));
        }
    }
    if (c.poses % 2000 == 0)
        std::printf("[pose] fast against the VU0's: %llu poses, %llu joints, %llu floats differing: largest |d| rows 0-2 %.3g, row 3 %.3g (relative %.3g), "
                    "%u ulps\n",
                    static_cast<unsigned long long>(c.poses), static_cast<unsigned long long>(c.joints), static_cast<unsigned long long>(c.differing),
                    static_cast<double>(c.rows), static_cast<double>(c.trans), static_cast<double>(c.trans_rel), c.ulps);
}
}  // namespace

// orig: 0x00250fe0
void moby_joints_pose(uint32_t a0, uint32_t a1) {
    static const bool check = std::getenv("RAC1_WIIU_FAST_POSE_CHECK") != nullptr && std::atoi(std::getenv("RAC1_WIIU_FAST_POSE_CHECK")) != 0;
    if (__builtin_expect(check, 0)) pose_checked(a0, a1);
    else pose(a0, a1, g_pose_fast);
}

}  // namespace rac1
