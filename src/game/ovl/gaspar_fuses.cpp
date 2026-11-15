// Gaspar's own moby classes: the charges that set off the structures (classes 1172 and 1181) — hand-ported from Gaspar's
// own overlay. Header gaspar_mobys.h, registered in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`); Gaspar-only data
// is L9_ADDR, the base ELF's are bare (the story bytes 0x13d388.., the frame time's square 0x15ed70, the sound slots 0x13e550).
//
// The structures' parts they set off are the classes 1172 (0x494), 1181 (0x49d) and the structure's base 1184 (0x4a0), named
// by their index in the level's moby table (W[0x15ffd8], 0x100 bytes each); set off: their +0xbc 1 (and drawn: +0x30 0xff).
//   class 1172 a charge with hit points (pvars +0x20 its hit state, +0x60 its flash): hit to 0 it sets off its link (+0x70)
//              and drops its bolts; set off itself (+0xbc 1) it hums (+0x7c), sets off +0x78 and bursts 10 frames later
//   class 1181 a charge hung between two parts (+0x60 the one it hangs from, +0x64 the next; found at the set-up): it sways
//              toward the first, falls when the next fell; set off it sets off +0x6c, drops and bursts 10 frames later
#include <game/math/vecmath.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// the moby of index `i` of the level's moby table (-1: none)
uint32_t moby_at(int32_t i) { return lwu(L9_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// a moby still there (not 0, not being removed: +0x20 0xfe / 0xfd)
bool alive(uint32_t o) { return o != 0 && lbu(o + 0x20) != 0xfe && lbu(o + 0x20) != 0xfd; }

// one of the structures' parts (classes 1172, 1181, 1184)
bool part(uint32_t o) {
    const int32_t c = lh(o + 0xa6);
    return c == 0x494 || c == 0x49d || c == 0x4a0;
}

// the part of index `i` set off (and drawn)
void set_off(int32_t i) {
    if (i == -1) return;
    const uint32_t o = moby_at(i);
    if (!alive(o) || !part(o)) return;
    sb(o + 0xbc, 1);
    sb(o + 0x30, 0xff);
}

// the moby of index `i` set off when it waits (its state 1)
void pass_on(int32_t i) {
    if (i == -1) return;
    const uint32_t o = moby_at(i);
    if (lbu(o + 0x20) == 1) sb(o + 0xbc, 1);
}

// the sound in the slot +`at` of the pvars stopped when it is still the moby's and playing; the slot -1
void sound_off(uint32_t m, uint32_t slot_at) {
    const int32_t slot = lw(slot_at);
    if (slot != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(slot_at, 0xffffffffu);
}

// the burst that ends a charge, away from the camera
void burst(uint32_t m, int32_t n, int32_t a, int32_t b, float f14, float f15, float f16, float f17, float f18) {
    const uint32_t sp = ee_stack_alloc(0x10);
    vec3_sub(V(sp), V(L9_ADDR(0x00166f40)), V(m + 0x10));
    explosion_effect(m, sp, 0, n, a, b, 0, 0, 1, -1, 0, 0.0f, 0.0f, f14, f15, f16, f17, f18);
    ee_stack_free(0x10);
    moby_retire(m);
}

// the charge's turn toward the part `to` by `rate`: its pitch from the height over the distance across, its heading
void turn_to(uint32_t m, uint32_t to, float across, float rate) {
    const float pitch = -atan2_f(across, fpu_sub(lwc1(to + 0x18), lwc1(m + 0x18)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), fpu_mul(angle_sub(pitch, lwc1(m + 0x44)), rate)));
    const float heading = atan2_f(fpu_sub(lwc1(to + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(to + 0x14), lwc1(m + 0x14)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(angle_sub(heading, lwc1(m + 0x48)), rate)));
}

// `v` (a vector from the charge to a part) made the pull of that part: its length less 1.3, times `k`
void pull(Vec4& v, float k) {
    const float len = fpu_sub(vec3_length(v), 1.3f);
    vec3_set_length(v, v, k == 1.0f ? len : fpu_mul(len, k));
}
}  // namespace

// Class 1172's hits: while not bursting (state 3), a hit (masks 0x330000) as its hit state reads it (results 1 and 2: its
// hit points +0 cleared); a hit that counts (state 2 on) takes the damage from its hit points: to 0 it loses its flag 0x1000,
// sets off its link (+0x70), drops its bolts, flashes and bursts (state 3), else it flashes for 60 frames. Its flash runs.
// orig: L9:0x00303b30
void gas_303b30_l9(uint32_t m, uint32_t pv, uint32_t hs) {
    if (lbu(m + 0x20) != 3) {
        const uint32_t hit = sub_0025a420(m, 0x330000u, 0);
        // (with a hit the original prints a debug note: its class and damage; nothing in the game)
        int32_t state = 0;
        const int32_t r = moby_hit_react_r(m, hit, hs, 0, state, nullptr, nullptr, 4);
        if (r == 1 || r == 2) sw(hs, 0);  // (the jump table 0x208fc0: 0x303bec for 1 and 2, else 0x303bf0)
        if (!(state < 2)) {
            const float dmg = lwc1(hit + 0x2c);
            const float hp = lwc1(hs);
            if (hp <= dmg) {
                sw(hs, 0);
                sh(m + 0x34, lhu(m + 0x34) & 0xefff);
                const int32_t link = lw(pv + 0x70);
                if (link != -1) {
                    const uint32_t o = moby_at(link);
                    if (alive(o) && part(o)) sb(o + 0xbc, 1);
                }
                moby_drop_bolts(m, 0, -1);
                sb(pv + 0x67, 0x78);
                color_pulse_start(m, pv + 0x60);
                sb(m + 0x20, 3);
            } else {
                swc1(hs, fpu_sub(hp, dmg));
                sb(pv + 0x67, 0xfa);
                sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(60)));
                color_pulse_start(m, pv + 0x60);
            }
        }
        sb(m + 0xa4, 0xff);
    }
    color_pulse_update(m, pv + 0x60);
}

// Class 1172. Its hits; 0: gone when its story byte (pvars +0x76) is set, else waiting (1). 1: every eighth frame (by its
// slot) its hum kept on; set off (+0xbc 1): it sets off +0x78 (when that one waits), stops its hum and bursts in 10 frames (2).
// 2: then sets off its link +0x70 (3). 3: the burst; gone.
// orig: L9:0x00303d10
void gas_class1172_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    gas_303b30_l9(m, pv, pv + 0x20);
    switch (lbu(m + 0x20)) {
        case 0: {
            const int32_t story = lh(pv + 0x76);
            if (story != -1 && lbu(0x0013d388 + static_cast<uint32_t>(story) + 0x39) != 0) {
                moby_retire(m);
                return;
            }
            sw(pv + 0x7c, 0xffffffffu);
            sb(m + 0x20, 1);
            return;
        }
        case 1: {
            if ((lwu(L9_ADDR(0x0015f5cc)) & 7) == ((m >> 8) & 7)) {
                const int32_t slot = lw(pv + 0x7c);
                if (slot == -1 || (sub_0028d8c0(m, slot) == 0 && lw(pv + 0x7c) == -1))
                    sw(pv + 0x7c, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
            }
            if (lbu(m + 0xbc) != 1) return;
            pass_on(lw(pv + 0x78));
            sh(pv + 0x74, static_cast<uint32_t>(frame_scale_i(10)));
            sound_off(m, pv + 0x7c);
            sb(m + 0x20, 2);
            sb(m + 0x30, 0xff);
            return;
        }
        case 2:
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x74))) == 0) return;
            set_off(lw(pv + 0x70));
            sb(m + 0x20, 3);
            return;
        case 3:
            burst(m, 0x14, 9, 0x20, 10.0f, 7.0f, 20.0f, 2.0f, 40.0f);
            return;
        default:
            return;
    }
}

// The part nearest `at` (an EE vector; with `joint` not -1 the distance to that joint of each) among the level's mobys (the
// list 0x15ffe4, chained by +0x28) of the classes 1172, 1181 and 1184, `m` left out: within 0.5, or a base within 10; else 0.
// orig: L9:0x003041d0
uint32_t gas_3041d0_l9(uint32_t m, uint32_t at, int32_t joint) {
    const Vec4 from = V(at);
    float best = u2f(0x4e6e6b28u);
    uint32_t found = 0;
    for (uint32_t o = lwu(L9_ADDR(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
        if (o == m) continue;
        if (lbu(o + 0x20) == 0xfe || lbu(o + 0x20) == 0xfd || !part(o)) continue;
        float d;
        if (joint == -1) {
            d = vec3_distance(from, V(o + 0x10));
        } else {
            Vec4 p;
            moby_chain_point_v(o, static_cast<uint32_t>(joint), p);
            d = vec3_distance(from, p);
        }
        if (d < best) {
            best = d;
            found = o;
        }
    }
    if (found == 0) return 0;
    if (best < 0.5f) return found;
    if (lh(found + 0xa6) != 0x4a0) return 0;
    return best < 10.0f ? found : 0;
}

// Class 1181. 0: gone when its story byte (pvars +0x69) is set; the part it hangs from (+0x60: the nearest to its joint 0)
// and the next (+0x64: the nearest to its place, by their joint 0), then waiting (1). 1: near the camera (or not drawn) or on
// its eighth frame: while the next part hangs on (its +0x68) it sways toward the first, pulled by both (it falls when the
// next is gone); set off (+0xbc 1): it sets off +0x6c (when that one waits) and drops (2). 2: it falls toward the first, then
// sets off both (3). 3: the burst; gone.
// orig: L9:0x00304360
void gas_class1181_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
        case 0: {
            const int32_t story = static_cast<int8_t>(lbu(pv + 0x69));
            if (story != -1 && lbu(0x0013d388 + static_cast<uint32_t>(story) + 0x39) != 0) {
                moby_retire(m);
                return;
            }
            const uint32_t sp = ee_stack_alloc(0x10);
            if (lw(pv + 0x60) == -1) {
                moby_chain_point_v(m, 0, V(sp));
                const uint32_t o = gas_3041d0_l9(m, sp, -1);
                // (none: the original prints a debug note)
                if (o != 0) sw(pv + 0x60, static_cast<uint32_t>(static_cast<int32_t>(o - lwu(L9_ADDR(0x0015ffd8))) >> 8));
            }
            if (lw(pv + 0x64) == -1) {
                lq_sq(sp, m + 0x10);
                const uint32_t o = gas_3041d0_l9(m, sp, 0);
                if (o != 0) sw(pv + 0x64, static_cast<uint32_t>(static_cast<int32_t>(o - lwu(L9_ADDR(0x0015ffd8))) >> 8));
            }
            ee_stack_free(0x10);
            sb(m + 0x20, 1);
            return;
        }
        case 1: {
            if (lbu(m + 0x31) == 0 || 32.0f < vec3_distance(V(m + 0x10), V(L9_ADDR(0x00166f40)))) {
                const int32_t slot = lh(m + 0xb2);
                if (ee_rem(lw(L9_ADDR(0x0015f5cc)), 8) != static_cast<int16_t>(ee_rem(slot, 8))) return;
            }
            uint32_t from = 0, next = 0;
            if (lw(pv + 0x60) != -1) {
                from = moby_at(lw(pv + 0x60));
                if (!alive(from)) from = 0;
            }
            if (lw(pv + 0x64) != -1) {
                next = moby_at(lw(pv + 0x64));
                if (!alive(next)) {
                    sb(pv + 0x68, 0);
                    next = 0;
                } else if (lbu(lwu(next + 0x78) + 0x68) == 0) {
                    sb(pv + 0x68, 0);
                }
            }
            if (lbu(pv + 0x68) != 0 || from == 0 || next == 0 || lh(next + 0xa6) != 0x49d) {
                sb(pv + 0x68, 1);
            } else if (lbu(lwu(next + 0x78) + 0x68) != 0) {  // hanging: swaying toward the first
                turn_to(m, from, vec2_distance(V(m + 0x10), V(from + 0x10)), u2f(0x3dcccccdu));
            } else {  // the next one fell: pulled by both, falling
                const uint32_t pos = m + 0x10;
                Vec4 a, sum;
                vec3_sub(a, V(from + 0x10), V(pos));
                pull(a, u2f(0x3f333333u));
                const uint32_t n = next;
                if (lh(n + 0xa6) == 0x49d && lbu(n + 0x20) != 0xfe && lbu(n + 0x20) != 0xfd) {
                    Vec4 b;
                    vec3_sub(b, V(n + 0x10), V(pos));
                    pull(b, u2f(0x3f333333u));
                    vec3_add(sum, a, b);
                } else {
                    sum = a;
                }
                sum.z = fpu_sub(sum.z, fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu)));
                vec3_add(V(pos), V(pos), sum);
                turn_to(m, from, vec2_distance(V(pos), V(from + 0x10)), u2f(0x3dcccccdu));
            }
            if (lbu(m + 0xbc) != 1) return;
            pass_on(lw(pv + 0x6c));
            sh(pv + 0x6a, static_cast<uint32_t>(frame_scale_i(10)));
            sb(m + 0x20, 2);
            sb(m + 0x30, 0xff);
            return;
        }
        case 2: {
            sb(m + 0x30, 0xff);
            uint32_t from = 0;
            if (lw(pv + 0x60) != -1) {
                from = moby_at(lw(pv + 0x60));
                if (!alive(from)) from = 0;
            }
            if (from != 0) {  // falling toward it, faster each frame
                const uint32_t pos = m + 0x10;
                Vec4 a;
                vec3_sub(a, V(from + 0x10), V(pos));
                pull(a, 1.0f);
                const int32_t n = frame_scale_i(10) - (lh(pv + 0x6a) - 3);
                const float g = fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu));
                a.z = fpu_sub(a.z, fpu_mul(fpu_add(g, g), int_to_float(n)));
                vec3_add(V(pos), V(pos), a);
                turn_to(m, from, vec2_distance(V(pos), V(from + 0x10)), 0.5f);
            }
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x6a))) == 0) return;
            set_off(lw(pv + 0x60));
            set_off(lw(pv + 0x64));
            sb(m + 0x20, 3);
            return;
        }
        case 3:
            burst(m, 0xa, 5, 8, 2.0f, 1.0f, 10.0f, 1.0f, 10.0f);
            return;
        default:
            return;
    }
}

}  // namespace rac1
