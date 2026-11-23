// Oracle batch 130 (task 0365): the title scene's own render passes on the boot module's image (SCUS_971.99's sections, the
// ports relocated onto it: ovl_reloc_set_level(OVL_FRONT); batch 89's states). The original is the boot module's code at its own
// addresses, run in the EE interpreter; the ports are its own sky routine 0x22ae70 (src/game/boot/title_sky.cpp, transliterated)
// and its sky pass 0x1e9ab8, and the overlays' ports of the code it shares with the levels (front_reloc.txt): the tfrag / tie /
// shrub passes 0x2333a8 / 0x2358c8 / 0x228b38 (the overlays' 0x294240 / 0x295c80 / 0x286e60), their end routines 0x233308 /
// 0x235780 / 0x228a30 and the moby animation lerp slots 0x20cc60 (0x24f6e0). Per case the camera varied: the title's own camera at
// a random frame of its scene (0x1eaf88), or a random point looking at a random tie instance; the view set up (0x1f6e48), the
// zone mask and the packet cursor varied. Every word of EE RAM and the scratchpad compared (the frame's chain the passes build,
// their lighting lists, the sky's GIF packets, its random seed). The lighting routines' boot module copies are batch 101's.

#include "ovl_oracle.h"

#include <game/boot/boot_start.h>
#include <game/boot/front_end.h>
#include <game/boot/front_image.h>
#include <game/boot/title.h>
#include <game/boot/title_sky.h>
#include <game/lib/libmc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/world_cull.h>
#include <game/render/render_toggles.h>
#include <runtime/kernel/disc.h>

#include <cmath>

namespace rac1::oracle {

namespace {
constexpr uint32_t SCENE = 0x0018cb20;  // the scene (cutscene) state (the overlays' 0x16c860)

// 989snd's IOP (batch 89's): replies a function of the command, the stream system's disc reads done at once
void iop_stand_in() {
    auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
        const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
        return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
    };
    sif_rpc_register(0x123456, [result](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
        if (fno == 0x4d) {
            const uint32_t n = ee_read<uint32_t>(send);
            uint32_t p = send + 4;
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                if (id == 0x38) {
                    disc_read_ee(ee_read<uint32_t>(p + 0xc), ee_read<uint32_t>(p + 4), ee_read<uint32_t>(p + 8));
                    ee_write<uint32_t>(0x00137b10, 0);
                    ee_write<uint32_t>(0x00137b00, 0);
                }
                p += 4 + ((size + 3) & ~3u);
            }
            ee_write<uint32_t>(recv, 0xffffffffu);
            ee_write<uint32_t>(recv + 4 + n * 4, 0xffffffffu);
            return;
        }
        ee_write<uint32_t>(recv + 4, result(fno, send != 0 ? ee_read<uint32_t>(send) : 0u));
        ee_write<uint32_t>(recv, 0xffffffffu);
        ee_write<uint32_t>(recv + 8, 0xffffffffu);
    });
}

std::vector<uint8_t> g_snap;

// the title loaded and running (batch 89's state: the boot module started, the title's loop 70 frames in)
void title_state() {
    uint8_t* ram = ee_get_ram_base();
    if (!g_snap.empty()) {
        std::memcpy(ram, g_snap.data(), g_snap.size());
        return;
    }
    front_image_load();
    ovl_reloc_set_level(OVL_FRONT);
    mc_reset();
    boot_start();
    w16(0x001518d0, 512);
    w16(0x001518d2, 416);
    w32(0x0015ec8c, 1);
    w32(0x00137b00, 0);
    const uint32_t bank = 0x100000;  // the title's sound tables (the main after its intro; the bank's handle as the IOP gave it)
    ee_write<uint32_t>(0x001861e0 + 0x9c, bank);
    ee_write<uint32_t>(0x00186100 + 0xdc, bank);
    ee_write<uint32_t>(0x0015f634, 0x00186100);
    ee_write<uint32_t>(0x0015f630, 7);
    for (uint32_t o : {0x1cu, 0x3cu, 0x5cu, 0x7cu, 0x9cu, 0xbcu}) ee_write<uint32_t>(0x00186100 + o, bank);
    for (uint32_t o : {0x1cu, 0x3cu, 0x5cu, 0x7cu}) ee_write<uint32_t>(0x001861e0 + o, bank);
    title_loop_start();
    title_loop_ready();
    for (int i = 0; i < 70; ++i) title_loop_frame();
    g_snap.assign(ram, ram + 0x02000000);
}

// a frame's state for a pass: the camera (the title's at a random frame of its scene, or a random point looking at a random tie
// instance), the view set up from it, the zone mask, the packet cursor in a free area
void frame_state() {
    title_state();
    if (ri(0, 2) != 0) {
        w32(SCENE + 0x38, u(ri(0, 0x5f)));
        title_camera();
    } else {
        const uint32_t tb = ee_read<uint32_t>(OV(0x00160fc0));  // the tie instances' bounds (0x20 each: centre, radius)
        const int32_t nt = ee_read<int32_t>(OV(0x00160fcc));
        auto at = [&](float* c) {
            const uint32_t e = tb + u(ri(0, std::max(0, nt - 1))) * 0x20;
            for (int i = 0; i < 3; ++i) c[i] = ee_read<float>(e + 4 * u(i));
        };
        float a[3], b[3];
        at(a);
        at(b);
        const float dist = ri(0, 3) == 0 ? rf(0, 200) : rf(0, 40);
        float p[3], f[3];
        for (int i = 0; i < 3; ++i) p[i] = a[i] + rf(-1, 1) * dist;
        for (int i = 0; i < 3; ++i) f[i] = ri(0, 4) == 0 ? rf(-1, 1) : b[i] - p[i] + rf(-1, 1) * 10.0f;
        auto norm = [](float* v) {
            const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            for (int i = 0; i < 3; ++i) v[i] = l > 1e-6f ? v[i] / l : (i == 0 ? 1.0f : 0.0f);
        };
        norm(f);
        float up[3] = {rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), 1.0f}, l[3] = {f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]};
        norm(l);
        const float u2[3] = {l[1] * f[2] - l[2] * f[1], l[2] * f[0] - l[0] * f[2], l[0] * f[1] - l[1] * f[0]};
        for (int i = 0; i < 3; ++i) {
            wf(OV(0x00166dc0) + 4 * u(i), p[i]);
            wf(OV(0x00166fd0) + 4 * u(i), f[i]);
            wf(OV(0x00166fe0) + 4 * u(i), l[i]);
            wf(OV(0x00166ff0) + 4 * u(i), u2[i]);
        }
    }
    render_view_setup();
    w32(OV(0x001611c0), 0x01e20000u + 16u * u(ri(0, 64)));
    const int zk = ri(0, 3);  // the zone mask (as the title draw fills it: all visible; or some cleared)
    for (uint32_t o = 0; o < 0x80; ++o) w8(OV(0x00173d00) + o, zk == 0 ? (R() & 0xffu) : 0xffu);
}
}  // namespace

void batch_130() {
    if (g_only_batch != 130) return;
    g_batch = 130;
    if (g_oracle_level != 0) {
        skip_check("title_sky_shells", 0x0022ae70, "the boot module is not a level");
        return;
    }
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    const bool forced = g_world_chain_forced;
    g_world_chain_forced = true;  // (the title draw's passes keep their chain, as the original: the host's choice, task 0365)
    g_oracle_front = true;
    iop_stand_in();
    // the host's stand-ins on both sides (batch 89's): libmc (no card), the disc read 0x216828 (the overlays' 0x266040), the pad
    g_extra_hooks = {{0x00123d68, [](EeCpu& c) { c.set32(2, mc_get_info(static_cast<int32_t>(c.r32(4)), static_cast<int32_t>(c.r32(5)), c.r32(6), c.r32(7), c.r32(8))); }},
                     {0x00123c30, [](EeCpu& c) { c.set32(2, mc_sync(static_cast<int32_t>(c.r32(4)), c.r32(5), c.r32(6))); }},
                     {0x00216828, [](EeCpu& c) { c.set32(2, disc_read_wait(c.r32(4), c.r32(5), c.r32(6))); }},
                     {0x00217a10, [](EeCpu&) { pad_update(); }}};
    title_state();
    std::printf("batch 130 (boot module): %d tfrag entries, %d tie instances, %d shrub instances, sky %08x\n", ee_read<int32_t>(OV(0x00160f50)),
                ee_read<int32_t>(OV(0x00160fcc)), ee_read<int32_t>(OV(0x00160490)), ee_read<uint32_t>(0x0016045c));
    // (sanity, ORACLE_STATS=1) what a pass left: the chain's size
    auto stats = [](const char* name, uint32_t before) {
        if (std::getenv("ORACLE_STATS") != nullptr) std::printf("stats %s: chain %u bytes\n", name, ee_read<uint32_t>(OV(0x001611c0)) - before);
    };

    // ---- the sky: the boot module's own routine (its first call seeds the stars' rand and marks the sky +8; later ones not), then its
    // whole pass with the start / end routines (the overlays' 0x28b680 / 0x28b710)
    check("title_sky_shells", 0x0022ae70, 60, Ret::None, [] {
        frame_state();
        const uint32_t sky = ee_read<uint32_t>(0x0016045c);
        if (ri(0, 2) == 0) w16(sky + 8, 0);
        return Args{};
    }, [stats](A) {
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0));
        (void)front_sky_shells_tr();
        stats("sky shells", c0);
        return uint64_t{0};
    });
    check("title_sky", 0x001e9ab8, 40, Ret::None, [] {
        frame_state();
        const uint32_t sky = ee_read<uint32_t>(0x0016045c);
        if (ri(0, 2) == 0) w16(sky + 8, 0);
        return Args{};
    }, [](A) { title_sky(); return uint64_t{0}; });

    // ---- the world passes (the levels' code at the boot module's addresses)
    check("front:world_tfrag_pass", 0x002333a8, 40, Ret::None, [] { frame_state(); return Args{}; }, [stats](A) {
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0));
        world_tfrag_pass();
        stats("tfrag", c0);
        return uint64_t{0};
    });
    check("front:world_tie_pass", 0x002358c8, 40, Ret::None, [] { frame_state(); return Args{}; }, [stats](A) {
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0));
        world_tie_pass();
        stats("tie", c0);
        return uint64_t{0};
    });
    check("front:world_shrub_pass", 0x00228b38, 40, Ret::None, [] { frame_state(); return Args{}; }, [stats](A) {
        const uint32_t c0 = ee_read<uint32_t>(OV(0x001611c0));
        world_shrub_pass();
        stats("shrub", c0);
        return uint64_t{0};
    });
    // the end routines on the state the passes leave (made by the port: proved above)
    check("front:world_tfrag_end", 0x00233308, 20, Ret::None, [] {
        frame_state();
        world_tfrag_pass();
        return Args{};
    }, [](A) { world_tfrag_end(); return uint64_t{0}; });
    check("front:world_tie_end", 0x00235780, 20, Ret::None, [] {
        frame_state();
        world_tie_pass();
        return Args{};
    }, [](A) { world_tie_end(); return uint64_t{0}; });
    check("front:world_shrub_end", 0x00228a30, 20, Ret::None, [] {
        frame_state();
        world_shrub_pass();
        return Args{};
    }, [](A) { world_shrub_end(); return uint64_t{0}; });
    // the moby animation lerp slots (0x196cc0 relocated): some slots hold the scene's mobys, some flagged dead, some another byte +0x52
    check("front:moby_anim_lerp_slots_validate", 0x0020cc60, 30, Ret::None, [] {
        title_state();
        const int32_t n = lh(SCENE + 0x44);
        for (uint32_t i = 0; i < 16; ++i) {
            const uint32_t slot = OV(0x00196cc0u) + 4 * i;
            if (ri(0, 2) == 0 || n <= 0) {
                w32(slot, 0);
                continue;
            }
            const uint32_t m = lwu(SCENE + 0x178 + u(ri(0, n - 1)) * 4);
            w32(slot, m);
            if (ri(0, 2) == 0) w8(m + 0x20, R() & 0xffu);
            if (ri(0, 2) == 0) w8(m + 0x52, ri(0, 1) == 0 ? 0xffu : (R() & 0xffu));
        }
        return Args{};
    }, [](A) { moby_anim_lerp_slots_validate(); return uint64_t{0}; });

    g_extra_hooks.clear();
    g_oracle_front = false;
    g_world_chain_forced = forced;
    ovl_reloc_set_level(0);
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
