// Rilgar's (level 5) people who talk: the man who flies the hero to the next planet (class 919) and the one who hands
// over the item 0x17 (class 925). Both turn their head toward the hero when he stands in front of them, else look
// around now and then. Ported by hand from Rilgar's own code (L5:0x317470, 0x3180a0), the original's order of calls
// and float operations kept.
#include <game/level/level_data.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
constexpr uint32_t kHeroPos = 0x0013f3d0;   // the hero's position
constexpr uint32_t kHeroHead = 0x0013f420;  // the point the people look at when they look at him
constexpr uint32_t kHeroVel = 0x0013f450;   // his velocity
uint32_t frames_between(float lo, float hi) {
    return static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(lo, hi))));
}

// The head of a talker: hold (+hold, a timer) is set while the hero stands still in front of him within 8 (two
// seconds after he last moved: it keeps the look on him); when it runs out the look target (+target) is where his head
// was. Every 3 to 5 seconds (+look) another point 6 in front, up to 90 degrees aside and 30 up, is taken. In a
// talk (the animation +0x53 not 0) the head stays. The head's yaw and pitch (+0xa4 the pitch, the yaw split into
// +0xa8 and +0x128) then spring the two joints (+0x40, +0xc0) at the frame's rates.
// (0x00317620 and 0x003182a8, the same code in the two)
void head_look(uint32_t m, uint32_t pv, uint32_t hold, uint32_t look, uint32_t target, float yaw_a8, float yaw_128) {
    float k = u2f(0x3ca3d70au), d = u2f(0x3e99999au);  // (0.02, 0.3)
    const uint32_t sp = ee_stack_alloc(0x30);
    const bool talking = lbu(m + 0x53) != 0;
    if (!talking) {
        bool in_front = false;
        if (vec2_distance(V(m + 0x10), V(kHeroPos)) < 8.0f) {
            const float a = atan2_f(fpu_sub(lwc1(kHeroHead), lwc1(m + 0x10)), fpu_sub(lwc1(kHeroHead + 4), lwc1(m + 0x14)));
            in_front = angle_dist(lwc1(m + 0x48), a) < u2f(0x3fc90fdbu);
        }
        if (in_front) {
            if (u2f(0x3c23d70au) < vec3_length(V(kHeroVel))) sw(pv + hold, static_cast<uint32_t>(frame_scale_i(0x78)));
            else timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + hold)));
        } else if (lw(pv + hold) != 0) {
            sw(pv + hold, 0);
            lq_sq(pv + target, kHeroHead);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + look))) != 0) {
            const float deg = u2f(0x3c8efa35u);
            sw(pv + look, frames_between(180.0f, 300.0f));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float up = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(pv + target), 6.0f, yaw, up);
            vec3_add(V(pv + target), V(pv + target), V(m + 0x10));
        }
        if (lw(pv + hold) != 0) {
            lq_sq(sp, kHeroHead);
            k = u2f(0x3d23d70au);  // (0.04)
            d = u2f(0x3e99999au);
        } else {
            lq_sq(sp, pv + target);
        }
        // the yaw and pitch to the target from a unit above his feet, kept within 90 and 30 degrees
        lq_sq(sp + 0x10, m + 0x10);
        swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), 1.0f));
        vec3_sub(V(sp + 0x20), V(sp), V(sp + 0x10));
        float yaw = angle_sub(atan2_f(lwc1(sp + 0x20), lwc1(sp + 0x24)), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(V(sp + 0x20)), lwc1(sp + 0x28));
        const float quarter = u2f(0x3fc90fdbu), sixth = u2f(0x3f060a92u);
        if (quarter < yaw) yaw = quarter;
        else if (yaw < -quarter) yaw = -quarter;
        if (sixth < pitch) pitch = sixth;
        else if (pitch < -sixth) pitch = -sixth;
        swc1(pv + 0xa4, pitch);
        swc1(pv + 0xa8, fpu_mul(yaw, yaw_a8));
        swc1(pv + 0x128, fpu_mul(yaw, yaw_128));
    }
    if (lbu(0x0015edb0) != 0) swc1(pv + 0xb0, 2.75f);
    float t = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0x40, 0, fpu_mul(k, t), fpu_mul(d, t));
    t = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0xc0, 1, fpu_mul(k, t), fpu_mul(d, t));
    ee_stack_free(0x30);
}
}  // namespace

// Class 919, the pilot: gone once the flag 0x13dd47 is set; set up with its talk (the record 0x161ef0, its reach 3.7);
// in its talk the hero is walked up to him (2.5 in front); the talk's end 5 takes the hero to planet 7 (visited, the
// level changed) and the pilot goes. Out of a talk he changes his idle animation (1 or 2) every 20 to 40 seconds and
// goes back to the first when the talk's animation ends (+0x70 bit 2).
// orig: L5:0x00317470
void ril_class919_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_level(m, 0);
    if (pv == 0) return;
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {  // (0x003174cc)
        if (lbu(0x0013dd47) != 0) {
            moby_retire(m);
            return;
        }
        sw(pv + 0x20, L5_ADDR(0x00161ef0u));
        sb(m + 0x20, 1);
        talk_init(m, pv);
        swc1(pv + 0xc, u2f(0x406ccccdu));  // (3.7)
        band_set(m);
    } else if (state == 1) {  // (0x00317518)
        if (talk_update(m, pv) != 0) hero_walk_to_l1(2.5f, m);  // (L5:0x28d960, the shared port: batch 501)
        if (lh(pv + 4) == 5) {
            level_mark_visited(7);
            level_change_request(0, -1);
            moby_retire(m);
            return;
        }
        bool back = lbu(m + 0x53) != 0;
        if (!back) {
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x144))) == 0) {
                back = true;
            } else {
                sw(pv + 0x144, frames_between(1200.0f, 2400.0f));
                const uint32_t set = rand_mod(2) != 0 ? state : 2u;
                if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(0xa));
            }
        }
        if (back && (lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
    }
    head_look(m, pv, 0x148, 0x14c, 0x150, 0.5f, 0.5f);
}

// Class 925, the man with the item 0x17: lights by the floor near the point 0x1671c0 (within 40, while shown); gone
// once the flag 0x13d4ff is set; set up on the floor below him with his talk (the record 0x161ef8); in his talk the
// hero is walked up to him (2 in front); the talk's end 3 or 4 gives the item (equipped), shows the message 0x1396 and
// changes the level, and he goes. Out of a talk his idle animation 1 is set again every 20 to 40 seconds and the
// first when the talk's animation ends.
// orig: L5:0x003180a0
void ril_class925_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class_mates_mark_fixed(m, 0);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L5_ADDR(0x001671c0u))) < 40.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x20);
    }
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {  // (0x00318138)
        if (lbu(0x0013d4ff) != 0) {
            moby_retire(m);
            return;
        }
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.5f));
        swc1(m + 0x18, floor_probe(0.5f, m + 0x10, 0));
        sw(pv + 0x20, L5_ADDR(0x00161ef8u));
        sb(m + 0x20, 1);
        talk_init(m, pv);
    } else if (state == 1) {  // (0x00318190)
        if (talk_update(m, pv) != 0) hero_walk_to_l1(2.0f, m);  // (L5:0x28d960, the shared port: batch 501)
        if (static_cast<uint32_t>(lhu(pv + 4) - 3u) < 2u) {
            item_give(0x17, 1);
            sub_00263d40(TXT(0x1396), -1);
            level_change_request(0, -1);
            moby_retire(m);
            return;
        }
        if (lbu(m + 0x53) == 0 && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x154))) != 0) {
            sw(pv + 0x154, frames_between(1200.0f, 2400.0f));
            if (lbu(m + 0x53) != state) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        } else if (lbu(m + 0x53) == 1 && (lbu(m + 0x70) & 2u) != 0) {
            moby_anim_set(m, 0, 0, frame_scale_i(0xa));
        }
    }
    head_look(m, pv, 0x158, 0x15c, 0x140, u2f(0x3f19999au), u2f(0x3ecccccdu));
}

}  // namespace rac1
