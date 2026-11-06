// The moby drop shadows' VU0 routines (moby_shadows_asm.cpp's), written by hand from the routines: the same arithmetic, the same
// memory read and written in the same order, the VU0 registers as a local copy (vu0_local.h) written back at the end. Unlike the
// world passes' routines, these read the MAC flags after ordinary ops (the tube's end test), so their transliteration runs
// vu0_regs.h's ops, every op making its flags: the copy here keeps them the same way (vu0l::VuF) and makes them words where they are
// read and at the end. Where a routine runs the VU0 micro programs 0x192 / 0x190 (a sine and a cosine), the copy is written back
// before and read again after. The transliterations (moby_shadows_asm.cpp's *_tr) stay the reference: RAC1_DROP_SHADOWS_FAST
// (moby_shadows_fast.cpp) picks one or checks one against the other.
//
// What the routines do (one frame's shadows: moby_shadows -> drop_shadows_draw, moby_shadows.cpp):
//  - shadow_points (0x285a68): for each moby the draw loop deferred for its shadow (0x1ac600: the moby, two halfwords), its joint
//    matrices (moby_pose_matrices into the scratchpad) and its class's shadow list (class +0x14 / +0xf): each entry one joint
//    (0x20 bytes: a point) or two (0x30: a segment, its ends pulled towards each other by two weights), through the joints and the
//    moby's matrix (+0xc0 .. +0xe0, position +0x10), scaled by its +0x2c; written to the list 0x1ac780 (a header and the
//    entries, 0x20 / 0x30 bytes, ended by an empty quadword at the end; 0x1f80 bytes at most);
//  - shadow_point_draw (0x2860a8) / shadow_segment_draw (0x2861f8): one entry made a tube's section as point pairs (near, far =
//    near + the light's vector 0x160450) in the scratchpad: a circle of `n` points about the light's axis (0x160430, the
//    rotation by 2 pi / n from the sine / cosine programs), for a segment the two half circles about its ends (their angles from
//    the segment's slope, asin), the first pair repeated at the end; the count to 0x160460;
//  - shadow_points_clip (0x286848): the pairs moved along themselves to the frame's two planes' z;
//  - shadow_quads (0x285d30): the pairs to the screen (the view matrix 0x166d40.. about the camera, a perspective divide, the
//    viewport 0x16cbd0 / 0x16cbe0), with the clip test when the moby's group asks for it (a pair outside marked), then per quad
//    (a pair and the next) its facing at SPR 0x3000 (the sign of the 2D cross product: 1 or 2, 0 when a point is clipped) and
//    the pair's clip bytes at 0x3800, and the pair in the GS's form: x - 0.5, y - 0.5 in 12.4 fixed point packed with z, and the
//    position against 0x16cbe0 / 0x16cc40 scaled by 1/512 as a float pair; the caps' facing (the first three near and far
//    points' cross products) to 0x160468;
//  - shadow_packets (0x2868c0): the quads of one layer (front / back) as GS packets in the frame's buffer (0x1611c0), then the
//    tube's two caps (the near and far points of the unclipped pairs) when they face the camera.
// orig: (host)
#include <game/ovl/moby_shadows_hand.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/world_cull_hand.h>

#include <runtime/vu/vu0.h>

namespace rac1::shadow_hand {

namespace {

using world_hand::Qw;
using world_hand::Spr;

constexpr uint32_t SPR = 0x70000000u;
constexpr uint32_t FACES = SPR + 0x3000;  // shadow_quads' facing byte per quad
constexpr uint32_t CLIPS = SPR + 0x3800;  // its clip bytes per pair (near, far)

// the frame's shadow data (OV: the overlay's own addresses)
uint32_t light_vec() { return OV(0x00160450u); }   // the light's vector (the tube's length)
uint32_t light_axis() { return OV(0x00160430u); }  // the light's direction, normalised
uint32_t light_side() { return OV(0x00160440u); }  // a unit vector across it
uint32_t pair_count() { return OV(0x00160460u); }  // the tube's point pairs (+ 1)
uint32_t facing() { return OV(0x00160468u); }      // shadow_quads' two caps' 2D cross products
uint32_t packet() { return OV(0x001611c0u); }      // the frame's packet buffer's next quadword

// the GPR model of a routine's `at` / `v0` where a QMTC2 copies them whole: a 32-bit value (sign-extended into the low doubleword)
// keeps the register's high doubleword, which QMFC2 / LQ set
struct Gpr {
    uint64_t lo = 0, hi = 0;
    void set(uint32_t v) { lo = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
};

uint32_t s8(uint32_t v) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(v))); }
uint32_t rd32(uint32_t a) { return ee_read<uint32_t>(a); }
void wr32(uint32_t a, uint32_t v) { ee_write<uint32_t>(a, v); }
void wr64(uint32_t a, uint64_t v) { ee_write<uint64_t>(a & ~7u, v); }

struct Shadows {
    vu0l::VuF u;
    Spr spr;

    // --- the VU0 copy and the GPRs
    void qmtc2(int r, const Gpr& g) {
        u.set_words(r, static_cast<uint32_t>(g.lo), static_cast<uint32_t>(g.lo >> 32), static_cast<uint32_t>(g.hi),
                    static_cast<uint32_t>(g.hi >> 32));
    }
    Gpr qmfc2(int r) const {
        Gpr g;
        g.lo = u.word(r, 0) | static_cast<uint64_t>(u.word(r, 1)) << 32;
        g.hi = u.word(r, 2) | static_cast<uint64_t>(u.word(r, 3)) << 32;
        return g;
    }
    void ram_vf(int r, uint32_t a) { world_hand::ram_load_vf(u, r, a); }
    // a VU0 micro program run on the registers (the copy written back first, read again after)
    void program(uint32_t at) {
        u.store();
        vu0::call(at);
        vu0::drain();
        u.load();
    }
    float one() const { return u.lane(0, 3); }

    // orig: (helper) the sine and cosine programs on vf1 (0x192 into vf31.x, 0x190 into vf31.y), `arg2` the register the second
    // takes (the routines pass the same one, or the angle again with the high half the first program left); `at` the QMFC2 of
    // each result
    void sincos(const Gpr arg1, Gpr& at, bool again_from_at, const Gpr arg2) {
        using namespace vu0l;
        qmtc2(1, arg1);
        program(0x192);
        at = qmfc2(1);
        u.addbc(31, 0, u.lane(1, 0), X);
        if (again_from_at) {
            Gpr g = at;
            g.lo = arg2.lo;
            qmtc2(1, g);
        } else {
            qmtc2(1, arg2);
        }
        program(0x190);
        at = qmfc2(1);
        u.addbc(31, 0, u.lane(1, 0), Y);
    }
    // orig: (helper) the rotation by the angle (vf31: x the sine, y the cosine) about the light's axis (vf30) as the rows vf1 / vf2 /
    // vf3: w = 1 - cos, vf29 = axis * sin, row i lane j = axis.i axis.j (1 - cos) + (cos on the diagonal, +- vf29 off it)
    void rotation_rows() {
        using namespace vu0l;
        u.subbc(31, 0, u.lane(31, 1), W);
        u.mulbc(29, 30, u.lane(31, 0), XYZ);
        const int lane[3] = {X, Y, Z};
        // the off-diagonal terms: row r, lane c: + or - vf29.(other)
        for (int r = 0; r < 3; ++r) {
            const int d = r + 1;
            for (int c = 0; c < 3; ++c) {
                u.mulbc(d, 30, u.lane(30, r), lane[c]);
                u.mulbc(d, d, u.lane(31, 3), lane[c]);
                if (r == c) {
                    u.addbc(d, d, u.lane(31, 1), lane[c]);
                } else {
                    const int other = 3 - r - c;  // the third axis
                    // (r, c) = (0, 1) -z, (0, 2) +y, (1, 0) +z, (1, 2) -x, (2, 0) -y, (2, 1) +x
                    const bool minus = (c == (r + 1) % 3);
                    if (minus) u.subbc(d, d, u.lane(29, other), lane[c]);
                    else u.addbc(d, d, u.lane(29, other), lane[c]);
                }
            }
        }
    }
    // orig: (helper) v = rows * v (VMULAx / VMADDAy / VMADDz on xyz)
    void rotate_by_rows(int d, int v) { u.rotate(d, 1, 2, 3, v, vu0l::XYZ); }

    // --- shadow_points_clip (0x286848)
    void points_clip(uint32_t a0, uint32_t a1) {
        using namespace vu0l;
        int32_t n = static_cast<int32_t>(rd32(pair_count()));
        uint32_t p = SPR;
        u.addbc(20, 0, one(), XYZ);
        ram_vf(10, a0);
        ram_vf(11, a1);
        do {
            spr.load_vf(u, 1, p);
            --n;
            spr.load_vf(u, 2, p + 0x10);
            u.sub(3, 2, 1, XYZ);
            u.div(one(), u.lane(3, 2));
            u.sub(4, 10, 1, Z);
            u.sub(5, 11, 2, Z);
            u.mulbc(4, 3, u.lane(4, 2), XYZ);
            u.mulbc(5, 3, u.lane(5, 2), XYZ);
            u.mulbc(4, 4, u.q, XYZ);
            u.mulbc(5, 5, u.q, XYZ);
            u.add(1, 1, 4, XYZ);
            u.add(2, 2, 5, XYZ);
            spr.store_vf(u, 1, p);
            spr.store_vf(u, 2, p + 0x10);
            p += 0x20;
        } while (n != 0);
    }

    // --- shadow_point_draw (0x2860a8)
    void point_draw(uint32_t e) {
        using namespace vu0l;
        int32_t n = static_cast<int32_t>(rd32(e + 4));
        ram_vf(27, e + 0x10);
        ram_vf(28, light_vec());
        uint32_t p = SPR, pairs = 0;
        ram_vf(30, light_axis());
        u.sub(30, 0, 30, XYZ);
        const float step = fpu_div(u2f(0x40c8f5c3u), static_cast<float>(n));  // 2 pi / n
        Gpr at;
        at.set(f2u(step));
        sincos(at, at, true, at);
        rotation_rows();
        ram_vf(26, light_side());
        u.mulbc(26, 26, u.lane(27, 3), XYZ);
        do {
            rotate_by_rows(26, 26);
            u.add(5, 26, 27, XYZ);
            spr.store_vf(u, 5, p);
            u.add(5, 5, 28, XYZ);
            spr.store_vf(u, 5, p + 0x10);
            p += 0x20;
            ++pairs;
        } while (--n != 0);
        close_tube(p, pairs);
    }
    // orig: (helper) the first pair again after the last; the count
    void close_tube(uint32_t p, uint32_t pairs) {
        spr.load_vf(u, 5, SPR);
        spr.store_vf(u, 5, p);
        spr.load_vf(u, 5, SPR + 0x10);
        spr.store_vf(u, 5, p + 0x10);
        wr32(pair_count(), pairs + 1);
    }

    // --- shadow_segment_draw (0x2861f8)
    void segment_draw(uint32_t e) {
        using namespace vu0l;
        int32_t n1 = static_cast<int32_t>(rd32(e + 4)), n2 = static_cast<int32_t>(rd32(e + 8));
        ram_vf(27, e + 0x10);  // the first end (w its radius)
        ram_vf(25, e + 0x20);  // the second end (w its radius)
        ram_vf(28, light_vec());
        uint32_t p = SPR, pairs = 0;
        ram_vf(30, light_axis());
        // vf24: the segment crossed with the light's axis (across both), its length squared into vf21.x
        u.sub(1, 25, 27, XYZ);
        u.opmula(1, 30);
        u.opmsub(24, 30, 1);
        u.sub(30, 0, 30, XYZ);
        u.addbc(20, 0, one(), XYZ);
        u.mul(1, 24, 24, XYZ);
        u.addabc(1, u.lane(1, 1), X);
        u.maddbc(21, 20, u.lane(1, 2), X);
        u.sqrt(u.lane(21, 0));
        u.addbc(21, 0, u.q, X);
        // the ends' radii against the length: one end inside the other (MAC x sign) -> a point's circle about the first end
        u.addbc(1, 21, u.lane(25, 3), X);
        u.subbc(1, 1, u.lane(27, 3), X);
        Gpr at, v0;
        if ((u.macflags() & 0x80u) != 0) return segment_as_point(n1, p, pairs);
        // the half circles' angles (vf16 the first's start, vf17 / vf18 / vf19 the spans): equal radii (MAC w zero) a half turn each
        u.sub(1, 27, 25, W);
        if ((u.macflags() & 0x1u) == 0) {
            u.mulbc(1, 21, u.lane(25, 3), X);
            u.sub(1, 27, 25, W);
            u.div(u.lane(1, 0), u.lane(1, 3));
            u.mulbc(1, 0, u.q, W);
            u.div(u.lane(25, 3), u.lane(1, 3));
            u.addbc(1, 0, u.q, X);
            at = qmfc2(1);
            const float a = asin_f(u2f(static_cast<uint32_t>(at.lo)));
            at.set(f2u(a));
            qmtc2(1, at);
            at.set(0x3fc8f5c3u);  // pi / 2
            qmtc2(2, at);
            u.sub(16, 2, 1, X);
            at.set(0x4048f5c3u);  // pi
            qmtc2(2, at);
            u.sub(1, 2, 16, X);
        } else {
            at.set(0x3fc8f5c3u);
            qmtc2(16, at);
            at.set(0x4048f5c3u);
            qmtc2(2, at);
            u.sub(1, 2, 16, X);
        }
        u.add(17, 1, 1, X);
        u.add(18, 16, 17, X);
        u.add(19, 16, 16, X);
        // vf24 normalised; vf22 / vf23 its lengths at the two ends' radii
        u.mul(2, 24, 24, XYZ);
        u.addabc(2, u.lane(2, 1), X);
        u.maddbc(2, 20, u.lane(2, 2), X);
        u.rsqrt(one(), u.lane(2, 0));
        u.mulbc(24, 24, u.q, XYZ);
        u.mulbc(22, 24, u.lane(27, 3), XYZ);
        u.mulbc(23, 24, u.lane(25, 3), XYZ);
        // the first end's half circle (n1 steps of vf17 / n1 from the start pi / 2 + vf16), or its two side points
        if (n1 != 0) {
            at.set(0x3fc8f5c3u);
            qmtc2(1, at);
            u.add(1, 1, 16, X);
            v0 = qmfc2(1);
            sincos(v0, at, false, v0);
            rotation_rows();
            rotate_by_rows(26, 22);
            const float step = fpu_div(u.lane(17, 0), static_cast<float>(n1));
            at = qmfc2(17);
            v0.set(f2u(step));
            sincos(v0, at, false, v0);
            rotation_rows();
            do {
                u.add(5, 26, 27, XYZ);
                rotate_by_rows(26, 26);
                spr.store_vf(u, 5, p);
                u.add(5, 5, 28, XYZ);
                --n1;
                ++pairs;
                spr.store_vf(u, 5, p + 0x10);
                p += 0x20;
            } while (n1 != 0);
        } else {
            side_points(27, 22, p);
            pairs += 2;
        }
        // the second end's (from pi / 2 + vf18 - 2 pi, n2 steps of vf19 / n2)
        if (n2 != 0) {
            at.set(0x3fc8f5c3u);
            qmtc2(1, at);
            u.add(1, 1, 18, X);
            at.set(0x40c8f5c3u);  // 2 pi
            qmtc2(2, at);
            u.sub(1, 1, 2, X);
            v0 = qmfc2(1);
            sincos(v0, at, false, v0);
            rotation_rows();
            rotate_by_rows(26, 23);
            const float step = fpu_div(u.lane(19, 0), static_cast<float>(n2));
            at = qmfc2(19);
            v0.set(f2u(step));
            sincos(v0, at, false, v0);
            rotation_rows();
            do {
                u.add(5, 26, 25, XYZ);
                rotate_by_rows(26, 26);
                spr.store_vf(u, 5, p);
                u.add(5, 5, 28, XYZ);
                ++pairs;
                --n2;
                spr.store_vf(u, 5, p + 0x10);
                p += 0x20;
            } while (n2 != 0);
        } else {
            side_points_rev(25, 23, p);
            pairs += 2;
        }
        close_tube(p, pairs);
    }
    // orig: (helper) an end without a half circle: its two side points (end - side, end + side; the far points + the light)
    void side_points(int end, int side, uint32_t& p) {
        using namespace vu0l;
        u.sub(1, end, side, XYZ);
        spr.store_vf(u, 1, p);
        u.add(1, 1, 28, XYZ);
        spr.store_vf(u, 1, p + 0x10);
        u.add(1, end, side, XYZ);
        spr.store_vf(u, 1, p + 0x20);
        u.add(1, 1, 28, XYZ);
        spr.store_vf(u, 1, p + 0x30);
        p += 0x40;
    }
    // (the second end's: + first, then -)
    void side_points_rev(int end, int side, uint32_t& p) {
        using namespace vu0l;
        u.add(1, end, side, XYZ);
        spr.store_vf(u, 1, p);
        u.add(1, 1, 28, XYZ);
        spr.store_vf(u, 1, p + 0x10);
        u.sub(1, end, side, XYZ);
        spr.store_vf(u, 1, p + 0x20);
        u.add(1, 1, 28, XYZ);
        spr.store_vf(u, 1, p + 0x30);
        p += 0x40;
    }
    // a segment whose second end lies inside the first's sphere: the first end's full circle, as a point's
    void segment_as_point(int32_t n, uint32_t p, uint32_t pairs) {
        using namespace vu0l;
        if (n != 0) {
            const float step = fpu_div(u2f(0x40c8f5c3u), static_cast<float>(n));
            Gpr v0, at;
            v0.set(f2u(step));
            sincos(v0, at, false, v0);
            rotation_rows();
            ram_vf(26, light_side());
            u.mulbc(26, 26, u.lane(27, 3), XYZ);
            do {
                rotate_by_rows(26, 26);
                u.add(5, 26, 27, XYZ);
                spr.store_vf(u, 5, p);
                u.add(5, 5, 28, XYZ);
                spr.store_vf(u, 5, p + 0x10);
                p += 0x20;
                ++pairs;
            } while (--n != 0);
        }
        close_tube(p, pairs);
    }

    // --- shadow_quads (0x285d30)
    void quads(uint32_t pts, uint32_t count, uint32_t clipped) {
        using namespace vu0l;
        const uint32_t m = OV(0x00166c80u), vp = OV(0x0016ca40u);
        ram_vf(20, m + 0xc0);
        ram_vf(21, m + 0xd0);
        ram_vf(22, m + 0xe0);
        ram_vf(23, m + 0xf0);
        ram_vf(31, m + 0x140);
        ram_vf(28, vp + 0x190);
        ram_vf(29, vp + 0x1a0);
        ram_vf(18, vp + 0x200);
        u.set_words(19, 0x3b000000u, 0, 0, 0);  // 1/512
        u.set_words(30, 0x44800000u, 0, 0, 0);  // 1024
        u.subbc(30, 0, u.lane(30, 0), Y);       // y -1024
        u.set_words(1, 0x41800000u, 0, 0, 0);
        u.mulbc(30, 0, u.lane(1, 0), W);        // w 16
        u.set_words(1, 0x3f000000u, 0, 0, 0);
        u.addbc(30, 0, u.lane(1, 0), Z);        // z 0.5
        // vf24..vf27: the view matrix (vf20..vf23) through (1024, 0, 0, 0), (0, 1024, 0, 0), (0, 0, 1024, 0) and (-1024 * the camera
        // (vf31), 1): the points (1/1024 units) relative to the camera
        u.mulbc(1, 0, u.lane(0, 0), XYZW);
        u.mulbc(2, 0, u.lane(0, 0), XYZW);
        u.mulbc(3, 0, u.lane(0, 0), XYZW);
        u.move(4, 0, XYZW);
        u.addbc(1, 1, u.lane(30, 0), X);
        u.addbc(2, 2, u.lane(30, 0), Y);
        u.addbc(3, 3, u.lane(30, 0), Z);
        u.mulbc(4, 31, u.lane(30, 1), XYZ);
        u.move(4, 0, W);
        u.transform(24, 20, 21, 22, 23, 1, u.lane(1, 3));
        u.transform(25, 20, 21, 22, 23, 2, u.lane(2, 3));
        u.transform(26, 20, 21, 22, 23, 3, u.lane(3, 3));
        u.transform(27, 20, 21, 22, 23, 4, u.lane(4, 3));
        uint32_t p = pts;
        int32_t n = static_cast<int32_t>(count);
        if (clipped == 0) {
            // two points a step, the projection of the next started before the stores (the routine's software pipeline)
            spr.load_vf(u, 5, p);
            u.mulabc(27, one(), XYZW);
            u.maddabc(26, u.lane(5, 2), XYZW);
            u.maddabc(25, u.lane(5, 1), XYZW);
            u.maddbc(7, 24, u.lane(5, 0), XYZW);
            do {
                u.div(one(), u.lane(7, 3));
                spr.load_vf(u, 6, p + 0x10);
                spr.load_vf(u, 5, p + 0x20);
                --n;
                u.mulabc(27, one(), XYZW);
                u.maddabc(26, u.lane(6, 2), XYZW);
                u.maddabc(25, u.lane(6, 1), XYZW);
                u.maddbc(8, 24, u.lane(6, 0), XYZW);
                u.mulbc(9, 7, u.q, XYZ);
                u.mulabc(27, one(), XYZW);
                u.maddabc(26, u.lane(5, 2), XYZW);
                u.maddabc(25, u.lane(5, 1), XYZW);
                u.maddbc(7, 24, u.lane(5, 0), XYZW);
                u.div(one(), u.lane(8, 3));
                u.mulav(9, 28, XYZW);
                u.maddbc(9, 29, one(), XYZW);
                u.sub(9, 0, 0, W);
                u.mulbc(8, 8, u.q, XYZ);
                u.mulav(8, 28, XYZW);
                u.maddbc(8, 29, one(), XYZW);
                spr.store_vf(u, 9, p);
                u.sub(8, 0, 0, W);
                p += 0x20;
                spr.store_vf(u, 8, p - 0x10);
            } while (n != 0);
        } else {
            // with the clip test: a pair with a point outside gets its w bytes set to 1
            do {
                spr.load_vf(u, 5, p);
                spr.load_vf(u, 6, p + 0x10);
                u.transform(5, 24, 25, 26, 27, 5, one());
                u.clipw(5);
                const uint32_t c1 = u.clip;
                u.div(one(), u.lane(5, 3));
                --n;
                u.transform(6, 24, 25, 26, 27, 6, one());
                u.clipw(6);
                u.mulbc(5, 5, u.q, XYZ);
                const uint32_t c2 = u.clip;
                u.div(one(), u.lane(6, 3));
                const uint32_t out = (c2 | c1) & 0x3fu;
                u.mulav(5, 28, XYZW);
                u.maddbc(5, 29, one(), XYZW);
                u.mulbc(6, 6, u.q, XYZ);
                u.mulav(6, 28, XYZW);
                u.maddbc(6, 29, one(), XYZW);
                u.sub(5, 0, 0, W);
                u.sub(6, 0, 0, W);
                spr.store_vf(u, 5, p);
                spr.store_vf(u, 6, p + 0x10);
                if (out != 0) {
                    spr.set8(p + 0xc, 1);
                    spr.set8(p + 0x1c, 1);
                }
                p += 0x20;
            } while (n != 0);
        }
        // the caps' facing: the 2D cross products of the first three near points and of the first three far ones
        p = pts;
        n = static_cast<int32_t>(count);
        uint32_t face = FACES, clip = CLIPS;
        const uint32_t caps = facing();
        for (uint32_t k = 0; k < 2; ++k) {
            spr.load_vf(u, 5, p + 0x10 * k);
            spr.load_vf(u, 6, p + 0x20 + 0x10 * k);
            spr.load_vf(u, 7, p + 0x40 + 0x10 * k);
            cross2d(7, 6, 5);
            wr32(caps + 4 * k, u.word(1, 0));
        }
        // per quad (this pair and the next): its facing, the pair's clip bytes; the pair to fixed point and packed
        do {
            spr.load_vf(u, 5, p);
            spr.load_vf(u, 6, p + 0x10);
            --n;
            const uint32_t c_near = s8(spr.u8(p + 0xc)), c_next = s8(spr.u8(p + 0x2c));
            const uint32_t any = c_next | c_near;
            const uint32_t c_far = s8(spr.u8(p + 0x1c));
            spr.set8(clip, c_near);
            spr.set8(clip + 1, c_far);
            clip += 2;
            uint32_t f = 0;
            if (any == 0) {
                spr.load_vf(u, 7, p + 0x20);
                cross2d(7, 6, 5);
                f = u.negative(1, 0) ? 2 : 1;
            }
            u.sub(7, 5, 29, XY);
            spr.set8(face, f);
            u.sub(8, 6, 29, XY);
            u.subbc(5, 5, u.lane(30, 2), XY);
            ++face;
            u.subbc(6, 6, u.lane(30, 2), XY);
            u.add(7, 7, 18, XY);
            u.add(8, 8, 18, XY);
            u.mulbc(5, 5, u.lane(30, 3), XY);
            u.mulbc(6, 6, u.lane(30, 3), XY);
            u.mulbc(7, 7, u.lane(19, 0), XY);
            u.mulbc(8, 8, u.lane(19, 0), XY);
            u.ftoi(5, 5, XYZ, 1.0f);
            u.ftoi(6, 6, XYZ, 1.0f);
            // the words: x, y of vf7 / vf8 (the texture coordinates), then x | y << 16 of vf5 / vf6 and their z
            const uint64_t t_near = u.word(7, 0) | static_cast<uint64_t>(u.word(7, 1)) << 32;
            const uint64_t t_far = u.word(8, 0) | static_cast<uint64_t>(u.word(8, 1)) << 32;
            spr.set64(p, t_near);
            spr.set64(p + 0x10, t_far);
            spr.set32(p + 0x8, (u.word(5, 0) & 0xffffu) | (u.word(5, 1) << 16));
            spr.set32(p + 0x18, (u.word(6, 0) & 0xffffu) | (u.word(6, 1) << 16));
            spr.set32(p + 0xc, u.word(5, 2));
            spr.set32(p + 0x1c, u.word(6, 2));
            p += 0x20;
        } while (n != 0);
    }
    // orig: (helper) vf1.x = the 2D cross product of (c - a) and (b - a) (vf3 / vf2 its terms)
    void cross2d(int c, int b, int a) {
        using namespace vu0l;
        u.sub(1, c, a, XY);
        u.sub(2, b, a, XY);
        u.mulbc(3, 2, u.lane(1, 1), X);
        u.mulbc(2, 1, u.lane(2, 1), X);
        u.sub(1, 2, 3, X);
    }

    // --- shadow_packets (0x2868c0)
    void packets(uint32_t layer, uint32_t rgba) {
        using namespace vu0l;
        int32_t n = static_cast<int32_t>(rd32(pair_count()) - 1);
        if (n == 0) return;
        Gpr at;  // (its high doubleword, which the caps' QMTC2 copies)
        // the layer's quads (a GIF tag of 0x54 and the quad's four quadwords, 0x50 bytes each)
        uint32_t face = FACES, src = SPR;
        uint32_t out = rd32(packet()) + 0x40, quads = 0;
        do {
            const uint32_t f = s8(spr.u8(face));
            --n;
            ++face;
            if (f == layer) {
                wr64(out + 8, 0x54);
                wr64(out, 0);
                const Qw q0 = spr.q(src), q1 = spr.q(src + 0x10);
                world_hand::ram_setq(out + 0x10, q0);
                world_hand::ram_setq(out + 0x20, q1);
                const Qw q2 = spr.q(src + 0x20), q3 = spr.q(src + 0x30);
                world_hand::ram_setq(out + 0x30, q2);
                world_hand::ram_setq(out + 0x40, q3);
                at.lo = q3.lo;
                at.hi = q3.hi;
                out += 0x50;
                ++quads;
            }
            src += 0x20;
        } while (n != 0);
        const uint32_t head = rd32(packet());
        const uint32_t qwc = ((out - head) >> 4) - 1;
        wr32(head, 0x10000000u | qwc);
        wr32(head + 4, 0);
        wr32(head + 8, 0);
        wr32(head + 0xc, 0x50000000u | qwc);
        wr32(head + 0x10, 1);
        wr32(head + 0x14, 0x14000000u);
        wr32(head + 0x18, 0xf1);
        wr32(head + 0x1c, 0);
        wr32(head + 0x20, rgba);
        wr32(head + 0x24, 0x3f800000u);
        wr64(head + 0x28, 0);
        wr32(head + 0x30, quads | 0x8000u);
        wr32(head + 0x34, 0xa4000000u);
        wr32(head + 0x38, 0x5252520fu);
        wr32(head + 0x3c, 0x52);
        wr32(packet(), out);
        // the caps: the near one when its facing (times -1 for the back layer 2) is not negative, the far one when it is
        for (uint32_t k = 0; k < 2; ++k) {
            at.set(rd32(facing() + 4 * k));
            qmtc2(2, at);
            if (layer == 2) u.subbc(1, 0, one(), X);
            else u.addbc(1, 0, one(), X);
            u.mul(1, 1, 2, X);
            at = qmfc2(1);
            const int32_t s = static_cast<int32_t>(u.word(1, 0));
            if (k == 0 ? s < 0 : s > 0) {
                if (k == 0) continue;
                return;
            }
            cap(k, rgba, at);
            if (k == 1) return;
        }
    }
    // orig: (helper) a cap: the near (k 0) or far (k 1) points of the pairs whose clip byte is 0, as one fan's packet
    void cap(uint32_t k, uint32_t rgba, Gpr& at) {
        int32_t n = static_cast<int32_t>(rd32(pair_count()));
        uint32_t src = SPR + 0x10 * k, clip = CLIPS + k;
        uint32_t out = rd32(packet()) + 0x40, verts = 0;
        do {
            const uint32_t c = s8(spr.u8(clip));
            --n;
            clip += 2;
            if (c == 0) {
                const Qw q = spr.q(src);
                at.lo = q.lo;
                at.hi = q.hi;
                ++verts;
                world_hand::ram_setq(out, q);
                out += 0x10;
            }
            src += 0x20;
        } while (n != 0);
        if (verts == 0) return;
        const uint32_t head = rd32(packet());
        const uint32_t qwc = verts + 3;
        wr32(head, 0x10000000u | qwc);
        wr32(head + 4, 0);
        wr32(head + 8, 0);
        wr32(head + 0xc, 0x50000000u | qwc);
        wr32(head + 0x10, 1);
        wr32(head + 0x14, 0x24000000u);
        wr64(head + 0x18, 1);
        wr32(head + 0x20, rgba);
        wr32(head + 0x24, 0x3f800000u);
        wr64(head + 0x28, 0x55);
        wr32(head + 0x30, verts | 0x8000u);
        wr32(head + 0x34, 0x24000000u);
        wr64(head + 0x38, 0x52);
        wr32(packet(), head + ((verts + 4) << 4));
    }

    // --- shadow_points (0x285a68)
    void points() {
        using namespace vu0l;
        int32_t left = static_cast<int32_t>(rd32(OV(0x0015fffcu)));
        uint32_t ent = OV(0x001ac600u), out = OV(0x001ac780u);
        const uint32_t kinds = OV(0x001aeb80u), end = out + 0x1f80;
        Gpr at;               // (its high doubleword: the last LQ's, which the QMTC2s below copy)
        uint32_t t9 = 0;      // (the matrices' base, passed on to moby_pose_matrices as the routine's t9)
        for (;;) {
            const uint32_t moby = rd32(ent);
            --left;
            const uint32_t cls = rd32(moby + 0x24);
            u.store();
            moby_pose_matrices(moby, static_cast<int32_t>(ee_read<uint8_t>(cls + 8)), -1, 0, t9);
            u.load();
            at.set(rd32(moby + 0x2c));
            const uint32_t h4 = ee_read<uint16_t>(ent + 4), h6 = ee_read<uint16_t>(ent + 6);
            ent += 8;
            qmtc2(10, at);
            u.set_words(1, 0x3a800000u, 0, 0, 0);  // 1/1024
            u.set_words(11, h4, 0, 0, 0);
            u.mul(10, 10, 1, X);
            u.itof(11, 11, X, 1.0f / 4096.0f);
            uint32_t list = rd32(cls + 0x14) - (static_cast<uint32_t>(ee_read<uint8_t>(cls + 0xf)) << 4);
            u.mul(10, 10, 11, X);  // the scale: moby +0x2c / 1024 * h4 / 4096
            ram_vf(12, moby + 0xc0);
            ram_vf(13, moby + 0xd0);
            ram_vf(14, moby + 0xe0);
            at.set(out - end);
            ram_vf(15, moby + 0x10);
            if (static_cast<int32_t>(out - end) > 0) break;
            // the moby's header: h6, +0x84, +0x88, then the quadword of its kind (+0xbd)
            const uint32_t head = out;
            const uint32_t w84 = rd32(moby + 0x84), w88 = rd32(moby + 0x88);
            uint32_t entries = 0;
            {
                const Qw k = world_hand::ram_q((static_cast<uint32_t>(ee_read<uint8_t>(moby + 0xbd)) << 4) + kinds);
                at.lo = k.lo;
                at.hi = k.hi;
                t9 = SPR;
                wr32(out + 4, h6);
                wr32(out + 8, w84);
                wr32(out + 0xc, w88);
                world_hand::ram_setq(out + 0x10, k);
            }
            out += 0x20;
            uint32_t last = 0;
            for (;;) {
                const uint32_t joint2 = ee_read<uint8_t>(list);
                if (last != 0) break;
                last = ee_read<uint8_t>(list + 1);
                if (joint2 == 0) {
                    // one joint: its point (+0x10) through the joint's matrix (SPR + 0x40 * (+4)), then the moby's
                    const uint32_t mtx = (rd32(list + 4) << 6) + t9;
                    const uint32_t w8 = rd32(list + 8);
                    ram_vf(1, list + 0x10);
                    ++entries;
                    joint_point(1, mtx);
                    u.mulbc(1, 1, u.lane(10, 0), XYZW);
                    u.transform3(1, 12, 13, 14, 15, 1, one());
                    wr32(out, joint2);
                    wr32(out + 4, w8);
                    wr64(out + 8, 0);
                    world_hand::ram_store_vf(u, 1, out + 0x10);
                    out += 0x20;
                    list += 0x20;
                } else {
                    // two joints (+4 / +6): a segment, its ends pulled along it by the weights +8 / +0xc when negative
                    const uint32_t m1 = (static_cast<uint32_t>(ee_read<uint16_t>(list + 4)) << 6) + t9;
                    const uint32_t m2 = (static_cast<uint32_t>(ee_read<uint16_t>(list + 6)) << 6) + t9;
                    ram_vf(1, list + 0x10);
                    ram_vf(2, list + 0x20);
                    joint_point(1, m1);
                    joint_point(2, m2);
                    u.mulbc(1, 1, u.lane(10, 0), XYZW);
                    u.mulbc(2, 2, u.lane(10, 0), XYZW);
                    uint32_t w1 = rd32(list + 8), w2 = rd32(list + 0xc);
                    u.transform3(1, 12, 13, 14, 15, 1, one());
                    u.transform3(2, 12, 13, 14, 15, 2, one());
                    at.set(0u - w1);
                    if (static_cast<int32_t>(w1) < 0) {
                        pull(1, 2, at);
                        w1 = 0;
                    }
                    at.set(0u - w2);
                    if (static_cast<int32_t>(w2) < 0) {
                        pull(2, 1, at);
                        w2 = 0;
                    }
                    wr32(out, joint2);
                    wr32(out + 0xc, 0);
                    wr32(out + 4, w1);
                    wr32(out + 8, w2);
                    world_hand::ram_store_vf(u, 1, out + 0x10);
                    world_hand::ram_store_vf(u, 2, out + 0x20);
                    out += 0x30;
                    ++entries;
                    list += 0x30;
                }
            }
            wr32(head, entries);
            if (left <= 0) break;
        }
        world_hand::ram_setq(out, Qw{0, 0});
    }
    // orig: (helper) a point (vf `r`, xyz) through a joint's matrix in the scratchpad (rows at m, m + 0x10, m + 0x20, its
    // translation m + 0x30): vf4..vf7 loaded, the xyz lanes
    void joint_point(int r, uint32_t m) {
        using namespace vu0l;
        spr.load_vf(u, 4, m);
        spr.load_vf(u, 5, m + 0x10);
        spr.load_vf(u, 6, m + 0x20);
        spr.load_vf(u, 7, m + 0x30);
        u.transform3(r, 4, 5, 6, 7, r, one());
    }
    // orig: (helper) a segment's end `a` pulled towards `b` by the weight (at << 4, as 1/4096 fixed point): a += (a - b) w
    void pull(int a, int b, Gpr at) {
        using namespace vu0l;
        at.lo <<= 4;
        at.lo = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(at.lo))));
        qmtc2(4, at);
        u.sub(3, a, b, XYZ);
        u.itof(4, 4, X, 1.0f / 4096.0f);
        u.mulbc(3, 3, u.lane(4, 0), XYZ);
        u.add(a, a, 3, XYZ);
    }
};

template <class F>
bool run(F f) {
#if defined(RAC1_VU0_EXACT)
    (void)f;
    return false;
#else
    if (!vu0l::usable()) return false;
    Shadows s;
    s.u.load();
    f(s);
    s.u.store();
    return true;
#endif
}

}  // namespace

bool points() { return run([](Shadows& s) { s.points(); }); }
bool points_clip(uint32_t a0, uint32_t a1) { return run([&](Shadows& s) { s.points_clip(a0, a1); }); }
bool point_draw(uint32_t e) { return run([&](Shadows& s) { s.point_draw(e); }); }
bool segment_draw(uint32_t e) { return run([&](Shadows& s) { s.segment_draw(e); }); }
bool packets(uint32_t layer, uint32_t rgba) { return run([&](Shadows& s) { s.packets(layer, rgba); }); }
bool quads(uint32_t pts, uint32_t count, uint32_t clipped) {
    if ((pts >> 14) != (SPR >> 14)) return false;  // (the scratchpad's points only, as the draw passes them)
    return run([&](Shadows& s) { s.quads(pts, count, clipped); });
}

}  // namespace rac1::shadow_hand
