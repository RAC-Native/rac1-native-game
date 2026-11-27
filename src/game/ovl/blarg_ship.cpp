// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1109, Ratchet's ship at the station. Standing on it,
// Ratchet is offered a flight (a HUD message) between the station's three landing sites; a flight is a scene (the
// screen fades, the camera follows a path and looks along another while the ship follows a third, its size changing on
// the way, Ratchet put aside). One route blasts through the station on the way (explosions at boxes along the camera's
// path, then a group of pieces flung apart with a flash) and lands by an infobot: Ratchet walking up to it plays its
// cutscene and movie (the coordinates of level 5) and opens the planet map. With its explosions, the blast and the
// pieces' launch. Header blarg_own.h; registered in blarg_mobys.cpp. Addresses are level 6's (`orig: L6:`); its own data
// is L6_ADDR (the ship's tuning and the particles' at 0x1620f4 .. 0x162194), the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/level/level_read.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/hydro.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 his position, +0x2fc the moby he stands on)
constexpr uint32_t HERO_POS = PLAYER + 0x80;
const float kPi = u2f(0x40490fdbu);
const float kTwoPi = u2f(0x40c90fdbu);

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the moby of the level's moby table at index `i`
uint32_t table_moby(uint32_t i) { return lwu(L6_ADDR(0x0015ffd8)) + (i << 8); }

// the point `i` of the path at `path` (its points from +0x10, 16 bytes each; their w lanes carry the flight's events)
uint32_t path_point(uint32_t path, int32_t i) { return path + 0x10 + (static_cast<uint32_t>(i) << 4); }

// the index of the path at `path`'s last point, as a float
float path_last(uint32_t path) { return static_cast<float>(static_cast<int32_t>(lvd::path_u32(path) - 1u)); }

// the level's checkpoint whose index the pvar `word` holds taken (its byte of this level's row of 0x14c050 is 0xff)
bool checkpoint_taken(uint32_t word) {
    return lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lwu(word)) == 0xff;
}

// the ship put at the box whose index the pvar `word` holds (its centre and rotation)
void ship_to_box(uint32_t m, uint32_t word) {
    const uint32_t b = lvd::box(lvd::BOXES, lw(word));
    lq_sq(m + 0x10, b + 0x30);
    lq_sq(m + 0x40, b + 0x70);
}
}  // namespace

// The flight offered to Ratchet standing on the ship (+0x9c): from site 1 the choice 1, from site 2 the choice 4, from
// site 0 the choice 0 until the checkpoint +0xf8 is taken, then 3 once the checkpoint +0xfc is too, else -1 (none). While
// one of the flags 0x15ee1c / 0x15ee1d is set the choice 0 is replaced by the help voice 0x177a (its record 0x53 at
// 0x141c00 never shown: the choice then -1); otherwise the choice's HUD message (owner 10).
// orig: L6:0x00302fd8
void ship_offer_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t site = lw(pv + 0xbc);
    if (site == 1) {
        sw(pv + 0x9c, 1);
    } else if (site == 2) {
        sw(pv + 0x9c, 4);
    } else if (site == 0) {
        if (!checkpoint_taken(pv + 0xf8)) sw(pv + 0x9c, 0);
        else sw(pv + 0x9c, checkpoint_taken(pv + 0xfc) ? 3u : 0xffffffffu);
    }
    const int32_t choice = lw(pv + 0x9c);
    if (lhu(0x00141c00) == 0 && choice == 0 && (lbu(0x0015ee1d) != 0 || lbu(0x0015ee1c) != 0)) {
        sub_00203908(0x177a, 0x53);
        sw(pv + 0x9c, 0xffffffffu);
        return;
    }
    switch (choice) {  // (the table 0x201930, choices 0 .. 4: 1 and 4 share a case; its words past 4 are data)
    case 0: hud_message_request(0xa, TXT(0x1782)); break;
    case 1:
    case 4: hud_message_request(0xa, TXT(0x1785)); break;
    case 2: hud_message_request(0xa, TXT(0x1786)); break;
    case 3: hud_message_request(0xa, TXT(0x1784)); break;
    default: break;
    }
}

// An explosion of the moby `m` at `pos`: the burst (size 2, 13, no sound), `count` flames (particle 0x0b: a random
// colour of each of the two tables of six at 0x201948 / 0x201960, lives of 15 .. 20 and 25 .. 30 frames, size 400000
// times `scale`, moving along `dir` at 10 a second times `scale`), and the level's count 0x162150 times `count` sparks
// within 1 of `pos` (two velocities toward `dir` within 0.9, at the speeds 0x162114 .. 0x162118 and 0x162124 ..
// 0x162128 a second, their sizes 0x16211c .. 0x162120 and 0x16212c .. 0x162130 times `scale`, colours between
// 0x162134 / 0x162138 and 0x16213c / 0x162140, frame counts from 0x162144, 0x162148, 0x16214c; texture 0x162154 + the
// half 0x162158).
// orig: L6:0x00303748
void ship_explosion_l6(uint32_t m, uint32_t pos, uint32_t dir, int32_t count, float scale) {
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t at = sp + 0x10, toward = sp + 0x20, vel = sp + 0x30, vel2 = sp + 0x40;
    const uint32_t colours_a = sp + 0x50, colours_b = sp + 0x70;
    explosion_burst(m, pos, -1, 2.0f, 13.0f);
    for (uint32_t k = 0; k < 0x18; k += 8) {  // (the two colour tables copied to the frame, as the original does)
        sd(colours_a + k, ld(L6_ADDR(0x00201948) + k));
        sd(colours_b + k, ld(L6_ADDR(0x00201960) + k));
    }
    for (int32_t left = count; left > 0; --left) {
        const float size = fpu_mul(scale, 400000.0f);
        const uint32_t colour_a = lwu(colours_a + (static_cast<uint32_t>(rand_mod(6)) << 2));
        const uint32_t colour_b = lwu(colours_b + (static_cast<uint32_t>(rand_mod(6)) << 2));
        const int32_t life_a = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
        const int32_t life_b = rand_range(frame_scale_i(0x19), frame_scale_i(0x1e));
        spawn_particle_0b(pos, dir, colour_a, colour_b, static_cast<uint32_t>(life_a), static_cast<uint32_t>(life_b), 0, 0,
                          size, fpu_mul(fpu_mul(dt(), 10.0f), scale));
    }
    for (int32_t i = 0; i < static_cast<int32_t>(lwu(L6_ADDR(0x00162150)) * static_cast<uint32_t>(count)); ++i) {
        rand_sphere_point(at, 0.0f, 1.0f);
        vec3_add(V(at), V(at), V(pos));
        rand_sphere_point(toward, 0.0f, 0.9f);
        vec3_add(V(toward), V(toward), V(dir));
        vec3_scale(V(vel), V(toward), fpu_mul(rand_range_f(lwc1(L6_ADDR(0x00162114)), lwc1(L6_ADDR(0x00162118))), dt()));
        rand_sphere_point(toward, 0.0f, 0.9f);
        vec3_add(V(toward), V(toward), V(dir));
        vec3_scale(V(vel2), V(toward), fpu_mul(rand_range_f(lwc1(L6_ADDR(0x00162124)), lwc1(L6_ADDR(0x00162128))), dt()));
        swc1(vel + 0xc, fpu_mul(rand_range_f(lwc1(L6_ADDR(0x0016211c)), lwc1(L6_ADDR(0x00162120))), scale));
        swc1(vel2 + 0xc, fpu_mul(rand_range_f(lwc1(L6_ADDR(0x0016212c)), lwc1(L6_ADDR(0x00162130))), scale));
        const uint32_t c0 = rgba_lerp(lwu(L6_ADDR(0x00162134)), lwu(L6_ADDR(0x00162138)), rand_range_f(0.0f, 1.0f));
        const uint32_t c1 = rgba_lerp(lwu(L6_ADDR(0x0016213c)), lwu(L6_ADDR(0x00162140)), rand_range_f(0.0f, 1.0f));
        const uint32_t n = lwu(L6_ADDR(0x00162144));  // (frames n .. 2n)
        const int32_t life = float_to_int(frame_scale_f(
            rand_range_f(static_cast<float>(static_cast<int32_t>(n)), static_cast<float>(static_cast<int32_t>(n << 1)))));
        const float g = static_cast<float>(lw(L6_ADDR(0x00162148)));  // (g .. 1.2 g)
        const int32_t fade = float_to_int(frame_scale_f(rand_range_f(g, fpu_mul(g, 1.2f))));
        const float h = static_cast<float>(lw(L6_ADDR(0x0016214c)));  // (0.8 h .. h)
        const int32_t grow = float_to_int(frame_scale_f(rand_range_f(fpu_mul(h, 0.8f), h)));
        const int32_t tex = static_cast<int32_t>(lwu(L6_ADDR(0x00162154)) + (lwu(L6_ADDR(0x00162158)) << 16));
        spawn_particle_02(at, vel, vel2, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(fade),
                          static_cast<uint32_t>(grow), tex);
    }
    ee_stack_free(0x160);
}

// A piece of the blasted group launched: its state 2, its velocity (pvars +0) and its spin (pvars +0x10).
// orig: L6:0x00300f70
void ship_piece_launch_l6(uint32_t piece, uint32_t vel, uint32_t spin) {
    const uint32_t pv = lwu(piece + 0x78);
    sb(piece + 0x20, 2);
    lq_sq(pv, vel);
    lq_sq(pv + 0x10, spin);
}

// The blast of the ship's flight through the station: a flash at (426, 189.75, 173.07) (the level's 0x162108 frames,
// colours 0x16210c / 0x162110); every piece of the group +0xcc an explosion (scale 30, one flame, toward a random point
// 1 .. 2 away), then flung out from the pieces' centre at 30 a second (less for the bigger ones, by their size +0xc
// against the biggest), spinning by up to 15 / 5 degrees a second, with the level's count 0x162190 smoke puffs each
// (particle 0x02: within its size / 2048, speed 0x16215c, sizes 0x162170 .. 0x162174 / 0x162178 .. 0x16217c, colours
// 0x162180 .. 0x16218c, frame counts 0x162160 .. 0x162168 spread by 0x16216c, texture 0x162194); the last piece's +0xbc 30.
// orig: L6:0x00303b68
void ship_blast_group_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t flash = sp + 0x10, centre = sp + 0x20, out = sp + 0x30, spin = sp + 0x40;
    const uint32_t at = sp + 0x50, vel = sp + 0x60, drift = sp + 0x70;
    swc1(flash, 426.0f);
    swc1(flash + 4, 189.75f);
    swc1(flash + 8, 173.07f);
    sw(flash + 0xc, 0);
    flash_spawn(flash, frame_scale_i(lw(L6_ADDR(0x00162108))), lwu(L6_ADDR(0x0016210c)), lwu(L6_ADDR(0x00162110)));
    vec_clear(V(centre));
    const uint32_t list = lvd::group(lw(pv + 0xcc));
    float biggest = 0.0f;
    int32_t count = 0;
    for (uint32_t e = list;; e += 2) {
        const uint32_t piece = table_moby(lhu(e) & 0x7fffu);
        rand_sphere_point(out, 1.0f, 2.0f);
        ship_explosion_l6(m, piece + 0x10, out, 1, 30.0f);
        vec3_add(V(centre), V(centre), V(piece + 0x10));
        ++count;
        if (biggest < lwc1(piece + 0xc)) biggest = lwc1(piece + 0xc);
        if (lh(e) < 0) break;
    }
    vec3_scale(V(centre), V(centre), fpu_div(1.0f, int_to_float(count)));
    uint32_t last = 0;
    for (uint32_t e = list;; e += 2) {
        last = lhu(e) & 0x7fffu;
        const uint32_t piece = table_moby(last);
        vec3_sub(V(out), V(piece + 0x10), V(centre));
        const float speed = fpu_mul(dt(), 30.0f);
        const float bigger = -fpu_div(fpu_sub(lwc1(piece + 0xc), biggest), biggest);
        vec3_set_length(V(out), V(out), fpu_add(fpu_mul(bigger, speed), speed));
        swc1(out + 0xc, 6.0f);
        rand_sphere_point(spin, fpu_mul(dt(), u2f(0x3e860a92u)), fpu_mul(dt(), u2f(0x3db2b8c2u)));  // (pi / 12, pi / 36)
        ship_piece_launch_l6(piece, out, spin);
        for (int32_t k = 0; k < lw(L6_ADDR(0x00162190)); ++k) {
            rand_sphere_point(at, 0.0f, fpu_mul(lwc1(piece + 0xc), u2f(0x3a000000u)));  // (1 / 2048)
            vec3_add(V(at), V(at), V(piece + 0x10));
            rand_sphere_point(vel, 0.0f, fpu_mul(lwc1(L6_ADDR(0x0016215c)), dt()));
            vec_clear(V(drift));
            swc1(vel + 0xc, rand_range_f(lwc1(L6_ADDR(0x00162170)), lwc1(L6_ADDR(0x00162174))));
            swc1(drift + 0xc, rand_range_f(lwc1(L6_ADDR(0x00162178)), lwc1(L6_ADDR(0x0016217c))));
            const uint32_t c0 = rgba_lerp(lwu(L6_ADDR(0x00162180)), lwu(L6_ADDR(0x00162184)), rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(lwu(L6_ADDR(0x00162188)), lwu(L6_ADDR(0x0016218c)), rand_range_f(0.0f, 1.0f));
            const float r = rand_range_f(0.0f, 1.0f);
            const int32_t life =
                float_to_int(frame_scale_f(fpu_add(fpu_mul(static_cast<float>(lw(L6_ADDR(0x00162160))), r), 1.0f)));
            const float r2 = fpu_add(rand_range_f(-lwc1(L6_ADDR(0x0016216c)), lwc1(L6_ADDR(0x0016216c))), 1.0f);
            const int32_t fade = float_to_int(frame_scale_f(fpu_mul(static_cast<float>(lw(L6_ADDR(0x00162164))), r2)));
            const float r3 = fpu_add(rand_range_f(-lwc1(L6_ADDR(0x0016216c)), lwc1(L6_ADDR(0x0016216c))), 1.0f);
            const int32_t grow = float_to_int(frame_scale_f(fpu_mul(static_cast<float>(lw(L6_ADDR(0x00162168))), r3)));
            const uint32_t p = spawn_particle_02(at, vel, drift, c0, c1, static_cast<uint32_t>(life),
                                                 static_cast<uint32_t>(fade), static_cast<uint32_t>(grow),
                                                 lw(L6_ADDR(0x00162194)));
            if (p != 0) sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) - 0x20));
        }
        if (lh(e) < 0) break;
    }
    sb(table_moby(last) + 0xbc, 0x1e);
    ee_stack_free(0x160);
}

// The ship's flight on the route `route` (its paths: the camera's +0x60, where the camera looks +0x74, the ship's +0x88,
// one word a route): its progress +0xf4 toward 1 over 1200 frames; the camera and the ship along their paths, the ship
// facing along its path (pitch and yaw blended between the segments), turned by its roll +0xb0, which seeks a half turn
// over the last fifth of the flight; the camera's angles toward the look path from its second point on; the ship's size
// from +0xc4 to +0xc8. A camera path point carrying an event (its w, a whole number) fires as the camera reaches it (the
// point and the one before it set to -1): 0 .. 7 the level's sound 0 and an explosion at the box +0xd0 + 4 * event,
// 10 the sound 1 and the blast. True once the flight is over.
// orig: L6:0x00303160
bool ship_flight_l6(uint32_t m, int32_t route) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t angles = sp, cam = sp + 0x10;
    const uint32_t r4 = static_cast<uint32_t>(route) << 2;
    const uint32_t cam_path = lvd::path(lw(pv + 0x60 + r4));
    const uint32_t look_path = lvd::path(lw(pv + 0x74 + r4));
    const uint32_t ship_path = lvd::path(lw(pv + 0x88 + r4));
    approach_f(pv + 0xf4, 1.0f, fpu_div(1.0f, static_cast<float>(frame_scale_i(0x4b0))));
    const float t = lwc1(pv + 0xf4);
    const float cam_at = fpu_mul(t, path_last(cam_path));
    const int32_t ci = float_to_int(cam_at);
    const float cam_frac = fpu_sub(cam_at, static_cast<float>(ci));
    const int32_t li = float_to_int(fpu_mul(t, path_last(look_path)));
    const float ship_at = fpu_mul(t, path_last(ship_path));
    const float frac = fpu_sub(ship_at, static_cast<float>(ci));  // (the original subtracts the camera's index here)
    const int32_t si = float_to_int(ship_at);
    vec3_lerp(V(cam), V(path_point(cam_path, ci)), V(path_point(cam_path, ci + 1)), cam_frac);
    vec3_lerp(V(m + 0x10), V(path_point(ship_path, si)), V(path_point(ship_path, si + 1)), frac);
    uid5_set_pvar_80(cam);
    if (si > 0) {
        const uint32_t prev = path_point(ship_path, si - 1), here = path_point(ship_path, si);
        const uint32_t next = path_point(ship_path, si + 1);
        const float yaw_in = atan2_f(fpu_sub(lwc1(here), lwc1(prev)), fpu_sub(lwc1(here + 4), lwc1(prev + 4)));
        const float yaw_out = atan2_f(fpu_sub(lwc1(next), lwc1(here)), fpu_sub(lwc1(next + 4), lwc1(here + 4)));
        swc1(m + 0x48, angle_add(fpu_mul(angle_sub(yaw_out, yaw_in), frac), yaw_in));
        const float pitch_in = -atan2_f(vec2_distance(V(prev), V(here)), fpu_sub(lwc1(here + 8), lwc1(prev + 8)));
        const float pitch_out = -atan2_f(vec2_distance(V(here), V(next)), fpu_sub(lwc1(next + 8), lwc1(here + 8)));
        swc1(m + 0x44, angle_add(fpu_mul(angle_sub(pitch_out, pitch_in), frac), pitch_in));
        const float roll_cos = cos_vu(lwc1(pv + 0xb0));
        swc1(m + 0x44, fpu_mul(lwc1(m + 0x44), roll_cos));
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0xb0)));
    } else {
        const uint32_t p0 = path_point(ship_path, 0), p1 = path_point(ship_path, 1);
        swc1(m + 0x44, -atan2_f(vec2_distance(V(p0), V(p1)), fpu_sub(lwc1(p1 + 8), lwc1(p0 + 8))));
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(p1), lwc1(p0)), fpu_sub(lwc1(p1 + 4), lwc1(p0 + 4))));
    }
    if (ci > 0 && li > 0) {
        vec_clear(V(angles));
        const uint32_t a = path_point(look_path, li), b = path_point(look_path, li + 1);
        const float yaw_a = atan2_f(fpu_sub(lwc1(a), lwc1(cam)), fpu_sub(lwc1(a + 4), lwc1(cam + 4)));
        const float yaw_b = atan2_f(fpu_sub(lwc1(b), lwc1(cam)), fpu_sub(lwc1(b + 4), lwc1(cam + 4)));
        swc1(angles + 8, angle_add(fpu_mul(angle_sub(yaw_b, yaw_a), frac), yaw_a));
        const float pitch_a = -atan2_f(vec2_distance(V(cam), V(a)), fpu_sub(lwc1(a + 8), lwc1(cam + 8)));
        const float pitch_b = -atan2_f(vec2_distance(V(cam), V(b)), fpu_sub(lwc1(b + 8), lwc1(cam + 8)));
        swc1(angles + 4, angle_add(fpu_mul(angle_sub(pitch_b, pitch_a), frac), pitch_a));
        uid5_set_pvar_90(angles);
    }
    const float from = lwc1(pv + 0xc4);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24),
                           fpu_sub(from, fpu_mul(fpu_sub(from, lwc1(pv + 0xc8)), lwc1(pv + 0xf4)))));
    if (0.8f < lwc1(pv + 0xf4)) {
        const float accel = fpu_mul(dt_sq(), u2f(0x41490fdbu));  // (4 pi)
        yaw_seek(pv + 0xb0, pv + 0xb4, kPi, accel, accel, fpu_mul(dt(), kTwoPi));
    }
    // the camera path's events (the w of the point reached and of the one before it; with the point 0, the word the
    // original writes is the path's header word +0xc)
    const uint32_t here_w = path_point(cam_path, ci) + 0xc;
    const uint32_t prev_w = path_point(cam_path, ci - 1) + 0xc;
    const int32_t event = float_to_int(lwc1(here_w));
    const int32_t prev_event = ci == 0 ? -1 : float_to_int(lwc1(prev_w));
    if (event >= 0 || prev_event >= 0) {
        lvd::set_path_f32(here_w, -1.0f);
        lvd::set_path_f32(prev_w, -1.0f);
        if (event < 8 && prev_event < 8) {
            level_sound_play_l6(0, 0, m);
            const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0xd0 + (static_cast<uint32_t>(event) << 2)));
            ship_explosion_l6(m, b + 0x30, b, 6, 6.0f);
        } else if (event == 10 || prev_event == 10) {
            level_sound_play_l6(1, 0, m);
            ship_blast_group_l6(m);
        }
    }
    const bool done = lwc1(pv + 0xf4) == 1.0f;
    ee_stack_free(0xf0);
    return done;
}

// The blasted group +0xcc's pieces retired (the blast done before: the checkpoint +0xf8 taken).
// orig: L6:0x00304028
void ship_pieces_retire_l6(uint32_t m) {
    const uint32_t list = lvd::group(lw(lwu(m + 0x78) + 0xcc));
    for (uint32_t e = list;; e += 2) {
        moby_retire(table_moby(lhu(e) & 0x7fffu));
        if (lh(e) < 0) break;
    }
}

// Class 1109, Ratchet's ship. pvars: its attach frame +0x20 (what stands on it moves with it), the routes' paths +0x60
// (the camera's), +0x74 (where it looks), +0x88 (the ship's), five routes; the choice offered +0x9c (0 .. 4, -1 none); the
// landing sites' boxes +0xa0 / +0xa4 / +0xa8 (sites 0 / 1 / 2); the box Ratchet waits at during a flight +0xac; its roll
// +0xb0 and the roll's rate +0xb4; its site +0xbc; the route +0xc0; its size at the flight's start +0xc4 and end +0xc8
// (the level's 0x1620f4 / 0x1620f8); the blasted group +0xcc; the explosions' boxes +0xd0 .. +0xec; the box that starts
// the blast route +0xf0; the flight's progress +0xf4; the checkpoints +0xf8 (the blast's) / +0xfc; the boxes that move
// it to sites 2 / 0 / 1 when Ratchet enters them +0x100 / +0x104 / +0x108; the infobot (a moby index) +0x10c; a flight
// asked +0x110; the blast route's landing +0x114. The level's word 0x162104 asks a flash at the hero's +0xd0, 0x162100 a
// flight on the blast route; 0x15fdf8 is the planet map's landing site (1: the site 1, first used marks 0x13d3a9). Every
// frame its size the class's times 0x1620f8 and its attach frame. States (the table 0x2018e0, 0 .. 9): 0 set up (the
// group retired if the checkpoint +0xf8 was taken, else 0x13d3ac cleared; the hero's look +0x38, the animation 2, site
// 0, route 0); 1 parked: on it (not on a voice line with 0x15ee1c / 0x15ee1d), or with 0x162100, the offer, and the
// button (0x13cae4 bit 4, the HUD owner 0x15f594 10, a choice) or 0x162100 picks the route (0: to site 1, the music 4;
// 1: to site 0, the music 0; 2: the blast route; 3: to site 2; 4: from site 2 to site 0); off it, the boxes +0x100 ..
// +0x108 move it to another site; a flight asked starts it (3: the fade, the scripted camera, Ratchet at the box +0xac,
// its sound 2); 2 waits for Ratchet in the box +0xf0 (the blast route); 3 flies, then lands at its site's box (Ratchet put
// behind it, the camera back; after the blast route 0x13d3ac set, the infobot sparkling, 6; else 4); 4 back to 1 (the
// animation 2); 5 nothing; 6 waits for Ratchet within 3.5 of the infobot (its cutscene 0xc, the music 0, level 5 marked
// visited, the checkpoint +0xf8 taken and saved: 8); 8 after the cutscene (0x15f5c4 not 2) its movie 5 (7); 7 then the
// cutscene 5 when 0x13d4de is set (9), else the arrival message of level 5 and the planet map (1); 9 the same after it.
// orig: L6:0x00302578
void blg_class1109_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t old_pos = sp, old_rot = sp + 0x10, moved = sp + 0x20;
    const uint32_t spot = sp + 0x30, angles = sp + 0x40, from = sp + 0x50;
    lq_sq(old_pos, m + 0x10);
    lq_sq(old_rot, m + 0x40);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L6_ADDR(0x001620f8))));
    if (lw(L6_ADDR(0x00162104)) != 0) {  // (a flash asked for)
        sw(L6_ADDR(0x00162104), 0);
        lq_sq(spot, PLAYER + 0xd0);
        flash_spawn(spot, frame_scale_i(lw(L6_ADDR(0x00162108))), lwu(L6_ADDR(0x0016210c)), lwu(L6_ADDR(0x00162110)));
    }
    // a flight starts: the fade, the scripted camera at the route's first camera point looking at the look path's first,
    // Ratchet waiting at the box +0xac, the ship's sound 2
    auto launch = [&] {
        sw(pv + 0x110, 0);
        sb(m + 0x20, 3);
        hud_message_release(0xa);
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 0);
        screen_fade(frame_scale_i(0xa));
        sw(pv + 0xb0, 0);
        const uint32_t r4 = static_cast<uint32_t>(lw(pv + 0xc0)) << 2;
        lq_sq(from, path_point(lvd::path(lw(pv + 0x60 + r4)), 0));
        lq_sq(spot, path_point(lvd::path(lw(pv + 0x74 + r4)), 0));
        vec_clear(V(angles));
        swc1(angles + 8, atan2_f(fpu_sub(lwc1(spot), lwc1(from)), fpu_sub(lwc1(spot + 4), lwc1(from + 4))));
        swc1(angles + 4, -atan2_f(vec2_distance(V(from), V(spot)), fpu_sub(lwc1(spot + 8), lwc1(from + 8))));
        camera_scripted_set(from, angles, 2, static_cast<uint32_t>(frame_scale_i(0x12c)), 0);
        camera_angles_set(0.001f, 1.0f, 1.0f, 0.001f, 1.0f, 1.0f);
        uid5_set_pvar_80(from);
        uid5_set_pvar_90(angles);
        const uint32_t b = lvd::box(lvd::BOXES, lw(pv + 0xac));
        hero_place(b + 0x30, b + 0x70, 0x72, 0);
        moby_sound_start(2, 0, m);
    };
    switch (lbu(m + 0x20)) {  // (the table 0x2018e0: one case a state, 0 .. 9; 5 nothing)
    case 0: {
        sw(L6_ADDR(0x0015fdf8), 0);
        if (checkpoint_taken(pv + 0xf8)) ship_pieces_retire_l6(m);
        else sb(0x0013d3ac, 0);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0x80);
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, 1);
        sd(m + 0x38, ld(lwu(0x001413d0) + 0x38));  // (the hero moby's look)
        if (set != 2) moby_anim_set(m, 2, 0, 0);
        sw(pv + 0xbc, 0);
        sw(pv + 0xc0, 0);
        break;
    }
    case 1: {
        bool busy = false;  // (a voice line playing while 0x15ee1c / 0x15ee1d is set)
        if (lbu(0x0015ee1d) != 0 || lbu(0x0015ee1c) != 0)
            busy = lw(L6_ADDR(0x00179b90)) != 0 || lw(L6_ADDR(0x00179bb4)) != -1;
        const bool on_it = !busy && lwu(PLAYER + 0x2fc) == m && lh(PLAYER + 0x30e) == 0;
        if (on_it || lw(L6_ADDR(0x00162100)) != 0) {
            ship_offer_l6(m);
            const bool asked = lw(L6_ADDR(0x00162100)) != 0;
            if (asked || ((lwu(0x0013cae4) & 0x10u) != 0 && lw(L6_ADDR(0x0015f594)) == 10 && lw(pv + 0x9c) >= 0)) {
                sw(pv + 0x110, 1);
                sw(L6_ADDR(0x0015fdf8), 0);
                if (asked) {
                    sw(pv + 0x9c, 2);
                    sw(L6_ADDR(0x00162100), 0);
                }
                switch (lw(pv + 0x9c)) {  // (the table 0x201910: one case a choice, 0 .. 4)
                case 0:
                    sw(pv + 0xc0, 0);
                    sw(pv + 0xbc, 1);
                    swc1(pv + 0xc4, lwc1(L6_ADDR(0x001620f8)));
                    swc1(pv + 0xc8, lwc1(L6_ADDR(0x001620f4)));
                    music_track_request(4, 8);
                    break;
                case 1:
                    sw(pv + 0xbc, 0);
                    sw(pv + 0xc0, 1);
                    swc1(pv + 0xc4, lwc1(L6_ADDR(0x001620f4)));
                    swc1(pv + 0xc8, lwc1(L6_ADDR(0x001620f8)));
                    music_track_request(0, 0xa);
                    break;
                case 2:
                    sw(pv + 0xc0, 2);
                    swc1(pv + 0xc4, lwc1(L6_ADDR(0x001620f4)));
                    swc1(pv + 0xc8, lwc1(L6_ADDR(0x001620f8)));
                    sw(pv + 0xbc, 0);
                    break;
                case 3:
                    sw(pv + 0xc0, 3);
                    sw(pv + 0xbc, 2);
                    swc1(pv + 0xc8, lwc1(L6_ADDR(0x001620f8)));
                    swc1(pv + 0xc4, lwc1(L6_ADDR(0x001620f8)));
                    break;
                case 4:
                    sw(pv + 0xc0, 4);
                    swc1(pv + 0xc8, lwc1(L6_ADDR(0x001620f8)));
                    sw(pv + 0xbc, 0);
                    swc1(pv + 0xc4, lwc1(L6_ADDR(0x001620f8)));
                    break;
                default:
                    break;
                }
            }
        } else if (lw(pv + 0x110) == 0) {
            // Ratchet elsewhere in the station: the ship moved to the site near him
            if (lw(pv + 0xbc) == 0 && point_in_box(HERO_POS, lw(pv + 0x100)) != 0) {
                ship_to_box(m, pv + 0xa8);
                sw(pv + 0xbc, 2);
                sw(L6_ADDR(0x0015fdf8), 0);
            } else if (lw(pv + 0xbc) == 2 && point_in_box(HERO_POS, lw(pv + 0x104)) != 0) {
                ship_to_box(m, pv + 0xa0);
                sw(pv + 0xbc, 0);
                sw(L6_ADDR(0x0015fdf8), 0);
            } else if (lw(pv + 0xbc) != 1 && point_in_box(HERO_POS, lw(pv + 0x108)) != 0) {
                ship_to_box(m, pv + 0xa4);
                sw(pv + 0xbc, 1);
                sw(L6_ADDR(0x0015fdf8), 1);
                if (lbu(0x0013d3a9) == 0) sb(0x0013d3a9, 1);
            }
        }
        if (lw(pv + 0x110) != 0) launch();
        break;
    }
    case 2:
        if (point_in_box(HERO_POS, lw(pv + 0xf0)) == 0) break;
        sw(pv + 0xc0, 2);
        swc1(pv + 0xc4, lwc1(L6_ADDR(0x001620f4)));
        swc1(pv + 0xc8, lwc1(L6_ADDR(0x001620f8)));
        sw(pv + 0xbc, 0);
        sw(pv + 0x110, 1);
        sw(pv + 0x114, 1);
        sb(m + 0x20, 1);
        break;
    case 3: {
        if (!ship_flight_l6(m, lw(pv + 0xc0))) break;
        sb(m + 0x20, 4);
        sw(pv + 0xf4, 0);
        ship_to_box(m, pv + 0xa0 + (static_cast<uint32_t>(lw(pv + 0xbc)) << 2));
        euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24));
        if (lw(pv + 0xbc) == 1) {
            sw(L6_ADDR(0x0015fdf8), 1);
            if (lbu(0x0013d3a9) == 0) sb(0x0013d3a9, 1);
        } else {
            sw(L6_ADDR(0x0015fdf8), 0);
        }
        // Ratchet put 2 behind it and 1 up (its matrix's rows), facing away
        lq_sq(spot, m + 0x10);
        vec3_set_length(V(from), V(m + 0xc0), -2.0f);
        vec3_add(V(from), V(from), V(m + 0xe0));
        vec3_add(V(spot), V(spot), V(from));
        vec_clear(V(angles));
        swc1(angles + 8, angle_add(lwc1(m + 0x48), kPi));
        hero_place(spot, angles, 0, 0);
        if (lw(pv + 0x114) != 0) {
            sb(0x0013d3ac, 1);
            moby_rest_sparkles(table_moby(lwu(pv + 0x10c)));
            sw(pv + 0x114, 0);
            sb(m + 0x20, 6);
            if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(0x1e));
        }
        hydro_2eac18(0);
        break;
    }
    case 4: {
        const uint32_t set = lbu(m + 0x53);
        sb(m + 0x20, 1);
        if (set != 2) moby_anim_set(m, 2, 0, frame_scale_i(0x1e));
        break;
    }
    case 6: {
        const uint32_t bot = table_moby(lwu(pv + 0x10c));
        if (!(vec2_distance(V(HERO_POS), V(bot + 0x10)) < 3.5f)) break;
        lq_sq(spot, bot + 0x10);
        lq_sq(angles, bot + 0x40);
        swc1(angles + 8, angle_add(lwc1(angles + 8), u2f(0x40490fd0u)));  // (a half turn, 3.14159)
        hero_place(spot, angles, 0x72, 1);
        music_track_request(0, 0xa);
        cutscene_start(0xc);
        moby_retire(bot);
        level_mark_visited(5);
        sb(m + 0x20, 8);
        checkpoint_mark_collected(lwu(pv + 0xf8));
        checkpoint_save(spot, angles);
        break;
    }
    case 7:
        if (lw(L6_ADDR(0x0015f5c4)) == 2) break;
        if (lbu(0x0013d4de) != 0) {
            cutscene_start(5);
            sb(m + 0x20, 9);
            break;
        }
        level_arrival_message(5);
        sb(m + 0x20, 1);
        hero_enter_idle_l6();
        level_change_request(0, -1);
        break;
    case 8:
        if (lw(L6_ADDR(0x0015f5c4)) == 2) break;
        movie_play(5);
        sb(m + 0x20, 7);
        break;
    case 9:
        if (lw(L6_ADDR(0x0015f5c4)) == 2) break;
        level_arrival_message(5);
        level_change_request(0, -1);
        sb(m + 0x20, 1);
        break;
    default:
        break;
    }
    vec3_sub(V(moved), V(m + 0x10), V(old_pos));
    attach_frame_set(pv + 0x20, moved, old_rot, m + 0x40);
    ee_stack_free(0xf0);
}

}  // namespace rac1
