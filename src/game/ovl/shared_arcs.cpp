// The electric arcs of Eudora's (level 4) and Batalia's (8) machines: fourteen arcs between pairs of the class's points (in
// the moby's frame, a table of the level's), each a line of sparks (particle 0x17) with a burst (particle 0x15) at both ends.
// The arcs and their pairs are the same code on both levels, relinked; the sparks' function is another build on each: on
// Eudora the sparks are brighter (alpha from 0x40, 0x7e glow) and live 90 frames at the frame rate, on Batalia their life
// is a word of the level's. Level-only words (the points, the arcs' counts, the tuning) are per level below. Header
// shared_arcs.h.
#include <game/ovl/shared_arcs.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
bool on_eudora_build() { return on_level(EUDORA); }
uint32_t arc_points() { return on_eudora_build() ? L4_ADDR(0x001d2c90) : L8_ADDR(0x001db100); }  // (16 bytes each, w: the width)
uint32_t arc_counts() { return on_eudora_build() ? L4_ADDR(0x001618c0) : L8_ADDR(0x001619b0); }  // (the sparks of each kind of arc)
uint32_t arc_width() { return on_eudora_build() ? L4_ADDR(0x001618b0) : L8_ADDR(0x00161994); }
uint32_t arc_rise() { return on_eudora_build() ? L4_ADDR(0x001618b4) : L8_ADDR(0x00161998); }
uint32_t arc_size() { return on_eudora_build() ? L4_ADDR(0x001618bc) : L8_ADDR(0x001619a0); }
uint32_t arc_life() { return L8_ADDR(0x001619a8); }  // (Batalia's build: the sparks' life, a half and its low byte)

uint32_t grey(int32_t g) {
    const uint32_t v = static_cast<uint32_t>(g);
    return v | ((v << 16) | (v << 8));
}

// a burst of sparks (particle 0x15) at `at`, `size` big
void burst(uint32_t at, uint32_t vel, float size) {
    swc1(vel, fpu_mul(cos_vu(rand_angle()), 0.05f));
    const float s = sin_vu(rand_angle());
    sw(vel + 8, 0);
    swc1(vel + 4, fpu_mul(s, 0.05f));
    swc1(vel + 8, rand_range_f(0.01f, 0.03f));
    const int32_t lo = frame_scale_i(10);
    const int32_t hi = frame_scale_i(20);
    const int32_t n = rand_range(lo, hi);
    spawn_particle_15_v(V(at), V(vel), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(n), 1, size);
}
}  // namespace

// orig: L4:0x002c41e0 (Eudora's build) L8:0x002da3f0 (Batalia's build)
void arc_sparks(uint32_t m, uint32_t from, uint32_t to, int32_t count, float w0, float w1) {
    (void)m;  // (unread)
    const bool eudora = on_eudora_build();
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t step = sp, at = sp + 0x10, vel = sp + 0x20;
    const float n = static_cast<float>(count);
    vec3_sub(V(step), V(to), V(from));
    vec3_scale(V(step), V(step), fpu_div(1.0f, n));
    const float widen = fpu_div(fpu_sub(w1, w0), n);
    V(vel) = Vec4{0.0f, 0.0f, 0.01f, 1.0f};
    for (int32_t i = 0; i < count; ++i) {
        vec3_scale(V(at), V(step), int_to_float(i));
        vec3_add(V(at), V(from), V(at));
        swc1(vel, rand_range_signed_f(0.0f, 0.005f));
        swc1(vel + 4, rand_range_signed_f(0.0f, 0.005f));
        const float rise = lwc1(arc_rise());
        swc1(vel + 8, rand_range_f(fpu_mul(rise, 0.1f), rise));
        const int32_t alpha = rand_range(eudora ? 0x40 : 0x30, 0x70);
        const uint32_t colour = (static_cast<uint32_t>(alpha) << 24) | grey(rand_range(eudora ? 0x40 : 0x30, 0x7f));
        const float jitter = rand_range_f(1.0f, u2f(0x3f828f5cu));  // (1.02)
        const int32_t spin = rand_range(-2, 2);
        const uint32_t p = spawn_particle_17(at, spin, vel, colour, lwc1(arc_size()), 1.0f, jitter, fpu_add(widen, fpu_mul(w0, 210000.0f)));
        if (p == 0) continue;
        if (rand_mod(2) != 0) {  // (one in two glows)
            sb(p + 3, eudora ? 0x7eu : 0x44u);
            sw(p + 4, (static_cast<uint32_t>(alpha) << 24) | grey(rand_range(0x60, 0xe0)));
        }
        if (eudora) {
            sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x5a)));
            sb(p + 0x2a, static_cast<uint32_t>(alpha));
            sw(p + 0x24, 2);
            sb(p + 0x2b, static_cast<uint32_t>(frame_scale_i(0x5a)));
        } else {
            sh(p + 0xa, lhu(arc_life()));
            sw(p + 0x24, 2);
            sb(p + 0x2a, static_cast<uint32_t>(alpha));
            sb(p + 0x2b, lbu(arc_life()));
        }
    }
    burst(from, vel, 10000.0f);
    burst(to, vel, 20000.0f);
    ee_stack_free(0x40);
}

// orig: L4:0x002c45b8 L8:0x002da7c0
void arc_between(uint32_t m, int32_t a, int32_t b, int32_t count) {
    const uint32_t pa = arc_points() + (static_cast<uint32_t>(a) << 4);
    const uint32_t pb = arc_points() + (static_cast<uint32_t>(b) << 4);
    const uint32_t sp = ee_stack_alloc(0x20);
    const uint32_t from = sp, to = sp + 0x10;
    V(from) = V(pa);
    swc1(from + 0xc, 1.0f);
    mat3_transform(V(from), V(from), M(m + 0xc0));
    vec3_add(V(from), V(from), V(m + 0x10));
    V(to) = V(pb);
    swc1(to + 0xc, 1.0f);
    mat3_transform(V(to), V(to), M(m + 0xc0));
    vec3_add(V(to), V(to), V(m + 0x10));
    const float width = lwc1(arc_width());
    arc_sparks(m, from, to, count, fpu_mul(lwc1(pa + 0xc), width), fpu_mul(lwc1(pb + 0xc), width));
    ee_stack_free(0x20);
}

// orig: L4:0x002c46d0 L8:0x002da8d8
void arcs_update(uint32_t m) {
    struct Arc { int32_t a, b; uint32_t kind; };
    static constexpr Arc kArcs[] = {{0, 1, 0}, {2, 3, 1}, {4, 5, 2}, {5, 6, 3}, {6, 7, 4}, {8, 9, 2}, {9, 10, 3}, {10, 11, 4},
                                    {12, 13, 5}, {14, 15, 5}, {16, 17, 6}, {20, 21, 6}, {18, 19, 7}, {22, 23, 7}};
    for (const Arc& arc : kArcs) arc_between(m, arc.a, arc.b, lw(arc_counts() + arc.kind * 4));
}

}  // namespace rac1
