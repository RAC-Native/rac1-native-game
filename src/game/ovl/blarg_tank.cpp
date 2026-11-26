// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1083, the glass tank that breaks open (a scene when
// Ratchet is near: the camera on it, a fade, Ratchet set aside; or a break by itself when he is far), and its break (the
// broken copy, class 0x43c, and 50 shards, class 1085). It carries 1.02 Fixes fix 20 (the far break without its shards,
// fixes102_blarg_far_debris), which the PAL disc 2.00's own build has whatever the fix's row. Header blarg_own.h;
// registered in blarg_mobys.cpp (its four draw callbacks 0x3003f0 / 0x300588 / 0x300720 / 0x300958 are blarg_part2's).
// Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR, the base ELF's are bare.
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_beam.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l6.h>
#include <game/level/level_write.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0 a direction, +0x80 his position, +0x90 his rotation)
constexpr uint32_t HERO_POS = PLAYER + 0x80;

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the scene's screen fade from the frame count `t` of the level's scene length 0x162044
void tank_fade(int32_t t) {
    swc1(L6_ADDR(0x0015f3fc), fpu_sub(1.0f, fpu_div(int_to_float(t), static_cast<float>(lw(L6_ADDR(0x00162044))))));
}

// the hero's place mark (the box pvar +0xc's centre) put where he stands, half a unit lower, and the mark used up
void mark_hero_place(uint32_t pv) {
    const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0xc));
    lvd::copy_shape_row(b + 0x30, HERO_POS);
    lvd::set_shape_f32(b + 0x38, fpu_sub(lwc1(b + 0x38), 0.5f));
    sw(pv + 0xc, 0xffffffffu);
}
}  // namespace

// The tank `m` breaks: its pvar +4 cleared, its broken copy (class 0x43c) put in its place (its look +0x34 / +0x38, its
// size +0x2c), itself hidden; with `debris` (the 1.02 build's argument; the North American build always has it) 50
// shards (class 1085, beam_shard_spawn) thrown out at random within 2 of it, 0.5 .. 4.5 above the level's height
// 0x16204c, at 1 .. 6 units a frame.
// orig: L6:0x00300278
void tank_break_l6(uint32_t m, bool debris) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t at = sp, dir = sp + 0x10;
    sw(lwu(m + 0x78) + 0x4, 0);
    const uint32_t copy = moby_spawn(0x43c);
    sb(copy + 0x31, 1);
    sh(copy + 0x32, 0x40);
    sd(copy + 0x38, ld(m + 0x38));
    sh(copy + 0x34, lhu(m + 0x34));
    lq_sq(copy + 0x10, m + 0x10);
    lq_sq(copy + 0x40, m + 0x40);
    swc1(copy + 0x2c, lwc1(m + 0x2c));
    moby_update_transform(copy);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    if (debris) {
        for (int k = 0; k < 50; ++k) {
            const float speed = fpu_mul(rand_range_f(1.0f, 6.0f), dt());
            const float a = rand_angle();
            swc1(dir, fpu_add(cos_vu(a), cos_vu(a)));
            swc1(dir + 4, fpu_add(sin_vu(a), sin_vu(a)));
            sw(dir + 8, 0);
            vec3_add(V(at), V(m + 0x10), V(dir));
            const float h = lwc1(L6_ADDR(0x0016204c));
            swc1(at + 8, fpu_add(lwc1(at + 8), rand_range_f(fpu_add(h, 0.5f), fpu_add(h, 4.5f))));
            vec3_set_length(V(dir), V(dir), speed);
            beam_shard_spawn(at, dir);
        }
    }
    ee_stack_free(0x80);
}

// Class 1083, the glass tank. pvars: +0 / +4 its look (the draw callback: whole or broken, which), +8 the moby whose +0xbc
// sets it off, +0xc the box Ratchet's place is marked in, +0x10 the box he is put at for the scene, +0x14 the scene's
// camera box, +0x18 the camera's move (seek_1d to 1, its rate +0x1c), +0x20 the moby it breaks for (-1 none), +0x24 the
// scene's frame count. States: 0 set up; 1 waits for its trigger (the moby +8's +0xbc, or no moby +0x20): within 32 of
// Ratchet the scene (2, after 30 frames), else it breaks by itself (4); 2 the scene's start: the scripted camera at the
// box +0x14, the scripted mode 0x12 (0x15f404 / 0x15f408), Ratchet put at the box +0x10 facing it (state 0x72); the
// screen fades over 0x162044 frames; 3 the scene: the camera moves from its box to behind Ratchet, the tank breaking once
// the moby +0x20 has moved from it (0.1); when the camera is there, Ratchet is let go (hydro_2eac18(2)) (5); 4 broken by
// itself: Ratchet's place is marked when he comes within 16 (5); 5 nothing more.
// orig: L6:0x002ffcb0
void blg_class1083_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xc0);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L6_ADDR(0x00162048))));
    switch (lbu(m + 0x20)) {  // (the table 0x201860: one case a state, 0 .. 4)
    case 0:
        sw(pv + 0x4, 1);
        sb(m + 0x20, 1);
        break;
    case 1: {
        const int32_t trigger = lw(pv + 0x8);
        const bool set_off = trigger >= 0 && lbu(lwu(L6_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(trigger) << 8) + 0xbc) != 0;
        if (!set_off && lw(pv + 0x20) != -1) break;
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 32.0f) {
            sb(m + 0x20, 2);
            sw(pv + 0x24, static_cast<uint32_t>(frame_scale_i(0x1e)));
        } else {
            tank_break_l6(m, fixes102_blarg_far_debris(lw(pv + 0x20)));  // (1.02 Fixes, fix 20: the far break)
            sb(m + 0x20, 4);
        }
        break;
    }
    case 2: {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x24))) != 0) {
            sb(m + 0x20, 3);
            sw(L6_ADDR(0x0015f404), 1);
            const uint32_t cam = lvd::box(lvd::BOXES, lw(pv + 0x14));
            sw(L6_ADDR(0x0015f408), 0x12);
            lq_sq(sp, cam + 0x30);
            lq_sq(sp + 0x10, cam + 0x70);
            camera_scripted_set(sp, sp + 0x10, 1, static_cast<uint32_t>(frame_scale_i(0x12c)), 0);
            const float tiny = u2f(0x3a83126fu);  // 0.001
            camera_angles_set(tiny, 1.0f, 1.0f, tiny, 1.0f, 1.0f);
            uid5_set_pvar_80(sp);
            uid5_set_pvar_90(sp + 0x10);
            sw(pv + 0x18, 0);
            const uint32_t place = lvd::box(lvd::BOXES, lw(pv + 0x10));
            lvd::set_shape_f32(place + 0x78, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(place + 0x30)), fpu_sub(lwc1(m + 0x14), lwc1(place + 0x34))));
            hero_place(place + 0x30, place + 0x70, 0x72, 0);
        }
        const int32_t t = lw(pv + 0x24);
        if (t < lw(L6_ADDR(0x00162044))) tank_fade(t);
        break;
    }
    case 3: {
        const int32_t t = lw(pv + 0x24);
        if (t < lw(L6_ADDR(0x00162044))) {
            sw(pv + 0x24, static_cast<uint32_t>(t + 1));
            tank_fade(t + 1);
        }
        const float move = lwc1(pv + 0x18);
        if (u2f(0x3f7fff58u) <= move) {  // (0.99999)
            sb(m + 0x20, 5);
            hero_set_state(0, 0);
            hydro_2eac18(2);
            sw(L6_ADDR(0x0015f404), 0);
            break;
        }
        if (u2f(0x3d4ccccdu) <= move && lw(pv + 0xc) >= 0) mark_hero_place(pv);  // (0.05)
        if (lw(pv + 0x4) == 1) {
            const uint32_t other = lwu(L6_ADDR(0x0015ffd8)) + (lwu(pv + 0x20) << 8);
            if (0.1f < vec2_distance(V(m + 0x10), V(other + 0x10))) {
                moby_sound_start(0, 0, m);
                tank_break_l6(m, true);
            }
        }
        seek_1d(pv + 0x18, pv + 0x1c, 1.0f, dt_sq(), dt_sq(), fpu_mul(dt(), 1.5f));
        // the camera: from the box +0x14 (its yaw replaced by 0x162050 degrees) to two units behind Ratchet, 0.85 over him
        const uint32_t cam = lvd::box(lvd::BOXES, lw(pv + 0x14));
        const uint32_t at = sp, rot = sp + 0x20, to = sp + 0x10, to_rot = sp + 0x30, back = sp + 0x40;
        lq_sq(at, cam + 0x30);
        lq_sq(rot, cam + 0x70);
        swc1(rot, fpu_mul(lwc1(L6_ADDR(0x00162050)), u2f(0x3c8efa35u)));
        lq_sq(to, HERO_POS);
        lq_sq(to_rot, PLAYER + 0x90);
        swc1(to_rot + 4, lwc1(rot + 4));
        vec3_set_length(V(back), V(PLAYER), -2.0f);
        vec3_add(V(to), V(to), V(back));
        swc1(to + 8, fpu_add(lwc1(to + 8), u2f(0x3f59999au)));  // 0.85
        vec3_lerp(V(at), V(at), V(to), lwc1(pv + 0x18));
        vec3_lerp(V(rot), V(rot), V(to_rot), lwc1(pv + 0x18));
        uid5_set_pvar_80(at);
        uid5_set_pvar_90(rot);
        break;
    }
    case 4:
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 16.0f) {
            mark_hero_place(pv);
            sb(m + 0x20, 5);
        }
        break;
    default:
        break;
    }
    // its draw: whole (+4 set) or broken, the one or the other look (+0)
    if (lw(pv + 0x4) != 0)
        sub_001f93e0(lw(pv + 0x0) != 0 ? L6_ADDR(0x00300720) : L6_ADDR(0x00300958), m);
    else
        sub_001f93e0(lw(pv + 0x0) != 0 ? L6_ADDR(0x003003f0) : L6_ADDR(0x00300588), m);
    ee_stack_free(0xc0);
}

}  // namespace rac1
