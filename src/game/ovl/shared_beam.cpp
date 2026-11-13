// The electric beams of Blarg (level 6), Orxon (10) and Gemlik Base (13): class 1068's beam of 16 points (pvars from +0x230,
// on Gemlik from +0x210: each point's w its strength) drawn as 15 pairs of quads that flicker, with sparks off its strong
// points; and the shards (classes 1085 .. 1089) that fall and spin until their life is out. The three levels' copies are the
// same code, relinked, apart from the beam's place in the pvars (0x20 earlier on Gemlik); the tuning words and the beam's
// texture coordinates are each level's own data (per-level bases below). Header shared_beam.h.
#include <game/ovl/shared_beam.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
// The beam's draw words: the ALPHA register's fields +0 .. +0x10, the effect texture +0x14, the colours of the core +0x18 /
// +0x1c and of the edge +0x20 / +0x24; then the sparks' (+0x28): their colours +0, their w words +8 / +0xc, the frame
// counts' ranges +0x10 / +0x18 / +0x20, the speed range +0x28, the rise range +0x30, the chance +0x38, the texture +0x3c.
uint32_t beam_tuning() {
    if (on_level(ORXON)) return L10_ADDR(0x00161cec);
    if (on_level(GEMLIK)) return L13_ADDR(0x00161e88);
    return L6_ADDR(0x00161fdc);
}
// the beam's four texture coordinates (level data; the draw moves two of its t's)
uint32_t beam_st() {
    if (on_level(ORXON)) return L10_ADDR(0x001da0f0);
    if (on_level(GEMLIK)) return L13_ADDR(0x001d9b70);
    return L6_ADDR(0x001f2e90);
}
// the shards' size factor
uint32_t shard_tuning() {
    if (on_level(ORXON)) return L10_ADDR(0x00161d6c);
    if (on_level(GEMLIK)) return L13_ADDR(0x00161d34);
    return L6_ADDR(0x00162080);
}
// the beam's 16 points in the pvars, and its origin before them
uint32_t beam_points(uint32_t pv) { return pv + (on_level(GEMLIK) ? 0x210u : 0x230u); }

constexpr int32_t kPoints = 16;

uint64_t sx(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }
}  // namespace

// Sparks (particle 2) off the beam: at each point (0 .. 14) where it or the next one is strong (w 0.99 or more), by chance.
// orig: L6:0x002ff978 L10:0x002dc990 L13:0x00309bc8
void beam_sparks(uint32_t m) {
    const uint32_t points = beam_points(lwu(m + 0x78));
    const uint32_t t = beam_tuning() + 0x28;
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t at = sp + 0x10, vel0 = sp + 0x20, vel1 = sp + 0x30;
    const float strong = u2f(0x3f7d70a4u);  // (0.99)
    for (int32_t i = 0; i < kPoints - 1; ++i) {
        const uint32_t p = points + static_cast<uint32_t>(i) * 0x10;
        if (lwc1(p + 0xc) < strong && lwc1(p + 0x1c) < strong) continue;
        if (lwc1(t + 0x38) < rand_range_f(0.0f, 1.0f)) continue;
        const float a = rand_angle();
        const float b = rand_angle();
        swc1(vel0, fpu_mul(cos_vu(a), rand_range_f(lwc1(t + 0x28), lwc1(t + 0x2c))));
        swc1(vel0 + 4, fpu_mul(sin_vu(a), rand_range_f(lwc1(t + 0x28), lwc1(t + 0x2c))));
        sw(vel0 + 8, 0);
        swc1(vel1, fpu_mul(cos_vu(b), rand_range_f(lwc1(t + 0x28), lwc1(t + 0x2c))));
        swc1(vel1 + 4, fpu_mul(sin_vu(b), rand_range_f(lwc1(t + 0x28), lwc1(t + 0x2c))));
        sw(vel1 + 8, 0);
        swc1(vel0 + 8, rand_range_f(lwc1(t + 0x30), lwc1(t + 0x34)));
        swc1(vel1 + 8, rand_range_f(lwc1(t + 0x30), lwc1(t + 0x34)));
        const int32_t life = float_to_int(frame_scale_f(rand_range_f(lwc1(t + 0x10), lwc1(t + 0x14))));
        const int32_t grow = float_to_int(frame_scale_f(rand_range_f(lwc1(t + 0x18), lwc1(t + 0x1c))));
        const int32_t fade = float_to_int(frame_scale_f(rand_range_f(lwc1(t + 0x20), lwc1(t + 0x24))));
        vec3_scale(V(vel0), V(vel0), fpu_div(1.0f, int_to_float(life)));  // (per frame of its life)
        swc1(vel0 + 0xc, lwc1(t + 8));
        vec3_scale(V(vel1), V(vel1), fpu_div(1.0f, int_to_float(fade)));
        swc1(vel1 + 0xc, lwc1(t + 0xc));
        vec3_lerp(V(at), V(p), V(p), rand_range_f(0.0f, 1.0f));  // (between the point and itself, as the original does)
        spawn_particle_02(at, vel0, vel1, lwu(t), lwu(t + 4), static_cast<uint32_t>(life), static_cast<uint32_t>(grow),
                          static_cast<uint32_t>(fade), lw(t + 0x3c));
    }
    ee_stack_free(0x40);
}

// The beam: for each of its 15 segments a quad from the point to the next (the core: the second corner of each pair pulled
// 0.5 back toward the origin and 0.125 down, coloured as the edge), drawn twice, the second time with those corners 0.25 up;
// the texture's t at the far end flickers each segment.
// orig: L6:0x002ff680 L10:0x002dc698 L13:0x003098d0
void beam_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t points = beam_points(pv);
    const uint32_t origin = points - 0x20;
    const uint32_t tune = beam_tuning();
    const uint32_t st = beam_st();
    // (the frame's slots as the original lays them out: two quad descriptors, sp +0 and +0x90, the matrix +0x120)
    const uint32_t sp = ee_stack_alloc(0x250);
    const uint32_t desc = sp, desc2 = sp + 0x90, matrix = sp + 0x120, pull = sp + 0x160;
    sub_002a0ba8(0x47, 0x5380bu);
    const uint64_t tex = effect_tex0(lw(tune + 0x14));
    sd(desc + 0x78, tex);
    const uint64_t tex2 = effect_tex0(lw(tune + 0x14));
    const uint64_t alpha = sx(lwu(tune)) | (sx(lwu(tune + 4)) << 2) | (sx(lwu(tune + 8)) << 4) | (sx(lwu(tune + 0xc)) << 6) |
                           (sx(lwu(tune + 0x10)) << 32);
    const uint64_t tex1 = (0xff90ull << 32) | 0x260u;
    sd(desc2 + 0x78, tex2);
    sd(desc + 0x80, tex1);
    sd(desc + 0x88, alpha);
    sd(desc2 + 0x80, tex1);
    sd(desc2 + 0x70, 0);
    sd(desc + 0x70, 0);
    sd(desc2 + 0x88, alpha);
    mat4_identity(M(matrix));
    for (int32_t seg = 0; seg < kPoints - 1; ++seg) {
        swc1(st + 0xc, lwc1(st + 0x1c));
        swc1(st + 0x1c, rand_range_f(0.0f, u2f(0x3e4ccccdu)));  // (0.2)
        for (int32_t k = 0; k < 4; ++k) {
            const uint32_t corner = desc + static_cast<uint32_t>(k) * 0x10;
            const uint32_t p = points + static_cast<uint32_t>(seg + k / 2) * 0x10;
            V(corner) = V(p);
            uint32_t from, to;
            if ((k & 1) != 0) {
                vec3_sub(V(pull), V(corner), V(origin));
                vec3_set_length(V(pull), V(pull), 0.5f);
                vec3_sub(V(corner), V(corner), V(pull));
                swc1(corner + 8, fpu_sub(lwc1(corner + 8), 0.125f));
                from = lwu(tune + 0x20);
                to = lwu(tune + 0x24);
            } else {
                from = lwu(tune + 0x18);
                to = lwu(tune + 0x1c);
            }
            sw(desc + 0x40 + static_cast<uint32_t>(k) * 4, rgba_lerp(from, to, lwc1(p + 0xc)));
            const float s = lwc1(st + static_cast<uint32_t>(k) * 8), tt = lwc1(st + static_cast<uint32_t>(k) * 8 + 4);
            swc1(desc + 0x50 + static_cast<uint32_t>(k) * 8, s);
            swc1(desc + 0x54 + static_cast<uint32_t>(k) * 8, tt);
            swc1(desc2 + 0x50 + static_cast<uint32_t>(k) * 8, s);
            swc1(desc2 + 0x54 + static_cast<uint32_t>(k) * 8, tt);
        }
        quad_draw(desc, matrix);
        for (int32_t k = 1; k < 4; k += 2) {
            const uint32_t corner = desc + static_cast<uint32_t>(k) * 0x10;
            swc1(corner + 8, fpu_add(lwc1(corner + 8), 0.25f));
        }
        quad_draw(desc, matrix);
    }
    sub_002a0ba8(0x47, 0x5360bu);
    ee_stack_free(0x250);
}

// The shard: its size from its class's, it falls (gravity 15) and spins (its turn added twice: the vector's, then each angle's
// wrapped), and it is gone when its life (+0xbc, frames) is out.
// orig: L6:0x00300b90 L10:0x002dd010 L13:0x00303878
void beam_shard_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float fall = fpu_mul(lwc1(0x0015ed70), 15.0f);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(shard_tuning())));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fall));
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    vec3_add(V(m + 0x40), V(m + 0x40), V(pv + 0x10));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x18)));
    const uint32_t life = (lbu(m + 0xbc) - 1u) & 0xffu;
    sb(m + 0xbc, life);
    if (life == 0) moby_retire(m);
}

// A shard (a class from 1085 to 1089 at random) spawned at `pos` (EE address) with the speed `vel` (into its pvars): its size
// the class's x the level's factor, shown (+0x30 0xff, +0x32 0x7e, +0x31 1), its turn random up to a turn a second on each
// axis, its life 60 .. 120 frames (scaled). Returns the shard, or 0 when none could be spawned. (Blarg's and Orxon's copy:
// classes 302 and 1068 break into these.)
// orig: L6:0x00300c60 L10:0x002dd0e0
uint32_t beam_shard_spawn(uint32_t pos, uint32_t vel) {
    const uint32_t sp = ee_stack_alloc(0x20);
    const uint32_t at = sp, speed = sp + 0x10;
    lq_sq(at, pos);
    lq_sq(speed, vel);
    const float cls = rand_range_f(u2f(0x4487a000u), u2f(0x44882000u));  // (1085 .. 1089)
    const uint32_t o = moby_spawn(static_cast<uint32_t>(float_to_int(cls)));
    if (o != 0) {
        const uint32_t pv = lwu(o + 0x78);
        sb(o + 0x30, 0xff);
        sh(o + 0x32, 0x7e);
        sb(o + 0x31, 1);
        swc1(o + 0x2c, fpu_mul(lwc1(lwu(o + 0x24) + 0x24), lwc1(shard_tuning())));
        lq_sq(o + 0x10, at);
        lq_sq(pv, speed);
        const float turn = u2f(0x43b40000u), degree = u2f(0x3c8efa35u);  // (360, a degree in radians)
        for (uint32_t k = 0; k < 3; ++k)
            swc1(pv + 0x10 + k * 4, fpu_mul(fpu_mul(rand_range_f(-turn, turn), degree), lwc1(0x0015ed6c)));
        sb(o + 0xbc, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(60.0f, 120.0f)))));
        moby_update_transform(o);
    }
    ee_stack_free(0x20);
    return o;
}

}  // namespace rac1
