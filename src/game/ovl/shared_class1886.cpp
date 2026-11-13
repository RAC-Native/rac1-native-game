// Class 1886, a blob that class 1885 throws, and its two helpers: one port of the copies Gaspar and Gemlik Base carry (the
// same code relinked). Nothing of it is the level's own: the collision's results are the engine's (OV: the hit point
// 0x173e60, the point it was met from 0x173e70, the normal 0x173e80, the moby met 0x173e58), the frame's scale (0x15ed60)
// and its square (0x15ed70) the base ELF's (bare). Header shared_class1886.h.
//
// The pvars: +0 the velocity, +0x10 the thrower (a moby of class 1885), +0x14 the mark it left, +0x18 the frames it flies
// (a timer), +0x1a the mark grows with it while it fades, +0x1b the frames until its bounce sound can play again, +0x1c its
// index in the thrower's list (the thrower's pvar +0xb0, 20 words). States: 0 flying, 1 come to rest, 2 fading.
#include <game/ovl/shared_class1886.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr int32_t THROWER_CLASS = 0x75d;  // (1885)
constexpr int32_t BLOB_CLASS = 0x75e;     // (1886)
constexpr int32_t SLOTS = 20;             // (the thrower's list of its blobs)

float frame_scale() { return lwc1(0x0015ed60); }
float class_scale(uint32_t m) { return lwc1(lwu(m + 0x24) + 0x24); }

// A bounce off what the last collision met (the moby 0x173e58, the normal 0x173e80): off another blob the blob only
// goes on with its height speed jittered; else the velocity is reflected and, off a floor flatter than 40 degrees
// (when sub_001f35d8 says -1), sped up (off a moby) or halved, the blob coming to rest (state 1) once it is slower than
// 0.025 (off the scenery). Its bounce sound then plays unless it played in the last 12 frames.
void bounce(uint32_t m, uint32_t pv) {
    const uint32_t met = lwu(OV(0x00173e58));
    const uint32_t normal = OV(0x00173e80);
    if (met != 0 && lh(met + 0xa6) == BLOB_CLASS) {
        swc1(pv + 8, fpu_mul(lwc1(pv + 8), rand_range_f(0.9f, 1.1f)));
    } else {
        vec3_reflect(V(pv), V(pv), V(normal));
        const float slope = atan2_f(lwc1(normal + 8), vec2_length(V(normal)));
        if (slope < u2f(0x3f32b8c2u) && sub_001f35d8() == -1) {  // (40 degrees)
            if (met != 0) {
                vec3_scale(V(pv), V(pv), 1.2f);
            } else {
                vec3_scale(V(pv), V(pv), 0.5f);
                if (vec3_length(V(pv)) < 0.025f) sb(m + 0x20, 1);
            }
        }
    }
    if (lbu(pv + 0x1b) == 0) {
        moby_sound_start(0, 0, m);
        sb(pv + 0x1b, 0xc);
    }
}

// The blob's flight (state 0): its scale grows to its class's, it moves by its velocity; it bounces where the segment
// from where it was to 0.2 past where it is meets the scenery (put back 0.215 off the hit), and where a sphere of 0.2 about
// it meets something (put 0.015 out from the point met); it is pushed off the thrower's other live blobs nearer than
// 1.8 (across), falls, and turns with its velocity; its timer ending fades it.
void fly(uint32_t m, uint32_t pv, uint32_t from, uint32_t ahead, uint32_t apart) {
    const uint32_t pos = m + 0x10;
    {
        const float sc = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(sc, fpu_mul(fpu_sub(class_scale(m), sc), fpu_mul(frame_scale(), 0.05f))));
    }
    vec3_add(V(pos), V(pos), V(pv));
    vec3_set_length(V(ahead), V(pv), 0.2f);
    vec3_add(V(ahead), V(ahead), V(pos));
    sw(m + 0x94, 0);
    sw(lwu(pv + 0x10) + 0x94, 0);
    if (segment_collide_v(V(from), V(ahead), 4, m, nullptr) != 0) {
        vec3_sub(V(ahead), V(OV(0x00173e60)), V(ahead));
        vec3_set_length(V(ahead), V(ahead), 0.215f);
        vec3_add(V(pos), V(pos), V(ahead));
        bounce(m, pv);
    }
    if (sphere_collide(pos, 0.2f, 4, m) != 0) {
        vec3_sub(V(ahead), V(OV(0x00173e60)), V(OV(0x00173e70)));
        vec3_set_length(V(ahead), V(ahead), 0.015f);
        vec3_add(V(pos), V(OV(0x00173e70)), V(ahead));
        bounce(m, pv);
    }
    const uint32_t list = lwu(lwu(pv + 0x10) + 0x78) + 0xb0;
    for (int32_t i = 0; i < SLOTS; ++i) {
        const uint32_t other = lwu(list + static_cast<uint32_t>(i) * 4);
        if (other == m || other == 0 || lh(other + 0xa6) != BLOB_CLASS) continue;
        const uint32_t st = lbu(other + 0x20);
        if (st == 0xfe || st == 0xfd) continue;
        vec3_sub(V(apart), V(pos), V(other + 0x10));
        sw(apart + 8, 0);
        const float d = vec3_length(V(apart));
        if (d < 1.8f) {
            const float push = fpu_mul(fpu_sub(1.8f, d), fpu_add(fpu_mul(frame_scale(), -0.5f), 1.0f));
            vec3_scale(V(apart), V(apart), push);
            vec3_add(V(pos), V(pos), V(apart));
        }
    }
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x18))) != 0) sb(m + 0x20, 2);
    swc1(m + 0x40, angle_wrap_mod(lwc1(pv)));
    swc1(m + 0x44, angle_wrap_mod(lwc1(pv + 4)));
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    const float fall = fpu_mul(lwc1(0x0015ed70), 9.8f);
    const float fs = frame_scale();
    const uint32_t thrower = lwu(pv + 0x10);
    sw(thrower + 0x94, lwu(lwu(thrower + 0x24) + 0x10));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fall));
    swc1(m + 0x40, fpu_add(lwc1(m + 0x40), fpu_mul(fs, 0.01f)));
    swc1(m + 0x44, fpu_add(lwc1(m + 0x44), fpu_mul(fs, 0.02f)));
}

// The blob fading (state 2): it shrinks, its mark grows to its class's scale with it (while +0x1a); once smaller than a
// twentieth of a frame of its class's scale it goes (when its mark is gone, does not grow, or has grown to its scale).
void fade(uint32_t m, uint32_t pv) {
    const float k = fpu_mul(frame_scale(), 0.05f);
    {
        const float sc = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_sub(sc, fpu_mul(sc, k)));
    }
    {
        const uint32_t mark = lwu(pv + 0x14);
        if (mark != 0 && lbu(pv + 0x1a) != 0) {
            const float ms = lwc1(mark + 0x2c);
            swc1(mark + 0x2c, fpu_add(ms, fpu_mul(fpu_sub(class_scale(mark), ms), k)));
        }
    }
    if (!(lwc1(m + 0x2c) < fpu_mul(class_scale(m), fpu_mul(frame_scale(), 0.05f)))) return;
    swc1(m + 0x2c, u2f(0x38d1b717u));  // (1e-4)
    const uint32_t mark = lwu(pv + 0x14);
    bool gone = mark == 0 || lbu(pv + 0x1a) == 0;
    if (!gone) {
        const uint32_t st = lbu(mark + 0x20);
        gone = st == 0xfe || st == 0xfd || !(lwc1(mark + 0x2c) < fpu_sub(class_scale(mark), 0.01f));
    }
    if (gone) class1886_retire(m, pv);
}
}  // namespace

// orig: L9:0x0030a778 L13:0x0030d148
uint32_t class1886_leave_mark(uint32_t thrower, uint32_t pos) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t at = sp, out = sp + 0x10;
    lq_sq(at, pos);
    if (thrower == 0) {
        ee_stack_free(0x70);
        return 0;
    }
    const uint32_t tpv = lwu(thrower + 0x78);
    sw(out, 0);
    if (group_first(out, lw(tpv + 0x94), 1, 1) != 0) {
        debug_printf_stub();  // (the group has no free member: a debug message with the thrower's id)
        ee_stack_free(0x70);
        return 0;
    }
    sb(tpv + 0x91, lbu(tpv + 0x91) + 1);
    const uint32_t b = lwu(out);
    lq_sq(b + 0x10, at);
    sw(b + 0x1c, 0);
    sb(b + 0x20, 0);
    sb(b + 0xbc, 0);
    const uint32_t cls = lwu(b + 0x24);
    sh(b + 0x34, lhu(cls + 0x44));
    swc1(b + 0x2c, lwc1(cls + 0x24));
    sb(b + 0x30, 0xff);
    sh(b + 0x32, 0xff);
    sb(b + 0x31, 1);
    if (lwu(cls + 0x40) != 0) sh(b + 0x34, lhu(b + 0x34) | 0x10u);
    if (lbu(cls + 0xf) != 0) sh(b + 0x34, lhu(b + 0x34) | 0x400u);
    sh(b + 0x36, 0x7f80);
    sb(b + 0x71, 0xff);
    sb(b + 0x72, 0xff);
    sb(b + 0xa4, 0xff);
    if (lbu(tpv + 0x93) != 0) swc1(b + 0x2c, fpu_mul(lwc1(cls + 0x24), 0.1f));
    moby_set_anim(b, 0, 0);
    const uint32_t hook = class_hook_10(b);
    if (hook != 0) {
        sw(hook + 0x6c, 0);
        sw(hook + 0x94, 0);
        sh(hook + 0x68, 0);
    }
    // (the thrower's +0xb4 shared out: the mark takes at most what the thrower has, at least 1; the thrower keeps the
    // rest, at least 1)
    if (lh(thrower + 0xb4) < lh(b + 0xb4)) sh(b + 0xb4, lhu(thrower + 0xb4));
    if (lh(b + 0xb4) <= 0) sh(b + 0xb4, 1);
    {
        const uint32_t rest = lhu(thrower + 0xb4) - lhu(b + 0xb4);
        sh(thrower + 0xb4, rest);
        if (static_cast<int16_t>(rest) < 0) sh(thrower + 0xb4, 1);
    }
    sw(b + 0x94, lwu(lwu(b + 0x24) + 0x10));
    moby_copy_hero_color(b);
    moby_update_transform(b);
    const uint32_t word = moby_pvar_word0(b);
    if (word != 0) {
        sw(word + 0x34, 0);
        swc1(word, int_to_float(lh(word + 4)));
        sw(word + 0x30, 0);
    }
    ee_stack_free(0x70);
    return b;
}

// orig: L9:0x0030b350 L13:0x0030dd20
void class1886_retire(uint32_t m, uint32_t pv) {
    const uint32_t thrower = lwu(pv + 0x10);
    if (thrower != 0 && lh(thrower + 0xa6) == THROWER_CLASS) {
        const uint32_t st = lbu(thrower + 0x20);
        if (st != 0xfe && st != 0xfd) sw(lwu(thrower + 0x78) + (lbu(pv + 0x1c) << 2) + 0xb0, 0);
    }
    moby_retire(m);
}

// Every state then glows on the floor (the scale against its class's) and the blob goes when it is below 5 or above 500,
// else it is kept inside 5 .. 1018 on each axis. State 1 sets it on the floor, leaves the mark and fades.
// orig: L9:0x0030b3b0 L13:0x0030dd80
void class1886_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t from = sp, ahead = sp + 0x10, apart = sp + 0x20;
    countdown_u8(pv + 0x1b);
    lq_sq(from, m + 0x10);
    switch (lbu(m + 0x20)) {
    case 0:
        fly(m, pv, from, ahead, apart);
        break;
    case 1:
        swc1(m + 0x18, fpu_add(floor_probe_v(0.5f, V(m + 0x10), 0), 0.2f));
        sw(m + 0x1c, 0);
        lq_sq(ahead, m + 0x10);
        sw(pv + 0x14, class1886_leave_mark(lwu(pv + 0x10), ahead));
        sb(m + 0x20, 2);
        break;
    case 2:
        fade(m, pv);
        break;
    default:
        break;
    }
    floor_light_add(m, fpu_div(fpu_mul(lwc1(m + 0x2c), 0.3f), class_scale(m)));
    const float z = lwc1(m + 0x18);
    if (z < 5.0f || 500.0f < z) {
        class1886_retire(m, pv);
        ee_stack_free(0xe0);
        return;
    }
    for (uint32_t axis = 0x10; axis <= 0x18; axis += 4) {
        if (lwc1(m + axis) < 5.0f) swc1(m + axis, 5.0f);
        if (1018.0f < lwc1(m + axis)) swc1(m + axis, 1018.0f);
    }
    ee_stack_free(0xe0);
}

}  // namespace rac1
