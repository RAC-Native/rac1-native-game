// Blarg's mesh draws: each build ported once and given each copy's tables (the copies are the same code relinked on other
// tables, compared with the relocated fields masked, and normalised; class 1062's two differ in their quad count too).
// Their words and tables are Blarg's own (below); the camera 0x166dc0 and its turn 0x166dd0 are the engine's (OV).
// Header shared_blarg_meshes.h.
//
// A mesh: its quads' corners (vertex index, texture coordinate index: two halves each, four per quad), its vertices
// (16 bytes), its texture coordinates (pairs); the culled builds' quad centres and normals (16 bytes each).
#include <game/ovl/shared_blarg_meshes.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {

struct Mesh {
    uint32_t corners, verts, uvs, centres, normals;
    int32_t quads;
};

uint64_t sx(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }

// The GS blend word from the words at `w` (+0, +4 << 2, +8 << 4, +0xc << 6) and the fixed alpha at `fix` (<< 32).
uint64_t blend_of(uint32_t w, uint32_t fix) {
    return sx(lwu(w)) | (sx(lwu(w + 4)) << 2) | (sx(lwu(w + 8)) << 4) | (sx(lwu(w + 0xc)) << 6) | (sx(lwu(fix)) << 32);
}

// A descriptor's GS words: its texture, the TEX1 word, the blend, CLAMP 0.
void desc_regs(uint32_t d, uint64_t tex, uint64_t blend) {
    sd(d + 0x78, tex);
    sd(d + 0x80, 0x0000ff9000000260ull);
    sd(d + 0x88, blend);
    sd(d + 0x70, 0);
}

void desc_colour(uint32_t d, uint32_t c) {
    for (uint32_t k = 0; k < 0x10; k += 4) sw(d + 0x40 + k, c);
}

// Quad `i`'s centre (scaled, through `mat`) seen from the camera: 0 < (its normal through `mat`) . (the way from the camera
// to it) is a quad facing away. `centre` / `dir` scratch vectors.
bool faces_away(const Mesh& g, int32_t i, float scale, uint32_t mat, uint32_t centre, uint32_t dir) {
    const uint32_t k = static_cast<uint32_t>(i) << 4;
    vec3_scale(V(centre), V(g.centres + k), scale);
    mat4_transform(V(centre), V(centre), M(mat));
    vec3_sub(V(dir), V(centre), V(OV(0x00166dc0)));
    vec3_set_length(V(dir), V(dir), 1.0f);
    mat3_transform(V(centre), V(g.normals + k), M(mat));
    return 0.0f < vec3_dot(V(dir), V(centre));
}

// Class 1083's words: +8..+0x14 the blend and +0x1c its fixed alpha, +0x18 the effect texture, +0x20 the colour, +0x28 the scale; +0 also the
// mesh's height (its matrix's translation's z).
uint32_t words_1083() { return L6_ADDR(0x0016204c); }

// The first build: every quad drawn.
void draw_1083_all(const Mesh& g) {
    const uint32_t w = words_1083();
    const uint32_t sp = ee_stack_alloc(0xe0);
    const uint32_t d = sp, mat = sp + 0x90;
    const uint64_t tex = effect_tex0(lw(w + 0x18));
    desc_regs(d, tex, blend_of(w + 8, w + 0x1c));
    mat4_identity(M(mat));
    swc1(mat + 0x38, lwc1(w));
    desc_colour(d, lwu(w + 0x20));
    for (int32_t i = 0; i < g.quads; ++i) {
        uint32_t e = g.corners + (static_cast<uint32_t>(i) << 4);
        for (uint32_t c = 0; c < 4; ++c, e += 4) {
            vec3_scale(V(d + c * 0x10), V(g.verts + (static_cast<uint32_t>(lh(e)) << 4)), lwc1(w + 0x28));
            const uint32_t uv = g.uvs + (static_cast<uint32_t>(lh(e + 2)) << 3);
            swc1(d + 0x50 + c * 8, lwc1(uv));
            swc1(d + 0x54 + c * 8, lwc1(uv + 4));
        }
        quad_draw(d, mat);
    }
    ee_stack_free(0xe0);
}

// The second build: the quads facing the camera.
void draw_1083_facing(const Mesh& g) {
    const uint32_t w = words_1083();
    const uint32_t sp = ee_stack_alloc(0x120);
    const uint32_t d = sp, mat = sp + 0x90, dir = sp + 0xd0, centre = sp + 0xe0;
    const uint64_t tex = effect_tex0(lw(w + 0x18));
    desc_regs(d, tex, blend_of(w + 8, w + 0x1c));
    mat4_identity(M(mat));
    desc_colour(d, lwu(w + 0x20));
    swc1(mat + 0x38, lwc1(w));
    for (int32_t i = 0; i < g.quads; ++i) {
        if (faces_away(g, i, lwc1(w + 0x28), mat, centre, dir)) continue;
        uint32_t e = g.corners + (static_cast<uint32_t>(i) << 4);
        for (uint32_t c = 0; c < 4; ++c, e += 4) {
            vec3_scale(V(d + c * 0x10), V(g.verts + (static_cast<uint32_t>(lh(e)) << 4)), lwc1(w + 0x28));
            const uint32_t uv = g.uvs + (static_cast<uint32_t>(lh(e + 2)) << 3);
            swc1(d + 0x50 + c * 8, lwc1(uv));
            swc1(d + 0x54 + c * 8, lwc1(uv + 4));
        }
        quad_draw(d, mat);
    }
    ee_stack_free(0x120);
}

// Class 1062's words: +0..+0x10 the blend, +0x14 the effect texture, +0x18 / +0x1c the two passes' colours, +0x20 the
// scale, +0x24 / +0x28 the texture's drift by the camera's turn (its z / y).
void draw_1062(const Mesh& g) {
    const uint32_t w = L6_ADDR(0x00161f7c);
    const uint32_t sp = ee_stack_alloc(0x1d0);
    const uint32_t da = sp, db = sp + 0x90, mat = sp + 0x120, dir = sp + 0x160, centre = sp + 0x170;
    const float drift_v = fpu_mul(lwc1(w + 0x28), lwc1(OV(0x00166dd4)));
    const float drift_u = fpu_mul(lwc1(w + 0x24), lwc1(OV(0x00166dd8)));
    sd(da + 0x78, effect_tex0(lw(w + 0x14)));
    const uint64_t tex_b = effect_tex0(0x2a);
    const uint64_t blend = blend_of(w, w + 0x10);
    sd(db + 0x78, tex_b);
    sd(da + 0x88, blend);
    sd(db + 0x88, blend);
    sd(da + 0x80, 0x0000ff9000000260ull);
    sd(db + 0x80, 0x0000ff9000000260ull);
    sd(db + 0x70, 0);
    sd(da + 0x70, 0);
    mat4_identity(M(mat));
    desc_colour(da, lwu(w + 0x18));
    desc_colour(db, lwu(w + 0x1c));
    for (int32_t i = 0; i < g.quads; ++i) {
        if (faces_away(g, i, lwc1(w + 0x20), mat, centre, dir)) continue;
        uint32_t e = g.corners + (static_cast<uint32_t>(i) << 4);
        for (uint32_t c = 0; c < 4; ++c, e += 4) {
            vec3_scale(V(da + c * 0x10), V(g.verts + (static_cast<uint32_t>(lh(e)) << 4)), lwc1(w + 0x20));
            lq_sq(db + c * 0x10, da + c * 0x10);
            const uint32_t uv = g.uvs + (static_cast<uint32_t>(lh(e + 2)) << 3);
            const float u = lwc1(uv), v = lwc1(uv + 4);
            swc1(da + 0x50 + c * 8, fpu_add(u, drift_u));
            swc1(da + 0x54 + c * 8, fpu_add(v, drift_v));
            swc1(db + 0x50 + c * 8, u);
            swc1(db + 0x54 + c * 8, v);
        }
        quad_draw(da, mat);
        quad_draw(db, mat);
    }
    ee_stack_free(0x1d0);
}

}  // namespace

// orig: L6:0x003003f0
void blarg_class1083_draw_a(uint32_t) { draw_1083_all(Mesh{L6_ADDR(0x001f34b0), L6_ADDR(0x001f2eb0), L6_ADDR(0x001f38b0), 0, 0, 64}); }
// orig: L6:0x00300588
void blarg_class1083_draw_b(uint32_t) { draw_1083_all(Mesh{L6_ADDR(0x001f5930), L6_ADDR(0x001f5330), L6_ADDR(0x001f5d30), 0, 0, 64}); }
// orig: L6:0x00300720
void blarg_class1083_draw_c(uint32_t) {
    draw_1083_facing(Mesh{L6_ADDR(0x001f7eb0), L6_ADDR(0x001f77b0), L6_ADDR(0x001f84b0), L6_ADDR(0x001faff0), L6_ADDR(0x001fa2f0), 96});
}
// orig: L6:0x00300958
void blarg_class1083_draw_d(uint32_t) {
    draw_1083_facing(Mesh{L6_ADDR(0x001fbcf0), L6_ADDR(0x001fb5f0), L6_ADDR(0x001fc2f0), L6_ADDR(0x001fe1f0), L6_ADDR(0x001fd4f0), 96});
}
// orig: L6:0x002fd460
void blarg_class1062_draw_b(uint32_t) {
    draw_1062(Mesh{L6_ADDR(0x001ef310), L6_ADDR(0x001eed30), L6_ADDR(0x001ef6b0), L6_ADDR(0x001f0560), L6_ADDR(0x001efbe0), 58});
}
// orig: L6:0x002fd748
void blarg_class1062_draw_c(uint32_t) {
    draw_1062(Mesh{L6_ADDR(0x001df6f0), L6_ADDR(0x001dbff0), L6_ADDR(0x001e2cf0), L6_ADDR(0x001eb730), L6_ADDR(0x001e4a30), 864});
}

}  // namespace rac1
