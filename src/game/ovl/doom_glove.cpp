// Ported overlay code: the Glove of Doom (item 0x14, hand moby class 0xe5). Its hand moby's update
// 0x2c7a58 keeps an "agent" (class 0xe6, spawned by 0x2c8218) in the glove, aims every frame (0x2c7548:
// the hero's facing, or with the strafe aim along the camera, the nearest hit as the target) and, on the
// throw (state 3 of the glove), sets the agent flying (0x2c82f0) on the arc 0x2c7320 computes. The
// glove's pvars: +0x40 the agent's place in the hand, +0x50 the agent held, +0x54 the throw timer.
#include <game/enhance/advanced.h>
#include <game/enhance/fixes102.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
constexpr uint32_t PAD = 0x0013c940;
}  // namespace

// The velocity written to `out` (EE address) of an agent thrown from `from` toward `target` (cut to
// `range`): as the bomb's arc (0x2c2488) with the speed 8.5 * 0x15ed6c, its rise from 0x25abf0 under
// gravity `g`, capped by g * (2.5, or 3.5 with the strafe aim) / speed / 2.
// orig: 0x002c7320
void doom_throw_velocity(uint32_t from, uint32_t target, uint32_t out, float g, float lo, float range) {
    (void)lo;
    const uint32_t frame = ee_stack_alloc(0x40);
    const uint32_t sp = frame;
    if (range < vec3_distance(V(from), V(target))) {
        vec3_sub(V(sp), V(target), V(from));
        vec3_set_length(V(sp), V(sp), range - u2f(0x3c23d70au));
        vec3_add(V(sp), V(sp), V(from));
    } else {
        V(sp) = V(target);
    }
    float yaw = atan2_f(lwc1(sp) - lwc1(from), lwc1(sp + 4) - lwc1(from + 4));
    float f1 = lwc1(sp + 4);
    (void)f1;
    bool facing = u2f(0x3f5f66f3u) < angle_dist(lwc1(P + 0x98), yaw);
    if (!facing) facing = vec2_distance(V(from), V(sp)) < u2f(0x3fc00000u);
    if (facing) {
        yaw = lwc1(P + 0x98);
        Vec4 d{};
        d.x = cos_vu(yaw) * u2f(0x3fc00000u);
        d.y = sin_vu(lwc1(P + 0x98)) * u2f(0x3fc00000u);
        d.z = 0.0f;
        vec3_add(d, d, V(from));
        swc1(sp, d.x);
        swc1(sp + 4, d.y);
    }
    const float speed = lwc1(0x0015ed6c) * u2f(0x41080000u);
    swc1(out, cos_vu(yaw) * speed);
    swc1(out + 4, sin_vu(yaw) * speed);
    sw(out + 8, 0);
    swc1(out + 8, sub_0025abf0(from, sp, nullptr, speed, -g));
    float k = advanced_camera_aim(lwu(PAD + 0x1a0)) ? fpu_div(u2f(0x40600000u), speed) : fpu_div(u2f(0x40200000u), speed);
    const float cap = (g * k) * 0.5f;
    if (cap < lwc1(out + 8)) swc1(out + 8, cap);
    ee_stack_free(0x40);
}

// An agent (class 0xe6) for the glove `hand` at `pos`, its pvars +0 the vector `vel`, +0x30 the glove;
// scale 0.01 of its class's +0x24; flagged 0x41 when the hero's 0x20a5 / 0x20af is set. 0 when none.
// orig: 0x002c8218
uint32_t doom_agent_spawn(uint32_t hand, uint32_t pos, uint32_t vel) {
    const uint32_t a = moby_spawn(0xe6);
    if (a == 0) return 0;
    const uint32_t pv = lwu(a + 0x78);
    sh(a + 0x32, 0xff);
    sb(a + 0x30, 0xff);
    sb(a + 0x31, 1);
    sb(a + 0x20, 0);
    V(a + 0x10) = V(pos);
    V(pv) = V(vel);
    sw(pv + 0x30, hand);
    sh(pv + 0x38, 0);
    sw(pv + 0x34, 0);
    swc1(a + 0x2c, lwc1(lwu(a + 0x24) + 0x24) * u2f(0x3c23d70au));
    if (lbu(P + 0x20a5) != 0 || lbu(P + 0x20af) != 0) sh(a + 0x34, lhu(a + 0x34) | 0x41u);
    // (1.02 Fixes, fix 3: the 1.02 build, 0x2c9578, keeps the last 20 agents in a ring, 1.02's 0x1dd400 / index
    // 0x16190c, here FIX_RING: the agent 20 spawns ago, when still an agent and not dying, 0xfd / 0xfe, goes to state 4)
    if (fixes102_on(3)) {
        const int32_t next = lw(FIX_RING_NEXT);
        const uint32_t old = lwu(FIX_RING + static_cast<uint32_t>(next) * 4);
        if (old != 0 && lh(old + 0xa6) == 0xe6 && lbu(old + 0x20) != 0xfe && lbu(old + 0x20) != 0xfd) sb(old + 0x20, 4);
        sw(FIX_RING + static_cast<uint32_t>(next) * 4, a);
        sw(FIX_RING_NEXT, static_cast<uint32_t>(ee_rem(next + 1, 0x14)));
    }
    return a;
}

// The agent's launch (0x2c82f0): its velocity (pvars +0) plus the hero's carried velocity when riding,
// its place the throw's start (a wall between the hero and it puts it at the hit, pushed out by the
// normal 0.1 less than 9 gravity units, the velocity reflected and damped to 0.6), state 2.
// orig: 0x002c82f0
void doom_agent_launch(uint32_t from, uint32_t agent, uint32_t vel) {
    const uint32_t pv = lwu(agent + 0x78);
    V(pv) = V(vel);
    if (lwu(P + 0x2fc) != 0 && moby_pvar_word2(lwu(P + 0x2fc)) != 0) vec3_add(V(pv), V(pv), V(P + 0x100));
    V(agent + 0x10) = V(from);
    const uint32_t frame = ee_stack_alloc(0x40);
    const uint32_t sp = frame;
    V(sp) = V(lwu(P + 0x2080) + 0x10);
    swc1(sp + 8, lwc1(from + 8));
    if (segment_collide(sp, from, 0, lwu(P + 0x2080), 0) != 0) {
        V(agent + 0x10) = V(OV(0x00173e60));
        const float g = lwc1(0x0015ed70);
        const float len = g * u2f(0x41100000u) - g * u2f(0x3dcccccdu);
        vec3_set_length(V(sp + 0x10), V(OV(0x00173e80)), len);
        vec3_add(V(agent + 0x10), V(agent + 0x10), V(sp + 0x10));
        vec3_reflect(V(pv), V(pv), V(OV(0x00173e80)));
        vec3_scale(V(pv), V(pv), u2f(0x3f19999au));
    }
    ee_stack_free(0x40);
    sb(agent + 0x20, 2);
}

// The glove's aim, every frame (0x2c7548): the hold point (the hero's facing, rotated -0.2 and 0.89 away,
// 0.39 up) and, while an agent is held, the target: the facing times 2.5 (3.5 with the strafe aim: R1 /
// L1 held), with the strafe aim the camera's frame (pitch clamped) on the camera's line, the hit of it
// (not the hero) the target, else 5 times the line; the arc 0x2c7320 into the agent's pvars. Not
// aiming resets 0x13fda0.
// orig: 0x002c7548
void doom_aim(uint32_t hand, uint32_t pv, uint32_t from_pos) {
    const uint32_t frame = ee_stack_alloc(0x1a0);
    const uint32_t sp = frame;
    const uint32_t cam = OV(0x00166c80);
    V(sp) = V(from_pos);
    mat3_copy(M(sp + 0x10), M(cam));
    mat3_transpose(M(sp + 0x40), M(sp + 0x10));
    const uint32_t agent = lwu(pv + 0x50);
    if (agent == 0) {
        ee_stack_free(0x1a0);
        return;
    }
    const uint32_t apv = lwu(agent + 0x78);
    V(sp + 0x80) = Vec4{};
    swc1(sp + 0x80, 1.0f);
    mat3_transform(V(sp + 0x80), V(sp + 0x80), M(P + 0x640));
    const float lean = u2f(0xbeb953dfu);
    swc1(sp, cos_vu(angle_add(lean, atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * u2f(0x3f5be9fbu));
    swc1(sp + 4, sin_vu(angle_add(lean, atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * u2f(0x3f5be9fbu));
    sw(sp + 8, 0);
    vec3_add(V(sp), V(sp), V(P + 0x80));
    swc1(sp + 8, lwc1(sp + 8) + u2f(0x3ec8198fu));
    (void)atan2_f(lwc1(sp + 0x80), lwc1(sp + 0x84));
    const bool strafe = advanced_camera_aim(lwu(PAD + 0x1a0));
    vec3_scale(V(sp + 0x90), V(sp + 0x80), strafe ? u2f(0x40600000u) : u2f(0x40200000u));
    vec3_add(V(sp + 0x90), V(sp + 0x90), V(sp));
    float f22 = 0.0f, f21 = 0.0f, f23 = 0.0f;
    bool aimed = false;
    if (strafe && lbu(P + 0x20ac) == 0) {
        V(sp + 0xa0) = Vec4{};
        swc1(sp + 0xa8, 1.0f);
        mat4_transform(V(sp + 0xa0), V(sp + 0xa0), M(sp + 0x40));
        const uint32_t hero = lwu(P + 0x2080);
        (void)angle_sub(lwc1(hero + 0x48), atan2_f(lwc1(sp + 0xa0), lwc1(sp + 0xa4)));
        f22 = atan2_f(lwc1(sp + 0xa0), lwc1(sp + 0xa4));
        float pitch = atan2_f(vec2_length(V(sp + 0xa0)), lwc1(sp + 0xa8));
        if (u2f(0x3f17e9d8u) < pitch) pitch = u2f(0x3f17e9d8u);
        if (pitch < u2f(0xbfb2b8c2u)) pitch = u2f(0xbfb2b8c2u);
        const float rr = advanced_camera_aim(lwu(PAD + 0x1a0)) ? u2f(0x40600000u) : u2f(0x40200000u);
        vec_from_spherical_v(V(sp + 0xa0), rr, f22, pitch);
        V(sp + 0xb0) = V(OV(0x00166dc0));
        const float len = vec2_length(V(sp + 0xa0));
        const float slope = len == 0.0f ? fpu_div(lwc1(sp + 0xa8), u2f(0x3c23d70au)) : fpu_div(lwc1(sp + 0xa8), len);
        const float g70 = lwc1(0x0015ed70);
        const float f2 = g70 * u2f(0x41100000u);
        const float R = advanced_camera_aim(lwu(PAD + 0x1a0)) ? u2f(0x40600000u) : u2f(0x40200000u);
        const float s = sqrt_f((f2 * R) * 0.5f);
        f21 = fpu_div(((s + s) * s) * (1.0f - slope), g70 * u2f(0x41100000u));
        f23 = slope * f21;
        if (0.0f < f21) {
            swc1(sp + 0xa0, cos_vu(f22) * f21);
            swc1(sp + 0xa4, sin_vu(f22) * f21);
            swc1(sp + 0xa8, f23);
            vec3_add(V(sp + 0xc0), V(OV(0x00166dc0)), V(sp + 0xa0));
            if (segment_collide_v(V(OV(0x00166dc0)), V(sp + 0xc0), 2, hand, nullptr) != 0 && lwu(OV(0x00173e40) + 0x18) != lwu(P + 0x2080)) {
                V(sp + 0x90) = V(OV(0x00173e60));
                aimed = true;
                f22 = atan2_f(lwc1(sp + 0x90) - lwc1(sp), lwc1(sp + 0x94) - lwc1(sp + 4));
            }
            if (!aimed) {
                swc1(sp + 0x90, (cos_vu(f22) * f21) * u2f(0x40a00000u));
                swc1(sp + 0x94, (sin_vu(f22) * f21) * u2f(0x40a00000u));
                swc1(sp + 0x98, f23 * u2f(0x40a00000u));
                vec3_add(V(sp + 0x90), V(sp + 0x90), V(OV(0x00166dc0)));
            }
        }
    } else {
        sw(0x0013fda0, 0);
    }
    const float g = lwc1(0x0015ed70) * u2f(0x41100000u);
    const float range = strafe ? u2f(0x40600000u) : u2f(0x40200000u);
    doom_throw_velocity(sp, sp + 0x90, apv, g, u2f(0x3727c5acu), range);
    ee_stack_free(0x1a0);
}

// The glove's frame (see the file's head): the agent held sits at the hand's chain point 0 plus
// (0.01, -0.139, -0.05) in the hand's frame; the throw starts on the hero's throw frame (state 0x23
// at 16 frames, or a quick 0x1e with the button 0x13cae4); the aim runs every frame; then the state
// (jump table 0x1e9d00): 0 / 1 wait for the hand to be drawn, 3 launches the agent (stats 0x141720,
// the glove's list), 4 waits for the throw to end, 5 / 6 end.
// orig: 0x002c7a58
void doom_glove_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t frame = ee_stack_alloc(0xe0);
    const uint32_t sp = frame;
    Vec4 hand_off{};
    hand_off.x = u2f(0x3c23d70au);
    hand_off.y = u2f(0xbe0f5c29u);
    hand_off.z = u2f(0xbd4ccccdu);
    V(sp + 0x10) = hand_off;
    moby_chain_point(m, 0, sp);
    moby_chain_world_one(m, 0, sp + 0x30);
    mat4_transform(V(sp + 0x20), V(sp + 0x10), M(sp + 0x30));
    vec3_add(V(pv + 0x40), V(sp), V(sp + 0x20));
    if (lwu(pv + 0x50) != 0) {
        sw(lwu(pv + 0x50) + 0x98, 1);
        const uint32_t b = lwu(pv + 0x50);
        if (b == 0 || lbu(b + 0x20) == 0xfe || lbu(b + 0x20) == 0xfd) sw(pv + 0x50, 0);
    } else {
        sw(pv + 0x50, 0);
    }
    if (lw(P + 0x2084) == 1) hero_set_state(0x1e, 1);
    bool thrown = false;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x54))) != 0 && lbu(P + 0x20ac) == 0) {
        if (lbu(P + 0x20a8) != 0 && lw(P + 0x1bc) == frame_scale_i(0x11)) thrown = true;
        if (!thrown && (lwu(0x0013cae4) & lwu(P + 0x10a0)) != 0 && lw(P + 0x2084) == 0x1e && sub_00233e98(-1) != 0) thrown = true;
        if (!thrown && lw(P + 0x2084) == 0x23 && lw(P + 0x198) == frame_scale_i(0x10)) thrown = true;
        if (thrown) {
            hero_sound_start(0x1a, 0);
            sub_00233db8(-1, 1);
            sb(m + 0x20, 3);
        }
    }
    V(sp + 0x70) = V(pv + 0x40);
    doom_aim(m, pv, sp + 0x70);
    const uint32_t st = lbu(m + 0x20);
    if (st <= 6) {
        switch (st) {
        case 0:
            sw(pv + 0x50, 0);
            sb(m + 0x20, (lbu(m + 0x70) & 2) != 0 ? 2u : 1u);
            [[fallthrough]];
        case 1:
            if ((lbu(m + 0x70) & 2) != 0) sb(m + 0x20, 2);
            break;
        case 3:
            if (lwu(pv + 0x50) != 0) {
                const uint32_t c = lhu(0x00141680 + 0xa0);
                if (!(0xfffe < c)) sh(0x00141680 + 0xa0, c + 1);
                const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
                if (static_cast<int32_t>(lhu(0x00141680 + 0xa2)) < q) sh(0x00141680 + 0xa2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
                sw(0x00141680 + 0xa4, lwu(0x00141680 + 0xa4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
                doom_agent_launch(pv + 0x40, lwu(pv + 0x50), lwu(lwu(pv + 0x50) + 0x78));
                sw(pv + 0x50, 0);
            } else {
                (void)moby_sound_start(0, 0, m);
            }
            sb(m + 0x20, 4);
            sw(pv + 0x54, static_cast<uint32_t>(frame_scale_i(0x14)));
            break;
        case 4:
            if (lw(P + 0x2084) != 0x23 && lbu(P + 0x20a8) == 0) sb(m + 0x20, 2);
            break;
        case 5:
            sb(m + 0x20, 6);
            break;
        case 6:
            ee_stack_free(0xe0);
            return;
        default:
            break;
        }
    }
    if (lwu(pv + 0x50) != 0) {
        V(lwu(pv + 0x50) + 0x10) = V(pv + 0x40);
    } else if (sub_00233e98(-1) != 0) {
        V(sp + 0x70) = Vec4{};
        sw(pv + 0x50, doom_agent_spawn(m, pv + 0x40, sp + 0x70));
    }
    ee_stack_free(0xe0);
}

// orig: (none) — the glove's update for the hand moby's indirect call.
void doom_glove_fns_register() {
    ovl_register_moby_fn(0x002c7a58, doom_glove_update);
}

}  // namespace rac1
