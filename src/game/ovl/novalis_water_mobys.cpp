// Novalis's own moby classes around its water (hand-ported from Novalis's own overlay): the water grid's class (751: its
// set-up, the ripples by the camera's zone, the drips and the bubbles), the drip it lets fall (787), the scrolling strips'
// class (761), and the particles and splash they use. Header novalis_mobys2.h, registered in novalis_mobys2.cpp; the water's
// own code (the grid, its draw) is novalis_water.cpp's. Addresses are Novalis's (`orig: L1:`); Novalis-only data is L1_ADDR,
// the base ELF's are bare (the frame time 0x15ed6c / its square 0x15ed70).
//
// The water: 21 tiles of 0x1190 bytes at 0x1e34c0 (their points 0x1fa6c0, one quadword each); seven zones of them: the
// first tile 0x1fa5e8, the count 0x1fa608, the ripples' chance 0x1fa628, a halfword per tile 0x1fa590; the drips' points
// 0x1fa650 (five); the bubbles' heights by the frame 0x1fa6a0 (20 bytes). Class 751's pvars: the zones' boxes +0 .. +0x18.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/novalis_water.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/veldin_mobys.h>

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
constexpr uint32_t kTile = 0x1190;          // a water tile's record
constexpr int32_t kTiles = 0x15;
uint32_t tiles() { return L1_ADDR(0x001e34c0); }
uint32_t tile(int32_t i) { return L1_ADDR(0x001e34c0) + static_cast<uint32_t>(i) * kTile; }
}  // namespace

// A splash on the water's grid at (x, y): region_height_splash with nothing added (the flag 0).
// orig: L1:0x002b8288
void nov_2b8288_l1(uint32_t regs, int32_t n, float x, float y, float radius, float amount) {
    region_height_splash(regs, n, 0, x, y, radius, amount);
}

// A bubble (particle 0x38) at `pos` moving by `vel` (EE vectors), size `size`: a random light grey (0x60 .. 0x7e), the
// texture of the level's list 0x1b25e0, a random start frame.
// orig: L1:0x00287b00
uint32_t nov_287b00_l1(uint32_t pos, uint32_t vel, float size) {
    const uint32_t p = alloc_particle(0x38);
    if (p == 0) return 0;
    lq_sq(p + 0x10, pos);
    const uint32_t g = (static_cast<uint32_t>(rand_mod(16)) << 1) + 0x60;
    sw(p + 4, (g << 16) | (g << 8) | 0x40000000u | g);
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x20));
    sb(p + 3, 0x48);
    sb(p + 1, 0);
    swc1(p + 0xc, size);
    sb(p + 2, lbu(lwu(L1_ADDR(0x001b25e0))));
    lq_sq(p + 0x20, vel);
    sb(p + 8, static_cast<uint32_t>(rand_mod(0x100)));
    return p;
}

// A ring of the bubbles (particle 0x39) at `pos` moving by `vel` (EE vectors): size `size` x 210000, a random tint, the
// texture of the level's list 0x1b25e4, 128 frames, a random start (+0x30, its byte +8) and the growth `grow` (+0x34).
// orig: L1:0x00287c80
uint32_t nov_287c80_l1(uint32_t pos, uint32_t vel, float size, float grow) {
    const uint32_t p = alloc_particle(0x39);
    if (p == 0) return 0;
    lq_sq(p + 0x10, pos);
    sw(p + 4, (static_cast<uint32_t>(rand_mod(16)) << 25) | 0x808080u);
    sb(p + 9, static_cast<uint32_t>(float_to_int(12.0f) + 0x20));
    sb(p + 3, 0x48);
    sb(p + 1, 1);
    sh(p + 0xa, 0x80);
    sb(p + 2, lbu(lwu(L1_ADDR(0x001b25e4))));
    swc1(p + 0xc, fpu_mul(size, 210000.0f));
    lq_sq(p + 0x20, vel);
    const float start = rand_range_f(0.0f, 256.0f);
    swc1(p + 0x30, start);
    sb(p + 8, static_cast<uint32_t>(float_to_int(start)));
    swc1(p + 0x34, grow);
    return p;
}

// A drip (class 787) at `pos` (an EE vector), scaled `scale`: the hero's colour, a random heading, falling (its pvars: the
// velocity +0 with z -dt, gravity +0x10, 120 frames +0x14, a random spin +0x1c).
// orig: L1:0x002ffcd0
uint32_t nov_2ffcd0_l1(uint32_t pos, float scale) {
    const uint32_t d = moby_spawn(0x313);
    if (d == 0) return 0;
    const uint32_t pv = lwu(d + 0x78);
    moby_copy_hero_color(d);
    sw(d + 0x40, 0);
    swc1(d + 0x2c, fpu_mul(lwc1(d + 0x2c), scale));
    sh(d + 0x32, 0x40);
    sb(d + 0x30, 0xff);
    swc1(d + 0x48, rand_angle());
    lq_sq(d + 0x10, pos);
    V(pv) = Vec4{};
    sw(pv + 0x14, 0x78);
    swc1(pv + 8, -dt());
    swc1(pv + 0x10, fpu_mul(dt_sq(), 15.0f));
    swc1(pv + 0x1c, rand_range_f(u2f(0x3b4de32fu), u2f(0x3d80adfdu)));
    moby_update_transform(d);
    return d;
}

// Class 751: the water. 0 sets the grid up (its records, the scroll rates of tiles 13 .. 16, the weights, the light from
// Novalis's sun direction, the regions' record 0x161200.., each tile's fixed point) and splashes each tile twice. 1, every
// frame: the zones whose box holds the camera keep their tiles' ripples (and splash them now and then), the others' are
// stilled; the tint by the zone; the grid's step and its draw queued; in zones 0 and 6 a drip now and then, in zone 5 the
// bubbles.
// orig: L1:0x002fd0e8
void nov_class751_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.5f));
        grid_records_reset(tiles(), kTiles);
        const float rate = u2f(0xbad1b717u);
        sw(tile(15) + 0x44, 8);
        swc1(tile(13) + 0x28, rate);
        swc1(tile(15) + 0x2c, 0.0f);
        swc1(tile(13) + 0x2c, 0.0f);
        swc1(tile(16) + 0x28, rate);
        swc1(tile(15) + 0x28, rate);
        swc1(tile(14) + 0x28, rate);
        swc1(tile(16) + 0x2c, 0.0f);
        swc1(tile(14) + 0x2c, 0.0f);
        grid_weights_scale(1.0f);
        const uint32_t light = L1_ADDR(0x001cad00);
        swc1(light, 16.0f);
        swc1(light + 0xc, -8.0f);
        swc1(light + 0x18, u2f(0x3f666666u));
        swc1(light + 0x20, u2f(0x3dcccccdu));
        swc1(light + 8, -8.0f);
        swc1(light + 0x14, 1.0f);
        swc1(light + 0x10, 1.0f);
        swc1(light + 4, 16.0f);
        const float k = u2f(0x3f13cd36u);  // (1 / sqrt 3)
        const float sun = atan2_f(lwc1(L1_ADDR(0x00180350)), lwc1(L1_ADDR(0x00180354)));
        swc1(light + 0x30, fpu_mul(cos_vu(sun), k));
        swc1(light + 0x34, fpu_mul(sin_vu(sun), k));
        sb(light + 0x3d, 0x40);
        swc1(light + 0x38, u2f(0xbf13cd36u));
        sw(light + 0x28, 0x29);
        sw(light + 0x2c, 0x2a);
        sb(light + 0x3c, 0x1c);
        sw(L1_ADDR(0x001612d8), kTiles);
        swc1(L1_ADDR(0x00161208), 0.0f);
        swc1(L1_ADDR(0x0016120c), 32768.0f);
        swc1(L1_ADDR(0x00161210), 255.0f);
        swc1(L1_ADDR(0x00161214), 48.0f);
        sw(L1_ADDR(0x001612d0), tiles());
        sw(L1_ADDR(0x001612d4), L1_ADDR(0x001fa6c0));
        for (int32_t i = 0; i < kTiles; ++i) grid_point_fixed(tile(i), L1_ADDR(0x001fa6c0) + static_cast<uint32_t>(i) * 0x10);
        for (int32_t i = 0; i < kTiles; ++i) {
            for (int k2 = 0; k2 < 2; ++k2) {
                const float dx = rand_range_f(-6.0f, 6.0f);
                const float x = fpu_add(lwc1(tile(i)), dx);
                const float dy = rand_range_f(-6.0f, 6.0f);
                nov_2b8288_l1(tiles(), kTiles, x, fpu_add(lwc1(tile(i) + 4), dy), 2.0f, u2f(0x3e4ccccdu));
            }
        }
        return;
    }
    if (st != 1) return;
    const Frame f(0x30);
    const uint32_t sp = f.sp;
    int32_t found = -1;  // (the last zone that holds the camera)
    for (int32_t z = 0; z < 7; ++z) {
        const uint32_t zi = static_cast<uint32_t>(z) << 2;
        const int32_t box = lw(pv + zi);
        if (box == -1 || point_in_box(L1_ADDR(0x00167240), box) == 0) {  // stilled
            const int32_t n = lw(L1_ADDR(0x001fa608) + zi);
            if (n <= 0) continue;
            uint32_t t = tile(lw(L1_ADDR(0x001fa5e8) + zi)) + 0x1e;
            for (int32_t k = n; k != 0; --k, t += kTile) sh(t, 0);
            continue;
        }
        found = z;
        for (int32_t k = 0; k < lw(L1_ADDR(0x001fa608) + zi); ++k) {
            const int32_t ti = lw(L1_ADDR(0x001fa5e8) + zi) + k;
            sh(tile(ti) + 0x1e, lhu(L1_ADDR(0x001fa590) + (static_cast<uint32_t>(ti) << 2)));
            if (rand_mod(lw(L1_ADDR(0x001fa628) + zi)) != 0) continue;
            const float dx = rand_range_f(-4.0f, 4.0f);
            const float x = fpu_add(lwc1(tile(lw(L1_ADDR(0x001fa5e8) + zi) + k)), dx);
            const float dy = rand_range_f(-4.0f, 4.0f);
            const int32_t first = lw(L1_ADDR(0x001fa5e8) + zi);
            const uint32_t regs = lwu(L1_ADDR(0x001612d0)) + static_cast<uint32_t>(first) * kTile + static_cast<uint32_t>(k) * kTile;
            const float y = fpu_add(lwc1(tile(first + k) + 4), dy);
            region_height_splash(regs, 1, 1, x, y, 1.0f, u2f(0xbd4ccccdu));
        }
    }
    if (found >= 0) {  // the tint of the regions' record by the zone (1 .. 3, or the others)
        if (static_cast<uint32_t>(found - 1) < 3) {
            sb(L1_ADDR(0x00161205), 0x28);
            sb(L1_ADDR(0x00161203), 0x40);
            sb(L1_ADDR(0x00161206), 0x30);
            sb(L1_ADDR(0x00161200), 0);
            sb(L1_ADDR(0x00161201), 0x28);
            sb(L1_ADDR(0x00161202), 0x30);
            sb(L1_ADDR(0x00161204), 0);
        } else {
            sb(L1_ADDR(0x00161204), 0x18);
            sb(L1_ADDR(0x00161205), 0x30);
            sb(L1_ADDR(0x00161206), 0x90);
            sb(L1_ADDR(0x00161200), 0x18);
            sb(L1_ADDR(0x00161201), 0x30);
            sb(L1_ADDR(0x00161202), 0x90);
            sb(L1_ADDR(0x00161203), 0x30);
        }
    }
    nov_water_grid_update_l1(tiles(), kTiles);
    sub_001f93e0(L1_ADDR(0x002fd0c0), m);
    if (found == 0 || found == 6) {  // a drip now and then (the countdown 0x161c84; 300 .. 1200 frames)
        const int32_t wait = lw(L1_ADDR(0x00161c84)) - 1;
        sw(L1_ADDR(0x00161c84), static_cast<uint32_t>(wait));
        if (wait <= 0) {
            lq_sq(sp, L1_ADDR(0x001fa650) + (static_cast<uint32_t>(rand_mod(5)) << 4));
            swc1(sp, fpu_add(lwc1(sp), rand_range_f(u2f(0xbe19999au), u2f(0x3e19999au))));
            swc1(sp + 4, fpu_add(lwc1(sp + 4), rand_range_f(u2f(0xbe19999au), u2f(0x3e19999au))));
            nov_2ffcd0_l1(sp, u2f(0x3e4ccccdu));
            sw(L1_ADDR(0x00161c84), static_cast<uint32_t>(rand_range(300, 1200)));
        }
    }
    if (found != 5) return;
    // the bubbles: a ring rising from the height of the frame's step (20 frames a cycle), then a column of them
    const uint32_t at = sp + 0x10;
    const uint8_t step = static_cast<uint8_t>(lbu(L1_ADDR(0x001fa6a0) + static_cast<uint32_t>(ee_rem(lw(L1_ADDR(0x0015f5cc)), 20))));
    float h = fpu_div(static_cast<float>(static_cast<int32_t>(step)), 20.0f);
    swc1(sp, u2f(0x3ca3d70au));
    sw(sp + 4, 0);
    sw(sp + 8, 0);
    swc1(at, fpu_add(rand_range_f(u2f(0xbf19999au), u2f(0x3f19999au)), 177.0f));
    const float jy = rand_range_f(u2f(0xbdcccccdu), u2f(0x3dcccccdu));
    swc1(at + 8, 39.0f);
    swc1(at + 4, fpu_add(fpu_add(fpu_mul(h, -11.0f), 176.0f), jy));
    const float grow = rand_range_f(u2f(0x3ecccccdu), 0.5f);
    nov_287c80_l1(at, sp, 1.0f, grow);
    sw(sp, 0);
    sw(sp + 4, 0);
    sw(sp + 8, 0);
    h = lwc1(sp);
    do {
        if (rand_mod(32) == 0) {
            swc1(at, fpu_add(rand_range_f(u2f(0xbf19999au), u2f(0x3f19999au)), 178.0f));
            const float j = rand_range_f(u2f(0xbdcccccdu), u2f(0x3dcccccdu));
            swc1(at + 4, fpu_add(fpu_add(fpu_mul(h, -11.0f), 176.0f), j));
            swc1(at + 8, fpu_add(rand_range_f(u2f(0xbe4ccccdu), 0.0f), 39.0f));
            nov_287b00_l1(at, sp, rand_range_f(4.0f, 10.0f));
        }
        h = fpu_add(h, u2f(0x3d4ccccdu));
    } while (h < 1.0f);
}

// Class 761: the strips of 0x1fbc80 (four records of 0x60: their vertices +0, the count +0xc). The first one set up (the
// flag 0x161ce0) sets every strip's vertices' +8 and +0xc to +8 less 0.45 and staggers the pairs' u by 0.05 (+8 / +0x1c up,
// +0xc / +0x18 down, every other pair the other way); then every frame the strips' wave height (+0x5c) set to 0.5 + 0.5
// sin(((frame & 63) - 32) x pi / 32) (the frame counter 0x15f5cc) and their draw queued.
// orig: L1:0x002feb58
void nov_class761_update_l1(uint32_t m) {
    const uint32_t strips = L1_ADDR(0x001fbc80);
    if (lwu(L1_ADDR(0x00161ce0)) == 0) {
        sw(L1_ADDR(0x00161ce0), 1);
        for (uint32_t s = 0; s < 4; ++s) {
            const uint32_t rec = strips + s * 0x60;
            if (lw(rec + 0xc) <= 0) continue;
            uint32_t v = lwu(rec);
            for (int32_t i = 0; i < lw(rec + 0xc); ++i, v += 0x10) {
                const float u = fpu_sub(lwc1(v + 8), u2f(0x3ee66666u));
                swc1(v + 0xc, u);
                swc1(v + 8, u);
            }
        }
        int32_t phase = 1;  // (one back at each strip, one on at each pair: the pairs alternate across the strips)
        const float d = u2f(0x3d4ccccdu);
        for (uint32_t s = 0; s < 4; ++s) {
            const uint32_t rec = strips + s * 0x60;
            --phase;
            if (lw(rec + 0xc) <= 0) continue;
            uint32_t v = lwu(rec);
            for (int32_t i = 0; i < lw(rec + 0xc); i += 2, v += 0x20, ++phase) {
                if ((phase & 1) != 0) {
                    swc1(v + 8, fpu_add(lwc1(v + 8), d));
                    swc1(v + 0xc, fpu_sub(lwc1(v + 0xc), d));
                    swc1(v + 0x18, fpu_sub(lwc1(v + 0x18), d));
                    swc1(v + 0x1c, fpu_add(lwc1(v + 0x1c), d));
                } else {
                    swc1(v + 8, fpu_sub(lwc1(v + 8), d));
                    swc1(v + 0xc, fpu_add(lwc1(v + 0xc), d));
                    swc1(v + 0x18, fpu_add(lwc1(v + 0x18), d));
                    swc1(v + 0x1c, fpu_sub(lwc1(v + 0x1c), d));
                }
            }
        }
    }
    const float x = fpu_sub(int_to_float(static_cast<int32_t>(lw(L1_ADDR(0x0015f5cc)) & 0x3f)), 32.0f);
    const float h = fpu_add(fpu_mul(sin_vu(fpu_mul(x, u2f(0x3dc90fdbu))), 0.5f), 0.5f);
    for (int32_t i = 3; i >= 0; --i) swc1(strips + 0x5c + static_cast<uint32_t>(i) * 0x60, h);
    sub_001f93e0(L1_ADDR(0x002feb28), m);
}

// Class 787: a drip falling (gravity +0x10 on its velocity +0, spinning by +0x1c) until it meets the world: into the water
// (sub_001f35d8 0: no surface) it splashes the grid (once, its sound), rises as a ring with 16 drops and sinks slowly,
// shrinking; on anything else it goes. Gone when its 120 frames (+0x14) run out.
// orig: L1:0x002ffdc0
void nov_class787_update_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x40);
    const uint32_t sp = f.sp;
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), lwc1(pv + 0x10)));
    swc1(m + 0x40, fpu_add(lwc1(m + 0x40), lwc1(pv + 0x1c)));
    vec3_add(V(sp), V(m + 0x10), V(pv));
    if (segment_collide_v(V(m + 0x10), V(sp), 2, 0, nullptr) != 0) {
        if (sub_001f35d8() != 0) {
            moby_retire(m);
            return;
        }
        if (lw(pv + 0x18) == 0) moby_sound_start(0, 0, m);
        region_height_splash(lwu(L1_ADDR(0x001612d0)), lw(L1_ADDR(0x001612d8)), 1, lwc1(m + 0x10), lwc1(m + 0x14), 0.5f,
                             u2f(0xbeb33333u));
        sw(pv + 0x14, 0x78);
        sw(pv + 0x18, 1);
        sw(pv + 0x1c, 0);
        swc1(pv + 8, fpu_mul(dt(), -1.5f));
        swc1(pv + 0x10, fpu_mul(dt_sq(), u2f(0x3f4ccccdu)));
        const uint32_t at = sp + 0x10, vel = sp + 0x20;
        swc1(at, lwc1(m + 0x10));
        swc1(at + 4, lwc1(m + 0x14));
        swc1(at + 8, coll_hit_point().z);
        const uint32_t ring = spawn_moby_307(at, 2.0f);
        if (ring != 0) sb(ring + 0x23, 0x70);
        for (int i = 0; i < 16; ++i) {
            const float a = rand_angle();
            const float r = rand_range_f(fpu_mul(dt(), 0.0f), fpu_mul(dt(), 3.0f));
            swc1(vel, fpu_mul(cos_vu(a), r));
            swc1(vel + 4, fpu_mul(sin_vu(a), r));
            swc1(vel + 8, rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.5f)));
            const int32_t life = rand_range(90, 120);
            spawn_particle_23(at, vel, rand_mod(2), static_cast<uint32_t>(life));
        }
    }
    V(m + 0x10) = V(sp);
    if (lw(pv + 0x18) != 0) swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), u2f(0x3f7d70a4u)));
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0) moby_retire(m);
}

}  // namespace rac1
