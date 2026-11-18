// Hoven's (level 12) scene, barrier and waterfall mesh classes: class 1345 (energy barrier tied to moby `pv + 0`, queuing
// draw callback 0x308350), class 1404 (box trigger starting cutscene `pv + 8`, with the 1.02 walk-to byte fork), class 1557
// (scene moby smoke during scenes 1 and 7), and class 1901 (waterfall / ice UV-scroll controller queuing draw callback
// 0x30be68). Level 12 only: `orig: L12:`, Hoven-only data L12_ADDR, base ELF bare. Registered in hoven_mobys.cpp.
#include <game/math/vecmath.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_scroll.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

// Class 1345's update: retires in state 0 when its checkpoint is already collected; in state 1 queues draw callback
// L12:0x308350 while on screen, starts hum sound 0, and shuts down into state 2 once partner moby `pv + 0` reaches state 7.
// orig: L12:0x003081b0
void hov_class1345_update_l12(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (st == 0) {
        const uint32_t cp = lbu(m + 0xb0);
        if (cp != 0xff && lbu(0x0014c050u + (lwu(0x0015ed84u) << 4) + cp) == 0xff) {
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 1);
    } else if (st == 1) {
        if (moby_on_screen_l1(m, 48.0f) != -1) {
            sub_001f93e0(L12_ADDR(0x00308350), m);
        }
        if (lw(pv + 4) == -1) {
            sw(pv + 4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        }
        if (lw(pv + 0) < 0) return;

        // (as in the original: stops the looping sound and clears `pv + 4` whenever `pv + 0 >= 0`, before checking state 7)
        const int32_t ch = lw(pv + 4);
        if (ch != -1) {
            const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
            if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) sub_0028d918(ch);
        }
        const uint32_t partner_idx = lwu(pv + 0);
        sw(pv + 4, 0xffffffffu);
        const uint32_t partner = lwu(L12_ADDR(0x0015ffd8)) + (partner_idx << 8);
        if (lbu(partner + 0x20) != 7) return;

        const uint32_t cp = lbu(m + 0xb0);
        if (cp != 0xff) checkpoint_mark_collected(cp);
        moby_sound_start(1, 0, m);
        sw(m + 0x94, 0);
        sb(m + 0x20, 2);
    }
}

// Class 1404's update: retires once progress flag `0x13d3e4 + *(pv + 4)` is set; when the hero enters box `pv + 0`, marks
// the flag, records the hero's post-scene walk-to pose in the scene block (byte `+0x4a` on the 1.02 build, halfword `+0x46`
// otherwise), and starts cutscene `pv + 8`.
// orig: L12:0x003093c8
void hov_class1404_update_l12(uint32_t m) {
    if (lw(L12_ADDR(0x0015f5cc)) == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (lw(L12_ADDR(0x0015f5c4)) != 0) return;
    if (lbu(0x0013d388u + lwu(pv + 4) + 0x5c) != 0) {
        moby_retire(m);
        return;
    }
    constexpr uint32_t kHero = 0x0013f3d0u;
    if (point_in_box(kHero, lw(pv + 0)) == 0) return;

    sb(0x0013d388u + lwu(pv + 4) + 0x5c, 1);
    const uint32_t scene = OV(0x0016c860);
    lq_sq(scene + 0x10, kHero);
    swc1(scene + 0x18, lwc1(kHero + 0x258));
    lq_sq(scene + 0x20, kHero + 0x10);
    if (region_core_moved()) sb(scene + 0x4a, 1);  // (region: the 1.02 build's walk-to flag, a byte at +0x4a)
    else sh(scene + 0x46, 1);
    cutscene_start(lw(pv + 8));
}

// Class 1557's update: emits smoke on scene moby slot 2 during cutscene 1 and slot 0 during cutscene 7.
// orig: L12:0x003094a0
void hov_class1557_update_l12(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
    } else if (st == 1) {
        if (lw(L12_ADDR(0x0015f5c4)) != 2) return;
        const uint32_t scene = OV(0x0016c860);
        const int32_t id = lw(scene + 0x30);
        if (id != 1 && id != 7) return;
        const uint32_t slot = id == 1 ? 2u : 0u;
        scene_moby_smoke(lwu(scene + 0x178 + (slot << 2)));
    }
}

// Class 1901's update: scales its waterfall / ice mesh vertex colours once (`0x1620f0`), wraps three UV scroll pairs in
// `[-1, 1]` by `0x15ed7c`, and queues draw callback L12:0x30be68.
// orig: L12:0x0030bff0
void hov_class1901_update_l12(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        if (lw(L12_ADDR(0x001620f0)) == 0) {
            const float r = u2f(0x3dcccccdu), b = u2f(0x3ecccccdu);
            mesh_colours_scale(L12_ADDR(0x00208ae0), 5, L12_ADDR(0x00205a80), 1, r, 0.5f, b, 0.5f);
            mesh_colours_scale(L12_ADDR(0x00205a58), 10, L12_ADDR(0x001fbd10), 1, r, u2f(0x3f19999au), b, 0.5f);
            sw(L12_ADDR(0x001620f0), 1);
        }
        sb(m + 0x20, 1);
        sh(m + 0x32, 0xff);
        sb(m + 0x30, 0xff);
        for (uint32_t i = 0; i < 2; ++i) {
            sw(L12_ADDR(0x001620f8) + i * 8, 0);
            sw(L12_ADDR(0x001620fc) + i * 8, 0);
        }
    } else if (st == 1) {
        auto step_uv = [](uint32_t addr, float rate, float dt) {
            const float next = fpu_add(lwc1(addr), fpu_mul(rate, dt));
            swc1(addr, next);
            if (1.0f < next) swc1(addr, fpu_sub(next, 1.0f));
            const float wrapped = lwc1(addr);
            if (wrapped < -1.0f) swc1(addr, fpu_add(wrapped, 1.0f));
        };
        const float dt0 = lwc1(0x0015ed7cu);
        for (uint32_t i = 0; i < 2; ++i) {
            step_uv(L12_ADDR(0x001620f8) + i * 8, lwc1(L12_ADDR(0x001620e0) + i * 8), dt0);
            step_uv(L12_ADDR(0x001620fc) + i * 8, lwc1(L12_ADDR(0x001620e4) + i * 8), dt0);
        }
        step_uv(L12_ADDR(0x00162108), lwc1(L12_ADDR(0x001620d8)), lwc1(0x0015ed7cu));
        step_uv(L12_ADDR(0x0016210c), lwc1(L12_ADDR(0x001620dc)), lwc1(0x0015ed7cu));
        draw_hook_add_dc80_l2(L12_ADDR(0x0030be68), m);
    }
}

}  // namespace rac1
