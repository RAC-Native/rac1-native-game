// Cut content brought back (see cut_content.h): behaviours reconstructed, or ported from the beta build.
#include <game/enhance/cut_content.h>

#include <formats/level_core.h>
#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/hydro.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>

#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/level/host_layout.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <span>
#include <string>
#include <vector>

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
// The idle they play (fred_idle_hook) is the grind boots seller's crouch, knees bent: the models' root stays at their
// standing height, so a citizen playing it is lowered by this much to keep its feet on the ground (#136; checked in a
// headless shot sheet on Kerwan's streets)
constexpr float FRED_CROUCH = 0.25f;

// orig: (none) — enhancement: a kept citizen put on the ground under its placement (above)
void fred_snap_to_ground(uint32_t m) {
    Vec4 from = V(m + 0x10), to = V(m + 0x10);
    from.z = from.z + FRED_SNAP_UP;
    to.z = to.z - FRED_SNAP_DOWN;
    if (segment_collide_v(from, to, 2, m, nullptr) != 0) swc1(m + 0x18, lwc1(OV(0x00173e60) + 8));  // (the hit point)
}

// Kerwan's citizens' idle (#128: they held their rest pose, arms out). Nebula G34's citizen "Fred" (level 6, class 1105)
// has Fred 3's skeleton (65 joints, the same hierarchy; Fred 1 and 2 have the same with six more joints, 55..60, under
// the head) and the same rest rotations but for the arms (its bones are longer: the rotations carry over, the lengths
// stay each model's own). That citizen is the grind boots seller ("Fred Prime"); it has two sequences: 0, a stiff standing
// idle (50 frames: only the head and a shoulder move, #136), and 1, the one his update plays while he waits (15 frames at
// 0.25 a game frame, 60 game frames, a loop: crouched, the hands together at the chest), the one used here (#136, Anya's
// choice). It is read from the player's disc (level 6's core), once a run, and given to each Fred class in
// place of its one-frame sequence (the class's slot 0), from host memory above the game's 32 MB
// (host_layout::CUT_CONTENT_SEQS): the game's own animation code plays it. Nothing of it is in the repository.
constexpr int IDLE_LEVEL = 6, IDLE_CLASS = 1105, IDLE_SEQ = 1;
constexpr uint32_t IDLE_JOINTS = 65;
constexpr uint32_t IDLE_CLASS_ROOM = 0x10000;  // one Fred class's sequence: the head, the frame table, its frames (0x250 at most)

struct IdleSource {
    bool tried = false;
    uint32_t rate = 0;                             // the sequence's frame rate (+0x18, a float's bits)
    std::vector<std::vector<uint64_t>> rotations;  // a frame's joints' rotations (quaternions, 4 x s16)
    std::vector<std::vector<uint64_t>> moves;      // a frame's joint translations (x, y, z, the joint: 4 x s16)
} g_idle;

// orig: (none) — enhancement: the idle's rotations read from level 6's core on the disc (once a run; none: the citizens
// keep their rest pose)
void fred_idle_read() {
    if (g_idle.tried) return;
    g_idle.tried = true;
    std::vector<std::byte> wad;
    std::vector<uint8_t> index, data;
    if (!disc_read_level_wad(IDLE_LEVEL, wad) || !extract_core_from_level_wad(wad, index, data)) {
        std::printf("[cut content] level %d's core not read: Kerwan's citizens keep their rest pose\n", IDLE_LEVEL);
        return;
    }
    try {
        const DecodedLevelCoreIndex ix = decode_level_core_index(std::as_bytes(std::span(index)), std::as_bytes(std::span(data)));
        for (const LevelCoreSubAsset& a : ix.sub_assets) {
            if (a.kind != "moby_class" || a.id != IDLE_CLASS || a.packed || a.offset >= data.size()) continue;
            const uint8_t* c = data.data() + a.offset;
            const size_t room = data.size() - a.offset;
            auto rd32 = [&](size_t o) {
                uint32_t v = 0;
                if (o + 4 <= room) std::memcpy(&v, c + o, 4);
                return v;
            };
            const uint32_t seq = rd32(0x48 + 4 * IDLE_SEQ);
            if (room < 0x48 || c[8] != IDLE_JOINTS || seq == 0 || seq + 0x1c > room) break;
            g_idle.rate = rd32(seq + 0x18);
            for (uint32_t f = 0, n = c[seq + 0x10]; f < n; ++f) {
                const uint32_t fo = rd32(seq + 0x1c + 4 * f) & 0x0fffffffu;  // (a plain frame: no flag in the top bits)
                uint16_t rot_bytes = 0;
                if (fo == 0 || fo + 0x10 + IDLE_JOINTS * 8 > room) break;
                std::memcpy(&rot_bytes, c + fo + 8, 2);
                if (rot_bytes != IDLE_JOINTS * 8) break;
                std::vector<uint64_t> r(IDLE_JOINTS);
                std::memcpy(r.data(), c + fo + 0x10, IDLE_JOINTS * 8);
                g_idle.rotations.push_back(std::move(r));
                // its translations: +0xe of them, 8 bytes each, at +0xc past the rotations' start
                uint16_t at = 0, count = 0;
                std::memcpy(&at, c + fo + 0xc, 2);
                std::memcpy(&count, c + fo + 0xe, 2);
                std::vector<uint64_t> t(count);
                if (count != 0 && fo + 0x10 + at + count * 8u <= room) std::memcpy(t.data(), c + fo + 0x10 + at, count * 8u);
                else t.clear();
                g_idle.moves.push_back(std::move(t));
            }
            if (g_idle.rotations.size() != c[seq + 0x10]) {
                g_idle.rotations.clear();
                g_idle.moves.clear();
            }
            break;
        }
    } catch (const std::exception& e) {  // (a damaged core: the rest pose, not the game stopped)
        std::printf("[cut content] level %d's core not decoded (%s)\n", IDLE_LEVEL, e.what());
        g_idle.rotations.clear();
        g_idle.moves.clear();
    }
    if (g_idle.rotations.empty()) std::printf("[cut content] class %d's idle not found: Kerwan's citizens keep their rest pose\n", IDLE_CLASS);
}

// orig: (none) — enhancement: Fred class `cls` (915..917) given the idle in place of its one-frame sequence (slot 0),
// built at host_layout::CUT_CONTENT_SEQS on the class's own joints: Fred 3's 65 are the idle's; Fred 1 and 2's six more
// (55..60, under the head) keep their rest rotations, their 61..70 take the idle's 55..64. Each frame keeps the class's
// own frame head (its sizes) and the sequence the class's own head (its bounding sphere) but for the frame count and the
// rate. True when the class plays it (already, or now).
bool fred_idle_hook(uint32_t m) {
    const int32_t id = lh(m + 0xa6);
    const uint32_t cls = lwu(m + 0x24);
    if (id < 915 || id > 917 || cls == 0) return false;
    const uint32_t at = host_layout::CUT_CONTENT_SEQS + static_cast<uint32_t>(id - 915) * IDLE_CLASS_ROOM;
    const uint32_t slot = cls + 0x48;
    if (lwu(slot) == at) return true;
    fred_idle_read();
    const uint32_t joints = lbu(cls + 8);
    const uint32_t seq = lwu(slot);
    const size_t n = g_idle.rotations.size();
    if (n == 0 || seq == 0 || lbu(seq + 0x10) != 1 || (joints != IDLE_JOINTS && joints != IDLE_JOINTS + 6)) return false;
    const uint32_t rest = lwu(seq + 0x1c);  // (its one frame: the rest pose)
    const uint32_t size = 0x10 + lhu(rest + 6) * 16u;
    if (lhu(rest + 8) != joints * 8 || 0x1c + 4 * n + n * size > IDLE_CLASS_ROOM) return false;
    for (uint32_t q = 0; q < 0x1c; q += 4) ee_write<uint32_t>(at + q, lwu(seq + q));
    ee_write<uint8_t>(at + 0x10, static_cast<uint8_t>(n));
    ee_write<uint32_t>(at + 0x18, g_idle.rate);
    const uint32_t frames = (at + 0x1c + 4 * static_cast<uint32_t>(n) + 0xf) & ~0xfu;
    for (size_t f = 0; f < n; ++f) {
        const uint32_t fr = frames + static_cast<uint32_t>(f) * size;
        ee_write<uint32_t>(at + 0x1c + 4 * static_cast<uint32_t>(f), fr);
        for (uint32_t q = 0; q < size; q += 4) ee_write<uint32_t>(fr + q, lwu(rest + q));
        ee_write<uint32_t>(fr, g_idle.rate);  // (the frame's own rate, word 0)
        for (uint32_t j = 0; j < joints; ++j) {
            if (joints != IDLE_JOINTS && j >= 55 && j < 61) continue;  // (Fred 1 / 2's own joints under the head)
            const uint32_t src = joints != IDLE_JOINTS && j >= 61 ? j - 6 : j;
            ee_write<uint64_t>(fr + 0x10 + j * 8, g_idle.rotations[f][src]);
        }
        // the frame's joint translations (the seller's crouch lowers his hips: joint 4), past the rotations, when the
        // class's frame has the room (its size rounds the rotations up to 16 bytes: one translation fits); their joint
        // numbers on the class's own joints
        const std::vector<uint64_t>& mv = g_idle.moves[f];
        const uint32_t rot = joints * 8;
        ee_write<uint16_t>(fr + 0xa, 0);  // (no scales)
        ee_write<uint16_t>(fr + 0xc, static_cast<uint16_t>(rot));
        uint16_t kept = 0;
        for (uint64_t e : mv) {
            if (0x10 + rot + (kept + 1u) * 8u > size) break;
            uint32_t jt = static_cast<uint16_t>(e >> 48);
            if (joints != IDLE_JOINTS && jt >= 55) jt += 6;
            ee_write<uint64_t>(fr + 0x10 + rot + kept * 8u, (e & 0x0000ffffffffffffull) | (static_cast<uint64_t>(jt) << 48));
            ++kept;
        }
        ee_write<uint16_t>(fr + 0xe, kept);
    }
    sw(slot, at);
    return true;
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
// retires it; here it is kept: its sequence 0 looped, and the heading turned towards Ratchet within FRED_RANGE. (That
// sequence is one frame long in the three models: the rest pose, the arms held out; it is replaced by the grind boots
// seller's idle when the disc gives it, fred_idle_hook, the citizen then lowered by FRED_CROUCH. No collision: Ratchet
// walks through them)
void cut_content_fred_update_l3(uint32_t m) {
    if (lbu(m + 0x20) != FRED_KEPT) {
        sb(m + 0x20, FRED_KEPT);
        fred_snap_to_ground(m);
        if (fred_idle_hook(m)) {
            swc1(m + 0x18, lwc1(m + 0x18) - FRED_CROUCH);  // (the seller's crouch: the feet on the ground)
            // the idle, each citizen from its own frame (they do not breathe in step); the game's animation loops it
            const uint32_t n = lbu(lwu(lwu(m + 0x24) + 0x48) + 0x10);
            moby_anim_set(m, 0, static_cast<int32_t>(((m >> 8) * 17u) % n), 1);
        } else {
            moby_anim_set(m, 0, 0, 0);
        }
    } else if (lbu(lwu(lwu(m + 0x24) + 0x48) + 0x10) == 1) {
        if ((lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 0, 0, 0);  // (the one-frame rest pose ended: again)
    } else if ((lhu(m + 0x34) & 0x40) != 0) {
        moby_anim_advance(m);  // (their placements' flags (+0x34 0x40) keep the game's moby loop from advancing it)
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
