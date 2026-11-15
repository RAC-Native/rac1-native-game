// Rilgar's (level 5) air traffic (class 79): the flying cars that cruise the paths high over Blackwater City with
// red / green running lights and three light trails, updated only while the camera is above the rooftops; a hit
// brings one down in pieces. Ported by hand from Rilgar's own code (L5:0x2d7140, 0x2d7020, 0x2d7890), the original's
// order of calls and float operations kept.
#include <game/ovl/rilgar_own.h>

#include <game/level/level_data.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
// the frame counter (the level's word 0x15f5cc): the draw callbacks are queued once a frame by it
uint32_t frame_count() { return lwu(L5_ADDR(0x0015f5ccu)); }
}  // namespace

// The car's running lights, while it is drawn and in a group (+0x21 not 0xff): its three light points (chain points
// 0..2 into pvars +0x330), the colour pulsing between red and green (+0x368 the phase; the moby's +0x90, and the
// lights' colour +0x360 with alpha 0x30), and the lights' draw (0x2d6f48) queued once a frame (the stamp at *(+0x364)).
// orig: L5:0x002d7020
void ril_2d7020_l5(uint32_t m) {
    if (lbu(m + 0x21) == 0xff || lbu(m + 0x31) == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t i = 0; i < 3; ++i) moby_chain_point(m, i, pv + 0x330 + i * 16);
    swc1(pv + 0x368, angle_add(lwc1(pv + 0x368), fpu_mul(dt(), u2f(0x3ff5be0bu))));  // (110 degrees a second)
    const float t = fpu_add(fpu_mul(sin_vu(lwc1(pv + 0x368)), 0.5f), 0.5f);
    const uint32_t colour = rgba_lerp(NOT_ADDR(0x1e1eb4u), NOT_ADDR(0x1eb41eu), t);
    sw(m + 0x90, colour);
    sw(pv + 0x360, (colour & 0xff0000u) | 0x30000000u | (colour & 0xff00u) | (colour & 0xffu));
    const uint32_t frame = frame_count();
    if (frame == 0) return;
    const uint32_t stamp = lwu(pv + 0x364);
    if (lwu(stamp) == frame) return;
    sw(stamp, frame);
    sub_001f93e0(L5_ADDR(0x002d6f48u), m);
}

// The car moves along `path` (a loop) at the level's speed 0x1613d0 (units a second, over the first leg's length
// +0xc): its place in points +8, wrapped at the point count; the moby put there (position, angles), its roll +0x40
// from the path's +0x4c.
// orig: L5:0x002d7890
void ril_2d7890_l5(uint32_t m, uint32_t path) {
    const uint32_t pv = lwu(m + 0x78);
    float at = fpu_add(lwc1(pv + 8), fpu_div(fpu_mul(lwc1(L5_ADDR(0x001613d0u)), dt()), lwc1(pv + 0xc)));
    swc1(pv + 8, at);
    const float points = static_cast<float>(lw(path));
    if (points < at) swc1(pv + 8, fpu_sub(at, points));
    path_point(lwc1(pv + 8), path, 1, m + 0x10, m + 0x40, 0);
    swc1(m + 0x40, lwc1(m + 0x4c));
}

// Class 79, the flying cars. pvars: the path +0 (the level's path table), where on it the car starts +4 (a fraction of
// its points) and is +8 (in points), the first leg's length +0xc, the colour phase +0x1c, three light trails of 16
// points (+0x20, +0x120, +0x220: the trail's head +0x322, its length +0x320 growing to the level's 0x1613e0), the
// trails' draw stamp *(+0x324), the engine's sound +0x328, the running lights (0x2d7020: +0x330 .. +0x368).
// The path's turn at each point (its points' w lanes, -1 at point 0 until done) is worked out by the first car on it,
// scaled to 60 degrees at the sharpest. States: 0 set up; 1 flies (hit by damage over 0: 2); 2 falls apart (three
// pieces 0x72e thrown forward and up) and goes. Nothing runs while the camera is below 55 (the camera's height
// 0x1671c8); the lights run in every state but 2 while the level's switch 0x1613e4 is on.
// orig: L5:0x002d7140
void ril_class79_update_l5(uint32_t m) {
    if (lwc1(L5_ADDR(0x001671c8u)) < 55.0f) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    const uint32_t sp = ee_stack_alloc(0xe0);
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002d71dc) set up
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L5_ADDR(0x001613d4u))));
        swc1(pv + 0x1c, rand_angle());
        sw(pv + 0x328, 0xffffffffu);
        swc1(pv + 0x368, rand_angle());
        const uint32_t path = lvd::path(lw(pv));
        auto point = [path](int32_t i) { return path + 0x10 + static_cast<uint32_t>(i) * 16; };
        // the heading from point `a` to point `b`
        auto heading = [&](int32_t a, int32_t b) {
            return atan2_f(fpu_sub(lwc1(point(b)), lwc1(point(a))), fpu_sub(lwc1(point(b) + 4), lwc1(point(a) + 4)));
        };
        if (lwc1(path + 0x1c) == -1.0f) {  // the path's turns not worked out yet
            const int32_t last = lw(path) - 1;
            float sharpest = 0.0f;
            swc1(point(0) + 0xc, angle_sub(heading(0, 1), heading(last, 0)));
            int32_t i = 1;
            for (; i < lw(path) - 1; ++i) {
                const float in = heading(i - 1, i);
                const float turn = angle_sub(heading(i, i + 1), in);
                swc1(point(i) + 0xc, turn);
                if (sharpest < abs_f(turn)) sharpest = abs_f(lwc1(point(i) + 0xc));
            }
            const int32_t n = lw(path);
            {
                const float in = heading(n - 2, n - 1);
                swc1(point(n - 1) + 0xc, angle_sub(heading(n - 1, 0), in));
            }
            const float scale = fpu_div(u2f(0x3f860a92u), sharpest);  // (60 degrees over the sharpest)
            if (lw(path) > 0)
                for (int32_t k = 0; k < lw(path); ++k) swc1(point(k) + 0xc, fpu_mul(lwc1(point(k) + 0xc), scale));
        }
        swc1(pv + 0xc, vec3_distance(V(path + 0x10), V(path + 0x20)));
        sb(m + 0x20, 1);
        swc1(pv + 8, fpu_mul(static_cast<float>(lw(path)), lwc1(pv + 4)));
        break;
    }
    case 1: {  // (0x002d7450) flies
        swc1(pv + 0x1c, angle_add(lwc1(pv + 0x1c), fpu_mul(dt(), u2f(0x41490fdbu))));  // (two turns a second)
        const float t = fpu_mul(fpu_add(sin_vu(lwc1(pv + 0x1c)), 1.0f), 0.5f);
        sw(m + 0x90, rgba_lerp(lwu(L5_ADDR(0x001613d8u)), lwu(L5_ADDR(0x001613dcu)), t));
        ril_2d7890_l5(m, lvd::path(lw(pv)));
        if (sub_0028d8c0(m, lw(pv + 0x328)) == 0) sw(pv + 0x328, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
        if ((frame_count() & 3u) == 0) {  // every fourth frame the trails move on a point (and grow, up to 0x1613e0)
            const float length = static_cast<float>(lh(pv + 0x320));
            sh(pv + 0x322, (lhu(pv + 0x322) + 1u) & 0xfu);
            if (length < lwc1(L5_ADDR(0x001613e0u))) sh(pv + 0x320, lhu(pv + 0x320) + 1u);
        }
        // the trails' heads: three points on the car in its own axes (left wing tip, tail, right wing tip: x back, y
        // across, z up), turned with it and put at its position, into the three trails at the head
        struct Tip { uint32_t x, y, z, trail; };
        static constexpr Tip tips[3] = {{0xbf99999au, 0x3ecccccdu, 0x3ee66666u, 0x20},     // (-1.2, 0.4, 0.45)
                                        {0xbfc00000u, 0x00000000u, 0xbe99999au, 0x120},    // (-1.5, 0, -0.3)
                                        {0xbf99999au, 0xbecccccdu, 0x3ee66666u, 0x220}};  // (-1.2, -0.4, 0.45)
        for (int32_t k = 0; k < 3; ++k) {
            const uint32_t v = sp + 0x10 + static_cast<uint32_t>(k) * 0x10;
            sw(v, tips[k].x);
            sw(v + 4, tips[k].y);
            sw(v + 8, tips[k].z);
            sw(v + 0xc, 0);
            mat3_transform(V(v), V(v), M(m + 0xc0));
            const uint32_t at = pv + tips[k].trail + (static_cast<uint32_t>(lh(pv + 0x322)) << 4);
            vec3_add(V(at), V(v), V(m + 0x10));
            swc1(at + 0xc, 1.0f);
        }
        const uint32_t frame = frame_count();
        const uint32_t stamp = lwu(pv + 0x324);
        if (lwu(stamp) != frame) {  // the trails' draw (0x2d7920), queued once a frame
            sw(stamp, frame);
            sub_001f93e0(L5_ADDR(0x002d7920u), m);
        }
        // its collision (+0x94, the class's +0x10), none while the car is near the level's corner (x or y below 8)
        if (lwc1(m + 0x10) < 8.0f || lwc1(m + 0x14) < 8.0f) sw(m + 0x94, 0);
        else sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        break;
    }
    case 2: {  // (0x002d7724) falls apart
        moby_sound_start(0, 0, m);
        const uint32_t zero = L5_ADDR(0x0015f580u);  // (a zero vector)
        const uint32_t vel = sp + 0x10;
        vec3_scale(V(vel), V(m + 0xc0), fpu_mul(lwc1(L5_ADDR(0x001613e8u)), lwc1(0x0015ed60)));
        swc1(vel + 8, fpu_add(lwc1(vel + 8), fpu_mul(lwc1(L5_ADDR(0x001613ecu)), lwc1(0x0015ed60))));
        for (int32_t k = 0; k < 3; ++k)
            debris_spawn(m, 0x72e, m + 0x10, m + 0x40, frame_scale_i(0x5a), 0, vel, zero, zero, fpu_mul(lwc1(0x0015ed70), 12.0f));
        moby_retire(m);
        ee_stack_free(0xe0);
        return;
    }
    default:
        break;
    }
    if (lw(L5_ADDR(0x001613e4u)) != 0) ril_2d7020_l5(m);
    ee_stack_free(0xe0);
}

}  // namespace rac1
