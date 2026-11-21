// Eudora's classes 432 and 434: parts driven by a Hydrodisplacer pump (class 280, shared_class280.cpp; its pvars +0 the
// work done, 0 .. 1), the pump's moby index in their pvars +0. Class 432 changes its model (class 0x1b0 <-> 0x41c) when
// the pump's work is done; class 434 follows the pump's work with its animation blend (+0x54), drives the pump back as
// it settles, and holds another moby (pvars +4) at its joint 0. Both play a loop sound while the work moves and a stop
// sound at either end. Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`);
// Eudora-only data is L4_ADDR, the base ELF's are bare.
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
constexpr uint32_t PUMP_CLASS = 0x118;

// the level's moby of index `idx` (the moby table L4:0x15ffd8, 0x100 bytes each)
uint32_t moby_at(int32_t idx) { return lwu(L4_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }

// The moby's sound in the slot whose number the word `word` holds stopped, if the slot still plays it; the word -1.
void sound_stop(uint32_t m, uint32_t word) {
    const int32_t slot = lw(word);
    if (slot != -1) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot) * 0x70u;
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(word, 0xffffffffu);
}

// The moby made of class `cls` (its index in the level's class table read at `index_at`, L4:0x197d40 + cls): its
// class record (the table L4:0x197480), size, flags and draw data from it.
void set_class(uint32_t m, uint32_t cls, uint32_t index_at) {
    sh(m + 0xa6, cls);
    const uint32_t idx = lbu(index_at);
    sb(m + 0x22, idx);
    const uint32_t rec = lwu(L4_ADDR(0x00197480) + (idx & 0xffu) * 4);
    sb(m + 0x71, 0xff);
    sw(m + 0x24, rec);
    swc1(m + 0x2c, lwc1(rec + 0x24));
    sub_0024ebc0(m);
    sb(m + 0x72, lbu(lwu(m + 0x24) + 0xe));
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    moby_update_transform_kept(m);
}
}  // namespace

// Class 432. States: 0 set up (animation 1; class 0x1b0 made 0x41c), 1 follows the pump, 2 done (its sound stopped).
// pvars: the pump +0, the loop sound's slot +4.
// orig: L4:0x002c6858
void eud_class432_update_l4(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) {
        moby_retire(m);
        return;
    }
    const float blend = lwc1(m + 0x54);
    switch (lbu(m + 0x20)) {
    case 0:
        moby_anim_set(m, 1, 0, frame_scale_i(0x12c));
        sb(m + 0x20, 1);
        sw(m + 0x58, 0);
        sw(pv + 4, 0xffffffffu);
        if (lh(m + 0xa6) == 0x1b0) set_class(m, 0x41c, L4_ADDR(0x0019815c));
        return;
    case 1: {
        if (lw(pv) == -1) return;
        const uint32_t pump = moby_at(lw(pv));
        if (lh(pump + 0xa6) != PUMP_CLASS) return;
        const uint32_t pp = lwu(pump + 0x78);
        const float work = lwc1(pp);
        swc1(m + 0x54, work);
        if (work == blend) {  // still: the loop sound stopped
            if (sub_0028d8c0(m, lw(pv + 4)) != 0) sound_stop(m, pv + 4);
            return;
        }
        if (lwc1(pp) == 1.0f || lwc1(pp) == 0.0f) {  // at an end: the loop stopped, the stop sound; done: class 0x1b0 again
            if (sub_0028d8c0(m, lw(pv + 4)) != 0) sound_stop(m, pv + 4);
            moby_sound_start(1, 0, m);
            if (lh(m + 0xa6) != 0x41c || lwc1(pp) != 1.0f) return;
            set_class(m, 0x1b0, L4_ADDR(0x00197ef0));
            sb(m + 0x20, 2);
            return;
        }
        if (sub_0028d8c0(m, lw(pv + 4)) == 0) sw(pv + 4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        return;
    }
    case 2:
        if (sub_0028d8c0(m, lw(pv + 4)) != 0) sound_stop(m, pv + 4);
        return;
    default:
        return;
    }
}

// Class 434. States: 0 set up (animation 1), 1 follows the pump (1 - its work; the pump being worked, state 3) and waits
// until the work is done and the hero stands on its held moby (or the pump is not worked), 2 settles (+0x54 up to 1 at
// half the frame time, the pump's work and its turn driven back). pvars: the pump +0, the held moby +4 (-1 none), the
// loop sound's slot +8, +0xc: no wait for the hero.
// orig: L4:0x002c6bb8
void eud_class434_update_l4(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) {
        moby_retire(m);
        return;
    }
    const float before = lwc1(m + 0x54);
    // the loop sound as the pump's work `work` moves (an end: the stop sound), stopped when +0x54 did not change
    auto sounds = [&](uint32_t pp) {
        if (lwc1(m + 0x54) == before) {
            if (sub_0028d8c0(m, lw(pv + 8)) != 0) sound_stop(m, pv + 8);
            return;
        }
        if (lwc1(pp) == 1.0f || lwc1(pp) == 0.0f) {
            if (sub_0028d8c0(m, lw(pv + 8)) != 0) sound_stop(m, pv + 8);
            moby_sound_start(1, 0, m);
            return;
        }
        if (sub_0028d8c0(m, lw(pv + 8)) == 0) sw(pv + 8, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
    };
    switch (lbu(m + 0x20)) {
    case 0:
        moby_anim_set(m, 1, 0, frame_scale_i(0x12c));
        sb(m + 0x20, 1);
        sw(m + 0x58, 0);
        sw(pv + 8, 0xffffffffu);
        break;
    case 1: {
        if (lw(pv) == -1) break;
        const uint32_t pump = moby_at(lw(pv));
        if (lh(pump + 0xa6) != PUMP_CLASS) break;
        const uint32_t pp = lwu(pump + 0x78);
        if (lbu(pump + 0x20) == 3) {
            swc1(m + 0x54, fpu_sub(1.0f, lwc1(pp)));
        } else if (lwc1(pp) != 1.0f) {
            sb(m + 0x20, 2);
        } else if (lw(pv + 4) != -1 && lw(pv + 0xc) == 0) {
            // the work done: settling once the hero (standing, state < 2, 9 or 0xc) stands on the held moby
            const uint32_t st = lwu(0x0013f350 + 0x208c);
            if ((st < 2 || st == 9 || st == 0xc) && lwu(0x0013f350 + 0x2fc) == moby_at(lw(pv + 4))) sb(m + 0x20, 2);
        }
        sounds(pp);
        break;
    }
    case 2: {
        const float up = fpu_add(before, fpu_mul(lwc1(0x0015ed6c), 0.5f));
        swc1(m + 0x54, 1.0f < up ? 1.0f : up);
        if (lw(pv) == -1) break;
        const uint32_t pump = moby_at(lw(pv));
        if (lh(pump + 0xa6) != PUMP_CLASS) break;
        const uint32_t pp = lwu(pump + 0x78);
        if (lbu(pump + 0x20) == 3) {  // worked again: following it
            sb(m + 0x20, 1);
            swc1(m + 0x54, fpu_sub(1.0f, lwc1(pp)));
        } else {  // the pump driven back: its work and its turn
            const float work = fpu_sub(1.0f, lwc1(m + 0x54));
            const float turn = fpu_mul(lwc1(pp + 0x30), u2f(0x40c90fdbu));
            swc1(pp, work);
            swc1(pp + 0x18, fpu_mul(work, turn));
        }
        sounds(pp);
        break;
    }
    default:
        break;
    }
    // the held moby at the joint 0
    if (const int32_t held = lw(pv + 4); held != -1) moby_chain_point(m, 0, moby_at(held) + 0x10);
}

}  // namespace rac1
