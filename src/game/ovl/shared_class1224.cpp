// Class 1224, a bolt of lightning along one of the level's volumes (the level's volume table *0x1600ec, 0x80 bytes a
// volume: its axes +0x00 / +0x10 / +0x20, its centre +0x30, its rotation +0x70), its helpers and its draw callback: one port
// of the copies Oltanis, Quartu and Drek's fleet carry (the same code relinked). Each level keeps its own block of the
// class's tuning words (per level below); the rest is the engine's (OV: the volume table, the camera 0x166dc0, the
// collision's hit point 0x173e60) or the base ELF's (bare: the hero 0x13f3d0, the frame time 0x15ed6c). Header
// shared_class1224.h.
//
// The pvars: +0 the volume's index, +4 / +6 the frames between strikes (the second: of a quick strike, 0 for none), +8 / +0xa
// the bolt's two ends still to be clipped to the scenery, +0xc the frames the bolt lasts, +0x10 the timer, +0x12 the
// bolt's frames left, +0x14 its colour, +0x18 the forks' four points (indices into the bolt), +0x1c the reach of the strike
// sweeps; +0x20 the bolt's 32 points, +0x220 their drift; +0x420 the four forks' four points (one row of four a step),
// +0x520 their drift (the same); +0x620 the bolt's frames at its start, +0x624 the volume's length (the cull radius),
// +0x630 / +0x640 the clipped ends.
#include <game/ovl/shared_class1224.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// The level's tuning block (0x6c bytes): +0 strikes left in state 1, +4 / +8 the bolt's frames (from, to), +0xc / +0x10
// the fade's two frame marks, +0x14 the class off, +0x18 no fade, +0x1c not drawn, +0x20 no strike sweeps, +0x24 the
// texture, +0x28..+0x34 the blend's four selectors, +0x38 / +0x3c / +0x40 the fade's three colours, +0x44 the bolt's
// spread, +0x48 its drift, +0x50 / +0x54 the sparks' colours, +0x5c their w, +0x64 / +0x68 their two textures.
uint32_t tuning() {
    if (on_level(QUARTU)) return L15_ADDR(0x00161f44);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161f24);
    return L14_ADDR(0x001620b8);
}
// the level's four texture coordinates of a quad's corners ((s, t) pairs)
uint32_t corner_uvs() {
    if (on_level(QUARTU)) return L15_ADDR(0x001d94f0);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x001d9860);
    return L14_ADDR(0x001e5fd0);
}
// the level's copy of the draw callback (queued by the update: the levels still register their copies)
uint32_t draw_callback() {
    if (on_level(QUARTU)) return L15_ADDR(0x002e6a10);
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x002e6e40);
    return L14_ADDR(0x00301fa8);
}

constexpr int32_t POINTS = 32;
constexpr int32_t FORKS = 4;

// the moby's volume (pvar +0 its index)
uint32_t volume(uint32_t pv) { return lwu(OV(0x001600ec)) + (lwu(pv) << 7); }

uint32_t point(uint32_t pv, int32_t i) { return pv + 0x20 + static_cast<uint32_t>(i) * 0x10; }
uint32_t drift(uint32_t pv, int32_t i) { return pv + 0x220 + static_cast<uint32_t>(i) * 0x10; }
// a fork's point `step` (0: where it leaves the bolt) and its drift (`step` 1 .. 4: rows 0 .. 3)
uint32_t fork_point(uint32_t pv, int32_t fork, int32_t step) { return pv + 0x420 + static_cast<uint32_t>(step) * 0x40 + static_cast<uint32_t>(fork) * 0x10; }
uint32_t fork_drift(uint32_t pv, int32_t fork, int32_t step) { return pv + 0x520 + static_cast<uint32_t>(step) * 0x40 + static_cast<uint32_t>(fork) * 0x10; }

// One sweep of the strike: the segment from `a` + `off` to `b` + `off`, hitting what it crosses with the record `hit`.
void sweep(uint32_t m, uint32_t a, uint32_t b, uint32_t off, uint32_t from, uint32_t to, uint32_t hit, bool add) {
    if (add) {
        vec3_add(V(from), V(a), V(off));
        vec3_add(V(to), V(b), V(off));
    } else {
        vec3_sub(V(from), V(a), V(off));
        vec3_sub(V(to), V(b), V(off));
    }
    segment_collide_v(V(from), V(to), 1, m, static_cast<const uint32_t*>(ee_ram_ptr(hit)));
}

// One of the bolt's ends clipped to the scenery: the segment from the volume's centre `centre` to `end` tested once; on a
// hit the end stops just short of what it meets (true), else it is `end` itself. The result in `out`.
bool clip_end(uint32_t m, uint32_t centre, uint32_t end, uint32_t out, uint32_t gap) {
    if (segment_collide_v(V(centre), V(end), 0, m, nullptr) == 0) {
        lq_sq(out, end);
        return false;
    }
    vec3_sub(V(gap), V(OV(0x00173e60)), V(centre));
    const float length = vec3_length(V(gap));
    vec3_scale(V(gap), V(gap), fpu_div(fpu_mul(length, u2f(0x3f7ae148u)), length));  // (0.98 of the way)
    vec3_add(V(out), V(centre), V(gap));
    return true;
}

// A strip of quads from `from` to `to` (each a point), half a width `side` / `prev_side` across at each end, the corners'
// s coordinate moved on by a quarter for each step past `u_base`.
void quad_corners(uint32_t quad, uint32_t from, uint32_t to, uint32_t prev_side, uint32_t side, uint32_t uvs, int32_t step, int32_t u_base) {
    for (int32_t k = 0; k < 4; ++k) {
        const uint32_t corner = quad + static_cast<uint32_t>(k) * 0x10;
        lq_sq(corner, k < 2 ? from : to);
        const uint32_t across = k < 2 ? prev_side : side;
        if ((k & 1) != 0) vec3_add(V(corner), V(corner), V(across));
        else vec3_sub(V(corner), V(corner), V(across));
        swc1(quad + 0x50 + static_cast<uint32_t>(k) * 8,
             fpu_add(lwc1(uvs + static_cast<uint32_t>(k) * 8), fpu_mul(int_to_float(step - u_base), 0.25f)));
    }
}

// The half width across the strip at `mid` seen from the camera (`eye`, in the volume's frame): 0.1 along
// (mid - eye) x (a - b), `a` / `b` the points either side of it; `along` / `to_eye` are scratch.
void side_of(uint32_t side, uint32_t a, uint32_t b, uint32_t mid, uint32_t eye, uint32_t along, uint32_t to_eye) {
    vec3_sub(V(along), V(a), V(b));
    vec3_sub(V(to_eye), V(mid), V(eye));
    vec3_cross_ba(V(side), V(to_eye), V(along));
    vec3_set_length(V(side), V(side), 0.1f);
}
}  // namespace

// orig: L14:0x003017f8 L15:0x002e6260 L17:0x002e6690
void class1224_bolt(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t tune = tuning();
    const uint32_t sp = ee_stack_alloc(0x560);
    const uint32_t points = sp + 0x10, drifts = sp + 0x210;  // (32 points and their drift, before smoothing)
    const uint32_t step = sp + 0x410, jitter = sp + 0x420, matrix = sp + 0x430;
    swc1(volume(pv) + 0x3c, 1.0f);
    const int32_t lo = frame_scale_i(lw(tune + 4));
    const int32_t frames = rand_range(lo, frame_scale_i(lw(tune + 8)));
    sw(pv + 0x620, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(frames))));
    sh(pv + 0x12, static_cast<uint32_t>(frames));
    const float length = vec3_length(V(volume(pv) + 0x10));
    const float span = fpu_add(length, length);
    // the raw points along the volume's y axis: the three at each end straight, the rest spread and drifting
    for (int32_t i = 0; i < POINTS; ++i) {
        const uint32_t p = points + static_cast<uint32_t>(i) * 0x10, d = drifts + static_cast<uint32_t>(i) * 0x10;
        float along = fpu_div(int_to_float(i), int_to_float(POINTS - 1));
        along = fpu_mul(fpu_sub(0.5f, along), span);
        if (static_cast<uint32_t>(i - 3) < 26u) {
            rand_sphere_point(p, 0.0f, lwc1(tune + 0x44));
            rand_sphere_point(d, 0.0f, fpu_mul(lwc1(tune + 0x48), lwc1(0x0015ed6c)));
        } else {
            vec_clear(V(p));
            vec_clear(V(d));
        }
        swc1(p + 0xc, 1.0f);
        swc1(p + 4, fpu_add(lwc1(p + 4), along));
    }
    // smoothed: each inner point (and drift) the mean of it and its two neighbours; the ends as they are
    for (int32_t i = 0; i < POINTS; ++i) {
        const uint32_t p = points + static_cast<uint32_t>(i) * 0x10, d = drifts + static_cast<uint32_t>(i) * 0x10;
        if (i == 0 || i == POINTS - 1) {
            lq_sq(point(pv, i), p);
            lq_sq(drift(pv, i), d);
            continue;
        }
        vec3_add(V(step), V(p - 0x10), V(p));
        vec3_add(V(step), V(step), V(p + 0x10));
        vec3_scale(V(point(pv, i)), V(step), u2f(0x3eaa7efau));  // (0.333)
        vec3_add(V(step), V(d - 0x10), V(d));
        vec3_add(V(step), V(step), V(d + 0x10));
        vec3_scale(V(drift(pv, i)), V(step), u2f(0x3eaa7efau));
    }
    // the four forks: each leaves the bolt at a random point of its own sixth (4 + 0..23) and goes three steps along a
    // random way, each step jittered and drifting
    for (int32_t fork = 0; fork < FORKS; ++fork) {
        rand_sphere_point(step, lwc1(tune + 0x44), lwc1(tune + 0x44));
        const float at = rand_range_f(fpu_mul(static_cast<float>(fork), 6.0f), fpu_mul(static_cast<float>(fork + 1), 6.0f));
        const int32_t from = float_to_int(at) + 4;
        sb(pv + 0x18 + static_cast<uint32_t>(fork), static_cast<uint32_t>(from));
        lq_sq(fork_point(pv, fork, 0), point(pv, from));
        lq_sq(fork_drift(pv, fork, 0), drift(pv, from));
        for (int32_t k = 0; k < 3; ++k) {
            rand_sphere_point(jitter, 0.1f, 0.1f);
            rand_sphere_point(fork_drift(pv, fork, k + 1), 0.0f, fpu_mul(lwc1(tune + 0x48), lwc1(0x0015ed6c)));
            vec3_add(V(fork_point(pv, fork, k + 1)), V(fork_point(pv, fork, k)), V(step));
            vec3_add(V(fork_point(pv, fork, k + 1)), V(fork_point(pv, fork, k + 1)), V(jitter));
        }
    }
    // the sparks (only with the camera 4 or more away from the volume's centre in the plane): at every eighth point of the
    // bolt and at the first two forks' ends, in the world (the volume's rotation and centre)
    const uint32_t vol = volume(pv);
    if (4.0f <= vec2_distance(V(vol + 0x30), V(OV(0x00166dc0)))) {
        euler_to_mat4_vu(M(matrix), V(volume(pv) + 0x70));
        lq_sq(matrix + 0x30, volume(pv) + 0x30);
        auto spark = [tune, step, jitter](uint32_t n, int32_t texture) {
            const int32_t life = frame_scale_i(rand_mod(10) + 20);
            spawn_particle_02(step, jitter, jitter, lwu(tune + 0x50), lwu(tune + 0x54), n, 0xf, static_cast<uint32_t>(life), texture);
        };
        for (int32_t i = 0; i < POINTS; i += 8) {
            mat4_transform(V(step), V(point(pv, i)), M(matrix));
            lq_sq(jitter, drift(pv, i));
            swc1(jitter + 0xc, lwc1(tune + 0x5c));
            const int32_t texture = rand_mod(2) != 0 ? lw(tune + 0x64) : lw(tune + 0x68);
            spark(static_cast<uint32_t>(i >> 1), texture);
        }
        for (int32_t fork = 0; fork < 2; ++fork) {
            mat4_transform(V(step), V(fork_point(pv, fork, 3)), M(matrix));  // (the fork's last point)
            lq_sq(jitter, fork_drift(pv, fork, 3));
            swc1(jitter + 0xc, lwc1(tune + 0x5c));
            rand_mod(2);  // (drawn and not used)
            spark(lbu(pv + 0x18 + static_cast<uint32_t>(fork)) >> 1, lw(tune + 0x68));
        }
    }
    ee_stack_free(0x560);
}

// orig: L14:0x00302538 L15:0x002e6fa0 L17:0x002e73d0
void class1224_strike(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lwc1(pv + 0x1c) == 0.0f) return;
    const uint32_t sp = ee_stack_alloc(0x150);
    // the hit record the sweeps carry (+0 the push's way, +0xc the damage's word, +0x10 the moby, +0x14 the kinds, +0x18 /
    // +0x19 / +0x1a its type, flag and class, +0x1c / +0x20)
    const uint32_t hit = sp;
    const uint32_t facing = sp + 0x30, to_hero = sp + 0x40, centre = sp + 0x50, end_a = sp + 0x60, end_b = sp + 0x70;
    const uint32_t from = sp + 0x80, to = sp + 0x90, off = sp + 0xa0;
    swc1(hit + 0x1c, 1.0f);
    sw(hit + 0x20, 1);
    sw(hit + 0x10, m);
    sw(hit + 0x14, 0x10001);
    vec_clear(V(hit));
    swc1(hit + 8, 1.0f);
    sb(hit + 0x19, 1);
    swc1(hit + 0xc, u2f(0x45afdf66u));  // (5627.92)
    sb(hit + 0x18, 3);
    sh(hit + 0x1a, lhu(m + 0xa6));
    // the push: the moby's facing in the plane, turned towards the hero
    swc1(facing, cos_vu(lwc1(m + 0x48)));
    swc1(facing + 4, sin_vu(lwc1(m + 0x48)));
    swc1(facing + 8, 0.0f);
    vec3_sub(V(to_hero), V(0x0013f3d0), V(m + 0x10));
    swc1(to_hero + 8, 0.0f);
    const float ahead = vec3_dot(V(facing), V(to_hero));
    if (vec3_length(V(to_hero)) != 0.0f) {
        if (ahead < 0.0f) vec3_scale(V(facing), V(facing), -1.0f);
        swc1(hit, lwc1(facing));
        swc1(hit + 4, lwc1(facing + 4));
    }
    // the two ends, clipped once each (pvars +8 / +0xa set by a new bolt)
    lq_sq(centre, volume(pv) + 0x30);
    if (lh(pv + 8) != 0) {
        vec3_add(V(end_a), V(centre), V(volume(pv) + 0x10));
        if (clip_end(m, centre, end_a, pv + 0x630, from)) sh(pv + 8, 0);
    }
    if (lh(pv + 0xa) != 0) {
        vec3_sub(V(end_b), V(centre), V(volume(pv) + 0x10));
        if (clip_end(m, centre, end_b, pv + 0x640, from)) sh(pv + 0xa, 0);
    }
    lq_sq(end_a, pv + 0x630);
    lq_sq(end_b, pv + 0x640);
    // the sweeps: the bolt moved out and back along the volume's x and z axes by the reach
    vec3_set_length(V(off), V(volume(pv)), lwc1(pv + 0x1c));
    sweep(m, end_a, end_b, off, from, to, hit, true);
    sweep(m, end_a, end_b, off, from, to, hit, false);
    vec3_set_length(V(off), V(volume(pv) + 0x20), lwc1(pv + 0x1c));
    sweep(m, end_a, end_b, off, from, to, hit, true);
    sweep(m, end_a, end_b, off, from, to, hit, false);
    ee_stack_free(0x150);
}

// orig: L14:0x00301de8 L15:0x002e6850 L17:0x002e6c80
void class1224_fade(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t tune = tuning();
    if (lh(pv + 0x12) != 0) {
        sh(pv + 0x12, lhu(pv + 0x12) - 1u);
        for (int32_t i = 4; i < 27; ++i) vec3_add(V(point(pv, i)), V(point(pv, i)), V(drift(pv, i)));
        for (int32_t fork = 0; fork < FORKS; ++fork)
            for (int32_t k = 1; k < 4; ++k) vec3_add(V(fork_point(pv, fork, k)), V(fork_point(pv, fork, k)), V(fork_drift(pv, fork, k)));
        for (int32_t fork = 0; fork < FORKS; ++fork) lq_sq(fork_point(pv, fork, 0), point(pv, lbu(pv + 0x18 + static_cast<uint32_t>(fork))));
    }
    // the colour: from the first to the second between the start and the first mark, then to the third until the second
    const int32_t left = lh(pv + 0x12);
    const int32_t mark1 = lw(tune + 0xc), mark2 = lw(tune + 0x10);
    if (mark1 < left) {
        const float t = fpu_div(int_to_float(left - mark1), int_to_float(lw(pv + 0x620) - lw(tune + 0xc)));
        sw(pv + 0x14, rgba_lerp(lwu(tune + 0x3c), lwu(tune + 0x38), t));
    } else if (mark2 < left) {
        const float t = fpu_div(int_to_float(left - mark2), int_to_float(lw(tune + 0xc) - lw(tune + 0x10)));
        sw(pv + 0x14, rgba_lerp(lwu(tune + 0x40), lwu(tune + 0x3c), t));
    } else {
        sw(pv + 0x14, 0);
    }
}

// The bolt as 31 quads from point to point and each fork as three, in the volume's frame (its rotation and centre), each
// quad turned to face the camera, its corners' t from the level's table and s moved on a quarter a step (from each fourth
// point), all in the bolt's colour.
// orig: L14:0x00301fa8 L15:0x002e6a10 L17:0x002e6e40
void class1224_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t tune = tuning();
    const uint32_t sp = ee_stack_alloc(0x370);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70;
    const uint32_t matrix = sp + 0x1b0, inverse = sp + 0x1f0, eye = sp + 0x230;
    const uint32_t prev_side = sp + 0x240, side = sp + 0x250, to_eye = sp + 0x260, along = sp + 0x270;
    euler_to_mat4_vu(M(matrix), V(volume(pv) + 0x70));
    mat3_transpose(M(inverse), M(matrix));
    lq_sq(matrix + 0x30, volume(pv) + 0x30);
    vec3_sub(V(eye), V(OV(0x00166dc0)), V(volume(pv) + 0x30));
    mat3_transform(V(eye), V(eye), M(inverse));
    // the quad's GS words (as class 1135's ring: the texture's TEX0, the CLAMP-like word, the blend from four selectors,
    // loaded sign-extended and shifted as doublewords)
    const uint64_t tex0 = effect_tex0(lw(tune + 0x24));
    auto word = [tune](uint32_t o) { return static_cast<uint64_t>(static_cast<int64_t>(lw(tune + o))); };
    const uint64_t blend = word(0x28) | word(0x2c) << 2 | word(0x30) << 4 | word(0x34) << 6 | 0x8000000000ull;
    sd(regs, 0);
    sd(regs + 8, tex0);
    sd(regs + 0x10, 0x0000ff9000000260ull);
    sd(regs + 0x18, blend);
    // (the original fills two more descriptors after this one the same way and never draws them: left out)
    const uint32_t uv = corner_uvs();
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(uvs + k * 8 + 4, lwc1(uv + k * 8 + 4));
        sw(colours + k * 4, lwu(pv + 0x14));
    }
    vec_clear(V(side));
    for (int32_t i = 0; i < POINTS - 1; ++i) {
        lq_sq(prev_side, side);
        if (i < POINTS - 2) side_of(side, point(pv, i), point(pv, i + 2), point(pv, i + 1), eye, along, to_eye);
        else vec_clear(V(side));
        quad_corners(quad, point(pv, i), point(pv, i + 1), prev_side, side, uv, i, (i >> 2) << 2);
        quad_draw(quad, matrix);
    }
    for (int32_t fork = 0; fork < FORKS; ++fork) {
        vec_clear(V(side));
        if (lbu(pv + 0x18 + static_cast<uint32_t>(fork)) >= POINTS - 1) continue;
        for (int32_t k = 0; k < 3; ++k) {
            lq_sq(prev_side, side);
            if (k < 2) side_of(side, fork_point(pv, fork, k), fork_point(pv, fork, k + 2), fork_point(pv, fork, k + 1), eye, along, to_eye);
            else vec_clear(V(side));
            if (k == 0) lq_sq(prev_side, side);
            quad_corners(quad, fork_point(pv, fork, k), fork_point(pv, fork, k + 1), prev_side, side, uv, k, 0);
            quad_draw(quad, matrix);
        }
    }
    ee_stack_free(0x370);
}

// The bolt's cycle: 0 a first bolt, 1 waiting (a bolt each time the last one ends while the level's count lasts), 2 the
// timer to the next bolt, 3 a bolt and the timer again (the quick interval, then state 2, when there is one). Culled
// (out of state 0) by its volume's length about its place; nothing while the level's class switch is on.
// orig: L14:0x003015d0 L15:0x002e6038 L17:0x002e6468
void class1224_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t tune = tuning();
    if (lbu(m + 0x20) != 0) {
        const uint32_t sp = ee_stack_alloc(0x50);
        lq_sq(sp, m + 0x10);
        swc1(sp + 0xc, lwc1(pv + 0x624));
        const bool off_screen = point_on_screen(sp, 52.0f) < 0;
        ee_stack_free(0x50);
        if (off_screen) return;
    }
    if (lw(tune + 0x14) != 0) return;
    switch (lbu(m + 0x20)) {
    case 0:
        class1224_bolt(m);
        sh(pv + 0x10, static_cast<uint32_t>(frame_scale_i(lw(pv + 0xc))));
        sb(m + 0x20, 2);
        swc1(pv + 0x624, vec3_length(V(volume(pv) + 0x10)));
        sh(pv + 0xa, 1);
        sh(pv + 8, 1);
        break;
    case 1: {
        const int32_t count = lw(tune);
        if (count == 0 || lh(pv + 0x12) != 0) break;
        sw(tune, static_cast<uint32_t>(count - 1));
        class1224_bolt(m);
        break;
    }
    case 2:
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x10))) != 0) {
            sb(m + 0x20, 3);
            sh(pv + 0x10, static_cast<uint32_t>(frame_scale_i(lh(pv + 4))));
            class1224_bolt(m);
        }
        break;
    case 3:
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x10))) != 0) {
            int32_t frames = lh(pv + 4);
            if (lh(pv + 6) != 0) {
                sb(m + 0x20, 2);
                frames = lh(pv + 6);
            }
            sh(pv + 0x10, static_cast<uint32_t>(frame_scale_i(frames)));
        } else if (lh(pv + 0x12) == 0) {
            class1224_bolt(m);
        }
        break;
    default:
        break;
    }
    if (lw(tune + 0x20) == 0 && vec2_distance(V(m + 0x10), V(0x0013f3d0)) < 10.0f) class1224_strike(m);
    if (lh(pv + 0x12) == 0) return;
    if (lw(tune + 0x18) == 0) class1224_fade(m);
    if (!(lw(tune + 0x10) < lh(pv + 0x12))) return;
    if (lw(tune + 0x1c) != 0) return;
    draw_callback_add(draw_callback(), m);
}

}  // namespace rac1
