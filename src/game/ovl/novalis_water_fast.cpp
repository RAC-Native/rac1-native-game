// nov_water_grid_light_l1 (novalis_water_asm.cpp, L1:0x00261df8) native (2026-11-04, the Wii U's game step; RAC1_NWG_FAST, the Wii
// U's default 1, the PC's 0). The transliterated routine runs each VU0 lane op through the shared register file - a call, the
// operands and the result clamped, the registers in memory - and each MMI unpack byte by byte: on Novalis the water's grid was
// ~14% of the game thread (17 x 17 vertices a tile, ~40 ops each). Here the same routine: the same memory reads and writes in the
// same order (the heights at 0x70000070.., the colours to 0x70001088.., the uvs to 0x70000780..), the VU0 registers in a local copy
// for the whole call - as words, the lanes' bits kept whatever they hold - with the same ops and the same rules (vuc on the
// operands and the result, the masked lanes only, ACC and Q as the ops leave them, VOPMULA / VOPMSUB's MAC and status flags), the
// GPR the routine moves its words through (at) followed whole - its high half reaches vf25..vf29 -, and all of it written back at
// the end: the VU0 file as the routine leaves it. Not used while a VU0 micro program runs (the routine's macro ops would wait for
// it: the transliterated routine then).
// orig: (host)
#include <game/ovl/novalis_water.h>

#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/vu0_regs.h>
#include <runtime/vu/vu0.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rac1 {

namespace {
using namespace eer;

// the VU0 registers as words (a lane's bits kept: a word moved in by QMTC2 may be any pattern)
struct Vu {
    uint32_t v[32][4];
    uint32_t acc[4];
    float q;
    uint32_t mac, status;
};

[[gnu::always_inline]] inline float F(uint32_t w) { return u2f(w); }
[[gnu::always_inline]] inline uint32_t W(float f) { return f2u(f); }
[[gnu::always_inline]] inline float cf(uint32_t w) { return vuc(u2f(w)); }  // (an operand, clamped)
[[gnu::always_inline]] inline uint32_t cr(float f) { return f2u(vuc(f)); }   // (a result, clamped)

// the no-flags ops (vu0_nf.h's: the operands and the result clamped; the lanes computed first, then written)
template <int M, class Fn>
[[gnu::always_inline]] inline void lanes(uint32_t* d, Fn f) {
    uint32_t r[4];
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = f(i);
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) d[i] = r[i];
}
template <int M>
[[gnu::always_inline]] inline void add(uint32_t* d, const uint32_t* a, const uint32_t* b) {
    lanes<M>(d, [&](int i) { return cr(cf(a[i]) + cf(b[i])); });
}
template <int M>
[[gnu::always_inline]] inline void sub(uint32_t* d, const uint32_t* a, const uint32_t* b) {
    lanes<M>(d, [&](int i) { return cr(cf(a[i]) - cf(b[i])); });
}
template <int M>
[[gnu::always_inline]] inline void mul(uint32_t* d, const uint32_t* a, const uint32_t* b) {
    lanes<M>(d, [&](int i) { return cr(cf(a[i]) * cf(b[i])); });
}
template <int M>
[[gnu::always_inline]] inline void addbc(uint32_t* d, const uint32_t* a, uint32_t b) {
    const float y = cf(b);
    lanes<M>(d, [&](int i) { return cr(cf(a[i]) + y); });
}
template <int M>
[[gnu::always_inline]] inline void subbc(uint32_t* d, const uint32_t* a, uint32_t b) {
    const float y = cf(b);
    lanes<M>(d, [&](int i) { return cr(cf(a[i]) - y); });
}
template <int M>
[[gnu::always_inline]] inline void mulbc(uint32_t* d, const uint32_t* a, float b) {  // (`b` a lane's float, or Q)
    const float y = vuc(b);
    lanes<M>(d, [&](int i) { return cr(cf(a[i]) * y); });
}
// VMADDbc: d = ACC + a b (the product not clamped before the sum)
template <int M>
[[gnu::always_inline]] inline void maddbc(Vu& u, uint32_t* d, const uint32_t* a, uint32_t b) {
    const float y = cf(b);
    lanes<M>(d, [&](int i) { return cr(cf(u.acc[i]) + cf(a[i]) * y); });
}
// VADDAbc: ACC = a + b
template <int M>
[[gnu::always_inline]] inline void addabc(Vu& u, const uint32_t* a, uint32_t b) { addbc<M>(u.acc, a, b); }

// a lane's MAC bits (zero, sign, underflow, overflow at 3 - i, 7 - i, 11 - i, 15 - i) and its value clamped (vu0_regs.h's fin_lanes /
// mac_lane_in: a zero exponent the signed zero, the largest the signed largest float)
[[gnu::always_inline]] inline uint32_t fin(uint32_t v, int i, uint32_t& mac) {
    const uint32_t e = (v >> 23) & 0xffu, j = 3u - static_cast<uint32_t>(i);
    uint32_t r = v;
    if (e == 0xffu) {
        r = (v & 0x80000000u) | 0x7f7fffffu;
        mac |= 1u << (12 + j);
    } else if (e == 0) {
        r = v & 0x80000000u;
        mac |= 1u << j;
        if ((v & 0x7fffffu) != 0) mac |= 1u << (8 + j);
    }
    mac |= (v >> 31) << (4 + j);
    return r;
}
[[gnu::always_inline]] inline uint32_t status_bits(uint32_t mac) {
    uint32_t st = 0;
    if ((mac & 0x000f) != 0) st |= 1;
    if ((mac & 0x00f0) != 0) st |= 2;
    if ((mac & 0x0f00) != 0) st |= 4;
    if ((mac & 0xf000) != 0) st |= 8;
    return st;
}
// VOPMULA (vu0_regs.h's: ACC.xyz = a.yzx b.zxy, each lane's MAC flags set - w's kept -, the status from them)
[[gnu::always_inline]] inline void opmula(Vu& u, const uint32_t* a, const uint32_t* b) {
    uint32_t mac = u.mac & 0x1111u;  // (w's bits kept)
    uint32_t r[3];
    for (int i = 0; i < 3; ++i) {
        uint32_t m1 = 0;
        r[i] = fin(f2u(cf(a[(i + 1) % 3]) * cf(b[(i + 2) % 3])), i, m1);
        mac |= m1;
    }
    for (int i = 0; i < 3; ++i) u.acc[i] = r[i];
    u.mac = mac;
    const uint32_t st = status_bits(mac);
    u.status = (u.status & 0xfc0u) | st | (st << 6);
}
// VOPMSUB (vu0_regs.h's: d.xyz = ACC - a.yzx b.zxy, the product and the difference rounded apart, the operands and ACC clamped; the
// xyz lanes' MAC flags, w's cleared)
[[gnu::always_inline]] inline void opmsub(Vu& u, uint32_t* d, const uint32_t* a, const uint32_t* b) {
    static constexpr int ai[3] = {1, 2, 0}, bi[3] = {2, 0, 1};
    uint32_t mac = 0, r[3];
    for (int i = 0; i < 3; ++i) {
        const float p = cf(a[ai[i]]) * cf(b[bi[i]]);
        r[i] = fin(f2u(cf(u.acc[i]) - p), i, mac);
    }
    for (int i = 0; i < 3; ++i) d[i] = r[i];
    u.mac = mac;
    const uint32_t st = status_bits(mac);
    u.status = (u.status & 0xfc0u) | st | (st << 6);
}

// QMTC2 / QMFC2 of a GPR's 128 bits
[[gnu::always_inline]] inline void mtc2(uint32_t* d, const Q128& x) {
    d[0] = static_cast<uint32_t>(x.lo), d[1] = static_cast<uint32_t>(x.lo >> 32), d[2] = static_cast<uint32_t>(x.hi), d[3] = static_cast<uint32_t>(x.hi >> 32);
}
[[gnu::always_inline]] inline Q128 mfc2(const uint32_t* s) {
    Q128 r;
    r.lo = s[0] | static_cast<uint64_t>(s[1]) << 32;
    r.hi = s[2] | static_cast<uint64_t>(s[3]) << 32;
    return r;
}
[[gnu::always_inline]] inline void lqc2(uint32_t* d, uint32_t a) { mtc2(d, lq(a)); }
// a neighbour's height: SET32 of the word, PCPYLD with zero (lo 0, hi the word sign-extended), QMTC2
[[gnu::always_inline]] inline void load_side(Q128& at, uint32_t* d, uint32_t a) {
    set32(at, lw_(a));
    at.hi = at.lo;
    at.lo = 0;
    mtc2(d, at);
}
}  // namespace

namespace {
int fast_mode() {
    static const int m = [] {
        const char* e = std::getenv("RAC1_NWG_FAST");
#if defined(__WIIU__)
        return e == nullptr ? 1 : std::atoi(e);
#else
        return e == nullptr ? 0 : std::atoi(e);
#endif
    }();
    return m;
}
bool g_in_verify = false;  // (RAC1_NWG_FAST=2: the transliterated routine run by the check, the hook not taken)
}  // namespace

bool nov_water_grid_light_native(uint32_t tile);

// orig: L1:0x00261df8 (the host form; false: not taken, the transliterated routine runs). (testing, RAC1_NWG_FAST=2: the host form
// run, its writes and the VU0 state kept, the VU0 state put back, the transliterated routine run, both compared - the colours, the
// uvs, every VU0 register, ACC, Q, the MAC and status flags -, the first differences on stderr)
bool nov_water_grid_light_fast(uint32_t tile) {
    if (g_in_verify || fast_mode() == 0 || vu0::running()) return false;
    if (fast_mode() != 2) return nov_water_grid_light_native(tile);
    vu0m::Regs& R = vu0m::regs();
    const vu0m::Regs before = R;
    if (!nov_water_grid_light_native(tile)) return false;
    const vu0m::Regs mine = R;
    constexpr uint32_t kCol = 0x70001088u, kUv = 0x70000780u, nCol = 17 * 17 * 4, nUv = 17 * 17 * 8;
    static uint8_t col[nCol], uv[nUv];
    for (uint32_t i = 0; i < nCol; ++i) col[i] = static_cast<uint8_t>(lbu(kCol + i));
    for (uint32_t i = 0; i < nUv; ++i) uv[i] = static_cast<uint8_t>(lbu(kUv + i));
    R = before;
    g_in_verify = true;
    nov_water_grid_light_l1(tile);
    g_in_verify = false;
    static uint64_t calls = 0, bad = 0, data_bad = 0, flags_bad = 0;
    ++calls;
    int diffs = 0;
    for (uint32_t i = 0; i < nCol; ++i) diffs += static_cast<uint8_t>(lbu(kCol + i)) != col[i] ? 1 : 0;
    for (uint32_t i = 0; i < nUv; ++i) diffs += static_cast<uint8_t>(lbu(kUv + i)) != uv[i] ? 1 : 0;
    int regs_bad = 0;
    for (int r = 0; r < 32; ++r)
        if (std::memcmp(&R.vf[static_cast<size_t>(r)], &mine.vf[static_cast<size_t>(r)], 16) != 0) {
            if (bad < 4) {
                uint32_t a[4], b[4];
                std::memcpy(a, &R.vf[static_cast<size_t>(r)], 16);
                std::memcpy(b, &mine.vf[static_cast<size_t>(r)], 16);
                std::printf("[nwg-fast] vf%d: routine %08x %08x %08x %08x, host %08x %08x %08x %08x\n", r, a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
            }
            ++regs_bad;
        }
    const bool acc_bad = std::memcmp(&R.acc, &mine.acc, 16) != 0, q_bad = f2u(R.q) != f2u(mine.q);
    const bool fl_bad = R.macflag != mine.macflag || R.statusflag != mine.statusflag;
    if (diffs != 0 || regs_bad != 0 || acc_bad || q_bad) ++data_bad;
    else if (fl_bad) ++flags_bad;  // (the PC: its routine's every op sets the flags, vu0m's; the Wii U's only VOPMULA / VOPMSUB, vu0_nf.h's - this form's)
    if (diffs != 0 || regs_bad != 0 || acc_bad || q_bad || fl_bad) {
        if (bad++ < 4)
            std::printf("[nwg-fast] tile %08x: %d bytes differ, %d registers, ACC %d, Q %d, flags %d (mac %04x / %04x, status %03x / %03x)\n", tile, diffs,
                         regs_bad, acc_bad, q_bad, fl_bad, R.macflag, mine.macflag, R.statusflag, mine.statusflag);
    }
    if (calls % 200 == 0)
        std::printf("[nwg-fast] %llu calls checked: %llu with the data different, %llu with the flags alone\n", static_cast<unsigned long long>(calls),
                    static_cast<unsigned long long>(data_bad), static_cast<unsigned long long>(flags_bad));
    return true;
}

bool nov_water_grid_light_native(uint32_t tile) {
    vu0m::Regs& R = vu0m::regs();
    Vu u;
    std::memcpy(u.v, R.vf.data(), sizeof u.v);
    std::memcpy(u.acc, &R.acc, sizeof u.acc);
    u.q = R.q;
    u.mac = R.macflag;
    u.status = R.statusflag;
    uint32_t(&vf)[32][4] = u.v;
    const uint32_t* v0 = vf[0];
    Q128 at;  // (the routine's at: its high half reaches vf25 through QMTC2)
    // the constants: vf14 (0.5, 65536 + 0x40 ulp, ~0.573, 3.25), vf15 / vf16 / vf21 from 0x1cad00.., vf20 the vector at 0x167240, vf19.x 1
    set32(at, 0x3f000000u);
    mtc2(vf[14], at);
    set32(at, 0x47800040u);  // (v0: the same register shape)
    Q128 t;
    t = at;
    mtc2(vf[1], t);
    addbc<2>(vf[14], v0, vf[1][0]);
    set32(t, 0x3f12b021u);
    mtc2(vf[1], t);
    addbc<4>(vf[14], v0, vf[1][0]);
    set32(t, 0x40500000u);
    mtc2(vf[1], t);
    const uint32_t c = L1_ADDR(0x001cad00u);
    set32(at, c);
    lqc2(vf[15], c + 0x10);
    addbc<8>(vf[14], v0, vf[1][0]);
    lqc2(vf[16], c);
    addbc<1>(vf[19], v0, v0[3]);
    lqc2(vf[21], c + 0x30);
    const uint32_t c2 = L1_ADDR(0x00167100u);
    set32(at, c2);
    uint32_t t4 = 0x70000070u, t6 = 0x70000780u, t7 = 0x70001088u;
    lqc2(vf[20], c2 + 0x140);
    addbc<15>(vf[10], v0, v0[0]);
    addbc<15>(vf[11], v0, v0[0]);
    addbc<15>(vf[12], v0, v0[0]);
    addbc<15>(vf[13], v0, v0[0]);
    subbc<2>(vf[10], vf[10], vf[15][1]);
    addbc<2>(vf[11], vf[11], vf[15][1]);
    addbc<2>(vf[12], vf[12], vf[15][1]);
    subbc<2>(vf[13], vf[13], vf[15][1]);
    subbc<1>(vf[11], vf[12], vf[15][0]);
    addbc<1>(vf[13], vf[13], vf[15][0]);
    lqc2(vf[17], tile);
    addbc<2>(vf[18], vf[17], vf[16][3]);
    for (uint32_t row = 0; row < 0x11; ++row) {
        addbc<1>(vf[18], vf[17], vf[16][2]);
        uint32_t t5 = t4;
        set32(at, lw_(t5));
        mtc2(vf[25], at);
        load_side(at, vf[26], t5 - 0x4);
        load_side(at, vf[27], t5 + 0x4);
        load_side(at, vf[28], t5 - 0x60);
        load_side(at, vf[29], t5 + 0x60);
        t5 += 4;
        for (uint32_t col = 0; col < 0x11; ++col) {
            // the normal: two cross products of the neighbours' differences, summed, normalized
            addbc<4>(vf[18], v0, vf[25][0]);
            subbc<4>(vf[10], vf[18], vf[29][2]);
            subbc<4>(vf[11], vf[29], vf[27][2]);
            subbc<4>(vf[12], vf[18], vf[28][2]);
            subbc<4>(vf[13], vf[28], vf[26][2]);
            opmula(u, vf[11], vf[10]);
            opmsub(u, vf[1], vf[10], vf[11]);
            opmula(u, vf[13], vf[12]);
            opmsub(u, vf[2], vf[12], vf[13]);
            add<7>(vf[1], vf[1], vf[2]);
            mul<7>(vf[3], vf[1], vf[1]);
            addabc<1>(u, vf[3], vf[3][1]);
            maddbc<1>(u, vf[3], vf[19], vf[3][2]);
            u.q = vu0m::vrsqrt(F(v0[3]), F(vf[3][0]));
            // the next vertex's heights
            set32(at, lw_(t5));
            mtc2(vf[25], at);
            set32(at, lw_(t5 - 0x4));
            addbc<4>(vf[18], vf[18], vf[17][2]);
            at.hi = at.lo;
            at.lo = 0;
            mtc2(vf[26], at);
            load_side(at, vf[27], t5 + 0x4);
            load_side(at, vf[28], t5 - 0x60);
            load_side(at, vf[29], t5 + 0x60);
            mulbc<7>(vf[1], vf[1], u.q);
            t5 += 4;
            // the view vector to the light's point, its length; the colour from the normal's facing
            sub<7>(vf[2], vf[20], vf[18]);
            sub<7>(vf[1], v0, vf[1]);
            mul<7>(vf[4], vf[2], vf[2]);
            addabc<1>(u, vf[4], vf[4][1]);
            maddbc<1>(u, vf[3], vf[19], vf[4][2]);
            u.q = vu0m::vrsqrt(F(v0[3]), F(vf[3][0]));
            mul<7>(vf[4], vf[21], vf[1]);
            addabc<1>(u, vf[4], vf[4][1]);
            maddbc<1>(u, vf[3], vf[19], vf[4][2]);
            subbc<1>(vf[3], vf[3], vf[14][2]);
            mulbc<1>(vf[3], vf[3], F(vf[14][3]));
            addbc<1>(vf[3], vf[3], vf[14][2]);
            vf[3][0] = W(vu0m::vmax(F(vf[3][0]), F(v0[0])));
            vf[3][0] = W(vu0m::vmini(F(vf[3][0]), F(vf[19][0])));
            mulbc<7>(vf[2], vf[2], u.q);
            addbc<1>(vf[3], vf[3], vf[14][1]);
            sub<7>(vf[5], v0, vf[2]);
            at = mfc2(vf[3]);
            mul<7>(vf[4], vf[5], vf[1]);
            at = pextlb(at, at);
            addabc<1>(u, vf[4], vf[4][1]);
            at = pextlh(at, at);
            maddbc<1>(u, vf[4], vf[19], vf[4][2]);
            // the reflection: 2 (n.v) n - v when n.v >= 0, the view's own direction otherwise
            const bool facing_away = (vf[4][0] >> 31) != 0;
            mulbc<7>(vf[1], vf[1], F(vf[4][0]));
            sub<7>(vf[1], vf[1], vf[5]);
            if (!facing_away) {
                add<7>(vf[1], vf[1], vf[1]);
                add<7>(vf[2], vf[5], vf[1]);
            }
            // the uvs: (v.xy w + 1) 0.5; the colour's bytes
            mulbc<3>(vf[2], vf[2], F(vf[15][2]));
            mulbc<3>(vf[2], vf[2], F(vf[14][0]));
            at.lo &= 0xffffffu;
            addbc<3>(vf[2], vf[2], vf[14][0]);
            sw_(t7, static_cast<uint32_t>(at.lo));
            at = mfc2(vf[2]);
            sd_(t6, at.lo);
            addbc<1>(vf[18], vf[18], vf[15][0]);
            t7 += 4;
            t6 += 8;
        }
        addbc<2>(vf[18], vf[18], vf[15][1]);
        t4 += 0x60;
    }
    std::memcpy(R.vf.data(), u.v, sizeof u.v);
    std::memcpy(&R.acc, u.acc, sizeof u.acc);
    R.q = u.q;
    R.macflag = u.mac;
    R.statusflag = u.status;
    return true;
}

}  // namespace rac1
