// Pokitaru's (level 11) great gate (class 1156), ported by hand from the level's own code (L11:0x30c788 and its helper
// 0x30d1b0): eighteen parts (classes 1152 .. 1155) it spawns and lays out about itself, hidden until its lift (class
// 1179, the moby its pvar +0 names) reaches its state 2; then the scripted camera looks on while the parts swing open
// one after the other (the level's words 0x161ec4 .. 0x161ed8, their speeds 0x161ee0 ..), the camera glides from one
// box to the other (0x161edc), the hero is put by the lift and, once open, the camera is let go. Its progress is the
// level's word 0x15f3fc. The original's order of calls and float operations kept.
#include <game/ovl/pokitaru_own.h>

#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l11.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
const float kDeg = u2f(0x3c8efa35u);
const float kHalfPi = u2f(0x3fc90fdbu);
const float kSixthPi = u2f(0x3f060a92u);
// the level's words of the gate: its parts' lists and the opening's values
float word(uint32_t a) { return lwc1(a); }

// A part's place: its angles the gate's, turned by `yaw_sign` quarter turns, pitched by the level's 0x161ebc and
// rolled by `roll`; at 14 in front of the gate (raised by 0x161eac), out along its own up axis (0x161eb0) and the
// gate's side (`side` * 0x161eb4).
void place_part(uint32_t m, uint32_t p, float roll, float side) {
    lq_sq(p + 0x40, m + 0x40);
    swc1(p + 0x48, angle_add(lwc1(p + 0x48), fpu_mul(side, kHalfPi)));
    swc1(p + 0x44, fpu_mul(word(L11_ADDR(0x00161ebcu)), kDeg));
    swc1(p + 0x40, roll);
    euler_to_mat3_vu(M(p + 0xc0), V(p + 0x40));
    Vec4 front{}, at{}, out{};
    vec_clear(front);
    vec3_set_length(front, V(m + 0xc0), 14.0f);
    front.z = fpu_add(front.z, word(L11_ADDR(0x00161eacu)));
    vec3_add(front, front, V(m + 0x10));
    vec3_set_length(at, V(p + 0xe0), word(L11_ADDR(0x00161eb0u)));
    vec3_set_length(out, V(m + 0xd0), fpu_mul(side, word(L11_ADDR(0x00161eb4u))));
    vec3_add(at, at, out);
    vec3_add(at, at, front);
    V(p + 0x10) = at;
}

// The parts' four lists (6 of class 1152 at 0x1da710, 4 of 1153 at 0x161f20, 4 of 1154 at 0x161f30, 4 of 1155 at
// 0x161f10): each part spawned at the gate's place, the hero moby's colour words, hidden.
void spawn_parts(uint32_t m, uint32_t list, uint32_t cls, int32_t n, bool links) {
    for (int32_t k = 0; k < n; ++k) {
        const uint32_t slot = list + (static_cast<uint32_t>(k) << 2);
        sw(slot, moby_spawn(cls));
        sh(lwu(slot) + 0x32, 0x40);
        sb(lwu(slot) + 0x31, 1);
        const uint32_t hero = lwu(0x0013f350 + 0x2080);
        sw(lwu(slot) + 0x38, lwu(hero + 0x38));
        sw(lwu(slot) + 0x3c, lwu(hero + 0x3c));
        const uint32_t p = lwu(slot);
        lq_sq(p + 0x10, m + 0x10);
        lq_sq(p + 0x40, m + 0x40);
        if (links) {  // (class 1155: drawn with its pvar's link list +0x20, +0x5c bit 0)
            sh(p + 0x34, lhu(p + 0x34) | 0x20u);
            const uint32_t ppv = lwu(lwu(slot) + 0x78);
            const uint32_t flags = lwu(ppv + 0x5c);
            sw(ppv + 8, ppv + 0x20);
            sw(ppv + 0x5c, flags | 1u);
            sh(lwu(slot) + 0x34, lhu(lwu(slot) + 0x34) | 1u);
        } else {
            sh(p + 0x34, lhu(p + 0x34) | 1u);
        }
        sb(lwu(slot) + 0x31, 0);
        sw(lwu(slot) + 0x94, 0);
    }
}
// a list's parts shown (drawn, their class's model)
void show_parts(uint32_t list, int32_t n) {
    for (int32_t k = 0; k < n; ++k) {
        const uint32_t p = lwu(list + (static_cast<uint32_t>(k) << 2));
        sh(p + 0x34, lhu(p + 0x34) & 0xfffeu);
        sb(lwu(list + (static_cast<uint32_t>(k) << 2)) + 0x31, 1);
        const uint32_t q = lwu(list + (static_cast<uint32_t>(k) << 2));
        sw(q + 0x94, lwu(lwu(q + 0x24) + 0x10));
    }
}
// one of the opening's values toward 1 (`fast`: twice the rates)
void open_seek(uint32_t value, uint32_t speed, bool fast) {
    const float a = fast ? fpu_add(dt2(), dt2()) : dt2();
    const float v = fast ? fpu_add(dt(), dt()) : dt();
    seek_1d(value, speed, 1.0f, a, a, v);
}
}  // namespace

// The gate's eighteen parts laid out by the opening's values: the six of the first list in two rows of three (rolled by
// thirty degrees a column, opened by 0x161ec4), the four of the second (opened by 0x161ec4, turned by 0x161ec8 /
// 0x161ecc), the four of the third on the second's (turned by 0x161ed0 / 0x161ed4), the four of the last at the gate's
// sides (spread by 0x161ed8).
// orig: L11:0x0030d1b0
void pok_30d1b0_l11(uint32_t m) {
    for (int32_t i = 0; i < 6; ++i) {
        const float open = fpu_mul(fpu_add(fpu_mul(word(L11_ADDR(0x00161eb8u)), fpu_sub(1.0f, word(L11_ADDR(0x00161ec4u)))), 30.0f), kDeg);
        const float roll = fpu_sub(fpu_mul(int_to_float(ee_rem(i, 3)), kSixthPi), open);
        const float side = int_to_float(i < 3 ? -1 : 1);
        place_part(m, lwu(L11_ADDR(0x001da710u) + (static_cast<uint32_t>(i) << 2)), roll, side);
        moby_update_transform(lwu(L11_ADDR(0x001da710u) + (static_cast<uint32_t>(i) << 2)));
    }
    const float swing_to = u2f(0x3fb2b8c2u);  // (80 degrees)
    for (int32_t i = 0; i < 4; ++i) {
        const uint32_t p = lwu(L11_ADDR(0x00161f20u) + (static_cast<uint32_t>(i) << 2));
        const float open = fpu_mul(fpu_add(fpu_mul(word(L11_ADDR(0x00161eb8u)), fpu_sub(1.0f, word(L11_ADDR(0x00161ec4u)))), 15.0f), kDeg);
        const float roll = fpu_sub(fpu_mul(int_to_float(i - ((i + (i >> 31)) >> 1 << 1)), kSixthPi), open);
        const int32_t turn_sign = (i & 1) == 0 ? 1 : -1;
        place_part(m, p, roll, int_to_float(i < 2 ? -1 : 1));
        const float swing = angle_sub(fpu_mul(int_to_float(turn_sign), u2f(0x3e860a92u)), swing_to);
        const float by = turn_sign == -1 ? word(L11_ADDR(0x00161ec8u)) : word(L11_ADDR(0x00161eccu));
        const float roll2 = angle_add(fpu_mul(swing, by), swing_to);
        swc1(p + 0x40, angle_add(lwc1(p + 0x40), roll2));
        moby_update_transform(p);
    }
    for (int32_t i = 0; i < 4; ++i) {
        const uint32_t base = lwu(L11_ADDR(0x00161f20u) + (static_cast<uint32_t>(i) << 2));
        const uint32_t p = lwu(L11_ADDR(0x00161f30u) + (static_cast<uint32_t>(i) << 2));
        Vec4 up{}, out{};
        vec3_set_length(up, V(base + 0xe0), word(L11_ADDR(0x00161efcu)));
        vec3_set_length(out, V(base + 0xd0), word(L11_ADDR(0x00161f00u)));
        vec3_add(up, up, out);
        vec3_add(V(p + 0x10), V(base + 0x10), up);
        lq_sq(p + 0x40, base + 0x40);
        const float by = (i & 1) == 0 ? word(L11_ADDR(0x00161ed4u)) : word(L11_ADDR(0x00161ed0u));
        const float turn = fpu_mul(-fpu_mul(word(L11_ADDR(0x00161f04u)), kDeg), by);
        swc1(p + 0x44, angle_add(lwc1(p + 0x44), turn));
        moby_update_transform(p);
    }
    for (int32_t i = 0; i < 4; ++i) {
        const float sign = i < 2 ? -1.0f : 1.0f;
        float spread = fpu_add(fpu_add(fpu_mul(int_to_float(i & 1), 7.5f), 3.75f), fpu_mul(fpu_sub(1.0f, word(L11_ADDR(0x00161ed8u))), 13.25f));
        spread = 20.0f < spread ? fpu_mul(20.0f, sign) : fpu_mul(spread, sign);
        Vec4 at{};
        vec3_set_length(at, V(m + 0xc0), fpu_add(spread, 14.0f));
        const uint32_t p = lwu(L11_ADDR(0x00161f10u) + (static_cast<uint32_t>(i) << 2));
        at.z = word(L11_ADDR(0x00161f08u));
        vec3_add(V(p + 0x10), at, V(m + 0x10));
        lq_sq(p + 0x40, m + 0x40);
        moby_update_transform(p);
    }
}

// Class 1156: state 0 hides it, clears the opening's values, spawns and lays out its parts (1); 1 waits for its lift
// (class 1179, pvar +0) in its state 2; 2 fades its progress (pvar +0xc, the level's 0x15f3fc) in, then puts the hero
// by the lift facing the gate, starts the scripted camera (the boxes pvar +4 / +8), shows the parts and its sound (3);
// 3 opens the parts one after the other (a sound as each starts), lays them out, keeps its progress at 1 (or back to 0
// once all the parts but the last are open and 0x13d3de, set when the hero pressed a button, is not), glides the camera
// from the first box to the second; when the progress is full, all open: the camera let go (state 4, the hero put back
// where he stands, 0x13d3de set); 4 fades the progress out (5).
// orig: L11:0x0030c788
void pok_class1156_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b480)
    case 0: {  // (0x0030c7e4)
        sb(m + 0x31, 0);
        sw(m + 0x94, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        sw(L11_ADDR(0x00161ec4u), 0);
        sw(L11_ADDR(0x00161ec8u), 0);
        sw(L11_ADDR(0x00161eccu), 0);
        sw(L11_ADDR(0x00161ed0u), 0);
        sw(L11_ADDR(0x00161ed4u), 0);
        sw(L11_ADDR(0x00161ed8u), 0);
        sw(L11_ADDR(0x00161edcu), 0);
        sb(m + 0xbc, 0);
        spawn_parts(m, L11_ADDR(0x001da710u), 0x480, 6, false);
        spawn_parts(m, L11_ADDR(0x00161f20u), 0x481, 4, false);
        spawn_parts(m, L11_ADDR(0x00161f30u), 0x482, 4, false);
        spawn_parts(m, L11_ADDR(0x00161f10u), 0x483, 4, true);
        pok_30d1b0_l11(m);
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x0030ca80)
        if (lw(pv) == -1) break;
        if (moby_is_in_state(lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv) << 8), 0x49b, 2) != 0) sb(m + 0x20, 2);
        break;
    case 2: {  // (0x0030caac)
        approach_f(pv + 0xc, 1.0f, fpu_mul(dt(), 4.0f));
        const float progress = lwc1(pv + 0xc);
        swc1(L11_ADDR(0x0015f3fcu), progress);
        if (!(1.0f <= progress)) break;
        sb(m + 0x20, 3);
        const uint32_t sp = ee_stack_alloc(0x20);
        const uint32_t at = sp, ang = sp + 0x10;
        lq_sq(at, lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv) << 8) + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), 5.0f));
        swc1(at + 8, floor_probe_v(0.5f, V(at), 0));
        vec_clear(V(ang));
        swc1(ang + 8, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(at)), fpu_sub(lwc1(m + 0x14), lwc1(at + 4))));
        hero_place(at, ang, 0x72, 1);
        ee_stack_free(0x20);
        camera_scripted_set(lvd::box(lvd::BOXES, lw(pv + 4)) + 0x30, lvd::box(lvd::BOXES, lw(pv + 8)) + 0x70, 1, 0, 0);
        show_parts(L11_ADDR(0x001da710u), 6);
        show_parts(L11_ADDR(0x00161f20u), 4);
        show_parts(L11_ADDR(0x00161f30u), 4);
        show_parts(L11_ADDR(0x00161f10u), 4);
        moby_sound_start(0, 0, m);
        break;
    }
    case 3: {  // (0x0030ccec)
        const uint32_t ec4 = L11_ADDR(0x00161ec4u), ec8 = L11_ADDR(0x00161ec8u), ecc = L11_ADDR(0x00161eccu);
        const uint32_t ed0 = L11_ADDR(0x00161ed0u), ed4 = L11_ADDR(0x00161ed4u), ed8 = L11_ADDR(0x00161ed8u);
        const float nine_tenths = u2f(0x3f666666u);
        if (word(ec4) < 1.0f) open_seek(ec4, L11_ADDR(0x00161ee0u), false);
        if (nine_tenths < word(ec4) && word(ec8) == 0.0f) moby_sound_start(1, 0, m);
        if (nine_tenths < word(ec4) && word(ec8) < 1.0f) open_seek(ec8, L11_ADDR(0x00161ee4u), true);
        if (0.5f < word(ec8) && word(ecc) == 0.0f) moby_sound_start(1, 0, m);
        if (0.5f < word(ec8) && word(ecc) < 1.0f) open_seek(ecc, L11_ADDR(0x00161ee8u), true);
        if (1.0f <= word(ec8) && word(ed0) < 1.0f) open_seek(ed0, L11_ADDR(0x00161eecu), true);
        if (1.0f <= word(ecc) && word(ed4) < 1.0f) open_seek(ed4, L11_ADDR(0x00161ef0u), true);
        if (nine_tenths < word(ed4) && word(ed8) < 1.0f)
            seek_1d(ed8, L11_ADDR(0x00161ef4u), 1.0f, fpu_add(dt2(), dt2()), fpu_mul(dt2(), 4.0f), fpu_add(dt(), dt()));
        pok_30d1b0_l11(m);
        if (lbu(0x0013d3de) != 0 && lw(0x0013cae4) != 0) sb(m + 0xbc, 1);
        approach_f(pv + 0xc, word(ed8) < 1.0f && lbu(m + 0xbc) == 0 ? 0.0f : 1.0f, fpu_mul(dt(), 4.0f));
        swc1(L11_ADDR(0x0015f3fcu), lwc1(pv + 0xc));
        const float glide = u2f(0x3e4ccccdu);  // (0.2)
        seek_1d(L11_ADDR(0x00161edcu), L11_ADDR(0x00161ef8u), 1.0f, fpu_mul(dt2(), glide), fpu_mul(dt2(), glide), fpu_mul(dt(), glide));
        const uint32_t sp = ee_stack_alloc(0x20);
        const uint32_t from = lvd::box(lvd::BOXES, lw(pv + 4)), to = lvd::box(lvd::BOXES, lw(pv + 8));
        vec3_lerp(V(sp), V(from + 0x30), V(to + 0x30), word(L11_ADDR(0x00161edcu)));
        vec3_lerp(V(sp + 0x10), V(from + 0x70), V(to + 0x70), word(L11_ADDR(0x00161edcu)));
        uid5_set_pvar_80(sp);
        uid5_set_pvar_90(sp + 0x10);
        ee_stack_free(0x20);
        if (!(1.0f <= lwc1(pv + 0xc))) break;
        swc1(ed8, 1.0f);
        swc1(ec4, 1.0f);
        swc1(ec8, 1.0f);
        swc1(ecc, 1.0f);
        swc1(ed0, 1.0f);
        swc1(ed4, 1.0f);
        pok_30d1b0_l11(m);
        sb(m + 0x20, 4);
        hydro_2eac18(3);
        hero_place(0x0013f3d0, 0x0013f3e0, 0, 1);
        sb(0x0013d3de, 1);
        break;
    }
    case 4: {  // (0x0030d140)
        approach_f(pv + 0xc, 0.0f, fpu_mul(dt(), 4.0f));
        const float progress = lwc1(pv + 0xc);
        swc1(L11_ADDR(0x0015f3fcu), progress);
        if (progress == 0.0f) sb(m + 0x20, 5);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
