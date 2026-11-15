// Ported overlay code: the planet map (the menu page 0x1b3518, state 0xb, opened with Select;
// see planet_map.h). State block MAP = 0x1841f0: +0 / +4 the
// level's map image (packed / pixels), +8 the marker buffers, +0xc the explored mask (fog), +0x10,
// +0x14 the second image, +0x20 the level's marker records (0x28 bytes, ended by flag 4 at +4),
// +0x24 a map shown, +0x28 the level has a map, +0x30.. 8 marker slots, +0xb4 / +0x104 / +0x154
// per level the zoom and the scroll x / y, +0x1a4 the reveal circle (32 x 32 bits), +0x224 the
// planet shown, +0x228 the planet drawn (-1 none, -2 to load), +0x22c / +0x234 the stash entry
// of the current level's map and its size, +0x230 the map-o-matic's (item 0x21) version, +0x23c
// the unpack buffer, +0x240..0x250 GS addresses, +0x258..0x268 the planets' TEX0, +0x278.. 5
// buffers of planet data (+0x28c their planet | 0x1000 while read, +0x2a4 their sizes, +0x2a0 the
// one being read).
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <game/lib/libc.h>
#include <game/lib/snd989.h>
#include <game/port_stubs.h>
#include <host/level_slots.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001841f0u> MAP{};
constexpr OvConst<0x001b9cf0u> MS{};   // the menu's state
constexpr uint32_t PAD = 0x0013c940;  // pad 1
constexpr uint32_t SPR = 0x70000000;  // the scratchpad
// orig: (helper) EE CVT.W.S: truncate toward zero, saturating
int32_t cvt_w_s(float f) {
    if (f >= 2147483648.0f) return INT32_MAX;
    if (f <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(f);
}
float* fp(uint32_t a) { return static_cast<float*>(ee_ram_ptr(a)); }
}  // namespace

// An empty function the map calls (a debug hook).
// orig: 0x0024abf0
void map_nop() {}

// A 1-bit image unpacked to `dst` (0x8000 bytes): runs of set / clear pixels (`runs`, a byte
// each, 0 skipped toggling the colour) placed by (skip, count) pairs (`spans`) on the
// scratchpad, 8 bytes packed into one, 0x400 bytes at a time.
// orig: 0x00249fc0
void map_bits_unpack(uint32_t dst, uint32_t runs, uint32_t spans) {
    const uint32_t end = dst + 0x8000;
    const uint32_t lim = SPR + 0x2000;
    uint32_t colour = 1;
    uint32_t left = lbu(runs) >> 1;
    ++runs;
    mem_fill32(SPR, 0, 0x2400);
    uint32_t p = SPR;
    for (;;) {
        const uint32_t next = dst + 0x400;
        do {
            const uint32_t skip = lbu(spans);
            ++spans;
            uint32_t n = lbu(spans);
            p += skip;
            ++spans;
            while (n != 0) {
                --n;
                if (left == 0) {
                    do {
                        left = lbu(runs);
                        colour = colour == 0 ? 1u : 0u;
                        ++runs;
                    } while (left == 0);
                }
                sb(p, colour);
                --left;
                ++p;
            }
        } while (p < lim);
        uint32_t o = SPR;
        for (uint32_t i = SPR; i < lim; i += 8, ++o) {
            uint32_t v = 0;
            for (uint32_t k = 0; k < 8; ++k) v |= static_cast<uint32_t>(lbu(i + k)) << k;
            sb(o, v);
        }
        mem_copy128(dst, SPR, 0x400);
        dst = next;
        if (next == end) return;
        const uint32_t back = p - 0x2000;
        mem_fill32(SPR, 0, 0x2000);
        if (lim < p) {
            uint32_t d = SPR;
            for (uint32_t s = lim; s < p; ++s, ++d) sb(d, lbu(s));
        }
        mem_fill32(lim, 0, 0x400);
        p = back;
    }
}

// A 1-bit image (128 rows of 16 bytes) widened to 4 bits a pixel (0 / 0xf), each row 4 times.
// orig: 0x0024a1f0
void map_bits_widen(uint32_t dst, uint32_t src) {
    for (int32_t r = 0; r < 0x80; ++r) {
        uint32_t row[16];
        for (int32_t i = 0; i < 16; ++i) {
            const uint32_t b = lbu(src++);
            uint32_t w = 0;
            for (uint32_t k = 0; k < 8; ++k)
                if ((b & (1u << k)) != 0) w |= 0xfu << (k * 4);
            row[i] = w;
        }
        const uint32_t buf = ee_stack_alloc(0x40);
        for (uint32_t i = 0; i < 16; ++i) sw(buf + i * 4, row[i]);
        for (int32_t k = 0; k < 4; ++k) {
            mem_copy128(dst, buf, 0x40);
            dst += 0x40;
        }
        ee_stack_free(0x40);
    }
}

// The explored mask of a planet from its packed form (`src`: flag 1 run-length, else 1 bit a
// pixel widened).
// orig: 0x00249f48
void map_mask_unpack(uint32_t dst, uint32_t src, uint32_t spans) {
    map_nop();
    if (lw(OV(0x00184218)) == 0) return;
    if ((lbu(src) & 1) != 0) map_bits_unpack(dst, src, spans);
    else map_bits_widen(dst, src);
}

// The explored mask of a planet from its 256 tiles of 32 x 32 4-bit pixels (run-length,
// 0x2483c8): a bit per non-zero pixel, 16 tiles a row.
// orig: 0x00248278
void map_mask_from_tiles(uint32_t dst, uint32_t base, uint32_t table) {
    for (int32_t t = 0; t < 0x100; ++t) {
        rle4_decode(SPR, t, base, table);
        const int32_t q = (t < 0 ? t + 15 : t) >> 4;
        uint32_t out = dst + static_cast<uint32_t>(((t - (q << 4)) + (q << 9)) << 2);
        for (uint32_t r = 0; r < 0x20; ++r) {
            uint32_t bits = 0;
            for (uint32_t x = 0; x < 0x20; ++x) {
                const uint32_t b = lbu(SPR + r * 16 + (x >> 1));
                const uint32_t px = (x & 1) != 0 ? b >> 4 : b & 0xf;
                if (px != 0) bits |= 1u << x;
                if (x == 0x1f) {
                    sw(out, bits);
                    out += 4;
                    bits = 0;
                }
            }
            out += 0x3c;
        }
    }
}

// An 8-bit image (2^lw x 2^lh, its pixels and palette at EE `pix` / `pal`) queued for upload
// (0x16cd80) to the GS addresses `vpal` / `vpix` (bytes); its TEX0.
// orig: 0x00245b60
uint64_t image_upload_at(int32_t lw_, int32_t lh_, uint32_t pix, uint32_t pal, int32_t vpal, int32_t vpix) {
    int32_t tbw = lw_ - 6;
    if (!(-1 < tbw)) tbw = 0;
    const int32_t tbp = vpix >> 8;
    const int32_t cbp = vpal >> 8;
    const int64_t bw = static_cast<int32_t>(1u << tbw);
    uint64_t t = static_cast<uint64_t>(static_cast<int64_t>(tbp)) | (static_cast<uint64_t>(bw) << 14);
    t |= (static_cast<uint64_t>(static_cast<int64_t>(lw_)) << 26) | 0x1300000ull;
    t |= static_cast<uint64_t>(static_cast<int64_t>(lh_)) << 30;
    t |= (static_cast<uint64_t>(static_cast<int64_t>(cbp)) << 37) | 0x400000000ull;
    t |= 0x8000000000000000ull;
    const int32_t n = lw(OV(0x0015f418));
    if (n < 0x40) {
        const uint32_t r = OV(0x0016cd80) + static_cast<uint32_t>(n) * 16;
        sw(r, pix);
        sh(r + 6, static_cast<uint32_t>(cbp));
        sh(r + 4, 0);
        sw(r + 8, pal);
        sh(r + 0xe, static_cast<uint32_t>(tbp));
        sb(r + 0xc, static_cast<uint32_t>(lw_));
        sb(r + 0xd, static_cast<uint32_t>(lh_));
        sw(OV(0x0015f418), static_cast<uint32_t>(n + 1));
    }
    return t;
}

// 0x8000 bytes merged by a bit mask: `a` where its bit is set, else `b`.
// orig: 0x00246f40
void map_bytes_select(uint32_t dst, uint32_t a, uint32_t b, uint32_t mask) {
    for (int32_t i = 0; i < 0x8000; ++i, ++mask) {
        uint32_t bit = 1;
        for (int32_t k = 0; k < 8; ++k) {
            const uint32_t v = (lbu(mask) & bit) != 0 ? lbu(a) : lbu(b);
            bit <<= 1;
            sb(dst, v);
            ++a;
            ++b;
            ++dst;
        }
    }
}

// A buffer holding data no planet uses (+0x28c -1), from the last (`down` nonzero) or the
// first; -1 for none.
// orig: 0x00246a60
int32_t map_buffer_free(int32_t down) {
    for (int32_t i = 0; i < 5; ++i) {
        const int32_t k = down != 0 ? 4 - i : i;
        if (lwu(MAP + 0x278 + static_cast<uint32_t>(k) * 4) == 0) continue;
        if (lw(MAP + 0x28c + static_cast<uint32_t>(k) * 4) != -1) continue;
        return k;
    }
    return -1;
}

// Buffer `from`'s planet data copied to buffer `to` (its size, its planet), `from` freed.
// orig: 0x00246b68
void map_buffer_move(int32_t to, int32_t from) {
    const uint32_t f = static_cast<uint32_t>(from) * 4, t = static_cast<uint32_t>(to) * 4;
    mem_copy128(lwu(MAP + 0x278 + t), lwu(MAP + 0x278 + f), lw(MAP + 0x2a4 + f) << 4);
    sw(MAP + 0x28c + t, lwu(MAP + 0x28c + f));
    sw(MAP + 0x2a4 + t, lwu(MAP + 0x2a4 + f));
    sw(MAP + 0x28c + f, 0xffffffffu);
}

// A buffer for a planet's read: a free one (from the last), else buffer 0's data moved to the
// first of 1..4 not being read and with memory; its index.
// orig: 0x00246ac8
int32_t map_buffer_for_read() {
    int32_t k = map_buffer_free(1);
    if (k != 0) return k;
    k = 1;
    if ((lwu(MAP + 0x290) & 0x1000) == 0 && lwu(MAP + 0x27c) != 0) {
        map_buffer_move(0, k);
        return k;
    }
    for (;;) {
        ++k;
        if (!(k < 5)) break;
        if ((lwu(MAP + 0x28c + static_cast<uint32_t>(k) * 4) & 0x1000) != 0) continue;
        if (lwu(MAP + 0x278 + static_cast<uint32_t>(k) * 4) == 0) continue;
        break;
    }
    map_buffer_move(0, k);
    return k;
}

// The buffer holding planet `p`'s data, -1 for none.
// orig: 0x00246c08
int32_t map_buffer_of(int32_t p) {
    for (int32_t i = 0; i < 5; ++i) {
        if (lwu(MAP + 0x278 + static_cast<uint32_t>(i) * 4) == 0) continue;
        if (lw(MAP + 0x28c + static_cast<uint32_t>(i) * 4) == p) return i;
    }
    return -1;
}

// Planet `p`'s place in the list of planets (*0x1602a0, 0-ended, at most 20), -1 if absent.
// orig: 0x00246d88
int32_t map_planet_index(int32_t p) {
    const uint32_t list = lwu(OV(0x001602a0));
    if (lw(list) == 0) return -1;
    for (int32_t i = 0;;) {
        if (lw(list + static_cast<uint32_t>(i) * 4) == p) return i;
        if (lw(list + static_cast<uint32_t>(i + 1) * 4) == 0) return -1;
        ++i;
        if (!(i < 0x14)) return -1;
    }
}

// The next planet to read ahead: the shown one's neighbours in the list (+1, -1, +2, -2, +3, -3;
// +0x100 with the map-o-matic) whose data no buffer holds; -1 for none.
// orig: 0x00246c50
int32_t map_planet_to_read() {
    const int32_t add = lbu(0x0013d4e1) != 0 ? 0x100 : 0;
    int32_t p = lw(MAP + 0x224) + add;
    if (map_buffer_of(p) == -1) return p;
    const int32_t cur = lw(MAP + 0x224);
    int32_t at = 0;
    if (cur < 0x14) {
        const uint32_t list = lwu(OV(0x001602a0));
        if (lw(list) != cur) {
            uint32_t q = list;
            do {
                q += 4;
                ++at;
            } while (lw(q) != cur);
        }
    }
    int32_t d = 1;
    while (d != 4) {
        const int32_t i = at + d;
        if (!(i < 0) && i < 0x14) {
            const int32_t v = lw(lwu(OV(0x001602a0)) + static_cast<uint32_t>(i) * 4);
            if (v != 0) {
                p = v + add;
                if (map_buffer_of(p) == -1) return p;
            }
        }
        d = d < 1 ? 1 - d : -d;
    }
    return -1;
}

// A buffer to reuse for the shown planet: one free (from the last; on the first planet), else
// the one whose planet is farthest in the list (its data dropped); 1 when the shown planet is
// not in the list.
// orig: 0x00246de0
int32_t map_buffer_to_reuse() {
    int32_t far = 0, best = -1;
    if (lw(MAP + 0x224) == 0) {
        for (int32_t k = 4; k >= 0; --k) {
            if (lwu(MAP + 0x278 + static_cast<uint32_t>(k) * 4) == 0) continue;
            if (k == lw(MAP + 0x2a0)) continue;
            if (lw(MAP + 0x28c + static_cast<uint32_t>(k) * 4) == -1) return k;
        }
    }
    const int32_t here = map_planet_index(lw(MAP + 0x224));
    if (here == -1) return 1;
    for (int32_t k = 0; k < 5; ++k) {
        if (lwu(MAP + 0x278 + static_cast<uint32_t>(k) * 4) == 0) continue;
        if (k == lw(MAP + 0x2a0)) continue;
        const int32_t p = lw(MAP + 0x28c + static_cast<uint32_t>(k) * 4);
        if (p == -1) return k;
        const int32_t d = abs_i(map_planet_index(p & 0xff) - here);
        if (far < d) {
            far = d;
            best = k;
        }
    }
    if (best == -1) best = 0;
    sw(MAP + 0x28c + static_cast<uint32_t>(best) * 4, 0xffffffffu);
    return best;
}

// The map's zoom (the right stick's y, 0.65..4) and scroll (the left stick, kept on the map)
// for the planet drawn; 1 with L3 / R3 held (0x500), else 0.
// orig: 0x00246fa8
int32_t map_zoom_scroll() {
    map_nop();
    if ((lwu(PAD + 0x1a4) & 0x500) != 0) return 1;
    if (lw(MAP + 0x24) == 0) return 0;
    if (lw(MAP + 0x228) < 0) return 0;
    const uint32_t zi = MAP + 0xb4 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    const float k = fpu_mul(lwc1(PAD + 0x104), u2f(0x3ca3d70a));
    swc1(zi, fpu_mul(lwc1(zi), fpu_sub(1.0f, k)));
    const uint32_t z1 = MAP + 0xb4 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    if (4.0f < lwc1(z1)) swc1(z1, 4.0f);
    const uint32_t z2 = MAP + 0xb4 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    if (lwc1(z2) < u2f(0x3f266666)) swc1(z2, u2f(0x3f266666));
    const uint32_t i4 = static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    const float z = lwc1(MAP + 0xb4 + i4);
    const float s = fpu_div(u2f(0x4a371b00), z);
    const int32_t dx = cvt_w_s(fpu_mul(lwc1(PAD + 0x108), s));
    const int32_t dy = cvt_w_s(fpu_mul(lwc1(PAD + 0x10c), s));
    sw(MAP + 0x104 + i4, static_cast<uint32_t>(lw(MAP + 0x104 + i4) + dx));
    const uint32_t yi = MAP + 0x154 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    sw(yi, static_cast<uint32_t>(lw(yi) + dy));
    const uint32_t j4 = static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    const float zz = lwc1(MAP + 0xb4 + j4);
    const int32_t hy = cvt_w_s(fpu_div(1280.0f, zz)) << 15;
    const int32_t hx = cvt_w_s(fpu_div(0.0f, zz)) << 15;
    const int32_t maxy = 0x10000000 - hy, maxx = 0x10000000 - hx;
    if (lw(MAP + 0x104 + j4) < hx) sw(MAP + 0x104 + j4, static_cast<uint32_t>(hx));
    const uint32_t x2 = MAP + 0x104 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    if (maxx < lw(x2)) sw(x2, static_cast<uint32_t>(maxx));
    const uint32_t y2 = MAP + 0x154 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    if (lw(y2) < hy) sw(y2, static_cast<uint32_t>(hy));
    const uint32_t y3 = MAP + 0x154 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    if (maxy < lw(y3)) sw(y3, static_cast<uint32_t>(maxy));
    return 0;
}

// The stream system's disc read waited for (`wait` nonzero: frames passed, the music and the
// sound kept going, until it is done); whether one is still going.
// orig: 0x002660c0
int32_t disc_read_sync(int32_t wait) {
    if (wait != 0) {
        while (lh(0x001516d8) != 0) {
            TODO_PORT(0x00122298, "kernel: vsync wait")(0);
            music_streams_frame();
            snd_release();
            snd_flush();
            snd_hold();
        }
        return lh(0x001516d8);
    }
    music_streams_frame();
    snd_release();
    snd_flush();
    snd_hold();
    return lh(0x001516d8);
}

// A marker's text (record `idx` of the level's: its text +0xa) to `out`, its one "%b" the ammo
// (+0xc: the weapon table 0x1c40b0's first word), another "%" "error".
// orig: 0x0024a618
void map_marker_text(int32_t idx, uint32_t out) {
    const uint32_t r = lwu(MAP + 0x20) + static_cast<uint32_t>(idx * 0x28);
    uint32_t s = sub_00203ad8(lh(r + 0xa));
    if (s == 0) return;
    uint32_t c = lbu(s);
    if (c != 0 && c != 0x25) {
        for (;;) {
            sb(out, c);
            ++s;
            c = lbu(s);
            ++out;
            if (c == 0) break;
            if (c == 0x25) break;
        }
    }
    if (lbu(s) == 0) {
        sb(out, lbu(s));
        return;
    }
    ++s;
    const uint32_t buf = ee_stack_alloc(0x40);
    if (lbu(s) == 0x62) {
        const uint32_t rec = lwu(MAP + 0x20) + static_cast<uint32_t>(idx * 0x28);
        std::vector<uint64_t> a(1, static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x001c40b0) + static_cast<uint32_t>(lh(rec + 0xc) * 0x18)))));
        libc_sprintf(buf, OV(0x0015fe38), a);
    } else {
        libc_sprintf(buf, OV(0x0015fe40), std::vector<uint64_t>());
    }
    ++s;
    for (uint32_t p = buf; lbu(p) != 0; ++p, ++out) sb(out, lbu(p));
    for (; lbu(s) != 0; ++s, ++out) sb(out, lbu(s));
    sb(out, 0);
    ee_stack_free(0x40);
}

// Planet `planet`'s marker records set up: the level's (0x186cc0, when its map is known,
// 0x13dd58) to +0x20; on the hero's level (`hero` nonzero) the scroll centred on the hero
// (0x13f3d0, 0x24a7e8); each record's place (its kind -1..-9 fixed, else the moby 0x1791b8 or
// the point 0x13d5b0 of the level), its shown flag (+0x24: its moby class, 0x13d5bc), its text's
// box sized (+0xe / +0x10) to the text.
// orig: 0x0024e248
void map_markers_setup(int32_t planet, int32_t hero) {
    if (planet < 0x13 && lbu(0x0013dd58 + static_cast<uint32_t>(planet)) != 0) sw(MAP + 0x20, lwu(OV(0x00186cc0) + static_cast<uint32_t>(planet) * 4));
    else sw(MAP + 0x20, 0);
    if (!(lwc1(0x0013f3d8) == 0.0f) && planet == lw(0x0015ed84) && hero != 0) {
        const int32_t idx = lw(OV(0x0015fdf8)) == 0 ? planet : planet + 0x64;
        const uint32_t xy = ee_stack_alloc(0x10);
        sub_0024a7e8(fp(xy), fp(xy + 4), idx, lwc1(0x0013f3d0), lwc1(0x0013f3d4));
        const int32_t x = cvt_w_s(fpu_mul(lwc1(xy), 4096.0f)), y = cvt_w_s(fpu_mul(lwc1(xy + 4), 4096.0f));
        sw(MAP + static_cast<uint32_t>(planet) * 4 + 0x104, static_cast<uint32_t>(x << 16));
        sw(MAP + static_cast<uint32_t>(planet) * 4 + 0x154, static_cast<uint32_t>(y << 16));
        ee_stack_free(0x10);
    }
    if (lwu(MAP + 0x20) == 0) return;
    if ((lhu(lwu(MAP + 0x20) + 4) & 4) == 0) {
        static const uint32_t fixed[9][2] = {{0x3e700000, 0x3e9a0000}, {0x3f0a0000, 0x3ebb0000}, {0x3f150000, 0x3f300000},
                                             {0x3f388000, 0x3f3a8000}, {0x3ec50000, 0x3ecb0000}, {0, 0},
                                             {0x3f5e0000, 0x3e6c0000}, {0x3ef60000, 0x3f120000}, {0x3f200000, 0x3f390000}};
        uint32_t off = 0;
        do {
            const uint32_t r = lwu(MAP + 0x20) + off;
            const int32_t k = lh(r);
            if (k < 0 && k >= -9 && k != -6) {
                swc1(r + 0x18, u2f(fixed[-k - 1][0]));
                swc1(r + 0x1c, u2f(fixed[-k - 1][1]));
            } else if (planet == lw(0x0015ed84)) {
                const uint32_t m = lwu(OV(0x001791b8) + static_cast<uint32_t>(k) * 4);
                if (m != 0) {
                    sub_0024a7e8(fp(r + 0x18), fp(r + 0x1c), planet, lwc1(m + 0x10), lwc1(m + 0x14));
                    swc1(off + lwu(MAP + 0x20) + 0x20, lwc1(lwu(OV(0x001791b8) + static_cast<uint32_t>(lh(r)) * 4) + 0x48));
                }
            } else {
                const uint32_t pt = 0x0013d5b0 + static_cast<uint32_t>(k << 4);
                sub_0024a7e8(fp(r + 0x18), fp(r + 0x1c), planet, lwc1(pt), lwc1(pt + 4));
                swc1(off + lwu(MAP + 0x20) + 0x20, lwc1(0x0013d5b8 + static_cast<uint32_t>(lh(r) << 4)));
            }
            off += 0x28;
        } while ((lhu(lwu(MAP + 0x20) + off + 4) & 4) == 0);
    }
    if ((lhu(lwu(MAP + 0x20) + 4) & 4) != 0) return;
    int32_t i = 0;
    do {
        const uint32_t r = lwu(MAP + 0x20) + static_cast<uint32_t>(i * 0x28);
        sw(r + 0x24, 0);
        sh(r + 4, lhu(r + 4) & 0xffefu);
        const int32_t cls = lh(r + 2);
        uint32_t shown;
        if (cls == -1) shown = 1;
        else shown = lh(lwu(OV(0x00186d20)) + static_cast<uint32_t>(cls * 0x28) + 0x24) == 1 ? 1u : 0u;
        sw(r + 0x24, shown);
        if ((lhu(r + 4) & 0x1000) != 0 && (lwu(0x0013d5bc + static_cast<uint32_t>(lh(r) << 4)) & 1) == 0) sw(r + 0x24, 0);
        const uint32_t r2 = static_cast<uint32_t>(i * 0x28) + lwu(MAP + 0x20);
        if (lh(r2 + 0xa) != 0) {
            const uint32_t sp = ee_stack_alloc(0xc0);  // the text +0, the box +0x80, its template +0xa0
            sh(r2 + 4, lhu(r2 + 4) | 0x10u);
            map_marker_text(i, sp);
            const uint32_t tpl = sp + 0xa0, box = sp + 0x80;
            libc_memset(tpl, 0, 0x18);
            sh(tpl + 2, lhu(r + 0x10));
            sh(tpl + 0x10, 0xf);
            sh(tpl + 6, lhu(r + 0xe));
            sh(tpl + 8, 4);
            sh(tpl + 0xa, 4);
            sh(tpl + 0x12, 4);
            for (uint32_t o = 0; o < 0x18; o += 2) sh(box + o, lhu(tpl + o));
            text_box_draw(box, 0x80ffa888ull, sp, -1);
            const int32_t h = lh(box + 0xe);
            sh(r + 0x10, lhu(box + 0xe) + 8u);
            bool grew = false;
            uint32_t wd;
            do {
                wd = lhu(box + 6);
                sh(box + 6, wd - 4u);
                text_box_draw(box, 0x80ffa888ull, sp, -1);
                if ((lh(box + 0xe) ^ h) != 0) grew = true;
                wd = lhu(box + 6);
            } while (!grew);
            sh(r + 0xe, wd + 4u);
            ee_stack_free(0xc0);
        }
        ++i;
    } while ((lhu(lwu(MAP + 0x20) + static_cast<uint32_t>(i * 0x28) + 4) & 4) == 0);
}

// The planet map's set up at the overlay's start: zooms and scrolls (0x183ef0), the planets'
// map rectangles (0x182810 -> scale / offset 0x182a70), the current planet; with a map for this
// level (0x1383a0, the map-o-matic's 0x138438): its data read from the disc, kept in the stash,
// unpacked into its image buffers, the marker buffers, the explored mask (0x141ec0 known or the
// tiles), the reveal circle.
// orig: 0x002464a8
void planet_map_init() {
    sw(MAP + 0x10, 1);
    for (uint32_t i = 0; i < 0x14; ++i) {
        const uint32_t src = OV(0x00183ef0) + i * 8;
        sw(MAP + 0x104 + i * 4, lwu(src));
        sw(MAP + 0x154 + i * 4, lwu(src + 4));
        swc1(MAP + 0xb4 + i * 4, u2f(0x3f266666));
    }
    const int32_t lv = lw(0x0015ed84);
    sw(MAP + 0x22c, 0xffffffffu);
    sw(MAP + 0x224, static_cast<uint32_t>(0x12 < lv ? 0 : lv));
    for (uint32_t k = 0; k < 0x13; ++k) {
        const uint32_t a = OV(0x00182810) + k * 0x20, o = OV(0x00182a70) + k * 0x10;
        float x0 = lwc1(a), y0 = lwc1(a + 4), u0 = lwc1(a + 0x10), v0 = lwc1(a + 0x14);
        const float x1 = fpu_sub(x0, lwc1(a + 8)), y1 = fpu_sub(y0, lwc1(a + 0xc));
        const float sx = fpu_div(fpu_sub(u0, lwc1(a + 0x18)), x1);
        const float sy = fpu_div(fpu_sub(v0, lwc1(a + 0x1c)), y1);
        x0 = fpu_mul(sx, x0);
        swc1(o + 4, sx);
        y0 = fpu_mul(sy, y0);
        swc1(o + 0xc, sy);
        swc1(o, fpu_sub(u0, x0));
        swc1(o + 8, fpu_sub(v0, y0));
    }
    uint32_t table;
    if (lbu(0x0013d4e1) != 0) {
        table = 0x00138438;
        sw(MAP + 0x230, 1);
    } else {
        table = 0x001383a0;
        sw(MAP + 0x230, 0);
    }
    const uint32_t e = (lwu(MAP + 0x224) << 3) + table;
    if (lwu(e + 4) == 0) {
        sw(MAP, 0);
        sw(MAP + 0x28, 0);
        sw(MAP + 8, 0);
        sw(MAP + 0xc, 0);
        sw(MAP + 4, 0);
        return;
    }
    sw(MAP + 0x28, 1);
    const uint32_t pk = lwu(OV(0x00173e08));
    const uint32_t buf = pk + 0x9a800;
    const uint32_t n = lwu(e + 4);
    disc_read_wait(buf, lwu(e), n);
    const int32_t qw = static_cast<int32_t>(n << 7);
    sw(MAP + 0x22c, static_cast<uint32_t>(stash_put(buf, qw, 0x4800, OV(0x001e8b00))));
    sw(MAP + 0x234, static_cast<uint32_t>(qw));
    wad_decompress(buf, pk);
    int32_t size = lw(pk + 4) - lw(pk);
    uint32_t m = hud_alloc(size);
    sw(MAP, m);
    mem_copy128(m, pk + lwu(pk), size);
    m = lwu(MAP) + 8;
    sw(MAP + 4, m);
    sw(MAP, m);
    size = lw(pk + 8) - lw(pk + 4);
    m = hud_alloc(size);
    sw(MAP + 0x14, m);
    mem_copy128(m, pk + lwu(pk + 4), size);
    sw(MAP + 8, hud_alloc(0x1000));
    for (uint32_t k = 0; k < 8; ++k) {
        sw(MAP + 0x30 + k * 0x10, 0xffffffffu);
        sw(MAP + 0x34 + k * 0x10, 0xffffffffu);
        sw(MAP + 0x38 + k * 0x10, lwu(MAP + 8) + (k << 9));
    }
    const uint32_t mask = hud_alloc(0x8000);
    sw(MAP + 0xc, mask);
    const uint32_t known = (lwu(MAP + 0x224) << 11) + 0x00141ec0;
    if (lbu(known) != 0) map_mask_unpack(mask, known, lwu(MAP + 0x14));
    else map_mask_from_tiles(mask, lwu(MAP), lwu(MAP + 4));
    for (int32_t r = 0; r < 0x20; ++r) {
        const float dy = fpu_sub(15.5f, static_cast<float>(r));
        const float dy2 = fpu_mul(dy, dy);
        for (int32_t c = 0; c < 0x20; ++c) {
            const float dx = fpu_sub(15.5f, static_cast<float>(c));
            const float d = fpu_add(fpu_mul(dx, dx), dy2);
            if (256.0f < d) {
                const int32_t q = (c < 0 ? c + 7 : c) >> 3;
                const uint32_t b = static_cast<uint32_t>(q + (r << 2)) + OV(0x00184394);
                sb(b, lbu(b) | (1u << ((c - (q << 3)) & 31)));
            }
        }
    }
}

// The map page's start (0x276368, states 0xb / 0xf): the markers of the planet shown, its
// buffers (the menu's 0xfc / 0x100 cursors), and the current level's map back from the stash
// (or, with the map-o-matic not yet used, read from the disc: 0x137b80 + 0x8b8).
// orig: 0x002468b8
void planet_map_page_start() {
    map_nop();
    map_markers_setup(lw(MAP + 0x224), 1);
    const uint32_t a = lwu(MS + 0xfc), b = lwu(MS + 0x100);
    const uint32_t buf1 = a + 0x9a800;
    const int32_t entry = lw(MAP + 0x22c);
    sw(MS + 0x100, b + 0x48000);
    sw(MS + 0xfc, buf1 + 0x48000);
    sw(MAP + 0x23c, a);
    sw(MAP + 0x278, 0);
    sw(MAP + 0x27c, buf1);
    sw(MAP + 0x280, b);
    sw(MAP + 0x284, 0);
    sw(MAP + 0x288, 0);
    for (uint32_t k = 0; k < 6; ++k) sw(MAP + 0x28c + k * 4, 0xffffffffu);
    if (entry == -1) {
        sw(MAP + 0x24, 0);
        return;
    }
    if (lw(MAP + 0x230) == 0 && lbu(0x0013d4e1) != 0) {
        const uint32_t e = 0x00137b80 + (lwu(MAP + 0x224) << 3);
        const uint32_t n = lwu(e + 0x8bc);
        stream_disc_read(buf1, lwu(e + 0x8b8), n);
        const int32_t qw = static_cast<int32_t>(n << 7);
        disc_read_sync(1);
        sw(MAP + 0x230, 1);
        sw(MAP + 0x234, static_cast<uint32_t>(qw));
        stash_write(lwu(MAP + 0x27c), lwu(MAP + 0x22c), 0, qw);
        sw(MAP + 0x228, 0xfffffffeu);
        sw(MAP + 0x2a8, static_cast<uint32_t>(qw));
        sw(MAP + 0x290, static_cast<uint32_t>(lw(0x0015ed84) + 0x100));
        sw(MAP + 0x24, 1);
        return;
    }
    const uint32_t idx = lwu(MAP + 0x22c);
    if (idx == 0xffffffffu) {
        sw(MAP + 0x24, 0);
        return;
    }
    sw(MAP + 0x2a8, lwu(MAP + 0x234));
    stash_read(lwu(MAP + 0x27c), idx, 0, lw(MAP + 0x234), 0);
    const int32_t lv = lw(0x0015ed84);
    sw(MAP + 0x228, 0xfffffffeu);
    sw(MAP + 0x290, static_cast<uint32_t>(lbu(0x0013d4e1) != 0 ? lv + 0x100 : lv));
    sw(MAP + 0x24, 1);
}

// orig: (helper) repeated in 0x00279aa8 / 0x002799e8: a finished planet read (+0x2a0) marked
// done (0x1000 cleared) when no disc read is going
static void map_read_done() {
    if (lh(0x001516d8) != 0) return;
    const int32_t k = lw(MAP + 0x2a0);
    if (k == -1) return;
    const uint32_t v = MAP + 0x28c + static_cast<uint32_t>(k) * 4;
    sb(MS + 0xcb, 0);
    sw(v, lwu(v) ^ 0x1000u);
    sw(MAP + 0x2a0, 0xffffffffu);
}

// The map page's update: zoom / scroll (0x246fa8); the exit keys; cross goes to the page
// 0x1b3778, circle to the planet's screen (M+0xc 3, back to this page); L1 / R1 the previous /
// next planet (the goals re-evaluated). When the planet shown changes and its data is in a
// buffer: unpacked (0x24d8d0), its explored mask (current level: the live one; else known
// 0x13dd58 / the tiles) merged, its images uploaded (0x245b60 x 3, the mask 9 x 9), the markers
// set up. Then the next disc read: a finished one marked done, the next planet's data read
// (from the stash for the current level, else the disc 0x1383a0 / 0x138438) into a buffer.
// orig: 0x00279aa8
int32_t planet_map_update(uint32_t w) {
    if (level_slots_map_row()) {  // enhancement: a level mod's row on the ship's map: its picture, not the base planet's (level_slots.h)
        map_read_done();
        level_slots_map_planet_upload();
        return 0;
    }
    map_zoom_scroll();
    int32_t prev = 0;
    if ((lwu(w + 0x34) & 0x40) == 0) {
        prev = lw(MAP + 0x224);
        if ((lwu(PAD + 0x1c4) & 0xd00) != 0 && lw(MS + 0x124) == 0) return 1;
        if ((lwu(PAD + 0x1c4) & 0x10) != 0) {
            const uint32_t parent = lwu(lwu(MS + 4) + 0x38);
            if (parent != 0) sw(MS + 8, parent);
            else if (lw(MS + 0x124) == 0) return -1;
        }
        if ((lwu(PAD + 0x1c4) & 0x40) != 0) sw(MS + 8, OV(0x001b3778));
        if ((lwu(PAD + 0x1c4) & 0x20) != 0 && lwu(MAP + 0x224) != 0) {
            sw(MS + 0xe4, lwu(MAP + 0x224));
            sw(MS + 0xf0, OV(0x001b3518));
            sw(MS + 0xf4, 0xb);
            sw(MS + 0xc, 3);
            moby_sound_start(0, 0x11, lwu(w + 0x14));
            return 0;
        }
        const int32_t i = map_planet_index(lw(MAP + 0x224));
        if (!(i < 0)) {
            if ((lwu(PAD + 0x1c4) & 8) != 0 && i < 0x13) {
                const uint32_t v = lwu(lwu(OV(0x001602a0)) + static_cast<uint32_t>(i) * 4 + 4);
                if (v != 0) sw(MAP + 0x224, v);
            }
            if ((lwu(PAD + 0x1c4) & 4) != 0 && i != 0) {
                const uint32_t v = lwu(lwu(OV(0x001602a0)) + static_cast<uint32_t>(i) * 4 - 4);
                if (v != 0) sw(MAP + 0x224, v);
            }
        }
        if (lw(MAP + 0x224) != prev) {
            moby_sound_start(1, 0x11, lwu(w + 0x14));
            goals_update();
        }
    }
    int32_t key = lw(MAP + 0x224);
    if (lbu(0x0013d4e1) != 0) key += 0x100;
    int32_t have = map_buffer_of(key);
    if (lw(MAP + 0x224) != lw(MAP + 0x228) && have != -1) {
        const int32_t reuse = map_buffer_to_reuse();
        if (lwu(MAP + 0x278) != 0 && reuse > 0) {
            map_buffer_move(reuse, 0);
            if (have == 0) have = reuse;
        }
        wad_decompress(lwu(MAP + 0x278 + static_cast<uint32_t>(have) * 4), lwu(MAP + 0x23c));
        const uint32_t hdr = lwu(MAP + 0x23c);
        const uint32_t img = hdr + lwu(hdr + 8);
        const uint32_t mask = hdr + lwu(hdr + 0xc);
        const uint32_t tmp = menu_buf_alloc(0);
        if (tmp != 0) {
            const uint32_t base = lwu(MAP + 0x23c);
            const uint32_t a1 = base + lwu(hdr) + 8;
            if (lw(MAP + 0x224) == lw(0x0015ed84)) {
                map_bytes_select(mask, img, mask, lwu(MAP + 0xc));
            } else {
                const int32_t pl = lw(MAP + 0x224);
                if (lbu(0x0013dd58 + static_cast<uint32_t>(pl)) != 0) map_mask_unpack(tmp, (static_cast<uint32_t>(pl) << 11) + 0x00141ec0, base + lwu(hdr + 4));
                else map_mask_from_tiles(tmp, a1, a1);
                map_bytes_select(mask, img, mask, tmp);
            }
        }
        if ((lwu(w + 0x34) & 0x80) == 0) {
            const uint32_t base = lwu(MAP + 0x23c);
            const uint32_t p0 = base + lwu(hdr + 0x10);
            const uint32_t p2 = base + lwu(hdr + 0x18);
            const uint32_t p1 = base + lwu(hdr + 0x14);
            const uint32_t pix = p0 + 0x20;
            sd(MAP + 0x258, image_upload_at(7, 7, pix, p0 + 0x420, lw(MAP + 0x244), lw(MAP + 0x248)));
            sd(MAP + 0x260, image_upload_at(7, 7, pix, p1 + 0x420, lw(MAP + 0x244), lw(MAP + 0x24c)));
            sd(MAP + 0x268, image_upload_at(7, 7, pix, p2 + 0x420, lw(MAP + 0x244), lw(MAP + 0x250)));
            if (tmp != 0) image_upload_at(9, 9, mask, mask, 0x3ff000, lw(MAP + 0x240));
            texture_uploads();
        }
        if (tmp != 0) menu_buf_free(tmp);
        sw(MAP + 0x278, img);
        sw(MAP + 0x228, lwu(MAP + 0x224));
        map_markers_setup(lw(MAP + 0x224), 0);
    }
    map_read_done();
    int32_t slot = -1;
    if (lh(0x001516d8) == 0) {
        slot = map_buffer_for_read();
        if (slot != -1) {
            const int32_t p = map_planet_to_read();
            if (p != -1) {
                const int32_t own = (lw(0x0015ed84) + lw(MAP + 0x230)) == 0 ? 0 : 0x100;
                const uint32_t s4 = static_cast<uint32_t>(slot) * 4;
                if (own == p) {
                    stash_read(lwu(MAP + 0x278 + s4), lwu(MAP + 0x22c), 0, lw(MAP + 0x234), 0);
                    sw(MAP + 0x2a4 + s4, lwu(MAP + 0x234));
                } else {
                    const uint32_t e = (p & 0x100) != 0 ? (static_cast<uint32_t>(p ^ 0x100) << 3) + 0x00138438 : (static_cast<uint32_t>(p) << 3) + 0x001383a0;
                    stream_disc_read(lwu(MAP + 0x278 + s4), lwu(e), lwu(e + 4));
                    sb(MS + 0xcb, 1);
                    sw(MAP + 0x2a4 + s4, static_cast<uint32_t>(static_cast<int32_t>(lwu(e + 4) << 11) >> 4));
                }
                sw(MAP + 0x28c + s4, static_cast<uint32_t>(p) | 0x1000u);
                sw(MAP + 0x2a0, static_cast<uint32_t>(slot));
            }
        }
    }
    const int32_t h = map_buffer_of(key);
    if (lw(MAP + 0x224) == lw(MAP + 0x228)) return 0;
    if (lh(0x001516d8) != 0) return 0;
    if (h != -1) return 0;
    if (slot != h) return 0;
    map_buffer_to_reuse();
    return 0;
}

// The map page's focus out: a finished read marked done; one going stopped (its buffer's
// planet forgotten).
// orig: 0x002799e8
int32_t planet_map_leave(uint32_t) {
    map_read_done();
    if (lh(0x001516d8) == 0) return 0;
    if (lbu(MS + 0xcb) == 0) return 0;
    disc_read_break();
    sb(MS + 0xcb, 0);
    sw(MAP + 0x28c + static_cast<uint32_t>(lw(MAP + 0x2a0)) * 4, 0xffffffffu);
    sw(MAP + 0x2a0, 0xffffffffu);
    return 0;
}

// The map's focus in: the planet to draw again (+0x228 -1).
// orig: 0x0027a710
int32_t planet_map_enter(uint32_t) {
    sw(MAP + 0x228, 0xffffffffu);
    return 0;
}

}  // namespace rac1
