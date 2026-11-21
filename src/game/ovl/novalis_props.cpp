// Novalis's own moby classes, part 5: the props (water lilies, rocks, plants...). Header
// novalis_mobys.h. Addresses are Novalis's (`orig: L1:`); Novalis-only data is
// L1_ADDR, the globals the engine shares with Veldin are the level's own (OV).
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
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
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

// A damped spring on one axis (Novalis's 0x2f81c8): the velocity at `vel` loses `damp` of itself per frame
// (0x15ed60 scaled), is pushed toward `target` by min((pos - target)^2, r^2) * k / r^2, and the position
// moves by it. Returns |target - the new position|.
// orig: L1:0x002f81c8
float lily_spring_l1(float target, float k, float damp, float r, uint32_t pos, uint32_t vel) {
    const float r2 = r * r;
    const float f14 = ((1.0f - damp) - 1.0f) * lwc1(0x0015ed60) + 1.0f;
    float v = lwc1(vel) * f14;
    swc1(vel, v);
    const float p = lwc1(pos);
    float d = (p - target) * (p - target);
    if (r2 < d) d = r2;
    d = d * k;
    d = fpu_div(d, r2);
    v = target < p ? v - d : v + d;
    swc1(vel, v);
    swc1(pos, lwc1(pos) + lwc1(vel));
    return abs_f(target - (p + v));
}

// Class 695, a water lily: its x / y ride springs (pvar +0 / +4 the rest position, velocities at +0x10 / +0x14) with
// the hero's weight pushing it away within radius L1:0x161bf8 of him (and less than 0.7 above him), the walkers' step
// (flags 3) keeping it on the water, at most L1:0x161bfc * dt a frame; its z follows the surface height at most
// 3 * dt a frame.
// orig: L1:0x002f8268
void water_lily_update_l1(uint32_t m) {
    const uint32_t buf = ee_stack_alloc(0x40);
    const uint32_t pos0 = buf, from = buf + 0x10, step = buf + 0x20, moved = buf + 0x30;
    lq_sq(pos0, m + 0x10);
    const uint32_t pv = lwu(m + 0x78);
    lq_sq(from, m + 0x10);
    if (pv == 0) {
        ee_stack_free(0x40);
        return;
    }
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        lq_sq(pv, pos0);
        sb(m + 0x20, 1);
    } else if (st == 1) {
        const uint32_t hero = 0x0013f3d0;
        const float A = lwc1(L1_ADDR(0x00161bf0)), B = lwc1(L1_ADDR(0x00161bf4)), C = lwc1(L1_ADDR(0x00161bf8));
        const float three = u2f(0x40400000u);
        lily_spring_l1(lwc1(pv), A * lwc1(0x0015ed70), B, three, m + 0x10, pv + 0x10);
        lily_spring_l1(lwc1(pv + 4), lwc1(L1_ADDR(0x00161bf0)) * lwc1(0x0015ed70), lwc1(L1_ADDR(0x00161bf4)), three, m + 0x14, pv + 0x14);
        if (vec2_distance(V(m + 0x10), V(hero)) < lwc1(L1_ADDR(0x00161bf8)) && abs_f(lwc1(m + 0x18) - lwc1(hero + 8)) < u2f(0x3f333333u)) {
            const float yaw = atan2_f(lwc1(m + 0x10) - lwc1(hero), lwc1(m + 0x14) - lwc1(hero + 4));
            swc1(m + 0x10, cos_vu(yaw) * lwc1(L1_ADDR(0x00161bf8)));
            const float y = sin_vu(yaw) * lwc1(L1_ADDR(0x00161bf8));
            swc1(m + 0x14, y);
            swc1(m + 0x10, lwc1(m + 0x10) + lwc1(hero));
            swc1(m + 0x14, y + lwc1(hero + 4));
        }
        (void)C;
        walker_step(m, from, m + 0x10, 3, u2f(0x3dcccccdu), lwc1(L1_ADDR(0x00161c00)), u2f(0x44160000u), u2f(0x3fc90fdbu));
        swc1(m + 0x10, lwc1(from));
        swc1(m + 0x14, lwc1(from + 4));
        swc1(m + 0x18, lwc1(pos0 + 8));
        const float lim = lwc1(L1_ADDR(0x00161bfc)) * lwc1(0x0015ed6c);
        if (lim < vec3_distance(V(pos0), V(m + 0x10))) {
            vec3_sub(V(moved), V(m + 0x10), V(pos0));
            lq_sq(step, moved);
            vec3_set_length(V(step), V(step), lwc1(L1_ADDR(0x00161bfc)) * lwc1(0x0015ed6c));
            vec3_add(V(moved), V(pos0), V(step));
            lq_sq(m + 0x10, moved);
        }
        float h = surface_height(m + 0x10, 0) - lwc1(m + 0x18);
        float cap = lwc1(0x0015ed6c) * three;
        if (cap < h) h = cap;
        else {
            cap = -cap;
            if (h < cap) h = cap;
        }
        swc1(m + 0x18, lwc1(m + 0x18) + h);
    }
    ee_stack_free(0x40);
}

// Class 605, an invisible zone marker (its polygon path pvar +0x10 / box +0x14): on its first frame it keeps
// its position (pvar +0..), hides itself (+0x34 |= 0x41) and, the first marker of its level's hint nibble
// (0x14bf10 row, one nibble per marker counted in 0x161af0), loses 5 times the nibble from its pvar +0x18 and
// is removed when that runs out; later frames: when the hero (or the moby he rides, +0x1090, state 0x1b) is in its
// polygon / box (or it has none) it offers itself as the nearest marker (0x141390 the moby, 0x141394 its
// distance, 0x141398 set when it is nearer than 20).
// Every level but Veldin has a copy of it (the same code, the counter a word of each level's own small data; Eudora's calls
// its own copy of point_in_polygon, the same code): `counter` is the level's (zone_marker_update picks it).
// orig: L1:0x002f2eb8 L2:0x002d74e0 L3:0x002cbc38 L4:0x002d46b8 L5:0x00301cd8 L6:0x002e1518 L7:0x002fd698 L8:0x002f1c08 L9:0x002f7658 L10:0x002c6470 L11:0x002fc980 L12:0x002f5540 L13:0x002f51f0 L14:0x002ead20 L15:0x002d5728 L16:0x002d2a80 L17:0x002d3da0 L18:0x002d8ba8
void zone_marker_update_at(uint32_t m, uint32_t counter) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t P = g::PLAYER;
    if (lbu(m + 0x20) == 0) {
        if (pv == 0) {
            moby_retire(m);
            return;
        }
        sb(m + 0x30, 0xff);
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, 1);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        const int32_t level = lw(0x0015ed84);
        if (lbu(m + 0xbc) == 0 && level < 0x14) {
            const int32_t n = lw(counter);
            uint32_t b = lbu(0x0014bf10 + static_cast<uint32_t>((n + static_cast<int32_t>(static_cast<uint32_t>(n) >> 31)) >> 1) + (static_cast<uint32_t>(level) << 4));
            b = (n & 1) != 0 ? b & 0xfu : (b >> 4) & 0xfu;
            sw(counter, static_cast<uint32_t>(n + 1));
            sb(m + 0xbc, lbu(counter));
            const int32_t left = lw(pv + 0x18) - static_cast<int32_t>(b * 5);
            sw(pv + 0x18, static_cast<uint32_t>(left));
            if (!(left > 0)) {
                moby_retire(m);
                return;
            }
        }
    } else {
        sw(counter, 0);
    }
    const uint32_t buf = ee_stack_alloc(0x10);
    const uint32_t ridden = lwu(P + 0x1090);
    lq_sq(buf, ridden != 0 && lw(P + 0x10b8) == 0x1b ? lwu(ridden + 0x78) + 0x10 : 0x0013f3d0u);
    bool in_zone = false;
    if (lw(pv + 0x10) != -1) {
        const uint32_t path = lwu(OV(0x001b04b0) + static_cast<uint32_t>(lw(pv + 0x10)) * 4);
        in_zone = point_in_polygon(buf, path + 0x10, lw(path)) != 0;
    }
    if (!in_zone) {
        if (lw(pv + 0x14) != -1) in_zone = point_in_box(buf, lw(pv + 0x14)) != 0;
        if (!in_zone && ld(pv + 0x10) != 0xffffffffffffffffull) {
            ee_stack_free(0x10);
            return;
        }
    }
    const float d = vec2_distance(V(m + 0x10), V(buf));
    if (d < lwc1(P + 0x2044)) {
        sw(P + 0x2040, m);
        swc1(P + 0x2044, d);
        if (d < u2f(0x41a00000u)) sw(P + 0x2048, 1);
    }
    ee_stack_free(0x10);
}

namespace {
// the level's count of the zone markers met this frame (a word of its small data)
uint32_t zone_marker_counter() {
    if (on_level(NOVALIS)) return L1_ADDR(0x00161af0);
    if (on_level(ARIDIA)) return L2_ADDR(0x00161a60);
    if (on_level(KERWAN)) return L3_ADDR(0x00161990);
    if (on_level(EUDORA)) return L4_ADDR(0x001619e0);
    if (on_level(RILGAR)) return L5_ADDR(0x00161a70);
    if (on_level(BLARG)) return L6_ADDR(0x00161960);
    if (on_level(UMBRIS)) return L7_ADDR(0x00161930);
    if (on_level(BATALIA)) return L8_ADDR(0x00161e30);
    if (on_level(GASPAR)) return L9_ADDR(0x001619f0);
    if (on_level(ORXON)) return L10_ADDR(0x00161950);
    if (on_level(POKITARU)) return L11_ADDR(0x00161ac0);
    if (on_level(HOVEN)) return L12_ADDR(0x00161b20);
    if (on_level(GEMLIK)) return L13_ADDR(0x00161a20);
    if (on_level(OLTANIS)) return L14_ADDR(0x00161c00);
    if (on_level(QUARTU)) return L15_ADDR(0x00161bc0);
    if (on_level(KALEBO)) return L16_ADDR(0x00161a70);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161a50);
    return L18_ADDR(0x00161ab8);
}
}  // namespace

// Class 605 on the level whose overlay is loaded (its counter from the table above).
// orig: L1:0x002f2eb8 L2:0x002d74e0 L3:0x002cbc38 L4:0x002d46b8 L5:0x00301cd8 L6:0x002e1518 L7:0x002fd698 L8:0x002f1c08 L9:0x002f7658 L10:0x002c6470 L11:0x002fc980 L12:0x002f5540 L13:0x002f51f0 L14:0x002ead20 L15:0x002d5728 L16:0x002d2a80 L17:0x002d3da0 L18:0x002d8ba8
void zone_marker_update(uint32_t m) { zone_marker_update_at(m, zone_marker_counter()); }

// orig: L1:0x002f2eb8 (the counter at 0x161af0)
void zone_marker_update_l1(uint32_t m) { zone_marker_update_at(m, L1_ADDR(0x00161af0)); }

// orig: L2:0x002d74e0 (the counter at 0x161a60)
void zone_marker_update_l2(uint32_t m) { zone_marker_update_at(m, L2_ADDR(0x00161a60)); }

// orig: L3:0x002cbc38 (Kerwan's build: the same code, the counter at gp - 0x5270)
void zone_marker_update_l3(uint32_t m) { zone_marker_update_at(m, L3_ADDR(0x00161990)); }

// Classes 531 (Green Ship) and 1816: the update function is a bare `jr ra` (nothing to do: static scenery; no
// instance on Novalis).
// orig: L1:0x002ece90
void empty_update_2ece90_l1(uint32_t) {}

// orig: L1:0x0030d200
void empty_update_30d200_l1(uint32_t) {}

// Class 676: appends the draw hook 0x2f60f8 (the water-surface strips of the scene: GS packets, not ported) for
// this moby to the draw callbacks (sub_001f93e0).
// orig: L1:0x002f6128
void draw_hook_676_update_l1(uint32_t m) { sub_001f93e0(L1_ADDR(0x002f60f8), m); }

// Class 678: on the first call works out, for each of the two records at 0x1e3380 (0x60 bytes: +0 the list of
// points, +8 where the sphere goes, +0xc the count), the points' centroid and the largest distance of one
// from it (the sphere's radius in w), stored at +8; then appends the draw hook 0x2f6150 for this moby.
// orig: L1:0x002f6180
void draw_hook_678_update_l1(uint32_t m) {
    if (lw(L1_ADDR(0x00161ba0)) == 0) {
        sw(L1_ADDR(0x00161ba0), 1);
        const uint32_t base = L1_ADDR(0x001e3380);
        const uint32_t buf = ee_stack_alloc(0x20);
        for (uint32_t k = 0; k < 2; ++k) {
            const uint32_t rec = base + k * 0x60;
            vec_clear(V(buf));
            uint32_t p = lwu(rec);
            const int32_t n = lw(rec + 0xc);
            for (int32_t i = 0; i < n; ++i, p += 0x10) vec3_add(V(buf), V(buf), V(p));
            vec3_scale(V(buf), V(buf), fpu_div(1.0f, int_to_float(lw(rec + 0xc))));
            sw(buf + 0xc, 0);
            p = lwu(rec);
            for (int32_t i = 0; i < lw(rec + 0xc); ++i, p += 0x10) {
                vec3_sub(V(buf + 0x10), V(buf), V(p));
                const float len = vec3_length(V(buf + 0x10));
                if (lwc1(buf + 0xc) < len) swc1(buf + 0xc, len);
            }
            lq_sq(lwu(rec + 8), buf);
        }
        ee_stack_free(0x20);
    }
    sub_001f93e0(L1_ADDR(0x002f6150), m);
}

// Class 1225: hides itself (+0x30 / +0x32 = 0xff) and appends the draw hook 0x309bf8 for it.
// orig: L1:0x00309c98
void draw_hook_1225_update_l1(uint32_t m) {
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    sub_001f93e0(L1_ADDR(0x00309bf8), m);
}

}  // namespace rac1
