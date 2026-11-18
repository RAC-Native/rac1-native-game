// Pokitaru's (level 11) sewer set pieces, ported by hand from the level's own code: class 1157 (the iris gate and its
// camera sweep, with its blade helper L11:0x30dd18), class 1158 (the sewer's rising water tiles; 1.02 Fixes, fix 35,
// always on the PAL 2.00 disc), class 1159 (a sliding plug and its twin), class 1178 (a tilting platform) and class 1180
// (a swinging valve). The original's order of calls and float operations kept.
#include <game/ovl/pokitaru_own.h>

#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/hydro.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_tiles.h>
#include <game/ovl/variants_l11.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t kHeroPos = 0x0013f3d0u;  // the hero's position (0x13f350 + 0x80)
float dt() { return lwc1(0x0015ed6cu); }
float dt2() { return lwc1(0x0015ed70u); }
uint32_t moby_at(uint32_t index) { return lwu(L11_ADDR(0x0015ffd8u)) + (index << 8); }
}  // namespace

// Places the twenty-four blades (four groups of six, class 0x4b7, kept at pvar +0 .. +0x5f) around the iris gate `m`
// according to its opening progress (pvar +0x6c, 0 .. 1).
// orig: L11:0x0030dd18
void pok_30dd18_l11(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t pv = lwu(m + 0x78);
    vec3_set_length(V(sp), V(m + 0xc0), lwc1(L11_ADDR(0x00161f40u)));
    vec3_set_length(V(sp + 0x10), V(m + 0xd0), lwc1(L11_ADDR(0x00161f44u)));
    const float open = lwc1(pv + 0x6c);
    float deg_front = fpu_sub(90.0f, fpu_mul(open, 180.0f));
    if (deg_front < -90.0f) deg_front = -90.0f;
    float deg_back = fpu_sub(90.0f, fpu_mul(fpu_sub(open, 0.5f), 180.0f));
    if (90.0f < deg_back) deg_back = 90.0f;
    else if (deg_back < 0.0f) deg_back = 0.0f;

    const float deg_to_rad = u2f(0x3c8efa35u);
    const float neg_qtr_pi = u2f(0xbf490fdbu);
    const float half_pi = u2f(0x3fc90fdbu);
    const float qtr_pi = u2f(0x3f490fdbu);
    const float pi = u2f(0x40490fdbu);
    const float step_rad = u2f(0x3e860a92u);
    for (int32_t g = 0; g < 4; ++g) {
        const bool front = g < 2;
        const bool odd = (g & 1) != 0;
        uint32_t slot = pv + static_cast<uint32_t>(g * 0x18);
        for (int32_t k = 0; k < 6; ++k, slot += 4) {
            const float deg = front ? deg_front : deg_back;
            const uint32_t b = lwu(slot);
            if (front) vec3_add(V(b + 0x10), V(m + 0x10), V(sp));
            else vec3_sub(V(b + 0x10), V(m + 0x10), V(sp));
            float off = fpu_mul(fpu_sub(int_to_float(k), 2.5f), step_rad);
            if (odd) {
                swc1(b + 0x48, angle_add(lwc1(m + 0x48), 0.0f));
                const uint32_t b1 = lwu(slot);
                swc1(b1 + 0x48, lwc1(b + 0x48));
                off = angle_add(off, fpu_mul(deg, deg_to_rad));
                if (off < neg_qtr_pi) off = fpu_add(off, half_pi);
            } else {
                swc1(b + 0x48, angle_add(lwc1(m + 0x48), pi));
                const uint32_t b1 = lwu(slot);
                swc1(b1 + 0x48, lwc1(b + 0x48));
                off = angle_sub(off, fpu_mul(deg, deg_to_rad));
                if (qtr_pi < off) off = fpu_sub(off, half_pi);
            }
            if (qtr_pi < off) off = qtr_pi;
            else if (off < neg_qtr_pi) off = neg_qtr_pi;
            const uint32_t b2 = lwu(slot);
            swc1(b2 + 0x48, angle_add(lwc1(b2 + 0x48), off));
            const uint32_t b3 = lwu(slot);
            euler_to_mat3_vu(M(b3 + 0xc0), V(b3 + 0x40));
            const uint32_t b4 = lwu(slot);
            vec3_set_length(V(sp + 0x20), V(b4 + 0xd0), -lwc1(L11_ADDR(0x00161f48u)));
            if (odd) vec3_add(V(sp + 0x20), V(sp + 0x20), V(sp + 0x10));
            else vec3_sub(V(sp + 0x20), V(sp + 0x20), V(sp + 0x10));
            const uint32_t b5 = lwu(slot);
            vec3_add(V(b5 + 0x10), V(b5 + 0x10), V(sp + 0x20));
            moby_update_transform(lwu(slot));
        }
    }
    ee_stack_free(0x110);
}

// Class 1157, the sewer iris gate: state 0 spawns its 24 blades (class 0x4b7); 1 waits for its trigger moby (pvar +0x60:
// +0xbc == 1 starts the opening scene in 2, +0xbc == 2 snaps open to 5); 2 fades out/in over 20 frames each way, placing
// the hero at the trigger in state 0x72 and starting a scripted camera at box +0x64; 3 opens the iris (+0x6c -> 1) while
// sweeping the camera from box +0x64 to box +0x68; 4 fades back to gameplay, then 5 (open).
// orig: L11:0x0030d800
void pok_class1157_update_l11(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b4a0)
    case 0:  // (0x0030d864)
        for (int32_t g = 0; g < 4; ++g) {
            uint32_t slot = pv + static_cast<uint32_t>(g * 0x18);
            for (int32_t k = 0; k < 6; ++k, slot += 4) {
                const uint32_t b = moby_spawn(0x4b7);
                sw(slot, b);
                sh(b + 0x32, 0x40);
                const uint32_t b1 = lwu(slot);
                sb(b1 + 0x31, 1);
                const uint32_t b2 = lwu(slot);
                sd(b2 + 0x38, ld(lwu(0x001413d0u) + 0x38));
                const uint32_t b3 = lwu(slot);
                sh(b3 + 0x34, lhu(m + 0x34));
                const uint32_t b4 = lwu(slot);
                lq_sq(b4 + 0x10, m + 0x10);
                lq_sq(b4 + 0x40, m + 0x40);
            }
        }
        sw(pv + 0x6c, 0);
        sb(m + 0x20, 1);
        break;
    case 1: {  // (0x0030d910)
        const uint32_t trig = moby_at(lwu(pv + 0x60));
        const uint8_t bc = lbu(trig + 0xbc);
        if (bc == 1) {
            sb(m + 0x20, 2);
            swc1(pv + 0x78, -1.0f);
            sw(L11_ADDR(0x00161f54u), 0);
            moby_sound_start(0, 0, m);
        } else if (bc == 2) {
            swc1(pv + 0x6c, 1.0f);
            sb(m + 0x20, 5);
        }
        break;
    }
    case 2: {  // (0x0030d97c)
        const float prev = lwc1(pv + 0x78);
        const float step = fpu_div(1.0f, int_to_float(frame_scale_i(0x14)));
        approach_f(pv + 0x78, 1.0f, step);
        swc1(L11_ADDR(0x0015f3fcu), fpu_sub(1.0f, abs_f(lwc1(pv + 0x78))));
        if (prev < 0.0f && 0.0f <= lwc1(pv + 0x78)) {
            const uint32_t trig = moby_at(lwu(pv + 0x60));
            hero_place(trig + 0x10, trig + 0x40, 0x72, 1);
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x64));
            camera_scripted_set(box + 0x30, box + 0x70, 1, 0, 0);
        } else if (1.0f <= lwc1(pv + 0x78)) {
            sb(m + 0x20, 3);
        }
        break;
    }
    case 3: {  // (0x0030da60)
        const float d2 = dt2();
        const float accel = fpu_mul(lwc1(L11_ADDR(0x00161f58u)), d2);
        const float decel = fpu_mul(lwc1(L11_ADDR(0x00161f5cu)), d2);
        const float vmax = fpu_mul(lwc1(L11_ADDR(0x00161f60u)), dt());
        swc1(pv + 0x74, lwc1(pv + 0x6c));
        seek_1d(pv + 0x6c, pv + 0x70, 1.0f, accel, decel, vmax);
        const uint32_t from = lvd::box(lvd::BOXES, lw(pv + 0x64));
        const uint32_t to = lvd::box(lvd::BOXES, lw(pv + 0x68));
        vec3_lerp(V(sp), V(from + 0x30), V(to + 0x30), lwc1(pv + 0x6c));
        vec_clear(V(sp + 0x10));
        for (uint32_t off = 0; off < 12; off += 4) {
            const uint32_t b_to = lvd::box(lvd::BOXES, lw(pv + 0x68));
            const uint32_t b_from = lvd::box(lvd::BOXES, lw(pv + 0x64));
            const float diff = fpu_mul(angle_sub(lwc1(b_to + 0x70 + off), lwc1(b_from + 0x70 + off)), lwc1(pv + 0x6c));
            swc1(sp + 0x10 + off, diff);
            const uint32_t b_base = lvd::box(lvd::BOXES, lw(pv + 0x64));
            swc1(sp + 0x10 + off, angle_add(lwc1(b_base + 0x70 + off), diff));
        }
        uid5_set_pvar_80(sp);
        uid5_set_pvar_90(sp + 0x10);
        if (1.0f <= lwc1(pv + 0x6c)) {
            sb(m + 0x20, 4);
            swc1(pv + 0x78, -1.0f);
        }
        break;
    }
    case 4: {  // (0x0030dc28)
        const float prev = lwc1(pv + 0x78);
        const float step = fpu_div(1.0f, int_to_float(frame_scale_i(0x14)));
        approach_f(pv + 0x78, 1.0f, step);
        swc1(L11_ADDR(0x0015f3fcu), fpu_sub(1.0f, abs_f(lwc1(pv + 0x78))));
        if (prev < 0.0f && 0.0f <= lwc1(pv + 0x78)) {
            hero_place(kHeroPos, kHeroPos + 0x10, 0, 1);
            hydro_2eac18(3);
        } else if (1.0f <= lwc1(pv + 0x78)) {
            sb(m + 0x20, 5);
        }
        break;
    }
    default:  // (0x0030dcd4)
        break;
    }
    pok_30dd18_l11(m);
    ee_stack_free(0xd0);
}

// Class 1158, the sewer's rising water tiles: state 0 assigns the tile's grid slot (0x161f64, up to 128), sets up the
// 20-record water grid (0x1da880 / 0x1da740 / 0x1cb280) on tile 0, and initializes its own grid record; state 1 links
// its neighbours (tile_links) -> 2; state 2 rises toward 137 when triggered (+0xbc & 8); state 3 lowers toward 132.36
// when triggered (+0xbc & 4); both splash at random and tile 0 steps and queues the water grid draw (0x30e708).
// orig: L11:0x0030e088
void pok_class1158_update_l11(uint32_t m) {
    if (fixes102_pokitaru_sewer_tint() && lwc1(L11_ADDR(0x001677c8u)) <= 180.0f) {  // (1.02 Fixes, fix 35: PAL 2.00 0x30f4f8)
        sb(L11_ADDR(0x00161205u), 0x28);
        sb(L11_ADDR(0x00161203u), 0x40);
        sb(L11_ADDR(0x00161206u), 0x30);
        sb(L11_ADDR(0x00161200u), 0);
        sb(L11_ADDR(0x00161201u), 0x28);
        sb(L11_ADDR(0x00161202u), 0x30);
        sb(L11_ADDR(0x00161204u), 0);
    }
    const uint32_t pv = lwu(m + 0x78);
    const uint8_t state = lbu(m + 0x20);
    auto water_move_and_splash = [&](uint8_t trig_bit, uint32_t group_state, uint8_t bc_out, float target_z) {
        if ((lbu(m + 0xbc) & trig_bit) != 0) {
            sw(pv + 8, 1);
            moby_group_set_state(static_cast<int32_t>(lbu(m + 0x21)), group_state);
        }
        sb(m + 0xbc, bc_out);
        if (lw(pv + 8) != 0) {
            const float accel = fpu_mul(dt2(), 4.0f);
            seek_1d(m + 0x18, pv, target_z, accel, accel, fpu_mul(dt(), 4.0f));
            tile_group_height(m);
        }
        if (rand_mod(0xc8) == 0) {
            const float dx = rand_range_f(-4.0f, 4.0f);
            const uint32_t rec0 = L11_ADDR(0x001da880u) + static_cast<uint32_t>(lw(pv + 4) * 0x1190);
            const float sx = fpu_add(lwc1(rec0), dx);
            const float dy = rand_range_f(-4.0f, 4.0f);
            const uint32_t rec1 = L11_ADDR(0x001da880u) + static_cast<uint32_t>(lw(pv + 4) * 0x1190);
            region_height_splash(rec1, 1, 1, sx, fpu_add(lwc1(rec1 + 4), dy), 1.0f, u2f(0xbcf5c28fu));
        }
    };

    if (state == 0) {
        sb(m + 0x30, 0xff);
        const int32_t idx = lw(L11_ADDR(0x00161f64u));
        sw(pv + 4, static_cast<uint32_t>(idx));
        sw(L11_ADDR(0x00161f64u), static_cast<uint32_t>(idx + 1));
        if (!(idx + 1 < 128)) debug_printf_stub();
        sb(m + 0x30, 0xff);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        const float r = lwc1(lwu(m + 0x24) + 0x24);
        swc1(m + 0x2c, fpu_add(r, r));
        if (lw(pv + 4) == 0) {
            for (int32_t k = 19; k >= 0; --k)
                sh(L11_ADDR(0x001da880u) + static_cast<uint32_t>(k * 0x1190) + 0x1eu, 0);
            grid_records_reset(L11_ADDR(0x001da880u), 0x14);
            grid_weights_scale(1.0f);
            const uint32_t cfg = L11_ADDR(0x001cb280u);
            const float dir_x = lwc1(L11_ADDR(0x001808d0u));
            const float dir_y = lwc1(L11_ADDR(0x001808d4u));
            swc1(cfg, 16.0f);
            swc1(cfg + 0xc, -8.0f);
            swc1(cfg + 0x18, u2f(0x3f666666u));
            swc1(cfg + 0x20, u2f(0x3dcccccdu));
            swc1(cfg + 8, -8.0f);
            swc1(cfg + 0x14, 1.0f);
            swc1(cfg + 0x10, 1.0f);
            swc1(cfg + 4, 16.0f);
            const float ang = atan2_f(dir_x, dir_y);
            const float inv_sqrt3 = u2f(0x3f13cd36u);
            swc1(cfg + 0x30, fpu_mul(cos_vu(ang), inv_sqrt3));
            swc1(cfg + 0x34, fpu_mul(sin_vu(ang), inv_sqrt3));
            sb(cfg + 0x3d, 0x40);
            swc1(cfg + 0x38, u2f(0xbf13cd36u));
            sw(cfg + 0x28, 0x38);
            sw(cfg + 0x2c, 0x39);
            sb(cfg + 0x3c, 0x1c);
            sw(L11_ADDR(0x001612d0u), L11_ADDR(0x001da880u));
            sw(L11_ADDR(0x001612d4u), L11_ADDR(0x001da740u));
            sw(L11_ADDR(0x001612d8u), 0x14);
            swc1(L11_ADDR(0x0016120cu), 32768.0f);
            swc1(L11_ADDR(0x00161210u), 255.0f);
            swc1(L11_ADDR(0x00161214u), 48.0f);
            sw(L11_ADDR(0x00161208u), 0);
        }
        const uint32_t base = L11_ADDR(0x001da880u);
        swc1(base + static_cast<uint32_t>(lw(pv + 4) * 0x1190), lwc1(m + 0x10));
        swc1(base + static_cast<uint32_t>(lw(pv + 4) * 0x1190) + 4, lwc1(m + 0x14));
        swc1(base + static_cast<uint32_t>(lw(pv + 4) * 0x1190) + 8, lwc1(m + 0x18));
        sw(base + static_cast<uint32_t>(lw(pv + 4) * 0x1190) + 0x14, 0x38);
        sw(base + static_cast<uint32_t>(lw(pv + 4) * 0x1190) + 0x18, 0x39);
        sb(base + static_cast<uint32_t>(lw(pv + 4) * 0x1190) + 0x1c, 0x1c);
        sb(base + static_cast<uint32_t>(lw(pv + 4) * 0x1190) + 0x1d, 0x44);
        sh(base + static_cast<uint32_t>(lw(pv + 4) * 0x1190) + 0x1e, 0xffffu);
        const int32_t cur = lw(pv + 4);
        grid_point_fixed(base + static_cast<uint32_t>(cur * 0x1190), L11_ADDR(0x001da740u) + (static_cast<uint32_t>(cur) << 4));
        sb(m + 0x20, 1);
    } else if (state == 1) {
        const int32_t idx = lw(pv + 4);
        sw(L11_ADDR(0x00161f64u), 0);
        tile_links(idx);
        sb(m + 0x20, 2);
    } else if (state == 2) {
        water_move_and_splash(8u, 3u, 2u, 137.0f);
    } else if (state == 3) {
        water_move_and_splash(4u, 2u, 1u, u2f(0x43045c29u));
    }

    if (lw(pv + 4) >= 0) {
        const bool vis = moby_on_screen_l1(m, 64.0f) != -1;
        sh(L11_ADDR(0x001da880u) + static_cast<uint32_t>(lw(pv + 4) * 0x1190) + 0x1eu, vis ? 0xffffu : 0u);
        if (lw(pv + 4) == 0) {
            nov_water_grid_update_l1(L11_ADDR(0x001da880u), 0x14);
            sub_001f93e0(L11_ADDR(0x0030e708u), m);
        }
    }
}

// Class 1159, a sliding plug and its twin: state 0 lifts `m` by 1.9 x pvar +0xc, spawns its twin (class 0x487) flipped
// by pi around x, copies its pvars +8 / +0x10 / +0x14 and enters 1; state 1 waits for its linked moby (pvar +8: class
// 0x267 in state 4, or class 0x33e with +0xbc set) to set the level byte 0x13d388 + 0x57 + pvar +0x14, play sound 0 and
// enter 2; state 2 slides along its -up axis (+0xd0) by seek_1d toward pvar +0x10.
// orig: L11:0x0030e978
void pok_class1159_update_l11(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(pv + 0xc), u2f(0x3ff33333u))));
        const uint32_t twin = moby_spawn(0x487);
        sb(twin + 0x31, 1);
        sh(twin + 0x32, 0x40);
        sd(twin + 0x38, ld(m + 0x38));
        sh(twin + 0x34, lhu(m + 0x34));
        lq_sq(twin + 0x10, m + 0x10);
        lq_sq(twin + 0x40, m + 0x40);
        swc1(twin + 0x40, angle_add(lwc1(twin + 0x40), u2f(0x40490fdbu)));
        moby_update_transform(twin);
        const uint32_t tpv = lwu(twin + 0x78);
        sw(tpv + 8, lwu(pv + 8));
        swc1(tpv + 0x10, lwc1(pv + 0x10));
        sw(tpv + 0x14, lwu(pv + 0x14));
        const uint32_t cls = lwu(m + 0x24);
        swc1(m + 0x2c, fpu_mul(lwc1(cls + 0x24), lwc1(pv + 0xc)));
        sb(twin + 0x20, 1);
        swc1(twin + 0x2c, fpu_mul(lwc1(cls + 0x24), lwc1(pv + 0xc)));
        sb(m + 0x20, 1);
        break;
    }
    case 1: {
        const uint32_t link = moby_at(lwu(pv + 8));
        const int16_t lcls = lh(link + 0xa6);
        if ((lcls == 0x267 && lbu(link + 0x20) == 4) || (lcls == 0x33e && lbu(link + 0xbc) != 0)) {
            sb(m + 0x20, 2);
            sb(0x0013d388u + lwu(pv + 0x14) + 0x57u, 1);
            moby_sound_start(0, 0, m);
        }
        break;
    }
    case 2: {
        const float d = dt(), d2 = dt2();
        seek_1d(pv, pv + 4, lwc1(pv + 0x10), d2, d2, fpu_add(d, d));
        vec3_set_length(V(sp), V(m + 0xd0), -lwc1(pv + 4));
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp));
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x60);
}

// Class 1178, a tilting platform: damps and restores its pitch velocity (pvar +0x60); while the hero stands on it
// (0x13f64c == m, not busy 0x13f65e) his offset along its forward axis tilts it further and knocks him off (hero state
// 6) past its tilt limit (0x161f84 degrees); updates its attachment frame (pvar +0x20).
// orig: L11:0x0030eb90
void pok_class1178_update_l11(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t pv = lwu(m + 0x78);
    vec3_scale(V(sp), V(m + 0x10), -1.0f);
    lq_sq(sp + 0x10, m + 0x40);
    const float deg = u2f(0x3c8efa35u);
    sw(pv + 0x5c, 1);
    const float d2 = dt2();
    const float restore = fpu_mul(fpu_mul(fpu_mul(lwc1(L11_ADDR(0x00161f7cu)), deg), d2),
                                  fpu_mul(lwc1(L11_ADDR(0x00161f6cu)), lwc1(m + 0x44)));
    const float vel0 = lwc1(pv + 0x60);
    const float damp = fpu_mul(fpu_mul(fpu_mul(lwc1(L11_ADDR(0x00161f80u)), deg), d2),
                               fpu_mul(lwc1(L11_ADDR(0x00161f70u)), vel0));
    swc1(pv + 0x60, fpu_add(fpu_add(vel0, restore), damp));

    if (lwu(0x0013f64cu) == m && lh(0x0013f65eu) == 0) {
        vec3_sub(V(sp + 0x60), V(kHeroPos), V(m + 0x10));
        mat3_transpose(M(sp + 0x20), M(m + 0xc0));
        mat4_transform(V(sp + 0x60), V(sp + 0x60), M(sp + 0x20));
        const float rate = fpu_mul(fpu_mul(lwc1(L11_ADDR(0x00161f78u)), deg), dt2());
        float push = fpu_mul(rate, fpu_mul(lwc1(L11_ADDR(0x00161f68u)), lwc1(sp + 0x60)));
        if (fpu_mul(lwc1(pv + 0x60), push) < 0.0f && fpu_mul(lwc1(m + 0x44), push) < 0.0f) {
            push = fpu_add(push, push);
        }
        const float bias = fpu_mul(fpu_mul(lwc1(L11_ADDR(0x00161f74u)), u2f(0x3c8efa35u)), dt2());
        push = push < 0.0f ? fpu_sub(push, bias) : fpu_add(push, bias);
        swc1(pv + 0x60, fpu_add(lwc1(pv + 0x60), push));
        const float limit = fpu_mul(lwc1(L11_ADDR(0x00161f84u)), u2f(0x3c8efa35u));
        if (limit < abs_f(lwc1(m + 0x44)) && lwu(0x001413dcu) < 2u) {
            hero_set_state(6, 1);
        }
    }
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x60)));
    vec3_add(V(sp), V(sp), V(m + 0x10));
    attach_frame_set(pv + 0x20, sp, sp + 0x10, m + 0x40);
    ee_stack_free(0xf0);
}

// Class 1180, a swinging valve: state 0 hides it from the moby pass (+0x30 0xff) and saves its yaw (pvar +8); state 1
// waits for its linked class-0x49b moby (pvar +0) to reach state 2 (snapping open to 3 on the level's first 10 frames,
// else setting 0x13d3e3, playing sound 0 and swinging in 2); state 2 turns by 60 degrees -> 3.
// orig: L11:0x0030f2a8
void pok_class1180_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float swing = u2f(0x3f860a92u);  // (pi / 3)
    switch (lbu(m + 0x20)) {
    case 0: {
        const float yaw = lwc1(m + 0x48);
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        swc1(pv + 8, yaw);
        break;
    }
    case 1:
        if (moby_is_in_state(moby_at(lwu(pv)), 0x49b, 2) != 0) {
            if (lw(L11_ADDR(0x0015f5ccu)) < 11) {
                sb(m + 0x20, 3);
                swc1(m + 0x48, angle_add(lwc1(pv + 8), swing));
            } else {
                sb(0x0013d3e3u, 1);
                sb(m + 0x20, 2);
                moby_sound_start(0, 0, m);
            }
        }
        break;
    case 2: {
        const float target = angle_add(lwc1(pv + 8), swing);
        const float accel = fpu_mul(dt2(), u2f(0x3eb2b8c2u));
        if (yaw_seek(m + 0x48, pv + 4, target, accel, accel, fpu_mul(dt(), u2f(0x3f490fdbu))) == 0.0f) sb(m + 0x20, 3);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
