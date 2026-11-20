// Veldin's finale (level 18): the shots of Drek's side. Class 41, the rocket the troopers (class 1454) fire, homing on the
// hero when its owner asks for it; and three attacks of the boss's mech (class 1422, which spawns them): class 564, the
// shell it lobs at a point, class 624, the ring of energy that drops, lands and widens along the ground, and class 628,
// the short burst about its feet. Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR,
// the base ELF's bare (the hero 0x13f350, his state 0x1413d4, the frame time 0x15ed6c and its square 0x15ed70).
// Registered in veldin_finale_mobys.cpp; the draw callbacks the shots queue (L18:0x2d5768, 0x2dba20, 0x2dc4b8) stay generated.
//
// The pvars:
//   class 41   +0 its velocity, +0x10 its life (frames, a halfword), +0x12 homing on the hero, +0x14 the trooper that fired it
//   class 564  +0 its first velocity, +0x10 the point it falls on, +0x24 / +0x28 the turn of its arc (two angles), +0x2c its
//              frame, +0x30 half its frames, +0x34 its frames, +0x38 flags (1 a big blast, 2 its marker shown near the end,
//              4 no shock to Giant Clank)
//   class 624  +0 its velocity while it falls, +0x20 the ring's growth per frame, +0x24 its radius, +0x28 the radius it ends at,
//              +0x2c the wait on the ground, +0x30 its glow's phase, +0x34 a moby its segments ignore
//   class 628  +0 its radius, +4 its glow (grows), +8 its life (frames), +0xc the moby it hurts for (the boss)
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/devastator.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
constexpr uint32_t HERO_POS = 0x0013f3d0;   // the hero's position (0x13f350 + 0x80)

// an EE stack frame of the original's size (its locals are passed to the callees by address, and the stale lanes they
// read are the fresh frame's, as in the transliteration)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

void clear_quad(uint32_t a) {
    sd(a, 0);
    sd(a + 8, 0);
}

// The smoke of a rocket or a shell: a puff (dev_2712b8's particle with the level's smoke texture) at a random point of
// this frame's move `dir` from the moby, and a spark of type 0x15 drifting along the moby's own z. `buf` is three
// quadwords of the caller's frame (the puff's spread, its point, the spark's drift), `frames` the particles' life.
void smoke_puffs(uint32_t m, uint32_t dir, uint32_t buf, int32_t frames) {
    const uint32_t spread = buf, at = buf + 0x10, drift = buf + 0x20;
    rand_sphere_point(spread, fpu_mul(dt(), 0.1f), fpu_mul(dt(), 0.2f));
    vec3_scale(V(at), V(dir), rand_range_f(0.0f, 1.0f));
    vec3_add(V(at), V(at), V(m + 0x10));
    const uint32_t p = static_cast<uint32_t>(dev_2712b8(at, spread, static_cast<uint32_t>(frame_scale_i(frames)), 0x7f, 0x606060, 3,
                                                         40000.0f, 1000.0f, 1.0f, u2f(0xb951b717u), 0.0f));
    if (p != 0) {
        sb(p + 3, 0x44);
        sb(p + 2, lbu(lwu(L18_ADDR(0x001b2adc))));  // (the level's smoke texture)
    }
    clear_quad(drift);
    swc1(drift + 8, u2f(0x3ca3d70au));  // 0.02
    mat3_transform(V(drift), V(drift), M(m + 0xc0));
    spawn_particle_15_v(V(at), V(drift), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(frame_scale_i(frames)), 1, 20000.0f);
}

// the moby's yaw and pitch along `dir`
void face_along(uint32_t m, uint32_t dir) {
    swc1(m + 0x48, atan2_f(lwc1(dir + 0), lwc1(dir + 4)));
    swc1(m + 0x44, -atan2_f(vec2_length(V(dir)), lwc1(dir + 8)));
}

// The push of a hit toward the hero, flat: `push` = the way from `from` to the hero, its z 0, of length dt * 4, then its
// z set to dt * 4 (a hop); the hit record at `rec` made from it with damage `dmg` and the flags 0x10001.
void hit_toward_hero(uint32_t m, uint32_t from, uint32_t rec, uint32_t push, float dmg) {
    vec3_sub(V(push), V(HERO_POS), V(from));
    sw(push + 8, 0);
    vec3_set_length(V(push), V(push), fpu_mul(dt(), 4.0f));
    swc1(push + 8, fpu_mul(dt(), 4.0f));
    sub_00259888(dmg, rec, static_cast<int32_t>(m), 0x10001, push);
}
}  // namespace

// ---- class 41: the troopers' rocket

// Spins about its axis; homing (+0x12) it turns toward the hero (0x13f410) keeping its speed, and is gone when on him. It
// flies one step, hitting what the segment of the step meets (not its trooper) with the push of 2 away from its trooper
// when that is a live class 46 moby (else along its own flight): on a hit or at the end of its life it bursts; else it
// leaves smoke.
// orig: L18:0x002a7220
void vf_class41_update_l18(uint32_t m) {
    const Frame f(272);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    const uint32_t next = sp + 0x20, rec = sp + 0x30, push = sp + 0x60;
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(dt(), u2f(0x40c90fdbu))));  // (2 pi a second)
    if (lh(pv + 0x12) != 0) {
        const float speed = vec3_length(V(pv));
        vec3_sub(V(pv), V(0x0013f410), V(pos));
        const float d = vec3_length(V(pv));
        if (speed < d) {
            vec3_set_length(V(pv), V(pv), speed);
            face_along(m, pv);
        } else if (d < 0.1f) {
            moby_retire(m);
            return;
        }
    }
    vec3_add(V(next), V(pos), V(pv));
    const uint32_t owner = lwu(pv + 0x14);
    if (owner != 0 && lh(owner + 0xa6) == 0x2e && lbu(owner + 0x20) != 0xfe && lbu(owner + 0x20) != 0xfd) {
        vec3_sub(V(push), V(pos), V(owner + 0x10));
        sw(push + 8, 0);
        vec3_set_length(V(push), V(push), 2.0f);
        swc1(push + 0xc, u2f(0x45afdf66u));  // (5627.92)
    } else {
        vec3_set_length(V(push), V(pv), 2.0f);
    }
    sub_00259888(2.0f, rec, static_cast<int32_t>(m), 1, push);
    sh(rec + 0x1a, lhu(m + 0xa6));
    const uint32_t r = rec;
    if (segment_collide_v(V(pos), V(next), 0, lwu(pv + 0x14), static_cast<const uint32_t*>(ee_ram_ptr(r))) != 0 ||
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x10))) != 0) {
        explosion_effect(m, pv, pos, 10, 6, 4, 0, 0, 0, -1, 0, 1.5f, 1.0f, 1.0f, 0.5f, 9.0f, 0.5f, 0.0f);
        moby_retire(m);
        return;
    }
    lq_sq(pos, next);
    smoke_puffs(m, pv, sp + 0x70, 10);
}

// ---- class 564: the boss's shell

// Its smoke along this frame's move `dir`.
// orig: L18:0x002d5488
void vf_shell_smoke_l18(uint32_t m, uint32_t dir) {
    const Frame f(128);
    smoke_puffs(m, dir, f.sp, 0x14);
}

// Its blast: bigger with flag 1; with flag 4 no shock, else, the hero being Giant Clank (the movement mode 2), a shock
// sphere of radius 2 that hurts him.
// orig: L18:0x002d5610
void vf_shell_blast_l18(uint32_t m, uint32_t dir) {
    const Frame f(112);
    const uint32_t sp = f.sp;
    lq_sq(sp + 0x20, dir);
    const uint32_t flags = lwu(lwu(m + 0x78) + 0x38);
    const bool big = (flags & 1) != 0;
    const float s = big ? 2.0f : 0.75f;
    const float b = big ? 4.0f : 2.0f;
    const float c = big ? 2.0f : 1.0f;
    const float radius = (flags & 4) != 0 ? 0.0f : 1.5f;
    explosion_effect(m, sp + 0x20, m + 0x10, 0x14, 6, 0x20, 1, 0, 0, -1, 0, radius, 1.0f, b, c, 9.0f, s, 5.0f);
    if (radius == 0.0f) return;
    if (lbu(0x001413f4) != 2) return;  // (the hero's movement mode: 2 Giant Clank)
    moby_sphere_contact(m, m + 0x10, 2, 0, 1, 0, 2.0f, lwc1(L18_ADDR(0x0016196c)), 1.0f);
}

// Flies its arc for +0x34 frames from where it is to the point +0x10: this frame's step is the straight way left over
// the frames left, turned by the arc's two angles about that way's yaw, blended from its first velocity by how far it
// is. It bursts on the ground, on a class 479 moby (its flags but 4 cleared) or class 419, or at its last frame; other
// mobys it passes. Near its end (flag 2) it shows its marker (the draw callback L18:0x2d5768). Gone when the hero dies
// or the level's mode +0x15f5c4 is 2.
// orig: L18:0x002d5050
void vf_class564_update_l18(uint32_t m) {
    const Frame f(384);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    if (lw(g::HERO_STATE) == 0x72 || lw(L18_ADDR(0x0015f5c4)) == 2) {
        moby_retire(m);
        return;
    }
    if (lbu(m + 0x20) != 0) return;
    const uint32_t step = sp, dir = sp + 0x10, ang = sp + 0x20, from = sp + 0x30;
    const uint32_t yaw_m = sp + 0x40, arc_m = sp + 0x70, unyaw_m = sp + 0xa0;
    lq_sq(from, pos);
    float t = fpu_div(int_to_float(lw(pv + 0x2c)), int_to_float(lw(pv + 0x30)));
    if (1.0f < t) t = 1.0f;
    vec3_sub(V(step), V(pv + 0x10), V(pos));
    vec3_scale(V(step), V(step), fpu_div(1.0f, int_to_float(static_cast<int32_t>(lwu(pv + 0x34) - lwu(pv + 0x2c)))));
    vec_clear(V(ang));
    swc1(ang + 8, atan2_f(lwc1(step + 0), lwc1(step + 4)));
    euler_to_mat3_vu(M(yaw_m), V(ang));
    mat3_transpose(M(unyaw_m), M(yaw_m));
    vec_clear(V(ang));
    swc1(ang + 8, lwc1(pv + 0x24));
    swc1(ang + 4, lwc1(pv + 0x28));
    euler_to_mat3_vu(M(arc_m), V(ang));
    mat3_transform(V(step), V(step), M(unyaw_m));
    mat3_transform(V(step), V(step), M(arc_m));
    mat3_transform(V(step), V(step), M(yaw_m));
    vec3_lerp(V(dir), V(pv), V(step), t);
    vf_shell_smoke_l18(m, dir);
    vec3_add(V(pos), V(pos), V(dir));
    face_along(m, dir);
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(dt(), u2f(0x4196cbe4u))));  // (6 pi a second)
    sw(pv + 0x2c, lwu(pv + 0x2c) + 1);
    bool burst = segment_collide_v(V(from), V(pos), 0, m, nullptr) != 0;
    if (burst) {
        const uint32_t hit = coll_hit_moby();
        if (hit != 0) {
            const int16_t cls = lh(hit + 0xa6);
            if (cls == 0x1df) sw(pv + 0x38, lwu(pv + 0x38) & 4);
            else if (cls != 0x1a3) burst = false;
        }
    }
    const uint32_t frame = lwu(pv + 0x2c), frames = lwu(pv + 0x34);
    if (frame == frames || burst) {
        vf_shell_blast_l18(m, dir);
        moby_retire(m);
        return;
    }
    if ((lwu(pv + 0x38) & 2) == 0) return;
    const float left = static_cast<float>(static_cast<int32_t>(frames - frame));
    if (left <= frame_scale_f(lwc1(L18_ADDR(0x00161964)))) sub_001f93e0(L18_ADDR(0x002d5768), m);
}

// ---- class 624: the boss's ring of energy

// A spark pair on the ring's segment `a` .. `b` (its middle on screen, by the chance L18:0x161c98): two particles of type
// 0x3c at a random point of it, flying outward at the ring's growth.
// orig: L18:0x002dc0c8
void vf_ring_sparks_l18(uint32_t m, uint32_t a, uint32_t b) {
    const Frame f(160);
    const uint32_t sp = f.sp, mid = sp, at = sp + 0x10, vel = sp + 0x20;
    const uint32_t pv = lwu(m + 0x78);
    vec3_lerp(V(mid), V(a), V(b), 0.5f);
    swc1(mid + 0xc, fpu_mul(vec3_distance(V(a), V(b)), 0.5f));
    if (point_on_screen(mid, lwc1(L18_ADDR(0x00161bf4))) == -1) return;
    if (!(rand_range_f(0.0f, 1.0f) < lwc1(L18_ADDR(0x00161c98)))) return;
    vec3_lerp(V(at), V(a), V(b), rand_range_f(0.0f, 1.0f));
    vec3_sub(V(vel), V(at), V(m + 0x10));
    vec3_set_length(V(vel), V(vel), lwc1(pv + 0x20));
    const uint32_t color = rgba_lerp(lwu(L18_ADDR(0x00161c80)), lwu(L18_ADDR(0x00161c84)), rand_range_f(0.0f, 1.0f));
    {
        const uint32_t life = static_cast<uint32_t>(frame_scale_i(lw(L18_ADDR(0x00161c8c)))) & 0xff;
        const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xff;
        spawn_particle_3c(at, vel, color, life, frame, 0, lwc1(L18_ADDR(0x00161c88)));
    }
    swc1(vel + 8, 0.0f);
    {
        const uint32_t life = static_cast<uint32_t>(frame_scale_i(lw(L18_ADDR(0x00161c90)))) & 0xff;
        const uint32_t frame = static_cast<uint32_t>(rand_mod(0xff)) & 0xff;
        spawn_particle_3c(at, vel, color, life, frame, 0, lwc1(L18_ADDR(0x00161c88)));
    }
}

// The ring's hurt: its 64 segments half a unit over the ground hit what they meet (not the moby +0x34) with a damage of
// 3 and a push toward the hero; while its radius is under 3 each segment also sparks.
// orig: L18:0x002dbe80
void vf_ring_hurt_l18(uint32_t m) {
    const Frame f(272);
    const uint32_t sp = f.sp, rec = sp, push = sp + 0x30, a = sp + 0x40, b = sp + 0x50;
    const uint32_t pv = lwu(m + 0x78), pos = m + 0x10;
    hit_toward_hero(m, pos, rec, push, 3.0f);
    const float step = fpu_mul(fpu_div(1.0f, int_to_float(0x40)), u2f(0x40c90fd0u));  // (a 64th of 2 pi)
    float from = u2f(0xc0490fd0u);                                                     // (-pi)
    float to = angle_add(from, step);
    for (int32_t k = 0x3f; k-- >= 0;) {
        swc1(a + 0, fpu_mul(cos_vu(from), lwc1(pv + 0x24)));
        swc1(a + 4, fpu_mul(sin_vu(from), lwc1(pv + 0x24)));
        sw(a + 8, 0);
        swc1(b + 0, fpu_mul(cos_vu(to), lwc1(pv + 0x24)));
        swc1(b + 4, fpu_mul(sin_vu(to), lwc1(pv + 0x24)));
        sw(b + 8, 0);
        vec3_add(V(a), V(a), V(pos));
        swc1(a + 8, fpu_add(lwc1(a + 8), 0.5f));
        vec3_add(V(b), V(b), V(pos));
        swc1(b + 8, fpu_add(lwc1(b + 8), 0.5f));
        segment_collide_v(V(a), V(b), 1, lwu(pv + 0x34), static_cast<const uint32_t*>(ee_ram_ptr(rec)));
        if (lwc1(pv + 0x24) < 3.0f) vf_ring_sparks_l18(m, a, b);
        from = angle_add(from, step);
        to = angle_add(to, step);
    }
}

// States: 0 falls (its smoke: L18:0x161c40 particles of type 2 a frame), glowing, until it lands (state 1, a frame's wait;
// gone under z 10 instead); 1 waits, then with a sound turns into the ring (state 2, radius 0.5, hidden moby); 2 widens by
// +0x20 a frame, hurting along its edge (and drawn: L18:0x2dba20), past +0x28 state 3; 3 gone. Gone too when the hero
// dies or the level's mode +0x15f5c4 is 2.
// orig: L18:0x002db460
void vf_class624_update_l18(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    if (lw(g::HERO_STATE) == 0x72 || lw(L18_ADDR(0x0015f5c4)) == 2) {
        moby_retire(m);
        return;
    }
    // the glow: its phase on, the colour between L18:0x161bfc and 0x161c00
    auto glow = [m, pv]() {
        swc1(pv + 0x30, angle_add(lwc1(pv + 0x30), fpu_mul(dt(), u2f(0x4196cbe4u))));
        const float k = fpu_mul(fpu_add(sin_vu(lwc1(pv + 0x30)), 1.0f), 0.5f);
        sw(m + 0x90, rgba_lerp(lwu(L18_ADDR(0x00161bfc)), lwu(L18_ADDR(0x00161c00)), k));
    };
    switch (lbu(m + 0x20)) {
    case 0: {
        const uint32_t vel = sp + 0x10, drift = sp + 0x20, spread = sp + 0x30, at = sp + 0x40;
        sw(sp + 0x54, pos);  // (the original keeps the position's address in its frame)
        sw(sp + 0x50, spread);
        for (int32_t n = 0; n < lw(L18_ADDR(0x00161c40));) {
            ++n;
            vec3_scale(V(vel), V(pv), lwc1(L18_ADDR(0x00161c08)));
            rand_sphere_point(spread, 0.0f, fpu_mul(lwc1(L18_ADDR(0x00161c0c)), dt()));
            vec3_add(V(vel), V(spread), V(vel));
            rand_sphere_point(at, 0.0f, 0.5f);
            vec3_add(V(at), V(at), V(pos));
            vec_clear(V(drift));
            swc1(vel + 0xc, rand_range_f(lwc1(L18_ADDR(0x00161c20)), lwc1(L18_ADDR(0x00161c24))));
            swc1(drift + 0xc, rand_range_f(lwc1(L18_ADDR(0x00161c28)), lwc1(L18_ADDR(0x00161c2c))));
            const uint32_t c0 = rgba_lerp(lwu(L18_ADDR(0x00161c30)), lwu(L18_ADDR(0x00161c34)), rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(lwu(L18_ADDR(0x00161c38)), lwu(L18_ADDR(0x00161c3c)), rand_range_f(0.0f, 1.0f));
            const float r0 = rand_range_f(0.0f, 1.0f);
            const int32_t life =
                float_to_int(frame_scale_f(fpu_add(fpu_mul(static_cast<float>(lw(L18_ADDR(0x00161c10))), r0), 1.0f)));
            const float r1 = fpu_add(rand_range_f(-lwc1(L18_ADDR(0x00161c1c)), lwc1(L18_ADDR(0x00161c1c))), 1.0f);
            const int32_t fade = float_to_int(frame_scale_f(fpu_mul(static_cast<float>(lw(L18_ADDR(0x00161c14))), r1)));
            const float r2 = fpu_add(rand_range_f(-lwc1(L18_ADDR(0x00161c1c)), lwc1(L18_ADDR(0x00161c1c))), 1.0f);
            const int32_t grow = float_to_int(frame_scale_f(fpu_mul(static_cast<float>(lw(L18_ADDR(0x00161c18))), r2)));
            const float fall = fpu_mul(fpu_mul(lwc1(L18_ADDR(0x00161c04)), dt_sq()), static_cast<float>(fade));
            swc1(drift + 8, fpu_sub(lwc1(drift + 8), fall));
            spawn_particle_02(at, vel, drift, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(fade), static_cast<uint32_t>(grow),
                              lw(L18_ADDR(0x00161c44)));
        }
        glow();
        swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(dt_sq(), 10.0f)));
        vec3_add(V(pos), V(pos), V(pv));
        const float ground = floor_probe_v(0.5f, V(pos), 0);
        const float lift = u2f(0x3eaa7efau);  // 0.333
        if (fpu_sub(lwc1(m + 0x18), lift) < ground) {
            sb(m + 0x20, 1);
            swc1(m + 0x18, fpu_add(ground, lift));
            sw(pv + 0x2c, static_cast<uint32_t>(frame_scale_i(1)));
        }
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), fpu_mul(dt(), u2f(0x410ba058u))));  // (500 degrees a second)
        if (lwc1(m + 0x18) < 10.0f) moby_retire(m);
        else floor_light_add(m, 1.0f);
        return;
    }
    case 1:
        glow();
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2c))) == 0) return;
        moby_sound_play(0xf, 0, m, 0x58e);
        sb(m + 0x20, 2);
        swc1(pv + 0x24, 0.5f);
        sw(m + 0x94, 0);
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1);
        return;
    case 2:
        swc1(pv + 0x24, fpu_add(lwc1(pv + 0x24), lwc1(pv + 0x20)));
        vf_ring_hurt_l18(m);
        sub_001f93e0(L18_ADDR(0x002dba20), m);
        if (lwc1(pv + 0x28) < lwc1(pv + 0x24)) sb(m + 0x20, 3);
        return;
    case 3:
        moby_retire(m);
        return;
    default:
        return;
    }
}

// ---- class 628: the boss's burst

// Lives +8 frames: the hero within its radius (and half a unit) is hit with a damage of 1 pushed away; its glow grows;
// every other 4 frames of the level's counter its sphere shocks for its owner +0xc; drawn by L18:0x2dc4b8.
// orig: L18:0x002dc260
void vf_class628_update_l18(uint32_t m) {
    const Frame f(176);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) != 0) {
        moby_retire(m);
        return;
    }
    if (vec2_distance(V(pos), V(HERO_POS)) < fpu_add(lwc1(pv + 0), 0.5f)) {
        hit_toward_hero(m, pos, sp, sp + 0x30, 1.0f);
        moby_hit_record(lwu(g::HERO_MOBY), sp);
    }
    swc1(pv + 4, fpu_add(lwc1(pv + 4), fpu_mul(lwc1(L18_ADDR(0x00161ca0)), dt())));
    if ((lwu(L18_ADDR(0x0015f5cc)) & 4) == 0) moby_sphere_contact(lwu(pv + 0xc), pos, 0x10000, 0, 1, 0, lwc1(pv + 0), 1.0f, 1.0f);
    sub_001f93e0(L18_ADDR(0x002dc4b8), m);
}

}  // namespace rac1
