// Oracle batch 63: the gameplay frame on Veldin's real level data (needs the ISO).

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
namespace rac1::oracle {

void batch_63() {
    if (g_only_batch != 63) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 63;
    g_ignored_words = vcross_lanes();
    g_extra_noop_hooks = {OVF(0x001f3aa0), OVF(0x001f4490)};  // (0x201730, the screens, ported: task 0301)
    g_ignored_words.push_back(OV(0x00160188));  // the particle update's saved return address
    g_ignored_words.push_back(OV(0x0016018c));
    // (task 0392) moby_pose_matrices 0x2531e0 spills its caller's $t8 / $t9 whole (sq) to the scratchpad 0x3700 / 0x3710; the port
    // has their low words only (the upper 96 bits taken as 0). On Kerwan and Eudora the frame's code leaves t9's upper lanes set
    // before a pose (0x400 at 0x3718: gameplay_frame failed 68 of 120 cases on master): their upper halves are skipped
    for (uint32_t a : {0x70003708u, 0x7000370cu, 0x70003718u, 0x7000371cu}) g_ignored_words.push_back(a);

    static std::vector<uint8_t> snap;
    auto level = [boot] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            boot();
            // (task 0392) the particle type table 0x1b1e80, as the overlay's start fills it (overlay_init_tables; batch 70 does the
            // same): without it a particle alive in the compared frame calls type function 0 in the original (Blarg's level start
            // spawns some; the interpreter ran the null page into the base ELF)
            particle_table_init();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            snap.assign(ram, ram + 0x02000000);
            // the class update functions not ported yet: no-ops on both sides
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) {
                const uint32_t fn = ee_read<uint32_t>(m + 0x74);
                if (fn != 0 && !ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
            }
            // update functions of mobys spawned later (the wrench on the first frame, a crate's companion)
            for (uint32_t fn : {OVF(0x002a84c8u), OVF(0x002d89f0u)}) if (!ovl_moby_fn_ported(fn)) g_extra_noop_hooks.push_back(fn);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // (stage 5, task C2a) A whole frame may run mobys_in_sphere and sphere_mesh_collide, whose vertices and cell list the port keeps
    // on the stack: the words the original leaves at 0x70002000 .. and 0x70003800 .. after the frame (stale vertices, the cell
    // list's entries past the words later code wrote over) are left out of the two frame checks below. Only those two areas:
    // narrower than A2's collision ranges (oracle_ignore_collision_scratch), which would also hide the drawing's and the other
    // collision routines' scratch words over a whole frame. (Measured with ORACLE_DIFFS: 0x70002200 .. 0x70002210 and
    // 0x70003820 .. 0x7000383c differ, nothing else.)
    g_ignored_spr_ranges = {{0x70002000u, 0x70003000u}, {0x70003800u, 0x70003c00u}};
    check("gameplay_frame", 0x00297f78, 120, Ret::None, [level] {
        level();
        w32(OV(0x0015f5c4), ri(0, 3) == 0 ? u(ri(1, 6)) : 0u);
        w32(OV(0x0015f5c8), u(ri(0, 12)));
        w32(0x0013cadc, coin() ? R() : 0u);
        const uint32_t btn = (ri(0, 2) == 0 ? 0x800u : 0u) | (ri(0, 3) == 0 ? 0x500u : 0u) | (ri(0, 3) == 0 ? 0x200u : 0u) | (R() & 0xf0ffu);
        w32(0x0013cae0, btn); w32(0x0013cae4, coin() ? btn : 0u); w32(0x0013cae8, 0);
        w32(OV(0x0016c060), ri(0, 3) == 0 ? (R() & 0x1f) : 0xfu); w32(OV(0x0016c144), ri(0, 4) == 0 ? u(ri(0, 3)) : 0u);
        w32(OV(0x0016c06c), ri(0, 4) == 0 ? u(ri(1, 2)) : 0u); w32(OV(0x0016c070), u(ri(0, 6)));
        if (ri(0, 4) == 0) w32(0x0015ed84, 0xf);
        wf(0x0013ca40, ri(0, 2) ? 0.0f : rf(-1, 1)); wf(0x0013ca44, ri(0, 2) ? 0.0f : rf(-1, 1));
        return Args{};
    }, [](A) { gameplay_frame(); return uint64_t{0}; });
    g_ignored_spr_ranges.clear();
    check("help_hint_update", 0x00264328, 40, Ret::None, [level] {
        level();
        w32(OV(0x0015f590), u(ri(0, 2))); w32(OV(0x0015f594), u(ri(0, 1))); w32(OV(0x00161e08), u(ri(0, 1))); w32(OV(0x00161e04), coin() ? 0xffffffffu : u(ri(0, 3)));
        return Args{};
    }, [](A) { help_hint_update(); return uint64_t{0}; });
    check("draw_lists_reset", 0x00297e30, 10, Ret::None, [level] { level(); wrand(OV(0x0015f424), 5); return Args{}; }, [](A) { draw_lists_reset(); return uint64_t{0}; });
    check("moby_free_slots_count", 0x0024e7a8, 60, Ret::None, [level] {
        level();
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffdc)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100) {
            if (ri(0, 4) == 0) w8(m + 0x20, ri(0, 2) == 0 ? 0xffu : 0xfeu);
            ee_write<uint64_t>(m + 0x38, u(ri(0, 200)));
        }
        w32(OV(0x0015f5cc), u(ri(0, 200)));
        return Args{};
    }, [](A) { moby_free_slots_count(); return uint64_t{0}; });
    check("spin_timers_update", 0x001fea18, 40, Ret::None, [level] {
        level();
        for (uint32_t i = 0; i < 16; ++i) { w16(OV(0x0016ea40) + i * 0x20 + 0x10, u(ri(-1, 20))); w16(OV(0x0016ea40) + i * 0x20 + 0x12, R()); wf(OV(0x0016ea40) + i * 0x20 + 0x18, rf(-3, 3)); }
        return Args{};
    }, [](A) { spin_timers_update(); return uint64_t{0}; });
    check("shadow_direction_update", 0x0024fba8, 20, Ret::None, [level] { level(); return Args{}; }, [](A) { shadow_direction_update(); return uint64_t{0}; });
    check("frame_counters_advance", 0x00297e70, 40, Ret::None, [level] {
        level();
        w32(0x0013c940 + 0x1a0, coin() ? 0u : R()); for (uint32_t k = 0; k < 4; ++k) wf(0x0013c940 + 0x100 + k * 4, ri(0, 3) == 0 ? rf(-1, 1) : 0.0f);
        w32(OV(0x00160ff0), u(ri(0, 1000))); w32(OV(0x0015f598), u(ri(0, 1)));
        return Args{};
    }, [](A) { frame_counters_advance(); return uint64_t{0}; });
    // frames of play from the level's start with the wrench thrown (R1 + square) and caught:
    // the port plays the frames before, the frame compared is one around the catch
    auto pad = [](uint32_t f) {
        uint16_t held = 0;
        if (f >= 30 && (f % 120) < 30) held = (f % 120) < 4 ? 0x0088 : 0x0008;
        const uint16_t low = static_cast<uint16_t>(~held);
        const uint8_t pk[6] = {static_cast<uint8_t>(low >> 8), static_cast<uint8_t>(low & 0xff), 0x7f, 0x7f, 0x7f, 0x7f};
        pad_read_packet(0x0013c940, pk, 6);
    };
    const bool null_ram_t = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime (the wrench reads through null pointers)
    // (task 0392) A lone w lane is skipped (x, y, z equal on both sides): whole frames run every class, and Blarg's class 1035
    // (L6:0x2f7000) adds stack temporaries whose w it never writes into its tables 0x1db870 / 0x1db0f0 (vec3_add 0x1ff278 keeps
    // a1's w): stale stack lanes, different on the two sides
    g_ignore_lone_w_lanes = true;
    g_ignored_spr_ranges = {{0x70002000u, 0x70003000u}, {0x70003800u, 0x70003c00u}};  // (C2a: as for gameplay_frame above)
    check("gameplay_frame_throw", 0x00297f78, 12, Ret::None, [level, pad] {
        level();
        const uint32_t n = u(ri(185, 196));
        for (uint32_t f = 0; f < n; ++f) {
            pad(f);
            gameplay_frame();
            w32(OV(0x0015f3f8), ee_read<uint32_t>(OV(0x0015f3f8)) + 1);
        }
        pad(n);
        return Args{};
    }, [](A) { gameplay_frame(); return uint64_t{0}; });
    g_ignored_spr_ranges.clear();
    g_ee_null_is_ram = null_ram_t;
    g_ignore_lone_w_lanes = false;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
