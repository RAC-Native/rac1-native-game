// Kalebo III's (level 16) class 257: a moby that runs once along a level path (pvar +0x4) at its speed (+0x8 a second) when
// Ratchet, free to move (his sub-state 0xf), comes into its box (+0x0); it hums along the way (sound 4, slot +0x14) and
// plays sound 1 once when it passes within 7 of him. At the path's end it hides and waits; once Ratchet is no longer free
// to move and the path's start is off screen, it is put back at the start. Level 16 only: `orig: L16:`, level data lvd::*,
// base ELF bare. Registered in kalebo_mobys.cpp; checked by batch 250 (class257).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/globals.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// Class 257. States: 0 sets up (retired without a box or a path; its step along the path a frame +0xc = speed x dt / the
// length of the path's first segment); 1 waits for Ratchet; 2 runs (+0x10 the place along the path); 3 waits at the end.
// orig: L16:0x002c3d38
void kal_class257_update_l16(uint32_t m) {
    const kalebo::Frame f(96);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv + 4));
    switch (lbu(m + 0x20)) {
    case 0: {
        if (lw(pv) == -1 || lw(pv + 4) == -1) { moby_retire(m); break; }
        const float first = vec3_distance(V(path + 0x10), V(path + 0x20));
        swc1(pv + 0xc, fpu_div(fpu_mul(lwc1(pv + 8), lwc1(0x0015ed6c)), first));
        sb(m + 0x20, 1);
        sb(m + 0x30, 0x64);
        sb(m + 0xbc, 0);
        break;
    }
    case 1:
        if (lw(g::HERO_SUBSTATE) != 0xf) break;
        if (point_in_box(0x0013f350u + 0xd0, lw(pv)) == 0) break;
        sb(m + 0x20, 2);
        sh(pv + 0x16, 0);
        break;
    case 2: {
        const float t = fpu_add(lwc1(pv + 0x10), lwc1(pv + 0xc));
        swc1(pv + 0x10, t);
        if (static_cast<float>(lw(path) - (1)) <= t) {
            sb(m + 0x30, 0xff);
            sb(m + 0x20, 3);
        } else {
            path_point(t, path, 0, m + 0x10, m + 0x40, 0);
        }
        if (lbu(m + 0xbc) != 0) sw(m + 0x94, 0);
        if (sub_0028d8c0(m, lh(pv + 0x14)) == 0) sh(pv + 0x14, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        if (lh(pv + 0x16) != 0) break;
        if (vec2_distance(V(m + 0x10), V(0x0013f3d0)) < (7.0f)) {
            sh(pv + 0x16, 1);
            moby_sound_start(1, 0, m);
        }
        break;
    }
    case 3:
        if (lbu(m + 0x31) == 0) {
            sb(m + 0x31, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
            sw(m + 0x94, 0);
        }
        if (lw(g::HERO_SUBSTATE) == 0xf) break;
        V(f.sp) = V(path + 0x10);
        swc1(f.sp + 0xc, 3.0f);
        if (point_on_screen(f.sp, 64.0f) != -1) break;
        sb(m + 0xbc, 0);
        sw(pv + 0x10, 0);
        path_point(lwc1(pv + 0x10), path, 0, m + 0x10, m + 0x40, 0);
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        sh(pv + 0x16, 0);
        sb(m + 0x20, 1);
        break;
    default:
        break;
    }
}

}  // namespace rac1
