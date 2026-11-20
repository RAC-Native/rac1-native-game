// Drek's fleet's (level 17) hazards, ported by hand from the level's own code: class 669, a spinning electric fan
// (three arms of five points turning about its axis; near the hero each arm hurts what crosses it, on screen the
// first and last points throw sparks of particle 0x3c, and its draw L17:0x2d7cf0 (still generated) is queued), and
// class 835, a floating mine (it drifts about its place, bursts when a shot of another class hits it or when it
// touches a moby other than another mine, scattering 150 sparks of particle 0x22). The original's order of calls and
// float operations kept. Level 17 only: `orig: L17:`, the collision facade, the level's own data L17_ADDR, base ELF
// bare. Registered in veldin_orbit_mobys.cpp.
#include <game/ovl/veldin_orbit_mobys.h>

#include <game/collision/collision.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

float dt() { return lwc1(0x0015ed6cu); }

constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t DEG = 0x3c8efa35u;  // (pi / 180)
}  // namespace

// Class 669, the electric fan. Pvars: +0 its turning speed (degrees, scaled by the level's 0x161b78), +4 the turn a
// frame. The level's tuning words: 0x161b7c the arms' first angle, 0x161b80 the spread of an arm's points, 0x161b84
// the arms' length, 0x161b88 / 0x161b8c / 0x161b90 / 0x161b94 the sparks' count, size, life and colour. States: 0 set
// up, 1 turning (its draw queued).
// orig: L17:0x002d77f0
void vo_class669_update_l17(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (sub_0020d568() == 0) return;
    const Frame f(496);
    const uint32_t sp = f.sp;
    const uint32_t pos = m + 0x10, mat = m + 0xc0;
    const uint8_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        swc1(pv + 0x4, fpu_mul(fpu_mul(fpu_mul(lwc1(pv + 0x0), lwc1(L17_ADDR(0x00161b78))), u2f(DEG)), dt()));
    } else if (st == 1) {
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x4)));
        sub_001f93e0(L17_ADDR(0x002d7cf0), m);
    }
    // the hit record (sp + 0x20): pushed away from the fan's face on the hero's side
    vec3_sub(V(sp), V(HERO_POS), V(pos));
    const float side = vec3_dot(V(sp), V(mat));
    vec3_set_length(V(sp + 0x10), V(mat), 0.0f < side ? 1.0f : -1.0f);
    const uint32_t rec = sp + 0x20;
    sw(rec + 0x14, 0x10001u);
    sb(rec + 0x19, 1);
    sw(rec + 0x20, 1);
    sb(rec + 0x18, 1);
    swc1(rec + 0x8, 1.0f);
    swc1(rec + 0xc, u2f(0x45afdf66u));
    sh(rec + 0x1a, lhu(m + 0xa6));
    sw(rec + 0x10, m);
    swc1(rec + 0x1c, 1.0f);
    // the fan's turned frame
    euler_to_mat3_vu(M(sp + 0x50), V(m + 0x40));
    mat3_normalize_columns(sp + 0x50);
    mat3_copy_affine(M(sp + 0x80), M(sp + 0x50));
    const uint32_t point = sp + 0xc0, centre = sp + 0xd0, from = sp + 0xe0, vel = sp + 0xf0, span = sp + 0x100;
    for (int32_t arm = 0; arm < 3; ++arm) {
        for (int32_t j = 0; j < 5; ++j) {
            const float spread = angle_wrap(fpu_mul(fpu_mul(fpu_mul(lwc1(L17_ADDR(0x00161b80)), u2f(DEG)), int_to_float(j)), 0.5f));
            const float half = fpu_mul(lwc1(L17_ADDR(0x00161b80)), u2f(DEG));
            const float first = fpu_mul(lwc1(L17_ADDR(0x00161b7c)), u2f(DEG));
            const float offset = fpu_sub(spread, half);
            const float arm_angle = angle_wrap(fpu_mul(int_to_float(arm), u2f(0x40060a92u)));
            const float a = angle_add(angle_add(first, arm_angle), offset);
            vec_from_spherical_v(V(point), lwc1(L17_ADDR(0x00161b84)), u2f(0x3fc90fdbu), a);
            mat4_transform(V(point), V(point), M(sp + 0x80));
            vec3_add_inplace(V(point), V(pos));
            if (vec3_distance(V(pos), V(HERO_POS)) < 30.0f) segment_collide(pos, point, 9, m, rec);
            lq_sq(centre, pos);
            swc1(centre + 0xc, lwc1(L17_ADDR(0x00161b84)));
            if (point_on_screen(centre, 20.0f) == -1) continue;
            if (j != 0 && j != 4) continue;
            // the arm's first and last points: sparks along it, from the centre out or from the tip in
            eer::sq(vel, eer::Q128{});
            swc1(vel + 0x8, 12.0f);
            mat3_transform(V(vel), V(vel), M(mat));
            for (int32_t k = 0; k < lw(L17_ADDR(0x00161b88));) {
                const float dir = (libc_rand() & 1) != 0 ? 1.0f : -1.0f;
                const float life_max = lwc1(L17_ADDR(0x00161b90));
                const int32_t frames = float_to_int(frame_scale_f(rand_range_f(fpu_mul(life_max, 0.5f), life_max)));
                const int32_t life = 0 < frames ? frames : 1;
                vec3_sub(V(span), V(point), V(pos));
                lq_sq(vel, span);
                vec3_scale(V(vel), V(vel), dir);
                const float len = vec3_length(V(vel));
                lq_sq(from, dir < 0.0f ? point : pos);
                ++k;
                vec3_scale(V(vel), V(vel), rand_range_f(0.0f, u2f(0x3f666666u)));
                vec3_add_inplace(V(from), V(vel));
                const float lo = fpu_mul(dt(), 10.0f);
                const float hi = fpu_div(len, int_to_float(life << 2));
                vec3_set_length(V(vel), V(vel), rand_range_f(lo, hi));
                const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
                spawn_particle_3c(from, vel, lwu(L17_ADDR(0x00161b94)), static_cast<uint32_t>(life) & 0xffu, frame, 0,
                                  lwc1(L17_ADDR(0x00161b8c)));
            }
        }
    }
}

// Class 835, the floating mine. Pvars: +0x20 its hit record, +0x60 / +0x64 its drift's angle and phase (the level's
// 0x161d2c / 0x161d30: the drift's size and speed). States: 0 set up, 1 drifting. Bursts when hit by a shot of
// another class, or when it touches a moby (other than another mine) while the hero does not stand on it.
// orig: L17:0x002dc3f8
void vo_class835_update_l17(uint32_t m) {
    const Frame f(320);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    vec_clear(V(sp + 0x20));
    auto burst_fx = [&] {
        explosion_effect(m, sp + 0x20, 0, 3, 3, 5, 1, 0, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 4.0f, 1.0f, 7.0f);
    };
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x230000u), 0);
    int32_t state = 0;
    moby_hit_react_r(m, hit, pv + 0x20, 0, state, nullptr, nullptr, 4);
    if (1 < state && hit != 0 && lh(lwu(hit + 0x20) + 0xa6) != 0x343) {
        burst_fx();
        moby_retire(m);
        return;
    }
    const uint8_t st = lbu(m + 0x20);
    if (st == 0) {
        swc1(pv + 0x60, rand_angle());
        sw(pv + 0x64, 0);
        sb(m + 0x20, 1);
        return;
    }
    if (st != 1) return;
    sub_00262b00(m, pv + 0x60, pv + 0x64, lwc1(L17_ADDR(0x00161d2c)),
                 fpu_mul(fpu_mul(lwc1(L17_ADDR(0x00161d30)), u2f(DEG)), dt()));
    const uint32_t rec = sp + 0x30;
    sub_00259888(1.0f, rec, static_cast<int32_t>(m), 0x10001, sp + 0x20);
    const uint32_t probe = sp + 0x60;
    lq_sq(probe, m + 0x10);
    swc1(probe + 0x8, fpu_add(lwc1(probe + 0x8), 0.75f));
    const bool stood_on = lwu(0x0013f58cu) == m || lwu(0x0013f590u) == m;
    if (!stood_on && mobys_in_sphere(probe, 1.0f, 0x10, m, rec) == 0) return;
    const uint32_t met = coll_hit_moby();
    if (met == 0 || lh(met + 0xa6) == 0x343) return;
    burst_fx();
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.0f));
    const float ground = surface_height(m + 0x10, sp + 0x70);
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 1.0f));
    const uint32_t dir = sp + 0x90, vel = sp + 0x80;
    for (int i = 0; i < 150; ++i) {
        eer::sq(dir, eer::Q128{});
        swc1(dir + 0x0, rand_range_f(-1.0f, 1.0f));
        swc1(dir + 0x4, rand_range_f(-1.0f, 1.0f));
        swc1(dir + 0x8, rand_range_f(-0.5f, 2.0f));
        const float lo = fpu_mul(dt(), 4.0f), hi = fpu_mul(dt(), 8.0f);
        lq_sq(vel, dir);
        vec3_set_length(V(vel), V(vel), rand_range_f(lo, hi));
        spawn_particle_22(m + 0x10, vel, fpu_mul(rand_range_f(u2f(0x3d4ccccdu), u2f(0x3dcccccdu)), 210000.0f), ground);
    }
    moby_retire(m);
}

}  // namespace rac1
