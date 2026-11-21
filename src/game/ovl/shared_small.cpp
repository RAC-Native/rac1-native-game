// Small helpers several levels carry a copy of: a level movie's start, a point turned about an origin, the camera's flag
// +0x88, a push with no direction. Each one's copies are the same code, relinked: the movies' table 0x137b80, the hero
// 0x13f350 and the game's words 0x15ed80 / 0x15ed88 are the base ELF's; the camera 0x166e00 and the zero vector 0x15f580 go
// through OV(). Header shared_small.h.
#include <game/ovl/shared_small.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

// The movie's two words (its file and its part) from the movies' table: the second language's (+0x1a10) when the game's
// word 0x15ed80 is set, else the first's (+0x19f8).
// orig: L2:0x00298c68 L8:0x002a1880 L16:0x002923d0
void level_movie_play(int32_t index) {
    if (index < 0) return;
    const uint32_t rec = 0x00137b80u + (static_cast<uint32_t>(index) << 3);
    uint32_t part, file;
    if (lw(0x0015ed80) != 0) {
        part = lwu(rec + 0x1a14);
        file = lwu(rec + 0x1a10);
    } else {
        part = lwu(rec + 0x19fc);
        file = lwu(rec + 0x19f8);
    }
    if (lwu(0x001415f8) == 0) {  // (the hero's +0x22a8)
        hero_lost();
        return;
    }
    if (lbu(0x00141401) != 0) return;  // (the hero's +0x20b1)
    movie_start(file, part, lwu(0x0015ed88));
}

// orig: L7:0x00288968 L9:0x0027fe88
void point_turned(uint32_t point, uint32_t out, uint32_t origin, uint32_t from, uint32_t to) {
    // (the frame's slots as the original lays them out: the inverse of `from` +0 (its fourth row euler_to_mat4's), `to` +0x40)
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t undo = sp, redo = sp + 0x40, d = sp + 0x80;
    euler_to_mat4_vu(M(undo), V(from));
    mat3_transpose(M(undo), M(undo));
    euler_to_mat4_vu(M(redo), V(to));
    vec3_sub(V(d), V(point), V(origin));
    mat4_transform(V(d), V(d), M(undo));
    mat4_transform(V(d), V(d), M(redo));
    vec3_add(V(out), V(d), V(origin));
    ee_stack_free(0x90);
}

// orig: L8:0x00313ba8 L9:0x00312888 L12:0x00312f58
void camera_flag_88() {
    sb(lwu(OV(0x00166e00)) + 0x88, 1);
}

// orig: L10:0x0024c7f0 L13:0x00265d68
void push_without_direction(uint32_t m, uint32_t a, uint32_t b, uint32_t pos, float strength) {
    sub_00259bc8(m, a, b, pos, OV(0x0015f580), strength);
}

}  // namespace rac1
