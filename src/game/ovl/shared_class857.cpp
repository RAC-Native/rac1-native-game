// Class 857 of Blarg (L6) and Orxon (L10), the Gadgebot (one of the little robots Clank commands): its update, its helpers
// and its draw callback, one port of the two levels' copies, which are the same code relinked (the normalised texts
// compared, the jump table's entries the same offsets) but for the draw's glow: Orxon's copy calls Veldin's glow_draw,
// Blarg's its own build of it (L6:0x272498, a variant, still transliterated). Its addresses are the engine's (OV: the
// level's moby list 0x1aba00, the moby table 0x15ffd8, the frame counter 0x15f5cc, the camera 0x166dc0 and its yaw
// 0x166dd8, the path table 0x1b04b0, the collision results 0x173e40 .., the Gadgebots in the command ring's range 0x17e504)
// or the base ELF's (bare: the hero 0x13f3d0 (his height also at 0x13f5f8) and his moby 0x1413d0, the hero's mode
// 0x1413f4, the command given 0x141610, the frame time 0x15ed6c, its square 0x15ed70 and the frame scales 0x15ed60 /
// 0x15ed64, the planet 0x15ed84, the collected checkpoints 0x14c050, the size flag 0x15edb3); the level's own data is one
// tuning word (per level below) and the draw callback. The state table is a switch on the index. Header shared_class857.h.
//
// The pvars: the waiting spot +0 and the home +0xe0, the walker record +0x30 (+0x30 its radius in 1024ths, +0x34 its
// height, +0x38 / +0x3c its steps, +0x40 its reach, +0x44 its turn, +0x48 its fall speed, +0x54 its speed), the knock-back
// record +0x80 (+0x90 its time, +0x98 / +0x9c, +0xa4, +0xbd, +0xcc .. +0xd4), the follow's last spot +0xf0, its joints
// +0x100 / +0x101, the chain points +0x140 .. +0x160, the light it waits by +0x170 (class 302), its target +0x174 (the
// one dealt to it +0x178, its offset +0x17c), the follow's frames +0x184 and distance +0x188, the frame of its deal +0x18c,
// the stand-off flag +0x190, the attack's timer +0x194, the near flag +0x198, the pick-again flag +0x19a, the moby under it
// +0x19c, its candidates +0x1a0 (8, their scores +0x1c0, the count +0x192), the walls +0x1e0 (16 path indices, -1 none;
// their count +0x310) and their paths +0x220, the walls it is inside +0x27c (-1 none), the slope speed +0x284, the floor's
// spring +0x288, the glow's phase +0x28c and colour +0x290, the group's frame word +0x294, the switches and their boxes
// +0x2a0 / +0x2c0 (8 each), the last positions +0x2e0 / +0x2f0, the moby it enters +0x304, the size +0x308, the group
// +0x30c.
#include <game/ovl/shared_class857.h>

#include <game/math/vecmath.h>
#include <game/ovl/blarg_part2.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/glows.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_class302.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_path_links.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/walker.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;   // the hero's position
constexpr uint32_t HERO_MOBY = 0x001413d0;  // the hero's moby
constexpr int32_t CLASS = 857;
constexpr int32_t ENTRANCE = 1302;  // the Gadgebots' entrance (class1302_update)
constexpr int32_t PAD = 1024;       // (a pad it goes in by, as the entrance)
constexpr int32_t SLOT_945 = 945;   // (mobys it takes a place about: 945, 1301, 351)
constexpr int32_t SLOT_1301 = 1301;
constexpr int32_t SLOT_351 = 351;
constexpr int32_t SWITCH = 1015;    // the switches of its boxes

float frame_time() { return lwc1(0x0015ed6c); }
float frame_time_sq() { return lwc1(0x0015ed70); }
float frame_scale64() { return lwc1(0x0015ed64); }
int32_t frame_counter() { return lw(OV(0x0015f5cc)); }
uint32_t first_moby() { return lwu(OV(0x0015ffd8)); }

// the level's word of the class: the walker's height over the ground (0.27, or 0.35 while the size flag is set)
uint32_t standoff() { return on_level(ORXON) ? L10_ADDR(0x00161bd0) : L6_ADDR(0x00161ce0); }
// the draw callback (the level's copy of class857_draw)
uint32_t draw_callback() { return on_level(ORXON) ? L10_ADDR(0x002d7808) : L6_ADDR(0x002f37d0); }

// a timer word ticked (timer_tick_i32 on EE memory)
int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }

// the yaw from the moby to the point (x, y)
float yaw_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// the animation `set` from frame `frame`, over 7 frames (scaled), unless it is the one playing
void anim_to(uint32_t m, uint32_t set, int32_t frame = 0) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, frame, frame_scale_i(7));
}

// whether the point is inside the walls' path `path` (an index of the path table: a closed polygon)
bool inside(uint32_t pos, int32_t path) {
    const uint32_t p = lwu(OV(0x001b04b0) + (static_cast<uint32_t>(path) << 2));
    return point_in_polygon(pos, p + 0x10, lw(p)) != 0;
}

// the point a Gadgebot goes to at `o`: the moby's position, or (but for classes 1301 and 351) a step along its x axis
void goal_of(uint32_t out, uint32_t o) {
    const int32_t cls = lh(o + 0xa6);
    if (cls == SLOT_1301 || cls == SLOT_351) lq_sq(out, o + 0x10);
    else vec3_add(V(out), V(o + 0x10), V(o + 0xc0));
}

// One sparkle (particle 0x35) about `centre`: at `pos` (up to 0.25 out, 0.5 up), its speed `vel` (the vector there scaled
// to up to a frame's time, rising 2.5 .. 5 a second), a random colour, its size, its life 15 .. 30 frames, its spin either
// way.
void sparkle(uint32_t pos, uint32_t vel, uint32_t centre) {
    const float size = rand_range_f(u2f(0x3dcccccdu), 1.0f);  // (0.1 .. 1)
    const int32_t c0 = rand_range(0x40, 0xff);
    const int32_t c1 = rand_range(0x40, 0xff);
    const int32_t c2 = rand_range(0x40, 0xff);
    const float a = rand_angle();
    const float x = cos_vu(a);
    swc1(pos, fpu_mul(x, rand_range_f(0.0f, 0.25f)));
    const float b = rand_angle();
    const float y = sin_vu(b);
    swc1(pos + 4, fpu_mul(y, rand_range_f(0.0f, 0.25f)));
    swc1(pos + 8, 0.0f);
    swc1(pos + 8, fpu_add(lwc1(pos + 8), rand_range_f(0.0f, 0.5f)));
    vec3_add_inplace(V(pos), V(centre));
    vec3_set_length(V(vel), V(vel), rand_range_f(0.0f, frame_time()));
    swc1(vel + 8, rand_range_f(fpu_mul(frame_time(), 2.5f), fpu_mul(frame_time(), 5.0f)));
    const int32_t life = frame_scale_i(rand_range(15, 30));
    const uint32_t colour =
        0x7f000000u | (static_cast<uint32_t>(c2) << 16) | (static_cast<uint32_t>(c1) << 8) | static_cast<uint32_t>(c0);
    const int32_t spin = rand_mod(2) == 0 ? -1 : 1;
    spawn_particle_35(pos, static_cast<uint32_t>(life), colour, 0, static_cast<uint32_t>(spin), vel,
                      fpu_mul(size, u2f(0x3d8f5c29u)), fpu_mul(size, u2f(0x3f333333u)), fpu_mul(frame_time_sq(), 15.0f));
}

// the walls (of the moby's 16) the point is inside (+0x27c: the last one found)
void walls_inside(uint32_t pv, uint32_t pos) {
    for (uint32_t k = 0; k < 16; ++k) {
        const uint32_t w = pv + 0x1e0 + k * 4;
        if (lw(w) < 0) continue;
        if (nov_276a48_l1(w, 1, lw(w + 0x40), pos, 0.0f) != 0) sw(pv + 0x27c, k);
    }
}
// the walls (of the moby's 16) that hold both the hero and the moby (+0x27c: the last one found)
void walls_with_hero(uint32_t m, uint32_t pv) {
    for (uint32_t k = 0; k < 16; ++k) {
        const int32_t w = lw(pv + 0x1e0 + k * 4);
        if (w < 0) continue;
        if (inside(HERO_POS, w) && inside(m + 0x10, w)) sw(pv + 0x27c, k);
    }
}

// The way toward `to` inside the walls the moby is in (+0x27c; nov_276fe8_l1 into `out`, else straight to `to`), and the
// moby kept inside them (pushed off their edge by 0.17, the push kept only while still inside; `scratch` a quad).
void way_inside(uint32_t m, uint32_t pv, uint32_t to, uint32_t out, uint32_t scratch) {
    const int32_t k = lw(pv + 0x27c);
    if (k < 0) {
        lq_sq(out, to);
        return;
    }
    const uint32_t w = pv + 0x1e0 + (static_cast<uint32_t>(k) << 2);
    if (nov_276fe8_l1(w, 1, lw(w + 0x40), m + 0x10, to, out, u2f(0x3dcccccdu)) == 0) lq_sq(out, to);
    lq_sq(scratch, m + 0x10);
    path_push_out_2d(lwu(w), m + 0x10, scratch, u2f(0x3e2e147bu));  // (0.17)
    if (inside(scratch, lw(w))) lq_sq(m + 0x10, scratch);
}

// the yaw turned toward the angle (the turn record +0x44), 300 degrees a second at most
void turn_to(uint32_t m, uint32_t pv, float yaw) {
    const float scale = frame_scale64();
    yaw_turn_damped(m, pv + 0x44, yaw, fpu_mul(scale, u2f(0x3d8f5c29u)), fpu_mul(scale, u2f(0x3e99999au)),
                    fpu_mul(frame_time(), u2f(0x40a78d36u)));
}
// the yaw turned toward the hero, 200 degrees a second at most
void face_hero(uint32_t m, uint32_t pv) {
    const float scale = frame_scale64();
    yaw_turn_damped(m, pv + 0x44, yaw_to(m, lwc1(HERO_POS), lwc1(HERO_POS + 4)), fpu_mul(scale, u2f(0x3d4ccccdu)),
                    fpu_mul(scale, u2f(0x3e99999au)), fpu_mul(frame_time(), u2f(0x405f66f3u)));
}

// the update's EE frame: the walker step's output, its start, scratch quads, the yaw and the spring's value
struct Frame {
    uint32_t walk_out, start, buf40, buf50, buf60, buf70, buf80, yaw, spring;
    uint32_t walked;  // (the follow's walker step's result bits)
};
}  // namespace

// Every moby of the moby's group (+0x21) of its class that is shown (+0x31) gets three glows, at its pvars' points
// +0x140 / +0x150 / +0x160 in its colour +0x290, the size doubled while 0x15edb3 is set.
// orig: L6:0x002f37d0 L10:0x002d7808
void class857_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const float size = lbu(0x0015edb3) != 0 ? u2f(0x3de978d5u) : u2f(0x3d6978d5u);
    const float toward = u2f(0x3da3d70au);
    group_first(sp, lbu(m + 0x21), 0, 0);
    while (lwu(sp) != 0) {
        const uint32_t o = lwu(sp);
        if (lbu(o + 0x31) != 0 && lh(o + 0xa6) == lh(m + 0xa6)) {
            const uint32_t pv = lwu(o + 0x78);
            for (uint32_t k = 0; k < 3; ++k) {
                const uint32_t point = pv + 0x140 + k * 0x10;
                glow_draw(point, lwu(pv + 0x290), size, toward);  // (Blarg's copy, 0x272498, is the same code)
            }
        }
        group_next(sp, lwu(sp), 0, 0);
    }
    ee_stack_free(0x70);
}

// Its score for a target (lower is better): the distance, ten times the turn the Gadgebot needs, three times the angle off
// the camera's view; 10 more when the target is hidden (+0x31), 100 more when `state` is 9.
// orig: L6:0x002f2148 L10:0x002d6180
float class857_score(uint32_t m, uint32_t o, int32_t state) {
    const float dist = vec2_distance(V(m + 0x10), V(o + 0x10));
    const float off_view = angle_dist(lwc1(OV(0x00166dd8)), yaw_to(m, lwc1(o + 0x10), lwc1(o + 0x14)));
    const float turn = angle_dist(lwc1(m + 0x48), yaw_to(m, lwc1(o + 0x10), lwc1(o + 0x14)));
    float score = fpu_add(dist, fpu_mul(turn, 10.0f));
    score = fpu_add(score, fpu_mul(off_view, 3.0f));
    if (lbu(o + 0x31) == 0) score = fpu_add(score, 10.0f);
    if (state == 9) score = fpu_add(score, 100.0f);
    return score;
}

// The targets dealt among the Gadgebots of the level's list that attack (state 5, not striking) without a live target, at
// most 50: each lists the level's mobys of target kind 5 (their class's +0x46) within reach (12, 5 while +0x198 is set; 1
// in height) and in sight (the segment between them, 0.25 up, hits nothing but the target), its eight best scores kept in
// order (+0x1a0 / +0x1c0, the count +0x192). Then the targets, the most wanted first (each listing weighs 8 for its best ..
// 1 for its eighth), each go to the Gadgebots that score them best, as many as the target takes (its pvars' +4, else 1),
// fanned out about it 30 degrees apart (100 at most; +0x17c). Every Gadgebot dealt keeps the frame (+0x18c) and takes its
// target (+0x174; +0x19a set, and +0x198 when it has one).
// orig: L6:0x002f2250 L10:0x002d6288
void class857_assign() {
    const uint32_t sp = ee_stack_alloc(0x250);
    const uint32_t bots = sp, cands = sp + 0xf0, weights = sp + 0x1c0, dealt = sp + 0x200, from = sp + 0xe0, to = sp + 0xd0;
    const uint32_t list = OV(0x001aba00);
    int32_t nbots = 0;
    for (uint32_t p = list; lwu(p) != 0; p += 4) {
        const uint32_t o = lwu(p);
        if (lh(o + 0xa6) != CLASS || lbu(o + 0x20) != 5 || lbu(o + 0xbc) == 2 || nbots == 50) continue;
        const uint32_t t = lwu(lwu(o + 0x78) + 0x174);
        if (t != 0 && lbu(t + 0x20) != 0xfe && lbu(t + 0x20) != 0xfd) continue;
        sw(bots + static_cast<uint32_t>(nbots) * 4, o);
        ++nbots;
    }
    // each one's candidates, in the order of their scores
    for (int32_t i = 0; i < nbots; ++i) {
        const uint32_t bot = lwu(bots + static_cast<uint32_t>(i) * 4), bpv = lwu(bot + 0x78);
        sh(bpv + 0x192, 0);
        for (uint32_t p = list; lwu(p) != 0; p += 4) {
            const uint32_t c = lwu(p);
            const uint32_t cls = lwu(c + 0x24);
            if ((cls == 0 ? 0 : lh(cls + 0x46)) != 5) continue;
            const float reach = lh(bpv + 0x198) != 0 ? 5.0f : 12.0f;
            if (reach < vec2_distance(V(bot + 0x10), V(c + 0x10))) continue;
            if (1.0f < abs_f(fpu_sub(lwc1(bot + 0x18), lwc1(c + 0x18)))) continue;
            lq_sq(from, bot + 0x10);
            lq_sq(to, c + 0x10);
            swc1(from + 8, fpu_add(lwc1(from + 8), 0.25f));
            swc1(to + 8, fpu_add(lwc1(to + 8), 0.25f));
            if (segment_collide_v(V(from), V(to), 2, bot, nullptr) != 0 && lwu(OV(0x00173e40) + 0x18) != c) continue;
            const float score = class857_score(bot, c, 5);
            int32_t at = 0;
            for (; at < lh(bpv + 0x192); ++at) {
                if (!(score < lwc1(bpv + 0x1c0 + static_cast<uint32_t>(at) * 4))) continue;
                // (the worse ones moved down, the eighth dropped)
                for (int32_t j = std::min(lh(bpv + 0x192) - 1, 6); j >= at; --j) {
                    const uint32_t o = static_cast<uint32_t>(j) * 4;
                    sw(bpv + 0x1a4 + o, lwu(bpv + 0x1a0 + o));
                    swc1(bpv + 0x1c4 + o, lwc1(bpv + 0x1c0 + o));
                }
                break;
            }
            if (at < 8) {
                sw(bpv + 0x1a0 + static_cast<uint32_t>(at) * 4, c);
                swc1(bpv + 0x1c0 + static_cast<uint32_t>(at) * 4, score);
                const uint32_t n = lhu(bpv + 0x192) + 1u;
                sh(bpv + 0x192, n);
                if (!(static_cast<int16_t>(n) < 9)) sh(bpv + 0x192, 8);
            }
        }
    }
    // the targets listed, each weighed by its places in the lists (at most 50 targets)
    int32_t ncands = 0;
    for (int32_t i = 0; i < nbots; ++i) {
        const uint32_t bpv = lwu(lwu(bots + static_cast<uint32_t>(i) * 4) + 0x78);
        for (int32_t k = 0; k < lh(bpv + 0x192); ++k) {
            if (!(ncands < 50)) continue;
            const uint32_t weight = static_cast<uint32_t>(8 - k);
            const uint32_t c = lwu(bpv + 0x1a0 + static_cast<uint32_t>(k) * 4);
            int32_t j = 0;
            for (; j < ncands; ++j) {
                if (lwu(cands + static_cast<uint32_t>(j) * 4) != c) continue;
                sb(weights + static_cast<uint32_t>(j), lbu(weights + static_cast<uint32_t>(j)) + weight);
                break;
            }
            if (j == ncands) {
                sw(cands + static_cast<uint32_t>(ncands) * 4, c);
                sb(weights + static_cast<uint32_t>(ncands), weight);
                ++ncands;
            }
        }
    }
    for (int32_t i = 0; i < nbots; ++i) sw(lwu(lwu(bots + static_cast<uint32_t>(i) * 4) + 0x78) + 0x178, 0);
    // the targets dealt, the most wanted first
    for (int32_t round = 0; round < ncands; ++round) {
        int32_t best = 0, most = 0;
        for (int32_t j = 0; j < ncands; ++j) {
            const int32_t w = static_cast<int8_t>(lbu(weights + static_cast<uint32_t>(j)));
            if (most < w) {
                most = w;
                best = j;
            }
        }
        sb(weights + static_cast<uint32_t>(best), 0xff);
        const uint32_t c = lwu(cands + static_cast<uint32_t>(best) * 4);
        const uint32_t takes = moby_pvar_word0(c);
        const int32_t wanted = takes != 0 ? lh(takes + 4) : 1;
        int32_t got = 0;
        while (got < wanted) {
            float best_score = u2f(0x4b18967fu);  // (9999999)
            uint32_t winner = 0;
            for (int32_t i = 0; i < nbots; ++i) {
                const uint32_t bot = lwu(bots + static_cast<uint32_t>(i) * 4), bpv = lwu(bot + 0x78);
                if (lwu(bpv + 0x178) != 0) continue;
                for (int32_t k = 0; k < lh(bpv + 0x192); ++k) {
                    if (lwu(bpv + 0x1a0 + static_cast<uint32_t>(k) * 4) != c) continue;
                    const float s = lwc1(bpv + 0x1c0 + static_cast<uint32_t>(k) * 4);
                    if (s < best_score) {
                        best_score = s;
                        winner = bot;
                    }
                }
            }
            if (winner == 0) break;
            sw(lwu(winner + 0x78) + 0x178, c);
            sw(dealt + static_cast<uint32_t>(got) * 4, winner);
            ++got;
        }
        if (got == 0) continue;
        const float gaps = static_cast<float>(got - 1);
        float fan = fpu_mul(gaps, u2f(0x3f060a92u));  // (30 degrees a gap, 100 at most)
        if (u2f(0x3fdf66f3u) < fan) fan = u2f(0x3fdf66f3u);
        const float step = got < 2 ? 0.0f : fpu_div(fan, gaps);
        const float first = fpu_mul(fan, -0.5f);
        for (int32_t i = 0; i < got; ++i) {
            const uint32_t bpv = lwu(lwu(dealt + static_cast<uint32_t>(i) * 4) + 0x78);
            swc1(bpv + 0x17c, angle_add(first, fpu_mul(static_cast<float>(i), step)));
        }
    }
    const int32_t frame = frame_counter();
    for (int32_t i = 0; i < nbots; ++i) {
        const uint32_t bpv = lwu(lwu(bots + static_cast<uint32_t>(i) * 4) + 0x78);
        const uint32_t t = lwu(bpv + 0x178);
        sw(bpv + 0x18c, static_cast<uint32_t>(frame));
        sw(bpv + 0x174, t);
        sh(bpv + 0x19a, 1);
        if (t != 0) sh(bpv + 0x198, 1);
    }
    ee_stack_free(0x250);
}

// The Gadgebot `m` (its pvars `pv`) at `o`, a moby of its group or of the level's list. At a slot (classes 351, 945, 1301:
// within 1 of its goal, the slot's pvars' first word not negative) it takes the slot's next place (the slot's +4; class
// 1301 sounds 0x515), sparkles out (ten sparkles), moves to its place about the slot's linked moby (a step out, 60 degrees a
// place round from that moby's yaw), sparkles in, stands (state 4; *out = 1) and finds the walls it is inside. At an
// entrance or a pad (classes 1302, 1024, not in state 2), unless `busy`, within 1 of it, it goes in (state 8, +0x304 the
// moby; *out = 2). (The original returns 0 whatever happens: its callers' test of the result never succeeds.)
// orig: L6:0x002f2c28 L10:0x002d6c60
void class857_arrive(uint32_t m, uint32_t pv, uint32_t o, uint32_t busy, uint32_t out) {
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t goal = sp, buf_a = sp + 0x10, buf_b = sp + 0x20, buf_c = sp + 0x30;
    goal_of(goal, o);
    const int32_t cls = lh(o + 0xa6);
    if (cls == PAD || cls == ENTRANCE) {
        if (busy == 0 && lbu(o + 0x20) != 2 && vec2_distance(V(m + 0x10), V(goal)) < 1.0f) {
            sb(m + 0x20, 8);
            sw(pv + 0x304, o);
            sw(out, 2);
        }
        ee_stack_free(0x40);
        return;
    }
    if ((cls != SLOT_351 && cls != SLOT_945 && cls != SLOT_1301) || !(vec2_distance(V(m + 0x10), V(goal)) < 1.0f)) {
        ee_stack_free(0x40);
        return;
    }
    const uint32_t opv = lwu(o + 0x78);
    if (lw(opv) < 0) {
        ee_stack_free(0x40);
        return;
    }
    const int32_t place = lw(opv + 4);
    sw(opv + 4, static_cast<uint32_t>(place + 1));
    if (cls == SLOT_1301) moby_sound_play(0, 0, o, 0x515);
    for (int32_t k = 0; k < 10; ++k) sparkle(buf_b, buf_a, m + 0x10);
    const uint32_t link = first_moby() + (lwu(opv) << 8);
    const float a = angle_wrap(fpu_mul(fpu_mul(int_to_float(place), 60.0f), u2f(0x3c8efa35u)));
    swc1(buf_a, cos_vu(angle_add(lwc1(link + 0x48), a)));
    swc1(buf_a + 4, sin_vu(angle_add(lwc1(link + 0x48), a)));
    swc1(buf_a + 8, 0.0f);
    vec3_add(V(m + 0x10), V(buf_a), V(link + 0x10));
    lq_sq(pv + 0x2f0, m + 0x10);
    lq_sq(pv + 0x2e0, m + 0x10);
    for (int32_t k = 0; k < 10; ++k) sparkle(buf_c, buf_b, m + 0x10);  // (the speed's vector: where the last sparkle out was)
    sb(m + 0x20, 4);
    sw(out, 1);
    if (lw(pv + 0x310) >= 2) walls_inside(pv, m + 0x10);
    ee_stack_free(0x40);
}

// The Gadgebot's destination (+0x174, 0 none): of its group (+0x30c; else the level's list) the nearest moby of class 945,
// 1024 / 1302 (not in state 2) or 1301 (its pvars' first word not -1) within 12 and 0.2 in height; +0x19a set.
// orig: L6:0x002f2910 L10:0x002d6948
void class857_pick_dest(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x10);
    float best = 12.0f;
    sw(pv + 0x174, 0);
    auto consider = [m, pv, &best](uint32_t o) {
        const int32_t cls = lh(o + 0xa6);
        if (cls != SLOT_945 && cls != PAD && cls != SLOT_1301 && cls != ENTRANCE) return;
        if (best < vec2_distance(V(m + 0x10), V(o + 0x10))) return;
        if (u2f(0x3e4ccccdu) < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(o + 0x18)))) return;  // (0.2)
        if (1.5f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(o + 0x18)))) return;
        if ((cls == PAD || cls == ENTRANCE) && lbu(o + 0x20) == 2) return;
        if (cls == SLOT_1301 && lw(lwu(o + 0x78)) == -1) return;
        best = vec2_distance(V(m + 0x10), V(o + 0x10));
        sw(pv + 0x174, o);
    };
    if (lw(pv + 0x30c) != -1) {
        group_first(sp, lw(pv + 0x30c), 0, 0);
        while (lwu(sp) != 0) {
            consider(lwu(sp));
            group_next(sp, lwu(sp), 0, 0);
        }
    } else {
        for (uint32_t p = OV(0x001aba00); lwu(p) != 0; p += 4) consider(lwu(p));
    }
    sh(pv + 0x19a, 1);
    ee_stack_free(0x10);
}

// Whether the Gadgebot arrives at one of its group's mobys (+0x30c; else the level's list's): class857_arrive on each, `busy`
// (not in state 7) keeping it from going in. Returns what arrive gave (0 nothing, 1 at a slot, 2 going in).
// orig: L6:0x002f32f0 L10:0x002d7328
int32_t class857_find(uint32_t m, uint32_t busy) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x10);
    const uint32_t found = sp + 4;
    sw(found, 0);
    if (lw(pv + 0x30c) != -1) {
        group_first(sp, lw(pv + 0x30c), 0, 0);
        while (lwu(sp) != 0) {
            class857_arrive(m, pv, lwu(sp), busy, found);
            group_next(sp, lwu(sp), 0, 0);
        }
    } else {
        for (uint32_t p = OV(0x001aba00); lwu(p) != 0; p += 4) class857_arrive(m, pv, lwu(p), busy, found);
    }
    const int32_t r = lw(found);
    ee_stack_free(0x10);
    return r;
}

// A hit (mask 0x330001) from anything but the hero and the Gadgebots knocks it away (state 9: a jump of 2 up, its push
// along the hit's direction, sound 2), unless it is in state 9 or its animation 2 is shown or set. The hit slot (+0xa4) is
// cleared.
// orig: L6:0x002f3640 L10:0x002d7678
void class857_react(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 9 || lbu(m + 0x52) == 2 || lbu(m + 0x53) == 2) {
        sb(m + 0xa4, 0xff);
        return;
    }
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330001), 0);
    if (hit == 0) {
        sb(m + 0xa4, 0xff);
        return;
    }
    const uint32_t sp = ee_stack_alloc(0x20);
    const uint32_t dir = sp, yaw = sp + 0x10;
    swc1(yaw, atan2_f(lwc1(hit + 0x10), lwc1(hit + 0x14)));
    const uint32_t from = lwu(hit + 0x20);
    if (from != lwu(HERO_MOBY) && lh(from + 0xa6) != lh(m + 0xa6)) {
        const int32_t frames = frame_scale_i(0x50) * frame_scale_i(0x50);
        sb(pv + 0xbd, 0);
        sw(pv + 0xa4, 9);
        swc1(pv + 0x90, fpu_div(4.0f, static_cast<float>(frames)));
        jump_arc_set(pv + 0x80, 2.0f, 0.5f);
        lq_sq(dir, hit + 0x10);
        vec_yaw_marked(dir, yaw, pv + 0x98, pv + 0x9c);
        move_state_start(m, pv + 0x80, 4, 5, 2, lwc1(yaw));
        swc1(pv + 0xd0, 10.0f);
        swc1(pv + 0xd4, 20.0f);
        swc1(pv + 0xcc, fpu_add(frame_time(), frame_time()));
        moby_sound_start(2, 0, m);
        sb(m + 0x20, 9);
    }
    sb(m + 0xa4, 0xff);
    ee_stack_free(0x20);
}

// The glow (unless the moby has no group, +0x21 0xff): its colour (+0x90) eased toward its state's (green while following
// or waiting, blue while attacking or knocked away, red while going somewhere, else white) pulsing at 170 degrees a second
// (+0x28c); the three glow points at chain points 1 .. 3 (+0x140 ..), the colour kept (+0x290), and once a frame per group
// (its frame word +0x294) the draw callback added.
// orig: L6:0x002f38c8 L10:0x002d7900
void class857_glow(uint32_t m) {
    if (lbu(m + 0x21) == 0xff) return;
    const uint32_t pv = lwu(m + 0x78);
    uint32_t colour = 0x3228aaaau;
    switch (lbu(m + 0x20)) {
    case 3: case 6: colour = 0x3228aa28u; break;
    case 5: case 9: colour = 0x322828aau; break;
    case 7: colour = 0x32aa2828u; break;
    default: break;
    }
    const float phase = angle_add(lwc1(pv + 0x28c), fpu_mul(frame_time(), u2f(0x403de44eu)));
    swc1(pv + 0x28c, phase);
    const float k = fpu_add(fpu_mul(sin_vu(phase), 0.5f), 1.0f);
    auto scaled = [k](uint32_t c) {
        const int32_t v = float_to_int(fpu_mul(static_cast<float>(static_cast<int32_t>(c)), k));
        return v < 256 ? static_cast<uint32_t>(v) : 0xffu;
    };
    const uint32_t c0 = scaled(colour & 0xff), c1 = scaled((colour >> 8) & 0xff), c2 = scaled((colour >> 16) & 0xff);
    const uint32_t want = ((colour >> 24) << 24) | (c2 << 16) | (c1 << 8) | c0;
    sw(m + 0x90, rgba_lerp(lwu(m + 0x90), want, u2f(0x3dcccccdu)));
    for (uint32_t i = 0; i < 3; ++i) moby_chain_point_v(m, i + 1, V(pv + 0x140 + i * 0x10));
    const int32_t frame = frame_counter();
    sw(pv + 0x290, lwu(m + 0x90));
    if (frame == 0) return;
    const uint32_t word = lwu(pv + 0x294);
    if (lw(word) == frame) return;
    sw(word, static_cast<uint32_t>(frame));
    sub_001f93e0(draw_callback(), m);
}

namespace {
// State 3 (following the hero), its follow (+0xbc 0 starts it, 1) and its wait by him (2).
void follow(uint32_t m, uint32_t pv, Frame& f, float dist, float height) {
    lq_sq(f.buf40, m + 0x10);
    if (lw(0x0015ed84) != 6) {
        sw(pv + 0x27c, 0xffffffffu);
        if (!(lw(pv + 0x310) < 2)) walls_with_hero(m, pv);
    }
    // a switch (class 1015, not in state 4) whose box (+0x2c0 ..) holds the hero but not the Gadgebot: it stays out (state 4)
    if (lw(pv + 0x2a0) != -1 && lw(pv + 0x2c0) != -1) {
        for (uint32_t k = 0; k < 8; ++k) {
            if (k > 0 && (lw(pv + 0x2a0 + k * 4) == -1 || lw(pv + 0x2c0 + k * 4) == -1)) break;
            const uint32_t s = first_moby() + (lwu(pv + 0x2a0 + k * 4) << 8);
            if (lh(s + 0xa6) != SWITCH || lbu(s + 0x20) == 4) continue;
            if (point_in_box(lwu(HERO_MOBY) + 0x10, lw(pv + 0x2c0 + k * 4)) == 0) continue;
            if (point_in_box(m + 0x10, lw(pv + 0x2c0 + k * 4)) == 0) {
                sb(m + 0x20, 4);
                lq_sq(m + 0x10, f.buf40);
                break;
            }
        }
    }
    if (lbu(m + 0x20) == 4) return;
    const uint32_t sub = lbu(m + 0xbc);
    if (sub == 0) {
        sb(m + 0xbc, 1);
        sw(pv + 0x184, 0);
        sh(pv + 0x190, 0);
        lq_sq(pv + 0xf0, m + 0x10);
        swc1(pv + 0x17c, rand_range_signed_f(u2f(0x3e0efa35u), u2f(0x3eb2b8c2u)));  // (8 .. 20 degrees off)
    }
    if (sub == 0 || sub == 1) {
        anim_to(m, 1);
        lq_sq(f.buf60, m + 0x10);
        way_inside(m, pv, HERO_POS, f.buf50, f.buf70);
        turn_to(m, pv, angle_add(yaw_to(m, lwc1(f.buf50), lwc1(f.buf50 + 4)), lwc1(pv + 0x17c)));
        // its speed: faster when far, slower when near
        const float ft3 = fpu_mul(frame_time(), 3.0f);
        if (u2f(0x40133333u) < dist) approach_f(pv + 0x54, fpu_mul(ft3, u2f(0x3faccccdu)), frame_time_sq());  // (2.3: 1.35)
        else if (dist < u2f(0x3f99999au))
            approach_f(pv + 0x54, fpu_mul(ft3, u2f(0x3f451eb8u)), fpu_add(frame_time_sq(), frame_time_sq()));  // (1.2: 0.77)
        else approach_f(pv + 0x54, fpu_mul(ft3, u2f(0x3f733333u)), frame_time_sq());
        const float most = fpu_mul(fpu_mul(frame_time(), 3.0f), u2f(0x3faccccdu));
        if (most < lwc1(pv + 0x54)) swc1(pv + 0x54, most);
        if (u2f(0x3f4ccccdu) < dist) f.walked = walker_record_step(1.0f, m, pv + 0x30, HERO_POS, f.walk_out);
        // one frame in eight (by its slot in the moby table) its contact with the others
        if (frame_counter() % 8 == (static_cast<int32_t>(m) >> 8) % 8)
            moby_sphere_contact(m, m + 0x10, 0x1000000, 0, 1, 0, u2f(0x3ecccccdu), 1.0f, 1.0f);
        const float near = vec2_distance(V(m + 0x10), V(HERO_POS));
        const float below = abs_f(fpu_sub(lwc1(m + 0x18), lwc1(HERO_POS + 0x228)));
        floor_probe_v(0.5f, V(m + 0x10), 0);
        const uint32_t under = lwu(OV(0x00173e58));
        if (under != 0 && lh(under + 0xa6) == SLOT_945) lq_sq(m + 0x10, f.buf60);
        // stuck: every 14 frames of walking, within its reach of where it was
        bool stuck = false;
        if ((f.walked & 1) != 0 && ee_rem(lw(pv + 0x184), 14) == 0) {
            if (vec2_distance(V(pv + 0xf0), V(m + 0x10)) < lwc1(pv + 0x40)) {
                swc1(pv + 0x188, vec2_distance(V(HERO_POS), V(m + 0x10)));
                stuck = true;
            }
            lq_sq(pv + 0xf0, m + 0x10);
        }
        if ((f.walked & 0x20) != 0) {
            sb(m + 0x20, 4);
            lq_sq(m + 0x10, f.buf60);
        }
        if (near < u2f(0x3f4ccccdu) || stuck || 1.75f < below) {
            lq_sq(pv, m + 0x10);
            sb(m + 0xbc, 2);
        }
    } else if (sub == 2) {
        // waiting: off again when the hero is 1.8 from its spot (0.5 past the stuck distance after a stand-off)
        const float d = vec2_distance(V(pv), V(HERO_POS));
        const bool away = lh(pv + 0x190) != 0 ? fpu_add(lwc1(pv + 0x188), 0.5f) < d : u2f(0x3fe66666u) < d;
        if (away && height < 1.75f) sb(m + 0xbc, 0);
        anim_to(m, 0);
        face_hero(m, pv);
    }
    sw(pv + 0x184, lw(pv + 0x184) + 1);
}

// State 5 (attacking): its target dealt (class857_assign), approached (1), struck (2: a hit of kind 0x10000 at the
// animation's frames 12 .. 15), a pause (3), no target (4).
void attack(uint32_t m, uint32_t pv, Frame& f) {
    if (lw(0x0015ed84) != 6) {
        sw(pv + 0x27c, 0xffffffffu);
        walls_with_hero(m, pv);
    }
    if (lbu(m + 0xbc) == 0) {
        sb(m + 0xbc, 1);
        sw(pv + 0x174, 0);
        sh(pv + 0x198, 0);
        sh(pv + 0x19a, 0);
        swc1(pv + 0x17c, rand_range_signed_f(u2f(0x3e0efa35u), u2f(0x3eb2b8c2u)));
        sw(pv + 0x18c, static_cast<uint32_t>(frame_counter()));
    }
    switch (lbu(m + 0xbc)) {
    case 1: {
        if (lw(pv + 0x18c) < frame_counter()) class857_assign();
        const uint32_t t = lwu(pv + 0x174);
        if (t == 0) {
            if (lh(pv + 0x19a) != 0) {
                sh(pv + 0x19a, 0);
                sw(pv + 0x194, static_cast<uint32_t>(frame_scale_i(60)));
                sb(m + 0xbc, 4);
            }
            break;
        }
        anim_to(m, 5);
        way_inside(m, pv, t + 0x10, f.buf40, f.buf60);
        const float toward = angle_add(yaw_to(m, lwc1(f.buf40), lwc1(f.buf40 + 4)), lwc1(pv + 0x17c));
        swc1(f.buf50, fpu_mul(cos_vu(toward), 3.0f));
        swc1(f.buf50 + 4, fpu_mul(sin_vu(toward), 3.0f));
        sw(f.buf50 + 8, 0);
        vec3_add(V(f.buf50), V(f.buf50), V(m + 0x10));
        // (its way 3 ahead crossing its walls: the offset eased to 0)
        if (lw(pv + 0x27c) != -1 && path_crossed(lwu(pv + 0x1e0 + (lwu(pv + 0x27c) << 2)), m + 0x10, f.buf50) != 0)
            turn_towards(pv + 0x17c, 0, 0.0f, fpu_mul(frame_time(), u2f(0x3fc90fdbu)));
        turn_to(m, pv, toward);
        swc1(pv + 0x54, fpu_mul(frame_time(), 4.5f));
        if ((walker_record_step(1.0f, m, pv + 0x30, t + 0x10, f.walk_out) & 0x20) != 0) {
            sb(m + 0x20, 4);
            lq_sq(m + 0x10, f.start);
        }
        if (vec2_distance(V(m + 0x10), V(t + 0x10)) < 1.5f) {
            // in reach: the segment 0.4 .. 0.7 ahead of it, 0.4 up, meets the target: strike
            const float out = u2f(0x3ecccccdu), on = u2f(0x3e99999au);
            lq_sq(f.buf70, m + 0x10);
            swc1(f.buf70 + 8, fpu_add(lwc1(f.buf70 + 8), out));
            swc1(f.buf70, fpu_add(lwc1(f.buf70), fpu_mul(cos_vu(lwc1(m + 0x48)), out)));
            swc1(f.buf70 + 4, fpu_add(lwc1(f.buf70 + 4), fpu_mul(sin_vu(lwc1(m + 0x48)), out)));
            lq_sq(f.buf80, f.buf70);
            swc1(f.buf70, fpu_add(lwc1(f.buf70), fpu_mul(cos_vu(lwc1(m + 0x48)), on)));
            swc1(f.buf70 + 4, fpu_add(lwc1(f.buf70 + 4), fpu_mul(sin_vu(lwc1(m + 0x48)), on)));
            if (segment_collide_v(V(f.buf70), V(f.buf80), 0, m, nullptr) != 0 && lwu(OV(0x00173e58)) == lwu(pv + 0x174))
                sb(m + 0xbc, 2);
        }
        break;
    }
    case 2: {
        anim_to(m, 2, 8);
        const uint32_t t = lwu(pv + 0x174);
        if (t != 0) {
            const float time = moby_anim_time(m);
            if (12.0f <= time && time <= 15.0f) {
                swc1(f.buf70, cos_vu(lwc1(m + 0x48)));
                swc1(f.buf70 + 4, sin_vu(lwc1(m + 0x48)));
                sw(f.buf70 + 8, 0);
                sub_00259888(2.0f, f.buf40, static_cast<int32_t>(m), 0x10000, f.buf70);
                moby_hit_record(t, f.buf40);
            }
        }
        if ((lbu(m + 0x70) & 2) != 0) {
            sh(pv + 0x19a, 0);
            sw(pv + 0x194, static_cast<uint32_t>(frame_scale_i(30)));
            sb(m + 0xbc, 3);
        }
        break;
    }
    case 3:
        anim_to(m, 0);
        if (lh(pv + 0x19a) != 0 && lwu(pv + 0x174) == 0) {
            sh(pv + 0x19a, 0);
            sw(pv + 0x194, static_cast<uint32_t>(frame_scale_i(60)));
            sb(m + 0xbc, 4);
        }
        if (tick32(pv + 0x194) != 0 && lwu(pv + 0x174) != 0) sb(m + 0xbc, 1);
        break;
    case 4: {
        if (lw(pv + 0x18c) < frame_counter()) class857_assign();
        anim_to(m, 0);
        const int32_t done = tick32(pv + 0x194);
        const bool again = lh(pv + 0x19a) != 0;
        const uint32_t t = lwu(pv + 0x174);
        if (again && t == 0) {
            if (done != 0) {  // (nothing to attack: back to following)
                sb(m + 0x20, 3);
                sb(m + 0xbc, 0);
            } else {
                sh(pv + 0x19a, 0);
            }
        }
        if (t != 0) sb(m + 0xbc, 1);
        break;
    }
    default:
        break;
    }
}

// State 8: going in at +0x304 (an entrance or a pad), turned and drawn in toward it 4 a second, shrinking in its last 1 with
// a glow (particle 0x1a); in, it counts off the entrance's +0x14 (sound 0x516: 1 at the last) and is gone.
void go_in(uint32_t m, uint32_t pv, Frame& f) {
    const uint32_t e = lwu(pv + 0x304);
    swc1(f.yaw, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(e + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(e + 0x14))));
    float d = vec2_distance(V(e + 0x10), V(m + 0x10));
    turn_towards(f.yaw, 0, lwc1(e + 0x48), fpu_mul(frame_time(), u2f(0x40c90fdbu)));
    d = fpu_sub(d, fpu_mul(frame_time(), 4.0f));
    if (d <= 0.0f) {
        const uint32_t epv = lwu(e + 0x78);
        if (lw(epv + 0x14) > 0) sw(epv + 0x14, static_cast<uint32_t>(lw(epv + 0x14) - 1));
        moby_sound_play(lw(epv + 0x14) == 0 ? 1 : 0, 0, e, 0x516);
        moby_retire(m);
        return;
    }
    swc1(f.buf40, fpu_mul(cos_vu(lwc1(f.yaw)), d));
    swc1(f.buf40 + 4, fpu_mul(sin_vu(lwc1(f.yaw)), d));
    swc1(f.buf40 + 8, 0.0f);
    swc1(m + 0x10, fpu_add(lwc1(f.buf40), lwc1(e + 0x10)));
    swc1(m + 0x14, fpu_add(lwc1(f.buf40 + 4), lwc1(e + 0x14)));
    if (d < 1.0f) {
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), d));
        spawn_particle_1a(m, 0x3000407fu, static_cast<uint32_t>(frame_scale_i(2)), 3,
                          fpu_mul(fpu_mul(fpu_sub(1.0f, d), 0.5f), u2f(0x484d1400u)));  // (x 210000)
    }
    if (lbu(m + 0x31) != 0) class857_glow(m);
    sw(m + 0x94, 0);
    sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
}

// The frame's end for the states that walk: a slot or an entrance reached (class857_find; going in ends the frame); then (not
// in state 1) the walker's step in place, its move kept to the slope's spring (+0x284), the push out of the walls (unless at a
// slot: six tries, up a slope only) and the floor (back to the start when it drops by +0x3c), the fall (7 down a second
// squared, 8 a second at most) eased onto the floor (+0x288), and back home (state 4) when it fell 5 below it or left the
// level's box; then (all) the moby under it (+0x19c), a step back to the start when the way down from 1 up meets a
// Gadgebot, a class-1033 moby or a class-500 moby in range, the floor band near the camera, the glow, and its last positions.
void tail(uint32_t m, uint32_t pv, Frame& f) {
    const int32_t arrived = class857_find(m, lbu(m + 0x20) != 7 ? 1u : 0u);
    if (arrived == 2) return;
    if (lbu(m + 0x20) != 1) {
        lq_sq(f.buf40, m + 0x10);
        // (the original also adds the level's word standoff() and 0.02 to the height of a stack copy it never reads)
        const float reach = lwc1(pv + 0x40), speed = lwc1(pv + 0x54);
        sw(pv + 0x40, 0);
        sw(pv + 0x54, 0);
        walker_record_step(1.0f, m, pv + 0x30, m + 0x10, f.walk_out);
        swc1(pv + 0x40, reach);
        swc1(pv + 0x54, speed);
        swc1(m + 0x18, lwc1(f.buf40 + 8));
        if (u2f(0x3ba3d70au) < vec3_distance(V(f.buf40), V(m + 0x10))) {  // (0.005)
            vec3_sub(V(f.buf50), V(m + 0x10), V(f.buf40));
            if (vec3_length(V(f.buf50)) < 0.25f) vec3_set_length(V(f.buf50), V(f.buf50), 0.25f);
            lq_sq(m + 0x10, f.buf40);
            const float most = fpu_mul(frame_time(), 3.0f);
            swc1(f.spring, -vec3_length(V(f.buf50)));
            const float scale = frame_scale64();
            spring_to(f.spring, pv + 0x284, 0.0f, fpu_mul(scale, u2f(0x3ccccccdu)), fpu_mul(scale, u2f(0x3e99999au)), most);
            const float len = vec3_length(V(f.buf50));
            if (len < lwc1(pv + 0x284)) swc1(pv + 0x284, len);
            if (lwc1(pv + 0x284) < len) vec3_set_length(V(f.buf50), V(f.buf50), lwc1(pv + 0x284));
            vec3_add(V(m + 0x10), V(m + 0x10), V(f.buf50));
        } else {
            sw(pv + 0x284, 0);
            lq_sq(m + 0x10, f.buf40);
        }
        if (lbu(m + 0x20) != 9) {
            if (arrived == 0) {
                const float z = lwc1(m + 0x18);
                for (int32_t k = 0; k < 6; ++k) {
                    lq_sq(f.buf50, m + 0x10);
                    swc1(f.buf50 + 8, fpu_add(lwc1(f.buf50 + 8), lwc1(pv + 0x34)));
                    if (sphere_collide(f.buf50, fpu_mul(int_to_float(lw(pv + 0x30)), u2f(0x3a800000u)), 6, m) == 0) break;
                    const float flat = vec2_length(V(OV(0x00173e80)));
                    if (u2f(0x3f060a92u) < atan2_f(lwc1(OV(0x00173e88)), flat)) {  // (the normal 30 degrees up or more)
                        lq_sq(m + 0x10, OV(0x00173e70));
                        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x34)));
                    }
                }
                swc1(m + 0x18, z);
                const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
                if (lwc1(pv + 0x3c) <= abs_f(fpu_sub(lwc1(f.start + 8), floor))) {
                    lq_sq(m + 0x10, f.start);
                    sw(pv + 0x284, 0);
                }
            }
            const float fall = fpu_sub(lwc1(pv + 0x48), fpu_mul(frame_time_sq(), 7.0f));
            const float fastest = fpu_mul(frame_time(), -8.0f);
            swc1(pv + 0x48, fall < fastest ? fastest : fall);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.25f));
            const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
            const float z = fpu_sub(lwc1(m + 0x18), 0.25f);
            swc1(m + 0x18, z);
            if (z < floor) {
                const float scale = frame_scale64();
                spring_to(m + 0x18, pv + 0x288, floor, fpu_mul(scale, u2f(0x3d23d70au)), fpu_mul(scale, u2f(0x3e99999au)),
                          fpu_mul(frame_time(), 1.5f));
                sw(pv + 0x48, 0);
            } else if (floor < z) {
                const float next = fpu_add(z, lwc1(pv + 0x48));
                swc1(m + 0x18, next < floor ? floor : next);
            }
        }
        const float z = lwc1(m + 0x18), x = lwc1(m + 0x10), y = lwc1(m + 0x14);
        if (z < fpu_sub(lwc1(pv + 0xe8), 5.0f) || x < 2.0f || 1021.0f < x || y < 2.0f || 1021.0f < y || z < 2.0f || 1021.0f < z) {
            lq_sq(m + 0x10, pv + 0xe0);
            sb(m + 0x20, 4);
            return;
        }
    }
    const uint32_t under = lwu(OV(0x00173e58));
    sw(pv + 0x19c, under != 0 && lw(OV(0x00173e5c)) > 0 ? under : 0u);
    lq_sq(f.buf40, m + 0x10);
    const float low = fpu_sub(lwc1(f.buf40 + 8), 2.0f);
    swc1(f.buf40 + 8, low < 0.5f ? 0.5f : low);
    lq_sq(f.buf50, m + 0x10);
    swc1(f.buf50 + 8, fpu_add(lwc1(f.buf50 + 8), 1.0f));
    if (segment_collide_v(V(f.buf50), V(f.buf40), 0, m, nullptr) != 0) {
        const uint32_t o = lwu(OV(0x00173e58));
        if (o != 0) {
            const int32_t cls = lh(o + 0xa6);
            if (cls == 0x409 || cls == CLASS || moby_in_range_class_500(o) != 0) lq_sq(m + 0x10, f.start);
        }
    }
    if (lbu(m + 0x31) != 0) {
        if (vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 17.0f) {
            moby_floor_band(m);
            sb(m + 0x7f, 0xd);
        }
        if (lbu(m + 0x31) != 0) class857_glow(m);
    }
    lq_sq(pv + 0x2f0, pv + 0x2e0);
    lq_sq(pv + 0x2e0, m + 0x10);
}
}  // namespace

// The Gadgebot. Each frame (but in state 1) a hit may knock it away (class857_react), and in range of the hero (16, 1.5 in
// height) in states 2 .. 7 it counts in the command ring's range and takes the hero's command (0x141610: 1 follow, state 3;
// 2 stand, 4; 3 attack, 5; 4 go, 7); its size eased to 2.3 while the size flag is set (1 in state 1). Then the state: 0 sets
// up (its light, class 302, and class 303 spawned; gone at once when its checkpoint was collected); 1 dormant by its light
// until the light is broken; 2 wanders; 3 follows the hero; 4 stands facing him; 5 attacks; 7 goes to the nearest slot or
// entrance; 8 goes in; 9 knocked away (home on landing, sparkling). The walking states end with tail().
// orig: L6:0x002f0040 L10:0x002d4078
void class857_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xa0);
    Frame f{sp, sp + 0x20, sp + 0x40, sp + 0x50, sp + 0x60, sp + 0x70, sp + 0x80, sp + 0x90, sp + 0x94, 0};
    const float dist = vec2_distance(V(m + 0x10), V(HERO_POS));
    const float height = abs_f(fpu_sub(lwc1(m + 0x18), lwc1(0x0013f5f8)));
    lq_sq(f.start, m + 0x10);
    if (lbu(m + 0x20) != 1) {
        class857_react(m);
        if (dist < 16.0f && abs_f(fpu_sub(lwc1(m + 0x18), lwc1(HERO_POS + 8))) < 1.5f) {
            const uint32_t st = lbu(m + 0x20);
            if (st >= 2 && st <= 7) {
                const int32_t cmd = lw(0x00141610);
                sw(OV(0x0017e504), lwu(OV(0x0017e504)) + 1);
                if (cmd == 2) {
                    sb(m + 0x20, 4);
                } else if (cmd == 1 || cmd == 3) {
                    const uint32_t want = cmd == 1 ? 3u : 5u;
                    if (lbu(m + 0x20) != want) {
                        sb(m + 0x20, want);
                        sb(m + 0xbc, 0);
                    }
                } else if (cmd == 4) {
                    sb(m + 0xbc, 0);
                    sb(m + 0x20, 7);
                }
            }
        }
    }
    if (lbu(0x0015edb3) != 0) {
        if (lbu(pv + 0x101) == 0) sub_0024f590(m, 0, pv + 0x100);
        const float size = lbu(m + 0x20) == 1 ? 1.0f : u2f(0x40133333u);  // (2.3)
        approach_f(pv + 0x308, size, fpu_mul(lwc1(0x0015ed60), u2f(0x3dcccccdu)));
        const float s = lwc1(pv + 0x308);
        swc1(pv + 0x128, s);
        swc1(pv + 0x120, s);
        swc1(pv + 0x124, s);
    } else if (lbu(pv + 0x101) != 0) {
        sub_0024f608(m, pv + 0x100);
    }
    bool walking = true;  // (the states that end with tail())
    // the states' table (L6 0x201600); 6 and past 9 (0x2f1b18): tail() only
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x2f02c8)
        swc1(pv + 0x308, 1.0f);
        swc1(m + 0x18, floor_probe_v(0.5f, V(m + 0x10), 0));
        class303_spawn(m);
        lq_sq(pv + 0x2f0, m + 0x10);
        lq_sq(pv + 0x2e0, m + 0x10);
        if (lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) == 0xff) {  // (its checkpoint collected)
            moby_retire(m);
            walking = false;
            break;
        }
        sw(pv + 0x170, class302_spawn(m));
        swc1(m + 0x18, floor_probe_v(0.5f, V(m + 0x10), 0));
        lq_sq(pv, m + 0x10);
        lq_sq(pv + 0xe0, m + 0x10);
        const float ft = frame_time(), scale = frame_scale64();
        swc1(pv + 0x14, fpu_add(ft, ft));
        swc1(pv + 0x18, fpu_mul(ft, u2f(0x40278d36u)));  // (150 degrees a second)
        sw(pv + 0x30, 300);
        swc1(pv + 0x34, u2f(0x3f028f5cu));  // (0.51)
        const float step = lwc1(pv + 0x300);
        swc1(pv + 0x3c, step);
        swc1(pv + 0x54, fpu_add(fpu_mul(ft, 3.0f), fpu_mul(ft, u2f(0x3dcccccdu))));
        swc1(pv + 0x4c, fpu_mul(scale, u2f(0x3ca3d70au)));
        swc1(pv + 0x58, u2f(0x3f32b8c2u));  // (40 degrees)
        swc1(pv + 0x5c, u2f(0x3e4ccccdu));
        swc1(pv + 0x6c, fpu_mul(scale, u2f(0x3cf5c28fu)));
        swc1(pv + 0x70, fpu_mul(scale, u2f(0x3e99999au)));
        swc1(pv + 0x74, fpu_mul(ft, u2f(0x40490fdbu)));
        swc1(pv + 0x38, step);
        swc1(pv + 0x50, fpu_mul(scale, u2f(0x3e99999au)));
        sw(pv + 0x78, static_cast<uint32_t>(frame_scale_i(4)));
        sw(pv + 0x68, lwu(pv + 0x68) | 1u);
        for (uint32_t k = 0; k < 16; ++k)
            if (lw(pv + 0x1e0 + k * 4) >= 0) sw(pv + 0x310, static_cast<uint32_t>(lw(pv + 0x310) + 1));
        swc1(pv + 0x28c, rand_angle());
        sw(pv + 0x27c, 0xffffffffu);
        sb(m + 0x20, 1);
        sw(m + 0x94, 0);
        break;
    }
    case 1: {  // (0x2f04bc)
        const uint32_t light = lwu(pv + 0x170);
        if (light != 0 && lbu(light + 0x20) < 2) {  // dormant by its light: hidden and still while the hero is Clank
            if (lbu(0x001413f4) == 0) {
                sh(m + 0x34, lhu(m + 0x34) | 0x41u);
                sh(light + 0x34, lhu(light + 0x34) | 0x41u);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                sh(light + 0x34, lhu(light + 0x34) & 0xefffu);
                sw(light + 0x94, 0);
            } else {
                sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
                sh(light + 0x34, lhu(light + 0x34) & 0xffbeu);
                sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
                sh(light + 0x34, lhu(light + 0x34) | 0x1000u);
                sw(light + 0x94, lwu(lwu(light + 0x24) + 0x10));
            }
            if (lbu(m + 0x31) != 0) class857_glow(m);
            walking = false;
            break;
        }
        // the light broken: awake (its walls' links built, the walls it is inside found), following
        if (lw(pv + 0x310) > 0) {
            for (uint32_t k = 0; k < 16; ++k) {
                const uint32_t w = pv + 0x1e0 + k * 4;
                if (lw(w) < 0) continue;
                path_links_build(w, 1, lwu(w + 0x40));
                if (nov_276a48_l1(w, 1, lw(w + 0x40), m + 0x10, 0.0f) != 0) sw(pv + 0x27c, k);
            }
        }
        if (lbu(0x0015edb3) != 0) {
            swc1(standoff(), u2f(0x3eb33333u));  // (0.35)
            sw(pv + 0x30, 0x177);
        } else {
            sw(pv + 0x30, 300);
            swc1(standoff(), u2f(0x3e8a3d71u));  // (0.27)
        }
        sh(m + 0x34, (lhu(m + 0x34) & 0xffbeu) | 0x1000u);
        sb(m + 0x20, 3);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        break;
    }
    case 2:  // (0x2f0664)
        anim_to(m, 1);
        wander_update(m, pv, u2f(0x3eb33333u), 0.25f);
        break;
    case 3:  // (0x2f06c8)
        follow(m, pv, f, dist, height);
        break;
    case 4:  // (0x2f0e54)
        anim_to(m, 0);
        face_hero(m, pv);
        break;
    case 5:  // (0x2f0ef8)
        attack(m, pv, f);
        break;
    case 7: {  // (0x2f15c8)
        const uint32_t sub = lbu(m + 0xbc);
        if (sub == 0) {
            sb(m + 0xbc, 1);
            class857_pick_dest(m);
        } else if (sub == 1) {
            const uint32_t t = lwu(pv + 0x174);
            if (t == 0) {
                if (lh(pv + 0x19a) != 0) {
                    sb(m + 0xbc, 0);
                    sb(m + 0x20, 3);
                }
            } else {
                anim_to(m, 1);
                goal_of(f.buf40, t);
                turn_to(m, pv, yaw_to(m, lwc1(f.buf40), lwc1(f.buf40 + 4)));
                if ((walker_record_step(1.0f, m, pv + 0x30, f.buf40, f.walk_out) & 0x20) != 0) {
                    sb(m + 0x20, 4);
                    lq_sq(m + 0x10, f.start);
                }
            }
        }
        break;
    }
    case 8:  // (0x2f172c)
        go_in(m, pv, f);
        walking = false;
        break;
    case 9:  // (0x2f18f0)
        if ((enemy_physics(m, pv + 0x80) & 0x41u) != 0) {  // (landed: home, sparkling)
            explosion_burst(m, m + 0x10, -1, 0.5f, 13.0f);
            lq_sq(m + 0x10, pv + 0xe0);
            sb(m + 0x20, 4);
            for (int32_t k = 0; k < 10; ++k) sparkle(f.buf50, f.buf40, m + 0x10);
            walking = false;
        }
        break;
    default:
        break;
    }
    if (walking) tail(m, pv, f);
    ee_stack_free(0xa0);
}

}  // namespace rac1
