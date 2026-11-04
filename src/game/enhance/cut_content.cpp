// Cut content brought back (see cut_content.h): behaviours reconstructed, or ported from the beta build.
#include <game/enhance/cut_content.h>

#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/hydro.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

#include <cstdlib>
#include <cstring>
#include <string>

namespace rac1 {

namespace {
// orig: (none) — enhancement: RAC1_CUT_CONTENT's list holds `item` (-1: the variable unset, 0 no, 1 yes)
int cut_content_env(const char* item) {
    const char* e = std::getenv("RAC1_CUT_CONTENT");
    if (e == nullptr) return -1;
    const std::string list = std::string(",") + e + ",";
    return list.find(std::string(",") + item + ",") != std::string::npos ? 1 : 0;
}

// Kerwan's citizens: how near Ratchet must come for them to turn to him, and their turn (yaw_turn_damped's constants:
// the acceleration and the damping the game's turning mobys use, a slow turn at most)
constexpr float FRED_RANGE = 15.0f;
constexpr float FRED_ACCEL = 0.02f, FRED_DAMP = 0.3f, FRED_VMAX = 0.05f;
constexpr uint32_t FRED_KEPT = 1;  // the state byte (+0x20) of a citizen kept: 0 on its first update
constexpr uint32_t FRED_SPIN = 0x1bc;  // the turn's spin, in its pvars
// Their placements stand some of them in the air (never seen in the retail game: they retire at once): three (at x 314
// to 323, y 139 to 146) 0.25 above the ground, four more up to 0.1. At the first update each one is
// put on the ground under it: the collision met by a segment from FRED_SNAP_UP above its placement down to
// FRED_SNAP_DOWN below it (the game's own segment test, segment_collide: the level's triangles and the mobys that
// collide); with nothing there it stays where it was placed.
constexpr float FRED_SNAP_UP = 1.0f, FRED_SNAP_DOWN = 8.0f;

// orig: (none) — enhancement: a kept citizen put on the ground under its placement (above)
void fred_snap_to_ground(uint32_t m) {
    Vec4 from = V(m + 0x10), to = V(m + 0x10);
    from.z = from.z + FRED_SNAP_UP;
    to.z = to.z - FRED_SNAP_DOWN;
    if (segment_collide_v(from, to, 2, m, nullptr) != 0) swc1(m + 0x18, lwc1(OV(0x00173e60) + 8));  // (the hit point)
}

// Eudora's Transport Ships (class 484): the beta's update keeps its own state in the pvars (+0x154: 0 the first update,
// 1 waiting for Ratchet, 2 flying); a ship kept by the item is one past its first update
constexpr uint32_t SHIP_STATE = 0x154;
constexpr uint16_t SHIP_CLASS_SAW = 0x154, SHIP_CLASS_TROOPER = 0x1ab;  // the classes it carries (340, 427)
constexpr uint32_t TROOPER_GUN = 0xf8;  // a Blarg Trooper's gun moby, in its pvars (retail's place; the beta's +0x98)
}  // namespace

// orig: (none) — enhancement (cut_content.h)
bool cut_content_on(const char* item) {
    if (g_advanced_disabled) return false;
    const int env = cut_content_env(item);
    if (env >= 0) return env != 0;
    return advanced_cut_content();
}

// orig: (none) — enhancement (cut_content.h)
void cut_content_fred_dispatch_l3(uint32_t m) {
    // (the choice is made at the citizen's first update, right after the level's load: the original retires it there)
    if (lbu(m + 0x20) == FRED_KEPT || cut_content_on("fred")) {
        cut_content_fred_update_l3(m);
        return;
    }
    fred_update_l3(m);
}

// orig: (none) — cut content restored (cut_content.h). The game's own stub measures what a citizen needs to face the hero
// - the distance to him (0x13f3d0) and the heading towards his moby (0x13ff80 +0x80) against its own (+0x48) - and then
// retires it; here it is kept: its one sequence (set 0) looped, and the heading turned towards Ratchet within FRED_RANGE.
// (That sequence is one frame long in the three models: the rest pose, the arms held out. No collision: Ratchet walks
// through them)
void cut_content_fred_update_l3(uint32_t m) {
    if (lbu(m + 0x20) != FRED_KEPT) {
        sb(m + 0x20, FRED_KEPT);
        fred_snap_to_ground(m);
        moby_anim_set(m, 0, 0, 0);
    } else if ((lbu(m + 0x70) & 2) != 0) {
        moby_anim_set(m, 0, 0, 0);  // (the sequence ended: again)
    }
    // the turn's spin: the last word of its 448 bytes of pvars (placement data no code of the game reads)
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    if (vec3_distance(V(m + 0x10), V(0x0013f3d0)) >= FRED_RANGE) {
        swc1(pv + FRED_SPIN, 0.0f);  // (no spin left over)
        return;
    }
    const float to = atan2_f(lwc1(0x0013ff80 + 0x80) - lwc1(m + 0x10), lwc1(0x0013ff80 + 0x84) - lwc1(m + 0x14));
    yaw_turn_damped(m, pv + FRED_SPIN, to, FRED_ACCEL, FRED_DAMP, FRED_VMAX * lwc1(0x0015ed60));
}

// orig: (none) — enhancement (cut_content.h)
void cut_content_ships_dispatch_l4(uint32_t m) {
    // (the choice is made at the ship's first update, right after the level's load: the original retires it there)
    const uint32_t pv = lwu(m + 0x78);
    if ((pv != 0 && lh(pv + SHIP_STATE) != 0) || cut_content_on("ships")) {
        cut_content_ships_update_l4(m);
        return;
    }
    eud_class484_update_l4(m);
}

// orig: beta 2002-08 L4:0x002a18e8 (prototype 2002-06 L4:0x0029abf0; retail L4:0x002ce060 is moby_retire) — cut content
// restored (cut_content.h): the Transport Ship. Its pvars: +0x74 its path, +0x140..+0x14c the four mobys it carries
// (moby indexes, -1 none), +0x150 the trigger box, +0x154 its state, +0x156 the drop timer, +0x158 dropping, +0x15c the
// path point where it drops (the first point whose w is 1), +0x160 the camera box of its arrival (-1 none), +0x164 the
// camera's timer, +0x168 the camera's seconds, +0x16c the camera's turn, +0x170 the camera's turn limit (degrees).
// Its flight is the shared ship update (beta 0x29e5c8 = retail L4:0x2ca420, which still has its class 484 case).
void cut_content_ships_update_l4(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t mobys = lwu(OV(0x0015ffd8));
    const int32_t state = lh(pv + SHIP_STATE);
    // the scripted camera's end: the hero back to his idle (beta 0x2a1ca8 / 0x2a1d4c)
    auto camera_end = []() {
        hydro_2eac18(3);
        hero_set_state(0, 1);  // (beta 0x200730)
        sw(OV(0x0015f404), 0);
    };
    if (state == 0) {
        // already done on this level (the collected-things row 0x14c050, its entry +0xb0): the ship and what it carries
        // retire
        if (lbu(0x0014c050 + static_cast<uint32_t>(lw(0x0015ed84)) * 16 + lbu(m + 0xb0)) != 0xff) {
            sh(pv + SHIP_STATE, 1);
            sw(pv + 0x158, 0);
            sw(pv + 0x164, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1);  // (hidden until it comes)
            return;
        }
        for (uint32_t k = 0; k < 4; ++k) {
            const int32_t id = lw(pv + 0x140 + k * 4);
            if (id == -1) continue;
            const uint32_t o = mobys + (static_cast<uint32_t>(id) << 8);
            if (o == 0) continue;
            const uint32_t st = lbu(o + 0x20);
            if (st != 0xfe && st != 0xfd) moby_retire(o);
        }
        moby_retire(m);
        return;
    }
    if (state == 1) {
        // Ratchet in its trigger box: it shows, and the arrival's camera starts
        if (point_in_box(0x0013f3d0, lw(pv + 0x150)) != 0) {
            sh(pv + SHIP_STATE, 2);
            sh(m + 0x34, lhu(m + 0x34) & 0xfffe);
            if (lw(pv + 0x160) != -1) {
                hero_set_state(0x1f, 0);  // (beta 0x200730)
                const uint32_t cam = lwu(OV(0x001600ec)) + static_cast<uint32_t>(lw(pv + 0x160)) * 0x80;
                camera_scripted_set(cam + 0x30, cam + 0x70, 0, 0, 0);  // (beta 0x2bcf38)
                sw(OV(0x0015f404), 1);
                sw(pv + 0x164, static_cast<uint32_t>(frame_scale_i(float_to_int(lwc1(pv + 0x168) * 60.0f))));
            }
            const int32_t path = lw(pv + 0x74);
            if (path != -1) {
                const uint32_t p = lwu(OV(0x001b04b0) + static_cast<uint32_t>(path) * 4);
                const int32_t n = lw(p);
                for (int32_t i = 0; i < n; ++i) {
                    if (lwc1(p + 0x1c + static_cast<uint32_t>(i) * 16) == 1.0f) {
                        sw(pv + 0x15c, static_cast<uint32_t>(i));
                        break;
                    }
                }
            }
        }
    } else if (state == 2) {
        eud_class466_update_l4(m);  // (beta 0x29e5c8: its flight along the path)
        if (lw(pv + 0x160) != -1) {
            // the camera turns to follow it, within +0x170 degrees of its box's heading
            const uint32_t view = OV(0x00166c80);
            float to = atan2_f(lwc1(m + 0x10) - lwc1(view + 0x140), lwc1(m + 0x14) - lwc1(view + 0x144));
            const uint32_t cam = lwu(OV(0x001600ec)) + static_cast<uint32_t>(lw(pv + 0x160)) * 0x80;
            const float d = angle_sub(lwc1(cam + 0x78), to);
            constexpr float DEG = 0.017453292f;  // (0x3c8efa35)
            if (d < 0.0f) {
                to = lwc1(cam + 0x78);
            } else if (lwc1(pv + 0x170) * DEG < d) {
                to = angle_add(lwc1(cam + 0x78), -lwc1(pv + 0x170) * DEG);
            }
            const float fs = lwc1(0x0015ed70), fs2 = lwc1(0x0015ed6c);
            const uint32_t rot = ee_stack_alloc(0x10);
            swc1(rot, lwc1(cam + 0x70));
            swc1(rot + 4, lwc1(cam + 0x74));
            swc1(rot + 0xc, 0.0f);
            const float yaw = angle_seek_rate(pv + 0x16c, lwc1(view + 0x158), to, fs * 12.566371f, fs * 125.66371f,
                                              fs2 * 3.1415927f);
            swc1(rot + 8, yaw);
            uid5_set_pvar_80(cam + 0x30);
            uid5_set_pvar_90(rot);
            ee_stack_free(0x10);
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x164))) != 0) {
                camera_end();
                sw(pv + 0x160, 0xffffffffu);
            }
        }
        const int32_t path = lw(pv + 0x74);
        if (path != -1) {
            const uint32_t p = lwu(OV(0x001b04b0) + static_cast<uint32_t>(path) * 4);
            const int32_t at = lw(pv + 0x60);  // (the shared update's point on the path)
            if (at == lw(pv + 0x15c) && lw(pv + 0x158) == 0) {
                sh(pv + 0x156, 0);
                sw(pv + 0x158, 1);  // (the drop point: dropping starts)
            } else if (at == lw(p) - 2) {
                // (the path's end)
                if (lw(pv + 0x164) == 0) {
                    moby_retire(m);
                    return;
                }
                if (lw(pv + 0x160) != -1) camera_end();
            }
        }
    }
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x156)));
    if (lw(pv + 0x158) != 0 && lh(pv + 0x156) == 0) {
        // dropping: the next one it carries let go, facing Ratchet, in its class's drop state; the next 5 to 8 frames on
        const uint32_t hero = 0x0013f350;
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t link = pv + 0x140 + k * 4;
            const int32_t id = lw(link);
            if (id == -1) continue;
            const uint32_t o = mobys + (static_cast<uint32_t>(id) << 8);
            sh(o + 0x34, lhu(o + 0x34) & 0xfff9);
            vec_clear(V(o + 0x40));
            swc1(o + 0x48, atan2_f(lwc1(hero + 0x80) - lwc1(m + 0x10), lwc1(hero + 0x84) - lwc1(m + 0x14)));
            const int16_t cls = lh(o + 0xa6);
            if (cls == SHIP_CLASS_SAW) {
                sb(o + 0x20, 2);
            } else if (cls == SHIP_CLASS_TROOPER) {
                sb(o + 0x20, 4);
            }
            sw(link, 0xffffffffu);
            sh(pv + 0x156, static_cast<uint32_t>(rand_range(5, 8)));
            return;
        }
        return;
    }
    // carrying: each one held at its place under the ship (L4:0x1dc320, four offsets in the ship's frame), hidden
    // while the ship waits
    for (uint32_t k = 0; k < 4; ++k) {
        const int32_t id = lw(pv + 0x140 + k * 4);
        if (id == -1) continue;
        const uint32_t o = mobys + (static_cast<uint32_t>(id) << 8);
        // (a Blarg Trooper's gun: the beta keeps it at the trooper's pvars +0x98, retail at +0xf8 - the same code
        // around it in both builds' class 427 update, beta 0x29a1b0 / retail L4:0x2c62b0)
        if (lh(o + 0xa6) == SHIP_CLASS_TROOPER && lwu(lwu(o + 0x78) + TROOPER_GUN) != 0) {
            moby_retire(lwu(lwu(o + 0x78) + TROOPER_GUN));
            sw(lwu(o + 0x78) + TROOPER_GUN, 0);
        }
        if (lh(pv + SHIP_STATE) == 1) {
            sh(o + 0x34, lhu(o + 0x34) | 1);
            sb(o + 0x30, 0xff);
        } else {
            sh(o + 0x34, lhu(o + 0x34) & 0xfffe);
        }
        Vec4 ofs{};
        mat3_transform(ofs, V(LVL_ADDR(4, 0x001dc320u) + k * 16), M(m + 0xc0));
        vec3_add(V(o + 0x10), V(m + 0x10), ofs);
        lq_sq(o + 0x40, m + 0x40);
        moby_anim_advance(o);  // (beta 0x2282d8)
        moby_update_transform(o);
        sh(o + 0x34, lhu(o + 0x34) | 6);
    }
}

}  // namespace rac1
