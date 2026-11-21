// Eudora's class 1120: the Suck Cannon (item 9) where the hero finds it. It turns (a quarter turn a frame-time unit) a
// unit above its placement with four sparkles about it, hidden while a scene plays (the game mode L4:0x15f5c4 2). Gone
// when the hero has the item already (0x13d4e8 + 9). Within 3 of the hero it starts scene 2; once the scene is over it
// shows its help message (0x13d4c0 + 9 set: one text, else another), gives the item (equipped), saves the checkpoint at
// the moby whose index its pvars +4 hold, asks for the level's save (the level change request 0, -1) and is gone the
// frame after. Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`); Eudora-only
// data is L4_ADDR, the base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/helmet_tr.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 its position)
constexpr int32_t SUCK_CANNON = 9;     // (the item)
const float kHalfPi = u2f(0x3fc90fdbu);
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
bool scene_playing() { return lw(L4_ADDR(0x0015f5c4)) == 2; }

// the level's moby of index `idx` (the moby table L4:0x15ffd8, 0x100 bytes each)
uint32_t moby_at(int32_t idx) { return lwu(L4_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }
}  // namespace

// The cannon's four sparkles (their fields set up by helmet_init_fx: phase +0x10 .. +0x1c (0 .. 255, wrapped), its rate
// +0x20 .., the cycle timers +0x30 .. (255 frames), the sizes +0x40 ..): from 0.3 behind it as the camera point
// (L4:0x166f40) sees it (a quarter up), 0.1 apart further back, blue fading in and out over the cycle; none spawned while
// a scene plays.
// orig: L4:0x002e1d00
void cannon_sparkles_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, at = sp, step = sp + 0x10;  // (EE frame locals: the sparkle's place, the step back)
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.25f));
    vec3_sub(V(at), V(L4_ADDR(0x00166f40)), V(pos));
    vec3_set_length(V(at), V(at), u2f(0xbe99999au));
    vec3_set_length(V(step), V(at), u2f(0x3dcccccdu));
    vec3_add(V(at), V(at), V(pos));
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 0.25f));
    for (uint32_t k = 0; k < 0x10; k += 4) {
        const float phase = fpu_add(lwc1(pv + 0x10 + k), lwc1(pv + 0x20 + k));
        swc1(pv + 0x10 + k, phase);
        if (255.0f <= phase) {
            swc1(pv + 0x10 + k, fpu_sub(phase, 255.0f));
        } else if (phase <= 0.0f) {
            swc1(pv + 0x10 + k, fpu_add(phase, 255.0f));
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x30 + k))) != 0)
            sw(pv + 0x30 + k, static_cast<uint32_t>(frame_scale_i(0xff)));
        const float elapsed = int_to_float(frame_scale_i(0xff) - lw(pv + 0x30 + k));
        const float fade = abs_f(fpu_sub(0.5f, fpu_div(elapsed, static_cast<float>(frame_scale_i(0xff)))));
        const uint32_t colour = rgba_lerp(0x4040ffffu, 0x1040ffffu, fade);
        if (!scene_playing()) {
            const uint32_t frame = static_cast<uint32_t>(float_to_int(lwc1(pv + 0x10 + k))) & 0xffu;
            spawn_particle_3b(at, colour, frame, 0x35, 1, 2, 0, lwc1(pv + 0x40 + k));
        }
        vec3_add(V(at), V(at), V(step));
    }
    ee_stack_free(0xc0);
}

// pvars: the sparkles +0x10 .. +0x4c (cannon_sparkles_l4), the checkpoint's moby index +4 (-1: none). States: 0 set up,
// 1 waits for the hero, 2 its scene, 3 gone.
// orig: L4:0x002e1a10
void eud_class1120_update_l4(uint32_t m) {
    ee_stack_alloc(0x40);  // (the original's frame; it keeps no locals there)
    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(dt(), kHalfPi)));
    if (scene_playing()) {
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    } else if (lhu(m + 0x34) & 1) {
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    }
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002e1ac0) set up: the sparkles; gone if the hero has the cannon; a unit up
        helmet_init_fx(m);
        if (lbu(0x0013d4e8 + SUCK_CANNON) != 0) {
            moby_retire(m);
            break;
        }
        sb(m + 0x20, 1);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.0f));
        break;
    case 1:  // (0x002e1b00) waiting: the hero within 3, hidden, scene 2
        cannon_sparkles_l4(m);
        if (!(vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 3.0f)) break;
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        cutscene_start(2);
        sb(m + 0x20, 2);
        break;
    case 2: {  // (0x002e1b54) the scene over: the message, the cannon, the checkpoint, the save request
        if (scene_playing()) break;
        sub_00263d40(lbu(0x0013d4c0 + SUCK_CANNON) != 0 ? TXT(0x53df) : TXT(0xfa7), -1);
        sw(L4_ADDR(0x0015f640), 0xb4);
        item_give(SUCK_CANNON, 1);
        if (const int32_t at = lw(pv + 4); at != -1) {
            const uint32_t spot = moby_at(at);
            checkpoint_save(spot + 0x10, spot + 0x40);
        } else {
            debug_printf_stub();  // (a debug message: no checkpoint's moby)
        }
        sb(m + 0x20, 3);
        level_change_request(0, -1);
        break;
    }
    case 3:  // (0x002e1c00)
        moby_retire(m);
        break;
    default:
        break;
    }
    ee_stack_free(0x40);
}

}  // namespace rac1
