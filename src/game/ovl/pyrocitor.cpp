// Ported overlay code: the Pyrocitor (item 0x10, hand moby class 0xb0). Its hand moby's update
// 0x2b77f0 fires while the weapon's button is held (and ammo is left): a light at the hand, the
// flame's reach found against the level, a spray of flame particles (type 0xc, 0x26b230) along
// it, the mobys burnt at two points in front of the hand, sparks (type 0x19) now and then, the
// ammo used every few frames; the nozzle glow moby (class 0xb3, 0x2bb360 / 0x2bb400) follows
// the hand's chain point 0. The kept flame particles (0x2b8798) burn what they touch (0x2b8438)
// and throw streaks when they die. The hand's pvars: +0 the nozzle moby, +4..+0x3f 15 kept
// flame particles, +0x44 the light slot, +0x4a the fire sound, +0x4c the fire's least time,
// +0x4e the sparks' timer, +0x50 the frames fired in a row (0x2b8230).
#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
constexpr uint32_t PAD = 0x0013c940;
int16_t& h16(uint32_t a) { return ee_ref<int16_t>(a); }
}  // namespace

// The UV offsets of every packet of the moby class `cls` moved by (`du`, `dv`) (a scrolling
// texture): per packet (+0 table, 0x10 each, +4 / +5 counts) the UV block at its word +0, the
// count at byte +2, then (u, v) u16 pairs.
// orig: 0x0024efb0
void class_uv_scroll(uint32_t cls, int32_t du, int32_t dv) {
    const int32_t n = static_cast<int32_t>(lbu(cls + 4) + lbu(cls + 5));
    if (n == 0) return;
    uint32_t t = lwu(cls);
    for (int32_t k = 0; k < n; ++k) {
        uint32_t a = lwu(t);
        t += 0x10;
        const uint32_t c = lbu(a + 2);
        a += 4;
        for (uint32_t i = 0; i < c; ++i, a += 4) {
            sh(a, static_cast<uint32_t>(lhu(a) + static_cast<uint32_t>(du)));
            sh(a + 2, static_cast<uint32_t>(lhu(a + 2) + static_cast<uint32_t>(dv)));
        }
    }
}

// A flame particle (type 0xc) at `pos` moving by `vel`, size word `size` (+0x30): with flag 1
// the long-lived look (colours / life / size from 0x1601c8..0x1601e4, the texture 0x1b20e0),
// else the short one (0x1601a8..0x1601c4, a texture of the table 0x1b20b0, the upgraded set
// 8.. with flag 4 and the upgrade 0x13e530); with flag 4 its life scaled by (upgrade + 2) / 2.
// The colour swaps of the upgrade are computed and dropped (as in the original).
// orig: 0x0026b230
uint32_t spawn_particle_0c(uint32_t pos, uint32_t vel, uint32_t flags, float size) {
    const uint32_t fl = flags & 0xffu;
    const uint32_t p = alloc_particle(0xc);  // (0x2678c0 with a1 = 0: alloc_particle's body)
    if (p == 0) return 0;
    const uint32_t q = p + 0x20;
    V(p + 0x10) = V(pos);
    V(q) = V(vel);
    uint32_t up4;
    if ((fl & 1) != 0) {
        const float sz = lwc1(OV(0x001601d8)) * 210000.0f;
        const float lo = static_cast<float>(lw(OV(0x001601e0))), hi = static_cast<float>(lw(OV(0x001601e4)));
        up4 = fl & 4;
        sw(p + 4, lwu(OV(0x001601c8)));
        swc1(p + 0xc, sz);
        sh(p + 0xa, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(lo, hi)))));
        sw(q + 0x14, rgba_lerp(lwu(OV(0x001601c8)), lwu(OV(0x001601cc)), rand_range_f(0.0f, 1.0f)));
        sw(q + 0x18, rgba_lerp(lwu(OV(0x001601d0)), lwu(OV(0x001601d4)), rand_range_f(0.0f, 1.0f)));
        sb(p + 3, 0x48);
        sb(p + 2, lbu(lwu(OV(0x001b20e0))));
    } else {
        const float lo = static_cast<float>(lw(OV(0x001601c0))), hi = static_cast<float>(lw(OV(0x001601c4)));
        const float sz = lwc1(OV(0x001601b8)) * 210000.0f;
        int32_t set = 0;
        sw(p + 4, lwu(OV(0x001601a8)));
        swc1(p + 0xc, sz);
        sh(p + 0xa, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(lo, hi)))));
        sw(q + 0x14, rgba_lerp(lwu(OV(0x001601a8)), lwu(OV(0x001601ac)), rand_range_f(0.0f, 1.0f)));
        sw(q + 0x18, rgba_lerp(lwu(OV(0x001601b0)), lwu(OV(0x001601b4)), rand_range_f(0.0f, 1.0f)));
        up4 = fl & 4;
        if (up4 != 0) set = lbu(0x0013e530) != 0 ? 8 : 0;
        const int32_t k = rand_mod(8) + set;
        sb(p + 3, 0x44);
        sb(p + 2, lbu(lwu(OV(0x001b20b0)) + static_cast<uint32_t>(k)));
    }
    sb(p + 1, 0);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
    sb(q + 0x1e, static_cast<uint32_t>(float_to_int(rand_range_signed_f(1.0f, 4.0f))));
    swc1(q + 0x10, size);
    sb(q + 0x1f, fl);
    if (up4 != 0) {
        const int32_t v = lh(p + 0xa) * static_cast<int32_t>(lbu(0x0013e530) + 2u);
        sh(p + 0xa, static_cast<uint32_t>(v / 2));
        (void)color_swap_channels(lwu(p + 4), lbu(0x0013e530));
        (void)color_swap_channels(lwu(q + 0x14), lbu(0x0013e530));
        (void)color_swap_channels(lwu(q + 0x18), lbu(0x0013e530));
    }
    sb(q + 0x1d, lbu(p + 0xa));
    return p;
}

// Type 0xc (a flame): spins (+8 by +0x3e), moves by its velocity whose speed eases from its
// length toward 0 (the seek's position +0x30, rate 0x1601e8), rises (0x1601ec), grows from the
// end size to the start size of its look (0x1601d8 / 0x1601dc with flag 1, else 0x1601b8 /
// 0x1601bc; the start one times 1 + upgrade / 2 with flag 4) and lerps its colour between +0x38
// and +0x34 over its life, fading out over its last 10 of 15 frames; freed off the level's
// 2 .. 1021 box or when its life runs out (which stalls while +0x30 is under 0.01).
// orig: 0x0026b500
void particle_0c_update(uint32_t p) {
    const uint32_t q = p + 0x20;
    float big = 1.0f;
    sb(p + 8, lbu(p + 8) + lbu(q + 0x1e));
    if ((lbu(q + 0x1f) & 4) != 0) big = int_to_float(lbu(0x0013e530)) * 0.5f + big;
    vec3_add(V(p + 0x10), V(p + 0x10), V(q));
    float vel = -vec3_length(V(q));
    seek_1d_v(*static_cast<float*>(ee_ram_ptr(p + 0x30)), vel, 0.0f, 0.0f, lwc1(OV(0x001601e8)) * lwc1(0x0015ed70), vel);
    vec3_set_length(V(q), V(q), -vel);
    swc1(p + 0x18, lwc1(p + 0x18) + lwc1(OV(0x001601ec)) * lwc1(0x0015ed6c));
    const float t = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(lbu(q + 0x1d)));
    float s0, s1;
    if ((lbu(q + 0x1f) & 1) != 0) {
        s0 = lwc1(OV(0x00166c00u - 0x6a24));
        s1 = lwc1(OV(0x00166c00u - 0x6a28));
    } else {
        s0 = lwc1(OV(0x00166c00u - 0x6a44));
        s1 = lwc1(OV(0x00166c00u - 0x6a48));
    }
    s0 = s0 * big;
    swc1(p + 0xc, ((s1 - s0) * t + s0) * 210000.0f);
    sw(p + 4, rgba_lerp(lwu(q + 0x18), lwu(q + 0x14), t));
    if (lh(p + 0xa) < frame_scale_i(0xf)) {
        const float k = fpu_div(int_to_float(lh(p + 0xa)), int_to_float(frame_scale_i(10)));
        const uint32_t c = lwu(p + 4);
        sw(p + 4, rgba_lerp(c & 0x00ffffffu, c, k));
    }
    const float x = lwc1(p + 0x10), y = lwc1(p + 0x14), z = lwc1(p + 0x18);
    const float hi = u2f(0x447f4000u);
    if (x < 2.0f || hi < x || y < 2.0f || hi < y || z < 2.0f || hi < z || timer_tick_i16(h16(p + 0xa)) != 0) {
        free_particle(p);
        return;
    }
    if (lwc1(q + 0x10) < u2f(0x3c23d70au)) sh(p + 0xa, lhu(p + 0xa) - 1u);
}

// The nozzle glow (class 0xb3) for the hand moby `m`: drawn (+0x34 0x204), alpha 0x50, its
// parent at pvars +8, the hand's matrix.
// orig: 0x002bb360
uint32_t pyrocitor_nozzle_spawn(uint32_t m) {
    const uint32_t o = moby_spawn(0xb3);
    if (o == 0) return 0;
    const uint32_t pv = lwu(o + 0x78);
    sb(o + 0x30, 0xff);
    sh(o + 0x32, 0xff);
    sb(o + 0x31, 1);
    sh(o + 0x34, 0x204);
    sb(o + 0x23, 0x50);
    sb(o + 0x20, 0);
    sw(pv + 8, m);
    vec_clear(V(o + 0x40));
    for (uint32_t i = 0; i < 0x30; i += 0x10) V(o + 0xc0 + i) = V(m + 0xc0 + i);
    moby_update_transform_kept(o);
    moby_update_transform(o);
    return o;
}

// The nozzle glow's update: retired with its hand moby (state 0 or 0xf0..); shown as the hand
// is; hidden early in the hero's item switch (+0x10b0 below 25 frames); its texture scrolled
// (pvars +4, steps of -0xc0 wrapping by 0xf40 / -0x10c0 within 0..0x1000); a flickering size
// (the class scale +0x24 times +0xc times 0.5..1) and a spin of +0xc a frame.
// orig: 0x002bb400
void pyrocitor_nozzle_update(uint32_t o) {
    const uint32_t pv = lwu(o + 0x78);
    const uint32_t hand = lwu(pv + 8);
    const uint32_t hs = lbu(hand + 0x20);
    if (hs == 0 || (hs & 0xf0u) == 0xf0u) {
        moby_retire(o);
        return;
    }
    if (hand != 0 && (lhu(hand + 0x34) & 1u) != (lhu(o + 0x34) & 1u)) {
        const uint32_t f = lhu(o + 0x34) & 0xfffeu;
        sh(o + 0x34, f);
        sh(o + 0x34, f | (lhu(lwu(pv + 8) + 0x34) & 1u));
    }
    if (lw(P + 0x10b0) < frame_scale_i(0x19)) sh(o + 0x34, lhu(o + 0x34) | 0x41u);
    const int32_t was = lw(pv + 4);
    const int32_t now = was - 0xc0;
    sw(pv + 4, static_cast<uint32_t>(now));
    int32_t dv = -0xc0;
    if (!(now < 0x1001)) {
        sw(pv + 4, static_cast<uint32_t>(was - 0x10c0));
        dv = -0x10c0;
    } else if (now < 0) {
        sw(pv + 4, static_cast<uint32_t>(was + 0xf40));
        dv = 0xf40;
    }
    class_uv_scroll(lwu(o + 0x24), 0, dv);
    const float k = rand_range_f(0.5f, 1.0f);
    swc1(o + 0x2c, lwc1(lwu(o + 0x24) + 0x24) * lwc1(pv + 0xc) * k);
    swc1(o + 0x44, angle_add(lwc1(o + 0x44), angle_wrap_mod(lwc1(pv + 0xc))));
    moby_update_transform_kept(o);
}

// The flame particle `p` kept in the first free one of the hand's 15 slots (pvars +4..), if any.
// orig: 0x002b8798
void pyrocitor_flame_keep(uint32_t m, uint32_t p) {
    const uint32_t pv = lwu(m + 0x78);
    if (lwu(pv + 4) == 0) {
        sw(pv + 4, p);
        return;
    }
    for (uint32_t i = 1; i < 0xf; ++i) {
        if (lwu(pv + 4 + i * 4) == 0) {
            sw(pv + 4 + i * 4, p);
            return;
        }
    }
}

// The fire's bookkeeping, before the update: on the hand's drop (+0x53 == 2) it goes (state 4)
// with its nozzle, light and sound; else (no cutscene 0x141bd8, the stats 0x141848 open) the
// frames fired in a row (+0x50) are counted while firing (state 3) and, when it stops, a long
// burst (over 120 frames) is logged in the stats (+0x50 count, +0x52 the longest in tens of
// seconds, the bit 0x15ed84 of +0x54), a short one (under 30 frames) counted in the hero's
// +0x20b4, the third of which shows the help message 0x4e24.
// orig: 0x002b8230
void pyrocitor_fire_stats(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x53) == 2) {
        sb(m + 0x20, 4);
        if (lwu(pv) != 0) moby_retire(lwu(pv));
        if (lh(pv + 0x44) != -1) {
            region_clear(static_cast<uint32_t>(lh(pv + 0x44)));
            sh(pv + 0x44, 0xffff);
        }
        const int32_t s = lh(pv + 0x4a);
        if (s != -1) {
            const uint32_t r = static_cast<uint32_t>(s * 0x70) + 0x0013e550;
            if (lwu(r + 0x88) == m && lbu(r + 0x74) != 0) sub_0028d918(s);
        }
        sh(pv + 0x4a, 0xffff);
        return;
    }
    if (lhu(0x00141bd8) != 0) return;
    constexpr uint32_t ST = 0x00141848;
    if (lhu(ST + 0x50) != 0) return;
    if (lbu(m + 0x20) == 3) {
        sh(pv + 0x50, lhu(pv + 0x50) + 1u);
        return;
    }
    const int32_t n = lh(pv + 0x50);
    if (n == 0) return;
    if (frame_scale_i(0x78) < n) {
        const uint32_t c = lhu(ST + 0x50);
        if (!(0xfffeu < c)) sh(ST + 0x50, c + 1u);
        const int32_t tens = frame_scale_i(lw(0x0015eea4)) / 0x258;
        if (static_cast<int32_t>(lhu(ST + 0x52)) < tens) sh(ST + 0x52, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4)) / 0x258));
        sw(ST + 0x54, lwu(ST + 0x54) | (1u << (lwu(0x0015ed84) & 31u)) | 0x80000000u);
    } else if (lh(pv + 0x50) < frame_scale_i(0x1e)) {
        sb(P + 0x20b4, lbu(P + 0x20b4) + 1u);
    }
    sh(pv + 0x50, 0);
    if (!(lbu(P + 0x20b4) < 3)) sub_00203908(0x4e24, 0x4e);
}

// The kept flame particles burn what they touch: each live one (type 0xc) hits the mobys in its
// half size (a type-5 hit along its flight, the upgrade's damage); one in its tenth-last frame
// throws streaks (count 0x16155c, colours 0x161544.., life 0x161554.., texture 0x16152c); the
// gone ones (or under 2 frames) are dropped from the slots.
// orig: 0x002b8438
void pyrocitor_flames_hit(uint32_t m) {
    const uint32_t f = ee_stack_alloc(0x160);  // the original's frame (its records go by address)
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t i = 0; i < 0xf; ++i) {
        const uint32_t slot = pv + 4 + i * 4;
        const uint32_t p = lwu(slot);
        if (p == 0) continue;
        if (lbu(p) != 0xc || static_cast<int8_t>(lbu(p + 1)) < 0) {
            sw(slot, 0);
            continue;
        }
        const float r = fpu_div(lwc1(p + 0xc), 210000.0f) * 0.5f;
        vec2_set_length(V(f + 0x10), V(p + 0x20), 1.0f);
        swc1(f + 0x1c, u2f(0x45afdf66u));
        swc1(f + 0x18, 1.0f);
        sub_00259888(int_to_float(lbu(0x0013e530)) + 1.0f, f + 0x20, static_cast<int32_t>(m), 0x10000, f + 0x10);
        sh(f + 0x3a, lhu(m + 0xa6));
        sb(f + 0x38, 5);
        sb(f + 0x39, 1);
        mobys_in_sphere(p + 0x10, r, 0, m, f + 0x20);
        const int32_t left = lh(p + 0xa);
        if (left != 10) {
            if (left < 2) sw(slot, 0);
            continue;
        }
        for (int32_t k = 0; k < lw(OV(0x00166c00u - 0x56a4));) {
            V(f + 0x50) = V(p + 0x20);
            ++k;
            rand_sphere_point(f + 0x60, 0.0f, lwc1(OV(0x00166c00u - 0x56c8)) * lwc1(0x0015ed6c));
            swc1(f + 0x5c, lwc1(OV(0x00166c00u - 0x56c4)));
            swc1(f + 0x6c, lwc1(OV(0x00166c00u - 0x56c0)));
            swc1(f + 0x68, lwc1(0x0015ed6c) * 4.0f);
            const uint32_t c0 = rgba_lerp(lwu(OV(0x00166c00u - 0x56bc)), lwu(OV(0x00166c00u - 0x56b8)), rand_range_f(0.0f, 1.0f));
            (void)color_swap_channels(c0, lbu(0x0013e530));
            const uint32_t c1 = rgba_lerp(lwu(OV(0x00166c00u - 0x56b4)), lwu(OV(0x00166c00u - 0x56b0)), rand_range_f(0.0f, 1.0f));
            (void)color_swap_channels(c1, lbu(0x0013e530));
            const int32_t h32 = float_to_int(frame_scale_f(rand_range_f(static_cast<float>(lw(OV(0x00166c00u - 0x56ac))), static_cast<float>(lw(OV(0x00166c00u - 0x56a8))))));
            const int32_t h34 = float_to_int(frame_scale_f(rand_range_f(static_cast<float>(lw(OV(0x00166c00u - 0x56ac))), static_cast<float>(lw(OV(0x00166c00u - 0x56a8))))));
            rand_sphere_point(f + 0x70, 0.0f, r + r);
            vec3_add(V(f + 0x70), V(f + 0x70), V(p + 0x10));
            spawn_particle_02(f + 0x70, f + 0x50, f + 0x60, c0, c1, 10, static_cast<uint32_t>(h32), static_cast<uint32_t>(h34),
                              lw(OV(0x00166c00u - 0x56d4)));
        }
    }
    ee_stack_free(0x160);
}

// The Pyrocitor's frame (see the file's head). States: 0 set up, 1 drawn out (its animation
// once past 10 frames with the button held), 2 ready (the nozzle glow spawned, the light faded
// out; the button held with ammo starts the fire, pressed without ammo clicks), 3 firing (the
// fire sound, the light at the hand pulsing between the colours 0x1614f0 / 0x161500, the
// flame's reach seeked toward the free length (the camera's line in first person), the flame
// particles, two burn spheres 0.3 / 0.8 ahead, sparks, ammo, back to 2 when the button is let go
// after the least time or the ammo runs out). Then the nozzle glow on chain point 0 and the
// kept flames' hits.
// orig: 0x002b77f0
void pyrocitor_update(uint32_t m) {
    const uint32_t f = ee_stack_alloc(0x180);  // the original's frame (its records go by address)
    const uint32_t nozzle = f, dir = f + 0x10, from = f + 0x20, to = f + 0x30, off = f + 0x40, pt = f + 0x50, rec = f + 0x60;
    const uint32_t pv = lwu(m + 0x78);
    pyrocitor_fire_stats(m);
    moby_chain_point(m, 0, nozzle);
    auto light = [&](uint32_t o) { return (static_cast<uint32_t>(lh(pv + 0x44)) << 5) + OV(0x001802c0u) + o; };
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sh(pv + 0x4a, 0xffff);
        sh(pv + 0x44, 0xffff);
        sb(m + 0x20, 1);
    } else if (st == 1) {
        if (lbu(m + 0x52) == 1) sb(m + 0x20, 2);
        if (lbu(m + 0x52) == 0 && 10.0f < moby_anim_time(m) && (lw(PAD + 0x1a0) & lw(P + 0x10a0)) != 0 && lbu(m + 0x53) != 1)
            moby_anim_set(m, 1, 0, frame_scale_i(5));
    } else if (st == 2) {
        if (lwu(pv) == 0) {
            const uint32_t o = pyrocitor_nozzle_spawn(m);
            sw(pv, o);
            if (o != 0) {
                sh(o + 0x34, lhu(o + 0x34) | 0x100u);
                swc1(lwu(lwu(pv) + 0x78) + 0xc, lwc1(OV(0x00166c00u - 0x56f0)));
            }
        }
        if (lh(pv + 0x44) != -1) {
            vec3_lerp(V(light(0)), V(light(0)), V(OV(0x0015f580)), u2f(0x3dcccccdu));
            if (vec3_length(V(light(0))) < u2f(0x3a83126fu)) {
                region_clear(static_cast<uint32_t>(lh(pv + 0x44)));
                sh(pv + 0x44, 0xffff);
            }
        }
        if ((lw(PAD + 0x1a0) & lw(P + 0x10a0)) != 0 && lbu(P + 0x20ac) == 0 && sub_00233e98(-1) != 0) {
            sb(m + 0x20, 3);
            hero_hold_start();
            sh(pv + 0x4c, static_cast<uint32_t>(frame_scale_i(10)));
            if (lh(pv + 0x44) == -1) sh(pv + 0x44, static_cast<uint32_t>(sub_0023e5e0(m + 0x10, 0, 7.5f, 0.0f)));
        } else if ((lw(PAD + 0x1a4) & lw(P + 0x10a0)) != 0 && sub_00233e98(-1) == 0) {
            moby_sound_start(0, 0, m);
        }
    } else if (st == 3) {
        if (sub_0028d8c0(m, lh(pv + 0x4a)) == 0) sh(pv + 0x4a, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        if (lh(pv + 0x44) != -1 && lbu(m + 0x20) == 3) {
            // the light: at the hero's side, pulsing every 120 frames
            V(dir) = Vec4{2.0f, 0.0f, 2.0f, 0.0f};
            mat3_transform(V(dir), V(dir), M(0x0013f990));
            vec3_add(V(light(0x10)), V(0x0013f3d0), V(dir));
            const int32_t fr = lw(OV(0x0015f5cc)) % 0x78;
            swc1(light(0x1c), 7.5f);
            const float a = fpu_div(static_cast<float>(fr), 120.0f) * u2f(0x40c90fd0u) - u2f(0x40490fd0u);
            const float t = sin_vu(a) * 0.5f + 0.5f;
            vec3_lerp(V(from), V(OV(0x001614f0)), V(OV(0x00161500)), t);
            vec3_lerp(V(light(0)), V(light(0)), V(from), u2f(0x3dcccccdu));
        }
        // the flame's line: the hand along the hero's facing, or the camera's line
        swc1(f + 0x90, 0.0f);
        swc1(f + 0x94, 0.0f);
        const float up4 = int_to_float(lbu(0x0013e530)) * 4.0f;
        sw(f + 0x98, (lw(OV(0x0015f5cc)) & 3) == 0 ? 1u : 0u);
        if (lw(0x001413d4) == 1 && (lhu(m + 0x34) & 1u) != 0) {
            vec3_set_length(V(from), V(OV(0x00166ff0)), -1.0f);
            vec3_add(V(from), V(from), V(OV(0x00166dc0)));
            vec3_set_length(V(dir), V(OV(0x00166fd0)), lwc1(OV(0x00166c00u - 0x56e0)) + up4 + 0.75f);
            vec3_add(V(to), V(dir), V(from));
        } else {
            V(from) = V(nozzle);
            vec3_set_length(V(dir), V(0x0013f9c0), lwc1(OV(0x00166c00u - 0x56e0)) + up4 + 0.75f);
            advanced_aim_shot(nozzle, dir);  // enhancement: the modern aim's line (advanced.h), the facing otherwise
            vec3_add(V(to), V(dir), V(nozzle));
        }
        float len;
        if (segment_collide(0x0013f420, from, 2, 0, 0) != 0) {
            len = 0.0f;
        } else if (segment_collide(from, to, 2, 0, 0) == 0) {
            len = lwc1(OV(0x00166c00u - 0x56e0)) + up4;
        } else {
            len = vec3_distance(V(from), V(OV(0x00173e60))) - 0.75f;
            if (len < 0.0f) len = 0.0f;
        }
        const float acc = lwc1(OV(0x001601e8));
        seek_1d(f + 0x90, f + 0x94, len, 1.0f, acc * lwc1(0x0015ed70), lwc1(OV(0x00166c00u - 0x56ec)) * lwc1(0x0015ed6c));
        vec3_set_length(V(dir), V(dir), lwc1(f + 0x94));
        int32_t lasting = lw(OV(0x00166c00u - 0x56e8));
        if (len < 4.0f || (lhu(m + 0x34) & 1u) != 0) lasting = 1;
        const int32_t count = lw(OV(0x00166c00u - 0x56e4)) + lasting;
        for (int32_t i = 0; i < count; ++i) {
            const float lo = lwc1(OV(0x00166c00u - 0x56d8));
            const float reach = fpu_div(len, lwc1(OV(0x00166c00u - 0x56e0)) + up4);
            const float spread = ((lwc1(OV(0x00166c00u - 0x56dc)) - lo) * reach + lo) * lwc1(0x0015ed6c);
            rand_sphere_point(pt, 0.0f, spread);
            vec3_add(V(pt), V(pt), V(dir));
            vec3_scale(V(off), V(dir), rand_range_f(0.0f, 1.0f));
            vec3_add(V(off), V(off), V(from));
            const uint32_t p = spawn_particle_0c(off, pt, (i < lasting ? 1u : 0u) | 4u, len);
            if (i == 2 && lw(f + 0x98) != 0) pyrocitor_flame_keep(m, p);
        }
        // the burn: two spheres ahead of the hand, a type-5 hit along the flame
        V(off) = Vec4{u2f(0xbe99999au), 0.0f, 0.0f, 0.0f};
        mat3_transform(V(pt), V(off), M(0x0013f990));
        vec3_add(V(pt), V(pt), V(nozzle));
        sw(rec + 0x14, 0x10000);
        sw(rec + 0x10, m);
        const float dmg = int_to_float(lbu(0x0013e530)) + 1.0f;
        V(rec) = V(dir);
        swc1(rec + 0x1c, dmg);
        sw(rec + 0x20, 1);
        vec2_set_length(V(rec), V(rec), 1.0f);
        swc1(rec + 0xc, u2f(0x45afdf66u));
        swc1(rec + 8, 1.0f);
        sw(rec + 0x20, 1);
        sb(rec + 0x18, 5);
        mobys_in_sphere(pt, u2f(0x3ed70a3du), 5, lwu(P + 0x2080), rec);
        swc1(off, u2f(0xbf4ccccdu));
        mat3_transform(V(pt), V(off), M(0x0013f990));
        vec3_add(V(pt), V(pt), V(nozzle));
        mobys_in_sphere(pt, u2f(0x3ed70a3du), 5, lwu(P + 0x2080), rec);
        if (timer_tick_i16(h16(pv + 0x4e)) != 0) {  // sparks
            const int32_t n = float_to_int(frame_scale_f(rand_range_f(10.0f, 30.0f)));
            sh(pv + 0x4e, static_cast<uint32_t>(n));
            rand_sphere_point(pt, 0.0f, lwc1(0x0015ed6c) + lwc1(0x0015ed6c));
            vec3_scale(V(off), V(dir), rand_range_f(0.5f, 0.1f));
            vec3_add(V(off), V(off), V(pt));
            spawn_particle_19(from, off, lbu(0x0013e530));
        }
        const int32_t per = frame_scale_i(lw(OV(0x00166c00u - 0x56d0)));
        if (per != 0 && lw(OV(0x0015f5cc)) % per == 0) sub_00233db8(-1, 1);  // (a zero period traps on the EE: break 7)
        const uint32_t ammo = sub_00233e98(-1);
        if (timer_tick_i16(h16(pv + 0x4c)) != 0) {
            if (!((lw(PAD + 0x1a0) & lw(P + 0x10a0)) != 0 && lbu(P + 0x20ac) == 0 && ammo != 0)) {
                sb(m + 0x20, 2);
                hero_hold_release();
                const int32_t s = lh(pv + 0x4a);
                if (s != -1) {
                    const uint32_t r = static_cast<uint32_t>(s * 0x70) + 0x0013e550;
                    if (lwu(r + 0x88) == m && lbu(r + 0x74) != 0) sub_0028d918(s);
                }
                sh(pv + 0x4a, 0xffff);
                if (ammo == 0) moby_sound_start(0, 0, m);
            }
        }
    }
    if (const uint32_t o = lwu(pv); o != 0) {
        euler_to_mat3_vu(M(o + 0xc0), V(o + 0x40));
        mat3_mul(M(lwu(pv) + 0xc0), M(m + 0xc0), M(lwu(pv) + 0xc0));
        V(lwu(pv) + 0x10) = V(nozzle);
        moby_update_transform(lwu(pv));
    }
    pyrocitor_flames_hit(m);
    ee_stack_free(0x180);
}

// orig: (none) — registers the Pyrocitor's moby functions for the moby update calls.
void pyrocitor_fns_register() {
    ovl_register_moby_fn(0x002b77f0, pyrocitor_update);
    ovl_register_moby_fn(0x002bb400, pyrocitor_nozzle_update);
    ovl_register_moby_fn(0x0026b500, particle_0c_update);
}

}  // namespace rac1
