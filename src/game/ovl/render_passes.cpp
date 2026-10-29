// Ported overlay code: the frame packet's passes (the DMA chain at 0x1611c0 the GS gets): its
// fixed packets, the texture uploads into the GS memory (the queue 0x16cd80), the draw callback
// lists, the screen bars. The 3D passes (VU1 programs) are the host renderer's.
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>
#include <game/render/mrp_fx.h>
#include <game/render/mrp_lod.h>
#include <game/render/render_view.h>
#include <game/ovl/aridia_classes_tr.h>
#include <game/ovl/batalia_part2.h>
#include <game/ovl/blarg_part2.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/novalis_draw_cbs.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/quartu_mobys.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/umbris_part2.h>
#include <game/ovl/veldin_finale_mobys.h>

#include <cstdlib>

namespace rac1 {

namespace {
constexpr OvConst<0x001611c0u> CUR{};  // the packet's cursor
// orig: (helper) repeated in 0x002a0d90 / 0x002a0df0 / 0x002a0e50 / 0x002012f0 / 0x002a0a58
void packet_tag(uint32_t w0, uint32_t addr, uint32_t w3) {
    const uint32_t p = lwu(CUR);
    sw(p, w0);
    sw(p + 4, addr);
    sw(p + 8, 0);
    sw(p + 0xc, w3);
    sw(CUR, p + 0x10);
}
}  // namespace

// orig: 0x002a0d90 — a REF of the fixed GS packet 0x1c2f00 (3 quadwords, DIRECT).
void packet_ref_1c2f00() { packet_tag(0x30000003, OV(0x001c2f00), 0x50000003); }
// orig: 0x002a0df0 — a REF of the fixed GS packet 0x1c24c0.
void packet_ref_1c24c0() { packet_tag(0x30000003, OV(0x001c24c0), 0x50000003); }
// orig: 0x002a0e50 — a REF of the fixed GS packet 0x1c24f0.
void packet_ref_1c24f0() { packet_tag(0x30000003, OV(0x001c24f0), 0x50000003); }
// orig: 0x002012f0 — a REF of the fixed GS packet 0x151900 (0x26 quadwords).
void packet_ref_151900() { packet_tag(0x30000026, 0x00151900, 0x50000026); }
// orig: 0x002a0a58 — a REF of `qwc` quadwords at `addr` (no VIF code).
void packet_ref(uint32_t addr, uint32_t qwc) { packet_tag(qwc | 0x30000000u, addr, 0); }
// orig: 0x002a13a0 — an empty CNT tag with the interrupt bit (its upper bits from `v`).
void packet_mark(uint32_t v) {
    const uint32_t p = lwu(CUR);
    sw(CUR, p + 0x10);
    sw(p, v - 0x70000000u);
    sw(p + 0xc, 0);
    sw(p + 4, 0);
    sw(p + 8, 0);
}
// orig: 0x00297e28 — (empty in this build)
void render_pass_297e28() {}

// The frame's fixed packets (REFs of 0x13cfc0, 0x13cf10) and the fog colour (0x16cc70..78).
// orig: 0x001f8450
void render_static_packets() {
    packet_tag(0x30000013, 0x0013cfc0, 0x50000013);
    packet_tag(0x3000000b, 0x0013cf10, 0x5000000b);
    sub_002a0ba8(0x3d, static_cast<uint64_t>(lw(OV(0x0016cc70))) | (static_cast<uint64_t>(lw(OV(0x0016cc74))) << 8) |
                           (static_cast<uint64_t>(lw(OV(0x0016cc78))) << 16));
}

// The texture uploads of the queue 0x16cd80 (0x15f418 entries: palette +0 (none: 0) into GS
// block +6, texels +8 into block +0xe, log2 width / height +0xc / +0xd): each a GS transfer
// (BITBLTBUF, TRXREG, TRXDIR from the template 0x160820) and a REF of its data.
// orig: 0x0024d760
void texture_uploads() {
    uint32_t e = OV(0x0016cd80);
    uint32_t p = lwu(CUR);
    int32_t n = lw(OV(0x0015f418));
    auto head = [&]() {
        for (uint32_t k = 0; k < 0x40; k += 4) sw(p + k, lwu(OV(0x00160820) + k));
        for (uint32_t k = 0x40; k < 0x50; k += 4) sw(p + k, 0);
        for (uint32_t k = 0; k < 0x10; k += 4) sw(p + 0x50 + k, lwu(OV(0x00160860) + k));
        sw(p + 0x48, 0x53);
    };
    auto tail = [&](uint32_t data, uint32_t qwc) {
        sw(p + 0x50, qwc | 0x8000);
        for (uint32_t k = 0x60; k < 0x70; k += 4) sw(p + k, 0);
        sw(p + 0x64, data);
        sw(p + 0x60, 0x30000000u + qwc);
        sw(p + 0x6c, 0x50000000u + qwc);
        p += 0x70;
    };
    while (n != 0) {
        --n;
        const uint32_t pal = lwu(e);
        const uint32_t pv = lhu(e + 4), pbp = lhu(e + 6);
        if (pal != 0) {
            head();
            sb(p + 0x27, pv);
            sh(p + 0x24, pbp);
            sw(p + 0x30, 0x10);
            sw(p + 0x34, 0x10);
            tail(pal, 0x40u >> ((pv >> 1) & 31));
        }
        const uint32_t tex = lwu(e + 8);
        const uint32_t sz = lhu(e + 0xc);
        head();
        sh(p + 0x24, lhu(e + 0xe));
        const uint32_t wl = sz & 0xff, hl = sz >> 8;
        const int32_t bw = static_cast<int32_t>(wl) - 6;
        if (bw > 0) sb(p + 0x26, 1u << (bw & 31));
        const uint32_t h = 1u << (hl & 31);
        sw(p + 0x34, h);
        sw(p + 0x30, 1u << (wl & 31));
        tail(tex, (h << (wl & 31)) >> 4);
        e += 0x10;
    }
    sw(CUR, p);
}

// The texture upload block's start: a tag reserved (0x15f410), the GS memory's dynamic area
// reset (0x15ee74 from 0x15ee78), the queue emptied, the cached TEX0s 0x16d180 (0x15f41c)
// cleared; with `keep` 0 the HUD textures' residence marks (above 0x15ee8c) too.
// orig: 0x001f9060
void texture_block_begin(int32_t keep) {
    const uint32_t p = lwu(CUR);
    sw(OV(0x0015f410), p);
    sw(CUR, p + 0x10);
    sw(0x0015ee74, lwu(0x0015ee78));
    sw(OV(0x0015f418), 0);
    for (int32_t i = 0; i < lw(OV(0x0015f41c)); ++i) sd(OV(0x0016d180) + static_cast<uint32_t>(i) * 0x10, 0);
    if (keep != 0) return;
    const uint32_t hud = OV(0x0017e4d8);
    for (int32_t i = 0; i < lw(lwu(hud + 0x18) + 0x44); ++i) {
        const uint32_t e = lwu(hud + 0x24) + static_cast<uint32_t>(i) * 8;
        if (!(static_cast<int32_t>(lhu(e + 4)) < (lw(0x0015ee8c) >> 8))) sh(e + 4, 0);
    }
    for (int32_t i = 0; i < lw(lwu(hud + 0x18) + 0x24); ++i) sh(lwu(hud + 0x28) + static_cast<uint32_t>(i) * 8 + 4, 0);
}

// The texture upload block's end: the reserved tag jumps to the uploads (with 0x16a01c), they
// jump back to the block's draws, which jump past the uploads.
// orig: 0x001f9178
void texture_block_end() {
    const uint32_t c = lwu(CUR);
    sw(OV(0x0015f414), c);
    sw(CUR, c + 0x10);
    const uint32_t a = lwu(OV(0x0015f410));
    sw(a, 0x20000000);
    sw(a + 4, lwu(CUR));
    sw(a + 8, 0);
    sw(a + 0xc, 0);
    if (lw(OV(0x0016a01c)) != 0) {
        texture_uploads();
        packet_ref_1c2f00();
    }
    const uint32_t q = lwu(CUR);
    sw(q, 0x20000000);
    sw(q + 4, lwu(OV(0x0015f410)) + 0x10);
    sw(q + 8, 0);
    sw(q + 0xc, 0);
    sw(CUR, q + 0x10);
    const uint32_t b = lwu(OV(0x0015f414));
    sw(b, 0x20000000);
    sw(b + 4, lwu(CUR));
    sw(b + 8, 0);
    sw(b + 0xc, 0);
}

namespace {
// orig: (host) whether the draw-list callback `fn` draws a full-screen 2D layer over the whole 4:3 frame (render_view_layer_note):
// class 832's, the Visibomb's out-of-range static (a grid of 32 x 32 sprites with random texels from effect texture 0x1e, from
// x -16 past the frame's right edge, thicker as its counter grows), on every level that carries it (the same code in each overlay)
bool screen_layer_fn(uint32_t fn) {
    static constexpr MobyFn kLayers[] = {
        nov_class832_draw_l1, class832_draw_l2,     kerwan_class832_draw_l3, eud_class832_draw_l4, ril_cb_30d8b8_l5, blg_class832_draw_l6,
        umb_class832_draw_l7, bat_class832_draw_l8, gas_class832_draw_l9,    orx_cb_2ce868_l10,    pok_cb_302e60_l11, hov_cb_2fa028_l12,
        gml_cb_2fcc08_l13,    olt_cb_2f6128_l14,    qua_cb_2de080_l15,       vf_cb_2e3f58_l18};
    const MobyFn f = ovl_moby_fn_resolve(fn);
    for (MobyFn l : kLayers)
        if (f == l) return true;
    return false;
}

// orig: (helper) repeated in 0x001f9430 / 0x001f94a8 / 0x001f9520 / 0x001f95e8
void draw_list_run(uint32_t count, uint32_t fns, uint32_t args) {
    static const bool inventory = std::getenv("RAC1_MRP_FX_INVENTORY") != nullptr;  // (host: testing)
    mrp_lod_draw_list_widen(true);  // (host: in a widened view's shadow block, the callbacks' clip matrix widened, mrp_lod.h)
    // (host: a widened view's full-screen 2D layers noted in the real block, not in a shadow run's)
    const bool layers = (g_view_widen > 1.0f || g_view_draw_widen > 1.0f) && mrp_lod_shadow_slot() < 0;
    for (int32_t i = 0; i < lw(count); ++i) {
        const uint32_t fn = lwu(fns + static_cast<uint32_t>(i) * 4);
        const bool layer = layers && screen_layer_fn(fn);
        const uint32_t at = lwu(CUR);  // (host: the callback's chain range: the MRP's program 7 surfaces, the notes below)
        if (inventory) mrp_fx_list_call(0, 0, 0);
        ovl_call_moby_fn(fn, lwu(args + static_cast<uint32_t>(i) * 4));
        if (inventory) mrp_fx_list_call(fn, at, lwu(CUR));
        {  // (host: its VU1 program 7 batches drawn by the MRP, mrp_fx.h; the argument's class when it is a moby, for the emissive surfaces)
            const uint32_t arg = lwu(args + static_cast<uint32_t>(i) * 4), m0 = lwu(OV(0x0015ffd8u)), m1 = lwu(OV(0x0015ffdcu));
            const uint32_t cls = arg >= m0 && arg < m1 && m0 != 0 && ((arg - m0) & 0xff) == 0 ? lhu(arg + 0xa6) : 0;
            mrp_fx_note_list(at, lwu(CUR), cls);
        }
        if (layer) render_view_layer_note(at, lwu(CUR));  // (host)
    }
    mrp_lod_draw_list_widen(false);  // (host)
}
}  // namespace

// The draw callback lists (function, argument) of the frame:
// orig: 0x001f9430 — 0x16d880 / 0x16d980 (count 0x15f424)
void draw_list_d880() { draw_list_run(OV(0x0015f424), OV(0x0016d880), OV(0x0016d980)); }
// orig: 0x001f94a8 — 0x16dc80 / 0x16dd80 (count 0x15f42c)
void draw_list_dc80() { draw_list_run(OV(0x0015f42c), OV(0x0016dc80), OV(0x0016dd80)); }
// orig: 0x001f9520 — 0x16de80 / 0x16df80 (count 0x15f430)
void draw_list_de80() { draw_list_run(OV(0x0015f430), OV(0x0016de80), OV(0x0016df80)); }
// orig: 0x001f95e8 — 0x16da80 / 0x16db80 (count 0x15f428)
void draw_list_da80() { draw_list_run(OV(0x0015f428), OV(0x0016da80), OV(0x0016db80)); }

// The screen's black bars (0x15f408 lines, growing by 0x15f404 up to 0x18): two sprites at the
// top and the bottom of the screen area 0x13e510..0x13e51c (the GS packet 0x1608e0 / 0x1608f0).
// orig: 0x001f9bd8
void screen_bars() {
    int32_t n;
    if (lw(OV(0x0015f404)) == 0) {
        n = lw(OV(0x0015f408)) - 1;
        if (lw(OV(0x0015f408)) == 0) return;
    } else {
        n = lw(OV(0x0015f408));
        if (lw(OV(0x0015f408)) < 0x18) n = lw(OV(0x0015f408)) + 1;
    }
    sw(OV(0x0015f408), static_cast<uint32_t>(n));
    if (n == 0) return;
    const uint32_t p = lwu(CUR);
    sw(p, 0x10000007);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000007);
    for (uint32_t k = 0; k < 0x10; k += 4) sw(p + 0x10 + k, lwu(OV(0x001608e0) + k));
    sh(p + 0x10, 0x8001);
    sd(p + 0x20, 0x104);
    sd(p + 0x28, 0x80000000);
    for (uint32_t k = 0; k < 0x10; k += 4) sw(p + 0x30 + k, lwu(OV(0x001608f0) + k));
    sh(p + 0x30, 0x8008);
    const int32_t x0 = lw(0x0013e510), y0 = lw(0x0013e514), x1 = lw(0x0013e518), y1 = lw(0x0013e51c);
    const uint64_t z = 0xfffff300000000ull;
    auto xy = [&](int32_t x, int32_t y) { return static_cast<uint64_t>(static_cast<int64_t>(x)) | (static_cast<uint64_t>(static_cast<int64_t>(y)) << 16) | z; };
    const int32_t d = n * 0x10;
    sd(p + 0x40, xy(x0, y0));
    sd(p + 0x48, xy(x0, y0 + d));
    sd(p + 0x50, xy(x1, y0));
    sd(p + 0x58, xy(x1, y0 + d));
    sd(p + 0x60, xy(x1, y1));
    sd(p + 0x68, xy(x1, y1 - d));
    sd(p + 0x70, xy(x0, y1));
    sd(p + 0x78, xy(x0, y1 - d));
    sw(CUR, p + 0x80);
}

}  // namespace rac1
