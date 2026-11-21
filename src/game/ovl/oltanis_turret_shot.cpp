// Oltanis (level 14), class 681: the pop-up turrets' shot (class30_spawn_shot in oltanis_popup_turret.cpp spawns it).
// It flies with its velocity (pvar +0x00) from its start (+0x10) for 30 units; each frame its step is tested against the
// world and the mobys but its owner (+0x20; segment flags 1 when +0x24 is 0): it passes through class 252's mobys (up to
// three of them a frame) and stops at anything else, which it hits (a hit record of the shot's heading); at a stop or at the
// end of its range it is retired, with sparks when visible (+0x31). It is retired too when it leaves the level's cube.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

// a random direction in the unit cube, unit-ish (w 0)
Vec4 random_cube_dir() {
    Vec4 d{0.0f, 0.0f, 0.0f, 0.0f};
    d.x = rand_range_f(-1.0f, 1.0f);
    d.y = rand_range_f(-1.0f, 1.0f);
    d.z = rand_range_f(-1.0f, 1.0f);
    return d;
}

}  // namespace

// orig: L14:0x002ece00
void olt_class681_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t frame = ee_stack_alloc(0x80);
    const uint32_t from = frame, to = frame + 0x10, rec = frame + 0x20, heading = frame + 0x50, vel = frame + 0x60;
    V(from) = V(m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    V(to) = V(m + 0x10);
    uint32_t ignore = lwu(pv + 0x20);
    V(heading) = V(pv);
    sw(heading + 0x8, 0);
    vec2_set_length(V(heading), V(heading), 1.0f);
    swc1(heading + 0x8, 1.0f);
    sub_00259888(1.0f, rec, static_cast<int32_t>(m), 0x10001, heading);
    sb(rec + 0x19, 1);
    sb(rec + 0x18, 1);
    const uint32_t flags = lwu(pv + 0x24) < 1 ? 1u : 0u;

    if (30.0f < vec2_distance(V(m + 0x10), V(pv + 0x10))) {  // the end of its range
        if (lbu(m + 0x31) != 0) {
            for (int k = 0; k < 3; ++k) {
                V(vel + 0x10) = random_cube_dir();
                V(vel) = V(vel + 0x10);
                vec3_set_length(V(vel), V(vel), fpu_mul(vec3_length(V(vel + 0x10)), 0.1f));
                vec3_add(V(vel), V(vel + 0x10), V(vel));
                const float fs = lwc1(0x0015ed6cu);
                const float hi = fpu_mul(fs, 4.0f);
                vec3_set_length(V(vel), V(vel), rand_range_f(fpu_add(fs, fs), hi));
                particle_33_spawn(m + 0x10, vel);
            }
        }
        ee_stack_free(0x80);
        moby_retire(m);
        return;
    }
    for (int32_t tries = 0; tries < 3;) {
        if (segment_collide_v(V(from), V(to), flags, ignore, nullptr) == 0) break;
        const uint32_t met = coll_hit_moby();
        ++tries;
        if (met != 0 && lh(met + 0xa6) == 0xfc) {  // class 252: through it, from where it was met
            V(from) = coll_hit_point();
            ignore = met;
            continue;
        }
        // a stop: what it meets is hit, the shot ends there
        segment_collide_v(V(from), V(to), flags, ignore, static_cast<const uint32_t*>(ee_ram_ptr(rec)));
        V(m + 0x10) = coll_hit_point();
        if (lbu(m + 0x31) != 0) {
            for (int k = 0; k < 5; ++k) {
                V(vel + 0x10) = random_cube_dir();
                V(vel) = V(vel + 0x10);
                vec3_reflect(V(vel + 0x10), V(pv), coll_hit_normal());
                vec3_set_length(V(vel), V(vel), fpu_mul(vec3_length(V(vel + 0x10)), 0.5f));
                vec3_add(V(vel), V(vel + 0x10), V(vel));
                const float fs = lwc1(0x0015ed6cu);
                vec3_set_length(V(vel), V(vel), rand_range_f(fpu_mul(fs, 3.0f), fpu_mul(fs, 6.0f)));
                const int32_t lo = frame_scale_i(10);
                const int32_t hi = frame_scale_i(15);
                spawn_particle_1b(m + 0x10, vel, 0x7f2f4f6fu, static_cast<uint32_t>(rand_range(lo, hi)), 30000.0f);
            }
        }
        moby_sound_start(0, 0, m);
        ee_stack_free(0x80);
        moby_retire(m);
        return;
    }
    ee_stack_free(0x80);
    const Vec4 p = V(m + 0x10);
    if (p.x < 2.0f || 1021.0f < p.x || p.y < 2.0f || 1021.0f < p.y || p.z < 2.0f || 1021.0f < p.z) moby_retire(m);
}

}  // namespace rac1
