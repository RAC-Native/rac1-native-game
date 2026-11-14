// Novalis's own moby classes: class 1848, five meshes drawn with reflection-mapped, scrolling texture coordinates
// (hand-ported from Novalis's own overlay). Header novalis_draw_cbs.h (the draw and its helper) and novalis_mobys2.h (the
// update); the update is registered in novalis_mobys2.cpp, the draw in novalis_draw_cbs.cpp. Addresses are Novalis's
// (`orig: L1:`); Novalis-only data is L1_ADDR, the base ELF's are bare (the frame's time step 0x15ed7c).
//
// The scroll: two offsets at 0x162110 / 0x162114 (kept in -1 .. 1). The meshes: five, their vertex counts 0x202ea0, their
// positions 0x208108 and normals 0x208138 (12 bytes a vertex), a third list 0x208120 (not read), their colours 0x208150; the
// texture coordinates are written into one buffer 0x208170 (8 bytes a vertex) for each mesh in turn.
#include <game/math/vecmath.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/novalis_draw_cbs.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>

namespace rac1 {

// Class 1848: 0 clears the scroll (state 1); 1 moves both offsets by 0.025 of the frame's time step, wraps them into -1 .. 1
// and queues the draw (0x30f0e0).
// orig: L1:0x0030f208
void nov_class1848_update_l1(uint32_t m) {
    const uint32_t su = L1_ADDR(0x00162110), sv = L1_ADDR(0x00162114);
    if (lbu(m + 0x20) == 0) {
        sw(su, 0);
        sw(sv, 0);
        sb(m + 0x20, 1);
        return;
    }
    if (lbu(m + 0x20) != 1) return;
    const float step = fpu_mul(lwc1(0x0015ed7c), u2f(0x3ccccccdu));
    const float u = fpu_add(lwc1(su), step), v = fpu_add(lwc1(sv), step);
    swc1(su, u);
    swc1(sv, v);
    if (1.0f < u) swc1(su, fpu_sub(u, 1.0f));
    if (lwc1(su) < -1.0f) swc1(su, fpu_add(lwc1(su), 1.0f));
    if (1.0f < lwc1(sv)) swc1(sv, fpu_sub(lwc1(sv), 1.0f));
    if (lwc1(sv) < -1.0f) swc1(sv, fpu_add(lwc1(sv), 1.0f));
    sub_001f93e0(L1_ADDR(0x0030f0e0), m);
}

// The texture coordinates of `n` vertices (positions `pos`, normals `nrm`: 12 bytes each) into `out` (8 bytes each): the
// direction from the camera (0x167240) to the vertex reflected off the reversed normal, mapped as a sphere map (x and y over
// twice the length of the reflection with z + 1, + 0.5, doubled) plus the scroll.
// orig: L1:0x0030ef18
void nov_30ef18_l1(int32_t n, uint32_t nrm, uint32_t pos, uint32_t out) {
    for (int32_t i = 0; i < n; ++i, out += 8) {
        const uint32_t k = static_cast<uint32_t>(i) * 0xc;
        const Vec4 p{lwc1(pos + k), lwc1(pos + k + 4), lwc1(pos + k + 8), 1.0f};
        Vec4 eye;
        vec3_sub(eye, p, V(L1_ADDR(0x00167240)));
        vec3_set_length(eye, eye, 1.0f);
        Vec4 normal{-lwc1(nrm + k), -lwc1(nrm + k + 4), -lwc1(nrm + k + 8), 1.0f};
        vec3_set_length(normal, normal, 1.0f);
        const float d = vec3_dot(normal, eye);
        Vec4 r;
        vec3_scale(r, normal, fpu_add(d, d));
        vec3_sub(r, eye, r);
        vec3_set_length(r, r, 1.0f);
        r.z = fpu_add(r.z, 1.0f);
        const float len = vec3_length(r);
        const float twice = fpu_add(len, len);
        float s = fpu_add(fpu_div(r.x, twice), 0.5f);
        float t = fpu_add(fpu_div(r.y, twice), 0.5f);
        s = fpu_add(s, s);
        t = fpu_add(t, t);
        swc1(out, fpu_add(s, lwc1(L1_ADDR(0x00162110))));
        swc1(out + 4, fpu_add(t, lwc1(L1_ADDR(0x00162114))));
    }
}

// Class 1848's draw: the GS state (the effect texture 0x28, alpha blending, no test), VU1 program 7, then each of the five
// meshes' texture coordinates and its packet.
// orig: L1:0x0030f0e0
void nov_class1848_draw_l1(uint32_t) {
    sub_002a0ba8(6, effect_tex0(0x28));
    sub_002a0ba8(0x42, (uint64_t{0x8000} << 22) | 0x64);
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x14, (uint64_t{0xff90} << 32) | 0x260);
    shadows_vu1_setup();
    const uint32_t uvs = L1_ADDR(0x00208170);
    for (uint32_t k = 0; k < 5; ++k) {
        const uint32_t w = k << 2;
        nov_30ef18_l1(lw(L1_ADDR(0x00202ea0) + w), lwu(L1_ADDR(0x00208138) + w), lwu(L1_ADDR(0x00208108) + w), uvs);
        prim_list_packet(lw(L1_ADDR(0x00202ea0) + w), lwu(L1_ADDR(0x00208108) + w), lwu(L1_ADDR(0x00208150) + w), uvs, 1);
    }
}

}  // namespace rac1
