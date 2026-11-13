// Class 1134, the gold bolt: one port of the copies Novalis .. Veldin's finale carry (the same code relinked). Each level
// keeps its own words (the bolt's tunings: per level below); the rest is the engine's (OV: the camera 0x166dc0, the camera
// path list 0x1600ec, the level change's delay 0x15f640, the scene's flag 0x15f404) or the base ELF's (bare: the hero
// 0x13f350 and its position 0x13f3d0 / angles 0x13f3e0, the gold bolts taken 0x14bec0 (four a level), the level 0x15ed84,
// the frame scale 0x15ed6c). Where a level's copy calls its own build of a helper the port does too: Novalis's and
// Aridia's hero_mode_exit, Novalis's uid5_set_pvar_90 and moby_anim_time. Header shared_gold_bolt.h.
//
// The pvars: +0 the bolt's index (of the level's four; -1 none), +4 / +8 the camera path's two points (-1 none), +0xc the
// difficulty's skip (not 0: taken at once), +0x10 its place, +0x1c its yaw, +0x20 .. +0x5c the glitter (four colour
// phases, their rates, the timers, the sizes), +0x60 .. +0x68 the spin's rates, +0x6c the scene's timer.
#include <game/ovl/shared_gold_bolt.h>

#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/hydro.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position, +0x90 its angles)
constexpr float DEGREE = 0.0174532924f;  // (0x3c8efa35)

// the level's tunings: +0 the scale, +4 / +8 the bob's height and size, +0x10 .. +0x1c the spin's four rates (degrees a
// second), +0x2c / +0x30 the glitter's two colours, +0x38 / +0x3c its particle words, +0x40 the sparkles' chance,
// +0x44 .. +0x50 their speeds, +0x54 their life, +0x58 / +0x5c their colours, +0x60 / +0x64 their sizes
uint32_t tunings() {
    switch (ovl_reloc_level()) {
    case NOVALIS: return L1_ADDR(0x00161f10);
    case ARIDIA: return L2_ADDR(0x00161ecc);
    case KERWAN: return L3_ADDR(0x00161bec);
    case EUDORA: return L4_ADDR(0x00161ce0);
    case RILGAR: return L5_ADDR(0x00161f30);
    case BLARG: return L6_ADDR(0x0016223c);
    case UMBRIS: return L7_ADDR(0x00161bc0);
    case BATALIA: return L8_ADDR(0x001621f0);
    case GASPAR: return L9_ADDR(0x00161c24);
    case ORXON: return L10_ADDR(0x00161d70);
    case POKITARU: return L11_ADDR(0x00161e20);
    case HOVEN: return L12_ADDR(0x00161ce0);
    case GEMLIK: return L13_ADDR(0x00161d38);
    case OLTANIS: return L14_ADDR(0x00162004);
    case QUARTU: return L15_ADDR(0x00161ec4);
    case KALEBO: return L16_ADDR(0x00161c78);
    case VELDIN_ORBIT: return L17_ADDR(0x00161e3c);
    default: return L18_ADDR(0x0016210c);
    }
}

// the moby's animation time (Novalis's own build)
float anim_time_here(uint32_t m) { return ovl_reloc_level() == NOVALIS ? moby_anim_scalar_l1(m) : moby_anim_time(m); }
// the special mode left (Novalis's and Aridia's own build)
void hero_mode_exit_here() {
    if (ovl_reloc_level() == NOVALIS || ovl_reloc_level() == ARIDIA) hero_mode_exit_l1();
    else hero_mode_exit();
}
// the camera's angles set (Novalis's own build)
void uid5_set_pvar_90_here(uint32_t src) {
    if (ovl_reloc_level() == NOVALIS) nov_316e28_l1(src, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    else uid5_set_pvar_90(src);
}
// the bolt marked taken (the level's four bytes at 0x14bec0)
void mark_taken(uint32_t pv) { sb(0x0014bec0 + lwu(pv) + (lwu(0x0015ed84) << 2), 1); }
// the gold bolt's jingle and the level change's delay
void taken_jingle() {
    sw(OV(0x0015f640), static_cast<uint32_t>(frame_scale_i(0xb4)));
    sub_00263d40(0x53b3, -1);
}
// one spin rate (degrees a second) a frame
float rate(uint32_t t, uint32_t at) { return fpu_mul(fpu_mul(lwc1(t + at), DEGREE), lwc1(0x0015ed6c)); }
// the point the effects start from: in its spin the moby's first quadword scaled (by 1 / 1024), else its chain point 0
void effect_point(uint32_t m, uint32_t at) {
    if (lbu(m + 0x20) == 1) vec3_scale(V(at), V(m), u2f(0x3a800000u));
    else moby_chain_point_v(m, 0, V(at));
}
}  // namespace

// orig: L1:0x00308470 L2:0x002eb508 L3:0x002dd958 L4:0x002e2708 L5:0x0031a430 L6:0x00305b78 L7:0x0031bdc0 L8:0x00303b40 L9:0x00302ac0 L10:0x002de0b0 L11:0x0030bf00 L12:0x00300580 L13:0x00304120 L14:0x003003e8 L15:0x002e4f40 L16:0x002df9b8 L17:0x002e47c8 L18:0x002ed410 (at 0x20)
// orig: L2:0x002ea7d0 L6:0x002f4bf8 L15:0x002eba70 L17:0x002f1488 (at 0)
void glitter_pvars_init(uint32_t m, uint32_t at) {
    const uint32_t g = lwu(m + 0x78) + at;
    for (uint32_t k = 0; k < 4; ++k) swc1(g + k * 4, static_cast<float>(rand_mod(0xff)));
    swc1(g + 0x10, -1.0f);
    swc1(g + 0x14, -2.25f);
    swc1(g + 0x18, 1.25f);
    swc1(g + 0x1c, 2.5f);
    for (uint32_t k = 0; k < 4; ++k) sw(g + 0x20 + k * 4, static_cast<uint32_t>(frame_scale_i(static_cast<int32_t>(0x3f + 0x40 * k))));
    swc1(g + 0x3c, 2.5f);
    swc1(g + 0x38, 3.0f);
    swc1(g + 0x30, 2.5f);
    swc1(g + 0x34, 3.0f);
}

// orig: L1:0x00308550 L2:0x002eb5e8 L3:0x002dda38 L4:0x002e27e8 L5:0x0031a510 L6:0x00305c58 L7:0x0031bea0 L8:0x00303c20 L9:0x00302ba0 L10:0x002de190 L11:0x0030bfe0 L12:0x00300660 L13:0x00304200 L14:0x003004c8 L15:0x002e5020 L16:0x002dfa98 L17:0x002e48a8 L18:0x002ed4f0
void gold_bolt_glitter(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t at = sp, centre = sp + 0x10, step = sp + 0x20;
    float fade = 1.0f;
    if (lbu(m + 0x20) == 1) {
        vec3_scale(V(centre), V(m), u2f(0x3a800000u));
    } else {  // (in the scene: shrinking as its animation runs to 160)
        moby_chain_point_v(m, 0, V(centre));
        fade = fpu_mul(fpu_div(fpu_sub(160.0f, anim_time_here(m)), 160.0f), 16.0f);
        if (1.0f < fade) fade = 1.0f;
    }
    // the first a little toward the camera, each next 0.1 further along
    vec3_sub(V(at), V(OV(0x00166dc0)), V(centre));
    vec3_set_length(V(at), V(at), u2f(0xbe99999au));  // (-0.3)
    vec3_set_length(V(step), V(at), u2f(0x3dcccccdu));  // (0.1)
    vec3_add(V(at), V(at), V(centre));
    const uint32_t t = tunings();
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t phase = pv + 0x20 + k * 4, timer = pv + 0x40 + k * 4;
        const float p = fpu_add(lwc1(phase), lwc1(pv + 0x30 + k * 4));
        swc1(phase, p);
        if (255.0f <= p) swc1(phase, fpu_sub(p, 255.0f));
        else if (p <= 0.0f) swc1(phase, fpu_add(p, 255.0f));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer))) != 0) sw(timer, static_cast<uint32_t>(frame_scale_i(0xff)));
        const float done = fpu_div(int_to_float(frame_scale_i(0xff) - lw(timer)), static_cast<float>(frame_scale_i(0xff)));
        const uint32_t colour = rgba_lerp(lwu(t + 0x2c), lwu(t + 0x30), abs_f(fpu_sub(0.5f, done)));
        const uint32_t frame = static_cast<uint32_t>(float_to_int(lwc1(phase))) & 0xff;
        spawn_particle_3b(at, colour, frame, lwu(t + 0x3c), lwu(t + 0x38), 2, 0, fpu_mul(lwc1(pv + 0x50 + k * 4), fade));
        vec3_add(V(at), V(at), V(step));
    }
    ee_stack_free(0xe0);
}

// orig: L1:0x003087e0 L2:0x002eb878 L3:0x002ddcc8 L4:0x002e2a78 L5:0x0031a7a0 L6:0x00305ee8 L7:0x0031c130 L8:0x00303eb0 L9:0x00302e30 L10:0x002de420 L11:0x0030c270 L12:0x003008f0 L13:0x00304490 L14:0x00300758 L15:0x002e52b0 L16:0x002dfd28 L17:0x002e4b38 L18:0x002ed780
void gold_bolt_camera(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t pos = sp, ang = sp + 0x10, ang_a = sp + 0x20, ang_b = sp + 0x30;
    const float played = lbu(m + 0x52) == 1 ? anim_time_here(m) : 170.0f;
    const float k = fpu_div(played, 170.0f);
    const int32_t a = lw(pv + 4), b = lw(pv + 8);
    if (a == -1 || b == -1) {
        // 5 from the hero, 1 up, swinging round by 40 degrees as it plays, looking back at it, tilting down
        const float yaw = angle_add(fpu_sub(fpu_mul(k, u2f(0x3f32b8c2u)), u2f(0x3eb2b8c2u)), lwc1(HERO + 0x98));  // (40, 20 degrees)
        swc1(pos, fpu_mul(cos_vu(yaw), 5.0f));
        swc1(pos + 4, fpu_mul(sin_vu(yaw), 5.0f));
        sw(pos + 8, 0);
        vec3_add(V(pos), V(pos), V(HERO + 0x80));
        swc1(pos + 8, fpu_add(lwc1(pos + 8), 1.0f));
        vec_clear(V(ang));
        swc1(ang + 8, angle_add(yaw, u2f(0x40490fd0u)));  // (3.14159)
        swc1(ang + 4, fpu_mul(k, u2f(0xbe2e147bu)));        // (-0.17)
    } else {
        // between the camera path's two points (0x80 bytes each: +0x30 the place, +0x70 the angles)
        const uint32_t list = lwu(OV(0x001600ec));
        vec3_lerp(V(pos), V(list + (static_cast<uint32_t>(a) << 7) + 0x30), V(list + (static_cast<uint32_t>(b) << 7) + 0x30), k);
        lq_sq(ang_a, lwu(OV(0x001600ec)) + (lwu(pv + 4) << 7) + 0x70);
        lq_sq(ang_b, lwu(OV(0x001600ec)) + (lwu(pv + 8) << 7) + 0x70);
        for (uint32_t c : {8u, 4u, 0u})
            swc1(ang + c, angle_add(fpu_mul(angle_sub(lwc1(ang_b + c), lwc1(ang_a + c)), k), lwc1(ang_a + c)));
    }
    uid5_set_pvar_80(pos);
    uid5_set_pvar_90_here(ang);
    ee_stack_free(0x80);
}

// orig: L1:0x003089f0 L2:0x002eba88 L3:0x002dded8 L4:0x002e2c88 L5:0x0031a9b0 L6:0x003060f8 L7:0x0031c340 L8:0x003040c0 L9:0x00303040 L10:0x002de630 L11:0x0030c480 L12:0x00300b00 L13:0x003046a0 L14:0x00300968 L15:0x002e54c0 L16:0x002dff38 L17:0x002e4d48 L18:0x002ed990
void gold_bolt_sparkles(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t at = sp, vel = sp + 0x10, kick = sp + 0x20;
    const uint32_t t = tunings();
    effect_point(m, at);
    {
        const float dt = lwc1(0x0015ed6c);
        rand_sphere_point(vel, fpu_mul(lwc1(t + 0x44), dt), fpu_mul(lwc1(t + 0x48), dt));
    }
    {
        const uint32_t life = lbu(t + 0x54);
        const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xff;
        spawn_particle_3c(at, vel, lwu(t + 0x58), life, frame, 0, lwc1(t + 0x60));
    }
    for (int32_t k = 0; k < 3; ++k) {  // (three more, each its velocity kicked further)
        const float dt = lwc1(0x0015ed6c);
        rand_sphere_point(kick, fpu_mul(lwc1(t + 0x4c), dt), fpu_mul(lwc1(t + 0x50), dt));
        vec3_add(V(vel), V(vel), V(kick));
        const uint32_t life = lbu(t + 0x54);
        const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xff;
        spawn_particle_3c(at, vel, lwu(t + 0x5c), life, frame, 0, lwc1(t + 0x64));
    }
    ee_stack_free(0x80);
}

// orig: L1:0x00308380 L2:0x002eb418 L3:0x002dd868 L4:0x002e2618 L5:0x0031a340 L6:0x00305a88 L7:0x0031bcd0 L8:0x00303a50 L9:0x003029d0 L10:0x002ddfc0 L11:0x0030be10 L12:0x00300490 L13:0x00304030 L14:0x003002f8 L15:0x002e4e50 L16:0x002df8c8 L17:0x002e46d8 L18:0x002ed320
void gold_bolt_spin(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t t = tunings();
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(t)));
    if (lbu(m + 0x20) == 1) {
        const float bob = sin_vu(lwc1(m + 0x4c));
        swc1(m + 0x18, fpu_add(fpu_add(lwc1(pv + 0x18), lwc1(t + 4)), fpu_mul(lwc1(t + 8), bob)));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x68)));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x64)));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x60)));
        swc1(m + 0x4c, angle_add(lwc1(m + 0x4c), rate(t, 0x1c)));
    }
    gold_bolt_glitter(m);
    if (rand_mod(lw(t + 0x40)) == 0) gold_bolt_sparkles(m);
}

// The update: 0 set up (gone if no bolt or taken already), 1 spinning over its place until the hero (on foot or in the
// mode 3) comes within 3 (and 2 in height): taken at once, or its scene starts (2): the hero placed before it, the camera
// about them, the bolt at the hero's hand; when the animation ends, the level change to the tally.
// orig: L1:0x00307ca0 L2:0x002ead38 L3:0x002dd188 L4:0x002e1f38 L5:0x00319c60 L6:0x003053a8 L7:0x0031b5f0 L8:0x00303370 L9:0x003022f0 L10:0x002dd8e0 L11:0x0030b730 L12:0x002ffdb0 L13:0x00303950 L14:0x002ffc18 L15:0x002e4770 L16:0x002df1e8 L17:0x002e3ff8 L18:0x002ecc40
void gold_bolt_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t t = tunings();
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        const int32_t idx = lw(pv);
        if (idx == -1 || lbu(0x0014bec0 + static_cast<uint32_t>(idx) + (lwu(0x0015ed84) << 2)) != 0) {
            moby_retire(m);
            return;
        }
        lq_sq(pv + 0x10, m + 0x10);
        swc1(pv + 0x1c, lwc1(m + 0x48));
        swc1(m + 0x40, rand_angle());
        swc1(m + 0x44, rand_angle());
        swc1(m + 0x48, rand_angle());
        swc1(m + 0x4c, rand_angle());
        swc1(pv + 0x68, rate(t, 0x10));
        swc1(pv + 0x60, rate(t, 0x18));
        swc1(pv + 0x64, rate(t, 0x14));
        sb(m + 0x20, 1);
        glitter_pvars_init(m, 0x20);
        return;
    }
    if (state == 1) {
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(t)));
        const float bob = sin_vu(lwc1(m + 0x4c));
        swc1(m + 0x18, fpu_add(fpu_add(lwc1(pv + 0x18), lwc1(t + 4)), fpu_mul(lwc1(t + 8), bob)));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), rate(t, 0x10)));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), rate(t, 0x14)));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), rate(t, 0x18)));
        swc1(m + 0x4c, angle_add(lwc1(m + 0x4c), rate(t, 0x1c)));
        gold_bolt_glitter(m);
        if (rand_mod(lw(t + 0x40)) == 0) gold_bolt_sparkles(m);
        // taken: the hero within 3 (2 in height), in a state that may take it, on foot (0) or in the mode 3
        if (!(vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 3.0f)) return;
        if (!(abs_f(fpu_sub(lwc1(m + 0x18), lwc1(HERO + 0x88))) < 2.0f)) return;
        if (lwu(HERO + 0x22a8) == 0) return;
        const uint32_t mode = lbu(HERO + 0x20a4);
        if (mode != 3 && mode != 0) return;
        if (lh(pv + 0xc) != 0) {  // (taken at once)
            taken_jingle();
            mark_taken(pv);
            level_change_request(0, -1);
            moby_retire(m);
            return;
        }
        const uint32_t sp = ee_stack_alloc(0x100);
        const uint32_t place = sp, facing = sp + 0x10, cam = sp + 0x20, cam_ang = sp + 0x30;
        // the hero 2.5 before the bolt's place, facing it
        swc1(place, fpu_mul(cos_vu(lwc1(pv + 0x1c)), 2.5f));
        swc1(place + 4, fpu_mul(sin_vu(lwc1(pv + 0x1c)), 2.5f));
        sw(place + 8, 0);
        vec3_add(V(place), V(place), V(pv + 0x10));
        vec_clear(V(facing));
        swc1(facing + 8, angle_add(lwc1(pv + 0x1c), u2f(0x40490fdbu)));  // (pi)
        screen_fade(frame_scale_i(0xa));
        if (lbu(HERO + 0x20a4) == 3) hero_mode_exit_here();
        hero_place(place, facing, 0x72, 0);
        moby_sound_start(0, 0, m);
        // the camera 1.25 before and 4 to the side, 1 up, looking across
        const float half_pi = u2f(0x3fc90fdbu);
        swc1(cam, fpu_mul(cos_vu(lwc1(pv + 0x1c)), 1.25f));
        swc1(cam + 4, fpu_mul(sin_vu(lwc1(pv + 0x1c)), 1.25f));
        sw(cam + 8, 0);
        swc1(cam, fpu_add(lwc1(cam), fpu_mul(cos_vu(angle_add(lwc1(pv + 0x1c), half_pi)), 4.0f)));
        swc1(cam + 4, fpu_add(lwc1(cam + 4), fpu_mul(sin_vu(angle_add(lwc1(pv + 0x1c), half_pi)), 4.0f)));
        vec3_add(V(cam), V(cam), V(pv + 0x10));
        swc1(cam + 8, fpu_add(lwc1(cam + 8), 1.0f));
        vec_clear(V(cam_ang));
        swc1(cam_ang + 8, angle_sub(lwc1(pv + 0x1c), half_pi));
        camera_scripted_set(cam, cam_ang, 1, 0, 0);
        uid5_set_pvar_80(cam);
        uid5_set_pvar_90_here(cam_ang);
        ee_stack_free(0x100);
        lq_sq(m + 0x10, HERO + 0x80);
        lq_sq(m + 0x40, HERO + 0x90);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 0);
        hero_anim_set(0x82, 0, 0.0f);
        if (lbu(0x0015edb5) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000);
        sb(HERO + 0x20af, 1);
        sw(OV(0x0015f404), 1);
        sb(m + 0x20, 2);
        sw(pv + 0x6c, static_cast<uint32_t>(frame_scale_i(0x3c)));
        return;
    }
    if (state != 2) return;
    // the scene: the bolt at the hero's hand
    gold_bolt_camera(m);
    lq_sq(m + 0x10, HERO + 0x80);
    lq_sq(m + 0x40, HERO + 0x90);
    if (lbu(m + 0x52) == 1) {
        gold_bolt_spin(m);
        if (rand_mod(0xa) == 0) gold_bolt_sparkles(m);
        if ((lbu(m + 0x70) & 2) != 0) {  // (its animation played: hidden, a last burst of sparkles)
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
            sb(m + 0x31, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1);
            for (int32_t k = 0; k < 4; ++k) gold_bolt_sparkles(m);
        }
    }
    if ((lwu(HERO + 0xa98) & 2) != 0) hero_anim_set(0, 0, static_cast<float>(frame_scale_i(0x1e)));
    if (lbu(m + 0x52) != 0) return;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x6c))) == 0) return;
    taken_jingle();
    mark_taken(pv);
    sw(OV(0x0015f404), 0);
    hero_place(HERO + 0x80, HERO + 0x90, 0, 0);
    hydro_2eac18(0);
    level_change_request(0, -1);
    moby_retire(m);
}

}  // namespace rac1
