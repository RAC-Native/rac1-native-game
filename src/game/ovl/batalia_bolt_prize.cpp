// Batalia (level 8) own moby classes, hand-ported: class 152, a bolt prize (none placed in the level: its spawner sets its
// value +0x20 and whether it falls, +0x2c). It spins and bobs with a glow on it, falls under gravity when thrown, and
// when Ratchet touches it its value goes to his bolts (the HUD's counter shown) and it is gone. With its helpers: its set-up
// and its glow. Registered in batalia_mobys.cpp. Addresses are level 8's (`orig: L8:`); its own data is L8_ADDR (the
// class's tuning at 0x16144c .. 0x16146c, the glow's 0x160200 .. 0x160208, the camera's position 0x1675c0), the base
// ELF's are bare.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 his position, +0x510 / +0x520 / +0x530 three of his points,
                                         // +0x2288 / +0x228c his pickup reach across / up)
constexpr uint32_t HERO_POS = PLAYER + 0x80;

// the prize's glow (+0x24) put out
void glow_out(uint32_t pv) {
    const uint32_t glow = lwu(pv + 0x24);
    if (glow != 0) sh(glow + 0xa, 0);
}
}  // namespace

// The prize `m`'s glow: a particle of kind 0x27 the radius 0x160208 from it towards the camera, its look 0x160204, 0x160200
// and the level's texture 0x1b291c's first byte, 255 bright, 30 frames, its moby `m` (+0x24). The particle (0: none).
// orig: L8:0x00279f00
uint32_t bolt_prize_glow_l8(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t p = alloc_particle(0x27);
    if (p == 0) {
        ee_stack_free(0x60);
        return 0;
    }
    const uint32_t off = sp;
    vec3_sub(V(off), V(m + 0x10), V(L8_ADDR(0x001675c0)));
    vec3_set_length(V(off), V(off), lwc1(L8_ADDR(0x00160208)));
    vec3_add(V(p + 0x10), V(m + 0x10), V(off));
    sw(p + 0x4, lwu(L8_ADDR(0x00160204)));
    sb(p + 0x9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 0x3, 0x48);
    sb(p + 0x1, 0);
    sb(p + 0x2, lbu(lwu(L8_ADDR(0x001b291c))));
    swc1(p + 0xc, lwc1(L8_ADDR(0x00160200)));
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x1e)));
    swc1(p + 0x20, 255.0f);
    sw(p + 0x24, m);
    sw(p + 0x28, 0);
    ee_stack_free(0x60);
    return p;
}

// The prize `m` set up: upright, tilted 30 degrees (+0x44), a random spin rate 0x161468 .. 0x16146c (+0xc), its frame
// count (+0x28) cleared, its height kept (+0x8), lit like the hero, its glow (+0x24).
// orig: L8:0x002b8228
void bolt_prize_setup_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(m + 0x40, 0);
    sw(m + 0x48, 0);
    swc1(m + 0x44, u2f(0x3f060a92u));  // (pi / 6)
    swc1(pv + 0xc, rand_range_f(lwc1(L8_ADDR(0x00161468)), lwc1(L8_ADDR(0x0016146c))));
    sw(pv + 0x28, 0);
    swc1(pv + 0x8, lwc1(m + 0x18));
    moby_copy_hero_color(m);
    sw(pv + 0x24, bolt_prize_glow_l8(m));
}

// Class 152, the bolt prize. pvars: +0x8 its height before the bob, +0xc its spin rate, +0x10 its velocity while it falls,
// +0x20 its value in bolts, +0x24 its glow, +0x28 its frame count, +0x2c set: it falls (state 1), else it stays (state 2).
// States (the table 0x1fb890, 0 .. 4): 0 set up; 1 falls (gravity 9.8, at most 1.8 times its radius 0x16144c a frame):
// on its frames of the moby's parity it probes its way for a hit (the level's probe, a stub that finds nothing), so it
// never lands; then as 2: spins and is taken when Ratchet touches it (in his mode 0xf: one of his three points within 0.5
// across; else within his reach): its value added to the bolts 0x15ed98 and the level's total 0x13df38, its sound, the
// bolt counter shown, gone; 3 nothing more; 4 gone. In every state but 0 its height is put back before and it bobs
// (0x161464 high, over 0x161460 frames) after.
// orig: L8:0x002b82a0
void bat_class152_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xf0);
    sw(pv + 0x28, lwu(pv + 0x28) + 1);
    if (lbu(m + 0x20) != 0) swc1(m + 0x18, lwc1(pv + 0x8));
    bool fall = false, spin = false;
    switch (lbu(m + 0x20)) {  // (the table 0x1fb890: states 0 .. 4; 5 and above as 3)
    case 0:  // (0x002b831c)
        bolt_prize_setup_l8(m);
        sb(m + 0x20, lw(pv + 0x2c) != 0 ? 1 : 2);
        break;
    case 1:  // (0x002b8344)
        fall = true;
        spin = true;
        break;
    case 2:  // (0x002b85b8)
        spin = true;
        break;
    case 4:  // (0x002b871c)
        glow_out(pv);
        moby_retire(m);
        ee_stack_free(0xf0);
        return;
    default:  // (0x002b8738: 3, and 5 and above)
        break;
    }

    if (fall) {
        const uint32_t pos = sp + 0x40, step = sp, from = sp + 0x20, to = sp + 0x30, vel = pv + 0x10;
        lq_sq(pos, m + 0x10);
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(lwc1(0x0015ed70), 9.8f)));
        float speed = vec3_length(V(vel));
        const float most = fpu_mul(fpu_mul(lwc1(L8_ADDR(0x0016144c)), 1.8f), lwc1(0x0015ed60));
        if (most < speed) {
            vec3_set_length(V(vel), V(vel), most);
            speed = fpu_mul(fpu_mul(lwc1(L8_ADDR(0x0016144c)), 1.8f), lwc1(0x0015ed60));
        }
        vec3_add(V(step), V(pos), V(vel));
        // its probe from its radius behind it to its radius past the step
        vec3_set_length(V(from), V(vel), lwc1(L8_ADDR(0x0016144c)));
        vec3_sub(V(from), V(pos), V(from));
        vec3_set_length(V(to), V(vel), fpu_add(speed, lwc1(L8_ADDR(0x0016144c))));
        vec3_add(V(to), V(to), V(pos));
        const int32_t index = static_cast<int32_t>(m - lwu(L8_ADDR(0x0015ffd8))) >> 8;
        bool moved = false;  // (the step taken as it is when not)
        if (lw(L8_ADDR(0x0015f5cc)) % 2 == index % 2 && zero_stub() != 0) {  // (the probe: the level's stub, 0 always)
            const uint32_t hit_moby = coll_hit_moby();
            if (hit_moby == 0 || lw(pv + 0x28) >= 10) {
                bool bounce = true;
                if (speed < lwc1(L8_ADDR(0x00161458))) {
                    bounce = false;
                } else {
                    // back to its radius off the hit point
                    const uint32_t back = sp + 0x50;
                    vec3_sub(V(back), V(pos), coll_hit_point());
                    vec3_set_length(V(back), V(back), lwc1(L8_ADDR(0x0016144c)));
                    vec3_add(V(pos), coll_hit_point(), V(back));
                }
                if (bounce) {
                    const uint32_t out = sp + 0x10;
                    vec3_reflect(V(out), V(vel), coll_hit_normal());
                    float left = fpu_sub(vec3_length(V(out)), lwc1(L8_ADDR(0x0016145c)));
                    if (left < 0.0f) left = 0.0f;
                    vec3_set_length(V(vel), V(out), left);
                } else {
                    // it settles on the hit point, its radius above it; on the ground (no moby hit) it stays (state 2)
                    vec_clear(V(vel));
                    coll_vec_store(pos, coll_hit_point());
                    swc1(pos + 8, fpu_add(lwc1(pos + 8), lwc1(L8_ADDR(0x0016144c))));
                    if (coll_hit_moby() == 0) {
                        vec_clear(V(vel));
                        sb(m + 0x20, 2);
                    }
                }
                moved = true;
            }
        }
        if (!moved) lq_sq(pos, step);
        lq_sq(m + 0x10, pos);
    }

    if (spin) {
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0xc)));
        bool taken = false;
        if (lw(0x001413dc) == 0xf) {
            const float near = 0.5f;
            if (vec2_distance(V(PLAYER + 0x510), V(m + 0x10)) < near) taken = true;
            if (vec2_distance(V(PLAYER + 0x520), V(m + 0x10)) < near) taken = true;
            if (vec2_distance(V(PLAYER + 0x530), V(m + 0x10)) < near) taken = true;
        } else if (vec2_distance(V(m + 0x10), V(HERO_POS)) < lwc1(PLAYER + 0x2288) &&
                   abs_f(fpu_sub(lwc1(m + 0x18), lwc1(PLAYER + 0x88))) < lwc1(PLAYER + 0x228c)) {
            taken = true;
        }
        if (taken) {
            glow_out(pv);
            const int32_t value = lw(pv + 0x20);
            const uint32_t total = 0x0013df38 + (lwu(0x0015ed84) << 2);  // (the level's)
            sw(0x0015ed98, static_cast<uint32_t>(lw(0x0015ed98) + value));
            sw(total, static_cast<uint32_t>(lw(total) + value));
            moby_sound_start(0, 0, m);
            camera_request(2, 0x754e, L8_ADDR(0x002444a8), L8_ADDR(0x002444e0), L8_ADDR(0x002445d8), 0x0015ed98, 0x0098967f);
            moby_retire(m);
            ee_stack_free(0xf0);
            return;
        }
    }

    // the bob: its height kept, then 0x161464 times the sine of its frame count's phase in 0x161460 frames
    const int32_t period = lw(L8_ADDR(0x00161460));
    const float phase = fpu_sub(fpu_mul(fpu_div(static_cast<float>(ee_rem(lw(pv + 0x28), period)), static_cast<float>(period - 1)),
                                        u2f(0x40c90fdbu)),  // (2 pi)
                                u2f(0x40490fdbu));          // (pi)
    const float s = sin_vu(phase);
    swc1(pv + 0x8, lwc1(m + 0x18));
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(L8_ADDR(0x00161464)), s)));
    ee_stack_free(0xf0);
}

}  // namespace rac1
