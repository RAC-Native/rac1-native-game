// Novalis's own moby classes: the rock pieces of the level (hand-ported from Novalis's own overlay). Header novalis_mobys2.h,
// registered in novalis_mobys2.cpp. Addresses are Novalis's (`orig: L1:`); Novalis-only data is L1_ADDR, the base ELF's
// are bare (the hero 0x13f350, the frame time 0x15ed6c / its square 0x15ed70, the level number 0x15ed84, the per-level
// flag words 0x14c190).
//   class 700  a source of falling rocks and dust (the rocks rained by shared_rock.cpp's rock_spawn)
//   class 701  a rock that rumbles once the hero stands by its box, then drops 20 units and stays down (a story flag)
//   class 704  a rock that bursts when hit (hit mask 0x10000): dust, 40 rocks, its bolts
//   class 709  a rock that bursts when a shell of class 686 hits it (20 rocks; a story flag)
//   class 729  a big rock that bursts when hit (hit mask 0x800000): dust, 100 rocks (a story flag)
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/hydro.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/shared_rock.h>

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

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
int32_t tick16(uint32_t a) { return timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(a))); }  // 1 idle, 0 running, 2 expired
int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }

// the moby's story flag (its index +0xb2): the level's byte (Novalis's 0x1bb6b0 + index + 0x454), or the bit of the
// level's flag words (0x14c190, 0x100 bytes a level)
uint32_t flag_word(uint32_t m) {
    const int32_t idx = lh(m + 0xb2);
    return 0x0014c190 + (lwu(0x0015ed84) << 8) + (static_cast<uint32_t>(idx >> 5) << 2);
}
bool done_before(uint32_t m) {
    const int32_t idx = lh(m + 0xb2);
    if (lbu(L1_ADDR(0x001bb6b0) + static_cast<uint32_t>(idx) + 0x454) != 0) return true;
    return ((lw(flag_word(m)) >> (lhu(m + 0xb2) & 0x1f)) & 1) != 0;
}
// the flag set: the level's word and the bit of Novalis's own words (0x1ba950)
void mark_done(uint32_t m) {
    const uint32_t bit = 1u << (lhu(m + 0xb2) & 0x1f);
    const uint32_t w = flag_word(m);
    sw(w, lwu(w) | bit);
    const uint32_t own = L1_ADDR(0x001ba950) + (static_cast<uint32_t>(lh(m + 0xb2) >> 5) << 2);
    sw(own, lwu(own) | bit);
}

// one of the three rock classes of a table of three words (the level's data)
uint32_t rock_class(uint32_t table) { return lwu(table + (static_cast<uint32_t>(rand_mod(3)) << 2)); }

// (cos a, sin a, 0) r for a random angle a, r taken first
Vec4 ring_offset(float r) {
    const float a = rand_angle();
    const float x = fpu_mul(cos_vu(a), r);
    return Vec4{x, fpu_mul(sin_vu(a), r), 0.0f, 0.0f};
}

// the dust of the bursting rocks: `n` puffs (particle 0x16) about the rock, spread (x, y) along its heading, their speed
// (lo .. hi units a frame), size (x 210000) and life (frames)
void burst_dust(uint32_t m, uint32_t sp, int n, float spread_x, float spread_y, float z_hi, float lo, float hi, float size_lo,
                float size_hi, int32_t f_lo, int32_t f_hi) {
    for (int i = 0; i < n; ++i) {
        V(sp + 0x10) = Vec4{};
        swc1(sp + 0x10, rand_range_f(-1.0f, 1.0f));
        swc1(sp + 0x14, rand_range_f(-1.0f, 1.0f));
        swc1(sp + 0x18, rand_range_f(-1.0f, 1.0f));
        V(sp + 0x20) = Vec4{};
        V(sp) = V(sp + 0x10);  // the direction
        swc1(sp + 0x20, fpu_mul(rand_range_f(-spread_x, spread_x), cos_vu(lwc1(m + 0x48))));
        swc1(sp + 0x24, fpu_mul(rand_range_f(-spread_y, spread_y), sin_vu(lwc1(m + 0x48))));
        swc1(sp + 0x28, rand_range_f(0.0f, z_hi));
        V(sp + 0x10) = V(sp + 0x20);
        vec3_add(V(sp + 0x10), V(sp + 0x10), V(m + 0x10));
        vec3_set_length(V(sp), V(sp), fpu_mul(rand_range_f(lo, hi), dt()));
        const float size = fpu_mul(rand_range_f(size_lo, size_hi), 210000.0f);
        const int32_t frames = frame_scale_i(rand_range(f_lo, f_hi));
        particle_16_spawn(sp + 0x10, sp, 0x5f787878u, NOT_ADDR(0x00181818u), frames, size);
    }
}

// the rocks of a burst: `n` of the table's classes about the rock (spread (x, y) along its heading, z from z_lo .. 2 or 8),
// thrown at 1 .. 5 units a frame, scale `s_lo` .. `s_hi`, life 60 .. 180 frames
void burst_rocks(uint32_t m, uint32_t sp, int n, uint32_t table, float spread_x, float spread_y, float z_lo, float z_hi, float s_lo,
                 float s_hi) {
    for (int i = 0; i < n; ++i) {
        V(sp + 0x20) = Vec4{};
        swc1(sp + 0x20, rand_range_f(-1.0f, 1.0f));
        swc1(sp + 0x24, rand_range_f(-1.0f, 1.0f));
        swc1(sp + 0x28, rand_range_f(-1.0f, 1.0f));
        V(sp + 0x30) = Vec4{};
        V(sp + 0x10) = V(sp + 0x20);  // the direction
        swc1(sp + 0x30, fpu_mul(rand_range_f(-spread_x, spread_x), cos_vu(lwc1(m + 0x48))));
        swc1(sp + 0x34, fpu_mul(rand_range_f(-spread_y, spread_y), sin_vu(lwc1(m + 0x48))));
        swc1(sp + 0x38, rand_range_f(z_lo, z_hi));
        V(sp + 0x20) = V(sp + 0x30);
        vec3_add(V(sp + 0x20), V(sp + 0x20), V(m + 0x10));
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), fpu_mul(rand_range_f(1.0f, 5.0f), dt()));
        const uint32_t cls = rock_class(table);
        const float scale = rand_range_f(s_lo, s_hi);
        const int32_t life = rand_range(60, 180);
        rock_spawn(sp + 0x20, sp + 0x10, cls, life, 0, scale, 1.0f, 1.0f, 0.75f);
    }
}

// a rock rained at `home` + a random point of radius r_lo .. r_hi, drifting (x, y) by up to half a unit a frame, life 180 ..
// 300 frames, scale 0.05
void rain_rock(uint32_t sp, uint32_t home, float r_lo, float r_hi) {
    V(sp + 0x20) = Vec4{};
    swc1(sp + 0x20, rand_range_f(fpu_mul(dt(), -0.5f), fpu_mul(dt(), 0.5f)));
    swc1(sp + 0x24, rand_range_f(fpu_mul(dt(), -0.5f), fpu_mul(dt(), 0.5f)));
    V(sp + 0x10) = V(sp + 0x20);  // the drift
    const float r = rand_range_f(r_lo, r_hi);
    V(sp + 0x20) = ring_offset(r);
    vec3_add(V(sp + 0x20), V(sp + 0x20), V(home));
}
}  // namespace

// Class 700: while its timer +0 runs nothing (it resets the rain's +4 to 120 frames when idle); with +0 zero it does nothing at
// all. Else, while +6 is out, a rock now and then (5 %) about it 0 .. 0.25 away and +0xc higher; every frame two puffs of dust
// (particle 0x10, 70000 .. 140000) rising from it, their height half a unit below the source.
// orig: L1:0x002f8c58
void nov_class700_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    tick16(pv + 4);
    tick16(pv + 6);
    if (lwu(pv) == 0) return;
    if (tick32(pv) != 0) sh(pv + 4, static_cast<uint32_t>(frame_scale_i(120)));
    const Frame f(0x100);
    const uint32_t sp = f.sp;
    if (lh(pv + 6) == 0 && rand_mod(100) < 5) {
        rain_rock(sp, m + 0x10, 0.0f, 0.25f);
        swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), lwc1(pv + 0xc)));
        const uint32_t cls = rock_class(L1_ADDR(0x00161c08));
        const int32_t life = rand_range(180, 300);
        rock_spawn(sp + 0x20, sp + 0x10, cls, life, 0, u2f(0x3d4ccccdu), 1.0f, 1.0f, 0.75f);
    }
    for (int i = 0; i < 2; ++i) {
        V(sp + 0x30) = Vec4{};
        swc1(sp + 0x30, rand_range_f(fpu_mul(dt(), -0.5f), fpu_mul(dt(), 0.5f)));
        swc1(sp + 0x34, rand_range_f(fpu_mul(dt(), -0.5f), fpu_mul(dt(), 0.5f)));
        swc1(sp + 0x38, rand_range_f(fpu_mul(dt(), 0.0f), fpu_mul(dt(), 3.0f)));
        V(sp + 0x10) = V(sp + 0x30);  // the velocity
        const float r = rand_range_f(0.0f, 0.25f);
        V(sp + 0x30) = ring_offset(r);
        vec3_add(V(sp + 0x30), V(sp + 0x30), V(m + 0x10));
        const float rise = fpu_mul(fpu_mul(r, dt()), 8.0f);
        swc1(sp + 0x38, fpu_add(lwc1(sp + 0x38), lwc1(pv + 0xc)));
        swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), rise));
        const float size = rand_range_f(70000.0f, 140000.0f);
        const int32_t frames = frame_scale_i(180);
        const uint32_t tex = lwu(L1_ADDR(0x00161c18) + (static_cast<uint32_t>(rand_mod(3)) << 2));
        const uint32_t p = spawn_particle_10(sp + 0x30, sp + 0x10, 0x0f081020u, 0x00081020u, static_cast<uint32_t>(frames), tex, size);
        if (p != 0) swc1(p + 0x2c, fpu_sub(lwc1(m + 0x18), 0.5f));
    }
}

// Class 701's camera while it rumbles and drops: turned toward the direction of its box (+0x80) rotated about the box's
// axis, the distance, height and look height of Novalis's table 0x161c28 ..
// orig: L1:0x002f9000
void nov_2f9000_l1(uint32_t m) {
    const Frame f(0x30);
    const uint32_t box = lvd::box(lvd::BOXES, lw(lwu(m + 0x78) + 0x80));
    vec_rotate_axis(V(f.sp), V(box), V(box + 0x20), lwc1(L1_ADDR(0x00161c28)));
    cam0_turn_dir(f.sp, u2f(0x3e0efa35u), 0.0f);
    cam0_set_distance(0, lwc1(L1_ADDR(0x00161c2c)), lwc1(L1_ADDR(0x00161c38)));
    cam0_set_height(lwc1(L1_ADDR(0x00161c30)), lwc1(L1_ADDR(0x00161c38)));
    cam0_set_look_height(0, lwc1(L1_ADDR(0x00161c34)), lwc1(L1_ADDR(0x00161c38)));
}

// Class 701: a rock that drops. State 0 keeps its place (+0x70) and goes down at once when its flag is set; 1 waits for the
// hero standing (states 0, 1 or 9) within 10 of its box's centre and not below it, then rumbles (2: the camera, the
// shake, its sound) until +0x8a runs out, and falls (3) under gravity until 20 units down (4: the shake, its sound, the flag
// set) or +0x86 runs out; in 4 the hero is let go when the rain's timer +0x84 is out. While not waiting, rocks rain about it
// now and then.
// orig: L1:0x002f9080
void nov_class701_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xa0);
    const uint32_t sp = f.sp;
    if (lbu(m + 0x20) != 1 && tick16(pv + 0x84) == 0 && rand_mod(100) < 5) {
        rain_rock(sp, pv + 0x70, 2.0f, 3.0f);
        const uint32_t cls = rock_class(L1_ADDR(0x00161c40));
        const int32_t life = rand_range(180, 300);
        rock_spawn(sp + 0x20, sp + 0x10, cls, life, 0, u2f(0x3d4ccccdu), 1.0f, 1.0f, 0.75f);
    }
    auto shake = [](float amount, int32_t frames) {
        swc1(L1_ADDR(0x00167260), amount);
        sw(L1_ADDR(0x00167268), static_cast<uint32_t>(frame_scale_i(frames)));
    };
    switch (lbu(m + 0x20)) {  // (the table 0x20b4d0)
        case 0:  // (0x2f921c) set up
            sh(pv + 0x3e, 5);
            sw(pv + 0x20, 0);
            sh(pv + 0x24, 0);
            sb(pv + 0x28, 4);
            V(pv + 0x70) = V(m + 0x10);
            sb(m + 0x20, 1);
            if (done_before(m)) {
                vec_clear(V(pv + 0x60));
                sb(m + 0x20, 4);
                swc1(m + 0x18, fpu_sub(lwc1(pv + 0x78), 20.0f));
                sw(pv + 0x8c, 0xffffffffu);
            }
            break;
        case 1: {  // (0x2f92d0) waiting for the hero
            const int32_t bi = lw(pv + 0x80);
            if (bi == -1) break;
            const uint32_t box = lvd::box(lvd::BOXES, bi);
            if (!(lwc1(box + 0x38) <= lwc1(0x0013f3d8))) break;
            if (!(vec2_distance(V(0x0013f3d0), V(box + 0x30)) < 10.0f)) break;
            const uint32_t hs = lwu(0x001413dc);
            if (!(hs < 2 || hs == 9)) break;
            const int32_t lo = frame_scale_i(300), hi = frame_scale_i(600);
            sh(pv + 0x84, static_cast<uint32_t>(rand_range(lo, hi)));
            sh(pv + 0x8a, static_cast<uint32_t>(frame_scale_i(lh(pv + 0x88))));
            sb(m + 0x20, 2);
            shake(u2f(0x3ecccccdu), 30);
            moby_sound_start(1, 0, m);
            break;
        }
        case 2:  // (0x2f93c0) rumbling
            nov_2f9000_l1(m);
            if (tick16(pv + 0x8a) == 0) break;
            vec_clear(V(pv + 0x60));
            sh(pv + 0x86, static_cast<uint32_t>(frame_scale_i(45)));
            sb(m + 0x20, 3);
            break;
        case 3: {  // (0x2f93f8) falling
            nov_2f9000_l1(m);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 0.0f));
            V(sp) = V(m + 0x10);  // (a copy the original keeps and never reads)
            swc1(pv + 0x68, fpu_sub(lwc1(pv + 0x68), fpu_mul(dt_sq(), 30.0f)));
            vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x60));
            if (tick16(pv + 0x86) != 0 && 20.0f < fpu_sub(lwc1(pv + 0x78), lwc1(m + 0x18))) {
                shake(u2f(0x3dcccccdu), 20);
                vec_clear(V(pv + 0x60));
                sb(m + 0x20, 4);
                moby_sound_start(0, 0, m);
                swc1(m + 0x18, fpu_sub(lwc1(pv + 0x78), 20.0f));
                mark_done(m);
            }
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.0f));
            break;
        }
        case 4:  // (0x2f955c) down: the hero let go once the rain stops
            if (lh(pv + 0x84) != 0) break;
            if (lw(pv + 0x8c) == -1) break;
            hydro_2eac18(0);
            hero_set_state_l1(0, 1);
            sw(L1_ADDR(0x0015f404), 0);
            sw(pv + 0x8c, 0xffffffffu);
            break;
        default:
            break;
    }
}

// Class 704: hit (mask 0x10000) for some damage, it bursts: its sound, 200 puffs of dust, 40 rocks (the table 0x161c50),
// its bolts; then it goes. Its hit slot +0xa4 is cleared every frame.
// orig: L1:0x002f9810
void nov_class704_update_l1(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000u, 0);
    sb(m + 0xa4, 0xff);
    if (hit == 0) return;
    if (!(0.0f < lwc1(hit + 0x2c))) return;
    moby_sound_start(0, 0x10, m);
    const Frame f(0xe0);
    burst_dust(m, f.sp, 200, 1.0f, u2f(0x3e99999au), 2.0f, 0.5f, 1.5f, 0.5f, 1.0f, 30, 90);
    burst_rocks(m, f.sp, 40, L1_ADDR(0x00161c50), u2f(0x3f99999au), u2f(0x3ecccccdu), 0.5f, 2.0f, u2f(0x3d23d70au), u2f(0x3db851ecu));
    moby_drop_bolts(m, 0, -1);
    moby_retire(m);
}

// Class 709: retired when its flag is set (and still bursts if hit that frame); hit by a shell of class 686 (mask 0x830000)
// it bursts into 20 rocks (the table 0x161c60), sets its flag and goes. Else its hit slot +0xa4 is cleared.
// orig: L1:0x002f9d80
void nov_class709_update_l1(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00830000u), 0);
    if (done_before(m)) moby_retire(m);
    if (hit == 0 || lh(lwu(hit + 0x20) + 0xa6) != 0x2ae) {
        sb(m + 0xa4, 0xff);
        return;
    }
    const Frame f(0x80);
    const uint32_t sp = f.sp;
    for (int i = 0; i < 20; ++i) {
        V(sp + 0x20) = Vec4{};
        swc1(sp + 0x20, rand_range_f(-dt(), dt()));
        swc1(sp + 0x24, rand_range_f(-dt(), dt()));
        swc1(sp + 0x28, rand_range_f(-dt(), dt()));
        V(sp + 0x10) = V(sp + 0x20);  // the direction
        const float r = rand_range_f(2.0f, 3.0f);
        const float a = rand_angle();
        swc1(sp + 0x20, fpu_mul(cos_vu(a), r));
        swc1(sp + 0x24, fpu_mul(sin_vu(a), r));
        sw(sp + 0x28, 0);
        const float speed = rand_range_f(fpu_mul(dt(), 5.0f), fpu_mul(dt(), 20.0f));
        vec3_set_length(V(sp + 0x10), V(sp + 0x10), speed);
        vec3_add(V(sp + 0x20), V(sp + 0x20), V(m + 0x10));
        const uint32_t cls = rock_class(L1_ADDR(0x00161c60));
        const int32_t life = rand_range(180, 300);
        rock_spawn(sp + 0x20, sp + 0x10, cls, life, 0, u2f(0x3d4ccccdu), 1.0f, 1.0f, 0.75f);
    }
    mark_done(m);
    moby_retire(m);
}

// Class 729: gone when its flag is set; hit (mask 0x800000) it bursts: its sound, 500 puffs of dust, 100 rocks (the table
// 0x161c70), its flag and the story byte 0x13d396 set; then it goes.
// orig: L1:0x002fa800
void nov_class729_update_l1(uint32_t m) {
    if (done_before(m)) {
        moby_retire(m);
        return;
    }
    if (sub_0025a420(m, 0x800000u, 0) == 0) return;
    moby_sound_start(0, 0, m);
    const Frame f(0x100);
    burst_dust(m, f.sp, 500, 4.0f, 1.0f, 8.0f, 1.5f, 3.5f, 1.5f, 3.5f, 120, 240);
    burst_rocks(m, f.sp, 100, L1_ADDR(0x00161c70), 4.0f, 1.0f, 1.0f, 8.0f, u2f(0x3d4ccccdu), u2f(0x3e19999au));
    sb(0x0013d396, 1);
    mark_done(m);
    moby_retire(m);
}

}  // namespace rac1
