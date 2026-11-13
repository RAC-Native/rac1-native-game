// Small helpers several levels carry copies of (task F5): one port each, proved on every copy by batch 508. The copies are
// the same code relinked (compared with the relocated fields masked, and normalised); the constants they differ by are
// parameters (or a wrapper per copy). What they name is the engine's (OV: the class table 0x197300, the camera entries
// 0x15ef50, the active camera 0x166e00, the level's paths 0x1b04b0, particle 0x33's word 0x1b214c) or a level's own data
// (Kerwan's rates: below). Header shared_little.h.
#include <game/ovl/shared_little.h>

#include <game/math/vecmath.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/helmet_tr.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l3.h>

namespace rac1 {

namespace {

int32_t carry_hook(uint32_t which, uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    if (which == 0) return carry_hook0(m, a1, a2, f12);
    if (which == 4) return carry_hook4(m, a1, a2, f12);
    return carry_hook8(m, a1, a2, f12);
}

// the camera entry `i`'s record
uint32_t cam_entry_record(int32_t i) { return lwu(lwu(OV(0x0015ef50)) + (static_cast<uint32_t>(i) << 5) + 0x1c); }

// class `cls`'s animation set `set`
uint32_t anim_set(uint32_t cls, uint32_t set) { return lwu(lwu(OV(0x00197300) + (cls << 2)) + (set << 2) + 0x48); }

// Turned about by the level's rates (degrees a second at `rates` +4, +0xc, +0x10, with +0 and +8) in the pvars at `at`
// (+4 / +0 with the first, +8 / +0xc with the others).
void kerwan_turn(uint32_t m, uint32_t rates, uint32_t at) {
    const uint32_t pv = lwu(m + 0x78);
    const float deg = u2f(0x3c8efa35u);
    const float r1 = fpu_mul(fpu_mul(lwc1(rates + 4), deg), lwc1(0x0015ed6c));
    sub_00262b00(m, pv + at + 4, pv + at, lwc1(rates), r1);
    const float r2 = fpu_mul(lwc1(rates + 0xc), deg);
    const float r3 = fpu_mul(lwc1(rates + 0x10), deg);
    const float dt = lwc1(0x0015ed6c);
    sub_00262b80(m, pv + at + 8, pv + at + 0xc, lwc1(rates + 8), fpu_mul(r2, dt), fpu_mul(r3, dt));
}

}  // namespace

// orig: L3:0x002c6c60 L11:0x0030f270 L15:0x002d9570 L17:0x002d7778
uint32_t moby_is_in_state(uint32_t m, int32_t cls, uint32_t state) {
    return lh(m + 0xa6) == cls && lbu(m + 0x20) == state ? 1u : 0u;
}

// orig: L3:0x002c5b78 L3:0x002c6d58
uint32_t moby_alive_in_state(uint32_t m, int32_t cls, uint32_t state) {
    if (m == 0) return 0;
    const uint32_t st = lbu(m + 0x20);
    if (st == 0xfe || st == 0xfd) return 0;
    return lh(m + 0xa6) == cls && st == state ? 1u : 0u;
}

// orig: L5:0x002899f8 L8:0x00269c70
uint32_t moby_largest(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t best = a;
    for (const uint32_t o : {b, c, d}) {
        if (o == 0) continue;
        if (best == 0 || !(lwc1(o + 0x2c) < lwc1(best + 0x2c))) best = o;
    }
    return best;
}

// orig: L14:0x002b3330 L15:0x00297118
void moby_show_default(uint32_t m) {
    const uint32_t flags = lhu(m + 0x34) & 0xffbeu;
    sh(m + 0x34, flags);
    sh(m + 0x34, flags | 0x1000u);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
}

// orig: L6:0x002e6148 L7:0x00302208
void moby_rest_sparkles(uint32_t m) {
    sb(m + 0x20, 2);
    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    helmet_init_fx(m);
}

// orig: L10:0x002f5a50 L13:0x00316f20 L14:0x00314168 L15:0x002f78b8
void cam_entry_mark(int32_t i) {
    const uint32_t r = cam_entry_record(i);
    sb(r + 0x38, 1);
    sb(r + 0x39, 0);
}

// orig: L13:0x00316f48 L14:0x00314190
void cam_entry_mark_type19(int32_t i) {
    const uint32_t r = cam_entry_record(i);
    if (lh(lwu(OV(0x00166e00)) + 0x86) == 0x13) sb(r + 0x39, 1);
}

// orig: L2:0x002f8aa0 L6:0x00312b40 L17:0x002fe2c8
void camera_angles_set(float a, float b, float c, float d, float e, float f) {
    const uint32_t blk = lwu(lwu(OV(0x00166e00)) + 0x70);
    swc1(blk + 0x58, f);
    swc1(blk + 0x50, e);
    swc1(blk + 0x54, d);
    swc1(blk + 0x18, c);
    swc1(blk + 0x10, b);
    swc1(blk + 0x14, a);
}

// orig: L4:0x00241910 L15:0x0023e4c8 L17:0x00249618
float class_anim_last_time(uint32_t cls, uint32_t set) {
    if (!(set < 0xff)) return 0.0f;
    const uint32_t s = anim_set(cls, set);
    const uint32_t frame = lwu(s + ((lbu(s + 0x10) - 1u) << 2) + 0x1c);
    return fpu_mul(int_to_float(lh(frame + 4)), 0.0625f);
}

// orig: L4:0x002418b0 L17:0x002495b8
float class_anim_first_time(uint32_t cls, uint32_t set) {
    if (!(set < 0xff)) return 0.0f;
    const uint32_t frame = lwu(anim_set(cls, set) + 0x1c);
    return fpu_mul(int_to_float(lh(frame + 4)), 0.0625f);
}

// orig: L7:0x0029a2a0 L13:0x0027d8d0 L14:0x00279aa0
uint32_t particle_33_spawn(uint32_t pos, uint32_t vel) {
    const uint32_t p = alloc_particle(0x33);
    if (p == 0) return 0;
    lq_sq(p + 0x10, pos);
    lq_sq(p + 0x20, vel);
    sw(p + 4, 0x5e806040u);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    sb(p + 8, 0);
    sh(p + 0xa, 0x78);
    sb(p + 2, lbu(lwu(OV(0x001b214c))));
    swc1(p + 0xc, 6300.0f);
    return p;
}

// orig: L3:0x002d5208 L7:0x00305a38 L15:0x002dee78
uint32_t class848_spawn(uint32_t from, uint32_t pos, uint32_t a, uint32_t b, float f) {
    const uint32_t r = moby_spawn(0x350);
    if (r == 0) return 0;
    const uint32_t pv = lwu(r + 0x78);
    sh(r + 0x32, 0x7e);
    sb(r + 0x30, 0xff);
    sb(r + 0x20, 1);
    sb(r + 0x31, 1);
    lq_sq(r + 0x10, pos);
    lq_sq(pv, from);
    swc1(pv + 0xc, f);
    sw(pv + 0x14, b);
    sw(pv + 0x10, a);
    sw(pv + 0x18, 0);
    return r;
}

// orig: L11:0x00316210 L11:0x00316270
int32_t carry_keep_state_hook(uint32_t m, uint32_t a1, uint32_t a2, float f12, uint32_t which) {
    const int32_t r = carry_hook(which, m, a1, a2, f12);
    const uint32_t st = lbu(m + 0x20);
    if (r != 0) {
        if (st != 0x14) {
            sb(m + 0x20, 0x14);
            sb(m + 0xbc, st);
        }
    } else if (st == 0x14) {
        sb(m + 0x20, lbu(m + 0xbc));
    }
    return r;
}

// orig: L17:0x002efea8 L17:0x002eff30 L17:0x002effb8
int32_t carry_drop_hook(uint32_t m, uint32_t a1, uint32_t a2, float f12, uint32_t which) {
    const int32_t r = carry_hook(which, m, a1, a2, f12);
    if (r != 0) {
        sb(m + 0x20, 0xc);
        return r;
    }
    if (lbu(m + 0x20) != 0xc) return r;
    const bool on_7 = lbu(m + 0x53) == 7;
    sb(m + 0x20, 6);
    if (!on_7) moby_anim_set(m, 7, 0, frame_scale_i(0xa));
    return r;
}

// orig: L3:0x002d3c40
void kerwan_class816_turn(uint32_t m) { kerwan_turn(m, L3_ADDR(0x00161af0), 0xe8); }

// orig: L3:0x002dd0f0
void kerwan_class1012_turn(uint32_t m) { kerwan_turn(m, L3_ADDR(0x00161bd8), 0xa8); }

// orig: L6:0x002fb4b0 L6:0x002fb870 L6:0x002fbc30
void blarg_class1051_switch(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    hero_set_state(0, 0);
    const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0xec) << 2));
    sw(path, lwu(pv + 0x114));
    hydro_2eac18(2);
}

// orig: L0:0x00262360 L10:0x002554e0
void path_point_even(uint32_t path, uint32_t index, uint32_t frac, uint32_t out, float dist, float leg) {
    const int32_t i = float_to_int(fpu_div(dist, leg));
    sw(index, static_cast<uint32_t>(i));
    const int32_t last = lw(path) - 1;
    if (!(i < last) || i < 0) {
        sw(index, static_cast<uint32_t>(i < last ? 0 : last));
        sw(frac, 0);
        lq_sq(out, path + 0x10 + (lwu(index) << 4));
        return;
    }
    const uint32_t sp = ee_stack_alloc(0x60);
    swc1(frac, fpu_sub(dist, fpu_mul(int_to_float(i), leg)));
    const uint32_t at = path + 0x10 + (lwu(index) << 4);
    vec3_sub(V(sp), V(at + 0x10), V(at));
    lq_sq(out, sp);
    vec3_set_length(V(out), V(out), lwc1(frac));
    vec3_add(V(sp), V(out), V(path + 0x10 + (lwu(index) << 4)));
    lq_sq(out, sp);
    ee_stack_free(0x60);
}

// orig: L10:0x002cc1b8 L15:0x002dd600
uint32_t class819_launch(uint32_t vel, uint32_t pos, uint32_t a, uint32_t b, int32_t frames, float scale) {
    const uint32_t r = moby_spawn(0x333);
    if (r == 0) return 0;
    swc1(r + 0x2c, fpu_mul(lwc1(r + 0x2c), scale));
    sb(r + 0x30, 0xff);
    sh(r + 0x32, 0x7e);
    sb(r + 0x20, 1);
    sb(r + 0x31, 1);
    const uint32_t pv = lwu(r + 0x78);
    lq_sq(r + 0x10, pos);
    lq_sq(pv, vel);
    const int32_t life = frame_scale_i(frames);
    sw(pv + 0x10, a);
    sw(pv + 0x14, b);
    sw(pv + 0x28, 0);
    sw(pv + 0x18, static_cast<uint32_t>(life));
    const float dist = vec3_distance(V(r + 0x10), V(0x0013f3d0));
    sw(pv + 0x30, 0);
    sw(pv + 0x34, 0);
    swc1(pv + 0x38, dist);
    swc1(r + 0x48, atan2_f(lwc1(pv), lwc1(pv + 4)));
    swc1(r + 0x44, -atan2_f(vec2_length(V(pv)), lwc1(pv + 8)));
    moby_update_transform(r);
    return r;
}

// orig: L17:0x002f54d8 L17:0x002f57f0 L17:0x002f5b08 L17:0x002f5e20 L17:0x002f6450
void envmap_uvs4(int32_t n, uint32_t normals, uint32_t points, uint32_t /*unused*/, uint32_t out) {
    const uint32_t sp = ee_stack_alloc(0x50);
    const uint32_t eye = sp, at = sp + 0x10, r = sp + 0x20, normal = sp + 0x30;
    for (int32_t i = 0; i < n; ++i, out += 8) {
        const uint32_t k = static_cast<uint32_t>(i) * 0xc;
        swc1(at + 0xc, 1.0f);
        swc1(at, lwc1(points + k));
        swc1(at + 4, lwc1(points + k + 4));
        swc1(at + 8, lwc1(points + k + 8));
        vec3_sub(V(eye), V(at), V(OV(0x00166dc0)));
        vec3_set_length(V(eye), V(eye), 1.0f);
        swc1(normal + 0xc, 1.0f);
        swc1(normal, lwc1(normals + k));
        swc1(normal + 8, lwc1(normals + k + 8));
        swc1(normal + 4, lwc1(normals + k + 4));
        vec3_set_length(V(normal), V(normal), 1.0f);
        const float dot = vec3_dot(V(normal), V(eye));
        vec3_scale(V(r), V(normal), fpu_add(dot, dot));
        vec3_sub(V(r), V(eye), V(r));
        vec3_set_length(V(r), V(r), 1.0f);
        swc1(r + 8, fpu_add(lwc1(r + 8), 1.0f));
        const float len = vec3_length(V(r));
        const float twice = fpu_add(len, len);
        const float v = fpu_add(fpu_div(lwc1(r + 4), twice), 0.5f);
        const float u = fpu_add(fpu_div(lwc1(r), twice), 0.5f);
        swc1(out + 4, fpu_mul(v, 4.0f));
        swc1(out, fpu_mul(u, 4.0f));
    }
    ee_stack_free(0x50);
}

// orig: L14:0x002fc890 L14:0x002fd808
void path_mover_start(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x68) << 2));
    for (int32_t i = 0; i < lw(path) - 1; ++i) {
        const uint32_t at = path + 0x10 + static_cast<uint32_t>(i) * 0x10;
        swc1(at + 0xc, vec3_distance(V(at), V(at + 0x10)));
    }
    sw(pv + 0x60, 0);
    sw(pv + 0x64, 0);
    lq_sq(m + 0x10, path + 0x10);
    swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
    for (const uint32_t o : {0x80u, 0x78u, 0x74u, 0x70u, 0x88u, 0x84u}) swc1(pv + o, 0.0f);
}

}  // namespace rac1
