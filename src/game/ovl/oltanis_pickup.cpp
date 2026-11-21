// Oltanis (level 14), class 1354: a pickup in the way of the gold bolt (shared_gold_bolt.cpp), the level's own build. Set
// up (state 0) on the floor under it (the floor probe's point, lifted by the level's height), its place kept in its pvars
// (+0); its slot of the level's checkpoints (0x14c050 + level * 16 + its +0xb0) already 0xff, it goes at once. Spinning
// (1) at its place with its glitter (four sparkle colours phasing, toward the camera); every tenth frame the hero within 1
// of it (2 up or down) takes it: item 0x15 given, scene 6 started, +0x18 of its pvars noting whether the flag 0x13d4d5
// was set (which message it shows after the scene). After the scene (2) the checkpoint is marked taken and the level
// change requested (unless the slot is 0xff), the message shown with its hint, and it goes; during the scene (game mode 2)
// it stays hidden. State 3 fades its +0x2c to 0 (the class's rate) and then goes. Ported by hand from Oltanis's own code
// (L14:0x305758, 0x305a38, 0x305b18), the original's order of calls and float operations kept.
//
// The glitter (0x305a38 / 0x305b18) is a near copy of the gold bolt's (glitter_pvars_init / gold_bolt_glitter): other
// sizes (2.0 / 2.5), colours (0x4040ffff / 0x1040ffff) and particle words (0x35 / 1), the centre its pvars +0 lifted by
// 0.1 while the camera's direction is taken, no fade: a copy of its own, not the shared port.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {
struct Frame {  // an EE stack frame of the original's size (its locals' offsets kept)
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    uint32_t n, sp;
};

// the moby's slot of the level's checkpoints (0xff: none left)
uint32_t checkpoint_slot(uint32_t m) { return lbu(0x0014c050u + (lwu(0x0015ed84u) << 4) + lbu(m + 0xb0)); }

// The glitter's pvars: four colour phases (+0x20, random 0 .. 254), their rates (+0x30), four timers (+0x40, 63 .. 255
// frames apart) and four sizes (+0x50).
// orig: L14:0x00305a38
void glitter_init(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t k = 0; k < 4; ++k) swc1(pv + 0x20 + k * 4, static_cast<float>(rand_mod(0xff)));
    swc1(pv + 0x30, -1.0f);
    swc1(pv + 0x34, -2.25f);
    swc1(pv + 0x38, 1.25f);
    swc1(pv + 0x3c, 2.5f);
    for (uint32_t k = 0; k < 4; ++k) sw(pv + 0x40 + k * 4, static_cast<uint32_t>(frame_scale_i(static_cast<int32_t>(0x3f + 0x40 * k))));
    swc1(pv + 0x5c, 2.0f);
    swc1(pv + 0x58, 2.5f);
    swc1(pv + 0x50, 2.0f);
    swc1(pv + 0x54, 2.5f);
}

// The glitter: four sparkles in a row from a little toward the camera (0.3 from its place, lifted 0.1), 0.1 apart, each
// phasing its colour and frame.
// orig: L14:0x00305b18
void glitter(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xb0);
    const uint32_t at = f.sp, step = f.sp + 0x10;
    swc1(pv + 8, fpu_add(lwc1(pv + 8), 0.1f));
    vec3_sub(V(at), V(L14_ADDR(0x001674c0)), V(pv));  // (the camera's position)
    vec3_set_length(V(at), V(at), -0.3f);
    vec3_set_length(V(step), V(at), 0.1f);
    vec3_add(V(at), V(at), V(pv));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), 0.1f));
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t phase = pv + 0x20 + k * 4, timer = pv + 0x40 + k * 4;
        const float p = fpu_add(lwc1(phase), lwc1(pv + 0x30 + k * 4));
        swc1(phase, p);
        if (255.0f <= p) swc1(phase, fpu_sub(p, 255.0f));
        else if (p <= 0.0f) swc1(phase, fpu_add(p, 255.0f));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer))) != 0) sw(timer, static_cast<uint32_t>(frame_scale_i(0xff)));
        const float done = fpu_div(int_to_float(frame_scale_i(0xff) - lw(timer)), static_cast<float>(frame_scale_i(0xff)));
        const uint32_t colour = rgba_lerp(0x4040ffffu, 0x1040ffffu, abs_f(fpu_sub(0.5f, done)));
        const uint32_t frame = static_cast<uint32_t>(float_to_int(lwc1(phase))) & 0xff;
        spawn_particle_3b(at, colour, frame, 0x35, 1, 2, 0, lwc1(pv + 0x50 + k * 4));
        vec3_add(V(at), V(at), V(step));
    }
}
}  // namespace

// orig: L14:0x00305758
void olt_class1354_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x70);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x003057bc) on the floor under it, its place kept; its checkpoint slot already 0xff: gone
        if (checkpoint_slot(m) == 0xff) {
            moby_retire(m);
            return;
        }
        glitter_init(m);
        floor_probe_v(0.5f, V(m + 0x10), 0);  // (the floor's point is read from the hit record)
        sw(m + 0x40, 0);
        swc1(m + 0x44, u2f(0xbfc90fdbu));  // (-pi / 2)
        swc1(m + 0x18, fpu_add(coll_hit_point().z, lwc1(L14_ADDR(0x001621e8))));
        lq_sq(pv, m + 0x10);
        sb(m + 0x20, 1);
        return;
    case 1: {  // (0x00305840) spinning, its glitter; every tenth frame the hero near enough takes it
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(L14_ADDR(0x001621ec))));
        lq_sq(m + 0x10, pv);
        vec3_scale(V(f.sp), V(m + 0xd0), lwc1(L14_ADDR(0x001621f0)));
        vec3_add(V(m + 0x10), V(m + 0x10), V(f.sp));
        glitter(m);
        if (ee_rem(lw(L14_ADDR(0x0015f5cc)), 10) != 0) return;
        if (!(vec2_distance(V(m + 0x10), V(0x0013f3d0u)) < 1.0f)) return;
        if (!(abs_f(fpu_sub(lwc1(m + 0x18), lwc1(0x0013f3d8u))) < 2.0f)) return;
        sw(pv + 0x18, lbu(0x0013d4d5u) != 0 ? 1u : 0u);  // (the state, 1, when the flag is set)
        item_give(0x15, 1);
        cutscene_start(6);
        sb(m + 0x20, 2);
        return;
    }
    case 2:  // (0x00305934) after the scene: the checkpoint taken, the level change asked for, the message; hidden during it
        if (lw(L14_ADDR(0x0015f5c4)) == 2) {
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            return;
        }
        if (checkpoint_slot(m) != 0xff) {
            checkpoint_mark_collected(lbu(m + 0xb0));
            level_change_request(0, -1);
        }
        sub_00263d40(lw(pv + 0x18) != 0 ? TXT(0x53e0) : TXT(0x36b2), -1);
        sub_00203908(0x36b0, 0x7a);  // (a hint: its id, not a text id)
        moby_retire(m);
        return;
    case 3: {  // (0x003059d4) fading out at the class's rate, then gone
        approach_f(m + 0x2c, 0.0f, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 0.02f));
        if (lwc1(m + 0x2c) == 0.0f) moby_retire(m);
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
