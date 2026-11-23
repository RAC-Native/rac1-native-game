// Orxon's own moby classes (level 10): the hazards. Class 702, a vent that puffs steam (particle 0x28) on a timer;
// class 939, a pipe that spits blobs of sludge (class 938) at random, and the blob, which flies, bounces, is knocked
// away by a hit and bursts; class 947, a spark that runs along its path with a light and a sound, shedding sparks of
// class 1090; class 859, a shot that keeps to the ground or to its target's height and bursts on what it meets. Header
// orxon_mobys.h, registered in orxon_mobys.cpp; level 10's own addresses (`orig: L10:`).
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace orxon;

// ---- class 702, the steam vent

// Class 702: state 0 waits its off time (pvar +0xc, from +8) and turns on; 1 puffs three clouds of steam (particle 0x28)
// a frame along its facing (speed: pvar +0 times 3, by the frame time), keeps its hiss (sound set 0x519, slot +0x10)
// going, and after its on time (+0xc, from +4) turns off and stops the hiss.
// orig: L10:0x002c7a20
void orx_class702_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        sw(pv + 0x10, 0xffffffffu);
        if (tick32(pv + 0xc) != 0) {
            sw(pv + 0xc, lwu(pv + 4));
            sb(m + 0x20, 1);
        }
        break;
    case 1: {
        const uint32_t frame = ee_stack_alloc(0xd0);
        const uint32_t turn = frame, pos = frame + 0x40, vel = frame + 0x50, step = frame + 0x60;
        lq_sq(pos, m + 0x10);
        euler_to_mat4_vu(M(turn), V(m + 0x40));
        vec_clear(V(vel));
        swc1(vel + 8, fpu_mul(fpu_mul(lwc1(pv), 3.0f), dt()));
        mat4_transform(V(vel), V(vel), M(turn));
        vec3_scale(V(step), V(vel), u2f(0x3ea8f5c3u));
        for (int32_t k = 0; k < 3; ++k) {
            particle_28_spawn(pos, vel, 0x40, m);
            if (k < 2) vec3_add(V(pos), V(pos), V(step));
        }
        if (sub_0028d8c0(m, lw(pv + 0x10)) == 0) sw(pv + 0x10, static_cast<uint32_t>(moby_sound_play(0, 4, m, 0x519)));
        if (tick32(pv + 0xc) != 0) {
            sw(pv + 0xc, lwu(pv + 8));
            sb(m + 0x20, 0);
            sound_stop_owned(m, lw(pv + 0x10));
            sw(pv + 0x10, 0xffffffffu);
        }
        ee_stack_free(0xd0);
        break;
    }
    default:
        break;
    }
}

// ---- class 939, the sludge pipe, and class 938, its blob

// A blob (class 938) of pipe `pipe` at `pos` moving by `vel` a frame: lit, its pvars the velocity +0, the pipe +0x10,
// its spins +0x18 / +0x1c (60 to 180 degrees a frame time), the bounce timer +0x14 (60 frames), raised by the level's
// offset L10:0x161bec (the blobs fly that much lower than they are drawn). The blob (0: none).
// orig: L10:0x002d8058
uint32_t orx_2d8058_l10(uint32_t pipe, uint32_t pos, uint32_t vel) {
    const uint32_t b = moby_spawn(0x3aa);
    if (b == 0) return 0;
    const uint32_t bpv = lwu(b + 0x78);
    sb(b + 0x30, 0xff);
    sh(b + 0x32, 0x7f);
    sb(b + 0x31, 1);
    sb(b + 0x20, 0);
    sw(bpv + 0x10, pipe);
    sh(bpv + 0x16, 0);
    lq_sq(b + 0x10, pos);
    swc1(b + 0x18, fpu_add(lwc1(b + 0x18), lwc1(L10_ADDR(0x00161bec))));
    lq_sq(bpv, vel);
    const float lo = u2f(0x3f860a92u), hi = u2f(0x40490fdbu);  // (60 and 180 degrees)
    swc1(bpv + 0x18, rand_range_f(fpu_mul(dt(), lo), fpu_mul(dt(), hi)));
    swc1(bpv + 0x1c, rand_range_f(fpu_mul(dt(), lo), fpu_mul(dt(), hi)));
    sh(bpv + 0x14, static_cast<uint32_t>(frame_scale_i(0x3c)));
    moby_update_transform(b);
    return b;
}

// Class 939: every so often (the level's L10:0x161bf0 .. 0x161bf4 frames) a blob at a random speed (0x161c04 ..
// 0x161c08 a frame time) and pitch (0x161bfc .. 0x161c00 degrees) within 0x161bf8 degrees of its heading, and its sound.
// orig: L10:0x002d8c00
void orx_class939_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    auto rearm = [&] {
        sw(pv, static_cast<uint32_t>(frame_scale_i(rand_range(lw(L10_ADDR(0x00161bf0)), lw(L10_ADDR(0x00161bf4))))));
    };
    if (lbu(m + 0x20) == 0) {
        rearm();
        sb(m + 0x20, 1);
    }
    if (tick32(pv) == 0) return;
    const float degree = u2f(0x3c8efa35u);
    const float spread = lwc1(L10_ADDR(0x00161bf8));
    const float yaw = angle_add(lwc1(m + 0x48), fpu_mul(rand_range_f(-spread, spread), degree));
    const float pitch = fpu_mul(rand_range_f(lwc1(L10_ADDR(0x00161bfc)), lwc1(L10_ADDR(0x00161c00))), degree);
    const float speed = rand_range_f(lwc1(L10_ADDR(0x00161c04)), lwc1(L10_ADDR(0x00161c08)));
    const uint32_t frame = ee_stack_alloc(0x80);
    const uint32_t vel = frame, at = frame + 0x10;
    vec_from_spherical_v(V(vel), fpu_mul(speed, dt()), yaw, pitch);
    vec3_add(V(at), V(m + 0x10), V(vel));
    orx_2d8058_l10(m, at, vel);
    moby_sound_start(0, 0, m);
    rearm();
    ee_stack_free(0x80);
}

namespace {
// The blob turned toward the surface's normal `n`: its pitch (+0x40) and roll (+0x44) eased 3 degrees toward the normal
// seen in its heading's frame.
void blob_lean(uint32_t m, const Vec4& n) {
    const float rate = u2f(0x3d567750u);
    const float c = cos_vu(lwc1(m + 0x48)), s = sin_vu(lwc1(m + 0x48));
    const float ahead = fpu_add(fpu_mul(n.x, c), fpu_mul(n.y, s));
    const float c2 = cos_vu(lwc1(m + 0x48)), s2 = sin_vu(lwc1(m + 0x48));
    const float side = fpu_sub(fpu_mul(n.y, c2), fpu_mul(n.x, s2));
    const float flat = sqrt_f(fpu_add(fpu_mul(ahead, ahead), fpu_mul(n.z, n.z)));
    swc1(m + 0x40, angle_seek_limited(lwc1(m + 0x40), -atan2_f(flat, side), rate));
    swc1(m + 0x44, angle_seek_limited(lwc1(m + 0x44), atan2_f(n.z, ahead), rate));
}
}  // namespace

// The blob's bounce (`pv` its pvars, `from` its place at the start of the frame), once its bounce timer (+0x14) is out:
// along the way it hit a moby (pushed out from the moby's centre, the record's normal set to that: 1) or the world
// (pushed out along the normal: on a floor - within 45 degrees of up - it stays on it, its fall turned into a small
// hop, and a blob slower than the frame time stops: 3; else 2): its speed reflected and damped (0.35 off a moby, 0.5
// off the world), the blob leaned along the normal and its bounce sound. 0: no bounce.
// orig: L10:0x002d8170
uint32_t orx_2d8170_l10(uint32_t m, uint32_t pv, uint32_t from) {
    const Vec4 start = V(from);
    const Vec4 up{0.0f, 0.0f, 1.0f, 0.0f};
    if (lh(pv + 0x14) != 0) return 0;
    const uint32_t pos = m + 0x10;
    if (segment_collide_v(start, V(pos), 0x10, m, nullptr) == 0) return 0;
    Vec4 push;
    const uint32_t hit = coll_hit_moby();
    if (hit != 0) {
        Vec4 normal = coll_hit_normal();  // (the hit record's normal +0x40: written here when a moby was hit)
        vec3_sub(normal, V(pos), V(hit + 0x10));
        coll_hit_set_normal(normal);
        V(pos) = coll_hit_point();
        vec3_set_length(push, coll_hit_normal(), fpu_mul(dt2(), u2f(0x411b3333u)));
        vec3_add(V(pos), V(pos), push);
        vec3_reflect(V(pv), V(pv), coll_hit_normal());
        vec3_scale(V(pv), V(pv), u2f(0x3eb33333u));
        vec3_set_length(push, coll_hit_normal(), 1.0f);  // (and its dot with up, which the original leaves unused)
        blob_lean(m, coll_hit_normal());
        moby_sound_start(1, 0, m);
        return 1;
    }
    if (static_cast<int32_t>(coll_hit_flags()) <= 0) return 0;
    V(pos) = coll_hit_point();
    vec3_set_length(push, coll_hit_normal(), fpu_mul(dt2(), u2f(0x411b3333u)));
    vec3_add(V(pos), V(pos), push);
    vec3_reflect(V(pv), V(pv), coll_hit_normal());
    vec3_scale(V(pv), V(pv), 0.5f);
    vec3_set_length(push, coll_hit_normal(), 1.0f);
    if (u2f(0x3f34fdf4u) < vec3_dot(push, up)) {  // a floor
        const float floor = floor_probe_v(0.5f, V(pos), 0);
        if (lwc1(m + 0x18) < floor) swc1(m + 0x18, floor);
        const float vz = lwc1(pv + 8);
        if (vz < 0.0f) swc1(pv + 8, fpu_mul(vz, u2f(0xbecccccdu)));
        if (vec3_length(V(pv)) < dt()) {
            vec_clear(V(pv));
            return 3;
        }
    }
    blob_lean(m, coll_hit_normal());
    moby_sound_start(1, 0, m);
    return 2;
}

namespace {
// The blob hit (mask 0x830000) by anything but another blob: a strong hit (+0x28) bursts it (state 3); else it is
// knocked away from the hero (10 frame times out and up, along the hit's direction: vec_yaw_marked_v) for 15 frames
// (state 2). Its hit slot cleared either way.
void blob_hit(uint32_t m, uint32_t pv, uint32_t yaw_at) {
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x830000u), 0);
    if (hit != 0) {
        const uint32_t src = lwu(hit + 0x20);
        if (src != 0 && lh(src + 0xa6) != lh(m + 0xa6)) {
            if (lbu(hit + 0x28) != 0) {
                sb(m + 0x20, 3);
            } else {
                const float yaw = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO_POS)), fpu_sub(lwc1(m + 0x14), lwc1(HERO_POS + 4)));
                const float kick = fpu_mul(dt(), 10.0f);
                const uint32_t yaw_w = yaw_at, out_w = yaw_at + 4, up_w = yaw_at + 8;  // (the in-out floats, in the frame)
                swc1(yaw_w, yaw);
                swc1(up_w, kick);
                swc1(out_w, kick);
                const Vec4 dir = V(hit + 0x10);
                vec_yaw_marked_v(dir, *static_cast<float*>(ee_ram_ptr(yaw_w)), *static_cast<float*>(ee_ram_ptr(out_w)),
                                 *static_cast<float*>(ee_ram_ptr(up_w)));
                swc1(pv, fpu_mul(cos_vu(lwc1(yaw_w)), lwc1(out_w)));
                sw(pv + 8, 0);
                swc1(pv + 4, fpu_mul(sin_vu(lwc1(yaw_w)), lwc1(out_w)));
                swc1(pv + 8, lwc1(up_w));
                sh(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0xf)));
                sb(m + 0x20, 2);
            }
        }
    }
    sb(m + 0xa4, 0xff);
}
// the blob's fall: its speed down by 9.8 times the frame's square, then moved
void blob_fly(uint32_t m, uint32_t pv) {
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(dt2(), u2f(0x411ccccdu))));
}
bool outside(float v) { return v < 1.0f || u2f(0x43ff8000u) < v; }  // (the level's cube: 1 .. 511)
}  // namespace

// Class 938, the blob (lowered by the level's offset L10:0x161bec while it updates). State 0 flies and bounces (resting
// after a bounce that stopped it: 1, for 240 frames); 1 fades to black over its rest, then bursts (3); 2, knocked away,
// flies lit until it bounces or falls below 2 (then 3); 3 bursts: a hit on the mobys within 1.5 and its explosion. A
// hit can knock it away (2) or burst it (3) in states 0 and 1; it goes outside the level's cube.
// orig: L10:0x002d85c8
void orx_class938_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (outside(lwc1(m + 0x10)) || outside(lwc1(m + 0x14)) || outside(lwc1(m + 0x18))) {
        moby_retire(m);
        return;
    }
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lwc1(L10_ADDR(0x00161bec))));
    const uint32_t frame = ee_stack_alloc(0x100);
    const uint32_t start = frame;
    lq_sq(start, m + 0x10);
    const uint32_t state = lbu(m + 0x20);
    switch (state) {
    case 0:
        blob_fly(m, pv);
        blob_hit(m, pv, frame + 0x50);
        if (tick16(pv + 0x14) != 0 && orx_2d8170_l10(m, pv, start) == 3) {
            sh(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0xf0)));
            sb(m + 0x20, 1);
        } else if (lwc1(m + 0x18) < 2.0f) {
            sb(m + 0x20, 3);
        }
        break;
    case 1: {
        moby_get_color_hi(m, frame + 0x5c, frame + 0x60, frame + 0x64);
        const int32_t rest = frame_scale_i(0xf0);
        const int32_t span = frame_scale_i(0xf0);
        const int32_t gone = rest - lh(pv + 0x14);
        moby_set_color_hi(m, ee_div(static_cast<int32_t>(static_cast<uint32_t>(gone) * 0x7fu), span), lw(frame + 0x60), lw(frame + 0x64));
        if (tick16(pv + 0x14) != 0) sb(m + 0x20, 3);
        else blob_hit(m, pv, frame + 0x68);
        break;
    }
    case 2: {
        moby_get_color_hi(m, frame + 0x5c, frame + 0x60, frame + 0x64);
        moby_set_color_hi(m, 0xff, lw(frame + 0x60), lw(frame + 0x64));
        // (the original also fills a hit record on its stack here, +0x20 .. +0x3c, that nothing reads)
        blob_fly(m, pv);
        bool burst = tick16(pv + 0x14) != 0 && orx_2d8170_l10(m, pv, start) != 0;
        if (!burst) burst = lwc1(m + 0x18) < 2.0f;
        if (burst) sb(m + 0x20, 3);
        break;
    }
    case 3: {
        const int32_t n = mobys_in_sphere(m + 0x10, 1.5f, 0x10, m, 0);
        const uint32_t at = frame + 0x10;
        lq_sq(at, m + 0x10);
        moby_list_hit(m, at, coll_hit_list(), n, 0, NOT_ADDR(0x810001u), 2, 1, 1.0f, 1.0f, 1.0f);
        explosion_burst(m, m + 0x10, 0, 0.5f, 13.0f);
        moby_retire(m);
        ee_stack_free(0x100);
        return;
    }
    default:
        break;
    }
    ee_stack_free(0x100);
    const float z = fpu_add(lwc1(m + 0x18), lwc1(L10_ADDR(0x00161bec)));
    swc1(m + 0x18, z);
    if (outside(lwc1(m + 0x10)) || outside(lwc1(m + 0x14)) || outside(z)) moby_retire(m);
}

// ---- class 947, the spark on its path, and class 1090, the sparks it sheds

// A spark (class 1090) shed by `owner` at `pos` on its path `path` (pvars: the path +0, the distance along it +4, the
// speed +8, the leg +0xc, the owner +0x10, its life +0x14: 10 over 5 frame times, a fade +0x18 of 0.97 .. 0.999), orange
// with its glow (particle 0x1a). The spark (0: none).
// orig: L10:0x002dd270
uint32_t orx_2dd270_l10(uint32_t owner, uint32_t pos, uint32_t path, float dist, float speed, float leg) {
    const uint32_t s = moby_spawn(0x442);
    if (s == 0) return 0;
    sh(s + 0x32, 0xff);
    sb(s + 0x31, 1);
    sb(s + 0x23, 0x40);
    sb(s + 0x30, 0xff);
    sb(s + 0x20, 0);
    const uint32_t spv = lwu(s + 0x78);
    lq_sq(s + 0x10, pos);
    moby_set_color_hi(s, 0x7f, 0x40, 0);
    sw(spv, path);
    swc1(spv + 4, dist);
    swc1(spv + 8, speed);
    swc1(spv + 0xc, leg);
    sw(spv + 0x10, owner);
    sw(spv + 0x14, static_cast<uint32_t>(float_to_int(fpu_div(10.0f, fpu_mul(dt(), 5.0f)))));
    swc1(spv + 0x18, rand_range_f(u2f(0x3f7fbe77u), u2f(0x3f7851ecu)));
    const int32_t life = float_to_int(fpu_div(10.0f, fpu_mul(dt(), 5.0f)));
    spawn_particle_1a(s, 0x1f4f7f7fu, static_cast<uint32_t>(life), 0xffffffffu, u2f(0x47f61801u));
    return s;
}

// Class 947: its path (pvar +0; -1: it goes). State 0, off: its light (+0xc) and sound (+0x10) stopped, until its timer
// (+8) runs out; 1 runs along the path at 5 a frame time (+4, the path's first leg the spacing) with its light (radius
// 3) and, every other frame on average, a shed spark (on odd frames), its sound kept at its place, and at the path's
// last point it goes off for 120 frames. It is always drawn bright (+0x30).
// orig: L10:0x002d8d30
void orx_class947_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv) == -1) {
        debug_printf_stub();
        moby_retire(m);
        return;
    }
    const uint32_t path = lvd::path(lw(pv));
    sb(m + 0x30, 0xff);
    switch (lbu(m + 0x20)) {
    case 0: {
        if (lw(pv + 0xc) != -1) {
            region_clear(lwu(pv + 0xc));
            sw(pv + 0xc, 0xffffffffu);
        }
        sound_stop_owned(m, lw(pv + 0x10));
        sw(pv + 0x10, 0xffffffffu);
        if (tick32(pv + 8) != 0) {
            sb(m + 0x20, 1);
            sw(pv + 4, 0);
        }
        break;
    }
    case 1: {
        const uint32_t frame = ee_stack_alloc(0x80);
        const uint32_t at = frame, index = frame + 0x10, frac = frame + 0x14;
        const float leg = vec3_distance(V(path + 0x10), V(path + 0x20));
        const float dist = fpu_add(lwc1(pv + 4), fpu_mul(dt(), 5.0f));
        swc1(pv + 4, dist);
        path_point_even(path, index, frac, at, dist, leg);
        if (lw(pv + 0xc) == -1) {
            sw(pv + 0xc, static_cast<uint32_t>(light_add(at, 3.0f, 0.0f, 1.0f, 1.0f, 0.5f)));
        } else {
            const uint32_t light = OV(0x001802c0) + (lwu(pv + 0xc) << 5);
            lq_sq(light + 0x10, at);
            swc1(light + 0x1c, 3.0f);
            swc1(light + 4, 1.0f);
            swc1(light + 8, 0.5f);
            sw(light + 0xc, 0);
            swc1(light, 1.0f);
        }
        if (rand_mod(2) == 0) {
            if ((lw(OV(0x0015f5cc)) & 1) != 0) orx_2dd270_l10(m, at, path, lwc1(pv + 4), fpu_mul(dt(), 5.0f), leg);
            if (sub_0028d8c0(m, lw(pv + 0x10)) != 0) {
                lq_sq(SOUND_SLOTS + static_cast<uint32_t>(lw(pv + 0x10)) * 0x70 + 0x90, at);
            } else {
                const int32_t slot = moby_sound_play(0, 0xc, m, 0x442);
                sw(pv + 0x10, static_cast<uint32_t>(slot));
                if (slot != -1) lq_sq(SOUND_SLOTS + static_cast<uint32_t>(slot) * 0x70 + 0x90, at);
            }
            if (lw(index) == lw(path) - 1) {
                sb(m + 0x20, 0);
                sw(pv + 8, static_cast<uint32_t>(frame_scale_i(0x78)));
            }
        }
        ee_stack_free(0x80);
        break;
    }
    default:
        break;
    }
}

// Class 1090, the spark class 947 sheds (orx_2dd270_l10's pvars): it runs on along its path, slowing by its fade a frame
// (+0x18, toward 1 by the frame scale 0x15ed60), tumbling, for its life (+0x14); it grows to half the class's size over
// the first quarter of its life and shrinks over its last half; while bigger than a quarter of the class's size it hits the
// mobys within 0.15 of it (but its shedder).
// orig: L10:0x002dd3d8
void orx_class1090_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float fade = fpu_add(fpu_mul(fpu_sub(lwc1(pv + 0x18), 1.0f), lwc1(0x0015ed60)), 1.0f);
    const float dist = fpu_add(lwc1(pv + 4), lwc1(pv + 8));
    swc1(pv + 4, dist);
    swc1(pv + 8, fpu_mul(lwc1(pv + 8), fade));
    const uint32_t frame = ee_stack_alloc(0x40);
    const uint32_t index = frame + 0x20, frac = frame + 0x24, at = frame + 0x10;
    path_point_even(lwu(pv), index, frac, m + 0x10, dist, lwc1(pv + 0xc));
    swc1(m + 0x40, rand_angle());
    swc1(m + 0x44, rand_angle());
    swc1(m + 0x48, rand_angle());
    if (tick32(pv + 0x14) != 0) {
        moby_retire(m);
        ee_stack_free(0x40);
        return;
    }
    auto life = [] { return float_to_int(fpu_div(10.0f, fpu_mul(dt(), 5.0f))); };  // (its whole life, as orx_2dd270_l10 set it)
    const float full = lwc1(lwu(m + 0x24) + 0x24);  // (the class's scale)
    if (lw(pv + 0x14) < life() / 2) {  // shrinking: its last half
        const float left = int_to_float(lw(pv + 0x14));
        swc1(m + 0x2c, fpu_mul(fpu_div(fpu_mul(full, left), int_to_float(life() / 2)), 0.5f));
    } else if (life() / 4 * 3 < lw(pv + 0x14)) {  // growing: its first quarter
        const float gone = int_to_float(life() - lw(pv + 0x14));
        swc1(m + 0x2c, fpu_mul(fpu_div(fpu_mul(full, gone), int_to_float(life() / 4)), 0.5f));
    }
    if (fpu_mul(lwc1(lwu(m + 0x24) + 0x24), 0.25f) < lwc1(m + 0x2c)) {
        const int32_t n = mobys_in_sphere(m + 0x10, u2f(0x3e19999au), 0x10, lwu(pv + 0x10), 0);
        lq_sq(at, m + 0x10);
        moby_list_hit(lwu(pv + 0x10), at, coll_hit_list(), n, 0, NOT_ADDR(0x810001u), 2, 1, 2.0f, 1.0f, 1.0f);
    }
    ee_stack_free(0x40);
}

// ---- class 1073, the strands

// Class 1073: four groups (+0, 0x2580 apart) of ten strands of 20 points (0x140 apart, the strands 16 bytes apart), their
// speeds 0x9600 further, each strand's timer at +0x12c00 (the group's 30 words apart). State 0 (gone at once in Clank's
// part on level 10) sets the level's renewal (L10:0x161d64, 45 frames) and the strands' first timers (staggered by 4.5
// frames a strand, 11 to 12 frames a group); 1 moves the phase +0x12de0 by L10:0x161d5c a frame (wrapped to 1) and each
// strand either drifts (its points 1 .. 18 by their speeds) or, its timer out, is laid out again along y (the level's
// length L10:0x161d58 over 20 points, w 1) with new speeds: random offsets (radius up to L10:0x161d54 frame times)
// smoothed over three neighbours.
// orig: L10:0x002dcc58
void orx_class1073_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t timers = pv + 0x12c00;
    switch (lbu(m + 0x20)) {
    case 0: {
        if (level_number() == 10 && lbu(HERO_FORM) == 1) {
            moby_retire(m);
            return;
        }
        sw(L10_ADDR(0x00161d64), 0x2d);
        sw(L10_ADDR(0x00161d68), 0x16);
        for (int32_t j = 0; j < 10; ++j) {
            const int32_t q = ee_div(j * 0x2d, 10);
            const uint32_t t = timers + static_cast<uint32_t>(j) * 4;
            sw(t, static_cast<uint32_t>(q));
            sw(t + 0x78, static_cast<uint32_t>(q + 0xb));
            sw(t + 0xf0, static_cast<uint32_t>(q + 0x17));
            sw(t + 0x168, static_cast<uint32_t>(q + 0x22));
        }
        sb(m + 0x20, 1);
        break;
    }
    case 1: {
        const uint32_t phase = pv + 0x12de0;
        const float p = fpu_add(lwc1(phase), fpu_mul(lwc1(L10_ADDR(0x00161d5c)), dt()));
        swc1(phase, 1.0f < p ? fpu_sub(p, 1.0f) : p);
        const uint32_t offsets = ee_stack_alloc(0xc80);  // (the random offsets: 20 points x 10 strands, as the original's stack)
        for (int32_t i = 0; i < 4; ++i) {
            const uint32_t points = pv + static_cast<uint32_t>(i) * 0x2580;
            const uint32_t speeds = points + 0x9600;
            for (int32_t j = 0; j < 10; ++j) {
                const uint32_t strand = static_cast<uint32_t>(j) * 16;
                const uint32_t timer = timers + static_cast<uint32_t>(i) * 0x78 + static_cast<uint32_t>(j) * 4;
                if (tick32(timer) == 0) {  // drifting
                    for (uint32_t k = 1; k < 19; ++k)
                        vec3_add(V(points + strand + k * 0x140), V(points + strand + k * 0x140), V(speeds + strand + k * 0x140));
                    continue;
                }
                for (int32_t k = 0; k < 20; ++k) {
                    const uint32_t pt = points + strand + static_cast<uint32_t>(k) * 0x140;
                    vec_clear(V(pt));
                    const float len = lwc1(L10_ADDR(0x00161d58));
                    const float step = fpu_div(len, 20.0f);
                    const float reach = fpu_mul(lwc1(L10_ADDR(0x00161d54)), dt());
                    swc1(pt + 0xc, 1.0f);
                    swc1(pt + 4, fpu_sub(fpu_mul(static_cast<float>(k), step), fpu_mul(len, 0.5f)));
                    rand_sphere_point(offsets + strand + static_cast<uint32_t>(k) * 0xa0, 0.0f, reach);
                }
                for (uint32_t k = 1; k < 19; ++k) {
                    const uint32_t v = speeds + strand + k * 0x140;
                    const uint32_t r = offsets + strand + k * 0xa0;
                    vec3_add(V(v), V(r - 0xa0), V(r));
                    vec3_add(V(v), V(v), V(r + 0xa0));
                    vec3_scale(V(v), V(v), u2f(0x3eaa7efau));
                }
                sw(timer, lwu(L10_ADDR(0x00161d64)));
            }
        }
        ee_stack_free(0xc80);
        break;
    }
    default:
        break;
    }
}

// ---- class 859, the ground shot

// The shot's height eased toward `z`: by 0.1 at most.
// orig: L10:0x002d7b18
void orx_2d7b18_l10(uint32_t m, float z) {
    const float now = lwc1(m + 0x18);
    const float d = fpu_sub(z, now);
    if (u2f(0x3dcccccdu) < d) swc1(m + 0x18, fpu_add(now, u2f(0x3dcccccdu)));
    else if (d < u2f(0xbdcccccdu)) swc1(m + 0x18, fpu_sub(now, u2f(0x3dcccccdu)));
    else swc1(m + 0x18, z);
}

// Class 859: a shot moving by its velocity (pvar +0) for its life (+0x10) that keeps a height (+0x1c) by its mode
// (+0x12): 1 a fixed height, 2 above the ground under it, 3 its target's (within 20); facing its way (sub_00259888, its
// spin +0x18). It bursts on what it meets between its last place and this one (not a Gadgebot, class 857): moved there,
// five sparks (particle 0x1b) off the surface, and when it was the hero (and its shooter +0x20 is alive) the byte at its
// +0x14 set. It goes at the end of its life, outside the level's cube (2 .. 510) or beyond 80 of the camera.
// orig: L10:0x002d7b80
void orx_class859_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t frame = ee_stack_alloc(0x160);
    const uint32_t last = frame, target = frame + 0x10, dir = frame + 0x60, rec = frame + 0x80, spark = frame + 0xb0, vel = frame + 0x70;
    lq_sq(last, m + 0x10);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    target_select_any(m, target, 20.0f);
    switch (lh(pv + 0x12)) {  // (the table at L10:0x001dd100)
    case 1:  // (0x002d7c10)
        orx_2d7b18_l10(m, lwc1(pv + 0x1c));
        break;
    case 2: {  // (0x002d7c24)
        Vec4 from = V(m + 0x10), to = V(m + 0x10);
        from.z = fpu_add(from.z, u2f(0x3dcccccdu));
        to.z = fpu_sub(to.z, fpu_add(lwc1(pv + 0x1c), lwc1(pv + 0x1c)));
        if (segment_collide_v(from, to, 2, m, nullptr) != 0) orx_2d7b18_l10(m, fpu_add(coll_hit_point().z, lwc1(pv + 0x1c)));
        else orx_2d7b18_l10(m, fpu_sub(lwc1(m + 0x18), u2f(0x3dcccccdu)));
        break;
    }
    case 3:  // (0x002d7cc0)
        if (lw(target + 0x40) != 0) orx_2d7b18_l10(m, fpu_add(lwc1(target + 8), lwc1(pv + 0x1c)));
        break;
    default:  // 0, 4 (0x002d7ce0)
        break;
    }
    lq_sq(dir, pv);
    sw(dir + 8, 0);
    vec2_set_length(V(dir), V(dir), 1.0f);
    swc1(dir + 8, 1.0f);
    sub_00259888(lwc1(pv + 0x18), rec, m, 0x10001, dir);
    bool gone = tick16(pv + 0x10) != 0;
    if (!gone && segment_collide_v(V(last), V(m + 0x10), 0, lwu(pv + 0x20), static_cast<const uint32_t*>(ee_ram_ptr(rec))) != 0) {
        const uint32_t hit = coll_hit_moby();
        if (hit == 0 || lh(hit + 0xa6) != 0x359) {
            V(m + 0x10) = coll_hit_point();
            const uint32_t note = lwu(pv + 0x14);
            if (note != 0 && hit != 0 && hit == lwu(0x001413d0)) {  // (the hero's moby)
                const uint32_t shooter = lwu(pv + 0x20);
                if (shooter != 0 && lbu(shooter + 0x20) != 0xfe && lbu(shooter + 0x20) != 0xfd) sb(note, 1);
            }
            for (int32_t k = 0; k < 5; ++k) {
                V(spark) = Vec4{};
                swc1(spark, rand_range_f(-1.0f, 1.0f));
                swc1(spark + 4, rand_range_f(-1.0f, 1.0f));
                swc1(spark + 8, rand_range_f(-1.0f, 1.0f));
                lq_sq(vel, spark);
                vec3_reflect(V(spark), V(pv), coll_hit_normal());
                vec3_set_length(V(vel), V(vel), fpu_mul(vec3_length(V(spark)), 0.5f));
                vec3_add(V(vel), V(spark), V(vel));
                vec3_set_length(V(vel), V(vel), rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.0f)));
                const int32_t lo = frame_scale_i(0xa);
                const int32_t hi = frame_scale_i(0xf);
                spawn_particle_1b(m + 0x10, vel, 0x7f2f4f6fu, static_cast<uint32_t>(rand_range(lo, hi)), 30000.0f);
            }
            gone = true;
        }
    }
    if (!gone) {
        const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
        const float lo = 2.0f, hi = 510.0f;
        gone = x < lo || y < lo || z < lo || hi < x || hi < y || hi < z || 80.0f < vec2_distance(V(m + 0x10), V(OV(0x00166dc0)));
    }
    if (gone) moby_retire(m);
    ee_stack_free(0x160);
}

}  // namespace rac1
