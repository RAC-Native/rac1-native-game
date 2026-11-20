// Oltanis (level 14), four small classes of part 4 (P14.4): class 1352 (a group of mobys kept 20 below their places,
// hidden, until the moby it watches (pvars +0xc) reaches its state 2: then (its save bits set, the scripted camera's entry
// +0x10 marked) they rise eased into place over 120 frames), class 1397 (its record set up, and its state 2 turned into
// 3), class 1418 (the sky-reflecting objects' owner: it queues their draw, env_objects_callback at L14:0x307a28, every
// frame) and class 1559 (a group's watcher: once its group (pvars +0) has no member left, the flag 0x13d41f is set
// once, with a sound and a message; during scenes 3 .. 5 it puffs smoke at the scene's second moby). Ported by hand from
// Oltanis's own code (L14:0x305408, 0x305600, 0x305680, 0x3061d8, 0x307a80, 0x3087c0), the original's order of calls and
// float operations kept.
#include <game/level/level_data.h>
#include <game/ovl/aridia_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/veldin_mobys.h>

#include <cstdint>

namespace rac1 {

namespace {
uint32_t level_moby(int32_t index) { return lwu(L14_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8); }

// a moby's bit by its uid in a table of words (the save's bits of this level, the level's own)
void set_uid_bit(uint32_t table, uint32_t m) {
    const uint32_t uid = lhu(m + 0xb2);
    const uint32_t w = table + (static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(uid)) >> 5) << 2);
    sw(w, lwu(w) | (1u << (uid & 0x1f)));
}

// The group's members (pvars +0: the group; its list of moby indices, the last with bit 15) put 20 below their places
// and hidden, each place kept in the level's table 0x1ee5e0 (one word per member, in the list's order).
// orig: L14:0x00305600
void group_sink(uint32_t m) {
    uint32_t e = lvd::group(lw(lwu(m + 0x78)));
    if (e == 0) return;
    uint32_t keep = L14_ADDR(0x001ee5e0);
    for (;; e += 2) {
        const uint32_t o = level_moby(static_cast<int32_t>(lhu(e) & 0x7fffu));
        swc1(keep, lwc1(o + 0x18));
        keep += 4;
        const float z = fpu_sub(lwc1(o + 0x18), 20.0f);
        sh(o + 0x34, lhu(o + 0x34) | 0x41u);
        swc1(o + 0x18, z);
        if (lh(e) < 0) break;
    }
}

// The group's members shown, risen from 20 below their kept places by the eased share of the timer run out (pvars +4,
// one over its length at +8).
// orig: L14:0x00305680
void group_rise(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t e = lvd::group(lw(pv));
    if (e == 0) return;
    const float t = cubic_interp(-1.0f, 0.0f, 1.0f, 0.0f, fpu_sub(1.0f, fpu_mul(int_to_float(lw(pv + 4)), lwc1(pv + 8))));
    uint32_t keep = L14_ADDR(0x001ee5e0);
    for (;; e += 2) {
        const float top = lwc1(keep);
        keep += 4;
        const float low = fpu_sub(top, 20.0f);
        const uint32_t o = level_moby(static_cast<int32_t>(lhu(e) & 0x7fffu));
        sh(o + 0x34, lhu(o + 0x34) & 0xffbeu);
        swc1(o + 0x18, fpu_add(low, fpu_mul(fpu_sub(top, low), t)));
        if (lh(e) < 0) break;
    }
}
}  // namespace

// orig: L14:0x00305408
void olt_class1352_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x00305458) set up: its watched moby, group and camera entry; done before: at rest (3)
        if (lw(pv + 0xc) < 0 || lw(pv) < 0 || lw(pv + 0x10) < 0) {
            moby_retire(m);
            return;
        }
        const uint32_t uid = lhu(m + 0xb2);
        const int32_t id = static_cast<int16_t>(uid);
        if (lbu(L14_ADDR(0x001bb930) + static_cast<uint32_t>(id) + 0x454) != 0 ||
            ((lw(0x0014c190u + (lwu(0x0015ed84u) << 8) + (static_cast<uint32_t>(id >> 5) << 2)) >> (uid & 0x1f)) & 1) != 0) {
            sb(m + 0x20, 3);
            return;
        }
        sb(m + 0x20, 1);
        group_sink(m);
        return;
    }
    case 1: {  // (0x003054fc) its watched moby in state 2: done (its bits), the rise started, the camera entry marked
        const uint32_t watched = lbu(level_moby(lw(pv + 0xc)) + 0x20);
        if (watched != 2) return;
        set_uid_bit(0x0014c190u + (lwu(0x0015ed84u) << 8), m);
        set_uid_bit(L14_ADDR(0x001babd0), m);
        sb(m + 0x20, watched);
        const int32_t len = frame_scale_i(0x78);
        sw(pv + 4, static_cast<uint32_t>(len));
        swc1(pv + 8, fpu_div(1.0f, int_to_float(len)));
        cam_entry_mark(lw(pv + 0x10));
        return;
    }
    case 2:  // (0x003055c8) rising
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 4)));
        group_rise(m);
        if (lw(pv + 4) == 0) sb(m + 0x20, 3);
        return;
    default:
        return;
    }
}

// orig: L14:0x003061d8
void olt_class1397_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x003061f8)
        sb(pv + 0x28, 4);
        sh(pv + 0x3e, 0);
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        sb(m + 0x20, 1);
        break;
    case 2:  // (0x0030621c)
        sh(pv + 0x3e, 5);
        sb(m + 0x20, 3);
        break;
    default:
        break;
    }
}

// orig: L14:0x00307a80
void olt_class1418_update_l14(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:  // (0x00307aa8)
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        break;
    case 1:  // (0x00307abc) the objects' draw (env_objects_callback) queued
        draw_hook_add_dc80_l2(L14_ADDR(0x00307a28), m);
        break;
    default:
        break;
    }
}

// orig: L14:0x003087c0
void olt_class1559_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t group = lw(pv);
    if (group != -1 && moby_group_count_l1(group, -1) == 0 && lbu(0x0013d41fu) == 0) {
        sb(0x0013d41fu, 1);
        sound_play_indexed(1, 0, 0);
        sub_00263d40(TXT(0x53d6), -1);
    }
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {  // (0x00308848)
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1) return;
    // (0x0030885c) during scenes 3 .. 5: smoke at the scene's moby (its second; the first if the game mode were past the
    // scene's offset, which it never is)
    const uint32_t mode = lwu(L14_ADDR(0x0015f5c4));
    if (mode != 2) return;
    const uint32_t S = L14_ADDR(0x0016cf60);
    const uint32_t scene = lwu(S + 0x30) - 3u;
    if (!(scene < 3)) return;
    const uint32_t which = mode < scene ? 0u : st;
    scene_moby_smoke(lwu(S + (which << 2) + 0x178));
}

}  // namespace rac1
