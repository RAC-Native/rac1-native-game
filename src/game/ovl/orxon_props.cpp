// Orxon's own moby classes (level 10): the props. Class 794, a box that follows (or leads) another moby and pushes the
// mobys in its way aside, until its watched mobys reach their states; class 1047, a wall that breaks under a hit (rocks,
// two explosions) and stays broken; class 1067, the panel of shuffled lights whose draw callback (L10:0x002da690,
// orxon_effects.cpp) it queues while it is on screen; class 1346, a launcher that fires shells at up to three mobys;
// classes 1855 / 1856, crates that break when hit. Headers orxon_mobys.h / orxon_classes.h, registered in
// orxon_mobys.cpp; level 10's own addresses (`orig: L10:`).
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/orxon_classes.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/shared_spawns.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

#include <vector>

namespace rac1 {

using namespace orxon;

// ---- class 794

// Class 794's push: unless its flags (pvar +8) have 0x100, the mobys (not dying) within the box's reach (its width
// pvar +0 by its depth +4, centred half the larger ahead of it) that touch the box - the nearest point of the box (with
// their radius +0x48 / +0x4c around it) within the moby's own radius +0x48 - pushed away from that point
// (push_without_direction, strength +0x50).
// orig: L10:0x002c9958
void orx_2c9958_l10(uint32_t m, uint32_t pv) {
    if ((lwu(pv + 8) & 0x100u) != 0) return;
    const uint32_t frame = ee_stack_alloc(0x1a0);
    const uint32_t turn = frame, centre = frame + 0x30, rel = frame + 0x40, local = frame + 0x50, back = frame + 0x60,
                   near = frame + 0xa0;
    euler_to_mat3_vu(M(turn), V(m + 0x40));
    const float width = lwc1(pv), depth = lwc1(pv + 4);
    const float reach = depth < width ? fpu_mul(width, 0.5f) : fpu_mul(depth, 0.5f);
    V(centre) = Vec4{};
    swc1(centre + 8, reach);
    mat3_transform(V(centre), V(centre), M(turn));
    vec3_add(V(centre), V(centre), V(m + 0x10));
    const int32_t n = mobys_in_sphere(centre, reach, 0x10, m, 0);
    // (the original copies the hit list onto its stack: the inner searches overwrite it)
    std::vector<uint32_t> found;
    for (int32_t k = 0; k < n; ++k) found.push_back(coll_hit_list_at(k));
    for (const uint32_t o : found) {
        if (o == 0) continue;
        const uint32_t st = lbu(o + 0x20);
        if (st == 0xfe || st == 0xfd) continue;
        vec3_sub(V(rel), V(o + 0x10), V(m + 0x10));
        mat3_transpose(M(back), M(turn));
        mat4_transform(V(local), V(rel), M(back));
        lq_sq(near, local);
        {  // the nearest point of the box: x within the half width, z within the half depth (each with the margin)
            const float w = lwc1(pv), half = fpu_mul(w, 0.5f);
            const float lim = fpu_add(half, lwc1(pv + 0x48));
            const float x = lwc1(near);
            if (x < -lim) swc1(near, fpu_mul(-w, 0.5f));
            else if (lim < x) swc1(near, half);
        }
        {
            const float d = lwc1(pv + 4), half = fpu_mul(d, 0.5f);
            const float lim = fpu_add(half, lwc1(pv + 0x4c));
            const float z = lwc1(near + 8);
            if (z < -lim) swc1(near + 8, fpu_mul(-d, 0.5f));
            else if (lim < z) swc1(near + 8, half);
        }
        sw(near + 4, 0);
        mat3_transform(V(near), V(near), M(turn));
        vec3_add(V(near), V(near), V(m + 0x10));
        const int32_t touching = mobys_in_sphere(near, lwc1(pv + 0x48), 0x10, m, 0);
        for (int32_t j = 0; j < touching; ++j)
            if (coll_hit_list_at(j) == o) push_without_direction(o, m, 0x10001, near, lwc1(pv + 0x50));
    }
    ee_stack_free(0x1a0);
}

// Class 794: tied to a moby (pvar +0x10; -1 none) and watching up to five (+0x18, 8 bytes each: the index, its class +4,
// the state it must reach +6, -1 any). State 0 learns the classes and its partner's class (+0x14) and alpha (+0x44),
// copies its place to / from the partner and its turn to / from it (flags +8: 0x10 / 0x40, 0x20 / 0x80), sizes itself
// (the larger of +0 / +4 times the class's scale) and waits (1); 1, once no watched moby (alive, of its class, not
// flagged) is short of its state, puts its partner in state +0x16 (-1: none) and fades (2) over +0xc frames (+0x40)
// - the partner's alpha too unless flag 2 - until the partner reaches state +0x17 (unless flag 4) or the time is out
// (unless flag 8): 3 removes it (and the partner unless flag 1). States 1 and 2 push mobys aside.
// orig: L10:0x002c9c70
void orx_class794_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t partner = lw(pv + 0x10) != -1 ? moby_at(lw(pv + 0x10)) : 0;
    const uint32_t watch = pv + 0x18;
    const uint32_t state = lbu(m + 0x20);
    int32_t waiting = 0;  // the watched mobys still short of their states
    if ((lbu(m + 0xb0) == 0xff || gone_byte(m) != 0xff) && state != 0) {
        const uint32_t words = 0x0014c190 + (level_number() << 8);
        for (int32_t k = 0; k < 5; ++k) {
            const uint32_t w = watch + static_cast<uint32_t>(k) * 8;
            if (lw(w) == -1) continue;
            const uint32_t o = moby_at(lw(w));
            if (o == 0) continue;
            if (lh(o + 0xa6) != lh(w + 4)) continue;
            const uint32_t st = lbu(o + 0x20);
            if (st == 0xfe || st == 0xfd) continue;
            const int32_t idx = lh(o + 0xb2);
            const uint32_t flags = lwu(words + (static_cast<uint32_t>(idx >> 5) << 2));
            if (((static_cast<int32_t>(flags) >> (lhu(o + 0xb2) & 0x1f)) & 1) != 0) continue;
            const int32_t want = static_cast<int8_t>(lbu(w + 6));
            if (want == -1 || static_cast<int32_t>(lbu(o + 0x20)) != want) ++waiting;
        }
    }
    switch (state) {  // (the table at L10:0x001dce30)
    case 0: {  // (0x002c9df4)
        if (partner == 0) {
            debug_printf_stub();
        } else {
            sh(pv + 0x14, lhu(partner + 0xa6));
            swc1(pv + 0x44, int_to_float(lbu(partner + 0x23)));
            if ((lwu(pv + 8) & 0x10u) != 0) lq_sq(m + 0x10, partner + 0x10);
            else if ((lwu(pv + 8) & 0x40u) != 0) lq_sq(partner + 0x10, m + 0x10);
            if ((lwu(pv + 8) & 0x20u) != 0) lq_sq(m + 0x40, partner + 0x40);
            else if ((lwu(pv + 8) & 0x80u) != 0) lq_sq(partner + 0x40, m + 0x40);
        }
        for (int32_t k = 0; k < 5; ++k) {  // (the original prints each watched moby)
            const uint32_t w = watch + static_cast<uint32_t>(k) * 8;
            if (lw(w) != -1) sh(w + 4, lhu(moby_at(lw(w)) + 0xa6));
            debug_printf_stub();
        }
        const float width = lwc1(pv), depth = lwc1(pv + 4);
        const float scale = lwc1(lwu(m + 0x24) + 0x24);
        swc1(m + 0x2c, width < depth ? fpu_mul(depth, scale) : fpu_mul(width, scale));
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x002c9f48)
        if (waiting == 0) {
            if (partner != 0 && static_cast<int8_t>(lbu(pv + 0x16)) != -1) sb(partner + 0x20, lbu(pv + 0x16));
            sw(pv + 0x40, lwu(pv + 0xc));
            sb(m + 0x20, 2);
        }
        orx_2c9958_l10(m, pv);
        break;
    case 2: {  // (0x002c9f7c)
        if (partner != 0 && (lwu(pv + 8) & 2u) == 0) {
            const float left = int_to_float(lw(pv + 0x40));
            const float all = int_to_float(lw(pv + 0xc));
            sb(partner + 0x23, static_cast<uint32_t>(float_to_int(fpu_div(fpu_mul(lwc1(pv + 0x44), left), all))));
        }
        if ((lwu(pv + 8) & 4u) == 0 && partner != 0) {
            const int32_t want = static_cast<int8_t>(lbu(pv + 0x17));
            if (want != -1 && static_cast<int32_t>(lbu(partner + 0x20)) == want) sb(m + 0x20, 3);
        }
        if ((lwu(pv + 8) & 8u) == 0 && tick32(pv + 0x40) != 0) sb(m + 0x20, 3);
        orx_2c9958_l10(m, pv);
        break;
    }
    case 3:  // (0x002ca028)
        if (partner != 0 && (lwu(pv + 8) & 1u) == 0) moby_retire(partner);
        moby_retire(m);
        break;
    default:
        break;
    }
}

// ---- class 1047

// Class 1047: a wall broken before (its one-shot flag) leaves its two pieces (classes 1122 and 1921 over it) and goes.
// Else a hit on it (mask 0x40000) breaks it: an explosion at it, its sound (set 0x99), 30 rocks (class 1806) thrown
// out of its back face, a second explosion, the two pieces, its flag set, and it goes.
// orig: L10:0x002d9eb8
void orx_class1047_update_l10(uint32_t m) {
    if (flag_set(m)) {
        spawn_over_at_59(m, 0x462);
        spawn_over_at_59(m, 0x781);
        moby_retire(m);
        return;
    }
    if (sub_0025a420(m, 0x40000, 0) == 0) return;
    const uint32_t frame = ee_stack_alloc(0x140);
    const uint32_t none = frame + 0x20, pos = frame + 0x30, vel = frame + 0x40;
    V(none) = Vec4{};
    explosion_effect(m, none, m + 0x10, 5, 3, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);
    moby_sound_play(0, 0, m, 0x99);
    for (int32_t k = 0; k < 30; ++k) {
        V(vel) = Vec4{};
        swc1(vel, -1.0f);
        swc1(vel + 4, rand_range_f(-3.0f, 3.0f));
        swc1(vel + 8, rand_range_f(-3.0f, 3.0f));
        lq_sq(pos, vel);
        mat3_transform(V(pos), V(pos), M(m + 0xc0));
        vec3_set_length(V(vel), V(pos), fpu_mul(rand_range_f(5.0f, 10.0f), dt()));
        vec3_add_inplace(V(pos), V(m + 0x10));
        const uint32_t cls = rand_mod(1) == 0 ? 0x70eu : 0x70fu;
        const float scale = rand_range_f(0.5f, 1.0f);
        const int32_t life = rand_range(0x3c, 0x78);
        const float gravity = rand_range_f(2.0f, 3.0f);
        const uint32_t fade = rand_mod(4) == 0 ? 1u : 0u;
        rock_spawn(pos, vel, cls, life, fade, scale, gravity, 1.0f, 3.0f);
    }
    // the second explosion at the wall's offset (2.86, 1.57, 1.831) turned with it (not moved to its place)
    V(pos) = Vec4{u2f(0x40370a3du), u2f(0x3fc8f5c3u), u2f(0x3fea5e35u), 0.0f};
    mat3_transform(V(pos), V(pos), M(m + 0xc0));
    explosion_effect(m, none, pos, 5, 3, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);
    spawn_over_at_59(m, 0x462);
    spawn_over_at_59(m, 0x781);
    flag_mark(m);
    moby_retire(m);
    ee_stack_free(0x140);
}

// ---- class 1100

// Class 1100: a prop knocked out from one side. Gone at once once its one-shot flag is set; else its +0x32 set to 0x80 and
// each of the frame's 64 hit records naming it with kind 0x20000 is checked: one from a moby (+0x20) of class 0xac or 0x99
// lying more than 0.25 on its negative x side (in its own frame) sets its flag and it goes; any other is cleared.
// orig: L10:0x002dd650
void orx_class1100_update_l10(uint32_t m) {
    if (flag_set(m)) {
        moby_retire(m);
        return;
    }
    sh(m + 0x32, 0x80);
    for (int32_t k = 0; k < 64; ++k) {
        const uint32_t e = coll_contact(k);
        if (lwu(e + 0x34) != m || (lwu(e + 0x24) & 0x20000u) == 0) continue;
        const uint32_t src = lwu(e + 0x20);
        const int32_t cls = lh(src + 0xa6);
        if (cls == 0xac || cls == 0x99) {
            Vec4 rel;
            vec3_sub(rel, V(m + 0x10), V(src + 0x10));
            Mat4 to_local;
            mat3_transpose(to_local, M(m + 0xc0));
            mat4_transform(rel, rel, to_local);
            if (0.25f < rel.x) {
                flag_mark(m);
                moby_retire(m);
                return;
            }
        }
        sw(e + 0x34, 0);
    }
}

// ---- class 1067

namespace {
// the panel's centre (half its rows' height above its place: the row count +4 times the spacing +8) with the radius of
// its face (+0 its width) in w, at `at`: queued for its draw (L10:0x002da690) while within 80 of the camera and on screen
void draw_if_seen(uint32_t m, uint32_t pv, uint32_t at) {
    const float half_h = fpu_mul(fpu_mul(static_cast<float>(lw(pv + 4)), lwc1(pv + 8)), 0.5f);
    const float half_w = fpu_mul(lwc1(pv), 0.5f);
    lq_sq(at, m + 0x10);
    swc1(at + 8, fpu_add(lwc1(at + 8), half_h));
    swc1(at + 0xc, sqrt_f(fpu_add(fpu_mul(half_h, half_h), fpu_mul(half_w, half_w))));
}
}  // namespace

// Class 1067: (its pvar +0xc -1: printed and removed, the state still run.) State 0, gone at once in Clank's part on
// level 10; else its lights (+0x10, pvar +4 of them) dealt from the level's ten (L10:0x161ca8) shuffled, turned a
// quarter, faint (+0x30), its timer (+0x34) the level's seconds L10:0x161c98, hidden from the moby pass, on (1); 1 is
// drawn when seen and keeps its hum (slot +0x30) going; 2 stops the hum and is drawn when seen (its timer runs).
// orig: L10:0x002da2c8
void orx_class1067_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0xc) == -1) {
        debug_printf_stub();
        debug_printf_stub();
        moby_retire(m);
    }
    const uint32_t frame = ee_stack_alloc(0x70);
    switch (lbu(m + 0x20)) {
    case 0: {
        if (level_number() == 10 && lbu(HERO_FORM) == 1) {
            moby_retire(m);
            break;
        }
        const uint32_t deck = frame;  // (the ten lights, on the stack as the original has them)
        for (uint32_t i = 0; i < 10; ++i) sb(deck + i, lbu(L10_ADDR(0x00161ca8) + i));
        for (int32_t pass = 0; pass < 10; ++pass) {
            for (int32_t k = 0; k < rand_mod(9); ++k) {
                const uint32_t a = lbu(deck + static_cast<uint32_t>(k)), b = lbu(deck + static_cast<uint32_t>(k) + 1);
                sb(deck + static_cast<uint32_t>(k) + 1, a);
                sb(deck + static_cast<uint32_t>(k), b);
            }
        }
        for (int32_t i = 0; i < lw(pv + 4); ++i) sb(pv + 0x10 + static_cast<uint32_t>(i), lbu(deck + static_cast<uint32_t>(i)));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x3fc90fdbu)));
        sb(m + 0x20, 1);
        sb(m + 0x30, 0x60);
        sw(pv + 0x30, 0xffffffffu);
        sw(pv + 0x34, static_cast<uint32_t>(frame_scale_i(float_to_int(fpu_mul(lwc1(L10_ADDR(0x00161c98)), 60.0f)))));
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        break;
    }
    case 1: {
        const uint32_t at = frame + 0x10;
        draw_if_seen(m, pv, at);
        if (vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 80.0f && static_cast<int32_t>(point_on_screen(at, 80.0f)) >= 0)
            draw_callback_add(L10_ADDR(0x002da690), m);
        if (sub_0028d8c0(m, lw(pv + 0x30)) == 0) sw(pv + 0x30, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        break;
    }
    case 2: {
        tick32(pv + 0x34);
        sound_stop_owned(m, lw(pv + 0x30));
        const uint32_t at = frame;
        sw(pv + 0x30, 0xffffffffu);
        draw_if_seen(m, pv, at);
        if (vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 80.0f && static_cast<int32_t>(point_on_screen(at, 80.0f)) >= 0)
            draw_callback_add(L10_ADDR(0x002da690), m);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x70);
}

namespace {
// the moby at index `i` while it is alive (not 0xfe or 0xfd); none (-1, or a dead one): 0
uint32_t live_at(int32_t i) {
    if (i == -1) return 0;
    const uint32_t o = moby_at(i);
    if (o == 0) return 0;
    const uint32_t st = lbu(o + 0x20);
    return st == 0xfe || st == 0xfd ? 0u : o;
}
}  // namespace

// Class 1346: a launcher that fires at the mobys pvars +4 / +8 / +0xc in turn. 0: its one-shot flag set: done (3); its
// first target -1: gone; else waiting (1, its turn speed +0x10 cleared). 1 waits (another moby sets 2). 2: its flag
// set: done; else it turns (its back, 180 degrees about its heading) towards the first target still alive and, within
// 2.5 degrees, fires a shell from 1.5 behind it at 40 a second along the line to the target (the target crossed off);
// none left: done (3), the moby pvars +0x14 put in state 1; in both cases the moby pvars +0 given +0xbc = 1. 3: the
// targets still alive retired (the first one twice, as the original does) and its flag set.
// orig: L10:0x002e8ab0
void orx_class1346_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        if (flag_set(m)) {
            sb(m + 0x20, 3);
            break;
        }
        if (lw(pv + 4) == -1) {
            moby_retire(m);
            break;
        }
        sw(pv + 0x10, 0);
        sb(m + 0x20, 1);
        break;
    case 2: {
        if (flag_set(m)) {
            sb(m + 0x20, 3);
            break;
        }
        uint32_t target = live_at(lw(pv + 4));
        if (target == 0) target = live_at(lw(pv + 8));
        if (target == 0) target = live_at(lw(pv + 0xc));
        if (target != 0) {
            const float half_turn = u2f(0x40490fdbu);
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), half_turn));
            const float to = atan2_f(fpu_sub(lwc1(target + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x14), lwc1(m + 0x14)));
            const float k = fpu_mul(dt2(), u2f(0x3fc90fdbu)), damp = fpu_mul(dt2(), half_turn), vmax = fpu_mul(dt(), u2f(0x3f490fdbu));
            swc1(m + 0x48, angle_seek_rate(pv + 0x10, lwc1(m + 0x48), to, k, damp, vmax));
            const float now = atan2_f(fpu_sub(lwc1(target + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x14), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), now) < u2f(0x3d32b8c2u)) {  // aimed: a shell
                const uint32_t frame = ee_stack_alloc(0x30);
                const uint32_t vel = frame, at = frame + 0x10;
                swc1(at, fpu_mul(cos_vu(lwc1(m + 0x48)), 1.5f));
                const float y = fpu_mul(sin_vu(lwc1(m + 0x48)), 1.5f);
                sw(at + 8, 0);
                swc1(at + 4, y);
                vec3_add_inplace(V(at), V(m + 0x10));
                const float speed = fpu_mul(dt(), 40.0f);
                const float yaw = atan2_f(fpu_sub(lwc1(target + 0x10), lwc1(at)), fpu_sub(lwc1(target + 0x14), lwc1(at + 4)));
                const float d = vec2_distance(V(at), V(target + 0x10));
                const float pitch = atan2_f(d, fpu_sub(lwc1(target + 0x18), lwc1(at + 8)));
                vec_from_spherical_v(V(vel), speed, yaw, pitch);
                shell_launch(m, at, vel, frame_scale_i(0x1e));
                if (target == moby_at(lw(pv + 4))) sw(pv + 4, 0xffffffffu);
                else if (target == moby_at(lw(pv + 8))) sw(pv + 8, 0xffffffffu);
                else sw(pv + 0xc, 0xffffffffu);
                ee_stack_free(0x30);
            }
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), half_turn));
        } else {
            sb(m + 0x20, 3);
            if (lw(pv + 0x14) != -1) {
                sb(moby_at(lw(pv + 0x14)) + 0x20, 1);
                sw(pv + 0x14, 0xffffffffu);
            }
        }
        if (lw(pv) != -1) {
            sb(moby_at(lw(pv)) + 0xbc, 1);
            sw(pv, 0xffffffffu);
        }
        break;
    }
    case 3: {
        const uint32_t first = live_at(lw(pv + 4));
        uint32_t second = 0;
        if (first != 0) {
            moby_retire(first);
            second = first;
        } else {
            second = live_at(lw(pv + 8));
        }
        if (second != 0) moby_retire(second);
        flag_mark(m);
        break;
    }
    default:
        break;
    }
}

namespace {
// classes 1855 / 1856: started (1); hit (any hit of kind 0x10000 with a positive damage +0x2c): broken (2); 2 breaks it (its
// sound 0, its bolts, then `pieces`) and retires it
template <typename Pieces>
void crate_update(uint32_t m, Pieces pieces) {
    const uint32_t hit = sub_0025a420(m, 0x10000u, 0);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        break;
    case 1:
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        break;
    case 2:
        moby_sound_start(0, 0, m);
        plant_bolts_drop_l1(m);
        pieces();
        moby_retire(m);
        break;
    default:
        break;
    }
}
}  // namespace

// Class 1855: a crate: broken, two pieces 0x742 (one at random besides), 11 debris (debris_burst).
// orig: L10:0x002eb6f0
void orx_class1855_update_l10(uint32_t m) {
    crate_update(m, [m] { debris_burst(m, 0x742, 1, 0x742, 1, 0xb, 2); });
}

// Class 1856: a crate: broken, a moby of class 0x741 left in its place.
// orig: L10:0x002eb7e0
void orx_class1856_update_l10(uint32_t m) {
    crate_update(m, [m] { moby_clone_l1(m, 0x741); });
}

}  // namespace rac1
