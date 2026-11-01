// world_tfrag_lighting (world_lighting.cpp, 0x295350) with its three vertex loops native (2026-11-01, the Wii U's game step;
// RAC1_WTL_FAST, the Wii U's default 1, the PC's 0). The transliterated routine runs each VU0 lane op through the shared register
// file - a call, the operands and the result clamped, the registers in memory - and each MMI unpack byte by byte: on Nebula G34 (a
// flickering light relights 64 tfrags a frame) its vertex loops were ~15% of the game thread. Here the same routine: the same
// memory reads and writes in the same order (the scratchpad's normals, the colour words, the DMA registers, the s registers'
// spill), the VU0 registers kept in a local copy for the whole call - the same lane ops with the same rules (vuc on the operands
// and the result, the masked lanes only, ACC and Q as the ops leave them), the unpacks done on words - and written back at its end,
// so the VU0 file is left as the routine leaves it. The raw data the ops read (the (cos, sin) table, vf21, the light sets, the point
// lights) checked VU-clean once: then every operand is clean already (that data, or a VU result) and an op clamps its result alone -
// the same values (vuc(x) is x for those); the full rules when a check fails. The point lights' range test reads the MAC flags: that op runs as the
// routine's (vu0m's, flags and all) on the real registers. Not used while a VU0 micro program runs (the routine's macro ops would
// wait for it; the transliterated routine then).
// orig: (host)
#include <game/ovl/world_lighting.h>

#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vu0_regs.h>
#include <runtime/vu/vu0.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rac1 {

namespace {
using namespace eer;

// the VU0 registers the routine uses, as a local copy (v[0] the constant vf0)
struct Vu {
    float v[32][4];
    float acc[4];
    float q;
};

[[gnu::always_inline]] inline float cl(float x) { return vuc(x); }
// an operand's clamp: none when the caller knows every operand is clean (C: a VU result, or read from data checked clean - vuc(x) is
// x then, the same value)
template <bool C>
[[gnu::always_inline]] inline float op(float x) { return C ? x : vuc(x); }

// the lane ops (vu0_nf.h's: operands and result clamped, the lanes computed first, then written)
template <int M>
[[gnu::always_inline]] inline void set_lanes(float* d, const float r[4]) {
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) d[i] = r[i];
}
template <bool C, int M>
[[gnu::always_inline]] inline void add(float* d, const float* a, const float* b) {
    float r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = cl(op<C>(a[i]) + op<C>(b[i]));
    set_lanes<M>(d, r);
}
template <bool C, int M>
[[gnu::always_inline]] inline void sub(float* d, const float* a, const float* b) {
    float r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = cl(op<C>(a[i]) - op<C>(b[i]));
    set_lanes<M>(d, r);
}
template <bool C, int M>
[[gnu::always_inline]] inline void mul(float* d, const float* a, const float* b) {
    float r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = cl(op<C>(a[i]) * op<C>(b[i]));
    set_lanes<M>(d, r);
}
template <bool C, int M>
[[gnu::always_inline]] inline void addbc(float* d, const float* a, float b) {
    const float y = op<C>(b);
    float r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = cl(op<C>(a[i]) + y);
    set_lanes<M>(d, r);
}
template <bool C, int M>
[[gnu::always_inline]] inline void subbc(float* d, const float* a, float b) {
    const float y = op<C>(b);
    float r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = cl(op<C>(a[i]) - y);
    set_lanes<M>(d, r);
}
template <bool C, int M>
[[gnu::always_inline]] inline void mulbc(float* d, const float* a, float b) {
    const float y = op<C>(b);
    float r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = cl(op<C>(a[i]) * y);
    set_lanes<M>(d, r);
}
// VMADDbc: d = ACC + a b (the product rounded on its own)
template <bool C, int M>
[[gnu::always_inline]] inline void maddbc(Vu& u, float* d, const float* a, float b) {
    const float y = op<C>(b);
    float r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) {
            const float p = op<C>(a[i]) * y;
            r[i] = cl(op<C>(u.acc[i]) + p);
        }
    set_lanes<M>(d, r);
}
template <bool C, int M>
[[gnu::always_inline]] inline void addabc(Vu& u, const float* a, float b) { addbc<C, M>(u.acc, a, b); }
template <bool C, int M>
[[gnu::always_inline]] inline void mulabc(Vu& u, const float* a, float b) { mulbc<C, M>(u.acc, a, b); }
template <bool C, int M>
[[gnu::always_inline]] inline void maddabc(Vu& u, const float* a, float b) { maddbc<C, M>(u, u.acc, a, b); }
// VMAXbc / VMINIbc on one lane's bits
[[gnu::always_inline]] inline float vmaxf(float a, float b) { return vu0m::vmax(a, b); }
template <int M>
[[gnu::always_inline]] inline void minibc(float* d, float b) {
    float r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = vu0m::vmini(d[i], b);
    set_lanes<M>(d, r);
}
// QMTC2 / LQC2 / SQC2 on the copy
[[gnu::always_inline]] inline void set_words(float* d, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3) {
    d[0] = u2f(w0), d[1] = u2f(w1), d[2] = u2f(w2), d[3] = u2f(w3);
}
[[gnu::always_inline]] inline void lqc2(float* d, uint32_t a) {
    const Q128 q = lq(a);
    set_words(d, static_cast<uint32_t>(q.lo), static_cast<uint32_t>(q.lo >> 32), static_cast<uint32_t>(q.hi), static_cast<uint32_t>(q.hi >> 32));
}
[[gnu::always_inline]] inline void sqc2(uint32_t a, const float* s) {
    Q128 q;
    q.lo = f2u(s[0]) | static_cast<uint64_t>(f2u(s[1])) << 32;
    q.hi = f2u(s[2]) | static_cast<uint64_t>(f2u(s[3])) << 32;
    sq(a, q);
}
// the normal of a vertex: the halfword at `h` two indices into the (cos, sin) table at 0x70003800 (pextlb / pextlh / psllw / ld /
// pcpyld), then n.xy *= n.z, n.z = 0 + n.w, n.xyz = 0 - n.xyz
template <bool C>
[[gnu::always_inline]] inline void normal(Vu& u, float* d, uint32_t h) {
    const uint32_t a = 0x70003800u | (h & 0xffu) << 3, b = 0x70003800u | ((h >> 8) & 0xffu) << 3;
    const uint64_t ta = ld_(a), tb = ld_(b);
    set_words(d, static_cast<uint32_t>(ta), static_cast<uint32_t>(ta >> 32), static_cast<uint32_t>(tb), static_cast<uint32_t>(tb >> 32));
    mulbc<C, 3>(d, d, d[2]);
    addbc<C, 4>(d, u.v[0], d[3]);
    sub<C, 7>(d, u.v[0], d);
}
// a 1:5:5:5 colour as the four magic floats 65536 + byte / 128 (pext5 / pextlb / pextlh / padduw with 0x47800000)
[[gnu::always_inline]] inline void colour5(float* d, uint32_t c) {
    const uint32_t r = (c & 0x1fu) << 3, g = ((c >> 5) & 0x1fu) << 3, b = ((c >> 10) & 0x1fu) << 3, al = (c & 0x8000u) != 0 ? 0x80u : 0u;
    set_words(d, 0x47800000u + r, 0x47800000u + g, 0x47800000u + b, 0x47800000u + al);
}
// a colour word's four bytes as the magic floats (pextlb / pextlh / padduw)
[[gnu::always_inline]] inline void colour8(float* d, uint32_t c) {
    set_words(d, 0x47800000u + (c & 0xffu), 0x47800000u + ((c >> 8) & 0xffu), 0x47800000u + ((c >> 16) & 0xffu), 0x47800000u + (c >> 24));
}
// QMFC2 / PPACH / PPACB: each lane's low byte
[[gnu::always_inline]] inline uint32_t pack(const float* s) {
    return (f2u(s[0]) & 0xffu) | (f2u(s[1]) & 0xffu) << 8 | (f2u(s[2]) & 0xffu) << 16 | (f2u(s[3]) & 0xffu) << 24;
}
inline uint32_t sx16(uint32_t a) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu_(a)))); }
inline uint32_t sx8(uint32_t a) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(lbu_(a)))); }

bool fast_on() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_WTL_FAST");
#if defined(__WIIU__) && !defined(RAC1_VU0_EXACT)
        return e == nullptr || std::atoi(e) != 0;
#else
        return e != nullptr && std::atoi(e) != 0;
#endif
    }();
    return on;
}
}  // namespace

// a lane VU-clean (vuc leaves it as it is: a normal number or a zero)
[[gnu::always_inline]] inline bool clean(uint32_t w) {
    const uint32_t e = w & 0x7f800000u;
    return e != 0x7f800000u && (e != 0 || (w & 0x007fffffu) == 0);
}
// `n` words of EE memory at `a` all clean
inline bool clean_at(uint32_t a, uint32_t n) {
    for (uint32_t k = 0; k < n; ++k)
        if (!clean(lw_(a + 4 * k))) return false;
    return true;
}
inline bool clean_v(const float* v) { return clean(f2u(v[0])) && clean(f2u(v[1])) && clean(f2u(v[2])) && clean(f2u(v[3])); }

// one light set for the whole entry (loaded into vf24 / vf25 / vf27 / vf28 by the caller): two vertices an iteration
template <bool C>
void loop_one_set(Vu& u, uint32_t& t3, uint32_t& t4, uint32_t& t5, int32_t t1) {
    float* const v0 = u.v[0];
    addbc<C, 1>(u.v[31], v0, u.v[27][3]);
    addbc<C, 2>(u.v[31], v0, u.v[28][3]);
    subbc<C, 8>(u.v[27], v0, v0[3]);
    subbc<C, 8>(u.v[28], v0, v0[3]);
    do {
        const uint32_t na = sx16(t3 + 0x2), ca = sx16(t3 + 0x4), nb = sx16(t3 + 0xa), cb = sx16(t3 + 0xc);
        t3 += 0x10;
        normal<C>(u, u.v[1], na);
        colour5(u.v[30], ca);
        sqc2(t5, u.v[1]);
        normal<C>(u, u.v[6], nb);
        colour5(u.v[23], cb);
        sqc2(t5 + 0x10, u.v[6]);
        mul<C, 7>(u.v[2], u.v[1], u.v[25]);
        mul<C, 7>(u.v[1], u.v[1], u.v[24]);
        mul<C, 7>(u.v[7], u.v[6], u.v[25]);
        mul<C, 7>(u.v[6], u.v[6], u.v[24]);
        addabc<C, 1>(u, u.v[2], u.v[2][1]);
        maddbc<C, 1>(u, u.v[5], u.v[21], u.v[2][2]);
        addabc<C, 1>(u, u.v[1], u.v[1][1]);
        maddbc<C, 1>(u, u.v[4], u.v[21], u.v[1][2]);
        addabc<C, 1>(u, u.v[7], u.v[7][1]);
        maddbc<C, 1>(u, u.v[9], u.v[21], u.v[7][2]);
        addabc<C, 1>(u, u.v[6], u.v[6][1]);
        maddbc<C, 1>(u, u.v[8], u.v[21], u.v[6][2]);
        mulbc<C, 1>(u.v[1], u.v[4], u.v[31][0]);
        mulbc<C, 1>(u.v[2], u.v[5], u.v[31][1]);
        mulbc<C, 1>(u.v[6], u.v[8], u.v[31][0]);
        mulbc<C, 1>(u.v[7], u.v[9], u.v[31][1]);
        u.v[4][0] = vmaxf(u.v[4][0], u.v[1][0]);
        u.v[5][0] = vmaxf(u.v[5][0], u.v[2][0]);
        u.v[8][0] = vmaxf(u.v[8][0], u.v[6][0]);
        u.v[9][0] = vmaxf(u.v[9][0], u.v[7][0]);
        mulabc<C, 15>(u, u.v[30], v0[3]);
        maddabc<C, 15>(u, u.v[27], u.v[4][0]);
        maddbc<C, 15>(u, u.v[1], u.v[28], u.v[5][0]);
        mulabc<C, 15>(u, u.v[23], v0[3]);
        maddabc<C, 15>(u, u.v[27], u.v[8][0]);
        maddbc<C, 15>(u, u.v[6], u.v[28], u.v[9][0]);
        minibc<7>(u.v[1], u.v[22][0]);
        minibc<7>(u.v[6], u.v[22][0]);
        t5 += 0x20;
        sw_(t4, pack(u.v[1]));
        sw_(t4 + 4, pack(u.v[6]));
        t4 += 8;
        --t1;
    } while (t1 != 0);
}

// a light set per vertex: one of the 16 sets at t6, or two blended by the vertex's weight
template <bool C>
void loop_set_per_vertex(Vu& u, uint32_t t6, uint32_t& t3, uint32_t& t4, uint32_t& t5, int32_t t1) {
    float* const v0 = u.v[0];
    do {
        const uint32_t a0 = sx16(t3 + 0x6);
        if ((a0 & 0xff00u) == 0) {
            const uint32_t at = ((a0 & 0xfu) << 6) + t6;
            lqc2(u.v[27], at);
            lqc2(u.v[24], at + 0x10);
            lqc2(u.v[28], at + 0x20);
            lqc2(u.v[25], at + 0x30);
        } else {
            // qmtc2(1, at): x the weight's word, y its sign (0), z / w the high half `at` held (only x / w are read before vf1 is
            // written whole below)
            u.v[1][0] = u2f((a0 >> 4) & 0xff0u);
            u.v[1][1] = 0.0f;
            u.v[1][0] = vu0m::itof(u.v[1][0], 1.0f / 4096.0f);
            subbc<C, 8>(u.v[1], v0, u.v[1][0]);
            const uint32_t at = ((a0 & 0xfu) << 6) + t6, bt = (((a0 >> 4) & 0xfu) << 6) + t6;
            lqc2(u.v[2], at);
            lqc2(u.v[3], at + 0x10);
            lqc2(u.v[4], at + 0x20);
            lqc2(u.v[5], at + 0x30);
            lqc2(u.v[6], bt);
            mulbc<C, 15>(u.v[2], u.v[2], u.v[1][3]);
            lqc2(u.v[7], bt + 0x10);
            mulbc<C, 7>(u.v[3], u.v[3], u.v[1][3]);
            lqc2(u.v[8], bt + 0x20);
            mulbc<C, 15>(u.v[4], u.v[4], u.v[1][3]);
            lqc2(u.v[9], bt + 0x30);
            mulbc<C, 7>(u.v[5], u.v[5], u.v[1][3]);
            mulbc<C, 7>(u.v[7], u.v[7], u.v[1][0]);
            mulbc<C, 7>(u.v[9], u.v[9], u.v[1][0]);
            mulbc<C, 15>(u.v[6], u.v[6], u.v[1][0]);
            mulbc<C, 15>(u.v[8], u.v[8], u.v[1][0]);
            add<C, 15>(u.v[24], u.v[3], u.v[7]);
            add<C, 15>(u.v[25], u.v[5], u.v[9]);
            add<C, 15>(u.v[27], u.v[2], u.v[6]);
            add<C, 15>(u.v[28], u.v[4], u.v[8]);
            mul<C, 15>(u.v[1], u.v[24], u.v[24]);
            addabc<C, 1>(u, u.v[1], u.v[1][1]);
            maddbc<C, 1>(u, u.v[1], u.v[21], u.v[1][2]);
            u.q = vu0m::vrsqrt(v0[3], u.v[1][0]);
            mul<C, 15>(u.v[1], u.v[25], u.v[25]);
            addabc<C, 1>(u, u.v[1], u.v[1][1]);
            maddbc<C, 1>(u, u.v[1], u.v[21], u.v[1][2]);
            mulbc<C, 7>(u.v[24], u.v[24], u.q);
            u.q = vu0m::vrsqrt(v0[3], u.v[1][0]);
            mulbc<C, 7>(u.v[25], u.v[25], u.q);
        }
        addbc<C, 1>(u.v[31], v0, u.v[27][3]);
        addbc<C, 2>(u.v[31], v0, u.v[28][3]);
        subbc<C, 8>(u.v[27], v0, v0[3]);
        subbc<C, 8>(u.v[28], v0, v0[3]);
        normal<C>(u, u.v[1], sx16(t3 + 0x2));
        sqc2(t5, u.v[1]);
        colour5(u.v[30], sx16(t3 + 0x4));
        mul<C, 7>(u.v[2], u.v[1], u.v[25]);
        mul<C, 7>(u.v[1], u.v[1], u.v[24]);
        addabc<C, 1>(u, u.v[1], u.v[1][1]);
        maddbc<C, 1>(u, u.v[4], u.v[21], u.v[1][2]);
        addabc<C, 1>(u, u.v[2], u.v[2][1]);
        maddbc<C, 1>(u, u.v[5], u.v[21], u.v[2][2]);
        mulbc<C, 1>(u.v[1], u.v[4], u.v[31][0]);
        mulbc<C, 1>(u.v[2], u.v[5], u.v[31][1]);
        u.v[4][0] = vmaxf(u.v[4][0], u.v[1][0]);
        u.v[5][0] = vmaxf(u.v[5][0], u.v[2][0]);
        mulabc<C, 15>(u, u.v[30], v0[3]);
        maddabc<C, 15>(u, u.v[27], u.v[4][0]);
        maddbc<C, 15>(u, u.v[1], u.v[28], u.v[5][0]);
        minibc<7>(u.v[1], u.v[22][0]);
        sw_(t4, pack(u.v[1]));
        t5 += 0x10;
        t4 += 4;
        --t1;
        t3 += 8;
    } while (t1 != 0);
}

// one point light (vf26 / vf29 loaded by the caller) over the entry's vertices: the colour words in the scratchpad brightened
template <bool C>
void loop_point_light(Vu& u, vu0m::Regs& R, uint32_t s5, uint32_t p3, uint32_t p4, int32_t t1) {
    float* const v0 = u.v[0];
    uint32_t p5 = 0x70000000u;
    do {
        colour8(u.v[30], lw_(p4));
        {
            const uint32_t v1 = s5 + lhu_(p3);
            const uint32_t x = sx16(v1), y = sx16(v1 + 2), z = sx16(v1 + 4);
            // (pextlw / pcpyld: w the sign word of z's register)
            set_words(u.v[10], x << 2, y << 2, z << 2, (static_cast<int32_t>(z) < 0 ? 0xffffffffu : 0u) << 2);
            for (int i = 0; i < 3; ++i) u.v[10][i] = vu0m::itof(u.v[10][i], 1.0f / 4096.0f);
        }
        add<C, 7>(u.v[10], u.v[10], u.v[20]);
        u.q = vu0m::div(v0[3], u.v[26][3]);
        sub<C, 7>(u.v[2], u.v[10], u.v[26]);
        mul<C, 7>(u.v[4], u.v[2], u.v[2]);
        mul<C, 8>(u.v[4], u.v[26], u.v[26]);
        addabc<C, 1>(u, u.v[4], u.v[4][1]);
        maddbc<C, 1>(u, u.v[4], u.v[21], u.v[4][2]);
        mulbc<C, 8>(u.v[11], v0, u.q);
        // the range test: the routine's op into vf0, for its MAC flags (on the real registers)
        std::memcpy(R.vf[4].v, u.v[4], sizeof u.v[4]);
        vu0m::subbc(R.vf[0], R.vf[4], R.vf[4].v[0], 8);
        const uint32_t mac = cfc2(17);
        u.q = vu0m::vsqrt(u.v[4][0]);
        if ((mac & 0x10u) == 0) {
            mulbc<C, 8>(u.v[11], u.v[11], u.q);
            mulbc<C, 8>(u.v[4], v0, u.q);
            subbc<C, 8>(u.v[11], v0, u.v[11][3]);
            u.q = vu0m::div(v0[3], u.v[4][3]);
            mulbc<C, 8>(u.v[11], u.v[11], u.q);
            mulbc<C, 7>(u.v[2], u.v[2], u.v[11][3]);
            lqc2(u.v[1], p5);
            mul<false, 7>(u.v[1], u.v[1], u.v[2]);  // (vf1 read back from the scratchpad: its operands clamped in any case)
            addabc<C, 1>(u, u.v[1], u.v[1][1]);
            maddbc<C, 1>(u, u.v[4], u.v[21], u.v[1][2]);
            mulbc<C, 1>(u.v[1], u.v[4], u.v[29][3]);
            u.v[4][0] = vmaxf(u.v[4][0], u.v[1][0]);
            mulabc<C, 15>(u, u.v[30], v0[3]);
            maddbc<C, 15>(u, u.v[1], u.v[29], u.v[4][0]);
            minibc<7>(u.v[1], u.v[22][0]);
            sw_(p4, pack(u.v[1]));
        }
        p5 += 0x10;
        p4 += 4;
        --t1;
        p3 += 8;
    } while (t1 != 0);
}

// orig: 0x00295350 (the host form above; false: not taken, the transliterated routine runs)
bool world_tfrag_lighting_fast(uint32_t list) {
    static const bool stats = std::getenv("RAC1_WTL_STATS") != nullptr;
    static uint32_t n_taken = 0, n_not = 0;
    if (!fast_on() || vu0::running()) {
        if (stats && ++n_not % 100 == 1) std::fprintf(stderr, "[wtl-fast] not taken %u (%s)\n", n_not, fast_on() ? "a VU0 program runs" : "off");
        return false;
    }
    if (stats && ++n_taken % 100 == 1) std::fprintf(stderr, "[wtl-fast] %u calls taken, %u not\n", n_taken, n_not);
    // the prologue: the caller's s registers spilled to 0x70000800 (the routine's own: zero), the constants, the (cos, sin) table
    // to the scratchpad by DMA (toSPR)
    for (uint32_t k = 0; k < 8; ++k) sd_(0x70000800u + k * 8, 0);
    const uint32_t t2 = lw_(OV(0x00160f4cu));
    const uint32_t t6 = OV(0x0017fec0u), s1 = OV(0x001802c0u);
    vu0m::Regs& R = vu0m::regs();
    Vu u;
    std::memcpy(u.v, R.vf.data(), sizeof u.v);
    std::memcpy(u.acc, &R.acc, sizeof u.acc);
    u.q = R.q;
    lqc2(u.v[21], OV(0x0015fb40u));
    set_words(u.v[22], 0x478000ffu, 0, 0, 0);  // (qmtc2 of a set32 value: x the word, y its sign, z / w the register's high half: 0)
    sw_(0x1000d480u, 0x3800u);
    sw_(0x1000d410u, OV(0x00166080u));
    sw_(0x1000d420u, 0x80u);
    sw_(0x1000d400u, 0x100u);
    while ((lw_(0x1000d400u) & 0x100u) != 0) {
    }
    uint32_t s6 = 0x70001000u;
    // the raw data every entry reads: the (cos, sin) table now in the scratchpad (256 pairs) and vf21; the sets / lights per use
    const bool base_clean = clean_at(0x70003800u, 512) && clean_v(u.v[21]);
    int sets_clean = -1;
    for (uint32_t s8 = list;; ) {
        const uint32_t idx = lhu_(s8);
        s8 += 2;
        if (idx == 0xffffu) break;
        const uint32_t a3 = t2 + (idx << 6);
        const uint32_t s5 = lw_(a3 + 0x10);
        uint32_t t3 = s5 + sx16(a3 + 0x30) + 0x10;
        uint32_t t4 = s6, t5 = 0x70000000u;
        const uint32_t set = sx8(a3 + 0x34);
        if (static_cast<int32_t>(set) >= 0) {
            const uint32_t t0 = t6 + (set << 6);
            const int32_t t1 = static_cast<int32_t>(sx8(a3 + 0x29) << 1);
            lqc2(u.v[27], t0);
            lqc2(u.v[28], t0 + 0x20);
            lqc2(u.v[24], t0 + 0x10);
            lqc2(u.v[25], t0 + 0x30);
            // (the lanes read raw from memory: the set, the normals' table, vf21; every other operand is a VU result)
            if (base_clean && clean_v(u.v[27]) && clean_v(u.v[28]) && clean_v(u.v[24]) && clean_v(u.v[25])) loop_one_set<true>(u, t3, t4, t5, t1);
            else loop_one_set<false>(u, t3, t4, t5, t1);
        } else {
            const int32_t t1 = static_cast<int32_t>(sx8(a3 + 0x3c));
            if (sets_clean < 0) sets_clean = clean_at(t6, 16 * 16) ? 1 : 0;  // (the 16 sets a vertex can name)
            if (base_clean && sets_clean != 0) loop_set_per_vertex<true>(u, t6, t3, t4, t5, t1);
            else loop_set_per_vertex<false>(u, t6, t3, t4, t5, t1);
        }
        // the point lights named by the nibbles of entry[+0x36] (0xf ends), each over the entry's vertices again
        if (sx8(a3 + 0x35) != 0) sb_(a3 + 0x35, 0);
        int32_t lights = static_cast<int32_t>(lhu_(a3 + 0x36) | 0xf0000u);
        for (;;) {
            const uint32_t n = static_cast<uint32_t>(lights) & 0xfu;
            if (n == 0xfu) break;
            lights >>= 4;
            const uint32_t lt = (n << 5) + s1;
            lqc2(u.v[26], lt + 0x10);
            lqc2(u.v[29], lt);
            const uint32_t p3 = s5 + sx16(a3 + 0x30);
            {
                const Q128 o = lq(p3);
                set_words(u.v[20], static_cast<uint32_t>(o.lo) << 2, static_cast<uint32_t>(o.lo >> 32) << 2, static_cast<uint32_t>(o.hi) << 2,
                          static_cast<uint32_t>(o.hi >> 32) << 2);
                for (int i = 0; i < 3; ++i) u.v[20][i] = vu0m::itof(u.v[20][i], 1.0f / 4096.0f);
            }
            const int32_t t1 = static_cast<int32_t>(sx8(a3 + 0x3c));
            // (vf20's xyz and vf10's are integers' floats; vf1's xyz from the scratchpad are the normals made above)
            if (base_clean && clean_v(u.v[26]) && clean_v(u.v[29])) loop_point_light<true>(u, R, s5, p3 + 0x10, s6, t1);
            else loop_point_light<false>(u, R, s5, p3 + 0x10, s6, t1);
        }
        // the entry's colours to its data (DMA fromSPR), the other buffer next
        while ((lw_(0x1000d000u) & 0x100u) != 0) {
        }
        sw_(0x1000d080u, s6 & 0xffffu);
        sw_(0x1000d010u, lw_(a3 + 0x10) + sx16(a3 + 0x1e));
        sw_(0x1000d020u, sx8(a3 + 0x29));
        sw_(0x1000d000u, 0x100u);
        s6 = 0x70000000u | ((0x2000u - (s6 & 0xffffu)) + 0x1000u);
    }
    while ((lw_(0x1000d000u) & 0x100u) != 0) {
    }
    // the s registers put back from the spill (the routine's locals: nothing kept); the VU0 registers as the routine leaves them
    std::memcpy(R.vf.data(), u.v, sizeof u.v);
    std::memcpy(&R.acc, u.acc, sizeof u.acc);
    R.q = u.q;
    return true;
}

}  // namespace rac1
