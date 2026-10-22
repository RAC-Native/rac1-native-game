// Ported overlay code: the gameplay frame (0x297f78) and the overlay's empty debug hooks.
//. Callees not ported (or not yet proven with the oracle) are TODO_PORT.
#include <game/enhance/advanced.h>
#include <game/ovl/memory_card.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/arrival.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/port_stubs.h>
#include <profiler/host_profiler.h>

namespace rac1 {

// orig: 0x001f6d98 — profiler marker (empty in this build).
void prof_mark(uint32_t, int32_t) {}
// orig: 0x001f6bf8 — debug view 2 (empty in this build).
void debug_view_2() {}
// orig: 0x001f6c00 — debug view 3 (empty in this build).
void debug_view_3() {}
// orig: 0x001f6c08 — debug view 4 (empty in this build).
void debug_view_4() {}
// orig: 0x001f6c10 — debug view 5 (empty in this build).
void debug_view_5() {}
// orig: 0x001f6c18 — debug view 6 (empty in this build).
void debug_view_6() {}

namespace {
constexpr OvConst<0x0016c058u> C{};      // the frame's control block (+8 enable bits, +0x14 cutscene, +0x18 debug view)
constexpr uint32_t PAD = 0x0013c940;    // pad 1 (+0x1a0 held, +0x1a4 pressed, +0x1a8 released)
constexpr uint32_t P = 0x0013f350;      // the player
// enhancement: the world held still this frame (Advanced > pause in the quick select)
bool g_world_paused = false;
// orig: (helper) a subsystem bit of C+8 is on (or, with bit 0x10, single-stepped by R1 = 0x200)
bool on(uint32_t bit) {
    if (g_world_paused && (bit & 0xf) != 0) return false;  // enhancement: hero, mobys, particles, camera
    const uint32_t f = lwu(C + 8);
    return (f & bit) != 0 || ((f & 0x10) != 0 && (lwu(PAD + 0x1a4) & 0x200) != 0);
}
// orig: (helper) the play-time statistic of a level: uses +0 (to 0xffff), longest +2 (in
// 600-frame units of 0x15eea4), level bit +4.
void level_stat(uint32_t s) {
    if (!(0xfffeu < lhu(s))) sh(s, lhu(s) + 1u);
    const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
    if (lhu(s + 2) < q) sh(s + 2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
    sw(s + 4, lwu(s + 4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
}
}  // namespace

// orig: 0x00297f78 — one gameplay frame: the pause / menu checks (start, select, the vendor and
// the ship), the pad's single step, the level updates (tfrag, mobys, HUD bar, effects), the
// hero, the camera, the HUD and the end-of-frame passes, each gated by its bit of C+8.
void gameplay_frame() {
    RAC1_PROF_ZONE(kGameLogic);  // (host: the profiler's sub-stages of the game code, nothing of the game's)
    advanced_frame();
    g_world_paused = false;
    if (lw(C + 0xec) != 0) {
        const int32_t v = lw(C + 0xec) - 1;
        sw(C + 0xec, static_cast<uint32_t>(v));
        if (v == 0) sw(C + 8, 0x10);
    }
    if ((lwu(C + 8) & 0x10) != 0 && (lwu(PAD + 0x1a4) & 0x200) != 0) {
        const uint32_t held = lwu(PAD + 0x1a0), prev = lwu(C + 0xa0);
        sw(PAD + 0x1a8, ~held & prev);
        sw(C + 0xa0, held & ~0x200u);
        sw(PAD + 0x1a4, ~prev & held);
    }
    auto menu = [](int32_t which) { screen_open(which, 0); };
    auto screen = [](int32_t which) { menu_open(which); };
    const int32_t state = lw(OV(0x0015f5c4));
    if (state == 0 && (lw(0x0015eeb4) & 1) != 0 && !(lw(OV(0x0015f5c8)) < 8)) {
        menu(3);
        return;
    }
    const bool no_pad = lw(PAD + 0x19c) == 0;
    if (lw(OV(0x001b9de0)) != 0 && state == 0) {
        screen(0);
        return;
    }
    const int32_t level = lw(0x0015ed84);
    auto pressed = [](uint32_t m) { return (lwu(PAD + 0x1a4) & m) != 0; };
    auto cam_type = [] { return lh(lwu(OV(0x00166e00)) + 0x86); };
    if (level == 0xf && !(lw(OV(0x0015f5c8)) < 8) && lbu(P + 0x20a4) == 2 && (pressed(0x800) || no_pad)) {
        if (lw(P + 0x2084) != 0x72) {
            if (cam_type() == 0x13) return;
            menu(4);
            return;
        }
    }
    bool vendor = false;
    if (lw(P + 0x208c) == 9 && lw(P + 0x2084) == 0x32 && lwu(P + 0x15f0) != 0) {
        const int32_t cls = lh(lwu(P + 0x15f0) + 0xa6);
        if (cls == 0x45 || cls == 0x563 || cls == 0x4da) vendor = true;
    }
    if (vendor && !(lw(OV(0x0015f5c8)) < 8) && (pressed(0x800) || no_pad) && lw(P + 0x2084) != 0x72 && cam_type() != 0x13) {
        menu(1);
        return;
    }
    if (state == 0 && (level == 8 || level == 0xc) && (pressed(0x800) || no_pad) && lw(P + 0x2084) == 0x32) {
        menu(1);
        return;
    }
    if ((pressed(0x800) || no_pad) && !(lw(OV(0x0015f5c8)) < 8) && lw(P + 0x2084) != 0x72 && lw(P + 0x208c) == 0x16) {
        menu(0);
        return;
    }
    if ((pressed(0x800) || no_pad) && !(lw(OV(0x0015f5c8)) < 8)) {
        const int32_t hs = lw(P + 0x2084);
        if (hs != 0x72 && lbu(P + 0x10ac) != 2 && hs != 0x32 && hs != 0x1d && lbu(P + 0x20b1) == 0 && lw(P + 0x22a8) != 0 && state == 0) {
            screen(0);
            return;
        }
    }
    if ((lwu(C + 8) & 0x10) == 0 && !(lw(OV(0x0015f5c8)) < 8) && (lwu(PAD + 0x1a4) & 0x500) != 0) {
        const int32_t hs = lw(P + 0x2084);
        if (lw(P + 0x208c) != 0x16 && hs != 0x72 && hs != 0x32 && hs != 0x1d && lbu(P + 0x20b1) == 0 && lw(P + 0x22a8) != 0 && state == 0 &&
            !vendor && lw(C + 0xec) == 0) {
            screen(0xa);
            return;
        }
    }
    if (lw(0x0013d384) != 0 && lbu(0x0013d398) == 0 && !(lw(OV(0x0015f5c8)) < 8) && lw(0x0015ee20) == 0 && lbu(P + 0x20b1) == 0 &&
        lw(P + 0x22a8) != 0 && frame_scale_i(0x1e) < lw(OV(0x0015f5cc)) && lw(0x0015ed84) != 0) {
        menu(5);
        sb(0x0013d398, 1);
        return;
    }
    {
        const int32_t hs = lw(P + 0x2084);
        if (hs != 0x1d && hs != 0x32) help_hint_update();
        const int32_t hs2 = lw(P + 0x2084);
        if (pressed(0x200) && hs2 != 0x1d && hs2 != 0x32) {
            const int32_t k = camera_request_7535();
            sub_00235e18(static_cast<uint32_t>(k), frame_scale_i(0xb4));
            const int32_t k2 = camera_request(2, 0x754e, OVF(0x00239cc8), OVF(0x00239d00), OVF(0x00239df8), 0x0015ed98, 0x0098967f);
            sub_00235e18(static_cast<uint32_t>(k2), frame_scale_i(0xb4));
        }
    }
    g_world_paused = advanced_world_paused();  // enhancement: Advanced > pause in the quick select
    if (lw(OV(0x00184214)) != 0 && !g_world_paused) level_stat(0x00141890);
    if (lw(OV(0x0015f630)) != 0) {
        sw(OV(0x0015f630), 0);
        if (lw(P + 0x22a8) != 0) {
            ship_takeoff_start();
            return;
        }
    }
    if (on(2)) draw_lists_reset();
    if (on(2)) {
        RAC1_PROF_ZONE(kGameMobyUpdates);
        moby_free_slots_count();
        moby_updates();
        sound_emitters_update();
        wind_update();
    }
    prof_mark(OV(0x00160ff8), 3);
    advanced_aim_before_hero();  // enhancement: the modern controls' aim
    if (on(1)) {
        RAC1_PROF_ZONE(kGameHero);
        hero_frame_update();
    }
    if (on(1)) advanced_aim_after_hero();
    prof_mark(OV(0x00161008), 7);
    if (on(4)) {
        RAC1_PROF_ZONE(kGameParticles);
        particles_update();
        spin_timers_update();
    }
    prof_mark(OV(0x00161018), 5);
    advanced_aim_before_camera();  // enhancement: the modern controls' aim
    if (on(8)) {
        RAC1_PROF_ZONE(kGameCamera);
        camera_update();
    } else if (lw(C + 0x14) != 0 && !g_world_paused) {
        TODO_PORT(0x001f3aa0, "cutscene camera")();
        if (lw(C + 0x14) == 2) TODO_PORT(0x001f4490, "cutscene camera target")();
    }
    advanced_aim_after_camera();
    prof_mark(OV(0x00161028), 8);
    sound_frame();
    prof_mark(OV(0x00161038), 6);
    level_lights_update();
    shadow_direction_update();
    hud_update(1);
    help_update();
    if (on(2)) frame_counters_advance();
    const int32_t dv = lw(C + 0x18);
    if (dv == 2) debug_view_2();
    if (lw(C + 0x18) == 3) debug_view_3();
    if (lw(C + 0x18) == 4) debug_view_4();
    if (lw(C + 0x18) == 5) debug_view_5();
    if (lw(C + 0x18) == 6) debug_view_6();
}

// The help timer 0x15f590 (its end clears 0x15f594); while help is on and allowed (0x161e08)
// the help camera request 0x161e04 is made (id 0xc) or kept alive (10 frames).
// orig: 0x00264328
void help_hint_update() {
    if (lw(OV(0x0015f590)) != 0) {
        const int32_t v = lw(OV(0x0015f590)) - 1;
        sw(OV(0x0015f590), static_cast<uint32_t>(v));
        if (v == 0) sw(OV(0x0015f594), 0);
    }
    if (lw(OV(0x0015f594)) == 0 || lw(OV(0x00161e08)) == 0) {
        sw(OV(0x00161e04), 0xffffffffu);
    } else if (lw(OV(0x00161e04)) == -1) {
        sw(OV(0x00161e04), static_cast<uint32_t>(camera_request(0xc, 0, OVF(0x00237190), OVF(0x002371e0), OVF(0x00237200), 0, 0)));
    } else {
        sub_00235e18(lwu(OV(0x00161e04)), 10);
    }
}

// The frame's draw lists emptied (their counts 0x15f424..0x15f434, 0x17e504, 0x15f2f0 / 0x15f2f4).
// orig: 0x00297e30
void draw_lists_reset() {
    sw(OV(0x0015f424), 0);
    sw(OV(0x0017e504), 0);
    sw(OV(0x0015f42c), 0);
    sw(OV(0x0015f430), 0);
    sw(OV(0x0015f428), 0);
    sw(OV(0x0015f434), 0);
    sw(OV(0x0015f2f0), 0);
    sw(OV(0x0015f2f4), 0);
}

// 0x15ffbc: the moby slots from the spawned ones (0x15ffdc) on that are free again (dead, their
// reuse frame +0x38 passed), counting every slot after the first such once one is found.
// orig: 0x0024e7a8
void moby_free_slots_count() {
    uint32_t m = lwu(OV(0x0015ffdc));
    bool found = false;
    sw(OV(0x0015ffbc), 0);
    if (!(m < lwu(OV(0x0015ffe0)))) return;
    const uint64_t now = lwu(OV(0x0015f5cc));
    const uint32_t end = lwu(OV(0x0015ffe0));
    do {
        const uint32_t st = lbu(m + 0x20);
        if ((st >= 0xfe && !(now < ld(m + 0x38))) || found) {
            sw(OV(0x0015ffbc), lwu(OV(0x0015ffbc)) + 1);
            if (lbu(m + 0x20) == 0xff) found = true;
        }
        m += 0x100;
    } while (m < end);
}

// 16 spinning markers 0x16ea40 (0x20 each): frames left +0x10, height +0x12 (up 5 a frame in the
// first part, down in the last 12), angle +0x18 (turning 5.24 rad/s); cleared when done.
// orig: 0x001fea18
void spin_timers_update() {
    for (uint32_t i = 0; i < 16; ++i) {
        const uint32_t e = OV(0x0016ea40) + i * 0x20;
        if (lh(e + 0x10) <= 0) continue;
        swc1(e + 0x18, angle_sub(lwc1(e + 0x18), lwc1(0x0015ed6c) * u2f(0x40a78d36u)));
        if (lh(e + 0x10) < 0xd) sh(e + 0x12, lhu(e + 0x12) - 5u);
        else sh(e + 0x12, lhu(e + 0x12) + 5u);
        const uint32_t n = lhu(e + 0x10) - 1u;
        sh(e + 0x10, n);
        if (static_cast<int32_t>(n << 16) <= 0) mem_fill32(e, 0, 0x20);
    }
}

// The shadows' direction 0x1aeb80: 0.14 along the angle of the hero's region colour (0x1413d0),
// -0.99 down.
// orig: 0x0024fba8
void shadow_direction_update() {
    Vec4 c = Vec4{};
    moby_palette_colour_1_v(lwu(0x001413d0), c);
    const float a = atan2_f(c.x, c.y);
    const float k = u2f(0x3e0f5c29u);
    swc1(OV(0x001aeb80), cos_vu(a) * k);
    const float s = sin_vu(a) * k;
    swc1(OV(0x001aeb88), u2f(0xbf7d70a4u));
    swc1(OV(0x001aeb84), s);
}

// The frame's end: the idle frames 0x160ff0 (no button, sticks centred), the frame counters
// 0x15f5cc and 0x15eea4 (+1), and with 0x15f598 the active play time 0x15ee28 (idle under 15 s).
// orig: 0x00297e70
void frame_counters_advance() {
    const uint32_t pad = 0x0013c940;
    if (lw(pad + 0x1a0) != 0 || lwc1(pad + 0x108) != 0.0f || lwc1(pad + 0x10c) != 0.0f || lwc1(pad + 0x100) != 0.0f || lwc1(pad + 0x104) != 0.0f) {
        sw(OV(0x00160ff0), 0);
    } else {
        sw(OV(0x00160ff0), lwu(OV(0x00160ff0)) + 1);
    }
    sw(OV(0x0015f5cc), lwu(OV(0x0015f5cc)) + 1);
    sw(0x0015eea4, lwu(0x0015eea4) + 1);
    if (lw(OV(0x0015f598)) != 0) {
        const int32_t lim = static_cast<int32_t>(static_cast<float>(frame_scale_i(0xf)) * 60.0f);
        if (lw(OV(0x00160ff0)) < lim) sw(0x0015ee28, lwu(0x0015ee28) + 1);
    }
}

}  // namespace rac1
