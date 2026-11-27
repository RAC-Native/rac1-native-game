// Umbris's own moby class 1106, the Blargian Snagglebeast (the boss in the lava arena), and its 22 private helpers —
// hand-ported from Umbris's own overlay. Header umbris_mobys.h, registered in umbris_mobys.cpp (its three draw callbacks
// 0x312420 / 0x312dc8 / 0x3131b0 and the suction vortex's draw 0x317910 live in umbris_part2). It carries 1.02 Fixes
// fix 33 (the walk flags tested with `& 4` and `& 2` in place of `== 4` and `== 2`, at L7:0x315a94 and L7:0x313b70), which
// the PAL 2.00 disc's own code runs whatever the fix's row says. Addresses are Umbris's (`orig: L7:`); Umbris-only data is
// L7_ADDR, level data and collision hits go through Stage 5's facades, and the base ELF's addresses are bare.
#include <game/collision/collision.h>
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_level_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_path_links.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>
#include <game/level/level_write.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0u;

// stop the beast's active sound slot (`pv + 0x1f8`) if it still belongs to `m` and is playing, then clear `+0x1f8` to -1
void stop_beast_sound(uint32_t m, uint32_t pv) {
    const int32_t snd = lw(pv + 0x1f8);
    if (snd != -1) {
        const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(snd * 0x70);
        if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) sub_0028d918(snd);
    }
    sw(pv + 0x1f8, 0xffffffffu);
}

// set `m` to animation `set` with random start frame `[f_lo, f_hi]` and blend `[b_lo, b_hi]` unless already in `set`
void anim_rand(uint32_t m, uint32_t set, int32_t f_lo, int32_t f_hi, int32_t b_lo, int32_t b_hi) {
    if (lbu(m + 0x53) == set) return;
    const int32_t frame = rand_range(f_lo, f_hi);
    moby_anim_set(m, set, frame, rand_range(b_lo, b_hi));
}

// pick idle animation 0 or 1 at random unless `m + 0x53` already matches the coin toss
void idle_anim_pick(uint32_t m, int32_t f_hi, int32_t b_lo, int32_t b_hi) {
    const uint32_t cur = lbu(m + 0x53);
    if (cur == (static_cast<uint32_t>(libc_rand()) & 1u)) return;
    const uint32_t next = static_cast<uint32_t>(libc_rand()) & 1u;
    const int32_t frame = rand_range(0, f_hi);
    moby_anim_set(m, next, frame, rand_range(b_lo, b_hi));
}
}  // namespace

// Spawn a stomp spark particle (kind 0x41) at `pos` with velocity `vel`.
// orig: L7:0x0029c4e8
void umb_29c4e8_l7(uint32_t pos, uint32_t vel) {
    const uint32_t p = alloc_particle(0x41);
    if (p == 0) return;
    lq_sq(p + 0x10, pos);
    const float one = 1.0f;
    const float alpha = u2f(0x3f19999au);  // (0.6)
    sw(p + 0x4, rgba_pack(one, one, one, alpha));
    const uint32_t b9 = static_cast<uint32_t>(float_to_int(4.0f)) + 0x40u;
    sb(p + 0x1, 0);
    sb(p + 0x9, b9);
    sb(p + 0x3, 0x48);
    swc1(p + 0xc, rand_range_f(20000.0f, 120000.0f));
    sb(p + 0x8, 0);
    sb(p + 0x2, lbu(lwu(L7_ADDR(0x001b2164))));
    swc1(p + 0x38, one);
    swc1(p + 0x3c, alpha);
    swc1(p + 0x30, one);
    swc1(p + 0x34, one);
    sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x50)));
    lq_sq(p + 0x20, vel);
}

// Spawn a stomp shockwave ring (class 1046 = 0x416) at `pos` for `owner`, shaking the camera by distance.
// orig: L7:0x0030dbb0
uint32_t umb_30dbb0_l7(uint32_t owner, uint32_t pos, uint32_t extra, float r0, float r1, float height, float step, float push) {
    const uint32_t s = moby_spawn(0x416);
    if (s == 0) return 0;
    sh(s + 0x32, 0xff);
    const uint32_t pv = lwu(s + 0x78);
    sb(s + 0x30, 0xff);
    sb(s + 0x31, 1);
    lq_sq(s + 0x10, pos);
    swc1(pv + 0x1c, push);
    sw(pv + 0x14, owner);
    swc1(pv + 0x4, r1);
    swc1(pv + 0x10, height);
    swc1(pv + 0xc, step);
    swc1(pv + 0x8, r0);
    sw(pv + 0x18, extra);
    swc1(pv + 0x0, r0);
    sh(s + 0x34, lhu(s + 0x34) | 0x200u);
    const float d = vec2_distance(V(s + 0x10), V(L7_ADDR(0x00166e40)));
    swc1(L7_ADDR(0x00166e60), fpu_sub(u2f(0x3e99999au), fpu_div(d, 100.0f)));  // (0.3 - d / 100)
    sw(L7_ADDR(0x00166e68), static_cast<uint32_t>(frame_scale_i(0x14)));
    moby_update_transform(s);
    moby_sound_start(0, 0, s);
    return s;
}

// Start or refresh the Snagglebeast's dynamic light region (`pv + 0x280..0x28c`, slot in `0x180340`).
// orig: L7:0x00311eb8
void umb_311eb8_l7(uint32_t m, uint32_t pv, int16_t kind, uint32_t at, int16_t dur, uint32_t c0, uint32_t c1, float size) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    lq_sq(sp, at != 0 ? at : m + 0x10);
    if (lh(pv + 0x280) == -1) {
        if (dur > 0) {
            if (size == -1.0f) size = 15.0f;
            if (c0 == 0xffffffffu) c0 = 0x80004080u;
            if (c1 == 0xffffffffu) c1 = 0;
            sh(pv + 0x280, static_cast<uint32_t>(sub_0023e5e0(sp, c0, size, 0.0f)));
            sh(pv + 0x284, 0);
        }
        if (lh(pv + 0x280) == -1) {
            ee_stack_free(0xa0);
            return;
        }
    }
    const uint32_t reg = L7_ADDR(0x00180340) + static_cast<uint32_t>(lh(pv + 0x280) << 5);
    if (lh(pv + 0x282) != kind || kind == -1 || lh(pv + 0x284) == 0) {
        if (dur != -1) {
            sh(pv + 0x286, static_cast<uint32_t>(dur));
            sh(pv + 0x284, static_cast<uint32_t>(dur));
        }
    } else if (dur != -1) {
        const int32_t knee = (7 * lh(pv + 0x286)) / 8;
        if (!(knee < lh(pv + 0x284))) sh(pv + 0x284, static_cast<uint32_t>(knee));
    }
    if (!(size == -1.0f)) swc1(reg + 0x1c, size);
    if (c0 != 0xffffffffu) {
        sw(pv + 0x288, c0);
        const float k = u2f(0x3c000000u);  // (1 / 128)
        swc1(reg + 0x0, fpu_mul(int_to_float(static_cast<int32_t>(c0 & 0xffu)), k));
        swc1(reg + 0x4, fpu_mul(int_to_float(static_cast<int32_t>((c0 >> 8) & 0xffu)), k));
        swc1(reg + 0x8, fpu_mul(int_to_float(static_cast<int32_t>((c0 >> 16) & 0xffu)), k));
    }
    if (c1 != 0xffffffffu) sw(pv + 0x28c, c1);
    if (at != 0) lq_sq(reg + 0x10, at);
    if (kind != -1) sh(pv + 0x282, static_cast<uint32_t>(kind));
    ee_stack_free(0xa0);
}

// Shake the camera according to `m`'s 3D distance (up to 80) from the camera (`0x166e40`).
// orig: L7:0x003120d8
void umb_3120d8_l7(uint32_t m) {
    float d = vec3_distance(V(m + 0x10), V(L7_ADDR(0x00166e40)));
    if (80.0f < d) d = 80.0f;
    sw(L7_ADDR(0x00166e68), 0x14);
    swc1(L7_ADDR(0x00166e60), fpu_mul(fpu_div(fpu_sub(80.0f, d), 80.0f), u2f(0x3dcccccdu)));  // (0.1)
}

// Tick and colour-fade the Snagglebeast's dynamic light region (`pv + 0x280..0x28c`), clearing it when its timer expires.
// orig: L7:0x00312150
void umb_312150_l7(uint32_t pv) {
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x284))) != 0) {
        const int32_t slot = lh(pv + 0x280);
        if (slot == -1) return;
        region_clear(static_cast<uint32_t>(slot));
        sh(pv + 0x282, 0xffffu);
        sh(pv + 0x280, 0xffffu);
        return;
    }
    if (lh(pv + 0x280) == -1) return;
    const int32_t total = lh(pv + 0x286);
    const int32_t left = lh(pv + 0x284);
    const int32_t knee = (7 * total) / 8;
    float frac;
    uint32_t c0, c1;
    if (knee < left) {
        frac = fpu_div(int_to_float(left - knee), int_to_float(lh(pv + 0x286) - knee));
        c0 = 0;
        c1 = lwu(pv + 0x288);
    } else {
        frac = fpu_div(int_to_float(left), int_to_float(knee));
        c0 = lwu(pv + 0x288);
        c1 = lwu(pv + 0x28c);
    }
    const uint32_t col = rgba_lerp(c0, c1, fpu_sub(1.0f, frac));
    const uint32_t reg = L7_ADDR(0x00180340) + static_cast<uint32_t>(lh(pv + 0x280) << 5);
    const float k = u2f(0x3c000000u);  // (1 / 128)
    swc1(reg + 0x0, fpu_mul(int_to_float(static_cast<int32_t>(col & 0xffu)), k));
    swc1(reg + 0x4, fpu_mul(int_to_float(static_cast<int32_t>((col >> 8) & 0xffu)), k));
    swc1(reg + 0x8, fpu_mul(int_to_float(static_cast<int32_t>((col >> 16) & 0xffu)), k));
}

// Spawn a falling shockwave ball (class 1049 = 0x419) at `pos` with velocity `vel`, ignoring `ignore`.
// orig: L7:0x0030e370
uint32_t umb_30e370_l7(uint32_t pos, uint32_t vel, uint32_t ignore) {
    const uint32_t s = moby_spawn(0x419);
    if (s == 0) return 0;
    sh(s + 0x32, 0xff);
    sb(s + 0x30, 0xff);
    sb(s + 0x31, 1);
    const uint32_t pv = lwu(s + 0x78);
    swc1(s + 0x40, rand_angle());
    swc1(s + 0x44, rand_angle());
    swc1(s + 0x48, rand_angle());
    lq_sq(s + 0x10, pos);
    lq_sq(pv + 0x0, vel);
    sw(pv + 0x10, ignore);
    sb(s + 0x23, 0x30);
    sh(s + 0x34, lhu(s + 0x34) | 0x200u);
    moby_update_transform(s);
    spawn_particle_1a(s, 0x800040ffu, 0x80, 0xffffffffu, 500000.0f);
    return s;
}

// Hurl alternating hand shots (joints 0 and 1) toward `target` every 9 scaled frames up to 21 shots, returning the next state.
// orig: L7:0x003122b0
uint32_t umb_3122b0_l7(uint32_t m, uint32_t pv, uint32_t target) {
    const uint32_t sp = ee_stack_alloc(0x70);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) == 0) {
        const uint32_t st = lbu(m + 0x20);
        ee_stack_free(0x70);
        return st;
    }
    moby_chain_point_v(m, lwu(pv + 0x164) & 1u, V(sp));
    const uint32_t vel = sp + 0x10;
    vec3_sub(V(vel), V(target), V(sp));
    vec2_clamp_length(V(vel), V(vel), u2f(0x412b3333u));  // (10.7)
    const float vz = fpu_add(lwc1(vel + 0x8), 0.5f);
    swc1(vel + 0x8, 0.0f < vz ? 0.0f : vz);
    vec3_set_length(V(vel), V(vel), fpu_mul(lwc1(0x0015ed6c), 9.0f));
    umb_30e370_l7(sp, vel, m);
    moby_sound_start(0, 0, m);
    umb_311eb8_l7(m, pv, 0, sp, static_cast<int16_t>(frame_scale_i(0x1e)), 0x800060ffu, 0, 10.0f);
    sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(9)));
    const int32_t count = lw(pv + 0x164) + 1;
    sw(pv + 0x164, static_cast<uint32_t>(count));
    uint32_t next;
    if (count < 21) {
        next = lbu(m + 0x20);
    } else {
        sh(pv + 0x198, static_cast<uint32_t>(frame_scale_i(0x23)));
        sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0x1e)));
        next = 1;
    }
    ee_stack_free(0x70);
    return next;
}

// Gather the beast's active wall-path indices (`+0x170..+0x184`, filtered by `+0x19c` bits 0..2) into `out`, returning the count.
// orig: L7:0x00313350
int32_t umb_313350_l7(uint32_t m, uint32_t pv, uint32_t out) {
    (void)m;
    uint32_t n = 0;
    if (lw(pv + 0x170) != -1) {
        sw(out + 0, lwu(pv + 0x170));
        n = 1;
    }
    if (lw(pv + 0x174) != -1) {
        sw(out + (n << 2), lwu(pv + 0x174));
        ++n;
    }
    if (lw(pv + 0x178) != -1) {
        sw(out + (n << 2), lwu(pv + 0x178));
        ++n;
    }
    if (((lbu(pv + 0x19c) ^ 1u) & 1u) != 0 && lw(pv + 0x17c) != -1) {
        sw(out + (n << 2), lwu(pv + 0x17c));
        ++n;
    }
    if ((lbu(pv + 0x19c) & 2u) == 0 && lw(pv + 0x180) != -1) {
        sw(out + (n << 2), lwu(pv + 0x180));
        ++n;
    }
    if ((lbu(pv + 0x19c) & 4u) == 0 && lw(pv + 0x184) != -1) {
        sw(out + (n << 2), lwu(pv + 0x184));
        ++n;
    }
    return static_cast<int32_t>(n);
}

// Hit reaction, weapon damage caps, shield flash (`+0x1c6`), sixth-of-health phase thresholds (`+0x1c5` / `+0x1c7`), and death.
// orig: L7:0x00313420
void umb_313420_l7(uint32_t m, uint32_t pv, uint32_t hp_ptr) {
    const uint32_t sp = ee_stack_alloc(0x90);
    if (lbu(m + 0x20) != 0xa) {
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
        {
            int32_t st = lw(sp);
            moby_hit_react_r(m, hit, hp_ptr, 0, st, nullptr, nullptr, 4);
            sw(sp, static_cast<uint32_t>(st));
        }
        if (hit != 0) {
            if (0.0f < lwc1(hit + 0x2c)) {
                const uint32_t cls = lhu(hit + 0x2a);
                if (cls == 0xba || cls == 0x79) swc1(hit + 0x2c, 1.0f);
                else if (cls == 0x131) swc1(hit + 0x2c, u2f(0x3e4ccccdu));  // (0.2)
            }
            const uint32_t src = lwu(hit + 0x20);
            if (src == 0 || lh(src + 0xa6) == 0x419 || lh(src + 0xa6) == 0x416) sw(sp, 1);
        }
        const int32_t hit_st = lw(sp);
        if (0.0f < lwc1(pv + 0x1c8) || lbu(pv + 0x1c5) != 0) {
            if (!(hit_st < 2)) sb(pv + 0x1c6, 0xff);
        } else if (!(hit_st < 2)) {
            const float old_hp = lwc1(hp_ptr);
            const float dmg = lwc1(hit + 0x2c);
            if (old_hp <= dmg) {
                swc1(hp_ptr, 0.0f);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                moby_drop_bolts(m, 0, -1);
                sb(pv + 0x67, 0x78);
                color_pulse_start(m, pv + 0x60);
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, rand_range(0xa, 0xd));
                if (lw(0x001413d4) == 0x78) hero_set_state(0, 1);
                if (lw(pv + 0x1f0) != -1) {
                    talker_word_set(m, 1);
                    moby_drop_bolts(m, 0, -1);
                    checkpoint_mark_collected(lbu(m + 0xb0));
                    const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0x1f0));
                    checkpoint_save(b + 0x30, b + 0x70);
                }
                sb(m + 0x20, 0xa);
            } else {
                const float d2 = fpu_add(dmg, dmg);
                const float sixth = fpu_div(static_cast<float>(lh(hp_ptr + 4)), 6.0f);
                const float t175 = fpu_mul(sixth, 1.75f);
                const float t125 = fpu_mul(sixth, 1.25f);
                swc1(pv + 0x1fc, 2.0f < d2 ? -2.0f : -d2);
                {
                    float whole = lwc1(sp + 4);
                    split_int_frac(&whole, fpu_div(old_hp, sixth));
                    swc1(sp + 4, whole);
                }
                if (fpu_sub(static_cast<float>(lh(pv + 0x24)), 1.0f) < old_hp) swc1(sp + 4, 5.0f);
                swc1(sp + 4, fpu_mul(lwc1(sp + 4), sixth));
                swc1(hp_ptr, fpu_sub(lwc1(hp_ptr), lwc1(hit + 0x2c)));
                sb(pv + 0x67, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0)));
                color_pulse_start(m, pv + 0x60);
                if (lbu(m + 0x20) < 10u && lwc1(hp_ptr) < lwc1(sp + 4)) {
                    if (lw(0x001413d4) == 0x78) hero_set_state(0, 1);
                    anim_rand(m, 2, 0, 2, 0x14, 0x16);
                    swc1(pv + 0x1c8, 0.0f);
                    sb(pv + 0x1c5, 1);
                    stop_beast_sound(m, pv);
                    sw(pv + 0x1f8, static_cast<uint32_t>(moby_sound_start(0xf, 4, m)));
                    sb(m + 0x20, 0xb);
                    sb(m + 0xbc, lbu(m + 0xbc) + 1u);
                }
                if ((t175 < old_hp && lwc1(hp_ptr) <= t175) || (t125 < old_hp && lwc1(hp_ptr) <= t125)) sb(pv + 0x1c7, 1);
            }
        }
        const uint32_t flash = lbu(pv + 0x1c6);
        sb(pv + 0x1c6, flash < 21u ? 0u : flash - 0x14u);
        sb(m + 0xa4, 0xff);
    }
    color_pulse_update(m, pv + 0x60);
    const float tilt = fpu_add(lwc1(pv + 0x1fc), u2f(0x3e99999au));  // (0.3)
    swc1(pv + 0x1fc, 0.0f < tilt ? 0.0f : tilt);
    ee_stack_free(0x90);
}

// Add uniform random xy jitter in `[-r, r]` to `pos`.
// orig: L7:0x00313920
void umb_313920_l7(uint32_t pos, float r) {
    const float lo = -r;
    const float dx = rand_range_f(lo, r);
    swc1(pos + 0, fpu_add(lwc1(pos + 0), dx));
    const float dy = rand_range_f(lo, r);
    swc1(pos + 4, fpu_add(lwc1(pos + 4), dy));
}

// Pick the next step point `out` from `m` toward `goal` around the active wall paths (`+0x188` / `+0x290`).
// orig: L7:0x00313988
void umb_313988_l7(uint32_t m, uint32_t pv, uint32_t goal, uint32_t out) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t pos = m + 0x10;
    const int32_t n = umb_313350_l7(m, pv, sp);
    if (nov_276fe8_l1(sp, n, lw(pv + 0x188), pos, goal, out, 0.0f) == 0) {
        const int32_t p0 = path_nearest_l1(goal, lvd::path(lw(pv + 0x290)), 0.0f);
        const uint32_t pt0 = lvd::path(lw(pv + 0x290)) + static_cast<uint32_t>(p0 << 4) + 0x10;
        const int32_t p1 = path_nearest_l1(pt0, lvd::path(lw(pv + 0x188)), 0.0f);
        const uint32_t pt1 = lvd::path(lw(pv + 0x188)) + static_cast<uint32_t>(p1 << 4) + 0x10;
        if (nov_276fe8_l1(sp, n, lw(pv + 0x188), pos, pt1, out, 0.0f) == 0) {
            const int32_t p2 = path_nearest_l1(pos, lvd::path(lw(pv + 0x188)), 0.0f);
            lq_sq(out, lvd::path(lw(pv + 0x188)) + static_cast<uint32_t>(p2 << 4) + 0x10);
        }
    }
    ee_stack_free(0xa0);
}

// Step `m` toward `goal` (via `step_target` when beyond `reach`, else turning toward Ratchet), pulling back onto the arena
// path `+0x290` on walk flag 2 (1.02 Fixes, fix 33) and shaking floating platforms (class 1069 = 0x42d) under foot.
// orig: L7:0x00313af0
uint32_t umb_313af0_l7(uint32_t m, uint32_t pv, uint32_t goal, uint32_t step_target, float reach) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t pos = m + 0x10;
    const float dist = vec2_distance(V(pos), V(goal));
    uint32_t flags = 4;
    const uint32_t arena = lvd::path(lw(pv + 0x290));
    if (reach < dist) {
        flags = static_cast<uint32_t>(doomlet_259028(m, pv + 0x70, step_target, pv + 0x100));
        // (1.02 Fixes, fix 33: the 1.02 build tests bit 2 with `& 2` in place of `== 2`: L7:0x314f50 on the PAL 2.00 disc)
        if (fixes102_umbris_snagglebeast_walk() ? (flags & 2u) != 0 : flags == 2u) {
            const int32_t idx = path_nearest_l1(pos, arena, 0.0f);
            vec3_sub(V(sp), V(arena + static_cast<uint32_t>(idx << 4) + 0x10), V(pos));
            vec3_clamp_length_vu(V(sp), V(sp), lwc1(pv + 0x94));
            vec3_add(V(pos), V(pos), V(sp));
        }
    } else {
        const float yaw = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(m + 0x14)));
        const float dt2 = lwc1(0x0015ed70);
        const float rate = u2f(0x40060a92u);  // (120 degrees)
        const float dt = lwc1(0x0015ed6c);
        yaw_turn_damped(m, pv + 0x168, yaw, fpu_mul(dt2, rate), fpu_mul(dt2, u2f(0x3f32b8c2u)), fpu_mul(dt, rate));
    }
    if ((lwu(L7_ADDR(0x0015f5cc)) & 3u) == 3u && walker_probe(u2f(0x3dcccccdu), 1.0f, pos, m, 0) != 0) {
        const uint32_t hm = coll_hit_moby();
        if (hm != 0 && static_cast<int32_t>(coll_hit_flags()) > 0 && lh(hm + 0xa6) == 0x42d && lbu(hm + 0xbc) == 0) {
            if (lbu(lwu(hm + 0x78) + 0xbe) != 0) {
                vec3_sub(V(sp), V(hm + 0x10), V(pos));
                sw(sp + 8, 0);
                vec3_scale(V(sp), V(sp), 0.5f);
                vec3_add(V(pos), V(pos), V(sp));
            }
            sb(coll_hit_moby() + 0xbc, 1);
        }
    }
    ee_stack_free(0x90);
    return flags;
}

// Scale the beast's walk step (`+0x94` / `+0x1bc`) and animation rate (`m + 0x58`) by distance above `near`.
// orig: L7:0x00313d30
float umb_313d30_l7(uint32_t m, uint32_t pv, float dist, float near, float anim_scale) {
    const float spd0 = lwc1(0x0015ed60);
    float step = fpu_mul(spd0, u2f(0x3d75c28fu));  // (0.06)
    if (near < dist) {
        float d = fpu_sub(dist, near);
        if (50.0f < d) d = 50.0f;
        if (d < 0.0f) d = 0.0f;
        d = fpu_div(d, 50.0f);
        const float lo = fpu_mul(spd0, u2f(0x3d75c28fu));
        const float hi = fpu_mul(spd0, u2f(0x3dcccccdu));  // (0.1)
        step = fpu_add(fpu_mul(fpu_mul(d, d), fpu_sub(hi, lo)), lo);
    }
    const float base = fpu_mul(lwc1(0x0015ed60), u2f(0x3d75c28fu));
    swc1(pv + 0x1bc, step);
    swc1(pv + 0x94, step);
    swc1(m + 0x58, fpu_mul(fpu_div(lwc1(pv + 0x80), base), anim_scale));
    return step;
}

// If `at` is further than `pv + 0x98` from `m`'s yaw, enter turn state 7 (remembering the return state/anim in `+0x1f4..0x1f6`).
// orig: L7:0x00313de8
int32_t umb_313de8_l7(uint32_t m, uint32_t pv, uint32_t at, uint32_t done_anim, uint32_t fixed_target) {
    const float heading0 = atan2_f(fpu_sub(lwc1(at), lwc1(m + 0x10)), fpu_sub(lwc1(at + 4), lwc1(m + 0x14)));
    if (!(lwc1(pv + 0x98) < angle_dist(lwc1(m + 0x48), heading0))) return 0;
    sw(pv + 0x168, 0);
    swc1(m + 0x58, 1.0f);
    sb(pv + 0x1f4, lbu(m + 0x20));
    sb(pv + 0x1f6, done_anim);
    sb(pv + 0x1f5, fixed_target);
    lq_sq(pv + 0xc0, at);
    const float heading1 = atan2_f(fpu_sub(lwc1(at), lwc1(m + 0x10)), fpu_sub(lwc1(at + 4), lwc1(m + 0x14)));
    anim_rand(m, heading1 < 0.0f ? 3u : 0xeu, 0, 1, 0xf, 0x14);
    sb(m + 0x20, 7);
    return 1;
}

// Aim head joint 0xb toward `look_at` (clamped to +-80 degrees, zeroed from state 10 up) and run its spring (`pv + 0x200`).
// orig: L7:0x00313f60
void umb_313f60_l7(uint32_t m, uint32_t pv, uint32_t look_at) {
    if (lbu(m + 0x20) < 10u) {
        const float yaw = atan2_f(fpu_sub(lwc1(look_at), lwc1(m + 0x10)), fpu_sub(lwc1(look_at + 4), lwc1(m + 0x14)));
        swc1(pv + 0x268, angle_sub(yaw, lwc1(m + 0x48)));
    } else {
        sw(pv + 0x268, 0);
    }
    const float a = lwc1(pv + 0x268);
    if (u2f(0x3fb2b8c2u) < a) swc1(pv + 0x268, u2f(0x3fb2b8c2u));
    else if (a < u2f(0xbfb2b8c2u)) swc1(pv + 0x268, u2f(0xbfb2b8c2u));
    const float dt = lwc1(0x0015ed64);
    swc1(pv + 0x264, lwc1(pv + 0x1fc));
    joint_node_spring(m, pv + 0x200, 0xb, fpu_mul(dt, u2f(0x3cf5c28fu)), fpu_mul(dt, u2f(0x3e99999au)));
}

// Apply the big-head cheat (`0x15edb0`, scale 2.75 on joint 0xb) to the cutscene actor matching `m`'s class during cutscenes.
// orig: L7:0x00314058
void umb_314058_l7(uint32_t m) {
    if (lw(L7_ADDR(0x0015f5c4)) != 2 || lbu(0x0015edb0) == 0) return;
    const uint32_t cs = L7_ADDR(0x0016c8e0);
    if (lh(L7_ADDR(0x0016c924)) <= 0) return;
    for (int32_t i = 0; i < lh(cs + 0x44); ++i) {
        const uint32_t cm = lwu(cs + 0x178 + static_cast<uint32_t>(i) * 4);
        if (lh(cm + 0xa6) != lh(m + 0xa6) || lbu(0x0015edb0) == 0) continue;
        const uint32_t node = L7_ADDR(0x0020c6c0);
        if (lbu(node + 1) != 0) continue;
        sub_0024f590(cm, 0xb, node);
        swc1(node + 0x28, 2.75f);
        swc1(node + 0x20, 2.75f);
        swc1(node + 0x24, 2.75f);
    }
}

// Mark the suction vortex state (`0x161b6c` / `0x161b70` / `0x20dbc8`) ready to re-initialise.
// orig: L7:0x00317c98
void umb_317c98_l7() {
    sw(L7_ADDR(0x00161b70), 1);
    sh(L7_ADDR(0x0020dbc8), 0);
    sw(L7_ADDR(0x00161b6c), 1);
}

// Point `out` on segment `[p0, p1]` closest to the sphere `(center, radius)` (ties broken toward `tie`), and its distance error.
// orig: L7:0x0029e6d8
float umb_29e6d8_l7(uint32_t out, uint32_t tie, uint32_t center, uint32_t p0, uint32_t p1, float radius) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t seg = sp + 0x10;
    vec3_sub(V(sp), V(p0), V(center));
    vec3_sub(V(seg), V(p1), V(p0));
    const float dd = vec3_dot(V(seg), V(seg));
    const float proj = -vec3_dot(V(sp), V(seg));
    const float t = fpu_div(proj, dd);
    const uint32_t closest = sp + 0x20;
    vec3_scale(V(closest), V(seg), t);
    vec3_add(V(closest), V(closest), V(p0));
    const float d_line = vec3_distance(V(closest), V(center));
    float err;
    if (d_line < radius) {
        const float half_chord = sqrt_f(fpu_sub(fpu_mul(radius, radius), fpu_mul(d_line, d_line)));
        const float dt = fpu_div(half_chord, vec3_length(V(seg)));
        float t0 = fpu_sub(t, dt);
        if (t0 < 0.0f) t0 = 0.0f;
        else if (1.0f < t0) t0 = 1.0f;
        float t1 = fpu_add(t, dt);
        if (t1 < 0.0f) t1 = 0.0f;
        else if (1.0f < t1) t1 = 1.0f;
        const uint32_t q0 = sp + 0x30, q1 = sp + 0x40;
        vec3_scale(V(q0), V(seg), t0);
        vec3_add(V(q0), V(q0), V(p0));
        float e0 = abs_f(fpu_sub(vec3_distance(V(q0), V(center)), radius));
        const float eps = fpu_mul(radius, u2f(0x3c23d70au));  // (0.01)
        if (e0 < eps) e0 = 0.0f;
        vec3_scale(V(q1), V(seg), t1);
        vec3_add(V(q1), V(q1), V(p0));
        float e1 = abs_f(fpu_sub(vec3_distance(V(q1), V(center)), radius));
        if (e1 < eps) e1 = 0.0f;
        bool pick0 = e0 < e1;
        if (!pick0 && e0 == e1) {
            const float d0 = vec3_distance(V(q0), V(tie));
            const float d1 = vec3_distance(V(q1), V(tie));
            pick0 = d0 < d1;
        }
        if (pick0) {
            lq_sq(out, q0);
            err = e0;
        } else {
            lq_sq(out, q1);
            err = e1;
        }
    } else {
        if (t < 0.0f) {
            lq_sq(out, p0);
            err = abs_f(fpu_sub(vec3_distance(V(out), V(center)), radius));
        } else if (1.0f < t) {
            lq_sq(out, p1);
            err = abs_f(fpu_sub(vec3_distance(V(out), V(center)), radius));
        } else {
            // (for 0 <= t <= 1 the original keeps `d_line` in fr[1] without subtracting `radius`)
            lq_sq(out, closest);
            err = d_line;
        }
        if (err < fpu_mul(radius, u2f(0x3c23d70au))) err = 0.0f;
    }
    ee_stack_free(0x110);
    return err;
}

// Initialise the suction vortex's 10 spine points (`0x20db00`), alpha ramps (`0x20dbc8`), sway (`0x161b90..0x161bb0`), and
// 200 particle slots (`0x20c840`) on the first frame of a breath.
// orig: L7:0x00317cb0
void umb_317cb0_l7(uint32_t m, uint32_t mouth) {
    if (lw(L7_ADDR(0x00161b6c)) == 0) return;
    const uint32_t sp = ee_stack_alloc(0xc0);
    sw(L7_ADDR(0x00161b6c), 0);
    vec3_set_length(V(sp), V(m + 0xc0), u2f(0x40333333u));  // (2.8)
    vec3_set_length(V(sp + 0x10), V(m + 0xe0), 1.0f);
    lq_sq(L7_ADDR(0x0020db00), mouth);
    sh(L7_ADDR(0x0020dbc8), 0);
    for (uint32_t i = 0; i < 9; ++i) {
        vec3_add(V(L7_ADDR(0x0020db10) + i * 0x10), V(L7_ADDR(0x0020db00) + i * 0x10), V(sp));
        sh(L7_ADDR(0x0020dbca) + i * 2, 0);
    }
    vec_clear(V(L7_ADDR(0x00161b90)));
    V(sp + 0x20) = Vec4{0.0f, 0.0f, u2f(0x3dcccccdu), 1.0f};
    V(sp + 0x30) = Vec4{1.0f, 0.0f, 0.0f, 1.0f};
    const float ang = rand_angle();
    vec_rotate_axis(V(L7_ADDR(0x00161ba0)), V(sp + 0x20), V(sp + 0x30), ang);
    swc1(L7_ADDR(0x00161bb0), ang);
    swc1(L7_ADDR(0x00161b7c), u2f(0x3bc49ba6u));  // (0.006)
    sw(L7_ADDR(0x00161b80), 0);
    sw(L7_ADDR(0x00161b84), 0);
    if (lw(L7_ADDR(0x00161b70)) != 0) {
        sw(L7_ADDR(0x00161b70), 0);
        for (uint32_t i = 0; i < 200; ++i) sw(L7_ADDR(0x0020dafc) - i * 0x18, 0);
    }
    ee_stack_free(0xc0);
}

// Step the suction vortex from `mouth` toward Ratchet: update its 10 spine points and 20-vertex rings, queue its draw
// (`0x317910`), spawn and advance up to 200 sucked particles, and return Ratchet's closest distance to the spine.
// orig: L7:0x00317e60
float umb_317e60_l7(uint32_t m, uint32_t mouth) {
    const uint32_t sp = ee_stack_alloc(0x2a0);
    umb_317cb0_l7(m, mouth);
    float min_dist = 1000.0f;
    lq_sq(sp + 0x0, HERO_POS);
    swc1(sp + 0x8, fpu_add(lwc1(sp + 0x8), u2f(0x3f19999au)));  // (+0.6)
    sh(L7_ADDR(0x0020dbc8), lhu(L7_ADDR(0x0020dbc8)) + 4u);
    for (uint32_t i = 0; i < 9; ++i) {
        sh(L7_ADDR(0x0020dbca) + i * 2, lhu(L7_ADDR(0x0020dbc8) + i * 2) - 0x20u);
    }
    const float seg_step = u2f(0x40333333u);  // (2.8)
    vec3_sub(V(sp + 0x10), V(HERO_POS), V(mouth));
    swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), u2f(0x3f19999au)));
    vec3_set_length(V(sp + 0x10), V(sp + 0x10), seg_step);
    vec3_cross_ba(V(sp + 0x20), V(sp + 0x10), V(m + 0xd0));
    vec3_set_length(V(sp + 0x20), V(sp + 0x20), 1.0f);
    lq_sq(L7_ADDR(0x0020db00), mouth);
    mat3_transform(V(sp + 0x30), V(L7_ADDR(0x00161b90)), M(m + 0xc0));
    vec3_add(V(sp + 0x30), V(sp + 0x30), V(sp + 0x10));
    mat3_transform(V(sp + 0x40), V(L7_ADDR(0x00161ba0)), M(m + 0xc0));
    vec3_add(V(sp + 0x40), V(sp + 0x40), V(sp + 0x10));
    {
        const float span = int_to_float(frame_scale_i(0x3c));
        const int32_t t0 = lw(L7_ADDR(0x00161b80));
        const float frac = fpu_div(static_cast<float>(t0), span);
        sw(L7_ADDR(0x00161b80), static_cast<uint32_t>(t0 + 1));
        const float x3 = lwc1(sp + 0x30), y3 = lwc1(sp + 0x34), z3 = lwc1(sp + 0x38);
        const float dx = fpu_mul(fpu_sub(lwc1(sp + 0x40), x3), frac);
        const float dz = fpu_mul(fpu_sub(lwc1(sp + 0x48), z3), frac);
        const float dy = fpu_mul(fpu_sub(lwc1(sp + 0x44), y3), frac);
        swc1(sp + 0x10, fpu_add(x3, dx));
        swc1(sp + 0x18, fpu_add(z3, dz));
        swc1(sp + 0x14, fpu_add(y3, dy));
        if (!(lw(L7_ADDR(0x00161b80)) < frame_scale_i(0x3c))) {
            lq_sq(L7_ADDR(0x00161b90), L7_ADDR(0x00161ba0));
            vec3_set_length(V(sp + 0x50), V(m + 0xc0), seg_step);
            V(sp + 0x60) = Vec4{0.0f, 0.0f, u2f(0x3dcccccdu), 1.0f};
            V(sp + 0x70) = Vec4{1.0f, 0.0f, 0.0f, 1.0f};
            const float deg = rand_range_f(45.0f, 315.0f);
            const float ang = angle_add(lwc1(L7_ADDR(0x00161bb0)), fpu_mul(deg, u2f(0x3c8efa35u)));
            vec_rotate_axis(V(L7_ADDR(0x00161ba0)), V(sp + 0x60), V(sp + 0x70), ang);
            swc1(L7_ADDR(0x00161bb0), ang);
            sw(L7_ADDR(0x00161b80), 0);
        }
    }
    swc1(L7_ADDR(0x00161b84), angle_add(lwc1(L7_ADDR(0x00161b84)), u2f(0x3dcccccdu)));
    vec3_set_length(V(sp + 0x50), V(0x0013f5e0), -1.0f);
    lq_sq(sp + 0xa0, sp + 0x50);
    vec3_cross_ba(V(sp + 0x90), V(sp + 0x10), V(sp + 0xa0));
    vec3_set_length(V(sp + 0x90), V(sp + 0x90), 1.0f);
    vec3_cross_ba(V(sp + 0x80), V(sp + 0xa0), V(sp + 0x90));
    for (uint32_t j = 0; j < 20; ++j) {
        vec3_scale(V(sp + 0x60), V(L7_ADDR(0x0020c700) + j * 0x10), u2f(0x3ca3d70au));  // (0.02)
        mat3_transform(V(sp + 0x60), V(sp + 0x60), M(sp + 0x80));
        vec3_add(V(L7_ADDR(0x0020dbe0) + j * 0x10), V(L7_ADDR(0x0020db00)), V(sp + 0x60));
    }
    for (int32_t ring = 1; ring < 10; ++ring) {
        const uint32_t off = static_cast<uint32_t>(ring) << 4;
        const uint32_t cur_pt = L7_ADDR(0x0020db00) + off;
        const uint32_t prev_pt = L7_ADDR(0x0020daf0) + off;
        vec3_sub(V(sp + 0x60), V(cur_pt), V(prev_pt));
        lq_sq(sp + 0x70, sp + 0x10);
        vec3_cross_ba(V(sp + 0xb0), V(sp + 0x70), V(sp + 0x60));
        const float dot = vec3_dot(V(sp + 0x60), V(sp + 0x70));
        const float denom = fpu_mul(vec3_length(V(sp + 0x60)), seg_step);
        const float target_ang = fpu_sub(u2f(0x3fc90fdbu), asin_f(fpu_div(dot, denom)));
        float spring_ang = camera_angle_spring(0.0f, target_ang, u2f(0x3d75c28fu), u2f(0x3e4ccccdu), 0.0f,
                                               L7_ADDR(0x0020dba0) + static_cast<uint32_t>(ring << 2));
        const float diff = angle_sub(target_ang, spring_ang);
        if (u2f(0x3ea0d97cu) < abs_f(diff)) spring_ang = angle_add(spring_ang, angle_sub(diff, u2f(0x3ea0d97cu)));
        vec_rotate_axis(V(sp + 0xc0), V(sp + 0x60), V(sp + 0xb0), spring_ang);
        vec3_set_length(V(sp + 0xd0), V(sp + 0xc0), seg_step);
        vec3_lerp(V(sp + 0xc0), V(sp + 0xc0), V(sp + 0xd0), 1.0f);
        const float len = vec3_length(V(sp + 0xc0));
        if (u2f(0x4079999au) < len) vec3_scale(V(sp + 0xc0), V(sp + 0xc0), fpu_div(u2f(0x4079999au), len));  // (3.9)
        const float rf = static_cast<float>(ring);
        if (!(ring < 6)) {
            vec3_lerp(V(sp + 0xc0), V(sp + 0xc0), V(sp + 0x60), fpu_add(fpu_div(rf, int_to_float(10)), 0.0f));
        }
        vec3_add(V(cur_pt), V(prev_pt), V(sp + 0xc0));
        const float d = umb_29e6d8_l7(sp + 0xe0, sp, sp, prev_pt, cur_pt, 0.0f);
        if (d < min_dist) min_dist = d;
        lq_sq(sp + 0xe0, sp + 0x10);
        if (u2f(0x40066666u) < abs_f(lwc1(sp + 0x18))) {  // (2.1)
            lq_sq(sp + 0x110, sp + 0x50);
            vec3_cross_ba(V(sp + 0x120), V(sp + 0x110), V(sp + 0x10));
            vec3_sub(V(sp + 0x140), V(L7_ADDR(0x0020dae0) + off), V(prev_pt));
            vec3_cross_ba(V(sp + 0x130), V(sp + 0x110), V(sp + 0x140));
            if (0.0f < vec3_dot(V(sp + 0x120), V(sp + 0x130))) vec3_scale(V(sp + 0x110), V(sp + 0x110), -1.0f);
        } else {
            vec3_set_length(V(sp + 0x110), V(0x0013f5e0), -1.0f);
        }
        vec3_set_length(V(sp + 0xf0), V(sp + 0xe0), 1.0f);
        vec3_cross_ba(V(sp + 0x100), V(sp + 0xf0), V(sp + 0x110));
        vec3_set_length(V(sp + 0x100), V(sp + 0x100), 1.0f);
        vec3_cross_ba(V(sp + 0x110), V(sp + 0x100), V(sp + 0xf0));
        lq_sq(sp + 0x50, sp + 0x110);
        const float r_base = fpu_mul(rf, u2f(0x3e4ccccdu));  // (0.2)
        const float r_step = fpu_mul(rf, u2f(0x3f333333u));  // (0.7)
        const uint32_t ring_v = L7_ADDR(0x0020dbe0) + static_cast<uint32_t>(ring * 0x140);
        for (uint32_t j = 0; j < 20; ++j) {
            const float rad0 = fpu_add(r_base, u2f(0x3ca3d70au));  // (+0.02)
            // (the delay slot of the loop back-branch reloads `0x161b84` each vertex rather than accumulating)
            const float s = sin_vu(angle_add(fpu_add(lwc1(L7_ADDR(0x00161b84)), r_step), 0.0f));
            const float scale = fpu_add(rad0, fpu_mul(fpu_mul(rad0, 0.25f), s));
            vec3_scale(V(sp + 0x120), V(L7_ADDR(0x0020c700) + j * 0x10), scale);
            mat3_transform(V(sp + 0x120), V(sp + 0x120), M(sp + 0xf0));
            vec3_add(V(ring_v + j * 0x10), V(cur_pt), V(sp + 0x120));
        }
        vec3_set_length(V(sp + 0x10), V(sp + 0xc0), seg_step);
    }
    sub_001f93e0(L7_ADDR(0x00317910), m);
    int32_t spawned_main = 0, spawned_aux = 0;
    for (int32_t idx = 0; idx < 200; ++idx) {
        const uint32_t off = static_cast<uint32_t>(idx * 0x18);
        const uint32_t rec = L7_ADDR(0x0020c840) + off;
        const uint32_t dist_p = L7_ADDR(0x0020c844) + off;
        const uint32_t spd_p = L7_ADDR(0x0020c850) + off;
        const uint32_t part_p = L7_ADDR(0x0020c854) + off;
        if (lwu(part_p) == 0 && spawned_main < 5) {
            int32_t seg = 9;
            if (lh(L7_ADDR(0x0020dbda)) < 64) {
                seg = 8;
                uint32_t a = L7_ADDR(0x0020dbda);
                while (seg >= 0) {
                    a -= 2;
                    if (!(lh(a) < 64)) break;
                    --seg;
                }
            }
            if (seg > 0) {
                if (!(seg < 2) && spawned_aux < 2) {
                    const int32_t vtx = rand_mod(0x14);
                    const float t = rand_range_f(u2f(0x3e4ccccdu), 1.0f);
                    const uint32_t v0_addr = L7_ADDR(0x0020dbe0) + static_cast<uint32_t>(vtx << 4);
                    vec3_lerp(V(sp + 0x60), V(v0_addr), V(v0_addr + static_cast<uint32_t>(seg * 0x140)), t);
                    vec3_lerp(V(sp + 0xb0), V(L7_ADDR(0x0020db00)), V(L7_ADDR(0x0020db00) + static_cast<uint32_t>(seg << 4)), t);
                    const float sz = rand_range_f(60000.0f, 180000.0f);
                    const int32_t spin = rand_mod(5) + 1;
                    const uint32_t pa = spawn_particle_17(sp + 0x60, spin, sp + 0x70, 0x00404040u, 0.0f, 1.0f, u2f(0x3f83d70au), sz);
                    if (pa != 0) {
                        vec3_sub(V(sp + 0xc0), V(sp + 0x60), V(sp + 0xb0));
                        ++spawned_aux;
                        vec3_set_length(V(pa + 0x30), V(sp + 0xc0), u2f(0x3c23d70au));  // (0.01)
                        swc1(pa + 0x38, fpu_add(lwc1(pa + 0x38), u2f(0x3ca3d70au)));     // (+0.02)
                    }
                }
                lq_sq(sp + 0x60, L7_ADDR(0x0020db00) + static_cast<uint32_t>(seg << 4));
                vec_clear(V(sp + 0x70));
                const float sz = rand_range_f(60000.0f, 180000.0f);
                const uint32_t pm = spawn_particle_17(sp + 0x60, 6, sp + 0x70, 0x7f404040u, 0.0f, 1.0f, 1.0f, sz);
                sw(part_p, pm);
                if (pm != 0) {
                    sh(pm + 0xa, 0xc8);
                    sw(pm + 0x24, 2);
                    ++spawned_main;
                    sb(pm + 0x2a, 0x7f);
                    sb(pm + 0x2b, 0xc8);
                }
                sh(rec + 0x0, static_cast<uint32_t>(seg));
                sw(dist_p, 0);
                sw(rec + 0x8, 0);
                sh(rec + 0x2, static_cast<uint32_t>(rand_mod(0x14)));
                swc1(rec + 0xc, rand_range_f(u2f(0x3dcccccdu), u2f(0x3f4ccccdu)));  // (0.1 .. 0.8)
                swc1(spd_p, rand_range_f(u2f(0x3dcccccdu), u2f(0x3ecccccdu)));      // (0.1 .. 0.4)
            }
        }
        if (lwu(part_p) == 0) continue;
        const uint32_t seg0 = static_cast<uint32_t>(lh(rec + 0x0)) << 4;
        vec3_sub(V(sp + 0x60), V(L7_ADDR(0x0020daf0) + seg0), V(L7_ADDR(0x0020db00) + seg0));
        float seg_len = vec3_length(V(sp + 0x60));
        swc1(dist_p, fpu_add(lwc1(dist_p), lwc1(spd_p)));
        sh(lwu(part_p) + 0xa, 0xc8);
        if (seg_len < lwc1(dist_p)) {
            const int16_t next_seg = static_cast<int16_t>(lhu(rec + 0x0) - 1u);
            sh(rec + 0x0, static_cast<uint16_t>(next_seg));
            if (next_seg > 0) {
                swc1(dist_p, fpu_sub(lwc1(dist_p), seg_len));
                const uint32_t s_off = static_cast<uint32_t>(lh(rec + 0x0)) << 4;
                vec3_sub(V(sp + 0x60), V(L7_ADDR(0x0020daf0) + s_off), V(L7_ADDR(0x0020db00) + s_off));
                seg_len = vec3_length(V(sp + 0x60));
            } else {
                free_particle(lwu(part_p));
                sw(part_p, 0);
            }
        }
        const uint32_t p = lwu(part_p);
        if (p == 0) continue;
        const float dist_along = lwc1(dist_p);
        const int32_t seg = lh(rec + 0x0);
        const float frac = fpu_div(dist_along, seg_len);
        if (seg == 2) {
            sh(p + 0xa, 0x64);
        } else if (seg < 2) {
            sh(p + 0xa, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(1.0f, frac), 100.0f))));
            if (lh(p + 0xa) < 61) sh(p + 0xa, 0x3c);
        }
        if (lh(rec + 0x0) == 1) {
            vec3_sub(V(sp + 0x70), V(mouth), V(p + 0x10));
            vec3_set_length(V(sp + 0x70), V(sp + 0x70), lwc1(spd_p));
            vec3_add(V(p + 0x10), V(p + 0x10), V(sp + 0x70));
            const float damp = fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbd4cccd0u)), 1.0f);
            swc1(p + 0xc, fpu_mul(lwc1(p + 0xc), damp));
        } else {
            vec3_scale(V(sp + 0x60), V(sp + 0x60), frac);
            const int32_t s = lh(rec + 0x0);
            const uint32_t s_off = static_cast<uint32_t>(s) << 4;
            const uint32_t r_off = static_cast<uint32_t>(s * 0x140);
            vec3_add(V(p + 0x10), V(L7_ADDR(0x0020db00) + s_off), V(sp + 0x60));
            const int32_t v0 = lh(rec + 0x2);
            const uint32_t v0_off = static_cast<uint32_t>(v0) << 4;
            vec3_sub(V(sp + 0x70), V(L7_ADDR(0x0020dbe0) + r_off + v0_off), V(L7_ADDR(0x0020db00) + s_off));
            vec3_sub(V(sp + 0xb0), V(L7_ADDR(0x0020daa0) + r_off + v0_off), V(L7_ADDR(0x0020daf0) + s_off));
            vec4_lerp(sp + 0xc0, sp + 0x70, sp + 0xb0, frac);
            const int32_t v_prev = ee_rem(v0 + 0x13, 0x14);
            const uint32_t vp_off = static_cast<uint32_t>(v_prev) << 4;
            vec3_sub(V(sp + 0xd0), V(L7_ADDR(0x0020dbe0) + r_off + vp_off), V(L7_ADDR(0x0020db00) + s_off));
            vec3_sub(V(sp + 0xe0), V(L7_ADDR(0x0020daa0) + r_off + vp_off), V(L7_ADDR(0x0020daf0) + s_off));
            vec4_lerp(sp + 0xf0, sp + 0xd0, sp + 0xe0, frac);
            vec4_lerp(sp + 0xc0, sp + 0xc0, sp + 0xf0, lwc1(rec + 0x8));
            const float vf = fpu_add(lwc1(rec + 0x8), lwc1(rec + 0xc));
            if (1.0f <= vf) {
                swc1(rec + 0x8, fpu_sub(vf, 1.0f));
                sh(rec + 0x2, static_cast<uint32_t>(v_prev));
            } else {
                swc1(rec + 0x8, vf);
            }
            vec3_add(V(p + 0x10), V(p + 0x10), V(sp + 0xc0));
        }
    }
    ee_stack_free(0x2a0);
    return min_dist;
}

// Fade out the suction vortex's 10 alpha ramps (`0x20dbc8..0x20dbda`) and queue its draw while positive.
// orig: L7:0x00318e30
void umb_318e30_l7(uint32_t m) {
    if (lh(L7_ADDR(0x0020dbc8)) <= 0) return;
    for (uint32_t i = 0; i < 10; ++i) {
        const uint32_t at = L7_ADDR(0x0020dbc8) + i * 2;
        if (!(lh(at) < 65)) sh(at, 0x40);
        sh(at, lhu(at) - 4u);
    }
    sub_001f93e0(L7_ADDR(0x00317910), m);
}

// End the suction breath: release any active vortex particles (`0x20c840`) with outward/forward drift and fade the vortex.
// orig: L7:0x00318e98
void umb_318e98_l7(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xe0);
    const float k = fpu_mul(lwc1(L7_ADDR(0x00161b7c)), 0.5f);
    sw(L7_ADDR(0x00161b70), 1);
    sw(L7_ADDR(0x00161b6c), 1);
    swc1(L7_ADDR(0x00161b7c), k);
    moby_sound_start(0xa, 0, m);
    for (uint32_t off = 0; off < 200u * 0x18u; off += 0x18u) {
        const uint32_t rec = L7_ADDR(0x0020c840) + off;
        const uint32_t p = lwu(rec + 0x14);
        if (p == 0) continue;
        sw(rec + 0x14, 0);
        const uint32_t s0 = static_cast<uint32_t>(lh(rec + 0x0)) << 4;
        vec3_sub(V(sp), V(L7_ADDR(0x0020daf0) + s0), V(L7_ADDR(0x0020db00) + s0));
        const float len = vec3_length(V(sp));
        vec3_scale(V(sp), V(sp), fpu_div(lwc1(rec + 0x4), len));
        const uint32_t s1 = static_cast<uint32_t>(lh(rec + 0x0)) << 4;
        vec3_add(V(sp + 0x10), V(L7_ADDR(0x0020db00) + s1), V(sp));
        vec3_sub(V(sp + 0x20), V(p + 0x10), V(sp + 0x10));
        vec3_set_length(V(sp + 0x20), V(sp + 0x20), u2f(0x3cf5c28fu));  // (0.03)
        vec3_scale(V(sp), V(sp), fpu_div(u2f(0x3dcccccdu), len));       // (0.1)
        vec3_add(V(p + 0x30), V(sp + 0x20), V(sp));
        sh(p + 0xa, 0x46);
    }
    umb_318e30_l7(m);
    ee_stack_free(0xe0);
}

// Class 1106, the Blargian Snagglebeast. States (`m + 0x20`, jump table 0x211940): 0 set up (paths' segment lengths, wall
// links, scale x1.75); 1 idle, picking attacks by timers and sub-phase (`m + 0xbc`, jump table 0x211910); 2 hurl shockwave
// balls; 3 triple ground stomp (spawning class 1046 rings and 0x41 sparks); 4 ground rush; 5 / 6 walk / chase around the
// lava walls (with 1.02 Fixes fix 33 at 0x315a94); 7 turn in place toward `+0xc0` and return to `+0x1f4`; 8 suction vortex
// (pulling Ratchet into state 0x78 and spitting ammo/health pickups along `+0x18c`); 9 pillar charge (6 sub-steps in `+0x164`,
// jump table 0x211990); 10 death explosion -> 16; 11 flinch recovery -> pillar charge 9 or idle 1; 12..15 platform-sink
// sequence in the lava (12 tell next platform, 13 sink/flail, 14 leap/stomp damage tick, 15 climb back onto the arena -> 6);
// 16 raise lava (`+0x1d0`) to 34.5 and retire; 17 idle tail.
// orig: L7:0x00314150
void umb_class1106_update_l7(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x1f0);
    const uint32_t pv = lwu(m + 0x78);
    joint_node_scale(m, 0xb, pv + 0x2a0, 2.5f);
    umb_314058_l7(m);
    if (lw(L7_ADDR(0x0015f5c4)) == 2) {
        ee_stack_free(0x1f0);
        return;
    }
    if (lw(pv + 0x1a4) != -1 && point_in_box(HERO_POS, lw(pv + 0x1a4)) == 0) {
        ee_stack_free(0x1f0);
        return;
    }
    const uint32_t pos = m + 0x10;
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L7_ADDR(0x00166e40))) < 35.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x1c);
    }
    lq_sq(sp + 0x20, pos);
    const uint32_t hp_ptr = pv + 0x20;
    umb_313420_l7(m, pv, hp_ptr);
    bool want_walk = false;
    const uint32_t tgt = sp + 0x30;
    target_select_any(m, tgt, 30.0f);
    const float tgt_dist = vec2_distance(V(tgt), V(pos));
    const float tgt_yaw = atan2_f(fpu_sub(lwc1(tgt), lwc1(m + 0x10)), fpu_sub(lwc1(tgt + 4), lwc1(m + 0x14)));
    const float hero_dist = vec2_distance(V(pos), V(HERO_POS));
    const float hero_yaw = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(m + 0x14)));
    if (countdown_u8(pv + 0x1f7) != 0) {
        if (8.0f < vec2_distance(V(pos), V(HERO_POS)) && lbu(m + 0x20) != 5) {
            want_walk = true;
        } else if (6.0f < vec2_distance(V(pos), V(HERO_POS)) && (lbu(m + 0x20) == 5 || (lbu(m + 0xbc) % 3u) == 1u)) {
            want_walk = true;
        }
    }
    bool want_stomp = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x194))) > 0;
    bool want_charge = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x19a))) != 0 && 9.0f < tgt_dist;
    bool want_suck = false;
    if (lbu(pv + 0x1c7) != 0 && lwu(sp + 0x70) == lwu(0x001413d0) && tgt_dist < 30.0f) {
        if (abs_f(angle_sub(tgt_yaw, lwc1(m + 0x48))) < u2f(0x3fb2b8c2u)) want_suck = true;  // (80 degrees)
    }
    bool want_balls = false;
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x198))) != 0) {
        const float lim = (lbu(m + 0x20) == 5 || (lbu(m + 0xbc) % 3u) == 1u) ? 14.0f : 10.0f;
        if (tgt_dist < lim) want_balls = true;
    }
    bool to_waypoint = false;
    bool hero_on_pillar = false;
    if (fpu_add(lwc1(m + 0x18), 1.0f) < lwc1(0x0013f628)) {
        const uint32_t ground_m = lwu(0x0013f64c);
        if (ground_m != 0) {
            const int32_t uid = lh(ground_m + 0xa6);
            hero_on_pillar = uid >= 1113 && uid < 1117;
        }
    }
    switch (lbu(m + 0xbc)) {  // (jump table 0x211910: sub-phases 0 .. 8)
        case 0:  // (0x00314560)
            want_walk = false;
            want_suck = false;
            break;
        case 3:  // (0x0031453c)
            if (lw(pv + 0x1a8) == -1) {
                want_walk = false;
            } else {
                want_walk = true;
                to_waypoint = true;
                want_stomp = false;
                want_balls = false;
            }
            want_suck = false;
            break;
        case 6:  // (0x0031456c)
            if (lw(pv + 0x1a8) == -1) {
                want_walk = false;
            } else {
                want_walk = true;
                to_waypoint = true;
                want_stomp = false;
                want_balls = false;
            }
            break;
        case 1:
        case 4:
        case 7:  // (0x00314598)
            want_stomp = false;
            want_suck = false;
            if (lbu(m + 0x20) != 1) want_charge = false;
            break;
        case 2:
        case 5:
        case 8:  // (0x003145b0)
            want_balls = false;
            want_stomp = false;
            want_suck = false;
            want_charge = false;
            break;
        default:
            break;
    }
    {
        const uint32_t bar = static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(pv + 0x20), 16.0f))) & 0xfff0u;
        sw(pv + 0x1b4, bar);
        const int32_t full = float_to_int(fpu_mul(fpu_sub(static_cast<float>(lh(pv + 0x24)), 0.5f), 16.0f));
        if (!(static_cast<int32_t>(bar) < full)) sw(pv + 0x1b4, bar | 0xfu);
        if ((lbu(m + 0xbc) % 3u) == 0u) {
            const float hp = lwc1(pv + 0x20);
            if (0.5f < hp) {
                const int32_t hp_i = float_to_int(hp);
                const int32_t seg = ee_div(lh(pv + 0x24), 6);
                const int32_t rem = ee_rem(hp_i, seg);
                int32_t sub = ee_div(rem << 4, seg);
                if (!(-1 < sub)) sub = 0;
                sw(pv + 0x1b4, lwu(pv + 0x1b4) + (static_cast<uint32_t>(sub) & 0xfu));
            } else {
                sw(pv + 0x1b4, 0);
            }
        }
        if ((lbu(m + 0xbc) % 3u) == 2u) sw(pv + 0x1b4, lwu(pv + 0x1b4) + 0xfu);
    }
    if (lbu(m + 0x20) != 8 && lbu(m + 0x20) != 0) umb_318e30_l7(m);
    if (lbu(pv + 0x1c5) == 1) {
        if (lw(pv + 0x1f8) == -1) sw(pv + 0x1f8, static_cast<uint32_t>(moby_sound_start(0xf, 4, m)));
        const float g = fpu_add(lwc1(pv + 0x1c8), fpu_mul(lwc1(0x0015ed60), u2f(0x3c23d70au)));  // (0.01)
        swc1(pv + 0x1c8, g);
        if (1.0f <= g) sb(pv + 0x1c5, 0);
        if (1.0f < lwc1(pv + 0x1c8)) swc1(pv + 0x1c8, 1.0f);
    }
    if (lbu(pv + 0x1c5) == 2) {
        if (lw(pv + 0x1f8) == -1) sw(pv + 0x1f8, static_cast<uint32_t>(moby_sound_start(0xf, 4, m)));
        const float g = fpu_sub(lwc1(pv + 0x1c8), fpu_mul(lwc1(0x0015ed60), u2f(0x3c23d70au)));
        swc1(pv + 0x1c8, g);
        if (g <= 0.0f) {
            stop_beast_sound(m, pv);
            sb(pv + 0x1c5, 0);
        }
        if (lwc1(pv + 0x1c8) < 0.0f) swc1(pv + 0x1c8, 0.0f);
    }
    if (0.0f < lwc1(pv + 0x1c8)) sub_001f93e0(L7_ADDR(0x00312dc8), m);
    umb_313f60_l7(m, pv, HERO_POS);
    {
        const uint32_t st = lbu(m + 0x20);
        if (st != 0 && st != 0xa && st != 0x10) {
            sw(pv + 0x1b0, static_cast<uint32_t>(camera_request(0x16, 0xffff, L7_ADDR(0x0025dbe8), L7_ADDR(0x0025dd08),
                                                                L7_ADDR(0x00262078), pv + 0x1b4,
                                                                static_cast<uint32_t>(lh(pv + 0x24) << 4) | 0xfu)));
        }
    }

    auto turn_damped_std = [&](float target_yaw) {
        const float dt2 = lwc1(0x0015ed70);
        const float rate = u2f(0x40060a92u);  // (120 degrees)
        const float dt = lwc1(0x0015ed6c);
        yaw_turn_damped(m, pv + 0x168, target_yaw, fpu_mul(dt2, rate), fpu_mul(dt2, u2f(0x3f32b8c2u)), fpu_mul(dt, rate));
    };

    switch (lbu(m + 0x20)) {  // (jump table 0x211940: states 0 .. 17)
        case 0: {  // (0x003148f4)
            sw(L7_ADDR(0x00161b64), 0);
            sb(m + 0x20, 1);
            sb(m + 0xbc, 0);
            sh(pv + 0x280, 0xffffu);
            sw(pv + 0x190, 0);
            if (lw(pv + 0x18c) != -1) {
                const uint32_t p = lvd::path(lw(pv + 0x18c));
                const int32_t n = lw(p);
                for (int32_t i = 0; i < n; ++i) {
                    const uint32_t cur = p + static_cast<uint32_t>(i << 4);
                    const uint32_t nxt = p + static_cast<uint32_t>(ee_rem(i + 1, n) << 4);
                    lvd::set_path_f32(cur + 0x1c, vec3_distance(V(cur + 0x10), V(nxt + 0x10)));
                    const uint32_t p_again = lvd::path(lw(pv + 0x18c)) + static_cast<uint32_t>(i << 4);
                    swc1(pv + 0x190, fpu_add(lwc1(pv + 0x190), lwc1(p_again + 0x1c)));
                }
            }
            if (lw(pv + 0x290) != -1) {
                const uint32_t p = lvd::path(lw(pv + 0x290));
                for (int32_t i = 0; i < lw(p); ++i) {
                    const int32_t n = lw(p);
                    const uint32_t cur = p + static_cast<uint32_t>(i << 4);
                    const uint32_t nxt = p + static_cast<uint32_t>(ee_rem(i + 1, n) << 4);
                    lvd::set_path_f32(cur + 0x1c, vec3_distance(V(cur + 0x10), V(nxt + 0x10)));
                }
            }
            if (lw(pv + 0x188) != -1) {
                const uint32_t p0 = lvd::path(lw(pv + 0x188));
                if (lw(p0) > 0) {
                    for (int32_t i = 0;; ++i) {
                        const uint32_t p = lvd::path(lw(pv + 0x188));
                        const uint32_t slot = static_cast<uint32_t>(float_to_int(lwc1(p + static_cast<uint32_t>(i << 4) + 0x1c)));
                        if (slot < 4u) sb(pv + 0x1d4 + slot, static_cast<uint32_t>(i));
                        if (!(i + 1 < lw(lvd::path(lw(pv + 0x188))))) break;
                    }
                }
            }
            sw(pv + 0xa8, lwu(pv + 0xa8) & ~8u);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
            path_links_build(pv + 0x170, 3, lwu(pv + 0x188));
            sb(pv + 0x19c, 7);
            sb(m + 0x30, 0xff);
            sh(m + 0x32, 0xff);
            sw(pv + 0x1b0, 0xffffffffu);
            sw(pv + 0x1a8, 0xffffffffu);
            sw(pv + 0x1c0, static_cast<uint32_t>(path_nearest_l1(pos, lvd::path(lw(pv + 0x188)), 0.0f)));
            umb_317c98_l7();
            swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), 1.75f));
            sb(pv + 0x1c7, 0);
            swc1(pv + 0x98, u2f(0x3fdf66f3u));  // (100 degrees)
            sb(pv + 0x29, 1);
            sb(pv + 0x58, static_cast<uint32_t>(float_to_int(30.0f)));
            sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(40.0f)));
            break;
        }
        case 1: {  // (0x00314ba8)
            if ((lbu(m + 0x70) & 2u) != 0) idle_anim_pick(m, 0xf, 0x1e, 0x2a);
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) != 0) {
                if (want_charge) {
                    sw(pv + 0x164, 0);
                    sw(pv + 0x160, 0);
                    anim_rand(m, 5, 0, 2, 0x12, 0x14);
                    sb(m + 0x20, 4);
                }
                if (want_stomp && (lbu(m + 0x20) != 5 || rand_mod(4) == 0)) {
                    sw(pv + 0x164, 0);
                    anim_rand(m, 4, 0, 9, 0x14, 0x28);
                    sb(m + 0x20, 3);
                    swc1(m + 0x58, 1.0f);
                }
                if (want_suck) {
                    anim_rand(m, 6, 0, 2, 0xf, 0x19);
                    sw(pv + 0x164, 0);
                    sw(pv + 0x16c, 0);
                    sb(pv + 0x1c4, 0);
                    sb(m + 0x20, 8);
                    sw(pv + 0x294, 0);
                    sw(pv + 0x1f8, static_cast<uint32_t>(moby_sound_start(9, 4, m)));
                }
                if (want_balls) {
                    anim_rand(m, 9, 0, 2, 0x11, 0x19);
                    sb(m + 0x20, 2);
                }
            }
            if (want_walk && lbu(m + 0x20) != 8) {
                anim_rand(m, 0xa, 0, 4, 0x14, 0x1e);
                swc1(m + 0x58, 1.5f);
                sw(pv + 0x298, 0);
                sb(m + 0x20, 5);
            }
            break;
        }
        case 2: {  // (0x00314e04)
            const float old_yaw = lwc1(m + 0x48);
            if (ee_rem(lw(L7_ADDR(0x0015f5cc)), 6) == 0) chain_spark_spawn(m, 0, 600000.0f, 5.0f);
            if (ee_rem(lw(L7_ADDR(0x0015f5cc)) - 2, 6) == 0) chain_spark_spawn(m, 1, 600000.0f, 5.0f);
            if (umb_313de8_l7(m, pv, HERO_POS, 9, 0) != 0) break;
            turn_damped_std(tgt_yaw);
            const float turned = angle_dist(lwc1(m + 0x48), old_yaw);
            const uint32_t set = fpu_mul(lwc1(0x0015ed6c), u2f(0x3e32b8c2u)) < turned ? 0xdu : 9u;  // (10 degrees/s)
            anim_rand(m, set, 0, 1, 0xf, 0x16);
            const uint32_t prev_set = lbu(m + 0x52);
            if (prev_set == 9 || prev_set == 0xd) sb(m + 0x20, umb_3122b0_l7(m, pv, tgt));
            const uint32_t sub = lbu(m + 0xbc) % 3u;
            if ((sub == 0u && 13.0f < vec2_distance(V(tgt), V(pos))) || (sub == 1u && 20.0f < vec2_distance(V(tgt), V(pos)))) {
                sh(pv + 0x198, static_cast<uint32_t>(frame_scale_i(0x23)));
                sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0x1e)));
                sb(m + 0x20, 1);
            }
            break;
        }
        case 3: {  // (0x00315088)
            {
                const float dt2 = lwc1(0x0015ed70);
                const float dt = lwc1(0x0015ed6c);
                yaw_turn_damped(m, pv + 0x168, tgt_yaw, fpu_mul(dt2, u2f(0x3fc90fdbu)), fpu_mul(dt2, u2f(0x3f32b8c2u)),
                                fpu_mul(dt, u2f(0x3f860a92u)));
            }
            if (anim_crossed(m, 31.0f) != 0 || anim_crossed(m, 67.0f) != 0 || anim_crossed(m, 101.0f) != 0) {
                sw(pv + 0x164, lwu(pv + 0x164) + 1u);
                const uint32_t foot = sp + 0x80;
                lq_sq(foot, pos);
                sw(pv + 0x168, 0);
                umb_30dbb0_l7(m, foot, 0x3a, 1.0f, 25.0f, 2.0f, fpu_mul(lwc1(0x0015ed6c), 8.0f), 1.0f);
                moby_chain_point_v(m, (lwu(pv + 0x164) & 1u) + 5u, V(foot));
                umb_311eb8_l7(m, pv, 1, foot, static_cast<int16_t>(frame_scale_i(0x1e)), 0x800060ffu, 0, 10.0f);
                swc1(foot + 8, lwc1(m + 0x18));
                for (int32_t k = 0; k < 0x37; ++k) {
                    const float az = rand_angle();
                    const float el = rand_range_f(u2f(0x3ec90fdbu), u2f(0x3f490fdbu));  // (pi/8 .. pi/4)
                    vec_from_spherical_v(V(sp + 0x90), fpu_mul(lwc1(0x0015ed6c), 5.0f), az, el);
                    vec3_set_length(V(sp + 0xa0), V(sp + 0x90), 0.75f);
                    swc1(sp + 0xa8, fpu_add(lwc1(sp + 0xa8), u2f(0x3dcccccdu)));
                    vec3_add(V(sp + 0xa0), V(sp + 0xa0), V(foot));
                    sw(sp + 0x9c, 0);
                    umb_29c4e8_l7(sp + 0xa0, sp + 0x90);
                }
                umb_3120d8_l7(m);
            }
            if (!(lw(pv + 0x164) < 3) && anim_crossed(m, 51.0f) != 0) {
                if (want_charge) {
                    sh(pv + 0x194, 0xc0);
                    sw(pv + 0x160, 0xaa);
                } else {
                    sh(pv + 0x194, static_cast<uint32_t>(frame_scale_i(0x3c)));
                    sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0x37)));
                    const int32_t cur_t = lh(pv + 0x19a);
                    if (!(frame_scale_i(0x37) < cur_t)) {
                        sh(pv + 0x19a, lhu(pv + 0x19a) + static_cast<uint32_t>(frame_scale_i(0x3c)));
                    }
                }
                swc1(m + 0x58, 1.0f);
                idle_anim_pick(m, 0xf, 0x1e, 0x2a);
                sb(m + 0x20, 1);
            }
            break;
        }
        case 4: {  // (0x0031537c)
            if (umb_313de8_l7(m, pv, HERO_POS, 5, 0) != 0) break;
            if (lw(pv + 0x160) < frame_scale_i(0x3c) / 2) {
                moby_chain_point_v(m, 2, V(sp + 0x80));
                umb_311eb8_l7(m, pv, 2, sp + 0x80, static_cast<int16_t>(frame_scale_i(0x1e)), 0x800060ffu, 0, 10.0f);
                if ((lwu(L7_ADDR(0x0015f5cc)) & 7u) == 0u) chain_spark_spawn(m, 2, 0.0f, u2f(0x3e4ccccdu));
                if (((lwu(L7_ADDR(0x0015f5cc)) - 4u) & 7u) == 0u) chain_spark_spawn(m, 3, 0.0f, u2f(0x3e4ccccdu));
            }
            if (lbu(m + 0x52) != 5) break;
            lq_sq(sp + 0x80, tgt);
            swc1(sp + 0x88, floor_probe_v(0.5f, V(sp + 0x80), 0));
            sw(pv + 0x160, lwu(pv + 0x160) + 1u);
            if (lw(pv + 0x164) != 0) {
                if (lw(pv + 0x160) < frame_scale_i(0x3c)) {
                    vec3_add(V(pv + 0x140), V(pv + 0x140), V(pv + 0x130));
                    sub_001f93e0(L7_ADDR(0x00312420), m);
                    sw(pv + 0x164, 2);
                } else {
                    sw(pv + 0x164, 0);
                    sh(pv + 0x19a, static_cast<uint32_t>(frame_scale_i(0xf0)));
                    sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0xc8)));
                    sb(m + 0x20, 1);
                }
                break;
            }
            vec3_sub(V(sp + 0x90), V(pos), V(sp + 0x80));
            sw(sp + 0x98, 0);
            vec3_set_length(V(sp + 0x90), V(sp + 0x90), 1.0f);
            vec3_set_length(V(sp + 0xd0), V(sp + 0x90), u2f(0x3d99999au));  // (0.075)
            swc1(sp + 0xa0, cos_vu(lwc1(sp + 0x48)));
            swc1(sp + 0xa4, sin_vu(lwc1(sp + 0x48)));
            sw(sp + 0xa8, 0);
            const float proj = vec3_dot(V(sp + 0xa0), V(sp + 0x90));
            vec3_set_length(V(sp + 0xe0), V(sp + 0x90), proj);
            vec3_sub(V(sp + 0xa0), V(sp + 0xa0), V(sp + 0xe0));
            sw(sp + 0xa8, 0);
            vec3_set_length(V(sp + 0xb0), V(sp + 0x90), 8.0f);
            vec3_set_length(V(sp + 0xa0), V(sp + 0xa0), 4.0f);
            vec3_add(V(sp + 0xb0), V(sp + 0xb0), V(sp + 0x80));
            vec3_sub(V(sp + 0xb0), V(sp + 0xb0), V(sp + 0xa0));
            vec3_set_length(V(sp + 0xa0), V(sp + 0xa0), 8.0f);
            vec3_add(V(sp + 0xa0), V(sp + 0xa0), V(sp + 0x80));
            vec3_set_length(V(sp + 0x90), V(sp + 0x90), 2.0f);
            vec3_sub(V(sp + 0xa0), V(sp + 0xa0), V(sp + 0x90));
            lq_sq(sp + 0xe0, 0x0013f450);
            sw(sp + 0xe8, 0);
            vec3_scale(V(sp + 0xc0), V(sp + 0xe0), static_cast<float>(frame_scale_i(0x28)));
            vec3_sub(V(sp + 0x90), V(sp + 0xa0), V(sp + 0xb0));
            vec3_scale(V(sp + 0x90), V(sp + 0x90), 0.5f);
            vec3_add(V(sp + 0x90), V(sp + 0x90), V(sp + 0xb0));
            vec3_sub(V(sp + 0x90), V(sp + 0x90), V(sp + 0x80));
            vec3_sub(V(sp + 0x90), V(sp + 0x90), V(sp + 0xd0));
            vec3_sub(V(sp + 0x90), V(sp + 0xc0), V(sp + 0x90));
            vec3_add(V(sp + 0xb0), V(sp + 0xb0), V(sp + 0x90));
            vec3_add(V(sp + 0xa0), V(sp + 0xa0), V(sp + 0x90));
            lq_sq(pv + 0x110, sp + 0xb0);
            lq_sq(pv + 0x120, sp + 0xa0);
            lq_sq(pv + 0x140, sp + 0xb0);
            vec3_sub(V(pv + 0x130), V(sp + 0xa0), V(sp + 0xb0));
            vec3_scale(V(pv + 0x130), V(pv + 0x130), fpu_div(1.0f, static_cast<float>(frame_scale_i(0x3c))));
            lq_sq(pv + 0x150, pv + 0x140);
            sw(pv + 0x15c, 0);
            sw(pv + 0x164, 1);
            break;
        }
        case 5:
        case 6: {  // (0x00315774)
            float reach = 2.0f;
            const float old_yaw = lwc1(m + 0x48);
            umb_313350_l7(m, pv, sp + 0xa0);
            float dist_goal;
            if (to_waypoint) {
                const uint32_t p = lvd::path(lw(pv + 0x188));
                lq_sq(sp + 0x90, p + static_cast<uint32_t>(lw(pv + 0x1a8) << 4) + 0x10);
                dist_goal = vec2_distance(V(pos), V(sp + 0x90));
            } else {
                reach = 6.0f;
                bool from_island = false;
                if (lw(pv + 0x29c) != -1 && (lbu(pv + 0x19c) & 3u) == 0u && point_in_box(tgt, lw(pv + 0x29c)) != 0) {
                    const int32_t ni = path_nearest_l1(tgt, lvd::path(lw(pv + 0x1a0)), 0.0f);
                    lq_sq(sp + 0x90, lvd::path(lw(pv + 0x1a0)) + static_cast<uint32_t>(ni << 4) + 0x10);
                    dist_goal = vec2_distance(V(pos), V(sp + 0x90));
                    from_island = true;
                }
                if (!from_island) {
                    lq_sq(sp + 0x90, tgt);
                    const int32_t ni = path_nearest_l1(sp + 0x90, lvd::path(lw(pv + 0x290)), 0.0f);
                    lq_sq(sp + 0x90, lvd::path(lw(pv + 0x290)) + static_cast<uint32_t>(ni << 4) + 0x10);
                    dist_goal = vec2_distance(V(pos), V(sp + 0x90));
                    if (dist_goal < fpu_add(lwc1(pv + 0x9c), 1.0f)) lq_sq(sp + 0x90, tgt);
                }
            }
            umb_313d30_l7(m, pv, dist_goal, reach, 1.5f);
            if ((lwu(pv + 0x298) & 3u) == 0u) umb_313988_l7(m, pv, sp + 0x90, pv + 0x140);
            lq_sq(sp + 0x80, pv + 0x140);
            sw(pv + 0x298, lwu(pv + 0x298) + 1u);
            const float step_yaw = atan2_f(fpu_sub(lwc1(sp + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 0x84), lwc1(m + 0x14)));
            if (lwc1(pv + 0x98) < angle_dist(lwc1(m + 0x48), step_yaw)) {
                swc1(m + 0x58, 1.0f);
                sb(pv + 0x1f4, lbu(m + 0x20));
                sb(pv + 0x1f6, 0xa);
                sb(pv + 0x1f5, 1);
                lq_sq(pv + 0xc0, sp + 0x80);
                anim_rand(m, 3, 0, 1, 0xf, 0x16);
                sb(m + 0x20, 7);
            } else {
                const uint32_t walk_flags = umb_313af0_l7(m, pv, sp + 0x90, sp + 0x80, reach);
                angle_dist(lwc1(m + 0x48), old_yaw);
                const float moved_len = vec3_length(V(pv + 0x100));
                const bool close_tgt = !to_waypoint && tgt_dist < 20.0f;
                const bool moving = fpu_mul(lwc1(0x0015ed6c), u2f(0x3c23d70au)) < moved_len;
                // (1.02 Fixes, fix 33: the 1.02 build tests bit 4 with `& 4` in place of `== 4`: L7:0x316e6c on the PAL 2.00 disc)
                const bool reached = fixes102_umbris_snagglebeast_walk() ? (walk_flags & 4u) != 0 : walk_flags == 4u;
                if (reached || !moving) {
                    sb(pv + 0x1f7, static_cast<uint32_t>(frame_scale_i(0x5a)));
                    idle_anim_pick(m, 9, 0x14, 0x28);
                    sw(pv + 0x1a8, 0xffffffffu);
                    sb(m + 0x20, 1);
                    break;
                }
                if (lbu(m + 0x53) == 0xa) {
                    if (close_tgt && (lbu(m + 0x70) & 2u) != 0) moby_anim_set(m, 0xc, 5, rand_range(0xa, 0xd));
                } else if (!close_tgt && (lbu(m + 0x70) & 2u) != 0) {
                    moby_anim_set(m, 0xa, 5, rand_range(0xa, 0xd));
                }
                if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) != 0) {
                    if (want_charge) {
                        sw(pv + 0x164, 0);
                        sw(pv + 0x160, 0);
                        const uint32_t cur = lbu(m + 0x53);
                        swc1(m + 0x58, 1.0f);
                        if (cur != 5) {
                            const int32_t f = rand_range(0, 2);
                            moby_anim_set(m, 5, f, rand_range(0x14, 0x19));
                        }
                        sb(m + 0x20, 4);
                    }
                    if (want_stomp) {
                        sw(pv + 0x164, 0);
                        anim_rand(m, 4, 0, 5, 0x14, 0x28);
                        sb(m + 0x20, 3);
                        swc1(m + 0x58, 1.0f);
                    }
                    if (hero_on_pillar) {
                        sw(pv + 0x164, 0);
                        lq_sq(pv + 0x150, lwu(0x0013f64c) + 0x10);
                        swc1(pv + 0x158, fpu_add(lwc1(pv + 0x158), 3.0f));
                        sb(m + 0x20, 9);
                    }
                    if (want_suck) {
                        const uint32_t cur = lbu(m + 0x53);
                        swc1(m + 0x58, 1.0f);
                        if (cur != 6) {
                            const int32_t f = rand_range(0, 2);
                            moby_anim_set(m, 6, f, rand_range(0x14, 0x19));
                        }
                        sw(pv + 0x164, 0);
                        sw(pv + 0x16c, 0);
                        sb(pv + 0x1c4, 0);
                        sw(pv + 0x294, 0);
                        sb(m + 0x20, 8);
                        sw(pv + 0x1f8, static_cast<uint32_t>(moby_sound_start(9, 4, m)));
                    }
                }
                if (!want_walk) {
                    swc1(m + 0x58, 1.0f);
                    idle_anim_pick(m, 2, 0x1c, 0x28);
                    sb(m + 0x20, 1);
                }
                if (lbu(m + 0x52) == 0xc) umb_3122b0_l7(m, pv, tgt);
            }
            const uint32_t ps = lbu(m + 0x52);
            if (ps == 0xa || ps == 0xc) {
                if (anim_crossed(m, 27.5f) != 0) umb_3120d8_l7(m);
                if (anim_crossed(m, 73.0f) != 0) umb_3120d8_l7(m);
            }
            break;
        }
        case 7: {  // (0x00315dfc)
            if (lbu(pv + 0x1f5) == 0) lq_sq(pv + 0xc0, HERO_POS);
            const float diff = angle_sub(atan2_f(fpu_sub(lwc1(pv + 0xc0), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0xc4), lwc1(m + 0x14))),
                                         lwc1(m + 0x48));
            if (!want_balls && (lbu(m + 0x70) & 2u) != 0) {
                const uint32_t set = diff < u2f(0x38d1b717u) ? 3u : 0xeu;  // (0.0001)
                if (lbu(m + 0x53) != set) moby_anim_set(m, set, 5, rand_range(0xa, 0xd));
            }
            const uint32_t ps0 = lbu(m + 0x52);
            if (ps0 == 3 || ps0 == 0xe || ps0 == 0xd) {
                const float t = moby_anim_time(m);
                if (t < 14.5f || (19.0f < t && t < 35.5f) || 40.5f < t) {
                    turn_damped_std(atan2_f(fpu_sub(lwc1(pv + 0xc0), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0xc4), lwc1(m + 0x14))));
                }
                const float tol = u2f(0x3a83126fu);  // (0.001)
                if (tol < diff && lbu(m + 0x53) == 3) moby_anim_set(m, 0xe, 5, rand_range(0xa, 0xd));
                else if (diff < tol && lbu(m + 0x53) == 0xe) moby_anim_set(m, 3, 5, rand_range(0xa, 0xd));
            } else {
                sw(pv + 0x84, 0);
            }
            const float rem = angle_dist(lwc1(m + 0x48),
                                         atan2_f(fpu_sub(lwc1(pv + 0xc0), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0xc4), lwc1(m + 0x14))));
            if (rem < fpu_mul(lwc1(pv + 0x98), 0.25f)) {
                V(pv + 0x100) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
                anim_rand(m, lbu(pv + 0x1f6), 0, 2, 0xf, 0x1b);
                sb(m + 0x20, lbu(pv + 0x1f4));
            }
            const uint32_t ps1 = lbu(m + 0x52);
            if (ps1 == 3 || ps1 == 0xe) {
                if (anim_crossed(m, 16.0f) != 0) umb_3120d8_l7(m);
                if (anim_crossed(m, 37.0f) != 0) umb_3120d8_l7(m);
            } else if (ps1 == 0xd) {
                if (anim_crossed(m, 13.0f) != 0) umb_3120d8_l7(m);
                if (anim_crossed(m, 34.0f) != 0) umb_3120d8_l7(m);
            }
            break;
        }
        case 8: {  // (0x00316a3c)
            if (lw(pv + 0x294) <= 0 && umb_313de8_l7(m, pv, HERO_POS, 6, 0) != 0) break;
            sw(pv + 0x294, 1);
            turn_damped_std(hero_yaw);
            moby_chain_point_v(m, 4, V(sp + 0x80));
            const float min_d = umb_317e60_l7(m, sp + 0x80);
            if (lbu(m + 0x52) != 6) break;
            lq_sq(pv + 0x140, HERO_POS);
            const float reach_r = u2f(0x3f19999au);  // (0.6)
            const float cur_len = lwc1(pv + 0x16c);
            swc1(pv + 0x148, fpu_add(lwc1(pv + 0x148), reach_r));
            if (cur_len < hero_dist) {
                sw(pv + 0x160, 0);
                swc1(pv + 0x16c, fpu_add(cur_len, fpu_mul(lwc1(0x0015ed6c), 15.0f)));
            } else {
                bool caught = lbu(pv + 0x1c4) != 0;
                if (caught) {
                    sb(pv + 0x1c4, 1);
                } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) != 0 && min_d < reach_r) {
                    sb(pv + 0x1c4, 1);
                    caught = true;
                }
                if (caught) {
                    if (lw(0x001413d4) != 0x78) {
                        lq_sq(pv + 0x110, HERO_POS);
                        lq_sq(pv + 0x120, HERO_POS);
                        const float hz = lwc1(0x0013f3d8), bz = lwc1(m + 0x18);
                        swc1(pv + 0x128, fpu_add(bz < hz ? hz : bz, 2.0f));
                        hero_set_state(0x78, 1);
                        sw(pv + 0x164, 0);
                        swc1(pv + 0x1ac, 20.0f);
                        sh(pv + 0x19e, 0);
                    }
                    vec3_sub(V(sp + 0x90), V(pv + 0x120), V(pv + 0x110));
                    vec3_scale(V(sp + 0x90), V(sp + 0x90), fpu_mul(lwc1(0x0015ed60), u2f(0x3dcccccdu)));
                    vec3_add(V(pv + 0x110), V(pv + 0x110), V(sp + 0x90));
                    lq_sq(0x00141080, pv + 0x110);
                    lq_sq(HERO_POS, pv + 0x110);
                    const float spin = u2f(0x41c90fdbu);  // (8 * pi)
                    const float a0 = angle_add(lwc1(0x00141098), fpu_mul(lwc1(0x0015ed6c), spin));
                    swc1(0x00141098, a0);
                    swc1(0x0013f3e8, angle_add(a0, fpu_mul(lwc1(0x0015ed6c), spin)));
                    swc1(pv + 0x16c, hero_dist);
                    const int32_t item = lw(L7_ADDR(0x0020c600) + (lwu(pv + 0x164) << 4));
                    if (item == -1) {
                        sw(pv + 0x164, 0);
                        sh(pv + 0x196, static_cast<uint32_t>(frame_scale_i(0x320)));
                        sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0xf0)));
                        umb_318e98_l7(m);
                        if (lw(0x001413d4) == 0x78) hero_set_state(0, 1);
                        sb(pv + 0x1c7, 0);
                        stop_beast_sound(m, pv);
                        sb(m + 0x20, 1);
                    } else {
                        const uint32_t item_u = static_cast<uint32_t>(item);
                        const uint32_t ammo_addr = 0x0013d428u + (item_u << 2);
                        if (lh(pv + 0x19e) == 0) {
                            const int32_t cap = lh(L7_ADDR(0x00179b40) + item_u * 0x4cu + 0x3c);
                            const int16_t eighth = static_cast<int16_t>(lw(ammo_addr) / 8);
                            sh(pv + 0x19e, static_cast<uint16_t>(eighth < cap ? cap : eighth));
                            const float d0 = lwc1(pv + 0x1ac);
                            swc1(pv + 0x1ac, rand_range_f(fpu_add(d0, 20.0f), fpu_add(d0, 100.0f)));
                        }
                        int32_t drop = lh(pv + 0x19e);
                        const int32_t have = lw(ammo_addr);
                        if (have < drop) {
                            drop = have;
                            sh(pv + 0x19e, 0);
                            sw(pv + 0x164, lwu(pv + 0x164) + 1u);
                        }
                        const int32_t excess = lw(ammo_addr) - (drop << 3);
                        if (excess > 0) drop += excess;
                        if (drop > 0) {
                            float land_z = 0.0f;
                            if (lw(pv + 0x18c) == -1) {
                                const float dt0 = lwc1(0x0015ed6c);
                                rand_sphere_point(sp + 0x90, fpu_mul(dt0, 4.0f), fpu_mul(dt0, 10.0f));
                                const float dt1 = lwc1(0x0015ed6c);
                                if (lwc1(sp + 0x98) < fpu_add(dt1, dt1)) {
                                    swc1(sp + 0x98, rand_range_f(fpu_mul(dt1, 3.0f), fpu_mul(dt1, 6.0f)));
                                }
                                vec3_set_length(V(sp + 0xa0), V(sp + 0x90), u2f(0x3f666666u));  // (0.9)
                                vec3_add(V(sp + 0xa0), V(sp + 0xa0), V(HERO_POS));
                            } else {
                                const uint32_t p = lvd::path(lw(pv + 0x18c));
                                sw(sp + 0xf4, static_cast<uint32_t>(path_nearest_l1(HERO_POS, p, 0.0f)));
                                swc1(sp + 0xf8, 0.0f);
                                float step_d = lwc1(pv + 0x1ac);
                                for (;;) {
                                    step_d = fpu_add(step_d, 10.0f);
                                    swc1(pv + 0x1ac, step_d);
                                    int32_t pi = lw(sp + 0xf4);
                                    float pf = lwc1(sp + 0xf8);
                                    path_advance_v(p, V(sp + 0xb0), &pi, &pf, 1, step_d);
                                    sw(sp + 0xf4, static_cast<uint32_t>(pi));
                                    swc1(sp + 0xf8, pf);
                                    const float dh = vec2_distance(V(HERO_POS), V(sp + 0xb0));
                                    const float db = vec2_distance(V(pos), V(sp + 0xb0));
                                    if (!(dh < 8.0f) && !(db < 12.0f)) break;
                                    step_d = lwc1(pv + 0x1ac);
                                }
                                umb_313920_l7(sp + 0xb0, 0.5f);
                                vec3_sub(V(sp + 0xa0), V(sp + 0xb0), V(HERO_POS));
                                land_z = lwc1(sp + 0xb8);
                                vec3_set_length(V(sp + 0xa0), V(sp + 0xa0), u2f(0x3f666666u));
                                lq_sq(sp + 0x90, sp + 0xa0);
                                sw(sp + 0x98, 0);
                                vec3_set_length(V(sp + 0x90), V(sp + 0x90), fpu_mul(lwc1(0x0015ed6c), 15.0f));
                                vec3_add(V(sp + 0xa0), V(sp + 0xa0), V(HERO_POS));
                                const float spd = fpu_mul(lwc1(0x0015ed6c), 15.0f);
                                const float grav = fpu_mul(lwc1(0x0015ed70), u2f(0xc11ccccdu));  // (-9.8)
                                swc1(sp + 0x98, sub_0025abf0(sp + 0xa0, sp + 0xb0, nullptr, spd, grav));
                                const float extra = rand_range_f(u2f(0x3f333333u), 2.0f);
                                swc1(pv + 0x1ac, fpu_add(lwc1(pv + 0x1ac), extra));
                            }
                            moby_sound_start(0xb, 0, m);
                            const uint32_t pk = pickup_spawn(sp + 0xa0, item_u, drop, 0);
                            if (pk != 0) {
                                const uint32_t pkpv = lwu(pk + 0x78);
                                lq_sq(pkpv + 0x10, sp + 0x90);
                                swc1(pkpv + 0x28, land_z);
                                sw(ammo_addr, lwu(ammo_addr) - static_cast<uint32_t>(drop));
                                sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0)));
                            }
                        }
                    }
                }
            }
            if (!want_suck) {
                sw(pv + 0x164, 0);
                sh(pv + 0x196, static_cast<uint32_t>(frame_scale_i(0xf)));
                umb_318e98_l7(m);
                if (lw(0x001413d4) == 0x78) hero_set_state(0, 1);
                sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0x46)));
                sb(pv + 0x1c7, 0);
                stop_beast_sound(m, pv);
                sb(m + 0x20, 1);
            }
            if (want_balls) {
                sh(pv + 0x196, static_cast<uint32_t>(frame_scale_i(0x1e)));
                anim_rand(m, 9, 0, 2, 0x10, 0x19);
                umb_318e98_l7(m);
                if (lw(0x001413d4) == 0x78) hero_set_state(0, 1);
                stop_beast_sound(m, pv);
                sb(m + 0x20, 2);
            }
            break;
        }
        case 9: {  // (0x0031727c)
            if (lw(pv + 0x164) <= 0 && umb_313de8_l7(m, pv, pv + 0x150, 7, 1) != 0) break;
            turn_damped_std(atan2_f(fpu_sub(lwc1(pv + 0x150), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x154), lwc1(m + 0x14))));
            switch (lwu(pv + 0x164)) {  // (jump table 0x211990: pillar charge sub-steps 0 .. 5)
                case 0: {  // (0x00317330)
                    if (lbu(m + 0x53) != 7) moby_anim_set(m, 7, 0, rand_range(0xa, 0xd));
                    sw(pv + 0x160, 0);
                    sw(pv + 0x164, 1);
                    break;
                }
                case 1: {  // (0x0031736c)
                    if ((lwu(m + 0x50) & 0xffff0000u) != 0x07070000u && lbu(m + 0x53) != 7) {
                        moby_anim_set(m, 7, 0, rand_range(0xa, 0xd));
                    }
                    if (lbu(m + 0x52) != 7 || !(9.5f <= moby_anim_time(m))) break;
                    moby_chain_point_v(m, 0, V(sp + 0x90));
                    moby_chain_point_v(m, 1, V(sp + 0xa0));
                    vec3_add(V(sp + 0x80), V(sp + 0x90), V(sp + 0xa0));
                    vec3_scale(V(sp + 0x80), V(sp + 0x80), 0.5f);
                    lq_sq(pv + 0x110, sp + 0x80);
                    vec3_sub(V(pv + 0x130), V(pv + 0x150), V(pv + 0x110));
                    vec3_set_length(V(pv + 0x130), V(pv + 0x130), lwc1(0x0015ed60));
                    sw(pv + 0x164, 2);
                    break;
                }
                case 2: {  // (0x0031746c)
                    if ((lwu(m + 0x50) & 0xffff0000u) != 0x07070000u && lbu(m + 0x53) != 7) {
                        moby_anim_set(m, 7, 0, rand_range(7, 0xa));
                    }
                    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) != 0) {
                        sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0xa)));
                        chain_spark_spawn(m, 0, 800000.0f, 5.0f);
                        chain_spark_spawn(m, 1, 800000.0f, 5.0f);
                    }
                    if (31.5f <= moby_anim_time(m)) {
                        sw(pv + 0x16c, 0);
                        sw(pv + 0x164, 3);
                        sw(pv + 0x11c, 0);
                    }
                    break;
                }
                case 3: {  // (0x0031753c)
                    const float d = vec2_distance(V(pos), V(sp + 0x60));
                    const float next_d = fpu_add(lwc1(pv + 0x16c), lwc1(0x0015ed60));
                    swc1(pv + 0x16c, next_d);
                    if (d <= next_d) sw(pv + 0x164, 4);
                    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) != 0) {
                        sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0x14)));
                    }
                    sub_001f93e0(L7_ADDR(0x003131b0), m);
                    break;
                }
                case 4: {  // (0x00317590)
                    vec3_sub(V(sp + 0x80), V(pv + 0x120), V(pv + 0x110));
                    vec3_set_length(V(sp + 0x80), V(sp + 0x80), lwc1(0x0015ed60));
                    vec3_add(V(pv + 0x110), V(pv + 0x110), V(sp + 0x80));
                    umb_311eb8_l7(m, pv, 3, pv + 0x110, static_cast<int16_t>(frame_scale_i(0x1e)), 0x800060ffu, 0, 10.0f);
                    if (lw(pv + 0x164) != 0 && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) != 0) {
                        sw(pv + 0x164, 5);
                    }
                    sub_001f93e0(L7_ADDR(0x003131b0), m);
                    break;
                }
                case 5: {  // (0x0031763c)
                    swc1(m + 0x58, 1.0f);
                    sw(pv + 0x164, 0);
                    sw(pv + 0x160, 0);
                    idle_anim_pick(m, 7, 0x1b, 0x2d);
                    sb(m + 0x20, 1);
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case 10: {  // (0x003176ac)
            if (lw(pv + 0x1b0) != -1) {
                sub_002359f8(lwu(pv + 0x1b0), 0);
                sw(pv + 0x1b0, 0xffffffffu);
            }
            if ((lbu(m + 0x70) & 2u) == 0) break;
            vec3_set_length(V(pv + 0x100), V(pv + 0x100), 1.0f);
            swc1(pv + 0x108, 1.0f);
            lq_sq(sp + 0x80, pos);
            swc1(sp + 0x88, fpu_add(lwc1(pv + 0x1b8), 1.0f));
            explosion_effect(m, pv + 0x100, sp + 0x80, 0x14, 0xc, 0x40, -1, 1, 1, -1, 0, 0.0f, 0.0f, 7.0f, 5.0f, 10.0f, 1.0f, 50.0f);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            if (lw(pv + 0x1cc) != -1) moby_rest_sparkles(lwu(L7_ADDR(0x0015ffd8)) + (lwu(pv + 0x1cc) << 8));
            sb(m + 0x20, 0x10);
            swc1(m + 0x18, 20.0f);
            break;
        }
        case 11: {  // (0x003177a8)
            if ((lbu(m + 0x70) & 2u) == 0) break;
            if (hero_on_pillar) {
                sw(pv + 0x164, 0);
                lq_sq(pv + 0x150, lwu(0x0013f64c) + 0x10);
                swc1(pv + 0x158, fpu_add(lwc1(pv + 0x158), 3.0f));
                sb(m + 0x20, 9);
            } else {
                sb(m + 0x20, 1);
            }
            break;
        }
        case 12: {  // (0x00316190)
            const float sixth = int_to_float(ee_div(lh(pv + 0x24), 6));
            if (lwc1(pv + 0x20) <= sixth) {
                if (lbu(m + 0x53) != 8) moby_anim_set(m, 8, 0x35, rand_range(0xa, 0xd));
            } else if (lbu(m + 0x53) != 0xb) {
                moby_anim_set(m, 0xb, 0x3d, 1);
            }
            sb(m + 0x20, 0xd);
            sw(pv + 0x108, 0);
            if ((lbu(m + 0xbc) % 3u) < 2u) sb(m + 0xbc, lbu(m + 0xbc) + 1u);
            if ((lbu(m + 0xbc) % 3u) < 2u) sb(m + 0xbc, lbu(m + 0xbc) + 1u);
            if (!(lbu(m + 0xbc) < 8u)) sb(m + 0xbc, 8);
            uint32_t slot = lbu(pv + 0x19d);
            const int32_t p0 = lw(pv + 0x1d8 + (slot << 2));
            if (p0 == -1) break;
            const uint32_t alt = slot + 3u;
            const int32_t p1 = lw(pv + 0x1d8 + (alt << 2));
            if (p1 != -1) {
                const uint32_t base_m = lwu(L7_ADDR(0x0015ffd8));
                const uint32_t pv0 = lwu(base_m + (static_cast<uint32_t>(p0) << 8) + 0x78);
                const uint32_t pv1 = lwu(base_m + (static_cast<uint32_t>(p1) << 8) + 0x78);
                const int32_t b0 = lw(pv0 + 0x28), b1 = lw(pv1 + 0x28);
                if (b0 != -1 && b1 != -1) {
                    const float d0 = vec2_distance(V(HERO_POS), V(lvd::box(lvd::BOXES, b0) + 0x30));
                    const float d1 = vec2_distance(V(HERO_POS), V(lvd::box(lvd::BOXES, b1) + 0x30));
                    int32_t chosen = b0;
                    if (d1 < d0) {
                        chosen = b1;
                        slot = alt;
                    }
                    const uint32_t box = lvd::box(lvd::BOXES, chosen);
                    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(box + 0x30), lwc1(m + 0x10)), fpu_sub(lwc1(box + 0x34), lwc1(m + 0x14))));
                }
            }
            sb(lwu(L7_ADDR(0x0015ffd8)) + (lwu(pv + 0x1d8 + (slot << 2)) << 8) + 0xbc, 1);
            break;
        }
        case 13: {  // (0x003163a4)
            if (10.0f < moby_anim_time(m)) {
                moby_chain_point_v(m, 0xa, V(sp + 0x80));
                swc1(sp + 0x88, u2f(0x420a6666u));  // (34.6)
                if (rand_mod(0x1a) == 0) spawn_particle_2d(sp + 0x80, 0, 0x40103080u, u2f(0x3fcccccdu), 14700.0f);
            }
            bool done;
            if (lbu(m + 0xbc) == 8) {
                const float vz = lwc1(pv + 0x108);
                vec3_scale(V(pv + 0x100), V(pv + 0x100), u2f(0x3f333333u));  // (0.7)
                const float g = fpu_mul(lwc1(0x0015ed6c), u2f(0x411ccccdu));  // (9.8)
                swc1(pv + 0x108, fpu_sub(vz, g));
                vec3_add(V(pos), V(pos), V(pv + 0x100));
                done = lwc1(m + 0x18) < lwc1(pv + 0x1b8);
            } else {
                done = 79.0f < moby_anim_time(m);
            }
            if (!done) break;
            sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0xa)));
            sb(pv + 0x1c5, 2);
            sb(m + 0x20, 0xe);
            sw(pv + 0x164, static_cast<uint32_t>(ee_div(lh(pv + 0x24), 6)));
            break;
        }
        case 14: {  // (0x00316504)
            vec3_scale(V(pv + 0x100), V(pv + 0x100), u2f(0x3ecccccdu));  // (0.4)
            vec3_add(V(pos), V(pos), V(pv + 0x100));
            if (moby_anim_time(m) < 168.0f) {
                moby_chain_point_v(m, 0xa, V(sp + 0x80));
                swc1(sp + 0x88, u2f(0x420a6666u));
                if (rand_mod(0x1a) == 0) spawn_particle_2d(sp + 0x80, 0, 0x40103080u, u2f(0x3fcccccdu), 14700.0f);
            }
            if (anim_crossed(m, 168.0f) != 0) {
                moby_chain_point_v(m, 5, V(sp + 0x80));
                umb_30dbb0_l7(m, sp + 0x80, 0x3a, 1.0f, 25.0f, 2.0f, fpu_mul(lwc1(0x0015ed6c), 8.0f), 1.0f);
            }
            if (anim_crossed(m, 168.5f) != 0) {
                moby_chain_point_v(m, 6, V(sp + 0x80));
                umb_30dbb0_l7(m, sp + 0x80, 0x3a, 1.0f, 25.0f, 2.0f, fpu_mul(lwc1(0x0015ed6c), 8.0f), 1.0f);
                umb_3120d8_l7(m);
            }
            if (lw(pv + 0x164) > 0) {
                if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) == 0) break;
                sw(pv + 0x164, static_cast<uint32_t>(lw(pv + 0x164) - 1));
                swc1(hp_ptr, fpu_sub(lwc1(hp_ptr), 1.0f));
                sb(pv + 0x67, 0xfa);
                color_pulse_start(m, pv + 0x60);
                if (lwc1(hp_ptr) <= 0.0f) {
                    swc1(hp_ptr, 0.0f);
                    if (lbu(m + 0x53) != 8) moby_anim_set(m, 8, 0, rand_range(0xa, 0xd));
                    if (lw(pv + 0x1f0) != -1) {
                        talker_word_set(m, 1);
                        moby_drop_bolts(m, 0, -1);
                        checkpoint_mark_collected(lbu(m + 0xb0));
                        const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0x1f0));
                        checkpoint_save(b + 0x30, b + 0x70);
                    }
                    sb(m + 0x20, 0xa);
                } else {
                    sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x164) > 0 ? 0xa : 0x14)));
                }
                break;
            }
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x160))) == 0) break;
            const uint32_t kind = lbu(pv + 0x19d);
            if (kind == 0) {
                lq_sq(pv + 0x120, lvd::path(lw(pv + 0x1a0)) + 0x10);
                sb(pv + 0x19c, lbu(pv + 0x19c) & 0xfeu);
            } else if (kind == 1) {
                lq_sq(pv + 0x120, lvd::path(lw(pv + 0x1a0)) + 0x20);
                sb(pv + 0x19c, lbu(pv + 0x19c) & 0xfdu);
            } else if (kind == 2) {
                const float d2 = vec2_distance(V(HERO_POS), V(lvd::path(lw(pv + 0x1a0)) + 0x30));
                const float d3 = vec2_distance(V(HERO_POS), V(lvd::path(lw(pv + 0x1a0)) + 0x40));
                lq_sq(pv + 0x120, lvd::path(lw(pv + 0x1a0)) + (d3 < d2 ? 0x30u : 0x40u));
                sb(pv + 0x19c, lbu(pv + 0x19c) & 0xfbu);
            }
            sw(pv + 0x1a8, lbu(pv + lbu(pv + 0x19d) + 0x1d4));
            const int32_t wn = umb_313350_l7(m, pv, sp + 0x80);
            path_links_build(sp + 0x80, wn, lwu(pv + 0x188));
            sb(m + 0x20, 0xf);
            break;
        }
        case 15: {  // (0x00316914)
            if (198.0f < moby_anim_time(m)) {
                if (lbu(m + 0x53) != 0xa) moby_anim_set(m, 0xa, 0, 0);
                moby_chain_point_v(m, 0xa, V(sp + 0x80));
                lq_sq(pv + 0xc0, sp + 0x80);
                swc1(pv + 0xc8, lwc1(pv + 0x128));
                sw(sp + 0xf0, 0xa);
                moby_chain_matrices(m, 1, sp + 0xf0, sp + 0x90);
                mat_to_euler(sp + 0x90, sp + 0xd0);
                swc1(pv + 0xcc, angle_add(lwc1(m + 0x48), lwc1(sp + 0xd8)));
            }
            if (lbu(m + 0x52) == 0xa) {
                lq_sq(pos, pv + 0xc0);
                swc1(m + 0x48, lwc1(pv + 0xcc));
                const int32_t idx = lw(pv + 0x1d8 + (lbu(pv + 0x19d) << 2));
                if (idx != -1) sb(lwu(L7_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8) + 0xbc, 0);
                swc1(m + 0x58, 1.5f);
                sb(m + 0xbc, lbu(m + 0xbc) + 1u);
                sw(pv + 0x298, 0);
                sb(m + 0x20, 6);
                sw(pv + 0x160, static_cast<uint32_t>(frame_scale_i(0x37)));
            }
            break;
        }
        case 16: {  // (0x0031780c)
            const int32_t slot = lh(pv + 0x280);
            if (slot != -1) region_clear(static_cast<uint32_t>(slot));
            const int32_t lava_idx = lw(pv + 0x1d0);
            if (lava_idx != -1) {
                const uint32_t lava = lwu(L7_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(lava_idx) << 8);
                const float z = lwc1(lava + 0x18);
                if (z < 34.5f) {
                    swc1(lava + 0x18, fpu_add(z, fpu_mul(fpu_sub(u2f(0x420a6666u), z), u2f(0x3dcccccdu))));
                    break;
                }
            }
            moby_retire(m);
            ee_stack_free(0x1f0);
            return;
        }
        case 17:  // (0x0031788c)
        default:
            break;
    }

    vec3_sub(V(pv + 0x100), V(pos), V(sp + 0x20));
    if (lbu(m + 0x20) < 10u) floor_light_add(m, 2.5f);
    umb_312150_l7(pv);
    ee_stack_free(0x1f0);
}

}  // namespace rac1
