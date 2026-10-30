// Ported overlay code: the moby drop shadows (see moby_shadows.h), the compiled parts.
#include <game/ovl/moby_shadows.h>
#include <game/ovl/game_skips.h>

#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/render/mrp_shadows_gpu.inc>

namespace rac1 {

DropShadowObserver g_drop_shadow_observer;  // orig: (host)
bool (*g_drop_shadows_native)() = nullptr;  // orig: (host) mrp_shadows_gpu.inc

namespace {
constexpr uint32_t kShadowClearRgba = 0x7f808080u;  // shadow_buffer_clear's sprite colour (0x002854f8): the frame's alpha 0x7f
constexpr uint64_t kShadowApplyAlpha = (static_cast<uint64_t>(0x8000) << 22) | 0x64;  // 0x00285870's ALPHA_1 for the darkening: FIX 0x20
constexpr OvConst<0x001611c0u> CUR{};  // the packet buffer's next quadword
// orig: (helper) a 32-bit value as the 64-bit register holds it
uint64_t sx(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
// orig: (helper) repeated in 0x002854f8 / 0x002856a0 -- the screen in columns of 32 pixels: a DMA
// CNT / DIRECT of the GIF A+D set up (FRAME / TEST / ALPHA / ... from the caller) and the sprites
void buffer_packet(uint64_t test, uint64_t rgba, uint32_t gif2_lo, int32_t w, int32_t h, uint32_t& cols) {
    const int32_t c = (w > -1 ? w : w + 0x1f) >> 5;
    cols = static_cast<uint32_t>(c);
    const uint32_t q = static_cast<uint32_t>(c) + 5;
    const uint32_t p = lwu(CUR);
    sw(p, q | 0x10000000u);
    sw(lwu(CUR) + 4, 0);
    sw(lwu(CUR) + 8, 0);
    sw(lwu(CUR) + 0xc, q | 0x50000000u);
    const uint32_t t7 = lwu(CUR);
    const uint32_t v0 = t7 + 0x10;
    sw(CUR, v0);
    sd(t7 + 0x10, (static_cast<uint64_t>(0x8000) << 45) | 1);  // GIF tag: A+D, 1 x 5 registers
    sd(v0 + 0x48, 0x44);
    sd(v0 + 0x8, 0xe);
    sd(v0 + 0x10, test);
    sd(v0 + 0x18, 0x47);
    sd(v0 + 0x20, (static_cast<uint64_t>(0x9000) << 46) | gif2_lo);
    sd(v0 + 0x28, 0x10);
    sd(v0 + 0x30, 0x146);
    sd(v0 + 0x38, rgba);
    sd(v0 + 0x40, sx(static_cast<uint32_t>(c) | 0x8000u) | (static_cast<uint64_t>(0x9000) << 46));
    if (c > 0) {
        const uint64_t ty = sx(0x8000u - (static_cast<uint32_t>(h) << 3)) << 16;
        const uint64_t by = sx((static_cast<uint32_t>(h) << 3) + 0x7ff0u) << 16;
        uint64_t x0 = sx(0x8000u - (static_cast<uint32_t>(w) << 3));
        uint64_t x1 = sx(0x8200u - (static_cast<uint32_t>(w) << 3));
        uint32_t a = t7 + 0x60;
        for (int32_t i = 0; i < c; ++i) {
            sd(a, x0 | ty);
            sd(a + 8, x1 | by);
            a += 0x10;
            x1 = sx(static_cast<uint32_t>(x1) + 0x200);
            x0 = sx(static_cast<uint32_t>(x0) + 0x200);
        }
    }
    sw(CUR, lwu(CUR) + (static_cast<uint32_t>(c) << 4) + 0x50);
}
}  // namespace

// orig: 0x002854f8
void shadow_buffer_clear() {
    const int32_t w = lh(0x001518d0), h = lh(0x001518d2);
    sub_002a0ba8(0x42, 0x64);
    uint32_t cols = 0;
    buffer_packet(0x31001, kShadowClearRgba, 0x8001, w, h, cols);
}

// orig: 0x002856a0
void shadow_buffer_apply(uint32_t rgba) {
    const int32_t w = lh(0x001518d0), h = lh(0x001518d2);
    uint32_t cols = 0;
    buffer_packet(0x3d801, sx(rgba), 0x1, w, h, cols);
    const uint32_t p = lwu(CUR);
    sw(p, 0x10000000u);
    sw(lwu(CUR) + 4, 0);
    sw(lwu(CUR) + 8, 0x13000000u);
    sw(lwu(CUR) + 0xc, 0);
    sw(CUR, lwu(CUR) + 0x10);
}

// orig: 0x002852e0
void shadow_light_axes(uint32_t dir, float len) {
    const uint32_t t = ee_stack_alloc(0x10);
    lq_sq(t, dir);
    vec3_set_length(V(OV(0x00160430)), V(t), 1.0f);
    swc1(OV(0x00160450), fpu_mul(lwc1(OV(0x00160430)), len));
    swc1(OV(0x00160454), fpu_mul(lwc1(OV(0x00160434)), len));
    swc1(OV(0x00160458), fpu_mul(lwc1(OV(0x00160438)), len));
    const float x = lwc1(t), y = lwc1(t + 4), z = lwc1(t + 8);
    ee_stack_free(0x10);
    // a vector across the direction: its smallest component's axis (the others swapped)
    if (x < y) {
        if (!(x < z)) {
            swc1(OV(0x00160440), y);
            swc1(OV(0x00160444), x);
            swc1(OV(0x00160448), z);
        } else {
            swc1(OV(0x00160440), x);
            swc1(OV(0x00160444), z);
            swc1(OV(0x00160448), y);
        }
    } else if (!(y < z)) {
        swc1(OV(0x00160440), y);
        swc1(OV(0x00160444), x);
        swc1(OV(0x00160448), z);
    } else {
        swc1(OV(0x00160440), z);
        swc1(OV(0x00160444), y);
        swc1(OV(0x00160448), x);
    }
    vec3_cross_ba(V(OV(0x00160440)), V(OV(0x00160440)), V(OV(0x00160430)));
    vec3_set_length(V(OV(0x00160440)), V(OV(0x00160440)), 1.0f);
}

// orig: 0x00285410
uint32_t shadow_entry_draw(uint32_t e) {
    const int32_t kind = lh(e);
    if (kind == 0) {
        shadow_point_draw(e);
        return e + 0x20;
    }
    if (kind != 1) return e;
    shadow_segment_draw(e);
    return e + 0x30;
}

// orig: 0x00285468
void shadow_layer_send(uint32_t layer, uint32_t which, uint32_t n) {
    const uint32_t p = lwu(CUR);
    sw(p, 0x30000003u);
    sw(lwu(CUR) + 4, n * 0x30 + OV(0x001bbe90));
    sw(lwu(CUR) + 8, 0x13000000u);
    sw(lwu(CUR) + 0xc, 0x50000003u);
    const uint32_t s = lwu((which << 2) + OV(0x00160420));
    sw(CUR, lwu(CUR) + 0x10);
    shadow_packets(layer, s);
    if (g_drop_shadow_observer.layer != nullptr) g_drop_shadow_observer.layer(layer, s);  // (host)
}

// orig: 0x00285870
void drop_shadows_draw(uint32_t list) {
    uint32_t s2 = list, s0 = list;
    shadow_buffer_clear();
    if (g_drop_shadow_observer.begin != nullptr) g_drop_shadow_observer.begin(kShadowClearRgba);  // (host)
    sw(lwu(CUR), 0x30000007u);
    sw(lwu(CUR) + 4, lw(0x0015ed80) != 0 ? OV(0x001bbfc0u) : OV(0x001bbf50u));
    sw(lwu(CUR) + 8, 0x13000000u);
    sw(lwu(CUR) + 0xc, 0x50000007u);
    const uint32_t f = ee_stack_alloc(0x30);  // the frame's two vectors (+0 / +0x10) and a copy (+0x20)
    swc1(f + 0x14, 0.0f);
    swc1(f, 0.0f);
    swc1(f + 0x10, 0.0f);
    swc1(f + 4, 0.0f);
    sw(CUR, lwu(CUR) + 0x10);
    while (lw(s2) != 0) {
        lq_sq(f + 0x20, s0 + 0x10);
        s2 += 0x20;
        shadow_light_axes(f + 0x20, 1000.0f);
        int32_t n = 0;
        swc1(f + 8, lwc1(s0 + 0xc));
        swc1(f + 0x18, lwc1(s0 + 8));
        if (lw(s0) > 0) {
            do {
                ++n;
                s2 = shadow_entry_draw(s2);
                shadow_points_clip(f, f + 0x10);
                if (g_drop_shadow_observer.tube_points != nullptr) g_drop_shadow_observer.tube_points(lwu(OV(0x00160460)), lwu(s0 + 4));  // (host)
                shadow_quads(0x70000000u, lwu(OV(0x00160460)), lwu(s0 + 4));
                shadow_layer_send(2, 1, 2);
                shadow_layer_send(1, 0, 2);
            } while (n < lw(s0));
        }
        s0 = s2;
    }
    ee_stack_free(0x30);
    sub_002a0ba8(0x4c, lw(0x0015ed80) != 0 ? 0x80080u : 0x80070u);
    sub_002a0ba8(0x42, kShadowApplyAlpha);
    if (g_drop_shadow_observer.apply != nullptr) g_drop_shadow_observer.apply(kShadowApplyAlpha, 0);  // (host)
    shadow_buffer_apply(0);
    sub_002a0ba8(0x47, 0x5360b);
    sub_002a0ba8(0x42, (static_cast<uint64_t>(0x8000) << 24) | 0x44);
}

// orig: 0x0024fc38
void moby_shadows() {
    if (lw(OV(0x0015fffc)) == 0) {  // no deferred moby kept a shadow: an empty CNT in the slot
        const uint32_t r = lwu(OV(0x00160000));
        sw(r, 0x10000000u);
        sw(lwu(OV(0x00160000)) + 4, 0);
        sw(lwu(OV(0x00160000)) + 8, 0);
        sw(lwu(OV(0x00160000)) + 0xc, 0);
        return;
    }
    const uint32_t s0 = lwu(CUR);
    sw(CUR, s0 + 0x10);
    sw(lwu(OV(0x00160000)), 0x20000000u);  // the slot: NEXT to the shadows
    sw(lwu(OV(0x00160000)) + 4, lwu(CUR));
    sw(lwu(OV(0x00160000)) + 8, 0);
    sw(lwu(OV(0x00160000)) + 0xc, 0);
    // (host: RAC1_WIIU_SHADOWS_GPU - the shapes taken by the host, mrp_shadows_gpu.inc: the game's own drawing of them not run)
    if (!g_skip_shadows && !(g_drop_shadows_native != nullptr && g_drop_shadows_native())) {  // (testing: the GamePad debug menu, game_skips.h)
        shadow_points();
        drop_shadows_draw(OV(0x001ac780));
    }
    sw(lwu(CUR), 0x20000000u);  // NEXT back to the deferred mobys
    sw(lwu(CUR) + 4, lwu(OV(0x00160000)) + 0x10);
    sw(lwu(CUR) + 8, 0);
    sw(lwu(CUR) + 0xc, 0);
    sw(CUR, lwu(CUR) + 0x10);
    sw(s0, 0x20000000u);  // their chain's end: NEXT past the shadows
    sw(s0 + 0xc, 0);
    sw(s0 + 8, 0);
    sw(s0 + 4, lwu(CUR));
    if (g_drop_shadow_observer.chain != nullptr) g_drop_shadow_observer.chain(s0 + 0x10, lwu(CUR) - 1);  // (host) the shadows' tags: s0 + 0x10 .. the last NEXT
    prof_mark(OV(0x0015ff80), 8);
}

}  // namespace rac1
