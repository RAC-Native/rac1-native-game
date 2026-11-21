// Umbris's own moby classes: the level's story director (class 436: Qwark's talks at the course's start and on the
// trapdoor, the movie and the fall to the Snagglebeast's cave), the sprite-cache flush its last scene does (0x270248) and
// class 529 (a prop drawn by the level's 0x2ba260 as class 533) — hand-ported from Umbris's own overlay. Header
// umbris_mobys.h, registered in umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`); Umbris-only data is L7_ADDR, the
// base ELF's are bare (the story bytes 0x13d3b0..0x13d3b2, the hero 0x13f3d0).
//
// Class 436's pvars: three volumes (+0x0 the course's start, +0x4 the trapdoor's, -1: none), its mobys by index
// (+0x8 retired once the course started, +0xc and +0x14 hidden while Qwark talks, +0x10 the trapdoor: retired when the hero
// falls), the hero's places (+0x18 after the first talk, +0x1c on the trapdoor). The level's scene state 0x15f5c4 (2: a
// scene playing).
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/level_change.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;
constexpr uint32_t STORY = 0x0013d388;  // (the story bytes +0x28 the course started, +0x29 the trapdoor's talk, +0x2a fallen)

// the level's moby `i` (its table 0x15ffd8)
uint32_t moby_at(int32_t i) { return lwu(L7_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }
// moby `i` (-1: none) hidden (flags 0x41) or shown again
void moby_hide(int32_t i) {
    if (i == -1) return;
    const uint32_t m = moby_at(i);
    sh(m + 0x34, lhu(m + 0x34) | 0x41);
}
void moby_show(int32_t i) {
    if (i == -1) return;
    const uint32_t m = moby_at(i);
    sh(m + 0x34, lhu(m + 0x34) & 0xffbe);
}
// the hero put at the level's place `i` (-1: left where he is; the places 0x1600ec, 0x80 each: +0x30 where, +0x70 facing)
void hero_to_place(int32_t i, int32_t state) {
    if (i == -1) return;
    const uint32_t p = lwu(L7_ADDR(0x001600ec)) + (static_cast<uint32_t>(i) << 7);
    hero_place(p + 0x30, p + 0x70, state, 1);
}
bool scene_playing() { return lw(L7_ADDR(0x0015f5c4)) == 2; }
}  // namespace

// The level's sprite cache: the bits of the cells (columns col .. col+cols, rows row .. row+rows) of its bitmap
// (0x184270 +0xc, 64 bytes a row) that are set are cleared, each one's 32x32 block fetched first (sprite_cache_get).
// orig: L7:0x00270248
void umb_270248_l7(int32_t col, int32_t row, int32_t cols, int32_t rows) {
    const int32_t row_end = row + rows;
    for (int32_t r = row; r < row_end; ++r) {
        const int32_t col_end = col + cols;
        for (int32_t c = col; c < col_end; ++c) {
            const uint32_t at = lwu(L7_ADDR(0x00184270) + 0xc) + static_cast<uint32_t>(r << 6) +
                                static_cast<uint32_t>((c > -1 ? c : c + 7) >> 3);
            if (lbu(at) == 0) continue;
            const uint32_t bit = 1u << (c & 7);
            if ((lbu(at) & bit) == 0) continue;
            sprite_cache_get(static_cast<uint32_t>((c >> 5) + ((r >> 5) << 4)));
            sb(at, lbu(at) & ~bit);
        }
    }
}

// Class 436, the story director (state +0x20, 0 .. 13; the table's NTSC labels L7:0x2f5be8 .. 0x2f60a4):
// 0 waiting for the level's first scene to end (the course's mob +0x8 retired once started); 1 Qwark's first talk
// (once: story +0x28); 2 its end; 3 -> 4; 4 the hero in the start's volume: the talk at the course (once: +0x29); 5 .. 7
// the talk, the hero placed, the course's music; 8 the hero on the trapdoor: placed, its talk (once: +0x2a); 9 the movie;
// 10 the last scene; 11 the hero's fall: the trapdoor retired, the cave's sprites flushed, the level left for level 8;
// 12 the trapdoor gone; 13 done (retired).
// orig: L7:0x002f5ba0
void umb_class436_update_l7(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st >= 14) return;
    const uint32_t pv = lwu(m + 0x78);
    switch (st) {
        case 0:  // L7:0x2f5be8
            if (lbu(STORY + 0x29) != 0 && lw(pv + 0x8) != -1) moby_retire(moby_at(lw(pv + 0x8)));
            if (lw(L7_ADDR(0x0015f5c4)) == 0) sb(m + 0x20, 1);
            return;
        case 1:  // L7:0x2f5c2c
            if (lbu(STORY + 0x28) != 0) {
                sb(m + 0x20, 3);
                return;
            }
            sb(STORY + 0x28, 1);
            sb(m + 0x20, 2);
            cutscene_start(0);
            return;
        case 2:  // L7:0x2f5c60
            if (!scene_playing()) sb(m + 0x20, 3);
            return;
        case 3:  // L7:0x2f5c7c
            sb(m + 0x20, 4);
            return;
        case 4: {  // L7:0x2f5c88
            if (lbu(STORY + 0x29) != 0) {
                sb(m + 0x20, 7);
            } else {
                const int32_t vol = lw(pv + 0x0);
                if (vol == -1) {
                    sb(m + 0x20, 8);
                } else if (point_in_box(HERO_POS, vol) != 0) {
                    sb(STORY + 0x29, 1);
                    moby_hide(lw(pv + 0xc));
                    moby_hide(lw(pv + 0x14));
                    sb(m + 0x20, 5);
                    cutscene_start(1);
                }
            }
            if (lbu(m + 0x20) == 4) return;
            if (lw(pv + 0x8) != -1) moby_retire(moby_at(lw(pv + 0x8)));
            return;
        }
        case 5:  // L7:0x2f5d50
            if (scene_playing()) return;
            moby_hide(lw(pv + 0xc));
            sb(m + 0x20, 6);
            cutscene_start(2);
            return;
        case 6:  // L7:0x2f5d9c
            if (scene_playing()) return;
            hero_to_place(lw(pv + 0x18), 0);
            sb(m + 0x20, 7);
            return;
        case 7:  // L7:0x2f5de8
            moby_show(lw(pv + 0xc));
            moby_show(lw(pv + 0x14));
            music_track_request(2, 4);
            sb(m + 0x20, 8);
            return;
        case 8: {  // L7:0x2f5e44
            if (lbu(STORY + 0x2a) != 0) {
                sb(m + 0x20, 0xc);
            } else {
                bool test = true;  // the trapdoor's mob +0xc gone (none, null, retired) or dying (0xfd)
                const int32_t gate = lw(pv + 0xc);
                if (gate != -1) {
                    const uint32_t g = moby_at(gate);
                    if (g != 0 && lbu(g + 0x20) != 0xfe && lbu(g + 0x20) != 0xfd) test = false;
                }
                if (test) {
                    const int32_t vol = lw(pv + 0x4);
                    if (vol == -1) {
                        sb(m + 0x20, 0xd);
                    } else if (point_in_box(HERO_POS, vol) != 0) {
                        hero_to_place(lw(pv + 0x1c), 0x72);
                        sb(STORY + 0x2a, 1);
                        sb(m + 0x20, 9);
                        cutscene_start(3);
                    }
                }
            }
            if (lbu(m + 0x20) == 8) return;
            moby_hide(lw(pv + 0x10));
            return;
        }
        case 9:  // L7:0x2f5f38
            if (scene_playing()) return;
            sb(m + 0x20, 0xa);
            movie_play(8);
            return;
        case 10:  // L7:0x2f5f60
            if (scene_playing()) return;
            sb(m + 0x20, 0xb);
            cutscene_start(4);
            [[fallthrough]];
        case 11: {  // L7:0x2f5f80
            if (scene_playing()) return;
            hero_to_place(lw(pv + 0x1c), 0);
            const int32_t door = lw(pv + 0x10);
            if (door != -1) {
                // the trapdoor drawn toward the hero (0.9 at most) before it goes
                const uint32_t d = moby_at(door);
                Vec4 v;
                vec3_sub(v, V(HERO_POS), V(d + 0x10));
                vec3_clamp_length_vu(v, v, u2f(0x3f666666u));
                vec3_add(V(d + 0x10), V(HERO_POS), v);
                moby_retire(d);
            }
            umb_270248_l7(0x87, 0xc, 0x56, 0x3c);
            level_mark_visited(8);
            level_change_request(0, 8);
            level_exit_to(8);
            sb(m + 0x20, 0xc);
            return;
        }
        case 12: {  // L7:0x2f605c
            const int32_t door = lw(pv + 0x10);
            if (door != -1) {
                const uint32_t d = moby_at(door);
                if (d != 0 && lbu(d + 0x20) != 0xfe && lbu(d + 0x20) != 0xfd) moby_retire(d);
            }
            sb(m + 0x20, 0xd);
            return;
        }
        default:  // 13, L7:0x2f60a4
            moby_retire(m);
            return;
    }
}

// Class 529: a prop shown as class 533 (+0xa6) at full alpha 8 (+0x30), drawn by the level's 0x2ba260.
// orig: L7:0x002fbdb8
void umb_class529_update_l7(uint32_t m) {
    sb(m + 0x30, 8);
    sh(m + 0xa6, 0x215);
    sub_001f93e0(L7_ADDR(0x002ba260), m);
}

}  // namespace rac1
