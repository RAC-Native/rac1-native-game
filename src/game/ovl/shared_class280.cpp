// Class 280 of Novalis (L1), Eudora (L4) and Batalia (L8), the pump the hero works with the Hydrodisplacer (item 8) in hand:
// one port of the three levels' copies, which are the same code relinked (word by word: only the relocations differ; the
// state table's entries the same offsets). Its addresses are the base ELF's (bare: the hero 0x13f350.., the level number
// 0x15ed84, the per-level flag words 0x14c190 and bytes 0x14c050, the frame time 0x15ed6c / its square 0x15ed70 / the
// time step 0x15ed64, the cutscene flag 0x15edb5), the engine's (OV: the camera 0x166dc0.. and its current one 0x166e00,
// the camera records 0x1600ec, the moby list 0x15ffe4, the moby flags 0x1bb230 / 0x1ba4d0, the checkpoint list 0x1ba620)
// or the level's own: one float, the spring of the hero's distance to the pump. Novalis's build of uid5_set_pvar_90
// (0x316e28) is its own (still generated). Header shared_class280.h.
//
// The pvars: the work done +0 (0..1), the frames since it was last worked +0x14, the turn accumulated +0x18, the hero's
// speed round it +0x1c, the camera record +0x20, the hero's fall +0x24, the camera's angles +0x28 / +0x2c, the turns for
// the whole work +0x30, the checkpoint record +0x34 (-1 none), "done when released" +0x38, the pump's top +0x3c.
#include <game/ovl/shared_class280.h>

#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/hydro.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t HERO_POS = 0x0013f3d0;  // (+0x80)
constexpr uint32_t PUMP_STATE = 0x3b;       // the hero's state at the pump
constexpr uint32_t HYDRODISPLACER = 8;      // the item in hand

// the level's own float: the spring of the hero's distance to the pump
uint32_t spring_word() {
    if (on_level(NOVALIS)) return L1_ADDR(0x001618a0);
    if (on_level(EUDORA)) return L4_ADDR(0x00161840);
    return L8_ADDR(0x00161930);
}
// the scripted camera's pvar +0x90 (its rotation) from `src` (Novalis's own build)
void camera_rot_set(uint32_t src) {
    if (on_level(NOVALIS)) {
        nov_316e28_l1(src, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    uid5_set_pvar_90(src);
}

float frame_time() { return lwc1(0x0015ed6c); }
float frame_time2() { return lwc1(0x0015ed70); }
uint32_t camera_record(int32_t i) { return lwu(OV(0x001600ec)) + (static_cast<uint32_t>(i) << 7); }

// the pump's flag (its index +0xb2: the byte at 0x1bb230 + 0x454, or the bit of the level's words 0x14c190): worked before
bool done_before(uint32_t m) {
    const uint32_t idx = lhu(m + 0xb2);
    const int32_t sidx = static_cast<int16_t>(idx);
    if (lbu(OV(0x001bb230) + static_cast<uint32_t>(sidx) + 0x454) != 0) return true;
    const uint32_t word = lwu(0x0014c190 + (static_cast<uint32_t>(sidx >> 5) << 2) + (lwu(0x0015ed84) << 8));
    return ((static_cast<int32_t>(word) >> (idx & 0x1f)) & 1) != 0;
}
// the checkpoint the pump holds (+0x34) taken when its flag byte of the level is set
void checkpoint_take(uint32_t m, uint32_t pv) {
    if (lw(pv + 0x34) == -1) return;
    if (lbu(0x0014c050 + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff) return;
    moby_data_note_l1(pv, 4, m, 2, OV(0x001ba620));
    checkpoint_mark_collected(lbu(m + 0xb0));
    const int32_t rec = lw(pv + 0x34);
    if (rec != -1) checkpoint_save(camera_record(rec) + 0x30, camera_record(rec) + 0x70);
}
}  // namespace

// orig: L1:0x002e0bd8 L4:0x002bfca0 L8:0x002d81e0
void class280_level_flags(uint32_t m) {
    uint32_t level = lwu(0x0015ed84);
    if (level != 1) return;
    if (lh(m + 0xb2) == 0x34 && lh(m + 0xa6) == 0x118 && lbu(m + 0x20) == 5) {
        sb(0x0013d394, level);
        level = lwu(0x0015ed84);
    }
    if (level == 1 && lh(m + 0xb2) == 0x35 && lh(m + 0xa6) == 0x118 && lbu(m + 0x20) == 5) sb(0x0013d395, level);
}

// orig: L1:0x00316e88 L4:0x002ee798 L8:0x00313880
void camera5_angles_set(float yaw, float pitch) {
    const uint32_t cam = lwu(OV(0x00166e00));
    const uint32_t m = lh(cam + 0x86) == 5 ? cam : moby_table_find_by_uid(5);
    const uint32_t p = lwu(m + 0x70) + 0xe0;
    swc1(p + 0x48, pitch);
    swc1(p + 0x44, yaw);
}

// orig: L1:0x002e0c68 L4:0x002bfd30 L8:0x002d8270
void class280_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float two_pi = u2f(0x40c90fdb);
    if (pv == 0) {  // (a pump without pvars: it spins)
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(frame_time(), two_pi)));
        return;
    }
    // the hero left the pump's state while it was worked: released, the Hydrodisplacer put away
    if (lwu(HERO + 0x2084) != PUMP_STATE && lbu(m + 0x20) == 3) {
        sb(m + 0x20, 1);
        if (lwu(OV(0x00166e00)) == 0) return;
        hydro_2eac18(4);
        return;
    }
    const uint32_t sp = ee_stack_alloc(0x140);
    switch (lbu(m + 0x20)) {  // (the table 0x1fbd30: Novalis 0x20ae00, Eudora 0x1dbba0)
        case 0:  // (0x2d8344) its top
            swc1(pv + 0x3c, lwc1(m + 0x18));
            sb(m + 0x20, 1);
            break;
        case 1: {  // (0x2d8358) waiting: done before, or the hero at it facing it with the Hydrodisplacer, pressing
            if (done_before(m)) {
                sb(m + 0x20, 5);
                swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 0.5f));
                swc1(pv, 1.0f);
                break;
            }
            bool engaged = false;
            if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 1.75f && lbu(HERO + 0x20a4) == 0) {
                const float to = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84)));
                if (angle_dist(lwc1(HERO + 0x98), to) < u2f(0x3fc90fdb) &&
                    abs_f(fpu_sub(fpu_sub(lwc1(HERO + 0x88), u2f(0x3e99999a)), lwc1(m + 0x18))) < u2f(0x3e4ccccd) &&
                    lwu(HERO + 0x10b8) == HYDRODISPLACER && lwu(HERO + 0x1090) != 0 &&
                    pad_button_history(0x80, frame_scale_i(7), nullptr) != 0 &&
                    static_cast<uint32_t>(frame_scale_i(30)) < lwu(pv + 0x14) && lwc1(m + 0x18) == lwc1(pv + 0x3c) &&
                    lwc1(pv) < 1.0f) {
                    engaged = true;
                }
            }
            if (engaged) {
                sw(pv + 0x14, 0);
                sw(spring_word(), 0);
                // nothing in the way: no moby (of the list 0x15ffe4, flag 0x1000) within 3.25 of it
                bool blocked = false;
                for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
                    if ((lhu(o + 0x34) & 0x1000) != 0 && vec2_distance(V(m + 0x10), V(o + 0x10)) < 3.25f) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) break;
                hero_set_state(PUMP_STATE, 1);
                camera_scripted_set(OV(0x00166dc0), OV(0x00166dd0), 3, static_cast<uint32_t>(frame_scale_i(0xb4)), 0);
                camera_rot_set(camera_record(lw(pv + 0x20)) + 0x70);
                uid5_set_pvar_80(camera_record(lw(pv + 0x20)) + 0x30);
                camera5_angles_set(lwc1(pv + 0x28), lwc1(pv + 0x2c));
                sb(m + 0x20, 3);
                sw(pv + 0x24, 0);
                swc1(pv + 0x18, fpu_mul(lwc1(pv), fpu_mul(lwc1(pv + 0x30), two_pi)));
                break;
            }
            if (lwc1(pv) < 1.0f) {
                // not worked: the work runs down over a second, the pump turning back and rising to its top
                sw(pv + 0x14, lwu(pv + 0x14) + 1);
                approach_f(pv, 0.0f, fpu_div(1.0f, static_cast<float>(frame_scale_i(60))));
                if (0.0f < lwc1(pv)) {
                    const float back = fpu_div(fpu_mul(lwc1(pv + 0x30), two_pi), static_cast<float>(frame_scale_i(60)));
                    swc1(m + 0x48, angle_sub(lwc1(m + 0x48), back));
                }
                const float z = fpu_add(lwc1(m + 0x18), fpu_mul(frame_time(), 0.5f));
                swc1(m + 0x18, z);
                if (lwc1(pv + 0x3c) <= z) swc1(m + 0x18, lwc1(pv + 0x3c));
                break;
            }
            // worked through: its checkpoint (when the pump keeps one), then it sinks half a unit
            if (lwu(pv + 0x38) == 0) break;
            checkpoint_take(m, pv);
            const float z = fpu_sub(lwc1(m + 0x18), fpu_mul(frame_time(), 0.5f));
            swc1(m + 0x18, z);
            const float floor = fpu_sub(lwc1(pv + 0x3c), 0.5f);
            if (z <= floor) swc1(m + 0x18, floor);
            break;
        }
        case 3: {  // (0x2d87c0) worked: the hero walks round it with the stick, the pump turning as he goes
            const uint32_t angles = sp + 0x70, turn = sp, stick = sp + 0x40, push = sp + 0x50, along = sp + 0x60;
            const uint32_t at = sp + 0x80, to_hero = sp + 0x90, dist = sp + 0xa0;
            V(at) = Vec4{};
            swc1(at + 8, lwc1(OV(0x00166dd8)));
            lq_sq(angles, at);
            const float from = atan2_f(fpu_sub(lwc1(HERO + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0x84), lwc1(m + 0x14)));
            // released (the button let go after a second), or the work complete
            bool release = false;
            if (pad_button_history(0x80, frame_scale_i(7), nullptr) != 0 && frame_scale_i(60) < lw(HERO + 0x198)) release = true;
            else release = lwc1(pv) == 1.0f;
            if (release) {
                hero_set_state(0, 1);
                if (lwu(pv + 0x38) != 0) {
                    sb(m + 0x20, 1);
                } else if (lwc1(pv) == 1.0f) {
                    // done: its flags, a sound, its checkpoint
                    moby_data_note_l1(pv, 4, m, 2, OV(0x001ba620));
                    sb(m + 0x20, 4);
                    uint32_t idx = lhu(m + 0xb2);
                    const uint32_t w = 0x0014c190 + (static_cast<uint32_t>(static_cast<int16_t>(idx) >> 5) << 2) + (lwu(0x0015ed84) << 8);
                    sw(w, lwu(w) | (1u << (idx & 0x1f)));
                    idx = lhu(m + 0xb2);
                    const uint32_t b = OV(0x001ba4d0) + (static_cast<uint32_t>(static_cast<int16_t>(idx) >> 5) << 2);
                    sw(b, lwu(b) | (1u << (idx & 0x1f)));
                    moby_sound_start(0, 0, m);
                    checkpoint_take(m, pv);
                }
                hydro_2eac18(4);
                ee_stack_free(0x140);
                return;
            }
            // the stick (camera relative, at most 1) along the hero's facing: his speed round the pump
            swc1(stick + 4, -lwc1(HERO + 0x1d20));
            swc1(stick, -lwc1(HERO + 0x1d24));
            const float len = vec2_length(V(stick));
            if (1.0f < len) vec3_div(V(stick), V(stick), len);
            euler_to_mat4_vu(M(turn), V(angles));
            mat4_transform(V(stick), V(stick), M(turn));
            swc1(along, cos_vu(lwc1(lwu(HERO + 0x2080) + 0x48)));
            swc1(along + 4, sin_vu(lwc1(lwu(HERO + 0x2080) + 0x48)));
            sw(along + 8, 0);
            const float speed = lwc1(pv + 0x1c);
            float change = fpu_sub(vec3_dot(V(along), V(stick)), speed);
            const float most = fpu_mul(frame_time2(), 5.5f);
            if (most < change) change = most;
            if (change < -most) change = -most;
            const float sum = fpu_add(speed, change);
            const float top = fpu_mul(frame_time(), u2f(0x406ccccd));
            swc1(pv + 0x1c, top < sum ? top : sum);
            if (lwc1(pv + 0x1c) < 0.0f) swc1(pv + 0x1c, 0.0f);
            if (abs_f(lwc1(pv + 0x1c)) < fpu_mul(fpu_mul(frame_time2(), 5.5f), 0.25f)) swc1(pv + 0x1c, 0.0f);
            vec3_set_length(V(push), V(along), lwc1(pv + 0x1c));
            // the hero's animations: pushing (0x40 / 0x41 by the speed), standing (0x3f)
            bool check_hold = true;
            if ((lwu(HERO + 0xa98) & 2) != 0) {
                if (lbu(lwu(HERO + 0x2080) + 0x53) == 0x44) hero_anim_set(0x3f, 0, static_cast<float>(frame_scale_i(7)));
                else check_hold = false;
            }
            if (!(check_hold && lbu(lwu(HERO + 0x2080) + 0x53) == 0x44) && lwu(HERO + 0xa9c) == 0) {
                const float fast = fpu_mul(frame_time(), u2f(0x406ccccd));
                const uint32_t cur = lbu(lwu(HERO + 0x2080) + 0x52);
                bool slow_check = false;
                if (cur == 0x40 && abs_f(lwc1(pv + 0x1c)) == 0.0f) {
                    hero_anim_set(0x3f, 0, static_cast<float>(frame_scale_i(14)));
                } else if (cur == 0x3f) {
                    if (0.0f < lwc1(pv + 0x1c)) hero_anim_set(0x40, 0, static_cast<float>(frame_scale_i(14)));
                } else if (cur == 0x40) {
                    if (fpu_mul(fast, u2f(0x3f19999a)) < lwc1(pv + 0x1c)) hero_anim_set(0x41, 0, static_cast<float>(frame_scale_i(14)));
                    else slow_check = true;
                } else {
                    slow_check = true;
                }
                if (slow_check && lbu(lwu(HERO + 0x2080) + 0x52) == 0x41 &&
                    lwc1(pv + 0x1c) < fpu_mul(fpu_mul(frame_time(), u2f(0x406ccccd)), u2f(0x3ecccccd)))
                    hero_anim_set(0x40, 0, static_cast<float>(frame_scale_i(16)));
            }
            // the hero moved by it and held at his distance (a spring) round the pump, his height kept, turned along
            const float z = lwc1(HERO + 0x88);
            vec3_add(V(HERO_POS), V(HERO_POS), V(push));
            const float yaw = atan2_f(fpu_sub(lwc1(HERO + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0x84), lwc1(m + 0x14)));
            const float r = u2f(0x3f866666);  // 1.05
            swc1(push, fpu_mul(cos_vu(yaw), r));
            swc1(push + 4, fpu_mul(sin_vu(yaw), r));
            sw(push + 8, 0);
            vec3_add(V(at), V(m + 0x10), V(push));
            vec3_sub(V(to_hero), V(at), V(HERO_POS));
            sw(to_hero + 8, 0);
            swc1(dist, -vec2_length(V(to_hero)));
            const float step = lwc1(0x0015ed64);
            spring_to(dist, spring_word(), 0.0f, fpu_mul(step, u2f(0x3c75c28f)), fpu_mul(step, u2f(0x3e99999a)), fpu_mul(frame_time(), 7.0f));
            vec3_set_length(V(to_hero), V(to_hero), lwc1(spring_word()));
            vec3_add(V(HERO_POS), V(HERO_POS), V(to_hero));
            swc1(HERO + 0x88, z);
            const float side = lbu(0x0015edb5) != 0 ? u2f(0xbfc90fdb) : u2f(0x3fc90fdb);
            swc1(HERO + 0x180, angle_add(side, yaw));
            turn_towards(HERO + 0x98, 0, lwc1(HERO + 0x180), fpu_mul(frame_time(), two_pi));
            // he falls to the floor (two units over his feet)
            const float fall = fpu_add(lwc1(pv + 0x24), fpu_mul(frame_time2(), u2f(0x411ccccd)));
            swc1(pv + 0x24, fall);
            swc1(HERO + 0x88, fpu_sub(lwc1(HERO + 0x88), fpu_sub(fall, 2.0f)));
            const float floor = floor_probe_v(0.5f, V(HERO_POS), 0);
            const float feet = fpu_sub(lwc1(HERO + 0x88), 2.0f);
            swc1(HERO + 0x88, feet);
            if (feet < floor) {
                swc1(HERO + 0x88, floor);
                sw(pv + 0x24, 0);
            }
            // the pump turned toward the hero (a twelfth of a turn ahead, at most a sixth behind)
            if (lbu(lwu(HERO + 0x2080) + 0x53) != 0x44) {
                const float goal = angle_add(u2f(0x3e860a92), yaw);
                const float sixth = u2f(0x3f860a92);
                turn_towards(m + 0x48, 0, goal, fpu_mul(frame_time(), u2f(0x40490fdb)));
                if (sixth < angle_sub(lwc1(m + 0x48), goal)) swc1(m + 0x48, angle_sub(lwc1(m + 0x48), sixth));
                else if (angle_sub(lwc1(m + 0x48), goal) < -sixth) swc1(m + 0x48, angle_add(lwc1(m + 0x48), sixth));
            }
            // the work: the turn the hero went round, over the turns of the whole work
            swc1(pv + 0x18, fpu_add(lwc1(pv + 0x18), angle_dist(yaw, from)));
            const float work = fpu_div(lwc1(pv + 0x18), fpu_mul(lwc1(pv + 0x30), two_pi));
            swc1(pv, work);
            if (1.0f < work) swc1(pv, 1.0f);
            if (lwc1(pv) < 0.0f) swc1(pv, 0.0f);
            mem_clear128(HERO + 0xe0, 0x90);
            break;
        }
        case 4: {  // (0x2d9030) sinking half a unit, then done
            const float z = fpu_sub(lwc1(m + 0x18), fpu_mul(frame_time(), 0.5f));
            swc1(m + 0x18, z);
            const float floor = fpu_sub(lwc1(pv + 0x3c), 0.5f);
            if (z <= floor) {
                swc1(m + 0x18, floor);
                sb(m + 0x20, 5);
                swc1(pv, 1.0f);
            }
            break;
        }
        case 5:  // (0x2d907c) done
            swc1(pv, 1.0f);
            break;
        default:  // 2 (0x2d908c) and past the table
            break;
    }
    class280_level_flags(m);
    ee_stack_free(0x140);
}

}  // namespace rac1
