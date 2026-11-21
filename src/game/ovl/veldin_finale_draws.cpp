// Veldin's finale (level 18): the draw callbacks of the level's own classes queued on the level's draw list
// (sub_001f93e0), hand-written: the robots' (class 1906) glows and pop, the end scene's (class 1563) glows on the scene's
// moby. Each is called with the moby the class queued it for. Level 18 only: the addresses are the finale's (`orig:
// L18:`), its own data L18_ADDR, the base ELF's bare. Registered in veldin_finale_mobys.cpp.
//
// VU0: none of these callbacks has a VU0 instruction of its own; their vector work is in the engine's helpers (glow_draw,
// quad_draw, moby_chain_point_v / moby_chain_world_one, the vec3 helpers, sin_vu, rgba_lerp), which the generated code
// called the same way before (the notes of task P18.4 list the registers the originals leave).
#include <game/math/vecmath.h>
#include <game/level/level_data.h>
#include <game/ovl/glows.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>

#include <utility>

namespace rac1 {

namespace {
constexpr int16_t ROBOT_CLASS = 0x772;  // (1906)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
// the effects' quad descriptor's fixed part: texture 0xb, the GIF tag's register list, additive blending (0x48, fixed
// alpha 0x80), (s, t) (1, 1) (0, 1) (1, 0) (0, 0); the GS's test / z registers 0x4a / 0x47 set as the effects' draws do
void glow_quad_begin(uint32_t desc) {
    sd(desc + 0x78, effect_tex0(0xb));
    sd(desc + 0x80, (uint64_t{0xff90} << 32) | 0x260);
    sd(desc + 0x88, (uint64_t{0x8000} << 24) | 0x48);
    sd(desc + 0x70, 0);
    sub_002a0ba8(0x4a, 0);
    sub_002a0ba8(0x47, 0x51001);
}

void glow_quad_st(uint32_t desc) {
    swc1(desc + 0x50, 1.0f); swc1(desc + 0x54, 1.0f); sw(desc + 0x58, 0); swc1(desc + 0x5c, 1.0f);
    swc1(desc + 0x60, 1.0f); sw(desc + 0x64, 0); sw(desc + 0x68, 0); sw(desc + 0x6c, 0);
}

void glow_quad_colour(uint32_t desc, uint32_t colour) {
    for (uint32_t k = 0; k < 4; ++k) sw(desc + 0x40 + k * 4, colour);
}

// a camera-facing frame at `frame` (rows +0 toward the point from the camera, +0x10 / +0x20 across, +0x30 the point,
// already there): the reference `ref` (0, 0, 1, 1)
void facing_frame(uint32_t frame, uint32_t ref) {
    vec3_sub(V(frame), V(frame + 0x30), V(L18_ADDR(0x001677c0)));
    vec3_set_length(V(frame), V(frame), 1.0f);
    sw(ref, 0);
    sw(ref + 4, 0);
    swc1(ref + 8, 1.0f);
    swc1(ref + 0xc, 1.0f);
    vec3_cross_ba(V(frame + 0x10), V(frame), V(ref));
    vec3_set_length(V(frame + 0x10), V(frame + 0x10), 1.0f);
    vec3_cross_ba(V(frame + 0x20), V(frame + 0x10), V(frame));
}
}  // namespace

// The robot's pop (+0x1f4 of its pvars running back to 0): a camera-facing quad over it, wider as the pop ends (1 - pop
// + L18:0x162650), taller as it starts (pop + 1 + L18:0x16264c), from L18:0x162648 below its place; its texture
// L18:0x162644, its colour between L18:0x16263c and 0x162640 by the pop, its blend L18:0x16262c .. 0x162638 (fixed alpha
// 0x80), its (s, t) the level's four pairs at L18:0x1efcd0.
// orig: L18:0x002fc860
void vf_cb_2fc860_l18(uint32_t m) {
    const Frame f(0x180);
    const uint32_t desc = f.sp, side = f.sp + 0x90, face = f.sp + 0xa0, up = f.sp + 0xb0, at = f.sp + 0xc0;
    const uint32_t pv = lwu(m + 0x78);
    const float pop = lwc1(pv + 0x1f4);
    const float width = fpu_add(fpu_sub(1.0f, pop), lwc1(L18_ADDR(0x00162650)));
    const float height = fpu_add(fpu_add(pop, 1.0f), lwc1(L18_ADDR(0x0016264c)));
    lq_sq(at, m + 0x10);
    swc1(at + 8, fpu_sub(lwc1(at + 8), lwc1(L18_ADDR(0x00162648))));
    vec_clear(V(up));
    swc1(up + 8, height);
    vec3_sub(V(side), V(L18_ADDR(0x001677c0)), V(m + 0x10));
    vec3_cross_ba(V(face), V(side), V(up));
    vec3_cross_ba(V(side), V(face), V(up));
    vec3_set_length(V(side), V(side), width);
    vec3_set_length(V(face), V(face), width);
    sd(desc + 0x78, effect_tex0(lw(L18_ADDR(0x00162644))));
    sd(desc + 0x80, (uint64_t{0xff90} << 32) | 0x260);  // (the GIF tag's register list)
    sd(desc + 0x70, 0);
    const uint64_t blend = static_cast<uint64_t>(static_cast<int64_t>(lw(L18_ADDR(0x0016262c)))) |
                           (static_cast<uint64_t>(static_cast<int64_t>(lw(L18_ADDR(0x00162630)))) << 2) |
                           (static_cast<uint64_t>(static_cast<int64_t>(lw(L18_ADDR(0x00162634)))) << 4) |
                           (static_cast<uint64_t>(static_cast<int64_t>(lw(L18_ADDR(0x00162638)))) << 6) | (uint64_t{0x8000} << 24);
    sd(desc + 0x88, blend);
    const uint32_t colour = rgba_lerp(lwu(L18_ADDR(0x0016263c)), lwu(L18_ADDR(0x00162640)), lwc1(pv + 0x1f4));
    for (uint32_t k = 0; k < 4; ++k) {  // the corners: (0, +-1, 0 / 1, 1) in the quad's frame
        const uint32_t c = desc + k * 0x10, st = L18_ADDR(0x001efcd0) + k * 8;
        swc1(desc + 0x50 + k * 8, lwc1(st));
        swc1(desc + 0x54 + k * 8, lwc1(st + 4));
        vec_clear(V(c));
        if ((k & 1) == 0) sw(c + 8, 0);
        else swc1(c + 8, 1.0f);
        swc1(c + 4, k < 2 ? 1.0f : -1.0f);
        swc1(c + 0xc, 1.0f);
        sw(desc + 0x40 + k * 4, colour);
    }
    quad_draw(desc, side);
}

// The glows of the robots of the moby's group (the level's group list +0x21): on each robot (class 1906) in states 1 .. 5,
// a glow at its joints 2, 3, 4 in its own colour (+0x90) with the alpha L18:0x162654.
// orig: L18:0x002fca98
void vf_cb_2fca98_l18(uint32_t m) {
    const Frame f(0x90);
    const uint32_t at = f.sp;
    const uint32_t list = lvd::group(lbu(m + 0x21));
    if (list == 0) return;
    for (uint32_t e = list;; e += 2) {
        const uint32_t o = lwu(L18_ADDR(0x0015ffd8)) + ((lhu(e) & 0x7fff) << 8);
        const uint8_t st = lbu(o + 0x20);
        if (lh(o + 0xa6) == ROBOT_CLASS && st != 0 && st < 6) {
            const uint32_t colour = (lwu(o + 0x90) & 0xffffff) + (lwu(L18_ADDR(0x00162654)) << 24);
            for (uint32_t joint = 2; joint < 5; ++joint) {
                moby_chain_point_v(o, joint, V(at));
                glow_draw(at, colour, 0.17499999701976776f, 0.07999999821186066f);
            }
        }
        if (lh(e) < 0) break;
    }
}

// The scene's glows (class 1563 queues it on the scene's moby in scenes 0 .. 3): at joints 1 and 2 of the moby, set back
// along the joint's third row by L18:0x162500, a glow of size L18:0x162450 / 0x162454 whose colour pulses between
// L18:0x162458 and 0x16245c over frame_scale_i(L18:0x162460) frames of the level's counter.
// orig: L18:0x002f9a98
void vf_cb_2f9a98_l18(uint32_t m) {
    const Frame f(0xe0);
    const uint32_t at = f.sp, joint = f.sp + 0x10, back = f.sp + 0x50;
    const int32_t period = frame_scale_i(lw(L18_ADDR(0x00162460)));
    const float phase = fpu_div(int_to_float(ee_rem(lw(L18_ADDR(0x0015f5cc)), period)), int_to_float(period));
    const float x = fpu_sub(fpu_mul(phase, u2f(0x40c5dc9cu)), u2f(0x40490fd0u));  // (6.18318, 3.14159)
    const uint32_t colour = rgba_lerp(lwu(L18_ADDR(0x00162458)), lwu(L18_ADDR(0x0016245c)), fpu_add(fpu_mul(sin_vu(x), 0.5f), 0.5f));
    for (uint32_t id = 1; id < 3; ++id) {
        moby_chain_world_one(m, id, joint);
        vec3_scale(V(back), V(joint + 0x20), lwc1(L18_ADDR(0x00162500)));
        vec3_sub(V(at), V(joint + 0x30), V(back));
        glow_draw(at, colour, lwc1(L18_ADDR(0x00162450)), lwc1(L18_ADDR(0x00162454)));
    }
}

// The scene's flat glow (class 1563 queues it on the class 0x59b moby in scene 4 / 7's first window): a camera-facing
// square at the moby's offset L18:0x162560 (reset to (0, 0, -45) at the window's first frame), the level's four corners
// L18:0x1dfb90 scaled by L18:0x162508 in the colour L18:0x162504, then by L18:0x16250c in L18:0x162510, their alpha the
// window's L18:0x162524.
// orig: L18:0x002f9c20
void vf_cb_2f9c20_l18(uint32_t m) {
    const Frame f(0x150);
    const uint32_t desc = f.sp, frame = f.sp + 0x90, at = f.sp + 0xc0, ref = f.sp + 0xd0;
    if (lw(L18_ADDR(0x0016d294)) == frame_scale_i(lw(L18_ADDR(0x001624bc)) + 1)) {
        swc1(L18_ADDR(0x00162568), -45.0f);
        swc1(L18_ADDR(0x0016256c), 1.0f);
        sw(L18_ADDR(0x00162560), 0);
        sw(L18_ADDR(0x00162564), 0);
    }
    glow_quad_begin(desc);
    mat3_transform(V(at), V(L18_ADDR(0x00162560)), M(m + 0xc0));
    vec3_add(V(at), V(at), V(m + 0x10));
    facing_frame(frame, ref);
    glow_quad_st(desc);
    for (const auto& layer : {std::pair<uint32_t, uint32_t>{L18_ADDR(0x00162504), L18_ADDR(0x00162508)},
                              std::pair<uint32_t, uint32_t>{L18_ADDR(0x00162510), L18_ADDR(0x0016250c)}}) {
        glow_quad_colour(desc, lwu(layer.first) | (lwu(L18_ADDR(0x00162524)) << 24));
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t c = desc + k * 0x10;
            vec3_scale(V(c), V(L18_ADDR(0x001dfb90) + k * 0x10), lwc1(layer.second));
            mat4_transform(V(c), V(c), M(frame));
        }
        quad_draw(desc, 0);
    }
}

// The scene's halo before the camera (class 1563 queues it on the scene's moby +0x178 in scene 4 / 7's third window): a
// camera-facing square at the camera's offset L18:0x162550 (reset to (1, 0, +-0.15) at the window's first frame: -0.15 in
// scene 7), the level's four corners L18:0x1dfb90 scaled by L18:0x162520 in the colour L18:0x1624f8, then by 0.75 and
// 0.5 of it in L18:0x1624fc.
// orig: L18:0x002f9780
void vf_cb_2f9780_l18(uint32_t) {
    const Frame f(0x150);
    const uint32_t desc = f.sp, frame = f.sp + 0x90, at = f.sp + 0xc0, ref = f.sp + 0xd0;
    if (lw(L18_ADDR(0x0016d294)) == frame_scale_i(lw(L18_ADDR(0x00162498)) + 1)) {
        swc1(L18_ADDR(0x00162550), 1.0f);
        sw(L18_ADDR(0x00162554), 0);
        swc1(L18_ADDR(0x00162558), lw(L18_ADDR(0x0016d290)) == 7 ? -0.15000000596046448f : 0.15000000596046448f);
        swc1(L18_ADDR(0x0016255c), 1.0f);
    }
    glow_quad_begin(desc);
    mat3_transform(V(at), V(L18_ADDR(0x00162550)), M(L18_ADDR(0x001679d0)));
    vec3_add(V(at), V(at), V(L18_ADDR(0x001677c0)));
    facing_frame(frame, ref);
    glow_quad_st(desc);
    static const float parts[] = {1.0f, 0.75f, 0.5f};
    for (int layer = 0; layer < 3; ++layer) {
        if (layer < 2) glow_quad_colour(desc, lwu(layer == 0 ? L18_ADDR(0x001624f8) : L18_ADDR(0x001624fc)));
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t c = desc + k * 0x10;
            const float size = layer == 0 ? lwc1(L18_ADDR(0x00162520)) : fpu_mul(lwc1(L18_ADDR(0x00162520)), parts[layer]);
            vec3_scale(V(c), V(L18_ADDR(0x001dfb90) + k * 0x10), size);
            mat4_transform(V(c), V(c), M(frame));
        }
        quad_draw(desc, 0);
    }
}

// The scene's beam glow (class 1563 queues it on the class 0x59b moby in scene 4 / 7's second window): a quad along the
// moby's down axis, turned toward the camera, at its offset L18:0x162560; the level's four corners L18:0x1dfbd0 stretched
// across by L18:0x16252c and along by L18:0x162514 in the colour L18:0x162504 (opaque), then by L18:0x162518 and
// 0x16251c in L18:0x162510 (half alpha).
// orig: L18:0x002f9eb8
void vf_cb_2f9eb8_l18(uint32_t m) {
    const Frame f(0x1a0);
    const uint32_t desc = f.sp, down = f.sp + 0x90, face = f.sp + 0xa0, across = f.sp + 0xb0, at = f.sp + 0xc0, ref = f.sp + 0xd0;
    const uint32_t corners = f.sp + 0xe0;
    glow_quad_begin(desc);
    mat3_transform(V(at), V(L18_ADDR(0x00162560)), M(m + 0xc0));
    vec3_add(V(at), V(at), V(m + 0x10));
    sw(ref, 0);
    sw(ref + 4, 0);
    swc1(ref + 8, -1.0f);
    swc1(ref + 0xc, 1.0f);
    mat3_transform(V(down), V(ref), M(m + 0xc0));
    vec3_sub(V(face), V(at), V(L18_ADDR(0x001677c0)));
    vec3_cross_ba(V(across), V(face), V(down));
    vec3_set_length(V(across), V(across), 1.0f);
    vec3_cross_ba(V(face), V(down), V(across));
    glow_quad_st(desc);
    const struct { uint32_t colour, alpha, length; } layers[] = {
        {L18_ADDR(0x00162504), 0xff000000u, L18_ADDR(0x00162514)},
        {L18_ADDR(0x00162510), 0x7f000000u, L18_ADDR(0x00162518)},
        {L18_ADDR(0x00162510), 0x7f000000u, L18_ADDR(0x0016251c)}};
    for (const auto& layer : layers) {
        glow_quad_colour(desc, lwu(layer.colour) | layer.alpha);
        const float length = lwc1(layer.length), width = lwc1(L18_ADDR(0x0016252c));
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t c = corners + k * 0x10;
            lq_sq(c, L18_ADDR(0x001dfbd0) + k * 0x10);
            swc1(c + 8, fpu_mul(lwc1(c + 8), length));
            swc1(c + 0, fpu_mul(lwc1(c + 0), width));
        }
        for (uint32_t k = 0; k < 4; ++k) mat4_transform(V(desc + k * 0x10), V(corners + k * 0x10), M(down));
        quad_draw(desc, 0);
    }
}

}  // namespace rac1
