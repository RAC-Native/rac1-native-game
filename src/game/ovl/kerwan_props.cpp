// Kerwan's own moby classes, part 1: the small props, citizens and mechanisms. Header
// kerwan_mobys.h. Addresses are Kerwan's (`orig: L3:`); data only Kerwan
// has is L3_ADDR, the globals the engine shares with Veldin are the level's own (OV), the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

// Classes 915 / 916 / 917 (Fred 1 / 2 / 3, Kerwan's citizens that the game never shows): the distance to the hero,
// the angle towards him and its difference to the moby's own facing are computed and thrown away, then the moby
// retires on its first update (cut content). Orxon's class 915 is the same code (the base ELF's data and Veldin's functions
// only): this port serves it.
// orig: L3:0x002dc310 L10:0x002d7fe8
void fred_update_l3(uint32_t m) {
    (void)vec3_distance(V(m + 0x10), V(0x0013f3d0));
    const float to = atan2_f(fpu_sub(lwc1(0x0013f3d0), lwc1(m + 0x10)), fpu_sub(lwc1(0x0013f3d4), lwc1(m + 0x14)));  // (the hero's x, y)
    (void)angle_dist(to, lwc1(m + 0x48));
    moby_retire(m);
}

// Class 1827 (Lightpost): state 0 -> 1; in state 1 a hit record (sub_0025a420 with the mask 0x10000) with a
// positive damage (+0x2c) -> state 2; state 2 bursts: the sound, a bolt drop (0x251da8, the same code as Novalis's
// plant bolts), a debris burst of class 0x725 and a copy of class 0x724 (0x2523f8, Novalis's moby clone), and the
// moby is removed.
// orig: L3:0x002e2948
void lightpost_update_l3(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        bool hurt = false;
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) hurt = true;
        if (hurt) sb(m + 0x20, 2);
        return;
    }
    if (st == 0) {
        sb(m + 0x20, 1);
        return;
    }
    if (st != 2) return;
    moby_sound_start(0, 0, m);
    plant_bolts_drop_l1(m);
    debris_burst(m, 0x725, 1, 0x725, 1, 1, 2);
    moby_clone_l1(m, 0x724);
    moby_retire(m);
}

// Class 817 (Statue): as the lightpost up to state 2, which plays the sound, drops the bolts, throws three debris
// classes (0x727, 0x728, 0x729) from its place and retires (no clone).
// orig: L3:0x002d3cd8
void statue_update_l3(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        return;
    }
    if (st == 0) {
        sb(m + 0x20, 1);
        return;
    }
    if (st != 2) return;
    moby_sound_start(0, 0, m);
    plant_bolts_drop_l1(m);
    const uint32_t z = OV(0x0015f580);
    debris_spawn(m, 0x727, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
    debris_spawn(m, 0x728, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
    debris_spawn(m, 0x729, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
    moby_retire(m);
}

// Class 825 (Robo Shack Sign): always drawn (+0x30 = 0xff, +0x32 = 0xff), turns about z by 0.4795 (fs6c x
// 0x3ef5be0b) a frame and sets the attached frame (pvar +0x20) from the zero position and the angles as they were.
// orig: L3:0x002d3e58
void robo_shack_sign_update_l3(uint32_t m) {
    const Vec4 zero = {0.0f, 0.0f, 0.0f, 0.0f};
    const Vec4 base = V(m + 0x40);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(0x0015ed6c) * u2f(0x3ef5be0bu)));
    attach_frame_set_v(pv + 0x20, zero, base, V(m + 0x40));
}

// Class 997 (Garage Door): state 0 keeps its position (pvar +0..) and goes to 1 when the checkpoint-style flag of
// its index (+0xb2: the byte at 0x1bb230 + index + 0x454, or the bit in the level's table 0x14c190) says it was
// opened before; state 1 rises by fs6c a frame until 4 above the pvar's +8, then 2; state 2 holds at that height.
// orig: L3:0x002dca30
void garage_door_update_l3(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (st == 1) {
        const float z = lwc1(m + 0x18) + lwc1(0x0015ed6c);
        swc1(m + 0x18, z);
        if (lwc1(pv + 8) + 4.0f <= z) sb(m + 0x20, 2);
        return;
    }
    if (st == 0) {
        lq_sq(pv, m + 0x10);
        const uint32_t idx16 = lhu(m + 0xb2);
        const int32_t idx = static_cast<int16_t>(idx16);
        if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0) {
            sb(m + 0x20, 1);
            return;
        }
        const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
        if (((lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx16 & 0x1f)) & 1u) != 0) sb(m + 0x20, 1);
        return;
    }
    if (st == 2) swc1(m + 0x18, lwc1(pv + 8) + 4.0f);
}

// Class 1210 (Metropolis Train Carriage): skipped when its flags +0x34 have bit 1; the first frame sets the pvar's
// +0x60 (0), +0x64 (0), +0x68 (4), +0x7e (0xd), the distance from the chain point 0 to its place at +0xc4, keeps its
// place / angles at pvar +0xa0 / +0xb0, +0x5c = 3 and draws always; each frame it sets the attached frame (pvar
// +0x20) from the move since the last frame (place - the kept place, the kept angles, its angles) and keeps the
// new place and angles.
// orig: L3:0x002953f8
void train_carriage_update_l3(uint32_t m) {
    if ((lhu(m + 0x34) & 2u) != 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        Vec4 pt;
        sw(pv + 0x60, 0);
        sh(pv + 0x64, 0);
        sb(pv + 0x68, 4);
        sh(pv + 0x7e, 0xd);
        moby_chain_point_v(m, 0, pt);
        swc1(pv + 0xc4, vec3_distance(V(m + 0x10), pt));
        lq_sq(pv + 0xa0, m + 0x10);
        lq_sq(pv + 0xb0, m + 0x40);
        sw(pv + 0x5c, 3);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
    }
    Vec4 d = {0.0f, 0.0f, 0.0f, 0.0f};
    vec3_sub(d, V(m + 0x10), V(pv + 0xa0));
    attach_frame_set_v(pv + 0x20, d, V(pv + 0xb0), V(m + 0x40));
    lq_sq(pv + 0xa0, m + 0x10);
    lq_sq(pv + 0xb0, m + 0x40);
}

// ---- the escalators: class 899 lays its steps (class 898, 0x382) along a level path, one every 2.9 units; each step
// rides the path at the escalator's speed (L3:0x161bc0, three units a second, set by 899) and retires at the path's
// last leg. 899's pvars: +0 the path's index, +4 the timer to the next step, +8 its sound. The steps' pvars: +0x20 the
// attach frame (the hero standing on it), +0x60 the path, +0x64 the distance along it. The legs' lengths are written
// into the path's points' w lanes; a step reads the first leg's only (the legs are taken as equal).

namespace {
// the escalator's speed in units per frame (Kerwan's own word)
float escalator_speed() { return lwc1(L3_ADDR(0x00161bc0)); }
}  // namespace

// A step (class 0x382) at `along` on the path: its colour from the hero's, at the path's start facing its end.
// orig: L3:0x002db198
uint32_t escalator_step_spawn_l3(uint32_t path, float along) {
    const uint32_t s = moby_spawn(0x382);
    if (s == 0) return 0;
    const uint32_t pv = lwu(s + 0x78);
    sb(s + 0x20, 0);
    sb(s + 0x30, 0x80);
    sh(s + 0x32, 0x40);
    sb(s + 0x31, 1);
    sh(s + 0x34, lhu(s + 0x34) | 0x20u);
    moby_copy_hero_color(s);
    lq_sq(s + 0x10, path + 0x10);
    vec_clear(V(s + 0x40));
    const uint32_t last = path + ((lwu(path) - 1u) << 4);
    swc1(s + 0x48, atan2_f(fpu_sub(lwc1(last + 0x10), lwc1(s + 0x10)), fpu_sub(lwc1(last + 0x14), lwc1(s + 0x14))));
    swc1(pv + 0x64, along);
    sw(pv + 0x60, path);
    sw(pv + 8, pv + 0x20);
    moby_update_transform(s);
    return s;
}

// orig: L3:0x002db020
void kerwan_class898_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t old = sp, move = sp + 0x10, leg = sp + 0x20;
    lq_sq(old, m + 0x10);
    swc1(pv + 0x64, fpu_add(lwc1(pv + 0x64), escalator_speed()));
    // (a step along its facing, thrown away: the move is taken from the positions below)
    swc1(move, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x64)));
    sw(move + 8, 0);
    swc1(move + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x64)));
    vec3_add(V(move), V(move), V(lwu(pv + 0x60) + 0x10));
    const uint32_t path = lwu(pv + 0x60);
    const float len = lwc1(path + 0x1c);
    const int32_t i = float_to_int(fpu_div(lwc1(pv + 0x64), len));
    const float t = fpu_div(fpu_sub(lwc1(pv + 0x64), fpu_mul(int_to_float(i), len)), len);
    if (!(i < lw(path) - 2)) {
        moby_retire(m);
        ee_stack_free(0xb0);
        return;
    }
    const uint32_t at = static_cast<uint32_t>(i) << 4;
    vec3_sub(V(leg), V(path + at + 0x20), V(path + at + 0x10));
    vec3_scale(V(leg), V(leg), t);
    vec3_add(V(m + 0x10), V(leg), V(lwu(pv + 0x60) + at + 0x10));
    vec3_sub(V(move), V(m + 0x10), V(old));
    attach_frame_set(pv + 0x20, move, m + 0x40, m + 0x40);
    ee_stack_free(0xb0);
}

// orig: L3:0x002db280
void kerwan_class899_update_l3(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        if (lw(pv) == -1) {  // no path: a debug line (its class number) and gone
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        const uint32_t path = lvd::path(lw(pv));
        swc1(L3_ADDR(0x00161bc0), fpu_mul(lwc1(0x0015ed6c), 3.0f));
        for (int32_t i = 0; i < lw(path) - 2; ++i) {
            const uint32_t p = path + 0x10 + static_cast<uint32_t>(i) * 0x10;
            lvd::set_path_f32(p + 0xc, vec3_distance(V(p), V(p + 0x10)));
        }
        sw(pv + 4, 0);
        sb(m + 0x20, 1);
        sw(pv + 8, static_cast<uint32_t>(-1));
        const float gap = u2f(0x4039999au);  // 2.9
        for (float along = gap; along < fpu_mul(int_to_float(lw(path)), lwc1(path + 0x1c)); along = fpu_add(along, gap)) {
            escalator_step_spawn_l3(path, along);
        }
        sb(m + 0x30, 0x60);
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 4))) != 0) {
        sw(pv + 4, static_cast<uint32_t>(float_to_int(fpu_div(u2f(0x4039999au), escalator_speed()))));
        escalator_step_spawn_l3(lvd::path(lw(pv)), 0.0f);
    }
    if (sub_0028d8c0(m, lw(pv + 8)) == 0) sw(pv + 8, static_cast<uint32_t>(moby_sound_play(0, 4, m, 0x382)));
}

// Class 1548, the scenes' smoke and sparks (a scene_moby_smoke class like Veldin's 1545), only in game mode 2 (a scene;
// its block at Kerwan's 0x16c960: +0x30 the scene, +0x34 its frame, +0x178.. its mobys): on scene 4 its moby 2 smokes, on
// scene 10 its moby 0; on scene 3 from frame 900 to 10000 (at 60 Hz) two sparks a frame (particle 16s, still) at the
// chain points 0 and 1 of its moby 3.
// orig: L3:0x002e01d0
void kerwan_class1548_update_l3(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1) return;
    if (lw(OV(0x0015f5c4)) != 2) return;
    constexpr OvConst<0x0016c860u> SC{};
    const int32_t scene = lw(SC + 0x30);
    if (scene == 4 || scene == 10) scene_moby_smoke(lwu(SC + 0x178 + (scene == 4 ? 2u : 0u) * 4));
    if (lw(OV(0x0015f5c4)) != 2) return;
    if (lw(SC + 0x30) != 3) return;
    if (lw(SC + 0x34) < frame_scale_i(0x384)) return;
    if (frame_scale_i(0x2710) < lw(SC + 0x34)) return;
    const uint32_t src = lwu(SC + 0x184);
    if (src == 0) return;
    const uint32_t sp = ee_stack_alloc(0x80);
    for (uint32_t k = 0; k < 0x10; k += 4) sw(sp + 0x20 + k, 0);
    moby_chain_point_v(src, 0, V(sp));
    moby_chain_point_v(src, 1, V(sp + 0x10));
    {
        const float size = fpu_mul(rand_range_f(0.25f, 0.5f), 210000.0f);
        particle_16_spawn(sp, sp + 0x20, 0x7f7f7f7fu, 0x7f7f7fu, frame_scale_i(rand_range(0x2d, 0x3c)), size);
    }
    {
        const float size = fpu_mul(rand_range_f(0.25f, 0.5f), 210000.0f);
        particle_16_spawn(sp + 0x10, sp + 0x20, 0x407f7f7fu, 0x7f7f7fu, frame_scale_i(rand_range(0x2d, 0x3c)), size);
    }
    ee_stack_free(0x80);
}

// Class 914, a citizen of the talk system that a hit of kind 3 breaks (its pvars: the talk block, +4 / +0x36 its state,
// +0x4c a checkpoint box). Broken (its index +0xb2 marked in the level's tables: the byte at 0x1bb230 + index + 0x454, the
// bit of 0x14c190's level row) it goes to state 3, its sparks. State 0 starts the talk (in the hero's colour), 1 talks (then
// the hero walks to a spot 2.7 in front, 2); 2 waits for the scene's end (game mode 2), then talks again (1) and on the
// talk's state 2 takes the checkpoint at its box. A hit of kind 3 breaks it: the first time on the level the help message
// 0x53d6 with its sound, an explosion one unit up, state 3 and both marks. With the flag 0x13d4cc set only a broken one
// runs its states.
namespace {
bool citizen_broken(uint32_t m) {
    const uint32_t idx16 = lhu(m + 0xb2);
    const int32_t idx = static_cast<int16_t>(idx16);
    if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0) return true;
    const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
    return ((lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx16 & 0x1f)) & 1u) != 0;
}
// the bit of the moby's index (+0xb2) in a table of the levels' words
void citizen_mark(uint32_t table, uint32_t m) {
    const uint32_t idx16 = lhu(m + 0xb2);
    const uint32_t w = table + (static_cast<uint32_t>(static_cast<int16_t>(idx16) >> 5) << 2);
    sw(w, lwu(w) | (1u << (idx16 & 0x1f)));
}
}  // namespace

// orig: L3:0x002dbd90
void kerwan_class914_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 26.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    if (citizen_broken(m) && lbu(m + 0x20) != 3) {
        sb(m + 0x20, 3);
        moby_set_anim(m, 1, 0);
    }
    const uint32_t hit = sub_0025a420(m, 0xffffffffu, 0);
    if (hit != 0 && lwu(hit + 0x20) != 0 && lbu(hit + 0x28) == 3 && !citizen_broken(m)) {
        if (lbu(0x0013d40d) == 0) {
            sb(0x0013d40d, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        const uint32_t pos = sp + 0x30, still = sp + 0x20;
        lq_sq(pos, m + 0x10);
        for (uint32_t k = 0; k < 0x10; k += 4) sw(still + k, 0);
        swc1(pos + 8, fpu_add(lwc1(pos + 8), 1.0f));
        explosion_effect(m, still, pos, 10, 3, 0x10, 0, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
        sb(m + 0x20, 3);
        moby_set_anim(m, 1, 0);
        citizen_mark(0x0014c190 + (static_cast<uint32_t>(lw(0x0015ed84)) << 8), m);
        citizen_mark(OV(0x001ba4d0), m);
    }
    sb(m + 0xa4, 0xff);
    if (lbu(0x0013d4cc) != 0 && lbu(m + 0x20) != 3) {
        ee_stack_free(0xe0);
        return;
    }
    switch (lbu(m + 0x20)) {
    case 0:
        sd(m + 0x38, ld(lwu(0x001413d0) + 0x38));  // the hero's colour
        sb(m + 0x20, 1);
        talk_init(m, pv);
        if (lbu(0x0013d3a1) != 0) {
            sh(pv + 4, 1);
            sh(pv + 0x36, 2);
        }
        break;
    case 1:
        if (talk_update(m, pv) != 0) {
            sb(0x0013d3a1, 1);
            hero_walk_to_l1(u2f(0x402ccccdu), m);
            sb(m + 0x20, 2);
        }
        break;
    case 2:
        if (lw(OV(0x0015f5c4)) == 2) break;
        sb(m + 0x20, 1);
        if (lh(pv + 4) != 2) break;
        if (lw(pv + 0x4c) != -1) {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x4c));
            checkpoint_save(box + 0x30, box + 0x70);
        }
        break;
    case 3: {  // its sparks: one in three frames a still grey puff (particle 16) within half a unit of it
        const uint32_t vel = sp + 0x20, at = sp + 0x30;
        {
            const float c = cos_vu(rand_angle());
            const float r = rand_range_f(0.0f, 0.25f);
            swc1(vel, fpu_mul(c, fpu_mul(r, lwc1(0x0015ed6c))));
        }
        {
            const float sn = sin_vu(rand_angle());
            const float r = rand_range_f(0.0f, 0.25f);
            swc1(vel + 8, 0.0f);
            swc1(vel + 4, fpu_mul(sn, fpu_mul(r, lwc1(0x0015ed6c))));
        }
        {
            const float c = cos_vu(rand_angle());
            const float r = rand_range_f(0.0f, 0.5f);
            const float q = rand_range_f(0.0f, 1.0f);
            swc1(at, fpu_mul(fpu_mul(c, r), q));
        }
        {
            const float sn = sin_vu(rand_angle());
            const float r = rand_range_f(0.0f, 0.5f);
            const float q = rand_range_f(0.0f, 1.0f);
            swc1(at + 8, 0.0f);
            swc1(at + 4, fpu_mul(fpu_mul(sn, r), q));
        }
        vec3_add_inplace(V(at), V(m + 0x10));
        if (rand_mod(3) == 0) particle_16_spawn(at, vel, 0x20202020u, 0x7f7f7fu, frame_scale_i(rand_range(0x3c, 0x78)), 157500.0f);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0xe0);
}

}  // namespace rac1
