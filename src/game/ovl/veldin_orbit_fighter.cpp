// Drek's fleet's (level 17) class 1843, ported by hand from the level's own code: Drek's fighter, the same flyer as
// Pokitaru's class 1319 (pokitaru_bomber.cpp: the two levels' codes differ only in what a burst drops and in Pokitaru's
// record word 0x141c18, which this level never writes). It patrols its path (states 1 / 2) leaving vapour trails, waits
// off screen to come back (state 3), and when a cannon launches it (state 5, pvars +0x88 its slot) flies beside the
// camera in formation and fires at Ratchet's ship; a hit or a crash bursts it (states 2 / 6) and drops a pickup (class
// 0xe0 or 0xe4, spawn_at_place). Its trails, its pose on the path and its hits are the shared class1319_* ports. The
// original's order of calls and float operations kept. Level 17 only: `orig: L17:`, level data through lvd::*, the
// level's own data L17_ADDR, base ELF bare. Registered in veldin_orbit_mobys.cpp.
//
// The pvars: +0x60 / +0x64 / +0x68 its path, point and way to the next, +0x6c its speed along it (in formation: its speed),
// +0x70 / +0x74 the swing's angle and turn rate, +0x78 the swing's radius, +0x84 the frames off screen, +0x88 its
// formation slot (nonzero: it waits for a launch), +0x8c / +0x90 its yaw and pitch rates, +0x94 its aim's lead, +0x98 the
// frames it has broken off, +0xb0 Ratchet's place a frame ago (the formation moves with him).
//
// States (the table L17:0x1e6fc0, 7): 0 set up, 1 / 2 on its path (2: hit, bursts), 3 off screen, 4 hidden, 5 in formation,
// 6 crashed (bursts).
#include <game/ovl/veldin_orbit_mobys.h>

#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class1319.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/shared_ship_shots.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's place

// an EE stack frame of the original's size (its vectors and the seek's state are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// hidden in state `st`: its shown bit (+0x34 bit 0) set, its draw word +0x94 cleared
void hide_in(uint32_t m, uint32_t st) {
    sb(m + 0x20, st);
    sw(m + 0x94, 0);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
}

// the burst: the explosion, a pickup (class 0xe0 or 0xe4) where it was, its sound
void burst(uint32_t m) {
    explosion_burst(m, m + 0x10, -1, 2.0f, 13.0f);
    spawn_at_place(m + 0x10, rand_mod(2) != 0 ? 0xe0u : 0xe4u);
    moby_sound_start(1, 0, m);
}

// the formation moves with Ratchet: his move since the last frame (pvars +0xb0) added to the moby's place, into `move`
void follow_hero(uint32_t m, uint32_t pv, uint32_t move) {
    vec3_sub(V(move), V(HERO_POS), V(pv + 0xb0));
    lq_sq(pv + 0xb0, HERO_POS);
    vec3_add(V(m + 0x10), V(m + 0x10), V(move));
}

// state 5: in formation beside the camera, firing at Ratchet every tenth frame
void in_formation(uint32_t m, uint32_t pv) {
    const Frame f(400);
    const uint32_t sp = f.sp;
    const uint32_t step = sp, slot = sp + 0x10, move = sp + 0x20, cam = sp + 0x30, rows = sp + 0x40, to_hero = sp + 0x70;
    const uint32_t aim = sp + 0x80, shot = sp + 0x90, muzzle = sp + 0xa0, target = sp + 0xb0, seek = sp + 0xc0;
    swc1(seek, 0.0f);
    // the camera's angles, rolled level and pitched within 15 degrees, to a matrix
    lq_sq(cam, L17_ADDR(0x001676d0));
    const float pitch = lwc1(cam + 4);
    swc1(cam, 0.0f);
    if (u2f(0x3e860a92u) < pitch) swc1(cam + 4, u2f(0x3e860a92u));
    else if (pitch < u2f(0xbe860a92u)) swc1(cam + 4, u2f(0xbe860a92u));
    euler_to_mat3_vu(M(rows), V(cam));
    follow_hero(m, pv, move);

    // its slot: 8 out from the camera at the slot's angle (45 + 90 degrees x +0x88), 20 ahead
    const float angle = angle_add(u2f(0x3f490fdbu), fpu_mul(static_cast<float>(lw(pv + 0x88)), u2f(0x3fc90fdbu)));
    vec3_set_length(V(step), V(sp + 0x50), fpu_mul(sin_vu(angle), 8.0f));
    vec3_add(V(slot), V(L17_ADDR(0x001676c0)), V(step));
    vec3_set_length(V(step), V(sp + 0x60), fpu_mul(cos_vu(angle), 8.0f));
    vec3_add(V(slot), V(slot), V(step));
    vec3_set_length(V(step), V(rows), 20.0f);
    vec3_add(V(slot), V(slot), V(step));
    vec3_sub(V(step), V(slot), V(m + 0x10));
    const float dist = vec3_length(V(step));
    // the speed sought: at once while the ship boosts (0x14095f bit 1), fast while it breaks off, else slowly
    const float dt2 = lwc1(0x0015ed70);
    if ((lbu(0x0014095f) & 2u) != 0) {
        seek_1d(seek, pv + 0x6c, dist, fpu_add(dt2, dt2), fpu_mul(dt2, 20.0f), lwc1(0x0015ed6c));
    } else if (lbu(m + 0xbc) != 0) {
        seek_1d(seek, pv + 0x6c, dist, fpu_add(dt2, dt2), fpu_mul(dt2, 20.0f), fpu_mul(lwc1(0x0015ed6c), 15.0f));
    } else {
        const float dt2b = lwc1(0x0015ed70);
        seek_1d(seek, pv + 0x6c, dist, fpu_add(dt2b, dt2b), fpu_mul(dt2b, 20.0f), fpu_mul(lwc1(0x0015ed6c), 5.0f));
    }
    vec3_clamp_length(step, lwc1(pv + 0x6c));
    vec3_add(V(m + 0x10), V(m + 0x10), V(step));

    if (lbu(m + 0xbc) != 0) {  // breaking off: back in its slot, or after 120 frames shown
        if (lbu(m + 0x31) != 0) sw(pv + 0x98, lwu(pv + 0x98) + 1);
        if (vec3_distance(V(slot), V(m + 0x10)) < 2.0f) {
            sb(m + 0xbc, 0);
            return;
        }
        if (frame_scale_i(0x78) < lw(pv + 0x98)) sb(m + 0xbc, 0);
        return;
    }

    // turned toward Ratchet
    vec3_sub(V(to_hero), V(HERO_POS), V(m + 0x10));
    const float rate = u2f(0x41490fdbu);  // (4 pi)
    {
        const float acc = fpu_mul(lwc1(0x0015ed70), rate);
        yaw_seek(m + 0x48, pv + 0x8c, atan2_f(lwc1(to_hero), lwc1(to_hero + 4)), acc, acc, fpu_mul(lwc1(0x0015ed6c), rate));
    }
    {
        const float to_pitch = -atan2_f(vec2_length(V(to_hero)), lwc1(to_hero + 8));
        const float acc = fpu_mul(lwc1(0x0015ed70), rate);
        yaw_seek(m + 0x44, pv + 0x90, to_pitch, acc, acc, fpu_mul(lwc1(0x0015ed6c), rate));
    }
    if (ee_rem(lw(L17_ADDR(0x0015f5cc)), 10) == 0) {
        // a shot (class 1017) from a muzzle (the two by the frame) at Ratchet, led by +0x94 along his move
        moby_sound_start(2, 0, m);
        approach_f(pv + 0x94, 0.0f, 0.25f);
        vec3_set_length(V(target), V(move), lwc1(pv + 0x94));
        vec3_add(V(target), V(target), V(HERO_POS));
        const uint32_t offset = ee_rem(lw(L17_ADDR(0x0015f5cc)), 20) == 0 ? L17_ADDR(0x001de110) : L17_ADDR(0x001de120);
        mat3_transform(V(muzzle), V(offset), M(m + 0xc0));
        vec3_add(V(muzzle), V(muzzle), V(m + 0x10));
        vec3_sub(V(aim), V(target), V(muzzle));
        vec3_set_length(V(shot), V(aim), fpu_mul(lwc1(L17_ADDR(0x001623e8)), lwc1(0x0015ed6c)));
        vec3_add(V(shot), V(shot), V(move));
        const uint32_t s = class1017_launch(m, shot, muzzle, 128.0f, -1.0f, 10.0f);
        if (s != 0) {
            vec3_sub(V(shot), V(shot), V(move));
            swc1(s + 0x44, -atan2_f(vec2_length(V(shot)), lwc1(shot + 8)));
            swc1(s + 0x48, atan2_f(lwc1(shot), lwc1(shot + 4)));
        }
    }
    if (lbu(m + 0x31) == 0) hide_in(m, 4);
}
}  // namespace

// orig: L17:0x002f40d8
void vo_class1843_update_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class1319_hit(m);
    switch (lbu(m + 0x20)) {  // (the table L17:0x1e6fc0)
    case 0: {  // (0x002f4144)
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        if (lw(pv + 0x88) != 0) {
            hide_in(m, 4);
            break;
        }
        sb(m + 0x20, 1);
        swc1(pv + 0x6c, rand_range_signed_f(u2f(0x3e99999au), 0.5f));
        swc1(pv + 0x70, rand_angle());
        swc1(pv + 0x74, fpu_mul(fpu_mul(rand_range_signed_f(60.0f, 120.0f), u2f(0x3c8efa35u)), lwc1(0x0015ed6c)));
        swc1(pv + 0x78, rand_range_f(8.0f, 12.0f));
        break;
    }
    case 1: case 2: {  // (0x002f41d8)
        class1319_trails_record(m);
        const float t = fpu_add(lwc1(pv + 0x68), lwc1(pv + 0x6c));
        swc1(pv + 0x68, t);
        if (1.0f < t) {
            swc1(pv + 0x68, fpu_sub(t, 1.0f));
            const int32_t n = lw(lvd::path(lw(pv + 0x60)));
            sw(pv + 0x64, static_cast<uint32_t>(ee_rem(lw(pv + 0x64) + 1, n)));
        } else if (t < 0.0f) {
            swc1(pv + 0x68, fpu_add(t, 1.0f));
            const int32_t n = lw(lvd::path(lw(pv + 0x60)));
            sw(pv + 0x64, static_cast<uint32_t>(ee_rem(lw(pv + 0x64) + n - 1, n)));
        }
        (void)angle_add(lwc1(pv + 0x70), lwc1(pv + 0x74));  // (the original computes it and keeps nothing)
        class1319_path_pose(m);
        if (lbu(m + 0x20) == 2) {  // hit: bursts
            burst(m);
            hide_in(m, 3);
        } else if (lbu(m + 0x31) == 0 && vec3_distance(V(m + 0x10), V(HERO_POS)) < 32.0f) {
            hide_in(m, 3);
        }
        break;
    }
    case 3: {  // (0x002f4368) off screen, away from Ratchet for a second: back on its path
        {
            const Frame f(400);
            lq_sq(f.sp, m + 0x10);
            swc1(f.sp + 0xc, 3.0f);  // (a sphere the original builds and does not pass)
        }
        if (point_on_screen(m + 0x10, 512.0f) != -1) {
            sw(pv + 0x84, 0);
            break;
        }
        if (!(32.0f < vec3_distance(V(m + 0x10), V(HERO_POS)))) {
            sw(pv + 0x84, 0);
            break;
        }
        sw(pv + 0x84, lwu(pv + 0x84) + 1);
        if (frame_scale_i(0x3c) < lw(pv + 0x84)) {
            sb(m + 0x20, 1);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        }
        break;
    }
    case 5:  // (0x002f4434)
        in_formation(m, pv);
        break;
    case 6: {  // (0x002f4934) crashed
        {
            const Frame f(400);
            follow_hero(m, pv, f.sp);
        }
        burst(m);
        hide_in(m, 4);
        break;
    }
    default:  // (0x002f49dc: state 4, or 7 and above)
        break;
    }
}

}  // namespace rac1
