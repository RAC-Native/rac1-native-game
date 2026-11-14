// Ported overlay code: the level goals the pause menu shows (the goal records of the planet,
// their conditions and tests) and the counts behind the cheats page. See pause_menu.h.
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
// The goal tests of the records (+0x1c, a0 = +0x20) and the conditions of kind 7 (their
// argument is the test).
// orig: 0x0024e120 — a gold bolt found (0x13d4c0[i])
int32_t goal_test_24e120(uint32_t a0) { return lbu(0x0013d4c0 + a0) != 0 ? 1 : 0; }
// orig: 0x0024e138
int32_t goal_test_24e138(uint32_t) {
    constexpr uint32_t B = 0x0013d5b0;
    const uint32_t a = lwu(B + 0x14c), b = lwu(B + 0x18c);
    if (a != 0) {
        if (b == 0) return 1;
    } else if (b == 0) {
        return 0;
    }
    if (lbu(0x0013d4c2) == 0) return 0;
    return lwu(B + 0x16c) == 0 ? 2 : 0;
}
// orig: 0x0024e190
int32_t goal_test_24e190(uint32_t) {
    if (lwu(0x0013d8ac) == 0) return 0;
    return lbu(0x0013d3a8) == 0 ? 1 : 2;
}
// orig: 0x0024e1c0
int32_t goal_test_24e1c0(uint32_t) { return lbu(0x0013d3e9) != 0 && lbu(0x0013dd4d) != 0 ? 1 : 0; }
// orig: 0x0024dfe8
int32_t goal_test_24dfe8(uint32_t) { return lwu(0x0013d73c) != 0 && lbu(0x0013d3a0) != 0 ? 1 : 0; }
// orig: 0x0024e010
int32_t goal_test_24e010(uint32_t) {
    if (lwu(0x0013d8ac) == 0) return 0;
    if (lbu(0x0013d388 + 0x20) == 0) return 0;
    return lbu(0x0013d388 + 0x21) != 0 ? 1 : 0;
}
// orig: 0x0024e048
int32_t goal_test_24e048(uint32_t) { return lwu(0x0013d5b0 + 0x40c) != 0 && lwu(0x0013d5b0 + 0x3fc) != 0 ? 1 : 0; }
// orig: 0x0024e078
int32_t goal_test_24e078(uint32_t) { return lwu(0x0013d9dc) != 0 && lbu(0x0013d3dd) != 0 ? 1 : 0; }
// orig: 0x0024e0a0
int32_t goal_test_24e0a0(uint32_t) { return lwu(0x0013da1c) != 0 && lbu(0x0013d3e9) != 0 ? 1 : 0; }
// orig: 0x0024e0c8
int32_t goal_test_24e0c8(uint32_t) { return lwu(0x0013db2c) != 0 && lbu(0x0013d4fd) != 0 ? 1 : 0; }
// orig: 0x0024e0f0
int32_t goal_test_24e0f0(uint32_t) {
    if (lbu(0x0013d4c0 + 0x21) == 0) return 0;
    return lbu(0x0013d4c0 + 0x1f) != 0 ? 1 : 0;
}
// orig: 0x0024e1e8
int32_t goal_test_24e1e8(uint32_t) {
    if (lbu(0x0013d4d5) != 0) return 2;
    return lwu(0x0013db2c) != 0 ? 1 : 0;
}
// orig: 0x0024e210
int32_t goal_test_24e210(uint32_t) { return lbu(0x0013d4df) != 0 ? 1 : 0; }
// orig: 0x0024e220
int32_t goal_test_24e220(uint32_t) {
    if (lbu(0x0013d4c0 + 0x1f) != 0) return 2;
    return lbu(0x0013d4c0 + 0x21) != 0 ? 1 : 0;
}
}  // namespace

// orig: (helper) `jalr` to a goal record's test (the tests of the 19 planets' records)
int32_t goal_callback(uint32_t fn, uint32_t a0) {
    switch (ovl_code_to_veldin(fn)) {  // (the level's code address as Veldin's: the ports' cases)
        case 0x0024e120: return goal_test_24e120(a0);
        case 0x0024e138: return goal_test_24e138(a0);
        case 0x0024e190: return goal_test_24e190(a0);
        case 0x0024e1c0: return goal_test_24e1c0(a0);
        case 0x0024dfe8: return goal_test_24dfe8(a0);
        case 0x0024e010: return goal_test_24e010(a0);
        case 0x0024e048: return goal_test_24e048(a0);
        case 0x0024e078: return goal_test_24e078(a0);
        case 0x0024e0a0: return goal_test_24e0a0(a0);
        case 0x0024e0c8: return goal_test_24e0c8(a0);
        case 0x0024e0f0: return goal_test_24e0f0(a0);
        case 0x0024e1e8: return goal_test_24e1e8(a0);
        case 0x0024e210: return goal_test_24e210(a0);
        case 0x0024e220: return goal_test_24e220(a0);
        default: break;
    }
    TODO_PORT(fn, "goal test");
    return 0;
}

// A goal condition: kind 0 true, 1 the level visited (0x13dd40), 2 / 3 a gold bolt (0x13d4c0 /
// 0x13d4e8), 4 / 5 a moby flag set / past 1 (0x13d5b0 + i * 0x10 + 0xc, i < 0x79), 6 an item
// (0x13d388), 7 a test function, 8 a byte of the table 0x14bec0 (arg: row << 16 | column).
// orig: 0x0024dda8
int32_t goal_cond(int32_t kind, uint32_t arg) {
    const int32_t k = static_cast<int16_t>(kind);
    const int32_t a = static_cast<int32_t>(arg);
    switch (static_cast<uint32_t>(k) < 10u ? k : 9) {
        case 0: return 1;
        case 1: return lbu(0x0013dd40 + arg) != 0 ? 1 : 0;
        case 2: return lbu(0x0013d4c0 + arg) != 0 ? 1 : 0;
        case 3: return lbu(0x0013d4e8 + arg) != 0 ? 1 : 0;
        case 4:
            if (!(a < 0x79)) return 0;
            return lwu(0x0013d5b0 + arg * 16 + 0xc) != 0 ? 1 : 0;
        case 5:
            if (!(a < 0x79)) return 0;
            return lw(0x0013d5b0 + arg * 16 + 0xc) < 2 ? 0 : 1;
        case 6: return lbu(0x0013d388 + arg) != 0 ? 1 : 0;
        case 7: return goal_callback(arg, static_cast<uint32_t>(k << 16)) != 0 ? 1 : 0;
        case 8: {
            const uint32_t p = (arg & 0xffffu) + static_cast<uint32_t>((a >> 16) << 2) + 0x0014bec0u;
            return lbu(p) != 0 ? 1 : 0;
        }
        default: return 0;
    }
}

// The goals of the planet 0x184414 (records of 0x28 bytes from 0x186c70[planet], kept at
// 0x186d20): their state +0x24 (0 locked, 1 open, 2 done) from the two conditions +2/+4 and
// +8/+0xc (records flagged 4 only on a visited level), their test's result +0x26; 0x186d1c the
// count of open goals. True when none is open.
// orig: 0x0024dc08
int32_t goals_update() {
    constexpr OvConst<0x00186d10u> G{};
    const uint32_t first = lwu(OV(0x00186c70) + lwu(OV(0x00184414)) * 4);
    sw(G + 0x10, first);
    if (first == 0) return 0;
    for (uint32_t s = first; lh(s) != 0; s += 0x28) {
        if ((lhu(s + 0x10) & 4) != 0 && lbu(lwu(OV(0x00184414)) + 0x0013dd58) == 0) {
            sh(s + 0x24, 0);
            continue;
        }
        if (goal_cond(lh(s + 2), lwu(s + 4)) == 0) {
            sh(s + 0x24, 0);
            continue;
        }
        sh(s + 0x24, goal_cond(lh(s + 8), lwu(s + 0xc)) != 0 ? 2 : 1);
    }
    for (uint32_t s = lwu(G + 0x10); lh(s) != 0; s += 0x28) {
        if (lwu(s + 0x1c) != 0) sh(s + 0x26, static_cast<uint32_t>(goal_callback(lwu(s + 0x1c), lwu(s + 0x20))));
    }
    sw(G + 0xc, 0);
    for (uint32_t s = lwu(G + 0x10); lh(s) != 0; s += 0x28) {
        if (lh(s + 0x24) == 1 && (lhu(s + 0x10) & 2) == 0) sw(G + 0xc, lwu(G + 0xc) + 1);
    }
    return lwu(G + 0xc) == 0 ? 1 : 0;
}

// The skill points earned (0x13d408[0..31]), at most 30.
// orig: 0x00264640
int32_t skill_points_count() {
    int32_t n = 0;
    for (uint32_t i = 0; i < 0x20; ++i) if (lbu(0x0013d408 + i) != 0) ++n;
    if (n < 0) n = 0;
    return n < 0x1f ? n : 0x1e;
}

// The gold weapons owned (0x13e520[0..36]), at most 10.
// orig: 0x002645f8
int32_t gold_bolts_count() {
    int32_t n = 0;
    for (uint32_t i = 0; i < 0x25; ++i) if (lbu(0x0013e520 + i) != 0) ++n;
    if (n < 0) n = 0;
    return n < 0xb ? n : 0xa;
}

// The cheats page's entries (0x1b6900): enabled (action 3 / 0xa) or not (2) by the skill
// points (15, 30) and the gold weapons (10), their texts.
// orig: 0x00284ca0
void cheats_menu_entries() {
    const bool sp15 = !(skill_points_count() < 0xf);
    const bool sp30 = !(skill_points_count() < 0x1e);
    const bool gold = !(gold_bolts_count() < 0xa);
    constexpr OvConst<0x001b6900u> E{};
    sh(E + 0x3e, sp15 ? 3 : 2);
    sh(E + 0x4a, sp30 ? 3 : 2);
    sh(E + 0x56, gold ? 0xa : 2);
    sh(E + 0x62, gold ? 3 : 2);
    constexpr OvConst<0x001b6978u> T{};
    sw(T + 0x20, gold ? 0x4fd8 : 0x4fdb);
    sw(T + 0x14, sp15 ? 0x4fd3 : 0x4fd9);
    sw(T + 0x18, sp30 ? 0x4fd4 : 0x4fda);
    sw(T + 0x1c, gold ? 0x4fd7 : 0x4fdb);
    // region: the PAL builds (1.00 and 2.00) hide the page's last entry, Commercials (its text 0 ends the list there), on
    // the disc's region flag (1.00 0x002846f4..0x00284720; 2.00 the same code). Their discs have the three commercials.
    if (lw(0x0015ee90) != 0) sh(E + 0x60, 0);
}

}  // namespace rac1
