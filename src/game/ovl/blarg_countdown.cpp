// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1108, the station's countdown (once the creatures of
// a room are down and its alarm moby is set off, a scripted camera runs along a path to show the way out, then 30 seconds
// on the HUD's clock (its draw: shared_class1108.cpp), the station shaking, the warnings voiced in the disc's language;
// Ratchet out of reach of the clock (the hero's state 0x72, his ship's flight) ends it, else the screen fades and he is
// lost), and the small group helpers it alone calls. Header blarg_own.h; registered in blarg_mobys.cpp. Addresses are
// level 6's (`orig: L6:`); its own data is L6_ADDR, the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/hydro.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l6.h>
#include <game/level/level_write.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x2080 his moby, +0x2084 his state)

// the moby of the level's moby table at index `i`
uint32_t table_moby(int32_t i) { return lwu(L6_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }
// the moby of the level's moby table a group list's entry holds (bit 15: the list's last)
uint32_t listed_moby(uint32_t entry) { return lwu(L6_ADDR(0x0015ffd8)) + ((lhu(entry) & 0x7fffu) << 8); }

// each moby of the group list `list`, from its first entry (no test for a group without a list: the callers that make none
// read the null page's list, as the original did)
template <class F>
void each_listed(uint32_t list, F f) {
    for (uint32_t e = list;; e += 2) {
        f(listed_moby(e));
        if (lh(e) < 0) break;
    }
}

// the group's mobys still in play (state below 0x7f) put on hold in the state `st`: hidden (+0x34 bit 0, and `flags`),
// not drawn (bit 12 cleared), no model (+0x94 0)
void hold_live(uint32_t list, uint32_t st, uint32_t flags) {
    each_listed(list, [&](uint32_t m) {
        if (lbu(m + 0x20) >= 0x7f) return;
        const uint32_t f = lhu(m + 0x34);
        sb(m + 0x20, st);
        sw(m + 0x94, 0);
        sh(m + 0x34, (f | flags) & 0xefffu);
    });
}

// the disc's language as the index of the moby's per-language tables (the language word 0x15ed88 less 1, 0 for 0)
uint32_t language_index() {
    const int32_t lang = lw(0x0015ed88);
    return static_cast<uint32_t>(lang != 0 ? lang - 1 : 0);
}

// the rotation at `rot` of a camera looking along b -> a (z: the heading, x: the pitch, from the distance `d` and the
// height), the rest cleared
void camera_turn(uint32_t rot, uint32_t a, uint32_t b, float d) {
    vec_clear(V(rot));
    swc1(rot + 8, atan2_f(fpu_sub(lwc1(a), lwc1(b)), fpu_sub(lwc1(a + 4), lwc1(b + 4))));
    swc1(rot + 4, -atan2_f(d, fpu_sub(lwc1(a + 8), lwc1(b + 8))));
}
}  // namespace

// Group `group`'s creatures still in play held, hidden, to be let out later (creature_let_out_l6 takes the state 0xf).
// orig: L6:0x002e9e30
void group_hold_creatures_l6(int32_t group) { hold_live(lvd::group(group), 0xf, 0x41); }

// Group `group`'s mobys in the state 4 set back to 0.
// orig: L6:0x002f45a0
void group_restart_l6(int32_t group) {
    each_listed(lvd::group(group), [](uint32_t m) {
        if (lbu(m + 0x20) == 4) sb(m + 0x20, 0);
    });
}

// Group `group`'s mobys all put in the state 4 (waiting).
// orig: L6:0x002f45f0
void group_wait_l6(int32_t group) {
    each_listed(lvd::group(group), [](uint32_t m) { sb(m + 0x20, 4); });
}

// Group `group`'s mobys still in play (if it has a list) held, hidden, in the state 0x11.
// orig: L6:0x002ffc38
void group_hold_l6(int32_t group) {
    const uint32_t list = lvd::group(group);
    if (list != 0) hold_live(list, 0x11, 0x1);
}

// The voice line `which` of the countdown's (0 its start, 1 one of three warnings, 2 its half-time) in the disc's language
// queued for the dialog stream (0x1516ec, as the text id + 20000), from the moby's tables: +0x38 / +0x50 one word per
// language, +0x68 three rows of six for the warnings. A warning is picked at random among those not yet heard (0x1620b8:
// three flags, cleared once all were). Nothing for a word -1 or another `which`.
// orig: L6:0x00302400
void countdown_voice_l6(uint32_t m, int32_t which) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t id = -1;
    if (which == 0) {
        id = lw(pv + 0x38 + language_index() * 4);
    } else if (which == 2) {
        id = lw(pv + 0x50 + language_index() * 4);
    } else if (which == 1) {
        const uint32_t heard = L6_ADDR(0x001620b8);
        int32_t k = rand_mod(3);
        if (lw(heard + static_cast<uint32_t>(k) * 4) == 0) {
            sw(heard + static_cast<uint32_t>(k) * 4, 1);
        } else {
            // the next ones in turn; on the third try all three are cleared (so the fourth always finds one)
            for (int tries = 0;;) {
                k = ee_rem(k + 1, 3);
                if (tries == 2) {
                    sw(L6_ADDR(0x001620b8), 0);
                    sw(L6_ADDR(0x001620bc), 0);
                    sw(L6_ADDR(0x001620c0), 0);
                }
                if (++tries >= 4) break;
                if (lw(heard + static_cast<uint32_t>(k) * 4) == 0) {
                    sw(heard + static_cast<uint32_t>(k) * 4, 1);
                    break;
                }
            }
        }
        id = lw(pv + 0x68 + static_cast<uint32_t>(k) * 0x18 + language_index() * 4);
    }
    if (id != -1) sw(0x001516ec, static_cast<uint32_t>(id + 20000));
}

// A moby waiting (state 1) set off: state 2, its sound 0.
// orig: L6:0x003047b8
void moby_set_off_l6(uint32_t m) {
    if (lbu(m + 0x20) != 1) return;
    sb(m + 0x20, 2);
    moby_sound_start(0, 0, m);
}

// Group `group`'s smoke and spark sources (class 0x5e8, 1512) waiting (state 1) and on screen (within 64) set going (state
// 2) after a random 30 to 60 frames (their pvars +0x28).
// orig: L6:0x00309240
void smoke_sources_start_l6(int32_t group) {
    const uint32_t list = lvd::group(group);
    if (list == 0) return;
    const uint32_t sp = ee_stack_alloc(0x80);
    each_listed(list, [&](uint32_t m) {
        if (lh(m + 0xa6) != 0x5e8 || lbu(m + 0x20) != 1) return;
        lq_sq(sp, m + 0x10);
        swc1(sp + 0xc, 0.25f);
        if (point_on_screen(sp, 64.0f) < 0) return;
        sb(m + 0x20, 2);
        const uint32_t pv = lwu(m + 0x78);
        sw(pv + 0x28, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(30.0f, 60.0f)))));
    });
    ee_stack_free(0x80);
}

// Class 1108, the countdown. pvars: the groups +0 (waiting until the countdown), +4 / +8 (put away until then), +0x10 /
// +0x14 / +0x18 (held then), +0x1c (the creatures to beat first), +0x34 (the smoke sources), the box +0xc (raised by 10),
// the camera's path +0x24, the box Ratchet is put at +0x28, the mobys +0x20 (its state hook 3 at the start), +0x2c (the
// alarm), +0x30 (its state hook 5 at the end); +0xb0 the camera's point, +0xb4 the clock (frames), +0xb8 the fade,
// +0xbc its wait, +0xc0 the next shake; +0x38 / +0x50 / +0x68 the voice lines (countdown_voice_l6). States: 0 set up (gone
// once its checkpoint, the byte +0xb0 of this level's row of 0x14c050, is set); 1 waits for the creatures down, sets the
// alarm off and starts with its first voice line when the alarm says 1; 2 when the alarm says 2: the music, the groups,
// the scripted camera at the path's point 1, Ratchet put in the state 0x72; 3 the camera runs along the path (looking 30
// points back), the smoke sources going, then the clock (0x1620c4 seconds' worth), the hydro's state 2 and Ratchet back
// (4); 4 the clock runs: the dialog stream stopped at 30 and 5 seconds left, the half-time voice line at 15, every 8 to 10
// seconds a rumble and a shake, the clock drawn, Ratchet in the state 0x72 stops it (6), its end fades the screen (5);
// 5 the fade (6 a second), then Ratchet lost.
// orig: L6:0x00301b90
void blg_class1108_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t p0 = sp, rot = sp + 0x10, p2 = sp + 0x20;  // (two points of the camera's path, its rotation)
    switch (lbu(m + 0x20)) {  // (the table 0x2018c0: 0 .. 5, one case each; 6 and above nothing)
    case 0:
        if (lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) != 0) {
            moby_retire(m);
            break;
        }
        group_wait_l6(lw(pv + 0));
        group_put_away_l6(lw(pv + 4));
        group_put_away_l6(lw(pv + 8));
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        break;
    case 1:
        if (moby_group_count_l1(lw(pv + 0x1c), -1) != 0) break;
        moby_set_off_l6(table_moby(lw(pv + 0x2c)));
        if (state_query_blarg(table_moby(lw(pv + 0x2c))) != 1) break;
        sb(m + 0x20, 2);
        countdown_voice_l6(m, 0);
        break;
    case 2: {
        if (state_query_blarg(table_moby(lw(pv + 0x2c))) != 2) break;
        sb(m + 0x20, 3);
        sw(pv + 0xb0, 1);
        music_track_request(6, 9);
        if (lh(0x0015172a) == 3) dialog_stream_stop();
        group_hold_l6(lw(pv + 0x10));
        group_hold_creatures_l6(lw(pv + 0x14));
        group_hold_creatures_l6(lw(pv + 0x18));
        group_restart_l6(lw(pv + 0));
        group_bring_back_l6(lw(pv + 4));
        group_bring_back_l6(lw(pv + 8));
        moby_state_hook(table_moby(lw(pv + 0x20)), 3u);
        const uint32_t raised = lvd::box(lvd::BOXES, lw(pv + 0xc));
        lvd::set_shape_f32(raised + 0x38, fpu_add(lwc1(raised + 0x38), 10.0f));
        // the camera at the path's point 1 looking at its point 0
        const uint32_t path = lvd::path(lw(pv + 0x24));
        lq_sq(p0, path + 0x20);
        lq_sq(p2, path + 0x10);
        camera_turn(rot, p2, p0, vec2_distance(V(p0), V(p2)));
        camera_scripted_set(p0, rot, 2, static_cast<uint32_t>(frame_scale_i(0x12c)), 0);
        camera_angles_set(lwc1(L6_ADDR(0x00162098)), lwc1(L6_ADDR(0x0016209c)), lwc1(L6_ADDR(0x001620a0)),
                          lwc1(L6_ADDR(0x001620a4)), lwc1(L6_ADDR(0x001620a8)), lwc1(L6_ADDR(0x001620b0)));
        uid5_set_pvar_80(p0);
        uid5_set_pvar_90(rot);
        const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0x28));
        hero_place(b + 0x30, b + 0x70, 0x72, 0);
        break;
    }
    case 3: {
        // the camera to the path's next point
        const uint32_t path = lvd::path(lw(pv + 0x24));
        const int32_t at = lw(pv + 0xb0);
        lq_sq(p2, path + 0x10 + (static_cast<uint32_t>(at) << 4));
        sw(pv + 0xb0, static_cast<uint32_t>(at + 1));
        if (at + 1 == lw(lvd::path(lw(pv + 0x24))) - 1) {
            // the path's end: the clock starts
            sb(m + 0x20, 4);
            countdown_voice_l6(m, 1);
            sw(pv + 0xb4, static_cast<uint32_t>(float_to_int(frame_scale_f(lwc1(L6_ADDR(0x001620c4))))));
            hydro_2eac18(2);
            hero_set_state(0, 1);
            sw(L6_ADDR(0x0015f404), 0);
            break;
        }
        smoke_sources_start_l6(lw(pv + 0x34));
        int32_t back = at + 1 - 30;  // (the point looked at: 30 behind, the first at the start)
        if (!(-1 < back)) back = 0;
        lq_sq(p0, lvd::path(lw(pv + 0x24)) + 0x10 + (static_cast<uint32_t>(back) << 4));
        camera_turn(rot, p0, p2, vec2_distance(V(p2), V(p0)));  // (the camera at p2 looking at p0)
        uid5_set_pvar_80(p2);
        uid5_set_pvar_90(rot);
        break;
    }
    case 4: {
        const int32_t left = lw(pv + 0xb4);
        if (left == frame_scale_i(0x708)) {
            if (lh(0x0015172a) == 3) dialog_stream_stop();
        } else if (left == frame_scale_i(0x384)) {
            countdown_voice_l6(m, 2);
        } else if (left == frame_scale_i(0x12c)) {
            if (lh(0x0015172a) == 3) dialog_stream_stop();
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xc0))) != 0) {
            // a rumble and a shake (0.4 for 60 frames), the next in 8 to 10 seconds
            level_sound_play_l6(0, 0, lwu(PLAYER + 0x2080));
            sw(pv + 0xc0, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(480.0f, 600.0f)))));
            swc1(L6_ADDR(0x00167560), 0.4f);
            sw(L6_ADDR(0x00167568), static_cast<uint32_t>(frame_scale_i(0x3c)));
        }
        if (lw(L6_ADDR(0x0015f5c4)) == 0) sub_001f93e0(L6_ADDR(0x003021d8), m);  // (the clock's draw)
        if (lw(PLAYER + 0x2084) == 0x72) {
            sb(m + 0x20, 6);
            break;
        }
        if (lw(L6_ADDR(0x0015f5c4)) != 0) break;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xb4))) == 0) break;
        // the time is out
        sb(m + 0x20, 5);
        const int32_t wait = frame_scale_i(0xf);
        sw(pv + 0xb8, 0);
        sw(pv + 0xbc, static_cast<uint32_t>(wait));
        moby_state_hook(table_moby(lw(pv + 0x30)), 5u);
        level_sound_play_l6(1, 0, lwu(PLAYER + 0x2080));
        break;
    }
    case 5:
        if (lwc1(pv + 0xb8) == 1.0f) {
            hero_lost();
            sw(L6_ADDR(0x0015f400), 0);
        } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xbc))) != 0) {
            approach_f(pv + 0xb8, 1.0f, fpu_mul(lwc1(0x0015ed6c), 6.0f));
            swc1(L6_ADDR(0x0015f400), lwc1(pv + 0xb8));
        }
        break;
    default:
        break;
    }
    ee_stack_free(0x90);
}

}  // namespace rac1
