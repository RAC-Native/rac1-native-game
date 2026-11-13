// The background ships of Aridia (level 2), Gaspar (9) and Orxon (10): the ship of class 1213 (1973 on Orxon) that flies
// its path, puffs exhaust and glows, and blows up into three pieces of debris when it is hit; with the helpers both kinds of
// ship share (the glow's draw request, the exhaust). The three levels' copies are the same code, relinked: what differs is
// the class's tuning words (level data, a per-level base below) and the address of the glow's draw callback. The engine's
// words every overlay moves the same way go through OV() (the paths table 0x1b04b0, the frame counter 0x15f5cc, the zero
// vector 0x15f580); the base ELF's are bare (the game's level number 0x15ed84, the frame time 0x15ed6c / 0x15ed60 and its
// square 0x15ed70, the hero 0x1413d4 / 0x1413f4, the sound flags 0x13d408 ..). Header shared_bg_ships.h.
#include <game/ovl/shared_bg_ships.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
// the game's level number (10: Orxon, where the ships fly slower and loop their path; 9: Gaspar)
int32_t game_level() { return lw(0x0015ed84); }

// The ship's tuning words (the class's level data): its speed +0, the glow's turn rate +4 (degrees a second), the glow's two
// colours +8 / +0xc, the debris' push +0x10 (along its velocity) / +0x14 (up); the exhaust's: its speeds and scatter +0x18,
// its frame counts, spread and ranges +0x24, its colours +0x44; Orxon's slower exhaust's speeds +0x54 and colours +0x60.
uint32_t ship2_tuning() {
    if (on_level(GASPAR)) return L9_ADDR(0x00161d2c);
    if (on_level(ORXON)) return L10_ADDR(0x0016205c);
    return L2_ADDR(0x00161fc0);
}

// the ship's glow draw callback (each level's own copy of it)
uint32_t ship2_glow_draw() {
    if (on_level(GASPAR)) return L9_ADDR(0x00306570);
    if (on_level(ORXON)) return L10_ADDR(0x002eb958);
    return L2_ADDR(0x002ecc68);
}
}  // namespace

// orig: L2:0x002ec3f0 L2:0x002ecd50 L9:0x00305cf8 L9:0x00306658 L10:0x002eba40 (ship 1: the points 3 .. 5; ship 2: 2 .. 7; each kind's callback)
void bg_ship_glow_prep(uint32_t m, uint32_t first, uint32_t points, uint32_t draw) {
    if (lbu(m + 0x21) == 0xff) return;  // (not drawn)
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t i = 0; i < points; ++i) moby_chain_point_v(m, first + i, V(pv + 0x80 + i * 0x10));
    const uint32_t colour = pv + 0x80 + points * 0x10;
    const uint32_t frame = lwu(OV(0x0015f5cc));
    sw(colour, 0x30000000u | (lbu(m + 0x92) << 16) | (lbu(m + 0x91) << 8) | lbu(m + 0x90));
    if (frame == 0) return;
    const uint32_t last = lwu(colour + 4);  // (where the frame of the last request is kept)
    if (lwu(last) == frame) return;
    sw(last, frame);
    sub_001f93e0(draw, m);
}

// orig: L2:0x002eca30 L2:0x002ed420 L2:0x002ed658 L9:0x00306338 L9:0x00306d28 L9:0x00306f60 L10:0x002ec110 L10:0x002ec348
void bg_ship_exhaust(uint32_t m, uint32_t from, int32_t count, uint32_t scales, uint32_t timing, uint32_t colours, bool slowed) {
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t fast = sp + 0x10, slow = sp + 0x20, moved = sp + 0x30, scatter = sp + 0x40, at = sp + 0x50;
    const float one_and_half = 1.5f;
    vec3_sub(V(moved), V(m + 0x10), V(from));
    for (int32_t i = 0; i < count; ++i) {
        moby_chain_point_v(m, static_cast<uint32_t>(i), V(at));
        vec3_scale(V(fast), V(moved), lwc1(scales));
        vec3_scale(V(slow), V(moved), lwc1(scales + 4));
        rand_sphere_point(scatter, 0.0f, fpu_mul(lwc1(scales + 8), lwc1(0x0015ed6c)));
        vec3_add(V(slow), V(slow), V(scatter));
        float size0 = rand_range_f(lwc1(timing + 0x10), lwc1(timing + 0x14));
        if (slowed) size0 = fpu_div(size0, one_and_half);
        float size1 = rand_range_f(lwc1(timing + 0x18), lwc1(timing + 0x1c));
        if (slowed) size1 = fpu_div(size1, one_and_half);
        swc1(fast + 0xc, size0);
        swc1(slow + 0xc, size1);
        const uint32_t colour0 = rgba_lerp(lwu(colours), lwu(colours + 4), rand_range_f(0.0f, 1.0f));
        const uint32_t colour1 = rgba_lerp(lwu(colours + 8), lwu(colours + 0xc), rand_range_f(0.0f, 1.0f));
        const float life_r = rand_range_f(0.0f, 1.0f);
        const int32_t life = float_to_int(frame_scale_f(fpu_add(fpu_mul(int_to_float(lw(timing)), life_r), 1.0f)));
        const float spread = lwc1(timing + 0xc);
        const float grow_r = fpu_add(rand_range_f(-spread, spread), 1.0f);
        const int32_t grow = float_to_int(frame_scale_f(fpu_mul(int_to_float(lw(timing + 4)), grow_r)));
        const float fade_r = fpu_add(rand_range_f(-spread, spread), 1.0f);
        const int32_t fade = float_to_int(frame_scale_f(fpu_mul(int_to_float(lw(timing + 8)), fade_r)));
        const uint32_t p = spawn_particle_02(at, fast, slow, colour0, colour1, static_cast<uint32_t>(life), static_cast<uint32_t>(grow),
                                             static_cast<uint32_t>(fade), -1);
        if (p != 0) sb(p + 9, static_cast<uint32_t>(float_to_int(8.0f) + 0x60));
    }
    ee_stack_free(0x120);
}

// The ship (pvars: +0x60 its path's index in the paths table, +0x64 its place on the path, +0x68 the glow's phase, +0x6c its
// engine sound, +0x70 the path's first leg's length, +0x74 its speed factor). State 0 puts it at its path's start, 1 flies
// it (the exhaust while drawn; a hit with damage ends it), 2 blows it up (on Aridia a third and on Gaspar a fifth explosion
// with the hero in state 0x32 also plays a sound once).
// orig: L2:0x002ece18 L9:0x00306720 L10:0x002ebb08
void bg_ship2_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t paths = OV(0x001b04b0);
    const uint32_t path = lwu(paths + (lwu(pv + 0x60) << 2));  // (read before the index is checked, as the original does)
    const uint32_t tune = ship2_tuning();
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t old_pos = sp + 0x10;  // (also the debris' velocity)
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    bool retire = false;
    switch (lbu(m + 0x20)) {
    case 0: {
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        if (game_level() == 10) swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), 0.5f));
        const int32_t index = lw(pv + 0x60);
        if (index == -1) {
            debug_printf_stub();  // (the ship's moby has no path)
            retire = true;
            break;
        }
        const uint32_t own = lwu(paths + (static_cast<uint32_t>(index) << 2));
        if (lwu(own) == 0) {
            debug_printf_stub();  // (an empty path)
            retire = true;
            break;
        }
        const float leg = vec3_distance(V(own + 0x10), V(own + 0x20));
        sb(pv + 0x2c, 0x28);
        swc1(pv + 0x70, leg);
        sb(pv + 0x2b, 1);
        V(m + 0x10) = V(lwu(paths + (lwu(pv + 0x60) << 2)) + 0x10);  // (the 16 bytes as they are)
        swc1(pv + 0x68, rand_angle());
        sw(pv + 0x6c, 0xffffffffu);
        sb(m + 0x20, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x10u);
        swc1(pv + 0x64, fpu_mul(lwc1(pv + 0x74), int_to_float(lw(path))));
        break;
    }
    case 1: {
        if (lbu(0x001413f4) == 1 && game_level() == 10) {
            retire = true;
            break;
        }
        V(old_pos) = V(m + 0x10);
        if (sub_0028d8c0(m, lw(pv + 0x6c)) == 0) sw(pv + 0x6c, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        float step = fpu_mul(lwc1(tune), lwc1(0x0015ed6c));
        if (game_level() == 10) step = fpu_mul(step, 0.25f);
        float t = fpu_add(lwc1(pv + 0x64), fpu_div(step, lwc1(pv + 0x70)));
        swc1(pv + 0x64, t);
        const float points = int_to_float(lw(path));
        if (points < t) swc1(pv + 0x64, fpu_sub(t, points));
        const bool loops = static_cast<uint32_t>(game_level() - 9) < 2;  // (Gaspar and Orxon)
        path_point(lwc1(pv + 0x64), path, loops ? 1 : 0, m + 0x10, m + 0x40, 0);
        if (lwc1(m + 0x10) < 8.0f || lwc1(m + 0x14) < 8.0f) sw(m + 0x94, 0);
        else sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        if (lbu(m + 0x31) != 0) {
            if (game_level() == 10) bg_ship_exhaust(m, old_pos, 2, tune + 0x54, tune + 0x24, tune + 0x60, true);
            else bg_ship_exhaust(m, old_pos, 2, tune + 0x18, tune + 0x24, tune + 0x44, false);
        }
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        break;
    }
    case 2: {
        if (game_level() == 2) {  // (Aridia: the third ship blown up plays the sound once)
            const int32_t n = lw(0x0015edf4) + 1;
            sw(0x0015edf4, static_cast<uint32_t>(n));
            if (!(n < 3) && lbu(0x0013d40a) == 0) {
                sb(0x0013d40a, 1);
                sound_play_indexed(1, 0, 0);
                sub_00263d40(0x53d6, -1);
            }
        }
        if (game_level() == 9 && lw(0x001413d4) == 0x32) {  // (Gaspar, the hero in state 0x32: the fifth)
            const int32_t n = lw(0x0015edfc) + 1;
            sw(0x0015edfc, static_cast<uint32_t>(n));
            if (!(n < 5) && lbu(0x0013d417) == 0) {
                sb(0x0013d417, 1);
                sound_play_indexed(1, 0, 0);
                sub_00263d40(0x53d6, -1);
            }
        }
        moby_sound_start(0, 0, m);
        const uint32_t zero = OV(0x0015f580);
        vec3_scale(V(old_pos), V(m + 0xc0), fpu_mul(lwc1(tune + 0x10), lwc1(0x0015ed60)));
        swc1(old_pos + 8, fpu_add(lwc1(old_pos + 8), fpu_mul(lwc1(tune + 0x14), lwc1(0x0015ed60))));
        for (uint32_t cls : {0x5f6u, 0x5f8u, 0x5f9u}) {
            const int32_t life = frame_scale_i(0x5a);
            debris_spawn(m, cls, m + 0x10, m + 0x40, life, 0, old_pos, zero, zero, fpu_mul(lwc1(0x0015ed70), 12.0f));
        }
        retire = true;
        break;
    }
    default:
        break;
    }
    if (retire) {
        moby_retire(m);
    } else if (lbu(m + 0x31) != 0) {  // (drawn: the glow)
        float glow = fpu_sub(fpu_mul(sin_vu(lwc1(pv + 0x68)), 4.0f), 3.0f);
        if (1.0f < glow) glow = 1.0f;
        else if (glow < 0.0f) glow = 0.0f;
        sw(m + 0x90, rgba_lerp(lwu(tune + 8), lwu(tune + 0xc), glow));
        const float turn = fpu_mul(fpu_mul(lwc1(tune + 4), u2f(0x3c8efa35u)), lwc1(0x0015ed6c));  // (degrees to radians)
        swc1(pv + 0x68, angle_add(lwc1(pv + 0x68), turn));
        bg_ship_glow_prep(m, 2, 6, ship2_glow_draw());
    }
    ee_stack_free(0xb0);
}

}  // namespace rac1
