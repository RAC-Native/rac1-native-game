// Veldin's finale (level 18) class 1381: a raft that carries the platforms (class 1584) of its moby group. Set up (state
// 0) it keeps where it stands and how it is turned, takes up to sixteen class 1584 mobys of its group (+0x21) as its
// platforms (attach_bind) and waits (state 1). Started by the level (state 2) it glides, while the level's word
// L18:0x162288 is set, from where it stood to the box its +4 names (its progress +0x330 sought to 1.5, then to 2: the
// way is half of it), carrying its platforms; past the level's L18:0x1622b8 of its way it starts dropping rocks (class
// 696) about the hero, ever less often; past L18:0x1622b4, with its +8 set, it sinks (state 4). Once the hero has ridden
// it (stood on one of its platforms; fix 45 on the PAL 2.00 disc's build: for more than half a second) and is off it on
// the ground (on no class 1892 moby), it sinks when its wait +0x34c runs out. Sinking it falls, and below height 20 its
// platforms and it are gone. Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base
// ELF's bare. Registered in veldin_finale_mobys.cpp; the helper's declaration in veldin_finale_own.h.
//
// The pvars: +4 the box it glides to, +8 it sinks at the end, +0x10 / +0x20 where it stood and how it was turned, +0x30 its
// platforms' records (0x30 bytes each; the platform's moby index at +0x20 of its record), +0x330 its progress, +0x334 its
// speed, +0x338 its platforms, +0x340 its fall's speed, +0x344 the odds of a rock, +0x348 ridden, +0x34c its wait,
// +0x350 the frames ridden (fix 45).
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_attach.h>
#include <game/ovl/shared_rock.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
constexpr uint32_t HERO = 0x0013f350;       // the hero (+0x80 his position, +0x2fc the moby he stands on, +0x30e in the air)
constexpr int32_t PLATFORM_CLASS = 0x630;   // (1584)
constexpr int32_t GROUND_CLASS = 0x764;     // (1892)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

uint32_t first_moby() { return lwu(L18_ADDR(0x0015ffd8)); }
// the raft's platform `k`'s moby
uint32_t platform(uint32_t pv, uint32_t k) { return first_moby() + (lwu(pv + 0x50 + k * 0x30) << 8); }

// the set-up: its place and turn kept, its group's class 1584 mobys (sixteen at most) bound as its platforms; false
// when the group has no list
bool set_up(uint32_t m, uint32_t pv) {
    lq_sq(pv + 0x10, m + 0x10);
    lq_sq(pv + 0x20, m + 0x40);
    uint32_t e = lvd::group(lbu(m + 0x21));
    if (e == 0) return false;
    uint32_t n = 0;
    for (;; e += 2) {
        if (!(n < 16)) break;
        const uint32_t index = lhu(e) & 0x7fff;
        if (lh(first_moby() + (index << 8) + 0xa6) == PLATFORM_CLASS) {
            sw(pv + 0x50 + n * 0x30, index);
            ++n;
        }
        if (lh(e) < 0) break;
    }
    sw(pv + 0x348, 0);
    const bool fix45 = fixes102_finale_raft_ride();
    sw(pv + 0x34c, static_cast<uint32_t>(frame_scale_i(fix45 ? 5 : 0x1e)));
    if (fix45) sw(pv + 0x350, 0);
    sw(pv + 0x338, n);
    attach_bind(m, pv + 0x30, static_cast<int32_t>(n));
    sb(m + 0x20, 1);
    return true;
}

// the hero on the raft: whether he stands on one of its platforms (then ridden: at once, or with fix 45 past
// frame_scale_i(30) frames of it) or on a class 1892 moby (or, fix 45, on a sinking raft of its class); ridden, off it and
// on the ground, the raft sinks when its wait runs out
void ride(uint32_t m, uint32_t pv) {
    const bool fix45 = fixes102_finale_raft_ride();
    const uint32_t n = lwu(pv + 0x338);
    bool held = false;
    for (uint32_t k = 0; static_cast<int32_t>(k) < static_cast<int32_t>(n); ++k) {
        if (lwu(HERO + 0x2fc) != platform(pv, k)) continue;
        if (fix45) {
            sw(pv + 0x350, lwu(pv + 0x350) + 1);
            if (frame_scale_i(0x1e) < lw(pv + 0x350)) sw(pv + 0x348, 1);
        } else {
            sw(pv + 0x348, 1);
        }
        held = true;
        break;
    }
    const uint32_t under = lwu(HERO + 0x2fc);
    if (under != 0) {
        if (lh(under + 0xa6) == GROUND_CLASS) held = true;
        if (fix45 && lh(under + 0xa6) == lh(m + 0xa6) && lbu(under + 0x20) == 4) held = true;
    }
    if (lw(pv + 0x348) == 0 || held || lh(HERO + 0x30e) != 0) return;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x34c))) != 0) sb(m + 0x20, 4);
}

// a rock dropped 15 above a point up to 20 from the hero, within 60 degrees of the camera's heading
void drop_rock(uint32_t at) {
    const float a = angle_add(rand_range_f(u2f(0xbf860a92u), u2f(0x3f860a92u)), lwc1(L18_ADDR(0x001677d8)));
    const float r = rand_range_f(0.0f, 20.0f);
    swc1(at + 0, fpu_mul(cos_vu(a), r));
    const float y = fpu_mul(sin_vu(a), r);
    swc1(at + 8, 15.0f);
    swc1(at + 4, y);
    vec3_add(V(at), V(at), V(HERO + 0x80));
    rock_spawn(at, L18_ADDR(0x0015f580), 0x2b8, frame_scale_i(0xf0), 0, lwc1(L18_ADDR(0x001622c0)), 2.0f, 1.0f, 0.75f);
}
}  // namespace

// A platform of the raft gone (a class 1584 moby only), with the moby its +0x84 names.
// orig: L18:0x002fa8e0
void vf_raft_platform_retire_l18(uint32_t o) {
    if (lh(o + 0xa6) != PLATFORM_CLASS) return;
    const uint32_t other = lwu(lwu(o + 0x78) + 0x84);
    if (other != 0) moby_retire(other);
    moby_retire(o);
}

// orig: L18:0x002f16f0
void vf_class1381_update_l18(uint32_t m) {
    const Frame f(112);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    if (pv == 0) return;
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002f1790)
        if (!set_up(m, pv)) return;
        break;
    case 2: {  // gliding (0x002f1850)
        if (lw(L18_ADDR(0x00162288)) != 0) {
            if (lwc1(pv + 0x330) < 1.5f)
                seek_1d(pv + 0x330, pv + 0x334, 1.5f, fpu_mul(lwc1(L18_ADDR(0x00162290)), dt_sq()),
                        fpu_mul(lwc1(L18_ADDR(0x00162294)), dt_sq()), fpu_mul(lwc1(L18_ADDR(0x00162298)), dt()));
            else
                seek_1d(pv + 0x330, pv + 0x334, 2.0f, fpu_mul(lwc1(L18_ADDR(0x0016229c)), dt_sq()),
                        fpu_mul(lwc1(L18_ADDR(0x001622a0)), dt_sq()), fpu_mul(lwc1(L18_ADDR(0x001622a4)), dt()));
        }
        if (lwc1(L18_ADDR(0x001622b8)) < lwc1(pv + 0x330) && lwc1(pv + 0x344) == 0.0f)  // the rocks start
            swc1(pv + 0x344, static_cast<float>(lw(L18_ADDR(0x001622bc))));
        if (lwc1(L18_ADDR(0x001622b4)) < lwc1(pv + 0x330) && lw(pv + 8) != 0) sb(m + 0x20, 4);
        ride(m, pv);
        break;
    }
    case 4: {  // sinking (0x002f19e8)
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0x340)));
        swc1(pv + 0x340, fpu_sub(lwc1(pv + 0x340), fpu_mul(dt_sq(), 5.0f)));
        if (!(lwc1(m + 0x18) < 20.0f)) break;
        for (uint32_t k = 0; static_cast<int32_t>(k) < lw(pv + 0x338); ++k) vf_raft_platform_retire_l18(platform(pv, k));
        moby_retire(m);
        return;
    }
    default:
        break;
    }
    // (0x002f1a80) gliding or waiting: put along its way; its platforms carried; the rocks
    const uint32_t st = lbu(m + 0x20);
    if (st - 1 < 2) {
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 4));
        vec3_lerp(V(m + 0x10), V(pv + 0x10), V(box + 0x30), fpu_mul(lwc1(pv + 0x330), 0.5f));
        vec3_lerp(V(m + 0x40), V(pv + 0x20), V(box + 0x70), fpu_mul(lwc1(pv + 0x330), 0.5f));
    }
    attach_carry(m, pv + 0x30, lw(pv + 0x338));
    const float odds = lwc1(pv + 0x344);
    if (odds == 0.0f || lbu(m + 0x20) == 4) return;
    swc1(pv + 0x344, fpu_add(odds, 0.25f));
    if (rand_mod(float_to_int(lwc1(pv + 0x344))) != 0) return;
    drop_rock(sp);
}

}  // namespace rac1
