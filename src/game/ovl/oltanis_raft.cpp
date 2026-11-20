// Oltanis (level 14), class 712: the raft (Oltanis's build of the path-riding rafts, shared_floaters.h; Rilgar's class
// 998 in rilgar_ocean.cpp is its sibling). Its five stages (pvars +0xb4, the next +0xb6) are camera points (+0x80 ..
// +0x90: box indices); between them it rides the level's paths (+0x94 .. +0xa0). Waiting at a stage (state 1) with the
// hero aboard within its deck (1.2 across) it shows the stage's message and, with the button (0x13cae4 bit 0x10) while
// the level's word 0x15f594 is 4, sets off (3); the raft at the other end is told (2, hidden). Not aboard, the hero in
// one of the three activation boxes (+0x100 .. +0x108) moves it to that box's stage: in place during the level's first
// ten frames, else by a ride. It glides from the stage's point to the path's start (3), rides the path (4), glides from
// its end to the next stage's point (5) and waits there (1, the next stage); taking the button during the last glide it
// turns back (7: an eased glide from where it was). While riding (3 / 4) the hero is held to it, its path laid out and
// its sound (4, the slot +0xb2) kept; stopped, its sound stops. State 6 waits for one of its two slots (+0xb8, +0xbc)
// in the level's table of things done (0x14c050) and then comes back. Ported by hand from Oltanis's own code
// (L14:0x2f0538, 0x2f1140, 0x2f13f8), the original's order of calls and float operations kept.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_floaters.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350u;
float dt() { return lwc1(0x0015ed6cu); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70u); }  // its square

struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    uint32_t n, sp;
};

// the hero standing on the raft (his ground moby, +0x2fc) and on his feet (+0x30e)
bool hero_aboard(uint32_t m) { return lwu(HERO + 0x2fc) == m && lh(HERO + 0x30e) == 0; }

// The raft's next ride (Oltanis's build of Rilgar's raft_route): by its stage (+0xb4) the path to ride (the level's
// path at +0x94 / +0x98 / +0x9c / +0xa0, at +0xac) and the stage it ends at (+0xb6); stage 4 and past keep the path
// they have. Its progress (+0xc0, +0xc4) back to 0 and its pace (+0xd4 a frame, +0xd8 a frame squared) over the path's
// length (its first segment times its count).
// orig: L14:0x002f1140
void raft_route(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t stage = static_cast<uint32_t>(lh(pv + 0xb4));
    if (stage < 5) {
        uint32_t word = 0, ends = 0;
        bool set = true;
        switch (stage) {  // (the table 0x1fcba0)
        case 0: word = 0x94; ends = 1; break;  // (0x002f1178)
        case 1: word = 0x98; ends = 0; break;  // (0x002f119c)
        case 2: word = 0x9c; ends = 3; break;  // (0x002f11a4)
        case 3: word = 0xa0; ends = 0; break;  // (0x002f11c8)
        default: set = false; break;           // (0x002f11e8)
        }
        if (set) {
            const uint32_t path = lvd::path(lw(pv + word));
            sh(pv + 0xb6, ends);
            sw(pv + 0xac, path);
        }
    }
    const uint32_t path = lwu(pv + 0xac);
    const float len = fpu_mul(vec3_distance(V(path + 0x10), V(path + 0x20)), static_cast<float>(lw(path)));
    const float v = fpu_mul(dt(), 5.0f), a = fpu_mul(dt2(), 2.5f);
    sw(pv + 0xc4, 0);
    sw(pv + 0xc0, 0);
    const float inv = fpu_div(1.0f, len);
    swc1(pv + 0xd8, fpu_mul(a, inv));
    swc1(pv + 0xd4, fpu_mul(v, inv));
}

// The stage's message (as who 4): the stage the raft waits at (+0xb4; +0xb6 in state 5, the glide to it).
// orig: L14:0x002f13f8
void raft_message(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t stage = lbu(m + 0x20) == 5 ? lh(pv + 0xb6) : lh(pv + 0xb4);
    switch (static_cast<uint32_t>(stage)) {  // (the table 0x1fcbc0)
    case 0: hud_message_request(4, TXT(0x53ed)); break;                  // (0x002f143c)
    case 1: case 2: case 3: hud_message_request(4, TXT(0x53ee)); break;  // (0x002f1450)
    case 4: hud_message_request(4, TXT(0x1399)); break;                  // (0x002f1464)
    default: hud_message_request(4, TXT(0x139b)); break;                 // (0x002f1478)
    }
}

// The hero's place across the raft's deck (the raft's frame: its matrix +0xc0 transposed, at sp + mat; the offset at
// sp + 0x70, w 0): within 1.2 of its middle line.
bool on_deck(uint32_t m, uint32_t sp, uint32_t mat) {
    mat3_transpose(M(sp + mat), M(m + 0xc0));
    vec3_sub(V(sp + 0x70), V(0x0013f3d0u), V(m + 0x10));
    sw(sp + 0x7c, 0);
    mat4_transform(V(sp + 0x70), V(sp + 0x70), M(sp + mat));
    return abs_f(lwc1(sp + 0x74)) < u2f(0x3f99999au);
}

// The raft that serves the next stage (+0xb6) told to wait hidden there (state 2).
void hide_far_raft(uint32_t m, uint32_t pv) {
    const uint32_t far = floater_find_checked(m, lh(pv + 0xb6), 0xb4);
    if (far == 0) return;
    sb(far + 0x20, 2);
    sw(far + 0x94, 0);
    sh(far + 0x34, lhu(far + 0x34) | 1u);
    sb(far + 0x31, 0);
}
}  // namespace

// orig: L14:0x002f0538
void olt_class712_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x160);
    const uint32_t sp = f.sp;
    lq_sq(sp, m + 0x10);
    lq_sq(sp + 0x10, m + 0x40);
    talker_word_set(m, (lhu(m + 0x34) ^ 1u) & 1u);
    // the box (a camera point) of stage pvar word `stage_word`: its place and turn at sp + at / sp + at + 0x20
    auto stage_point = [&](uint32_t stage_word, uint32_t at) {
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x80 + (static_cast<uint32_t>(lh(pv + stage_word)) << 2)));
        lq_sq(sp + at, box + 0x30);
        lq_sq(sp + at + 0x20, box + 0x70);
    };
    // the ride point and turn (+0x60, +0x74 / +0x78) at `t` between sp + 0x30 (turn + 0x50) and sp + 0x40 (+ 0x60); the
    // progress +0xc0 at 1: done
    auto glide = [&](float t) {
        vec3_lerp(V(pv + 0x60), V(sp + 0x30), V(sp + 0x40), t);
        swc1(pv + 0x74, angle_add(fpu_mul(angle_sub(lwc1(sp + 0x64), lwc1(sp + 0x54)), t), lwc1(sp + 0x54)));
        swc1(pv + 0x78, angle_add(fpu_mul(angle_sub(lwc1(sp + 0x68), lwc1(sp + 0x58)), t), lwc1(sp + 0x58)));
        return 1.0f <= lwc1(pv + 0xc0);
    };
    // an activation box (pvar word `box_word`): the hero in it puts the raft at `stage` -- in place during the level's
    // first ten frames (waiting, its ride point that stage's), else by a ride from `from` (state 3)
    auto activate = [&](uint32_t box_word, uint32_t stage, uint32_t from) {
        if (!point_in_box(0x0013f3d0u, lw(pv + box_word))) return;
        if (lw(L14_ADDR(0x0015f5cc)) < frame_scale_i(10)) {
            const uint32_t at = lvd::box(lvd::BOXES, lw(pv + 0x80 + (stage << 2)));
            sh(pv + 0xb4, stage);
            sh(pv + 0xb6, stage);
            lq_sq(m + 0x10, at + 0x30);
            lq_sq(m + 0x40, at + 0x70);
            sb(m + 0x20, 1);
            const uint32_t now = lvd::box(lvd::BOXES, lw(pv + 0x80 + (static_cast<uint32_t>(lh(pv + 0xb4)) << 2)));
            lq_sq(pv + 0x60, now + 0x30);
            lq_sq(pv + 0x70, now + 0x70);
        } else {
            sh(pv + 0xb4, from);
            raft_route(m);
            sb(m + 0x20, 3);
        }
    };
    switch (lbu(m + 0x20)) {  // (the table 0x1fcb80)
    case 0:  // (0x002f05cc)
        sh(pv + 0xb2, 0xffff);
        sh(pv + 0xb0, 0xffff);
        floater_arrive_l14(m);
        break;
    case 1:  // (0x002f05e8) waits at its stage
        if (hero_aboard(m)) {
            if (!on_deck(m, sp, 0x30)) break;
            raft_route(m);
            raft_message(m);
            const uint32_t pad = lwu(0x0013cae4u) & 0x10u;
            const int32_t mode = lw(L14_ADDR(0x0015f594));
            if (pad == 0 || mode != 4) break;
            sb(m + 0x20, 3);
            hide_far_raft(m, pv);
            sh(pv + 0xb0, 0xffff);
            break;
        }
        // (0x002f06d8) not aboard: the activation boxes, each in turn (the stage one sets read by the next); a ride
        // from the first box starts at stage 3 when the raft is at 2, else at 1
        if (lw(pv + 0x100) >= 0 && lh(pv + 0xb4) != 0) activate(0x100, 0, lh(pv + 0xb4) == 2 ? 3u : 1u);
        if (lw(pv + 0x104) >= 0 && lh(pv + 0xb4) != 1) activate(0x104, 1, 0);
        if (lw(pv + 0x108) >= 0 && lh(pv + 0xb4) < 2) activate(0x108, 2, 2);
        break;
    case 3:  // (0x002f09b8) to the path's start
        if (hero_aboard(m)) cam0_set_distance(0, 7.0f, u2f(0x3b03126fu));
        seek_1d(pv + 0xc0, pv + 0xc4, 1.0f, dt2(), dt2(), fpu_mul(dt(), 0.5f));
        stage_point(0xb4, 0x30);
        path_point(0.0f, lwu(pv + 0xac), 0, sp + 0x40, sp + 0x60, 0);
        if (glide(lwc1(pv + 0xc0))) {
            swc1(pv + 0xc4, 0.0f);
            swc1(pv + 0xc0, 0.0f);
            sb(m + 0x20, 4);
        }
        break;
    case 7: {  // (0x002f0b0c) back to the path's start from where it turned, eased
        seek_1d(pv + 0xc0, pv + 0xc4, 1.0f, dt2(), dt2(), fpu_mul(dt(), 0.5f));
        lq_sq(sp + 0x30, pv + 0xe0);
        lq_sq(sp + 0x50, pv + 0xf0);
        path_point(0.0f, lwu(pv + 0xac), 0, sp + 0x40, sp + 0x60, 0);
        const float t = cubic_interp(1.0f, 0.0f, 1.0f, 0.0f, lwc1(pv + 0xc0));
        if (glide(t)) {
            swc1(pv + 0xc4, 0.0f);
            swc1(pv + 0xc0, 0.0f);
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4:  // (0x002f0c14) rides the path
        if (hero_aboard(m)) cam0_set_distance(0, 7.0f, u2f(0x3b449ba6u));
        if (floater_ride(m, 0xc0, 0xac) != 0) {
            sw(pv + 0xc0, 0);
            sw(pv + 0xc4, 0);
            sb(m + 0x20, 5);
        }
        break;
    case 5: {  // (0x002f0c70) from the path's end to the next stage's point
        seek_1d(pv + 0xc0, pv + 0xc4, 1.0f, dt2(), dt2(), fpu_mul(dt(), 0.25f));
        stage_point(0xb6, 0x40);
        const uint32_t path = lwu(pv + 0xac);
        path_point(static_cast<float>(lw(path)), path, 0, sp + 0x30, sp + 0x50, 0);
        if (glide(lwc1(pv + 0xc0))) {
            sw(pv + 0xc0, 0);
            sb(m + 0x20, 1);
            sw(pv + 0xc4, 0);
            floater_next_stage(m, 0xb4);
        }
        // (0x002f0d90) the button on the deck turns it back: from where it is now (kept over the arrival's reset)
        if (!hero_aboard(m) || !on_deck(m, sp, 0x80)) break;
        raft_message(m);
        if ((lwu(0x0013cae4u) & 0x10u) == 0) break;
        lq_sq(pv + 0xe0, pv + 0x60);
        lq_sq(pv + 0xf0, pv + 0x70);
        floater_arrive_l14(m);
        sh(pv + 0xb4, lhu(pv + 0xb6));
        raft_route(m);
        sb(m + 0x20, 7);
        lq_sq(pv + 0x60, pv + 0xe0);
        lq_sq(pv + 0x70, pv + 0xf0);
        hide_far_raft(m, pv);
        break;
    }
    case 6:  // (0x002f0ea4) gone until one of its slots is done
        if (lbu(0x0014c050u + lwu(pv + 0xb8) + (lwu(0x0015ed84u) << 4)) == 0xff ||
            lbu(0x0014c050u + lwu(pv + 0xbc) + (lwu(0x0015ed84u) << 4)) == 0xff) {
            const uint32_t anim = lwu(lwu(m + 0x24) + 0x10);
            const uint32_t flags = lhu(m + 0x34) & 0xfffeu;
            sb(m + 0x20, 1);
            sw(m + 0x94, anim);
            sh(m + 0x34, flags);
            sb(m + 0x31, 1);
        }
        break;
    default:  // (2: hidden at the far end, 0x002f0f10)
        break;
    }
    floater_bob(m, 0xc8);
    if (lbu(m + 0x20) - 3u < 2u) {  // (0x002f0f20) riding: the hero held, the path laid out, the sound kept
        if (hero_aboard(m)) {
            sh(0x0013f544u, 2);
            sh(HERO + 0x1f2, 2);
            floater_lay_path(m, 0xdc);
        }
        if (sub_0028d8c0(m, lh(pv + 0xb2)) == 0) sh(pv + 0xb2, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    } else if (sub_0028d8c0(m, lh(pv + 0xb2)) != 0) {  // (0x002f0f88) stopped: its sound stopped
        const int32_t slot = lh(pv + 0xb2);
        if (slot != -1) {
            const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
            if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
        }
        sh(pv + 0xb2, 0xffff);
    }
    vec3_sub(V(sp + 0x20), V(m + 0x10), V(sp));
    attach_frame_set(pv + 0x20, sp + 0x20, sp + 0x10, m + 0x40);
}

}  // namespace rac1
