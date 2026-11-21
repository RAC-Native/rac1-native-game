// Oltanis (level 14): draw-list callbacks of the level's own classes, all camera-facing textured quads through quad_draw.
// - 0x2d71f0, class 211's (the rail drones): the glow of its two lights (+0xe0, +0x200), a large white quad and a small
//   dark one.
// - 0x2df458, class 386's: two scrolling ribbons along its point lists (0x2df510).
// - 0x2eb810, class 610's: its 48 sparks (+0x20 + 16 i, the size in the w lane), each lit while its timer +0x320 + 2 i is
//   running, faded by +0x3e0 + 2 i.
// - 0x2edc18, class 684's: the level's lightning bolt and its three branches, ribbons of quads along the level's points.
// Every local the callees read or write lives in an EE frame of the original's size at the original's offsets.
// VU0: none of them has a VU0 macro instruction; their vector work is in the host helpers the generated code already
// called (vec3_*, mat4_transform, quad_draw), so the VU0 registers are left as they were.
// Level 14 only: `orig: L14:`, Oltanis data L14_ADDR, base ELF bare. Registered in oltanis_mobys.cpp.
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

constexpr uint64_t kTex1 = (0xff90ull << 32) | 0x260ull;  // TEX1: MMAG / MMIN linear, L 0, K -7
constexpr uint64_t kAlpha = (0x80ull << 32) | 0x48ull;    // ALPHA: (Cs - 0) * As + Cd, FIX 0x80

// quad_draw's descriptor (quad_draw.h): the GS words at +0x70 (CLAMP, TEX0, TEX1, ALPHA)
void quad_gs(uint32_t q, uint64_t clamp, uint64_t tex0) {
    sd(q + 0x70, clamp);
    sd(q + 0x78, tex0);
    sd(q + 0x80, kTex1);
    sd(q + 0x88, kAlpha);
}
void quad_rgba(uint32_t q, uint32_t rgba) {
    for (uint32_t k = 0; k < 4; ++k) sw(q + 0x40 + k * 4, rgba);
}
// a frame at `at` facing along `dir` (normalised), its side the cross with `axis` set to `side_len`, its up the cross of
// the two: rows at mat, +0x10, +0x20 (row 3, the place, at mat + 0x30 is the caller's)
void face(uint32_t mat, uint32_t axis, float side_len) {
    vec3_set_length(V(mat), V(mat), 1.0f);
    vec3_cross_ba(V(mat + 0x10), V(mat), V(axis));
    vec3_set_length(V(mat + 0x10), V(mat + 0x10), side_len);
    vec3_cross_ba(V(mat + 0x20), V(mat + 0x10), V(mat));
}
// the quad's corners: the template `corners` (4 points, 16 bytes each) scaled by `size`, through the frame `mat`
void lay_corners(uint32_t q, uint32_t corners, uint32_t mat, float size) {
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t c = q + k * 0x10;
        vec3_scale(V(c), V(corners + k * 0x10), size);
        mat4_transform(V(c), V(c), M(mat));
    }
}

// A ribbon along `n` points (16 bytes each, at `points`, in the moby's own frame): per segment two quads facing the camera,
// one 0.2 wide each way in grey (texture 0x2d, its s scrolled by the moby's +0x1ec), one twice as wide in red (texture
// 0x2c), with the alphas `alpha_a` / `alpha_b`; the ribbon whose second alpha is 0x10 fades out at both ends (the second
// point's near corners and the last but one's far corners clear). The width's curve is cubic_interp(-1, 0, 1, 0, i / n)
// times 0, so 0.2 all along: kept as the original computes it.
// orig: L14:0x002df510
void ribbon(uint32_t m, uint32_t points, uint32_t alpha_a, uint32_t alpha_b, int32_t n) {
    const Frame f(0x300);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t qa = sp, qb = sp + 0x90;             // the two quads; their corners 0 / 1 the near edge, 2 / 3 the far one
    const uint32_t half_a = sp + 0x120, half_b = sp + 0x130, heading = sp + 0x140;
    const uint32_t mat = sp + 0x150;                    // the segment's frame: along, towards the camera, across, its start
    const uint32_t rot = sp + 0x190;                    // the moby's matrix, its place in row 3
    const uint32_t p0 = sp + 0x1d0, p1 = sp + 0x1e0;     // the segment's ends
    const uint32_t cam = L14_ADDR(0x001674c0);
    const auto a_st = [&] {  // the grey quad's (s, t): s scrolled, (s, 0), (s, 1), (s + 1, 0), (s + 1, 1)
        swc1(qa + 0x54, 0.0f);
        swc1(qa + 0x50, fpu_add(lwc1(pv + 0x1ec), 0.0f));
        swc1(qa + 0x5c, 1.0f);
        swc1(qa + 0x58, fpu_add(lwc1(pv + 0x1ec), 0.0f));
        swc1(qa + 0x64, 0.0f);
        swc1(qa + 0x60, fpu_add(lwc1(pv + 0x1ec), 1.0f));
        swc1(qa + 0x6c, 1.0f);
        swc1(qa + 0x68, fpu_add(lwc1(pv + 0x1ec), 1.0f));
    };
    const auto segment_frame = [&] {
        vec3_sub(V(mat), V(p1), V(p0));
        vec3_set_length(V(mat), V(mat), 1.0f);
        vec3_sub(V(mat + 0x10), V(cam), V(p0));
        vec3_cross_ba(V(mat + 0x20), V(mat + 0x10), V(mat));
        vec3_set_length(V(mat + 0x20), V(mat + 0x20), 1.0f);
        vec3_cross_ba(V(mat + 0x10), V(mat), V(mat + 0x20));
        lq_sq(mat + 0x30, p0);
    };
    const auto edge = [&](uint32_t e) {  // a quad edge from its half width: +e the half width, +e + 0x10 its opposite
        vec3_scale(V(e + 0x10), V(e), -1.0f);
    };
    const auto place = [&](uint32_t e) {
        mat4_transform(V(e), V(e), M(mat));
        mat4_transform(V(e + 0x10), V(e + 0x10), M(mat));
    };
    quad_gs(qa, 0, effect_tex0(0x2d));
    quad_rgba(qa, (alpha_a << 24) | 0x7f7f7fu);
    quad_gs(qb, 0, effect_tex0(0x2c));
    quad_rgba(qb, (alpha_b << 24) | 0x7f4040u);
    swc1(qb + 0x50, 1.0f);
    sw(qb + 0x54, 0);
    swc1(qb + 0x58, 1.0f);
    swc1(qb + 0x5c, 1.0f);
    sw(qb + 0x60, 0);
    sw(qb + 0x64, 0);
    sw(qb + 0x68, 0);
    swc1(qb + 0x6c, 1.0f);
    a_st();
    for (uint32_t k = 0; k < 0x10; k += 4) {
        sw(half_a + k, 0);
        sw(half_b + k, 0);
    }
    swc1(half_a + 0xc, 1.0f);
    swc1(half_a + 8, u2f(0x3e4ccccdu));  // 0.2
    swc1(half_b + 8, u2f(0x3ecccccdu));  // 0.4
    swc1(half_b + 0xc, 1.0f);
    // a frame from the moby's heading (+0x48): overwritten below before it is used; kept, as its lanes' leftovers are the original's
    swc1(heading, cos_vu(lwc1(m + 0x48)));
    swc1(heading + 4, sin_vu(lwc1(m + 0x48)));
    swc1(heading + 8, 0.0f);
    lq_sq(mat, heading);
    vec3_sub(V(mat + 0x10), V(cam), V(m + 0x10));
    vec3_scale(V(mat + 0x10), V(mat + 0x10), 1.0f);
    vec3_cross_ba(V(mat + 0x20), V(mat + 0x10), V(mat));
    vec3_set_length(V(mat + 0x20), V(mat + 0x20), 1.0f);
    vec3_cross_ba(V(mat), V(mat + 0x20), V(mat + 0x10));
    lq_sq(mat + 0x30, m + 0x10);
    euler_to_mat4_vu(M(rot), V(m + 0x40));
    lq_sq(rot + 0x30, m + 0x10);
    // the first segment's near edges
    lq_sq(qa, half_a);
    edge(qa);
    lq_sq(qb, half_b);
    edge(qb);
    mat4_transform(V(p0), V(points), M(rot));
    mat4_transform(V(p1), V(points + 0x10), M(rot));
    segment_frame();
    place(qa);
    place(qb);
    for (int32_t i = 1; i < n; ++i) {
        const uint32_t pt = points + static_cast<uint32_t>(i) * 0x10;
        const uint32_t far_a = qa + 0x20, far_b = qb + 0x20;
        lq_sq(far_a, half_a);
        edge(far_a);
        lq_sq(far_b, half_b);
        edge(far_b);
        if (i == n - 1) {
            mat4_transform(V(mat + 0x30), V(pt), M(rot));  // the last point: the last segment's frame moved there
        } else {
            mat4_transform(V(p0), V(pt), M(rot));
            mat4_transform(V(p1), V(pt + 0x10), M(rot));
            segment_frame();
        }
        place(far_a);
        place(far_b);
        if (alpha_b == 0x10 && (i == 1 || i == n - 2)) {
            const uint32_t k = i == 1 ? 0 : 8;  // the near corners' colours, or the far ones'
            const uint32_t keep_a = lwu(qa + 0x40 + k), keep_b = lwu(qb + 0x40 + k);
            sw(qa + 0x44 + k, 0x00ffffffu);
            sw(qa + 0x40 + k, 0x00ffffffu);
            sw(qb + 0x44 + k, 0x007f4040u);
            sw(qb + 0x40 + k, 0x007f4040u);
            quad_draw(qa, 0);
            quad_draw(qb, 0);
            quad_rgba(qa, keep_a);
            quad_rgba(qb, keep_b);
        } else {
            quad_draw(qa, 0);
            quad_draw(qb, 0);
        }
        lq_sq(qa, far_a);  // the far edges become the next segment's near ones
        lq_sq(qa + 0x10, far_a + 0x10);
        lq_sq(qb, far_b);
        lq_sq(qb + 0x10, far_b + 0x10);
        const float t = fpu_div(int_to_float(i), int_to_float(n));
        const float w = fpu_add(fpu_mul(cubic_interp(-1.0f, 0.0f, 1.0f, 0.0f, t), 0.0f), u2f(0x3e4ccccdu));
        swc1(half_a + 8, w);
        swc1(half_b + 8, fpu_add(w, w));
        a_st();
    }
}

}  // namespace

// Class 386's draw: its texture scroll (+0x1ec, 0.2 a frame of the clock 0x15ed60, wrapped at -8) and its two ribbons
// (10 points at +0, 20 at +0xa0).
// orig: L14:0x002df458
void olt_cb_2df458_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    float s = fpu_sub(lwc1(pv + 0x1ec), fpu_mul(lwc1(0x0015ed60), u2f(0x3e4ccccdu)));
    if (s <= -8.0f) s = fpu_add(s, 8.0f);
    swc1(pv + 0x1ec, s);
    sub_002a0ba8(0x47, 0x51001);  // TEST
    ribbon(m, pv, 0x80, 0x40, 10);
    ribbon(m, pv + 0xa0, 0x30, 0x10, 20);
}

// Class 211's lights: each a quad of the corners L14:0x1de540 facing the camera at the light's place (the first +0xe0, w
// set to 1; the second +0x200 as it is), its side across the vector 0x13f5e0 (the hero's block); the first 0.4 large
// in white (0x80ffffff), the second 0.2 large in dark grey (0x202020ff). CLAMP 5 (region repeat), (s, t) (0, 0), (0, 1),
// (1, 0), (1, 1).
// orig: L14:0x002d71f0
void olt_cb_2d71f0_l14(uint32_t m) {
    const Frame f(0x180);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t quad = sp, mat = sp + 0x90, at = sp + 0xc0;
    lq_sq(at, pv + 0xe0);
    swc1(at + 0xc, 1.0f);
    vec3_sub(V(mat), V(L14_ADDR(0x001674c0)), V(at));
    face(mat, 0x0013f5e0u, -1.0f);
    quad_gs(quad, 5, effect_tex0(0xb));
    quad_rgba(quad, 0x80ffffffu);
    swc1(quad + 0x50, 0.0f);
    sw(quad + 0x54, 0);
    sw(quad + 0x58, 0);
    swc1(quad + 0x5c, 1.0f);
    swc1(quad + 0x60, 1.0f);
    sw(quad + 0x64, 0);
    swc1(quad + 0x68, 1.0f);
    swc1(quad + 0x6c, 1.0f);
    lay_corners(quad, L14_ADDR(0x001de540), mat, u2f(0x3ecccccdu));  // 0.4
    quad_draw(quad, 0);
    lq_sq(at, pv + 0x200);
    vec3_sub(V(mat), V(L14_ADDR(0x001674c0)), V(at));
    face(mat, 0x0013f5e0u, -1.0f);
    quad_rgba(quad, 0x202020ffu);
    lay_corners(quad, L14_ADDR(0x001de540), mat, u2f(0x3e4ccccdu));  // 0.2
    quad_draw(quad, 0);
}

// Class 610's sparks: for each of the 48 whose timer +0x320 + 2 i is not 0, a quad of the corners L14:0x1dfd40 scaled by
// its w lane, facing the camera (its side across z), in the colour L14:0x161c0c with the alpha +0x3e0 + 2 i.
// orig: L14:0x002eb810
void olt_cb_2eb810_l14(uint32_t m) {
    const Frame f(0x1a0);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    const uint32_t quad = sp, mat = sp + 0x90, axis = sp + 0xd0;
    quad_gs(quad, 0, effect_tex0(0x30));
    sub_002a0ba8(0x4a, 0);        // FBA
    sub_002a0ba8(0x47, 0x51001);  // TEST
    swc1(quad + 0x50, 1.0f);
    swc1(quad + 0x54, 1.0f);
    sw(quad + 0x58, 0);
    swc1(quad + 0x5c, 1.0f);
    swc1(quad + 0x60, 1.0f);
    sw(quad + 0x64, 0);
    sw(quad + 0x68, 0);
    sw(quad + 0x6c, 0);
    for (uint32_t i = 0; i < 48; ++i) {
        if (lh(pv + 0x320 + i * 2) == 0) continue;
        const uint32_t spark = pv + 0x20 + i * 0x10;
        quad_rgba(quad, lwu(L14_ADDR(0x00161c0c)) | (static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x3e0 + i * 2))) << 24));
        lq_sq(mat + 0x30, spark);
        sw(axis, 0);
        sw(axis + 4, 0);
        swc1(axis + 8, 1.0f);
        swc1(axis + 0xc, 1.0f);
        vec3_sub(V(mat), V(spark), V(L14_ADDR(0x001674c0)));
        face(mat, axis, 1.0f);
        lay_corners(quad, L14_ADDR(0x001dfd40), mat, lwc1(spark + 0xc));
        quad_draw(quad, 0);
    }
}

// Class 684's bolt (the level's one, its data in the level's globals; the moby is not read): 39 segments of two quads facing
// the camera from the bolt's end L14:0x161cd0 to its start L14:0x161cc0 through the points L14:0x1dfd80 (in the bolt's
// frame), grey (texture 0x2d, the colour L14:0x161c78) and red twice as wide (texture 0x2c, L14:0x161c7c, half the alpha),
// each point's alpha its timer (L14:0x1e0000 + 2 i) over the duration L14:0x161c70 (frame-scaled), less L14:0x161ce0, the
// width eased by cubic_interp from L14:0x161c84 to L14:0x161c80 (the red one's by L14:0x161c88); then three branches of 15
// segments from the points L14:0x161cf8 [b], stepping by the offsets L14:0x1e0050 + 0x100 b (x, z), their alphas from the
// timers L14:0x1e0350 + 0x20 b, three quarters as wide and narrowing by 0.95 a segment, drawn in the bolt's frame.
// orig: L14:0x002edc18
void olt_cb_2edc18_l14(uint32_t) {
    const Frame f(0x310);
    const uint32_t sp = f.sp;
    const uint32_t qa = sp, qb = sp + 0x90;            // the two quads; corners 0 / 1 the near edge, 2 / 3 the far one
    const uint32_t bolt = sp + 0x120;                  // the bolt's frame: along it, across, up, its end
    const uint32_t half_a = sp + 0x160, half_b = sp + 0x170;
    const uint32_t at = sp + 0x180;                    // the point the main bolt's segment reaches
    const uint32_t seg = sp + 0x1a0;                   // the segment's frame (its row 3, the place, at +0x30)
    const uint32_t local = sp + 0x1e0, prev = sp + 0x1f0, next = sp + 0x200;  // a branch's point (the bolt's frame), the last two placed
    const uint32_t cam = L14_ADDR(0x001674c0);
    const auto alpha = [](int32_t timer) {
        const float frac = fpu_div(int_to_float(timer), int_to_float(frame_scale_i(lw(L14_ADDR(0x00161c70)))));
        int32_t a = static_cast<int32_t>(static_cast<uint32_t>(float_to_int(fpu_mul(frac, 255.0f))) - lbu(L14_ADDR(0x00161ce0)));
        if (a < 0) a = 0;
        if (a > 255) a = 255;
        return static_cast<uint32_t>(a);
    };
    const auto colours = [&](uint32_t k, uint32_t a) {  // the near (k 0) or far (k 8) corners' colours
        sw(qa + 0x40 + k, lwu(L14_ADDR(0x00161c78)) | (a << 24));
        sw(qa + 0x44 + k, lwu(L14_ADDR(0x00161c78)) | (a << 24));
        sw(qb + 0x40 + k, lwu(L14_ADDR(0x00161c7c)) | ((a >> 1) << 24));
        sw(qb + 0x44 + k, lwu(L14_ADDR(0x00161c7c)) | ((a >> 1) << 24));
    };
    const auto edges = [&](uint32_t ea, uint32_t eb) {  // the quads' edges from the half widths: +0 the half width, +0x10 its opposite
        lq_sq(ea, half_a);
        vec3_scale(V(ea + 0x10), V(ea), -1.0f);
        lq_sq(eb, half_b);
        vec3_scale(V(eb + 0x10), V(eb), -1.0f);
    };
    const auto place = [&](uint32_t ea, uint32_t eb) {
        mat4_transform(V(ea), V(ea), M(seg));
        mat4_transform(V(ea + 0x10), V(ea + 0x10), M(seg));
        mat4_transform(V(eb), V(eb), M(seg));
        mat4_transform(V(eb + 0x10), V(eb + 0x10), M(seg));
    };
    const auto facing = [&] {  // the segment's frame at `at`: the bolt's direction, towards the camera, across
        lq_sq(seg, bolt);
        vec3_sub(V(seg + 0x10), V(cam), V(at));
        vec3_set_length(V(seg + 0x10), V(seg + 0x10), 1.0f);
        vec3_cross_ba(V(seg + 0x20), V(seg + 0x10), V(seg));
        lq_sq(seg + 0x30, at);
    };
    const auto draw = [&] {  // both quads, then the far edges and colours become the next segment's near ones
        quad_draw(qa, 0);
        quad_draw(qb, 0);
        sw(qa + 0x40, lwu(qa + 0x48));
        sw(qb + 0x40, lwu(qb + 0x48));
        sw(qa + 0x44, lwu(qa + 0x48));
        sw(qb + 0x44, lwu(qb + 0x48));
        lq_sq(qa, qa + 0x20);
        lq_sq(qa + 0x10, qa + 0x30);
        lq_sq(qb, qb + 0x20);
        lq_sq(qb + 0x10, qb + 0x30);
    };
    const auto width = [](float t) {  // the main bolt's width at t: L14:0x161c84 eased to L14:0x161c80
        const float c = cubic_interp(-1.0f, 0.0f, 1.0f, 0.0f, t);
        const float w0 = lwc1(L14_ADDR(0x00161c84));
        return fpu_add(w0, fpu_mul(fpu_sub(lwc1(L14_ADDR(0x00161c80)), w0), c));
    };
    const auto step_local = [&](uint32_t off) {
        swc1(local, fpu_add(lwc1(local), lwc1(off)));
        swc1(local + 8, fpu_add(lwc1(local + 8), lwc1(off + 8)));
    };

    // the grey quad (its colours, (s, t) (0, 0), (0, 1), (1, 0), (1, 1)); the red one a copy of it with its own texture and colours
    quad_gs(qa, 0, effect_tex0(0x2d));
    const uint32_t a_end = alpha(lh(L14_ADDR(0x001e0000)));
    sw(qa + 0x50, 0);
    sw(qa + 0x54, 0);
    sw(qa + 0x58, 0);
    swc1(qa + 0x5c, 1.0f);
    swc1(qa + 0x60, 1.0f);
    sw(qa + 0x40, lwu(L14_ADDR(0x00161c78)) | (a_end << 24));
    sw(qa + 0x44, lwu(L14_ADDR(0x00161c78)) | (a_end << 24));
    sw(qa + 0x64, 0);
    swc1(qa + 0x68, 1.0f);
    swc1(qa + 0x6c, 1.0f);
    mem_copy128(qb, qa, 0x90);
    sd(qb + 0x78, effect_tex0(0x2c));
    sw(qb + 0x40, lwu(L14_ADDR(0x00161c7c)) | ((a_end >> 1) << 24));
    sw(qb + 0x44, lwu(L14_ADDR(0x00161c7c)) | ((a_end >> 1) << 24));
    // the bolt's frame
    vec3_sub(V(bolt), V(L14_ADDR(0x00161cc0)), V(L14_ADDR(0x00161cd0)));
    vec3_set_length(V(bolt), V(bolt), 1.0f);
    sw(bolt + 0xc, 0);
    swc1(bolt + 0x20, 1.0f);
    sw(bolt + 0x24, 0);
    sw(bolt + 0x28, 0);
    sw(bolt + 0x2c, 0);
    vec3_cross_ba(V(bolt + 0x10), V(bolt), V(bolt + 0x20));
    vec3_set_length(V(bolt + 0x10), V(bolt + 0x10), 1.0f);
    sw(bolt + 0x1c, 0);
    vec3_cross_ba(V(bolt + 0x20), V(bolt + 0x10), V(bolt));
    sw(bolt + 0x2c, 0);
    lq_sq(bolt + 0x30, L14_ADDR(0x00161cd0));
    for (uint32_t k = 0; k < 0x10; k += 4) {
        sw(half_a + k, 0);
        sw(half_b + k, 0);
    }
    swc1(half_a + 8, lwc1(L14_ADDR(0x00161c84)));
    swc1(half_a + 0xc, 1.0f);
    swc1(half_b + 8, fpu_mul(lwc1(L14_ADDR(0x00161c84)), lwc1(L14_ADDR(0x00161c88))));
    swc1(half_b + 0xc, 1.0f);
    // the main bolt: its first near edges at its point 1, then segments to the points 2 .. 38 and the start
    edges(qa, qb);
    mat4_transform(V(at), V(L14_ADDR(0x001dfd90)), M(bolt));
    mat4_transform(V(at + 0x10), V(L14_ADDR(0x001dfda0)), M(bolt));  // (point 2: not read back)
    lq_sq(seg, bolt);
    vec3_sub(V(seg + 0x10), V(cam), V(at));
    vec3_set_length(V(seg + 0x10), V(seg + 0x10), 1.0f);
    vec3_cross_ba(V(seg + 0x20), V(seg + 0x10), V(seg));
    lq_sq(seg + 0x30, at);
    place(qa, qb);
    for (int32_t i = 1; i < 40; ++i) {
        edges(qa + 0x20, qb + 0x20);
        if (i == 39)
            lq_sq(at, L14_ADDR(0x00161cc0));
        else
            mat4_transform(V(at), V(L14_ADDR(0x001dfd80) + static_cast<uint32_t>(i) * 0x10), M(bolt));
        facing();
        place(qa + 0x20, qb + 0x20);
        colours(8, i == 39 ? 0u : alpha(lh(L14_ADDR(0x001e0000) + static_cast<uint32_t>(i) * 2)));
        draw();
        const float w = width(fpu_div(int_to_float(i), int_to_float(40)));
        swc1(half_a + 8, w);
        swc1(half_b + 8, fpu_mul(w, lwc1(L14_ADDR(0x00161c88))));
    }
    // the branches, in the bolt's frame
    lq_sq(seg, bolt);
    lq_sq(seg + 0x10, bolt + 0x10);
    lq_sq(seg + 0x20, bolt + 0x20);
    for (uint32_t b = 0; b < 3; ++b) {
        colours(0, alpha(lh(L14_ADDR(0x001e0350) + b * 0x20)));
        const int32_t from = lw(L14_ADDR(0x00161cf8) + b * 4);
        const float w = fpu_mul(width(fpu_div(int_to_float(from), int_to_float(40))), 0.75f);
        swc1(half_a + 8, w);
        swc1(half_b + 8, fpu_mul(w, 4.0f));
        edges(qa, qb);
        lq_sq(local, L14_ADDR(0x001dfd80) + (static_cast<uint32_t>(from) << 4));
        mat4_transform(V(prev), V(local), M(bolt));
        uint32_t off = L14_ADDR(0x001e0050) + b * 0x100;
        step_local(off);
        mat4_transform(V(next), V(local), M(bolt));
        lq_sq(seg + 0x30, prev);
        place(qa, qb);
        off += 0x10;
        uint32_t timer = L14_ADDR(0x001e0350) + b * 0x20 + 2;
        for (int k = 0; k < 15; ++k, off += 0x10, timer += 2) {
            edges(qa + 0x20, qb + 0x20);
            lq_sq(prev, next);
            step_local(off);
            mat4_transform(V(next), V(local), M(bolt));
            lq_sq(seg + 0x30, prev);
            place(qa + 0x20, qb + 0x20);
            colours(8, alpha(lh(timer)));
            draw();
            const float narrower = fpu_mul(lwc1(half_a + 8), u2f(0x3f733333u));  // 0.95
            swc1(half_a + 8, narrower);
            swc1(half_b + 8, fpu_mul(narrower, 4.0f));
        }
    }
}

}  // namespace rac1
