#include <game/lib/libgraph.h>

#include <runtime/ee/game_thread.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/kernel/ee_memory.h>

#include <cstdio>

namespace rac1 {

// orig: 0x00122330
int32_t gs_set_def_load_image(uint32_t lp, int32_t dbp, int32_t dbw, int32_t dpsm, int32_t x, int32_t y, int32_t w, int32_t h) {
    const int16_t sw = static_cast<int16_t>(w), sh = static_cast<int16_t>(h), spsm = static_cast<int16_t>(dpsm);
    const int32_t px = static_cast<int32_t>(sw) * sh;
    int64_t qwc = 0;
    switch (spsm) {
        case 0: case 0x30: qwc = px >> 2; break;
        case 1: case 0x31: qwc = (px * 3) >> 4; break;
        case 2: case 10: case 0x32: case 0x3a: qwc = px >> 3; break;
        case 0x13: case 0x1b: qwc = px >> 4; break;
        case 0x14: case 0x24: case 0x2c: qwc = px >> 5; break;
        default: break;
    }
    if (!(qwc < 0x8000)) {
        std::fprintf(stderr, "sceGsSetDefLoadImage: too big\n");  // the library's printf 0x1530b0
        return 0;
    }
    auto w64 = [lp](uint32_t o, uint64_t v) { ee_write<uint64_t>(lp + o, v); };
    // (int16 << 48) >> 16, arithmetic: the value in bits 32..47, its sign above
    auto hi32 = [](int32_t v) { return static_cast<uint64_t>((static_cast<int64_t>(static_cast<int16_t>(v)) << 48) >> 16); };
    auto s48 = [](int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(v))) << 48; };
    w64(0x58, 0);
    w64(0x50, static_cast<uint64_t>(qwc & 0x7fff) | 0x0800000000008000ull);
    w64(0x00, 0x1000000000000004ull);
    w64(0x08, 0xe);
    w64(0x10, hi32(dbp) | s48(dbw) | (static_cast<uint64_t>(static_cast<int64_t>(spsm)) << 56));
    w64(0x18, 0x50);
    w64(0x20, hi32(x) | s48(y));
    w64(0x28, 0x51);
    w64(0x30, static_cast<uint64_t>(static_cast<int64_t>(sw)) | hi32(sh));
    w64(0x38, 0x52);
    w64(0x48, 0x53);
    w64(0x40, 0);
    return 6;
}

// orig: 0x00122658
int32_t gs_exec_load_image(uint32_t lp, uint32_t src) {
    game_thread::wait_draw();  // orig: (host) the GS replica's memory is the drawing's until it is over (the game thread)
    gs::chain_join();  // orig: (host) the GS replica's chain run on its worker thread ended first
    gs::Gs& g = gs::gs_global();
    g.gif(static_cast<const uint8_t*>(ee_ram_ptr(lp)), 6);
    const uint32_t qwc = static_cast<uint32_t>(ee_read<uint64_t>(lp + 0x50) & 0x7fff);
    // (host: a source outside the 32 MB of EE RAM, as a level setup run without its files in a test reads from, is not sent)
    if (src >= 0x02000000u || qwc * 16u > 0x02000000u - src) return 0;
    g.gif(static_cast<const uint8_t*>(ee_ram_ptr(src)), qwc);
    return 0;
}

// orig: 0x00122518
int32_t gs_set_def_store_image(uint32_t lp, int32_t dbp, int32_t dbw, int32_t dpsm, int32_t x, int32_t y, int32_t w, int32_t h) {
    ee_write<uint64_t>(lp + 0x10, 0);  // (sq of the zero register: the quadword at +0x10 cleared before it is read)
    ee_write<uint64_t>(lp + 0x18, 0);
    const uint64_t old10 = ee_read<uint64_t>(lp + 0x10);
    const uint64_t old18 = ee_read<uint64_t>(lp + 0x18);
    const int64_t s_dbp = static_cast<int16_t>(dbp);
    const int64_t s_dbw = static_cast<int32_t>(static_cast<uint32_t>(dbw) << 16);
    const int64_t s_psm = (static_cast<int64_t>(static_cast<int16_t>(dpsm)) * (int64_t{1} << 48)) >> 24;
    const int64_t s_x = static_cast<int16_t>(x);
    const int64_t s_y = static_cast<int32_t>(static_cast<uint32_t>(y) << 16);
    const int64_t s_w = static_cast<int16_t>(w);
    const int64_t s_h = (static_cast<int64_t>(static_cast<int16_t>(h)) * (int64_t{1} << 48)) >> 16;
    uint64_t t4 = (((old10 & 0xffffffffffff8000ull) | 5u) | 0x8000u) & 0x0fffffffffffffffull;
    t4 |= 0x1000000000000000ull;
    const uint64_t t5 = (old18 & 0xfffffffffffffff0ull) | 0xeu;
    auto w64 = [lp](uint32_t o, uint64_t v) { ee_write<uint64_t>(lp + o, v); };
    ee_write<uint32_t>(lp + 4, 0x06008000u);
    w64(0x68, 0x53);
    ee_write<uint32_t>(lp + 8, 0x13000000u);
    ee_write<uint32_t>(lp + 0xc, 0x50000006u);
    w64(0x10, t4);
    w64(0x18, t5);
    w64(0x20, static_cast<uint64_t>(s_dbw | s_dbp) | static_cast<uint64_t>(s_psm));
    w64(0x28, 0x50);
    w64(0x30, static_cast<uint64_t>(s_y | s_x));
    w64(0x38, 0x51);
    w64(0x40, static_cast<uint64_t>(s_w | s_h));
    w64(0x48, 0x52);
    w64(0x58, 0x61);
    w64(0x60, 1);
    ee_write<uint32_t>(lp, 0);
    w64(0x50, 0);
    return 7;
}

// orig: (host) stands in for 0x001227d8
int32_t gs_exec_store_image(uint32_t lp, uint32_t dst) {
    game_thread::wait_draw();  // orig: (host) the GS replica's memory is the drawing's until it is over (the game thread)
    gs::chain_join();  // orig: (host) the GS replica's chain run on its worker thread ended first
    gs::Gs& g = gs::gs_global();
    const uint64_t bb = ee_read<uint64_t>(lp + 0x20), pos = ee_read<uint64_t>(lp + 0x30), rg = ee_read<uint64_t>(lp + 0x40);
    const uint32_t sbp = bb & 0x3fff, sbw = (bb >> 16) & 0x3f, spsm = (bb >> 24) & 0x3f;
    const uint32_t sx = pos & 0x7ff, sy = (pos >> 16) & 0x7ff;
    const uint32_t w = rg & 0xfff, h = (rg >> 32) & 0xfff;
    // (host) the host puts in the local memory what the GS would hold there (the frame it drew: the replica draws none)
    if (gs::StoreImageHook hook = gs::store_image_hook(); hook != nullptr) hook(sbp, sbw, spsm);
    // (host) the 32-bit frame copied whole, as the loop below would; not under a journal (its pages would not be saved: the
    // shadow runs, mrp_lod.h) nor for a null destination
    if (spsm == 0 && dst != 0 && dst < 0x02000000u && w * h * 4u <= 0x02000000u - dst && ee_detail::g_jseen == nullptr) {
        const uint8_t* vram = g.vram();
        uint8_t* out = static_cast<uint8_t*>(ee_ram_ptr(dst));
        for (uint32_t y = 0; y < h; ++y) {
            const uint32_t py = (sy + y) & 2047;
            for (uint32_t x = 0; x < w; ++x) {
                const uint32_t a = gs::Gs::addr32(sbp, sbw, (sx + x) & 2047, py);
                // (the local memory's pixel little-endian, as the PS2's; EE memory a host word: the same bytes on the PC)
                const uint32_t v = vram[a] | (vram[a + 1] << 8) | (vram[a + 2] << 16) | (static_cast<uint32_t>(vram[a + 3]) << 24);
                std::memcpy(out, &v, 4);
                out += 4;
            }
        }
        return 0;
    }
    uint32_t at = dst;
    bool half = false;  // (the 4-bit formats: two pixels a byte, the first in the low nibble)
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t v = g.read_pixel(spsm, sbp, sbw, (sx + x) % 2048, (sy + y) % 2048);
            switch (spsm) {
                case 1: ee_write<uint8_t>(at, static_cast<uint8_t>(v)); ee_write<uint8_t>(at + 1, static_cast<uint8_t>(v >> 8)); ee_write<uint8_t>(at + 2, static_cast<uint8_t>(v >> 16)); at += 3; break;
                case 2: case 10: ee_write<uint16_t>(at, static_cast<uint16_t>(v)); at += 2; break;
                case 0x13: case 0x1b: ee_write<uint8_t>(at, static_cast<uint8_t>(v)); at += 1; break;
                case 0x14: case 0x24: case 0x2c:
                    if (!half) ee_write<uint8_t>(at, static_cast<uint8_t>(v & 0xf));
                    else { ee_write<uint8_t>(at, static_cast<uint8_t>(ee_read<uint8_t>(at) | (v << 4))); ++at; }
                    half = !half;
                    break;
                default: ee_write<uint32_t>(at, v); at += 4; break;
            }
        }
    }
    return 0;
}

// orig: (host) stands in for 0x00120558 (sceGsSyncPath: the host's GS is done at once)
int32_t gs_sync_path(int32_t mode, int32_t timeout) {
    (void)mode;
    (void)timeout;
    return 0;
}

// orig: 0x00121a08
uint32_t gs_get_gparam() { return 0x00132d40; }

// orig: 0x00121f00
int32_t gs_zbuf_addr(int32_t psm, int32_t w, int32_t h) {
    const int32_t sw = static_cast<int16_t>(w), sh = static_cast<int16_t>(h);
    const uint32_t gp = gs_get_gparam();
    const int32_t bw = (sw + 0x3f > -1 ? sw + 0x3f : sw + 0x7e) >> 6;
    int32_t bh;
    if ((static_cast<int16_t>(psm) & 2) != 0) bh = (sh + 0x3f > -1 ? sh + 0x3f : sh + 0x7e) >> 6;
    else bh = (sh + 0x1f > -1 ? sh + 0x1f : sh + 0x3e) >> 5;
    const int32_t n = bw * bh;
    const bool one = (ee_read<uint64_t>(gp) & 0x0000ffff0000ffffull) == 1;
    return static_cast<int16_t>(static_cast<uint32_t>(n) << (one ? 16 : 17) >> 16);
}

// orig: 0x00121fc8
int32_t gs_set_def_draw_env(uint32_t env, int32_t psm, int32_t w, int32_t h, int32_t ztest, int32_t zpsm) {
    const int32_t p = static_cast<int16_t>(psm), sw = static_cast<int16_t>(w), sh = static_cast<int16_t>(h);
    const int32_t zt = static_cast<int16_t>(ztest), zp = static_cast<int16_t>(zpsm);
    auto w64 = [env](uint32_t o, uint64_t v) { ee_write<uint64_t>(env + o, v); };
    auto r64 = [env](uint32_t o) { return ee_read<uint64_t>(env + o); };
    w64(0x08, 0x4c);
    w64(0x00, (static_cast<uint64_t>(((sw + 0x3f) >> 6) & 0x3f) << 16) | (static_cast<uint64_t>(p & 0xf) << 24));
    w64(0x18, 0x4e);
    const int64_t za = static_cast<int16_t>(gs_zbuf_addr(p, sw, sh));
    uint64_t z = static_cast<uint64_t>(za) | (static_cast<uint64_t>(zp & 0xf) << 24);
    if (zt == 0) z |= 0x100000000ull;
    w64(0x10, z);
    const int64_t hh = static_cast<int16_t>(sh >> 1), hw = static_cast<int16_t>(sw >> 1);
    const uint64_t xyoff = (static_cast<uint64_t>(0x800 - hw) << 4) | (static_cast<uint64_t>(0x800 - hh) << 36);
    const uint64_t scissor = (static_cast<uint64_t>(static_cast<int64_t>(sw - 1)) << 16) | (static_cast<uint64_t>(static_cast<int64_t>(sh - 1)) << 48);
    const uint64_t prmc = r64(0x40) | 1, colc = r64(0x50) | 1;
    w64(0x28, 0x18);
    w64(0x20, xyoff);
    w64(0x38, 0x40);
    w64(0x30, scissor);
    w64(0x48, 0x1a);
    w64(0x40, prmc);
    w64(0x58, 0x46);
    w64(0x50, colc);
    w64(0x68, 0x45);
    w64(0x60, (p & 2) != 0 ? (r64(0x60) | 1) : (r64(0x60) & ~1ull));
    w64(0x78, 0x47);
    w64(0x70, zt != 0 ? ((static_cast<uint64_t>(zt & 3) << 17) | 0x10000) : 0x30000);
    return 8;
}

}  // namespace rac1
