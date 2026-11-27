// Drek's fleet's (level 17) laser cannons, ported by hand from the level's own code: class 347, a cannon on the
// flagship's hull (hits from the hero's ship's shots and rockets only, L17:0x2cbc10; its group's fighters, class 1843,
// laid out along a path at the start, L17:0x2cbd38), firing at a box's random point while the hero walks inside (state
// 2) and at the hero's ship (class 1379) while he flies within reach (state 3), and now and then sending one of its
// group's waiting fighters at the camera (L17:0x2f4fb8); its shot, class 1368 (spawned by L17:0x2e94d8), a bolt that
// homes on the hero's place a little, glows (particle 0x50, L17:0x26fdd0), hurts what it meets and bursts. The
// original's order of calls and float operations kept. Level 17 only: `orig: L17:`, level data through lvd::* and the
// collision facade, the level's own data L17_ADDR, base ELF bare. Registered in veldin_orbit_mobys.cpp.
#include <game/ovl/veldin_orbit_own.h>

#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/level/level_read.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/veldin_orbit_mobys.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

float dt() { return lwc1(0x0015ed6cu); }
float dt2() { return lwc1(0x0015ed70u); }

constexpr uint32_t PLAYER = 0x0013f350u;
constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr int16_t FIGHTER = 0x733;  // class 1843
constexpr int16_t HERO_SHIP = 0x563;  // class 1379

// the moby of a group list's entry (its index in the low 15 bits; bit 15 marks the list's last)
uint32_t group_moby(uint32_t e) { return lwu(L17_ADDR(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8); }
bool group_last(uint32_t e) { return lh(e) < 0; }

// the hero flies his ship: state 0x32 with a class-1379 moby as the vehicle
bool hero_in_ship() {
    if (lw(PLAYER + 0x2084) != 0x32) return false;
    const uint32_t ship = lwu(PLAYER + 0x15f0);
    return ship != 0 && lh(ship + 0xa6) == HERO_SHIP;
}

// the cannon turns to `aim` (from its pivot): its yaw and the barrel joint's pitch spring; the barrel's pitch error
float cannon_turn(uint32_t m, uint32_t pv, uint32_t aim) {
    const float yaw = atan2_f(lwc1(aim + 0x0), lwc1(aim + 0x4));
    const float accel = fpu_mul(dt2(), u2f(0x3f060a92u));
    yaw_seek(m + 0x48, pv + 0x78, yaw, accel, accel, fpu_mul(dt(), u2f(0x3f860a92u)));
    const float pitch = atan2_f(vec2_length(V(aim)), lwc1(aim + 0x8));
    const float k = lwc1(0x0015ed64u);
    swc1(pv + 0xf4, -pitch);
    joint_node_spring(m, pv + 0x90, 0, fpu_mul(k, u2f(0x3ca3d70au)), fpu_mul(k, u2f(0x3e99999au)));
    const float pitch2 = atan2_f(vec2_length(V(aim)), lwc1(aim + 0x8));
    return angle_dist(lwc1(pv + 0xd4), -pitch2);
}

// the muzzle's flash (particle 0x1b) at `pos`
void muzzle_flash(uint32_t pos, uint32_t vel) {
    const uint32_t p = spawn_particle_1b(pos, vel, 0x80ff8f6fu, 3, 1260000.0f);
    if (p != 0) sb(p + 0x9, static_cast<uint32_t>(float_to_int(4.0f) + 0x70));
}

// the level's record of the hero hit by a cannon's shot: the count, the time (in 600-frame units) kept at its
// latest, the level's bit
void hit_note() {
    sh(0x00141dc0u, lhu(0x00141dc0u) + 1u);
    if (static_cast<int32_t>(lhu(0x00141dc2u)) < frame_scale_i(lw(0x0015eea4u)) / 0x258)
        sh(0x00141dc2u, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4u)) / 0x258));
    sw(0x00141dc4u, lwu(0x00141dc4u) | (1u << (lwu(0x0015ed84u) & 31u)) | 0x80000000u);
}
}  // namespace

// Class 347's hits: only from the hero's ship's shots and rockets (classes 1009 and 295) or from no moby; the health
// (pvars +0x20) goes down by the damage, at zero the cannon breaks (state 4, its target flag off); the colour pulse.
// orig: L17:0x002cbc10
void vo_2cbc10_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) return;
    float dmg = 0.0f;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    uint32_t taken = hit;
    if (hit != 0 && lwu(hit + 0x20) != 0) {
        const int16_t cls = lh(lwu(hit + 0x20) + 0xa6);
        if (cls != 0x3f1) taken = cls == 0x127 ? hit : 0u;
    }
    int32_t state = 0;
    moby_hit_react_r(m, taken, pv + 0x20, 0, state, &dmg, nullptr, 4);
    if (state != 1 && lbu(m + 0x20) != 4) {
        const float health = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, health);
        if (health <= 0.0f) {
            sb(m + 0x20, 4);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        }
        sb(pv + 0x67, 0xf0);
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
}

// Class 347's start: the fighters (class 1843) of its group (pvars +0x80) laid out along its path (pvars +0x84), the
// path's points shared evenly between them in the list's order (each fighter's pvars: +0x60 the path, +0x64 its point,
// +0x68 0).
// orig: L17:0x002cbd38
void vo_2cbd38_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t group = lh(pv + 0x80);
    if (group == -1) return;
    if (lvd::group(group) == 0) return;
    int32_t count = 0;
    for (uint32_t e = lvd::group(group);; e += 2) {
        if (lh(group_moby(e) + 0xa6) == FIGHTER) ++count;
        if (group_last(e)) break;
    }
    if (count == 0) return;  // (the original divides by it: nothing below is reached with no fighter)
    const uint32_t path = lvd::path(lw(pv + 0x84));
    const int32_t step = static_cast<int16_t>(ee_div(lvd::path_n(path), count));
    int32_t point = 0;
    uint32_t at = path + 0x10;
    for (uint32_t e = lvd::group(group);; e += 2) {
        const uint32_t f = group_moby(e);
        if (lh(f + 0xa6) == FIGHTER) {
            const uint32_t fpv = lwu(f + 0x78);
            lvd::path_copy_row(f + 0x10, at);
            sw(fpv + 0x64, static_cast<uint32_t>(point));
            sw(fpv + 0x60, lwu(pv + 0x84));
            sw(fpv + 0x68, 0);
            point = static_cast<int16_t>(point + step);
            at += static_cast<uint32_t>(step) << 4;
        }
        if (group_last(e)) break;
    }
}

// Spawns class 1368, the cannon's shot, at `from` aimed at `to` with `speed` (its velocity, pvars +0; its target +0x10,
// its life +0x38: 600 frames, its shooter +0x3c), facing its way.
// orig: L17:0x002e94d8
uint32_t vo_2e94d8_l17(uint32_t m, uint32_t from, uint32_t to, float speed) {
    const Frame f(144);
    const uint32_t sp = f.sp;
    const uint32_t s = moby_spawn(0x558);
    if (s == 0) return 0;
    sb(s + 0x30, 0xff);
    sh(s + 0x32, 0xff);
    sb(s + 0x31, 1);
    sb(s + 0x20, 0);
    sd(s + 0x38, ld(m + 0x38));
    lq_sq(s + 0x10, from);
    const uint32_t spv = lwu(s + 0x78);
    swc1(spv + 0x40, speed);
    lq_sq(spv + 0x10, to);
    sw(spv + 0x38, static_cast<uint32_t>(frame_scale_i(0x258)));
    sw(spv + 0x3c, m);
    vec3_sub(V(sp), V(spv + 0x10), V(s + 0x10));
    vec3_set_length(V(spv), V(sp), lwc1(spv + 0x40));
    swc1(s + 0x48, atan2_f(fpu_sub(lwc1(to + 0x0), lwc1(s + 0x10)), fpu_sub(lwc1(to + 0x4), lwc1(s + 0x14))));
    const float flat = vec2_distance(V(s + 0x10), V(to));
    swc1(s + 0x44, -atan2_f(flat, fpu_sub(lwc1(to + 0x8), lwc1(s + 0x18))));
    return s;
}

// One fighter (class 1843) of group `group` waiting (state 4) sent off, picked at one chance in ten each: placed
// ahead of the camera to a side (by its pvars +0x88, a quarter turn each), aimed at the hero's place (pvars +0xb0),
// state 5; the fighter, or 0 when none went.
// orig: L17:0x002f4fb8
uint32_t vo_2f4fb8_l17(int32_t group) {
    const Frame f(272);
    const uint32_t sp = f.sp;
    const uint32_t ang = sp + 0x20, mat = sp + 0x30, place = sp + 0x10;
    for (uint32_t e = lvd::group(group);; e += 2) {
        const uint32_t fm = group_moby(e);
        if (lh(fm + 0xa6) == FIGHTER && lbu(fm + 0x20) == 4 && rand_mod(10) == 0) {
            const uint32_t fpv = lwu(fm + 0x78);
            lq_sq(ang, L17_ADDR(0x001676d0));  // the camera's angles, its pitch kept within 10 degrees, no roll
            const float pitch = lwc1(ang + 0x4);
            sw(ang + 0x0, 0);
            if (u2f(0x3e32b8c2u) < pitch) swc1(ang + 0x4, u2f(0x3e32b8c2u));
            else if (pitch < u2f(0xbe32b8c2u)) swc1(ang + 0x4, u2f(0xbe32b8c2u));
            euler_to_mat3_vu(M(mat), V(ang));
            const float side = static_cast<float>(lw(fpv + 0x88));
            const float a = angle_add(u2f(0x3f490fdbu), fpu_mul(side, u2f(0x3fc90fdbu)));
            vec3_set_length(V(sp), V(mat + 0x10), fpu_mul(sin_vu(a), 12.0f));
            vec3_add(V(place), V(L17_ADDR(0x001676c0)), V(sp));
            vec3_set_length(V(sp), V(mat + 0x20), fpu_mul(cos_vu(a), 12.0f));
            vec3_add(V(place), V(place), V(sp));
            vec3_set_length(V(sp), V(mat), 6.0f);
            vec3_add(V(place), V(place), V(sp));
            lq_sq(fpv + 0xb0, HERO_POS);
            lq_sq(fm + 0x10, place);
            sb(fm + 0xbc, 1);
            sb(fm + 0x20, 5);
            swc1(fpv + 0x94, 5.0f);
            swc1(fpv + 0x6c, fpu_mul(dt(), 20.0f));
            if (lbu(fm + 0x53) != 0) moby_anim_set(fm, 0, 0, frame_scale_i(10));
            sh(fm + 0x34, lhu(fm + 0x34) & 0xfffeu);
            sw(fm + 0x94, lwu(lwu(fm + 0x24) + 0x10));
            return fm;
        }
        if (group_last(e)) break;
    }
    return 0;
}

// Class 347, the laser cannon. Pvars: +0x20 the health, +0x24 (half) 15, +0x60 the colour pulse, +0x70 the box it
// fires into, +0x74 its firing slot (state 2: the frame count modulo 90 frames; state 3: a timer), +0x78 the yaw's
// speed, +0x7c its fighters' group (0: none), +0x80 / +0x84 the group and path of its start, +0x90 the barrel joint.
// States: 0 start (gone when the level's record says it was broken), 1 waiting, 2 firing into the box (the hero on
// foot, the way to the box clear), 3 firing at the hero's ship, 4 broken.
// orig: L17:0x002cb310
void vo_class347_update_l17(uint32_t m) {
    const Frame f(352);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    vo_2cbc10_l17(m);
    const uint8_t st = lbu(m + 0x20);
    switch (st) {  // (the jump table L17:0x1e68b0)
    case 0: {  // (0x002cb374)
        sb(m + 0x20, 1);
        sh(pv + 0x24, 0xf);
        swc1(pv + 0x20, 15.0f);
        vo_2cbd38_l17(m);
        const uint32_t record = 0x0014c050u + lbu(m + 0xb0) + (lwu(0x0015ed84u) << 4);
        if (lbu(record) == 0xff) moby_retire(m);
        return;
    }
    case 1: {  // (0x002cb3d8)
        if (lh(PLAYER + 0x308) != 0) {
            const uint32_t from = sp + 0x10, to = sp + 0x20;
            lq_sq(from, m + 0x10);
            swc1(from + 0x8, fpu_add(lwc1(from + 0x8), 10.0f));
            vec3_sub(V(to), V(lvd::box(lvd::BOXES, lw(pv + 0x70)) + 0x30), V(from));
            vec3_scale(V(to), V(to), u2f(0x3f666666u));
            vec3_add(V(to), V(to), V(from));
            swc1(to + 0x8, fpu_add(lwc1(to + 0x8), 0.5f));
            if (segment_collide(from, to, 2, m, 0) == 0) sb(m + 0x20, 2);
            return;
        }
        if (!hero_in_ship()) return;
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 254.0f) sb(m + 0x20, 3);
        return;
    }
    case 2: {  // (0x002cb4f8): into the box
        const uint32_t joints = sp + 0x30, muzzle = sp + 0x40, aim = sp + 0x10, mark = sp + 0x50, tmp = sp + 0x60;
        if (lh(PLAYER + 0x308) == 0) sb(m + 0x20, 1);
        moby_joint_positions(m, 2, L17_ADDR(0x00161980), joints);
        // a random point of the box: its centre plus each of its three axes scaled by 0 .. 1
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x70));
        vec3_scale(V(tmp), V(box), rand_range_f(0.0f, 1.0f));
        vec3_add(V(mark), V(tmp), V(lvd::box(lvd::BOXES, lw(pv + 0x70)) + 0x30));
        vec3_scale(V(tmp), V(lvd::box(lvd::BOXES, lw(pv + 0x70)) + 0x10), rand_range_f(0.0f, 1.0f));
        vec3_add(V(mark), V(mark), V(tmp));
        vec3_scale(V(tmp), V(lvd::box(lvd::BOXES, lw(pv + 0x70)) + 0x20), rand_range_f(0.0f, 1.0f));
        vec3_add(V(mark), V(mark), V(tmp));
        vec3_sub(V(aim), V(mark), V(joints));
        lq_sq(sp + 0x20, muzzle);
        const float off = cannon_turn(m, pv, aim);
        const int32_t period = frame_scale_i(0x5a);
        if (ee_rem(lw(L17_ADDR(0x0015f5cc)), period) != lw(pv + 0x74)) return;
        const float limit = u2f(0x3d0efa35u);
        if (!(off < limit)) return;
        const uint32_t target = lvd::box(lvd::BOXES, lw(pv + 0x70)) + 0x30;
        const float yaw = atan2_f(fpu_sub(lwc1(target + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x4), lwc1(m + 0x14)));
        if (!(angle_dist(lwc1(m + 0x48), yaw) < limit)) return;
        vec3_set_length(V(joints), V(aim), fpu_mul(dt(), 60.0f));
        vec3_add(V(muzzle), V(joints), V(muzzle));
        eer::sq(sp + 0x70, eer::Q128{});
        muzzle_flash(muzzle, sp + 0x70);
        moby_sound_start(0, 0, m);
        vo_2e94d8_l17(m, sp + 0x20, lvd::box(lvd::BOXES, lw(pv + 0x70)) + 0x30, fpu_mul(dt(), 60.0f));
        return;
    }
    case 3: {  // (0x002cb860): at the hero's ship
        if (!hero_in_ship()) {
            sb(m + 0x20, 1);
            return;
        }
        const uint32_t joints = sp + 0x80, muzzle = sp + 0x90, target = sp + 0x30, aim = sp + 0x10, from = sp + 0x20;
        moby_joint_positions(m, 2, L17_ADDR(0x00161980), joints);
        target_select_any(m, target, 512.0f);
        vec3_sub(V(aim), V(target), V(joints));
        lq_sq(from, muzzle);
        const float off = cannon_turn(m, pv, aim);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x74))) != 0) {
            const float limit = u2f(0x3d0efa35u);
            if (off < limit) {
                const float yaw = atan2_f(fpu_sub(lwc1(target + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x4), lwc1(m + 0x14)));
                if (angle_dist(lwc1(m + 0x48), yaw) < limit && segment_collide(from, target, 2, lwu(PLAYER + 0x15f0), 0) == 0) {
                    const uint32_t far = sp + 0xb0;
                    eer::sq(sp + 0xa0, eer::Q128{});
                    vec3_sub(V(far), V(muzzle), V(joints));
                    vec3_set_length(V(far), V(far), 250.0f);
                    vec3_add(V(far), V(far), V(muzzle));
                    muzzle_flash(muzzle, sp + 0xa0);
                    sw(pv + 0x74, static_cast<uint32_t>(frame_scale_i(0x5a) / 2));
                    moby_sound_start(0, 0, m);
                    vo_2e94d8_l17(m, from, far, fpu_mul(dt(), 60.0f));
                }
            }
        }
        if (lwu(pv + 0x7c) != 0 && rand_mod(0x1f4) == 0) vo_2f4fb8_l17(lw(pv + 0x7c));
        return;
    }
    case 4: {  // (0x002cbb44): broken: its two halves, a burst, gone
        const uint32_t zero = L17_ADDR(0x0015f580);  // (a zero vector)
        debris_spawn(m, 0x63a, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        debris_spawn(m, 0x63b, m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        explosion_burst(m, m + 0x10, -1, 10.0f, 13.0f);
        moby_retire(m);
        return;
    }
    default:
        return;
    }
}

// Spawns a laser glow (particle 0x50) at `pos`: kind 0 the wide red one (15 frames), else the bright core (5 frames),
// `fade` frames into its life (its colour moved toward its end colour); none when it would already be over.
// orig: L17:0x0026fdd0
uint32_t vo_26fdd0_l17(uint32_t pos, uint32_t kind, int32_t fade) {
    if ((kind == 0 ? 0xf : 5) - fade < 0) return 0;
    const uint32_t p = alloc_particle(0x50);
    if (p == 0) return 0;
    lq_sq(p + 0x10, pos);
    sw(p + 0x20, kind);
    sb(p + 0x9, static_cast<uint32_t>(float_to_int(1.0f) + 0x70));
    if (kind == 0) {
        sw(p + 0x4, 0x60ff3020u);
        sb(p + 0x3, 0x48);
        swc1(p + 0xc, 630000.0f);
    } else {
        sw(p + 0x4, 0x80d0a080u);
        sb(p + 0x3, 0x44);
        swc1(p + 0xc, 420000.0f);
    }
    const int32_t life = (kind == 0 ? 0xf : 5) - fade;
    sh(p + 0xa, static_cast<uint32_t>(life));
    if (fade != 0) {
        const float t = fpu_div(int_to_float(static_cast<int16_t>(life) - 1), int_to_float(lh(p + 0xa)));
        sw(p + 0x4, rgba_lerp(kind == 0 ? 0x603f1008u : 0x30ffffffu, lwu(p + 0x4), t));
    }
    sb(p + 0x1, 0);
    sb(p + 0x8, static_cast<uint32_t>(rand_mod(0xff)));
    sb(p + 0x2, lbu(lwu(L17_ADDR(0x001b2a54))));
    return p;
}

// Class 1368, the cannon's shot. Pvars: +0 the velocity, +0x10 the target (pulled toward the hero within 8 units),
// +0x38 the life, +0x3c the shooter, +0x40 the speed. States: 0 flying (glows; a hit on a moby other than its
// shooter, or on the world away from it, bursts it: the hero hurt and the level's record of hits on him noted), 1
// bursting (sparks for 30 frames).
// orig: L17:0x002e8e08
void vo_class1368_update_l17(uint32_t m) {
    const uint8_t st = lbu(m + 0x20);
    const Frame f(272);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    auto burst = [&] {
        sw(pv + 0x38, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sb(m + 0x20, 1);
    };
    if (st == 0) {
        sh(m + 0x32, 0);
        sb(m + 0x31, 0);
        const uint32_t hero = sp + 0x30;
        lq_sq(hero, 0x0013f420u);
        swc1(hero + 0x8, fpu_sub(lwc1(hero + 0x8), 0.5f));
        if (vec2_distance(V(hero), V(pv + 0x10)) < 8.0f) vec3_lerp(V(pv + 0x10), V(hero), V(pv + 0x10), u2f(0x3f7d70a4u));
        if (4.0f < vec2_distance(V(pos), V(pv + 0x10))) {
            const float yaw = atan2_f(fpu_sub(lwc1(pv + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x14), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3f490fdbu)) {
                vec3_sub(V(sp + 0x20), V(pv + 0x10), V(pos));
                vec3_set_length(V(pv), V(sp + 0x20), lwc1(pv + 0x40));
            }
        }
        // the glows: at the next place, half way back, and its tail (20 steps back)
        const uint32_t next = sp, tail = sp + 0x10, mid = sp + 0x40, rec = sp + 0x50;
        vec3_scale(V(sp + 0x20), V(pv), -20.0f);
        vec3_add(V(next), V(pos), V(pv));
        vec3_add(V(tail), V(pos), V(sp + 0x20));
        vec3_lerp(V(mid), V(next), V(pos), 0.5f);
        const int32_t life = lw(pv + 0x38);
        const int32_t fade = life < 60 ? (60 - life) >> 2 : 0;
        vo_26fdd0_l17(next, 1, fade);
        vo_26fdd0_l17(mid, 1, fade);
        vo_26fdd0_l17(next, 0, fade);
        swc1(tail + 0x8, fpu_add(lwc1(tail + 0x8), 0.25f));
        sub_00259888(6.0f, rec, static_cast<int32_t>(m), 1, pv);
        sh(sp + 0x6a, lhu(m + 0xa6));
        lq_sq(pos, next);
        coll_hit_set_moby(0);
        bool met = segment_collide(tail, next, 0, m, rec) != 0;
        if (!met) met = sphere_collide(pos, 0.5f, 0, m) != 0;
        if (met) {
            const uint32_t what = coll_hit_moby();
            if (what == 0) {  // the world: bursts unless still next to its shooter
                if (10.0f < vec2_distance(V(pos), V(lwu(pv + 0x3c) + 0x10))) {
                    V(pos) = coll_hit_point();
                    burst();
                    return;
                }
            } else if (what != lwu(pv + 0x3c)) {
                if (lh(PLAYER + 0x308) != 0 && what == lwu(0x001413d0u) && lhu(0x00141dc0u) == 0) hit_note();
                V(pos) = coll_hit_point();
                moby_sphere_contact(m, pos, 1, 0, 1, 0, 0.75f, 6.0f, 0.0f);
                if (lh(PLAYER + 0x308) != 0 && coll_hit_moby() == lwu(PLAYER + 0x2080) && lhu(0x00141dc0u) == 0) hit_note();
                burst();
                return;
            }
        }
        // flying on: its life cut short outside the fleet's space, gone at its end or far outside
        if (frame_scale_i(0x46) < lw(pv + 0x38)) {
            const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
            if (890.0f < x || 890.0f < y || 890.0f < z || x < 80.0f || y < 80.0f || z < 80.0f)
                sw(pv + 0x38, static_cast<uint32_t>(frame_scale_i(0x46)));
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x38))) != 0) {
            moby_retire(m);
            return;
        }
        const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
        if (968.0f < x || 968.0f < y || 968.0f < z || x < 10.0f || y < 10.0f || z < 10.0f) moby_retire(m);
    } else if (st == 1) {
        const float speed = rand_range_f(fpu_add(dt(), dt()), fpu_mul(dt(), 10.0f));
        const float a = rand_range_f(u2f(0xc0490fdbu), u2f(0x40490fdbu));
        const float b = rand_range_f(u2f(0xc0490fdbu), u2f(0x40490fdbu));
        vec_from_spherical_v(V(sp), speed, a, b);
        const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
        spawn_particle_3c(pos, sp, 0x60ff3020u, 0xf, frame, 0, 3.0f);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x38))) != 0) moby_retire(m);
    }
}

}  // namespace rac1
