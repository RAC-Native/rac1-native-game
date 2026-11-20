// Gemlik Base's (level 13) class 21: a lift the hero rides between two heights, with a scripted camera beside it for
// the ride. Level 13 only: the addresses are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's
// bare. Registered in gemlik_mobys.cpp.
//
// The moby: +0x18 its height, +0x20 the state (below), +0x48 its yaw, +0x90 its colour, +0xbc the state it takes after
// the camera's move (2 / 3) and whether the hero rides it (in the states 2 / 3: 0 no). The pvars: +0x20 the attach frame
// (attach_frame_set: what stands on it follows), +0x60 / +0x64 the two heights (the states 2 / 6 / 5 at +0x60, 3 / 7 /
// 4 at +0x64), +0x68 its vertical speed, +0x6c the ride's sound (-1 none), +0x70 / +0x74 the camera's move (0 .. 1) and
// its speed, +0x78 the colour's pulse angle, +0x7c armed (the hero has been off it since it stopped).
//
// States (the table L13:0x1f4250): 0 set up (to 3), 1 the camera moves to the ride's place, 2 / 3 ride to +0x60 /
// +0x64, 4 / 5 wait at +0x64 / +0x60 (the hero boards it, or calls it from the other height), 6 / 7 stopped at +0x60 /
// +0x64 (until the hero is off).
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his position, +0x2fc the moby he stands on, +0x30e in the air)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the hero stands on the lift, on the ground
bool hero_on(uint32_t m) { return lwu(P + 0x2fc) == m && lh(P + 0x30e) == 0; }
}  // namespace

// The ride's camera beside the lift: `pos` 2.5 (2.0 for class 21) from its centre across its yaw (to the left while it
// rides or waits at +0x64's side, to the right otherwise) and 1.5 (1.2) up, `rot` turned toward the hero's +0xd0.
// (The original returns the hero's address, which its caller never reads.)
// orig: L13:0x002b4060
void gml_2b4060_l13(uint32_t m, uint32_t pos, uint32_t rot) {
    const Frame f(96);
    float side = 2.5f, up = 1.5f;
    const uint32_t bc = lbu(m + 0xbc), st = lbu(m + 0x20);
    float yaw = lwc1(m + 0x48);
    const float kHalfPi = u2f(0x3fc90fdbu);
    if ((bc >= 2 && (bc & 1) != 0) || (st >= 2 && (st & 1) != 0)) yaw = angle_sub(yaw, kHalfPi);
    else yaw = angle_add(yaw, kHalfPi);
    if (lh(m + 0xa6) == 0x15) {
        side = 2.0f;
        up = u2f(0x3f99999au);
    }
    swc1(pos + 0, fpu_mul(cos_vu(yaw), side));
    swc1(pos + 8, up);
    swc1(pos + 4, fpu_mul(sin_vu(yaw), side));
    vec3_add(V(pos), V(pos), V(m + 0x10));
    vec_clear(V(rot));
    swc1(rot + 8, atan2_f(fpu_sub(lwc1(P + 0xd0), lwc1(pos + 0)), fpu_sub(lwc1(P + 0xd4), lwc1(pos + 4))));
    sw(rot + 4, 0);
}

// orig: L13:0x002b39e0
void gml_class21_update_l13(uint32_t m) {
    const Frame f(224);
    const uint32_t sp = f.sp, moved = sp, rot0 = sp + 0x10;
    const uint32_t pv = lwu(m + 0x78);
    vec3_scale(V(moved), V(m + 0x10), -1.0f);  // (its move this frame: the place before, subtracted at the end)
    lq_sq(rot0, m + 0x40);

    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4250)
    case 0: {  // (0x002b3a64)
        swc1(pv + 0x78, rand_angle());
        sw(pv + 0x6c, 0xffffffffu);
        sb(m + 0x20, 3);
        break;
    }
    case 1: {  // (0x002b3a84): the camera from the hero's to the ride's place, then the ride (+0xbc)
        const float step = fpu_mul(dt_sq(), 4.0f);
        const float before = lwc1(pv + 0x70);
        seek_1d(pv + 0x70, pv + 0x74, 1.0f, step, step, fpu_mul(dt(), 4.0f));
        const float now = lwc1(pv + 0x70);
        if (now < 1.0f) {
            const float left = fpu_div(fpu_sub(1.0f, now), fpu_sub(1.0f, before));  // (the part of the way still to go, kept from the camera's place)
            const uint32_t pos = sp + 0x30, ang = sp + 0x40, d = sp + 0x20;
            gml_2b4060_l13(m, pos, ang);
            vec3_sub(V(d), V(L13_ADDR(0x001670c0)), V(pos));
            vec3_scale(V(d), V(d), left);
            vec3_add(V(pos), V(pos), V(d));
            swc1(ang + 4, angle_add(fpu_mul(angle_sub(lwc1(L13_ADDR(0x001670d4)), lwc1(ang + 4)), left), lwc1(ang + 4)));
            swc1(ang + 8, angle_add(fpu_mul(angle_sub(lwc1(L13_ADDR(0x001670d8)), lwc1(ang + 8)), left), lwc1(ang + 8)));
            uid5_set_pvar_80(pos);
            uid5_set_pvar_90(ang);
        } else {
            const uint32_t next = lbu(m + 0xbc);
            sb(m + 0xbc, 1);
            sb(m + 0x20, next);
        }
        break;
    }
    case 2:
    case 3: {  // (0x002b3b9c): ride to +0x60 / +0x64
        if (sub_0028d8c0(m, lw(pv + 0x6c)) == 0) sw(pv + 0x6c, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        if (lbu(m + 0xbc) != 0) {
            gml_2b4060_l13(m, sp + 0x20, sp + 0x30);
            uid5_set_pvar_80(sp + 0x20);
            uid5_set_pvar_90(sp + 0x30);
        }
        const bool up = lbu(m + 0x20) == 2;
        const float to = up ? lwc1(pv + 0x60) : lwc1(pv + 0x64);
        const float acc = fpu_mul(dt_sq(), 8.0f);
        seek_1d(m + 0x18, pv + 0x68, to, acc, acc, fpu_mul(dt(), 40.0f));
        if (abs_f(fpu_sub(lwc1(m + 0x18), to)) == 0.0f) {
            sb(m + 0x20, up ? 6 : 7);
            if (lbu(m + 0xbc) != 0) {
                hero_set_state(0, 0);
                hydro_2eac18(1);
            }
        }
        break;
    }
    case 6:
    case 7: {  // (0x002b3ca0): stopped; waits again once the hero is off it (or off its centre)
        if (sub_0028d8c0(m, lw(pv + 0x6c)) != 0) {
            sub_0028d918(lw(pv + 0x6c));
            sw(pv + 0x6c, 0xffffffffu);
        }
        if (hero_on(m) && !(0.5f < vec2_distance(V(P + 0x80), V(m + 0x10)))) break;
        sb(pv + 0x7c, 0);
        sb(m + 0x20, lbu(m + 0x20) == 7 ? 5 : 4);
        break;
    }
    case 4:
    case 5: {  // (0x002b3d34): waits, its colour pulsing
        swc1(pv + 0x78, angle_add(lwc1(pv + 0x78), fpu_mul(dt(), u2f(0x40c90fdbu))));
        int32_t c = float_to_int(fpu_mul(fpu_sub(fpu_mul(sin_vu(lwc1(pv + 0x78)), 4.0f), 3.0f), 128.0f));
        if (c >= 129) c = 0x80;
        else if (c <= 0x1f) c = 0x20;
        const uint32_t cu = static_cast<uint32_t>(c);
        sw(m + 0x90, 0x80000000u | (cu << 16) | (cu << 8) | cu);
        if (!hero_on(m) || 1.1f < vec2_distance(V(0x0013f3d0), V(m + 0x10))) sb(pv + 0x7c, 1);
        if (lbu(pv + 0x7c) != 0 && hero_on(m) && vec2_distance(V(P + 0x80), V(m + 0x10)) < 0.5f) {  // boarded
            sw(m + 0x90, 0x80208020u);
            sb(m + 0xbc, lbu(m + 0x20) == 5 ? 2 : 3);
            sb(m + 0x20, 1);
            hero_set_state(0x72, 1);
            camera_scripted_set(L13_ADDR(0x001670c0), L13_ADDR(0x001670d0), 1, 0, 0);
            sw(pv + 0x70, 0);
            break;
        }
        // called from the other height: the hero 5 .. 32 away, within 2 of that height
        const bool at4 = lbu(m + 0x20) == 4;
        const float d = vec2_distance(V(m + 0x10), V(0x0013f3d0));
        if (abs_f(fpu_sub(lwc1(P + 0x2d8), lwc1(pv + (at4 ? 0x64 : 0x60)))) < 2.0f && d < 32.0f && 5.0f < d) {
            sb(m + 0xbc, 0);
            sb(m + 0x20, at4 ? 3 : 2);
            const float v = fpu_mul(dt(), 5.0f);
            swc1(pv + 0x68, at4 ? -v : v);
        }
        break;
    }
    default:
        break;
    }

    vec3_add(V(moved), V(moved), V(m + 0x10));
    attach_frame_set(pv + 0x20, moved, rot0, m + 0x40);
}

}  // namespace rac1
