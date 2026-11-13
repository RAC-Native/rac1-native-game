// Blarg's (level 6) and Veldin's finale's (18) flash, class 1898: spawned with its length, it is drawn each frame (its
// level's own draw callback) while its phase (pvar +0) grows by its step (+4) up to 2. With two small helpers both levels
// have: a camera type 0x12's flag, and a class's state query (another build on each level). The copies are the same code,
// relinked: the draw callback is each level's own (L6_ADDR / L18_ADDR); the cameras' records 0x15ef50 go through OV(). The
// flash's draw (its callback) reads the level's own look of it (per level below); the screen's offset 0x13e510 is the base
// ELF's.
// Header shared_flash.h.
#include <game/ovl/shared_flash.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_effects.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t kFlashClass = 0x76a;
uint32_t flash_draw() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x002fb690) : L6_ADDR(0x0030bfa8); }

// The flash's look (the level's own words): +0 its texture, +4 its size (pixels / 16), +8 .. +0x14 its quad's texture
// coordinates.
uint32_t look(uint32_t o) { return (on_level(VELDIN_FINALE) ? L18_ADDR(0x001625d4) : L6_ADDR(0x00162414)) + o; }

// a GS vertex's XYZ (x and y in 1/16 pixels, the depth 0xfffff0) from x and y as the original adds them: 32-bit sums
// sign-extended into the 64-bit word
uint64_t sext32(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
constexpr uint64_t XYZ_BASE = 0x00fffff080008000ull;
}  // namespace

// orig: L6:0x0030be60 L18:0x002fb548
void flash_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        moby_retire(m);
        return;
    case 1: {
        sub_001f93e0(flash_draw(), m);
        const float phase = fpu_add(lwc1(pv), lwc1(pv + 4));
        swc1(pv, phase);
        if (2.0f < phase) moby_retire(m);
        return;
    }
    default:
        return;
    }
}

// A screen quad at the flash's point, its size growing with the phase up to 1, its colour from the first word (+8) to the
// second (+0xc) as the phase goes from 1 to 2; drawn additively (the GS's alpha register 0x47 set and set back).
// orig: L6:0x0030bfa8 L18:0x002fb690
void flash_draw_quad(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t at = sp, xyz = sp + 0x10, colours = sp + 0x30, uvs = sp + 0x40, screen = sp + 0x50;
    const uint32_t pv = lwu(m + 0x78);
    lq_sq(at, m + 0x10);
    float grow = lwc1(pv);
    if (1.0f < grow) grow = 1.0f;
    float fade = fpu_sub(lwc1(pv), 1.0f);
    if (1.0f < fade) fade = 1.0f;
    else if (fade < 0.0f) fade = 0.0f;
    sub_002a0ba8(0x47, 0x5380b);
    screen_project(screen, at);
    const uint32_t x = static_cast<uint32_t>(float_to_int(fpu_sub(lwc1(screen), static_cast<float>(lw(0x0013e510))))) - 0x1000;
    const uint32_t y = static_cast<uint32_t>(float_to_int(fpu_sub(lwc1(screen + 4), static_cast<float>(lw(0x0013e510))))) - 0x1000;
    for (uint32_t k = 0; k < 0x10; k += 4) sw(uvs + k, lwu(look(8 + k)));
    const uint32_t colour = rgba_lerp(lwu(pv + 8), lwu(pv + 0xc), fade);
    for (uint32_t k = 0; k < 0x10; k += 4) sw(colours + k, colour);
    const uint32_t half = static_cast<uint32_t>(float_to_int(fpu_mul(static_cast<float>(static_cast<int32_t>(lwu(look(4)) << 4)), grow)));
    const uint64_t top = sext32((y + half) << 16) + XYZ_BASE, bottom = sext32((y - half) << 16) + XYZ_BASE;
    const uint64_t left = sext32(x - half), right = sext32(x + half);
    sd(xyz, bottom + right);
    sd(xyz + 8, bottom + left);
    sd(xyz + 0x10, top + right);
    sd(xyz + 0x18, top + left);
    gs_quad_packet(xyz, uvs, colours, effect_tex0(lw(look(0))), 1);
    sub_002a0ba8(0x47, 0x5360b);
    ee_stack_free(0xb0);
}

// orig: L6:0x0030bef8 L18:0x002fb5e0
uint32_t flash_spawn(uint32_t pos, int32_t frames, uint32_t a, uint32_t b) {
    const Vec4 at = V(pos);
    const uint32_t m = moby_spawn(kFlashClass);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    V(m + 0x10) = at;
    sw(pv + 0xc, b);
    sw(pv + 8, a);
    swc1(pv + 4, fpu_div(2.0f, int_to_float(frames)));
    sb(m + 0x20, 1);
    sb(m + 0x30, 0xff);
    return 1;
}

// orig: L6:0x00316330 L18:0x00306338
void camera_type12_flag(uint32_t cam) {
    if (lh(cam + 0x86) != 0x12) return;
    const uint32_t rec = lwu(OV(0x0015ef50)) + (static_cast<uint32_t>(static_cast<int32_t>(lh(cam + 0x84))) << 5);
    sw(lwu(rec + 0x1c) + 0x50, 1);
}

// orig: L6:0x003047f8
uint32_t state_query_blarg(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 7) return 2;
    return 3 < state ? 1 : 0;
}

// orig: L18:0x002dfba0
uint32_t state_query_finale(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    if (state == 2) return 1;
    return state == 3 ? 2 : 0;
}

}  // namespace rac1
