// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1123, an electric arc (four flickering strands of 30
// points that hurt what crosses them), switched on after the arc it is linked to is off and switched off for good by a
// switch moby (a shower of sparks), with its hit test and level 6's build of a small VU0 helper. Header blarg_own.h;
// registered in blarg_mobys.cpp (its draw callback 0x305170 is blarg_part2's). Addresses are level 6's (`orig: L6:`); its
// own data is L6_ADDR (the arcs' shared tuning 0x1621e8 .. 0x162238, the sparks' four directions 0x1fe890), the base
// ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// the moby of the level's moby table at index `i`
uint32_t table_moby(int32_t i) { return lwu(L6_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// The point (0, 0, 0, 1) (VU0's vf0) stored at `at`.
// orig: L6:0x00217300
void store_vf0_l6(uint32_t at) {
    sw(at + 0x0, 0);
    sw(at + 0x4, 0);
    sw(at + 0x8, 0);
    swc1(at + 0xc, 1.0f);
}

// one of the arc's two hit lines (arc_hit_test_l6): from (x, y, z) to (-x, y, z) in the moby `m`'s frame (its matrix
// +0xc0, its position +0x10), tested with the hit record `rec`; the lines' vectors in the frame `sp`
void arc_hit_line(uint32_t m, uint32_t sp, uint32_t rec, float x, float y, float z) {
    const uint32_t a = sp, b = sp + 0x10;
    swc1(b + 0x4, y);
    swc1(b + 0x8, z);
    swc1(b + 0x0, -x);
    swc1(a + 0x0, x);
    swc1(a + 0x4, y);
    swc1(a + 0x8, z);
    mat3_transform(V(a), V(a), M(m + 0xc0));
    vec3_add(V(a), V(a), V(m + 0x10));
    mat3_transform(V(b), V(b), M(m + 0xc0));
    vec3_add(V(b), V(b), V(m + 0x10));
    segment_collide_v(V(a), V(b), 0, m, static_cast<const uint32_t*>(ee_ram_ptr(rec)));
}
}  // namespace

// The arc `m`'s hit test: a hit record (6, flags 0x10000) on two lines across it, from +x to -x of the half-size 0x1621e8
// at +y and -y of 0x1621ec, at the height 0x1621f0, in its frame.
// orig: L6:0x00305000
void arc_hit_test_l6(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t rec = sp + 0x20;
    hit_rec_init(rec, m, 0x10000, 6.0f);
    arc_hit_line(m, sp, rec, lwc1(L6_ADDR(0x001621e8)), lwc1(L6_ADDR(0x001621ec)), lwc1(L6_ADDR(0x001621f0)));
    arc_hit_line(m, sp, rec, lwc1(L6_ADDR(0x001621e8)), -lwc1(L6_ADDR(0x001621ec)), lwc1(L6_ADDR(0x001621f0)));
    ee_stack_free(0xb0);
}

// Class 1123, an electric arc. pvars: +0 .. +0x780 four strands of 30 points (0x1e0 bytes each; the two ends stay, the
// 28 between drift), +0x790 + strand * 0x1e0 their 28 drifts, +0xf00 .. +0xf0c the strands' frames until they are drawn
// afresh (started 0, 11, 23, 34 frames apart; then every 0x162234 frames), +0xf10 the arc it is linked to, +0xf14 the
// switch moby (its +0xbc set: off), +0xf18 its hum's sound slot. Its size +0x2c: the class's scale times 0x162210.
// States: 0 set up (the level's 0x162238 = 22, 0x162234 = 45) (1, or 2 without a link); 1 waits while the linked arc (of
// class 1123) is on, below its state 3 (2, its +0x94 the class's +0x10); 2 on: hums (sound 0), hurts what crosses it
// (arc_hit_test_l6), the level's phase 0x1621f8 runs by 0x1621fc a frame (wrapped to 1); each strand drawn afresh along x
// (from -0x162204 / 2, by 0x162204 / 30; the start carries over from strand to strand) or moved by its drifts; its draw
// within 48 of the camera (0x167540); its switch set: the hum stopped, the sound 1 (3, +0x94 cleared); 3 a shower of
// sparks: four bursts of 50 (type 0x3c) along its x axis scaled by 0x162208, each burst turned by one of the four
// rotations 0x1fe890 and raised by 0.6 + 1.7 * its index (4); 4 off.
// orig: L6:0x003049f8
void blg_class1123_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x2a0);
    switch (lbu(m + 0x20)) {
    case 0:
        swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L6_ADDR(0x00162210))));
        sw(L6_ADDR(0x00162238), 0x16);
        sw(L6_ADDR(0x00162234), 0x2d);
        sw(pv + 0xf04, 0xb);
        sw(pv + 0xf08, 0x17);
        sw(pv + 0xf0c, 0x22);
        sw(pv + 0xf00, 0);
        sw(pv + 0xf18, 0xffffffffu);
        if (lw(pv + 0xf10) == -1) {
            sb(m + 0x20, 2);
        } else {
            sb(m + 0x20, 1);
            sw(m + 0x94, 0);
        }
        break;
    case 1: {
        const uint32_t link = table_moby(lw(pv + 0xf10));
        if (static_cast<int16_t>(lhu(link + 0xa6)) == 0x463 && lbu(link + 0x20) < 3) break;
        sb(m + 0x20, 2);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        break;
    }
    case 2: {
        if (sub_0028d8c0(m, lw(pv + 0xf18)) == 0) sw(pv + 0xf18, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        arc_hit_test_l6(m);
        float phase = fpu_add(lwc1(L6_ADDR(0x001621f8)), fpu_mul(lwc1(L6_ADDR(0x001621fc)), dt()));
        if (1.0f < phase) phase = fpu_sub(phase, 1.0f);
        swc1(L6_ADDR(0x001621f8), phase);
        const float length = lwc1(L6_ADDR(0x00162204));
        const float step = fpu_div(length, 30.0f);
        float x = -fpu_mul(length, 0.5f);  // (not reset between strands)
        for (uint32_t s = 0; s < 4; ++s) {
            const uint32_t count = pv + 0xf00 + s * 4;
            const uint32_t points = pv + s * 0x1e0, drifts = pv + s * 0x1e0 + 0x790;
            if (lw(count) == 0) {
                // drawn afresh: 30 points along x, 30 random jolts (frame sp) smoothed three by three into 28 drifts
                for (uint32_t k = 0; k < 30; ++k) {
                    store_vf0_l6(points + k * 0x10);
                    swc1(points + k * 0x10, x);
                    x = fpu_add(x, step);
                    rand_sphere_point(sp + k * 0x10, 0.0f, fpu_mul(lwc1(L6_ADDR(0x00162200)), dt()));
                }
                for (uint32_t k = 0; k < 28; ++k) {
                    const uint32_t d = drifts + k * 0x10;
                    vec3_add(V(d), V(sp + k * 0x10), V(sp + (k + 1) * 0x10));
                    vec3_add(V(d), V(d), V(sp + (k + 2) * 0x10));
                    vec3_scale(V(d), V(d), 0.333f);
                }
                sw(count, lwu(L6_ADDR(0x00162234)));
            } else {
                for (uint32_t k = 0; k < 28; ++k) vec3_add(V(points + 0x10 + k * 0x10), V(points + 0x10 + k * 0x10), V(drifts + k * 0x10));
                sw(count, lwu(count) - 1);
            }
        }
        if (vec3_distance(V(m + 0x10), V(L6_ADDR(0x00167540))) < 48.0f) sub_001f93e0(L6_ADDR(0x00305170), m);
        const int32_t sw_index = lw(pv + 0xf14);
        if (sw_index < 0 || lbu(table_moby(sw_index) + 0xbc) == 0) break;
        sw(m + 0x94, 0);
        sb(m + 0x20, 3);
        blarg_sound_stop(m, lw(pv + 0xf18));
        sw(pv + 0xf18, 0xffffffffu);
        moby_sound_start(1, 0, m);
        break;
    }
    case 3: {
        const uint32_t frame = sp, turn = sp + 0x40, centre = sp + 0x70, at = sp + 0x80, vel = sp + 0x90;
        euler_to_mat4_vu(M(frame), V(m + 0x40));
        mat4_scale_diag(M(turn), lwc1(L6_ADDR(0x00162208)));  // (L6:0x217ab8; it leaves VU0's vf1 .. vf5 set, as the engine's mat4_scale_diag now does: the moby pass's shadow meshes read vf5's w lane)
        mat4_mul(M(frame), M(frame), M(turn));
        for (int32_t b = 0; b < 4; ++b) {
            euler_to_mat4_vu(M(turn), V(L6_ADDR(0x001fe890) + static_cast<uint32_t>(b) * 0x10));
            mat4_mul(M(turn), M(frame), M(turn));
            vec3_add(V(centre), V(centre), V(m + 0x10));  // (the matrix's row 3: the burst's centre)
            swc1(centre + 0xc, 1.0f);
            swc1(centre + 0x8, fpu_add(lwc1(centre + 0x8), fpu_add(fpu_mul(static_cast<float>(b), 1.7f), 0.6f)));
            for (int k = 0; k < 50; ++k) {
                // a spark at a random point along the burst's x axis (row 0), within 0.75 of 0x162204 of its centre
                const float along = rand_range_signed_f(0.0f, fpu_mul(lwc1(L6_ADDR(0x00162204)), 0.75f));
                vec3_set_length(V(at), V(turn), along);
                vec3_add(V(at), V(at), V(centre));
                const uint32_t color = rgba_lerp(lwu(L6_ADDR(0x00162214)), lwu(L6_ADDR(0x00162218)), rand_range_f(0.0f, 1.0f));
                const float size = rand_range_f(lwc1(L6_ADDR(0x0016221c)), lwc1(L6_ADDR(0x00162220)));
                const int32_t life = float_to_int(rand_range_f(static_cast<float>(lw(L6_ADDR(0x00162224))),
                                                               static_cast<float>(lw(L6_ADDR(0x00162228)))));
                rand_sphere_point(vel, fpu_mul(lwc1(L6_ADDR(0x0016222c)), dt()), fpu_mul(lwc1(L6_ADDR(0x00162230)), dt()));
                const uint32_t frames = static_cast<uint32_t>(frame_scale_i(life)) & 0xffu;
                const uint32_t start = static_cast<uint32_t>(rand_mod(0xff)) & 0xffu;
                spawn_particle_3c(at, vel, color, frames, start, 0, size);
            }
        }
        sb(m + 0x20, 4);
        break;
    }
    default:  // (4 and on: off)
        break;
    }
    ee_stack_free(0x2a0);
}

}  // namespace rac1
