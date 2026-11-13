// The rocks thrown around by explosions and bosses (classes 696 / 697 / 698 on Novalis, Aridia, Umbris and the finale's
// Veldin; 1806 on Orxon, 200 on Quartu): one port of the copies six levels carry. The copies are the same code relinked
// (compared with the relocated fields masked); what they name is the engine's (OV: the collision's hit point 0x173e60 and
// normal 0x173e80) or the base ELF's (bare: the frame time 0x15ed6c and its square 0x15ed70). With their spawn (the
// copies on the same six levels). Header shared_rock.h.
//
// A rock's pvars: its velocity +0, its life +0x10, its radius +0x14, its spin +0x18, the frames before it collides
// +0x1c, its gravity +0x24, the fade mode +0x20 with the life it fades over +0x28.
#include <game/ovl/shared_rock.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>

namespace rac1 {

// orig: L1:0x002f8530 L2:0x002dd4d8 L7:0x00300150 L10:0x002eafc8 L15:0x002be0b0 L18:0x002dfbc0
uint32_t rock_spawn(uint32_t pos, uint32_t vel, uint32_t cls, int32_t life, uint32_t fade, float scale, float gravity, float spin,
                    float wait) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    lq_sq(sp + 0x10, vel);  // (both copied before the spawn, as the original does)
    lq_sq(sp, pos);
    const uint32_t r = moby_spawn(cls);
    if (r != 0) {
        const uint32_t pv = lwu(r + 0x78);
        const float pi = u2f(0x40490fdbu);
        moby_copy_hero_color(r);
        sh(r + 0x32, 0xff);
        sb(r + 0x30, 0xff);
        sb(r + 0x31, 1);
        swc1(r + 0x2c, fpu_mul(lwc1(r + 0x2c), scale));
        swc1(r + 0x40, rand_range_f(-pi, pi));
        swc1(r + 0x44, rand_range_f(-pi, pi));
        swc1(r + 0x48, rand_range_f(-pi, pi));
        lq_sq(r + 0x10, sp);
        lq_sq(pv, sp + 0x10);
        const float dt = lwc1(0x0015ed6c);
        const float rate = rand_range_f(fpu_mul(dt, u2f(0x3fc90fdbu)), fpu_mul(dt, u2f(0x40c90fdbu)));
        const float turn = (libc_rand() & 1) == 0 ? fpu_mul(rate, spin) : fpu_mul(-rate, spin);
        sw(pv + 0x28, static_cast<uint32_t>(life));
        swc1(pv + 0x14, scale);
        sw(pv + 0x10, static_cast<uint32_t>(life));
        swc1(pv + 0x18, turn);
        sw(pv + 0x1c, static_cast<uint32_t>(float_to_int(frame_scale_f(fpu_mul(wait, 60.0f)))));
        sw(pv + 0x20, fade);
        swc1(pv + 0x24, fpu_mul(fpu_mul(gravity, u2f(0x411ccccdu)), lwc1(0x0015ed70)));
        moby_update_transform(r);
    }
    ee_stack_free(0xc0);
    return r;
}

// The rock falls and spins; out of the level's box (2..1021) it leaves. Once its first frames are past (+0x1c), its
// path and then its radius are tested against the world: on a hit it bounces off the surface, slowed to 7..13 % with a
// random kick (up to 35 % of its fall speed, flat), tumbles at random, sits a tenth of a unit off the surface and spins
// the other way at a random rate. A fading rock (+0x20) fades with its life and leaves when that runs out; another one
// crumbles into 40 specks of dust (particle 0x16) when its life runs out, or when it has come to rest after its first
// frames.
// orig: L1:0x002f8718 L2:0x002dd6c0 L7:0x00300338 L10:0x002eb1b0 L15:0x002be298 L18:0x002dfda8
void rock_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t was = sp, kick = sp + 0x10, at = sp + 0x20, dir = sp + 0x30;
    lq_sq(was, pos);
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), lwc1(pv + 0x24)));
    vec3_add(V(pos), V(pos), V(pv));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x18)));
    bool gone = false;
    for (uint32_t k = 0; k < 0xc && !gone; k += 4) gone = lwc1(pos + k) < 2.0f || 1021.0f < lwc1(pos + k);

    if (!gone && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x1c))) != 0 &&
        (segment_collide_v(V(was), V(pos), 0, m, nullptr) != 0 || sphere_collide(pos, lwc1(pv + 0x14), 0, m) != 0)) {
        const float pi = u2f(0x40490fdbu);
        const float heading = atan2_f(lwc1(pv), lwc1(pv + 4));
        {
            const float a = angle_add(heading, rand_range_f(-pi, pi));
            const float c = cos_vu(a);
            swc1(kick, fpu_mul(c, rand_range_f(0.0f, fpu_mul(lwc1(pv + 8), u2f(0x3eb33333u)))));
        }
        {
            const float a = angle_add(heading, rand_range_f(-pi, pi));
            const float s = sin_vu(a);
            const float k = fpu_mul(s, rand_range_f(0.0f, fpu_mul(lwc1(pv + 8), u2f(0x3eb33333u))));
            swc1(kick + 8, 0.0f);
            swc1(kick + 4, k);
        }
        swc1(m + 0x40, rand_range_f(-pi, pi));
        swc1(m + 0x44, rand_range_f(-pi, pi));
        swc1(m + 0x48, rand_range_f(-pi, pi));
        const uint32_t normal = OV(0x00173e80);
        vec3_reflect(V(pv), V(pv), V(normal));
        vec3_scale(V(pv), V(pv), rand_range_f(u2f(0x3d8f5c29u), u2f(0x3e051eb8u)));
        vec3_add(V(pv), V(pv), V(kick));
        vec3_set_length(V(normal), V(normal), u2f(0x3dcccccdu));
        vec3_add(V(pos), V(normal), V(OV(0x00173e60)));
        const float dt = lwc1(0x0015ed6c);
        float spin = rand_range_f(fpu_mul(dt, u2f(0x3fc90fdbu)), fpu_mul(dt, u2f(0x40c90fdbu)));
        if (0.0f < lwc1(pv + 0x18)) spin = -spin;
        swc1(pv + 0x18, spin);
    }

    if (!gone) {
        int32_t& life = *static_cast<int32_t*>(ee_ram_ptr(pv + 0x10));
        if (lw(pv + 0x20) != 0) {
            if (timer_tick_i32(life) != 0) {
                gone = true;
            } else {
                const int32_t scaled = static_cast<int32_t>(lwu(pv + 0x10) * 127u);  // (the alpha: its life's share of 127)
                sb(m + 0x23, static_cast<uint32_t>(ee_div(scaled, lw(pv + 0x28))));
            }
        } else {
            bool crumble = timer_tick_i32(life) != 0;
            if (!crumble) crumble = vec3_length(V(pv)) < fpu_mul(lwc1(0x0015ed6c), 0.5f) && lw(pv + 0x1c) == 0;
            if (crumble) {
                for (int32_t n = 0; n < 40; ++n) {
                    V(at) = Vec4{};
                    swc1(at, rand_range_f(-1.0f, 1.0f));
                    swc1(at + 4, rand_range_f(-1.0f, 1.0f));
                    swc1(at + 8, rand_range_f(-1.0f, 1.0f));
                    V(dir) = Vec4{};
                    lq_sq(kick, at);
                    swc1(dir, rand_range_f(-1.0f, 1.0f));
                    swc1(dir + 4, rand_range_f(-1.0f, 1.0f));
                    swc1(dir + 8, rand_range_f(-1.0f, 1.0f));
                    lq_sq(at, dir);
                    vec3_set_length(V(at), V(at), rand_range_f(0.0f, 0.5f));
                    vec3_add(V(at), V(at), V(pos));
                    vec3_set_length(V(kick), V(kick), fpu_mul(rand_range_f(0.0f, 0.5f), lwc1(0x0015ed6c)));
                    vec3_add(V(kick), V(kick), V(pv));
                    const float size = rand_range_f(70000.0f, 140000.0f);
                    const int32_t frames = frame_scale_i(rand_range(0x14, 0x3c));
                    particle_16_spawn(at, kick, 0x1f0c1820u, 0x81020u, frames, size);
                }
                gone = true;
            }
        }
    }
    if (gone) moby_retire(m);
    ee_stack_free(0xb0);
}

}  // namespace rac1
