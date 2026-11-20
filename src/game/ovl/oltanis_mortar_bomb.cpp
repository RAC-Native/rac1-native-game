// Oltanis (level 14), class 1193: the bombers' mortar bomb (spawned by class 31, oltanis_bomber.cpp: its velocity at
// pvars +0x10, its fall per frame +0x1c, its fuse +0x20, its owner +0x24, the smoke's timer +0x28, its bounces +0x2a).
// Flying (state 1, then 0 once it falls) it falls, leaves three puffs of smoke a frame along its way while the smoke
// timer runs, stops where its way meets anything but its owner and, touching the world (0x162088 about it, the hit
// below it while falling) away from its owner, bounces off (a sound): the first bounce damped to the level's speed
// (0x1620a4), the next ones by 0x1620a0 (at least 0x162090 a second along the ground; while it still rises, only off
// what is above it); after the fourth it rolls (2) at
// a speed that decays by 0x162094 a frame (at least 0x162090), its colour turning from white toward the level's
// (0x1620a8) as its fuse runs. Touching any moby but its owner, or its fuse out, it explodes (3). It spins about its
// pitch by its speed. Ported by hand from Oltanis's own code (L14:0x300e00), the original's order of calls and float
// operations kept.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/weapons.h>

#include <cstdint>

namespace rac1 {

namespace {
struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    uint32_t n, sp;
};
}  // namespace

// orig: L14:0x00300e00
void olt_class1193_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x110);
    const uint32_t sp = f.sp, next = sp + 0x20, vel = pv + 0x10;
    const float dt = lwc1(0x0015ed6cu);
    // a moby met (not its owner): it explodes; its fuse out: as well
    if (sphere_collide(m + 0x10, u2f(0x3f266666u), 0, m) != 0 && coll_hit_moby() != 0 && coll_hit_moby() != lwu(pv + 0x24)) sb(m + 0x20, 3);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) != 0) sb(m + 0x20, 3);
    const uint32_t st = lbu(m + 0x20);
    if (st == 3) {  // (0x00301378)
        explosion_effect(m, vel, m + 0x10, 5, 2, 4, 0, 1, 1, -1, 0, 2.0f, 1.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        moby_retire(m);
        return;
    }
    if (st > 3) return;
    if (st == 2) {  // (0x00301264) rolling, its colour by the fuse's time left
        const float left = fpu_mul(static_cast<float>(lw(pv + 0x20)), lwc1(pv + 0x2c));
        const float r = lwc1(L14_ADDR(0x001620a8)), g = lwc1(L14_ADDR(0x001620ac)), b = lwc1(L14_ADDR(0x001620b0));
        const uint32_t ri = static_cast<uint32_t>(float_to_int(fpu_add(fpu_mul(fpu_sub(r, 255.0f), left), 255.0f)));
        const uint32_t gi = static_cast<uint32_t>(float_to_int(fpu_add(fpu_mul(fpu_sub(g, 0.0f), left), 0.0f)));
        const uint32_t bi = static_cast<uint32_t>(float_to_int(fpu_add(fpu_mul(fpu_sub(b, 0.0f), left), 0.0f)));
        const float drag = fpu_add(fpu_mul(fpu_sub(lwc1(L14_ADDR(0x00162094)), 1.0f), lwc1(0x0015ed60u)), 1.0f);
        sw(m + 0x90, (bi << 16) | 0xff000000u | (gi << 8) | ri);
        const float least = fpu_mul(lwc1(L14_ADDR(0x00162090)), dt);
        const float speed = fpu_mul(lwc1(pv + 0x1c), drag);
        swc1(pv + 0x1c, speed < least ? least : speed);
        vec3_set_length(V(vel), V(vel), lwc1(pv + 0x1c));
        vec3_add(V(m + 0x10), V(m + 0x10), V(vel));
    } else {  // (0x00300ec0) flying
        vec3_add(V(next), V(m + 0x10), V(vel));
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), lwc1(pv + 0x1c)));
        if (lbu(m + 0x20) == 1 && lwc1(pv + 0x18) < 0.0f) sb(m + 0x20, 0);
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x28)));
        if (lh(pv + 0x28) != 0) {  // three puffs of smoke along this frame's way
            for (int32_t k = 0; k < 3; ++k) {
                lq_sq(sp + 0x40, m + 0x10);
                vec4_lerp(sp + 0x30, sp + 0x40, next, fpu_mul(static_cast<float>(k), u2f(0x3eaaa64cu)));
                const int32_t r = rand_mod(6);
                const int32_t spin = rand_mod(2) != 0 ? -r : r;
                const float span = int_to_float(frame_scale_i(lw(L14_ADDR(0x0016208c))));
                const float size = fpu_add(fpu_mul(fpu_div(static_cast<float>(lh(pv + 0x28)), span), 45000.0f), 5000.0f);
                const uint32_t p = spawn_particle_17(sp + 0x30, spin, L14_ADDR(0x0015f580), 0x80808080u, u2f(0x3ccccccdu), 1.0f, 1.0f, size);
                if (p == 0) continue;
                sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x78)));
                sb(p + 0x2a, 0x7f);
                sw(p + 0x24, 2);
                sb(p + 0x2b, lbu(p + 0xa));
            }
        }
        // the way stops where it meets anything but the owner
        const bool met = segment_collide_v(V(m + 0x10), V(next), 0, m, nullptr) != 0;
        if (met && (coll_hit_moby() == 0 || coll_hit_moby() != lwu(pv + 0x24))) V(m + 0x10) = coll_hit_point();
        else lq_sq(m + 0x10, next);
        // touching the world: a bounce (while it rises, only off what is above it), not off its owner
        if (sphere_collide(m + 0x10, lwc1(L14_ADDR(0x00162088)), 0, m) != 0 &&
            (lbu(m + 0x20) == 0 || fpu_sub(lwc1(m + 0x18), coll_hit_point().z) < 0.0f) &&
            (coll_hit_moby() == 0 || coll_hit_moby() != lwu(pv + 0x24))) {
            vec3_set_length(V(sp + 0x30), coll_hit_normal(), lwc1(L14_ADDR(0x00162088)));
            vec3_add(V(m + 0x10), coll_hit_point(), V(sp + 0x30));
            vec3_reflect(V(vel), V(vel), coll_hit_normal());
            moby_sound_start(1, 0, m);
            if (lbu(m + 0x20) == 0) {  // (0x00301140) a bounce counted, its speed damped
                sb(m + 0x20, 1);
                const uint32_t n = (lhu(pv + 0x2a) + 1u) & 0xffffu;
                sh(pv + 0x2a, n);
                bool set = true;
                float to = 0.0f;
                if (static_cast<int16_t>(n) == 1) {
                    to = fpu_mul(lwc1(L14_ADDR(0x001620a4)), dt);
                } else {
                    vec3_scale(V(sp + 0x40), V(vel), lwc1(L14_ADDR(0x001620a0)));
                    to = fpu_mul(lwc1(L14_ADDR(0x00162090)), dt);
                    if (!(vec2_length(V(sp + 0x40)) < to)) {
                        lq_sq(vel, sp + 0x40);
                        set = false;
                    }
                }
                if (set) {
                    vec2_set_length(V(vel), V(vel), to);
                    swc1(pv + 0x18, fpu_mul(lwc1(pv + 0x18), lwc1(L14_ADDR(0x001620a0))));
                }
            }
            if (!(lh(pv + 0x2a) < 4)) {  // (0x003011f0) it rolls from now on
                sb(m + 0x20, 2);
                sw(pv + 0x18, 0);
                const float speed = fpu_mul(vec2_length(V(vel)), lwc1(L14_ADDR(0x001620a0)));
                const float least = fpu_mul(lwc1(L14_ADDR(0x00162090)), dt);
                swc1(pv + 0x1c, speed < least ? least : speed);
                vec3_set_length(V(vel), V(vel), lwc1(pv + 0x1c));
                swc1(pv + 0x2c, fpu_div(1.0f, int_to_float(lw(pv + 0x20))));
            }
        }
    }
    // (0x003013f0) its spin about the pitch: 1 to 8 degrees a frame by its speed (0.2 to 6 a second)
    const uint32_t now = lbu(m + 0x20);
    float speed = 0.0f;
    if (now < 2) speed = vec2_length(V(vel));
    else if (now == 2) speed = lwc1(pv + 0x1c);
    else return;
    const float lo = fpu_mul(lwc1(0x0015ed6cu), u2f(0x3e4ccccdu)), hi = fpu_mul(lwc1(0x0015ed6cu), 6.0f);
    if (speed < lo) speed = lo;
    if (hi < speed) speed = hi;
    const float turn = fpu_mul(fpu_add(fpu_mul(fpu_div(fpu_sub(speed, lo), fpu_sub(hi, lo)), 7.0f), 1.0f), u2f(0x3c8efa35u));
    swc1(m + 0x44, angle_add(turn, lwc1(m + 0x44)));
}

}  // namespace rac1
