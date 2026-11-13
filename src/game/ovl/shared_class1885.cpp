// Class 1885: one port of the copies Gaspar (L9) and Gemlik Base (L13) carry (the same code relinked; compared word by
// word: only relocated words differ). Nothing of it is the level's own: the engine's words are OV (the path table
// 0x1b04b0, the cameras' list 0x1600ec (0x80 bytes each, +0x30 a point, +0 / +0x10 two axes), the zero vector 0x15f580),
// the base ELF's bare (the hero 0x13f3d0, the frame's time 0x15ed6c and its square 0x15ed70, the level's number 0x15ed84).
// Header shared_class1885.h.
//
// The pvars: +0x20 the health, +0x60 the colour pulse, +0x70 the explosion's record, +0x80 the throw's reach, +0x84 a box
// and +0x88 a path (-1 none) its target is looked for in, +0x8c / +0x8e the frames between throws and their timer, +0x90
// the most blobs out at once, +0x91 those out, +0x92 aim at the hero's nearest camera point (else at a random one of its
// four), +0x93 the blobs' kind, +0x94 its group, +0xa0 .. +0xac four indices of the cameras' list (-1 none), +0xb0 its
// blobs (20 words), +0x100 the target's point, +0x140 the target, +0x144 2 when it has none.
//
// The 1.02 build (the PAL disc 2.00) changed two things here: a killed one's bolts on Gemlik Base, with Ratchet at the height
// 250 (within 4), get moby_drop_bolts' flag 0x200 (their flag 2, as when Ratchet stands next to it): 1.02 Gameplay fix 25
// (fixes102.h), off by default; and
// no blob is thrown, while some are out, when either of the frame's load factors (0x15f5d0 / 0x15f5d4) is 0.85 or more: a
// throttle for the frame's load, not ported.
#include <game/ovl/shared_class1885.h>

#include <game/enhance/fixes102.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/metal.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr int32_t BLOB_CLASS = 0x75e;  // (1886)
constexpr int32_t SLOTS = 20;          // (its list of blobs, pvars +0xb0)

// the cameras' list entry `idx`
uint32_t camera_point(int32_t idx) { return lwu(OV(0x001600ec)) + (static_cast<uint32_t>(idx) << 7); }
}  // namespace

// orig: L9:0x00277688 L13:0x00263988
float rand_radius(float radius) {
    const int32_t r = (libc_rand() >> 16) & 0xfff;
    const float u = fpu_mul(static_cast<float>(r), u2f(0x39800000u));  // (1 / 4096)
    return fpu_mul(fpu_sub(1.0f, fpu_mul(u, u)), radius);
}

// orig: L9:0x00277758 L13:0x00263a58
void rand_disc_offset(uint32_t out, float radius) {
    const float a = rand_angle();
    const float len = rand_radius(radius);
    vec_clear(V(out));
    swc1(out, fpu_mul(cos_vu(a), len));
    swc1(out + 4, fpu_mul(sin_vu(a), len));
}

// orig: L9:0x0030a9e8 L13:0x0030d3b8
void class1885_hit(uint32_t m, uint32_t pv) {
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0xc30000u), 0);
    int32_t st = 0;
    moby_hit_react_r(m, hit, pv + 0x20, 0, st, nullptr, nullptr, 4);
    if (st >= 2) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit + 0x2c));
        swc1(pv + 0x20, health);
        if (0.0f < health) {
            sb(pv + 0x67, 0x5a);
        } else {
            // enhancement: 1.02 Gameplay, fix 25 (fixes102.h): on Gemlik Base with Ratchet at the height 250, the bolts' flag 0x200
            const bool to_hero = gameplay102_on(25) && lw(0x0015ed84) == 0xd && abs_f(fpu_sub(lwc1(0x0013f3d8), 250.0f)) < 4.0f;
            moby_drop_bolts(m, to_hero ? 0x200u : 0u, -1);
            sb(m + 0x20, 2);
            sb(pv + 0x67, 0x78);
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
}

// orig: L9:0x0030aae0 L13:0x0030d4b0
int32_t class1885_free_slot(uint32_t, uint32_t pv) {
    sb(pv + 0x91, static_cast<uint32_t>(moby_group_count_l1(lw(pv + 0x94), -1)));
    int32_t free = -1;
    for (int32_t i = 0; i < SLOTS; ++i) {
        const uint32_t b = lwu(pv + 0xb0 + static_cast<uint32_t>(i) * 4);
        if (b == 0 || lh(b + 0xa6) != BLOB_CLASS || lbu(b + 0x20) == 0xfe || lbu(b + 0x20) == 0xfd) free = i;
        else sb(pv + 0x91, lbu(pv + 0x91) + 1);
    }
    return free;
}

// orig: L9:0x0030b218 L13:0x0030dbe8
uint32_t class1885_throw(uint32_t m, uint32_t pos, uint32_t vel, uint32_t kind, uint32_t slot) {
    const uint32_t b = moby_spawn(BLOB_CLASS);
    if (b == 0) return 0;
    const uint32_t bp = lwu(b + 0x78);
    sw(bp + 0x10, m);
    lq_sq(b + 0x10, pos);
    lq_sq(bp, vel);
    const float pi = u2f(0x40490fdbu);
    swc1(b + 0x40, rand_range_f(-pi, pi));
    swc1(b + 0x44, rand_range_f(-pi, pi));
    sw(bp + 0x14, 0);
    sh(bp + 0x18, static_cast<uint32_t>(frame_scale_i(0x1f4)));
    sb(b + 0x30, 0xff);
    sh(b + 0x32, 0x40);
    sb(b + 0x31, 1);
    sb(b + 0x20, 0);
    swc1(b + 0x2c, fpu_mul(lwc1(lwu(b + 0x24) + 0x24), u2f(0x3e99999au)));  // (0.3)
    sb(bp + 0x1c, slot & 0xff);
    sb(bp + 0x1a, kind & 0xff);
    sb(bp + 0x1b, 0);
    moby_update_transform(b);
    return b;
}

// Hit (twice in state 1), then by its state: 0 its group's members retired, then 1; 1 its target followed (a search one
// frame in seven: in its box and path, within its reach), and when its throw timer runs out, a blob in a free slot while
// fewer than its most are out: thrown in an arc (0.6 of the way a second, under gravity 9.8) at a point by a camera
// point (the hero's nearest, or a random one of its four, jittered along its two axes) or, with none, halfway to the
// target (3 to its reach) or about itself; 2 killed: an explosion, three pieces, gone.
// orig: L9:0x0030ab80 L13:0x0030d550
void class1885_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class1885_hit(m, pv);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        const uint32_t sp = ee_stack_alloc(0x130);
        const uint32_t member = sp + 0x60;
        sw(member, 0);
        while (group_first(member, lw(pv + 0x94), 0, 0) == 0) moby_retire(lwu(member));
        sb(m + 0x20, 1);
        ee_stack_free(0x130);
        return;
    }
    if (state == 2) {
        explosion_effect(m, pv + 0x70, 0, 0x1e, 8, 0x14, 1, 1, 1, -1, 0, 0.0f, 0.0f, 8.0f, 4.0f, 18.0f, 1.0f, 15.0f);
        const uint32_t none = OV(0x0015f580);
        for (uint32_t piece : {0x6c5u, 0x6c6u, 0x6c7u}) debris_spawn(m, piece, m + 0x10, m + 0x40, 0, 0, none, none, none, 0.0f);
        moby_retire(m);
        return;
    }
    if (state != 1) return;
    class1885_hit(m, pv);
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x8e))) == 0) return;
    const uint32_t point = pv + 0x100;
    if (rand_mod(7) != 0) {
        const uint32_t target = lwu(pv + 0x140);
        if (target == 0 || lbu(target + 0x20) == 0xfe || lbu(target + 0x20) == 0xfd) {
            sw(pv + 0x140, 0);
            sw(pv + 0x144, 2);
        } else {
            lq_sq(point, target + 0x10);
        }
    } else {
        uint32_t box = 0, poly = 0;
        int32_t nbox = 0, npoly = 0;
        if (lw(pv + 0x84) != -1) {
            box = pv + 0x84;
            nbox = 1;
        }
        if (lw(pv + 0x88) != -1) {
            const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x88) << 2));
            poly = path + 0x10;
            npoly = lw(path);
        }
        target_select(m, point, box, nbox, poly, npoly, lwc1(pv + 0x80));
    }
    if (lw(pv + 0x144) == 2) return;
    const int32_t slot = class1885_free_slot(m, pv);
    if (!(lbu(pv + 0x91) < lbu(pv + 0x90))) return;
    if (slot == -1) return;
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t aim = sp + 0x20, vel = sp + 0x30, from = sp + 0x40, nudge = sp + 0x50;
    const uint32_t pos = m + 0x10;
    // the camera point it aims by
    uint32_t cam = 0;
    if (lbu(pv + 0x92) != 0) {
        float nearest = 100000.0f;
        for (uint32_t k = 0; k < 4; ++k) {
            const int32_t idx = lw(pv + 0xa0 + k * 4);
            if (idx == -1) continue;
            const uint32_t e = camera_point(idx);
            const float d = vec3_distance(V(0x0013f3d0), V(e + 0x30));
            if (d < nearest) {
                cam = e;
                nearest = d;
            }
        }
        if (cam == 0) {
            // halfway to the target, 3 to its reach away
            vec3_sub(V(aim), V(point), V(pos));
            vec3_scale(V(aim), V(aim), 0.5f);
            if (3.0f < lwc1(pv + 0x80)) vec3_clamp_length_vu(V(aim), V(aim), 3.0f);
            if (vec3_length(V(aim)) < 3.0f) vec3_set_length(V(aim), V(aim), 3.0f);
            vec3_add(V(aim), V(aim), V(pos));
        }
    } else {
        const int32_t first = rand_range(0, 3);
        for (int32_t k = 0; k < 4; ++k) {
            const int32_t idx = lw(pv + 0xa0 + static_cast<uint32_t>((k + first) % 4) * 4);
            if (idx == -1) continue;
            cam = camera_point(idx);
            break;
        }
        if (cam == 0) {
            float reach = lwc1(pv + 0x80);
            if (reach < 3.0f) reach = 3.0f;
            rand_disc_offset(aim, reach);
            vec3_add(V(aim), V(aim), V(pos));
        }
    }
    if (cam != 0) {
        lq_sq(aim, cam + 0x30);
        vec3_scale(V(vel), V(cam), rand_range_f(-1.0f, 1.0f));
        vec3_add(V(aim), V(aim), V(vel));
        vec3_scale(V(vel), V(cam + 0x10), rand_range_f(-1.0f, 1.0f));
        vec3_add(V(aim), V(aim), V(vel));
    }
    // onto the floor there
    const float floor = floor_probe_v(0.5f, V(aim), 0);
    if (floor != 0.0f) swc1(aim + 8, floor);
    // the throw: 0.6 of the way a second, from its joint 0 nudged 0.05 along it (and jittered)
    vec3_sub(V(vel), V(aim), V(pos));
    const float speed = fpu_mul(vec3_length(V(vel)), fpu_mul(lwc1(0x0015ed6c), u2f(0x3f19999au)));  // (0.6)
    vec3_set_length(V(vel), V(vel), speed);
    moby_chain_point_v(m, 0, V(from));
    vec3_set_length(V(nudge), V(vel), u2f(0x3d4ccccdu));  // (0.05)
    metal_25f8e0(nudge, u2f(0x3ccccccdu));                 // (0.025)
    vec3_add(V(from), V(from), V(nudge));
    const float g = fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu));  // (9.8)
    swc1(vel + 8, sub_0025abf0(from, aim, nullptr, speed, -g));
    const uint32_t blob = class1885_throw(m, from, vel, lbu(pv + 0x93), static_cast<uint32_t>(slot) & 0xff);
    sw(pv + 0xb0 + static_cast<uint32_t>(slot) * 4, blob);
    sh(pv + 0x8e, static_cast<uint32_t>(frame_scale_i(lh(pv + 0x8c))));
    sb(pv + 0x91, lbu(pv + 0x91) + 1);
    ee_stack_free(0x130);
}

}  // namespace rac1
