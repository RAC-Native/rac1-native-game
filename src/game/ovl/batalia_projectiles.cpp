// Batalia (level 8): the two explosive projectiles the fort's enemies send at Ratchet, hand-ported: class 248, the grenade
// class 333 throws from its hand (spawned by bat_2d22e0: thrower, place, velocity), and class 425, the shell class 253
// fires from its gun (spawned by bat_2dc8a0 the same way). Both fly under gravity, test their path with segment_collide
// and blow up on what they hit or when their fuse runs out. Addresses are level 8's (`orig: L8:`); its own data and code
// are L8_ADDR, the base ELF's are bare (the frame time 0x15ed6c and its square 0x15ed70). The last collision query's
// block is level 8's 0x174640 (Veldin's 0x173e40): the moby hit +0x18, the flags +0x1c, the contact point +0x20 and the
// normal +0x40 through collision.h (OV() of Veldin's address gives level 8's on the three discs).
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the hit record both blasts hand mobys_in_sphere: the push (+0 .. +0xf) cleared, the source moby +0x10, the flags
// 0x810001 +0x14, the scale 1.0 +0x1c, the word +0x20 cleared (+0x18 left as the fresh frame has it)
void blast_record(uint32_t rec, uint32_t m) {
    sw(rec + 0x10, m);
    sw(rec + 0x14, 0x810001u);
    swc1(rec + 0x1c, 1.0f);
    vec_clear(V(rec));
    sw(rec + 0x20, 0);
}
}  // namespace

// Class 248, the grenade class 333 throws (it sets the flags +0xbc |= 3 and the fuse to 180 frames). pvars: +0 .. +0xf the
// colour pulse record (color_pulse_start / _update; +7 its strength), +0x10 the velocity, +0x20 the fuse (300 frames
// from the spawn, 180 after each bounce, 120 once at rest), +0x24 the bounces so far, +0x28 254 / the rest fuse (not read
// here), +0x2c 1.0, +0x30 the thrower. Flags +0xbc: bit 0 it bounces off the level (else it blows up on it), bit 1 its
// blast is the burst (0x63) rather than the explosion (0x62). States: 1 in flight (it spins with its speed, trails smoke
// until its first bounce; a moby touched -- not the thrower before the first bounce -- or the fuse sets off the blast);
// 2 at rest on the ground, blinking 60, 40 and 20 frames before the fuse ends, then the burst; 0x62 the explosion
// (explosion_effect, mobys within 1 hit) and 0x63 the burst (explosion_burst, the same hit), both retiring it; 0x37 (set
// elsewhere) skips the touch test and does nothing. Every state but the two blasts lights the floor under it.
// orig: L8:0x002d23f0
void bat_class248_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x150);
    const uint32_t pos = m + 0x10, vel = pv + 0x10;

    // the blast its flags choose
    const auto blast_kind = [&]() { return (lbu(m + 0xbc) & 2u) != 0 ? 0x63u : 0x62u; };
    // set off where it hit: the state `st`, at the contact point, its velocity bounced off the normal, two frames' worth
    const auto set_off = [&](uint32_t st) {
        sb(m + 0x20, st);
        coll_vec_store(pos, coll_hit_point());
        vec3_reflect(V(vel), V(vel), coll_hit_normal());
        vec3_set_length(V(vel), V(vel), fpu_add(dt(), dt()));
    };

    // a moby touched (not the thrower before the first bounce) sets it off
    if (lbu(m + 0x20) != 0x37 && sphere_collide(pos, 0.5f, 0x11, m) != 0) {
        const uint32_t hit = coll_hit_moby();
        if (hit != 0 && (hit != lwu(pv + 0x30) || lw(pv + 0x24) != 0)) set_off(blast_kind());
    }

    // in flight: the move, the path test, the bounce
    const auto fly = [&]() {
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(vec3_length(V(vel)), 0.5f)));
        if (lw(pv + 0x24) == 0) {
            // the smoke trail until the first bounce: one of the four colours 0x161868 at random, a random direction
            sd(sp + 0x70, ld(L8_ADDR(0x00161868)));
            sd(sp + 0x78, ld(L8_ADDR(0x00161870)));
            sd(sp + 0x90, 0);
            sd(sp + 0x98, 0);
            swc1(sp + 0x90, rand_range_f(-1.0f, 1.0f));
            swc1(sp + 0x94, rand_range_f(-1.0f, 1.0f));
            swc1(sp + 0x98, rand_range_f(-1.0f, 1.0f));
            lq_sq(sp + 0x80, sp + 0x90);
            vec3_set_length(V(sp + 0x80), V(sp + 0x80), dt());
            const uint32_t pick = static_cast<uint32_t>(rand_mod(4));
            spawn_particle_4(pos, sp + 0x80, lwu(sp + 0x70 + (pick << 2)), 0x4fff,
                             static_cast<uint32_t>(frame_scale_i(0x2d)), 0x32, 0x78, 1);
        }
        // the path from its place to the next, both 0.16 lower; gravity (0x161860) on the velocity
        lq_sq(sp + 0x20, pos);
        swc1(sp + 0x28, fpu_sub(lwc1(sp + 0x28), 0.16f));
        vec3_add(V(pos), V(pos), V(vel));
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(lwc1(L8_ADDR(0x00161860)), dt_sq())));
        lq_sq(sp + 0x30, pos);
        swc1(sp + 0x38, fpu_sub(lwc1(sp + 0x38), 0.16f));
        // (the original also fills a hit record at sp+0x40 .. +0x63 -- the velocity, the moby, 0x810000, 1.0, the state --
        // that it does not pass: segment_collide gets none)
        if (segment_collide_v(V(sp + 0x20), V(sp + 0x30), 0, m, nullptr) == 0) {
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) != 0) sb(m + 0x20, blast_kind());
            return;
        }
        const uint32_t hit = coll_hit_moby();
        if (hit != 0) {
            if (hit == lwu(pv + 0x30) && lw(pv + 0x24) == 0) return;  // (its thrower, before the first bounce)
            set_off(blast_kind());
            return;
        }
        if (static_cast<int32_t>(coll_hit_flags()) <= 0) return;  // (no triangle of the level)
        if ((lbu(m + 0xbc) & 1u) == 0) {
            set_off(0x62);
            return;
        }
        // a bounce: back to the contact 0.16 up, the velocity reflected, pushed sideways at random, slowed to 0.4
        moby_sound_start(1, 0, m);
        coll_vec_store(pos, coll_hit_point());
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.16f));
        vec3_reflect(V(vel), V(vel), coll_hit_normal());
        const uint32_t side = sp + 0x70;
        vec3_cross_ba(V(side), V(vel), coll_hit_normal());
        const float spread = fpu_mul(dt(), 1.5f);
        vec3_set_length(V(side), V(side), rand_range_f(-spread, spread));
        vec3_add(V(vel), V(vel), V(side));
        vec3_scale(V(vel), V(vel), 0.4f);
        sw(pv + 0x24, static_cast<uint32_t>(lw(pv + 0x24) + 1));
        sw(pv + 0x20, static_cast<uint32_t>(frame_scale_i(0xb4)));
        if (vec3_length(V(vel)) < dt()) {
            // too slow to bounce again: at rest, 120 frames left
            sb(m + 0x20, 2);
            const int32_t rest = frame_scale_i(0x78);
            sw(pv + 0x20, static_cast<uint32_t>(rest));
            swc1(pv + 0x28, fpu_div(254.0f, static_cast<float>(rest)));
            vec_clear(V(vel));
        }
    };

    switch (lbu(m + 0x20)) {
    case 1:
        fly();
        break;
    case 2:
        // at rest: the fuse; a blink 60, 40 and 20 frames before its end
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) != 0) {
            sb(m + 0x20, 0x63);
        } else {
            const int32_t t = lw(pv + 0x20);
            if (t == frame_scale_i(0x3c) || t == frame_scale_i(0x28) || t == frame_scale_i(0x14)) {
                sb(pv + 0x7, 0xfa);
                color_pulse_start(m, pv);
            }
        }
        color_pulse_update(m, pv);
        break;
    case 0x62:
        // the explosion
        moby_sound_start(0, 0, m);
        blast_record(sp + 0x20, m);
        mobys_in_sphere(pos, 1.0f, 0x10, m, sp + 0x20);
        explosion_effect(m, L8_ADDR(0x0015f580), 0, 7, 0xa, 0x14, -1, 0, 7, -1, 0, 0.0f, 0.0f, 3.0f, 1.7f, 4.0f, 1.0f, 7.0f);
        moby_retire(m);
        ee_stack_free(0x150);
        return;
    case 0x63:
        // the burst
        moby_sound_start(0, 0, m);
        blast_record(sp + 0x20, m);
        mobys_in_sphere(pos, 1.0f, 0x10, m, sp + 0x20);
        explosion_burst(m, pos, -1, 0.5f, 13.0f);
        moby_retire(m);
        ee_stack_free(0x150);
        return;
    default:
        break;
    }
    floor_light_add(m, 0.2f);
    ee_stack_free(0x150);
}

// Class 425, the shell class 253 fires (300 frames of fuse). pvars: +0 the velocity, +0x10 the fuse, +0x14 0 from the
// spawn (not 0: it may hit its owner too), +0x1c 1.0, +0x20 the owner. States: 1 in flight (gravity 10.8; a moby touched
// that is not its owner, or a hit of its path while it has an owner -- not the owner itself --, or the fuse sets it off);
// 0x63 the blast: mobys within 1 hit (flags 9), the sound, 50 particles in turn of four kinds (smoke, nothing, flame,
// glowing smoke thrown out), and it retires.
// orig: L8:0x002dc9a8
void bat_class425_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x140);
    const uint32_t pos = m + 0x10;

    // a moby touched that is not its owner (or any, with +0x14 set) sets it off
    if (mobys_in_sphere(pos, 0.5f, 0, m, 0) != 0) {
        const uint32_t hit = coll_hit_moby();
        if (hit != 0 && (hit != lwu(pv + 0x20) || lw(pv + 0x14) != 0)) sb(m + 0x20, 0x63);
    }

    switch (lbu(m + 0x20)) {
    case 1: {
        // in flight: the move, gravity, the path from the last place to the new one
        lq_sq(sp, pos);
        vec3_add(V(pos), V(pos), V(pv));
        swc1(pv + 0x8, fpu_sub(lwc1(pv + 0x8), fpu_mul(dt_sq(), 10.8f)));
        lq_sq(sp + 0x10, pos);
        // (the original also fills a hit record at sp+0x20 .. +0x43 -- the velocity, the moby, 0x810000, 1.0, the state --
        // that it does not pass: segment_collide gets none)
        if (segment_collide_v(V(sp), V(sp + 0x10), 0, m, nullptr) == 0) {
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x10))) != 0) sb(m + 0x20, 0x63);
            break;
        }
        const uint32_t owner = lwu(pv + 0x20);
        if (owner == 0 || coll_hit_moby() == owner) break;  // (no owner: the path's hits are ignored)
        sb(m + 0x20, 0x63);
        coll_vec_store(pos, coll_hit_point());
        vec3_reflect(V(pv), V(pv), coll_hit_normal());
        vec3_set_length(V(pv), V(pv), fpu_add(dt(), dt()));
        break;
    }
    case 0x63: {
        // the blast. sp+0 .. +0xc: the four kinds' highest rise speeds (14, 22, 12 a frame, the last 0)
        swc1(sp + 0x10, fpu_mul(dt(), 14.0f));
        swc1(sp + 0x14, fpu_mul(dt(), 22.0f));
        sw(sp + 0x1c, 0);
        swc1(sp + 0x18, fpu_mul(dt(), 12.0f));
        sd(sp + 0x0, ld(sp + 0x10));
        sd(sp + 0x8, ld(sp + 0x18));
        blast_record(sp + 0x20, m);
        mobys_in_sphere(pos, 1.0f, 9, m, sp + 0x20);
        moby_sound_start(0, 0, m);
        const uint32_t at = sp + 0x10, vel = sp + 0x50, rnd = sp + 0x60;
        for (int32_t i = 0; i < 50; ++i) {
            const uint32_t kind = static_cast<uint32_t>(i) & 3u;  // (i % 4)
            // the velocity: half a unit a frame either way across, 4 up to the kind's top speed
            sd(rnd, 0);
            sd(rnd + 0x8, 0);
            swc1(rnd, rand_range_f(fpu_mul(dt(), -0.5f), fpu_mul(dt(), 0.5f)));
            swc1(rnd + 0x4, rand_range_f(fpu_mul(dt(), -0.5f), fpu_mul(dt(), 0.5f)));
            swc1(rnd + 0x8, rand_range_f(fpu_mul(dt(), 4.0f), lwc1(sp + (kind << 2))));
            lq_sq(vel, rnd);
            // the place: within 0.25 of the shell across, lower (and slower to rise) the farther out
            const float r = rand_range_f(0.0f, 0.25f);
            const float a = rand_angle();
            swc1(at, fpu_mul(cos_vu(a), r));
            swc1(at + 0x8, 0.0f);
            swc1(at + 0x4, fpu_mul(sin_vu(a), r));
            vec3_add(V(at), V(at), V(pos));
            const float slower = fpu_mul(fpu_mul(r, dt()), 8.0f);
            swc1(at + 0x8, fpu_sub(lwc1(at + 0x8), fpu_mul(r, 0.717f)));
            swc1(vel + 0x8, fpu_sub(lwc1(vel + 0x8), slower));
            if (kind == 0) {
                // smoke
                const float size = rand_range_f(200000.0f, 300000.0f);
                const int32_t life = rand_range(frame_scale_i(0x3c), frame_scale_i(0x78));
                spawn_particle_10(at, vel, 0x1f101820u, 0x101010u, static_cast<uint32_t>(life), 0, size);
            } else if (kind == 2) {
                // flame: white-hot two times in five
                const uint32_t colour = rand_mod(100) > 39 ? 0x2f486078u : 0x5ff8f8f8u;
                const int32_t life = rand_range(frame_scale_i(0x1e), frame_scale_i(0x2d));
                spawn_particle_10(at, vel, colour, 0xf000020u, static_cast<uint32_t>(life), 2, 150000.0f);
            } else if (kind == 3) {
                // glowing smoke thrown out in a random direction (its yaw the place's angle)
                const uint32_t c0 = rgba_lerp_ovl(0x7f000000u, 0x7f182030u, rand_range_f(0.25f, 1.0f));
                const uint32_t c1 = rgba_lerp_ovl(0, 0x5f5f5fu, rand_range_f(0.5f, 1.0f));
                const float speed = fpu_mul(rand_range_f(0.0f, 1.0f), dt());
                vec_from_spherical_v(V(vel), speed, a, rand_angle());
                const float size = rand_range_f(150000.0f, 200000.0f);
                const int32_t life = rand_range(frame_scale_i(0x3c), frame_scale_i(0x78));
                spawn_particle_10(at, vel, c0, c1, static_cast<uint32_t>(life), 3, size);
            }
            // (kind 1: the random numbers drawn, no particle)
        }
        moby_retire(m);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x140);
}

}  // namespace rac1
