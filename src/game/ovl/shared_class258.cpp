// Class 258, a zone that changes the mobys it names while the hero (or the camera) is in it: one port of the copies nine
// levels carry (Rilgar, Umbris, Gaspar, Orxon, Hoven, Gemlik, Oltanis, Quartu, Drek's fleet). The copies are the same code
// relinked (compared with the relocated fields masked); what they name is the engine's (OV: the moby table 0x15ffd8,
// the camera 0x166dc0, the level's groups 0x1ab840, the paths 0x1b04b0) or the base ELF's (bare: the hero 0x13f3d0).
// Header shared_class258.h.
//
// The zone's pvars: a polygon (a path's index) +0, a radius +4, six boxes +8, twelve groups +0x20, eleven moby indices
// +0x50; what it does inside (outside: the opposite): +0x7c > 0 shows the mobys (< 0 hides them), +0x7d > 0 wakes them
// (+0x31 1, < 0 puts them to sleep), +0x7f > 0 makes them solid (+0x94 their class's collision, < 0 none); +0x7e: the
// camera's position decides, not the hero's.
#include <game/ovl/shared_class258.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// Outside the zone the classes 0x386 / 0x3ae / 0x4ef keep their visibility, and those with 0x571 their sleep (the
// original's checks; inside it changes them all but the sleep of the four).
// orig: L5:0x002f4f60 L7:0x002f23e8 L9:0x002ea098 L10:0x002bab98 L12:0x002e4a38 L13:0x002e4930 L14:0x002db0d8 L15:0x002c7118 L17:0x002c7a10
void class258_apply(int32_t idx, uint32_t pv, uint32_t inside) {
    const uint32_t m = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
    if (m == 0) return;
    const int32_t cls = lh(m + 0xa6);
    const bool keeps_shown = cls == 0x386 || cls == 0x3ae || cls == 0x4ef;
    const bool keeps_awake = keeps_shown || cls == 0x571;
    const int32_t show = static_cast<int8_t>(lbu(pv + 0x7c));
    const int32_t wake = static_cast<int8_t>(lbu(pv + 0x7d));
    const int32_t solid = static_cast<int8_t>(lbu(pv + 0x7f));
    auto shown = [m](bool on) { sh(m + 0x34, on ? (lhu(m + 0x34) & 0xfffdu) : (lhu(m + 0x34) | 2u)); };
    auto awake = [m](bool on) {
        sb(m + 0x31, on ? 1u : 0u);
        sh(m + 0x34, on ? (lhu(m + 0x34) & 0xfffeu) : (lhu(m + 0x34) | 1u));
    };
    if (inside != 0) {
        if (show != 0) shown(show > 0);
        if (wake != 0 && !keeps_awake) awake(wake > 0);
        if (solid != 0 && lwu(m + 0x24) != 0) sw(m + 0x94, solid > 0 ? lwu(lwu(m + 0x24) + 0x10) : 0u);
    } else {
        if (show != 0 && !keeps_shown) shown(show < 0);
        if (wake != 0 && !keeps_awake) awake(wake < 0);
        if (solid != 0) sw(m + 0x94, solid < 0 ? lwu(lwu(m + 0x24) + 0x10) : 0u);
    }
}

// The group's moby indices (halves, the last one with bit 15 set).
// orig: L5:0x002f5190 L7:0x002f2618 L9:0x002ea2c8 L10:0x002badc8 L12:0x002e4c68 L13:0x002e4b60 L14:0x002db308 L15:0x002c7348 L17:0x002c7c40
void class258_apply_group(uint32_t zone, uint32_t pv, int32_t group, uint32_t inside) {
    (void)zone;
    uint32_t e = lwu(OV(0x001ab840) + (static_cast<uint32_t>(group) << 2));
    if (e == 0) return;
    for (;; e += 2) {
        class258_apply(static_cast<int32_t>(lhu(e) & 0x7fffu), pv, inside);
        if (lh(e) < 0) break;
    }
}

// The hero (or the camera) is in the zone when within its radius of the moby (a radius above 0), in its polygon, or in
// one of its boxes. On its first frame (+0x20 0) and whenever that changes, the zone's mobys and groups get its effect;
// +0xbc keeps the state (+0x20 is it + 1). Drawn always (+0x30 0xff).
// orig: L5:0x002f5200 L7:0x002f2688 L9:0x002ea338 L10:0x002bae38 L12:0x002e4cd8 L13:0x002e4bd0 L14:0x002db378 L15:0x002c73b8 L17:0x002c7cb0
void class258_update(uint32_t m) {
    sb(m + 0x30, 0xff);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t at = sp;
    lq_sq(at, static_cast<int8_t>(lbu(pv + 0x7e)) != 0 ? OV(0x00166dc0) : 0x0013f3d0u);
    bool inside = false;
    if (0.0f < lwc1(pv + 4)) inside = vec3_distance(V(m + 0x10), V(at)) < lwc1(pv + 4);
    if (!inside && lw(pv) != -1) {
        const uint32_t poly = lwu(OV(0x001b04b0) + (lwu(pv) << 2));
        inside = point_in_polygon(at, poly + 0x10, lw(poly)) != 0;
    }
    for (uint32_t i = 0; i < 6 && !inside; ++i) {
        const int32_t box = lw(pv + 8 + i * 4);
        if (box != -1 && point_in_box(at, box) != 0) inside = true;
    }
    const bool was_inside = lbu(m + 0xbc) != 0;
    if (inside != was_inside || lbu(m + 0x20) == 0) {
        sb(m + 0xbc, inside ? 1u : 0u);
        sb(m + 0x20, lbu(m + 0xbc) + 1);
        const uint32_t in = inside ? 1u : 0u;
        for (uint32_t i = 0; i < 12; ++i) {
            const int32_t group = lw(pv + 0x20 + i * 4);
            if (group != -1) class258_apply_group(m, pv, group, in);
        }
        for (uint32_t i = 0; i < 11; ++i) {
            const int32_t idx = lw(pv + 0x50 + i * 4);
            if (idx != -1) class258_apply(idx, pv, in);
        }
    }
    ee_stack_free(0x90);
}

}  // namespace rac1
