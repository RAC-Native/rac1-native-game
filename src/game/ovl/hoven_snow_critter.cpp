// Hoven's (level 12) class 238: the small snow critters that lie buried in the snow in groups, burst out when a target comes
// near, chase it, bite and go home, can be carried (its class hooks, hoven_hooks.h) and knocked about. Level 12 only: the
// addresses are Hoven's (`orig: L12:`); Hoven-only data is L12_ADDR, the base ELF's are bare. Registered in hoven_mobys.cpp.
//
// The moby: +0x20 the state (below), +0x21 its group (0xff none), +0x31 drawn, +0x48 its yaw, +0x53 / +0x52 the animation set
// playing / asked, +0x58 the animation speed, +0x70 bit 1 the animation ended, +0xa4 the hit slot, +0xbc its group's word.
// The pvars: +0x20 health, +0x110 the colour pulse, +0x120 the move record (enemy_physics), +0x180 the walker record,
// +0x1d0 the wander record, +0x1e4 its speed, +0x200 the joint spring, +0x280 home, +0x2a0 the turn, +0x2a4 / +0x2a8 the
// heading offset while it chases (radians / degrees), +0x2ac its sight, +0x2bc buried at the start, +0x2c0 the offset's
// timer, +0x2c4 the alert timer, +0x2c8 / +0x2cc the wake and wander timers, +0x2d0 the path it keeps inside (-1 none),
// +0x2d4 the frames it made no way, +0x2d8 the frames it went home, +0x2dc, +0x2e0 the group's count of awake critters,
// +0x2e4 the frame it was last counted, +0x2e8 / +0x2ea the frames it was held away (by the hero's item).
//
// States (the table L12:0x20c2d0; 16 .. 98 and 100 up: only the tail): 0 set up, 1 at home, 3 turn to the target, 4 chase,
// 5 bite, 6 go home, 7 knocked back, 8 carried, 9 buried, 10 get up, 11 leap, 12 in the air, 13 out of the snow, 14 landed,
// 15 gone in a burst, 99 dying.
#include <game/lib/libc.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/shared_seas.h>
#include <game/ovl/metal.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/walker.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_MOBY = 0x001413d0;  // the hero's moby (a pointer)

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
int32_t trunc_w(float f) { return static_cast<int32_t>(f2u(eer::cvt_w(f))); }

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the animation set `set` unless it plays already
void anim_to(uint32_t m, uint32_t set, int32_t frame, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, frame, frames);
}

// the heading from the moby to the point `p` (an EE vector)
float heading_to(uint32_t m, uint32_t p) { return atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14))); }

// the heading offset it chases with: its sign flipped at random, then 30 .. 90 frames of it
void new_offset(uint32_t pv) {
    if ((rand_mod(0x100) & 1) != 0) swc1(pv + 0x2a8, -lwc1(pv + 0x2a8));
    swc1(pv + 0x2a4, fpu_mul(lwc1(pv + 0x2a8), u2f(0x3c8efa35u)));  // (degrees to radians)
    sw(pv + 0x2c0, static_cast<uint32_t>(frame_scale_i(trunc_w(rand_range_f(30.0f, 90.0f)))));
}

// the group's word +0xbc set to `value` (no group: nothing)
void tell_group(uint32_t m, uint32_t value) {
    if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), value);
}
}  // namespace

// One of its three walks, at random.
// orig: L12:0x002e18e8
void critter_walk_anim(uint32_t m) {
    const int32_t r = rand_mod(2);
    if (r == 0) anim_to(m, 3, 0, frame_scale_i(8));
    else if (r == 1) anim_to(m, 0xf, 0, frame_scale_i(8));
    else anim_to(m, 0x10, 0, frame_scale_i(8));
}

// The burst of snow where it comes out (`at`: an EE vector): 27 particles of type 2 about it, their speeds, sizes, colours
// and lives from the level's table L12:0x161888 .. 0x1618cc.
// orig: L12:0x002e19a8
void critter_snow_burst(uint32_t at) {
    const Frame f(240);
    const uint32_t sp = f.sp, pos = sp + 0x20, zero = sp + 0x30, vel = sp + 0x40;
    lq_sq(sp + 0x10, at);
    for (int k = 0; k < 27; ++k) {
        const float ang = rand_angle();
        lq_sq(pos, sp + 0x10);
        swc1(pos + 8, fpu_add(lwc1(pos + 8), rand_range_f(-0.25f, -0.75f)));
        swc1(pos + 0, fpu_add(lwc1(pos + 0), rand_range_f(-0.25f, 0.25f)));
        swc1(pos + 4, fpu_add(lwc1(pos + 4), rand_range_f(-0.25f, 0.25f)));
        vec_clear(V(zero));
        const float c = cos_vu(ang);
        swc1(vel + 0, fpu_mul(c, fpu_mul(rand_range_f(lwc1(L12_ADDR(0x00161898)), lwc1(L12_ADDR(0x0016189c))), dt())));
        const float s = sin_vu(ang);
        const float out = fpu_mul(rand_range_f(lwc1(L12_ADDR(0x00161898)), lwc1(L12_ADDR(0x0016189c))), dt());
        sw(vel + 8, 0);
        swc1(vel + 4, fpu_mul(s, out));
        swc1(zero + 8, fpu_mul(rand_range_f(lwc1(L12_ADDR(0x00161888)), lwc1(L12_ADDR(0x0016188c))), dt()));
        const float up = fpu_mul(rand_range_f(lwc1(L12_ADDR(0x00161890)), lwc1(L12_ADDR(0x00161894))), dt());
        swc1(zero + 0xc, lwc1(L12_ADDR(0x001618a0)));
        swc1(vel + 0xc, lwc1(L12_ADDR(0x001618a4)));
        swc1(vel + 8, up);
        const int32_t a = float_to_int(rand_range_f(static_cast<float>(lw(L12_ADDR(0x001618ac))), static_cast<float>(lw(L12_ADDR(0x001618b0)))));
        const int32_t b = float_to_int(rand_range_f(static_cast<float>(lw(L12_ADDR(0x001618b4))), static_cast<float>(lw(L12_ADDR(0x001618b8)))));
        const int32_t c3 = float_to_int(rand_range_f(static_cast<float>(lw(L12_ADDR(0x001618bc))), static_cast<float>(lw(L12_ADDR(0x001618c0)))));
        const int32_t tex = lw(L12_ADDR(0x001618cc));
        sw(sp, static_cast<uint32_t>(tex));
        spawn_particle_02(pos, zero, vel, lwu(L12_ADDR(0x001618c4)), lwu(L12_ADDR(0x001618c8)), static_cast<uint32_t>(a),
                          static_cast<uint32_t>(b), static_cast<uint32_t>(c3), tex);
    }
}

// Down onto the floor below it, at most 27 x the frame's square a frame.
// orig: L12:0x002e1878
void critter_to_floor(uint32_t m) {
    Vec4 q = V(m + 0x10);
    q.z = fpu_add(q.z, 0.5f);
    const float floor = floor_probe_v(0.5f, q, 0);
    approach_f(m + 0x18, floor, fpu_mul(dt_sq(), 27.0f));
}

// A step of its walk toward the heading `yaw`, kept inside its path (+0x2d0) when it has one; 2 when it made no way for more
// than 10 frames (stuck), else the walker's result.
// orig: L12:0x002e2eb8
uint32_t critter_walk(uint32_t m, float yaw) {
    const Frame f(224);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    lq_sq(sp, m + 0x10);  // (where it was)
    critter_to_floor(m);
    yaw_turn_damped(m, pv + 0x2a0, yaw, 0.05f, u2f(0x3e99999au), u2f(0x3e4ccccdu));
    const float c = cos_vu(lwc1(m + 0x48));
    swc1(sp + 0x10, fpu_add(c, c));
    const float s = sin_vu(lwc1(m + 0x48));
    sw(sp + 0x18, 0);
    swc1(sp + 0x14, fpu_add(s, s));
    uint32_t result = walker_record_step(1.0f, m, pv + 0x180, sp + 0x10, sp + 0x20);
    if (lw(pv + 0x2d0) == -1) return result;
    const uint32_t back = sp + 0x50, probe = sp + 0x40, cross = sp + 0x30, pos = m + 0x10;
    vec3_sub(V(back), V(sp), V(pos));
    vec3_set_length(V(back), V(back), 0.05f);
    vec3_add(V(probe), V(sp), V(back));
    if (path_crossing(lw(pv + 0x2d0), pos, probe, cross) == 0) return result;
    const uint32_t path = lvd::path(lw(pv + 0x2d0));
    if (point_in_polygon(pos, path + 0x10, lw(path)) == 0) lq_sq(pos, cross);
    if (vec3_distance(V(pos), V(sp)) < fpu_mul(dt(), 0.5f)) {
        sw(pv + 0x2d4, lwu(pv + 0x2d4) + 1);
        if (frame_scale_i(10) < lw(pv + 0x2d4)) result = 2;
    } else {
        sw(pv + 0x2d4, 0);
    }
    return result;
}

// The bits it flies apart into (when hit, and twice when it goes in a burst): `count` (L12:0x16183c; 1.5 x as many in
// state 15) chunks from about its middle, each with a trail of type-2 particles (L12:0x161840 of them); the speeds,
// colours and lives from the level's table L12:0x161844 .. 0x161884.
// orig: L12:0x002e3098
void critter_debris(uint32_t m) {
    const Frame f(320);
    const uint32_t sp = f.sp, pos = sp + 0x10, vel = sp + 0x20, acc = sp + 0x30, drift = sp + 0x40;
    const uint32_t pv = lwu(m + 0x78);
    int32_t count = lw(L12_ADDR(0x0016183c));
    if (lbu(m + 0x20) == 0xf) count = float_to_int(fpu_mul(static_cast<float>(count), 1.5f));
    const float k = u2f(0x3f0ccccdu);  // 0.55
    for (int32_t i = 0; i < count; ++i) {
        lq_sq(pos, m + 0x10);
        metal_25f8e0(pos, lwc1(L12_ADDR(0x00161844)));
        swc1(pos + 8, fpu_add(lwc1(pos + 8), k));
        const float ang = rand_angle();
        float out = rand_range_f(lwc1(L12_ADDR(0x00161850)), lwc1(L12_ADDR(0x00161854)));
        float up = rand_range_f(lwc1(L12_ADDR(0x00161858)), lwc1(L12_ADDR(0x0016185c)));
        if (lbu(m + 0x20) == 0xf) {
            out = fpu_mul(out, k);
            up = fpu_mul(up, k);
            swc1(pos + 8, fpu_sub(lwc1(pos + 8), u2f(0x3e4ccccdu)));
        }
        const float c = cos_vu(ang);
        swc1(vel + 0, fpu_mul(c, fpu_mul(out, dt())));
        const float s = sin_vu(ang);
        const float t = dt();
        const float z = fpu_add(fpu_mul(up, t), 0.0f);
        swc1(vel + 8, z);
        swc1(vel + 4, fpu_mul(s, fpu_mul(out, t)));
        vec3_scale(V(drift), V(pv + 0x120), 0.5f);
        swc1(drift + 8, 0.0f);
        vec3_add(V(vel), V(vel), V(drift));
        lq_sq(acc, vel);
        const float frames = static_cast<float>(frame_scale_i(lw(L12_ADDR(0x00161880))));
        swc1(acc + 8, fpu_sub(lwc1(acc + 8), fpu_mul(fpu_mul(lwc1(L12_ADDR(0x00161860)), dt_sq()), frames)));
        for (int32_t j = 0; j < lw(L12_ADDR(0x00161840)); ++j) {
            const float r = rand_range_f(0.5f, 1.5f);
            swc1(vel + 0xc, fpu_mul(r, lwc1(L12_ADDR(0x00161864))));
            swc1(acc + 0xc, fpu_mul(r, lwc1(L12_ADDR(0x00161868))));
            metal_25f8e0(pos, lwc1(L12_ADDR(0x00161848)));
            metal_25f8e0(acc, fpu_mul(lwc1(L12_ADDR(0x0016184c)), dt()));
            const uint32_t c0 = rgba_lerp(lwu(L12_ADDR(0x0016186c)), lwu(L12_ADDR(0x00161874)), rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(lwu(L12_ADDR(0x00161870)), lwu(L12_ADDR(0x00161878)), rand_range_f(0.0f, 1.0f));
            const int32_t life = frame_scale_i(lw(L12_ADDR(0x0016187c)));
            const int32_t fade = frame_scale_i(lw(L12_ADDR(0x00161880)));
            const float base = static_cast<float>(lw(L12_ADDR(0x00161884)));
            const int32_t size = float_to_int(frame_scale_f(rand_range_f(fpu_mul(base, 0.5f), fpu_mul(base, 2.5f))));
            sw(sp, 0xffffffffu);
            spawn_particle_02(pos, vel, acc, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(fade),
                              static_cast<uint32_t>(size), -1);
        }
    }
}

// Its hits (masks 0x330000) and the hero's item that holds it away: the alert's sight (20 while the alert timer runs,
// else 12), the push away from the hero after a long hold, the knock-back (7) or the death (99) with its bits.
// orig: L12:0x002e3528
void critter_hits(uint32_t m) {
    const Frame f(144);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    if (lwu(pv + 0x38) != 0) {
        sw(pv + 0x38, 0);
        sw(pv + 0x2c4, static_cast<uint32_t>(frame_scale_i(0xf0)));
    }
    const uint32_t st = lbu(m + 0x20);
    if (st != 9 && st != 0xb && st != 0) {
        const int32_t idle = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2c4)));
        swc1(pv + 0x2ac, idle == 0 ? 20.0f : 12.0f);
    }
    if (lh(pv + 0x2e8) != 0 && lbu(m + 0x20) != 9) {
        const uint32_t held = lhu(pv + 0x2ea) + 1;
        sh(pv + 0x2ea, held);
        const int32_t frames = frame_scale_i(0x3c);
        if (frames * 15 < static_cast<int16_t>(held) && lbu(m + 0x31) == 0 && lbu(m + 0x21) < 0xff &&
            moby_group_count_l1(lbu(m + 0x21), -1) >= 4 && rand_mod(0x45) == 0) {
            const float kPi = u2f(0x40490fdbu);
            swc1(sp + 0, cos_vu(angle_add(lwc1(m + 0x48), kPi)));
            swc1(sp + 4, sin_vu(angle_add(lwc1(m + 0x48), kPi)));
            sw(sp + 8, 0);
            sub_00259bc8(m, lwu(HERO_MOBY), 0x10000, m + 0x10, sp, 1.0f);
        }
    }
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    swc1(sp + 0x14, 0.0f);
    {
        int32_t state = lw(sp + 0x10);
        float dmg = lwc1(sp + 0x14);
        moby_hit_react_r(m, hit, pv + 0x20, 0, state, &dmg, nullptr, 4);
        sw(sp + 0x10, static_cast<uint32_t>(state));
        swc1(sp + 0x14, dmg);
    }
    if (hit != 0 && lbu(m + 0x20) != 0x63 && lbu(m + 0x20) != 8) {
        tell_group(m, 7);
        const float dmg = lwc1(sp + 0x14);
        if (dmg != 0.0f) {
            const float t = dt();
            const float health = fpu_sub(lwc1(pv + 0x20), dmg);
            swc1(pv + 0x130, u2f(0x3c03126fu));
            swc1(pv + 0x134, u2f(0x3a03126fu));
            swc1(pv + 0x138, fpu_mul(lwc1(L12_ADDR(0x001618d4)), t));
            swc1(pv + 0x13c, fpu_mul(lwc1(L12_ADDR(0x001618d0)), t));
            sw(pv + 0x144, 9);
            swc1(pv + 0x158, 1.0f);
            swc1(pv + 0x20, health);
            sb(pv + 0x15d, 0);
            swc1(pv + 0x150, 1.0f);
            swc1(pv + 0x154, 1.0f);
            if (health <= 0.0f) {  // killed: thrown up and away, then dying (99)
                sh(m + 0x34, lhu(m + 0x34) & 0xefff);
                swc1(pv + 0x138, fpu_mul(dt(), 8.0f));
                swc1(pv + 0x13c, fpu_mul(dt(), 10.0f));
                lq_sq(sp, hit + 0x10);
                vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x18)), *static_cast<float*>(ee_ram_ptr(pv + 0x138)),
                                 *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
                move_state_start(m, pv + 0x120, 6, 1, 0, lwc1(sp + 0x18));
                swc1(pv + 0x170, 11.0f);
                swc1(pv + 0x174, 18.0f);
                sb(m + 0x20, 0x63);
                sb(pv + 0x117, 0x78);
                color_pulse_start(m, pv + 0x110);
            } else {  // knocked back (7)
                lq_sq(sp, hit + 0x10);
                vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(sp + 0x1c)), *static_cast<float*>(ee_ram_ptr(pv + 0x138)),
                                 *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
                move_state_start(m, pv + 0x120, 0xe, 1, 0, lwc1(sp + 0x1c));
                swc1(pv + 0x174, -1.0f);
                swc1(pv + 0x170, -1.0f);
                sb(m + 0x20, 7);
                sb(pv + 0x117, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                color_pulse_start(m, pv + 0x110);
            }
            critter_debris(m);
        }
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x110);
}

// The update. Below z 26 it is gone; the rest by the state (the table L12:0x20c2d0), then the tail: the joint's spring and,
// near the camera, the floor band (not while buried or carried).
// orig: L12:0x002e1be0
void hov_class238_update_l12(uint32_t m) {
    const Frame f(240);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    critter_hits(m);
    moby_set_70(pv + 0x200, u2f(0x400ccccdu));
    target_select_any(m, sp, lwc1(pv + 0x2ac));
    uint32_t tgt = lwu(sp + 0x40);
    if (tgt == 0) tgt = lwu(HERO_MOBY);
    const uint32_t home = pv + 0x280;

    // what follows a state's part: the tail, a plain return, or retired
    enum class Next { Tail, Return, Retire };
    // into the chase (4), a walk picked
    auto start_chase = [m, pv]() {
        sb(m + 0x20, 4);
        sw(pv + 0x2d4, 0);
        critter_walk_anim(m);
    };
    auto run = [&]() -> Next {
        if (lwc1(m + 0x18) < 26.0f) return Next::Retire;
        const uint32_t st = lbu(m + 0x20);
        if (st >= 100) return Next::Tail;
        switch (st) {
        case 0: {  // set up: buried (9), in the air (11) or at home (1) (0x2e1c94)
            if (lwu(pv + 0x2bc) != 0) {
                lq_sq(home, m + 0x10);
                sb(m + 0x20, 9);
                anim_to(m, 0xc, 0, 3);
                sw(m + 0x94, 0);
                sh(m + 0x34, (lhu(m + 0x34) | 0x41) & 0xefff);
            } else if (lwc1(pv + 0x28c) != 0.0f) {
                sb(m + 0x20, 0xb);
                sh(m + 0x32, 0);
                sh(m + 0x34, lhu(m + 0x34) & 0xefff);
                sw(m + 0x94, 0);
            } else {
                lq_sq(home, m + 0x10);
                sb(m + 0x20, 1);
                anim_to(m, 0, 0, 3);
            }
            sw(pv + 0x2c4, 0);
            sw(pv + 0x2b8, 0);
            swc1(pv + 0x148, 0.5f);
            swc1(pv + 0x20, 2.0f);
            sb(pv + 0x5a, 5);
            sw(pv + 0x140, 0x200);
            sb(pv + 0x58, 8);
            sb(pv + 0x29, 1);
            walker_record_init_l1(pv + 0x180);
            sw(pv + 0x180, 0x38d);
            sw(pv + 0x1b8, lwu(pv + 0x1b8) | 0x10);
            swc1(pv + 0x1a4, fpu_mul(dt(), 5.5f));
            swc1(pv + 0x18c, 0.5f);
            swc1(pv + 0x188, 0.5f);
            if (lwu(pv + 0x2dc) != 0) sh(pv + 0x2c8, static_cast<uint32_t>(rand_range(frame_scale_i(0x14), frame_scale_i(0xaa))));
            lq_sq(pv + 0x1d0, m + 0x10);
            sh(pv + 0x2cc, static_cast<uint32_t>(rand_range(frame_scale_i(0x78), frame_scale_i(0x2bc))));
            lq_sq(sp, m + 0x10);
            swc1(sp + 8, fpu_add(lwc1(sp + 8), u2f(0x3f4ccccdu)));
            swc1(m + 0x18, floor_probe_v(0.5f, V(sp), 0));
            if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000);
            sw(pv + 0xd0, L12_ADDR(0x001618e8));
            return Next::Tail;
        }
        case 1: {  // at home: turn to a target in sight (3), now and then go round (3), else wander (0x2e1e94)
            const float dist = vec3_distance(V(tgt + 0x10), V(home));
            const float off = angle_dist(lwc1(m + 0x48), heading_to(m, tgt + 0x10));
            if (dist < lwc1(pv + 0x2ac) && abs_f(fpu_sub(lwc1(m + 0x18), lwc1(tgt + 0x18))) < 4.0f) {
                sb(m + 0x20, 3);
                if (off < u2f(0x3f75be0bu)) {
                    anim_to(m, 2, 0, frame_scale_i(5));
                    swc1(m + 0x58, u2f(0x3fc66666u));
                } else {
                    anim_to(m, 0x11, 0, frame_scale_i(5));
                }
                return Next::Tail;
            }
            if (((lbu(m + 0xbc) & 1) != 0 && rand_mod(0x13) == 0) || lbu(m + 0xbc) == 7) {
                new_offset(pv);
                sb(m + 0x20, 3);
                anim_to(m, 2, 0, frame_scale_i(7));
                sb(m + 0xbc, 0);
                return Next::Tail;
            }
            anim_to(m, 0xf, 0, frame_scale_i(0xe));
            const int32_t wander = lh(pv + 0x2cc);
            if (frame_scale_i(0x14) < wander) approach_f(pv + 0x1e4, fpu_mul(dt(), 2.5f), fpu_mul(dt_sq(), 7.0f));
            else approach_f(pv + 0x1e4, 0.0f, fpu_mul(dt_sq(), 7.0f));
            swc1(m + 0x58, u2f(0x3f4ccccdu));
            wander_update(m, pv + 0x1d0, u2f(0x3eb33333u), 0.25f);
            return Next::Tail;
        }
        case 9: {  // buried: out of the snow (13) when a target comes near, its group says so or its wake timer ends (0x2e2138)
            const uint32_t pos = m + 0x10, at = tgt + 0x10;
            if (lwu(pv + 0x2dc) != 0) {
                if (vec2_distance(V(pos), V(at)) < 2.0f) return Next::Tail;
                const int32_t now = lw(L12_ADDR(0x0015f5cc));
                if (lw(pv + 0x2e4) < now) {  // (once a frame: its group's critters out of the snow counted)
                    sw(pv + 0x2e4, static_cast<uint32_t>(now));
                    const uint32_t it = sp + 0x50;
                    int32_t awake = 0;
                    group_first(it, lbu(m + 0x21), 0, 0);
                    while (lwu(it) != 0) {
                        const uint32_t o = lwu(it);
                        if (static_cast<int8_t>(lbu(o + 0x20)) >= 0 && lbu(o + 0x20) != 9) ++awake;
                        group_next(it, o, 0, 0);
                    }
                    sw(lwu(pv + 0x2e0), static_cast<uint32_t>(awake));
                }
                if (lw(lwu(pv + 0x2e0)) >= 6) return Next::Tail;
            }
            vec3_sub(V(sp), V(at), V(pos));
            bool wake = false;
            if (vec3_length(V(sp)) < lwc1(pv + 0x2ac) && abs_f(fpu_sub(lwc1(m + 0x18), lwc1(tgt + 0x18))) < 8.0f && rand_mod(0x13) == 0) {
                tell_group(m, 1);
                wake = true;
            } else if ((lbu(m + 0xbc) & 1) != 0 && timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x2c8))) != 0) {
                wake = true;
            } else if (lbu(m + 0xbc) == 7) {
                wake = true;
            }
            if (!wake) return Next::Tail;
            sb(m + 0x20, 0xd);
            anim_to(m, 7, 0, 1);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000);
            lq_sq(sp + 0x10, m + 0x10);
            critter_snow_burst(sp + 0x10);
            return Next::Tail;
        }
        case 0xd: {  // out of the snow: turn to the target, then chase (0x2e2308)
            sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            yaw_turn_damped(m, pv + 0x2a0, heading_to(m, tgt + 0x10), u2f(0x3cf5c28fu), u2f(0x3e99999au), fpu_mul(dt(), u2f(0x40c90fdbu)));
            if ((lbu(m + 0x70) & 2) == 0) return Next::Tail;
            new_offset(pv);
            start_chase();
            sb(m + 0xbc, 0);
            return Next::Tail;
        }
        case 0xb: {  // the leap: toward home, aimed to land there (12) (0x2e2408)
            yaw_turn_damped(m, pv + 0x2a0, heading_to(m, home), 0.05f, u2f(0x3e99999au), u2f(0x3e4ccccdu));
            vec3_sub(V(sp), V(tgt + 0x10), V(home));
            const bool near = vec3_length(V(sp)) < lwc1(pv + 0x2ac) && abs_f(fpu_sub(lwc1(pv + 0x288), lwc1(tgt + 0x18))) < 8.0f;
            if (!near && lbu(m + 0xbc) != 2) return Next::Tail;
            tell_group(m, 2);
            const float g = lwc1(L12_ADDR(0x001618dc));
            const float speed = fpu_mul(lwc1(L12_ADDR(0x001618d8)), dt());
            sh(m + 0x32, 0x40);
            sw(pv + 0x134, 0);
            swc1(pv + 0x130, fpu_mul(g, dt_sq()));
            swc1(pv + 0x138, speed);
            float t = lwc1(sp + 0x54);
            swc1(pv + 0x13c, sub_0025abf0(m + 0x10, home, &t, speed, -g));
            swc1(sp + 0x54, t);
            sw(pv + 0x144, 5);
            sb(pv + 0x15d, 0);
            move_state_start(m, pv + 0x120, 8, 1, 0, lwc1(m + 0x48));
            swc1(pv + 0x170, 5.0f);
            swc1(pv + 0x174, 10.0f);
            sb(m + 0x20, 0xc);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            return Next::Tail;
        }
        case 0xc: {  // in the air: landed (14), or fallen below 0 (0x2e2594)
            if ((enemy_physics(m, pv + 0x120) & 1) != 0) {
                sb(m + 0x20, 0xe);
                anim_to(m, 9, 8, frame_scale_i(7));
                return Next::Return;
            }
            return lwc1(m + 0x18) < 0.0f ? Next::Retire : Next::Tail;
        }
        case 0xe: {  // landed: the landing played out, chase (0x2e25f8)
            if ((lbu(m + 0x70) & 2) == 0) return Next::Tail;
            sb(m + 0x20, 4);
            sw(pv + 0x2d4, 0);
            new_offset(pv);
            critter_walk_anim(m);
            return Next::Tail;
        }
        case 3: {  // turn to the target, then chase (0x2e2680)
            yaw_turn_damped(m, pv + 0x2a0, heading_to(m, tgt + 0x10), u2f(0x3cf5c28fu), u2f(0x3e99999au), fpu_mul(dt(), u2f(0x40c90fdbu)));
            if ((lbu(m + 0x70) & 2) == 0) return Next::Tail;
            start_chase();
            return Next::Tail;
        }
        case 4: {  // chase: on its offset heading; close: bite (5); stuck or the target far: home (6) (0x2e26f4)
            const float yaw = heading_to(m, tgt + 0x10);
            const float kLimit = u2f(0x3f32b8c2u);  // 40 degrees
            float turn = angle_sub(yaw, lwc1(m + 0x48));
            if (kLimit < turn) turn = kLimit;
            else if (turn < -kLimit) turn = -kLimit;
            swc1(pv + 0x268, turn);
            auto go_home = [m, pv]() {
                sb(m + 0x20, 6);
                sw(pv + 0x2d8, 0);
            };
            if (critter_walk(m, angle_add(yaw, lwc1(pv + 0x2a4))) == 2) {
                go_home();
                sb(m + 0xbc, 0);
                return Next::Tail;
            }
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2c0))) != 0) {
                sw(pv + 0x2c0, static_cast<uint32_t>(frame_scale_i(trunc_w(rand_range_f(30.0f, 90.0f)))));
                swc1(pv + 0x2a4, -lwc1(pv + 0x2a4));
            }
            const uint32_t at = tgt + 0x10;
            vec3_sub(V(sp), V(at), V(home));
            if (vec2_distance(V(m + 0x10), V(at)) < 1.5f) {
                sb(m + 0x20, 5);
                anim_to(m, 4, 0, frame_scale_i(5));
            } else if (vec3_length(V(sp)) < fpu_add(lwc1(pv + 0x2ac), 5.5f) && abs_f(fpu_sub(lwc1(m + 0x18), lwc1(tgt + 0x18))) < u2f(0x408ccccdu)) {
                tell_group(m, 1);
            } else if (lbu(m + 0xbc) != 1 && rand_mod(9) == 0) {
                go_home();
            }
            sb(m + 0xbc, 0);
            return Next::Tail;
        }
        case 5: {  // the bite: at its frame 13, a target close in front is pushed; then chase again unless still close (0x2e28e8)
            swc1(lwu(HERO_MOBY) + 0x58, u2f(0x3fa66666u));
            const float k = u2f(0x3e4ccccdu);  // 0.2
            yaw_turn_damped(m, pv + 0x2a0, heading_to(m, tgt + 0x10), 0.05f, u2f(0x3e99999au), k);
            const uint32_t at = tgt + 0x10;
            if (lbu(m + 0x52) == lbu(m + 0x53) && anim_crossed(m, 13.0f) != 0 && abs_f(fpu_sub(lwc1(m + 0x18), lwc1(tgt + 0x18))) < 0.5f &&
                vec2_distance(V(m + 0x10), V(at)) < 2.0f && angle_dist(heading_to(m, at), lwc1(m + 0x48)) < u2f(0x3e860a92u)) {
                swc1(sp + 0, fpu_mul(cos_vu(lwc1(m + 0x48)), k));
                const float s = fpu_mul(sin_vu(lwc1(m + 0x48)), k);
                sw(sp + 8, 0);
                swc1(sp + 4, s);
                lq_sq(sp + 0x10, at);
                swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), 0.75f));
                sub_00259bc8(tgt, m, 1, sp + 0x10, sp, 1.0f);
            }
            if ((lbu(m + 0x70) & 2) == 0) return Next::Tail;
            if (2.0f <= vec2_distance(V(m + 0x10), V(at))) start_chase();
            return Next::Tail;
        }
        case 6: {  // home: walk back; there: at home (1); a target in sight after 70 frames, or the group's word: chase (0x2e2ac4)
            critter_walk(m, heading_to(m, home));
            if (vec2_distance(V(m + 0x10), V(home)) < 2.0f) {
                sb(m + 0x20, 1);
                anim_to(m, 0, 0, frame_scale_i(7));
            }
            const float dist = vec3_distance(V(tgt + 0x10), V(home));
            sw(pv + 0x2d8, lwu(pv + 0x2d8) + 1);
            bool outside = false;
            if (lw(pv + 0x2d0) != -1) {  // (the hero outside its path)
                const uint32_t path = lvd::path(lw(pv + 0x2d0));
                outside = point_in_polygon(0x0013f3d0, path + 0x10, lw(path)) == 0;
            }
            if (!outside) {
                if (dist < lwc1(pv + 0x2ac)) {
                    if (frame_scale_i(0x46) < lw(pv + 0x2d8)) {
                        tell_group(m, 1);
                        start_chase();
                    }
                } else if (lbu(m + 0xbc) == 1) {
                    start_chase();
                }
            }
            sb(m + 0xbc, 0);
            return Next::Tail;
        }
        case 7: {  // knocked back: landed, get up (10); fallen below 27: gone (0x2e2c18)
            tell_group(m, 1);
            if ((enemy_physics(m, pv + 0x120) & 1) != 0) {
                sb(m + 0x20, 0xa);
                anim_to(m, 0xd, 8, frame_scale_i(8));
                return Next::Return;
            }
            return lwc1(m + 0x18) < 27.0f ? Next::Retire : Next::Tail;
        }
        case 8: {  // carried: let go, home (6) (0x2e2ca4)
            if (carry_update(m, pv + 0x120) != 0) {
                sb(m + 0x20, 6);
                sw(pv + 0x2d8, 0);
                sh(pv + 0xc8, 0);
            }
            return Next::Tail;
        }
        case 0xa: {  // get up: past frame 29, chase (0x2e2cc8)
            if (29.0f < moby_anim_time(m)) {
                start_chase();
                return Next::Return;
            }
            return Next::Tail;
        }
        case 0x63: {  // dying: on the ground or the fall played out, its bolts and the burst (15); fallen below 10: gone (0x2e2d04)
            const uint32_t phys = enemy_physics(m, pv + 0x120);
            if ((phys & 0x40) != 0 || (37.0f < moby_anim_time(m) && lbu(m + 0x52) == lbu(m + 0x53))) {
                sw(m + 0x58, 0);
                moby_drop_bolts(m, 0, -1);
                sb(m + 0x20, 0xf);
                sh(m + 0x34, lhu(m + 0x34) | 8);
                return Next::Tail;
            }
            if (lwc1(m + 0x18) < 10.0f) {
                moby_drop_bolts(m, 0, -1);
                return Next::Retire;
            }
            if ((phys & 1) != 0) particle34_puff_1(m);
            return Next::Tail;
        }
        case 0xf:  // the burst: a puff and its bits twice, gone (0x2e2dcc)
            particle34_puff_1(m);
            critter_debris(m);
            critter_debris(m);
            return Next::Retire;
        default:  // 2, 16 .. 98 (0x2e2df4)
            return Next::Tail;
        }
    };
    const Next next = run();
    if (next == Next::Retire) {
        moby_retire(m);
        return;
    }
    if (next == Next::Return) return;
    // the tail (0x2e2df4)
    if (lbu(m + 0x31) == 0) return;
    if (lbu(m + 0x20) == 9 || lbu(m + 0x20) == 8) return;
    const float k = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0x200, 0, fpu_mul(k, u2f(0x3c9374bcu)), fpu_mul(k, u2f(0x3e99999au)));
    if (lbu(m + 0x31) == 0) return;
    if (vec3_distance(V(m + 0x10), V(L12_ADDR(0x00167240))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
}

}  // namespace rac1
