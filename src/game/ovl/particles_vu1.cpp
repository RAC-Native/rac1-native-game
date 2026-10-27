// Ported VU1 microcode: the particle program (see particles_vu1.h), instruction by instruction
// (the micro addresses in the comments). Its state lives in the VU1 registers and memory as on
// the hardware: vf16..vf31, vi8, vi14 and vi15 (the double buffer) are kept from one start to the
// next. Float ops follow the VU rules (operands and results: denormals signed zero, Inf / NaN the
// signed largest float); a DIV's Q is used 7 instructions later, by the vertex it was issued
// for. MAC / status flags are not modelled (the program does not read them).
//
// Input chunk at the double buffer vi15 (qwords): per particle +0 the position relative to the
// camera (w its type: 0 a sprite turned by the angle, 1 a sprite of FPU-turned size, -1 a line,
// -2 a streak; w of the first: the chunk's quadword count), then for a sprite +1 the colour
// words (alpha the fade), +2 (sin, cos) x size / depth and the GS register word, +3 TEX0; a line
// +1 its colour, +2 the other end, +3 that end's colour; a streak +1 the other end, +2 the half
// width across, +3 / +4 the ends' colours, +5 TEX0 and the GS register word. Output from vi15 +
// 0x80: per particle a GIF tag (vf16 sprite, vf17 line, vf18 streak) and its data, the last tag
// made EOP, kicked from vi15 + 0x80.
#include <cstdio>
#include <game/ovl/particles_vu1.h>

#include <game/ovl/ovl_common.h>
#include <runtime/vu/vu1.h>

namespace rac1 {

namespace {

constexpr int X = 1, Y = 2, Z = 4, W = 8, XY = X | Y, XYZ = X | Y | Z, XYZW = 15;

// orig: (helper) a VU float operand / result by the VU rules
float op(const VUVector& v, int l) { return vuc(u2f(v.u[l])); }
void put(VUVector& d, int l, float r) { d.u[l] = f2u(vuc(r)); }
// orig: (helper) the FMAC ops: d = a op b (b a vector lane by lane, or a broadcast field)
void mul(VUVector& d, const VUVector& a, const VUVector& b, int m) {
    const VUVector A = a, B = b;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) * op(B, l));
}
void mulbc(VUVector& d, const VUVector& a, float b, int m) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) * vuc(b));
}
// orig: (helper) ADD
void add(VUVector& d, const VUVector& a, const VUVector& b, int m) {
    const VUVector A = a, B = b;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) + op(B, l));
}
void sub(VUVector& d, const VUVector& a, const VUVector& b, int m) {
    const VUVector A = a, B = b;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) - op(B, l));
}
// orig: (helper) ADDBC
void addbc(VUVector& d, const VUVector& a, float b, int m) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) + vuc(b));
}
void subbc(VUVector& d, const VUVector& a, float b, int m) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) put(d, l, op(A, l) - vuc(b));
}
// orig: (helper) MULA / MADDA (broadcast) into ACC, MADD (broadcast) from ACC into d
void mula(VUState& s, const VUVector& a, float b) { mulbc(s.acc, a, b, XYZW); }
void madda(VUState& s, const VUVector& a, float b) {
    const VUVector A = a;
    for (int l = 0; l < 4; ++l) put(s.acc, l, op(s.acc, l) + op(A, l) * vuc(b));
}
// orig: (helper) MADD
void madd(VUState& s, VUVector& d, const VUVector& a, float b) {
    const VUVector A = a, C = s.acc;
    for (int l = 0; l < 4; ++l) put(d, l, op(C, l) + op(A, l) * vuc(b));
}
// orig: (helper) the view transform of p: ACC = vf31 w, + vf28 x, + vf29 y, d = ACC + vf30 z
void view(VUState& s, VUVector& d, float x, float y, float z) {
    mula(s, s.vf[31], op(s.vf[0], 3));
    madda(s, s.vf[28], x);
    madda(s, s.vf[29], y);
    madd(s, d, s.vf[30], z);
}
// orig: (helper) FTOI4 of all four lanes (x 16, truncated, saturating)
void ftoi4(VUVector& d) {
    for (int l = 0; l < 4; ++l) {
        const float f = u2f(d.u[l]) * 16.0f;
        const uint32_t u = f2u(f);
        d.u[l] = (u & 0x7f800000u) >= 0x4f000000u ? ((u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu) : static_cast<uint32_t>(static_cast<int32_t>(f));
    }
}
// orig: (helper) DIV Q = vf25.x / w (by zero: the signed largest float)
float divq(const VUState& s, const VUVector& d) {
    const float a = op(s.vf[25], 0), b = op(d, 3);
    if (b == 0.0f) return u2f(((s.vf[25].u[0] ^ d.u[3]) & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu);
    return vuc(a / b);
}
// orig: (helper) LQ / SQ / ILW / ISW / MTIR / MFIR on the 16 KB memory (qword addresses wrap)
void lq(VUState& s, VUVector& d, int32_t a, int m = XYZW) {
    const uint32_t* q = reinterpret_cast<const uint32_t*>(&s.mem[(static_cast<uint32_t>(a) & 0x3ff) << 4]);
    for (int l = 0; l < 4; ++l) if (m & (1 << l)) d.u[l] = q[l];
}
void sq(VUState& s, const VUVector& v, int32_t a) {
    std::memcpy(&s.mem[(static_cast<uint32_t>(a) & 0x3ff) << 4], v.u, 16);
}
// orig: (helper) ILW
int16_t ilw(const VUState& s, int32_t a, int lane) {
    uint32_t w;
    std::memcpy(&w, &s.mem[((static_cast<uint32_t>(a) & 0x3ff) << 4) + 4 * static_cast<uint32_t>(lane)], 4);
    return static_cast<int16_t>(w & 0xffff);
}
// orig: (helper) ISWX
void iswx(VUState& s, int16_t v, int32_t a) {
    const uint32_t w = static_cast<uint16_t>(v);
    std::memcpy(&s.mem[(static_cast<uint32_t>(a) & 0x3ff) << 4], &w, 4);
}
int16_t mtir(const VUVector& v, int lane) { return static_cast<int16_t>(v.u[lane] & 0xffff); }
void mfirx(VUVector& d, int16_t v) { d.u[0] = static_cast<uint32_t>(static_cast<int32_t>(v)); }
// orig: (helper) ADDx.w d, vf0, vf0x
void one_w(VUState& s, VUVector& d) { put(d, 3, op(s.vf[0], 3) + op(s.vf[0], 0)); }

}  // namespace

// orig: 0x00101088
void particles_vu1(VUState& s, uint32_t start, const std::function<void(uint32_t)>& kick) {
    auto& vf = s.vf;
    auto& vi = s.vi;
    if (start == 0) {  // 0x000..0x013: the constants
        mulbc(vf[12], vf[12], op(vf[0], 0), Z | W);
        mulbc(vf[13], vf[13], op(vf[0], 0), Z | W);
        static const int regs[16] = {28, 29, 30, 31, 25, 26, 27, 21, 22, 23, 24, 16, 17, 18, 19, 20};
        for (int k = 0; k < 16; ++k) lq(s, vf[regs[k]], k);
        vi[14] = 0x3ff;
        vi[15] = 0x200;
        vi[8] = -2;
        return;
    }
    // 0x014: the other half of the double buffer, its count and first particle
    vi[15] = static_cast<int16_t>(vi[15] + 0x200);
    vi[15] = static_cast<int16_t>(vi[15] & vi[14]);
    vi[12] = ilw(s, vi[15] + 2, 3);
    vi[9] = ilw(s, vi[15], 3);
    lq(s, vf[1], vi[15]);
    vi[10] = vi[15];
    vi[11] = static_cast<int16_t>(vi[15] + 0x80);
    vi[13] = static_cast<int16_t>(vi[15] + 0x80);
    vi[12] = static_cast<int16_t>(vi[12] + vi[10]);
    // (host) a bound on the particles of one start: the buffer's 0x200 quadwords hold fewer than 0x80 of them. Valid data never
    // reaches it; a start on stale data (a chain walked with bytes the game did not write this frame) ran on for good, the
    // count's end never met on the 16-bit address: ended, nothing kicked, said once
    int guard = 0;
    for (;;) {
        if (++guard > 1024) {
            static bool said = false;
            if (!said) std::fprintf(stderr, "[VU1] particles: no end after 1024 particles (stale data): the start ended\n");
            said = true;
            return;
        }
        // 0x01d ibne vi9, vi0 -> 0x03f; its delay slot 0x01e
        const bool not_sprite = vi[9] != 0;
        mula(s, vf[31], op(vf[0], 3));
        lq(s, vf[15], vi[10] + 2);
        bool more;
        if (!not_sprite) {  // 0x01f: a sprite turned by its angle: the centre projected, the corners in screen space
            madda(s, vf[28], op(vf[1], 0));
            lq(s, vf[14], vi[10] + 1);
            madda(s, vf[29], op(vf[1], 1));
            lq(s, vf[20], vi[10] + 3, XY);
            madd(s, vf[2], vf[30], op(vf[1], 2));
            vi[10] = static_cast<int16_t>(vi[10] + 4);
            vi[1] = vi[11];
            mul(vf[12], vf[27], vf[15], XY);
            vi[11] = static_cast<int16_t>(vi[11] + 0xc);
            mulbc(vf[13], vf[27], op(vf[15], 0), Y);
            vi[2] = mtir(vf[15], 2);
            {  // 0x025: SUBy.x vf13, vf0, vf15y | DIV Q, vf25x, vf2w
                const float y = op(vf[15], 1);
                put(vf[13], 0, op(vf[0], 0) - y);
            }
            const float q = divq(s, vf[2]);
            one_w(s, vf[5]);
            lq(s, vf[1], vi[10]);
            one_w(s, vf[8]);
            one_w(s, vf[6]);
            sq(s, vf[16], vi[1]);
            one_w(s, vf[7]);
            mfirx(vf[19], vi[2]);
            add(vf[5], vf[26], vf[12], XYZW);
            sub(vf[8], vf[26], vf[12], XYZW);
            sq(s, vf[14], vi[1] + 3);
            s.q = q;  // 0x02c
            mulbc(vf[2], vf[2], s.q, XYZ);
            sq(s, vf[20], vi[1] + 2);
            add(vf[6], vf[26], vf[13], XYZW);
            sub(vf[7], vf[26], vf[13], XYZW);
            sq(s, vf[19], vi[1] + 1);
            add(vf[5], vf[5], vf[2], XYZ);
            sq(s, vf[21], vi[1] + 4);
            add(vf[6], vf[6], vf[2], XYZ);
            add(vf[7], vf[7], vf[2], XYZ);
            sq(s, vf[22], vi[1] + 6);
            add(vf[8], vf[8], vf[2], XYZ);
            ftoi4(vf[5]);
            sq(s, vf[23], vi[1] + 8);
            ftoi4(vf[6]);
            vi[2] = static_cast<int16_t>(0 - 0x7fff);
            ftoi4(vf[7]);
            sq(s, vf[24], vi[1] + 0xa);
            ftoi4(vf[8]);
            vi[9] = mtir(vf[1], 3);
            sq(s, vf[5], vi[1] + 5);
            sq(s, vf[6], vi[1] + 7);
            sq(s, vf[7], vi[1] + 9);
            more = vi[10] != vi[12];  // 0x03b ibne vi10, vi12 -> 0x01d; delay 0x03c
            sq(s, vf[8], vi[1] + 0xb);
        } else {
            // 0x03f ibltz vi9 -> 0x07c; its delay slot 0x040
            const bool negative = vi[9] < 0;
            lq(s, vf[14], vi[10] + 1);
            if (!negative) {  // 0x041: a sprite of turned size: four corners in view space, each projected
                lq(s, vf[20], vi[10] + 3, XY);
                vi[10] = static_cast<int16_t>(vi[10] + 4);
                add(vf[2], vf[1], vf[15], XY);
                vi[2] = mtir(vf[15], 2);
                sub(vf[5], vf[1], vf[15], XY);
                vi[1] = vi[11];
                subbc(vf[3], vf[1], op(vf[15], 1), X);
                vi[11] = static_cast<int16_t>(vi[11] + 0xc);
                addbc(vf[4], vf[1], op(vf[15], 1), X);
                addbc(vf[3], vf[1], op(vf[15], 0), Y);
                subbc(vf[4], vf[1], op(vf[15], 0), Y);
                mfirx(vf[19], vi[2]);
                mula(s, vf[31], op(vf[0], 3));
                sq(s, vf[16], vi[1]);
                madda(s, vf[28], op(vf[2], 0));
                madda(s, vf[29], op(vf[2], 1));
                sq(s, vf[14], vi[1] + 3);
                madd(s, vf[2], vf[30], op(vf[1], 2));
                mula(s, vf[31], op(vf[0], 3));
                sq(s, vf[20], vi[1] + 2);
                madda(s, vf[28], op(vf[3], 0));
                madda(s, vf[29], op(vf[3], 1));
                sq(s, vf[19], vi[1] + 1);
                madd(s, vf[3], vf[30], op(vf[1], 2));
                const float q2 = divq(s, vf[2]);  // 0x050
                view(s, vf[4], op(vf[4], 0), op(vf[4], 1), op(vf[1], 2));
                s.q = q2;  // 0x057
                mulbc(vf[2], vf[2], s.q, XYZ);
                const float q3 = divq(s, vf[3]);
                view(s, vf[5], op(vf[5], 0), op(vf[5], 1), op(vf[1], 2));
                s.q = q3;  // 0x05e
                mulbc(vf[3], vf[3], s.q, XYZ);
                const float q4 = divq(s, vf[4]);
                s.q = q4;  // 0x065
                mulbc(vf[4], vf[4], s.q, XYZ);
                const float q5 = divq(s, vf[5]);
                one_w(s, vf[2]);
                sq(s, vf[21], vi[1] + 4);
                one_w(s, vf[3]);
                one_w(s, vf[5]);
                sq(s, vf[22], vi[1] + 6);
                one_w(s, vf[4]);
                sq(s, vf[23], vi[1] + 8);
                s.q = q5;  // 0x06c
                mulbc(vf[5], vf[5], s.q, XYZ);
                sq(s, vf[24], vi[1] + 0xa);
                add(vf[2], vf[2], vf[26], XYZ);
                lq(s, vf[1], vi[10]);
                add(vf[3], vf[3], vf[26], XYZ);
                add(vf[4], vf[4], vf[26], XYZ);
                add(vf[5], vf[5], vf[26], XYZ);
                ftoi4(vf[2]);
                vi[2] = static_cast<int16_t>(0 - 0x7fff);
                ftoi4(vf[3]);
                ftoi4(vf[4]);
                ftoi4(vf[5]);
                vi[9] = mtir(vf[1], 3);
                sq(s, vf[2], vi[1] + 5);
                sq(s, vf[3], vi[1] + 7);
                sq(s, vf[4], vi[1] + 9);
                more = vi[10] != vi[12];  // 0x078; delay 0x079
                sq(s, vf[5], vi[1] + 0xb);
            } else {
                // 0x07c ibeq vi9, vi8 -> 0x0a1 (with its upper); delay slot 0x07d
                mula(s, vf[31], op(vf[0], 3));
                const bool streak = vi[9] == vi[8];
                madda(s, vf[28], op(vf[1], 0));
                vi[1] = vi[11];
                if (!streak) {  // 0x07e: a line, both ends projected
                    madda(s, vf[29], op(vf[1], 1));
                    vi[11] = static_cast<int16_t>(vi[11] + 6);
                    madd(s, vf[3], vf[30], op(vf[1], 2));
                    vi[2] = 0x44;
                    view(s, vf[4], op(vf[15], 0), op(vf[15], 1), op(vf[15], 2));
                    const float q3 = divq(s, vf[3]);  // 0x083
                    one_w(s, vf[3]);
                    lq(s, vf[15], vi[10] + 3);
                    mfirx(vf[19], vi[2]);
                    vi[10] = static_cast<int16_t>(vi[10] + 4);
                    s.q = q3;  // 0x08a
                    mulbc(vf[3], vf[3], s.q, XYZ);
                    const float q4 = divq(s, vf[4]);
                    one_w(s, vf[4]);
                    sq(s, vf[17], vi[1]);
                    sq(s, vf[19], vi[1] + 1);
                    sq(s, vf[14], vi[1] + 2);
                    s.q = q4;  // 0x091
                    mulbc(vf[4], vf[4], s.q, XYZ);
                    sq(s, vf[15], vi[1] + 4);
                    lq(s, vf[1], vi[10]);
                    add(vf[3], vf[3], vf[26], XYZ);
                    add(vf[4], vf[4], vf[26], XYZ);
                    ftoi4(vf[3]);
                    ftoi4(vf[4]);
                    vi[9] = mtir(vf[1], 3);
                    vi[2] = static_cast<int16_t>(0 - 0x7fff);
                    sq(s, vf[3], vi[1] + 3);
                    more = vi[10] != vi[12];  // 0x09d; delay 0x09e
                    sq(s, vf[4], vi[1] + 5);
                } else {  // 0x0a1: a streak: the ends moved across by the half width, four corners projected
                    add(vf[2], vf[1], vf[15], XYZ);
                    lq(s, vf[20], vi[10] + 5, XY);
                    sub(vf[3], vf[1], vf[15], XYZ);
                    mulbc(vf[15], vf[15], op(vf[14], 3), XYZ);
                    vi[2] = ilw(s, vi[10] + 5, 2);
                    view(s, vf[2], op(vf[2], 0), op(vf[2], 1), op(vf[2], 2));
                    add(vf[4], vf[14], vf[15], XYZ);
                    vi[11] = static_cast<int16_t>(vi[11] + 0xd);
                    sub(vf[5], vf[14], vf[15], XYZ);
                    mula(s, vf[31], op(vf[0], 3));
                    mfirx(vf[19], vi[2]);
                    madda(s, vf[28], op(vf[3], 0));
                    madda(s, vf[29], op(vf[3], 1));
                    madd(s, vf[3], vf[30], op(vf[3], 2));
                    const float q2 = divq(s, vf[2]);  // 0x0ad
                    mula(s, vf[31], op(vf[0], 3));
                    lq(s, vf[14], vi[10] + 3);
                    madda(s, vf[28], op(vf[4], 0));
                    madda(s, vf[29], op(vf[4], 1));
                    lq(s, vf[15], vi[10] + 4);
                    madd(s, vf[4], vf[30], op(vf[4], 2));
                    vi[10] = static_cast<int16_t>(vi[10] + 6);
                    s.q = q2;  // 0x0b4
                    mulbc(vf[2], vf[2], s.q, XYZ);
                    const float q3 = divq(s, vf[3]);
                    mula(s, vf[31], op(vf[0], 3));
                    sq(s, vf[18], vi[1]);
                    madda(s, vf[28], op(vf[5], 0));
                    madda(s, vf[29], op(vf[5], 1));
                    sq(s, vf[14], vi[1] + 3);
                    madd(s, vf[5], vf[30], op(vf[5], 2));
                    sq(s, vf[15], vi[1] + 8);
                    s.q = q3;  // 0x0bb
                    mulbc(vf[3], vf[3], s.q, XYZ);
                    const float q4 = divq(s, vf[4]);
                    sq(s, vf[19], vi[1] + 1);
                    sq(s, vf[20], vi[1] + 2);
                    s.q = q4;  // 0x0c2
                    mulbc(vf[4], vf[4], s.q, XYZ);
                    const float q5 = divq(s, vf[5]);
                    one_w(s, vf[2]);
                    sq(s, vf[21], vi[1] + 4);
                    one_w(s, vf[3]);
                    one_w(s, vf[5]);
                    sq(s, vf[22], vi[1] + 6);
                    one_w(s, vf[4]);
                    sq(s, vf[23], vi[1] + 9);
                    s.q = q5;  // 0x0c9
                    mulbc(vf[5], vf[5], s.q, XYZ);
                    sq(s, vf[24], vi[1] + 0xb);
                    add(vf[2], vf[2], vf[26], XYZ);
                    lq(s, vf[1], vi[10]);
                    add(vf[3], vf[3], vf[26], XYZ);
                    add(vf[4], vf[4], vf[26], XYZ);
                    add(vf[5], vf[5], vf[26], XYZ);
                    ftoi4(vf[2]);
                    vi[2] = static_cast<int16_t>(0 - 0x7fff);
                    ftoi4(vf[3]);
                    ftoi4(vf[4]);
                    ftoi4(vf[5]);
                    vi[9] = mtir(vf[1], 3);
                    sq(s, vf[2], vi[1] + 5);
                    sq(s, vf[3], vi[1] + 7);
                    sq(s, vf[4], vi[1] + 0xa);
                    more = vi[10] != vi[12];  // 0x0d5; delay 0x0d6
                    sq(s, vf[5], vi[1] + 0xc);
                }
            }
        }
        if (more) continue;
        // [E] XGKICK vi13; the delay slot makes the last tag EOP before the GIF reads it
        iswx(s, vi[2], vi[1]);
        kick(static_cast<uint16_t>(vi[13]));
        return;
    }
}

// orig: (none) the port's registration with the VU1 backend: the program's 217 instructions at
// micro address 0 hash (fnv1a64) to 0x901435981696d0d3
void particles_vu1_register() {
    vu1::register_program(0, 217, 0x901435981696d0d3ull, &particles_vu1, "particles");
}

}  // namespace rac1
