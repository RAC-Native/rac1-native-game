// Class 819, a homing shot: one port of the copies Orxon (L10), Pokitaru (L11), Quartu (L15) and Drek's fleet (L17)
// carry, the same code relinked (the normalised texts compared). Orxon's copy pushes what it hits through its own build of
// sub_00259bc8 (L10:0x24c6b8, a variant of the overlay map; Orxon's generated code calls the Veldin port there too). Every
// address is the engine's (OV: the collision result 0x173e40..) or the base ELF's (bare: the hero 0x13f350, the level
// number 0x15ed84, the frame time 0x15ed6c and its square 0x15ed70). Header shared_class819.h.
//
// The pvars: the velocity +0, the shooter +0x10 (never hit), the target +0x14, the life +0x18, the turn rates +0x28 / +0x2c,
// the warning sound's state +0x30 (closing in) / +0x34 (played), the distance to the hero last frame +0x38.
#include <game/ovl/shared_class819.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position, +0x100 his velocity, +0x2080 his moby, +0x20a4 his kind)
}

// State 1: gone with its target (none or retired: it explodes next frame). Aimed at the hero's moby while he stands or is
// on board (+0x20a4 1 / 2), it turns toward where he will be: the shot's speed meets his ground velocity in the time t
// that solves (s^2 - v^2) t^2 - 2 (v.r) t - r^2 = 0 (the larger root; the target's floor 0.2 up, two more on board),
// or toward him now when no time fits, at most a turn a second (half a turn a second squared), spinning a turn a second.
// Then its two trails of sparks, its move, the warning (sound 0) once when it passes the hero within 5 units, and the
// segment it moved along: a hit on anything but its shooter pushes what it hit (kinds 0x10003, its velocity's heading,
// upward) and explodes it (state 2), as does the end of its life. (The segment runs from its new position back to the
// old one.) State 2: an explosion (no size on Orxon, 13 elsewhere),
// sound 0x99, gone.
// orig: L10:0x002cc2e8 L11:0x00302510 L15:0x002dd730 L17:0x002dbaa8
void class819_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 1:
        break;
    case 2: {
        explosion_burst(m, m + 0x10, -1, 0.25f, lw(0x0015ed84) == 10 ? 0.0f : 13.0f);
        moby_sound_play(0, 0, m, 0x99);
        moby_retire(m);
        return;
    }
    default:
        return;
    }
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t from = sp, still = sp + 0x10, rel = sp + 0x20, hero_vel = sp + 0x30, aim = sp + 0x40, lead = sp + 0x50;
    const uint32_t pos = m + 0x10;
    lq_sq(from, pos);
    {
        const uint32_t t = lwu(pv + 0x14);
        if (t == 0 || lbu(t + 0x20) == 0xfe || lbu(t + 0x20) == 0xfd) sb(m + 0x20, 2);
    }
    const uint32_t kind = lbu(HERO + 0x20a4);
    if (kind - 1u < 2u && lwu(pv + 0x14) == lwu(HERO + 0x2080)) {
        const float speed = vec3_length(V(pv));
        lq_sq(aim, lwu(pv + 0x14) + 0x10);
        const float floor = fpu_add(floor_probe_v(0.5f, V(aim), 0), u2f(0x3e4ccccdu));
        lq_sq(hero_vel, HERO + 0x100);
        sw(hero_vel + 8, 0);
        swc1(aim + 8, kind == 2 ? fpu_add(floor, 2.0f) : floor);
        vec3_sub(V(rel), V(aim), V(pos));
        const float a = fpu_sub(fpu_mul(speed, speed), vec3_length_sq_vu(V(hero_vel)));
        const float vr = fpu_add(fpu_add(fpu_mul(lwc1(hero_vel), lwc1(rel)), fpu_mul(lwc1(hero_vel + 4), lwc1(rel + 4))),
                                 fpu_mul(lwc1(hero_vel + 8), lwc1(rel + 8)));
        const float b = fpu_mul(vr, -2.0f);
        const float c = -vec3_length_sq_vu(V(rel));
        const float root = sqrt_f(fpu_sub(fpu_mul(b, b), fpu_mul(fpu_mul(a, 4.0f), c)));
        const float t1 = fpu_div(fpu_add(-b, root), fpu_add(a, a));
        const float t2 = fpu_div(fpu_sub(-b, root), fpu_add(a, a));
        float t;
        if (0.0f < t1 && 0.0f < t2) t = t2 < t1 ? t1 : t2;
        else if (0.0f < t1) t = t1;
        else if (0.0f < t2) t = t2;
        else t = -1.0f;
        uint32_t toward = rel;
        if (0.0f < t) {
            vec3_scale(V(lead), V(hero_vel), t);
            vec3_add(V(lead), V(lead), V(rel));
            toward = lead;
        }
        const float yaw = atan2_f(lwc1(toward), lwc1(toward + 4));
        const float pitch = -atan2_f(vec2_length(V(toward)), lwc1(toward + 8));
        const float turn = u2f(0x40c90fdbu), half = u2f(0x40490fdbu);
        swc1(m + 0x48, angle_seek_rate(pv + 0x28, lwc1(m + 0x48), yaw, fpu_mul(lwc1(0x0015ed70), turn), fpu_mul(lwc1(0x0015ed70), half),
                                       fpu_mul(lwc1(0x0015ed6c), turn)));
        swc1(m + 0x44, angle_seek_rate(pv + 0x2c, lwc1(m + 0x44), pitch, fpu_mul(lwc1(0x0015ed70), turn), fpu_mul(lwc1(0x0015ed70), half),
                                       fpu_mul(lwc1(0x0015ed6c), turn)));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(lwc1(0x0015ed6c), turn)));
        vec_from_spherical_v(V(pv), speed, lwc1(m + 0x48), -lwc1(m + 0x44));
    }

    vec_clear(V(still));
    {
        const int32_t life = frame_scale_i(rand_range(0xf, 0x16));
        const int32_t size = rand_range(0x14, 0x23);
        spawn_particle_4(pos, still, 0x6f00afffu, 0xff, static_cast<uint32_t>(life), 0x28, static_cast<uint32_t>(size), 1);
    }
    {
        const int32_t life = frame_scale_i(rand_range(0x1e, 0x3c));
        const int32_t size = rand_range(0x32, 0x4b);
        spawn_particle_4(pos, still, 0x1fffffffu, 0x4f4f4fu, static_cast<uint32_t>(life), 0x28, static_cast<uint32_t>(size), 0);
    }
    const float dist = vec3_distance(V(pos), V(HERO + 0x80));
    vec3_add(V(pos), V(pos), V(pv));
    if (lw(pv + 0x34) == 0 && lw(pv + 0x30) != 0 && lwc1(pv + 0x38) < dist && dist < 5.0f) {
        moby_sound_start(0, 0, m);
        sw(pv + 0x34, 1);
    }
    if (dist < lwc1(pv + 0x38)) sw(pv + 0x30, 1);
    swc1(pv + 0x38, dist);
    if (segment_collide_v(V(pos), V(from), 0x10, lwu(pv + 0x10), nullptr) != 0) {  // (back from where it is now)
        const uint32_t hit = lwu(OV(0x00173e58));
        if (hit != lwu(pv + 0x10)) {
            if (hit != 0) {
                vec2_set_length(V(rel), V(pv), 1.0f);
                swc1(pv + 8, 1.0f);
                sub_00259bc8(lwu(OV(0x00173e58)), m, 0x10003u, OV(0x00173e60), rel, 1.0f);
            }
            sb(m + 0x20, 2);
        }
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x18))) != 0) sb(m + 0x20, 2);
    ee_stack_free(0xf0);
}

}  // namespace rac1
