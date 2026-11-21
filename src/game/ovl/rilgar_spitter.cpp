// Rilgar's (level 5) spitter (class 623): the city's common beast. It waits hidden until its trigger (a moby's flag or
// the hero in its box), wanders round its home, chases its target (the hero, or the nearest class mate of its group)
// round the level's walls, sprays a jet of green slime from its mouth or bites, is thrown back by hits and bursts in
// three pieces when its health runs out. Ported by hand from Rilgar's own code (L5:0x301f48 and its helpers 0x303390,
// 0x303b08, 0x303e50, 0x303f80, 0x304058), the original's order of calls and float operations kept.
#include <game/ovl/rilgar_own.h>

#include <game/level/level_data.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_path_links.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/walker.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;      // the hero's block (+0x80 his position, +0x1158 his mode: 6 in a vehicle)
constexpr uint32_t kHeroMoby = 0x001413d0;  // (the word holding the hero's moby)
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
// The spitter's tuning, the level's own words (0x161a74 .. 0x161ad4)
float size() { return lwc1(L5_ADDR(0x00161a98u)); }  // its size (1.5): its scale, its walker's and its walls' widths
float wall_width() { return fpu_mul(size(), 0.4f); }  // (0.4 of its size)
}  // namespace

// The nearest (within 120) visible (+0x34 bit 0x1000) live (state below 0x7f) moby of group `group`: 0 when none.
// orig: L5:0x00303f80
uint32_t ril_303f80_l5(uint32_t m, int32_t group) {
    uint32_t e = lvd::group(group);
    if (e == 0) return 0;
    uint32_t found = 0;
    float best = 120.0f;
    for (;; e += 2) {
        const uint32_t o = lwu(L5_ADDR(0x0015ffd8u)) + ((lhu(e) & 0x7fffu) << 8);
        if ((lhu(o + 0x34) & 0x1000u) != 0 && lbu(o + 0x20) < 127) {
            const float d = vec3_distance(V(o + 0x10), V(m + 0x10));
            if (d < best) {
                best = d;
                found = lwu(L5_ADDR(0x0015ffd8u)) + ((lhu(e) & 0x7fffu) << 8);
            }
        }
        if (lh(e) < 0) return found;
    }
}

// The level's walls (path `path`: segments between its points, a segment counts when either end's w lane is set)
// pushing the point `pos` out to `r` from each segment it is within `r` of (its ends as round caps), in the plane, the
// height kept; the result into `out` when something pushed. Returns the nearest distance to a wall's line (from 512).
// orig: L5:0x00304058
float ril_304058_l5(int32_t path, uint32_t pos, uint32_t out, float r) {
    const uint32_t sp = ee_stack_alloc(0x140);
    const uint32_t at = sp + 0x20, d = sp, seg = sp + 0x30, dir = sp + 0x40, cross = sp + 0x50, push = sp + 0x10, along = sp + 0x60;
    lq_sq(at, pos);
    bool pushed = false;
    float nearest = 512.0f;
    for (int32_t i = 0; i < lw(lvd::path(path)) - 1; ++i) {
        const uint32_t p = lvd::path(path) + 0x10 + static_cast<uint32_t>(i) * 16;
        if (lwc1(p + 0xc) == 0.0f && lwc1(p + 0x1c) == 0.0f) continue;
        vec3_sub(V(d), V(at), V(p));
        sw(d + 8, 0);
        vec3_sub(V(seg), V(lvd::path(path) + 0x20 + static_cast<uint32_t>(i) * 16), V(lvd::path(path) + 0x10 + static_cast<uint32_t>(i) * 16));
        sw(seg + 8, 0);
        vec3_set_length(V(dir), V(seg), 1.0f);
        vec3_cross_ba(V(cross), V(d), V(dir));
        const float off = abs_f(lwc1(cross + 8));
        if (off < nearest) nearest = off;
        if (r < off) continue;
        const float len = vec3_length(V(seg));
        const float t = vec3_dot(V(d), V(dir));
        if (len < t || t < 0.0f) {  // past an end: pushed from the segment's start
            if (!(vec3_length(V(d)) < r)) continue;
            pushed = true;
            vec3_set_length(V(push), V(d), r);
        } else {  // beside the segment: pushed off its line
            pushed = true;
            vec3_set_length(V(along), V(dir), t);
            vec3_sub(V(push), V(d), V(along));
            vec3_set_length(V(push), V(push), r);
            vec3_add(V(push), V(push), V(along));
        }
        vec3_add(V(at), V(push), V(lvd::path(path) + 0x10 + static_cast<uint32_t>(i) * 16));
        swc1(at + 8, lwc1(pos + 8));
    }
    if (pushed) lq_sq(out, at);
    ee_stack_free(0x140);
    return nearest;
}

// Is the way past the target `to` on the spitter's side (its heading to `to` turned by 1.5 x its side angle +0x294, as
// far as `to`) clear of its walls (+0x2c0) for `r` beyond? 1 when clear.
// orig: L5:0x00303e50
int32_t ril_303e50_l5(uint32_t m, uint32_t to, float r) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t side = sp, beyond = sp + 0x10;
    const float a = angle_add(atan2_f(fpu_sub(lwc1(to), lwc1(m + 0x10)), fpu_sub(lwc1(to + 4), lwc1(m + 0x14))),
                              fpu_mul(lwc1(pv + 0x294), 1.5f));
    swc1(side, fpu_mul(cos_vu(a), vec3_distance(V(to), V(m + 0x10))));
    swc1(side + 4, fpu_mul(sin_vu(a), vec3_distance(V(to), V(m + 0x10))));
    sw(side + 8, 0);
    vec3_add(V(side), V(side), V(m + 0x10));
    vec3_sub(V(beyond), V(side), V(to));
    vec3_set_length(V(beyond), V(beyond), r);
    vec3_add(V(beyond), V(beyond), V(m + 0x10));
    const int32_t crossed = path_crossed(lwu(pv + 0x2c0), side, beyond);
    ee_stack_free(0x90);
    return crossed == 0 ? 1 : 0;
}

// The spitter heads for `to` round its walls: every fourth frame (by its slot) the way point +0x280 found again (none:
// target lost, +0x164 = 2); turned toward the way point (720 degrees a second), past the target on its side by `offset`
// when that way is clear and the point is more than 5 away; its speed +0xf4 eased to 6 when facing it and more than 3
// away, else to 1; two walker steps; pushed off the walls unless they were all more than 2 away (then not tried for 5
// frames, +0x2d4).
// orig: L5:0x00303b08
void ril_303b08_l5(uint32_t m, uint32_t to, float offset) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t way = sp + 0x20, step = sp;
    if (((m >> 8) & 3u) == (lwu(L5_ADDR(0x0015f5ccu)) & 3u) &&
        nov_276fe8_l1(pv + 0x2c0, 1, lw(pv + 0x2c4), m + 0x10, to, pv + 0x280, wall_width()) == 0)
        sw(pv + 0x164, 2);
    lq_sq(way, pv + 0x280);
    const float dist = vec2_distance(V(m + 0x10), V(way));
    const float accel = fpu_mul(dt2(), u2f(0x41490fdbu));  // (4 pi)
    const float vmax = fpu_mul(dt(), u2f(0x41c90fdbu));    // (8 pi)
    auto heading = [&] { return atan2_f(fpu_sub(lwc1(way), lwc1(m + 0x10)), fpu_sub(lwc1(way + 4), lwc1(m + 0x14))); };
    if (5.0f < dist && offset != 0.0f && ril_303e50_l5(m, way, wall_width()) != 0)
        yaw_seek(m + 0x48, pv + 0x290, angle_add(heading(), offset), accel, accel, vmax);
    else
        yaw_seek(m + 0x48, pv + 0x290, heading(), accel, accel, vmax);
    if (angle_dist(lwc1(m + 0x48), heading()) < u2f(0x3fc90fdbu) && 3.0f < dist)
        approach_f(pv + 0xf4, fpu_mul(dt(), 6.0f), fpu_mul(dt2(), 6.0f));
    else
        approach_f(pv + 0xf4, dt(), fpu_mul(dt2(), 6.0f));
    swc1(step, fpu_add(cos_vu(lwc1(m + 0x48)), cos_vu(lwc1(m + 0x48))));
    sw(step + 8, 0);
    swc1(step + 4, fpu_add(sin_vu(lwc1(m + 0x48)), sin_vu(lwc1(m + 0x48))));
    walker_record_step(1.0f, m, pv + 0xd0, step, sp + 0x10);
    if (lw(pv + 0x2d4) == 0 && 2.0f < ril_304058_l5(lw(pv + 0x2c0), m + 0x10, m + 0x10, wall_width()))
        sw(pv + 0x2d4, static_cast<uint32_t>(frame_scale_i(5)));
    ee_stack_free(0x90);
}

// The spitter's frame before its state: the hit (mask 0x330000; ignored when a wall is between it and the source within
// 24 units) and its reaction (health +0x20; thrown back: state 0xb, or killed and thrown: state 0xe, its bolts), the
// colour pulse of the hit; the alert (+0x38: 3 to 4 seconds of 20 more range, +0x2b0); its target +0x160: what
// target_select finds on its path +0x2cc within the range (lost, +0x164 = 2, when out of range of its home or 3 off in
// height), a nearer class mate of its group +0x2c8, else the hero; its glow pulsing; its head turned at the hero
// (states 4..9); the head's and the jaw's springs.
// orig: L5:0x00303390
void ril_303390_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t eye = sp, from = sp + 0x10, hit_state = sp + 0x20, hit_dmg = sp + 0x24;
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2d4)));
    sw(hit_dmg, 0);
    uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    if (hit != 0) {  // the hit counts only with a clear line from the source (when it is within 24)
        lq_sq(eye, m + 0x10);
        swc1(eye + 8, fpu_add(lwc1(eye + 8), 0.5f));
        const uint32_t src = lwu(hit + 0x20);
        if (src == 0) {
            lq_sq(from, hit);
        } else if (lh(src + 0xa6) == 0x47) {
            lq_sq(from, kHero + 0xd0);
        } else {
            const uint32_t cls = lwu(src + 0x24);
            if ((cls != 0 && lh(cls + 0x46) == 5) || lh(src + 0xa6) == 0xba) lq_sq(from, eye);
            else lq_sq(from, src + 0x10);
        }
        if (vec3_distance(V(eye), V(from)) < 24.0f && segment_collide(eye, from, 2, m, 0) != 0) hit = 0;
    }
    int32_t kind = moby_hit_react(m, hit, pv + 0x20, 0, hit_state, hit_dmg, 0, 4);
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0xe) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, health);
        if (health <= 0.0f) kind = 1;
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(fpu_mul(size(), u2f(0x43cccccdu)))));  // (x 409.6)
        sw(pv + 0x94, 9);
        sb(pv + 0xad, 0);
        swc1(pv + 0x80, fpu_mul(lwc1(L5_ADDR(0x00161ad0u)), dt2()));
        swc1(pv + 0x98, wall_width());
        // thrown back from the hit's source, speeds `out` / `up` (the level's words), the move set `set`
        auto thrown = [&](float lo, float hi, uint32_t out, uint32_t up, uint32_t yaw, uint32_t set) {
            swc1(pv + 0xc0, lo);
            swc1(pv + 0xc4, hi);
            swc1(pv + 0x88, fpu_mul(lwc1(out), dt()));
            swc1(pv + 0x8c, fpu_mul(lwc1(up), dt()));
            const uint32_t src = lwu(hit + 0x20);
            swc1(yaw, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14))));
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked(sp, yaw, pv + 0x88, pv + 0x8c);
            move_state_start(m, pv + 0x70, set, 1, 0, lwc1(yaw));
        };
        switch (static_cast<uint32_t>(kind)) {  // (the reaction's jump table L5:0x002186f0, 12 entries, by its index)
        case 1:
        case 2: {  // (0x00303660) killed: thrown (move set 6, state 0xe), its bolts (more for a source of kind 5)
            swc1(pv + 0x80, fpu_mul(lwc1(L5_ADDR(0x00161ad4u)), dt2()));
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            thrown(5.5f, 11.0f, L5_ADDR(0x00161a90u), L5_ADDR(0x00161a94u), sp + 0x2c, 6);
            sb(m + 0x20, 0xe);
            sb(pv + 0x67, 0xf0);
            const uint32_t src = lwu(hit + 0x20);
            const uint32_t cls = src != 0 ? lwu(src + 0x24) : 0;
            moby_drop_bolts(m, (cls != 0 && lh(cls + 0x46) == 5) ? 0x800u : 0u, -1);
            break;
        }
        case 3: case 4: case 5: case 6: case 7: case 8:  // (0x003035b8) thrown back (move set 5, state 0xb)
            thrown(5.0f, 10.0f, L5_ADDR(0x00161a88u), L5_ADDR(0x00161a8cu), sp + 0x28, 5);
            sb(m + 0x20, 0xb);
            sb(pv + 0x67, 0x78);
            break;
        case 9:
        case 10:  // (0x003035ac)
            sb(pv + 0x67, 0xfa);
            break;
        default:  // (0, 11 and beyond: 0x0030376c)
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    if (lw(pv + 0x38) != 0) {  // alerted: 3 to 4 seconds of more range
        sw(pv + 0x2b0, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2b0)));
    float range = lwc1(pv + 0x29c);
    if (lw(pv + 0x2b0) != 0) range = fpu_add(range, 20.0f);
    swc1(pv + 0x2a0, range);
    {
        const uint32_t path = lvd::path(lw(pv + 0x2cc));
        if (target_select(m, pv + 0x120, 0, 0, path + 0x10, lw(path), lwc1(pv + 0x2a0)) != 2) {
            if (lwc1(pv + 0x2a0) < vec2_distance(V(pv + 0x270), V(pv + 0x120))) sw(pv + 0x164, 2);
            else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x128)))) sw(pv + 0x164, 2);
        }
    }
    const int32_t group = lw(pv + 0x2c8);
    if (group >= 0) {
        const uint32_t o = ril_303f80_l5(m, group);
        if (o != 0 && (lw(pv + 0x164) == 2 || vec2_distance(V(m + 0x10), V(o + 0x10)) <
                                                  vec2_distance(V(m + 0x10), V(lwu(pv + 0x160) + 0x10)))) {
            sw(pv + 0x160, o);
            sw(pv + 0x164, 1);
        }
    }
    if (lwu(pv + 0x160) == 0) sw(pv + 0x160, lwu(kHeroMoby));
    swc1(pv + 0x2d8, angle_add(lwc1(pv + 0x2d8), fpu_mul(dt(), u2f(0x4116cbe4u))));  // (3 pi)
    const float pulse = fpu_mul(fpu_add(sin_vu(lwc1(pv + 0x2d8)), 1.0f), 0.5f);
    sw(m + 0x90, rgba_lerp(lwu(L5_ADDR(0x00161a74u)), lwu(L5_ADDR(0x00161a78u)), pulse));
    const uint32_t state = lbu(m + 0x20);
    if (state >= 4 && state < 10 && lwu(pv + 0x160) == lwu(kHeroMoby)) {  // the head turned at the hero
        const uint32_t to = sp;
        vec3_sub(V(to), V(lwu(pv + 0x160) + 0x10), V(m + 0x10));
        float yaw = angle_sub(atan2_f(lwc1(to), lwc1(to + 4)), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(V(to)), lwc1(to + 8));
        if ((lhu(m + 0x34) & 0x8000u) != 0) yaw = -yaw;
        const float quarter = u2f(0x3fc90fdbu), sixth = u2f(0x3f060a92u);
        if (quarter < yaw) yaw = quarter;
        else if (yaw < -quarter) yaw = -quarter;
        if (sixth < pitch) pitch = sixth;
        else if (pitch < -sixth) pitch = -sixth;
        swc1(pv + 0x1d4, pitch);
        swc1(pv + 0x1d8, fpu_mul(yaw, u2f(0x3f333333u)));  // (0.7)
        swc1(pv + 0x258, fpu_mul(yaw, u2f(0x3e99999au)));  // (0.3)
    }
    const float k = u2f(0x3d23d70au), damp = u2f(0x3e99999au);  // (0.04, 0.3)
    joint_node_spring(m, pv + 0x170, 1, fpu_mul(lwc1(0x0015ed64), k), fpu_mul(lwc1(0x0015ed64), damp));
    joint_node_spring(m, pv + 0x1f0, 2, fpu_mul(lwc1(0x0015ed64), k), fpu_mul(lwc1(0x0015ed64), damp));
    ee_stack_free(0x90);
}

// Class 623, the spitter. pvars: its health +0x20, the hit's move record +0x60 / +0x70, the walker record +0xd0 (its
// speed +0xf4), the target finder's record +0x120, its target +0x160 and lost flag +0x164 (2), the head and jaw joints
// +0x170 / +0x1f0, its home +0x270, its wander point +0x280 / the way point, its yaw rate +0x290, its side angle +0x294
// (from +0x298 degrees, the side picked at random), its range +0x29c / +0x2a0, the side's hold +0x2ac, the alert +0x2b0,
// hidden until +0x2b4, its trigger moby +0x2b8 / box +0x2bc (both -1: none), its walls +0x2c0 (+0x2c4 their path), its
// class mates' group +0x2c8, its target path +0x2cc, the spray's wind-up +0x2d0, the wander's frames +0x2d2, the walls'
// rest +0x2d4, the glow's phase +0x2d8, hidden for a scene +0x2dc.
// States: 0 set up; 1 idle (now and then a way to the target: chase 4; alerted: 6; else wanders, 2); 2 wanders to a
// point near home (back to the state it came from, +0xbc); 4 chases on its side; 5 at a wall; 6 goes at the target;
// 7 the spray's wind-up then (8) the spray; 8 bites; 9 after the bite; 10 goes home; 11 thrown back; 13 the animation
// done: idle; 14 thrown dead: bursts on landing; 15 hidden, waiting for its trigger. 3, 12: nothing.
// orig: L5:0x00301f48
void ril_class623_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    moby_set_70(pv + 0x170, 2.5f);
    // a scene (game mode 0x15f5c4 = 2) hides it; back in play (0) it shows again
    if (lw(L5_ADDR(0x0015f5c4u)) == 2 && (lhu(m + 0x34) & 1u) == 0) {
        sw(pv + 0x2dc, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    } else if (lw(pv + 0x2dc) != 0 && lw(L5_ADDR(0x0015f5c4u)) == 0) {
        sw(pv + 0x2dc, 0);
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    }
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L5_ADDR(0x001671c0u))) < 28.0f) {  // near the camera
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    ril_303390_l5(m);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), size()));
    const uint32_t target = lwu(pv + 0x160);
    const uint32_t tgt = target + 0x10;
    const uint32_t pos = m + 0x10, home = pv + 0x270, wander = pv + 0x280;
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t step = sp + 0x10;
    auto anim_to = [m](uint32_t set, int32_t frames) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
    };
    auto heading_to = [pos](uint32_t p) { return atan2_f(fpu_sub(lwc1(p), lwc1(pos)), fpu_sub(lwc1(p + 4), lwc1(pos + 4))); };
    // the turn toward `p` (360 degrees a second)
    auto turn_to = [&](uint32_t p) {
        const float two_pi = u2f(0x40c90fdbu);
        yaw_seek(m + 0x48, pv + 0x290, heading_to(p), fpu_mul(dt2(), two_pi), fpu_mul(dt2(), two_pi), fpu_mul(dt(), two_pi));
    };
    // two walker steps along the heading, then pushed off the walls (only while facing within 90 degrees of `p`)
    auto walk_toward = [&](uint32_t p) {
        if (!(angle_dist(lwc1(m + 0x48), heading_to(p)) < u2f(0x3fc90fdbu))) return;
        swc1(step, fpu_add(cos_vu(lwc1(m + 0x48)), cos_vu(lwc1(m + 0x48))));
        swc1(step + 4, fpu_add(sin_vu(lwc1(m + 0x48)), sin_vu(lwc1(m + 0x48))));
        sw(step + 8, 0);
        walker_record_step(1.0f, m, pv + 0xd0, step, sp + 0x20);
        path_push_out_2d(lwu(pv + 0x2c0), pos, pos, wall_width());
    };
    // a new wander point (within 4 of home, on the ground of home), the state to come back to kept in +0xbc
    auto wander_from = [&](uint32_t frames) {
        rand_sphere_point(wander, 0.0f, 4.0f);
        swc1(wander + 8, 0.0f);
        vec3_add(V(wander), V(wander), V(home));
        sh(pv + 0x2d2, frames);
        sb(m + 0xbc, lbu(m + 0x20));
        sb(m + 0x20, 2);
        anim_to(1, frame_scale_i(0xa));
    };
    // the spray's wind-up (state 7), when the target is within 3
    auto wind_up_near = [&]() {
        if (!(vec2_distance(V(pos), V(tgt)) < 3.0f)) return;
        sb(m + 0x20, 7);
        sh(pv + 0x2d0, static_cast<uint32_t>(frame_scale_i(lw(L5_ADDR(0x00161ac8u)))));
        anim_to(2, frame_scale_i(0xa));
    };
    // the idle animation's end: a wander now and then (one in two), else the idle set again
    auto idle_end = [&](uint32_t wander_frames) {
        if ((lbu(m + 0x70) & 2) == 0) return;
        if (lbu(m + 0x31) != 0 && (rand_mod(0xff) & 1) != 0) {
            wander_from(wander_frames);
            return;
        }
        anim_to(0, frame_scale_i(0xa));
    };
    bool timing = true;  // (every way out but the retire stores the timer word)
    switch (lbu(m + 0x20)) {  // (the state's jump table L5:0x002186b0, 16 entries, by its index)
    case 0: {  // (0x00302080) set up
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        lq_sq(home, pos);
        swc1(pv + 0x20, 2.0f);
        sb(pv + 0x29, 0);
        sw(pv + 0x2b0, 0);
        sh(pv + 0x24, 2);
        swc1(pv + 0x30, 1.0f);
        sb(pv + 0x28, 1);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(14.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(u2f(0x41333333u))));  // (11.2)
        walker_record_init_l1(pv + 0xd0);
        swc1(pv + 0xd8, 2.0f);
        swc1(pv + 0xdc, 2.0f);
        sw(pv + 0xd0, static_cast<uint32_t>(float_to_int(fpu_mul(size(), u2f(0x43cccccdu)))));  // (x 409.6)
        swc1(pv + 0xf4, fpu_mul(dt(), 6.0f));
        swc1(pv + 0x2d8, rand_angle());
        if (rand_mod(2) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);  // (its side: the head's turn mirrored)
        else sh(m + 0x34, lhu(m + 0x34) & 0x7fffu);
        if (lw(pv + 0x2b4) != 0) {  // hidden 20 below until shown
            swc1(pos + 8, fpu_sub(lwc1(pos + 8), 20.0f));
            sb(m + 0x20, 0xc);
            sh(m + 0x32, 0);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            sb(m + 0x31, 0);
            sw(m + 0x94, 0);
        } else if (ld(pv + 0x2b8) != ~0ull) {  // a trigger: waits for it (15)
            sb(m + 0x20, 0xf);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            anim_to(0, 0);
        } else {
            sb(m + 0x20, 1);
            anim_to(0, 0);
        }
        path_links_build(pv + 0x2c0, 1, lwu(pv + 0x2c4));
        break;
    }
    case 1: {  // (0x0030221c) idle
        if (lw(pv + 0x164) != 2 && rand_mod(9) == 0) {
            if (nov_276fe8_l1(pv + 0x2c0, 1, lw(pv + 0x2c4), pos, tgt, sp + 0x10, wall_width()) == 0) {
                sw(pv + 0x164, 2);
            } else {  // a way: chases on a side (its angle flipped one time in two), for 0.5 to 1.5 seconds
                if ((rand_mod(0x100) & 1) != 0) swc1(pv + 0x298, -lwc1(pv + 0x298));
                swc1(pv + 0x294, fpu_mul(lwc1(pv + 0x298), u2f(0x3c8efa35u)));  // (degrees to radians)
                sw(pv + 0x2ac, static_cast<uint32_t>(frame_scale_i(static_cast<int32_t>(f2u(eer::cvt_w(rand_range_f(30.0f, 90.0f)))))));
                sb(m + 0x20, 4);
                anim_to(1, frame_scale_i(0xa));
            }
        }
        if (lw(pv + 0x164) == 2 && lw(pv + 0x2b0) != 0) {  // lost but alerted: goes at it
            sb(m + 0x20, 6);
            anim_to(1, frame_scale_i(0xa));
            break;
        }
        idle_end(0);
        break;
    }
    case 2: {  // (0x003023fc) wanders: back after 1 second at the point, or after 3 seconds
        turn_to(wander);
        walk_toward(wander);
        sh(pv + 0x2d2, lhu(pv + 0x2d2) + 1u);
        const int32_t frames = lh(pv + 0x2d2);
        if (!((vec2_distance(V(pos), V(wander)) < 1.0f && frame_scale_i(0x3c) < frames) || frame_scale_i(0xb4) < frames)) break;
        sb(m + 0x20, lbu(m + 0xbc));
        anim_to(0, frame_scale_i(0x14));
        break;
    }
    case 4: {  // (0x00302930) chases on its side; within 3 and facing (45 degrees): the wind-up
        const float d = vec2_distance(V(pos), V(tgt));
        ril_303b08_l5(m, tgt, lwc1(pv + 0x294));
        if (d < 3.0f && angle_dist(lwc1(m + 0x48), heading_to(tgt)) < u2f(0x3f490fdbu)) {
            sb(m + 0x20, 7);
            sh(pv + 0x2d0, static_cast<uint32_t>(frame_scale_i(lw(L5_ADDR(0x00161ac8u)))));
            anim_to(2, frame_scale_i(0xa));
        }
        if (lw(pv + 0x164) == 2) sb(m + 0x20, 0xa);
        break;
    }
    case 5:  // (0x00302790) at a wall: goes home when the alert is over, else the wind-up within 3
        if (lw(pv + 0x2b0) == 0) {
            sw(pv + 0xf4, 0);
            sb(m + 0x20, 0xa);
            anim_to(1, frame_scale_i(0x14));
            break;
        }
        wind_up_near();
        break;
    case 6: {  // (0x0030257c) goes straight at the target
        approach_f(pv + 0xf4, fpu_mul(dt(), 6.0f), fpu_mul(dt2(), 6.0f));
        turn_to(tgt);
        walk_toward(tgt);
        const uint32_t cross = sp + 0x30;
        if (path_crossing(lw(pv + 0x2c0), pos, tgt, cross) != 0 && vec3_distance(V(pos), V(cross)) < 1.5f) {  // a wall in front
            sb(m + 0x20, 5);
            anim_to(4, frame_scale_i(0x14));
            break;
        }
        if (lw(pv + 0x2b0) == 0) {
            sb(m + 0x20, 0xa);
            break;
        }
        wind_up_near();
        break;
    }
    case 7: {  // (0x00302a10) the wind-up: faces the target, then the spray (8)
        const float accel = fpu_mul(dt2(), u2f(0x41490fdbu));  // (4 pi)
        yaw_seek(m + 0x48, pv + 0x290, heading_to(tgt), accel, accel, fpu_mul(dt(), u2f(0x41c90fdbu)));
        if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) == 2) moby_anim_set(m, 7, 0, frame_scale_i(5));
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x2d0))) == 0) break;
        sb(m + 0x20, 8);
        sh(pv + 0x2d0, 0);
        anim_to(3, frame_scale_i(1));
        // the spray from the mouth (chain point 0): 30 drops of the first colours (0x161a9c / 0x161aa0) with the
        // full sizes (0x161ac0 / 0x161ac4), then 30 of the second (0x161aa4 / 0x161aa8) a tenth that size
        const uint32_t vel = sp + 0x10, drift = sp + 0x20, mouth = sp + 0x30, at = sp + 0x40;
        const float yaw = angle_add(lwc1(m + 0x48), 0.0f);
        swc1(vel, fpu_mul(cos_vu(yaw), fpu_mul(lwc1(L5_ADDR(0x00161ab8u)), dt())));
        swc1(vel + 4, fpu_mul(sin_vu(yaw), fpu_mul(lwc1(L5_ADDR(0x00161ab8u)), dt())));
        swc1(vel + 0xc, lwc1(L5_ADDR(0x00161ac0u)));
        swc1(drift + 8, 0.0f);
        swc1(drift + 0xc, lwc1(L5_ADDR(0x00161ac4u)));
        swc1(vel + 8, 0.0f);
        moby_chain_point(m, 0, mouth);
        const float jitter = u2f(0x3d851eb8u), grow = u2f(0x3f99999au);  // (0.065, 1.2)
        auto spray = [&](uint32_t c0, uint32_t c1) {
            for (int32_t k = 0; k < 30; ++k) {
                lq_sq(at, mouth);
                swc1(at, fpu_add(lwc1(at), rand_range_f(-jitter, jitter)));
                swc1(at + 4, fpu_add(lwc1(at + 4), rand_range_f(-jitter, jitter)));
                swc1(at + 8, fpu_add(lwc1(at + 8), rand_range_f(-jitter, jitter)));
                swc1(drift, rand_range_f(-dt(), dt()));
                swc1(drift + 4, rand_range_f(-dt(), dt()));
                const float rise = fpu_mul(lwc1(L5_ADDR(0x00161abcu)), dt());
                swc1(drift + 8, rand_range_f(-rise, rise));
                const float n0 = lwc1(L5_ADDR(0x00161aacu));
                const uint32_t n = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(n0, fpu_add(n0, n0)))));
                const float h0 = lwc1(L5_ADDR(0x00161ab0u));
                const uint32_t h = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(h0, fpu_mul(h0, grow)))));
                const float l0 = lwc1(L5_ADDR(0x00161ab4u));
                const uint32_t life = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(l0, fpu_mul(l0, grow)))));
                spawn_particle_02(at, vel, drift, lwu(c0), lwu(c1), n, h, life, -1);
            }
        };
        spray(L5_ADDR(0x00161a9cu), L5_ADDR(0x00161aa0u));
        swc1(vel + 0xc, fpu_mul(lwc1(L5_ADDR(0x00161ac0u)), u2f(0x3dcccccdu)));  // (0.1)
        swc1(drift + 0xc, fpu_mul(lwc1(L5_ADDR(0x00161ac4u)), u2f(0x3dcccccdu)));
        spray(L5_ADDR(0x00161aa4u), L5_ADDR(0x00161aa8u));
        break;
    }
    case 8: {  // (0x00302e40) bites: before frame 1.5, the target within 15 degrees, 3 away and 2 in height
        const float yaw = angle_add(lwc1(m + 0x48), 0.0f);
        if (moby_anim_time(m) < 1.5f && angle_dist(yaw, heading_to(tgt)) < u2f(0x3e860a92u) && vec2_distance(V(pos), V(tgt)) < 3.0f &&
            abs_f(fpu_sub(lwc1(tgt + 8), lwc1(pos + 8))) < 2.0f) {
            const uint32_t dir = sp + 0x10, at = sp + 0x20;
            swc1(dir, cos_vu(lwc1(m + 0x48)));
            swc1(dir + 4, sin_vu(lwc1(m + 0x48)));
            swc1(dir + 8, 0.0f);
            swc1(dir + 0xc, u2f(0x45afdf66u));  // (5627.92)
            lq_sq(at, tgt);
            swc1(at + 8, fpu_add(lwc1(at + 8), 0.75f));
            sub_00259bc8(target, m, lw(kHero + 0x1158) == 6 ? 0x10000u : 0x10001u, at, dir, 1.0f);
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 9);
        anim_to(4, frame_scale_i(0xa));
        break;
    }
    case 9: {  // (0x00302fcc) after the bite: again when facing within 3, else chases
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (vec2_distance(V(pos), V(tgt)) < 3.0f && angle_dist(lwc1(m + 0x48), heading_to(tgt)) < u2f(0x3e860a92u)) {
            sb(m + 0x20, 7);
            sh(pv + 0x2d0, static_cast<uint32_t>(frame_scale_i(lw(L5_ADDR(0x00161ac8u)))));
            anim_to(7, frame_scale_i(5));
            break;
        }
        sw(pv + 0xf4, 0);
        sb(m + 0x20, 4);
        anim_to(1, frame_scale_i(0xa));
        break;
    }
    case 10:  // (0x003030b4) goes home; there idle; with a target again: chases (alerted: goes at it)
        ril_303b08_l5(m, home, 0.0f);
        if (vec2_distance(V(pos), V(home)) < 1.0f) {
            sb(m + 0x20, 1);
            anim_to(0, frame_scale_i(3));
        } else if (lw(pv + 0x164) != 2) {
            sb(m + 0x20, 4);
            anim_to(1, frame_scale_i(3));
        } else if (lw(pv + 0x2b0) != 0) {
            sb(m + 0x20, 6);
        }
        break;
    case 11: {  // (0x00303150) thrown back: landed where it can get home, chases; else bursts. Under 2: goes
        if ((enemy_physics(m, pv + 0x70) & 0x60u) != 0) {
            if (nov_276fe8_l1(pv + 0x2c0, 1, lw(pv + 0x2c4), pos, home, sp + 0x10, wall_width()) != 0) {
                sb(m + 0x20, 4);
                anim_to(1, frame_scale_i(0xa));
                break;
            }
            explosion_burst(m, pos, -1, 0.5f, 13.0f);
            timing = false;
        } else if (lwc1(pos + 8) < 2.0f) {
            timing = false;
        }
        if (!timing) moby_retire(m);
        break;
    }
    case 13:  // (0x00303200) the animation done: idle
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 1);
        anim_to(0, frame_scale_i(0xa));
        break;
    case 14: {  // (0x00303244) thrown dead: bursts in three pieces on landing; under 0: goes
        if ((enemy_physics(m, pv + 0x70) & 0x60u) != 0) {
            explosion_burst(m, pos, -1, 0.5f, 13.0f);
            const uint32_t zero = L5_ADDR(0x0015f580u);  // (a zero vector)
            debris_spawn(m, 0x6da, pos, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            debris_spawn(m, 0x6db, pos, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            debris_spawn(m, 0x6dc, pos, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
            timing = false;
        } else if (lwc1(pos + 8) < 0.0f) {
            timing = false;
        }
        if (!timing) moby_retire(m);
        break;
    }
    case 15: {  // (0x0030282c) hidden: shown (idle) when its trigger moby's +0xbc is set or the hero is in its box
        const int32_t trigger = lw(pv + 0x2b8);
        const int32_t box = lw(pv + 0x2bc);
        if ((trigger != -1 && lbu(lwu(L5_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(trigger) << 8) + 0xbc) != 0) ||
            (box != -1 && point_in_box(kHero + 0x80, box) != 0)) {
            sh(m + 0x32, 0x40);
            sb(m + 0x20, 1);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        if ((rand_mod(0xff) & 1) != 0) {
            wander_from(static_cast<uint32_t>(frame_scale_i(0x5a)));
            break;
        }
        anim_to(0, frame_scale_i(0xa));
        break;
    }
    default:  // (3, 12 and beyond)
        break;
    }
    // (the EE's timer 0 count into the level's word 0x161a84 at every way out but the retire: a timing stamp of the
    // original's; the register reads as the runtime keeps it)
    if (timing) sw(L5_ADDR(0x00161a84u), eer::hw_lw(0x10000000u));
    ee_stack_free(0x100);
}

}  // namespace rac1
