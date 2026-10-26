// Ported overlay code: the effect light (class 0x27f, spawned by 0x2d3838): a point light at a
// moby or a spot, whose radius and red / green / blue intensities rise, hold and fall (or
// flicker) by the description copied into its pvars, kept in one of the 8 dynamic light slots
// (0x1802c0: colour, intensity, position, radius; 0x1804c0: the slot's use) while it is on
// screen.
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

#include <cmath>

namespace rac1 {

// A dynamic light at `pos`: radius `radius`, intensity `k`, colour `r` / `g` / `b`, in the
// first free one of the 8 slots; -1 when none is free or the frame is loaded (0x15f5d4 > 0.8).
// orig: 0x0023e738
int32_t light_add(uint32_t pos, float radius, float k, float r, float g, float b) {
    if (u2f(0x3f4ccccdu) < lwc1(OV(0x0015f5d4))) return -1;
    int32_t i = 0;
    while (i < 8 && lw(OV(0x001804c0) + static_cast<uint32_t>(i) * 0x30 + 0x10) != 0) ++i;
    if (i == 8) return -1;
    const uint32_t rec = OV(0x001802c0) + static_cast<uint32_t>(i) * 0x20;
    const uint32_t use = OV(0x001804c0) + static_cast<uint32_t>(i) * 0x30;
    V(rec + 0x10) = V(pos);
    swc1(rec + 0xc, k);
    swc1(rec + 0x1c, radius);
    swc1(rec, r);
    swc1(rec + 4, g);
    swc1(rec + 8, b);
    mem_clear128(use, 0x30);
    sw(use + 0x10, 1);
    sw(use + 0xc, OV(0x00180640) + static_cast<uint32_t>(i) * 0x400);
    return i;
}

namespace {
int16_t& h16(uint32_t a) { return ee_ref<int16_t>(a); }
int32_t& w32ref(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }
float pct(uint32_t a) { return fpu_div(int_to_float(lbu(a)), 100.0f); }

struct Channel {
    uint32_t val, per, tick, reset_byte, gate, hold, fall, stop_fall, cycle, bytes, hold_t, hc;
};
constexpr Channel kRed = {0x54, 0x6c, 0x74, 0x27, 0x10000, 0x20, 0x80, 0x40, 0x10, 0x26, 0x68, 0x7c};
constexpr Channel kGreen = {0x58, 0x6e, 0x76, 0x2c, 0x20000, 0x200, 0x800, 0x400, 0x100, 0x2b, 0x69, 0x7d};
constexpr Channel kBlue = {0x5c, 0x70, 0x78, 0x31, 0x40000, 0x2000, 0x8000, 0x4000, 0x1000, 0x30, 0x6a, 0x7e};
// orig: (helper) repeated in 0x002d3a10 — one colour channel's cycle: its value at pv + `val`;
// the reset period `per` and its timer `tick` (the value back to the byte `reset_byte`); the
// flag bits `gate` (random instead), `hold`, `fall`, `stop_fall` (stop after falling) and
// `cycle` (hold, then fall again); the bytes from `bytes` (rise-to, fall-to, rise step, fall
// step, in hundredths); the hold time `hold_t` and its countdown `hc`.
void channel_update(uint32_t pv, const Channel& c) {
    if (lhu(pv + c.per) != 0 && timer_tick_i16(h16(pv + c.tick)) != 0) {
        sh(pv + c.tick, static_cast<uint32_t>(frame_scale_i(lhu(pv + c.per))));
        sw(pv + 0x4c, lwu(pv + 0x4c) & ~c.fall);
        swc1(pv + c.val, static_cast<float>(lbu(pv + c.reset_byte)));
    }
    const uint32_t fl = lwu(pv + 0x4c);
    if ((fl & c.gate) == 0) {
        if ((fl & (c.hold | c.cycle)) == c.hold) sw(pv + 0x4c, fl ^ c.hold);
        const uint32_t v = lwu(pv + 0x4c);
        if ((v & c.hold) != 0) {
            if (lbu(pv + c.hold_t) != 0 && countdown_u8(pv + c.hc) != 0) sw(pv + 0x4c, lwu(pv + 0x4c) ^ c.hold);
        } else if ((v & c.fall) != 0) {
            const float to = pct(pv + c.bytes + 1);
            const float k = lwc1(pv + c.val) - pct(pv + c.bytes + 3) * lwc1(0x0015ed60);
            swc1(pv + c.val, k);
            if (k <= to) {
                swc1(pv + c.val, to);
                if ((v & c.stop_fall) != 0 && lhu(pv + c.per) == 0) sw(pv + 0x4c, v ^ c.fall);
            }
        } else {
            const float to = pct(pv + c.bytes);
            const float k = lwc1(pv + c.val) + pct(pv + c.bytes + 2) * lwc1(0x0015ed60);
            swc1(pv + c.val, k);
            if (to <= k) {
                swc1(pv + c.val, to);
                if ((v & c.cycle) == 0) {
                    sw(pv + 0x4c, v ^ c.fall);
                } else {
                    sb(pv + c.hc, static_cast<uint32_t>(frame_scale_i(lbu(pv + c.hold_t))));
                    sw(pv + 0x4c, (lwu(pv + 0x4c) | c.hold) ^ c.fall);
                }
            }
        }
    } else if (lhu(pv + c.per) == 0) {
        const float a = fpu_div(static_cast<float>(float_to_int(static_cast<float>(lbu(pv + c.bytes + 1)))), 100.0f);
        const float b = fpu_div(static_cast<float>(float_to_int(static_cast<float>(lbu(pv + c.bytes)))), 100.0f);
        swc1(pv + c.val, rand_range_f(a, b));
    }
}
}  // namespace

// orig: (none) — registers the effect light's update for the moby update calls.
void effect_light_fns_register() { ovl_register_moby_fn(0x002d3a10, effect_light_update); }

// The effect light's update: set up from its description on the first frame (after the delay
// +0xbc), retired when its life +0x48 runs out; it follows its moby (+0x20, the offset +0 turned
// by the moby's matrix with flag 0x400000, jittered with 0x100000) or stays put (0x800000);
// its radius +0x60 rises / holds / falls between +0x3c and +0x38 (or is random), as do its
// colour channels; its light slot is taken while on screen and given back when not.
// orig: 0x002d3a10
void effect_light_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    int32_t reset = 0;
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x30, 0xff);
        const uint32_t delay = lbu(pv + 0x1c);
        sb(m + 0xbc, delay);
        V(pv + 0x10) = V(m + 0x10);
        const uint32_t a = lhu(pv + 0x6e), fl = lwu(pv + 0x4c), r = lhu(pv + 0x6c), b = lhu(pv + 0x70), rad = lhu(pv + 0x72);
        sw(pv + 0x50, 0xffffffffu);
        sh(pv + 0x76, a);
        sb(pv + 0x1c, delay);
        sw(pv + 0xc, fl);
        sh(pv + 0x74, r);
        sh(pv + 0x78, b);
        sh(pv + 0x7a, rad);
        swc1(pv + 0x54, fpu_div(int_to_float(lbu(pv + 0x25)), 100.0f));
        swc1(pv + 0x58, fpu_div(int_to_float(lbu(pv + 0x2a)), 100.0f));
        swc1(pv + 0x5c, fpu_div(int_to_float(lbu(pv + 0x2f)), 100.0f));
        swc1(pv + 0x60, lwc1(pv + 0x34));
        if (lw(pv + 0x64) != -1) sw(pv + 0x20, lwu(OV(0x0015ffd8)) + (lwu(pv + 0x64) << 8));
        if (lwc1(pv + 0x40) < 0.0f) swc1(pv + 0x40, -lwc1(pv + 0x40));
        if (lwc1(pv + 0x44) < 0.0f) swc1(pv + 0x44, -lwc1(pv + 0x44));
        sb(m + 0x20, 1);
    }
    if (lbu(m + 0xbc) != 0) {
        sb(m + 0xbc, lbu(m + 0xbc) - 1u);
        return;
    }
    if (lw(pv + 0x48) != -1 && timer_tick_i32(w32ref(pv + 0x48)) != 0) {
        if (lw(pv + 0x50) != -1) {
            region_clear(lwu(pv + 0x50));
            sw(pv + 0x50, 0xffffffffu);
        }
        moby_retire(m);
        return;
    }
    const uint32_t f = ee_stack_alloc(0x30);  // the original's frame: the test point, the anchor, the offset
    auto done = [&] { ee_stack_free(0x30); };
    V(f) = V(m + 0x10);
    swc1(f + 0xc, lwc1(pv + 0x60));
    if (point_on_screen(f, 255.0f) == -1) {
        if (lw(pv + 0x50) != -1) {
            region_clear(lwu(pv + 0x50));
            sw(pv + 0x50, 0xffffffffu);
        }
        done();
        return;
    }
    const uint32_t at = f + 0x10, off = f + 0x20;
    if (lwu(pv + 0x20) != 0) V(at) = V(lwu(pv + 0x20) + 0x10);
    else V(at) = V(pv + 0x10);
    const uint32_t fl = lwu(pv + 0x4c);
    if ((fl & 0x800000u) == 0) {
        if ((fl & 0x100000u) != 0) {
            if ((fl & NOT_ADDR(0x200000u)) != 0) {
                swc1(off, rand_range_f(-lwc1(pv), lwc1(pv)));
                swc1(off + 4, rand_range_f(-lwc1(pv + 4), lwc1(pv + 4)));
                swc1(off + 8, rand_range_f(-lwc1(pv + 8), lwc1(pv + 8)));
            } else {
                const uint32_t keep = lwu(pv + 0xc);
                sw(pv + 0xc, 0);
                vec3_scale(V(off), V(pv), rand_range_f(lwc1(pv + 0xc), 1.0f));
                sw(pv + 0xc, keep);
            }
        } else {
            const uint32_t keep = lwu(pv + 0xc);
            sw(pv + 0xc, 0);
            V(off) = V(pv);
            sw(pv + 0xc, keep);
        }
        if ((lwu(pv + 0x4c) & 0x400000u) != 0 && lwu(pv + 0x20) != 0) {
            mat3_transform(V(off), V(off), *static_cast<const Mat4*>(ee_ram_ptr(lwu(pv + 0x20) + 0xc0)));
        }
        vec3_add(V(m + 0x10), V(at), V(off));
    } else {
        V(m + 0x10) = V(at);
    }
    // the radius
    if (lhu(pv + 0x72) != 0 && timer_tick_i16(h16(pv + 0x7a)) != 0) {
        sh(pv + 0x7a, static_cast<uint32_t>(frame_scale_i(lhu(pv + 0x72))));
        const uint32_t v = lwu(pv + 0x4c);
        if ((v & 4) != 0) {
            reset = 1;
            swc1(pv + 0x60, lwc1(pv + 0x3c));
            sw(pv + 0x4c, v & 0xfffffff7u);
        }
    }
    {
        const uint32_t a = lwu(pv + 0x4c);
        if ((a & 0x80000u) == 0) {
            if ((a & 3) == 2) sw(pv + 0x4c, a ^ 2u);
            const uint32_t v = lwu(pv + 0x4c);
            if ((v & 2) != 0) {
                if (lbu(pv + 0x6b) != 0 && countdown_u8(pv + 0x7f) != 0) sw(pv + 0x4c, lwu(pv + 0x4c) ^ 2u);
            } else if ((v & 8) != 0) {
                const float r = lwc1(pv + 0x60) - lwc1(pv + 0x44) * lwc1(0x0015ed60);
                swc1(pv + 0x60, r);
                if (r <= lwc1(pv + 0x3c)) {
                    swc1(pv + 0x60, lwc1(pv + 0x3c));
                    if ((v & 4) != 0 && frame_scale_i(lhu(pv + 0x72)) == 0) sw(pv + 0x4c, lwu(pv + 0x4c) ^ 8u);
                }
            } else {
                const float r = lwc1(pv + 0x60) + lwc1(pv + 0x40) * lwc1(0x0015ed60);
                swc1(pv + 0x60, r);
                if (lwc1(pv + 0x38) <= r) {
                    swc1(pv + 0x60, lwc1(pv + 0x38));
                    if ((v & 1) == 0) {
                        sw(pv + 0x4c, v ^ 8u);
                    } else {
                        sb(pv + 0x7f, static_cast<uint32_t>(frame_scale_i(lbu(pv + 0x6b))));
                        sw(pv + 0x4c, (lwu(pv + 0x4c) | 2u) ^ 8u);
                    }
                }
            }
        } else if (lhu(pv + 0x72) == 0 || reset == 1) {
            swc1(pv + 0x60, rand_range_f(lwc1(pv + 0x3c), lwc1(pv + 0x38)));
        }
    }
    channel_update(pv, kRed);
    channel_update(pv, kGreen);
    channel_update(pv, kBlue);
    // the light slot
    const int32_t slot = lw(pv + 0x50);
    if (slot == -1) {
        const int32_t s = light_add(m + 0x10, lwc1(pv + 0x60), fpu_div(int_to_float(lbu(pv + 0x24)), 100.0f), lwc1(pv + 0x54), lwc1(pv + 0x58), lwc1(pv + 0x5c));
        sw(pv + 0x50, static_cast<uint32_t>(s));
    } else {
        const uint32_t rec = OV(0x001802c0) + static_cast<uint32_t>(slot) * 0x20;
        V(rec + 0x10) = V(m + 0x10);
        swc1(rec + 0x1c, lwc1(pv + 0x60));
        swc1(rec, lwc1(pv + 0x54));
        swc1(rec + 4, lwc1(pv + 0x58));
        swc1(rec + 8, lwc1(pv + 0x5c));
        swc1(rec + 0xc, fpu_div(int_to_float(lbu(pv + 0x24)), 100.0f));
    }
    done();
}

}  // namespace rac1

namespace rac1 {

// Do the spheres at `a` and `b` (centre xyz, radius w) overlap: |a - b|^2 - (ra + rb)^2 < 0
// (VU0 macro, the sign of the result taken).
// orig: 0x001ff4b8
int32_t spheres_overlap(uint32_t a, uint32_t b) {
    const Vec4 p = V(a), q = V(b);
    const float dx = vuc(p.x - q.x), dy = vuc(p.y - q.y), dz = vuc(p.z - q.z), r = vuc(p.w + q.w);
    const float sx = vuc(dx * dx), sy = vuc(dy * dy), sz = vuc(dz * dz), r2 = vuc(r * r);
    float acc = vuc(sy + sx);
    acc = vuc(acc + vuc(1.0f * sz));
    const float res = vuc(acc - vuc(1.0f * r2));
    return std::signbit(res) ? 1 : 0;
}

namespace {
// orig: (helper) repeated in 0x0023ea10 — light `i` added to an object's four light nibbles
// at `h` (0xf: free): 0 when all four are taken.
bool light_nibble_add(uint32_t h, uint32_t i) {
    const uint32_t v = lhu(h);
    if (v == 0xffffu) sh(h, 0xfff0u | i);
    else if ((v & 0xfff0u) == 0xfff0u) sh(h, (v & 0xff0fu) | (i << 4));
    else if ((v & 0xff00u) == 0xff00u) sh(h, (v & 0xf0ffu) | (i << 8));
    else if ((v & 0xf000u) == 0xf000u) sh(h, (v & 0x0fffu) | (i << 12));
    else return false;
    return true;
}
}  // namespace

// Light `i` handed to what it reaches (its sphere grown by 8): the tfrag chunks (0x20 each from
// *(0x160fc0)), the mobys' spheres (0x40 each from *(0x160f4c), in 1/1024 units) and the shrubs
// (0x20 each from *(0x160494)), each taking it in a free light nibble (+0x1e / +0x36) and listed
// in the slot's 0x400-byte list; the slot's counts: +2 tfrag, +8 / +0xa the mobys, +4 / +6 shrubs.
// orig: 0x0023ea10
void light_assign(uint32_t i) {
    const uint32_t use = OV(0x001804c0) + i * 0x30;
    uint32_t list = lwu(use + 0xc);
    const uint32_t end = list + 0x400;
    Vec4 s = V(OV(0x001802c0) + i * 0x20 + 0x10);
    s.w = s.w + 8.0f;
    if (!(list < end)) return;
    const uint32_t sp = ee_stack_alloc(0x10);  // the sphere goes by address
    V(sp) = s;
    bool room = true;
    sh(use, 0);
    uint32_t n = 0;
    for (uint32_t o = lwu(OV(0x00160fc0)); o != lwu(OV(0x00160fc4)); o += 0x20, ++n) {
        if (spheres_overlap(sp, o) == 0) continue;
        sh(list, n);
        list += 2;
        if (!light_nibble_add(o + 0x1e, i)) list -= 2;
        if (!(list < end)) break;
    }
    room = list < end;
    const uint32_t tf = static_cast<uint32_t>(static_cast<int32_t>(list - lwu(use + 0xc)) >> 1);
    sh(use + 2, tf);
    if (!room) {
        ee_stack_free(0x10);
        return;
    }
    sh(use + 8, tf);
    vec4_scale(V(sp), V(sp), 1024.0f);
    n = 0;
    uint32_t o = lwu(OV(0x00160f4c));
    for (; static_cast<int32_t>(n) < lw(OV(0x00160f50)); ++n, o += 0x40) {
        if (spheres_overlap(sp, o) == 0) continue;
        sh(list, n);
        list += 2;
        if (!light_nibble_add(o + 0x36, i)) list -= 2;
        room = list < end;
        if (!room) break;
    }
    const uint32_t mb = static_cast<uint32_t>(static_cast<int32_t>(list - lwu(use + 0xc)) >> 1);
    sh(use + 0xa, mb - lhu(use + 8));
    if (!room) {
        ee_stack_free(0x10);
        return;
    }
    sh(use + 4, mb);
    n = 0;
    for (uint32_t r = lwu(OV(0x00160494)); r != lwu(OV(0x00160498)); r += 0x20, ++n) {
        if (spheres_overlap(sp, r) == 0) continue;
        sh(list, n);
        list += 2;
        if (!light_nibble_add(r + 0x1e, i)) list -= 2;
        if (!(list < end)) break;
    }
    sh(use + 6, static_cast<uint32_t>(static_cast<int32_t>(list - lwu(use + 0xc)) >> 1) - lhu(use + 4));
    ee_stack_free(0x10);
}

// Light `i` handed out again: its old holders released first.
// orig: 0x0023edc0
void light_reassign(uint32_t i) {
    region_release(i);
    light_assign(i);
}

// The level's lights each frame: the directional light set by the camera's yaw (0x180200:
// colour 0.8, direction 30 degrees down, turned 0.8 from the view), and the dynamic lights
// handed out again when they have moved more than 8 since (state 1: the first time).
// orig: 0x0023e890
void level_lights_update() {
    constexpr OvConst<0x0017fec0u> L{};
    swc1(L + 0x34c, u2f(0xbe99999au));
    swc1(L + 0x348, u2f(0x3f4ccccdu));
    swc1(L + 0x340, u2f(0x3f4ccccdu));
    swc1(L + 0x344, u2f(0x3f4ccccdu));
    const float a = angle_add(lwc1(OV(0x00166dd8)), u2f(0xbf4ccccdu));
    swc1(L + 0x350, cos_vu(a) * u2f(0x3f5db22du));
    swc1(L + 0x354, sin_vu(a) * u2f(0x3f5db22du));
    swc1(L + 0x358, -0.5f);
    sw(L + 0x35c, 0);
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t rec = OV(0x001802c0) + i * 0x20 + 0x10, use = OV(0x001804c0) + i * 0x30;
        if (lw(use + 0x10) == 0) continue;
        if (!(8.0f < vec3_distance(V(rec), V(use + 0x20)))) continue;
        V(use + 0x20) = V(rec);
        const int32_t st = lw(use + 0x10);
        if (st == 1) {
            light_assign(i);
            sw(use + 0x10, 2);
        } else if (st == 2) {
            light_reassign(i);
        }
    }
}

}  // namespace rac1
