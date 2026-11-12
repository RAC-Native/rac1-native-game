// Ported overlay code: game mode 4, the message screens: opened by 0x201730 (kind,
// argument) over the mode the game was in, updated by 0x202aa0 and drawn by 0x1f8ec8 / 0x2018c8.
// The state block 0x173040: +0 the kind (3: the memory card's screens, by the card state 0x15eeb0;
// 1 / 4 a message with up to three lines; ...), +4 a countdown, +8 / +0xc / +0x10 the lines'
// texts, +0x14 the mode to go back to, +0x18 the argument, +0x1c a step, +0x20 / +0x24 counters.

#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/snd989.h>
#include <game/enhance/bug_fixes.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/screen_draw_tr.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/vendor_tr.h>
#include <game/port_stubs.h>

#include <vector>

namespace rac1 {

namespace {
constexpr OvConst<0x00173040u> SB{};  // the screens' block
constexpr OvConst<0x0015f5c4u> MODE{};
constexpr uint32_t PRESSED = 0x0013cae4;  // the pad's buttons pressed (0x13c940 + 0x1a4)
constexpr uint32_t CARD = 0x0015eeb0, FLAGS = 0x0015eeb4;
constexpr OvConst<0x0015f5a8u> FE{};

// orig: (helper) repeated in 0x00202aa0: the screen closed, back to the mode it was opened over
void back() { sw(MODE, lwu(SB + 0x14)); }
}  // namespace

// The mode the screen was opened over.
// orig: 0x00201720
int32_t screen_prev_mode() { return lw(SB + 0x14); }

// A screen opened: the music paused unless over the full-screen menu (989snd command 0x16 0x1d,
// 0x265828), the mode kept (+0x14) and the argument (+0x18), game mode 4, the kind; by kind: 0 the
// lines 0x4f6e / 0x5248 / 0x5249 and its counters cleared; 1 / 4 0x5229 / 0x4ee0 / 0x524a; 2 the
// line 0x524a; 3 / 5 (5 after the text 0x4e2b shown, 0x204b60) the countdown and +0x24 30 frames,
// +0x20 0; 6 the countdown 30 frames, step 0; other kinds: no lines, 0x78 frames.
// orig: 0x00201730
void screen_open(int32_t kind, uint32_t arg) {
    if (lw(MODE) != 3) {
        snd_cmd_16(0x1d);
        sub_00265828(0);
    }
    sw(SB + 0x14, lwu(MODE));
    sw(SB + 0x18, arg);
    sw(MODE, 4);
    sw(SB, static_cast<uint32_t>(kind));
    if (!(static_cast<uint32_t>(kind) < 7)) {
        sw(SB + 8, 0);
        sw(SB + 4, 0x78);
        sw(SB + 0xc, 0);
        sw(SB + 0x10, 0);
        return;
    }
    switch (kind) {
        case 0:
            sw(SB + 8, static_cast<uint32_t>(sub_00203ad8(0x4f6e)));
            sw(SB + 0xc, static_cast<uint32_t>(sub_00203ad8(TXT(0x5248))));
            sw(SB + 0x10, static_cast<uint32_t>(sub_00203ad8(TXT(0x5249))));
            sw(SB + 0x28, 0);
            sw(SB + 4, 0);
            sw(SB + 0x1c, 0);
            sw(SB + 0x20, 0);
            sw(SB + 0x24, 0);
            return;
        case 1:
        case 4:
            sw(SB + 8, static_cast<uint32_t>(sub_00203ad8(TXT(0x5229))));
            sw(SB + 0xc, static_cast<uint32_t>(sub_00203ad8(0x4ee0)));
            sw(SB + 0x10, static_cast<uint32_t>(sub_00203ad8(TXT(0x524a))));
            sw(SB + 4, 0);
            return;
        case 2:
            sw(SB + 8, static_cast<uint32_t>(sub_00203ad8(TXT(0x524a))));
            sw(SB + 4, 0);
            sw(SB + 0xc, 0);
            return;
        case 5:
            sub_00204b60(0x4e2b);
            [[fallthrough]];
        case 3:
            sw(SB + 4, static_cast<uint32_t>(frame_scale_i(0x1e)));
            sw(SB + 0x20, 0);
            sw(SB + 0x24, static_cast<uint32_t>(frame_scale_i(0x1e)));
            return;
        default:  // 6
            sw(SB + 4, static_cast<uint32_t>(frame_scale_i(0x1e)));
            sw(SB + 0x1c, 0);
            return;
    }
}

namespace {
// Kind 3, the memory card's screens: the answers by the card state (0x15eeb0, from 2): cross
// (0x40) / triangle (0x10) / circle (0x20) close the screen, set the card flags, start the new
// game (0x24b628, then the level change to 0, 0x28eeb0, 0x13e05a 1) or go to a page (+0x18).
// orig: (helper) the kind-3 arm of 0x00202aa0
void card_screens() {
    sw(SB + 0x20, lwu(SB + 0x20) + 1);
    if (frame_scale_i(0x1e) < lw(SB + 0x20) && lw(SB + 0x24) != 0) sw(SB + 0x24, lwu(SB + 0x24) - 1);
    const uint32_t pressed = lwu(PRESSED);
    auto new_game = [] {
        new_game_state();
        sw(FLAGS, (lwu(FLAGS) & ~2u & ~4u) | 0x20);
        level_exit_to(0);
        sh(0x0013e05a, 1);
    };
    switch (lw(CARD)) {
        case 2:
            if (lw(SB + 4) != 0) return;
            if ((pressed & 0x40) == 0) return;
            sw(FLAGS, lwu(FLAGS) & ~1u);
            back();
            return;
        case 3:
        case 5:
        case 0xc:
            if (lw(CARD) == 0xc && (lwu(FLAGS) & 2) != 0) return;
            if ((pressed & 0x10) == 0) return;
            sw(FLAGS, lwu(FLAGS) & ~2u & ~4u);
            back();
            return;
        case 4: {
            const uint32_t f = lwu(FLAGS);
            if (lw(FE) != 0) {
                if ((f & 2) != 0) {
                    if ((lwu(0x0013c940 + 0x1a4) & 0x40) != 0) {
                        new_game_state();
                        sw(FLAGS, lwu(FLAGS) & ~2u & ~4u);
                        level_exit_to(0);
                        sh(0x0013e05a, 1);
                    }
                } else if ((f & 4) != 0) {
                    if ((pressed & 0x10) == 0) return;
                    sw(MODE, lwu(SB + 0x14));
                    sw(FLAGS, (f | 0x20) & ~2u & ~4u);
                    return;
                } else {
                    back();
                    return;
                }
            }
            if ((lwu(0x0013c940 + 0x1a4) & 0x10) == 0) return;
            sw(MODE, lwu(SB + 0x14));
            sw(FLAGS, (lwu(FLAGS) | 0x20) & ~2u & ~4u);
            return;
        }
        case 6:
        case 0xd: {
            const uint32_t f = lwu(FLAGS);
            if ((pressed & 0x20) != 0) {
                sw(FLAGS, f | (lw(CARD) == 6 ? 8u : 0x10u));
                return;
            }
            if ((pressed & 0x10) == 0) return;
            sw(FLAGS, (f | 0x20) & ~2u & ~4u);
            if (lw(FE) != 0) return;
            back();
            return;
        }
        case 9:
            if (lw(FE) == 0) return;
            if ((lwu(FLAGS) & 6) != 0) return;
            back();
            return;
        case 0x10:
            sw(OV(0x001b9cf8), lwu(SB + 0x18));
            back();
            return;
        case 0x11:
        case 0x12:
        case 0x14:
        case 0x15:
            if ((pressed & 0x40) == 0) return;
            sw(MODE, lwu(SB + 0x14));
            sw(FLAGS, ((lwu(FLAGS) ^ 0x40) & ~2u) & ~4u);
            return;
        case 0x13: {
            const uint32_t f = lwu(FLAGS);
            if (lw(FE) != 0 && (f & 2) != 0) {
                if ((pressed & 0x40) != 0) {
                    new_game();
                    return;
                }
                if ((pressed & 0x10) == 0) return;
                sw(FLAGS, f & ~2u & ~4u);
                back();
                return;
            }
            if ((pressed & 0x10) == 0) return;
            sw(MODE, lwu(SB + 0x14));
            sw(FLAGS, lwu(FLAGS) & ~2u & ~4u);
            return;
        }
        case 0x17:
        case 0x18:
            if ((pressed & 0x40) != 0) {
                new_game();
                return;
            }
            if ((pressed & 0x10) == 0) return;
            sw(FLAGS, lwu(FLAGS) | 0x20);
            back();
            return;
        default:
            return;
    }
}
}  // namespace

// Game mode 4's update: the sound frame, the countdown +4; by kind: 3 the memory card's screens
// (above); 5 after 0x5a / 0x78 frames cross on Novalis asks the level change to the save's level
// (-1, 0x24d430) (the mode 0); 4 triangle puts the hero back at the checkpoint (0x210b30, 0x216f90
// from 0x141050 / 0x141060), cross closes; 6 (the HelpDesk: a fade, 0x16040c off for 600 frames,
// then on) ...; 1 / 2 cross / triangle close (1: triangle sets 0x140f5f bit 0, the debug flag
// 0x15f608 2); 0 a count up to 8 + 8, then cross / circle / Start: the hero's death screen (the
// sound bank 0x140530 released 0x2359f8, 0x23af30; the deaths counted on Rilgar / Kalebo III 0x15ee38 /
// 0x15ee3c past 0x1068 frames; the hero back at 0x141050 / 0x141060) or the lives counter. Back in
// a mode other than 3 / 4: the music's pause lifted (command 0x17 0x1d, 0x265860).
// orig: 0x00202aa0
void screen_update() {
    sound_frame();
    if (lw(SB + 4) != 0) sw(SB + 4, lwu(SB + 4) - 1);
    const uint32_t kind = lwu(SB);
    const uint32_t pressed = lwu(PRESSED);
    if (kind < 7) {
        switch (kind) {
            case 5:
                sw(SB + 0x20, lwu(SB + 0x20) + 1);
                if (frame_scale_i(0x5a) < lw(SB + 0x20) && lw(SB + 0x24) != 0) sw(SB + 0x24, lwu(SB + 0x24) - 1);
                if (frame_scale_i(0x78) < lw(SB + 0x20) && (lwu(PRESSED) & 0x40) != 0) {
                    const bool novalis = lw(0x0015ed84) == 1;
                    sw(MODE, 0);
                    if (novalis) level_change_request(0, -1);
                }
                break;
            case 4:
                if ((pressed & 0x10) != 0) {
                    constexpr uint32_t P = 0x0013f350;
                    const uint32_t hero = lwu(P + 0x2080);
                    const uint32_t f34 = lhu(hero + 0x34);
                    sb(hero + 0x31, 0);
                    sw(hero + 0x94, 0);
                    sh(hero + 0x34, f34 | 1);
                    hero_mode_exit();
                    hero_place(P + 0x1d00, P + 0x1d10, 0, 1);
                    sw(MODE, 0);
                } else if ((pressed & 0x40) != 0) {
                    sw(MODE, 0);
                }
                break;
            case 6:
                switch (lw(SB + 0x1c)) {
                    case 0: {
                        const int32_t c = lw(SB + 4);
                        if (c != 0) {
                            sw(SB + 4, static_cast<uint32_t>(c - 1));
                            if (c - 1 != 0) break;
                        }
                        sw(SB + 0x1c, 1);
                        break;
                    }
                    case 1:
                        if ((pressed & 0x10) != 0) {
                            back();
                        } else if ((pressed & 0x40) != 0) {
                            screen_fade(4);
                            sb(OV(0x0016040c), 0);
                            sw(SB + 0x1c, 2);
                            sw(SB + 0x20, static_cast<uint32_t>(frame_scale_i(0x258)));
                        }
                        break;
                    case 2: {
                        const int32_t c = lw(SB + 0x20);
                        if (c == 0 || c - 1 == 0) {
                            if (c != 0) sw(SB + 0x20, 0);
                            sw(SB + 0x1c, 3);
                            sb(OV(0x0016040c), 1);
                            break;
                        }
                        sw(SB + 0x20, static_cast<uint32_t>(c - 1));
                        if ((pressed & 0x10) != 0) {
                            screen_fade(4);
                            sb(OV(0x0016040c), 1);
                            back();
                        } else if ((pressed & 0x40) != 0) {
                            sb(OV(0x0016040c), 0);
                            back();
                        }
                        break;
                    }
                    case 3:
                        if ((pressed & 0x40) != 0) back();
                        break;
                    default:
                        back();
                        break;
                }
                break;
            case 1:
                sw(OV(0x0015f608), 2);
                if ((pressed & 0x10) != 0) {
                    sw(MODE, 0);
                    sb(0x0013f350 + 0x160f, lbu(0x0013f350 + 0x160f) | 1);
                } else if ((pressed & 0x40) != 0) {
                    sw(MODE, 0);
                }
                break;
            case 2:
                sw(OV(0x0015f608), 2);
                if ((pressed & 0x40) != 0) sw(MODE, 0);
                break;
            case 0:
                switch (lw(SB + 0x1c)) {
                    case 0:
                        if (lw(SB + 0x20) < 8) sw(SB + 0x20, lwu(SB + 0x20) + 1);
                        else if (lw(SB + 0x24) < 8) sw(SB + 0x24, lwu(SB + 0x24) + 1);
                        else sw(SB + 0x1c, 1);
                        break;
                    case 1:
                        if ((pressed & 0x40) != 0) sw(SB + 0x1c, 2);
                        else if ((pressed & 0x820) != 0) sw(SB + 0x1c, 3);
                        break;
                    case 2:
                    case 3: {
                        if (lw(SB + 0x24) != 0) {
                            sw(SB + 0x24, lwu(SB + 0x24) - 1);
                            break;
                        }
                        if (lw(SB + 0x20) != 0) {
                            sw(SB + 0x20, lwu(SB + 0x20) - 1);
                            break;
                        }
                        constexpr uint32_t P = 0x0013f350;
                        if (lw(SB + 0x1c) == 2) {
                            const int32_t bank = lw(P + 0x880);
                            if (bank != -1) {
                                sub_002359f8(static_cast<uint32_t>(bank), 0);
                                sw(P + 0x880, 0xffffffffu);
                            }
                            ovl_23af30_tr(0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);  // (vendor_tr.cpp; it reads no argument)
                            if (frame_scale_i(0x1068) < lw(P + 0x19c)) {
                                const int32_t lv = lw(0x0015ed84);
                                if (lv == 5) sw(0x0015ee38, lwu(0x0015ee38) + 1);
                                else if (lv == 0x10) sw(0x0015ee3c, lwu(0x0015ee3c) + 1);
                            }
                            sw(MODE, 0);
                            hero_place(0x00141050, 0x00141060, 0, 1);
                            bug_fixes_race_quit();  // enhancement: Bug Fixes, fix 1 (the planet's music back on Rilgar)
                        } else {
                            sw(MODE, 0);
                            if (!(lh(P + 0x89a) < 3)) {
                                sh(P + 0x89a, static_cast<uint32_t>(lh(P + 0x89a) - 1));
                                sb(lwu(P + 0x890) + 0xbc, 3);
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
                break;
            default:  // 3
                card_screens();
                break;
        }
    }
    const uint32_t m = static_cast<uint32_t>(lw(MODE) - 3);
    if (m < 2) return;
    snd_cmd_17(0x1d);
    music_volumes_default();
    snd_flush();
}

// A frame of 9 rectangles (the screens' box): `colour`'s alpha (the mask 0xff000000 as lui
// builds it, sign-extended) with 4 for the first.
// orig: 0x001faea0
void screen_frame(int32_t a, int32_t b, int32_t c, int32_t d, uint64_t colour) {
    quad_fill(a, b, c, d, (colour & 0xffffffffff000000ull) | 4);
    quad_fill(a - 1, a + 1, c + 3, d + 5, colour);
    quad_fill(a - 3, a - 5, c - 1, d - 3, colour);
    quad_fill(a - 5, b - 3, c - 1, c + 1, colour);
    quad_fill(a + 3, b + 1, c - 3, c - 5, colour);
    quad_fill(b - 1, b + 1, c - 3, d - 3, colour);
    quad_fill(b + 3, b + 5, c + 3, d + 1, colour);
    quad_fill(a + 3, b + 3, d - 1, d + 1, colour);
    quad_fill(a + 1, b - 3, d + 3, d + 5, colour);
}

namespace {
// The memory card screens' message (kind 3), by the card state: the text, up to two answers (x
// 0xca / 0x135) or one (0x100); the flags 2 / 4 (new / load game) and the front end pick some.
// Some messages join two texts with two codes 1 between them ("%s%c%c%s", 0x15f510).
// orig: (helper) the kind-3 arm of 0x002018c8 (up to its drawing)
void card_message(uint32_t buf, uint32_t& text, int32_t& a1, int32_t& a2, int32_t& a3) {
    text = OV(0x0015f508);  // ""
    a1 = a2 = a3 = 0;
    auto join = [buf](int32_t t1, int32_t t2) {
        const uint32_t s1 = static_cast<uint32_t>(sub_00203ad8(t1));
        const uint32_t s2 = static_cast<uint32_t>(sub_00203ad8(t2));
        const std::vector<uint64_t> args = {s1, 1, 1, s2};
        libc_sprintf(buf, OV(0x0015f510), args);
        return buf;
    };
    auto plain = [&text](int32_t t) { text = static_cast<uint32_t>(sub_00203ad8(t)); };
    const uint32_t f = lwu(FLAGS);
    switch (lw(CARD)) {
        case 2:
            plain(0x4fa6);
            a3 = lw(SB + 4) != 0 ? 0 : TXT(0x524a);
            break;
        case 3:
        case 4:
            if (lw(FE) != 0 && (f & 2) != 0) {
                a1 = TXT(0x524e);
                a2 = TXT(0x524b);
                plain(0x4fab);
            } else {
                a3 = 0x4fa8;
                plain(0x4fac);
            }
            break;
        case 5:
            a3 = 0x4fa8;
            text = join(0x4fb0, 0x4fa7);
            break;
        case 6:
            a1 = TXT(0x524f);
            a2 = TXT(0x524b);
            plain(0x4faf);
            break;
        case 7:
        case 8:
            plain(0x4fb7);
            break;
        case 0xa:
        case 0xb:
            plain(0x4fc1);
            break;
        case 0xc:
            if ((f & 2) == 0) {
                a3 = 0x4fa8;
                plain(0x4fa7);
            } else {
                a1 = TXT(0x524f);
                a2 = TXT(0x524b);
                plain(0x4fad);
            }
            break;
        case 0xd:
            a1 = TXT(0x524f);
            a2 = TXT(0x524b);
            plain(0x4fad);
            break;
        case 0xe:
        case 0xf:
            plain(0x4fb8);
            break;
        case 0x11:
            a3 = TXT(0x524a);
            plain(0x4fba);
            break;
        case 0x12:
            a3 = TXT(0x524a);
            plain(0x4fbc);
            break;
        case 0x13:
            if ((f & 4) != 0) {
                a3 = 0x4fa8;
                plain(0x4fa7);
            } else if (lw(FE) == 0) {
                a3 = 0x4fa8;
                plain(0x4fa9);
            } else {
                a1 = TXT(0x524e);
                a2 = TXT(0x524b);
                text = join(0x4fa9, 0x4faa);
            }
            break;
        case 0x14:
            a3 = TXT(0x524a);
            plain(0x4fbb);
            break;
        case 0x15:
            a3 = TXT(0x524a);
            plain(0x4fbd);
            break;
        case 0x17:
            a1 = TXT(0x524e);
            a2 = TXT(0x524b);
            plain(0x4fb1);
            break;
        case 0x18:
            a1 = TXT(0x524e);
            a2 = TXT(0x524b);
            text = join(0x4fae, 0x4faa);
            break;
        default:  // 9, 0x10, 0x16 and others: no message
            break;
    }
}
}  // namespace

namespace {
inline uint64_t sxt64(uint32_t v) {
    return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v)));
}

inline int32_t cvt_w_i32(float f) {
    const uint32_t u = f2u(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) {
        return static_cast<int32_t>((u & 0x80000000u) != 0 ? 0x80000000u : 0x7fffffffu);
    }
    return static_cast<int32_t>(f);
}

uint64_t pulsing_frame_colour() {
    const float n = int_to_float(lw(OV(0x0015f4e0)));
    const int32_t phase = ee_rem(lw(OV(0x0015f3f8)), lw(OV(0x0015f4e0)));
    const float a = fpu_sub(fpu_mul(fpu_div(static_cast<float>(phase), n), u2f(0x40c90fd0)), u2f(0x40490fd0));
    const float s = fpu_add(fpu_mul(sin_vu(a), 0.5f), 0.5f);
    return static_cast<uint64_t>(rgba_lerp(lwu(OV(0x0015f4e4)), lwu(OV(0x0015f4e8)), s));
}
}  // namespace

// The screens' drawing (all kinds 0..6 and >= 7).
// orig: 0x002018c8
void screen_draw() {
    texture_block_begin(0);
    const uint32_t kind = lwu(SB);
    if (kind == 0) {
        const uint64_t frame_col = pulsing_frame_colour();
        float box_s = fpu_mul(static_cast<float>(lw(SB + 0x20)), 0.125f);
        if (1.0f < box_s) {
            box_s = 1.0f;
        } else if (box_s < u2f(0x3dcccccd)) {
            box_s = u2f(0x3dcccccd);
        }

        uint32_t prompt_col32 = lwu(OV(0x0015f4ec));
        uint64_t col_hi = sxt64(lwu(OV(0x0015f4f4)));
        uint64_t col_lo = sxt64(lwu(OV(0x0015f4fc)));
        if (lw(SB + 0x24) != 0) {
            float ts = fpu_mul(static_cast<float>(lw(SB + 0x24)), 0.125f);
            if (1.0f < ts) {
                ts = 1.0f;
            } else if (ts < 0.0f) {
                ts = 0.0f;
            }
            prompt_col32 = rgba_lerp(lwu(OV(0x0015f4ec)), lwu(OV(0x0015f4f0)), ts);
            col_hi = static_cast<uint64_t>(rgba_lerp(lwu(OV(0x0015f4f4)), lwu(OV(0x0015f4f8)), ts));
            col_lo = static_cast<uint64_t>(rgba_lerp(lwu(OV(0x0015f4fc)), lwu(OV(0x0015f500)), ts));
        }
        const uint64_t col_prompt = sxt64(prompt_col32);

        if (lh(0x0013fbea) < 3) {
            const int32_t dy0 = float_to_int(fpu_mul(static_cast<float>(lw(OV(0x0015f4c4))), box_s));
            const int32_t top = lw(OV(0x0015f4bc)) - dy0;
            const int32_t dy1 = float_to_int(fpu_mul(static_cast<float>(lw(OV(0x0015f4c4))), box_s));
            const int32_t bot = lw(OV(0x0015f4bc)) + dy1;
            const int32_t dx0 = float_to_int(fpu_mul(static_cast<float>(lw(OV(0x0015f4c0))), box_s));
            const int32_t left = lw(OV(0x0015f4b8)) - dx0;
            const int32_t dx1 = float_to_int(fpu_mul(static_cast<float>(lw(OV(0x0015f4c0))), box_s));
            const int32_t right = lw(OV(0x0015f4b8)) + dx1;
            screen_frame(top, bot, left, right, frame_col);
            text_centered_small(0x100, lw(OV(0x0015f4c8)), col_prompt, static_cast<uint32_t>(sub_00203ad8(0x4f6e)), -1);
            text_centered_small(0x100, lw(OV(0x0015f4c8)) + 0x18, col_prompt, static_cast<uint32_t>(sub_00203ad8(TXT(0x5249))), -1);
            text_centered_small(0x100, lw(OV(0x0015f4c8)) + 0x30, col_prompt, static_cast<uint32_t>(sub_00203ad8(TXT(0x5248))), -1);
        } else {
            const int32_t dy0 = float_to_int(fpu_mul(static_cast<float>(lw(OV(0x0015f4d8))), box_s));
            const int32_t top = lw(OV(0x0015f4d0)) - dy0;
            const int32_t dy1 = float_to_int(fpu_mul(static_cast<float>(lw(OV(0x0015f4d8))), box_s));
            const int32_t bot = lw(OV(0x0015f4d0)) + dy1;
            const int32_t dx0 = float_to_int(fpu_mul(static_cast<float>(lw(OV(0x0015f4d4))), box_s));
            const int32_t left = lw(OV(0x0015f4cc)) - dx0;
            const int32_t dx1 = float_to_int(fpu_mul(static_cast<float>(lw(OV(0x0015f4d4))), box_s));
            const int32_t right = lw(OV(0x0015f4cc)) + dx1;
            screen_frame(top, bot, left, right, frame_col);

            const uint32_t buf = ee_stack_alloc(0x50);
            libc_sprintf(buf, OV(0x0015f520), {sxt64(static_cast<uint32_t>(sub_00203ad8(TXT(0x5240))))});
            const int32_t place = lw(0x0013f350 + 0x8c0);
            if (place == 1) {
                libc_sprintf(buf, OV(0x0015f528), {1ull});
            } else if (place == 2) {
                libc_sprintf(buf, OV(0x0015f530), {2ull});
            } else if (place == 3) {
                libc_sprintf(buf, OV(0x0015f538), {3ull});
            } else {
                libc_sprintf(buf, OV(0x0015f540), {sxt64(static_cast<uint32_t>(place))});
            }
            sb(buf + 3, 0x20);
            const bool first_place = (lw(0x0013f350 + 0x8c0) == 1);
            text_centered_small(0x100, lw(OV(0x0015f4dc)), first_place ? col_hi : col_lo, buf, -1);

            const uint32_t rec_off = (lw(0x0015ed84) == 0x10) ? 4u : 0u;
            const int32_t best_time = lw(0x0015ee58 + rec_off);
            if (best_time == lw(0x0013fbe4)) {
                const int32_t fpm = (lw(0x0015ed80) != 0) ? 0xbb8 : 0xe10;
                const int32_t mins = ee_div(best_time, fpm);
                const int32_t fps = ee_div(fpm, 0x3c);
                const int32_t rem_f = best_time - mins * fpm;
                const int32_t secs = ee_div(rem_f, fps);
                const int32_t csecs = ee_div((rem_f - secs * fps) * 0x64, fps);
                libc_sprintf(buf, OV(0x0015f548), {sxt64(static_cast<uint32_t>(sub_00203ad8(TXT(0x50a3))))});
                text_centered_small(0x100, lw(OV(0x0015f4dc)) + 0x26, col_hi, buf, -1);
                libc_sprintf(buf, OV(0x0015f550), {sxt64(static_cast<uint32_t>(mins)), sxt64(static_cast<uint32_t>(secs)), sxt64(static_cast<uint32_t>(csecs))});
                text_centered_small(0x100, lw(OV(0x0015f4dc)) + 0x3a, col_hi, buf, -1);
            } else {
                libc_sprintf(buf, OV(0x0015f548), {sxt64(static_cast<uint32_t>(sub_00203ad8(TXT(0x50a2))))});
                text_centered_small(0x100, lw(OV(0x0015f4dc)) + 0x30, col_lo, buf, -1);
            }

            const uint32_t score_off = (lw(0x0015ed84) == 0x10) ? 4u : 0u;
            const int32_t best_score = lw(0x0015ee68 + score_off);
            if (best_score != 0 && best_score == lw(0x0013fbf8)) {
                libc_sprintf(buf, OV(0x0015f548), {sxt64(static_cast<uint32_t>(sub_00203ad8(TXT(0x50a5))))});
                text_centered_small(0x100, lw(OV(0x0015f4dc)) + 0x56, col_hi, buf, -1);
                libc_sprintf(buf, OV(0x0015f560), {sxt64(lwu(0x0013fbf8))});
                text_centered_small(0x100, lw(OV(0x0015f4dc)) + 0x6a, col_hi, buf, -1);
            } else {
                libc_sprintf(buf, OV(0x0015f548), {sxt64(static_cast<uint32_t>(sub_00203ad8(TXT(0x50a4))))});
                text_centered_small(0x100, lw(OV(0x0015f4dc)) + 0x60, col_lo, buf, -1);
            }

            text_centered_small(0x100, lw(OV(0x0015f4dc)) + 0x90, col_prompt, static_cast<uint32_t>(sub_00203ad8(TXT(0x5249))), -1);
            text_centered_small(0x100, lw(OV(0x0015f4dc)) + 0xa8, col_prompt, static_cast<uint32_t>(sub_00203ad8(TXT(0x5248))), -1);
            ee_stack_free(0x50);
        }
    } else if (kind == 2) {
        screen_frame(0x64, 0xa0, 0xb0, 0x150, pulsing_frame_colour());
        text_centered_small(0x100, 0x7a, 0x8000c0c0ull, lwu(SB + 8), -1);
    } else if (kind == 3) {
        const uint32_t buf = ee_stack_alloc(0x240);
        uint32_t text;
        int32_t a1, a2, a3;
        card_message(buf, text, a1, a2, a3);
        const uint32_t box = buf + 0x200, tmpl = buf + 0x220;
        libc_memset(tmpl, 0, 0x18);
        sh(tmpl + 2, lhu(0x0013e500 + 4));
        sh(tmpl + 4, 0x60);
        sh(tmpl + 6, 0x1a0);
        sh(tmpl + 8, 0x100);
        sh(tmpl + 0xa, 0x68);
        sh(tmpl + 0x10, 0x10);
        sh(tmpl + 0x12, 5);
        for (uint32_t i = 0; i < 0x18; i += 4) sw(box + i, lwu(tmpl + i));
        text_box_small(box, 0, text, -1);
        const int32_t hgt = lh(box + 0xe) + 0x28;
        const int32_t top = (lw(0x0013e500 + 4) - hgt) >> 1;
        sh(box + 0xa, static_cast<uint32_t>(top + 4));
        sh(box + 2, static_cast<uint32_t>(top + hgt));
        sh(box, static_cast<uint32_t>(top));
        const float t = fpu_sub(1.0f, fpu_div(int_to_float(lw(SB + 4)), int_to_float(frame_scale_i(0x1e))));
        box_rounded(lh(box), lh(box + 2), 0x60, 0x1a0, static_cast<uint32_t>(float_to_int(fpu_mul(t, 80.0f))));
        sh(box + 0x12, lhu(box + 0x12) ^ 4);
        text_box_small(box, rgba_lerp(lwu(OV(0x0015f4b0)), lwu(OV(0x0015f4b4)), t), text, -1);
        const float t2 = fpu_sub(1.0f, fpu_div(int_to_float(lw(SB + 0x24)), int_to_float(frame_scale_i(0x1e))));
        const uint32_t colour = rgba_lerp(NOT_ADDR(0x0020ffff), 0x8020ffff, t2);
        if (a1 != 0) text_centered_small(0xca, lh(box + 2) - 0x14, colour, static_cast<uint32_t>(sub_00203ad8(a1)), -1);
        if (a2 != 0) text_centered_small(0x135, lh(box + 2) - 0x14, colour, static_cast<uint32_t>(sub_00203ad8(a2)), -1);
        if (a3 != 0) text_centered_small(0x100, lh(box + 2) - 0x14, colour, static_cast<uint32_t>(sub_00203ad8(a3)), -1);
        ee_stack_free(0x240);
    } else if (kind == 5) {
        const float t = fpu_sub(1.0f, fpu_div(static_cast<float>(lw(SB + 4)), static_cast<float>(frame_scale_i(0x1e))));
        box_rounded(0x50, 0x154, 0x60, 0x1a0, static_cast<uint32_t>(cvt_w_i32(fpu_mul(t, 80.0f))));

        const uint32_t box = ee_stack_alloc(0x20);
        for (uint32_t i = 0; i < 0x18; i += 4) sw(box + i, lwu(OV(0x001e7a50) + i));
        const uint64_t txt_col = static_cast<uint64_t>(rgba_lerp(lwu(OV(0x0015f4b0)), lwu(OV(0x0015f4b4)), t));
        libc_strncpy(0x70000000u, static_cast<uint32_t>(sub_00203ad8(0x4e2b)), 0x400u);

        uint32_t split = 0x70000000u;
        if (!(lbu(0x70000000u) < 2u)) {
            split = 0x70000001u;
            while (!(lbu(split) < 2u)) ++split;
        }
        if (lbu(split) == 1u) {
            sb(split, 0u);
            for (;;) {
                ++split;
                if (lbu(split) != 1u) break;
                sb(split, 0u);
            }
        }

        const uint32_t font = OV(0x001c3150);
        text_box(box, txt_col, 0x70000000u, -1, static_cast<uint64_t>(effect_tex0(1)), font);

        int32_t mid_y = lh(box + 0xa) + lh(box + 0xe);
        sh(box + 0x12, lhu(box + 0x12) | 4u);
        text_box(box, txt_col, split, -1, static_cast<uint64_t>(effect_tex0(1)), font);

        sh(box + 0x12, lhu(box + 0x12) ^ 4u);
        sh(box + 0xa, 0x136u - lhu(box + 0xe));
        text_box(box, txt_col, split, -1, static_cast<uint64_t>(effect_tex0(1)), font);

        const float t2 = fpu_sub(1.0f, fpu_div(static_cast<float>(lw(SB + 0x24)), static_cast<float>(frame_scale_i(0x1e))));
        const uint64_t prompt_col = static_cast<uint64_t>(rgba_lerp(NOT_ADDR(0x0020ffff), 0x8020ffff, t2));
        text_centered_small(0x100, 0x140, prompt_col, static_cast<uint32_t>(sub_00203ad8(TXT(0x524a))), -1);

        mid_y = (mid_y + lh(box + 0xa)) >> 1;
        sub_002a0ba8(0x47, 0x3004bull);
        hud_sprite(hud_frame(0x755d, 0), 0xe0, mid_y - 0x20, 0x40, 0x40, 0x80ull);

        const int32_t rem = ee_rem(lw(OV(0x0015f3f8)), 0x37);
        const float rot_y = static_cast<float>(mid_y << 4);
        const float angle = fpu_div(fpu_mul(static_cast<float>(rem), u2f(0xc0c90fdb)), 55.0f);
        const uint64_t rot_tex = static_cast<uint64_t>(hud_tex0(hud_frame(0x755d, 1)));
        sprite_rotated(0x40, 0x40, rot_tex, 4096.0f, rot_y, 272.0f, 272.0f, angle);
        ee_stack_free(0x20);
    } else if (kind == 6) {
        const uint32_t box = ee_stack_alloc(0x20);
        for (uint32_t i = 0; i < 0x18; i += 4) sw(box + i, lwu(OV(0x001e7a68) + i));
        const float t = fpu_sub(1.0f, fpu_div(static_cast<float>(lw(SB + 4)), static_cast<float>(frame_scale_i(0x1e))));
        box_rounded(0x64, 0x12c, 0x60, 0x1a0, static_cast<uint32_t>(cvt_w_i32(fpu_mul(t, 80.0f))));
        const uint64_t body_col = static_cast<uint64_t>(rgba_lerp(lwu(OV(0x0015f4b0)), lwu(OV(0x0015f4b4)), t));
        const uint64_t btn_col = static_cast<uint64_t>(rgba_lerp(NOT_ADDR(0x0020ffff), 0x8020ffff, t));

        const int32_t step = lw(SB + 0x1c);
        int32_t msg_id = 0;
        if (step == 2) {
            text_centered_small(0xca, 0x118, btn_col, static_cast<uint32_t>(sub_00203ad8(TXT(0x524e))), -1);
            text_centered_small(0x135, 0x118, btn_col, static_cast<uint32_t>(sub_00203ad8(TXT(0x524b))), -1);
            msg_id = TXT(0x522b);
        } else if (step < 3) {
            if (step >= 0) {
                text_centered_small(0xca, 0x118, btn_col, static_cast<uint32_t>(sub_00203ad8(TXT(0x524e))), -1);
                text_centered_small(0x135, 0x118, btn_col, static_cast<uint32_t>(sub_00203ad8(TXT(0x524b))), -1);
                msg_id = TXT(0x522a);
            }
        } else if (step == 3) {
            text_centered_small(0x100, 0x118, btn_col, static_cast<uint32_t>(sub_00203ad8(TXT(0x524a))), -1);
            msg_id = TXT(0x522c);
        }
        if (msg_id != 0) {
            const uint32_t msg = static_cast<uint32_t>(sub_00203ad8(msg_id));
            const uint64_t tex = static_cast<uint64_t>(effect_tex0(1));
            text_box(box, body_col, msg, -1, tex, OV(0x001c3150));
        }
        ee_stack_free(0x20);
    } else if (kind == 1 || kind == 4 || !(kind < 7)) {
        screen_frame(0x50, lw(OV(0x0015f4ac)) + 0x1a, 0xb0, 0x150, pulsing_frame_colour());
        if (lw(SB + 8) != 0) text_centered_small(0x100, 0x5a, 0x8000c0c0ull, lwu(SB + 8), -1);
        if (lw(SB + 0xc) != 0) text_centered_small(0x100, lw(OV(0x0015f4a8)), 0x80ffa888ull, lwu(SB + 0xc), -1);
        if (lw(SB + 0x10) != 0) text_centered_small(0x100, lw(OV(0x0015f4ac)), 0x80ffa888ull, lwu(SB + 0x10), -1);
    }
    texture_block_end();
}

// orig: 0x002018c8
void screen_draw_tr(uint32_t) {
    screen_draw();
}

// Game mode 4's drawing (when the frame is drawn): over gameplay the frame's 3D (0x200fd8,
// 0x1f85b8 with the passes 0x100ff), over the full-screen menu the menu (0x2772c0(1)); dimmed
// (0x1fa050 black, alpha 0x40); the screen.
// orig: 0x001f8ec8
void screen_draw_mode4() {
    if (lw(OV(0x0015f5d8)) != 0) return;
    if (screen_prev_mode() == 0) {
        sub_00200fd8();
        sw(OV(0x0015f3f4), 0x100ff);
        render_frame();
    } else if (screen_prev_mode() == 3) {
        menu_draw(1);
    }
    sub_001fa050(0, 0, 0, 0x40);
    screen_draw();
}

}  // namespace rac1
