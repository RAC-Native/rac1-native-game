// Class 447 (Umbris L7, Gemlik L13; class 920 on Kerwan L3 and Rilgar L5): one port of the four levels' copies, which are
// the same code relinked. Its addresses are the base ELF's (bare: the hero 0x13f350.., his position 0x13f3d0, the time
// step 0x15ed64, the cutscene flag 0x15edb0) or the engine's (OV: the camera 0x166dc0, the moby table 0x15ffd8); the
// three warnings it prints for a missing setting are the level's own strings, which the port's empty debug_printf_stub
// does not take. Header shared_class447.h.
//
// The pvars: two joint springs' records (+0 and +0x80; the head's angles +0x64 / +0x68 / +0xe8, the spring +0x70), the
// point it looks at +0x100, the timer of its look at the hero +0x118, the timer of its next point +0x11c, the zone it waits
// for +0x120 and two mobys by index: the one it waits on +0x124 and the one it sets to state 3 when it leaves +0x128.
#include <game/ovl/shared_class447.h>

#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;   // the hero's position
constexpr uint32_t HERO_HEAD = 0x0013f420;  // the point of the hero the head looks at
constexpr uint32_t HERO_VEL = 0x0013f450;   // the hero's velocity
}  // namespace

// orig: L3:0x002dc478 L5:0x00317aa0 L7:0x002f61c0 L13:0x002edbe8
void class447_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float deg = u2f(0x3c8efa35);
    const float quarter = u2f(0x3fc90fdb);
    scripted_camera_nodes_l1();
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 30.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x18);
    }
    switch (lbu(m + 0x20)) {
        case 0: {  // set up: shown; gone at once when the moby it waits on is already past its start
            sb(m + 0x30, 0xff);
            if (lw(pv + 0x120) == -1) debug_printf_stub();
            if (lw(pv + 0x128) == -1) debug_printf_stub();
            const int32_t wait = lw(pv + 0x124);
            if (wait == -1) {
                debug_printf_stub();
                sb(m + 0x20, 1);
                break;
            }
            const uint32_t mobys = lwu(OV(0x0015ffd8));
            if (lbu(mobys + (static_cast<uint32_t>(wait) << 8) + 0x20) == 0) {
                sb(m + 0x20, 1);
                break;
            }
            const int32_t other = lw(pv + 0x128);
            if (other != -1) sb(mobys + (static_cast<uint32_t>(other) << 8) + 0x20, 3);
            moby_retire(m);
            return;
        }
        case 1: {  // waiting for the hero in its zone: then its talker word, and on
            const int32_t zone = lw(pv + 0x120);
            if (zone != -1 && point_in_box(HERO_POS, zone) != 0) {
                talker_word_set(m, 1);
                sb(m + 0x20, 2);
            }
            break;
        }
        case 2:
            sb(m + 0x20, 3);
            break;
        default:
            break;
    }

    // the head: while idle (animation 0), toward the hero near and in front of it (held two seconds after he stops), or
    // toward a point it picks around itself every three to five seconds
    float k = 0.02f, d = 0.3f;
    if (lbu(m + 0x53) == 0) {
        bool looking = false;
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 8.0f) {
            const float to = atan2_f(fpu_sub(lwc1(HERO_HEAD), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_HEAD + 4), lwc1(m + 0x14)));
            if (angle_dist(lwc1(m + 0x48), to) < quarter) {
                looking = true;
                if (0.01f < vec3_length(V(HERO_VEL))) sw(pv + 0x118, static_cast<uint32_t>(frame_scale_i(120)));
                else timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x118)));
            }
        }
        if (!looking && lw(pv + 0x118) != 0) {
            sw(pv + 0x118, 0);
            lq_sq(pv + 0x100, HERO_HEAD);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x11c))) != 0) {
            sw(pv + 0x11c, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 300.0f)))));
            const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-90.0f, 90.0f), deg));
            const float pitch = fpu_mul(rand_range_f(0.0f, 30.0f), deg);
            vec_from_spherical_v(V(pv + 0x100), 6.0f, yaw, pitch);
            vec3_add(V(pv + 0x100), V(pv + 0x100), V(m + 0x10));
        }
        Vec4 target;
        if (lw(pv + 0x118) != 0) {
            target = V(HERO_HEAD);
            k = u2f(0x3d23d70a);  // 0.04
            d = u2f(0x3e99999a);  // 0.3
        } else {
            target = V(pv + 0x100);
        }
        Vec4 eye = V(m + 0x10);
        eye.z = fpu_add(eye.z, 1.0f);
        Vec4 to;
        vec3_sub(to, target, eye);
        float yaw = angle_sub(atan2_f(to.x, to.y), lwc1(m + 0x48));
        float pitch = -atan2_f(vec2_length(to), to.z);
        if (quarter < yaw) yaw = quarter;
        else if (yaw < -quarter) yaw = -quarter;
        const float sixth = u2f(0x3f060a92);
        if (sixth < pitch) pitch = sixth;
        else if (pitch < -sixth) pitch = -sixth;
        swc1(pv + 0x64, pitch);
        swc1(pv + 0x68, fpu_mul(yaw, u2f(0x3f19999a)));
        swc1(pv + 0xe8, fpu_mul(yaw, u2f(0x3ecccccd)));
    }
    if (lbu(0x0015edb0) != 0) swc1(pv + 0x70, 2.75f);
    joint_node_spring(m, pv, 0, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
    joint_node_spring(m, pv + 0x80, 1, fpu_mul(k, lwc1(0x0015ed64)), fpu_mul(d, lwc1(0x0015ed64)));
}

}  // namespace rac1
