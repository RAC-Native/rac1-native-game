// The meshes' draw callbacks: one port of the seven copies (Kalebo III's one, Drek's fleet's six: the same code relinked;
// compared word by word, their only differences the effect texture (0x29 / 0x30), the mesh count (3, 2 or 1) and the blend
// word's shift (23 / 22)). Each copy's tables (the meshes' point counts, points, a word the draw does not read, normals and
// colours, 4 bytes a mesh) and the buffer of the mapped coordinates are the level's own (L16_ADDR / L17_ADDR below). The
// mapping is each level's build: Kalebo III's envmap_uvs, Drek's fleet's envmap_uvs4, and for class 1913 its own build
// (envmap_uvs4_to_camera). The draw's matrices: shadows_vu1_setup (Veldin's 0x1fc740, which the overlay maps pair with
// L16:0x1fafa8 and L17:0x1f97a8). Header shared_envmap_meshes.h.
#include <game/ovl/shared_envmap_meshes.h>

#include <game/math/vecmath.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/shared_envmap.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
enum class Uvs { Kalebo, Fleet, FleetToCamera };

struct Meshes {
    uint32_t counts, points, unused, normals, colours, buffer;
};

void meshes_draw(int32_t tex, int32_t meshes, uint32_t shift, const Meshes& t, Uvs uvs) {
    sub_002a0ba8(6, effect_tex0(tex));
    sub_002a0ba8(0x42, (0x8000ull << shift) | 0x64u);
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x14, (0xff90ull << 32) | 0x260u);
    shadows_vu1_setup();
    for (int32_t i = 0; i < meshes; ++i) {
        const uint32_t at = static_cast<uint32_t>(i) * 4;
        const int32_t n = lw(t.counts + at);
        switch (uvs) {
        case Uvs::Kalebo: envmap_uvs(n, lwu(t.normals + at), lwu(t.points + at), t.buffer); break;
        case Uvs::Fleet: envmap_uvs4(n, lwu(t.normals + at), lwu(t.points + at), lwu(t.unused + at), t.buffer); break;
        case Uvs::FleetToCamera: envmap_uvs4_to_camera(n, lwu(t.normals + at), lwu(t.points + at), lwu(t.unused + at), t.buffer); break;
        }
        prim_list_packet(lw(t.counts + at), lwu(t.points + at), lwu(t.colours + at), t.buffer, 1);
    }
}
}  // namespace

// orig: L16:0x002e9ce0
void class1952_draw(uint32_t) {
    meshes_draw(0x29, 3, 23, {L16_ADDR(0x00161fb8), L16_ADDR(0x00161fc8), L16_ADDR(0x00161fd8), L16_ADDR(0x00161fe8), L16_ADDR(0x00161ff8), L16_ADDR(0x001e28d0)},
                Uvs::Kalebo);
}
// orig: L17:0x002f5690
void class1907_draw(uint32_t) {
    meshes_draw(0x30, 2, 22, {L17_ADDR(0x001623f0), L17_ADDR(0x001623f8), L17_ADDR(0x00162400), L17_ADDR(0x00162408), L17_ADDR(0x00162410), L17_ADDR(0x001de8a0)},
                Uvs::Fleet);
}
// orig: L17:0x002f59a8
void class1908_draw(uint32_t) {
    meshes_draw(0x30, 1, 22, {L17_ADDR(0x00162420), L17_ADDR(0x00162440), L17_ADDR(0x00162448), L17_ADDR(0x00162450), L17_ADDR(0x00162458), L17_ADDR(0x001df6a0)},
                Uvs::Fleet);
}
// orig: L17:0x002f5cc0
void class1910_draw(uint32_t) {
    meshes_draw(0x30, 2, 22, {L17_ADDR(0x00162460), L17_ADDR(0x00162468), L17_ADDR(0x00162470), L17_ADDR(0x00162478), L17_ADDR(0x00162480), L17_ADDR(0x001e0160)},
                Uvs::Fleet);
}
// orig: L17:0x002f5fd8
void class1911_draw(uint32_t) {
    meshes_draw(0x30, 1, 22, {L17_ADDR(0x00162490), L17_ADDR(0x001624b0), L17_ADDR(0x001624b8), L17_ADDR(0x001624c0), L17_ADDR(0x001624c8), L17_ADDR(0x001e0c90)},
                Uvs::Fleet);
}
// orig: L17:0x002f62f0
void class1913_draw(uint32_t) {
    meshes_draw(0x30, 2, 22, {L17_ADDR(0x001624d0), L17_ADDR(0x001624d8), L17_ADDR(0x001624e0), L17_ADDR(0x001624e8), L17_ADDR(0x001624f0), L17_ADDR(0x001e2d20)},
                Uvs::FleetToCamera);
}
// orig: L17:0x002f6608
void class1918_draw(uint32_t) {
    meshes_draw(0x30, 1, 22, {L17_ADDR(0x00162500), L17_ADDR(0x00162520), L17_ADDR(0x00162528), L17_ADDR(0x00162530), L17_ADDR(0x00162538), L17_ADDR(0x001e3b20)},
                Uvs::Fleet);
}

}  // namespace rac1
