// Class 1633, the Sandmouse's shot, and class 1818, the Sandmouse (its update, helpers and glow): one port of the copies ten levels carry (Novalis, Aridia, Kerwan, Eudora, Rilgar, Blarg,
// Batalia, Pokitaru, Hoven, Oltanis). The copies are the same code relinked (compared with the relocated fields masked);
// what they name is the engine's (OV: the camera 0x166dc0, the collision's hit point 0x173e60) or the base ELF's (bare:
// the hero 0x13f3d0, the frame scales 0x15ed60 / 0x15ed6c / 0x15ed70). Header shared_sandmouse.h.
//
// The shot's pvars: its pitch +0 and yaw +4 turn rates (angle_seek_rate's), the aimed pitch +8 and yaw +0xc, the
// shooter +0x10, its life +0x14.
#include <game/ovl/shared_sandmouse.h>

#include <game/math/vecmath.h>
#include <game/lib/libc.h>
#include <game/ovl/batalia_mobys.h>
#include <game/ovl/batalia_part2.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/metal.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_particle_4a.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/weapons.h>
#include <game/ovl/blarg_part2.h>
#include <game/ovl/glows.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/rilgar_part2.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

// The levels' own builds of glow_draw 0x263618 (variants: their transliterations; Aridia's, Kerwan's and Eudora's are
// declared by no header of theirs)
TrRet lv2_264b50_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x00264b50
TrRet lv3_2517b0_l3(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L3:0x002517b0
TrRet eud_255f08_l4(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L4:0x00255f08

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position

// a glow through the level's own build of glow_draw (Novalis's is Veldin's code: the port)
void level_glow(uint32_t point, uint32_t rgba, float size, float toward) {
    using Glow = TrRet (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float);
    Glow g = nullptr;
    switch (ovl_reloc_level()) {
    case ARIDIA: g = lv2_264b50_l2; break;
    case KERWAN: g = lv3_2517b0_l3; break;
    case EUDORA: g = eud_255f08_l4; break;
    case RILGAR: g = rl2_28d788_l5; break;
    case BLARG: g = bl2_272498_l6; break;
    case BATALIA: g = bt2_26d590_l8; break;
    case POKITARU: g = pok_288cc0_l11; break;
    case HOVEN: g = hov_27bcd0_l12; break;
    case OLTANIS: g = olt_26b1b8_l14; break;
    default: glow_draw(point, rgba, size, toward); return;
    }
    g(point, rgba, 0, 0, 0, 0, 0, 0, size, toward, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}
}  // namespace

// orig: L1:0x0030de68 L2:0x002f0eb0 L3:0x002e1898 L4:0x002e68f8 L5:0x0031e730 L6:0x0030adb0 L8:0x0030ab50 L11:0x0031c728 L12:0x0030a618 L14:0x00309bb0
void sandmouse_glow(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t k = 0; k < 2; ++k) level_glow(pv + 0x70 + k * 0x10, 0x2528aa28u, 0.1f, u2f(0x3da3d70au));
}

// The shot grows to its class's scale; for its first frames (life above 35 - 1 frames' worth) it turns toward its aimed
// pitch and yaw, then keeps its own; it flies 40 units a second along them. Out of the level's box (2..1021), more than
// 64 units from the camera or its life out, it leaves; else its path this frame is tested against the world and the
// mobys (its shooter ignored) with a hit record of its own (pushing the hit away from the hero: damage 0.5, kinds
// 0x10001): on a hit it stops at the hit point and leaves when what it hit takes no damage (sub_001f35d8 not 0), else its
// life is cut to 3 frames.
// orig: L1:0x0030c9a8 L2:0x002efee0 L3:0x002e08c8 L4:0x002e5928 L5:0x0031d760 L6:0x003094f0 L8:0x00308920 L11:0x0031b758 L12:0x00309648 L14:0x00308be0
void class1633_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const float full = lwc1(lwu(m + 0x24) + 0x24);
    if (lwc1(m + 0x2c) < full)
        swc1(m + 0x2c, fpu_add(lwc1(m + 0x2c), fpu_mul(fpu_mul(lwc1(0x0015ed60), u2f(0x3e570a3du)), full)));
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t was = sp, move = sp + 0x10, rec = sp + 0x20;

    // the aim: the pvars' for the first frames, then the shot's own
    const bool aiming = lh(pv + 0x14) < frame_scale_i(0x23) - frame_scale_i(1);
    const float aim_pitch = aiming ? lwc1(pv + 8) : lwc1(m + 0x48);
    const float aim_yaw = aiming ? lwc1(pv + 0xc) : lwc1(m + 0x44);
    const float pi = u2f(0x40490fdbu), three_halves_pi = u2f(0x4096cbe4u);
    {
        const float sq = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
        swc1(m + 0x48, angle_seek_rate(pv, lwc1(m + 0x48), aim_pitch, fpu_mul(sq, three_halves_pi), fpu_mul(sq, pi), fpu_mul(dt, pi)));
    }
    {
        const float sq = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
        swc1(m + 0x44, angle_seek_rate(pv + 4, lwc1(m + 0x44), aim_yaw, fpu_mul(sq, three_halves_pi), fpu_mul(sq, pi), fpu_mul(dt, pi)));
    }
    vec_from_spherical_v(V(move), fpu_mul(lwc1(0x0015ed6c), 40.0f), lwc1(m + 0x48), -lwc1(m + 0x44));
    lq_sq(was, pos);
    vec3_add(V(pos), V(pos), V(move));

    const float x = lwc1(pos), y = lwc1(pos + 4), z = lwc1(pos + 8);
    bool leave = x < 2.0f || 1021.0f < x || y < 2.0f || 1021.0f < y || z < 2.0f || 1021.0f < z;
    leave = leave || x < 0.0f || y < 0.0f || z < 0.0f;
    leave = leave || 64.0f < vec2_distance(V(pos), V(OV(0x00166dc0)));
    if (!leave) {
        // the hit record: its direction (from the hero, flat, unit, z 1), the shot, kinds 0x10001, damage 0.5, the class
        sw(rec + 0x14, 0x10001);
        swc1(rec + 0x1c, 0.5f);
        sw(rec + 0x10, m);
        sw(rec + 0x20, 1);
        vec3_sub(V(rec), V(pos), V(HERO_POS));
        vec2_set_length(V(rec), V(rec), 1.0f);
        swc1(rec + 8, 1.0f);
        swc1(rec + 0xc, u2f(0x45afdf66u));
        sb(rec + 0x19, 1);
        sh(rec + 0x1a, lhu(m + 0xa6));
        sb(rec + 0x18, 1);
        leave = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x14))) != 0;
    }
    if (leave) {
        moby_retire(m);
    } else if (segment_collide_v(V(was), V(pos), 0, lwu(pv + 0x10), static_cast<const uint32_t*>(ee_ram_ptr(rec))) != 0) {
        if (sub_001f35d8() != 0) {
            lq_sq(pos, OV(0x00173e60));
            moby_retire(m);
        } else if (frame_scale_i(3) < lh(pv + 0x14)) {
            sh(pv + 0x14, static_cast<uint32_t>(frame_scale_i(3)));
        }
    }
    ee_stack_free(0xd0);
}


// ---- class 1818, the Sandmouse (its update and helpers)
//
// Its pvars: +0 the goal, +0x10 the step, +0x20 / +0x30 its home's place, +0x40 the zone (a box index) the hero must be in,
// +0x48 its house (a moby index; -1 none), +0x50 the frames it has not moved, +0x52 the out timer, +0x54 the shot timer,
// +0x56 which side shoots next, +0x58 the rest timer, +0x5c the shots fired, +0x60 its target, +0x64 its sound slot,
// +0x70 / +0x80 the glows' points, +0x90 a joint's record. The level's walker record (one per level, its own data) drives
// its steps; the level's tunings: +0 the shot's interval, +4 the target range, +8 / +0xc the aim's yaw and pitch reach
// (degrees), +0x10 the shot's start ahead, +0x14 a target's height (with no pvars), +0x18 the rest, +0x1c the shots it
// fires, +0x24 the out time, +0x28 / +0x2c the dust's count, +0x30 the burrow's size, +0x34 / +0x38 the dust's size,
// +0x3c its alpha, +0x40 its spread, +0x44 .. +0x4c the turn toward a target, +0x50 .. +0x5c the lean, +0x60 / +0x70 /
// +0x80 the sparks' three records.

// The levels' own builds (transliterated: Aridia's are declared by no header of theirs)
TrRet lv2_25b710_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x0025b710
TrRet lv2_2618a8_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x002618a8
TrRet lv2_273628_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x00273628

namespace {
constexpr uint32_t HERO = 0x0013f350;        // the hero
constexpr uint32_t HERO_MOBY = 0x001413d0;   // the hero's moby
constexpr uint32_t SOUND_SLOTS = 0x0013e550; // the sound slots (0x70 bytes: +0x74 playing, +0x88 the owner)

// the level's tunings (their own data)
uint32_t sandmouse_tunings() {
    switch (ovl_reloc_level()) {
    case NOVALIS: return L1_ADDR(0x00162080);
    case ARIDIA: return L2_ADDR(0x001621c0);
    case KERWAN: return L3_ADDR(0x00161c88);
    case EUDORA: return L4_ADDR(0x00161d80);
    case RILGAR: return L5_ADDR(0x00161fd8);
    case BLARG: return L6_ADDR(0x00162380);
    case BATALIA: return L8_ADDR(0x00162418);
    case POKITARU: return L11_ADDR(0x00162458);
    case HOVEN: return L12_ADDR(0x00162040);
    default: return L14_ADDR(0x001622b0);
    }
}
// the level's walker record (its own data): +0 the step's limit, +0x14 the yaw's rate, +0x30 the drop, +0x50 the
// Sandmouse holding the hero
uint32_t walker_rec() {
    switch (ovl_reloc_level()) {
    case NOVALIS: return L1_ADDR(0x001deb38);
    case ARIDIA: return L2_ADDR(0x001d3110);
    case KERWAN: return L3_ADDR(0x001d2c10);
    case EUDORA: return L4_ADDR(0x001d2e10);
    case RILGAR: return L5_ADDR(0x001d2fb0);
    case BLARG: return L6_ADDR(0x001d48c0);
    case BATALIA: return L8_ADDR(0x001db300);
    case POKITARU: return L11_ADDR(0x001d3550);
    case HOVEN: return L12_ADDR(0x001f5b60);
    default: return L14_ADDR(0x001dfb50);
    }
}
// the level's copy of the glow's draw callback (sandmouse_glow; the levels still register their copies)
uint32_t glow_callback() {
    switch (ovl_reloc_level()) {
    case NOVALIS: return L1_ADDR(0x0030de68);
    case ARIDIA: return L2_ADDR(0x002f0eb0);
    case KERWAN: return L3_ADDR(0x002e1898);
    case EUDORA: return L4_ADDR(0x002e68f8);
    case RILGAR: return L5_ADDR(0x0031e730);
    case BLARG: return L6_ADDR(0x0030adb0);
    case BATALIA: return L8_ADDR(0x0030ab50);
    case POKITARU: return L11_ADDR(0x0031c728);
    case HOVEN: return L12_ADDR(0x0030a618);
    default: return L14_ADDR(0x00309bb0);
    }
}

using LevelFn = TrRet (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float,
                          float, float, float, float);
using LevelFn12 = TrRet (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float);

// the walker's step toward the goal (Veldin's doomlet_259028 0x259028, or the level's own build): the result's bits
// 2 / 4 a wall, a drop
uint32_t walker_step_here(uint32_t m, uint32_t rec, uint32_t goal, uint32_t step) {
    LevelFn f = nullptr;
    switch (ovl_reloc_level()) {
    case NOVALIS: return nov_26de80_l1(m, rec, goal, step, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f).v0;
    case ARIDIA: f = lv2_25b710_l2; break;
    case KERWAN: f = lv3_247c10_l3; break;
    case EUDORA: f = eud_24bdd8_l4; break;
    case RILGAR: f = ril_283308_l5; break;
    case BLARG: f = blg_doomlet_259028_u; break;
    default: return static_cast<uint32_t>(doomlet_259028(m, rec, goal, step));
    }
    return f(m, rec, goal, step, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0;
}
// the burrow's dent at `pos` (Veldin's metal_25f8e0, or the level's own build)
void dent_here(uint32_t pos, float size) {
    LevelFn f = nullptr;
    switch (ovl_reloc_level()) {
    case ARIDIA: f = lv2_2618a8_l2; break;
    case KERWAN: f = lv3_24e328_l3; break;
    case EUDORA: f = eud_252910_l4; break;
    case RILGAR: f = ril_289be8_l5; break;
    default: metal_25f8e0(pos, size); return;
    }
    f(pos, 0, 0, 0, 0, 0, 0, 0, size, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}
// the dust (particle 0x35 0x272060, each level's own build: Veldin's spawn_particle_35)
void dust_here(uint32_t pos, int32_t n, uint32_t colour, int32_t spin, uint32_t vel, float size, float grow, float spread) {
    LevelFn f = nullptr;
    switch (ovl_reloc_level()) {
    case NOVALIS:
        nov_287328_l1(pos, static_cast<uint32_t>(n), colour, 2, static_cast<uint32_t>(spin), vel, 0, 0, size, grow, spread, 0.0f);
        return;
    case ARIDIA: f = lv2_273628_l2; break;
    case KERWAN: f = lv3_260550_l3; break;
    case EUDORA: f = eud_264b28_l4; break;
    case RILGAR: f = ril_29c350_l5; break;
    case BLARG: f = blg_280ef0_l6; break;
    case BATALIA: f = bat_27c540_l8; break;
    case POKITARU: f = pok_297498_l11; break;
    case HOVEN: f = hov_28a9a0_l12; break;
    default: f = olt_279cf0_l14; break;
    }
    f(pos, static_cast<uint32_t>(n), colour, 2, static_cast<uint32_t>(spin), vel, 0, 0, size, grow, spread, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}
// the light on the floor under the moby (Novalis's own build, else Veldin's floor_light_add)
void floor_light_here(uint32_t m, float scale) {
    if (ovl_reloc_level() == NOVALIS) nov_26eec8_l1(m, 0, 0, 0, 0, 0, 0, 0, scale, 0.0f, 0.0f, 0.0f);
    else floor_light_add(m, scale);
}
// the hero's state set (Novalis's and Aridia's build of hero_set_state, else Veldin's)
void hero_state_here(uint32_t state, int32_t mode) {
    if (ovl_reloc_level() == NOVALIS || ovl_reloc_level() == ARIDIA) hero_set_state_l1(state, mode);
    else hero_set_state(state, mode);
}

// the quadword at `a` zeroed
void zero_quad(uint32_t a) {
    for (uint32_t k = 0; k < 0x10; k += 4) sw(a + k, 0);
}
// the moby of index `idx` in the moby list
uint32_t moby_at(uint32_t idx) { return lwu(OV(0x0015ffd8)) + (idx << 8); }
// the sound slot `slot`
uint32_t sound_slot(uint32_t slot) { return SOUND_SLOTS + slot * 0x70; }
// whether the Sandmouse's sound slot still plays its sound
bool own_sound_playing(uint32_t m, uint32_t pv) {
    const uint32_t e = sound_slot(lwu(pv + 0x64));
    return lwu(e + 0x88) == m && lbu(e + 0x74) != 0;
}
// its sound (1) kept playing
void keep_sound(uint32_t m, uint32_t pv) {
    if (own_sound_playing(m, pv)) return;
    sw(pv + 0x64, 0xffffffffu);
    sw(pv + 0x64, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
}
// its sound stopped
void stop_sound(uint32_t m, uint32_t pv) {
    const int32_t slot = lw(pv + 0x64);
    if (slot != -1 && own_sound_playing(m, pv)) sub_0028d918(slot);
    sw(pv + 0x64, 0xffffffffu);
}
// the turn toward `yaw` through the walker record's yaw rate (2 pi, pi a second's acceleration and damping, 660 degrees a
// second at most)
void turn_toward(uint32_t m, float yaw) {
    const float sq = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
    yaw_turn_damped(m, walker_rec() + 0x14, yaw, fpu_mul(sq, u2f(0x40c90fdbu)), fpu_mul(sq, u2f(0x40490fdbu)),
                    fpu_mul(dt, u2f(0x41384e88u)));  // (2 pi, pi, 11.519173)
}
// the heading from the moby to the point `p`
float heading_to(uint32_t m, uint32_t p) {
    return atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14)));
}
// the animation `set` unless it is the one set, over `frames` (60 Hz) frames
void anim_unless(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}
// the idle animation (1) at a random speed
void idle_anim(uint32_t m) {
    if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(rand_range(7, 0xc)));
    swc1(m + 0x58, rand_range_f(u2f(0x3f733333u), u2f(0x3f866666u)));  // (0.95 .. 1.05)
}
}  // namespace

// orig: L1:0x002e30b8 L1:0x0030d208 L2:0x002cbda0 L2:0x002f0250 L3:0x002e0c38 L4:0x002c6fc0 L4:0x002e5c98 L5:0x0031dad0 L6:0x0030a150 L8:0x00309ef0 L11:0x0031bac8 L12:0x003099b8 L14:0x00308f50
void point_ahead_of_hero(uint32_t, uint32_t out) {
    const uint32_t sp = ee_stack_alloc(0x20);
    vec3_set_length(V(sp), V(lwu(HERO_MOBY) + 0xd0), 1.0f);
    vec3_add(V(out), V(sp), V(HERO + 0x80));
    ee_stack_free(0x20);
}

// orig: L1:0x0030d260 L2:0x002f02a8 L3:0x002e0c90 L4:0x002e5cf0 L5:0x0031db28 L6:0x0030a1a8 L8:0x00309f48 L11:0x0031bb20 L12:0x00309a10 L14:0x00308fa8
void sandmouse_goal_by_hero(uint32_t, uint32_t pv) {
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t ahead = sp, side = sp + 0x10;
    vec3_set_length(V(ahead), V(lwu(HERO_MOBY) + 0xd0), u2f(0x3f8ccccdu));  // (1.1)
    vec3_set_length(V(side), V(lwu(HERO_MOBY) + 0xe0), 1.0f);
    vec3_add(V(pv), V(ahead), V(side));
    vec3_add(V(pv), V(pv), V(HERO + 0x80));
    swc1(pv + 8, fpu_add(lwc1(pv + 8), 1.0f));
    ee_stack_free(0x30);
}

// orig: L1:0x0030ded0 L2:0x002f0f18 L3:0x002e1900 L4:0x002e6960 L5:0x0031e798 L6:0x0030ae18 L8:0x0030abb8 L11:0x0031c790 L12:0x0030a680 L14:0x00309c18
void sandmouse_glow_points(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t k = 0; k < 2; ++k) moby_chain_point_v(m, 3 + k, V(pv + 0x70 + k * 0x10));
    sub_001f93e0(glow_callback(), m);
}

// orig: L1:0x0030dda0 L2:0x002f0de8 L3:0x002e17d0 L4:0x002e6830 L5:0x0031e668 L6:0x0030ace8 L8:0x0030aa88 L11:0x0031c660 L12:0x0030a550 L14:0x00309ae8
void sandmouse_sparks(uint32_t m, uint32_t) {
    if (lbu(m + 0x31) == 0) return;
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t at = sp, vel = sp + 0x10, fall = sp + 0x20;
    zero_quad(fall);
    swc1(fall + 8, -fpu_mul(rand_range_f(1.0f, 2.0f), lwc1(0x0015ed6c)));
    lq_sq(vel, fall);
    moby_chain_point_v(m, 0, V(at));
    const uint32_t t = sandmouse_tunings();
    for (uint32_t k = 0; k < 3; ++k) particle_4a_spawn(m, at, vel, t + 0x60 + k * 0x10, 1);
    ee_stack_free(0x40);
}

// orig: L1:0x0030c898 L2:0x002efdd0 L3:0x002e07b8 L4:0x002e5818 L5:0x0031d650 L6:0x003093e0 L8:0x00308810 L11:0x0031b648 L12:0x00309538 L14:0x00308ad0
uint32_t class1633_spawn(uint32_t m, uint32_t pos, float aim8, float aimc, float pitch, float yaw) {
    const uint32_t sp = ee_stack_alloc(0x80);
    lq_sq(sp, pos);
    const uint32_t s = moby_spawn(0x661);
    if (s != 0) {
        const uint32_t spv = lwu(s + 0x78);
        sw(spv + 0x10, m);
        sb(s + 0x30, 0xff);
        sh(s + 0x32, 0x7f);
        sb(s + 0x31, 1);
        swc1(s + 0x44, yaw);
        sd(s + 0x38, ld(m + 0x38));
        swc1(s + 0x48, pitch);
        sw(s + 0x40, 0);
        lq_sq(s + 0x10, sp);
        swc1(spv + 8, aim8);
        swc1(spv + 0xc, aimc);
        sw(spv, 0);
        sw(spv + 4, 0);
        const int32_t life = frame_scale_i(0x23);
        sh(spv + 0x16, static_cast<uint32_t>(life));
        sh(spv + 0x14, static_cast<uint32_t>(life));
        sb(s + 0x23, 0x40);
        sh(s + 0x34, lhu(s + 0x34) | 0xa00);
        swc1(s + 0x2c, fpu_mul(lwc1(s + 0x2c), 0.5f));
    }
    ee_stack_free(0x80);
    return s;
}

// orig: L1:0x0030d308 L2:0x002f0350 L3:0x002e0d38 L4:0x002e5d98 L5:0x0031dbd0 L6:0x0030a250 L8:0x00309ff0 L11:0x0031bbc8 L12:0x00309ab8 L14:0x00309050
uint32_t sandmouse_find_target(uint32_t m, uint32_t from, uint32_t angles, float yaw_reach, float pitch_reach, float range,
                               float near, float near_yaw, float near_pitch) {
    uint32_t best = 0;
    float best_score = 1000000000.0f;
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t aim = sp;
    for (uint32_t e = OV(0x001aba00);; e += 4) {
        const uint32_t o = lwu(e);
        if (o == 0) break;
        if (lh(o + 0x32) == 0) continue;
        const uint32_t word = moby_pvar_word0(o);
        const uint32_t cls = lwu(o + 0x24);
        if (cls == 0) continue;
        const int32_t kind = lh(cls + 0x46);
        if (kind != 5 || word == 0) continue;
        if (!(0.0f < lwc1(word))) continue;
        const float d = vec3_distance(V(from), V(o + 0x10));
        if (!(d < range)) continue;
        // the mark 0.4 above it: within the yaw reach, or near and within the near reach; the same for the pitch
        lq_sq(aim, o + 0x10);
        const float yaw = atan2_f(fpu_sub(lwc1(aim), lwc1(from)), fpu_sub(lwc1(aim + 4), lwc1(from + 4)));
        swc1(aim + 8, fpu_add(lwc1(aim + 8), u2f(0x3ecccccdu)));  // (0.4)
        const float dy = angle_dist(lwc1(angles + 8), yaw);
        float score = fpu_mul(dy, dy);
        if (!(score < fpu_mul(yaw_reach, yaw_reach))) {
            if (!(d < near) || !(score < fpu_mul(near_yaw, near_yaw))) continue;
        }
        const float pitch = atan2_f(d, fpu_sub(lwc1(aim + 8), lwc1(from + 8)));
        const float dp = angle_dist(lwc1(angles + 4), pitch);
        const float dp2 = fpu_mul(dp, dp);
        if (!(dp2 < fpu_mul(pitch_reach, pitch_reach))) {
            if (!(d < near) || !(dp2 < fpu_mul(near_pitch, near_pitch))) continue;
        }
        score = fpu_mul(score, dp2);
        score = fpu_mul(score, sqrt_f(vec3_distance(V(from), V(aim))));
        if (!(score < best_score)) continue;
        // seen: nothing between, or what is between is of its kind (not the hero)
        if (segment_collide_v(V(from), V(aim), 0, m, nullptr) != 0) {
            const uint32_t hit = lwu(OV(0x00173e58));
            if (hit == 0 || hit == lwu(HERO_MOBY)) continue;
            const uint32_t hcls = lwu(hit + 0x24);
            if (hcls == 0 || lh(hcls + 0x46) != kind) continue;
        }
        best_score = score;
        best = o;
    }
    ee_stack_free(0x100);
    return best;
}

// orig: L1:0x0030d5f0 L2:0x002f0638 L3:0x002e1020 L4:0x002e6080 L5:0x0031deb8 L6:0x0030a538 L8:0x0030a2d8 L11:0x0031beb0 L12:0x00309da0 L14:0x00309338
void sandmouse_shoot(uint32_t m, uint32_t pv) {
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x54))) == 0) return;
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t from = sp, at = sp + 0x10, ahead = sp + 0x20;
    const uint32_t t = sandmouse_tunings();
    moby_chain_point_v(m, (lbu(pv + 0x56) & 1) + 1, V(from));
    sb(pv + 0x56, lbu(pv + 0x56) ^ 1);
    const float degree = u2f(0x3c8efa35u);
    const float pi = u2f(0x40490fdbu);
    const uint32_t target = sandmouse_find_target(m, from, m + 0x40, fpu_mul(lwc1(t + 8), degree), fpu_mul(lwc1(t + 0xc), degree),
                                                  lwc1(t + 4), 2.0f, pi, pi);
    if (target == 0) {
        sw(pv + 0x60, 0);
    } else {
        sw(pv + 0x60, target);
        // it shoots at a target above the hero's feet, in front of it, but in the mode 2
        if (fpu_add(lwc1(HERO + 0x88), u2f(0x3f8ccccdu)) < lwc1(m + 0x18) &&  // (1.1)
            angle_dist(lwc1(m + 0x48), heading_to(m, target + 0x10)) < u2f(0x3f060a92u) &&  // (30 degrees)
            lw(OV(0x0015f5c4)) != 2) {
            lq_sq(at, target + 0x10);
            const uint32_t word = moby_pvar_word0(target);
            const float lift = word != 0 ? lwc1(word + 0x10) : lwc1(t + 0x14);
            swc1(at + 8, fpu_add(lwc1(at + 8), lift));
            vec3_sub(V(ahead), V(at), V(from));
            vec3_set_length(V(ahead), V(ahead), lwc1(t + 0x10));
            vec3_add(V(from), V(from), V(ahead));
            auto yaw = [from, at] { return atan2_f(fpu_sub(lwc1(at), lwc1(from)), fpu_sub(lwc1(at + 4), lwc1(from + 4))); };
            auto pitch = [from, at] { return -atan2_f(vec2_distance(V(from), V(at)), fpu_sub(lwc1(at + 8), lwc1(from + 8))); };
            const float aim8 = yaw(), aimc = pitch(), yaw2 = yaw(), pitch2 = pitch();
            class1633_spawn(m, from, aim8, aimc, yaw2, pitch2);
            moby_sound_start(0, 0, m);
            const uint32_t fired = lwu(pv + 0x5c);
            sh(pv + 0x54, static_cast<uint32_t>(frame_scale_i(lw(t))));
            sw(pv + 0x5c, fired + 1);
        }
    }
    ee_stack_free(0x90);
}

// orig: L1:0x0030d880 L2:0x002f08c8 L3:0x002e12b0 L4:0x002e6310 L5:0x0031e148 L6:0x0030a7c8 L8:0x0030a568 L11:0x0031c140 L12:0x0030a030 L14:0x003095c8
void sandmouse_move(uint32_t m, uint32_t pv) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t step = sp;
    const uint32_t rec = walker_rec(), t = sandmouse_tunings();
    const uint32_t pos = m + 0x10, vel = pv + 0x10;
    vec3_sub(V(step), V(pv), V(pos));
    // the distance from its home (while going home, state 6) or from the hero
    const float away = vec2_distance(V(pos), V(lbu(m + 0x20) == 6 ? pv + 0x30 : HERO + 0x80));
    const uint32_t mode = lbu(m + 0xbc);  // (0: turning toward the goal, 2: walking)
    const float far = u2f(0x3fa66666u);    // (1.3)
    if (mode == 2) {
        const uint32_t target = lwu(pv + 0x60);
        if (target == 0) {
            turn_toward(m, heading_to(m, pv));
        } else {
            const float degree = u2f(0x3c8efa35u);
            const float sq = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
            yaw_turn_damped(m, rec + 0x14, heading_to(m, target + 0x10), fpu_mul(fpu_mul(lwc1(t + 0x44), degree), sq),
                            fpu_mul(fpu_mul(lwc1(t + 0x48), degree), sq), fpu_mul(fpu_mul(lwc1(t + 0x4c), degree), dt));
        }
        // the step: the way to the goal (0.04 of it a frame), less the last step, eased (0.13 far, 0.25 near), the fall slowed
        vec3_scale(V(step), V(step), fpu_mul(lwc1(0x0015ed60), u2f(0x3d23d70au)));  // (0.04)
        vec3_sub(V(step), V(step), V(vel));
        const float ease = far < away ? u2f(0x3e051eb8u) : 0.25f;  // (0.13)
        vec3_scale(V(step), V(step), fpu_mul(lwc1(0x0015ed64), ease));
        swc1(step + 8, fpu_mul(lwc1(step + 8), fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbdccccd0u)), 1.0f)));  // (-0.1)
        vec3_add(V(vel), V(vel), V(step));
        if (lbu(m + 0x20) == 6) vec3_clamp_length_vu(V(vel), V(vel), fpu_mul(lwc1(0x0015ed6c), u2f(0x402ccccdu)));  // (2.7)
        const uint32_t moved = walker_move_fx(m, vel, lw(rec), 0, 1.0f, lwc1(rec + 0x30));
        const float speed = vec3_length(V(vel));
        // stuck (moving, slower than 0.005) for 20 frames: put at its goal (but while drawn)
        if (moved == 0) {
            sh(pv + 0x50, 0);
        } else if (speed < u2f(0x3ba3d70au)) {
            const uint32_t n = lhu(pv + 0x50) + 1;
            sh(pv + 0x50, n);
            if (frame_scale_i(0x14) < static_cast<int16_t>(n) && lbu(m + 0x31) == 0) {
                lq_sq(pos, pv);
                sh(pv + 0x50, 0);
            }
        }
        // pushed away from the hero within 1.1
        const float close = u2f(0x3f8ccccdu);
        if (away < close) {
            const float push = fpu_sub(close, away);
            const float a = atan2_f(fpu_sub(lwc1(HERO + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(HERO + 0x84), lwc1(m + 0x14)));
            const float k = u2f(0x3dcccccdu);  // (0.1)
            swc1(vel, fpu_sub(lwc1(vel), fpu_mul(fpu_mul(push, cos_vu(a)), fpu_mul(lwc1(0x0015ed64), k))));
            swc1(vel + 4, fpu_sub(lwc1(vel + 4), fpu_mul(fpu_mul(push, sin_vu(a)), fpu_mul(lwc1(0x0015ed64), k))));
        }
    } else {
        if (mode == 0) turn_toward(m, heading_to(m, pv));
        else sb(m + 0xbc, 0);
        if (far < away) sb(m + 0xbc, 2);
    }
    // the lean: toward the turn (the yaw's rate) and the speed
    {
        const float lean = angle_wrap_mod(fpu_mul(lwc1(rec + 0x14), lwc1(t + 0x50)));
        const float now = lwc1(m + 0x40);
        swc1(m + 0x40, fpu_add(now, fpu_mul(fpu_sub(lean, now), lwc1(t + 0x58))));
    }
    {
        const float pitch = angle_wrap_mod(fpu_mul(vec3_length(V(vel)), lwc1(t + 0x54)));
        const float now = lwc1(m + 0x44);
        swc1(m + 0x44, fpu_add(now, fpu_mul(fpu_sub(pitch, now), lwc1(t + 0x5c))));
    }
    ee_stack_free(0xb0);
}

// The update: 0 set up at its house (or gone if collected), 1 hidden, waiting for the hero in its zone, 2 coming out (its
// house opening), 3 running to the hero, 4 the hero taken (state 0 / 1 given back), 5 running around shooting at
// targets, 6 going back into its house, 7 in it again; the glow's points and the floor's light while out.
// orig: L1:0x0030df40 L2:0x002f0f88 L3:0x002e1970 L4:0x002e69d0 L5:0x0031e808 L6:0x0030ae88 L8:0x0030ac28 L11:0x0031c800 L12:0x0030a6f0 L14:0x00309c88
void sandmouse_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t rec = walker_rec(), t = sandmouse_tunings();
    auto house = [pv] { return moby_at(lwu(pv + 0x48)); };
    auto has_house = [pv] { return lw(pv + 0x48) != -1; };
    // the house turned toward the hero (while it waits) or toward the Sandmouse
    if (has_house()) {
        const uint32_t h = house();
        const uint32_t to = lbu(m + 0x20) < 5 ? HERO + 0x80 : m + 0x10;
        swc1(lwu(h + 0x78), atan2_f(fpu_sub(lwc1(to), lwc1(h + 0x10)), fpu_sub(lwc1(to + 4), lwc1(h + 0x14))));
        if (has_house()) sw(house() + 0x94, 0);
    }
    if (lbu(m + 0x20) >= 3) {
        if (lbu(0x0015edb0) != 0) {
            if (lbu(pv + 0x91) == 0) sub_0024f590(m, 5, pv + 0x90);
            const float big = u2f(0x40066666u);  // (2.1)
            swc1(pv + 0xb8, big);
            swc1(pv + 0xb0, big);
            swc1(pv + 0xb4, big);
        } else if (lbu(pv + 0x91) != 0) {
            sub_0024f608(m, pv + 0x90);
        }
        sandmouse_glow_points(m);
    }
    const float sq = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c), scale = lwc1(0x0015ed60);
    enum class Exit { Out, Shown, Gone };
    auto run = [&]() -> Exit {
        switch (lbu(m + 0x20)) {  // (the level's jump table of nine states)
        case 0: {  // (L2:0x002f10fc) set up at its house: gone if no house or once collected
            zero_quad(pv);
            zero_quad(pv + 0x10);
            sh(pv + 0x50, 0);
            sh(pv + 0x52, 0);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
            sb(m + 0x30, 0xff);
            sh(m + 0x32, 0xff);
            if (!has_house()) {
                debug_printf_stub();  // (the original's message: no house)
                return Exit::Gone;
            }
            lq_sq(m + 0x10, house() + 0x10);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), u2f(0x3ca3d70au)));  // (0.02)
            const int32_t idx = lh(m + 0xb2);
            const bool collected =
                lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0 ||
                ((lwu(0x0014c190 + (lwu(0x0015ed84) << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx & 0x1f)) & 1u) != 0;
            if (collected) {
                debug_printf_stub();  // (the original's message: collected)
                return Exit::Gone;
            }
            lq_sq(pv + 0x20, m + 0x10);
            lq_sq(pv + 0x30, m + 0x10);
            walker_record_init_l1(rec);
            const float pi = u2f(0x40490fdbu);
            const uint32_t flags = lwu(rec + 0x38) | 8;
            sw(rec, 0x1f4);
            swc1(rec + 0xc, u2f(0x3e19999au));                        // (0.15)
            swc1(rec + 0x14, fpu_mul(scale, u2f(0x3ecccccdu)));       // (0.4)
            swc1(rec + 0x24, fpu_mul(dt, u2f(0x402ccccdu)));          // (2.7)
            swc1(rec + 0x1c, fpu_mul(scale, u2f(0x3c23d70au)));       // (0.01)
            swc1(rec + 4, u2f(0x3e99999au));                          // (0.3)
            swc1(rec + 0x30, u2f(0x3ba3d70au));                       // (0.005)
            sw(rec + 0x38, flags);
            swc1(rec + 0x3c, fpu_mul(sq, u2f(0x40c90fdbu)));          // (2 pi)
            swc1(rec + 0x40, fpu_mul(sq, pi));
            swc1(rec + 0x44, fpu_mul(dt, u2f(0x41384e88u)));          // (11.519173)
            swc1(rec + 8, u2f(0x3e19999au));
            swc1(rec + 0x10, fpu_mul(scale, u2f(0x3c23d70au)));
            const int32_t limit = frame_scale_i(4);
            swc1(rec + 0x28, pi);
            sw(rec + 0x48, static_cast<uint32_t>(limit));
            sb(m + 0x20, 1);
            return Exit::Shown;
        }
        case 1: {  // (L2:0x002f1330) hidden: the hero in its zone, on foot (state 5), seen: it takes the hero
            if (lbu(0x0013d4c5) == 0) return Exit::Out;
            if (lw(HERO + 0x1158) != 5) return Exit::Out;
            sw(pv + 0x5c, 0);
            sw(pv + 0x58, static_cast<uint32_t>(frame_scale_i(lw(t + 0x18))));
            if (lbu(m + 0x31) != 0 && (lbu(m + 0x70) & 2) != 0) anim_unless(m, 2, 0xa);
            if (has_house()) sb(house() + 0xbc, point_in_box(HERO + 0x80, lw(pv + 0x40)) != 0 ? 3 : 1);
            if (lwu(HERO + 0x1130) == 0 || lw(HERO + 0x1158) != 5) return Exit::Out;
            turn_toward(m, heading_to(m, HERO + 0x80));
            const uint32_t sp = ee_stack_alloc(0x90);
            const uint32_t from = sp, to = sp + 0x10;
            lq_sq(from, house() + 0x10);
            swc1(from + 8, fpu_add(lwc1(from + 8), 0.5f));
            lq_sq(to, HERO + 0x80);
            swc1(to + 8, fpu_add(lwc1(to + 8), u2f(0x3e4ccccdu)));  // (0.2)
            bool seen = point_in_box(HERO + 0x80, lw(pv + 0x40)) != 0;
            if (seen && segment_collide_v(V(from), V(to), 0, m, nullptr) != 0) seen = lwu(OV(0x00173e58)) == lwu(HERO + 0x2080);
            ee_stack_free(0x90);
            if (!seen) return Exit::Out;
            point_ahead_of_hero(m, pv);
            const uint32_t held = lwu(HERO + 0x1130);
            if (held != 0 && lh(held + 0xa6) == 0x1b1) {
                const uint32_t st = lbu(held + 0x20);
                if (st != 0xfe && st != 0xfd) sb(held + 0x20, 1);
            }
            sw(rec + 0x50, m);
            hero_state_here(0x1f, 0);
            hero_anim_set(0, 0, static_cast<float>(frame_scale_i(6)));
            if (has_house()) {
                sb(house() + 0xbc, 5);
                sb(m + 0x20, 2);
            } else {
                sh(m + 0x32, 0xff);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
                sb(m + 0x20, 3);
            }
            return Exit::Out;
        }
        case 2:  // (L2:0x002f15f0) coming out once the house is open (its state 5)
            if (has_house()) {
                if (lbu(house() + 0x20) != 5) return Exit::Out;
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
                sb(m + 0x20, 3);
                sh(pv + 0x52, 0);
                sh(m + 0x32, 0xff);
            } else {
                sh(pv + 0x52, 0);
                sh(m + 0x32, 0xff);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
                if (lw(pv + 0x64) == -1) sw(pv + 0x64, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
                sb(m + 0x20, 3);
            }
            return Exit::Out;
        case 3: {  // (L2:0x002f16b8) running to the hero (from far: beside it), or walking to its goal near it
            keep_sound(m, pv);
            uint32_t blocked;
            if (u2f(0x3f19999au) < vec2_distance(V(m + 0x10), V(pv + 0x30)) && lh(pv + 0x52) == 0) {  // (0.6)
                swc1(rec + 4, u2f(0x3f333333u));  // (0.7)
                sandmouse_goal_by_hero(m, pv);
                sandmouse_move(m, pv);
                blocked = lh(pv + 0x50) < 21 ? 0u : 2u;
            } else {
                point_ahead_of_hero(m, pv);
                turn_toward(m, heading_to(m, pv));
                sw(pv + 0x18, 0);
                blocked = walker_step_here(m, rec, pv, pv + 0x10);
            }
            if ((blocked & 6) != 0 || vec3_distance(V(m + 0x10), V(pv)) < 0.25f) {
                anim_unless(m, 2, 0xa);
                zero_quad(pv + 0x10);
                sb(m + 0x20, 4);
                sh(pv + 0x52, static_cast<uint32_t>(frame_scale_i(0x78)));
            }
            if ((lbu(m + 0x70) & 2) != 0) idle_anim(m);
            return Exit::Out;
        }
        case 4:  // (L2:0x002f18f8) the hero let go, shown again
            keep_sound(m, pv);
            hero_state_here(0, 1);
            sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
            if (has_house()) sh(house() + 0x34, lhu(house() + 0x34) & 0xffbe);
            sb(m + 0x20, 5);
            return Exit::Out;
        case 5: {  // (L2:0x002f198c) out: shooting while it may; back home when its time or its shots run out or the hero leaves
            keep_sound(m, pv);
            const int32_t left = lh(pv + 0x52);
            if (left == frame_scale_i(0x14) || lw(t + 0x1c) - 5 < lw(pv + 0x5c)) {
                anim_unless(m, 2, 5);
            } else if (frame_scale_i(0x14) < left && (lbu(m + 0x70) & 2) != 0) {
                idle_anim(m);
            }
            bool stay = false;
            if (lwu(HERO + 0x1130) != 0 && lw(HERO + 0x1158) == 5) {
                const int32_t hs = lw(HERO + 0x208c);
                if (hs == 0x16) {
                    stay = false;
                } else if (hs != 0x14 && lbu(HERO + 0x20a4) == 0 && lh(lwu(OV(0x00166e00)) + 0x86) != 0x14 &&
                           timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x58))) == 0) {
                    stay = lw(pv + 0x5c) < lw(t + 0x1c);
                }
            }
            if (stay) {
                sandmouse_goal_by_hero(m, pv);
                sandmouse_move(m, pv);
            } else {
                zero_quad(pv + 0x10);
                sh(pv + 0x52, lhu(t + 0x24));
                stop_sound(m, pv);
                moby_sound_start(2, 0, m);
                sb(m + 0x20, 6);
                if (has_house()) sb(house() + 0xbc, 5);
            }
            sandmouse_shoot(m, pv);
            return Exit::Out;
        }
        case 6: {  // (L2:0x002f1be4) burrowing back: dust, spinning, fading; at the end at home (7)
            stop_sound(m, pv);
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x52))) != 0) {
                sb(m + 0x23, 0x80);
                lq_sq(m + 0x10, pv + 0x30);
                lq_sq(pv, pv + 0x30);
                zero_quad(pv + 0x10);
                sb(m + 0x20, 7);
                sh(m + 0x34, lhu(m + 0x34) & 0xfdff);
                return Exit::Out;
            }
            const uint32_t sp = ee_stack_alloc(0x40);
            const uint32_t at = sp, still = sp + 0x10;
            zero_quad(still);
            lq_sq(at, m + 0x10);
            dent_here(at, lwc1(t + 0x30));
            const uint32_t red = static_cast<uint32_t>(libc_rand() + 0x30) & 0x3f;
            const uint32_t green = static_cast<uint32_t>(libc_rand() + 0x20) & 0x3f;
            const uint32_t blue = static_cast<uint32_t>(libc_rand()) & 0x2f;
            const uint32_t colour = red | (green << 8) | (blue << 16) | (lwu(t + 0x3c) << 24);
            const int32_t coin = libc_rand();
            int32_t spin = coin % 2;
            if ((lhu(pv + 0x52) & 1) != 0) spin = -spin;
            const int32_t n = rand_range(lw(t + 0x28), lw(t + 0x2c));
            const float size = rand_range_f(lwc1(t + 0x34), lwc1(t + 0x38));
            dust_here(at, n, colour, spin, still, size, u2f(0x3a83126fu), fpu_mul(lwc1(t + 0x40), sq));  // (0.001)
            ee_stack_free(0x40);
            const int32_t out = lw(t + 0x24);
            sh(m + 0x34, lhu(m + 0x34) | 0x200);
            sb(m + 0x23, lbu(m + 0x23) - static_cast<uint32_t>(ee_div(0x80, out)));
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(dt, u2f(0x41c90fdbu))));  // (8 pi)
            return Exit::Out;
        }
        case 7:  // (L2:0x002f1db0) home again, turned to face its house: hidden (1)
            stop_sound(m, pv);
            zero_quad(pv + 0x10);
            lq_sq(pv, house() + 0x10);
            sw(rec + 0x50, 0);
            turn_toward(m, heading_to(m, pv));
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(6));
            sb(house() + 0xbc, 1);
            sb(m + 0x20, 1);
            return Exit::Shown;
        default:  // (L2:0x002f1ee4: 8 and past the table)
            return Exit::Out;
        }
    };
    const Exit exit = run();
    if (exit == Exit::Gone) {
        moby_retire(m);
        return;
    }
    if (exit == Exit::Shown) sh(m + 0x32, 0);
    if (lbu(m + 0x20) >= 2) {
        floor_light_here(m, u2f(0x3e99999au));  // (0.3)
        sandmouse_sparks(m, pv);
    }
    if (has_house()) sw(house() + 0x94, lwu(lwu(house() + 0x24) + 0x10));
}

// The level pieces class 437 (shared_class437.cpp), an older build of the Sandmouse, shares with it: the same dispatch.
// orig: (host) the level's walker record
uint32_t sandmouse_walker_rec() { return walker_rec(); }
// orig: (host) the level's build of the walker's step (doomlet_259028)
uint32_t sandmouse_walker_step(uint32_t m, uint32_t rec, uint32_t goal, uint32_t step) { return walker_step_here(m, rec, goal, step); }
// orig: (host) the level's build of the burrow's dent (metal_25f8e0)
void sandmouse_dent(uint32_t pos, float size) { dent_here(pos, size); }
// orig: (host) the level's build of particle 0x35's spawn
void sandmouse_dust(uint32_t pos, int32_t n, uint32_t colour, int32_t spin, uint32_t vel, float size, float grow, float spread) {
    dust_here(pos, n, colour, spin, vel, size, grow, spread);
}
// orig: (host) the level's build of floor_light_add
void sandmouse_floor_light(uint32_t m, float scale) { floor_light_here(m, scale); }
// orig: (host) the level's build of hero_set_state
void sandmouse_hero_state(uint32_t state, int32_t mode) { hero_state_here(state, mode); }

}  // namespace rac1
