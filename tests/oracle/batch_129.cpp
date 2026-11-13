// Oracle batch 129 (task 0368): the debris pieces' class update 0x2e3d88 (debris_piece.cpp; Novalis's copy L1:0x30cd18 with
// ORACLE_LEVEL=1) (needs the ISO). On the level's data after its first gameplay frame: a piece of one of the level's classes that
// run the update, spawned by debris_spawn (0x263fd8) near the hero (some at the floor, falling, so that they bounce), its state
// (0..5), frames left, flags (bits 0 / 1: fade or blow up, the enemy burst or the generic blast), spin, size and gravity varied,
// the velocity's and spin's w lanes (+0xc / +0x1c, which debris_spawn leaves stale in the original) any value half the time.

#include "ovl_oracle.h"

#include <game/ovl/debris_piece.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1::oracle {

void batch_129() {
    if (g_only_batch != 129) return;
    auto boot = [] { veldin_boot(); };
    g_batch = 129;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime (null pointers read as RAM, as on the EE)
    static std::vector<uint8_t> snap;
    auto level = [boot] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // the level's loaded classes whose update is this function (its address in the level's own overlay)
    static std::vector<uint32_t> classes;
    auto piece = [level] {
        level();
        if (classes.empty()) {
            const uint32_t fn = g_oracle_level == 0 ? 0x002e3d88u : ovl_code_lookup(0x002e3d88u);
            for (uint32_t c = 0; c < 0x800; ++c) {
                const uint32_t ci = ee_read<uint8_t>(OV(0x00197bc0) + c);
                if (ci != 0xff && ee_read<uint32_t>(OV(0x00197680) + ci * 4) == fn) classes.push_back(c);
            }
            std::printf("  debris classes loaded:");
            for (uint32_t c : classes) std::printf(" 0x%x", c);
            std::printf("\n");
        }
        if (classes.empty()) return 0u;
        const uint32_t cls = classes[static_cast<size_t>(ri(0, static_cast<int32_t>(classes.size()) - 1))];
        const uint32_t hero = ee_read<uint32_t>(0x001413d0);
        Vec4 p = ee_ref<Vec4>(0x0013f3d0);
        p.x += rf(-3, 3);
        p.y += rf(-3, 3);
        p.z += ri(0, 2) == 0 ? rf(-0.3f, 0.6f) : rf(0, 3);
        const Vec4 ang{rf(-3, 3), rf(-3, 3), rf(-3, 3), 0};
        const Vec4 zero{};
        const Vec4 vel = coin() ? zero : Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.5f, 0.3f), 0};
        const Vec4 spin = coin() ? zero : Vec4{rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), 0};
        const Vec4 size = ri(0, 2) == 0 ? Vec4{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(0.1f, 1.5f)} : zero;
        const uint32_t m = debris_spawn_v(hero, cls, p, ang, ri(0, 2) == 0 ? 0 : ri(1, 30), u(ri(0, 3)), vel, spin, size, rf(0.005f, 0.05f));
        if (m == 0) return 0u;
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        // (the velocity's and the spin's w lanes, +0xc / +0x1c: the original's debris_spawn leaves a stack word / a VU0 lane
        // there, the native one 0; any value half the time, so that a reader of them would show here)
        if (coin()) { w32(pv + 0xc, R()); w32(pv + 0x1c, R()); }
        w8(m + 0x20, u(ri(0, 5)));
        if (ri(0, 2) == 0) w32(pv + 0x30, u(ri(-1, 3)));
        seed_rand();
        return m;
    };
    check("debris_piece_update", 0x002e3d88, 400, Ret::None, [piece] { return Args{{piece()}, {}}; },
          [](A a) { if (a.i[0] != 0) debris_piece_update(a.i[0]); return uint64_t{0}; });
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
