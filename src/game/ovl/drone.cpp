// Ported overlay code: the Drone Device's drones (class 479, spawned by use_agents 0x2cdcc0 through 0x2ce1f0): their
// update 0x2ce358 with the swarm's target search 0x2cdf28, their two trails (particle type 0x37, spawned by 0x2725d8 from
// 0x2cde78) and the trails' update 0x2726d8.
//
// The swarm lives in the hero struct (P = 0x13f350): +0x1ff4 the drones' turn counter this frame, +0x1ff6 the number of
// drones, +0x1ff7 how many of them are not dying, +0x1ff8 the swarm's spin, +0x1ffc nine words: the targets taken, +0x2020
// six words: the drones (each one's slot index in its pvar +0x54).
// A drone's pvar: +0x00 its velocity, +0x10 the target's point, +0x20 its distance from the hero, +0x24 / +0x28 its pitch /
// heading around the hero, +0x30 the target moby (+0x48 its class, to tell a reused moby), +0x34 / +0x40 the pitch / heading
// it heads for, +0x3c its height, +0x4c / +0x50 its two trail particles, +0x54 its slot, +0x56 its hit points against
// class 0x4d6, +0x58 / +0x5c the turn rates of its heading / pitch.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapon_classes.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;              // the hero struct
constexpr uint32_t HERO_POS = P + 0x80u;
constexpr uint32_t SWARM_TURN = P + 0x1ff4u;     // u8
constexpr uint32_t SWARM_COUNT = P + 0x1ff6u;    // u8
constexpr uint32_t SWARM_ACTIVE = P + 0x1ff7u;   // u8
constexpr uint32_t SWARM_SPIN = P + 0x1ff8u;     // float
constexpr uint32_t SWARM_TARGETS = P + 0x1ffcu;  // 9 words
constexpr uint32_t SWARM_SLOTS = P + 0x2020u;    // 6 words
constexpr uint32_t HERO_MOBY = P + 0x2080u;
constexpr uint32_t HERO_MODE = P + 0x2084u;
constexpr uint32_t FRAME_DT = 0x0015ed60u;      // the frame's time step (1.0 at 60 Hz)
constexpr uint32_t FRAME_SCALE = 0x0015ed6cu;
constexpr uint32_t FRAME_SCALE_2 = 0x0015ed70u;

constexpr OvConst<0x0015f5c4u> GAME_MODE{};
constexpr OvConst<0x0015ffe4u> MOBY_LIST{};  // the live mobys' list (linked through +0x28)
constexpr OvConst<0x00173e40u> HIT{};        // the last collision's record: +0x18 the moby hit, +0x30 the point
constexpr OvConst<0x00166dc0u> WALK_POS{};   // walker_move's resolved position, less its probe offset
constexpr OvConst<0x00166fd0u> WALK_OFS_A{};
constexpr OvConst<0x00166ff0u> WALK_OFS_B{};
constexpr OvConst<0x001b215cu> TRAIL_TEX{};

constexpr float PI = std::bit_cast<float>(0x40490fdbu);
constexpr float TWO_PI = std::bit_cast<float>(0x40c90fdbu);

bool moby_dying(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    return st == 0xfe || st == 0xfd;
}

// a target's point: its position, raised by its pvar word 0's +0x10 when it has one
Vec4 target_point(uint32_t t) {
    const uint32_t pw = moby_pvar_word0(t);
    Vec4 p = V(t + 0x10);
    if (pw != 0) p.z = fpu_add(p.z, lwc1(pw + 0x10));
    return p;
}

// The hit on the moby the drone ran into (of class kind `kind`): a record of 1.0 damage along the drone's velocity. A kind 5
// victim also blows the drone up (state 4), unless it is class 0x4d6 and the drone has hit points against it left (pvar
// +0x56): they go down instead.
// orig: (helper) repeated in 0x002ce358's states 2 and 3
void drone_hit(uint32_t m, uint32_t pv, uint32_t rec, uint32_t kind) {
    hit_rec_init(rec, m, 0x10000u, 1.0f);
    vec2_set_length(V(rec), V(pv), 1.0f);
    sb(rec + 0x19, 2);
    swc1(rec + 0x08, 1.0f);
    swc1(rec + 0x0c, u2f(0x45afdf66u));
    sb(rec + 0x18, 1);
    sh(rec + 0x1a, lhu(m + 0xa6));
    moby_hit_record(lwu(HIT + 0x18u), rec);
    if (kind != 5) return;
    const uint32_t victim = lwu(HIT + 0x18u);
    const int32_t hp = lh(pv + 0x56);
    if (victim != 0 && lh(victim + 0xa6) == 0x4d6 && hp > 0) {
        const int32_t dmg = float_to_int(lwc1(rec + 0x1c));
        sh(pv + 0x56, static_cast<uint32_t>(dmg <= 0 ? hp - 1 : hp - float_to_int(lwc1(rec + 0x1c))));
        return;
    }
    explosion_effect(m, pv, 0, 0, 1, 1, 1, 1, 1, -1, 0, 0.0f, 0.0f, u2f(0x3ecccccdu), u2f(0x3e4ccccdu), 4.0f, 1.0f, 8.0f);
    sb(m + 0x20, 4);
}

// The drone's heading and pitch set straight from the step it made (from `from` to its position).
// orig: (helper) the tail of 0x002ce358's state 3 (state 2 ends the same way, with turn rates)
void face_step(uint32_t m, const Vec4& from) {
    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(m + 0x10), from.x), fpu_sub(lwc1(m + 0x14), from.y)));
    const float d = vec2_distance(from, V(m + 0x10));
    swc1(m + 0x44, -atan2_f(d, fpu_sub(lwc1(m + 0x18), from.z)));
}

// The swarm leader's work, once per frame (by the first drone to run after all had their turn): the drone list tidied,
// the targets taken listed, and a new target given to the free drone closest to it.
// orig: (helper) part of 0x002ce358
void swarm_frame(uint32_t m) {
    sb(SWARM_TURN, 1);
    sb(SWARM_ACTIVE, 0);
    swc1(SWARM_SPIN, angle_add(lwc1(SWARM_SPIN), fpu_mul(lwc1(FRAME_DT), u2f(0x3d4ccccdu))));
    int32_t live = 0;
    for (uint32_t k = 0; k < 6; ++k) {
        const uint32_t slot = SWARM_SLOTS + k * 4;
        const uint32_t d = lwu(slot);
        if (d == 0) continue;
        if (lh(d + 0xa6) != 0x1df || moby_dying(d)) {
            sw(slot, 0);
            continue;
        }
        sw(SWARM_TARGETS + static_cast<uint32_t>(live) * 4, lwu(lwu(d + 0x78) + 0x30));
        ++live;
        if (lbu(d + 0x20) < 4) sb(SWARM_ACTIVE, lbu(SWARM_ACTIVE) + 1u);
    }
    for (int32_t i = live; i < 9; ++i) sw(SWARM_TARGETS + static_cast<uint32_t>(i) * 4, 0);

    const uint32_t target = drone_target_find(HERO_POS, SWARM_TARGETS);
    if (target == 0) return;
    Vec4 aim = target_point(target);
    uint32_t best = 0;
    float best_dist = 1000000.0f;
    for (uint32_t k = 0; k < 6; ++k) {
        const uint32_t d = lwu(SWARM_SLOTS + k * 4);
        if (d == 0) continue;
        const uint32_t busy = lwu(lwu(d + 0x78) + 0x30);
        if (busy != 0 && !moby_dying(busy)) continue;
        const float dist = vec2_distance(V(d + 0x10), aim);
        if (dist < best_dist) {
            best = d;
            best_dist = dist;
        }
    }
    if (best == 0) return;
    const uint32_t bp = lwu(best + 0x78);
    sw(bp + 0x30, target);
    sw(bp + 0x48, static_cast<uint32_t>(static_cast<int32_t>(lh(target + 0xa6))));
    aim = target_point(target);
    V(bp + 0x10) = aim;
    sb(best + 0x20, 3);
    const int32_t r = ee_rem(libc_rand(), 3);
    moby_sound_start(r + (0 < r ? 1 : 0), 0, m);
    sw(SWARM_TARGETS + static_cast<uint32_t>(live) * 4, lwu(bp + 0x30));
}
}  // namespace

// The swarm's next target near `from`: the closest live enemy (class kind 5 flagged 0x1000 in +0x34, 7 or 8; not one
// of the nine words at `taken`, not invisible but for classes 0x350 / 0x31) within its kind's reach (class 0x4d6: in
// front of it), with nothing busy in its hook +0x10 and a clear line to it. 0 for none.
// orig: 0x002cdf28
uint32_t drone_target_find(uint32_t from, uint32_t taken) {
    uint32_t best = 0;
    float best_dist = u2f(0x4e6e6b28u);
    for (uint32_t t = lwu(MOBY_LIST); t != 0; t = lwu(t + 0x28)) {
        const uint32_t cls = lwu(t + 0x24);
        const uint32_t kind = cls != 0 ? lbu(cls + 0x46) : 0u;
        if (moby_dying(t)) continue;
        if (kind != 5 && kind != 7 && kind != 8) continue;
        if (taken != 0) {
            bool is_taken = false;
            for (uint32_t i = 0; i < 9 && !is_taken; ++i) is_taken = lwu(taken + i * 4) == t;
            if (is_taken) continue;
        }
        if (kind == 5 && (lhu(t + 0x34) & 0x1000) == 0) continue;
        if (lbu(t + 0x31) == 0 && lh(t + 0xa6) != 0x350 && lh(t + 0xa6) != 0x31) continue;
        const uint32_t pos = t + 0x10;
        const float dist = vec3_distance(V(from), V(pos));
        if (lh(t + 0xa6) == 0x4d6) {
            Vec4 ahead{};
            vec3_set_length(ahead, V(lwu(t + 0x78)), dist);
            vec3_add(ahead, ahead, V(pos));
            if (dist < vec3_distance(V(from), ahead)) continue;
        } else {
            float reach = 0.0f;
            bool limited = true;
            if (kind == 7) reach = u2f(0x40a9999au);
            else if (kind == 5) reach = 4.0f;
            else if (kind == 8) reach = 6.0f;
            else limited = false;
            if (limited && reach < dist) continue;
        }
        if (!(dist < best_dist)) continue;
        const uint32_t hook = class_hook_10(t);
        if (hook != 0 && lh(hook + 0x68) > 0) continue;
        const uint32_t pw = moby_pvar_word0(t);
        Vec4 aim = V(pos);
        if (pw != 0) aim.z = fpu_add(aim.z, fpu_add(lwc1(pw + 0x10), u2f(0x3d4ccccdu)));
        if (segment_collide_v(V(from), aim, 6, t, nullptr) != 0) continue;
        best_dist = dist;
        best = t;
    }
    return best;
}

// A trail particle (type 0x37) on moby `m`'s chain point `id`: colour `color`, `size` shrinking by `shrink`.
// orig: 0x002725d8
uint32_t particle_37_spawn(uint32_t m, uint32_t id, uint32_t color, uint32_t color2, float size, float shrink) {
    const uint32_t p = alloc_particle(0x37);
    if (p == 0) return 0;
    sw(p + 0x04, color);
    sw(p + 0x0c, color2);
    sb(p + 0x09, static_cast<uint32_t>(float_to_int(u2f(0x3ecccccdu)) + 0x20));
    sb(p + 0x01, 3);
    sb(p + 0x03, 0x48);
    sb(p + 0x02, lbu(lwu(TRAIL_TEX)));
    moby_chain_point_v(m, id, V(p + 0x10));
    V(p + 0x20) = V(p + 0x10);
    swc1(p + 0x2c, shrink);
    swc1(p + 0x1c, size);
    sw(p + 0x30, m);
    swc1(p + 0x3c, size);
    sw(p + 0x38, static_cast<uint32_t>(static_cast<int32_t>(lh(m + 0xa6))));
    sw(p + 0x34, id);
    return p;
}

// The drone's two trails (pvar +0x4c / +0x50, on its chain points 0 and 1), made when missing.
// orig: 0x002cde78
void drone_trails_attach(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lwu(pv + 0x4c) == 0) sw(pv + 0x4c, particle_37_spawn(m, 0, 0x80808080u, 0x10808080u, u2f(0x3d4ccccdu), u2f(0x3c23d70au)));
    if (lwu(pv + 0x50) == 0) sw(pv + 0x50, particle_37_spawn(m, 1, 0x80808080u, 0x10808080u, u2f(0x3d4ccccdu), u2f(0x3c23d70au)));
}

// The trail particle follows its moby's chain point, its tail a little behind (with a random jitter) and its size
// flickering; gone with the moby.
// orig: 0x002726d8
void particle_37_update(uint32_t p) {
    const uint32_t owner = lwu(p + 0x30);
    if (owner == 0 || lh(owner + 0xa6) != lw(p + 0x38) || moby_dying(owner)) {
        free_particle(p);
        return;
    }
    const float flicker = rand_range_f(0.5f, 1.0f);
    const float size = fpu_mul(lwc1(p + 0x3c), flicker);
    const float shrink = lwc1(p + 0x2c);
    Vec4 back = V(p + 0x10);
    moby_chain_point_v(lwu(p + 0x30), lwu(p + 0x34), V(p + 0x10));
    vec3_sub(back, back, V(p + 0x10));
    const float jitter = rand_range_f(u2f(0xbcac0831u), u2f(0x3cac0831u));
    const float len = vec3_length(back) < 0.0625f ? fpu_add(jitter, 0.0625f) : fpu_add(vec3_length(back), jitter);
    vec3_set_length(back, back, len);
    vec3_add(V(p + 0x20), V(p + 0x10), back);
    swc1(p + 0x2c, shrink);
    swc1(p + 0x1c, size);
}

// Class 479, a drone of the Drone Device: it circles the hero (state 2) until the swarm leader gives it a target, then
// flies into it (3), fades out (4) and is retired (5).
// orig: 0x002ce358
void drone_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t mode = lw(GAME_MODE);
    if (mode == 2 || mode == 5 || mode == 6 || lw(HERO_MODE) == 0x32) {
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        return;
    }
    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);

    if (lbu(m + 0x20) < 5) {
        const uint32_t turn = lbu(SWARM_TURN) + 1u;
        sb(SWARM_TURN, turn);
        if (lbu(SWARM_COUNT) < (turn & 0xffu)) swarm_frame(m);

        // the place the drone circles at: by its turn among the swarm
        const int32_t count = lbu(SWARM_COUNT);
        const int32_t idx = lbu(SWARM_TURN);
        const int32_t n = count + 1;
        const float fcount = static_cast<float>(count != 0 ? count : 1);
        swc1(pv + 0x40, fpu_sub(fpu_mul(fpu_div(int_to_float(idx), fcount), TWO_PI), PI));
        const int32_t spread = ee_rem(idx * ee_div(n, 5), n);
        swc1(pv + 0x34, fpu_mul(fpu_sub(fpu_mul(fpu_div(int_to_float(spread), fcount), TWO_PI), PI), 0.25f));

        // hit by something of kind 5..8: it dies (against class 0x4d6, once its hit points' timer runs out)
        const uint32_t hit = sub_0025a420(m, 1, 0);
        if (hit != 0) {
            const uint32_t by = lwu(hit + 0x20);
            int32_t kind = 0;
            if (by != 0 && lwu(by + 0x24) != 0) kind = lh(lwu(by + 0x24) + 0x46);
            if (kind >= 5 && kind < 9) {
                if (by == 0 || lh(by + 0xa6) != 0x4d6 || timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x56))) != 0)
                    sb(m + 0x20, 4);
            }
        }
    }

    const uint32_t frame = ee_stack_alloc(0x60);
    const uint32_t want = frame;          // the walk's step (walker_move reads it by address)
    const uint32_t probe = frame + 0x10;  // the collision probe's point
    const uint32_t rec = frame + 0x20;    // the hit record
    switch (lbu(m + 0x20)) {
    case 2: {  // circling the hero
        sw(m + 0x94, 0);
        const Vec4 from = V(m + 0x10);
        drone_trails_attach(m);
        const float ease = fpu_mul(lwc1(FRAME_DT), u2f(0x3d4ccccdu));
        const float scale = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(scale, fpu_mul(fpu_sub(lwc1(lwu(m + 0x24) + 0x24), scale), ease)));
        const float radius = lwc1(pv + 0x20);
        swc1(pv + 0x20, fpu_add(radius, fpu_mul(fpu_sub(1.0f, radius), ease)));
        const float heading_to = angle_add(lwc1(pv + 0x40), lwc1(SWARM_SPIN));
        const float dh = angle_sub(heading_to, lwc1(pv + 0x28));
        swc1(pv + 0x28, angle_add(lwc1(pv + 0x28), fpu_mul(dh, fpu_mul(lwc1(FRAME_DT), u2f(0x3d4ccccdu)))));
        const float dp = angle_sub(lwc1(pv + 0x34), lwc1(pv + 0x24));
        swc1(pv + 0x24, angle_add(lwc1(pv + 0x24), fpu_mul(dp, fpu_mul(lwc1(FRAME_DT), u2f(0x3c23d70au)))));

        Vec4 off{};
        off.x = fpu_mul(cos_vu(lwc1(pv + 0x28)), lwc1(pv + 0x20));
        off.y = fpu_mul(sin_vu(lwc1(pv + 0x28)), lwc1(pv + 0x20));
        off.z = 0.0f;
        const Vec4 angles{0.0f, lwc1(pv + 0x24), 0.0f, 0.0f};
        Mat4 rot{};
        euler_to_mat4_vu(rot, angles);
        mat4_transform(off, off, rot);

        Vec4& w = V(want);
        vec3_add(w, V(HERO_POS), V(lwu(HERO_MOBY) + 0xe0));
        w.x = fpu_add(w.x, off.x);
        w.y = fpu_add(w.y, off.y);
        const float height = lwc1(pv + 0x3c);
        swc1(pv + 0x3c, fpu_add(height, fpu_mul(fpu_sub(w.z, height), fpu_mul(lwc1(FRAME_DT), u2f(0x3d4ccccdu)))));
        w.z = fpu_add(lwc1(pv + 0x3c), off.z);
        vec3_sub(w, w, V(m + 0x10));
        vec3_clamp_length_vu(w, w, fpu_mul(lwc1(FRAME_SCALE), 10.0f));
        if ((walker_move(m, want, 0, 0.0f, u2f(0x3e3851ecu), 0.0f) & 1u) != 0 && lbu(m + 0x31) == 0) {
            vec3_add(V(m + 0x10), V(WALK_POS), V(WALK_OFS_B));
            vec3_sub(V(m + 0x10), V(m + 0x10), V(WALK_OFS_A));
        }
        V(probe) = V(m + 0x10);
        if (sphere_collide(probe, u2f(0x3e4ccccdu), 0, m) != 0) {
            // a live moby of kind 5, 7 or 8 that is not the hero (class 0): a hit
            const uint32_t h = lwu(HIT + 0x18u);
            if (h != 0 && lh(h + 0xa6) != 0 && !moby_dying(h)) {
                const uint32_t hcls = lwu(h + 0x24);
                const uint32_t kind = hcls != 0 ? lbu(hcls + 0x46) : 0u;
                if (kind == 5 || kind == 7 || kind == 8) drone_hit(m, pv, rec, kind);
            }
        }
        vec3_sub(V(pv), V(m + 0x10), from);
        const float heading = atan2_f(fpu_sub(lwc1(m + 0x10), from.x), fpu_sub(lwc1(m + 0x14), from.y));
        const float d = vec2_distance(from, V(m + 0x10));
        const float pitch = -atan2_f(d, fpu_sub(lwc1(m + 0x18), from.z));
        {
            const float k = lwc1(FRAME_SCALE_2);
            swc1(m + 0x48, angle_seek_rate(pv + 0x58, lwc1(m + 0x48), heading, fpu_mul(k, TWO_PI), fpu_mul(k, PI),
                                           fpu_mul(lwc1(FRAME_SCALE), TWO_PI)));
        }
        {
            const float k = lwc1(FRAME_SCALE_2);
            swc1(m + 0x44, angle_seek_rate(pv + 0x5c, lwc1(m + 0x44), pitch, fpu_mul(k, TWO_PI), fpu_mul(k, PI),
                                           fpu_mul(lwc1(FRAME_SCALE), TWO_PI)));
        }
        break;
    }
    case 3: {  // flying into its target
        const uint32_t cls = lwu(m + 0x24);
        sw(m + 0x94, lwu(cls + 0x10));
        const Vec4 from = V(m + 0x10);
        const float scale = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(scale, fpu_mul(fpu_sub(lwc1(cls + 0x24), scale), fpu_mul(lwc1(FRAME_DT), u2f(0x3d4ccccdu)))));
        sw(m + 0x94, lwu(cls + 0x10));
        drone_trails_attach(m);
        const uint32_t t = lwu(pv + 0x30);
        if (t == 0 || lh(t + 0xa6) != lw(pv + 0x48) || moby_dying(t)) {
            sb(m + 0x20, 2);
            sw(pv + 0x48, 0xffffffffu);
            sw(pv + 0x30, 0);
            face_step(m, from);
            break;
        }
        const Vec4 tp = target_point(t);
        Vec4 lead{};
        vec3_sub(lead, tp, V(pv + 0x10));
        vec3_scale(lead, lead, u2f(0x40466666u));
        Vec4 goal{};
        vec3_add(goal, tp, lead);
        vec3_sub(V(pv), goal, V(m + 0x10));
        vec3_scale(V(pv), V(pv), fpu_add(fpu_mul(lwc1(FRAME_DT), u2f(0xbf4ccccdu)), 1.0f));
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
        V(pv + 0x10) = tp;
        V(probe) = V(m + 0x10);
        if (sphere_collide(probe, 0.25f, 0, m) != 0) {
            const uint32_t h = lwu(HIT + 0x18u);
            if (h == 0 || moby_dying(h) || h != lwu(pv + 0x30) || lh(h + 0xa6) == 0) {
                V(m + 0x10) = V(HIT + 0x30u);  // (stopped at the contact)
            } else {
                const uint32_t hcls = lwu(h + 0x24);
                drone_hit(m, pv, rec, hcls != 0 ? lbu(hcls + 0x46) : 0u);
            }
        }
        face_step(m, from);
        break;
    }
    case 4: {  // fading out, still drawn toward its target
        const uint32_t t = lwu(pv + 0x30);
        if (t == 0 || lh(t + 0xa6) != lw(pv + 0x48) || moby_dying(t)) {
            sw(pv + 0x30, 0);
        } else {
            const Vec4 tp = target_point(t);
            vec3_sub(V(pv), tp, V(m + 0x10));
            vec3_scale(V(pv), V(pv), fpu_add(fpu_mul(lwc1(FRAME_DT), u2f(0xbf4ccccdu)), 1.0f));
            vec3_clamp_length_vu(V(pv), V(pv), fpu_mul(lwc1(FRAME_DT), u2f(0x3f666666u)));
            vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
        }
        const uint32_t alpha = lbu(m + 0x23) - 4u;
        sb(m + 0x23, alpha);
        if ((alpha & 0xffu) < 6) sb(m + 0x20, 5);
        break;
    }
    case 5:  // gone: off the swarm
        sb(SWARM_COUNT, lbu(SWARM_COUNT) - 1u);
        sw(SWARM_SLOTS + static_cast<uint32_t>(static_cast<int32_t>(lh(pv + 0x54))) * 4, 0);
        moby_retire(m);
        ee_stack_free(0x60);
        return;
    default:
        break;
    }
    ee_stack_free(0x60);
    floor_light_add(m, u2f(0x3e2e147bu));
}

}  // namespace rac1
