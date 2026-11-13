// Class 78 of Quartu (level 15) and Drek's fleet (17), a sleeping turret the alarms of class 408 wake, and helpers of class 44's
// guards. One port of both levels' copies, the same code relinked: the draw callback is the level's own copy (per level
// below); the moby table 0x15ffd8, the group lists 0x1ab840, the levels' done bytes 0x1bb230 / 0x1ba5d0 (+0x454) and the
// class flags 0x15fc88, the particle look 0x1b2170 are the engine's (OV); the hero (0x13f3d0, 0x13f420 its head), the sound
// channels 0x13e550, the level number 0x15ed84 and the per-level flag words / bytes 0x14c190 / 0x14c050 the base ELF's.
// Header shared_class78.h.
//
// Class 78's moby: its flag +0xb0, its flag index +0xb2 (s16), its sound channel +0xbc. Its pvars: +0 the aim point (out:
// the hero's head in front of it, else 0), +0x10 a moby (index) whose state 2 breaks it, +0x14 the frames shown.
// States: 0 start, 1 awake (sparks, aiming), 2 shown for 61 frames, 3 done.
#include <game/ovl/shared_class78.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
constexpr uint32_t SHOT_CLASS = 0x3a6;  // (934)
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t HERO_HEAD = 0x0013f420;

// the level's copy of class 78's draw callback
uint32_t class78_draw() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x002a8808) : L15_ADDR(0x002a3138); }

uint32_t first_moby() { return lwu(OV(0x0015ffd8)); }
int32_t flag_index(uint32_t m) { return static_cast<int16_t>(lhu(m + 0xb2)); }
// a sound channel (0x13e550, 0x70 bytes each) playing on `m`
bool playing_on(uint32_t ch, uint32_t m) {
    const uint32_t c = 0x0013e550 + static_cast<uint32_t>(static_cast<int32_t>(ch) * 0x70);
    return lwu(c + 0x88) == m && lbu(c + 0x74) != 0;
}
// a quarter / a half of a frame count as the original rounds it (toward zero), through a float
int32_t quarter(int32_t n) { return float_to_int(static_cast<float>((n < 0 ? n + 3 : n) >> 2)); }
int32_t half(int32_t n) { return float_to_int(static_cast<float>((n + static_cast<int32_t>(static_cast<uint32_t>(n) >> 31)) >> 1)); }
}  // namespace

// orig: L15:0x002a2bf0 L17:0x002a82c0
void class78_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {  // done before (its done byte or its level flag): state 3, else asleep (1)
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        const int32_t idx = flag_index(m);
        bool done = lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0;
        if (!done) {
            const uint32_t word = lwu(0x0014c190 + (static_cast<uint32_t>(idx >> 5) << 2) + (lwu(0x0015ed84) << 8));
            done = ((word >> (lhu(m + 0xb2) & 0x1f)) & 1u) != 0;
        }
        if (!done) {
            sb(m + 0x20, 1);
            return;
        }
        sb(m + 0x20, 3);
        sb(m + 0xbc, 0xff);
        sw(m + 0x94, 0);
        return;
    }
    case 1: {
        // broken when its moby is in state 2, or its level flag byte is 0xff
        const int32_t watched = lw(pv + 0x10);
        if ((watched != -1 && lbu(first_moby() + (static_cast<uint32_t>(watched) << 8) + 0x20) == 2) ||
            lbu(0x0014c050 + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff) {
            class78_break(m);
            return;
        }
        // its hum
        if (!playing_on(lbu(m + 0xbc), m)) sb(m + 0xbc, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        const uint32_t sp = ee_stack_alloc(0xa0);
        const uint32_t head = sp + 0x10, rot = sp + 0x20, at = sp + 0x60, vel = sp + 0x70, dir = sp + 0x80;
        const uint32_t pos = m + 0x10, mat = m + 0xc0;
        // the hero's head in the moby's frame, its x cleared
        vec3_sub(V(head), V(HERO_HEAD), V(pos));
        mat3_transpose(M(rot), M(mat));
        mat4_transform(V(head), V(head), M(rot));
        sw(head, 0);
        const float zero = lwc1(head);
        // sixteen sparks about it in its frame, their two directions a thirtieth of a second's travel
        for (int32_t k = 0; k < 16; ++k) {
            swc1(at, rand_range_f(-0.25f, 0.25f));
            swc1(at + 4, rand_range_f(-5.0f, 5.0f));
            swc1(at + 8, rand_range_f(zero, 5.0f));
            swc1(vel, zero);
            swc1(vel + 4, rand_range_f(-5.0f, 5.0f));
            swc1(vel + 8, rand_range_f(-5.0f, 5.0f));
            swc1(dir, zero);
            swc1(dir + 4, rand_range_f(-5.0f, 5.0f));
            swc1(dir + 8, rand_range_f(-5.0f, 5.0f));
            mat3_transform(V(at), V(at), M(mat));
            mat3_transform(V(vel), V(vel), M(mat));
            mat3_transform(V(dir), V(dir), M(mat));
            vec3_add(V(at), V(at), V(pos));
            vec3_set_length(V(vel), V(vel), fpu_div(1.0f, static_cast<float>(frame_scale_i(0x1e))));
            vec3_set_length(V(dir), V(dir), fpu_div(1.0f, static_cast<float>(frame_scale_i(0x1e))));
            swc1(vel + 0xc, 0.125f);
            const uint32_t r = static_cast<uint32_t>(rand_mod(0x10));
            const uint32_t g = static_cast<uint32_t>(rand_mod(0x20));
            const uint32_t b = static_cast<uint32_t>(rand_mod(0x20));
            const uint32_t colour = ((r + 0x20) << 16) | ((g + 0x60) << 8) | 0x60000000u | (b + 0x60);
            const int32_t t1 = quarter(frame_scale_i(0x1e));
            const int32_t t2 = half(frame_scale_i(0x1e));
            const int32_t t3 = quarter(frame_scale_i(0x1e));
            particle_47_spawn(at, vel, dir, colour, 0x60808080u, static_cast<uint32_t>(t1), static_cast<uint32_t>(t2),
                              static_cast<uint32_t>(t3), m);
        }
        // the aim point: the hero's head when it is in front of it (|y| < 5, 0 < z < 5)
        const float z = lwc1(head + 8);
        if (abs_f(lwc1(head + 4)) < 5.0f && zero < z && z < 5.0f) {
            mat3_transform(V(pv), V(head), M(mat));
            vec3_add(V(pv), V(pv), V(pos));
        } else {
            V(pv) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        }
        sub_001f93e0(class78_draw(), m);
        ee_stack_free(0xa0);
        return;
    }
    case 2: {  // shown for 61 frames
        const int32_t shown = lw(pv + 0x14);
        sw(pv + 0x14, static_cast<uint32_t>(shown + 1));
        if (!(shown < 61)) {
            sb(m + 0x20, 3);
            return;
        }
        V(pv) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        sub_001f93e0(class78_draw(), m);
        return;
    }
    default:
        return;
    }
}

// orig: L15:0x002a2868 L17:0x002a7f38
uint32_t class78_wake(int32_t group, uint32_t pos) {
    uint32_t e = lwu(OV(0x001ab840) + (static_cast<uint32_t>(group) << 2));
    const uint32_t table = first_moby();
    for (;; e += 2) {
        const uint32_t o = table + ((lhu(e) & 0x7fffu) << 8);
        if (lbu(o + 0x20) == 1) {
            sb(o + 0x20, 2);
            if (lbu(o + 0x53) != 0) moby_anim_set(o, 0, 0, 0);
            const uint32_t flags = lhu(o + 0x34) | 1u;
            sb(o + 0x31, 0);
            sh(o + 0x34, flags);
            const uint32_t opv = lwu(o + 0x78);
            sw(o + 0x94, lwu(lwu(o + 0x24) + 0x10));
            swc1(opv + 0x64, fpu_mul(rand_range_f(-15.0f, 15.0f), u2f(0x3c8efa35u)));  // (degrees)
            sw(opv + 0x68, static_cast<uint32_t>(frame_scale_i(0x3c)));
            lq_sq(o + 0x10, pos);
            swc1(o + 0x18, fpu_sub(lwc1(o + 0x18), 1.0f));
            swc1(o + 0x48, atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(o + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(o + 0x14))));
            return 1;
        }
        if (lh(e) < 0) return 0;
    }
}

// orig: L15:0x002a35d8 L17:0x002a8ca8
void class78_break(uint32_t m) {
    sw(m + 0x94, 0);
    sb(OV(0x001ba5d0) + static_cast<uint32_t>(flag_index(m)) + 0x454, lbu(m + 0xb0) + 2);
    const uint32_t flag = lbu(m + 0xb0);
    // the done byte too, unless the class flag and the level flag byte say otherwise
    if (flag == 0xff || (lbu(OV(0x0015fc88) + flag) != 0xff && lbu(0x0014c050 + flag + (lwu(0x0015ed84) << 4)) == 0xff))
        sb(OV(0x001bb230) + static_cast<uint32_t>(flag_index(m)) + 0x454, flag + 2);
    const uint32_t ch = lbu(m + 0xbc);
    if (ch != 0xff && playing_on(ch, m)) sub_0028d918(static_cast<int32_t>(ch));
    sb(m + 0x20, 2);
    sb(m + 0xbc, 0xff);
}

// orig: L15:0x002647f0 L17:0x0026e978
uint32_t particle_47_spawn(uint32_t pos, uint32_t vel, uint32_t dir, uint32_t colour, uint32_t colour2, uint32_t t1, uint32_t t2,
                           uint32_t t3, uint32_t m) {
    const uint32_t p = alloc_particle(0x47);
    if (p == 0) return 0;
    const uint32_t sp = ee_stack_alloc(0x20);
    lq_sq(p + 0x10, pos);
    sw(p + 4, colour);
    const uint32_t size = static_cast<uint32_t>(float_to_int(4.0f)) + 0x40;
    sb(p + 3, 0x44);
    sb(p + 9, size);
    sb(p + 1, 0);
    swc1(p + 0xc, static_cast<float>(float_to_int(fpu_mul(lwc1(vel + 0xc), 210000.0f))));
    sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
    const uint32_t look = lbu(lwu(OV(0x001b2170)));
    sh(p + 0xa, t1);
    sb(p + 2, look);
    dir_pack(vel, p + 0x20);
    dir_pack(dir, p + 0x24);
    dir_unpack(V(sp), p + 0x20);
    dir_unpack(V(sp + 0x10), p + 0x24);
    sw(p + 0x28, colour);
    sw(p + 0x2c, colour2);
    sh(p + 0x30, t1);
    sh(p + 0x32, t2);
    sh(p + 0x34, t3);
    sw(p + 0x38, lwu(m + 0x78));
    ee_stack_free(0x20);
    return p;
}

// orig: L15:0x002e4580 L17:0x002e1e20
uint32_t class934_shot_spawn(uint32_t m, uint32_t dir, uint32_t pos) {
    const uint32_t o = moby_spawn(SHOT_CLASS);
    if (o == 0) return 0;
    sb(o + 0x30, 0xff);
    sh(o + 0x32, 0xff);
    const float scale = fpu_mul(lwc1(o + 0x2c), 20.0f);
    sb(o + 0x31, 1);
    sb(o + 0x20, 0);
    swc1(o + 0x2c, scale);
    sd(o + 0x38, ld(m + 0x38));
    lq_sq(o + 0x10, pos);
    const uint32_t opv = lwu(o + 0x78);
    vec3_scale(V(opv), V(dir), 5.0f);
    sw(opv + 0x38, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(opv + 0x3c, m);
    sb(o + 0x23, 0x30);
    sh(o + 0x34, lhu(o + 0x34) | 0x200u);
    swc1(o + 0x48, atan2_f(lwc1(dir), lwc1(dir + 4)));
    swc1(o + 0x44, -atan2_f(vec2_length(V(dir)), lwc1(dir + 8)));
    moby_update_transform(o);
    spawn_particle_1a(o, 0x600040ffu, 0x80, 0xffffffffu, 170000.0f);
    return o;
}

}  // namespace rac1
