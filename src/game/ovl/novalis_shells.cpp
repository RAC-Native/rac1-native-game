// Novalis's own moby classes: two thrown shells (hand-ported from Novalis's own overlay). Header novalis_mobys2.h,
// registered in novalis_mobys2.cpp. Addresses are Novalis's (`orig: L1:`); Novalis-only data is L1_ADDR, the base ELF's are
// bare (the frame time 0x15ed6c / its square 0x15ed70, the time step 0x15ed60).
//   class 722   a bomb lobbed in an arc: it bursts on what it meets (the world, a moby) or when its fuse runs out
//   class 1510  a burning shell (Novalis's build of class 325's: its own drag, gravity and burst), its glow drawn by
//               class325_draw (shared_class325.cpp)
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

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

float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// class 1510's drag on one velocity word: when faster than the level's limit (0x162060) a time step, eased by the level's
// drag (0x16205c) a time step
void drag(uint32_t w) {
    const float step = lwc1(0x0015ed60);
    if (fpu_mul(lwc1(L1_ADDR(0x00162060)), step) < abs_f(lwc1(w)))
        swc1(w, fpu_mul(lwc1(w), fpu_add(fpu_mul(fpu_sub(lwc1(L1_ADDR(0x0016205c)), 1.0f), step), 1.0f)));
}
}  // namespace

// A burst at `pos` (an EE vector) of size `scale`: three flashes of sparks (particle 0x0b, the colours of the level's two
// tables of six 0x20b4f0 / 0x20b508); for a moby `m` (not 0) two sets of its shards and, while it is not dying, its sound
// `sound` (-1 none); a flash of light (the effect 0x1e3440) sized `flash` (13 when below 0; none when 0).
// orig: L1:0x002fa1b0
void nov_2fa1b0_l1(uint32_t m, uint32_t pos, int32_t sound, float scale, float flash) {
    const Frame f(0x70);
    const uint32_t at = f.sp, still = f.sp + 0x10;
    lq_sq(at, pos);
    vec_clear(V(still));
    for (int i = 0; i < 3; ++i) {
        const float size = fpu_mul(scale, 400000.0f);
        const float speed = fpu_mul(rand_range_f(8.0f, 10.0f), dt());
        const uint32_t c0 = lwu(L1_ADDR(0x0020b4f0) + (static_cast<uint32_t>(rand_mod(6)) << 2));
        const uint32_t c1 = lwu(L1_ADDR(0x0020b508) + (static_cast<uint32_t>(rand_mod(6)) << 2));
        const int32_t lo = frame_scale_i(15);
        const int32_t life = rand_range(lo, frame_scale_i(20));
        const int32_t lo2 = frame_scale_i(25);
        const int32_t life2 = rand_range(lo2, frame_scale_i(30));
        spawn_particle_0b(at, still, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(life2), 0, 0, size,
                          fpu_mul(speed, scale));
    }
    if (m != 0) {
        shard_spawn(m, at, still, static_cast<uint32_t>(frame_scale_i(20)), 0x7f, 0, 0x40, 0x30, fpu_add(scale, scale));
        shard_spawn(m, at, still, static_cast<uint32_t>(frame_scale_i(29)), 0x20, 0, 0x20, 0, fpu_mul(scale, 1.5f));
        const uint32_t st = lbu(m + 0x20);
        if (st != 0xfe && st != 0xfd && sound != -1) moby_sound_start(sound, 0, m);
    }
    if (flash == 0.0f) return;
    const uint32_t light = L1_ADDR(0x001e3440);
    const float s = 0.0f < flash ? flash : 13.0f;
    swc1(light + 0x20, s);
    swc1(light + 0x24, s);
    swc1(light + 0x28, s);
    effect_spawn(light, at, 0, 0);
}

// Class 722: a bomb. Gravity (2 over 10 frames squared, frame-scaled) on its velocity (pvars +0), then the move; gone out
// of the level's positive octant or 64 away from the camera. Its fuse +0x14 out: it bursts where it is. Else it bursts on
// the world along its move (placed at the hit) or on a moby within 0.5 (its thrower +0x10 left out), the hit record
// (damage 1, its class) handed to what it meets.
// orig: L1:0x002fa4c0
void nov_class722_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x50);
    const uint32_t sp = f.sp;
    V(sp) = V(m + 0x10);  // (where it was)
    const float a = frame_scale_f(10.0f);
    const float b = frame_scale_f(10.0f);
    const float c = frame_scale_f(10.0f);
    const float one = fpu_add(fpu_mul(a, 0.0f), 1.0f);
    const float g = fpu_div(fpu_add(one, one), fpu_mul(b, c));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), g));
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    if (lwc1(m + 0x10) < 0.0f || lwc1(m + 0x14) < 0.0f || lwc1(m + 0x18) < 0.0f || 64.0f < vec2_distance(V(m + 0x10), V(L1_ADDR(0x00167240)))) {
        moby_retire(m);
        return;
    }
    // the hit record (sp + 0x10): its velocity, damage 1, 5627.92, the bomb, the kind 0x10001, its class
    const uint32_t rec = sp + 0x10;
    sw(sp + 0x20, m);
    sw(sp + 0x24, 0x10001u);
    swc1(sp + 0x2c, 1.0f);
    sw(sp + 0x30, 1);
    V(rec) = V(pv);
    swc1(sp + 0x18, 1.0f);
    swc1(sp + 0x1c, u2f(0x45afdf66u));
    sb(sp + 0x29, 1);
    sh(sp + 0x2a, lhu(m + 0xa6));
    sb(sp + 0x28, 1);
    const uint32_t copy = sp + 0x40;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0) {
        V(copy) = V(m + 0x10);
        nov_2fa1b0_l1(m, copy, 0, 0.5f, 0.0f);
        moby_retire(m);
        return;
    }
    if (segment_collide_v(V(sp), V(m + 0x10), 0, lwu(pv + 0x10), static_cast<const uint32_t*>(ee_ram_ptr(rec))) != 0) {
        V(m + 0x10) = coll_hit_point();
        V(copy) = V(m + 0x10);
        nov_2fa1b0_l1(m, copy, 0, 0.5f, 0.0f);
        moby_retire(m);
        return;
    }
    if (mobys_in_sphere(m + 0x10, 0.5f, 0x10, lwu(pv + 0x10), rec) != 0) {
        V(copy) = V(m + 0x10);
        nov_2fa1b0_l1(m, copy, 0, 0.5f, 0.0f);
        moby_retire(m);
    }
}

// Class 1510: a burning shell. Its glow queued every other frame or so (class325_draw at 0x30bfa8); the move; the drag on
// x / y when fast and on z while rising, the level's gravity (0x162058); the spin. Gone out of the level's box (2 .. 1021);
// on a fuse +0x20 out or the world within 2 (its thrower +0x24 left out) it bursts (bounced off what it hit, or stopped),
// else a trail: sparks (particle 0x15) and two puffs of grey smoke (particle 0x17).
// orig: L1:0x0030ba18
void nov_class1510_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (rand_mod(2) != 0) draw_callback_add(L1_ADDR(0x0030bfa8), m);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    drag(pv);
    drag(pv + 4);
    if (0.0f < lwc1(pv + 8))
        swc1(pv + 8, fpu_mul(lwc1(pv + 8), fpu_add(fpu_mul(fpu_sub(lwc1(L1_ADDR(0x0016205c)), 1.0f), lwc1(0x0015ed60)), 1.0f)));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(lwc1(L1_ADDR(0x00162058)), lwc1(0x0015ed70))));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x18)));
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(m + 0x10 + k * 4);
        if (c < 2.0f || 1021.0f < c) {
            moby_retire(m);
            return;
        }
    }
    const int32_t hit = sphere_collide(m + 0x10, 2.0f, 0, lwu(pv + 0x24));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20))) != 0 || hit != 0) {
        if (hit != 0) {
            if (coll_hit_flags() != 0) vec3_reflect(V(pv), V(pv), coll_hit_normal());
            else vec_clear(V(pv));
        }
        explosion_effect(m, pv, m + 0x10, 0x14, 3, 4, 0, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);
        moby_retire(m);
        return;
    }
    const float size = fpu_mul(rand_range_f(0.5f, 1.5f), 210000.0f);
    const int32_t lo = frame_scale_i(10);
    const int32_t count = rand_range(lo, frame_scale_i(20));
    spawn_particle_15_v(V(m + 0x10), V(L1_ADDR(0x0015f580)), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(count), 1, size);
    for (int i = 0; i < 2; ++i) {
        int32_t spin = rand_mod(6);
        if (rand_mod(2) != 0) spin = -spin;
        const float puff = rand_range_f(40000.0f, 100000.0f);
        const uint32_t grey = static_cast<uint32_t>(rand_range(0x30, 0xff));
        const uint32_t p = spawn_particle_17(m + 0x10, spin, L1_ADDR(0x0015f580), grey | (grey << 16) | (grey << 8) | 0x60000000u,
                                             u2f(0x3ccccccdu), 1.0f, 1.0f, puff);
        if (p == 0) continue;
        sb(p + 3, 0x44);
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sw(p + 0x24, 2);
        sb(p + 0x2a, 0x60);
        sb(p + 0x2b, lbu(p + 0xa));
    }
}

}  // namespace rac1
