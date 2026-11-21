// Gemlik Base's (level 13) class 170: a tower that breaks. It hums and sparks; it is hit through a hit proxy it spawns
// at its chain point 0 (class 1632, 0x660, which passes the hits on), and only by attackers more than 8.7 above it;
// broken (its health gone), it bursts at its chain points one after another, then into debris and flashes, and marks
// itself in the level's table L13:0x1847a8 (its +0x74; the save's bit, its +0xb2, keeps it broken). Level 13 only: the
// addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in
// gemlik_mobys.cpp.
//
// Its pvars: +0x20 its hit record (+0x20 its health), +0x60 its colour pulse, +0x70 its hum (a sound slot, -1 none),
// +0x74 its entry of L13:0x1847a8 (-1 none). States: 0 set up (or gone when already broken), 1 standing, 2 breaking
// (its +0xbc the bursts done). 1.02 Fixes, fix 43 (always on the PAL 2.00 disc): its proxy as the 1.02 build makes it.
#include <game/enhance/fixes102.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {
// the tower's entry of the level's table L13:0x1847a8 (+0x74, -1 none) set to `v`
void mark(uint32_t pv, uint32_t v) {
    const int32_t i = lw(pv + 0x74);
    if (i != -1) sw(L13_ADDR(0x001847a8) + (static_cast<uint32_t>(i) << 2), v);
}
}  // namespace

// The sound slot `slot` (0x13e550 + 0x70 slot) given the position `pos` (an EE vector) to follow: its +0x75 bit 0x40
// and its +0xa0. (The original returns 1, which its caller never reads.)
// orig: L13:0x002982e8
void gml_2982e8_l13(int32_t slot, uint32_t pos) {
    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
    sb(e + 0x75, lbu(e + 0x75) | 0x40u);
    lq_sq(e + 0xa0, pos);
}

// The tower's debris at `pos` (an EE point): smoke puffs, two sets of four chunks (classes 0x162 and 0x163 / 0x164)
// flung up, and with `flash`, sparks (more the nearer the camera) and flashes (two more when the screen's fade
// L13:0x15f5d0 is under 0.95).
// orig: L13:0x002ce688
void gml_2ce688_l13(uint32_t m, uint32_t pos, uint32_t flash) {
    const Frame f(336);
    const uint32_t sp = f.sp;
    const uint32_t at = sp, puff = sp + 0x10, vel = sp + 0x20, cp = sp + 0x30, rnd = sp + 0x40;
    lq_sq(at, pos);
    lq_sq(puff, at);
    swc1(puff + 8, fpu_add(lwc1(puff + 8), u2f(0x3e99999au)));
    spawn_particle_0d(puff, 1, 0x40808080u, u2f(0x3e99999au), u2f(0x3f8147aeu), u2f(0x3f9a3d71u), u2f(0x3d4ccccdu), 150000.0f);
    for (int k = 0; k < 5; ++k)
        spawn_particle_0d(puff, 1, 0x40808080u, u2f(0x3e99999au), u2f(0x3f8147aeu), u2f(0x3f970a3eu), u2f(0x3d4ccccdu), 150000.0f);
    // a chunk at a random place in the box (+-1.875, +-1.875, 0..3.75) above the puff, flung away from the point
    auto chunk_place = [&](uint32_t p, uint32_t v) {
        sq_zero(rnd);
        swc1(rnd + 0, rand_range_f(-1.875f, 1.875f));
        swc1(rnd + 4, rand_range_f(-1.875f, 1.875f));
        swc1(rnd + 8, rand_range_f(0.0f, 3.75f));
        lq_sq(p, rnd);
        vec3_add(V(p), V(p), V(puff));
        vec3_sub(V(v), V(p), V(at));
        vec3_set_length(V(v), V(v), rand_range_f(u2f(0x3ccccccdu), u2f(0x3e6b851fu)));
    };
    for (int k = 0; k < 4; ++k) {
        chunk_place(cp, vel);
        chunk_spawn(m, vel, cp, L13_ADDR(0x0015f580), 0x162, 0, u2f(0x3f666667u), u2f(0x40066666u));
    }
    for (int k = 0; k < 4; ++k) {
        chunk_place(cp, vel);
        chunk_spawn(m, vel, cp, L13_ADDR(0x0015f580), static_cast<uint32_t>(rand_mod(2) + 0x163), 0, u2f(0x3f99999au), u2f(0x4019999au));
    }
    if (flash == 0) return;
    vec3_sub(V(cp), V(L13_ADDR(0x001670c0)), V(at));
    vec_clear(V(rnd));
    const float d = vec3_length(V(cp));
    const int32_t n = d < 8.0f ? float_to_int(d) + 2 : 10;
    const float near = d < 7.0f ? fpu_sub(7.0f, d) : 0.0f;
    for (int32_t k = n; k > 0; --k) {  // sparks of the two colour tables L13:0x1f4950 / 0x1f4968 (six words each)
        const float r = rand_range_f(8.0f, 10.0f);
        for (uint32_t w = 0; w < 0x18; w += 4) sw(sp + 0x50 + w, lwu(L13_ADDR(0x001f4950) + w));
        for (uint32_t w = 0; w < 0x18; w += 4) sw(sp + 0x70 + w, lwu(L13_ADDR(0x001f4968) + w));
        const float speed = fpu_sub(fpu_mul(r, dt()), fpu_mul(near, dt()));
        const uint32_t c0 = sp + 0x50 + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const uint32_t c1 = sp + 0x70 + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const int32_t life = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
        const int32_t count = rand_range(frame_scale_i(0x19), frame_scale_i(0x1e));
        spawn_particle_0b(at, rnd, lwu(c0), lwu(c1), static_cast<uint32_t>(life), static_cast<uint32_t>(count), 0, 0, 1200000.0f, speed);
    }
    if (lwc1(L13_ADDR(0x0015f5d0)) < u2f(0x3f733333u)) {
        shard_spawn(m, at, rnd, static_cast<uint32_t>(frame_scale_i(0xf)), 0x7f, 0x7f, 0x7f, 0x20, 9.0f);
        shard_spawn(m, at, rnd, static_cast<uint32_t>(frame_scale_i(0x18)), 0x7f, 0x20, 0, 0x20, 6.0f);
    }
    shard_spawn(m, at, rnd, static_cast<uint32_t>(frame_scale_i(0x14)), 0x7f, 0x40, 0, 0x30, 9.0f);
    shard_spawn(m, at, rnd, static_cast<uint32_t>(frame_scale_i(0x1b)), 0x60, 0x10, 0, 0x40, 6.0f);
    shard_spawn(m, at, rnd, static_cast<uint32_t>(frame_scale_i(0x1d)), 0x20, 0, 0, 0x20, 3.0f);
}

// The tower's hits on its hit record `hs` (only from an attacker more than 8.7 above it): a hit that takes its health
// breaks it (state 2: its bolts, a burst at its chain point 0, its hum stopped); a lesser one takes its damage.
// orig: L13:0x002cecb8
void gml_2cecb8_l13(uint32_t m, uint32_t pv, uint32_t hs) {
    const Frame f(208);
    const uint32_t sp = f.sp;
    if (lbu(m + 0x20) != 2) {
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
        if (hit != 0) {  // (the original's debug prints of the hit's damage and place)
            debug_printf_stub();
            debug_printf_stub();
        }
        int32_t st = lw(sp + 0x40);
        const int32_t react = moby_hit_react_r(m, hit, hs, 0, st, nullptr, nullptr, 4);
        if (react == 1 || react == 2) sw(hs + 0, 0);  // (the table L13:0x1f49d0: reactions 1 and 2 (0x002cedb8))
        if (st >= 2 && hit != 0) {
            const uint32_t by = lwu(hit + 0x20);
            if (by != 0 && fpu_add(lwc1(m + 0x18), u2f(0x410b3333u)) < lwc1(by + 0x18)) {
                const float health = lwc1(hs + 0), dmg = lwc1(hit + 0x2c);
                if (health <= dmg) {
                    sq_zero(sp + 0x30);
                    swc1(sp + 0x38, fpu_mul(dt(), 8.0f));
                    sw(hs + 0, 0);
                    sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                    lq_sq(sp + 0x20, sp + 0x30);
                    moby_drop_bolts(m, 0, -1);
                    sb(pv + 0x67, 0x78);
                    color_pulse_start(m, pv + 0x60);
                    moby_chain_point_v(m, 0, V(sp + 0x30));
                    explosion_effect(m, sp + 0x20, sp + 0x30, 0xa, 3, 0x10, 1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 0.0f, 1.0f, 20.0f);
                    if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0);
                    const int32_t slot = lw(pv + 0x70);
                    if (slot != -1) {
                        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
                        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
                    }
                    sw(pv + 0x70, 0xffffffffu);
                    sb(m + 0x20, 2);
                } else {
                    swc1(hs + 0, fpu_sub(health, dmg));
                    sb(pv + 0x67, 0xfa);
                    sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                    color_pulse_start(m, pv + 0x60);
                }
            }
        }
        sb(m + 0xa4, 0xff);
    }
    color_pulse_update(m, pv + 0x60);
}

// The hit proxy (class 1632) of `parent` at `pos` (an EE point), `scale` times its class's scale: the parent and its
// class at its pvars +0x60 / +0x64. With fix 43 (the 1.02 build), its pvars +0 point at their +0x20 (a hit record) and
// its +0x34 gets the flag 0x20. The proxy, or 0.
// orig: L13:0x0030bfb0
uint32_t gml_30bfb0_l13(uint32_t parent, uint32_t pos, float scale) {
    const uint32_t s = moby_spawn(0x660);
    if (s == 0) return 0;
    const uint32_t pv = lwu(s + 0x78);
    sb(s + 0x30, 0xff);
    sh(s + 0x32, 0xff);
    sb(s + 0x31, 1);
    sb(s + 0x20, 0);
    sw(pv + 0x60, parent);
    swc1(s + 0x2c, fpu_mul(lwc1(lwu(s + 0x24) + 0x24), scale));
    lq_sq(s + 0x10, pos);
    if (parent != 0) sw(pv + 0x64, static_cast<uint32_t>(static_cast<int32_t>(lh(parent + 0xa6))));
    if (fixes102_gemlik_tower_proxy()) {  // (1.02 Fixes, fix 43: L13:0x30d4e0 .. 0x30d500 on the PAL 2.00 disc)
        sw(pv + 0x00, pv + 0x20);
        sh(s + 0x34, lhu(s + 0x34) | 0x20u);
    }
    moby_update_transform(s);
    return s;
}

// orig: L13:0x002cefc0
void gml_class170_update_l13(uint32_t m) {
    const Frame f(224);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {  // set up; gone at once when the save says it is broken
        sb(m + 0xbc, 0);
        if (static_cast<int32_t>(lbu(m + 0x30)) < lh(m + 0x32)) sb(m + 0x30, lbu(m + 0x32));
        sb(m + 0x20, 1);
        sw(pv + 0x70, 0xffffffffu);
        const uint32_t id = lhu(m + 0xb2);
        const uint32_t word = lwu(0x0014c190u + (lwu(0x0015ed84) << 8) + (static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(id)) >> 5) << 2));
        if (((static_cast<int32_t>(word) >> (id & 0x1f)) & 1) != 0) {
            mark(pv, 1);
            moby_retire(m);
            return;
        }
        moby_chain_point_v(m, 0, V(sp + 0x20));
        swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), u2f(0x3f8ccccdu)));
        // (1.02 Fixes, fix 43: 2.8, L13:0x2d03bc on the PAL 2.00 disc)
        gml_30bfb0_l13(m, sp + 0x20, fixes102_gemlik_tower_proxy() ? u2f(0x40333333u) : u2f(0x402ccccdu));
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        return;
    }
    case 1: {  // standing: its hum 8.7 above it, sparks now and then, its hits
        mark(pv, 0);
        bool place = true;
        if (lw(pv + 0x70) != -1 && sub_0028d8c0(m, lw(pv + 0x70)) != 0) {
            place = false;
        } else if (lw(pv + 0x70) == -1) {
            const int32_t slot = moby_sound_start(0, 4, m);
            sw(pv + 0x70, static_cast<uint32_t>(slot));
            place = slot != -1;
        }
        if (place) {
            sq_zero(sp + 0x20);
            swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), u2f(0x410b3333u)));
            gml_2982e8_l13(lw(pv + 0x70), sp + 0x20);
        }
        if (lbu(m + 0x31) != 0 && rand_mod(9) == 0) {
            const uint32_t r = static_cast<uint32_t>(libc_rand() + 0x30) & 0x3fu;
            const uint32_t g = static_cast<uint32_t>(libc_rand() + 0x20) & 0x3fu;
            const uint32_t b = static_cast<uint32_t>(libc_rand()) & 0x2fu;
            moby_chain_point_v(m, 0, V(sp + 0x20));
            swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), 0.5f));
            const float size = rand_range_f(900000.0f, 1800000.0f);
            spawn_particle_05(sp + 0x20, r, g, b, frame_scale_i(ee_rem(libc_rand(), 0x28) + 0x28), size, 0.0f);
        }
        gml_2cecb8_l13(m, pv, pv + 0x20);
        return;
    }
    case 2: {  // breaking: a burst at its chain points 1, 2, 3 as its animation crosses 8.5, 17, 17; at its end, debris
        sq_zero(sp + 0x30);
        swc1(sp + 0x38, fpu_mul(dt(), 8.0f));
        lq_sq(sp + 0x20, sp + 0x30);
        const uint32_t done = lbu(m + 0xbc);
        const float t = done == 0 ? 8.5f : (done == 1 || done == 2) ? 17.0f : 1000.0f;
        if (t < 100.0f && anim_crossed(m, t) != 0) {
            moby_chain_point_v(m, lbu(m + 0xbc) + 1, V(sp + 0x30));
            explosion_effect(m, sp + 0x20, sp + 0x30, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 0.0f, 1.0f, 20.0f);
            sb(m + 0xbc, lbu(m + 0xbc) + 1);
        }
        if ((lbu(m + 0x70) & 2u) == 0) return;
        explosion_effect(m, sp + 0x20, 0, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 0.0f, 1.0f, 20.0f);
        const uint32_t top = sp + 0x30, at = sp + 0x40;
        moby_chain_point_v(m, 3, V(top));
        vec3_sub(V(top), V(top), V(m + 0x10));
        for (int k = 0; k < 5; ++k) {  // debris along its height
            vec3_scale(V(at), V(top), rand_range_f(0.0f, 3.0f));
            vec3_add(V(at), V(at), V(m + 0x10));
            gml_2ce688_l13(m, at, 1);
        }
        mark(pv, 1);
        moby_retire(m);
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
