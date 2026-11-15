// Gaspar's own moby classes: the platforms floating on the lava that tilt and sink under the hero (classes 664 and 1293) —
// hand-ported from Gaspar's own overlay. Header gaspar_mobys.h, registered in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`);
// Gaspar-only data is L9_ADDR, the base ELF's are bare (the hero 0x13f350: his place +0x80, his push +0xf0, the moby he
// stands on +0x2fc, his air flag +0x30e; the frame time 0x15ed6c / its square 0x15ed70, the time scale 0x15ed64).
//
// The pvars: the attach frame +0x20, the tilt +0x60 / +0x64 and its rates +0x70 / +0x74, set up +0x80, its rest height +0x84,
// the timer since the hero stepped on +0x88, its two bobs +0x90 .. +0x9c. Gaspar's tuning in its data 0x1619f4 .. 0x161a18.
#include <game/math/vecmath.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/shared_small.h>

namespace rac1 {

namespace {
// a stack frame of EE memory for the vectors handed to the engine by address (cleared when it goes)
struct Frame {
    uint32_t sp, n;
    explicit Frame(uint32_t size) : sp(ee_stack_alloc(size)), n(size) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

constexpr uint32_t HERO = 0x0013f350;
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
const float kDegree = u2f(0x3c8efa35u);

// the tilt's two springs toward (x, y) with the spring constants of Gaspar's words `k`, `d`, `v` (degrees for k and v)
void tilt(uint32_t pv, float x, float y, uint32_t k, uint32_t d, uint32_t v) {
    auto spring = [](uint32_t ang, uint32_t vel, float target, uint32_t kw, uint32_t dw, uint32_t vw) {
        const float kk = fpu_mul(fpu_mul(lwc1(kw), kDegree), dt_sq());
        const float dd = fpu_mul(lwc1(dw), lwc1(0x0015ed64));
        const float vv = fpu_mul(fpu_mul(lwc1(vw), kDegree), dt());
        spring_angle(ang, vel, 0, target, kk, dd, vv);
    };
    spring(pv + 0x60, pv + 0x70, x, k, d, v);
    spring(pv + 0x64, pv + 0x74, y, k, d, v);
}
}  // namespace

// Class 664. Set up once (its tilt from its turn, 0.3 lower, that its rest height). The hero standing on it (not in the air)
// starts its timer (30 frames at the least; its sound when it was off). While the timer runs it tilts toward where the hero
// stands on it, sinks (its sound when it passes a quarter below its rest), and carries the hero (his push by its turn and its
// fall). Else it levels and rises back to its rest (its sound when it passes the quarter going up). Always: two bobs, its turn
// from the tilt, its attach frame.
// orig: L9:0x002f86a0
void gas_class664_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x60);
    const uint32_t sp = f.sp, delta = sp, old_ang = sp + 0x10;
    vec3_scale(V(delta), V(m + 0x10), -1.0f);
    lq_sq(old_ang, m + 0x40);
    const float quarter = fpu_sub(lwc1(pv + 0x84), 0.25f);  // (from the rest height before the set-up)
    const float old_z = lwc1(m + 0x18);
    if (lw(pv + 0x80) == 0) {
        lq_sq(pv + 0x60, m + 0x40);
        V(pv + 0x70) = Vec4{};
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), u2f(0x3e99999au)));
        sw(pv + 0x80, 1);
        swc1(pv + 0x84, lwc1(m + 0x18));
    }
    if (lh(HERO + 0x30e) == 0 && lwu(HERO + 0x2fc) == m) {  // the hero on it
        if (lw(pv + 0x88) == 0) moby_sound_start(0, 0, m);
        if (lw(pv + 0x88) < frame_scale_i(30)) sw(pv + 0x88, static_cast<uint32_t>(frame_scale_i(30)));
    }
    if (lw(pv + 0x88) != 0) {
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x88)));
        vec3_sub(V(sp + 0x20), V(HERO + 0x80), V(m + 0x10));
        const float along = vec3_dot(V(sp + 0x20), V(m + 0xc0));
        const float across = vec3_dot(V(sp + 0x20), V(m + 0xd0));
        const float k = lwc1(L9_ADDR(0x001619f4));
        lq_sq(sp + 0x30, pv + 0x60);  // (the tilt before, and its place)
        lq_sq(sp + 0x40, m + 0x10);
        tilt(pv, fpu_mul(-k, across), fpu_mul(k, along), L9_ADDR(0x00161a04), L9_ADDR(0x00161a08), L9_ADDR(0x00161a0c));
        const float z = fpu_sub(lwc1(m + 0x18), fpu_mul(lwc1(L9_ADDR(0x001619f8)), dt()));
        swc1(m + 0x18, z);
        if (quarter < old_z && z <= quarter) moby_sound_start(1, 0, m);
        // the hero carried: his place turned with the tilt, less where he is; plus its fall
        point_turned(HERO + 0x80, sp + 0x50, m + 0x10, sp + 0x30, pv + 0x60);
        vec3_sub(V(HERO + 0xf0), V(sp + 0x50), V(HERO + 0x80));
        swc1(HERO + 0xf8, fpu_add(lwc1(HERO + 0xf8), fpu_sub(lwc1(m + 0x18), lwc1(sp + 0x48))));
    } else {
        tilt(pv, 0.0f, 0.0f, L9_ADDR(0x00161a10), L9_ADDR(0x00161a14), L9_ADDR(0x00161a18));
        const float z = lwc1(m + 0x18);
        const float rise = fpu_mul(fpu_mul(lwc1(L9_ADDR(0x001619fc)), dt()), fpu_sub(lwc1(pv + 0x84), z));
        swc1(m + 0x18, fpu_add(z, rise));
        if (lwc1(pv + 0x84) < lwc1(m + 0x18)) swc1(m + 0x18, lwc1(pv + 0x84));
        if (old_z < quarter && quarter <= lwc1(m + 0x18)) moby_sound_start(2, 0, m);
    }
    sub_00262b00(m, pv + 0x98, pv + 0x9c, u2f(0x3e0f5c29u), fpu_mul(dt(), u2f(0x3f060a92u)));
    sub_00262b80(m, pv + 0x90, pv + 0x94, u2f(0x3db2b8c2u), fpu_mul(dt(), u2f(0x3ea0d97cu)), fpu_mul(dt(), u2f(0x3efa35ddu)));
    swc1(m + 0x40, angle_add(lwc1(pv + 0x60), lwc1(m + 0x40)));
    swc1(m + 0x44, angle_add(lwc1(pv + 0x64), lwc1(m + 0x44)));
    vec3_add(V(delta), V(delta), V(m + 0x10));
    attach_frame_set(pv + 0x20, delta, old_ang, m + 0x40);
}

// Class 1293, a platform that comes up from the lava once its moby (pvars +0x8c, by its index in the level's moby table) is gone,
// then floats like class 664 (Gaspar's tuning 0x161dc4 .. 0x161de8). 0: its rest height (+0x84), its tilt from its turn; with
// no moby (-1) floating at once (2, the original's debug note left out), else 5 below in a random turn, waiting (1). 1: its moby
// gone (or being removed), floating (2). 2: the hero standing on it (not in the air) starts its timer (30 frames at the least,
// its sound when it was off); while it runs it tilts toward where he stands, sinks (its sound below 0.3 under its rest) and
// carries him; else it levels and rises back (faster, the springs three times as stiff, while more than 2 below; its sound
// passing 0.3 under its rest going up). Always: two bobs, its turn from the tilt, its attach frame.
// orig: L9:0x003091b0
void gas_class1293_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x60);
    const uint32_t sp = f.sp, delta = sp, old_ang = sp + 0x10;
    vec3_scale(V(delta), V(m + 0x10), -1.0f);
    lq_sq(old_ang, m + 0x40);
    const float old_z = lwc1(m + 0x18);
    const float low = fpu_sub(lwc1(pv + 0x84), u2f(0x3e99999au));  // (from the rest height before the set-up)
    switch (lbu(m + 0x20)) {
        case 0:
            sw(pv + 0x80, 1);
            lq_sq(pv + 0x60, m + 0x40);
            V(pv + 0x70) = Vec4{};
            swc1(pv + 0x84, lwc1(m + 0x18));
            if (lw(pv + 0x8c) == -1) {
                sb(m + 0x20, 2);
                return;
            }
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 5.0f));
            swc1(m + 0x40, rand_range_f(u2f(0xbfc90fdbu), u2f(0x3fc90fdbu)));
            swc1(m + 0x44, rand_range_f(u2f(0xbfc90fdbu), u2f(0x3fc90fdbu)));
            sb(m + 0x20, 1);
            return;
        case 1: {
            const uint32_t o = lwu(L9_ADDR(0x0015ffd8)) + (lwu(pv + 0x8c) << 8);
            if (o == 0 || lbu(o + 0x20) == 0xfe || lbu(o + 0x20) == 0xfd) sb(m + 0x20, 2);
            return;
        }
        case 2:
            break;
        default:
            return;
    }
    if (lh(HERO + 0x30e) == 0 && lwu(HERO + 0x2fc) == m) {  // the hero on it
        if (lw(pv + 0x88) == 0) moby_sound_start(0, 0, m);
        if (lw(pv + 0x88) < frame_scale_i(30)) sw(pv + 0x88, static_cast<uint32_t>(frame_scale_i(30)));
    }
    if (lw(pv + 0x88) != 0) {
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x88)));
        vec3_sub(V(sp + 0x20), V(HERO + 0x80), V(m + 0x10));
        const float along = vec3_dot(V(sp + 0x20), V(m + 0xc0));
        const float across = vec3_dot(V(sp + 0x20), V(m + 0xd0));
        const float k = lwc1(L9_ADDR(0x00161dc4));
        lq_sq(sp + 0x30, pv + 0x60);  // (the tilt before, and its place)
        lq_sq(sp + 0x40, m + 0x10);
        tilt(pv, fpu_mul(-k, across), fpu_mul(k, along), L9_ADDR(0x00161dd4), L9_ADDR(0x00161dd8), L9_ADDR(0x00161ddc));
        const float z = fpu_sub(lwc1(m + 0x18), fpu_mul(lwc1(L9_ADDR(0x00161dc8)), dt()));
        swc1(m + 0x18, z);
        if (low < old_z && z <= low) moby_sound_start(1, 0, m);
        // the hero carried: his place turned with the tilt, less where he is; plus its fall
        point_turned(HERO + 0x80, sp + 0x50, m + 0x10, sp + 0x30, pv + 0x60);
        vec3_sub(V(HERO + 0xf0), V(sp + 0x50), V(HERO + 0x80));
        swc1(HERO + 0xf8, fpu_add(lwc1(HERO + 0xf8), fpu_sub(lwc1(m + 0x18), lwc1(sp + 0x48))));
    } else {
        float z = lwc1(m + 0x18);
        const float rate = fpu_mul(lwc1(L9_ADDR(0x00161dcc)), dt());
        z = fpu_add(z, fpu_mul(rate, fpu_sub(lwc1(pv + 0x84), z)));
        swc1(m + 0x18, z);
        if (z < fpu_sub(lwc1(pv + 0x84), 2.0f)) {  // far below: faster, stiffer
            swc1(m + 0x18, fpu_add(z, rate));
            auto spring = [](uint32_t ang, uint32_t vel) {
                const float kk = fpu_mul(fpu_mul(fpu_mul(lwc1(L9_ADDR(0x00161de0)), kDegree), dt_sq()), 3.0f);
                const float dd = fpu_mul(lwc1(L9_ADDR(0x00161de4)), lwc1(0x0015ed64));
                const float vv = fpu_mul(dt(), u2f(0x3fc90fdbu));
                spring_angle(ang, vel, 0, 0.0f, kk, dd, vv);
            };
            spring(pv + 0x60, pv + 0x70);
            spring(pv + 0x64, pv + 0x74);
        } else {
            tilt(pv, 0.0f, 0.0f, L9_ADDR(0x00161de0), L9_ADDR(0x00161de4), L9_ADDR(0x00161de8));
        }
        if (lwc1(pv + 0x84) < lwc1(m + 0x18)) swc1(m + 0x18, lwc1(pv + 0x84));
        if (old_z < low && low <= lwc1(m + 0x18)) moby_sound_start(2, 0, m);
    }
    sub_00262b00(m, pv + 0x98, pv + 0x9c, u2f(0x3e0f5c29u), fpu_mul(dt(), u2f(0x3f060a92u)));
    sub_00262b80(m, pv + 0x90, pv + 0x94, u2f(0x3db2b8c2u), fpu_mul(dt(), u2f(0x3edf66f3u)), fpu_mul(dt(), u2f(0x3f0a8263u)));
    swc1(m + 0x40, angle_add(lwc1(pv + 0x60), lwc1(m + 0x40)));
    swc1(m + 0x44, angle_add(lwc1(pv + 0x64), lwc1(m + 0x44)));
    vec3_add(V(delta), V(delta), V(m + 0x10));
    attach_frame_set(pv + 0x20, delta, old_ang, m + 0x40);
}

}  // namespace rac1
