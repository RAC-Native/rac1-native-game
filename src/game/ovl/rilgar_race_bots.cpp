// Rilgar's (level 5) little bots of the hoverboard course (class 133): they wander in groups of five from point to
// point of their paths, leaving puddle rings; a hit bursts one in sparks, and when Ratchet himself hit it his board
// gets a second of boost; it comes back once he is 48 units away. Ported by hand from Rilgar's own code (L5:0x2dac80,
// 0x2daf58, 0x2db018, 0x2db238), the original's order of calls and float operations kept.
#include <game/ovl/rilgar_own.h>

#include <game/level/level_data.h>
#include <game/ovl/metal.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/walker.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;  // the hero's block (+0x80 his position, +0x8c4 the board's boost frames)
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
}  // namespace

// The burst of sparks where the bot was: 20 bursts from its position (jittered by 0.25, one unit up), each thrown
// out (1 to 8 a second) and up (2 to 6) with part of `vel` (the hero's speed vector), of 10 particles 0x02 between
// that point and the point a second on (falling), yellow to white.
// orig: L5:0x002db238
void ril_2db238_l5(uint32_t m, uint32_t vel) {
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t at = sp + 0x10, va = sp + 0x20, vb = sp + 0x30;
    for (int32_t burst = 0; burst < 20; ++burst) {
        lq_sq(at, m + 0x10);
        metal_25f8e0(at, 0.25f);
        swc1(at + 8, fpu_add(lwc1(at + 8), 1.0f));
        const float a = rand_angle();
        const float speed = rand_range_f(1.0f, 8.0f);
        const float rise = rand_range_f(2.0f, 6.0f);
        vec3_scale(V(va), V(vel), rand_range_f(0.0f, 1.0f));
        swc1(va, fpu_add(lwc1(va), fpu_mul(cos_vu(a), fpu_mul(speed, dt()))));
        const float s = sin_vu(a);
        const float t = dt();
        swc1(va + 8, fpu_add(lwc1(va + 8), fpu_mul(rise, t)));
        swc1(va + 4, fpu_add(lwc1(va + 4), fpu_mul(s, fpu_mul(speed, t))));
        lq_sq(vb, va);
        const float frames = static_cast<float>(frame_scale_i(0x1e));
        swc1(vb + 8, fpu_sub(lwc1(vb + 8), fpu_mul(fpu_mul(dt2(), 20.0f), frames)));
        for (int32_t k = 0; k < 10; ++k) {
            const float r = rand_range_f(0.5f, 1.5f);
            swc1(va + 0xc, fpu_mul(r, 0.125f));
            swc1(vb + 0xc, fpu_mul(r, u2f(0x3d851eb8u)));  // (0.065)
            metal_25f8e0(at, u2f(0x3e4ccccdu));             // (0.2)
            metal_25f8e0(vb, fpu_mul(dt(), 0.5f));
            const uint32_t c0 = rgba_lerp(0x8000eeeeu, 0x8000ff90u, rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(0xffeeu, 0xffeeu, rand_range_f(0.0f, 1.0f));
            const uint32_t n = static_cast<uint32_t>(frame_scale_i(0xa));
            const uint32_t h = static_cast<uint32_t>(frame_scale_i(0x1e));
            const uint32_t life = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(5.0f, 25.0f))));
            spawn_particle_02(at, va, vb, c0, c1, n, h, life, -1);
        }
    }
    ee_stack_free(0x120);
}

// The bot's hit (mask 0x330000): hidden (+0x34 bit 0, +0x31), no collision, state 2, its sparks and sound; a hit from
// Ratchet himself (class 0) adds a second to the board's boost (the hero's +0x8c4).
// orig: L5:0x002daf58
void ril_2daf58_l5(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    if (hit == 0) return;
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    sb(m + 0x20, 2);
    sb(m + 0x31, 0);
    sw(m + 0x94, 0);
    ril_2db238_l5(m, kHero + 0x100);
    moby_sound_start(0, 0, m);
    const uint32_t src = lwu(hit + 0x20);
    if (src != 0 && lh(src + 0xa6) == 0) sw(kHero + 0x8c4, lwu(kHero + 0x8c4) + static_cast<uint32_t>(frame_scale_i(0x3c)));
    sb(m + 0xa4, 0xff);
}

// The puddle rings (particle 0x34) while the bot is visible: one in 19 frames a small one, one in 4 a large one,
// within 0.1 of it, for 2 to 3 seconds.
// orig: L5:0x002db018
void ril_2db018_l5(uint32_t m) {
    if (lbu(m + 0x31) == 0) return;
    const uint32_t sp = ee_stack_alloc(0x70);
    // a ring of size lo .. mid growing mid .. hi, words a / b
    auto ring = [m, sp](float lo, float mid, float hi, uint32_t a, uint32_t b) {
        const float ang = rand_angle();
        const float r = rand_range_f(0.0f, u2f(0x3dcccccdu));  // (0.1)
        const float size = rand_range_f(lo, mid);
        const float grow = rand_range_f(mid, hi);
        const int32_t life = float_to_int(rand_range_f(120.0f, 180.0f));
        swc1(sp, fpu_mul(cos_vu(ang), r));
        swc1(sp + 8, 0.0f);
        swc1(sp + 4, fpu_mul(sin_vu(ang), r));
        vec3_add(V(sp), V(sp), V(m + 0x10));
        nov_287158_l1(sp, a, b, frame_scale_i(life), size, grow);
    };
    if (rand_mod(0x13) == 0) ring(0.0f, 0.5f, 1.0f, 0x30002028u, 0x2020u);
    if (rand_mod(4) == 0) ring(0.0f, 1.75f, 2.25f, 0x20002020u, 0x2030u);
    ee_stack_free(0x70);
}

// Class 133, the bots. pvars: the walker record +0x60 (its step +0x84), the path +0xb0 (-1 or empty: the moby goes),
// the point it heads for +0xb4 and the timer to the next one +0xb8, its yaw rate +0xbc. States: 0 set up; 1 walks to
// its point (a random jump ahead along the path on arriving or every 10 seconds); 2 burst (0x2daf58): back as 1 when
// the hero is more than 48 away.
// orig: L5:0x002dac80
void ril_class133_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    ril_2daf58_l5(m);
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002dacdc) set up
        sb(m + 0x20, 1);
        sb(m + 0x30, 0x40);
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
        const int32_t idx = lw(pv + 0xb0);
        if (idx == -1 || lw(lvd::path(idx)) == 0) {
            moby_retire(m);
            break;
        }
        walker_record_init_l1(pv + 0x60);
        swc1(pv + 0x68, 1.0f);
        swc1(pv + 0x6c, 1.0f);
        sw(pv + 0x60, static_cast<uint32_t>(float_to_int(u2f(0x43cccccdu))));  // (409.6: 0.4 x 1024)
        swc1(pv + 0x84, fpu_mul(dt(), 4.0f));
        break;
    }
    case 1: {  // (0x002dad88) walks
        const uint32_t point = lvd::path(lw(pv + 0xb0)) + 0x10 + (lwu(pv + 0xb4) << 4);
        const float turn = fpu_mul(dt2(), u2f(0x3f860a92u));  // (60 degrees)
        yaw_seek(m + 0x48, pv + 0xbc, atan2_f(fpu_sub(lwc1(point), lwc1(m + 0x10)), fpu_sub(lwc1(point + 4), lwc1(m + 0x14))), turn, turn,
                 fpu_mul(dt(), u2f(0x40490fdbu)));
        const uint32_t sp = ee_stack_alloc(0x60);
        swc1(sp, cos_vu(lwc1(m + 0x48)));
        swc1(sp + 4, sin_vu(lwc1(m + 0x48)));
        sw(sp + 8, 0);
        walker_record_step(1.0f, m, pv + 0x60, sp, pv + 0x40);
        ee_stack_free(0x60);
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xb8))) != 0 ||
            vec2_distance(V(m + 0x10), V(lvd::path(lw(pv + 0xb0)) + 0x10 + (lwu(pv + 0xb4) << 4))) < 1.0f) {
            const int32_t ahead = rand_mod(lw(lvd::path(lw(pv + 0xb0))));
            sw(pv + 0xb4, static_cast<uint32_t>(ee_rem(lw(pv + 0xb4) + ahead, lw(lvd::path(lw(pv + 0xb0))))));
            sw(pv + 0xb8, static_cast<uint32_t>(frame_scale_i(0x258)));
        }
        ril_2db018_l5(m);
        break;
    }
    case 2:  // (0x002daef0) burst: back when the hero is far
        if (48.0f < vec2_distance(V(m + 0x10), V(kHero + 0x80))) {
            sb(m + 0x31, 1);
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
            sb(m + 0x20, 1);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        }
        break;
    default:
        break;
    }
}

}  // namespace rac1
