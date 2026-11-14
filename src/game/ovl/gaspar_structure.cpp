// Gaspar's own moby classes: a structure in pieces that bursts and sinks into the lava (classes 1182 .. 1189, one update; the
// base is class 1184) — hand-ported from Gaspar's own overlay. Header gaspar_mobys.h, registered in gaspar_mobys.cpp.
// Addresses are Gaspar's (`orig: L9:`); Gaspar-only data is L9_ADDR, the base ELF's are bare (the frame time 0x15ed6c / its
// square 0x15ed70, the hero 0x13f3d0, the story bytes 0x13d388..).
//
// The pvars: a piece's velocity +0 / spin +0x20 / +0x24, its fall +0x64, its timer +0x2a, its base +0x2c and its place on the
// base +0x30; the base's two swings +0x40 .. +0x60, whether it builds its pieces +0x28, its pieces +0x6c .. (nine words),
// its story byte +0x68 (-1 none). Gaspar's tuning in its data 0x161368 .. 0x1613d0.
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>

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
const float kDegree = u2f(0x3c8efa35u);
constexpr uint32_t kBase = 0x4a0;  // class 1184

// gone out of the level's box (12 .. 1011)
bool out_of_level(uint32_t m) {
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(m + 0x10 + k * 4);
        if (c < 12.0f || 1011.0f < c) return true;
    }
    return false;
}

// the move and the spin of a flying piece
void fly(uint32_t m, uint32_t pv) {
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x20)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x24)));
}

// a float from the integer word at `a` (as the original converts it)
float word_float(uint32_t a) { return int_to_float(lw(a)); }
}  // namespace

// One piece (class `cls`) of the base `m`: at `at` (an EE vector) moved so that its joint 0 sits where the base's is, the
// base's turn and matrix; its pvars' base and place. The moby or 0.
// orig: L9:0x002c2500
uint32_t gas_2c2500_l9(uint32_t m, uint32_t at, uint32_t cls) {
    const uint32_t p = moby_spawn(cls);
    if (p == 0) return 0;
    const Frame f(0x20);
    const uint32_t sp = f.sp;
    sb(p + 0x30, 0xff);
    sh(p + 0x32, 0xff);
    sb(p + 0x20, 1);
    const uint32_t pv = lwu(p + 0x78);
    sb(p + 0x31, 1);
    lq_sq(p + 0x10, at);
    lq_sq(pv + 0x10, at);
    sw(pv + 0x2c, m);
    moby_update_transform(p);
    moby_chain_point_v(p, 0, V(sp));
    vec3_sub(V(sp + 0x10), V(at), V(sp));
    mat3_transform(V(sp + 0x10), V(sp + 0x10), M(m + 0xc0));
    vec3_add(V(p + 0x10), V(at), V(sp + 0x10));
    lq_sq(p + 0x40, m + 0x40);
    lq_sq(p + 0xc0, m + 0xc0);
    lq_sq(p + 0xd0, m + 0xd0);
    lq_sq(p + 0xe0, m + 0xe0);
    moby_update_transform(p);
    return p;
}

// The nearest moby of class `cls` to `at` (an EE vector) in the level's moby list (0x15ffe4, chained by +0x28), within
// 100000; its distance into `out` (an EE float, 0: none). The moby or 0.
// orig: L9:0x002c2610
uint32_t gas_2c2610_l9(uint32_t at, uint32_t cls, uint32_t out) {
    float best = 100000.0f;
    uint32_t found = 0;
    for (uint32_t o = lwu(L9_ADDR(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
        if (static_cast<uint32_t>(lh(o + 0xa6)) != cls) continue;
        const float d = vec3_distance(V(at), V(o + 0x10));
        if (d < best) {
            best = d;
            found = o;
        }
    }
    if (out != 0) swc1(out, best);
    return found;
}

// Classes 1182 .. 1189. 0: gone when its story byte is set; the base builds its pieces (when +0x28 says so), a piece takes
// the nearest base within 20 (else a debug note) and its slot there; each keeps the place of its joint 0. 1 / 2: the base
// swings (two swings of its pvars); a piece rides its base until the base bursts (state 3) and takes its draw flag and
// state byte; struck (+0xbc 1) the base starts its pieces' fuse (2: the base turns on, then 3). 3: the burst: the story
// bytes, bolts, the blast (and the base's sound), thrown from the base's joint and away from the hero, spinning, a random
// fuse and fall. 4: flying, falling, sparks now and then, into the lava at 25 (a hiss, steam, slowed: 5). 5: sinking.
// Gone out of the level's box.
// orig: L9:0x002c26c8
void gas_class1182_update_l9(uint32_t m) {
    sw(L9_ADDR(0x001613c8), lwu(L9_ADDR(0x001613c8)) + 1);
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0xc0);
    const uint32_t sp = f.sp;
    switch (lbu(m + 0x20)) {  // (the table 0x2082e0)
        case 0: {  // (0x2c2734) set up
            moby_chain_point_v(m, 0, V(sp + 0x10));
            const int32_t story = lw(pv + 0x68);
            if (story != -1 && lbu(0x0013d388 + static_cast<uint32_t>(story) + 0x39) != 0) {
                moby_retire(m);
                return;
            }
            sh(m + 0x32, 0x3ff);
            if (lh(m + 0xa6) == static_cast<int32_t>(kBase)) {
                if (lh(pv + 0x28) != 0) {
                    sh(m + 0x32, 0xff);
                    sw(pv + 0x2c, 0);
                    for (uint32_t cls = 0x49e; cls < 0x4a6; ++cls)
                        if (cls != kBase) gas_2c2500_l9(m, sp + 0x10, cls);
                }
            } else {
                swc1(sp + 0xa0, 100000.0f);
                const uint32_t base = gas_2c2610_l9(m + 0x10, kBase, sp + 0xa0);
                if (lwc1(sp + 0xa0) < 20.0f) {
                    sw(pv + 0x2c, base);
                    sw(lwu(base + 0x78) + 0x6c + ((static_cast<uint32_t>(lh(m + 0xa6)) - 0x49e) << 2), m);
                }
                // (else the original prints a debug note: the class, its index, the distance; nothing in the game)
            }
            const float s = fpu_mul(lwc1(m + 0x2c), u2f(0x3a800000u));
            sw(sp + 0xa4, 0);
            moby_chain_matrices(m, 1, sp + 0xa4, sp + 0x20);
            vec3_scale(V(pv + 0x30), V(sp + 0x50), s);
            sb(m + 0x20, 1);
            break;
        }
        case 1:
        case 2: {  // (0x2c28c0) riding, fused
            if (lh(m + 0xa6) == static_cast<int32_t>(kBase)) {
                sub_00262b00(m, pv + 0x40, pv + 0x44, lwc1(pv + 0x48), fpu_mul(fpu_mul(lwc1(pv + 0x4c), kDegree), dt()));
                const float a = fpu_mul(lwc1(pv + 0x58), kDegree);
                const float b = fpu_mul(fpu_mul(lwc1(pv + 0x5c), kDegree), dt());
                const float c = fpu_mul(fpu_mul(lwc1(pv + 0x60), kDegree), dt());
                sub_00262b80(m, pv + 0x50, pv + 0x54, a, b, c);
            }
            if (lh(m + 0xa6) != static_cast<int32_t>(kBase)) {
                const uint32_t base = lwu(pv + 0x2c);
                if (base != 0 && lbu(base + 0x20) != 3) {  // riding: the base's turn, its joint 0 less the place turned
                    lq_sq(m + 0x40, base + 0x40);
                    euler_to_mat4_vu(M(sp + 0x60), V(base + 0x40));
                    mat4_transform(V(sp + 0x10), V(pv + 0x30), M(sp + 0x60));
                    moby_chain_point_v(lwu(pv + 0x2c), 0, V(sp + 0x20));
                    vec3_sub(V(m + 0x10), V(sp + 0x20), V(sp + 0x10));
                }
            }
            if (lwu(pv + 0x2c) != 0) {
                const uint32_t flags = (lhu(m + 0x34) & 0xfffe) | (lhu(lwu(pv + 0x2c) + 0x34) & 1);
                sh(m + 0x34, flags);
                sb(m + 0xbc, lbu(lwu(pv + 0x2c) + 0xbc));
            }
            if (lbu(m + 0x20) == 2) {
                if (lh(m + 0xa6) == static_cast<int32_t>(kBase))
                    swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(fpu_mul(lwc1(L9_ADDR(0x001613bc)), kDegree), dt())));
                if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x2a))) != 0) sb(m + 0x20, 3);
                break;
            }
            if (lbu(m + 0xbc) != 1) break;
            sb(m + 0x30, 0xff);
            const int32_t lo = frame_scale_i(lw(L9_ADDR(0x001613c0)));
            sh(pv + 0x2a, static_cast<uint32_t>(rand_range(lo, frame_scale_i(lw(L9_ADDR(0x001613c4))))));
            for (uint32_t k = 0; k < 9; ++k) {
                const uint32_t piece = lwu(pv + 0x6c + k * 4);
                if (piece != 0) sb(piece + 0x30, 0xff);
            }
            sb(m + 0x20, 2);
            break;
        }
        case 3: {  // (0x2c2ab0) the burst
            timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x2a)));
            sb(0x0013d3c0, 1);
            if (lw(pv + 0x68) != -1) sb(0x0013d388 + lwu(pv + 0x68) + 0x39, 1);
            moby_drop_bolts(m, 0x200, -1);
            explosion_burst(m, m + 0x10, -1, 4.0f, 13.0f);
            if (lh(m + 0xa6) == static_cast<int32_t>(kBase)) moby_sound_start(2, 0, m);
            moby_chain_point_v(m, 0, V(sp + 0x10));
            vec3_sub(V(pv), V(m + 0x10), V(sp + 0x10));
            vec3_scale(V(pv), V(pv), fpu_div(fpu_mul(lwc1(L9_ADDR(0x001613a8)), dt()), 20.0f));
            vec3_sub(V(sp + 0x20), V(sp + 0x10), V(0x0013f3d0));
            const bool heavy = lhu(m + 0xa6) - 0x4a1u < 2;  // (classes 1185 / 1186)
            const float away = heavy ? lwc1(L9_ADDR(0x001613b8)) : lwc1(L9_ADDR(0x001613ac));
            vec3_set_length(V(sp + 0x20), V(sp + 0x20), fpu_mul(away, dt()));
            vec3_add(V(pv), V(pv), V(sp + 0x20));
            sw(pv + 8, 0);
            const float sx = fpu_mul(fpu_mul(lwc1(L9_ADDR(0x001613b0)), kDegree), dt());
            swc1(pv + 0x20, rand_range_f(-sx, sx));
            const float sy = fpu_mul(fpu_mul(lwc1(L9_ADDR(0x001613b4)), kDegree), dt());
            swc1(pv + 0x24, rand_range_f(-sy, sy));
            const float lo = int_to_float(frame_scale_i(90));
            const float hi = int_to_float(frame_scale_i(150));
            const float fuse = rand_range_f(lo, hi);
            sh(pv + 0x2a, f2u(eer::cvt_w(fuse)));
            swc1(pv + 0x64, rand_range_f(fpu_mul(lwc1(L9_ADDR(0x00161398)), dt_sq()), fpu_mul(lwc1(L9_ADDR(0x0016139c)), dt_sq())));
            if (lhu(m + 0xa6) - 0x4a1u < 2) {  // (the heavy pieces: a quarter turn over the fuse, their own fall)
                const float n = int_to_float(lh(pv + 0x2a));
                const float one = fpu_div(lwc1(pv + 0x20), lwc1(pv + 0x20));
                const float per = fpu_div(u2f(0x3fc90fdbu), n);
                const float a = fpu_mul(lwc1(L9_ADDR(0x001613a0)), dt_sq());
                const float b = fpu_mul(lwc1(L9_ADDR(0x001613a4)), dt_sq());
                swc1(pv + 0x20, fpu_mul(one, per));
                swc1(pv + 0x64, rand_range_f(a, b));
            }
            sw(m + 0x94, 0);
            sb(m + 0x20, 4);
            break;
        }
        case 4: {  // (0x2c2d0c) flying
            swc1(pv + 8, fpu_sub(lwc1(pv + 8), lwc1(pv + 0x64)));
            fly(m, pv);
            if (out_of_level(m)) {
                moby_retire(m);
                return;
            }
            if (rand_mod(3) == 0) {  // sparks off it
                vec_clear(V(sp + 0x10));
                // (the two colours copied onto the stack, as the original does: the steam below adds to those words)
                sw(sp + 0x20, lwu(L9_ADDR(0x001613d0)));
                sw(sp + 0x24, lwu(L9_ADDR(0x001613d4)));
                const uint32_t c1 = lwu(sp + 0x20 + (static_cast<uint32_t>(rand_mod(2)) << 2));
                const int32_t lo = frame_scale_i(15);
                const int32_t l1 = rand_range(lo, frame_scale_i(20));
                const int32_t lo2 = frame_scale_i(25);
                const int32_t l2 = rand_range(lo2, frame_scale_i(30));
                const float speed = fpu_mul(rand_range_f(8.0f, 16.0f), dt());
                spawn_particle_0b(m + 0x10, sp + 0x10, 0x2f3f3f7fu, c1, static_cast<uint32_t>(l1), static_cast<uint32_t>(l2), 0, 0, 400000.0f, speed);
            }
            if (!(lwc1(m + 0x18) < 25.0f)) break;
            // into the lava: steam (particle 2) about it, slowed, a hiss
            for (int32_t i = 0; i < lw(L9_ADDR(0x00161368)); ++i) {
                const float a = rand_angle();
                lq_sq(sp + 0x30, m + 0x10);
                swc1(sp + 0x30, fpu_add(lwc1(sp + 0x30), rand_range_f(-3.0f, 3.0f)));
                swc1(sp + 0x34, fpu_add(lwc1(sp + 0x34), rand_range_f(-3.0f, 3.0f)));
                swc1(sp + 0x18, fpu_mul(lwc1(L9_ADDR(0x00161374)), dt()));
                swc1(sp + 0x10, fpu_add(lwc1(sp + 0x10), fpu_mul(cos_vu(a), lwc1(L9_ADDR(0x0016137c)))));
                swc1(sp + 0x14, fpu_add(lwc1(sp + 0x14), fpu_mul(sin_vu(a), lwc1(L9_ADDR(0x0016137c)))));
                swc1(sp + 0x28, fpu_mul(lwc1(L9_ADDR(0x00161378)), dt()));
                swc1(sp + 0x20, fpu_add(lwc1(sp + 0x20), fpu_mul(cos_vu(a), lwc1(L9_ADDR(0x00161380)))));
                swc1(sp + 0x24, fpu_add(lwc1(sp + 0x24), fpu_mul(sin_vu(a), lwc1(L9_ADDR(0x00161380)))));
                swc1(sp + 0x1c, lwc1(L9_ADDR(0x00161390)));
                swc1(sp + 0x2c, lwc1(L9_ADDR(0x00161394)));
                auto frames = [](uint32_t a_word) {
                    const int32_t n = lw(a_word);
                    return float_to_int(frame_scale_f(rand_range_f(int_to_float(n), int_to_float(n << 1))));
                };
                const int32_t n1 = frames(L9_ADDR(0x00161384));
                const int32_t n2 = frames(L9_ADDR(0x00161388));
                const int32_t n3 = frames(L9_ADDR(0x0016138c));
                spawn_particle_02(sp + 0x30, sp + 0x10, sp + 0x20, lwu(L9_ADDR(0x0016136c)), lwu(L9_ADDR(0x00161370)),
                                  static_cast<uint32_t>(n1), static_cast<uint32_t>(n2), static_cast<uint32_t>(n3), -1);
            }
            vec3_scale(V(pv), V(pv), u2f(0x3ee66666u));
            const float k = u2f(0x3f666666u);
            const float sy = fpu_mul(lwc1(pv + 0x24), k);
            const float sx = fpu_mul(lwc1(pv + 0x20), k);
            swc1(pv + 0x24, sy);
            swc1(pv + 0x20, sx);
            moby_sound_start(0, 0, m);
            sb(m + 0x20, 5);
            break;
        }
        case 5:  // (0x2c3130) sinking
            swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_div(fpu_mul(dt_sq(), u2f(0x412ccccdu)), 3.0f)));
            fly(m, pv);
            if (out_of_level(m)) moby_retire(m);
            break;
        default:
            break;
    }
}

}  // namespace rac1
