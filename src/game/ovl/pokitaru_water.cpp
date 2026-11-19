// Pokitaru's (level 11) waterfall / water-surface scroll classes (1903, 1919) and their draw-list callbacks
// (L11:0x31db50, L11:0x31e930) and UV helpers (0x31e048, 0x31d8d8, 0x31eea8, 0x31e6a8), ported by hand.
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/pokitaru_own.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/variants_l11.h>

namespace rac1 {

namespace {

void scroll_uv_pair(uint32_t speed_base, uint32_t uv_base, uint32_t index) {
    const uint32_t off = index << 3;
    const uint32_t spd = speed_base + off;
    const uint32_t uv = uv_base + off;
    for (uint32_t c = 0; c < 8; c += 4) {
        const float dt = lwc1(0x0015ed7cu);
        float v = fpu_add(lwc1(uv + c), fpu_mul(lwc1(spd + c), dt));
        if (1.0f < v) v = fpu_sub(v, 1.0f);
        swc1(uv + c, v);
        v = lwc1(uv + c);
        if (v < -1.0f) swc1(uv + c, fpu_add(v, 1.0f));
    }
}

void draw_mesh_strip(uint32_t uv_scratch, uint32_t uv_off_base, uint32_t count_tbl, uint32_t pos_tbl,
                     uint32_t uv_tbl, uint32_t col_tbl, int32_t n, uint32_t idx) {
    const uint32_t uv_delta = uv_off_base + (idx << 3);
    for (int32_t k = 0; k < n; ++k) {
        const uint32_t o = static_cast<uint32_t>(k) << 2;
        const int32_t cnt = lw(count_tbl + o);
        uv_offset_pairs(uv_scratch, lwu(uv_tbl + o), cnt, uv_delta);
        prim_list_packet(cnt, lwu(pos_tbl + o), lwu(col_tbl + o), uv_scratch, 1);
    }
}

}  // namespace

// Advances UV offset pair `index` for class 1903's water meshes, wrapping to [-1, 1].
// orig: L11:0x0031e048
void pok_31e048_l11(uint32_t index) {
    scroll_uv_pair(L11_ADDR(0x001f14d0u), L11_ADDR(0x001ff750u), index);
}

// Emits the 4 scrolled primitive lists for class 1903 using UV offset `index`.
// orig: L11:0x0031d8d8
void pok_31d8d8_l11(uint32_t index) {
    draw_mesh_strip(L11_ADDR(0x001ff770u), L11_ADDR(0x001ff750u), L11_ADDR(0x00162530u), L11_ADDR(0x00162540u),
                    L11_ADDR(0x00162550u), L11_ADDR(0x00162570u), 4, index);
}

// Class 1903: initializes vertex colours and UV scroll offsets in state 0; in state 1 steps the scroll layers when
// the camera is above z = 170 and the surface is on screen, fading its detail alpha between 15 and 35 units.
// orig: L11:0x0031e2f0
void pok_class1903_update_l11(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        if (lw(L11_ADDR(0x0016252cu)) == 0) {
            const float f07 = u2f(0x3f333333u);
            const float f09 = u2f(0x3f666666u);
            mesh_colours_scale(L11_ADDR(0x00162570u), 4, L11_ADDR(0x00162530u), 1, f07, 1.0f, f09, 0.5f);
            mesh_colours_scale(L11_ADDR(0x001f9a48u), 11, L11_ADDR(0x001f4978u), 1, 0.5f, 0.5f, 0.5f, 1.0f);
            mesh_colours_scale(L11_ADDR(0x001ff730u), 8, L11_ADDR(0x001f9a78u), 1, 0.25f, 0.25f, 0.25f, f07);
            sw(L11_ADDR(0x0016252cu), 1);
        }
        sb(m + 0x20, 1);
        for (uint32_t k = 0; k < 3; ++k) {
            sw(L11_ADDR(0x001ff750u) + (k << 3), 0);
            sw(L11_ADDR(0x001ff750u) + (k << 3) + 4, 0);
            sw(L11_ADDR(0x001625b0u), 0);
            sw(L11_ADDR(0x001625b4u), 0);
        }
        const float step = u2f(0x3ecccccd);
        for (int32_t k = 0; k < 2; ++k) {
            sw(L11_ADDR(0x00162598u) + (static_cast<uint32_t>(k) << 3), 0);
            swc1(L11_ADDR(0x0016259cu) + (static_cast<uint32_t>(k) << 3), fpu_mul(static_cast<float>(k), step));
        }
        sw(L11_ADDR(0x001625a8u), 0);
        swc1(L11_ADDR(0x001625acu), fpu_mul(0.0f, step));
        return;
    }
    if (state != 1) return;
    if (lwc1(L11_ADDR(0x001677c8u)) < 170.0f) return;
    const uint32_t sp = ee_stack_alloc(0x20);
    swc1(sp + 0x0, lwc1(L11_ADDR(0x001624e8u)));
    swc1(sp + 0x4, lwc1(L11_ADDR(0x001624ecu)));
    swc1(sp + 0x8, lwc1(L11_ADDR(0x001624f0u)));
    swc1(sp + 0xc, lwc1(L11_ADDR(0x001624f4u)));
    if (point_on_screen(sp, 1000.0f) < 0) {
        ee_stack_free(0x20);
        return;
    }
    if (!(180.0f < lwc1(L11_ADDR(0x001677c8u)))) {
        ee_stack_free(0x20);
        return;
    }
    swc1(sp + 0x10, lwc1(L11_ADDR(0x001624e8u)));
    swc1(sp + 0x14, lwc1(L11_ADDR(0x001624ecu)));
    swc1(sp + 0x18, lwc1(L11_ADDR(0x001624f0u)));
    swc1(sp + 0x1c, 1.0f);
    vec3_sub(V(sp + 0x10), V(L11_ADDR(0x001677c0u)), V(sp + 0x10));
    const float dist = vec3_length(V(sp + 0x10));
    if (dist <= 35.0f) {
        pok_31e048_l11(0);
        wave_step(L11_ADDR(0x001ff758u), L11_ADDR(0x001625b0u), u2f(0x3f051eb8u), u2f(0x3f1eb852u), u2f(0x3d75c28fu));
        pok_31e048_l11(2);
        if (dist < 15.0f) {
            sb(L11_ADDR(0x00162590u), 0xff);
        } else if (dist < 35.0f) {
            const float alpha = fpu_mul(fpu_sub(1.0f, fpu_div(fpu_sub(dist, 15.0f), 20.0f)), 255.0f);
            sb(L11_ADDR(0x00162590u), static_cast<uint32_t>(float_to_int(alpha)));
        } else {
            sb(L11_ADDR(0x00162590u), 0);
        }
    }
    uv_scroll_step(5, 0);
    uv_scroll_step(4, 0);
    uv_scroll_step(4, 1);
    sub_001f93e0(L11_ADDR(0x0031db50u), m);
    ee_stack_free(0x20);
}

// Draw-list callback for class 1903: sets up VU1 program 7 (via shadows_vu1_setup, preserving VU0 registers) and
// draws the water base, optional close-up shimmer layers, and scrolled meshes (sets 3 and 4).
// orig: L11:0x0031db50
void pok_cb_31db50_l11(uint32_t) {
    sub_002a0ba8(0x42, (0x8000ULL << 24) | 0x44ULL);
    sub_002a0ba8(0x08, 0);
    sub_002a0ba8(0x14, (0xff90ULL << 32) | 0x260ULL);
    sub_002a0ba8(0x47, 0x513f1ULL);
    shadows_vu1_setup();
    sub_002a0ba8(0x06, effect_tex0(0x2c));
    sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L11_ADDR(0x001624f8u))) << 32) | 0x44ULL);
    pok_31d8d8_l11(0);
    if (lbu(L11_ADDR(0x00162590u)) != 0) {
        sub_002a0ba8(0x06, effect_tex0(0x29));
        const uint32_t a1 = static_cast<uint32_t>((lw(L11_ADDR(0x001624fcu)) * static_cast<int32_t>(lbu(L11_ADDR(0x00162590u)))) >> 8);
        sub_002a0ba8(0x42, (static_cast<uint64_t>(a1) << 32) | 0x68ULL);
        pok_31d8d8_l11(1);
        const uint32_t a2 = static_cast<uint32_t>((lw(L11_ADDR(0x001624fcu)) * static_cast<int32_t>(lbu(L11_ADDR(0x00162590u)))) >> 8);
        sub_002a0ba8(0x42, (static_cast<uint64_t>(a2) << 32) | 0x62ULL);
        pok_31d8d8_l11(2);
    }
    sub_002a0ba8(0x06, effect_tex0(0x2a));
    sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L11_ADDR(0x00162518u))) << 32) | 0x48ULL);
    uv_scroll_meshes(3, 0);
    sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L11_ADDR(0x0016251cu))) << 32) | 0x48ULL);
    uv_scroll_meshes(3, 1);
    sub_002a0ba8(0x06, effect_tex0(0x2b));
    sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L11_ADDR(0x00162528u))) << 32) | 0x48ULL);
    uv_scroll_meshes(4, 0);
    sub_002a0ba8(0x47, 0x5360bULL);
}

// Advances UV offset pair `index` for class 1919's water meshes, wrapping to [-1, 1].
// orig: L11:0x0031eea8
void pok_31eea8_l11(uint32_t index) {
    scroll_uv_pair(L11_ADDR(0x00200710u), L11_ADDR(0x00217758u), index);
}

// Emits the 17 scrolled primitive lists for class 1919 using UV offset `index`.
// orig: L11:0x0031e6a8
void pok_31e6a8_l11(uint32_t index) {
    draw_mesh_strip(L11_ADDR(0x00217770u), L11_ADDR(0x00217758u), L11_ADDR(0x00207900u), L11_ADDR(0x00215148u),
                    L11_ADDR(0x00215190u), L11_ADDR(0x00215220u), 17, index);
}

// Class 1919: sister water-surface class to 1903, fading its detail alpha between 80 and 100 units and queuing
// callback 0x31e930.
// orig: L11:0x0031f150
void pok_class1919_update_l11(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        if (lw(L11_ADDR(0x001625fcu)) == 0) {
            const float f07 = u2f(0x3f333333u);
            const float f09 = u2f(0x3f666666u);
            mesh_colours_scale(L11_ADDR(0x00215220u), 17, L11_ADDR(0x00207900u), 1, f07, 1.0f, f09, 0.5f);
            mesh_colours_scale(L11_ADDR(0x00217740u), 5, L11_ADDR(0x00215268u), 1, 0.5f, 0.5f, 0.5f, 1.0f);
            mesh_colours_scale(L11_ADDR(0x002078d8u), 9, L11_ADDR(0x00200728u), 1, 0.25f, 0.25f, 0.25f, f07);
            sw(L11_ADDR(0x001625fcu), 1);
        }
        sb(m + 0x20, 1);
        for (uint32_t k = 0; k < 3; ++k) {
            sw(L11_ADDR(0x00217758u) + (k << 3), 0);
            sw(L11_ADDR(0x00217758u) + (k << 3) + 4, 0);
            sw(L11_ADDR(0x00162630u), 0);
            sw(L11_ADDR(0x00162634u), 0);
        }
        const float step = u2f(0x3ecccccd);
        for (int32_t k = 0; k < 2; ++k) {
            sw(L11_ADDR(0x00162618u) + (static_cast<uint32_t>(k) << 3), 0);
            swc1(L11_ADDR(0x0016261cu) + (static_cast<uint32_t>(k) << 3), fpu_mul(static_cast<float>(k), step));
        }
        sw(L11_ADDR(0x00162628u), 0);
        swc1(L11_ADDR(0x0016262cu), fpu_mul(0.0f, step));
        return;
    }
    if (state != 1) return;
    if (lwc1(L11_ADDR(0x001677c8u)) < 170.0f) return;
    const uint32_t sp = ee_stack_alloc(0x20);
    swc1(sp + 0x0, lwc1(L11_ADDR(0x001625b8u)));
    swc1(sp + 0x4, lwc1(L11_ADDR(0x001625bcu)));
    swc1(sp + 0x8, lwc1(L11_ADDR(0x001625c0u)));
    swc1(sp + 0xc, lwc1(L11_ADDR(0x001625c4u)));
    if (point_on_screen(sp, 1000.0f) < 0) {
        ee_stack_free(0x20);
        return;
    }
    if (!(180.0f < lwc1(L11_ADDR(0x001677c8u)))) {
        ee_stack_free(0x20);
        return;
    }
    swc1(sp + 0x10, lwc1(L11_ADDR(0x001625b8u)));
    swc1(sp + 0x14, lwc1(L11_ADDR(0x001625bcu)));
    swc1(sp + 0x18, lwc1(L11_ADDR(0x001625c0u)));
    swc1(sp + 0x1c, 1.0f);
    vec3_sub(V(sp + 0x10), V(L11_ADDR(0x001677c0u)), V(sp + 0x10));
    const float dist = vec3_length(V(sp + 0x10));
    if (dist <= 100.0f) {
        pok_31eea8_l11(0);
        wave_step(L11_ADDR(0x00217760u), L11_ADDR(0x00162630u), u2f(0x3f051eb8u), u2f(0x3f1eb852u), u2f(0x3d75c28fu));
        pok_31eea8_l11(2);
        if (dist < 80.0f) {
            sb(L11_ADDR(0x00162610u), 0xff);
        } else if (dist < 100.0f) {
            const float alpha = fpu_mul(fpu_sub(1.0f, fpu_div(fpu_sub(dist, 80.0f), 20.0f)), 255.0f);
            sb(L11_ADDR(0x00162610u), static_cast<uint32_t>(float_to_int(alpha)));
        } else {
            sb(L11_ADDR(0x00162610u), 0);
        }
    }
    uv_scroll_step(7, 0);
    uv_scroll_step(6, 0);
    uv_scroll_step(6, 1);
    sub_001f93e0(L11_ADDR(0x0031e930u), m);
    ee_stack_free(0x20);
}

// Draw-list callback for class 1919: sets up VU1 program 7 (via shadows_vu1_setup, preserving VU0 registers) and
// draws the base/shimmer/foam layers when the camera is above z = 255 or inside one of the three pvar boxes,
// plus the scrolled mesh set 5 unconditionally.
// orig: L11:0x0031e930
void pok_cb_31e930_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sub_002a0ba8(0x42, (0x8000ULL << 24) | 0x44ULL);
    sub_002a0ba8(0x08, 0);
    sub_002a0ba8(0x14, (0xff90ULL << 32) | 0x260ULL);
    sub_002a0ba8(0x47, 0x513f1ULL);
    shadows_vu1_setup();
    const uint32_t cam = L11_ADDR(0x001677c0u);
    const bool show_base = (255.0f < lwc1(L11_ADDR(0x001677c8u))) ||
                           (point_in_box(cam, lw(pv + 0x0)) != 0) ||
                           (point_in_box(cam, lw(pv + 0x4)) != 0) ||
                           (point_in_box(cam, lw(pv + 0x8)) != 0);
    if (show_base) {
        sub_002a0ba8(0x06, effect_tex0(0x2c));
        sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L11_ADDR(0x001625c8u))) << 32) | 0x44ULL);
        pok_31e6a8_l11(0);
    }
    if (show_base && lbu(L11_ADDR(0x00162610u)) != 0) {
        sub_002a0ba8(0x06, effect_tex0(0x29));
        const uint32_t a1 = static_cast<uint32_t>((lw(L11_ADDR(0x001625ccu)) * static_cast<int32_t>(lbu(L11_ADDR(0x00162610u)))) >> 8);
        sub_002a0ba8(0x42, (static_cast<uint64_t>(a1) << 32) | 0x68ULL);
        pok_31e6a8_l11(1);
        const uint32_t a2 = static_cast<uint32_t>((lw(L11_ADDR(0x001625ccu)) * static_cast<int32_t>(lbu(L11_ADDR(0x00162610u)))) >> 8);
        sub_002a0ba8(0x42, (static_cast<uint64_t>(a2) << 32) | 0x62ULL);
        pok_31e6a8_l11(2);
    }
    sub_002a0ba8(0x06, effect_tex0(0x2a));
    sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L11_ADDR(0x001625e8u))) << 32) | 0x48ULL);
    uv_scroll_meshes(5, 0);
    sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L11_ADDR(0x001625ecu))) << 32) | 0x48ULL);
    uv_scroll_meshes(5, 1);
    if (show_base) {
        sub_002a0ba8(0x06, effect_tex0(0x2b));
        sub_002a0ba8(0x42, (static_cast<uint64_t>(lwu(L11_ADDR(0x001625f8u))) << 32) | 0x48ULL);
        uv_scroll_meshes(6, 0);
    }
}

}  // namespace rac1
