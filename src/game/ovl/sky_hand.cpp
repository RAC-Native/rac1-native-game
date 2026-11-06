// The sky's routines (sky_ee.cpp's, Veldin's addresses relocated) written by hand from the routines: the same arithmetic, the
// same memory read and written in the same order, the VU0 registers as a local copy (vu0_local.h) written back at the end; as in
// the transliteration (vu0_nf.h), the ops make no flags. Each returns what its transliteration returns ($v0, $f0), which the
// callers read. The transliterations (sky_ee.cpp's *_ref_tr, generated) stay the reference: RAC1_SKY_FAST (sky_fast.cpp) picks
// one or checks one against the other.
//  - 0x28c14c: a sky part's vertices to the screen: per vertex (8 bytes: three s16 and a byte at +6) the point through the view
//    matrix (0x166d80.., times the sky's own 0x1bd7e0..), the perspective divide, the viewport (0x16cbd0 scaled by 1/4, 0x16cbe0),
//    as 12.4 fixed point with the vertex's byte at +0xc and its clip bits (0x2f of the VCLIP) at +0xf: 16 bytes each to `out`.
//    Returns the clip bits common to all the vertices (0x3f to start): not 0, the part is off screen.
//  - 0x28c3c0: a sky part's triangles (4 bytes each: three vertex indices, a texture) as GS packets into the frame's buffer
//    (0x1611c0): those not outside one plane (the AND of their vertices' clip bits), per triangle its texture's TEX0 when it changes
//    (made and its upload queued the first time a texture is used in the frame), then per vertex ST (the u16 pair of `uvs` / 4096),
//    RGBAQ (grey, the vertex's byte as alpha) and XYZF2 (the projected vertex); the DMA and GIF tags in front. Returns the GIF
//    tag's loop count (1 when nothing was drawn, the tags then not written).
//  - 0x28c298: the same for a part without a texture: per vertex RGBAQ (its colour word of `colours`) and XYZF2.
//  - 0x28bd58: the sky's sprites (the list at the sky's +0x1c, its count +8): each in the view (its size against the view's slopes,
//    a texture given) as a GS sprite in batches of 30 (vf17 / vf18 the batch's tags): its position, its colour, its size on the
//    screen turned by its angle (the VU0 programs 0x190 / 0x192: cosine and sine; the copy written back and read again around
//    them), its texture's TEX0 (made and its upload queued the first time); its seen flag (+1) kept. Returns $v0 as left.
//  - 0x28c680: the sky's spheres (0x20 bytes each: the centre, w the radius) against the view's planes (0x16ccc0.., 0x16cd00..,
//    0x16ccf0): a byte each, 1 when the sphere may be seen. Returns 0.
// orig: (host)
#include <game/ovl/sky_hand.h>

#include <game/ovl/ovl.h>
#include <game/ovl/world_cull_hand.h>

#include <runtime/vu/vu0.h>

#include <algorithm>

namespace rac1::sky_hand {

namespace {

using world_hand::Qw;

struct Sky {
    vu0l::Vu u;

    void load(int r, uint32_t a) { world_hand::ram_load_vf(u, r, a); }
    float one() const { return u.lane(0, 3); }
    // orig: (helper) a vertex's three s16 (PPACW of the sign-extended halfwords) as the integer lanes of vf11, w 0
    void vertex(uint32_t v) {
        const auto h = [](uint32_t a) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(ee_read<uint16_t>(a)))); };
        u.set_words(11, h(v), h(v + 2), h(v + 4), 0);
    }

    uint32_t project(uint32_t v, uint32_t out, int32_t n) {
        using namespace vu0l;
        // the sky's matrix (vf1..vf4) taken through the view's (vf11..vf14)
        load(11, OV(0x00166d80u));
        load(12, OV(0x00166d90u));
        load(13, OV(0x00166da0u));
        load(14, OV(0x00166db0u));
        load(1, OV(0x001bd7e0u));
        load(2, OV(0x001bd7f0u));
        load(3, OV(0x001bd800u));
        load(4, OV(0x001bd810u));
        for (int r = 1; r <= 4; ++r) u.transform(r, 11, 12, 13, 14, r, u.lane(r, 3));
        u.set_words(5, 0x3e800000u, 0, 0, 0);  // 1/4
        load(26, OV(0x0016cbd0u));
        load(27, OV(0x0016cbe0u));
        u.mulbc(26, 26, u.lane(5, 0), XY);
        u.mulbc(15, 0, u.lane(0, 0), XYZW);
        vertex(v);
        u.itof(11, 11, XYZ, 1.0f);
        uint32_t common = 0x3f;
        u.transform(10, 1, 2, 3, 4, 11, one());
        // (the next vertex read and transformed while this one is finished: the routine's software pipeline)
        do {
            u.clipw(10);
            v += 8;
            u.div(one(), u.lane(10, 3));
            --n;
            const uint32_t b = ee_read<uint8_t>(v - 2);
            vertex(v);
            u.mulbc(10, 10, u.q, XYZ);
            u.itof(11, 11, XYZ, 1.0f);
            u.mulav(10, 26, XYZW);
            u.maddbc(14, 27, one(), XYZW);
            const uint32_t clip = u.clip & 0x2fu;
            u.ftoi(15, 14, XYZ, 16.0f);
            u.transform(10, 1, 2, 3, 4, 11, one());
            world_hand::ram_store_vf(u, 15, out);
            ee_write<uint8_t>(out + 0xc, static_cast<uint8_t>(b));
            common &= clip;
            ee_write<uint8_t>(out + 0xf, static_cast<uint8_t>(clip));
            out += 0x10;
        } while (n != 0);
        return common;
    }

    // 0x28c3c0: the visible triangles as GS packets
    uint32_t triangles(int32_t n, uint32_t tri, uint32_t uvs, uint32_t verts) {
        const auto sx = [](uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); };
        const auto s16 = [](uint32_t a) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(ee_read<uint16_t>(a)))); };
        uint32_t tex = 0xffff;  // the texture set last (none)
        const uint32_t head = ee_read<uint32_t>(OV(0x001611c0u));
        uint32_t out = head + 0x20;
        vu0l::Vf uv{};
        bool uv_set = false;
        do {
            --n;
            const uint32_t t = tri;
            const uint32_t clip = ee_read<uint8_t>((static_cast<uint32_t>(ee_read<uint8_t>(t)) << 4) + verts + 0xf) &
                                  ee_read<uint8_t>((static_cast<uint32_t>(ee_read<uint8_t>(t + 1)) << 4) + verts + 0xf) &
                                  ee_read<uint8_t>((static_cast<uint32_t>(ee_read<uint8_t>(t + 2)) << 4) + verts + 0xf);
            const uint32_t want = ee_read<uint8_t>(t + 3);
            tri += 4;
            if (clip != 0) continue;  // (all three vertices outside one plane)
            if (tex != want) {
                // the texture: its TEX0 (made and its upload queued the first time it is used in the frame), then the register
                const uint64_t tex0 = texture(want, sx, s16);
                ee_write<uint64_t>(out, tex0);
                ee_write<uint64_t>(out + 8, 6);
                out += 0x10;
                tex = want;
            }
            for (uint32_t k = 0; k < 3; ++k) {
                const uint32_t v = ee_read<uint8_t>(t + k);
                // ST: the vertex's two u16 texture coordinates (PEXTLH of the sign-extended word) / 4096
                const uint32_t w = ee_read<uint32_t>((v << 2) + uvs);
                const uint32_t hi = (w & 0x80000000u) != 0 ? 0xffffu : 0u;
                const uint32_t lanes[4] = {w & 0xffffu, w >> 16, hi, hi};
                for (int i = 0; i < 4; ++i) uv.v[i] = u2f(lanes[i]);
                for (int i = 0; i < 2; ++i) uv.v[i] = vu0m::itof(uv.v[i], 1.0f / 4096.0f);
                uv_set = true;
                ee_write<uint64_t>(out, f2u(uv.v[0]) | static_cast<uint64_t>(f2u(uv.v[1])) << 32);
                ee_write<uint64_t>(out + 8, 2);
                // XYZF2: x | y << 16 of the projected vertex, its z; RGBAQ: grey, the vertex's byte as alpha, Q 1
                const uint32_t p = (v << 4) + verts;
                const uint32_t xy = (ee_read<uint32_t>(p) & 0xffffu) | (ee_read<uint32_t>(p + 4) & 0xffffu) << 16;
                ee_write<uint32_t>(out + 0x20, xy);
                ee_write<uint32_t>(out + 0x24, 0);
                ee_write<uint64_t>(out + 0x28, 5);
                ee_write<uint32_t>(out + 0x10, static_cast<uint32_t>(ee_read<uint8_t>(p + 0xc)) << 24 | 0x808080u);
                ee_write<uint32_t>(out + 0x14, 0x3f800000u);
                ee_write<uint64_t>(out + 0x18, 1);
                out += 0x30;
            }
        } while (n != 0);
        if (uv_set) {
            VUState& s = vu0::global().st;
            s.vf[10] = uv;
        }
        return close_packet(head, out);
    }
    // 0x28c298: the visible triangles without a texture (Gouraud: a colour per vertex) as GS packets
    uint32_t colour_triangles(int32_t n, uint32_t tri, uint32_t colours, uint32_t verts) {
        const uint32_t head = ee_read<uint32_t>(OV(0x001611c0u));
        uint32_t out = head + 0x20;
        do {
            --n;
            const uint32_t t = tri;
            const uint32_t clip = ee_read<uint8_t>((static_cast<uint32_t>(ee_read<uint8_t>(t)) << 4) + verts + 0xf) &
                                  ee_read<uint8_t>((static_cast<uint32_t>(ee_read<uint8_t>(t + 1)) << 4) + verts + 0xf) &
                                  ee_read<uint8_t>((static_cast<uint32_t>(ee_read<uint8_t>(t + 2)) << 4) + verts + 0xf);
            tri += 4;
            if (clip != 0) continue;
            for (uint32_t k = 0; k < 3; ++k) {
                const uint32_t v = ee_read<uint8_t>(t + k);
                // RGBAQ: the vertex's colour word (sign-extended, as LW leaves it); XYZF2: the projected vertex
                const uint32_t c = ee_read<uint32_t>((v << 2) + colours);
                ee_write<uint64_t>(out, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(c))));
                ee_write<uint64_t>(out + 8, 1);
                const uint32_t p = (v << 4) + verts;
                ee_write<uint32_t>(out + 0x10, (ee_read<uint32_t>(p) & 0xffffu) | (ee_read<uint32_t>(p + 4) & 0xffffu) << 16);
                ee_write<uint32_t>(out + 0x14, 0);
                ee_write<uint64_t>(out + 0x18, 5);
                out += 0x20;
            }
        } while (n != 0);
        return close_packet(head, out);
    }
    // orig: (helper) the two routines' end: the DMA and GIF tags in front (qwc + 1 quadwords; a PACKED tag of qwc loops of one
    // register list), the buffer's pointer moved; nothing when nothing was drawn. Returns $v0: qwc (1 when nothing was drawn).
    static uint32_t close_packet(uint32_t head, uint32_t out) {
        const uint32_t qwc = ((out - head) >> 4) - 2;
        if (qwc == 0) return 1;
        ee_write<uint32_t>(head, 0x10000000u | (qwc + 1));
        ee_write<uint32_t>(head + 4, 0);
        ee_write<uint32_t>(head + 8, 0);
        ee_write<uint32_t>(head + 0xc, 0x50000000u | (qwc + 1));
        ee_write<uint32_t>(head + 0x10, qwc | 0x8000u);
        ee_write<uint32_t>(head + 0x14, 0x10000000u);
        ee_write<uint64_t>(head + 0x18, 0xe);
        ee_write<uint32_t>(OV(0x001611c0u), out);
        return qwc;
    }
    // orig: (helper) a sky texture's TEX0 (16 bytes per texture at *(*(0x16051c) + 0x10): the TEX0 word, the image's and the CLUT's
    // offsets +8 / +0xa, its log2 width / height +0xc / +0xe), made and its upload queued (0x16cd80, count 0x15f418; the texture
    // memory's next block 0x15ee74) the first time it is used
    template <class Sx, class S16>
    uint64_t texture(uint32_t idx, Sx sx, S16 s16) {
        const uint32_t e = (idx << 4) + ee_read<uint32_t>(ee_read<uint32_t>(OV(0x0016051cu)) + 0x10);
        uint64_t tex0 = ee_read<uint64_t>(e);
        if (tex0 != 0) return tex0;
        uint32_t next = ee_read<uint32_t>(0x0015ee74u);
        const uint32_t cbp = next >> 8;  // the CLUT's block
        next += 0x400;
        const uint32_t tbp = next >> 8;  // the image's
        const uint32_t lw = s16(e + 0xc), lh = s16(e + 0xe);
        next += 1u << ((lw + lh) & 31);
        int32_t bw = static_cast<int32_t>(lw) - 6;
        ee_write<uint32_t>(0x0015ee74u, next);
        if (bw < 0) bw = 0;
        // TEX0: TBP0 | TBW << 14 | PSM 0x13 << 20 | TW << 26 | TH << 30 | TCC << 34 | CBP << 37 | CLD 4 << 61 (64-bit sums, as the
        // routine adds them)
        const uint64_t t2 = sx(1u << (static_cast<uint32_t>(bw) & 31)) << 14;
        uint64_t t1 = sx(static_cast<uint32_t>(t2) + tbp);
        t1 += uint64_t{0x13} << 20;
        t1 += sx(lw) << 26;
        t1 += sx(lh) << 30;
        t1 += uint64_t{1} << 34;
        t1 += sx(cbp) << 37;
        t1 += uint64_t{4} << 61;
        tex0 = t1;
        ee_write<uint64_t>(e, tex0);
        // the upload: image and CLUT addresses, their blocks, the sizes
        const uint32_t n = ee_read<uint32_t>(OV(0x0015f418u));
        ee_write<uint32_t>(OV(0x0015f418u), n + 1);
        const uint32_t q = (n << 4) + OV(0x0016cd80u);
        ee_write<uint32_t>(q, (s16(e + 0xa) << 4) + ee_read<uint32_t>(ee_read<uint32_t>(OV(0x0016051cu)) + 0x14));
        ee_write<uint16_t>(q + 4, 0);
        ee_write<uint16_t>(q + 6, static_cast<uint16_t>(cbp));
        ee_write<uint32_t>(q + 8, (s16(e + 8) << 4) + ee_read<uint32_t>(ee_read<uint32_t>(OV(0x0016051cu)) + 0x14));
        ee_write<uint8_t>(q + 0xc, static_cast<uint8_t>(s16(e + 0xc)));
        ee_write<uint8_t>(q + 0xd, static_cast<uint8_t>(s16(e + 0xe)));
        ee_write<uint16_t>(q + 0xe, static_cast<uint16_t>(tbp));
        return tex0;
    }

    // --- 0x28bd58: the sky's sprites
    // the GPR model where a QMTC2 copies a register whole (a 32-bit value sign-extended into the low doubleword keeps the high one)
    struct Gpr {
        uint64_t lo = 0, hi = 0;
        void set(uint32_t v) { lo = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
    };
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
    // a VU0 micro program run on the registers (the copy written back first, read again after)
    void program(uint32_t at) {
        u.store();
        vu0::call(at);
        vu0::drain();
        u.load();
    }
    // the batch's GIF tag (vf17) with its size, the closing quadword (vf18)
    void close_batch(uint32_t head, uint32_t& out, uint32_t count4, uint64_t& v0) {
        world_hand::ram_store_vf(u, 18, out);
        const uint32_t qwc = (out - head) >> 4;
        out += 0x10;
        world_hand::ram_store_vf(u, 17, head);
        ee_write<uint16_t>(head, static_cast<uint16_t>(qwc));
        ee_write<uint8_t>(head + 0xe, static_cast<uint8_t>(qwc - 1));
        ee_write<uint32_t>(head + 0x3c, count4);
        v0 = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(qwc - 1)));
    }
    uint32_t sprites() {
        using namespace vu0l;
        const uint32_t sky = ee_read<uint32_t>(OV(0x0016051cu));
        uint64_t v0 = static_cast<uint64_t>(OV(0x00166c80u));  // ($v0 as the routine leaves it: the caller's set32 takes it)
        uint32_t entry = ee_read<uint32_t>(sky + 0x1c);
        int32_t left = static_cast<int32_t>(ee_read<uint16_t>(sky + 8));
        const uint32_t images = ee_read<uint32_t>(sky + 0x14), textures = ee_read<uint32_t>(sky + 0x10);
        if (left == 0) return static_cast<uint32_t>(v0);
        uint32_t out = ee_read<uint32_t>(OV(0x001611c0u));
        load(25, OV(0x0016cc40u));
        load(26, OV(0x0016caf0u));
        load(27, OV(0x0016cc20u));
        entry -= 0x20;
        u.div(u.lane(25, 0), u.lane(26, 0));
        load(29, OV(0x00166c80u));
        load(30, OV(0x00166c90u));
        load(31, OV(0x00166ca0u));
        load(17, OV(0x001607a0u));
        load(18, OV(0x001607b0u));
        load(19, OV(0x001607c0u));
        load(20, OV(0x001604e0u));
        u.addbc(25, 0, u.q, X);
        // the packet's head: a REF DMA tag to the boot data's block (0x101080, its size the halfword at 0x101070), two quadwords of
        // the frame (0x160780 / 0x160790) about the view's matrix scaled by 1024 (0x166cc0..) and the viewport (0x16cc50 / 0x16cbe0)
        world_hand::ram_setq(out, Qw{0, 0});
        const uint32_t size = ee_read<uint32_t>(0x00101070u);
        ee_write<uint32_t>(out + 4, 0x00101080u);
        ee_write<uint32_t>(out, 0x30000000u);
        ee_write<uint16_t>(out, static_cast<uint16_t>(size));
        out += 0x10;
        Gpr at;
        {
            const Qw a = world_hand::ram_q(OV(0x00160780u)), b = world_hand::ram_q(OV(0x00160790u));
            at.lo = a.lo, at.hi = a.hi;
            world_hand::ram_setq(out, a);
            world_hand::ram_setq(out + 0x70, b);
            const Qw c = world_hand::ram_q(OV(0x0016cc50u)), d = world_hand::ram_q(OV(0x0016cbe0u));
            world_hand::ram_setq(out + 0x50, c);
            world_hand::ram_setq(out + 0x60, d);
            Gpr k;
            k.hi = c.hi;
            k.set(0x44800000u);  // 1024
            v0 = k.lo;
            qmtc2(5, k);
        }
        for (int r = 1; r <= 4; ++r) load(r, OV(0x00166cc0u) + 0x10 * static_cast<uint32_t>(r - 1));
        for (int r = 1; r <= 3; ++r) u.mulbc(r, r, u.lane(5, 0), XYZW);
        for (int r = 1; r <= 4; ++r) world_hand::ram_store_vf(u, r, out + 0x10 * static_cast<uint32_t>(r));
        out += 0x80;
        uint32_t count4 = 0, head = out;
        out += 0x10;
        // per sprite (0x20 bytes: +1 seen, +2 texture (negative: none), +3 a byte, +4 RGBA, +8 an angle, +0x10 position, +0x1c size)
        for (;;) {
            at.set(ee_read<uint32_t>(entry + 0x3c));
            if (left <= 0) break;
            load(1, entry + 0x30);
            entry += 0x20;
            qmtc2(10, at);
            --left;
            u.rotate(2, 29, 30, 31, 1, XYZ);  // the position in the view
            const uint32_t seen = ee_read<uint8_t>(entry + 1);
            const int32_t tex = static_cast<int8_t>(ee_read<uint8_t>(entry + 2));
            const uint32_t rgba = ee_read<uint32_t>(entry + 4);
            const uint64_t b3 = ee_read<uint8_t>(entry + 3);
            bool show = false;
            uint64_t tex0 = 0;
            uint32_t te = 0;
            Gpr angle;
            if (tex >= 0) {
                // in the view: |x|, |y| (less the sprite's size) against z times the view's slopes
                u.mulbc(4, 27, u.lane(10, 0), XY);
                angle.set(ee_read<uint32_t>(entry + 8));
                u.mulbc(10, 10, u.lane(25, 3), X);
                u.abs(3, 2, XY);
                u.mulbc(5, 26, u.lane(2, 2), XY);
                u.sub(3, 3, 4, XY);
                u.div(u.lane(10, 0), u.lane(2, 2));
                te = (static_cast<uint32_t>(tex) << 4) + textures;
                u.sub(3, 5, 3, XY);
                tex0 = ee_read<uint64_t>(te);
                u.addbc(11, 0, u.q, XYZW);  // the sprite's size on the screen
                show = !u.negative(3, 1) && !u.negative(3, 0);
            }
            if (!show) {
                if (seen != 0) ee_write<uint8_t>(entry + 1, 0);
                continue;
            }
            // its position, its rotation (the size times the cosine / sine of +8: the programs 0x190 / 0x192), colour, texture
            world_hand::ram_store_vf(u, 1, out);
            qmtc2(1, angle);
            u.store();
            vu0::call(0x190);
            if (seen == 0) ee_write<uint8_t>(entry + 1, 1);
            vu0::drain();
            u.load();
            at = qmfc2(1);
            u.mulbc(11, 11, u.lane(1, 0), X);
            qmtc2(1, angle);
            program(0x192);
            at = qmfc2(1);
            u.mulbc(11, 11, u.lane(1, 0), Y);
            ee_write<uint32_t>(out + 0xc, 0);
            world_hand::ram_setq(out + 0x10, Qw{(rgba & 0xffu) | static_cast<uint64_t>((rgba >> 8) & 0xffu) << 32,
                                                ((rgba >> 16) & 0xffu) | static_cast<uint64_t>(rgba >> 24) << 32});
            world_hand::ram_store_vf(u, 11, out + 0x20);
            ee_write<uint64_t>(out + 0x28, b3);
            if (static_cast<int64_t>(tex0) >= 0) tex0 = make_texture(te, images, at, v0);
            ee_write<uint64_t>(out + 0x30, tex0);
            count4 += 4;
            ee_write<uint64_t>(out + 0x38, 0);
            out += 0x40;
            if (static_cast<int32_t>(count4 - 0x78) < 0) continue;
            // 30 sprites: the batch closed, the next one started
            close_batch(head, out, count4, v0);
            count4 = 0;
            head = out;
            out += 0x10;
        }
        out -= 0x10;
        if (count4 != 0) {
            out += 0x10;
            close_batch(head, out, count4, v0);
        }
        world_hand::ram_store_vf(u, 19, out);
        out += 0x10;
        ee_write<uint32_t>(OV(0x001611c0u), out);
        return static_cast<uint32_t>(v0);
    }
    // orig: (helper) a sprite texture's TEX0, made and its upload queued (0x16cd80, count 0x15f418) the first time it is used in the
    // frame: the image and CLUT blocks from 0x15ee74, the image's and the CLUT's addresses (+0xa / +8, from `images`), its log2 sizes
    // (+0xc / +0xe) into vf20's TEX0 template. `at` / `v0` as the routine leaves them.
    uint64_t make_texture(uint32_t te, uint32_t images, Gpr& at, uint64_t& v0) {
        const uint32_t n = ee_read<uint32_t>(OV(0x0015f418u));
        uint32_t next = ee_read<uint32_t>(0x0015ee74u);
        ee_write<uint32_t>(OV(0x0015f418u), n + 1);
        const uint32_t q = (n << 4) + OV(0x0016cd80u);
        const uint32_t cbp = next >> 8;
        next += 0x400;
        ee_write<uint16_t>(q + 6, static_cast<uint16_t>(cbp));
        const uint32_t tbp = next >> 8;
        ee_write<uint16_t>(q + 0xe, static_cast<uint16_t>(tbp));
        const uint32_t lw = ee_read<uint8_t>(te + 0xc), lh = ee_read<uint8_t>(te + 0xe);
        ee_write<uint8_t>(q + 0xc, static_cast<uint8_t>(lw));
        const uint32_t blocks = (1u << (lw & 31)) << (lh & 31);
        ee_write<uint8_t>(q + 0xd, static_cast<uint8_t>(lh));
        next += blocks;
        ee_write<uint16_t>(q + 4, 0);
        ee_write<uint32_t>(0x0015ee74u, next);
        ee_write<uint32_t>(q, (static_cast<uint32_t>(ee_read<uint16_t>(te + 0xa)) << 4) + images);
        ee_write<uint32_t>(q + 8, (static_cast<uint32_t>(ee_read<uint16_t>(te + 8)) << 4) + images);
        // TEX0: vf20's template | TBP0 | TBW << 14 | TW << 26 | TH << 30 | CBP << 37 (PMAXW: TBW = max(log2 width - 6, 0); the high
        // words of `at` maxed with 0 too)
        const Gpr tmpl = qmfc2(20);
        at.set(lw - 6);
        at.lo = static_cast<uint64_t>(std::max<int32_t>(static_cast<int32_t>(at.lo), 0)) |
                static_cast<uint64_t>(static_cast<uint32_t>(std::max<int32_t>(static_cast<int32_t>(at.lo >> 32), 0))) << 32;
        at.hi = static_cast<uint64_t>(static_cast<uint32_t>(std::max<int32_t>(static_cast<int32_t>(at.hi), 0))) |
                static_cast<uint64_t>(static_cast<uint32_t>(std::max<int32_t>(static_cast<int32_t>(at.hi >> 32), 0))) << 32;
        const uint64_t tbw = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(1u << (static_cast<uint32_t>(at.lo) & 31)))) << 14;
        const uint64_t tex0 = tmpl.lo | tbp | tbw | static_cast<uint64_t>(lw) << 26 | static_cast<uint64_t>(lh) << 30 |
                              static_cast<uint64_t>(cbp) << 37;
        v0 = static_cast<uint64_t>(cbp) << 37;
        ee_write<uint64_t>(te, tex0);
        return tex0;
    }

    void spheres(uint32_t s, int32_t n, uint32_t out) {
        using namespace vu0l;
        load(24, OV(0x0016ccf0u));
        load(6, OV(0x0016ccc0u));
        load(7, OV(0x0016ccd0u));
        load(8, OV(0x0016cce0u));
        load(20, OV(0x0016cd00u));
        load(21, OV(0x0016cd10u));
        load(22, OV(0x0016cd20u));
        load(23, OV(0x0016cd30u));
        load(11, OV(0x001bd7e0u));
        load(12, OV(0x001bd7f0u));
        load(13, OV(0x001bd800u));
        load(14, OV(0x001bd810u));
        // the cone's rows (vf6..vf8) taken through the sky's matrix
        u.rotate(25, 6, 7, 8, 11, XYZ);
        u.rotate(26, 6, 7, 8, 12, XYZ);
        u.rotate(27, 6, 7, 8, 13, XYZ);
        do {
            load(1, s);
            u.mulbc(1, 1, u.lane(24, 3), XYZW);
            u.move(2, 1, XYZ);
            --n;
            u.addbc(3, 0, u.lane(1, 3), X);
            s += 0x20;
            u.subbc(3, 0, u.lane(1, 3), Y);
            u.rotate(2, 25, 26, 27, 2, XYZ);
            ++out;
            u.mulbc(8, 22, u.lane(1, 3), XY);
            u.mulbc(5, 20, u.lane(2, 2), XY);
            u.abs(2, 2, XY);
            u.sub(6, 2, 8, XY);
            u.sub(7, 5, 6, XY);
            // seen unless x or y of vf7 is negative
            ee_write<uint8_t>(out - 1, (u.negative(7, 1) || u.negative(7, 0)) ? 0 : 1);
        } while (n != 0);
    }
};

template <class F>
bool run(F f) {
#if defined(RAC1_VU0_EXACT)
    (void)f;
    return false;
#else
    if (!vu0l::usable()) return false;
    Sky s;
    s.u.load();
    f(s);
    s.u.store();
    return true;
#endif
}

}  // namespace

// orig: 0x0028c14c
bool project(uint32_t v, uint32_t out, uint32_t n, uint32_t& ret) {
    return run([&](Sky& s) { ret = s.project(v, out, static_cast<int32_t>(n)); });
}
// orig: 0x0028c3c0 (its one VU0 op, a VITOF into vf10, made on a local register and vf10 written at the end)
bool triangles(uint32_t n, uint32_t tri, uint32_t uvs, uint32_t verts, uint32_t& ret) {
#if defined(RAC1_VU0_EXACT)
    (void)n, (void)tri, (void)uvs, (void)verts, (void)ret;
    return false;
#else
    if (!vu0l::usable()) return false;
    Sky s;
    ret = s.triangles(static_cast<int32_t>(n), tri, uvs, verts);
    return true;
#endif
}
// orig: 0x0028bd58
bool sprites(uint32_t& ret) {
    return run([&](Sky& s) { ret = s.sprites(); });
}
// orig: 0x0028c298 (no VU0 op)
uint32_t colour_triangles(uint32_t n, uint32_t tri, uint32_t colours, uint32_t verts) {
    Sky s;
    return s.colour_triangles(static_cast<int32_t>(n), tri, colours, verts);
}
// orig: 0x0028c680
bool spheres(uint32_t s, uint32_t n, uint32_t out) {
    return run([&](Sky& k) { k.spheres(s, static_cast<int32_t>(n), out); });
}

}  // namespace rac1::sky_hand
