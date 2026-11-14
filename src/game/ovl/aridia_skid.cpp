// Aridia's own moby classes: Skid McMarx (the hoverboard race's host) and his agent, with the race's start, hand-ported
// from Aridia's overlay (`orig: L2:`). Aridia-only data is L2_ADDR, the engine's through OV() and the level-data facade;
// the base ELF's (the hero 0x13f350, the race record 0x141968, the story bytes 0x13d4xx) are bare. Declared in
// aridia_enemies.h, registered in aridia_mobys.cpp.
//
// Region: the PAL disc 2.00 (the 1.02 build, region_core_moved()) keeps the scene's walk-to flag as a byte at +0x4a of
// the scene block, the North American build as a halfword at +0x46: both NPCs read and write it there (the 2.00 disc's
// own code: L2:0x2e2d88 / 0x2e21f8, otherwise the same code with its addresses moved), as cutscene.cpp's scene end and
// hero_walk_to_l1 do.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/hydro.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/region_core.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/variants_l2.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position, +0xd0 his head, +0x100 his velocity)

// the scene block (the engine's 0x16c860) and its walk-to flag (see the file's head)
uint32_t scene() { return OV(0x0016c860); }
bool walk_to_set() { return region_core_moved() ? lbu(scene() + 0x4a) != 0 : lh(scene() + 0x46) != 0; }
void walk_to_flag_set() {
    if (region_core_moved()) sb(scene() + 0x4a, 1);  // (region: the 1.02 build's byte)
    else sh(scene() + 0x46, 1);
}

// the scene told to put the hero 2.5 in front of the NPC, facing it, when it ends (the walk-to flag set); Skid raises the
// spot 10 before the floor probe, the agent probes from it as it is
void walk_to_spot(uint32_t m, bool raise) {
    const uint32_t b = scene();
    vec3_set_length(V(b + 0x10), V(m + 0xc0), 2.5f);
    vec3_add(V(b + 0x10), V(b + 0x10), V(m + 0x10));
    if (raise) swc1(b + 0x18, fpu_add(lwc1(b + 0x18), 10.0f));
    swc1(b + 0x18, floor_probe_v(0.5f, V(b + 0x10), 0));
    vec_clear(V(b + 0x20));
    swc1(b + 0x28, angle_add(lwc1(m + 0x48), u2f(0x40490fd0u)));
    walk_to_flag_set();
}

// The NPC's idle animation: now and then (every 1200 .. 2400 frames, the timer at `timer`) a fidget (set `fidget`), back
// to the set 0 when it has played out.
void idle_fidget(uint32_t m, uint32_t timer, uint32_t fidget) {
    if (lbu(m + 0x53) == 0) {
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(timer))) == 0) return;
        sh(timer, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(1200.0f, 2400.0f)))));
        if (lbu(m + 0x53) != fidget) moby_anim_set(m, fidget, 0, frame_scale_i(0xa));
        return;
    }
    if (lbu(m + 0x53) != 1 || (lbu(m + 0x70) & 2) == 0) return;
    moby_anim_set(m, 0, 0, frame_scale_i(0xa));
}

// Where the NPC looks (its head joint 0, its neck joint 1: the spring records +0x40 / +0xc0), as both NPCs do it with
// their own offsets: the hero's head while he is near (within 8) in front of it (within 90 degrees) and moving (the hold
// timer `hold` rearmed for 120 frames) or for the hold's end; else a point 6 away at a random heading (+-90 degrees) and
// pitch (0 .. 30) about it, picked every 180 .. 300 frames (the timer `pick`) into `spot`; only while it plays its
// animation set 0. The head's yaw and pitch (+0xa8 / +0xa4) and the neck's (+0x128) from it, clamped.
struct Look {
    uint32_t hold, pick, spot;  // pvar offsets
    bool wide;                  // the timers are words (Skid) or halves (the agent)
    float head_up, head_k, neck_k;
};
void look_about(uint32_t m, uint32_t pv, uint32_t frame, const Look& c) {
    float k_spring = u2f(0x3ca3d70au), k_damp = u2f(0x3e99999au);  // (0.02, 0.3)
    auto tick = [&](uint32_t t) {
        return c.wide ? timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + t))) : timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + t)));
    };
    auto held = [&] { return c.wide ? lw(pv + c.hold) : static_cast<int32_t>(lh(pv + c.hold)); };
    auto hold_set = [&](uint32_t v) { if (c.wide) sw(pv + c.hold, v); else sh(pv + c.hold, v); };
    if (lbu(m + 0x53) == 0) {
        const uint32_t target = frame, from = frame + 0x10, d = frame + 0x20;
        bool tracking = false;
        if (vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 8.0f) {
            const float yaw = atan2_f(fpu_sub(lwc1(HERO + 0xd0), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0xd4), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3fc90fdbu)) {
                tracking = true;
                if (u2f(0x3c23d70au) < vec3_length(V(HERO + 0x100))) hold_set(static_cast<uint32_t>(frame_scale_i(0x78)));
                else tick(c.hold);
            }
        }
        if (!tracking && held() != 0) {
            hold_set(0);
            lq_sq(pv + c.spot, HERO + 0xd0);
        }
        if (tick(c.pick) != 0) {
            const float deg = u2f(0x3c8efa35u);
            const uint32_t next = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f))));
            if (c.wide) sw(pv + c.pick, next); else sh(pv + c.pick, next);
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float pitch = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(pv + c.spot), 6.0f, yaw, pitch);
            vec3_add(V(pv + c.spot), V(pv + c.spot), V(m + 0x10));
        }
        if (held() != 0) {
            lq_sq(target, HERO + 0xd0);
            k_spring = u2f(0x3d23d70au);  // (0.04)
            k_damp = u2f(0x3e99999au);
        } else {
            lq_sq(target, pv + c.spot);
        }
        lq_sq(from, m + 0x10);
        swc1(from + 8, fpu_add(lwc1(from + 8), c.head_up));
        vec3_sub(V(d), V(target), V(from));
        float yaw = angle_sub(atan2_f(lwc1(d), lwc1(d + 4)), lwc1(m + 0x48));
        const float pitch_raw = atan2_f(vec2_length(V(d)), lwc1(d + 8));
        float pitch = -pitch_raw;
        const float quarter = u2f(0x3fc90fdbu), sixth = u2f(0x3f060a92u);
        if (quarter < yaw) yaw = quarter;
        else if (yaw < -quarter) yaw = -quarter;
        if (sixth < pitch) pitch = sixth;
        else if (pitch < -sixth) pitch = -sixth;
        swc1(pv + 0xa4, pitch);
        swc1(pv + 0xa8, fpu_mul(yaw, c.head_k));
        swc1(pv + 0x128, fpu_mul(yaw, c.neck_k));
    }
    if (lbu(0x0015edb0) != 0) swc1(pv + 0xb0, 2.75f);
    const float t = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0x40, 0, fpu_mul(k_spring, t), fpu_mul(k_damp, t));
    const float t2 = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0xc0, 1, fpu_mul(k_spring, t2), fpu_mul(k_damp, t2));
}
}  // namespace

// Skid's race start (pvar +0x180: 0 waiting, 1 the hero in the race box +0x148, 2 started): the hero in the box asks the
// two race cameras (0x15 / 0x17, their HUD callbacks 0x23c990 / 0x23ccc8 / 0x23cda8, their done words +0x14c / +0x150);
// out of the box again they are dropped; with both done and the wait +0x168 over, the race starts: a fade, the cameras
// dropped, Skid at the start box +0x154 (facing along it), the hero put 1.5 in front of him facing him, the two racers'
// (+0x158 / +0x15c) and Skid's flags set, the talk's state +0x36 2.
// orig: L2:0x002e1fb8
void skid_race_start_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t mode = lw(pv + 0x180);
    if (mode == 1) {
        if (point_in_box(HERO + 0x80, lw(pv + 0x148)) == 0) {
            sw(pv + 0x180, 0);
            sub_002359f8(lwu(pv + 0x160), 0);
            sw(pv + 0x160, 0xffffffffu);
            sub_002359f8(lwu(pv + 0x164), 0);
            sw(pv + 0x164, 0xffffffffu);
            return;
        }
        if (lw(pv + 0x14c) != 0 || lw(pv + 0x150) != 0) return;
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x168))) == 0) return;
        sw(pv + 0x180, 2);
        screen_fade(frame_scale_i(0x10));
        sub_002359f8(lwu(pv + 0x160), 0);
        const uint32_t cam2 = lwu(pv + 0x164);
        sw(pv + 0x160, 0xffffffffu);
        sub_002359f8(cam2, 0);
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x154));
        sw(pv + 0x164, 0xffffffffu);
        lq_sq(m + 0x10, box + 0x30);
        swc1(m + 0x48, atan2_f(lwc1(box), lwc1(box + 4)));
        const uint32_t sp = ee_stack_alloc(0xa0);
        const uint32_t at = sp, rot = sp + 0x10;
        lq_sq(at, m + 0x10);
        swc1(at, fpu_add(lwc1(at), fpu_mul(cos_vu(lwc1(m + 0x48)), 1.5f)));
        swc1(at + 4, fpu_add(lwc1(at + 4), fpu_mul(sin_vu(lwc1(m + 0x48)), 1.5f)));
        vec_clear(V(rot));
        swc1(rot + 8, angle_add(lwc1(m + 0x48), u2f(0x40490fdbu)));
        hero_place(at, rot, 0, 1);
        ee_stack_free(0xa0);
        const uint32_t mobys = lwu(OV(0x0015ffd8));
        sb(mobys + (lwu(pv + 0x158) << 8) + 0xbc, 1);
        sb(mobys + (lwu(pv + 0x15c) << 8) + 0xbc, 1);
        sb(pv + 8, 1);
        sh(pv + 0x36, 2);
        return;
    }
    if (mode != 0) return;
    if (point_in_box(HERO + 0x80, lw(pv + 0x148)) == 0) return;
    sw(pv + 0x180, 1);
    sw(pv + 0x164, static_cast<uint32_t>(camera_request(0x15, 0x7d0, L2_ADDR(0x0023c990), L2_ADDR(0x0023ccc8), L2_ADDR(0x0023cda8),
                                                      pv + 0x14c, 0x64)));
    sw(pv + 0x160, static_cast<uint32_t>(camera_request(0x17, 0x7d1, L2_ADDR(0x0023c990), L2_ADDR(0x0023ccc8), L2_ADDR(0x0023cda8),
                                                      pv + 0x150, 7)));
}

// Skid McMarx (the talk record at his pvars, its name 0x161d00): gone once his race was won (0x13d506); state 1 talks
// (the talk's reach +0xc: 255 in the race box +0x16c after the race, else 2.5); a talk that ends puts the hero in front
// of him (2); the talk's state 2 (the race won) gives the hoverboard's reward (item 0x1e), the checkpoint and the level's
// change, and he is gone; state 2 tells the scene where to put the hero (the walk-to flag, see the head) and goes back to
// 1 once the frame's mode 0x15f5c4 is not 2. He fidgets (sets 1 / 2) and looks about.
// orig: L2:0x002e1950
void skid_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_fixed(m, 0);
    skid_race_start_l2(m);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        if (lbu(0x0013d506) != 0) {
            moby_retire(m);
            return;
        }
        sw(pv + 0x180, 0);
        sw(pv + 0x20, L2_ADDR(0x00161d00));
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        talk_init(m, pv);
        band_set(m);
    } else if (state == 1) {
        if (lbu(pv + 8) == 1 && point_in_box(HERO + 0x80, lw(pv + 0x16c)) != 0) swc1(pv + 0xc, 255.0f);
        else swc1(pv + 0xc, 2.5f);
        if (talk_update(m, pv) != 0) {
            hero_walk_to_l1(2.5f, m);
            sb(m + 0x20, 2);
        }
        if (lh(pv + 4) == 2) {  // the race won: the reward
            sub_00203908(0x7d0, 0xa);
            item_give(0x1e, 1);
            checkpoint_mark_collected(lbu(m + 0xb0));
            checkpoint_save(HERO + 0x80, HERO + 0x90);
            level_change_request(0, -1);
            moby_retire(m);
            return;
        }
        if (lbu(m + 0x53) == 0) {
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x16a))) != 0) {
                sh(pv + 0x16a, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(1200.0f, 2400.0f)))));
                const uint32_t set = rand_mod(2) == 0 ? 2u : 1u;
                if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(0xa));
            }
        } else if ((lbu(m + 0x70) & 2) != 0 && lbu(m + 0x53) != 0) {
            moby_anim_set(m, 0, 0, frame_scale_i(0xa));
        }
    } else if (state == 2) {
        if (!walk_to_set()) walk_to_spot(m, true);
        if (lw(L2_ADDR(0x0015f5c4)) != 2) sb(m + 0x20, 1);
    }
    const uint32_t sp = ee_stack_alloc(0xc0);
    look_about(m, pv, sp, Look{0x188, 0x18c, 0x170, true, 2.0f, u2f(0x3f19999au), u2f(0x3ecccccdu)});
    ee_stack_free(0xc0);
}

// Skid's agent (the talk record at its pvars, its name 0x161cf0), holding a gadget (class 0x38, pvar +0x178,
// at its joint 2): gone once it was bought (0x13d4ed); state 0 sets up (the talk, the gadget); 1 talks (its reach +0xc:
// 255 in its box +0x154 after the race, else 3.7); a talk that ends puts the hero in front of it (2); next to the hero
// (within 3, until 0x13d4c5 is set) it counts the hero's visits on the race record 0x141968 (+0x70 / +0x72 / +0x74,
// by 0x258-frame steps of the play time 0x15eea4) or comments (text 0x7d4); state 2 tells the scene where to put the
// hero (the walk-to flag, see the head) while the frame's mode 0x15f5c4 is 2, then back to 1: the talk's state 0 saves
// the checkpoint at the box +0x150, state 2 (the deal) hides both and plays the level's movie 2 (5); 5 gives the item 5
// and changes the level; 3 / 4 hand the gadget over along the boxes +0x158 -> +0x15c (pvar +0x180, the class-5 moby's
// place and turn, the fade +0x17c into 0x15f3fc), then the gadget and the agent are gone.
// orig: L2:0x002e0dc0
void skid_agent_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_fixed(m, 0);
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t race = 0x00141968u;
    auto run = [&]() -> bool {  // false: gone (no gadget, no looking about)
        switch (lbu(m + 0x20)) {  // (the original's jump table at L2:0x1fab00)
        case 0: {  // (0x002e0e20)
            if (lbu(0x0013d4ed) != 0) {
                moby_retire(m);
                return false;
            }
            sw(pv + 0x20, L2_ADDR(0x00161cf0));
            sb(m + 0x20, 1);
            talk_init(m, pv);
            band_set(m);
            sw(pv + 0x178, moby_spawn(0x38));
            sh(lwu(pv + 0x178) + 0x32, 0x40);
            sb(lwu(pv + 0x178) + 0x31, 1);
            sd(lwu(pv + 0x178) + 0x38, ld(m + 0x38));
            const uint32_t g = lwu(pv + 0x178);
            sh(g + 0x34, lhu(g + 0x34) | 0x100u);
            return true;
        }
        case 1: {  // (0x002e0e90)
            if (lbu(pv + 8) == 1 && point_in_box(HERO + 0x80, lw(pv + 0x154)) != 0) swc1(pv + 0xc, 255.0f);
            else swc1(pv + 0xc, u2f(0x406ccccdu));
            if (talk_update(m, pv) != 0) {
                hero_walk_to_l1(u2f(0x400ccccdu), m);
                sb(m + 0x20, 2);
            } else if (lbu(0x0013d4c5) == 0 && vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 3.0f) {
                hint_visit(race + 0x70, 0x7d4, 0xe);  // (the hero's visits to it: dialogue 0x7d4 when due)
            }
            idle_fidget(m, pv + 0x172, 1);
            return true;
        }
        case 2: {  // (0x002e112c)
            if (lw(L2_ADDR(0x0015f5c4)) == 2) {
                if (!walk_to_set()) walk_to_spot(m, false);
                return true;
            }
            sb(m + 0x20, 1);
            const int32_t talk = lh(pv + 4);
            if (talk == 0) {
                checkpoint_mark_collected(lbu(m + 0xb0));
                const int32_t b = lw(pv + 0x150);
                if (b == -1) return true;
                const uint32_t box = lvd::box(lvd::BOXES, b);
                checkpoint_save(box + 0x30, box + 0x70);
            } else if (talk == 2) {
                sb(m + 0x31, 0);
                sw(m + 0x94, 0);
                sh(m + 0x34, lhu(m + 0x34) | 1u);
                const uint32_t g = lwu(pv + 0x178);
                if (g != 0) {
                    sh(g + 0x34, lhu(g + 0x34) | 1u);
                    sb(lwu(pv + 0x178) + 0x31, 0);
                }
                level_movie_play(2);
                sb(m + 0x20, 5);
            }
            return true;
        }
        case 5:  // (0x002e1254)
            item_give(5, 1);
            level_change_request(0, -1);
            moby_retire(m);
            return false;
        case 3: {  // (0x002e1274)
            const float k = u2f(0x3dcccccdu);  // (0.1)
            seek_1d(pv + 0x180, pv + 0x184, 1.0f, fpu_mul(lwc1(0x0015ed70), k), fpu_mul(lwc1(0x0015ed70), 0.25f),
                    fpu_mul(lwc1(0x0015ed6c), 0.5f));
            const float t = lwc1(pv + 0x180);
            const uint32_t from = lvd::box(lvd::BOXES, lw(pv + 0x158)), to = lvd::box(lvd::BOXES, lw(pv + 0x15c));
            vec3_lerp(V(sp), V(from + 0x30), V(to + 0x30), t);
            lq_sq(sp + 0x20, from + 0x70);
            lq_sq(sp + 0x30, to + 0x70);
            for (const uint32_t k3 : {8u, 4u, 0u}) {  // (z, y, x: the turn's angles eased along)
                const float a0 = lwc1(sp + 0x20 + k3);
                swc1(sp + 0x10 + k3, angle_add(fpu_mul(angle_sub(lwc1(sp + 0x30 + k3), a0), lwc1(pv + 0x180)), lwc1(sp + 0x20 + k3)));
            }
            uid5_set_pvar_80(sp);
            uid5_set_pvar_90(sp + 0x10);
            if (lwc1(pv + 0x180) < 1.0f) {
                approach_f(pv + 0x17c, 0.0f, k);
            } else {
                approach_f(pv + 0x17c, 1.0f, k);
                if (lwc1(pv + 0x17c) == 1.0f) {
                    sb(m + 0x20, 4);
                    hydro_2eac18(0);
                }
            }
            swc1(L2_ADDR(0x0015f3fc), lwc1(pv + 0x17c));
            return true;
        }
        case 4: {  // (0x002e1408)
            approach_f(pv + 0x17c, 0.0f, u2f(0x3dcccccdu));
            const float f = lwc1(pv + 0x17c);
            swc1(L2_ADDR(0x0015f3fc), f);
            if (!(f == 0.0f)) return true;
            if (lwu(pv + 0x178) != 0) moby_retire(lwu(pv + 0x178));
            moby_retire(m);
            return false;
        }
        default:  // past the table
            return true;
        }
    };
    if (run()) {
        const uint32_t g = lwu(pv + 0x178);
        if (g != 0) {  // the gadget in its hand (joint 2), turned with it, offset by the level's 0x161ce0
            moby_chain_point_v(m, 2, V(g + 0x10));
            moby_chain_world_one(m, 2, sp);
            mat3_copy(M(lwu(pv + 0x178) + 0xc0), M(sp));
            mat3_transform(V(sp + 0x40), V(L2_ADDR(0x00161ce0)), M(lwu(pv + 0x178) + 0xc0));
            vec3_add(V(lwu(pv + 0x178) + 0x10), V(lwu(pv + 0x178) + 0x10), V(sp + 0x40));
            moby_update_transform(lwu(pv + 0x178));
        }
        look_about(m, pv, sp, Look{0x174, 0x176, 0x160, false, 1.0f, u2f(0x3f333333u), u2f(0x3e99999au)});
    }
    ee_stack_free(0xd0);
}

}  // namespace rac1
