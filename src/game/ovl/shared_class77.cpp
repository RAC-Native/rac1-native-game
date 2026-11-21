// Class 77, a mine, and its group's glow: one port of the copies Quartu and Drek's fleet carry (the same code relinked).
// Each level keeps its own words (the tuning block, a switch word, the glow's corners and texture coordinates: per level
// below); the rest is the engine's (OV: the frame counter 0x15f5cc, the moby list 0x15ffd8, the groups 0x1ab840, the
// camera 0x166c80) or the base ELF's (bare: the hero 0x13f350, the frame time 0x15ed6c). Header shared_class77.h.
//
// The pvars: +0x64 the angle off the hero's bearing it creeps along, +0x68 the timer, +0x6c a word holding the frame its
// group's glow was last queued (none: every frame).
#include <game/ovl/shared_class77.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// the level's tuning block: +0 the creeping speed, +4 / +8 / +0xc the sparks' colour, size and frames, +0x10 the glow's
// and the sparks' height, +0x14 the glow's four colours
uint32_t tuning() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x00161454) : L15_ADDR(0x001614b4); }
// the level's switch: the mines wake (non-zero) or stay resting
uint32_t wake_word() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x0016198c) : L15_ADDR(0x00161ab8); }
// the glow's four quads (four corners each) and their corners' (s, t)
uint32_t glow_corners() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001ce390) : L15_ADDR(0x001ce110); }
uint32_t glow_uvs() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001ce490) : L15_ADDR(0x001ce210); }
// the level's copy of the glow's draw (queued by the update: the levels still register their copies)
uint32_t draw_callback() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x002a8088) : L15_ADDR(0x002a29b8); }

// the blast: the explosion, the push for what is within 1, then resting again (not drawn)
void rest(uint32_t m) {
    sb(m + 0x20, 1);
    sb(m + 0x31, 0);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    sw(m + 0x94, 0);
}
}  // namespace

// Its states: 0 starts resting (1); out of state 1 each frame it sparks (and its group's glow is queued once a frame) while
// the level's switch is on, else it rests; 2 rises until its timer ends (the waking animation), 3 creeps to the hero (its
// bearing turned by pvar +0x64, climbing to stay 0.4 .. 0.6 above the hero's ground) until it is within 0.5 of his point +0xd0 or its
// timer ends, 4 blows up and rests. A hit of the kinds 0x330000 blows it up.
// orig: L15:0x002a2488 L17:0x002a7b58
void class77_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t tune = tuning();
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t step = sp, at = sp + 0x10;
    if (sub_0025a420(m, NOT_ADDR(0x330000), 0) != 0) {
        sb(m + 0x20, 4);
        sb(m + 0xa4, 0xff);
    }
    if (lbu(m + 0x20) != 1) {
        rand_sphere_point(step, 0.0f, fpu_add(lwc1(0x0015ed6c), lwc1(0x0015ed6c)));
        lq_sq(at, m + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), lwc1(tune + 0x10)));
        if (lw(wake_word()) == 0) {
            sb(m + 0x20, 1);
            sw(m + 0x94, 0);
            ee_stack_free(0x80);
            return;
        }
        const uint32_t count = static_cast<uint32_t>(float_to_int(static_cast<float>(frame_scale_i(lw(tune + 0xc))))) & 0xffu;
        const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
        spawn_particle_3c(at, step, lwu(tune + 4), count, frame, 0, lwc1(tune + 8));
        const uint32_t last = lwu(pv + 0x6c);
        bool queue = true;
        if (last != 0) {
            if (lwu(last) == lwu(OV(0x0015f5cc))) queue = false;
            else sw(last, lwu(OV(0x0015f5cc)));
        }
        if (queue) sub_001f93e0(draw_callback(), m);
    }
    switch (lbu(m + 0x20)) {
    case 0:
        rest(m);
        break;
    case 2:
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_add(lwc1(0x0015ed6c), lwc1(0x0015ed6c))));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x68))) == 0) break;
        sb(m + 0x20, 3);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(10));
        sw(pv + 0x68, static_cast<uint32_t>(frame_scale_i(0xf0)));
        break;
    case 3: {
        const float bearing = atan2_f(fpu_sub(lwc1(0x0013f3d0), lwc1(m + 0x10)), fpu_sub(lwc1(0x0013f3d4), lwc1(m + 0x14)));
        const float way = angle_add(lwc1(pv + 0x64), bearing);
        const float ground = lwc1(0x0013f418);
        swc1(step, fpu_mul(cos_vu(way), fpu_mul(lwc1(tune), lwc1(0x0015ed6c))));
        const float dt = lwc1(0x0015ed6c);
        swc1(step + 4, fpu_mul(sin_vu(way), fpu_mul(lwc1(tune), dt)));
        sw(step + 8, 0);
        const float above = fpu_sub(lwc1(m + 0x18), ground);
        if (u2f(0x3f19999au) < above) swc1(step + 8, -fpu_mul(dt, 3.0f));  // (0.6)
        else if (above < u2f(0x3ecccccdu)) swc1(step + 8, fpu_mul(dt, 3.0f));  // (0.4)
        walker_move(m, step, 0, 0.25f, 0.25f, 0.0f);
        if (vec2_distance(V(0x0013f420), V(m + 0x10)) < 0.5f || timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x68))) != 0) {
            sb(m + 0x20, 4);
        } else if (lbu(m + 0x53) == 1 && (lbu(m + 0x70) & 2) != 0) {
            moby_anim_set(m, 2, 0, frame_scale_i(10));
        }
        break;
    }
    case 4:
        explosion_burst(m, m + 0x10, -1, 0.25f, 13.0f);
        moby_sphere_contact(m, m + 0x10, 0x10001, 0, 1, 0, 1.0f, 1.0f, 1.0f);
        rest(m);
        break;
    default:  // (1 and past 4: nothing)
        break;
    }
    ee_stack_free(0x80);
}

// For each moby of the drawing moby's group (+0x21) not resting (state 1): four quads in the level's colours, turned to
// face the camera (its yaw and pitch from the moby), at the moby raised by the level's height; the effect texture 8.
// orig: L15:0x002a29b8 L17:0x002a8088
void class77_draw(uint32_t m) {
    const uint32_t tune = tuning();
    const uint32_t sp = ee_stack_alloc(0x190);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, angles = sp + 0x90, frame = sp + 0xa0;
    const uint64_t tex0 = effect_tex0(8);
    sd(regs + 8, tex0);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs + 0x18, 0x8000000044ull);
    sd(regs, 0);
    const uint32_t uv = glow_uvs();
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(uvs + k * 8, lwc1(uv + k * 8));
        swc1(uvs + k * 8 + 4, lwc1(uv + k * 8 + 4));
    }
    const uint32_t cam = OV(0x00166c80);
    uint32_t e = lwu(OV(0x001ab840) + (lbu(m + 0x21) << 2));
    for (;; e += 2) {  // (the group's moby indices, the last with bit 15 set)
        const uint32_t member = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (lbu(member + 0x20) != 1) {
            vec_clear(V(angles));
            swc1(angles + 8, atan2_f(fpu_sub(lwc1(cam + 0x140), lwc1(member + 0x10)), fpu_sub(lwc1(cam + 0x144), lwc1(member + 0x14))));
            const float flat = vec2_distance(V(member + 0x10), V(cam + 0x140));
            swc1(angles + 4, -atan2_f(flat, fpu_sub(lwc1(cam + 0x148), lwc1(member + 0x18))));
            euler_to_mat4_vu(M(frame), V(angles));
            lq_sq(frame + 0x30, member + 0x10);
            const float z = fpu_add(lwc1(frame + 0x38), lwc1(tune + 0x10));
            swc1(frame + 0x3c, 1.0f);
            swc1(frame + 0x38, z);
            const uint32_t tmpl = glow_corners();
            for (uint32_t q = 0; q < 4; ++q) {
                const uint32_t colour = lwu(tune + 0x14 + q * 4);
                for (uint32_t k = 0; k < 4; ++k) {
                    sw(colours + k * 4, colour);
                    lq_sq(quad + k * 0x10, tmpl + q * 0x40 + k * 0x10);
                }
                quad_draw(quad, frame);
            }
        }
        if (lh(e) < 0) break;
    }
    ee_stack_free(0x190);
}

}  // namespace rac1
