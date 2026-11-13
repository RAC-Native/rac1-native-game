// The swingshot's targets (classes 758 and 803) and the follow camera's look toward them: one port of the copies sixteen
// levels carry (Aridia to the finale's Veldin, but Novalis and Batalia). The copies are the same code relinked (the
// normalised texts compared; Eudora's and Rilgar's update differ only in how the generator wrote the colour call). Their
// addresses are the engine's (OV: the moby table 0x15ffd8, the frame counter 0x15f5cc, the camera 0x166c80.. and its
// active record 0x166e00, the look-height block 0x167000) or the base ELF's (bare: the hero 0x13f350, the right stick
// 0x13ca44, the level number 0x15ed84 and the per-level flags 0x14c050, the frame time 0x15ed6c and its square 0x15ed70);
// the levels' own words, per level below, are the sparkle's four tunings, the look's step (the callback's) and the address
// of the level's hook. Header shared_swingshot_target.h.
//
// The target's pvars: its flag in the level's flag table +0x30 (-1 none), its turn speed +0x38, the moby it waits for +0x3c
// (an index, -1 none).
#include <game/ovl/shared_swingshot_target.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;      // the hero (+0x80 his position, +0x2084 his state)
constexpr uint32_t HERO_POS = 0x0013f3d0;  // his position
constexpr uint32_t CLASS_803 = 0x323;

// The level's sparkle tunings of class 803 (its own data, four words): its life in frames +0 (a float), its colour +4,
// its size +8, its distance from the target +0xc.
uint32_t sparkle(uint32_t o) {
    switch (ovl_reloc_level()) {
    case ARIDIA: return L2_ADDR(0x00161cb0) + o;
    case KERWAN: return L3_ADDR(0x001619a4) + o;
    case EUDORA: return L4_ADDR(0x00161b24) + o;
    case RILGAR: return L5_ADDR(0x00161b84) + o;
    case BLARG: return L6_ADDR(0x00161aa4) + o;
    case UMBRIS: return L7_ADDR(0x00161944) + o;
    case GASPAR: return L9_ADDR(0x00161a1c) + o;
    case ORXON: return L10_ADDR(0x00161990) + o;
    case POKITARU: return L11_ADDR(0x00161c00) + o;
    case HOVEN: return L12_ADDR(0x00161b28) + o;
    case GEMLIK: return L13_ADDR(0x00161b64) + o;
    case OLTANIS: return L14_ADDR(0x00161d1c) + o;
    case QUARTU: return L15_ADDR(0x00161cec) + o;
    case KALEBO: return L16_ADDR(0x00161a90) + o;
    case VELDIN_ORBIT: return L17_ADDR(0x00161bd4) + o;
    default: return L18_ADDR(0x00161d18) + o;
    }
}
// The level's float the look height grows by (the same word the look-height callback counts it down by)
uint32_t look_step() {
    switch (ovl_reloc_level()) {
    case ARIDIA: return L2_ADDR(0x0016238c);
    case KERWAN: return L3_ADDR(0x00161e54);
    case EUDORA: return L4_ADDR(0x00161f4c);
    case RILGAR: return L5_ADDR(0x001621a4);
    case BLARG: return L6_ADDR(0x0016256c);
    case UMBRIS: return L7_ADDR(0x00161e24);
    case GASPAR: return L9_ADDR(0x00161f3c);
    case ORXON: return L10_ADDR(0x0016220c);
    case POKITARU: return L11_ADDR(0x00162774);
    case HOVEN: return L12_ADDR(0x0016224c);
    case GEMLIK: return L13_ADDR(0x001620c4);
    case OLTANIS: return L14_ADDR(0x0016247c);
    case QUARTU: return L15_ADDR(0x001623cc);
    case KALEBO: return L16_ADDR(0x00162194);
    case VELDIN_ORBIT: return L17_ADDR(0x0016267c);
    default: return L18_ADDR(0x00162794);
    }
}
// The level's look-height hook (its copy of cam0_level_hook, kerwan_camera.cpp), the address the install stores
uint32_t level_hook() {
    switch (ovl_reloc_level()) {
    case ARIDIA: return L2_ADDR(0x002fa710);
    case KERWAN: return L3_ADDR(0x002eb408);
    case EUDORA: return L4_ADDR(0x002f0448);
    case RILGAR: return L5_ADDR(0x00328f80);
    case BLARG: return L6_ADDR(0x003147b0);
    case UMBRIS: return L7_ADDR(0x00328090);
    case GASPAR: return L9_ADDR(0x00314220);
    case ORXON: return L10_ADDR(0x002f4b18);
    case POKITARU: return L11_ADDR(0x00327a90);
    case HOVEN: return L12_ADDR(0x003148f0);
    case GEMLIK: return L13_ADDR(0x00316b48);
    case OLTANIS: return L14_ADDR(0x003131e0);
    case QUARTU: return L15_ADDR(0x002f6980);
    case KALEBO: return L16_ADDR(0x002f3a88);
    case VELDIN_ORBIT: return L17_ADDR(0x002fff38);
    default: return L18_ADDR(0x00305318);
    }
}

// the moby the target waits for (its index +0x3c) is gone: none named, none, or retired (state 0xfe / 0xfd)
enum class Wait { None, Gone, Alive };
Wait waited(uint32_t pv) {
    const int32_t idx = lw(pv + 0x3c);
    if (idx == -1) return Wait::None;
    const uint32_t o = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
    if (o == 0) return Wait::Gone;
    const uint32_t st = lbu(o + 0x20);
    return st == 0xfe || st == 0xfd ? Wait::Gone : Wait::Alive;
}
}  // namespace

// orig: L2:0x002fa6d8 L3:0x002eb3d0 L4:0x002f0410 L5:0x00328f48 L6:0x00314778 L7:0x00328058 L9:0x003141e8 L10:0x002f4ae0 L11:0x00327a58 L12:0x003148b8 L13:0x00316b10 L14:0x003131a8 L15:0x002f6948 L16:0x002f3a50 L17:0x002fff00 L18:0x003052e0
void cam0_level_hook_install() {
    sw(OV(0x00167000), 0);
    sw(OV(0x00167010), level_hook());
    swc1(OV(0x00167008), 10000.0f);
    sw(OV(0x00167004), 0);
    sw(OV(0x0016700c), 0);
}

// With the right stick at rest and the active camera (*0x166e00) not in a scripted mode (+0x86), a target within 22 units
// of the hero, above him, nearer than the block's best so far (+8), within 64 degrees of the camera's forward from the hero
// and within 36 degrees of it from the camera, becomes the block's target (+4, the camera +0, its distance from the camera
// +8), and the look height +0xc grows by the level's step (up to 1).
// orig: L2:0x002fa7c8 L3:0x002eb4c0 L4:0x002f0500 L5:0x00329038 L6:0x00314868 L7:0x00328148 L9:0x003142d8 L10:0x002f4bd0 L11:0x00327b48 L12:0x003149a8 L13:0x00316c00 L14:0x00313298 L15:0x002f6a38 L16:0x002f3b40 L17:0x002ffff0 L18:0x003053d0
void swingshot_target_look(uint32_t m) {
    const uint32_t cam = lwu(OV(0x00166e00));
    if (lh(cam + 0x86) != 0 || u2f(0x3d4ccccdu) < abs_f(lwc1(0x0013ca44))) return;
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t rel = sp;
    const uint32_t block = OV(0x00167000);
    const float right_angle = u2f(0x3fc90fdbu);
    vec3_sub(V(rel), V(m + 0x10), V(HERO_POS));
    float d = vec3_length(V(rel));
    if (d < 22.0f && 0.0f < d && d < lwc1(block + 8) && 0.0f < lwc1(rel + 8) &&
        fpu_sub(right_angle, asin_f(fpu_div(vec3_dot(V(cam), V(rel)), d))) < u2f(0x3f8efa35u)) {
        vec3_sub(V(rel), V(m + 0x10), V(cam + 0x30));
        d = vec3_length(V(rel));
        if (d == 0.0f) d = u2f(0x38d1b717u);
        if (fpu_sub(right_angle, asin_f(fpu_div(vec3_dot(V(cam), V(rel)), d))) < u2f(0x3f20d97cu)) {
            const float look = fpu_add(lwc1(block + 0xc), lwc1(look_step()));
            sw(block, cam);
            swc1(block + 8, d);
            sw(block + 4, m);
            swc1(block + 0xc, look);
            if (1.0f < look) swc1(block + 0xc, 1.0f);
        }
    }
    ee_stack_free(0x90);
}

// State 0: with a flag in the level's table (+0x30) it hides (2); else, unless the moby it waits for is alive (then it
// hides too), it lights up (grey 0xc0), resets the camera's look block and is shown (1). State 1: class 803 turns toward
// the hero (a quarter of a turn a second at most, 4 pi a second squared) and, on every odd frame while shown, sparkles on
// one side or the other (every fourth frame); unless the hero is swinging (state 0x2c), the camera's look. State 2: when
// its flag is set in the table of this level, or the moby it waits for is gone, it shows again (state 0).
// orig: L2:0x002e0450 L3:0x002d0bd8 L4:0x002d9a58 L5:0x003098d8 L6:0x002e6270 L7:0x00302238 L9:0x002f8c70 L10:0x002c9590 L11:0x00300010 L12:0x002f7b28 L13:0x002fa708 L14:0x002f2cb8 L15:0x002db100 L16:0x002d6378 L17:0x002d95a8 L18:0x002e19d8
void swingshot_target_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {
        const Wait w = lw(pv + 0x30) != -1 ? Wait::Alive : waited(pv);
        if (w == Wait::Alive) {
            sb(m + 0x30, 0xff);
            sb(m + 0x20, 2);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            return;
        }
        moby_set_color_hi(m, 0xc0, 0xc0, 0xc0);
        cam0_level_hook_install();
        sb(m + 0x20, 1);
        return;
    }
    case 1: {
        if (lh(m + 0xa6) == static_cast<int32_t>(CLASS_803)) {
            const float yaw = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(m + 0x14)));
            const float accel = fpu_mul(lwc1(0x0015ed70), u2f(0x40c90fdbu));
            yaw_seek(m + 0x48, pv + 0x38, yaw, accel, accel, fpu_mul(lwc1(0x0015ed6c), u2f(0x41490fdbu)));
            const uint32_t frame = lwu(OV(0x0015f5cc));
            if ((frame & 1u) != 0 && lbu(m + 0x31) != 0) {
                const uint32_t sp = ee_stack_alloc(0x70);
                const uint32_t at = sp, vel = sp + 0x10;
                const float side = (frame & 4u) != 0 ? -1.0f : 1.0f;
                vec3_set_length(V(at), V(m + 0xd0), fpu_mul(side, lwc1(sparkle(0xc))));
                vec3_set_length(V(vel), V(at), fpu_div(-2.0f, frame_scale_f(lwc1(sparkle(0)))));
                vec3_add(V(at), V(at), V(m + 0x10));
                const uint32_t life = static_cast<uint32_t>(float_to_int(frame_scale_f(lwc1(sparkle(0))))) & 0xffu;
                const uint32_t start = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
                spawn_particle_3c(at, vel, lwu(sparkle(4)), life, start, 0, lwc1(sparkle(8)));
                ee_stack_free(0x70);
            }
        }
        if (lw(HERO + 0x2084) != 0x2c) swingshot_target_look(m);
        return;
    }
    case 2: {
        const uint32_t flags = 0x0014c050 + lwu(pv + 0x30) + (lwu(0x0015ed84) << 4);
        if (lbu(flags) == 0 && waited(pv) != Wait::Gone) return;
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sw(pv + 0x30, 0xffffffffu);
        sb(m + 0x20, 0);
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
