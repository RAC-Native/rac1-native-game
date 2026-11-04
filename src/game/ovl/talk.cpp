// The talk scripts: a talking moby of the level (the list 0x1791b8, the level's range
// 0x1c44b8[level..level+1]) has a script 0x1b1678[i] of records (0x1c bytes: +0 a text id, +4 the
// scene to play (-1 none, | 0x4000 a movie), +6 the next record, +8 a condition kind (1 bolts,
// 2 / 3 an item owned / not yet upgraded, 4 an item of 0x13d388, 5 / 6 gold bolts, 6 also the
// prompt's text formatted), +0xa its argument, +0xc / +0xe the record when it holds / not, +0x10
// flags (1 starts by itself, 4 plays the next scene straight away, 8 the condition's cost paid)),
// and per level index a state 0x13d5b0 + i * 0x10 (+0xc: talked to once). The Veldin ship moby
// (class 834) opens the first visit with its script's scene 4.
#include <game/ovl/cutscene.h>
#include <game/enhance/fixes102.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/libc.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

#include <vector>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;    // the hero
constexpr OvConst<0x00179100u> TK{};   // the talk state
constexpr uint32_t SPR = 0x70000000;  // the scratchpad
}  // namespace

// The index of moby `m` among the level's talking mobys (0x1791b8, from 0x1c44b8[level] to
// 0x1c44b8[level + 1]), -1 if it is not one; 0 past level 0x12.
// orig: 0x002667d0
int32_t talker_index(uint32_t m) {
    const uint32_t lvl = lwu(0x0015ed84);
    if (!(lvl < 0x13)) return 0;
    const int32_t hi = lw(OV(0x001c44b8) + (lvl + 1) * 4);
    for (int32_t i = lw(OV(0x001c44b8) + lvl * 4); i < hi; ++i) {
        if (lwu(OV(0x001791b8) + static_cast<uint32_t>(i) * 4) == m) return i;
    }
    return -1;
}

// The talk struct `t` of moby `m` set up from its script: +9 set, +4 none played, +0x38 the
// frame, +0x36 the first record (the first record's next one when talked to before and the
// first has no condition), +8 whether it starts by itself. The moby's index, or -1 (no script).
// orig: 0x002668a0
int32_t talk_init(uint32_t m, uint32_t t) {
    const int32_t i = talker_index(m);
    if (i == -1) return -1;
    const uint32_t slot = OV(0x001b1678) + static_cast<uint32_t>(i) * 4;
    if (lwu(slot) == 0) return -1;
    sb(t + 9, 1);
    const uint32_t st = 0x0013d5b0 + static_cast<uint32_t>(i) * 0x10;
    sh(t + 4, 0xffff);
    sw(t + 0x38, lwu(OV(0x0015f5cc)));
    sh(t + 0x36, 0);
    const uint32_t script = lwu(slot);
    sw(t + 0x3c, script);
    if (lw(st + 0xc) != 0 && lh(script + 8) == 0) sh(t + 0x36, lhu(script + 6));
    const uint32_t rec = lwu(t + 0x3c) + static_cast<uint32_t>(lh(t + 0x36) * 0x1c);
    sb(t + 8, lbu(rec + 0x10) & 1u);
    return i;
}

// A frame of moby `m`'s talk: when the hero is in his normal state, within twice the reach
// (+0xc) and no other mode runs, the record's prompt (talk_step); a record that does not start by
// itself also needs the hero facing the moby and the moby facing him (within the reach as an
// angle, 1.57) and the triangle button (else the "talk" prompt 0x754e is shown, 60 frames);
// from frame *(0x179110) on the record runs: the talk state set (talked to), the condition's cost
// paid (+0x10 & 8: kind 1 the bolts, kind 4 the item taken back), its scene played (game mode 2,
// 0x298840) or its movie (0x299460), or with no scene the next record. 1 when a
// record ran.
// orig: 0x00266448
int32_t talk_update(uint32_t m, uint32_t t) {
    if (lw(P + 0x2084) == 0x1d) return 0;
    if (lw(P + 0x22a8) == 0) return 0;
    int32_t mode = lw(OV(0x0015f5c4));
    if (lbu(t + 9) == 0) {
        talk_init(m, t);
        mode = lw(OV(0x0015f5c4));
    }
    if (mode != 0) return 0;
    if (t == 0) return 0;
    if (lh(t + 0x36) == -1) return 0;
    if (lbu(t + 8) == 0xff) return 0;
    const float d = vec3_distance(V(m + 0x10), V(P + 0x80));
    const float reach = lwc1(t + 0xc);
    if (reach + reach < d) return 0;
    // (1.02 Fixes, fix 11: the 1.02 build, 0x267290, calls talk_step only for a record that waits for the button, +8
    // 0, once the hero faces the moby and it faces him: not every frame within twice the reach)
    const bool step_late = fixes102_on(11);
    if (!step_late) talk_step(m, t, 0);
    if (lbu(t + 8) == 0) {
        const float a = atan2_f(lwc1(P + 0x80) - lwc1(m + 0x10), lwc1(P + 0x84) - lwc1(m + 0x14));
        if (lwc1(t + 0xc) < angle_dist(a, lwc1(m + 0x48))) return 0;
        const float b = atan2_f(lwc1(m + 0x10) - lwc1(P + 0x80), lwc1(m + 0x14) - lwc1(P + 0x84));
        if (u2f(0x3fc8f5c3u) < angle_dist(b, lwc1(P + 0x98))) return 0;
        if (step_late) talk_step(m, t, 0);
    }
    if (lw(OV(0x0015f5cc)) < lw(TK + 0x10)) return 0;
    if (lbu(t + 8) == 0 && (lwu(0x0013cae4) & 0x10) == 0) {
        // the prompt to talk (the hint key 0xc shown once, then 0x754e for 60 frames)
        const int32_t key = lw(OV(0x00160130));
        if (sub_00235e18(static_cast<uint32_t>(key), 0xa) != 0) {
            sw(OV(0x00160130), static_cast<uint32_t>(camera_request(0xc, 0, OVF(0x00237190), OVF(0x002371e0), OVF(0x00237200), 0, 0)));
        }
        const uint32_t rec = lwu(t + 0x3c) + static_cast<uint32_t>(lh(t + 0x36) * 0x1c);
        const int32_t kind = lh(rec + 8);
        if (kind != 1 && kind != 6) return 0;
        const int32_t s0 = camera_request(2, 0x754e, OVF(0x00239cc8), OVF(0x00239d00), OVF(0x00239df8), 0x0015ed98, 0x0098967f);
        sub_00235e18(static_cast<uint32_t>(s0), frame_scale_i(0x3c));
        return 0;
    }
    const int32_t i = talker_index(m);
    const uint32_t seen = 0x0013d5b0 + static_cast<uint32_t>(i * 0x10) + 0xc;
    if (lw(seen) == 0) sw(seen, 1);
    sw(TK + 0xc, t);
    sw(TK + 8, m);
    const uint32_t rec = lwu(t + 0x3c) + static_cast<uint32_t>(lh(t + 0x36) * 0x1c);
    if ((lhu(rec + 0x10) & 8) != 0) {
        const int32_t kind = lh(rec + 8);
        if (kind == 1) {
            const uint32_t row = OV(0x001c40b0) + static_cast<uint32_t>(lh(rec + 0xa) * 0x18);
            sw(0x0015ed98, lwu(0x0015ed98) - lwu(row));
        } else if (kind == 4) {
            sb(0x0013d388 + static_cast<uint32_t>(lh(rec + 0xa)), 0);
        }
    }
    if (lh(rec + 4) == -1) {
        talk_step(m, t, 1);
        return 1;
    }
    sw(TK + 0xc, t);
    sw(TK + 8, m);
    const uint32_t scene = lhu(rec + 4);
    if ((scene & 0x4000) != 0) {
        movie_play(static_cast<int16_t>(scene ^ 0x4000u));
        return 1;
    }
    hud_request_clear(lw(OV(0x00160130)));
    const int32_t sc = lh(rec + 4);
    sw(OV(0x00160130), 0xffffffffu);
    cutscene_start(sc);
    return 1;
}

// Moby `m`'s talk going on: with `next`, the record played (+4, +0x38 the frame) and the next one
// (+6) taken (a record flagged 4 plays the next one's scene or movie straight away); the record's
// text (+0 through 0x203ad8; kind 6: a price in the language's format 0x160150 with the bolts of
// the row +0xa, its "s" fixed with 0x160148) shown (0x237718) or the prompt cleared; then its
// condition (+8) picks the record +0xc (holds) or +0xe (not), stepped into when it changes.
// orig: 0x00266970
void talk_step(uint32_t m, uint32_t t, int32_t next) {
    const int32_t cur = lh(t + 0x36);
    const uint32_t script = lwu(t + 0x3c);
    const uint32_t curu = lhu(t + 0x36);
    uint32_t s1 = script + static_cast<uint32_t>(cur * 0x1c);
    if (script == 0) return;
    if (next != 0) {
        const uint32_t now = lwu(OV(0x0015f5cc));
        sh(t + 4, curu);
        sw(t + 0x38, now);
        const int32_t nx = lh(s1 + 6);
        if (cur != nx) {
            const uint32_t flags = lhu(s1 + 0x10);
            sh(t + 0x36, lhu(s1 + 6));
            s1 = static_cast<uint32_t>(nx * 0x1c) + script;
            sb(t + 8, lbu(s1 + 0x10) & 1u);
            if ((flags & 4) != 0) {
                sw(TK + 0xc, t);
                sw(TK + 8, m);
                const uint32_t scene = lhu(s1 + 4);
                if ((scene & 0x4000) != 0) {
                    movie_play(static_cast<int16_t>(scene ^ 0x4000u));
                } else {
                    hud_request_clear(lw(OV(0x00160130)));
                    sw(OV(0x00160130), 0xffffffffu);
                    cutscene_start(lh(s1 + 4));
                }
            }
        }
    }
    if (lh(t + 0x36) == -1) return;
    sw(TK + 0x14, lwu(s1));
    const uint32_t buf = ee_stack_alloc(0x140);  // the original's frame: the text +0, the price +0x120
    if (lh(s1 + 8) == 6) {
        libc_strcpy(SPR, sub_00203ad8(lw(s1)));
        const uint32_t at = libc_strstr(SPR, OV(0x00160148));
        if (at != 0) sb(at + 1, 0x73);
        const uint32_t row = OV(0x001c40b0) + static_cast<uint32_t>(lh(s1 + 0xa) * 0x18);
        const int32_t lang = lw(0x0015ed88);
        const int32_t bolts = static_cast<int32_t>(lhu(row + 0x14));
        const int32_t r6 = lang % 6;
        const std::vector<uint64_t> price = {static_cast<uint64_t>(static_cast<int64_t>(bolts / 1000)),
                                             static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x001e9120) + static_cast<uint32_t>(r6 * 4)))),
                                             static_cast<uint64_t>(static_cast<int64_t>(bolts % 1000))};
        libc_sprintf(buf + 0x120, OV(0x00160150), price);
        const std::vector<uint64_t> text = {buf + 0x120};
        libc_sprintf(buf, SPR, text);
    } else {
        const std::vector<uint64_t> none;
        libc_sprintf(buf, sub_00203ad8(lw(s1)), none);
    }
    if (lw(TK + 0x14) != 0) {
        hud_message_text_set(buf);
    } else {
        const int32_t key = lw(OV(0x00160130));
        if (key != -1) {
            hud_request_clear(key);
            sw(OV(0x00160130), 0xffffffffu);
        }
    }
    ee_stack_free(0x140);
    const int32_t kind = lh(s1 + 8);
    if (kind == 0) return;
    const int32_t k = static_cast<int16_t>(lhu(s1 + 8) - 1u);
    int32_t holds = 0;
    if (static_cast<uint32_t>(k) < 6u) {
        const int32_t arg = lh(s1 + 0xa);
        switch (k) {
            case 0: {  // the bolts reach the row's price
                const uint32_t row = OV(0x001c40b0) + static_cast<uint32_t>(arg * 0x18);
                holds = !(lw(0x0015ed98) < lw(row)) ? 1 : 0;
                break;
            }
            case 1:  // the item owned
                holds = lbu(0x0013d4c0 + static_cast<uint32_t>(arg)) != 0 ? 1 : 0;
                break;
            case 2:  // the item owned, not upgraded
                if (lbu(0x0013d4c0 + static_cast<uint32_t>(arg)) == 0) holds = 0;
                else holds = lbu(0x0013d4e8 + static_cast<uint32_t>(arg)) == 0 ? 1 : 0;
                break;
            case 3:  // the item of 0x13d388 owned
                holds = lbu(0x0013d388 + static_cast<uint32_t>(arg)) != 0 ? 1 : 0;
                break;
            case 4:  // the gold bolts
                holds = !(bytes_14bec0_left() < arg) ? 1 : 0;
                break;
            default: {  // case 5: the row's bolts (+0x14) and four gold bolts
                const uint32_t row = OV(0x001c40b0) + static_cast<uint32_t>(arg * 0x18);
                if (lw(0x0015ed98) < static_cast<int32_t>(lhu(row + 0x14))) holds = 0;
                else holds = !(bytes_14bec0_left() < 4) ? 1 : 0;
                break;
            }
        }
    }
    const int32_t to = holds != 0 ? lh(s1 + 0xc) : lh(s1 + 0xe);
    if (to == lh(t + 0x36)) return;
    const uint32_t now = lwu(OV(0x0015f5cc));
    const uint32_t sc = lwu(t + 0x3c);
    sh(t + 0x36, static_cast<uint32_t>(to));
    sw(t + 0x38, now);
    const uint32_t r = sc + static_cast<uint32_t>(to * 0x1c);
    sb(t + 8, lbu(r + 0x10) & 1u);
    talk_step(m, t, 0);
}

// The level's next planet asked for (*(0x15f5c0) = `level`), the frame cut short and the overlay's
// main loop left (0x15f570) for the level change.
// orig: 0x0028eeb0
void level_exit_to(int32_t level) {
    sw(OV(0x0015f5c0), static_cast<uint32_t>(level));
    sw(OV(0x0015f5d8), 1);
    sw(OV(0x0015f570), 1);
}

// The Veldin ship moby (class 834): hidden while a cutscene plays (mode 2, +0x31 0); state 0 its
// talk set up (0x141628 = 1), reach 255 on the first visit (its script's scene 4 at once) or 3
// with the prompt shown (+0x56) after it (0x13d390 set: the music no longer paused at the level's
// start); state 1 the talk each frame (reach 3, or 16 when the hero stands in the box *(pv+0x60)),
// 2 once a record ran (back to 1 out of mode 2); the record 4 reached (pv+0x24) outside mode 6:
// the level change to Novalis.
// orig: 0x002d9dc8
void veldin_ship_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(OV(0x0015f5c4)) == 2) {
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
    } else {
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
    }
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sh(0x00141628, 1);
        talk_init(m, pv + 0x20);
        if (lbu(0x0013d390) != 0) {
            sh(pv + 0x56, 1);
            swc1(pv + 0x2c, 3.0f);
            sh(pv + 0x24, 0);
        } else {
            swc1(pv + 0x2c, 255.0f);
        }
        sb(0x0013d390, 1);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st == 1) {
        if (lh(pv + 0x56) != 0) {
            if (point_in_box(P + 0x80, lw(pv + 0x60)) == 0) swc1(pv + 0x2c, 3.0f);
            else swc1(pv + 0x2c, 16.0f);
        }
        if (talk_update(m, pv + 0x20) != 0) {
            sb(m + 0x20, 2);
            swc1(pv + 0x2c, 3.0f);
        }
        if (lh(pv + 0x24) < 4) return;
        if (lw(OV(0x0015f5c4)) == 6) return;
        level_exit_to(1);
        return;
    }
    if (st == 2 && lw(OV(0x0015f5c4)) != 2) sb(m + 0x20, 1);
}

// orig: (none) — the class update above for the mobys' indirect calls (moby +0x74)
void talk_fns_register() {
    ovl_register_moby_fn(0x002d9dc8, veldin_ship_update);
}

}  // namespace rac1
