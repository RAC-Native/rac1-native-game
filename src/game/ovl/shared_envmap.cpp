// Kalebo III's (level 16) environment-mapped objects, classes 1943, 1948 and 1953: each draws two or three meshes of the
// level's (their points, normals and colours in the level's tables) with the sky's reflection, sphere-mapped from the camera,
// each mesh only when its bounding sphere is on screen. The three classes' code is the same with each its own tables and
// mesh count; the five copies of the mapping helper are the same function. Level-only words (the tables, the spheres' block,
// the draw callbacks) are L16_ADDR; the camera 0x166dc0 goes through OV(). The draw's matrices are set up by the level's own
// function 0x1fafa8 (still transliterated). Header shared_envmap.h.
#include <game/ovl/shared_envmap.h>

#include <game/math/vecmath.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

namespace {
// One class's meshes: their tables (4 words each: the point counts, the points, a word the draw does not read, the normals,
// the colours), the meshes' bounding spheres (16 bytes each, the coordinates' buffer after them), the mesh count.
struct EnvSet {
    uint32_t counts, points, normals, colours, spheres;
    int32_t meshes;
};

EnvSet set_1943() { return {L16_ADDR(0x00161ef0), L16_ADDR(0x00161f00), L16_ADDR(0x00161f20), L16_ADDR(0x00161f30), L16_ADDR(0x001dc600), 3}; }
EnvSet set_1948() { return {L16_ADDR(0x00161f90), L16_ADDR(0x00161f98), L16_ADDR(0x00161fa8), L16_ADDR(0x00161fb0), L16_ADDR(0x001df2b0), 2}; }
EnvSet set_1953() { return {L16_ADDR(0x00162008), L16_ADDR(0x00162018), L16_ADDR(0x00162038), L16_ADDR(0x00162048), L16_ADDR(0x001e5390), 3}; }

Vec4 packed(uint32_t at, float w) { return Vec4{lwc1(at), lwc1(at + 4), lwc1(at + 8), w}; }

// State 0: each mesh's bounding sphere (the middle of its box, the farthest point's distance); then 1: the draw each frame.
void envmap_update(uint32_t m, const EnvSet& s, uint32_t draw) {
    switch (lbu(m + 0x20)) {
    case 0:
        for (int32_t i = 0; i < s.meshes; ++i) {
            const uint32_t at = static_cast<uint32_t>(i) * 4;
            Vec4 hi{-1024.0f, -1024.0f, -1024.0f, 0.0f}, lo{1024.0f, 1024.0f, 1024.0f, 0.0f};
            const int32_t n = lw(s.counts + at);
            uint32_t p = lwu(s.points + at);
            for (int32_t k = 0; k < n; ++k, p += 0xc) {
                const float x = lwc1(p), y = lwc1(p + 4), z = lwc1(p + 8);
                if (hi.x < x) hi.x = x;
                if (hi.y < y) hi.y = y;
                if (hi.z < z) hi.z = z;
                if (x < lo.x) lo.x = x;
                if (y < lo.y) lo.y = y;
                if (z < lo.z) lo.z = z;
            }
            const uint32_t sphere = s.spheres + static_cast<uint32_t>(i) * 0x10;
            sw(sphere + 0xc, 0);
            swc1(sphere, fpu_mul(fpu_add(hi.x, lo.x), 0.5f));
            swc1(sphere + 4, fpu_mul(fpu_add(hi.y, lo.y), 0.5f));
            swc1(sphere + 8, fpu_mul(fpu_add(hi.z, lo.z), 0.5f));
            const uint32_t sp = ee_stack_alloc(0x10);  // (the point as the original holds it: its w the frame's leftover)
            for (int32_t k = 0; k < lw(s.counts + at); ++k) {
                const uint32_t q = lwu(s.points + at) + static_cast<uint32_t>(k) * 0xc;
                swc1(sp, lwc1(q));
                swc1(sp + 4, lwc1(q + 4));
                swc1(sp + 8, lwc1(q + 8));
                const float d = vec3_distance(V(sp), V(sphere));
                if (lwc1(sphere + 0xc) < d) swc1(sphere + 0xc, d);
            }
            ee_stack_free(0x10);
        }
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        sh(m + 0x32, 0xff);
        return;
    case 1:
        sub_001f93e0(draw, m);
        return;
    default:
        return;
    }
}

// The draw: the effect texture 0x29, the blending and the clamp for the reflection, the matrices; each mesh on screen
// (its sphere within 512 of the view) mapped and drawn.
void envmap_draw(const EnvSet& s) {
    sub_002a0ba8(6, effect_tex0(0x29));
    sub_002a0ba8(0x42, (0x8000ull << 23) | 0x64u);
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x14, (0xff90ull << 32) | 0x260u);
    kal_1fafa8_l16(0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);  // (reads none of them)
    const uint32_t uvs = s.spheres + static_cast<uint32_t>(s.meshes) * 0x10;
    for (int32_t i = 0; i < s.meshes; ++i) {
        const uint32_t at = static_cast<uint32_t>(i) * 4;
        if (point_on_screen(s.spheres + static_cast<uint32_t>(i) * 0x10, 512.0f) == -1) continue;
        envmap_uvs(lw(s.counts + at), lwu(s.normals + at), lwu(s.points + at), uvs);
        prim_list_packet(lw(s.counts + at), lwu(s.points + at), lwu(s.colours + at), uvs, 1);
    }
}
}  // namespace

// orig: L16:0x002e8b98 L16:0x002e90c8 L16:0x002e95f8 L16:0x002e9b38 L16:0x002e9e40
void envmap_uvs(int32_t n, uint32_t normals, uint32_t verts, uint32_t out) {
    for (int32_t i = 0; i < n; ++i, out += 8) {
        const uint32_t at = static_cast<uint32_t>(i) * 0xc;
        Vec4 eye{};  // (the camera to the point, made a unit; the w words 1.0 as the original sets them)
        vec3_sub(eye, packed(verts + at, 1.0f), V(OV(0x00166dc0)));
        vec3_set_length(eye, eye, 1.0f);
        Vec4 normal = packed(normals + at, 1.0f);
        vec3_set_length(normal, normal, 1.0f);
        const float dot = vec3_dot(normal, eye);
        Vec4 r{};  // (the reflection)
        vec3_scale(r, normal, fpu_add(dot, dot));
        vec3_sub(r, eye, r);
        vec3_set_length(r, r, 1.0f);
        r.z = fpu_add(r.z, 1.0f);
        const float len = vec3_length(r);
        const float m = fpu_add(len, len);
        const float v = fpu_div(r.y, m);
        const float u = fpu_div(r.x, m);
        const float vv = fpu_add(v, 0.5f);
        const float uu = fpu_add(u, 0.5f);
        swc1(out + 4, fpu_add(vv, vv));
        swc1(out, fpu_add(uu, uu));
    }
}

// orig: L16:0x002e98e0
void envmap_1948_update(uint32_t m) { envmap_update(m, set_1948(), L16_ADDR(0x002e97a0)); }
// orig: L16:0x002ea128
void envmap_1953_update(uint32_t m) { envmap_update(m, set_1953(), L16_ADDR(0x002e9fe8)); }
// orig: L16:0x002e8d40
void envmap_1943_draw(uint32_t) { envmap_draw(set_1943()); }
// orig: L16:0x002e97a0
void envmap_1948_draw(uint32_t) { envmap_draw(set_1948()); }
// orig: L16:0x002e9fe8
void envmap_1953_draw(uint32_t) { envmap_draw(set_1953()); }

}  // namespace rac1
