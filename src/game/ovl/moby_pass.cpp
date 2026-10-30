// Ported overlay code: the moby pass around its draw loop (see moby_pass.h, moby_draw.h).
#include <game/ovl/moby_pass.h>
#include <game/ovl/game_skips.h>

#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/moby_draw.h>
#include <game/ovl/moby_shadows.h>
#include <game/ovl/moby_skin.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/port_stubs.h>
#include <runtime/vu/vu0.h>
#include <profiler/host_profiler.h>

#include <chrono>
#include <cstdlib>

namespace rac1 {

bool g_moby_mrp_skip = false;
bool g_moby_mrp_skip_verify = false;
unsigned g_moby_mrp_skip_parts = 0xf;
uint64_t g_moby_mrp_skip_next_bad = 0, g_moby_mrp_skip_next_ok = 0;
double g_prof_moby_skin_ms = 0, g_prof_moby_fill_ms = 0, g_prof_moby_tex_ms = 0, g_prof_moby_slots_ms = 0;
uint32_t g_moby_upload_chain_first = 0, g_moby_upload_chain_last = 0;
bool g_moby_pass_shadow_run = false;
bool g_moby_pass_mrp = false;
bool g_moby_pass_mrp_verify = false;

namespace {
// orig: (host) the per-stage timer of RAC1_FRAME_PROF
struct StageTimer {
    static bool on() {
        static const bool v = std::getenv("RAC1_FRAME_PROF") != nullptr;
        return v;
    }
    double& acc;
    std::chrono::steady_clock::time_point t0;
    explicit StageTimer(double& a) : acc(a) {
        if (on()) t0 = std::chrono::steady_clock::now();
    }
    // orig: (host) the timer's end
    ~StageTimer() {
        if (on()) acc += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
};
}  // namespace

// orig: 0x002a0700
void vif0_send(uint32_t chain) {
    // (waits for the channel, QWC 0, TADR, CHCR 0x145: source chain, tags carry VIF codes; waits)
    vu0::vif0_send_chain(chain & 0x0fffffffu, true);
}

// orig: 0x0024fe50
void moby_pass_start() {
    packet_ref(0x0010faa0, lhu(0x0010fa90));  // the VU1 moby programs (MPG)
    sw(OV(0x0015f5e0), 6);
    vif0_send(0x00100080);  // the VU0 micro programs (skinning)
    sub_002a0ba8(0x47, 0x5360b);
    const uint32_t p = lwu(OV(0x001611c0));
    const uint32_t vram = lwu(0x0015ee78);
    sw(OV(0x0015ffcc), p);  // a chain slot: the jump to the textures' upload (0x24f9c8)
    sw(0x0015ee74, vram);
    sw(OV(0x001611c0), p + 0x10);
    prof_mark_end(OV(0x0015ff90), 1);
    sw(OV(0x00160000), 0);
    sw(OV(0x001611c8), lwu(OV(0x0015f5fc)) + 0xffff0000u);
    sw(OV(0x0015ffd4), lwu(OV(0x0015f5f8)));
}

// orig: 0x00250038
void moby_pass() {
    moby_pass_start();
    if (lw(OV(0x0016a018)) != 0) {
        spr_moby_fill();
        const uint32_t r = moby_draw_loop(lwu(OV(0x0015ffd8)), lwu(OV(0x0015ffd4)), -1, 1);
        const bool full = lw(OV(0x001611c8)) < lw(OV(0x001611c0));
        sw(OV(0x0015ffd4), r);
        if (full) debug_printf_stub();  // (0x1e8eb0: the packet buffer overflowed)
    }
    moby_pass_end();
}

// A moby list drawn on its own (the full-screen menu's mobys: the menu's draw calls it between
// the pass's start and end): the draw loop on `count` mobys from `m` at the ring position
// 0x15ffd4, the scratchpad's moby state loaded before and saved after, the position kept less
// 0x10.
// orig: 0x0024ff08
void moby_draw_list(uint32_t m, int32_t count) {
    sub_002a0ba8(0x47, 0x5360b);
    // (FlushCache: no memory effect)
    spr_moby_load();
    const uint32_t r = moby_draw_loop(m, lwu(OV(0x0015ffd4)), count, 0);
    sw(OV(0x0015ffd4), r);
    spr_moby_save();
    sw(OV(0x0015ffd4), lwu(OV(0x0015ffd4)) - 0x10);
}

// orig: 0x0024ff88
void moby_pass_end() {
    prof_mark_end(OV(0x0015ffa0), 3);
    moby_textures_chain();
    prof_mark(OV(0x0015ffa0), 5);
    if (lw(OV(0x0016a018)) != 0) {
        {
            RAC1_PROF_ZONE(kGameMobyJoints);  // (host: the profiler's sub-stage)
            if (!g_skip_joints) moby_joints();
        }
        // orig: (host) the MRP skip: the sub-mesh colour fill writes the moby chain's vertex
        // colours, read by the GS path only (the MRP takes the colour from moby +0x90)
        if (lw(OV(0x0015fff8)) != 0 && !(g_moby_mrp_skip && (g_moby_mrp_skip_parts & 2))) {
            StageTimer st(g_prof_moby_fill_ms);
            moby_submesh_fill();
        }
    }
    prof_mark(OV(0x0015ffb0), 3);
    // orig: (host) not in the LOD slider's shadow run (g_moby_pass_shadow_run)
    if (lw(OV(0x0016a018)) != 0 && lw(OV(0x00160000)) != 0 && !g_moby_pass_shadow_run) {
        RAC1_PROF_ZONE(kGameMobyShadows);  // (host: the profiler's sub-stage)
        moby_shadows();
    }
}

// orig: 0x0024f9c8
void moby_textures_chain() {
    const uint32_t s0 = lwu(OV(0x001611c0));
    sw(OV(0x001611c0), s0 + 0x10);
    sw(lwu(OV(0x0015ffcc)), 0x20000000u);  // the start's slot: NEXT to the uploads
    sw(lwu(OV(0x0015ffcc)) + 4, lwu(OV(0x001611c0)));
    sw(lwu(OV(0x0015ffcc)) + 8, 0);
    sw(lwu(OV(0x0015ffcc)) + 0xc, 0);
    uint32_t v1 = lwu(OV(0x001611c0));
    g_moby_upload_chain_first = g_moby_upload_chain_last = 0;
    if (lw(OV(0x0016a018)) != 0 && lw(OV(0x0016a014)) != 0) {
        {
            StageTimer st(g_prof_moby_tex_ms);
            moby_textures(lwu(0x0015ee74));
        }
        // orig: (host) the MRP-only pass (moby_pass.h): the uploads' packets not written, one NEXT over their place
        const uint32_t up_end = lwu(OV(0x001611c0));
        if (g_moby_pass_mrp && !g_moby_pass_mrp_verify && up_end != s0 + 0x10) {
            sw(s0 + 0x10, 0x20000000u);
            sw(s0 + 0x14, up_end);
            sw(s0 + 0x18, 0);
            sw(s0 + 0x1c, 0);
        }
        packet_ref_1c2f00();
        v1 = lwu(OV(0x001611c0));
    }
    g_moby_upload_chain_first = s0 + 0x10;  // (host) the uploads' tags (after the tag s0, its NEXT jumps over them): from the first ...
    g_moby_upload_chain_last = v1;                           // ... to the NEXT back (0 uploads: the two are the same tag)
    sw(v1, 0x20000000u);  // NEXT back to the mobys' chain
    sw(lwu(OV(0x001611c0)) + 4, lwu(OV(0x0015ffcc)) + 0x10);
    sw(lwu(OV(0x001611c0)) + 8, 0);
    sw(lwu(OV(0x001611c0)) + 0xc, 0);
    sw(OV(0x001611c0), lwu(OV(0x001611c0)) + 0x10);
    sw(s0, 0x20000000u);  // the mobys' chain's end: NEXT past the uploads
    sw(s0 + 0xc, 0);
    sw(s0 + 4, lwu(OV(0x001611c0)));
    sw(s0 + 8, 0);
}

// orig: 0x002555c0
uint32_t moby_textures(uint32_t vram) {
    constexpr uint32_t SPR = 0x70000000u;
    uint32_t far = OV(0x0019a280), depth = SPR + 0x3a00, groups = OV(0x0019a600), lists = OV(0x0019a980);
    const uint32_t n = lwu(OV(0x0015ffc0));
    uint32_t t8 = lwu(OV(0x001611c0));
    uint32_t t9 = vram >> 8;
    for (uint32_t a = SPR + 0x3e00; a != SPR + 0x3f00; a += 4) sw(a, 0);
    // the groups drawn this frame (their nearest depth set) and the mips they need: 1 the small
    // one, 3 both (nearer than the group's distance); the groups listed at 0x19a600, -1 ended
    for (uint32_t i = 0; i != n;) {
        const int32_t d = lw(depth);
        depth += 4;
        const int32_t f = lw(far);
        far += 4;
        lists += 0x10;
        ++i;
        if (d == 0x40000000) continue;
        sw(groups, i - 1);
        if (static_cast<int32_t>((static_cast<uint32_t>(f) << 1) - static_cast<uint32_t>(d)) <= 0) continue;
        const uint32_t mips = static_cast<int32_t>(static_cast<uint32_t>(f) - static_cast<uint32_t>(d)) > 0 ? 3u : 1u;
        groups += 4;
        for (uint32_t t = lists - 0x10;; ++t) {
            const uint32_t k = lbu(t);
            if (k == 0xff) break;
            sb(SPR + 0x3e00 + k, lbu(SPR + 0x3e00 + k) | mips);
        }
    }
    sw(groups, 0xffffffffu);
    uint64_t g[5][2];  // the templates 0x160820..0x160860
    for (uint32_t q = 0; q < 5; ++q) {
        g[q][0] = ld(OV(0x00160820) + 0x10 * q);
        g[q][1] = ld(OV(0x00160828) + 0x10 * q);
    }
    // orig: (host) the MRP-only pass (moby_pass.h): the packets not written (the VRAM slots, the positions as before)
    const bool packets = !g_moby_pass_mrp || g_moby_pass_mrp_verify;
    // one GS upload (BITBLTBUF / TRXPOS / TRXREG / TRXDIR, then the image by REF) per mip
    auto upload = [&](uint32_t log2, uint32_t data, uint32_t slot) {
        if (!packets) {
            const uint32_t side = 1u << (log2 & 31);
            sh(slot, t9);
            t9 += ((side << (log2 & 31)) >> 4) >> 4;
            t8 += 0x70;
            return static_cast<int32_t>(t9 - 0x3ffb) > 0;
        }
        for (uint32_t q = 0; q < 4; ++q) {
            sd(t8 + 0x10 * q, g[q][0]);
            sd(t8 + 0x10 * q + 8, g[q][1]);
        }
        sd(t8 + 0x40, 0);
        sd(t8 + 0x48, 0);
        sd(t8 + 0x50, g[4][0]);
        sd(t8 + 0x58, g[4][1]);
        sw(t8 + 0x48, 0x53);
        sh(t8 + 0x24, t9);
        const int32_t bw = static_cast<int32_t>(log2) - 6;
        if (bw > 0) sb(t8 + 0x26, 1u << (bw & 31));
        const uint32_t side = 1u << (log2 & 31);
        sw(t8 + 0x30, side);
        sw(t8 + 0x34, side);
        const uint32_t qwc = (side << (log2 & 31)) >> 4;
        sw(t8 + 0x50, 0x8000u + qwc);
        sd(t8 + 0x60, 0);
        sd(t8 + 0x68, 0);
        sw(t8 + 0x64, data);
        sw(t8 + 0x60, 0x30000000u + qwc);
        sh(slot, t9);
        t9 += qwc >> 4;
        sw(t8 + 0x6c, 0x50000000u + qwc);
        t8 += 0x70;
        return static_cast<int32_t>(t9 - 0x3ffb) > 0;  // past the GS memory: undone, the end
    };
    uint32_t desc = OV(0x00199a80), vt = OV(0x00199e80);
    const uint32_t end = lwu(OV(0x0015ffc8)) + SPR + 0x3e00;
    for (uint32_t t = SPR + 0x3e00; t != end;) {
        const uint32_t m = lbu(t);
        ++t;
        const uint32_t tex = lwu(desc);
        desc += 4;
        sw(vt, 0);
        vt += 4;
        if (m == 0) continue;
        const uint32_t log2 = tex >> 28;
        uint32_t data = (tex << 4) >> 4;
        if ((m & 2) != 0 && upload(log2, data, vt - 4)) {
            t8 -= 0x70;
            break;
        }
        const uint32_t l1 = log2 - 1;
        data += ((1u << (l1 & 31)) << (l1 & 31)) << 2;
        if (upload(l1, data, vt - 2)) {
            t8 -= 0x70;
            break;
        }
    }
    sw(OV(0x001611c0), t8);
    return t9 << 8;
}

// orig: 0x0024fad0
void moby_texture_slots() {
    // orig: (host) the MRP skip: the TBP bits of the class packets' TEX0 are read by the GS path only
    if (g_moby_mrp_skip && (g_moby_mrp_skip_parts & 4)) return;
    StageTimer st(g_prof_moby_slots_ms);
    // the groups drawn (0x19a600, -1 ended): each class's (0x197300) texture entries (+0x20: 16
    // bytes each, texture ids ended by 0xff, +0xc the packets, sign bit the last entry); a
    // texture's VRAM slots (0x199e80: big / small mip, 0 not uploaded) into the TBP of its
    // packet's two TEX0 words (+0x30 / +0x40, 0x40 per texture)
    uint32_t a0 = OV(0x0019a600);
    if (lw(a0) < 0) return;
    uint32_t t4;
    do {
        t4 = a0 + 4;
        uint32_t t0 = lwu(lwu(OV(0x00197300) + (lwu(a0) << 2)) + 0x20);
        do {
            uint32_t v1 = lwu(t0 + 0xc) & 0x7fffffffu;
            uint32_t a3 = t0;
            if (lbu(t0) != 0xff) {
                uint32_t v0 = lbu(a3);
                do {
                    const uint32_t a1 = (v0 << 2) + OV(0x00199e80u);
                    int32_t h = static_cast<int16_t>(lh(a1));
                    if (h != 0) {
                        sw(v1 + 0x30, (lwu(v1 + 0x30) & 0xffffc000u) | static_cast<uint32_t>(h));
                    }
                    h = static_cast<int16_t>(lh(a1 + 2));
                    ++a3;
                    if (h != 0) sw(v1 + 0x40, (lwu(v1 + 0x40) & 0xffffc000u) | static_cast<uint32_t>(h));
                    v0 = lbu(a3);
                    v1 += 0x40;
                } while (v0 != 0xff);
            }
            const int32_t last = lw(t0 + 0xc);
            t0 += 0x10;
            if (last < 0) break;
        } while (true);
        a0 = t4;
    } while (lw(a0) >= 0);
}

// orig: 0x00255870
void moby_submesh_fill() {
    uint32_t at = OV(0x001ac200);
    const uint32_t end = at + lwu(OV(0x0015fff8));
    do {
        const uint32_t colour = lwu(at);
        uint32_t pk = lwu(at + 4), e = lwu(at + 8);
        int32_t n = lw(at + 0xc);
        do {
            uint32_t p = lwu(e + 4);
            int32_t k = lbu(pk + 0xf);
            for (;;) {
                sw(p, colour);
                --k;
                p += 4;
                if (k == 0) break;
                sw(p, colour);
                --k;
                p += 4;
                if (k <= 0) break;
            }
            --n;
            e += 8;
            pk += 0x10;
        } while (n > 0);
        at += 0x10;
    } while (at != end);
}

}  // namespace rac1
