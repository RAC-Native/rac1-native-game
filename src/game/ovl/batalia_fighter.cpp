// Batalia (level 8) own moby classes, hand-ported: class 438, the Blarg fighter (25 on the level): the craft that loop
// over the fort along the level's paths trailing exhaust, and that, shot (mask 0x10000), fall burning and spinning until
// they crash; one shot down with the hero in state 0x32 gives the skill point 0x13d414. Addresses are level 8's
// (`orig: L8:`); its own data is L8_ADDR (the class's tuning at 0x161a44 .. 0x161a80), the base ELF's are bare (the hero's
// state 0x1413d4, the frame time 0x15ed6c and its square 0x15ed70).
#include <game/level/level_data.h>
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
}  // namespace

// The fighter `m`'s exhaust: a particle (kind 2) from its nozzle (-2.3, 0, 0.3 in its frame +0xc0), moving at 0x161a48
// times its move since `old_pos` (an EE vector), drifting at 0x161a4c times it plus up to 0x161a50 a second at random;
// the two w lanes 0x161a64 .. 0x161a68 and 0x161a6c .. 0x161a70 at random, its two colours between 0x161a74 / 0x161a78
// and 0x161a7c / 0x161a80, its times 0x161a54 (1 .. 1 + it frames), 0x161a58 and 0x161a5c (times 1 -+ 0x161a60); the
// particle's byte +9 set to 8 - 0x70.
// orig: L8:0x002dec90
void fighter_exhaust_l8(uint32_t m, uint32_t old_pos) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t pos = m + 0x10;
    const uint32_t vel = sp + 0x10, drift = sp + 0x20, move = sp + 0x30, jitter = sp + 0x40, nozzle = sp + 0x50,
                   local = sp + 0x60;
    vec3_sub(V(move), V(pos), V(old_pos));
    sd(local, 0);
    sd(local + 8, 0);
    swc1(local, -2.3f);
    swc1(local + 8, 0.3f);
    mat3_transform(V(nozzle), V(local), M(m + 0xc0));
    vec3_add(V(nozzle), V(nozzle), V(pos));
    vec3_scale(V(vel), V(move), lwc1(L8_ADDR(0x00161a48)));
    vec3_scale(V(drift), V(move), lwc1(L8_ADDR(0x00161a4c)));
    rand_sphere_point(jitter, 0.0f, fpu_mul(lwc1(L8_ADDR(0x00161a50)), dt()));
    vec3_add(V(drift), V(drift), V(jitter));
    swc1(vel + 0xc, rand_range_f(lwc1(L8_ADDR(0x00161a64)), lwc1(L8_ADDR(0x00161a68))));
    swc1(drift + 0xc, rand_range_f(lwc1(L8_ADDR(0x00161a6c)), lwc1(L8_ADDR(0x00161a70))));
    const uint32_t c0 = rgba_lerp(lwu(L8_ADDR(0x00161a74)), lwu(L8_ADDR(0x00161a78)), rand_range_f(0.0f, 1.0f));
    const uint32_t c1 = rgba_lerp(lwu(L8_ADDR(0x00161a7c)), lwu(L8_ADDR(0x00161a80)), rand_range_f(0.0f, 1.0f));
    const float r0 = rand_range_f(0.0f, 1.0f);
    const int32_t n0 =
        float_to_int(frame_scale_f(fpu_add(fpu_mul(static_cast<float>(lw(L8_ADDR(0x00161a54))), r0), 1.0f)));
    const float spread1 = lwc1(L8_ADDR(0x00161a60));
    const float r1 = fpu_add(rand_range_f(-spread1, spread1), 1.0f);
    const int32_t n1 = float_to_int(frame_scale_f(fpu_mul(static_cast<float>(lw(L8_ADDR(0x00161a58))), r1)));
    const float spread2 = lwc1(L8_ADDR(0x00161a60));
    const float r2 = fpu_add(rand_range_f(-spread2, spread2), 1.0f);
    const int32_t n2 = float_to_int(frame_scale_f(fpu_mul(static_cast<float>(lw(L8_ADDR(0x00161a5c))), r2)));
    const uint32_t p = spawn_particle_02(nozzle, vel, drift, c0, c1, static_cast<uint32_t>(n0), static_cast<uint32_t>(n1),
                                         static_cast<uint32_t>(n2), -1);
    if (p != 0) sb(p + 9, static_cast<uint32_t>(float_to_int(8.0f) - 0x70));
    ee_stack_free(0xf0);
}

// Class 438, the Blarg fighter. pvars: +0 its path's index (-1: none, retired at once), +4 its place on the path (a point
// count; set up as a fraction of the path), +8 its step a frame (0x161a44 a second over the path's first span), +0xc its
// height above the path (-5 .. 5 at random), +0x10 its velocity once shot. States: 0 set up (the step, the place, the
// height; drawn, +0x30 0xff) and on as 1; 1 flies its path (looping; its pitch halved); 2 shot: falls (15 a second squared),
// spinning (its roll +0x40 doubled plus 420 degrees a second, wrapped), its pitch from its velocity; below 10 high retired,
// touching anything (sphere_collide, 1.25) it blows up and is retired. Every frame it is not retired: its colour +0x94 its
// class's (0 below 4 on any axis), a hit (mask 0x10000) in any state but 2 shoots it down: the skill point 0x13d414 the first
// time with the hero in state 0x32 (its jingle and the text 0x53d6), its velocity its last move and 2 a second up, an
// explosion, state 2. Then drawn (+0xa4 0xff), and its exhaust while +0x31 is set.
// orig: L8:0x002de848
void bat_class438_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t pos = m + 0x10, old_pos = sp + 0x20, move = sp + 0x30, vel = pv + 0x10;
    lq_sq(old_pos, pos);
    const int32_t route = lw(pv);
    const uint32_t path = lvd::path(route);  // (read in every state, as the original)
    const auto retire = [&]() {
        moby_retire(m);
        ee_stack_free(0xc0);
    };

    switch (lbu(m + 0x20)) {  // (a compare tree, no jump table)
    case 0: {
        if (route == -1) {
            retire();
            return;
        }
        const float span = vec3_distance(V(path + 0x10), V(path + 0x20));
        // (the original also measures the distance from the first point to the last and drops it)
        swc1(pv + 8, fpu_div(fpu_mul(lwc1(L8_ADDR(0x00161a44)), dt()), span));
        swc1(pv + 4, fpu_mul(lwc1(pv + 4), static_cast<float>(lw(path))));
        swc1(pv + 0xc, rand_range_signed_f(0.0f, 5.0f));
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        [[fallthrough]];
    }
    case 1: {
        path_point(lwc1(pv + 4), path, 1, pos, m + 0x40, 0);
        swc1(m + 0x44, fpu_mul(lwc1(m + 0x44), 0.5f));
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0xc)));
        const float t = fpu_add(lwc1(pv + 4), lwc1(pv + 8));
        swc1(pv + 4, t);
        const float points = static_cast<float>(lw(path));
        if (points < t) swc1(pv + 4, fpu_sub(t, points));  // (round the loop)
        break;
    }
    case 2: {
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(dt_sq(), 15.0f)));
        const float spun = angle_add(lwc1(m + 0x40), fpu_mul(dt(), u2f(0x40ea927fu)));  // (420 degrees a second)
        swc1(m + 0x40, fpu_add(lwc1(m + 0x40), spun));
        swc1(m + 0x44, -atan2_f(vec2_length(V(vel)), lwc1(pv + 0x18)));
        vec3_add(V(pos), V(pos), V(vel));
        if (lwc1(m + 0x18) < 10.0f) {
            retire();
            return;
        }
        if (sphere_collide(pos, 1.25f, 0, m) != 0) {
            explosion_effect(m, vel, pos, 0x14, 0x28, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 10.0f, 5.0f, 9.0f, 1.0f, 15.0f);
            retire();
            return;
        }
        break;
    }
    default:
        break;
    }

    if (lwc1(pos) < 4.0f || lwc1(pos + 4) < 4.0f || lwc1(pos + 8) < 4.0f)
        sw(m + 0x94, 0);
    else
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    if (sub_0025a420(m, 0x10000, 0) != 0 && lbu(m + 0x20) != 2) {
        // shot down
        if (lw(0x001413d4) == 0x32 && lbu(0x0013d414) == 0) {
            sb(0x0013d414, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        vec3_sub(V(move), V(pos), V(old_pos));
        lq_sq(vel, move);
        swc1(pv + 0x18, fpu_add(lwc1(pv + 0x18), fpu_add(dt(), dt())));
        explosion_effect(m, move, pos, 0x14, 0x28, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 10.0f, 5.0f, 9.0f, 1.0f, 15.0f);
        sb(m + 0x20, 2);
    }
    sb(m + 0xa4, 0xff);
    if (lbu(m + 0x31) != 0) fighter_exhaust_l8(m, old_pos);
    ee_stack_free(0xc0);
}

}  // namespace rac1
