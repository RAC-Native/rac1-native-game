// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1028, the bridge that unfolds once the hero has used
// the Hydrodisplacer on its pump (a scene: the screen fades, the camera watches the bridge's sections slide out and their
// side plates fold up, the checkpoint is taken), with its pieces' placement and the level's build of hero_enter_idle.
// Header blarg_own.h; registered in blarg_mobys.cpp. Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR (the
// bridge's state lives in the level's words 0x161d50 .. 0x161db0), the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/hydro.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 its position, +0x2080 its moby, +0x20a4 its movement mode)
const float kDegree = u2f(0x3c8efa35u);  // pi / 180
const float kHalfPi = u2f(0x3fc90fdbu);
const float kQuarterPi = u2f(0x3f490fdbu);

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// an angle kept within [0, pi / 2]
float fold_clamp(float a) {
    if (kHalfPi < a) return kHalfPi;
    if (a < 0.0f) return 0.0f;
    return a;
}

// The bridge's four lists of pieces (pvars): 20 sections and their 20 mates (class 0x405), 10 side plates and their 10
// mates (class 0x406).
constexpr uint32_t SECTIONS = 0x70, SECTIONS_B = 0xc0, PLATES = 0x110, PLATES_B = 0x138;

// The scene's fade (pvar +0x6c from -1 up to 1, 20 frames a unit), the screen's fade level 1 - |fade|. True on the frame
// the fade crosses 0 (the screen black).
bool scene_fade(uint32_t pv) {
    const float before = lwc1(pv + 0x6c);
    approach_f(pv + 0x6c, 1.0f, fpu_div(1.0f, static_cast<float>(frame_scale_i(0x14))));
    swc1(L6_ADDR(0x0015f3fc), fpu_sub(1.0f, abs_f(lwc1(pv + 0x6c))));
    return before < 0.0f && 0.0f <= lwc1(pv + 0x6c);
}
}  // namespace

// The idle state of the hero's movement mode (PLAYER+0x20a4): Blarg's build of Veldin's hero_enter_idle (0x227638), mode 0
// state 0, mode 1 state 0x43 (without Veldin's test for Clank), mode 3 state 0x53, mode 2 nothing.
// orig: L6:0x00239528
void hero_enter_idle_l6() {
    const uint32_t mode = lbu(PLAYER + 0x20a4);
    if (mode == 1) {
        hero_set_state(0x43, 1);
    } else if (mode == 0) {
        hero_set_state(0, 1);
    } else if (mode == 3) {
        hero_set_state(0x53, 1);
    }
}

// The bridge's pieces placed and turned from its state (the level's words): section i (0 .. 19) and its mate slid out
// along the bridge's axis (its matrix +0xc0) by the length 0x161d78 less i (0 at least), at the bridge's offsets 0x161d50 /
// 0x161d60, tipped about x by the angle 0x161d7c degrees less 45 i (within 0 .. 90 degrees: one way, the mate the other);
// side plate k (0 .. 9) and its mate slid out by 0x161d78 less 2k and that less 1 (0x161d70 + (4 - k) 0x161d74 at least),
// folded about x by 0x161d7c and about y by 0x161d80, less 45 degrees for each plate (2k, 2k + 1); a plate whose angle
// leaves 0 this frame (0x161da0 / 0x161da4: the angles' rates) starts its sound (1 about x, 2 about y).
// orig: L6:0x002f5d78
void bridge_place_pieces_l6(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t at = sp, slide = sp + 0x10, slide_b = sp + 0x20;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t axis = m + 0xc0, pos = m + 0x10;
    // the piece `piece` at the offset `offset` slid out by `by`
    auto put = [&](uint32_t piece, uint32_t offset, uint32_t by) {
        mat3_transform(V(at), V(offset), M(axis));
        vec3_add(V(at), V(at), V(pos));
        vec3_add(V(at), V(at), V(by));
        lq_sq(lwu(piece) + 0x10, at);
    };
    for (int32_t i = 0; i < 20; ++i) {
        const uint32_t a = pv + SECTIONS + static_cast<uint32_t>(i) * 4, b = pv + SECTIONS_B + static_cast<uint32_t>(i) * 4;
        float len = fpu_sub(lwc1(L6_ADDR(0x00161d78)), static_cast<float>(i));
        if (len < 0.0f) len = 0.0f;
        vec3_set_length(V(slide), V(axis), len);
        put(a, L6_ADDR(0x00161d50), slide);
        put(b, L6_ADDR(0x00161d60), slide);
        const float tip = fold_clamp(fpu_sub(fpu_mul(lwc1(L6_ADDR(0x00161d7c)), kDegree),
                                             fpu_mul(static_cast<float>(i * 45), kDegree)));
        swc1(lwu(a) + 0x40, -tip);
        swc1(lwu(b) + 0x40, tip);
        moby_update_transform(lwu(a));
        moby_update_transform(lwu(b));
    }
    for (int32_t k = 0; k < 10; ++k) {
        const uint32_t a = pv + PLATES + static_cast<uint32_t>(k) * 4, b = pv + PLATES_B + static_cast<uint32_t>(k) * 4;
        const float kf = static_cast<float>(k);
        const float least = fpu_add(lwc1(L6_ADDR(0x00161d70)), fpu_mul(static_cast<float>(4 - k), lwc1(L6_ADDR(0x00161d74))));
        float len = fpu_sub(lwc1(L6_ADDR(0x00161d78)), fpu_add(kf, kf));
        float len_b = fpu_sub(len, 1.0f);
        if (len < least) len = least;
        if (len_b < least) len_b = least;
        vec3_set_length(V(slide), V(axis), len);
        vec3_set_length(V(slide_b), V(axis), len_b);
        put(a, L6_ADDR(0x00161d50), slide);
        put(b, L6_ADDR(0x00161d60), slide_b);
        const float plate = static_cast<float>(2 * k);
        // the fold about x
        float fold = fpu_sub(fpu_mul(lwc1(L6_ADDR(0x00161d7c)), kDegree), fpu_mul(plate, kQuarterPi));
        float before = fpu_sub(fold, fpu_mul(lwc1(L6_ADDR(0x00161da0)), kDegree));
        fold = fold_clamp(fold);
        before = fold_clamp(before);
        if (fold != 0.0f && before == 0.0f) moby_sound_start(1, 0, lwu(a));
        float fold_b = fpu_sub(fpu_sub(fpu_mul(lwc1(L6_ADDR(0x00161d7c)), kDegree), fpu_mul(plate, kQuarterPi)), kQuarterPi);
        before = fpu_sub(fold_b, fpu_mul(lwc1(L6_ADDR(0x00161da0)), kDegree));
        fold_b = fold_clamp(fold_b);
        before = fold_clamp(before);
        if (fold_b != 0.0f && before == 0.0f) moby_sound_start(1, 0, lwu(b));
        swc1(lwu(a) + 0x40, -fold);
        swc1(lwu(b) + 0x40, fold_b);
        // the fold about y
        fold = fpu_sub(fpu_mul(lwc1(L6_ADDR(0x00161d80)), kDegree), fpu_mul(plate, kQuarterPi));
        before = fpu_sub(fold, fpu_mul(lwc1(L6_ADDR(0x00161da4)), kDegree));
        fold = fold_clamp(fold);
        before = fold_clamp(before);
        if (fold != 0.0f && before == 0.0f) moby_sound_start(2, 0, lwu(a));
        fold_b = fpu_sub(fpu_sub(fpu_mul(lwc1(L6_ADDR(0x00161d80)), kDegree), fpu_mul(plate, kQuarterPi)), kQuarterPi);
        before = fpu_sub(fold, fpu_mul(lwc1(L6_ADDR(0x00161da4)), kDegree));  // (the original's: from the first plate's fold)
        fold_b = fold_clamp(fold_b);
        before = fold_clamp(before);
        if (fold_b != 0.0f && before == 0.0f) moby_sound_start(2, 0, lwu(b));
        swc1(lwu(a) + 0x44, -fold);
        swc1(lwu(b) + 0x44, -fold_b);
        moby_update_transform(lwu(a));
        moby_update_transform(lwu(b));
    }
    ee_stack_free(0x110);
}

// Class 1028, the bridge. State 0 spawns its 60 pieces where it stands (their colour +0x38 the hero moby's for the first
// of each pair, the bridge's for the mate) and goes to 1, or to 6 laid out in full (0x161d78 20, 0x161d7c / 0x161d80 1000)
// when its checkpoint (+0xb0) was taken on this level before. State 1 waits for the hero in movement mode 1 with the
// Hydrodisplacer (item 0x16) while no scene plays, then fades the screen out (state 3); at black the hero is put at his
// place in state 0x72 and the scripted camera at the box +0x60; faded in, the bridge's sound starts on its last plate
// (state 4). State 4 takes the checkpoint and lays the bridge out: 0x161d78 up to 20, then the folds 0x161d7c and 0x161d80
// up to 1000 (each starting when the one before passed 0x161dac / 0x161db0), the camera moving from the box +0x60 to +0x64
// as the bridge slides and on to +0x68 as it folds; the sound stops when the sections are out. Laid out, it fades out
// again (state 5): at black the hero idles and the Hydrodisplacer's mode ends, then a voice line (0x1776), a help text
// (0x177c) and the checkpoint's save (a level change request to the same level).
// orig: L6:0x002f55a0
void blg_class1028_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x120);
    switch (lbu(m + 0x20)) {
    case 0: {
        if (lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) == 0xff) {
            sb(m + 0x20, 6);
            swc1(L6_ADDR(0x00161d78), 20.0f);
            swc1(L6_ADDR(0x00161d80), 1000.0f);
            swc1(L6_ADDR(0x00161d7c), 1000.0f);
        } else {
            sb(m + 0x20, 1);
        }
        // a piece of class `cls` into the list word `slot`, its colour from `colour_of`
        auto spawn = [&](uint32_t cls, uint32_t slot, uint32_t colour_of) {
            const uint32_t p = moby_spawn(cls);
            sw(slot, p);
            sh(p + 0x32, 0x40);
            sb(lwu(slot) + 0x31, 1);
            const uint32_t src = colour_of == 0 ? lwu(PLAYER + 0x2080) : colour_of;
            sd(lwu(slot) + 0x38, ld(src + 0x38));
            sh(lwu(slot) + 0x34, lhu(m + 0x34));
            lq_sq(lwu(slot) + 0x10, m + 0x10);
            lq_sq(lwu(slot) + 0x40, m + 0x40);
        };
        for (uint32_t i = 0; i < 20; ++i) {
            spawn(0x405, pv + SECTIONS + i * 4, 0);
            spawn(0x405, pv + SECTIONS_B + i * 4, m);
        }
        for (uint32_t k = 0; k < 10; ++k) {
            spawn(0x406, pv + PLATES + k * 4, 0);
            spawn(0x406, pv + PLATES_B + k * 4, m);
        }
        bridge_place_pieces_l6(m);
        break;
    }
    case 1:
        if (lbu(PLAYER + 0x20a4) == 1 && lbu(0x0013d4c0 + 0x16) != 0 && lw(L6_ADDR(0x0015f5c4)) == 0) {
            swc1(L6_ADDR(0x0015f3fc), 1.0f);
            sb(m + 0x20, 3);
            swc1(pv + 0x6c, u2f(0xbc23d70au));  // -0.01
        }
        break;
    case 3:
        if (scene_fade(pv)) {
            hero_place(PLAYER + 0x80, PLAYER + 0x90, 0x72, 1);
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x60));
            camera_scripted_set(box + 0x30, box + 0x70, 1, 0, 0);
            break;
        }
        if (1.0f <= lwc1(pv + 0x6c)) {
            sb(m + 0x20, 4);
            sb(m + 0xbc, static_cast<uint32_t>(moby_sound_start(0, 4, lwu(pv + PLATES + 0x24))));
        }
        break;
    case 4: {
        checkpoint_mark_collected(lbu(m + 0xb0));
        // the bridge's value `v` seeking `goal` (its rate `rate`, the acceleration `accel` and top speed `top` of the level)
        auto seek = [](uint32_t v, uint32_t rate, float goal, uint32_t accel, uint32_t top) {
            const float a = fpu_mul(lwc1(accel), dt_sq());
            seek_1d(v, rate, goal, a, a, fpu_mul(lwc1(top), dt()));
        };
        if (lwc1(L6_ADDR(0x00161d78)) < 20.0f)
            seek(L6_ADDR(0x00161d78), L6_ADDR(0x00161d9c), 20.0f, L6_ADDR(0x00161d90), L6_ADDR(0x00161d84));
        if (lwc1(L6_ADDR(0x00161dac)) < lwc1(L6_ADDR(0x00161d78)) && lwc1(L6_ADDR(0x00161d7c)) < 1000.0f)
            seek(L6_ADDR(0x00161d7c), L6_ADDR(0x00161da0), 1000.0f, L6_ADDR(0x00161d94), L6_ADDR(0x00161d88));
        if (lwc1(L6_ADDR(0x00161db0)) < lwc1(L6_ADDR(0x00161d7c)) && lwc1(L6_ADDR(0x00161d80)) < 1000.0f)
            seek(L6_ADDR(0x00161d80), L6_ADDR(0x00161da4), 1000.0f, L6_ADDR(0x00161d98), L6_ADDR(0x00161d8c));
        if (1000.0f <= lwc1(L6_ADDR(0x00161d80))) {
            sb(m + 0x20, 5);
            swc1(pv + 0x6c, -1.0f);
        }
        if (20.0f <= lwc1(L6_ADDR(0x00161d78))) {
            const uint32_t owner = lwu(pv + PLATES + 0x24);
            const int32_t slot = lbu(m + 0xbc);
            if (sub_0028d8c0(owner, slot) != 0) {
                blarg_sound_stop(owner, slot);
                sb(m + 0xbc, 0xff);
            }
        }
        bridge_place_pieces_l6(m);
        // the scripted camera between two boxes: from +0x60 to +0x64 while the sections slide, from +0x64 to +0x68 while
        // the plates fold
        const uint32_t cam_at = sp, cam_rot = sp + 0x10, from = sp + 0x20, from_rot = sp + 0x30, to = sp + 0x40,
                       to_rot = sp + 0x50;
        int32_t box_from, box_to;
        float t;
        if (lwc1(L6_ADDR(0x00161d78)) < 20.0f) {
            t = fpu_div(lwc1(L6_ADDR(0x00161d78)), 20.0f);
            box_from = lw(pv + 0x60);
            box_to = lw(pv + 0x64);
        } else {
            box_from = lw(pv + 0x64);
            t = fpu_div(lwc1(L6_ADDR(0x00161d80)), 1000.0f);
            box_to = lw(pv + 0x68);
        }
        lq_sq(from, lvd::box(lvd::BOXES, box_from) + 0x30);
        lq_sq(to, lvd::box(lvd::BOXES, box_to) + 0x30);
        lq_sq(from_rot, lvd::box(lvd::BOXES, box_from) + 0x70);
        lq_sq(to_rot, lvd::box(lvd::BOXES, box_to) + 0x70);
        vec3_lerp(V(cam_at), V(from), V(to), t);
        for (uint32_t k = 0; k < 12; k += 4) {
            const float turn = fpu_mul(angle_sub(lwc1(to_rot + k), lwc1(from_rot + k)), t);
            swc1(cam_rot + k, turn);
            swc1(cam_rot + k, angle_add(lwc1(from_rot + k), turn));
        }
        uid5_set_pvar_80(cam_at);
        uid5_set_pvar_90(cam_rot);
        break;
    }
    case 5:
        if (scene_fade(pv)) {
            hero_enter_idle_l6();
            hydro_2eac18(3);
            break;
        }
        if (1.0f <= lwc1(pv + 0x6c)) {
            sb(m + 0x20, 6);
            sub_00203908(0x1776, 0x2d);
            sub_00263d40(TXT(0x177c), -1);
            level_change_request(0, -1);
        }
        break;
    default:
        break;
    }
    ee_stack_free(0x120);
}

}  // namespace rac1
