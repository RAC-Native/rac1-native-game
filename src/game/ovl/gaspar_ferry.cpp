// Gaspar's own moby class 1766, a raft that ferries the hero along a path over the lava — hand-ported from Gaspar's own
// overlay. Header gaspar_mobys.h, registered in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`); Gaspar-only data is
// L9_ADDR, the base ELF's are bare (the hero 0x13f350: his place +0x80, the moby he stands on +0x2fc, his air flag +0x30e; the
// frame time 0x15ed6c / its square 0x15ed70, the level number 0x15ed84, the per-level flag bytes 0x14c050).
//
// The pvars: the attach frame +0x20, its path +0xac, the boxes that call it from either end +0xb0 / +0xb4, its way along the
// path +0xb8 (1 forward, -1 back) and the point it heads for +0xba, its speed +0xa8, its heading's offset +0xbc (pi at an end:
// it turns; 0 once a point's w is 42), its spin +0xc0 and turn +0x70, its sway +0xcc (eased to 0 riding, to 1 at an end), its
// bobs +0xd0 .. +0xdc, whether it is used +0xa0, ridden +0xc8, its hum +0xc4.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;
const float kPi = u2f(0x40490fdbu);
float dt() { return lwc1(0x0015ed6c); }

// the two bobs at an end, their size by its sway (eased to 1)
void bob(uint32_t m, uint32_t pv) {
    const float s = fpu_add(lwc1(pv + 0xcc), fpu_mul(fpu_sub(1.0f, lwc1(pv + 0xcc)), u2f(0x3c23d70au)));
    const float d = fpu_mul(dt(), u2f(0x3f060a92u));
    swc1(pv + 0xcc, s);
    sub_00262b00(m, pv + 0xd8, pv + 0xdc, fpu_mul(s, u2f(0x3e0f5c29u)), d);
    sub_00262b80(m, pv + 0xd0, pv + 0xd4, fpu_mul(lwc1(pv + 0xcc), u2f(0x3db2b8c2u)), fpu_mul(dt(), u2f(0x3ea0d97cu)),
                 fpu_mul(dt(), u2f(0x3efa35ddu)));
}

// the hero inside the box `box` (-1: none)
bool hero_in(int32_t box) { return box != -1 && point_in_box(HERO + 0x80, box) != 0; }
}  // namespace

// An angle at `at` eased from `from` toward `to` by `k` (the difference times k, added).
// orig: L9:0x0030a238
void gas_30a238_l9(uint32_t at, float from, float to, float k) {
    const float d = fpu_mul(angle_sub(to, from), k);
    swc1(at, d);
    swc1(at, angle_add(d, from));
}

// The raft's ride toward the point it heads for (+0xba; the point lifted by its speed times Gaspar's 0x161df8): its sway eased
// to 0, moved its speed toward the point, its heading eased toward the point's (plus its offset) and its tilt toward level;
// within twice its speed of the point the next point (1: it was the last, past either end). Its speed eased toward the
// distance to the end it heads for; a point's w 42 ends its turning. Its spin by its speed; away from both ends by 8, now and
// then (one in 7) a splash of the lava (particle 0x2d) by it. 1 when it arrived.
// orig: L9:0x0030a298
int32_t gas_30a298_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv + 0xac));
    int32_t arrived = 0;
    swc1(pv + 0xcc, fpu_add(lwc1(pv + 0xcc), fpu_mul(fpu_sub(0.0f, lwc1(pv + 0xcc)), u2f(0x3ba3d70au))));
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t target = sp, step = sp + 0x10, at = sp + 0x20, seek = sp + 0x30, pos = m + 0x10;
    lq_sq(target, path + 0x10 + (static_cast<uint32_t>(lh(pv + 0xba)) << 4));
    swc1(target + 8, fpu_add(lwc1(target + 8), fpu_mul(lwc1(pv + 0xa8), lwc1(L9_ADDR(0x00161df8)))));
    vec3_sub(V(step), V(target), V(pos));
    vec3_set_length(V(step), V(step), lwc1(pv + 0xa8));
    vec3_add(V(pos), V(pos), V(step));
    const uint32_t p = path + (static_cast<uint32_t>(lh(pv + 0xba)) << 4);
    const float to = angle_add(atan2_f(fpu_sub(lwc1(p + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(p + 0x14), lwc1(m + 0x14))), lwc1(pv + 0xbc));
    const float k = u2f(0x3ca3d70au);
    gas_30a238_l9(m + 0x48, lwc1(m + 0x48), to, fpu_mul(fpu_sub(1.0f, lwc1(pv + 0xcc)), k));
    gas_30a238_l9(m + 0x40, lwc1(m + 0x40), 0.0f, k);
    gas_30a238_l9(m + 0x44, lwc1(m + 0x44), 0.0f, k);
    uint32_t end;
    if (lh(pv + 0xb8) > 0) {  // forward: toward the last point
        if (vec3_distance(V(target), V(pos)) < fpu_add(lwc1(pv + 0xa8), lwc1(pv + 0xa8))) {
            const uint32_t i = lhu(pv + 0xba) + 1;
            sh(pv + 0xba, i);
            arrived = static_cast<int16_t>(i) == lw(path) ? 1 : 0;
        }
        end = path + (lwu(path) << 4);
        swc1(seek, 0.0f);
        seek_1d(seek, pv + 0xa8, vec3_distance(V(pos), V(end)), fpu_mul(lwc1(0x0015ed70), 3.0f), fpu_mul(lwc1(0x0015ed70), 3.0f),
                fpu_mul(dt(), 10.0f));
    } else {  // back: toward the first
        if (vec3_distance(V(target), V(pos)) < fpu_add(lwc1(pv + 0xa8), lwc1(pv + 0xa8))) {
            const uint32_t i = lhu(pv + 0xba) - 1;
            sh(pv + 0xba, i);
            arrived = static_cast<int16_t>(i) == -1 ? 1 : 0;
        }
        end = path + 0x10;
        swc1(seek + 4, 0.0f);
        seek_1d(seek + 4, pv + 0xa8, vec3_distance(V(pos), V(end)), fpu_mul(lwc1(0x0015ed70), 3.0f), fpu_mul(lwc1(0x0015ed70), 3.0f),
                fpu_mul(dt(), 10.0f));
    }
    if (lwc1(path + (static_cast<uint32_t>(lh(pv + 0xba)) << 4) + 0x1c) == 42.0f) sw(pv + 0xbc, 0);
    const float spin = angle_add(lwc1(pv + 0xc0), lwc1(pv + 0xa8));
    swc1(pv + 0xc0, spin);
    quat_from_axis_angle(V(pv + 0x70), 0, spin);
    if (8.0f < vec2_distance(V(pos), V(path + (lwu(path) << 4))) && 8.0f < vec2_distance(V(pos), V(path + 0x10)) && rand_mod(7) == 0) {
        lq_sq(at, pos);
        metal_25f8e0(at, 0.5f);
        swc1(at + 8, u2f(0x41c8147bu));
        spawn_particle_2d(at, 0, 0x40103080u, fpu_div(fpu_mul(lwc1(pv + 0xa8), 3.0f), fpu_mul(dt(), 10.0f)), 12600.0f);
    }
    ee_stack_free(0x40);
    return arrived;
}

// Class 1766. 0: drawn, its offset pi; unused (+0xa0 0) it just turns about each frame; used, at the path's start (or its end
// when its flag byte of the level is set), heading for the next point, waiting (1). 1 (and 2): at an end, bobbing; called by
// the hero in the box at its far end (once ridden), or the hero standing on it: off (3, ridden). 2 waits for the hero to step
// off. 3: riding, its hum on; with its offset 0 (turned), the hero in the box at the end it left and it away from both ends by
// 8 turns it back; arrived: the hum off, the way reversed (the point it heads for the other end's), at the end (2).
// orig: L9:0x00309c08
void gas_class1766_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv + 0xac));
    const uint32_t sp = ee_stack_alloc(0x20);
    const uint32_t delta = sp, old_ang = sp + 0x10;
    vec3_scale(V(delta), V(m + 0x10), -1.0f);
    lq_sq(old_ang, m + 0x40);
    if (pv == 0) {
        ee_stack_free(0x20);
        return;
    }
    switch (lbu(m + 0x20)) {
        case 0:
            sb(m + 0x30, 0xff);
            swc1(pv + 0xbc, kPi);
            sw(pv + 0xc4, 0xffffffffu);
            sw(pv + 0xc8, 0);
            sw(pv + 0xcc, 0);
            if (lw(pv + 0xa0) != 0) {
                sub_0024f590(m, 0, pv + 0x60);
                const uint32_t idx = lbu(m + 0xb0);
                const uint32_t p = lvd::path(lw(pv + 0xac));
                sb(m + 0x20, 1);
                uint32_t from;
                if (lbu(0x0014c050 + idx + (lwu(0x0015ed84) << 4)) != 0xff) {
                    sh(pv + 0xba, 1);
                    from = p + 0x10;
                    sh(pv + 0xb8, 1);
                } else {
                    sh(pv + 0xb8, 0xffffffffu);
                    sh(pv + 0xba, lhu(p) - 2);
                    from = p + (lwu(p) << 4);
                }
                lq_sq(m + 0x10, from);
                const uint32_t to = p + (static_cast<uint32_t>(lh(pv + 0xba)) << 4);
                swc1(m + 0x48, atan2_f(fpu_sub(lwc1(to + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(to + 0x14), lwc1(m + 0x14))));
            }
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), kPi));
            break;
        case 1:
            bob(m, pv);
            if (lw(pv + 0xc8) != 0) {  // called back by the hero at the other end
                const int32_t way = lh(pv + 0xb8);
                if ((way > 0 && hero_in(lw(pv + 0xb4))) || (way < 0 && hero_in(lw(pv + 0xb0)))) sb(m + 0x20, 3);
            }
            if (lwu(HERO + 0x2fc) == m && lh(HERO + 0x30e) == 0) {
                sw(pv + 0xc8, 1);
                sb(m + 0x20, 3);
            }
            break;
        case 2:
            bob(m, pv);
            if (lwu(HERO + 0x2fc) != m) sb(m + 0x20, 1);
            break;
        case 3: {
            if ((lwu(L9_ADDR(0x0015f5cc)) & 7) == ((m >> 8) & 7)) {
                const int32_t slot = lw(pv + 0xc4);
                if (slot == -1 || (sub_0028d8c0(m, slot) == 0 && lw(pv + 0xc4) == -1))
                    sw(pv + 0xc4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
            }
            if (lwc1(pv + 0xbc) == 0.0f) {  // turned: the hero in the box behind it turns it back, away from both ends
                const int32_t way = lh(pv + 0xb8);
                const uint32_t pos = m + 0x10;
                auto away = [pos, path] {
                    return 8.0f < vec2_distance(V(pos), V(path + (lwu(path) << 4))) && 8.0f < vec2_distance(V(pos), V(path + 0x10));
                };
                if (way < 0) {
                    if (hero_in(lw(pv + 0xb4)) && away()) {
                        sh(pv + 0xb8, 1);
                        swc1(pv + 0xbc, kPi);
                    }
                } else if (way > 0) {
                    if (hero_in(lw(pv + 0xb0)) && away()) {
                        sh(pv + 0xb8, 0xffffffffu);
                        swc1(pv + 0xbc, kPi);
                    }
                }
            }
            if (gas_30a298_l9(m) == 0) break;
            const int32_t slot = lw(pv + 0xc4);
            if (slot != -1) {
                const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
                if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
            }
            sw(pv + 0xc4, 0xffffffffu);
            sw(pv + 0xcc, 0);
            sw(pv + 0xdc, 0);
            sb(m + 0x20, 2);
            swc1(pv + 0xbc, kPi);
            const uint32_t way = static_cast<uint32_t>(-lh(pv + 0xb8));
            sh(pv + 0xb8, way);
            if (static_cast<int16_t>(way) <= 0) sh(pv + 0xba, lhu(path) - 1);
            else sh(pv + 0xba, 0);
            break;
        }
        default:
            break;
    }
    vec3_add(V(delta), V(delta), V(m + 0x10));
    attach_frame_set(pv + 0x20, delta, old_ang, m + 0x40);
    ee_stack_free(0x20);
}

}  // namespace rac1
