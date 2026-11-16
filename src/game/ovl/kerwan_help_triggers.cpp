// Kerwan's own moby class 1342, the level's help triggers (Veldin's are class 1413): boxes of the level (its pvars) that
// show a help message (0x203908) when the hero is in one, standing (his state 0x1413dc below 2 or 9), no message is up (the
// help state 0x179410: none shown, no voice line), and that help was not seen before (its stat at 0x141968 + 8 * index:
// its count, or the sign of its levels word); and boxes that note the hero's game events (their stats at 0x141848 + 8 *
// event: the count, the first time in 10 s units, the levels seen on). Header kerwan_mobys.h, registered in
// kerwan_mobys.cpp. Addresses are Kerwan's (`orig: L3:`); the engine's words shared with Veldin are the level's own (OV),
// the base ELF's (the hero 0x13f350, the game's flags 0x13d3xx, the stats, the clock 0x15eea4) are bare.
#include <game/math/vecmath.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;         // the hero
constexpr uint32_t HERO_POS = 0x0013f3d0;  // his position
constexpr OvConst<0x00179410u> HELP{};     // the help state: +0 the message shown, +0x24 its voice line

// the hero standing (his move state below 2, or 9)
bool standing() {
    const uint32_t st = lwu(P + 0x208c);
    return st < 2u || st == 9u;
}
// no help message up
bool help_free() { return lw(HELP) == 0 && lw(HELP + 0x24) == -1; }
// a help's stat record (count +0, first time +2, levels +4)
uint32_t help_rec(uint32_t i) { return 0x00141968 + i * 8; }
// a game event's stat record (the same layout)
uint32_t event_rec(uint32_t k) { return 0x00141848 + k * 8; }
// an event noted: its count up (by one from 0, or `saturate`: kept below 0xffff), its first time (frames of the game
// clock / 600) if later, this level's bit and the seen bit in its levels word
void event_note(uint32_t rec, bool saturate) {
    const uint32_t n = lhu(rec);
    if (!saturate || !(0xfffeu < n)) sh(rec, n + 1);
    const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
    if (static_cast<int32_t>(lhu(rec + 2)) < q) sh(rec + 2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
    sw(rec + 4, lwu(rec + 4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
}
// the hero in the box of the pvar word `word`
bool in_box(uint32_t word) { return point_in_box(HERO_POS, lw(word)) != 0; }
// a help (text `id`, index `i`) from the box `word`, for the hero standing in it, no message up and `unseen`
void box_help(uint32_t word, int32_t id, int32_t i, bool (*unseen)(uint32_t)) {
    if (!in_box(word) || !standing() || !help_free()) return;
    if (unseen(help_rec(static_cast<uint32_t>(i)))) sub_00203908(id, i);
}
bool never_seen(uint32_t rec) { return lw(rec + 4) >= 0; }  // (its levels word without the seen bit)
bool never_shown(uint32_t rec) { return lhu(rec) == 0; }    // (its count 0)
}  // namespace

// Class 1342's pvars: +0 / +0xc / +0x18 the boxes of the helps 0x14 / 0x15 / 0x17 (until the flag 0x13d3a2 is set, which the
// box +0x78 sets for the hero standing in it), +0x24 the box where the hero's jumps (+0x50, one per 60 frames off the ground,
// +0x58 the wait) give help 0x16 every sixth jump until event 3 (the box +0x30) is noted, +0x28 help 0x18 an hour after
// event 9, +0x2c help 0x1b with the item 8 in hand (state 6) until event 0x10 (the hero's state 0x74), +0x34 event 0xf and
// the hero's hits (+0x54, P+0x988) giving help 0x1a from the third, +0x38 helps 0x40 (fewer than 3 of the level's word
// 0x15f68c) and 0x41 (state 1, after 0x40), +0x3c event 0x13 (the hero's state 0x1e) and help 0x4a (bolts' count 0xf over
// 20, the flag 0x13d4cf; no enemy of kind 5 within 8), +0x40 help 0x54, +0x44 help 0x79, +0x70 the flag 0x13d3a0, +0x74
// the help message 0x53d6 once (the flag 0x13d40b), +0x7c help 0x4f (two weapons owned, of the 37 slots 0x179ac8 not
// held, 8 and 24 left out); help 0x19 in gameplay with the flag 0x13d4f4.
// orig: L3:0x002df520
void kerwan_class1342_update_l3(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x58)));
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sw(pv + 0x54, 0);
        sw(pv + 0x50, 0);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1) return;
    if (in_box(pv + 0x78) && standing()) sb(0x0013d3a2, 1);  // (0x13d388 + 0x1a: the state, 1)
    if (lbu(0x0013d3a2) == 0) {
        box_help(pv + 0x00, 0xbb8, 0x14, never_seen);
        box_help(pv + 0x0c, 0xbb9, 0x15, never_seen);
        box_help(pv + 0x18, 0xbbb, 0x17, never_seen);
    }
    if (in_box(pv + 0x30) && lhu(event_rec(3)) == 0 && standing()) event_note(event_rec(3), false);
    // the jumps in the box +0x24
    bool timer_check = true;
    if (in_box(pv + 0x24) && lhu(event_rec(3)) == 0 && !standing() && lw(pv + 0x58) == 0) {
        sw(pv + 0x50, lwu(pv + 0x50) + 1);
        sw(pv + 0x58, static_cast<uint32_t>(frame_scale_i(0x3c)));
        timer_check = false;
    }
    if (timer_check && lw(pv + 0x58) != 0 && standing()) sw(pv + 0x58, 0);
    if (in_box(pv + 0x24) && lhu(event_rec(3)) == 0 && standing()) {
        const int32_t jumps = lw(pv + 0x50);
        if (ee_rem(jumps, 6) == 0 && jumps != 0 && help_free() && lhu(help_rec(0x16)) == 0) sub_00203908(0xbba, 0x16);
    }
    if (in_box(pv + 0x28) && standing() && help_free() && lhu(help_rec(0x18)) == 0) {
        const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(event_rec(9) + 2)) * 0x258;
        if (frame_scale_i(0x34bc0) < since) sub_00203908(0xbbc, 0x18);
    }
    // the enemies of kind 5 (their class data +0x46) within 8 of the hero, on the level's update list
    int32_t near = 0;
    for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
        const uint32_t cd = lwu(o + 0x24);
        if (cd == 0 || lh(cd + 0x46) != 5) continue;
        if (vec2_distance(V(o + 0x10), V(HERO_POS)) < 8.0f) ++near;
    }
    bool hint = false;
    if (in_box(pv + 0x3c) && near == 0 && standing() && lw(P + 0x2084) != 0x1e && lbu(0x0013d4cf) != 0 &&
        !(static_cast<int32_t>(sub_00233e98(0xf)) < 21) && help_free() && lhu(help_rec(0x4a)) == 0 && lhu(event_rec(0x13)) == 0) {
        sub_00203908(0x4e20, 0x4a);
        hint = true;
    }
    if (!hint && lw(P + 0x2084) == 0x1e) event_note(event_rec(0x13), true);
    if (in_box(pv + 0x38) && standing() && lw(OV(0x0015f68c)) < 3 && help_free() && lhu(help_rec(0x40)) == 0) sub_00203908(0x3ec, 0x40);
    if (in_box(pv + 0x38) && standing() && lw(P + 0x2084) == 1 && lw(OV(0x0015f688)) == -1 && help_free() &&
        lhu(help_rec(0x40)) != 0 && lhu(help_rec(0x41)) == 0)
        sub_00203908(0x3ed, 0x41);
    box_help(pv + 0x40, 0xbc0, 0x54, never_shown);
    if (lw(OV(0x0015f5c4)) == 0 && lbu(0x0013d4f4) != 0 && help_free() && lhu(help_rec(0x19)) == 0) sub_00203908(0xbbd, 0x19);
    if (in_box(pv + 0x34) && lhu(event_rec(0xf)) == 0 && standing()) event_note(event_rec(0xf), false);
    // the hero's hits (his word +0x988) until event 0xf: help 0x1a from the third
    if (lw(P + 0x988) != 0 && lhu(event_rec(0xf)) == 0) {
        sw(P + 0x988, 0);
        sw(pv + 0x54, lwu(pv + 0x54) + 1);
    }
    if (lhu(event_rec(0xf)) == 0 && standing() && !(lw(pv + 0x54) < 3) && help_free() && lhu(help_rec(0x1a)) == 0) sub_00203908(0xbbe, 0x1a);
    if (lw(P + 0x2084) == 0x74 && lhu(event_rec(0x10)) == 0) event_note(event_rec(0x10), false);
    if (in_box(pv + 0x2c) && lhu(event_rec(0x10)) == 0 && lw(P + 0x208c) == 6 && lw(P + 0x10b8) == 8 && help_free() &&
        lhu(help_rec(0x1b)) == 0)
        sub_00203908(0xbbf, 0x1b);
    box_help(pv + 0x44, 0x4e2f, 0x79, never_shown);
    if (in_box(pv + 0x7c) && lwu(P + 0x208c) < 2u && lhu(help_rec(0x4f)) == 0 && lhu(event_rec(0x14)) == 0) {
        int32_t owned = 0;
        for (uint32_t i = 0; i < 37; ++i) {
            if (lw(OV(0x00179ac8) + i * 0x4c) != 0 || i == 8 || i == 0x18) continue;
            if (lbu(0x0013d4c0 + i) != 0) ++owned;
        }
        if (!(owned < 2)) sub_00203908(0x4e25, 0x4f);
    }
    if (in_box(pv + 0x70)) sb(0x0013d3a0, 1);  // (0x13d388 + 0x18)
    if (in_box(pv + 0x74) && lbu(0x0013d40b) == 0) {
        sb(0x0013d40b, 1);
        sound_play_indexed(1, 0, 0);
        sub_00263d40(TXT(0x53d6), -1);
    }
}

}  // namespace rac1
