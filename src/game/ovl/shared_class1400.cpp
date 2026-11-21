// Class 1400: one port of the copies Batalia (L8), Hoven (L12) and Oltanis (L14) carry (the same code relinked; compared word
// by word: only relocated words differ). Each level keeps its own record (per level below): +0 the far drops' slowest
// fall, +4 the height over the camera below which a drop is spawned, +8 the near drops' spread, +0xc / +0x10 the fewest
// and most drops a wind gives, +0x14 the slowest fall when not below 0, +0x18 the far drops' kind, +0x1c the depth under
// the camera they fall to at least, +0x20 / +0x24 the wind, +0x30 its timer, +0x34 the drops' count. The rest is the
// engine's (OV: the camera 0x166dc0 (+0x14 / +0x18 its angles), the camera's move 0x160250 and the drops' lowest height
// 0x160260 the particles read, the particles' texture words 0x1b2080 / 0x1b21a4) or the base ELF's (bare: the level's
// number 0x15ed84, the frame's time 0x15ed6c). Header shared_class1400.h.
//
// The moby: +0x20 its state (0 set up, 2 running); the pvars: +0x30 the camera's place last frame.
#include <game/ovl/shared_class1400.h>

#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_height_map.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
// the level's record (+0 .. +0x34)
uint32_t record() {
    switch (ovl_reloc_level()) {
    case HOVEN: return L12_ADDR(0x00162000);
    case OLTANIS: return L14_ADDR(0x00162210);
    default: return L8_ADDR(0x00162340);
    }
}

int32_t trunc_w(float f) { return static_cast<int32_t>(f2u(eer::cvt_w(f))); }

// the point `at` (an EE vector) moved by 0 .. `reach` in a random direction (each axis its own angle and length)
void scatter(uint32_t at, float reach) {
    const float turn = u2f(0x40c90fdbu);  // (2 pi)
    const float cx = cos_vu(rand_range_f(0.0f, turn));
    const float dx = fpu_mul(cx, rand_range_f(0.0f, reach));
    swc1(at, fpu_add(lwc1(at), dx));
    const float sy = sin_vu(rand_range_f(0.0f, turn));
    const float dy = fpu_mul(sy, rand_range_f(0.0f, reach));
    swc1(at + 4, fpu_add(lwc1(at + 4), dy));
}
}  // namespace

// orig: L8:0x00307b90 L12:0x00308a80 L14:0x00306230
void class1400_wind(uint32_t motes) {
    const uint32_t rec = record();
    const uint32_t sp = ee_stack_alloc(0x40);
    float speed, soonest, most, fewest;  // (the wind's top speed a second, its shortest time, the drops' counts)
    if (motes == 0) {
        speed = 5.0f;
        soonest = 2.0f;
        most = 8.0f;
        fewest = 4.0f;
    } else {
        speed = 2.5f;
        soonest = 20.0f;
        most = 3.0f;
        fewest = 2.0f;
    }
    if (0.0f <= lwc1(rec + 0xc)) fewest = lwc1(rec + 0xc);
    if (0.0f <= lwc1(rec + 0x10)) most = lwc1(rec + 0x10);
    float lim = fpu_mul(speed, lwc1(0x0015ed6c));
    swc1(rec + 0x20, rand_range_f(-lim, lim));
    lim = fpu_mul(speed, lwc1(0x0015ed6c));
    swc1(rec + 0x24, rand_range_f(-lim, lim));
    const float lo = frame_scale_f(soonest);
    swc1(rec + 0x30, rand_range_f(lo, frame_scale_f(10.0f)));
    // the drops' count from the wind's strength against the strongest (the vector's z is the frame's leftover)
    const float top = fpu_mul(speed, lwc1(0x0015ed6c));
    swc1(sp + 4, top);
    swc1(sp, top);
    const float k = fpu_div(vec2_length(V(rec + 0x20)), vec3_length(V(sp)));
    sw(rec + 0x34, static_cast<uint32_t>(trunc_w(fpu_add(fpu_mul(k, fpu_sub(most, fewest)), fewest))));
    ee_stack_free(0x40);
}

// orig: L8:0x00272cb8 L12:0x00281420 L14:0x002706b0
void class1400_drop(uint32_t pos, int32_t kind, uint32_t vel, float floor) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t v = sp, step = sp + 0x10, end = sp + 0x20;
    lq_sq(v, vel);
    const uint32_t p = alloc_particle(0);
    if (p != 0) {
        float size = 0.0f, speed = 0.0f;
        int32_t alpha = 0;
        uint32_t tex = static_cast<uint32_t>(kind);
        if (kind == 0) {
            size = u2f(0x3c75c28fu);  // (0.015)
            alpha = 0x14;
            speed = rand_range_f(u2f(0x3c23d70au), 1.0f);  // (0.01 .. 1)
        } else if (kind == 1) {
            size = 0.5f;
            alpha = 0xd;
            speed = rand_range_f(1.0f, 5.0f);
        } else if (kind == 2) {
            size = 1.0f;
            alpha = 0xd;
            tex = 1;
            speed = rand_range_f(20.0f, 30.0f);
        }
        lq_sq(step, v);
        vec3_set_length(V(step), V(step), speed);
        sh(p + 0xa, static_cast<uint32_t>(kind));
        sw(p + 4, (static_cast<uint32_t>(alpha >> 2) << 24) | 0x606060u);
        sw(p + 0xc, (static_cast<uint32_t>(alpha) << 24) | 0x907070u);
        sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x60));
        sb(p + 1, 3);
        sb(p + 3, 0x44);
        sb(p + 2, lbu(lwu(OV(0x001b2080)) + tex));
        lq_sq(p + 0x10, pos);
        vec3_add(V(end), V(pos), V(step));
        lq_sq(p + 0x20, end);
        swc1(p + 0x1c, size);
        swc1(p + 0x2c, 1.0f);
        lq_sq(p + 0x30, v);
        swc1(p + 0x3c, floor);
    }
    ee_stack_free(0xc0);
}

// orig: L8:0x0027f5f0 L12:0x0028db38 L14:0x0027cfb0
void class1400_mote(uint32_t pos, int32_t kind, uint32_t vel, float floor) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t v = sp;
    lq_sq(v, vel);
    const uint32_t p = alloc_particle(0x49);
    if (p != 0) {
        float size = 0.0f;
        uint32_t alpha = 0;
        if (kind == 0) {
            alpha = 0x20;
            size = u2f(0x3df5c28fu);  // (0.12)
            rand_range_f(0.25f, 0.25f);  // (drawn and not used)
        } else if (kind == 1) {
            alpha = 0xd;
            size = 0.5f;
            rand_range_f(1.0f, 1.0f);  // (drawn and not used)
        }
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x37)));
        sw(p + 4, (alpha << 24) | 0xffffffu);
        sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x20));
        sb(p + 3, 0x48);
        sb(p + 1, 0);
        swc1(p + 0xc, fpu_mul(size, 210000.0f));
        sb(p + 2, lbu(lwu(OV(0x001b21a4)) + static_cast<uint32_t>(kind)));
        sb(p + 8, static_cast<uint32_t>(trunc_w(rand_range_f(0.0f, 360.0f))));
        lq_sq(p + 0x10, pos);
        lq_sq(p + 0x20, v);
        swc1(p + 0x2c, floor);
    }
    ee_stack_free(0x80);
}

// State 0 sets the wind up; state 2 deals it anew when its timer runs out, then spawns the wind's count of drops within
// the near spread about a point 16 ahead of the camera (falling faster the nearer they are, toward the far kind past 8)
// and 1.4 times as many within 8 of the camera, each to the ground's height under it (at least the camera's height less
// the record's depth); on the rain levels also as many splashes as near drops within 30 of a point 30 ahead and half as
// many within 8 of a point 4 ahead. Each frame it keeps the camera's place.
// orig: L8:0x00307cf0 L12:0x00308be0 L14:0x00306390
void class1400_update(uint32_t m) {
    const uint32_t level = lwu(0x0015ed84);
    const uint32_t motes = level == 0xc || level == 0x200 ? 1u : 0u;  // (Hoven, and the level number 0x200)
    const uint32_t rec = record();
    const uint32_t cam = OV(0x00166dc0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x150);
    const uint32_t at = sp, ahead = sp + 0x20, off = sp + 0x30, vel = sp + 0x40;
    // the near drops' spread (the wind's), the far drops' fall and its easing
    bool splashes;
    float spread, far_k, fall, slowest;
    if (motes != 0) {
        far_k = u2f(0x3e4ccccdu);  // (0.2)
        splashes = false;
        fall = 5.0f;
        slowest = 0.5f;
        spread = lwc1(0x0015ed6c);
    } else {
        const float ft = lwc1(0x0015ed6c);
        splashes = true;
        spread = fpu_add(ft, ft);
        far_k = u2f(0x3e19999au);  // (0.15)
        fall = 18.0f;
        slowest = lwc1(rec);
    }
    if (0.0f <= lwc1(rec + 0x14)) slowest = lwc1(rec + 0x14);
    const uint32_t state = lbu(m + 0x20);
    if (state == 0) {
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 2);
        class1400_wind(motes);
    } else if (state == 2) {
        if (float_timer_tick(rec + 0x30) != 0) class1400_wind(motes);
        vec3_sub(V(sp + 0x10), V(cam), V(pv + 0x30));
        lq_sq(OV(0x00160250), sp + 0x10);
        lq_sq(ahead, cam);
        const int32_t count = lw(rec + 0x34);
        sub_00262cf0(ahead, ahead, 16.0f, lwc1(cam + 0x18), lwc1(cam + 0x14));
        swc1(ahead + 8, fpu_add(lwc1(cam + 8), lwc1(rec + 4)));
        // a drop at `at` (its velocity at `v`) when the ground under it is below the spawn height: to the ground, or to the
        // lowest height when the ground is below that
        auto fall_to_ground = [rec, cam, motes, at](int32_t kind, uint32_t v) {
            swc1(OV(0x00160260), fpu_sub(lwc1(cam + 8), lwc1(rec + 0x1c)));
            const float ground = ground_height_at(V(at));
            if (!(ground < fpu_add(lwc1(cam + 8), lwc1(rec + 4)))) return;
            const float lowest = lwc1(OV(0x00160260));
            const float floor = ground < lowest ? lowest : ground;
            if (motes == 0) class1400_drop(at, kind, v, floor);
            if (motes == 1) class1400_mote(at, kind, v, floor);
        };
        for (int32_t k = 0; k < count; ++k) {
            lq_sq(at, ahead);
            scatter(at, lwc1(rec + 8));
            vec3_sub(V(off), V(at), V(cam));
            const float dist = vec2_length(V(off));
            float speed = fall;
            if (!(dist <= 8.0f)) {
                speed = fpu_sub(fall, fpu_mul(fpu_sub(dist, 8.0f), far_k));
                if (speed < slowest) speed = slowest;
            }
            swc1(vel, fpu_add(lwc1(rec + 0x20), rand_range_f(-spread, spread)));
            const float wy = rand_range_f(-spread, spread);
            swc1(vel + 0xc, 0.0f);
            swc1(vel + 4, fpu_add(lwc1(rec + 0x24), wy));
            swc1(vel + 8, -fpu_mul(speed, lwc1(0x0015ed6c)));
            fall_to_ground(8.0f < dist ? lw(rec + 0x18) : 0, vel);
        }
        // the near ones: 1.4 times as many, about the camera, falling at the full speed
        const int32_t near = trunc_w(fpu_mul(static_cast<float>(count), u2f(0x3fb33333u)));  // (1.4)
        lq_sq(ahead, cam);
        swc1(ahead + 8, fpu_add(lwc1(cam + 8), lwc1(rec + 4)));
        for (int32_t k = 0; k < near; ++k) {
            swc1(off, fpu_add(lwc1(rec + 0x20), rand_range_f(-spread, spread)));
            const float wy = rand_range_f(-spread, spread);
            lq_sq(at, ahead);
            sw(off + 0xc, 0);
            swc1(off + 4, fpu_add(lwc1(rec + 0x24), wy));
            swc1(off + 8, -fpu_mul(fall, lwc1(0x0015ed6c)));
            scatter(at, 8.0f);
            fall_to_ground(0, off);
        }
        if (splashes) {
            lq_sq(ahead, cam);
            sub_00262cf0(ahead, ahead, 30.0f, lwc1(cam + 0x18), lwc1(cam + 0x14));
            swc1(ahead + 8, fpu_add(lwc1(cam + 8), lwc1(rec + 4)));
            for (int32_t k = 0; k < near; ++k) {
                lq_sq(at, ahead);
                scatter(at, 30.0f);
                ground_puff_at(at);
            }
            lq_sq(ahead, cam);
            sub_00262cf0(ahead, ahead, 4.0f, lwc1(cam + 0x18), lwc1(cam + 0x14));
            swc1(ahead + 8, fpu_add(lwc1(cam + 8), lwc1(rec + 4)));
            for (int32_t k = 0; k < near / 2; ++k) {
                lq_sq(at, ahead);
                scatter(at, 8.0f);
                ground_puff_at(at);
            }
        }
    }
    lq_sq(pv + 0x30, cam);
    ee_stack_free(0x150);
}

}  // namespace rac1
