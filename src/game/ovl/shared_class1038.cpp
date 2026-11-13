// Classes 1038 and 1040: one port of the copies Blarg (L6), Orxon (L10) and Drek's fleet (L17) carry, the same code relinked
// (the normalised texts compared; Orxon's copy pushes the crystal through its own build of sub_00259bc8, L10:0x24c6b8, where
// Orxon's other generated code calls the Veldin port too). Its addresses are the engine's (OV: the camera 0x166dc0, the
// frame counter 0x15f5cc, the flag words 0x1ba4d0) or the base ELF's (bare: the level number 0x15ed84, the per-level flag
// words 0x14c190 and bytes 0x14c050, the sound channels 0x13e550, the frame time 0x15ed6c); the level's own data is one block
// of the classes' tunings (per level below). Header shared_class1038.h.
//
// Class 1038's moby: its flag +0xb0, its flag index +0xb2 (s16). Its pvars: four phases +0 and their rates +0x10, four timers
// +0x20, four sizes +0x30, the crystal +0x40. Class 1040's pvars: a pointer +0 (to its +0x20), its sound channel +0x60.
#include <game/ovl/shared_class1038.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t CRYSTAL_CLASS = 0x410;  // class 1040

// The level's tunings (its own data, one block): the glow's two colours +0 / +4, its particle's words +8 / +0xc; the sparkle's
// colour +0x10, size +0x14 and life +0x18; the crystal's spin rates +0x1c / +0x20 / +0x24 (degrees a second) and scale
// +0x28; its burst's radii +0x2c / +0x30, life +0x3c (a byte), two colour pairs +0x40 / +0x44 and +0x48 / +0x4c, sizes
// +0x50 / +0x54.
uint32_t tunings(uint32_t o) {
    if (on_level(ORXON)) return L10_ADDR(0x00161c14) + o;
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161de4) + o;
    return L6_ADDR(0x00161e58) + o;
}

// the moby's flag (index +0xb2) in the level's flag words (0x14c190, 0x100 bytes a level) and in the words 0x1ba4d0
uint32_t level_flag_word(uint32_t m) {
    return 0x0014c190 + (static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(m + 0xb2))) >> 5) << 2) + (lwu(0x0015ed84) << 8);
}
uint32_t flag_word(uint32_t m) {
    return OV(0x001ba4d0) + (static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(m + 0xb2))) >> 5) << 2);
}
uint32_t flag_bit(uint32_t m, uint32_t one) { return one << (lhu(m + 0xb2) & 0x1f); }
}  // namespace

// orig: L6:0x002f7548 L10:0x002d9810 L17:0x002e3950
void class1038_glow_init(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t i = 0; i < 4; ++i) swc1(pv + i * 4, int_to_float(rand_mod(0xff)));
    swc1(pv + 0x10, -2.0f);
    swc1(pv + 0x14, -4.25f);
    swc1(pv + 0x18, 2.25f);
    swc1(pv + 0x1c, 4.5f);
    for (uint32_t i = 0; i < 4; ++i) sw(pv + 0x20 + i * 4, static_cast<uint32_t>(frame_scale_i(static_cast<int32_t>(0x3f + i * 0x40))));
    swc1(pv + 0x3c, 1.75f);
    swc1(pv + 0x38, 2.25f);
    swc1(pv + 0x30, 1.75f);
    swc1(pv + 0x34, 2.25f);
}

// Four sprites (particle 0x3b, the phase's byte as its frame) from 0.3 in front of the crystal toward the camera, 0.1 apart:
// each phase steps by its rate (wrapped to 0..255), each timer runs out and starts again every 255 frames, the colour
// between the two by how far the timer is from its middle. On every fourth frame while shown, a sparkle (particle 0x3c) a
// little above or below the holder, falling.
// orig: L6:0x002f7628 L10:0x002d98f0 L17:0x002e3a30
void class1038_glow(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t at = sp, step = sp + 0x10, spark = sp + 0x20, fall = sp + 0x30;
    const uint32_t pv = lwu(m + 0x78);
    vec3_sub(V(at), V(OV(0x00166dc0)), V(lwu(pv + 0x40) + 0x10));
    vec3_set_length(V(at), V(at), u2f(0xbe99999au));
    vec3_set_length(V(step), V(at), u2f(0x3dcccccdu));
    vec3_add(V(at), V(at), V(lwu(pv + 0x40) + 0x10));
    const float full = 255.0f;
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t phase = pv + i * 4, timer = pv + 0x20 + i * 4;
        const float next = fpu_add(lwc1(phase), lwc1(pv + 0x10 + i * 4));
        swc1(phase, next);
        if (full <= next) swc1(phase, fpu_sub(next, full));
        else if (next <= 0.0f) swc1(phase, fpu_add(next, full));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer))) != 0) sw(timer, static_cast<uint32_t>(frame_scale_i(0xff)));
        const float left = int_to_float(frame_scale_i(0xff) - lw(timer));
        const float t = abs_f(fpu_sub(0.5f, fpu_div(left, int_to_float(frame_scale_i(0xff)))));
        const uint32_t colour = rgba_lerp(lwu(tunings(0)), lwu(tunings(4)), t);
        const uint32_t frame = static_cast<uint32_t>(float_to_int(lwc1(phase))) & 0xffu;
        spawn_particle_3b(at, colour, frame, lwu(tunings(0xc)), lwu(tunings(8)), 2, 0, lwc1(pv + 0x30 + i * 4));
        vec3_add(V(at), V(at), V(step));
    }
    const uint32_t frame = lwu(OV(0x0015f5cc));
    if ((frame & 3u) == 3 && lbu(m + 0x31) != 0) {
        const float side = (frame & 4u) != 0 ? -1.0f : 1.0f;
        vec_clear(V(spark));
        swc1(spark + 8, fpu_add(lwc1(spark + 8), fpu_mul(side, u2f(0x3f666666u))));
        vec3_set_length(V(fall), V(spark), fpu_div(-2.0f, int_to_float(frame_scale_i(lw(tunings(0x18))))));
        vec3_add(V(spark), V(spark), V(m + 0x10));
        swc1(spark + 8, fpu_add(lwc1(spark + 8), u2f(0x3f59999au)));
        const uint32_t life = static_cast<uint32_t>(float_to_int(int_to_float(frame_scale_i(lw(tunings(0x18)))))) & 0xffu;
        const uint32_t start = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
        spawn_particle_3c(spark, fall, lwu(tunings(0x10)), life, start, 0, lwc1(tunings(0x14)));
    }
    ee_stack_free(0xe0);
}

// State 0: unless its flag is set (on Orxon, unless its byte of level 10 is set too: then done, 2, +0xbc 1), the crystal
// (class 1040) spawned over it at random angles, the glow started, its flag cleared in both tables (1). State 1: a hit on the
// holder pushes the crystal (kinds 0x10000); the crystal gone (its state's sign), its flag set in both tables and done (2,
// +0xbc 1); else the glow.
// orig: L6:0x002f7288 L10:0x002d9550 L17:0x002e3690
void class1038_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        const uint32_t word = level_flag_word(m);
        const bool set = ((static_cast<int32_t>(lwu(word)) >> (lhu(m + 0xb2) & 0x1f)) & 1) != 0;
        if (set && (lw(0x0015ed84) != 10 || lbu(0x0014c050 + lbu(m + 0xb0) + 0xa0) == 0xff)) {
            sb(m + 0xbc, 1);
            sb(m + 0x20, 2);
            return;
        }
        const uint32_t o = moby_spawn(CRYSTAL_CLASS);
        sh(o + 0x32, 0x40);
        sb(o + 0x30, 0x40);
        sb(o + 0x31, 1);
        sd(o + 0x38, ld(m + 0x38));
        sh(o + 0x34, lhu(m + 0x34) | 0x5000u);
        lq_sq(o + 0x10, m + 0x10);
        swc1(o + 0x40, rand_angle());
        swc1(o + 0x44, rand_angle());
        swc1(o + 0x48, rand_angle());
        swc1(o + 0x18, fpu_add(lwc1(o + 0x18), u2f(0x3f59999au)));
        sw(pv + 0x40, o);
        sb(m + 0x20, 1);
        class1038_glow_init(m);
        sw(level_flag_word(m), lwu(level_flag_word(m)) & ~flag_bit(m, 1));
        sw(flag_word(m), lwu(flag_word(m)) & ~flag_bit(m, 1));
        return;
    }
    if (state != 1) return;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    if (hit != 0) {
        sub_00259bc8(lwu(pv + 0x40), lwu(hit + 0x20), 0x10000u, hit, hit + 0x10, 1.0f);
        sb(m + 0xa4, 0xff);
    }
    if (static_cast<int8_t>(lbu(lwu(pv + 0x40) + 0x20)) >= 0) {
        class1038_glow(m);
        return;
    }
    sb(m + 0xbc, state);
    sb(m + 0x20, 2);
    sw(level_flag_word(m), lwu(level_flag_word(m)) | flag_bit(m, state));
    sw(flag_word(m), lwu(flag_word(m)) | flag_bit(m, state));
}

// Forty sparks (particle 0x3c) out of the crystal: each a random direction between the two radii (a frame's worth), a size
// between the two, one of the two colour pairs at random, mixed at random.
// orig: L6:0x002f7c68 L10:0x002d9da8 L17:0x002e3ee8
void class1040_burst(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x70);
    for (int32_t i = 0; i < 40; ++i) {
        const float dt = lwc1(0x0015ed6c);
        rand_sphere_point(sp, fpu_mul(lwc1(tunings(0x2c)), dt), fpu_mul(lwc1(tunings(0x30)), dt));
        const float size = rand_range_f(lwc1(tunings(0x50)), lwc1(tunings(0x54)));
        uint32_t colour;
        if (rand_mod(2) != 0) colour = rgba_lerp(lwu(tunings(0x40)), lwu(tunings(0x44)), rand_range_f(0.0f, 1.0f));
        else colour = rgba_lerp(lwu(tunings(0x48)), lwu(tunings(0x4c)), rand_range_f(0.0f, 1.0f));
        const uint32_t life = lbu(tunings(0x3c));
        const uint32_t start = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
        spawn_particle_3c(m + 0x10, sp, colour, life, start, 0, size);
    }
    ee_stack_free(0x70);
}

// A hit bursts it: the burst, sound 1, its own sound stopped (the channel +0x60 still its), gone. State 0: grey, its scale,
// the pointer +0, shown in front (+0x34 0x20), no sound; then (1) it spins at the level's rates. Its hum (sound 0) restarted
// when the channel stopped.
// orig: L6:0x002f7ab8 L10:0x002d9bf8 L17:0x002e3d38
void class1040_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (sub_0025a420(m, NOT_ADDR(0x330000u), 0) != 0) {
        class1040_burst(m);
        moby_sound_start(1, 0, m);
        const int32_t ch = lw(pv + 0x60);
        if (ch != -1) {
            const uint32_t slot = static_cast<uint32_t>(ch * 0x70) + 0x0013e550;
            if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) sub_0028d918(ch);
        }
        sw(pv + 0x60, 0xffffffffu);
        moby_retire(m);
        return;
    }
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(tunings(0x28))));
        sw(pv, pv + 0x20);
        sb(m + 0x20, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x20u);
        sw(pv + 0x60, 0xffffffffu);
    }
    if (state <= 1) {
        const float deg = u2f(0x3c8efa35u);
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(fpu_mul(lwc1(tunings(0x1c)), deg), lwc1(0x0015ed6c))));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), fpu_mul(fpu_mul(lwc1(tunings(0x20)), deg), lwc1(0x0015ed6c))));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(fpu_mul(lwc1(tunings(0x24)), deg), lwc1(0x0015ed6c))));
    }
    if (sub_0028d8c0(m, lw(pv + 0x60)) == 0) sw(pv + 0x60, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
}

}  // namespace rac1
