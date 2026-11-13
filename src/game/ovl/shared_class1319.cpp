// Class 1319's (Pokitaru) / 1843's (Drek's fleet) trails, path pose and hit reaction: one port of the copies the two levels
// carry (the same code relinked). Each level keeps the trails' length, the strips' texture coordinates and its copy of the draw callback at
// addresses of its own (per level below); the rest is the engine's (OV: the frame counter 0x15f5cc, the moby table
// 0x15ffd8, the group lists 0x1ab840, the paths 0x1b04b0) or the base ELF's (bare: the game's records 0x157150.., the
// play time 0x15eea4, the level 0x15ed84, the counter 0x15ee00 and flag 0x13d41a of the reward, the hero's state
// 0x1413d4, the frame time 0x15ed6c and its square, the wide view flag 0x15edb2). Header shared_class1319.h.
//
// The pvars: +0x60 / +0x64 / +0x68 the path, its point and the way to the next, +0x6c turned round, +0x70 / +0x78 the
// swing's angle and reach, +0x80 the speed to the pose, +0x88 a hit sends it to state 6 (else 2); +0xc0 / +0x1c0 the two
// trails' 16 points, +0x2c0 the points recorded, +0x2c4 the head, +0x2c8 the group's word of the frame the draw was queued.
#include <game/ovl/shared_class1319.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// the trails' length (a float the draw sets: 5, 16 in the wide view)
uint32_t trail_length() {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x001623ec);
    return L11_ADDR(0x001623f0);
}
// the four corners' texture coordinates (s, t)
uint32_t corner_uvs() {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x001de0f0);
    return L11_ADDR(0x001f1490);
}
// the level's copy of the draw callback (queued by the record: the levels still register their copies)
uint32_t draw_callback() {
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x002f3b68);
    return L11_ADDR(0x003192c8);
}

constexpr int32_t TRAIL_POINTS = 16;
constexpr uint32_t QUAD = 0x90;  // (a quad's descriptor)

// a signed remainder of 16 (the original's division by 16 rounding to zero)
int32_t mod16(int32_t v) { return v - ((v < 0 ? v + 15 : v) >> 4) * 16; }

// a wing's point (`side` -0.7 / 0.7 across, 1.2 behind, 0.3 up, turned by the moby's matrix) at the trail's head
void wing_point(uint32_t m, uint32_t pv, uint32_t tmp, float side, uint32_t trail) {
    V(tmp) = Vec4{};
    swc1(tmp + 4, side);
    swc1(tmp, u2f(0xbf99999au));  // (-1.2)
    swc1(tmp + 8, 0.3f);
    mat3_transform(V(tmp), V(tmp), M(m + 0xc0));
    const uint32_t at = pv + trail + (lwu(pv + 0x2c4) << 4);
    vec3_add(V(at), V(tmp), V(m + 0x10));
    swc1(at + 0xc, 1.0f);
}
}  // namespace

// orig: L11:0x003192c8 L17:0x002f3b68
void class1319_trails_draw(uint32_t m) {
    if (lbu(m + 0x21) == 0xff) return;
    const uint32_t sp = ee_stack_alloc(0xa10);
    const uint32_t offsets = sp + 0x900;
    swc1(trail_length(), lbu(0x0015edb2) != 0 ? 16.0f : 5.0f);
    // 16 quads' descriptors: each step of the strip a little darker, the colours moving on every frame (of 4)
    for (int32_t step = 0; step < TRAIL_POINTS; ++step) {
        const uint32_t q = sp + static_cast<uint32_t>(step) * QUAD;
        sd(q + 0x78, effect_tex0(0x13));
        sd(q + 0x88, 0x8000000048ull);
        sd(q + 0x80, 0xff9000000260ull);
        sd(q + 0x70, 0);
        const float shift = fpu_div(int_to_float((lw(OV(0x0015f5cc)) + 3) & 3), fpu_mul(lwc1(trail_length()), 4.0f));
        for (int32_t k = 0; k < 4; ++k) {
            const uint32_t uv = corner_uvs() + static_cast<uint32_t>(k) * 8;
            swc1(q + 0x50 + static_cast<uint32_t>(k) * 8, lwc1(uv));
            swc1(q + 0x54 + static_cast<uint32_t>(k) * 8, lwc1(uv + 4));
            float t = fpu_add(fpu_div(int_to_float(step - ((k >> 1) - 1)), lwc1(trail_length())), shift);
            if (1.0f < t) t = 1.0f;
            else if (t < 0.0f) t = 0.0f;
            sw(q + 0x40 + static_cast<uint32_t>(k) * 4, rgba_lerp(0x8c28aa28u, 0x00144646u, t));
        }
    }
    libc_memset(offsets, 0, 0x20);
    swc1(offsets + 8, 0.25f);  // (each trail a strip 0.5 high)
    swc1(offsets + 0x18, -0.25f);
    const int32_t cls = lh(m + 0xa6);
    for (uint32_t e = lwu(OV(0x001ab840) + (lbu(m + 0x21) << 2));; e += 2) {
        const uint32_t member = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (lh(member + 0xa6) == cls && lbu(member + 0x31) != 0) {
            const uint32_t mpv = lwu(member + 0x78);
            for (int32_t i = 0; i < lw(mpv + 0x2c0); ++i) {
                const uint32_t q = sp + static_cast<uint32_t>(i) * QUAD;
                const int32_t from = mod16(lw(mpv + 0x2c4) - i + 0xf);
                for (uint32_t side = 0; side < 2; ++side) {
                    const uint32_t trail = mpv + 0xc0 + (side << 8);
                    for (int32_t k = 0; k < 4; ++k) {
                        const int32_t at = mod16(from + (k >> 1));
                        vec3_add(V(q + static_cast<uint32_t>(k) * 0x10), V(trail + (static_cast<uint32_t>(at) << 4)),
                                 V(offsets + (static_cast<uint32_t>(k & 1) << 4)));
                    }
                    quad_draw(q, 0);
                }
            }
        }
        if (lh(e) < 0) break;  // (the group's last member)
    }
    ee_stack_free(0xa10);
}

// orig: L11:0x00319670 L17:0x002f3f10
void class1319_trails_record(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xa0);
    if ((lw(OV(0x0015f5cc)) & 3) == 0) {
        const int32_t points = lw(pv + 0x2c0);
        sw(pv + 0x2c4, static_cast<uint32_t>(mod16(lw(pv + 0x2c4) + 1)));
        if (int_to_float(points) < lwc1(trail_length())) sw(pv + 0x2c0, static_cast<uint32_t>(points + 1));
    }
    wing_point(m, pv, sp, 0.7f, 0xc0);
    wing_point(m, pv, sp + 0x10, -0.7f, 0x1c0);
    const int32_t frame = lw(OV(0x0015f5cc));
    if (frame != 0) {
        const uint32_t queued = lwu(pv + 0x2c8);
        if (lw(queued) != frame) {
            sw(queued, static_cast<uint32_t>(frame));
            sub_001f93e0(draw_callback(), m);
        }
    }
    ee_stack_free(0xa0);
}

// orig: L11:0x0031a438 L17:0x002f4c90
void class1319_path_pose(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x150);
    const uint32_t matrix = sp, p0 = sp + 0x40, p1 = sp + 0x50, p2 = sp + 0x60, at = sp + 0x70, rot = sp + 0x80;
    const uint32_t step = sp + 0x90, dist = sp + 0xa0;
    const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x60) << 2));
    const int32_t i = lw(pv + 0x64), count = lw(path);
    lq_sq(p0, path + 0x10 + (static_cast<uint32_t>(i) << 4));
    lq_sq(p1, path + 0x10 + (static_cast<uint32_t>(ee_rem(i + 1, count)) << 4));
    lq_sq(p2, path + 0x10 + (static_cast<uint32_t>(ee_rem(i + 2, count)) << 4));
    vec_clear(V(rot));
    const float yaw01 = atan2_f(fpu_sub(lwc1(p0), lwc1(p1)), fpu_sub(lwc1(p0 + 4), lwc1(p1 + 4)));
    const float yaw12 = atan2_f(fpu_sub(lwc1(p1), lwc1(p2)), fpu_sub(lwc1(p1 + 4), lwc1(p2 + 4)));
    const float pitch01 = atan2_f(vec2_distance(V(p0), V(p1)), fpu_sub(lwc1(p1 + 8), lwc1(p0 + 8)));
    const float pitch12 = atan2_f(vec2_distance(V(p1), V(p2)), fpu_sub(lwc1(p2 + 8), lwc1(p1 + 8)));
    swc1(rot + 4, angle_add(fpu_mul(angle_sub(pitch12, pitch01), lwc1(pv + 0x68)), pitch01));
    swc1(rot + 8, angle_add(fpu_mul(angle_sub(yaw12, yaw01), lwc1(pv + 0x68)), yaw01));
    lq_sq(m + 0x40, rot);
    if (0.0f < lwc1(pv + 0x6c)) {
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x40490fd0u)));  // (pi)
        swc1(m + 0x44, -lwc1(m + 0x44));
    }
    vec3_lerp(V(at), V(p0), V(p1), lwc1(pv + 0x68));
    euler_to_mat4_vu(M(matrix), V(rot));
    vec3_set_length(V(step), V(matrix + 0x10), fpu_mul(lwc1(pv + 0x78), sin_vu(lwc1(pv + 0x70))));
    vec3_add(V(at), V(at), V(step));
    vec3_set_length(V(step), V(matrix + 0x20), fpu_mul(lwc1(pv + 0x78), cos_vu(lwc1(pv + 0x70))));
    vec3_add(V(at), V(at), V(step));
    swc1(dist, 0.0f);
    seek_1d(dist, pv + 0x80, vec3_distance(V(m + 0x10), V(at)), fpu_mul(lwc1(0x0015ed70), 10.0f), fpu_mul(lwc1(0x0015ed70), 10.0f),
            fpu_mul(lwc1(0x0015ed6c), 20.0f));
    vec3_sub(V(step), V(at), V(m + 0x10));
    vec3_set_length(V(step), V(step), lwc1(pv + 0x80));
    vec3_add(V(m + 0x10), V(m + 0x10), V(step));
    ee_stack_free(0x150);
}

// orig: L11:0x0031a1c0 L17:0x002f4a18
void class1319_hit(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    sb(m + 0xa4, 0xff);
    // (targeted only while the hero flies)
    sh(m + 0x34, lw(g::HERO_STATE) == 0x32 ? (lhu(m + 0x34) | 0x1000u) : (lhu(m + 0x34) & 0xefffu));
    if (hit == 0) {
        if (lbu(m + 0x20) != 5) return;
        if (lw(g::HERO_STATE) != 0x32) {
            sb(m + 0x20, 4);
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
        }
        if (sphere_collide(m + 0x10, 1.0f, 0, m) == 0) return;
        sb(m + 0x20, 6);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        return;
    }
    // the game's records: the count of these hits (at most 0xffff), the play time's most in 10-second units, the levels
    // visited
    const uint32_t records = 0x00157150u;
    if (!(0xfffeu < lhu(records))) sh(records, lhu(records) + 1);
    if (static_cast<int32_t>(lhu(records + 2)) < ee_div(frame_scale_i(lw(0x0015eea4)), 0x258))
        sh(records + 2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
    const int32_t level = lw(0x0015ed84);
    sw(records + 4, lwu(records + 4) | (1u << (level & 31)) | 0x80000000u);
    if (level == 0xb) {  // (Pokitaru: three hits by class 0xac earn the reward, once)
        const uint32_t by = lwu(hit + 0x20);
        if (by != 0 && lh(by + 0xa6) == 0xac) {
            const int32_t n = lw(0x0015ee00) + 1;
            sw(0x0015ee00, static_cast<uint32_t>(n));
            if (!(n < 3) && lbu(0x0013d41a) == 0) {
                sb(0x0013d41a, 1);
                sound_play_indexed(1, 0, 0);
                sub_00263d40(TXT(0x53d6), -1);
            }
        }
    }
    const uint32_t next = lw(pv + 0x88) != 0 ? 6 : 2;
    if (lbu(m + 0x20) == next) return;
    sb(m + 0x20, next);
    if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
}

}  // namespace rac1
