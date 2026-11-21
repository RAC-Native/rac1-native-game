// Umbris's own moby classes: two shots — class 880 (what class 1113 fires through 0x30b538: it flies straight, sparks
// out 30 units from where it started and bursts against what it hits) and class 882 (the turrets' shell, umbris.turret_shot:
// it falls, spins and explodes on what it hits) — hand-ported from Umbris's own overlay. Header umbris_mobys.h,
// registered in umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`); the frame time 0x15ed6c (and its square 0x15ed70)
// and the hero 0x13f3d0 are the base ELF's, the hit record the collision facades'.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;

// a shot out of the level's box (2 .. 1021 on each axis) retired
void retire_outside(uint32_t m) {
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(m + 0x10 + k * 4);
        if (c < 2.0f || 1021.0f < c) {
            moby_retire(m);
            return;
        }
    }
}
// a random direction (-1 .. 1 each, its w 0) in `r`, `out` = r and r's length scaled by `k` added on, the whole set to
// `len`'s length (the sparks' spread)
Vec4 rand_dir() {
    Vec4 r{0.0f, 0.0f, 0.0f, 0.0f};
    r.x = rand_range_f(-1.0f, 1.0f);
    r.y = rand_range_f(-1.0f, 1.0f);
    r.z = rand_range_f(-1.0f, 1.0f);
    return r;
}
}  // namespace

// Class 880: its pvars the velocity +0x30, where it started +0x40, the collision's ignored moby +0x50 and its flags
// (+0x54 0: 1), the shot record +0x0 (the collision's). Past 30 units from the start: three sparks (when shown +0x31),
// retired. Hitting something: put at the hit, five sparks off the surface (when shown), a sound, retired. Otherwise out
// of the box: retired.
// orig: L7:0x0030b6a8
void umb_class880_update_l7(uint32_t m) {
    const uint32_t pos = m + 0x10;
    const uint32_t pv = lwu(m + 0x78);
    const Vec4 old = V(pos);
    vec3_add(V(pos), V(pos), V(pv + 0x30));
    const uint32_t flags = lw(pv + 0x54) == 0 ? 1u : 0u;
    if (30.0f < vec2_distance(V(pos), V(pv + 0x40))) {
        if (lbu(m + 0x31) != 0) {
            for (int k = 0; k < 3; ++k) {
                const Vec4 r = rand_dir();
                Vec4 v = r;
                vec3_set_length(v, v, fpu_mul(vec3_length(r), u2f(0x3dcccccdu)));
                vec3_add(v, r, v);
                const float dt = lwc1(0x0015ed6c);
                const float hi = fpu_mul(dt, 4.0f);
                vec3_set_length(v, v, rand_range_f(fpu_add(dt, dt), hi));
                const uint32_t sp = ee_stack_alloc(0x10);
                V(sp) = v;
                particle_33_spawn(pos, sp);
                ee_stack_free(0x10);
            }
        }
        moby_retire(m);
        return;
    }
    const bool shown = lbu(m + 0x31) != 0;
    if (shown || vec2_distance(V(pos), V(HERO_POS)) < 30.0f) {
        if (segment_collide_v(old, V(pos), flags, lwu(pv + 0x50), static_cast<const uint32_t*>(ee_ram_ptr(pv))) != 0) {
            V(pos) = coll_hit_point();
            if (lbu(m + 0x31) != 0) {
                for (int k = 0; k < 5; ++k) {
                    Vec4 r = rand_dir();
                    Vec4 v = r;
                    vec3_reflect(r, V(pv + 0x30), coll_hit_normal());
                    vec3_set_length(v, v, fpu_mul(vec3_length(r), 0.5f));
                    vec3_add(v, r, v);
                    const float dt = lwc1(0x0015ed6c);
                    vec3_set_length(v, v, rand_range_f(fpu_mul(dt, 3.0f), fpu_mul(dt, 6.0f)));
                    const int32_t lo = frame_scale_i(10);
                    const int32_t n = rand_range(lo, frame_scale_i(15));
                    const uint32_t sp = ee_stack_alloc(0x10);
                    V(sp) = v;
                    spawn_particle_1b(pos, sp, 0x7f2f4f6fu, static_cast<uint32_t>(n), 30000.0f);
                    ee_stack_free(0x10);
                }
            }
            moby_sound_start(0, 0, m);
            moby_retire(m);
            return;
        }
    }
    retire_outside(m);
}

// Class 882, the turrets' shell: its pvars the velocity +0x0 (gravity 9.8 on z), the moby the collision ignores +0x10,
// its kind +0x14 (bit 0: the explosion's variant; a multiple of 3: a burn of 10), the spins +0x18 / +0x1c. Hitting
// something (not another shell 0x371 / 0x372): far from the camera (50) or not, the explosion's variants; when hidden and
// 5 or more from the hero, only a sound. Retired then, or once out of the box.
// orig: L7:0x0030bbc8
void umb_class882_update_l7(uint32_t m) {
    const uint32_t pos = m + 0x10;
    const uint32_t pv = lwu(m + 0x78);
    const Vec4 old = V(pos);
    vec3_add(V(pos), V(pos), V(pv));
    swc1(pv + 0x8, fpu_sub(lwc1(pv + 0x8), fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu))));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x18)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x1c)));
    if (segment_collide_v(old, V(pos), 0, lwu(pv + 0x10), nullptr) == 0 && sphere_collide(pos, 0.5f, 0, lwu(pv + 0x10)) == 0) {
        retire_outside(m);
        return;
    }
    const uint32_t hm = coll_hit_moby();
    if (hm != 0 && lhu(hm + 0xa6) - 0x371u < 2u) {
        retire_outside(m);
        return;
    }
    if (lbu(m + 0x31) == 0 && !(vec2_distance(V(pos), V(HERO_POS)) < 5.0f)) {
        moby_sound_start(0, 0, m);
        moby_retire(m);
        return;
    }
    const float burn = ee_rem(lw(pv + 0x14), 3) == 0 ? 10.0f : 0.0f;
    const bool far = 50.0f < vec2_distance(V(pos), V(L7_ADDR(0x00166e40)));
    const bool odd = (lw(pv + 0x14) & 1) != 0;
    if (far) {
        if (odd)
            explosion_effect(m, pv, 0, 1, 2, 3, -1, 0, 1, -1, 0, 2.0f, 1.0f, 0.0f, 0.0f, 1000.0f, u2f(0x3fa66666u), burn);
        else
            explosion_effect(m, pv, 0, 0, 2, 3, 0, 0, 1, -1, 0, 2.0f, 1.0f, 0.0f, 0.0f, 1000.0f, u2f(0x3f99999au), burn);
    } else {
        if (odd)
            explosion_effect(m, pv, 0, 0, 2, 4, 0, 0, 1, -1, 0, 2.0f, 1.0f, 0.0f, 1.0f, 1000.0f, 1.0f, burn);
        else
            explosion_effect(m, pv, 0, 1, 2, 4, 0, 0, 1, -1, 0, 2.0f, 1.0f, 0.0f, 0.0f, 1000.0f, 1.0f, burn);
    }
    moby_retire(m);
}

}  // namespace rac1
