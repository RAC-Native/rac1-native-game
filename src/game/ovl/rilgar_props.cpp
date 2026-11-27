// Rilgar's (level 5) smaller moving props: the rocking floats (class 810), the props riding on another moby (812), the
// shared hum (838), the fog and light zones (841), the gates that slide open and sink (843), the pods that burst (846),
// the gates let down into their slots (852), the lifts (877), the hatches over the lifts (893), the turning gates (895),
// the bobbing floats (984), the props hidden in the scenes (1099) and the scenes' smoke (1550). Ported by hand from
// Rilgar's own code (L5:0x30bdd8, 0x30bf98, 0x30dc68, 0x30e1f8, 0x30e508, 0x30f5c8, 0x314eb0, 0x3156d0 with 0x315ee0 and
// 0x315f70, 0x316258, 0x3166a0, 0x318b98, 0x319c28, 0x31d5c8), the original's order of calls and float operations kept.
#include <game/level/level_data.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_class344.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/hydro.h>
#include <game/ovl/arrival.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/veldin_mobys.h>
#include <game/level/level_write.h>

namespace rac1 {

namespace {
constexpr uint32_t kHero = 0x0013f350;  // the hero's block (+0x80 his position, +0x2f0 the height of the water he is in)
float dt() { return lwc1(0x0015ed6c); }   // the frame's time scale
float dt2() { return lwc1(0x0015ed70); }  // its square
float deg() { return u2f(0x3c8efa35u); }  // (one degree in radians)
uint32_t moby_at(int32_t i) { return lwu(L5_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(i) << 8); }
}  // namespace

// Class 810, a rocking float: it rolls (+0x40) and pitches (+0x44) on two sine phases (+0x6c, +0x60) at the level's rates
// and sizes (0x161cd4 .. 0x161ce0, degrees), creaks now and then (its timer +0x78, the level's range 0x161ce4 ..
// 0x161ce8 frames), and carries what rides it (the attach frame +0x20).
// orig: L5:0x0030bdd8
void ril_class810_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    // the next creak: the level's range of frames
    auto reroll = [pv] {
        const float lo = static_cast<float>(lw(L5_ADDR(0x00161ce4u))), hi = static_cast<float>(lw(L5_ADDR(0x00161ce8u)));
        sw(pv + 0x78, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(lo, hi)))));
    };
    switch (lbu(m + 0x20)) {
    case 0:  // (0x0030be20) set up: the creak's timer, the two phases at random
        reroll();
        swc1(pv + 0x60, rand_angle());
        swc1(pv + 0x6c, rand_angle());
        sb(m + 0x20, 1);
        break;
    case 1: {  // (0x0030be68) rocks
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x78))) != 0) {
            moby_sound_start(0, 0, m);
            reroll();
        }
        const uint32_t sp = ee_stack_alloc(0xb0);
        lq_sq(sp, m + 0x10);
        lq_sq(sp + 0x10, m + 0x40);
        swc1(pv + 0x60, angle_add(lwc1(pv + 0x60), fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161cd4u)), deg()), dt())));
        swc1(pv + 0x6c, angle_add(lwc1(pv + 0x6c), fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161cd8u)), deg()), dt())));
        swc1(m + 0x44, fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161cdcu)), deg()), sin_vu(lwc1(pv + 0x60))));
        swc1(m + 0x40, fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161ce0u)), deg()), sin_vu(lwc1(pv + 0x6c))));
        vec3_sub(V(sp + 0x20), V(m + 0x10), V(sp));
        attach_frame_set(pv + 0x20, sp + 0x20, sp + 0x10, m + 0x40);
        ee_stack_free(0xb0);
        break;
    }
    default:
        break;
    }
}

// Class 812, a prop riding on another moby (its index +0x80; -1: none): its place and angles kept relative to that moby
// (+0x60 / +0x70), and what rides the prop carried (the attach frame +0x20).
// orig: L5:0x0030bf98
void ril_class812_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x0030bfd8) set up: its place relative to its carrier
        sw(pv + 0x5c, 1);
        if (lw(pv + 0x80) < 0) break;
        sub_00260668(m, moby_at(lw(pv + 0x80)), m + 0x10, m + 0x40, pv + 0x60, pv + 0x70);
        sb(m + 0x20, 1);
        break;
    case 1: {  // (0x0030c018) rides
        const uint32_t sp = ee_stack_alloc(0x90);
        lq_sq(sp, m + 0x10);
        lq_sq(sp + 0x10, m + 0x40);
        sub_002604f0(m, moby_at(lw(pv + 0x80)), pv + 0x60, pv + 0x70, m + 0x10, m + 0x40);
        vec3_sub(V(sp + 0x20), V(m + 0x10), V(sp));
        attach_frame_set(pv + 0x20, sp + 0x20, sp + 0x10, m + 0x40);
        ee_stack_free(0x90);
        break;
    }
    default:
        break;
    }
}

// Class 838, one of a class's mobys sharing one looping sound (the slot word *(+4)): within 64 of the camera the
// nearest of them holds it (the slot's moby +0x88 and position +0x90), started when the slot is free or held by another
// class; the draw (0x30de90) queued once a frame (the stamp 0x161d38); when its switch moby (+0) is set (+0xbc) it
// stops: a sound, no collision, state 2.
// orig: L5:0x0030dc68
void ril_class838_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    constexpr uint32_t kSlots = 0x0013e550;  // (the sound slots, 0x70 bytes each: +0x88 the moby, +0x90 the position)
    switch (state) {
    case 0:  // (0x0030dcd4) set up: hidden, the shared slot none
        sb(m + 0x20, 1);
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        sw(lwu(pv + 4), 0xffffffffu);
        break;
    case 1: {  // (0x0030dcf8)
        const uint32_t frame = lwu(L5_ADDR(0x0015f5ccu));
        if (lwu(L5_ADDR(0x00161d38u)) != frame) {
            sw(L5_ADDR(0x00161d38u), frame);
            sub_001f93e0(L5_ADDR(0x0030de90u), m);
        }
        const uint32_t cam = L5_ADDR(0x001671c0u);
        if (vec3_distance(V(m + 0x10), V(cam)) < 64.0f) {
            const int32_t slot = lw(lwu(pv + 4));
            bool start = slot < 0;
            bool take = false;
            if (!start) {
                const uint32_t o = lwu(kSlots + static_cast<uint32_t>(slot) * 0x70 + 0x88);
                if (o == 0 || lh(o + 0xa6) != 0x346) start = true;
                else if (o != m) {
                    if (lbu(o + 0x20) != state) take = true;
                    else take = vec3_distance(V(m + 0x10), V(cam)) < vec3_distance(V(o + 0x10), V(cam));
                }
            }
            if (start) {
                sw(lwu(pv + 4), static_cast<uint32_t>(moby_sound_start(0, 4, m)));
            } else if (take) {
                sw(kSlots + static_cast<uint32_t>(lw(lwu(pv + 4))) * 0x70 + 0x88, m);
                lq_sq(kSlots + 0x90 + static_cast<uint32_t>(lw(lwu(pv + 4))) * 0x70, m + 0x10);
            }
        }
        const int32_t sw_moby = lw(pv);
        if (sw_moby >= 0 && lbu(moby_at(sw_moby) + 0xbc) != 0) {
            moby_sound_start(1, 0, m);
            sw(m + 0x94, 0);
            sb(m + 0x20, 2);
        }
        break;
    }
    case 2:  // (0x0030de54) stopped
        if (lwu(m + 0x94) != 0) sw(m + 0x94, 0);
        break;
    default:
        break;
    }
}

// Class 841, the fog and light zones: the level's fog (0x15f444 .. 0x15f454) switched to the zone's own (colour 0x49 /
// 0x53 / 0x0c, near 0, far 33792, 255) when the hero enters its box +0x2c, and back to the level's (saved in state 0)
// in its box +0x28; inside the box +0x38 the level's light colour (0x161200) blended between +0x30 and +0x34 along the
// box's first axis; inside the box +0x28 the hero's water height put out of reach (500).
// orig: L5:0x0030e1f8
void ril_class841_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hero = kHero + 0x80;
    if (point_in_box(hero, lw(pv + 0x28)) != 0) swc1(kHero + 0x2f0, 500.0f);
    // the level's fog from the colour bytes at `c` and the floats at `f` (near / far / two more, as the zone keeps them)
    auto set_fog = [](uint32_t c, uint32_t f0, uint32_t f1, uint32_t f2, uint32_t f3) {
        sb(L5_ADDR(0x0015f444u), lbu(c));
        sb(L5_ADDR(0x0015f445u), lbu(c + 1));
        sb(L5_ADDR(0x0015f446u), lbu(c + 2));
        const float a = lwc1(f0), b = lwc1(f1), d = lwc1(f2), e = lwc1(f3);
        swc1(L5_ADDR(0x0015f448u), a);
        swc1(L5_ADDR(0x0015f44cu), b);
        swc1(L5_ADDR(0x0015f450u), d);
        swc1(L5_ADDR(0x0015f454u), e);
    };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x0030e284) set up: the level's fog and light saved, the zone's set
        sb(pv + 0x20, lbu(L5_ADDR(0x0015f444u)));
        sb(pv + 0x21, lbu(L5_ADDR(0x0015f445u)));
        sb(pv + 0x22, lbu(L5_ADDR(0x0015f446u)));
        const float a = lwc1(L5_ADDR(0x0015f448u)), b = lwc1(L5_ADDR(0x0015f44cu)), c = lwc1(L5_ADDR(0x0015f450u)), d = lwc1(L5_ADDR(0x0015f454u));
        swc1(pv, a);
        swc1(pv + 8, b);
        swc1(pv + 4, c);
        swc1(pv + 0xc, d);
        sb(pv + 0x23, 0x49);
        sb(pv + 0x24, 0x53);
        sb(pv + 0x25, 0xc);
        swc1(pv + 0x18, 33792.0f);
        swc1(pv + 0x14, 255.0f);
        sw(pv + 0x10, 0);
        sw(pv + 0x1c, 0);
        sw(pv + 0x34, 0x40408070u);
        sw(pv + 0x30, lbu(L5_ADDR(0x00161200u)) + (lbu(L5_ADDR(0x00161201u)) << 8) + (lbu(L5_ADDR(0x00161202u)) << 16) +
                          (lbu(L5_ADDR(0x00161203u)) << 24));
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x0030e35c) outside: the zone's fog on entering its box
        if (point_in_box(hero, lw(pv + 0x2c)) == 0) break;
        set_fog(pv + 0x23, pv + 0x10, pv + 0x18, pv + 0x14, pv + 0x1c);
        sb(m + 0x20, 2);
        break;
    case 2:  // (0x0030e3cc) inside: the level's fog back on leaving through its box
        if (point_in_box(hero, lw(pv + 0x28)) == 0) break;
        set_fog(pv + 0x20, pv, pv + 8, pv + 4, pv + 0xc);
        sb(m + 0x20, 1);
        break;
    default:
        break;
    }
    if (point_in_box(hero, lw(pv + 0x38)) != 0) {  // the light's blend along the box
        const uint32_t sp = ee_stack_alloc(0x80);
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x38));
        vec3_sub(V(sp), V(hero), V(box + 0x30));
        sw(sp + 0xc, 0);
        mat3_transform(V(sp + 0x10), V(sp), M(lvd::box(lvd::BOXES, lw(pv + 0x38)) + 0x40));
        const uint32_t c = rgba_lerp(lwu(pv + 0x30), lwu(pv + 0x34), fpu_mul(fpu_add(lwc1(sp + 0x10), 1.0f), 0.5f));
        sb(L5_ADDR(0x00161201u), static_cast<uint32_t>(static_cast<int32_t>(c) >> 8));
        sb(L5_ADDR(0x00161202u), static_cast<uint32_t>(static_cast<int32_t>(c) >> 16));
        sb(L5_ADDR(0x00161203u), static_cast<uint32_t>(static_cast<int32_t>(c) >> 24));
        sb(L5_ADDR(0x00161200u), c);
        ee_stack_free(0x80);
    }
}

// Class 843, a gate in a box (+0x10, the level's box list; +0x18 how far from the box's centre along its first axis,
// the other way when +0x1c): when its switch moby (+0x14) is set (+0xbc) it slides out 3 times as far (its velocity
// +0, eased), then sinks 5.9 below the box's centre (its speed +8) and stays.
// orig: L5:0x0030e508
void ril_class843_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t off = sp;
    // the offset along the box's axis, `k` times the distance, flipped by +0x1c
    auto offset = [&](uint32_t box, float k_dist) {
        vec3_set_length(V(off), V(box + 0x10), k_dist);
        if (lw(pv + 0x1c) != 0) vec3_scale(V(off), V(off), -1.0f);
    };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x0030e56c) set up: in its box, along its axis
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x10));
        lq_sq(m + 0x10, box + 0x30);
        swc1(m + 0x48, lwc1(box + 0x78));
        vec3_set_length(V(off), V(box + 0x10), lwc1(pv + 0x18));
        if (lw(pv + 0x1c) != 0) {
            vec3_scale(V(off), V(off), -1.0f);
            sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        }
        vec3_add(V(m + 0x10), V(m + 0x10), V(off));
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x0030e5e8) waits for its switch
        if (lbu(moby_at(lw(pv + 0x14)) + 0xbc) == 0) break;
        moby_sound_start(0, 0, m);
        sb(m + 0x20, 2);
        vec_clear(V(pv));
        break;
    case 2: {  // (0x0030e62c) slides out
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x10));
        sw(sp + 0x10, 0);
        offset(box, fpu_mul(lwc1(pv + 0x18), 3.0f));
        vec3_add(V(off), V(box + 0x30), V(off));
        vec3_sub(V(off), V(off), V(m + 0x10));
        const float left = vec3_length(V(off));
        swc1(sp + 0x14, vec3_length(V(pv)));
        seek_1d(sp + 0x10, sp + 0x14, left, fpu_mul(dt2(), 20.0f), fpu_mul(dt2(), 30.0f), fpu_mul(dt(), 10.0f));
        vec3_set_length(V(pv), V(off), lwc1(sp + 0x14));
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
        if (vec3_length(V(off)) < u2f(0x3c23d70au)) sb(m + 0x20, 3);  // (within 0.01)
        break;
    }
    case 3: {  // (0x0030e74c) sinks
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x10));
        const float depth = u2f(0x40bccccdu);  // (5.9)
        seek_1d(m + 0x18, pv + 8, fpu_sub(lwc1(box + 0x38), depth), fpu_mul(dt2(), 20.0f), fpu_mul(dt2(), 40.0f), fpu_mul(dt(), 10.0f));
        if (lwc1(m + 0x18) < fpu_sub(lwc1(box + 0x38), depth)) sb(m + 0x20, 4);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x80);
}

// Class 895, a gate that turns open: placed a quarter turn off its yaw (+0xc, the closed yaw; the other way when +0),
// it turns 40 degrees past the closed yaw (720 degrees a second, its rate +4) once its switch moby (+8) is set (+0xbc).
// orig: L5:0x003166a0
void ril_class895_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float quarter = u2f(0x3fc90fdbu);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x003166f0) set up, then as state 1
        if (lw(pv) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        sb(m + 0x20, 1);
        swc1(pv + 0xc, lwc1(m + 0x48));
        swc1(m + 0x48, lw(pv) != 0 ? angle_sub(lwc1(m + 0x48), quarter) : angle_add(lwc1(m + 0x48), quarter));
        [[fallthrough]];
    case 1:  // (0x00316750) waits for its switch
        if (lbu(moby_at(lw(pv + 8)) + 0xbc) == 0) break;
        moby_sound_start(0, 0, m);
        sb(m + 0x20, 2);
        break;
    case 2: {  // (0x00316788) turns open
        const float forty = u2f(0x3f32b8c2u);
        const float target = lw(pv) != 0 ? angle_sub(lwc1(pv + 0xc), forty) : angle_add(lwc1(pv + 0xc), forty);
        const float k = u2f(0x41490fdbu);  // (4 pi)
        yaw_seek(m + 0x48, pv + 4, target, fpu_mul(dt2(), k), fpu_mul(dt2(), k), fpu_mul(dt(), k));
        break;
    }
    default:
        break;
    }
}

// Class 984, a float bobbing on the water: its scale the class's by the level's 0x161f08; it rises and falls a unit
// about its placed height (+4) on a phase (+0, 50 degrees a second); hidden in a scene (game mode 2).
// orig: L5:0x00318b98
void ril_class984_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L5_ADDR(0x00161f08u))));
    switch (state) {
    case 0:  // (0x00318be0)
        sb(m + 0x20, 1);
        swc1(pv, rand_angle());
        swc1(pv + 4, lwc1(m + 0x18));
        break;
    case 1:  // (0x00318bfc)
        if (lw(L5_ADDR(0x0015f5c4u)) == 2) {
            sb(m + 0x31, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
        } else {
            sb(m + 0x31, state);
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        }
        swc1(pv, angle_add(lwc1(pv), fpu_mul(dt(), u2f(0x3f5f66f3u))));
        swc1(m + 0x18, fpu_add(lwc1(pv + 4), sin_vu(lwc1(pv))));
        break;
    default:
        break;
    }
}

// Class 1099, a prop hidden during the scenes (game mode 2) and shown otherwise.
// orig: L5:0x00319c28
void ril_class1099_update_l5(uint32_t m) {
    if (lw(L5_ADDR(0x0015f5c4u)) == 2) {
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
    } else {
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
    }
}

// Class 1550, the scenes' smoke: in a scene (game mode 2) of the level's scene state 0x16cc60 (its scene +0x30) 10 or
// 11, smoke from the scene's moby 3 (its slots +0x178; the original's index test, the state 1 against the scene less 10,
// always picks 3).
// orig: L5:0x0031d5c8
void ril_class1550_update_l5(uint32_t m) {
    const uint32_t state = lbu(m + 0x20);
    switch (state) {
    case 0:  // (0x0031d5ec)
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        break;
    case 1: {  // (0x0031d600)
        if (lw(L5_ADDR(0x0015f5c4u)) != 2) break;
        const uint32_t scene = static_cast<uint32_t>(lw(L5_ADDR(0x0016cc90u)) - 10);
        if (!(scene < 2u)) break;
        const uint32_t idx = state < scene ? 0u : 3u;
        scene_moby_smoke(lwu(L5_ADDR(0x0016cc60u) + 0x178 + (idx << 2)));
        break;
    }
    default:
        break;
    }
}

// Class 852 (its code shared with class 853), a gate let down 8 into its slot (its height a unit above where it was
// placed kept, +0xc) until its switch moby (+0 of the pvars) says: 1 raises it again, 2 puts it up at once. Raised, it
// first waits for its delay (+4), then rises at the level's rates (0x161e80 .. 0x161e88; class 853 sounds when it is
// within 3 of the top and still moving); after its second delay (+8) its pitch (+0x44) eases back to 0 at the level's
// rates (0x161e8c .. 0x161e94, degrees) and it is done; class 852 sounds as that second delay ends.
// orig: L5:0x00314eb0
void ril_class852_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    switch (state) {
    case 0:  // (0x00314f04)
        swc1(pv + 0xc, fpu_add(lwc1(m + 0x18), 1.0f));
        sb(m + 0x20, 1);
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 8.0f));
        break;
    case 1: {  // (0x00314f34) waits for its switch
        const uint32_t s = lbu(moby_at(lw(pv)) + 0xbc);
        if (s == 1) {
            sb(m + 0x20, 2);
        } else if (s == 2) {
            sb(m + 0x20, 3);
            swc1(m + 0x18, lwc1(pv + 0xc));
            sw(m + 0x44, 0);
        }
        break;
    }
    case 2: {  // (0x00314f80) rises, then levels
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 4))) != 0) {
            seek_1d(m + 0x18, pv + 0x10, lwc1(pv + 0xc), fpu_mul(lwc1(L5_ADDR(0x00161e80u)), dt2()),
                    fpu_mul(lwc1(L5_ADDR(0x00161e84u)), dt2()), fpu_mul(lwc1(L5_ADDR(0x00161e88u)), dt()));
            if (lh(m + 0xa6) == 0x355 && abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0xc))) < 3.0f &&
                3.0f < abs_f(fpu_add(fpu_sub(lwc1(pv + 0xc), lwc1(m + 0x18)), lwc1(pv + 0x10))))
                moby_sound_start(0, 0, m);
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) != 0) {
            yaw_seek(m + 0x44, pv + 0x14, 0.0f, fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161e8cu)), deg()), dt2()),
                     fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161e90u)), deg()), dt2()),
                     fpu_mul(fpu_mul(lwc1(L5_ADDR(0x00161e94u)), deg()), dt()));
            if (lwc1(m + 0x44) == 0.0f) sb(m + 0x20, 3);
        } else if (lw(pv + 8) == 1 && lh(m + 0xa6) == 0x354) {
            moby_sound_start(0, 0, m);
        }
        break;
    }
    default:
        break;
    }
}

namespace {
// a piece the pod bursts into placed on it: drawn (+0x31), its draw distance and colour (+0x30, +0x32, +0x38) and its
// position and rotation the pod's
void piece_on(uint32_t p, uint32_t m) {
    sb(p + 0x30, 0x40);
    sd(p + 0x38, ld(m + 0x38));
    sb(p + 0x31, 1);
    sh(p + 0x32, 0x40);
    lq_sq(p + 0x10, m + 0x10);
    lq_sq(p + 0x40, m + 0x40);
}
}  // namespace

// Class 846, a pod that bursts when hit (the hit mask 0xb30000): a shell (class 0x3d4) thrown back along its facing
// (5 to 12 a second, 60 frames), its sound, and six shards (class 0x3d5) in a ring of 60 degrees each, 1.4 out and 0.75
// up in the pod's frame, each with ten puffs of smoke (two particles each, the spin either way) and thrown back 0 to 24
// a second for 1 to 2 seconds; then it is noted as burst (+0xbc in the level's list 0x1baaa0) and goes. One burst
// before (+0xbc set) also makes it go.
// orig: L5:0x0030f5c8
void ril_class846_update_l5(uint32_t m) {
    if (sub_0025a420(m, 0xb30000u, 0) == 0) {
        if (lbu(m + 0xbc) != 0) moby_retire(m);
        return;
    }
    const uint32_t sp = ee_stack_alloc(0x50);
    const uint32_t shell = moby_spawn(0x3d4);
    if (shell != 0) {  // (0x0030f630)
        sb(shell + 0x30, 0x40);
        sd(shell + 0x38, ld(m + 0x38));
        sb(shell + 0x31, 1);
        const uint32_t spv = lwu(shell + 0x78);
        sh(shell + 0x32, 0x40);
        lq_sq(shell + 0x10, m + 0x10);
        lq_sq(shell + 0x40, m + 0x40);
        const float speed = fpu_mul(rand_range_f(5.0f, 12.0f), dt());
        vec3_scale(V(sp), V(m + 0xc0), -speed);
        vec_clear(V(sp + 0x10));
        class344_throw(shell, m, sp, sp + 0x10, frame_scale_i(0x3c));
        sw(spv + 0xc, 0x200);
    }
    moby_sound_start(0, 0, shell);  // (0x0030f6d8: on the shell, or none)
    float yaw = 0.0f;
    for (int i = 0; i < 6; ++i) {  // (0x0030f710)
        yaw = angle_add(yaw, u2f(0x3f860a92u));
        const uint32_t p = moby_spawn(0x3d5);
        if (p == 0) continue;
        const uint32_t ppv = lwu(p + 0x78);
        for (uint32_t o = 0; o < 0x10; o += 4) {
            sw(sp + 0x10 + o, 0);
            sw(sp + 0x30 + o, 0);
        }
        swc1(sp + 0x18, 0.75f);
        swc1(sp + 0x34, fpu_mul(sin_vu(yaw), 1.4f));
        swc1(sp + 0x38, fpu_mul(cos_vu(yaw), 1.4f));
        lq_sq(sp + 0x20, sp + 0x30);
        mat3_transform(V(sp + 0x20), V(sp + 0x20), M(m + 0xc0));
        for (int k = 0; k < 10; ++k) {  // (0x0030f7e0) the smoke
            const float d = dt();
            rand_sphere_point(sp + 0x30, fpu_mul(d, 1.5f), fpu_add(d, d));
            vec3_scale(V(sp + 0x40), V(sp + 0x30), 5.0f);
            vec3_add(V(sp + 0x40), V(sp + 0x40), V(sp + 0x20));
            vec3_add(V(sp + 0x40), V(sp + 0x40), V(m + 0x10));
            const int32_t r = rand_mod(2);
            const int32_t spin = r != 0 ? r : -1;
            const float size = rand_range_f(0.4f, 0.5f);
            int32_t n = frame_scale_i(0x19);
            spawn_particle_35(sp + 0x40, static_cast<uint32_t>(n), 0x7f207f7fu, 0, static_cast<uint32_t>(spin), sp + 0x30,
                              fpu_mul(size, 0.2f), size, 0.0f);
            n = frame_scale_i(0x19);
            spawn_particle_35(sp + 0x40, static_cast<uint32_t>(n), 0x7f7f7f7fu, 1, static_cast<uint32_t>(-spin), sp + 0x30,
                              fpu_mul(size, 0.14f), fpu_mul(size, 0.7f), 0.0f);
        }
        piece_on(p, m);
        const float speed = rand_range_f(0.0f, 24.0f);
        const float frames = rand_range_f(60.0f, 120.0f);
        vec3_scale(V(sp), V(m + 0xc0), -fpu_mul(speed, dt()));
        vec_clear(V(sp + 0x10));
        class344_throw(p, m, sp, sp + 0x10, float_to_int(frame_scale_f(frames)));
        sw(ppv + 0xc, 0x200);
        swc1(p + 0x40, yaw);
        moby_update_transform(p);
    }
    sb(m + 0xbc, 1);
    moby_data_note_l1(m + 0xbc, 1, m, 1, L5_ADDR(0x001baaa0u));
    moby_retire(m);
    ee_stack_free(0x50);
}

namespace {
// class 893's hatch at the angle `a` about its hinge (the joint's rotation +0x20 of the pvars)
void hatch_at(uint32_t pv, float a) { quat_from_axis_angle(V(pv + 0x20), 0, a); }
}  // namespace

// Class 893, a hatch over a lift (the hatch the joint +0x10 of the pvars, its lift moby +0x58's +0xbc saying when):
// set up up or down (+0x54), turned round and raised 4 when it is the other way (+0x50); a closed hatch (state 1) opens
// when its lift moby says so (states 6 then 3); an open one (2) waits for anything but 2 and comes down (4), opens (5)
// and shuts (7). Moving it slides (states 3 / 4) to its place (+0, 1.9 below it along its up for 3) at up to 10 a
// second; its hatch swings (states 5 / 6) at 240 degrees a second squared to 68 / -34 degrees, stopped and bounced
// back a third at 0 and 34 (the first time it hits, +0xbc), done when it rests at the end.
// orig: L5:0x00316258
void ril_class893_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float open = u2f(0x3f17e9d8u);  // (34 degrees)
    const uint32_t state = lbu(m + 0x20);
    switch (state) {
    case 0: {  // (0x003162a4)
        if (lw(pv + 0x50) != 0) {
            swc1(m + 0x40, u2f(0x40490fdbu));
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 4.0f));
        }
        lq_sq(pv, m + 0x10);
        sub_0024f590(m, 0, pv + 0x10);
        if (lw(pv + 0x54) != 0) {
            sw(pv + 0x60, 0);
            sb(m + 0x20, 2);
            const uint32_t sp = ee_stack_alloc(0x10);
            vec3_scale(V(sp), V(m + 0xd0), lw(pv + 0x50) != 0 ? 2.0f : -2.0f);
            vec3_add(V(m + 0x10), V(m + 0x10), V(sp));
            ee_stack_free(0x10);
        } else {
            sb(m + 0x20, 1);
            swc1(pv + 0x60, open);
            hatch_at(pv, open);
        }
        sd(m + 0x38, ld(lwu(0x001413d0) + 0x38));
        break;
    }
    case 1:
    case 2: {  // (0x0031638c) waits for its lift moby
        const uint32_t s = lbu(moby_at(lw(pv + 0x58)) + 0xbc);
        if (s == 0) break;
        if (state == 2) {
            if (s != 2) sb(m + 0x20, 4);
        } else {
            moby_sound_start(0, 0, m);
            sb(m + 0x20, 6);
        }
        break;
    }
    case 3:
    case 4: {  // (0x00316544) slides to its place
        const uint32_t sp = ee_stack_alloc(0x30);
        if (state == 3) {
            vec3_scale(V(sp), V(m + 0xd0), u2f(0xbff33333u));
            vec3_add(V(sp), V(pv), V(sp));
        } else {
            lq_sq(sp, pv);
        }
        vec3_sub(V(sp + 0x10), V(sp), V(m + 0x10));
        sw(sp + 0x20, 0);
        seek_1d(sp + 0x20, pv + 0x5c, vec3_length(V(sp + 0x10)), fpu_mul(dt2(), 6.0f), fpu_mul(dt2(), 12.0f),
                fpu_mul(dt(), 10.0f));
        vec3_sub(V(sp + 0x10), V(sp), V(m + 0x10));
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), lwc1(pv + 0x5c));
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x10));
        if (vec3_distance(V(m + 0x10), V(sp)) < u2f(0x38d1b717u)) {
            if (state == 3) {
                moby_sound_start(1, 0, m);
                sb(m + 0x20, 7);
            } else {
                sb(m + 0x20, 5);
            }
        }
        ee_stack_free(0x30);
        break;
    }
    case 5:
    case 6: {  // (0x003163f4) the hatch swings
        const bool opening = state == 5;
        const float rest = opening ? open : 0.0f;
        const float k = fpu_mul(dt2(), u2f(0x40860a92u));
        yaw_seek(pv + 0x60, pv + 0x64, opening ? u2f(0x3f97e9d8u) : u2f(0xbf17e9d8u), k, k, fpu_mul(dt(), u2f(0x41490fdbu)));
        const float a = lwc1(pv + 0x60);
        bool stopped = false;
        if (open < a) {
            swc1(pv + 0x60, open);
            stopped = true;
        } else if (a < 0.0f) {
            swc1(pv + 0x60, 0.0f);
            stopped = true;
        }
        if (stopped) {
            if (lbu(m + 0xbc) == 0) {
                sb(m + 0xbc, 1);
                swc1(pv + 0x64, fpu_mul(lwc1(pv + 0x64), u2f(0xbea8f5c3u)));
            } else if (lwc1(pv + 0x60) == rest) {
                sb(m + 0x20, opening ? 7 : 3);
            }
        }
        hatch_at(pv, lwc1(pv + 0x60));
        break;
    }
    default:
        break;
    }
}

namespace {
// class 877's camera while it starts: the level's distance (0x161ec8) out along its facing and height (0x161ec0) above
// it, looking back down at the level's pitch (0x161ec4, degrees) and its yaw
// orig: L5:0x00315ee0
void lift_camera(uint32_t m, uint32_t pos, uint32_t rot) {
    vec3_set_length(V(pos), V(m + 0xc0), lwc1(L5_ADDR(0x00161ec8u)));
    vec3_add(V(pos), V(pos), V(m + 0x10));
    swc1(pos + 8, fpu_add(lwc1(pos + 8), lwc1(L5_ADDR(0x00161ec0u))));
    vec_clear(V(rot));
    const float pitch = fpu_mul(lwc1(L5_ADDR(0x00161ec4u)), deg());
    swc1(rot + 8, lwc1(m + 0x48));
    swc1(rot + 4, pitch);
}
// class 877's path (the level's path +0x68) laid out as a ring of 3.5 about it, a point every turn / (points - 1)
// orig: L5:0x00315f70
void lift_ring(uint32_t m) {
    const uint32_t path = lvd::path(lw(lwu(m + 0x78) + 0x68));
    const int32_t n = lw(path);
    const float step = fpu_div(u2f(0x40c90fd0u), static_cast<float>(n - 1));
    float a = 0.0f;
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t p = path + 0x10 + static_cast<uint32_t>(i) * 16;
        swc1(p, fpu_mul(cos_vu(a), 3.5f));
        const float y = fpu_mul(sin_vu(a), 3.5f);
        sw(p + 8, 0);
        swc1(p + 4, y);
        vec3_add(V(p), V(p), V(m + 0x10));
        lvd::path_rows_written(p);
        a = angle_add(a, step);
    }
}
// class 877's lamp: a pulse of its phase (+0x80, a turn a second) as grey 0x20 .. 0x80
void lift_lamp(uint32_t m, uint32_t pv) {
    swc1(pv + 0x80, angle_add(lwc1(pv + 0x80), fpu_mul(dt(), u2f(0x40c90fdbu))));
    int32_t g = float_to_int(fpu_mul(fpu_sub(fpu_mul(sin_vu(lwc1(pv + 0x80)), 4.0f), 3.0f), 128.0f));
    if (!(g < 129)) g = 0x80;
    else if (!(0x1f < g)) g = 0x20;
    const uint32_t c = static_cast<uint32_t>(g);
    sw(m + 0x90, (c << 16) | (c << 8) | 0x80000000u | c);
}
// the hero standing on moby `m` (0x13f64c) on his feet (0x13f65e 0) within 0.5 of its middle
bool hero_on_middle(uint32_t m) {
    return lwu(0x0013f64c) == m && lh(0x0013f65e) == 0 && vec2_distance(V(0x0013f3d0), V(m + 0x10)) < 0.5f;
}
}  // namespace

// Class 877, a lift between two heights (+0x60 the top, +0x64 the bottom of its pvars), turning on the way by its
// camera point's (+0x6c) turn: set up at the bottom (3). Standing at an end (4 the bottom, 5 the top) its lamp pulses;
// the hero stepping onto its middle starts it (its sound 1; its ring path laid out when it has one, +0x68) with its
// camera shown (state 1, +0x78 eased in; then 3 up or 2 down by +0xbc); the hero walking up from 5 to 32 away at
// the other end's height calls it there (+0xbc 0). Moving (2 up, 3 down) it eases its lift (+0x70, at its pace +0x88;
// with the camera: at 6 or 16 a second over its height, else a unit) and its camera out, its sound (4, the slot +0x74)
// kept; arrived it waits (6 at the top, 7 at the bottom) two seconds or until the hero steps off its middle, then
// stops its sound; the hero off it, the lift's hold (hydro_2eac18) let go and it is at that end (5 / 4); still on it
// after the wait, its lamp pulses and stepping on its middle again starts it back.
// orig: L5:0x003156d0
void ril_class877_update_l5(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x40);
    vec3_scale(V(sp), V(m + 0x10), -1.0f);
    lq_sq(sp + 0x10, m + 0x40);
    const uint32_t state = lbu(m + 0x20);
    auto stop_sound = [pv, m] {
        const int32_t slot = lw(pv + 0x74);
        if (slot == -1) return;
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    };
    switch (state) {  // (the table 0x2189a0)
    case 0:  // (0x00315748)
        swc1(pv + 0x80, rand_angle());
        sw(pv + 0x74, 0xffffffffu);
        swc1(pv + 0x84, lwc1(m + 0x48));
        sb(m + 0x20, 3);
        break;
    case 1: {  // (0x00315770) its camera eased in, then off
        sh(0x0014162a, lhu(pv + 0x68));
        approach_f(pv + 0x78, 1.0f, u2f(0x3e083127u));
        const float k = lwc1(pv + 0x78);
        swc1(L5_ADDR(0x0015f3fcu), k);
        if (k != 1.0f) break;
        lift_camera(m, sp + 0x20, sp + 0x30);
        camera_scripted_set(sp + 0x20, sp + 0x30, 1, 0, 0);
        const uint32_t next = lbu(m + 0xbc);
        sb(m + 0xbc, 1);
        sb(m + 0x20, next);
        break;
    }
    case 2:
    case 3: {  // (0x003157f4) moving
        if (sub_0028d8c0(m, lw(pv + 0x74)) == 0) sw(pv + 0x74, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        float a, v;
        if (lbu(m + 0xbc) != 0) {
            sh(0x0014162a, lhu(pv + 0x68));
            approach_f(pv + 0x78, 0.0f, u2f(0x3e083127u));
            swc1(L5_ADDR(0x0015f3fcu), lwc1(pv + 0x78));
            lift_camera(m, sp + 0x20, sp + 0x30);
            uid5_set_pvar_80(sp + 0x20);
            uid5_set_pvar_90(sp + 0x30);
            const float inv = fpu_div(1.0f, fpu_sub(lwc1(pv + 0x60), lwc1(pv + 0x64)));
            v = fpu_mul(fpu_mul(dt(), lbu(m + 0xbc) != 0 ? 6.0f : 16.0f), inv);
            a = fpu_mul(fpu_mul(dt2(), lbu(m + 0xbc) != 0 ? 6.0f : 16.0f), inv);
        } else {
            v = dt();
            a = dt2();
        }
        const float to = state == 2 ? 1.0f : 0.0f;
        seek_1d(pv + 0x70, pv + 0x88, to, a, a, v);
        const float lo = lwc1(pv + 0x64);
        swc1(m + 0x18, fpu_add(fpu_mul(fpu_sub(lwc1(pv + 0x60), lo), lwc1(pv + 0x70)), lo));
        const float turn = fpu_mul(angle_sub(lwc1(lvd::box(lvd::BOXES, lw(pv + 0x6c)) + 0x78), lwc1(pv + 0x84)), lwc1(pv + 0x70));
        swc1(m + 0x48, turn);
        swc1(m + 0x48, angle_add(turn, lwc1(pv + 0x84)));
        if (!(abs_f(fpu_sub(lwc1(pv + 0x70), to)) < u2f(0x3c23d70au))) break;
        // (0x003159a0) arrived: the wait, its sound stopped, its pace kept within a little
        sw(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(0x78)));
        stop_sound();
        const float pace = lwc1(pv + 0x88);
        sw(pv + 0x74, 0xffffffffu);
        const float little = u2f(0x3aa3d70au);
        if (little < pace) swc1(pv + 0x88, little);
        else if (pace < -little) swc1(pv + 0x88, -little);
        if (lbu(m + 0xbc) != 0) sb(m + 0x20, state == 2 ? 6 : 7);
        else sb(m + 0x20, state == 2 ? 4 : 5);
        break;
    }
    case 4:
    case 5:  // (0x00315c40) at an end
        lift_lamp(m, pv);
        if (hero_on_middle(m)) {
            moby_sound_start(1, 0, m);
            if (lw(pv + 0x68) != -1) lift_ring(m);
            sw(m + 0x90, 0x80208020u);
            sb(m + 0xbc, state == 5 ? 2 : 3);
            sb(m + 0x20, 1);
            sw(pv + 0x78, 0);
        } else {
            // the hero at the other end's height, 5 to 32 away: it goes for him
            const float d = vec2_distance(V(m + 0x10), V(0x0013f3d0));
            const float h = lwc1(state == 4 ? pv + 0x64 : pv + 0x60);
            if (abs_f(fpu_sub(lwc1(0x0013f628), h)) < 2.0f && d < 32.0f && 5.0f < d) {
                sb(m + 0xbc, 0);
                sb(m + 0x20, state == 4 ? 3 : 2);
            }
        }
        break;
    case 6:
    case 7: {  // (0x00315a88) arrived with the hero
        if (sub_0028d8c0(m, lw(pv + 0x74)) != 0) {
            sub_0028d918(lw(pv + 0x74));
            sw(pv + 0x74, 0xffffffffu);
        }
        if (frame_scale_i(0x1e) < lw(pv + 0x7c) || 0.5f < vec2_distance(V(0x0013f3d0), V(m + 0x10)))
            timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x7c)));
        if (lwu(0x0013f64c) != m) {
            hydro_2eac18(1);
            sb(m + 0x20, state == 7 ? 5 : 4);
            break;
        }
        if (lw(pv + 0x7c) != 0) break;
        lift_lamp(m, pv);
        if (hero_on_middle(m)) {
            moby_sound_start(1, 0, m);
            sw(m + 0x90, 0x80208020u);
            sb(m + 0x20, state == 7 ? 2 : 3);
        }
        break;
    }
    default:
        break;
    }
    vec3_add(V(sp), V(sp), V(m + 0x10));
    attach_frame_set(pv + 0x20, sp, sp + 0x10, m + 0x40);
    ee_stack_free(0x40);
}

}  // namespace rac1
