// The overlay oracle's unit batches (1-58, 60, 62, 64): synthetic cases on the boot ELF + the
// Veldin overlay.

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <optional>

namespace rac1::oracle {

// (stage 5, task C2b) From batch 44 on, a moby's primitive records that name joints (all types but 1 and 3) are kept to the
// joint sets that have joints: a record's set bit (+2: bit 0 the set counted at col +0, bit 1 the one at +2) is cleared while
// that set's joint count is 0. Without joints the query does not pose the moby, and such a record reads its "joints" through
// a pointer still at the scratchpad: the words a collision query left there. Within one query those are its own chunk
// buffers' (CollChunkBufs::spr_word gives them: batch 43's single-query checks keep such records and prove it); in a check
// that makes several queries (the hero's) they are the query before's, which the port no longer leaves in the scratchpad.
// No class data does that: a probe of the nine tours found no such record read (2026-11-14). Called wherever the
// generators set the counts.
static bool g_strip_jointless = false;
static void strip_jointless_sets(uint32_t col) {
    if (!g_strip_jointless) return;
    const uint32_t have = (ee_read<uint16_t>(col) != 0 ? 1u : 0u) | (ee_read<uint16_t>(col + 2) != 0 ? 2u : 0u);
    const uint32_t n = ee_read<uint32_t>(col + 4) / 0x20;
    for (uint32_t r = 0; r < n && r < 64; ++r) {
        const uint32_t pr = col + 0x10 + 0x20 * r;
        const int8_t ty = static_cast<int8_t>(ee_read<uint8_t>(pr));
        if (ty == 1 || ty == 3) continue;
        const uint32_t more = ee_read<uint16_t>(pr + 2);
        w16(pr + 2, more & ~(3u & ~have));
    }
}

void batches_unit() {

    // ---- 0x001exxxx / 0x001fxxxx
    check("moby_table_find_by_uid", 0x001eb1f8, N, Ret::Int32, [] {
        for (int i = 0; i < 48; ++i) w16(g::MOBY_TABLE_167150 + i * 0xa0 + 0x86, u(ri(0, 63)));
        return Args{{u(ri(-2, 80))}, {}};
    }, [](A a) { return r32(moby_table_find_by_uid(static_cast<int32_t>(a.i[0]))); });
    check("debug_printf_stub", 0x001eb078, N, Ret::None, [] { return Args{{R(), R(), R(), R()}, {}}; },
          [](A) { debug_printf_stub(); return uint64_t{0}; });
    check("sub_001f35d8", 0x001f35d8, N, Ret::Int32, [] { w32(OV(0x00173e5c), R()); return Args{}; },
          [](A) { return ri32(sub_001f35d8()); });
    check("sub_001f3608", 0x001f3608, N, Ret::Int32, [] { w32(OV(0x00173e5c), R()); return Args{}; },
          [](A) { return ri32(sub_001f3608()); });
    check("sub_001f6c20", 0x001f6c20, 4, Ret::None, [] { return Args{}; }, [](A) { sub_001f6c20(); return uint64_t{0}; });
    check("sub_001f93e0", 0x001f93e0, N, Ret::None, [] { w32(OV(0x0015f424), u(ri(0, 0x45))); return Args{{R(), R()}, {}}; },
          [](A a) { sub_001f93e0(a.i[0], a.i[1]); return uint64_t{0}; });

    // ---- 0x0020xxxx
    check("sub_00203a28", 0x00203a28, 8, Ret::None, [] { wrand(0x00141968, 0x94 * 2); return Args{}; },
          [](A) { sub_00203a28(); return uint64_t{0}; });
    check("sub_002050b8", 0x002050b8, N, Ret::Int32, [] { return Args{{R()}, {}}; }, [](A a) { return r32(sub_002050b8(a.i[0])); });
    check("sub_00205110", 0x00205110, N, Ret::Int32, [] {
        w32(g::HERO_MOBY, R()); w32(0x001404d4, R()); w32(0x001403e0, R());
        return Args{{u(ri(-1, 7))}, {}};
    }, [](A a) { return r32(sub_00205110(static_cast<int32_t>(a.i[0]))); });
    check("fx_params_set_0", 0x0020a1e0, N, Ret::None, [] { return Args{{}, {rf(-9, 9), rf(-9, 9)}}; }, [](A a) { fx_params_set_0(a.f[0], a.f[1]); return uint64_t{0}; });
    check("fx_params_set_1", 0x0020a1f8, N, Ret::None, [] { return Args{{}, {rf(-9, 9), rf(-9, 9)}}; }, [](A a) { fx_params_set_1(a.f[0], a.f[1]); return uint64_t{0}; });
    check("fx_params_set_2", 0x0020a210, N, Ret::None, [] { return Args{{}, {rf(-9, 9), rf(-9, 9)}}; }, [](A a) { fx_params_set_2(a.f[0], a.f[1]); return uint64_t{0}; });
    check("fx_params_set_3", 0x0020a228, N, Ret::None, [] { return Args{{}, {rf(-9, 9), rf(-9, 9)}}; }, [](A a) { fx_params_set_3(a.f[0], a.f[1]); return uint64_t{0}; });
    check("sub_0020a500", 0x0020a500, 8, Ret::None, [] { wrand(g::PLAYER + 0x1000, 0x40); return Args{}; },
          [](A) { sub_0020a500(); return uint64_t{0}; });
    auto slots = [] {
        for (int s = 0; s < 8; ++s) {
            const uint32_t p = g::PLAYER + s * 0x50;
            w32(p + 0x10b4, u(ri(0, 3))); w32(p + 0x1090, R()); w32(p + 0x10b8, R());
        }
        return Args{{u(ri(0, 7))}, {}};
    };
    check("sub_0020d460", 0x0020d460, N, Ret::Int32, slots, [](A a) { return r32(sub_0020d460(static_cast<int32_t>(a.i[0]))); });
    check("sub_0020d498", 0x0020d498, N, Ret::Int32, slots, [](A a) { return ri32(sub_0020d498(static_cast<int32_t>(a.i[0]))); });
    check("sub_0020d568", 0x0020d568, N * 2, Ret::Int32, [] {
        static const int32_t st[] = {0x6a, 0x82, 0x76, 0x75, 0x74, 0x00, 0x83};
        w32(g::HERO_SUBSTATE, u(ri(0x0e, 0x15)));
        w32(g::HERO_STATE, u(st[ri(0, 6)]));
        return Args{};
    }, [](A) { return ri32(sub_0020d568()); });
    check("sub_0020d5b8", 0x0020d5b8, N, Ret::Int32, [] { w8(M + 0xa5, R()); return Args{{M, coin() ? 0xffu : (R() & 0xff)}, {}}; },
          [](A a) { return r32(sub_0020d5b8(a.i[0], a.i[1])); });
    check("sub_0020d658", 0x0020d658, N * 2, Ret::Float, [] {
        w32(g::PLAYER + 0x21ac, u(ri(0, 40)));
        w32(g::PLAYER + 0x21a8, u(ri(0, 31)));
        for (int i = 0; i < 32; ++i) wf(g::PLAYER + 0x2128 + 4 * i, rf(-5, 5));
        return Args{{u(ri(-5, 40))}, {}};
    }, [](A a) { return rf32(sub_0020d658(static_cast<int32_t>(a.i[0]))); });
    check("sub_0020da10", 0x0020da10, N, Ret::Int32, [] { return Args{{u(ri(-2, 6))}, {}}; },
          [](A a) { return ri32(sub_0020da10(static_cast<int32_t>(a.i[0]))); });

    // ---- 0x0021xxxx
    check("moby_is_class_0x47", 0x00211360, N, Ret::Int32, [] {
        w16(M + 0xa6, coin() ? 0x47 : R());
        return Args{{coin() ? M : 0u}, {}};
    }, [](A a) { return ri32(moby_is_class_0x47(a.i[0])); });
    check("sub_00211870", 0x00211870, N * 2, Ret::Int32, [] {
        wf(g::PLAYER + 0xaa8, rf(-2, 2)); wf(g::PLAYER + 0xaac, rf(0, 2));
        return Args{{}, {rf(-3, 3)}};
    }, [](A a) { return ri32(sub_00211870(a.f[0])); });
    check("sub_00216ec0", 0x00216ec0, N, Ret::None, [] {
        for (int i = 0; i < 8; ++i) { w16(g::PLAYER + 0x21d8 + 8 * i, ri(0, 3) ? R() | 1 : 0); w16(g::PLAYER + 0x21da + 8 * i, R()); }
        return Args{{sx16(R()), sx16(R()), sx16(R())}, {}};
    }, [](A a) { sub_00216ec0(static_cast<int16_t>(a.i[0]), static_cast<int16_t>(a.i[1]), static_cast<int16_t>(a.i[2])); return uint64_t{0}; });

    // ---- 0x0023xxxx
    check("sub_002321a8", 0x002321a8, 4, Ret::None, [] { return Args{}; }, [](A) { sub_002321a8(); return uint64_t{0}; });
    check("sub_002321c0", 0x002321c0, N * 2, Ret::Int32, [] {
        const int n = ri(0, 6);
        for (int i = 0; i < n; ++i) { w16(T + 4 * i, u(ri(0, 10))); w16(T + 4 * i + 2, R()); }
        w16(T + 4 * n, 0xffff);
        w32(0x001415cc, ri(0, 5) ? T : 0u);
        w32(OUT, R());
        return Args{{u(ri(-1, 12)), OUT}, {}};
    }, [](A a) { return ri32(sub_002321c0(static_cast<int32_t>(a.i[0]), hp<int32_t>(a.i[1]))); });
    check("sub_002325e0", 0x002325e0, N * 2, Ret::None, [] {
        w32(g::PLAYER + 0x2080, M);
        const uint32_t k = u(ri(0, 3));
        w8(M + 0x53, k); w32(M + 0x24, T); w32(T + k * 4 + 0x48, T2); w8(T2 + 0x10, R());
        return Args{{u(ri(-3, 5)), u(ri(-2, 300))}, {}};
    }, [](A a) { sub_002325e0(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("sub_00232628", 0x00232628, 4, Ret::None, [] { return Args{}; }, [](A) { sub_00232628(); return uint64_t{0}; });
    auto heroes = [] {
        w32(g::PLAYER + 0x2080, M); w32(g::PLAYER + 0x1620, coin() ? M2 : 0u);
        w16(M + 0x34, R()); w16(M2 + 0x34, R());
        return Args{};
    };
    check("hero_mobys_set_flag1", 0x00233210, N, Ret::None, heroes, [](A) { hero_mobys_set_flag1(); return uint64_t{0}; });
    check("hero_mobys_clear_flag1", 0x00233248, N, Ret::None, heroes, [](A) { hero_mobys_clear_flag1(); return uint64_t{0}; });
    check("sub_00233288", 0x00233288, N, Ret::None, [] { w16(g::PLAYER + 0x12ae, R()); return Args{{R(), R(), R(), R()}, {rf(-9, 9), rf(-9, 9)}}; },
          [](A a) { sub_00233288((int32_t)a.i[0], (int32_t)a.i[1], (int32_t)a.i[2], (int32_t)a.i[3], a.f[0], a.f[1]); return uint64_t{0}; });
    check("sub_00233e98", 0x00233e98, N * 2, Ret::Int32, [] {
        w32(0x00140408, u(ri(0, 15)));
        for (int i = 0; i < 16; ++i) { w16(OV(0x001c40b0) + i * 0x18 + 0xe, coin() ? 0u : R()); w32(0x0013d428 + i * 4, R()); }
        return Args{{coin() ? 0xffffffffu : u(ri(0, 15))}, {}};
    }, [](A a) { return r32(sub_00233e98(static_cast<int32_t>(a.i[0]))); });
    check("sub_00233f38", 0x00233f38, N, Ret::Int32, [] { w32(g::HERO_STATE, u(ri(0x60, 0x6a))); return Args{}; },
          [](A) { return ri32(sub_00233f38()); });
    check("sub_00234e48", 0x00234e48, N * 2, Ret::Int32, [] {
        const int n = ri(0, 8);
        for (int i = 0; i < n; ++i) { w16(T + 8 * i, u(ri(0, 12))); w16(T + 8 * i + 2, R()); }
        w16(T + 8 * n, 0xffff);
        w32(OV(0x0017e4f4), T);
        return Args{{coin() ? u(ri(0, 14)) : R()}, {}};
    }, [](A a) { return ri32(sub_00234e48(a.i[0])); });

    // ---- 0x0024xxxx / 0x0025xxxx
    check("sub_0024f698", 0x0024f698, N * 2, Ret::Int32, [] {
        for (int i = 0; i < 16; ++i) w32(OV(0x00196cc0) + 4 * i, ri(0, 4) ? u(ri(1, 40)) : 0u);
        return Args{{u(ri(1, 40))}, {}};
    }, [](A a) { return ri32(sub_0024f698(a.i[0])); });
    check("moby_set_color_packed", 0x002502d0, N, Ret::None, [] {
        return Args{{M, R() & 0xff, R() & 0xff, coin() ? R() : (R() & 0xff), coin() ? R() : (R() & 0xff)}, {}};
    }, [](A a) { moby_set_color_packed(a.i[0], sx(a.i[1]), sx(a.i[2]), sx(a.i[3]), sx(a.i[4])); return uint64_t{0}; });
    check("moby_set_color_hi", 0x002502f0, N, Ret::None, [] { wrand(M + 0x38, 2); return Args{{M, R() & 0xff, R() & 0xff, coin() ? R() : (R() & 0xff)}, {}}; },
          [](A a) { moby_set_color_hi(a.i[0], sx(a.i[1]), sx(a.i[2]), sx(a.i[3])); return uint64_t{0}; });
    check("moby_get_color_hi", 0x00250320, N, Ret::None, [] { wrand(M + 0x38, 2); return Args{{M, OUT, OUT + 4, OUT + 8}, {}}; },
          [](A a) { moby_get_color_hi(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
    check("sub_00250a40", 0x00250a40, N * 2, Ret::Int32, [] {
        for (int i = 0; i < 64; ++i) w8(OV(0x001ab7e0) + i, ri(0, 5) ? 0xffu : R());
        return Args{{u(ri(0, 400)), u(ri(1, 20))}, {}};
    }, [](A a) { return ri32(sub_00250a40(a.i[0], static_cast<int32_t>(a.i[1]))); });
    check("rand_mod", 0x00257b50, N, Ret::Int32, [] { seed_rand(); return Args{{u(coin() ? ri(1, 10) : ri(-5000, 5000) | 1)}, {}}; },
          [](A a) { return ri32(rand_mod(static_cast<int32_t>(a.i[0]))); });
    check("rand_range", 0x00257b90, N, Ret::Int32, [] { seed_rand(); const int32_t lo = ri(-100, 100); return Args{{u(lo), u(lo + ri(0, 300))}, {}}; },
          [](A a) { return ri32(rand_range(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]))); });
    check("rand_range_f", 0x00257be8, N, Ret::Float, [] { seed_rand(); return Args{{}, {rf(-50, 50), rf(-50, 50)}}; },
          [](A a) { return rf32(rand_range_f(a.f[0], a.f[1])); });
    check("rand_angle", 0x00257cb0, N, Ret::Float, [] { seed_rand(); return Args{}; }, [](A) { return rf32(rand_angle()); });
    check("sub_00259888", 0x00259888, N, Ret::None, [] { wrand(T, 4); return Args{{M, R(), R(), T}, {rf(-9, 9)}}; },
          [](A a) { sub_00259888(a.f[0], a.i[0], (int32_t)a.i[1], (int32_t)a.i[2], a.i[3]); return uint64_t{0}; });
    check("sub_0025a420", 0x0025a420, N * 2, Ret::Int32, [] {
        const uint32_t idx = coin() ? 0xffu : u(ri(0, 7));
        w8(M + 0xa4, idx);
        for (int i = 0; i < 8; ++i) { w32(OV(0x00178100) + i * 64 + 0x34, coin() ? M : M2); w32(OV(0x00178100) + i * 64 + 0x24, R() & R()); }
        return Args{{M, R() & R(), u(ri(0, 1))}, {}};
    }, [](A a) { return r32(sub_0025a420(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))); });
    check("lerp_f", 0x0025b6a8, N, Ret::Float, [] { return Args{{}, {rf(-99, 99), rf(-99, 99), rf(-1, 2)}}; },
          [](A a) { return rf32(lerp_f(a.f[0], a.f[1], a.f[2])); });
    auto pvars = [] {
        w16(M + 0x34, R()); w32(M + 0x78, T); wrand(T, 4);
        return Args{{coin() || coin() ? M : 0u}, {}};
    };
    check("moby_pvar_word0", 0x0025c338, N, Ret::Int32, pvars, [](A a) { return r32(moby_pvar_word0(a.i[0])); });
    check("moby_pvar_word2", 0x002603d0, N, Ret::Int32, pvars, [](A a) { return r32(moby_pvar_word2(a.i[0])); });
    check("moby_copy_hero_color", 0x0025d1b8, N, Ret::None, [] { w32(g::HERO_MOBY, M2); wrand(M2 + 0x38, 2); return Args{{M}, {}}; },
          [](A a) { moby_copy_hero_color(a.i[0]); return uint64_t{0}; });
    check("moby_in_range_class_500", 0x0025e3b8, N * 2, Ret::Int32, [] {
        w32(OV(0x00166c00u - 0x6c28), M - 0x100 + u(ri(0, 2)) * 0x100);
        w32(OV(0x00166c00u - 0x6c20), M - 0x100 + u(ri(0, 2)) * 0x100);
        w16(M + 0xa6, u(ri(480, 560)));
        return Args{{ri(0, 5) ? M : 0u}, {}};
    }, [](A a) { return ri32(moby_in_range_class_500(a.i[0])); });

    // ---- 0x0026xxxx .. 0x002axxxx
    check("sub_00262500", 0x00262500, N, Ret::None, [] { wrand(M + 0x130, 4); w32(M + 0x13c, coin() ? 0u : R()); return Args{{R(), M}, {}}; },
          [](A a) { sub_00262500(a.i[0], a.i[1]); return uint64_t{0}; });
    check("sub_00265828", 0x00265828, N, Ret::None, [] { wrand(0x001516d0 + 0x40, 16); return Args{{u(ri(0, 2))}, {}}; },
          [](A a) { sub_00265828(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("pad_stick_history", 0x00266e80, N * 2, Ret::Float, [] {
        w32(g::PAD + 0x190, u(ri(0, 40)));
        w16(g::PAD + 0x18e, u(ri(-5, 40)));
        for (int i = -40; i < 30; ++i) wf(g::PAD + 0x2d0 + 4 * i, rf(-1, 1));
        return Args{{u(ri(-3, 40))}, {}};
    }, [](A a) { return rf32(pad_stick_history(static_cast<int32_t>(a.i[0]))); });
    auto pslots = [] {
        for (int s = 0; s < 8; ++s) { const uint32_t p = 0x0013e550 + s * 0x70; w8(p + 0x74, u(ri(0, 8))); w32(p + 0x88, u(ri(0, 3))); w32(p + 0x8c, R()); }
        return Args{{u(ri(0, 3)), u(ri(-2, 7))}, {}};
    };
    check("sub_0028d8c0", 0x0028d8c0, N * 2, Ret::Int32, pslots, [](A a) { return ri32(sub_0028d8c0(a.i[0], static_cast<int32_t>(a.i[1]))); });
    check("sub_0028d918", 0x0028d918, N * 2, Ret::None, [] {
        for (int i = 0; i < 8; ++i) { const uint32_t p = 0x0013e550 + i * 0x70; w8(p + 0x74, u(ri(0, 8))); w32(p + 0x88, R()); w32(p + 0x8c, R()); }
        return Args{{u(ri(-2, 7))}, {}};
    }, [](A a) { sub_0028d918(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("sub_002a07f8", 0x002a07f8, N, Ret::None, [] {
        w32(OV(0x00173e04), R()); w32(OV(0x00173e08), R()); w32(OV(0x001611cc), R()); w32(OV(0x0015f578), R());
        return Args{};
    }, [](A) { sub_002a07f8(); return uint64_t{0}; });


    // ======== batch 2
    g_batch = 2;
    check("sub_001eb128", 0x001eb128, 8, Ret::None, [] { w32(OV(0x00166e00), T); wrand(T, 0x28); wrand(OV(0x00169110), 0xa0); return Args{}; },
          [](A) { sub_001eb128(); return uint64_t{0}; });
    check("sub_001ed630", 0x001ed630, N, Ret::Int32, [] { wfrand(M, 4, -50, 50); wfrand(M2, 4, -50, 50); wfrand(OUT, 4, -3, 3); return Args{{OUT, M, M2}, {}}; },
          [](A a) { return ri32(sub_001ed630(a.i[0], a.i[1], a.i[2])); });
    check("sub_00200fd8", 0x00200fd8, N, Ret::None, [] { w32(OV(0x001611c0), coin() ? T : 0u); return Args{}; }, [](A) { sub_00200fd8(); return uint64_t{0}; });
    check("sub_00201040", 0x00201040, N, Ret::None, [] { w32(OV(0x001611c0), T); w32(0x0015eeb8, R()); return Args{}; }, [](A) { sub_00201040(); return uint64_t{0}; });
    check("sub_002039a0", 0x002039a0, N * 2, Ret::None, [] {
        constexpr uint32_t b = 0x001516d0;
        wrand(OV(0x00179410), 10); w32(OV(0x00179410), coin() ? 0u : 1u); w32(OV(0x00179430), u(ri(0, 3)));
        w32(OV(0x0015f660), T); w16(b + 0x54, u(ri(0x7530, 0x7534))); w16(b + 0x5a, u(ri(0, 10)));
        for (int i = 0; i < 4; ++i) w32(T + 16 * i + 8, ri(0, 2) == 0 ? 0xffffffffu : u(ri(0, 4)));
        return Args{};
    }, [](A) { sub_002039a0(); return uint64_t{0}; });
    check("sub_00203a68", 0x00203a68, N * 2, Ret::Int32, [] {
        w32(OV(0x0017943c), u(ri(-1, 6))); w32(OV(0x0015f660), T);
        for (int i = 0; i < 8; ++i) w32(T + 16 * i + 4, u(ri(0, 6)));
        return Args{{u(ri(0, 6))}, {}};
    }, [](A a) { return ri32(sub_00203a68(static_cast<int32_t>(a.i[0]))); });
    check("sub_00204af8", 0x00204af8, N * 2, Ret::Int32, [] {
        for (int i = 0; i < 300; ++i) w16(OV(0x00179450) + 2 * i, u(ri(0, 300)));
        w32(OUT, R());
        return Args{{sx16(u(ri(0, 300))), u(ri(0, 1)), coin() ? OUT : 0u}, {}};
    }, [](A a) { return ri32(sub_00204af8(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), a.i[2])); });
    check("sub_002072c8", 0x002072c8, N, Ret::None, [] {
        static const uint32_t ms[] = {0, M, M2, T, T2};
        for (int s = 0; s < 7; ++s) { w32(g::PLAYER + s * 0x50 + 0x1090, ms[ri(0, 4)]); w32(g::PLAYER + s * 0x50 + 0x1094, ms[ri(0, 4)]); }
        w32(g::PLAYER + 0x2080, M3); wrand(M3 + 0x38, 2);
        return Args{};
    }, [](A) { sub_002072c8(); return uint64_t{0}; });
    check("sub_00207d00", 0x00207d00, N, Ret::None, [] {
        w16(g::PLAYER + 0x1eb2, u(ri(0, 1))); w32(OV(0x0015f424), u(ri(0, 0x45))); w32(g::PLAYER + 0x2080, R());
        return Args{};
    }, [](A) { sub_00207d00(); return uint64_t{0}; });
    auto slot_kind = [] {
        for (int s = 0; s < 8; ++s) { const uint32_t p = g::PLAYER + s * 0x50; w32(p + 0x10b4, ri(0, 3) ? 2u : 1u); w32(p + 0x10b8, u(ri(0, 0x20))); }
        return Args{{u(ri(0, 7))}, {}};
    };
    check("sub_0020d4d0", 0x0020d4d0, N * 2, Ret::Int32, slot_kind, [](A a) { return ri32(sub_0020d4d0(static_cast<int32_t>(a.i[0]))); });
    check("sub_0020d518", 0x0020d518, N * 2, Ret::Int32, slot_kind, [](A a) { return ri32(sub_0020d518(static_cast<int32_t>(a.i[0]))); });
    check("sub_0020d540", 0x0020d540, N * 2, Ret::Int32, slot_kind, [](A a) { return ri32(sub_0020d540(static_cast<int32_t>(a.i[0]))); });
    check("sub_0020d5d0", 0x0020d5d0, 400, Ret::Int32, [] { wf(0x0013fdf8, rf(0, 50)); w32(OUT, R()); return Args{{u(ri(0x10, 0x72)), OUT}, {}}; },
          [](A a) { return ri32(sub_0020d5d0(static_cast<int32_t>(a.i[0]), hp<float>(a.i[1]))); });
    check("sub_0020fd80", 0x0020fd80, N * 2, Ret::None, [] { return Args{{u(ri(0, 30))}, {}}; }, [](A a) { sub_0020fd80(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("sub_0020fde0", 0x0020fde0, N * 2, Ret::None, [] { return Args{{u(ri(0, 30))}, {}}; }, [](A a) { sub_0020fde0(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("sub_0020fe30", 0x0020fe30, N * 4, Ret::Int32, [] {
        w32(g::HERO_STATE, ri(0, 3) ? 0x10u : 0x66u); w32(OV(0x0015f5c4), ri(0, 3) ? 0u : 1u); w8(g::PLAYER + 0x20a4, ri(0, 3) ? 0u : 1u);
        w32(g::HERO_SUBSTATE, u(ri(0, 6))); w8(g::PLAYER + 0x12e4, u(ri(0, 1)));
        return Args{};
    }, [](A) { return ri32(sub_0020fe30()); });
    check("sub_00210c80", 0x00210c80, N, Ret::None, [] { wfrand(M, 4, -9, 9); return Args{{M, u(ri(0, 1))}, {rf(-3, 3), rf(-3, 3)}}; },
          [](A a) { sub_00210c80(a.i[0], static_cast<int32_t>(a.i[1]), a.f[0], a.f[1]); return uint64_t{0}; });
    check("sub_00211830", 0x00211830, N, Ret::Float, [] { wfrand(0x00141070, 4, -1.5f, 1.5f); return Args{}; }, [](A) { return rf32(sub_00211830()); });
    check("sub_00213350", 0x00213350, N, Ret::Float, [] { wf(g::PLAYER + 0x98, rf(-4, 4)); wfrand(M, 4, -9, 9); return Args{{M}, {}}; },
          [](A a) { return rf32(sub_00213350(a.i[0])); });
    check("sub_00213de8", 0x00213de8, N, Ret::None, [] { wfrand(0x0013f400, 0x40, -9, 9); return Args{{}, {rf(-2, 2)}}; },
          [](A a) { sub_00213de8(a.f[0]); return uint64_t{0}; });
    check("sub_002322c8", 0x002322c8, N * 2, Ret::None, [] {
        w32(g::PLAYER + 0x2278, coin() ? M : 0u); w32(g::PLAYER + 0x227c, ri(0, 3) ? T : 0u);
        const int n = ri(0, 4);
        for (int i = 0; i < n; ++i) { w16(T + 4 * i, u(ri(0, 5))); w16(T + 4 * i + 2, R()); }
        w16(T + 4 * n, 0xffff);
        w32(g::PLAYER + 0x2080, M2); w8(M2 + 0x53, u(ri(0, 5))); wf(g::PLAYER + 0xa90, rf(-9, 9));
        return Args{};
    }, [](A) { sub_002322c8(); return uint64_t{0}; });
    check("sub_00232640", 0x00232640, N, Ret::None, [] { return Args{{coin() ? 0xffffffffu : R()}, {}}; },
          [](A a) { sub_00232640(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    auto cam = [] { wfrand(g::PLAYER, 0x24, -2, 2); w8(g::PLAYER + 0x20b3, u(ri(0, 4))); wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9); };
    check("sub_00233660", 0x00233660, N, Ret::None, [cam] { cam(); return Args{{M}, {rf(-9, 9), rf(-9, 9), rf(-9, 9)}}; },
          [](A a) { sub_00233660(a.i[0], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("sub_00233b20", 0x00233b20, N * 2, Ret::None, [cam] { cam(); return Args{{M, coin() ? M : M2}, {rf(-9, 9)}}; },
          [](A a) { sub_00233b20(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    check("sub_00233ba0", 0x00233ba0, N * 2, Ret::None, [cam] { cam(); return Args{{M, coin() ? M : M2}, {rf(-9, 9)}}; },
          [](A a) { sub_00233ba0(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    check("sub_00233d30", 0x00233d30, N * 2, Ret::Float, [cam] { cam(); return Args{{M}, {}}; }, [](A a) { return rf32(sub_00233d30(a.i[0])); });
    check("sub_00233db8", 0x00233db8, N * 2, Ret::Int32, [] {
        w32(0x00140408, u(ri(0, 15)));
        for (int i = 0; i < 16; ++i) { w16(OV(0x001c40b0) + i * 0x18 + 0xe, ri(0, 3) ? 1u : 0u); w32(0x0013d428 + i * 4, u(ri(0, 100))); w32(0x0013dea0 + i * 4, R()); }
        return Args{{coin() ? 0xffffffffu : u(ri(0, 15)), u(ri(0, 100))}, {}};
    }, [](A a) { return ri32(sub_00233db8(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]))); });
    check("sub_00235988", 0x00235988, N, Ret::None, [] {
        const int n = ri(0, 8);
        for (int i = 0; i < n; ++i) { w16(T + 8 * i, u(ri(0, 12))); w16(T + 8 * i + 2, R()); w32(T + 8 * i + 4, R()); }
        w16(T + 8 * n, 0xffff); wrand(T + 8 * n + 2, 3);
        w32(OV(0x0017e4f4), T); wrand(M, 0x14);
        return Args{{M, u(ri(0, 14))}, {}};
    }, [](A a) { sub_00235988(a.i[0], a.i[1]); return uint64_t{0}; });
    auto recs = [] {
        for (int i = 0; i < 13; ++i) { const uint32_t e = OV(0x0017dc50) + i * 0x90; w32(e + 0x64, u(ri(0, 20))); w32(e + 0x68, ri(0, 2) ? 0u : 1u); w32(e + 0x7c, u(ri(0, 50))); wrand(e, 1); wrand(e + 0x24, 1); }
        return Args{{u(ri(0, 20)), u(ri(0, 50))}, {}};
    };
    check("sub_002359f8", 0x002359f8, N * 2, Ret::Int32, recs, [](A a) { return r32(sub_002359f8(a.i[0], a.i[1])); });
    check("sub_00235e18", 0x00235e18, N * 2, Ret::Int32, recs, [](A a) { return ri32(sub_00235e18(a.i[0], static_cast<int32_t>(a.i[1]))); });
    check("sub_0024f590", 0x0024f590, N, Ret::None, [] {
        wrand(T, 12); w8(T + 1, ri(0, 3) ? 0u : 1u);
        w32(M + 0x24, T2); w32(T2 + 0x1c, T3); w32(M + 0x64, R());
        for (int i = 0; i < 4; ++i) w32(T3 + i * 4 + 4, T4 + i * 0x20);
        for (int i = 0; i < 4; ++i) { w8(T4 + i * 0x20, u(ri(0, 4))); for (int k = 1; k < 12; ++k) w8(T4 + i * 0x20 + k, R()); }
        return Args{{M, u(ri(0, 3)), T}, {}};
    }, [](A a) { sub_0024f590(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    check("sub_00250aa0", 0x00250aa0, N * 2, Ret::Int32, [] {
        for (int i = 0; i < 16; ++i) w32(OV(0x001ab7e0) + 4 * i, ri(0, 2) ? 0xffffffffu : (R() | R()));
        w32(OV(0x001ab7e0) + 4 * 16, 0);
        return Args{{u(ri(1, 8))}, {}};
    }, [](A a) { return ri32(sub_00250aa0(static_cast<int32_t>(a.i[0]))); });
    check("rand_range_signed_f", 0x00257c48, N, Ret::Float, [] { seed_rand(); return Args{{}, {rf(-50, 50), rf(-50, 50)}}; },
          [](A a) { return rf32(rand_range_signed_f(a.f[0], a.f[1])); });
    check("approach_f", 0x0025b868, N * 2, Ret::Float, [] { wf(OUT, rf(-9, 9)); return Args{{OUT}, {rf(-9, 9), rf(-2, 5)}}; },
          [](A a) { return rf32(approach_f(a.i[0], a.f[0], a.f[1])); });
    check("step_index", 0x0025d7a0, N * 2, Ret::Int32, [] {
        const uint32_t wrap = u(ri(0, 1));
        w32(OUT, u(wrap ? ri(1, 10) : ri(0, 10)));
        return Args{{OUT, u(ri(0, 10)), u(ri(-5, 5)), wrap}, {}};
    }, [](A a) { return ri32(step_index(a.i[0], (int32_t)a.i[1], (int32_t)a.i[2], (int32_t)a.i[3])); });
    check("sub_0025da70", 0x0025da70, N * 2, Ret::Float, [] { wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9); return Args{{M, M2}, {rf(-1, 10)}}; },
          [](A a) { return rf32(sub_0025da70(a.i[0], a.i[1], a.f[0])); });
    check("vec3_clamp_length", 0x0025f730, N, Ret::None, [] { wfrand(M, 4, -9, 9); return Args{{M}, {rf(0, 12)}}; },
          [](A a) { vec3_clamp_length(a.i[0], a.f[0]); return uint64_t{0}; });
    check("sub_0025f780", 0x0025f780, N * 2, Ret::None, [] {
        const uint32_t base = R() & 0x01fffff0u;
        w32(OV(0x0015ffd8), base);
        const int n = ri(0, 5);
        w16(T, u(n));
        for (int i = 1; i <= 7; ++i) w16(T + 2 * i, u(ri(0, 10)));
        return Args{{base + (u(ri(0, 10)) << 8) + (R() & 0xff), T, sx16(u(ri(0, 8)))}, {}};
    }, [](A a) { sub_0025f780(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    check("sub_00262b00", 0x00262b00, N, Ret::None, [] { wfrand(M, 8, -9, 9); wfrand(OUT, 2, -3, 3); return Args{{M, OUT, OUT + 4}, {rf(-2, 2), rf(-1, 1)}}; },
          [](A a) { sub_00262b00(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1]); return uint64_t{0}; });
    check("sub_002a0858", 0x002a0858, N, Ret::None, [] { wrand(OV(0x001611b8), 7); w32(OV(0x001611d0), u(ri(0, 1))); w32(OV(0x0015f578), R()); return Args{}; },
          [](A) { sub_002a0858(); return uint64_t{0}; });
    auto uid5 = [] {
        for (int i = 0; i < 48; ++i) w16(g::MOBY_TABLE_167150 + i * 0xa0 + 0x86, u(ri(0, 10)));
        w32(OV(0x00166e00), M); w16(M + 0x86, coin() ? 5u : 7u); w32(M + 0x70, T); w32(0x70, T2);
        for (int i = 0; i < 48; ++i) w32(g::MOBY_TABLE_167150 + i * 0xa0 + 0x70, T3);
        wrand(M2, 4);
        return Args{{M2}, {}};
    };
    check("uid5_set_pvar_80", 0x002ea9d8, N, Ret::None, uid5, [](A a) { uid5_set_pvar_80(a.i[0]); return uint64_t{0}; });
    check("uid5_set_pvar_90", 0x002eaa30, N, Ret::None, uid5, [](A a) { uid5_set_pvar_90(a.i[0]); return uint64_t{0}; });


    // ======== batch 3
    g_batch = 3;
    check("sub_00203ad8", 0x00203ad8, N * 2, Ret::Int32, [] {
        w32(OV(0x0017943c), u(ri(-1, 6))); w32(OV(0x0015f660), T);
        for (int i = 0; i < 8; ++i) { w32(T + 16 * i + 4, u(ri(0, 6))); w32(T + 16 * i, R()); }
        return Args{{u(ri(0, 6))}, {}};
    }, [](A a) { return r32(sub_00203ad8(static_cast<int32_t>(a.i[0]))); });
    check("sub_00208038", 0x00208038, N, Ret::None, [] {
        w16(g::PLAYER + 0x1eb0, u(ri(0, 7))); w16(g::PLAYER + 0x1eb2, u(ri(0, 1))); w32(OV(0x0015f424), u(ri(0, 0x45)));
        wrand(M, 4);
        return Args{{M, R()}, {rf(-9, 9), rf(-9, 9)}};
    }, [](A a) { sub_00208038(a.i[0], a.i[1], a.f[0], a.f[1]); return uint64_t{0}; });
    check("sub_0020d2f8", 0x0020d2f8, N, Ret::None, [] { wfrand(g::PLAYER, 0x24, -2, 2); return Args{{M}, {}}; },
          [](A a) { sub_0020d2f8(a.i[0]); return uint64_t{0}; });
    check("sub_00212088", 0x00212088, N * 2, Ret::Float, [] { wf(g::PLAYER + 0x190, rf(-5, 5)); wf(g::PLAYER + 0x194, rf(-5, 5)); return Args{{}, {rf(0, 2), rf(0, 2)}}; },
          [](A a) { return rf32(sub_00212088(a.f[0], a.f[1])); });
    check("sub_00232320", 0x00232320, N, Ret::None, [] {
        w32(g::PLAYER + 0x2278, coin() ? M : 0u); wf(g::PLAYER + 0xa94, rf(-9, 9));
        w32(M + 0x24, T); w8(M + 0x52, u(ri(0, 3))); w8(M + 0x53, u(ri(0, 3)));
        for (int i = 0; i < 4; ++i) w32(T + 0x48 + 4 * i, T2 + 0x100 * i);
        wrand(T2, 0x100);
        w32(g::PLAYER + 0x2080, M2); wf(M2 + 0x54, rf(-9, 9)); w8(M2 + 0x51, u(ri(0, 20)));
        return Args{};
    }, [](A) { sub_00232320(); return uint64_t{0}; });
    check("sub_00233440", 0x00233440, N * 4, Ret::Int32, [] {
        static const int32_t st[] = {0, 0, 0, 0x3f, 0x70, 0x71, 5};
        w32(g::HERO_STATE, u(st[ri(0, 6)])); w16(g::PLAYER + 0x308, R()); w8(g::PLAYER + 0x12e7, ri(0, 3) ? 1u : 0u);
        w8(0x0013d4dc, ri(0, 3) ? 1u : 0u); w16(g::PLAYER + 0x30e, u(ri(-3, 10)));
        return Args{};
    }, [](A) { return ri32(sub_00233440()); });
    check("sub_002335c8", 0x002335c8, N, Ret::None, [] { wfrand(g::PLAYER, 0x24, -2, 2); return Args{{M}, {rf(-9, 9), rf(-4, 4), rf(-9, 9)}}; },
          [](A a) { sub_002335c8(a.i[0], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("sub_00233a78", 0x00233a78, N * 2, Ret::Float, [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 4))); w32(g::PLAYER + 0x2080, M2); wfrand(M2 + 0x40, 4, -4, 4); wfrand(M, 4, -9, 9);
        return Args{{M}, {}};
    }, [](A a) { return rf32(sub_00233a78(a.i[0])); });
    check("sub_0024f608", 0x0024f608, N * 2, Ret::None, [] {
        // list of up to 5 nodes at T + 0x40*i, linked through +8
        const int n = ri(0, 5);
        for (int i = 0; i < 6; ++i) wrand(T + 0x40 * i, 16);
        for (int i = 0; i < n; ++i) w32(T + 0x40 * i + 8, i + 1 < n ? T + 0x40 * (i + 1) : 0u);
        w32(M + 0x64, n ? T : 0u); w32(8, 0);
        return Args{{M, ri(0, 5) ? T + 0x40 * u(ri(0, 5)) : 0u}, {}};
    }, [](A a) { sub_0024f608(a.i[0], a.i[1]); return uint64_t{0}; });
    auto pal = [] { wfrand(OV(0x0017fec0) - 0x40 * 0, 0x40 * 16, 0, 255); wrand(M + 0x38, 2); if (coin()) w8(M + 0x3a, 0); return Args{{M, OUT}, {}}; };
    check("moby_palette_colour_1", 0x00250350, N, Ret::None, pal, [](A a) { moby_palette_colour_1(a.i[0], a.i[1]); return uint64_t{0}; });
    check("moby_palette_colour_0", 0x002503c0, N, Ret::None, pal, [](A a) { moby_palette_colour_0(a.i[0], a.i[1]); return uint64_t{0}; });
    check("sub_00259468", 0x00259468, N * 2, Ret::Float, [] { wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9); w32(OUT, R()); return Args{{M, M2, coin() ? OUT : 0u}, {rf(0.05f, 2)}}; },
          [](A a) { return rf32(sub_00259468(a.i[0], a.i[1], a.i[2] ? hp<int32_t>(a.i[2]) : nullptr, a.f[0])); });
    check("sub_00259508", 0x00259508, N, Ret::None, [] { wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9); return Args{{OUT, M, M2, u(ri(-5, 40))}, {rf(0, 2)}}; },
          [](A a) { sub_00259508(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.f[0]); return uint64_t{0}; });
    check("quat_from_euler", 0x00259f50, N, Ret::None, [] { wfrand(M, 4, -4, 4); return Args{{OUT, M}, {}}; },
          [](A a) { quat_from_euler(a.i[0], a.i[1]); return uint64_t{0}; });
    check("vec4_lerp", 0x0025b6b8, N, Ret::None, [] { wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9); return Args{{OUT, M, M2}, {rf(-1, 2)}}; },
          [](A a) { vec4_lerp(a.i[0], a.i[1], a.i[2], a.f[0]); return uint64_t{0}; });
    check("rgba_pack", 0x0025bfe0, N, Ret::Int32, [] { return Args{{}, {rf(0, 1), rf(0, 1), rf(0, 1), rf(0, 1.2f)}}; },
          [](A a) { return r32(rgba_pack(a.f[0], a.f[1], a.f[2], a.f[3])); });
    check("vec_from_spherical", 0x00262c50, N, Ret::None, [] { return Args{{OUT}, {rf(-9, 9), rf(-4, 4), rf(-4, 4)}}; },
          [](A a) { vec_from_spherical(a.i[0], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("pad_button_history", 0x00266d60, N * 2, Ret::Int32, [] {
        w32(g::PAD + 0x190, u(ri(0, 40))); w16(g::PAD + 0x18e, u(ri(0, 29)));
        for (int i = -30; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, ri(0, 3) ? 0u : (R() & 0xffff));
        w32(OUT, R());
        return Args{{R() & 0xffff, u(ri(-2, 35)), coin() ? OUT : 0u}, {}};
    }, [](A a) { return r32(pad_button_history(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2] ? hp<int32_t>(a.i[2]) : nullptr)); });


    // ======== batch 4
    g_batch = 4;
    check("mat3_abs_diff", 0x0020f0b8, N, Ret::Float, [] { wfrand(T, 16, -9, 9); wfrand(T2, 16, -9, 9); return Args{{T, T2}, {}}; },
          [](A a) { return rf32(mat3_abs_diff(a.i[0], a.i[1])); });
    check("sub_002132b8", 0x002132b8, N * 2, Ret::None, [] {
        wf(g::PLAYER + 0x930, rf(0, 3)); wf(g::PLAYER + 0x934, rf(-3, 3)); wf(g::PLAYER + 0x938, rf(-3, 3)); wrand(0x0013fc70, 4);
        return Args{{}, {rf(0, 2), rf(0, 2)}};
    }, [](A a) { sub_002132b8(a.f[0], a.f[1]); return uint64_t{0}; });
    check("sub_002339d0", 0x002339d0, N * 2, Ret::Float, [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 4))); w32(g::PLAYER + 0x2080, M2); wfrand(M2 + 0x40, 4, -4, 4); wfrand(M, 4, -9, 9);
        return Args{{M}, {}};
    }, [](A a) { return rf32(sub_002339d0(a.i[0])); });
    check("sub_0024f0e8", 0x0024f0e8, N * 2, Ret::None, [] {
        const int n = ri(0, 5);
        for (int i = 0; i < 6; ++i) wrand(T + 0x40 * i, 16);
        for (int i = 0; i < n; ++i) w32(T + 0x40 * i + 0x1c, i + 1 < n ? T + 0x40 * (i + 1) : 0u);
        w32(M + 0x60, n ? T : 0u); w32(0x1c, 0);
        w32(OUT, ri(0, 5) ? T + 0x40 * u(ri(0, 5)) : 0u);
        return Args{{M, OUT}, {}};
    }, [](A a) { sub_0024f0e8(a.i[0], a.i[1]); return uint64_t{0}; });
    check("sub_00262cf0", 0x00262cf0, N, Ret::None, [] { wfrand(M2, 4, -9, 9); return Args{{M, M2}, {rf(-9, 9), rf(-4, 4), rf(-4, 4)}}; },
          [](A a) { sub_00262cf0(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; }, M + 0xc);
    check("sub_00263d40", 0x00263d40, N, Ret::None, [] {
        w32(OV(0x0017943c), u(ri(-1, 4))); w32(OV(0x0015f660), T);
        for (int i = 0; i < 4; ++i) { w32(T + 16 * i + 4, u(ri(0, 4))); w32(T + 16 * i, T2 + 0x40 * i); }
        for (int i = 0; i < 0x100; ++i) w8(T2 + i, ri(0, 12) ? u(ri(0x20, 0x7e)) : 0u);
        for (int i = 0; i < 0x40; ++i) w8(OV(0x001796a8) + i, ri(0, 12) ? u(ri(0x20, 0x7e)) : 0u);
        return Args{{u(ri(0, 5)), coin() ? 0xffffffffu : u(ri(0, 500))}, {}};
    }, [](A a) { sub_00263d40(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("pad_buttons_together", 0x00266e00, N * 2, Ret::Int32, [] {
        w32(g::PAD + 0x190, u(ri(0, 40))); w16(g::PAD + 0x18e, u(ri(0, 29)));
        for (int i = -30; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, ri(0, 2) ? 0u : (R() & 0xff));
        return Args{{R() & 0xff, R() & 0xff, u(ri(0, 30)), u(ri(0, 10))}, {}};
    }, [](A a) { return ri32(pad_buttons_together(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3]))); });


    // ======== batch 5
    g_batch = 5;
    check("sub_00205000", 0x00205000, N * 4, Ret::Int32, [] {
        w8(g::PLAYER + 0x20a4, u(ri(0, 4))); w32(g::PLAYER + 0x22a8, ri(0, 3) ? 0u : 1u); w8(g::PLAYER + 0x12e2, ri(0, 3) ? 0u : 1u);
        w8(g::PLAYER + 0x20a8, ri(0, 3) ? 1u : 0u); w8(g::PLAYER + 0x20aa, ri(0, 3) ? 1u : 0u); w16(g::PLAYER + 0x22c8, ri(0, 3) ? 0u : 1u);
        w32(g::PLAYER + 0x10b4, 2); w32(g::PLAYER + 0x10b8, u(ri(0, 10)));
        for (int i = 0; i < 11; ++i) w32(OV(0x00179ac0) + i * 0x4c + 0x24, R());
        return Args{{u(ri(0, 1))}, {}};
    }, [](A a) { return r32(sub_00205000(static_cast<int32_t>(a.i[0]))); });
    check("sub_00205538", 0x00205538, 10, Ret::None, [] {
        // owner mobys: the hero moby; nodes are the records themselves
        w32(g::HERO_MOBY, M); w32(M + 0x64, 0); w32(8, 0);
        for (int k = 0; k < 31; ++k) { const uint32_t r = g::FX_PARAMS + k * 0xb0; wrand(r, 0x2c); w8(r + 1, ri(0, 3) ? 0u : 1u); w16(r + 0xa2, ri(0, 2) ? 0u : 7u); w32(r + 8, 0); }
        return Args{};
    }, [](A) { sub_00205538(); return uint64_t{0}; });
    check("sub_00207c48", 0x00207c48, N * 2, Ret::None, [] {
        w32(OV(0x0015f680), ri(0, 4) ? 1u : 0u); w32(OV(0x0015f684), u(ri(0, 3))); wf(OV(0x0015f67c), rf(0, 0.6f));
        wf(OV(0x0015f3fc), coin() ? rf(0, 1) : (coin() ? 1.0f : 0.0f));
        return Args{};
    }, [](A) { sub_00207c48(); return uint64_t{0}; });
    check("sub_00209018", 0x00209018, N * 2, Ret::Float, [] {
        const int n = ri(1, 8);
        wf(g::PLAYER + 0xaa8, rf(0, static_cast<float>(n) - 0.01f)); wf(g::PLAYER + 0xaac, rf(0, 2)); w32(g::PLAYER + 0xa98, R());
        wfrand(T, 8, -9, 9);
        return Args{{T, u(n)}, {}};
    }, [](A a) { return rf32(sub_00209018(a.i[0], static_cast<int32_t>(a.i[1]))); });
    check("sub_00216078", 0x00216078, N, Ret::None, [] { w32(g::HERO_MOBY, M2); wfrand(M2 + 0x40, 4, -4, 4); wfrand(M, 4, -9, 9); return Args{{M, OUT}, {}}; },
          [](A a) { sub_00216078(a.i[0], hp<float>(a.i[1])); return uint64_t{0}; });
    check("sub_00231db0", 0x00231db0, N * 4, Ret::Int32, [] {
        w32(g::PLAYER + 3 * 0x50 + 0x10b4, ri(0, 4) ? 2u : 1u); w32(g::PLAYER + 3 * 0x50 + 0x10b8, u(ri(1, 5)));
        static const uint32_t lists[] = {OV(0x0017bbf0), OV(0x0017bbd0), OV(0x0015f6b8)};
        for (uint32_t l : lists) { const int n = ri(0, 4); for (int i = 0; i < n; ++i) { w16(l + 4 * i, R() & 0x7fff); w8(l + 4 * i + 2, u(ri(0, 6))); } w16(l + 4 * n, 0xffff); }
        w32(OUT, R());
        return Args{{u(ri(0, 6)), OUT}, {}};
    }, [](A a) { return ri32(sub_00231db0(a.i[0], hp<int32_t>(a.i[1]))); });
    check("sub_00233810", 0x00233810, N * 2, Ret::None, [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 4))); w32(g::PLAYER + 0x2080, M3); wfrand(M3 + 0x40, 4, -4, 4); wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9);
        return Args{{M, coin() ? M : M2}, {rf(-5, 5)}};
    }, [](A a) { sub_00233810(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    check("sub_0024ebc0", 0x0024ebc0, N * 2, Ret::None, [] {
        w8(M + 0x52, coin() ? 0xffu : u(ri(0, 3))); w8(M + 0x53, u(ri(0, 3))); w8(M + 0x50, u(ri(0, 7))); w8(M + 0x51, u(ri(0, 7)));
        w32(M + 0x24, T);
        for (int i = 0; i < 4; ++i) { w32(T + 0x48 + 4 * i, T2 + 0x80 * i); wrand(T2 + 0x80 * i, 0x20); }
        return Args{{M}, {}};
    }, [](A a) { sub_0024ebc0(a.i[0]); return uint64_t{0}; });
    check("sub_0024f028", 0x0024f028, N * 2, Ret::Int32, [] {
        for (int i = 0; i < 7; ++i) w8(OV(0x0018eb40) + i * 0x40 + 4, ri(0, 2) ? 1u : 0u);
        w32(M + 0x24, T); w32(T + 0x1c, T2); w32(M + 0x60, R());
        for (int i = 0; i < 8; ++i) { w32(T2 + 4 * i + 4, T3 + 0x20 * i); wrand(T3 + 0x20 * i, 8); w8(T3 + 0x20 * i, u(ri(0, 8))); }
        return Args{{M, u(ri(0, 7))}, {}};
    }, [](A a) { return r32(sub_0024f028(a.i[0], a.i[1])); });
    check("sub_0025abf0", 0x0025abf0, N * 2, Ret::Float, [] { wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9); w32(OUT, R()); return Args{{M, M2, coin() ? OUT : 0u}, {rf(0.1f, 3), rf(0, 2)}}; },
          [](A a) { return rf32(sub_0025abf0(a.i[0], a.i[1], a.i[2] ? hp<float>(a.i[2]) : nullptr, a.f[0], a.f[1])); });
    check("sub_0025bb38", 0x0025bb38, N * 4, Ret::Float, [] { return Args{{u(ri(-2, 2))}, {rf(-3.1f, 3.1f), coin() ? rf(-3.1f, 3.1f) : 0.0f}}; },
          [](A a) { return rf32(sub_0025bb38(static_cast<int32_t>(a.i[0]), a.f[0], a.f[1])); });
    check("mat3_normalize_columns", 0x0025c170, N, Ret::None, [] { wfrand(T, 16, -9, 9); return Args{{T}, {}}; },
          [](A a) { mat3_normalize_columns(a.i[0]); return uint64_t{0}; });
    check("sub_00262b80", 0x00262b80, N, Ret::None, [] { wfrand(OUT, 2, -4, 4); return Args{{M, OUT, OUT + 4}, {rf(-9, 9), rf(-1, 1), rf(-1, 1)}}; },
          [](A a) { sub_00262b80(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("sub_002a0ba8", 0x002a0ba8, N, Ret::None, [] { w32(OV(0x001611c0), T); return Args{{R(), R()}, {}}; },
          [](A a) { sub_002a0ba8(a.i[0], static_cast<uint64_t>(sx(a.i[1]))); return uint64_t{0}; });


    // ======== batch 6
    g_batch = 6;
    auto particles = [] {
        constexpr uint32_t PB = 0x01e40000;
        w32(OV(0x0016016c), PB);
        const int32_t nx = ri(0, 9) == 0 ? 0x800 : ri(0, 120);
        w32(OV(0x00160170), u(nx)); w32(OV(0x00160174), u(nx + ri(-5, 20))); w32(OV(0x00160178), R());
        for (int i = 0; i < 24; ++i) w8(OV(0x001b1880) + i, ri(0, 2) ? 0xffu : R());
        wrand(PB + u(nx) * 64, 16);
    };
    check("alloc_particle", 0x002678b8, N * 2, Ret::Int32, [particles] { particles(); return Args{{R() & 0xff}, {}}; },
          [](A a) { return r32(alloc_particle(a.i[0])); });
    check("spawn_particle_2f", 0x00271b18, N, Ret::Int32, [particles] {
        particles(); seed_rand(); wrand(M, 4); wrand(M2, 4); w32(OV(0x001b213c), T); w8(T, R());
        return Args{{M, M2}, {rf(-9, 9)}};
    }, [](A a) { return r32(spawn_particle_2f(a.i[0], a.i[1], a.f[0])); });
    check("spawn_particle_46", 0x00274cf8, N, Ret::Int32, [particles] {
        particles(); wrand(M, 4); w32(OV(0x001b2198), T); wrand(T, 8);
        return Args{{M, u(ri(0, 31))}, {rf(0, 9)}};
    }, [](A a) { return r32(spawn_particle_46(a.i[0], a.i[1], a.f[0])); });
    check("sub_001fa050", 0x001fa050, N, Ret::None, [] { w32(OV(0x001611c0), T); return Args{{R() & 0xff, R() & 0xff, R() & 0xff, R()}, {}}; },
          [](A a) { sub_001fa050(sx(a.i[0]), sx(a.i[1]), sx(a.i[2]), sx(a.i[3])); return uint64_t{0}; });
    check("sub_00205600", 0x00205600, N * 2, Ret::None, [] {
        seed_rand(); w32(g::PLAYER + 0xff0, ri(0, 3) ? 0u : 1u); w32(g::PLAYER + 0xff8, ri(0, 2) ? u(ri(1, 50)) : 0u);
        w32(g::PLAYER + 0xff4, u(ri(0, 100))); w32(OV(0x0015f5cc), u(ri(0, 100)));
        return Args{};
    }, [](A) { sub_00205600(); return uint64_t{0}; });
    check("sub_00206c08", 0x00206c08, N * 2, Ret::None, [] {
        w32(g::PLAYER + 0x2080, M);
        for (int k = 0; k < 8; ++k) { w32(g::PLAYER + 0x2218 + 4 * k, ri(0, 2) ? 0xffffffffu : u(ri(0, 7))); w32(g::PLAYER + 0x2238 + 4 * k, coin() ? 0u : (coin() ? M : M2)); }
        for (int s2 = 0; s2 < 8; ++s2) { const uint32_t p = 0x0013e550 + s2 * 0x70; w8(p + 0x74, u(ri(0, 8))); w32(p + 0x88, coin() ? M : M2); w32(p + 0x8c, R()); }
        return Args{};
    }, [](A) { sub_00206c08(); return uint64_t{0}; });
    check("sub_002079e8", 0x002079e8, N, Ret::None, [] {
        w32(g::HERO_STATE, ri(0, 3) ? 0x7fu : 0x10u); wf(0x0015ed6c, rf(0, 2)); w32(g::PLAYER + 0x2080, M); wfrand(g::PLAYER + 0x6b0, 2, -3, 3); wfrand(M + 0x40, 4, -9, 9);
        return Args{};
    }, [](A) { sub_002079e8(); return uint64_t{0}; });
    check("sub_002120d8", 0x002120d8, N * 2, Ret::None, [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 2))); wf(g::PLAYER + 0x98, rf(-3, 3)); wf(g::PLAYER + 0x194, rf(0, 5)); wf(g::PLAYER + 0x2e4, rf(-1, 1));
        w32(g::PLAYER + 0x2080, M); wfrand(M + 0x40, 4, -4, 4);
        return Args{{}, {rf(-3, 5)}};
    }, [](A a) { sub_002120d8(a.f[0]); return uint64_t{0}; }, 0x0013f43c);
    check("sub_00216710", 0x00216710, N * 2, Ret::None, [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 3))); w32(g::PLAYER + 0x2080, M); wfrand(M + 0x40, 4, -4, 4); wfrand(g::PLAYER, 0x24, -2, 2);
        wfrand(0x0013f430, 4, -9, 9); wfrand(0x0013f4a0, 4, ri(0, 3) ? -2.0f : -0.0001f, ri(0, 3) ? 2.0f : 0.0001f);
        return Args{{}, {rf(0, 1.5f)}};
    }, [](A a) { sub_00216710(a.f[0]); return uint64_t{0}; });
    check("sub_00232fe8", 0x00232fe8, N * 2, Ret::None, [] {
        static const uint32_t ms[] = {0, M, M2, M3};
        w32(g::PLAYER + 0xa88, M3);
        for (int k = 0; k < 7; ++k) { w32(g::PLAYER + k * 0x50 + 0x1090, ms[ri(0, 3)]); w32(g::PLAYER + k * 0x50 + 0x1094, ms[ri(0, 3)]); }
        w32(g::PLAYER + 0x118c, ms[ri(0, 3)]); w8(g::PLAYER + 0x20a4, u(ri(0, 2))); w32(g::PLAYER + 0x1624, ms[ri(0, 3)]);
        w32(g::PLAYER + 0x10b8, ri(0, 2) ? 8u : 3u); w32(OV(0x0015f5c4), ri(0, 3) ? 0u : 1u);
        for (uint32_t m : {M, M2, M3}) { w16(m + 0x34, R()); w8(m + 0x20, ri(0, 3) ? 1u : 0u); }
        return Args{};
    }, [](A) { sub_00232fe8(); return uint64_t{0}; });
    check("sub_002334d0", 0x002334d0, N * 3, Ret::None, [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 3))); wfrand(g::PLAYER + 0x270, 4, -3, 3); wfrand(0x0013f5e0, 4, -3, 3);
        w32(g::HERO_STATE, coin() ? 0x3eu : 0x10u); w16(g::PLAYER + 0x30e, ri(0, 2) ? 0u : 5u);
        wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9);
        return Args{{M, coin() ? M : M2}, {rf(-3, 3)}};
    }, [](A a) { sub_002334d0(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    check("sub_0024eed8", 0x0024eed8, N, Ret::None, [] {
        w32(M + 0x94, T); wf(M + 0x2c, rf(0.2f, 4)); wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -9, 9); wfrand(M2, 4, -9, 9);
        return Args{{M, u(ri(0, 7)), M2}, {rf(-9, 9), rf(-9, 9)}};
    }, [](A a) { sub_0024eed8(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.f[0], a.f[1]); return uint64_t{0}; });
    check("moby_set_anim", 0x002577c8, N * 2, Ret::None, [] {
        w32(M + 0x24, T);
        for (int i = 0; i < 4; ++i) { w32(T + 0x48 + 4 * i, T2 + 0x100 * i); wrand(T2 + 0x100 * i, 0x40); w8(T2 + 0x100 * i + 0x10, u(ri(1, 12))); w8(T2 + 0x100 * i + 0x11, R()); w8(T2 + 0x100 * i + 0x12, R()); }
        for (int i = 0; i < 4; ++i) for (int f = 0; f < 16; ++f) w32(T2 + 0x100 * i + 0x1c + 4 * f, T3 + 0x10 * f);
        wfrand(T3, 64, -9, 9);
        w8(M + 0x51, u(ri(0, 12))); w8(M + 0x70, R());
        return Args{{M, u(ri(0, 3)), u(ri(0, 14))}, {}};
    }, [](A a) { moby_set_anim(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    check("solve_quadratic", 0x002595a0, N * 2, Ret::Int32, [] {
        wrand(OUT, 2); const float a = rf(-3, 3), b = rf(-3, 3);
        return Args{{OUT, OUT + 4}, {a, b, ri(0, 5) ? rf(-3, 3) : b * b / (4 * a)}};
    }, [](A a) { return ri32(solve_quadratic(hp<float>(a.i[0]), hp<float>(a.i[1]), a.f[0], a.f[1], a.f[2])); });
    check("spring_step", 0x0025b798, N * 2, Ret::Float, [] { wf(OUT, rf(-9, 9)); return Args{{OUT}, {rf(-9, 9), rf(0, 1), rf(0, 1), rf(-1, 5)}}; },
          [](A a) { return rf32(spring_step(a.i[0], a.f[0], a.f[1], a.f[2], a.f[3])); });
    check("turn_towards", 0x0025bc00, N * 2, Ret::Float, [] { wf(OUT, rf(-3, 3)); return Args{{OUT, u(ri(-1, 1))}, {rf(-3, 3), rf(0, 1)}}; },
          [](A a) { return rf32(turn_towards(a.i[0], static_cast<int32_t>(a.i[1]), a.f[0], a.f[1])); });
    check("point_in_box", 0x0025f960, N * 2, Ret::Int32, [] {
        w32(OV(0x001600ec), T); for (int i = 0; i < 4; ++i) { wfrand(T + i * 128 + 0x30, 4, -2, 2); wfrand(T + i * 128 + 0x40, 16, -1, 1); }
        wfrand(M, 4, -2, 2);
        return Args{{M, u(ri(-1, 3))}, {}};
    }, [](A a) { return ri32(point_in_box(a.i[0], static_cast<int32_t>(a.i[1]))); });
    check("sub_0028c8b0", 0x0028c8b0, N * 2, Ret::Int32, [] {
        w32(M + 8, u(ri(-50, 50))); w32(M + 0xc, u(ri(-50, 200))); w8(M + 0x19, R());
        return Args{{M}, {rf(-2, 12), rf(0, 4), rf(5, 10)}};
    }, [](A a) { return ri32(sub_0028c8b0(a.i[0], a.f[0], a.f[1], a.f[2])); });


    // ======== batch 7
    g_batch = 7;
    check("sub_00204b60", 0x00204b60, N * 3, Ret::None, [] {
        for (int i = 0; i < 300; ++i) w16(OV(0x00179450) + 2 * i, u(ri(0, 40)));
        const int n = ri(0, 8);
        w32(0x0015ee30, u(n));
        for (int i = 0; i < 10; ++i) w8(0x00141e08 + i, u(ri(0, 40)));
        return Args{{sx16(u(ri(0, 40)))}, {}};
    }, [](A a) { sub_00204b60(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("sub_00207330", 0x00207330, N, Ret::None, [] {
        // whole palette (256 entries): FPU sums of denormal leftovers would flush on the PS2
        wfrand(OV(0x0017fec0), 0x10 * 256, 0, 255); wrand(M2 + 0x38, 2); if (coin()) w8(M2 + 0x3a, 0); wrand(M + 0x38, 2);
        return Args{{M, M2}, {}};
    }, [](A a) { sub_00207330(a.i[0], a.i[1]); return uint64_t{0}; });
    check("sub_00208f20", 0x00208f20, N * 2, Ret::None, [] {
        wf(g::PLAYER + 0xaa8, rf(0, 20)); w32(g::PLAYER + 0xa9c, u(ri(0, 1))); wf(g::PLAYER + 0xa94, rf(0.2f, 2)); wf(g::PLAYER + 0xa90, rf(-2, 2));
        w32(g::PLAYER + 0x2080, M); w8(M + 0x53, u(ri(0, 3))); w32(M + 0x24, T); for (int i = 0; i < 4; ++i) { w32(T + 0x48 + 4 * i, T2 + 0x40 * i); w8(T2 + 0x40 * i + 0x10, u(ri(1, 30))); }
        w32(M + 0x6c, T3); wf(T3, rf(0.2f, 2));
        return Args{{}, {rf(0, 20), rf(-1, 2), rf(-2, 2), coin() ? -1.0f : rf(0, 0.5f)}};
    }, [](A a) { sub_00208f20(a.f[0], a.f[1], a.f[2], a.f[3]); return uint64_t{0}; });
    auto modes = [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 3))); w32(g::PLAYER + 0x2080, M3); wfrand(M3 + 0x40, 4, -4, 4);
        wfrand(g::PLAYER + 0x270, 4, -3, 3); wfrand(g::PLAYER + 0x290, 4, -3, 3); wfrand(0x0013f5e0, 4, -3, 3);
        w32(g::HERO_STATE, coin() ? 0x3eu : 0x10u); w16(g::PLAYER + 0x30e, ri(0, 2) ? 0u : 5u);
        wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9);
    };
    check("sub_002336e8", 0x002336e8, N * 2, Ret::None, [modes] { modes(); return Args{{M, coin() ? M : M2}, {rf(-3, 3)}}; },
          [](A a) { sub_002336e8(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    check("sub_00233708", 0x00233708, N * 2, Ret::None, [modes] { modes(); return Args{{M, coin() ? M : M2}, {rf(-3, 3)}}; },
          [](A a) { sub_00233708(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });
    check("sub_002338d0", 0x002338d0, N * 2, Ret::None, [modes] { modes(); return Args{{M, coin() ? M : M2}, {}}; },
          [](A a) { sub_002338d0(a.i[0], a.i[1]); return uint64_t{0}; });
    check("sub_00233c30", 0x00233c30, N * 2, Ret::Float, [modes] { modes(); w32(OUT, R()); return Args{{M, M2, coin() ? OUT : 0u}, {rf(0.05f, 2)}}; },
          [](A a) { return rf32(sub_00233c30(a.i[0], a.i[1], a.i[2] ? hp<int32_t>(a.i[2]) : nullptr, a.f[0])); });
    check("sub_0024a7e8", 0x0024a7e8, N * 2, Ret::None, [] {
        wfrand(OV(0x00182a70), 0x13 * 4, -300, 300); w32(0x0015ed84, u(ri(-2, 20)));
        static const int32_t ids[] = {-1, 0, 3, 6, 18, 19, 25, 100, 106, 99, 118, 140};
        return Args{{OUT, OUT + 4, u(ids[ri(0, 11)])}, {rf(-2, 2), rf(-2, 2)}};
    }, [](A a) { sub_0024a7e8(hp<float>(a.i[0]), hp<float>(a.i[1]), static_cast<int32_t>(a.i[2]), a.f[0], a.f[1]); return uint64_t{0}; });
    check("moby_anim_time", 0x0024edc8, N * 3, Ret::Float, [] {
        w8(M + 0x52, ri(0, 3) ? u(ri(0, 1)) : 0xffu); w8(M + 0x53, u(ri(0, 1))); w8(M + 0x50, u(ri(0, 5))); w8(M + 0x51, u(ri(0, 5)));
        wf(M + 0x54, coin() ? 0.0f : rf(0, 1)); w32(M + 0x68, T); w32(M + 0x6c, T2); w16(T + 4, R()); w16(T2 + 4, R());
        return Args{{M}, {}};
    }, [](A a) { return rf32(moby_anim_time(a.i[0])); });
    check("spring_to", 0x0025b8c0, N * 2, Ret::Float, [] { wf(OUT, rf(-9, 9)); wf(OUT + 4, rf(-2, 2)); return Args{{OUT, OUT + 4}, {rf(-9, 9), rf(0, 1), rf(0, 1), rf(-1, 50)}}; },
          [](A a) { return rf32(spring_to(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2], a.f[3])); });
    check("sub_0025d238", 0x0025d238, N * 2, Ret::None, [] { wfrand(M, 4, 0.2f, 9); wfrand(M2, 4, -9, 9); return Args{{M, M2}, {}}; },
          [](A a) { sub_0025d238(a.i[0], a.i[1]); return uint64_t{0}; });
    check("sub_0028c9a0", 0x0028c9a0, N * 2, Ret::Int32, [] {
        wfrand(OV(0x00166dc0), 4, -9, 9); wfrand(M2, 4, -9, 9); w32(M + 8, T);
        wf(T, rf(0, 4)); wf(T + 4, rf(6, 15)); w32(T + 8, u(ri(-50, 50))); w32(T + 0xc, u(ri(-50, 200))); w8(T + 0x19, R());
        return Args{{M, M2}, {}};
    }, [](A a) { return ri32(sub_0028c9a0(a.i[0], a.i[1])); });


    // ======== batch 8
    g_batch = 8;
    check("sub_00203908", 0x00203908, N * 3, Ret::Int32, [] {
        w32(OV(0x00179410), ri(0, 3) ? 0u : 1u); w32(OV(0x00179434), ri(0, 3) ? 0xffffffffu : 2u);
        w32(0x001516d0 + 0x50, ri(0, 3) ? 0u : 1u); w32(0x001516d0 + 0x1c, ri(0, 3) ? 0xffffffffu : 3u);
        for (int i = 0; i < 8; ++i) w16(0x00141968 + 8 * i, ri(0, 3) ? R() & 0xfff : 0xffffu);
        for (int i = 0; i < 300; ++i) w16(OV(0x00179450) + 2 * i, u(ri(0, 40)));
        w32(0x0015ee30, u(ri(0, 6)));
        return Args{{sx16(u(ri(0, 40))), u(ri(0, 7))}, {}};
    }, [](A a) { return ri32(sub_00203908(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]))); });
    check("sub_002126b8", 0x002126b8, N * 2, Ret::None, [] {
        wfrand(0x0013f5e0, 4, -3, 3); wfrand(0x0013f3d0, 4, -9, 9); wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9);
        return Args{{M, M2, u(ri(0, 1))}, {rf(-3, 3), rf(-12, 3)}};
    }, [](A a) { sub_002126b8(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.f[0], a.f[1]); return uint64_t{0}; });
    check("rle4_decode", 0x002483c8, N, Ret::None, [] {
        // offsets table (s16) at T, data base T2: entry 0 starts at base+0x200
        int16_t o = 0x200;
        for (int i = 0; i < 5; ++i) { o = static_cast<int16_t>(o + ri(0, 12) * 3); w16(T + 2 * i, static_cast<uint16_t>(o)); }
        for (int i = 0; i < 0x300; ++i) w8(T2 + i, ri(0, 3) ? u(ri(1, 9)) : R());
        wrand(T4, 0x400);
        return Args{{T4, u(ri(0, 4)), T2, T}, {}};
    }, [](A a) { rle4_decode(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3]); return uint64_t{0}; });
    check("grid_find", 0x00256f14, N * 2, Ret::Int32, [] {
        constexpr uint32_t G = 0x01e20000;
        w32(OV(0x0015fbc0), G); w32(OV(0x0015fb80), T3);
        for (int cy = 0; cy < 3; ++cy) for (int cx = 0; cx < 3; ++cx) {
            const uint32_t cell = G + ((cy * 64 + cx) << 2);
            if (ri(0, 4) == 0) { w32(cell, 0); continue; }
            const uint32_t lo = 0x4000 + (cy * 3 + cx) * 0x40;
            w32(cell, lo); const int n = ri(1, 4); w32(G + lo, u(n));
            for (int i = 0; i < n; ++i) w32(G + lo + 4 + 4 * i, u(ri(0, 15)));
        }
        for (int i = 0; i < 16; ++i) { wfrand(T3 + i * 32, 3, 0, 48); wf(T3 + i * 32 + 12, rf(0, 300)); }
        wfrand(M, 2, 0, 47.9f); w32(OUT, R());
        return Args{{M, OUT}, {}};
    }, [](A a) { return r32(grid_find(a.i[0], hp<float>(a.i[1]))); });
    auto particles2 = [] {
        constexpr uint32_t PB = 0x01e40000;
        w32(OV(0x0016016c), PB); const int32_t nx = ri(0, 9) == 0 ? 0x800 : ri(0, 120);
        w32(OV(0x00160170), u(nx)); w32(OV(0x00160174), u(nx + ri(-5, 20))); w32(OV(0x00160178), R());
        for (int i = 0; i < 24; ++i) w8(OV(0x001b1880) + i, ri(0, 2) ? 0xffu : R());
        seed_rand(); wrand(M, 4); wrand(M2, 4);
    };
    check("spawn_particle_4", 0x00269958, N, Ret::None, [particles2] {
        particles2(); w32(OV(0x001b2090), T); w8(T, R());
        return Args{{M, M2, R(), R(), R(), R(), R(), u(ri(0, 1))}, {}};
    }, [](A a) { spawn_particle_4(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], static_cast<int32_t>(a.i[7])); return uint64_t{0}; });
    check("spawn_particle_23", 0x0026f548, N, Ret::None, [particles2] {
        particles2(); w32(OV(0x001b210c), T); w8(T, R()); wf(0x0015ed70, rf(0, 2));
        return Args{{M, M2, u(ri(0, 3)), R()}, {}};
    }, [](A a) { spawn_particle_23(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.i[3]); return uint64_t{0}; });


    // ======== batch 9
    g_batch = 9;
    check("sub_00206cf8", 0x00206cf8, N * 3, Ret::None, [] {
        w16(0x00141d28, ri(0, 2) ? 0u : 1u); w8(0x0013d4c4, ri(0, 2) ? 1u : 0u);
        w32(g::HERO_SUBSTATE, u(ri(0x10, 0x13))); w32(g::HERO_STATE, 0x6a);
        w32(OV(0x0015f5cc), ri(0, 2) ? u(ri(0, 3)) * 0x80u : R() & 0xfff);
        w16(0x00141be8, ri(0, 3) ? 0u : 1u); w16(0x001418f0, ri(0, 3) ? 0u : 1u);
        for (int i = 0; i < 0x25; ++i) { w32(OV(0x00179ac0) + 8 + i * 0x4c, ri(0, 3) ? 0u : 1u); w8(0x0013d4c0 + i, ri(0, 2) ? 1u : 0u); }
        w32(OV(0x00179410), 0); w32(OV(0x00179434), 0xffffffffu); w32(0x001516d0 + 0x50, 0); w32(0x001516d0 + 0x1c, 0xffffffffu);
        for (int i = 0; i < 300; ++i) w16(OV(0x00179450) + 2 * i, u(ri(0, 40)));
        w32(0x0015ee30, u(ri(0, 6)));
        return Args{};
    }, [](A) { sub_00206cf8(); return uint64_t{0}; });
    check("sub_00231ff8", 0x00231ff8, N * 3, Ret::Int32, [] {
        seed_rand();
        w32(g::PLAYER + 3 * 0x50 + 0x10b4, ri(0, 4) ? 2u : 1u); w32(g::PLAYER + 3 * 0x50 + 0x10b8, u(ri(1, 5)));
        static const uint32_t lists[] = {OV(0x0017bbf0), OV(0x0017bbd0), OV(0x0015f6b8)};
        for (uint32_t l : lists) { const int n = ri(0, 6); for (int i = 0; i < n; ++i) { w16(l + 4 * i, u(ri(0, 3))); w8(l + 4 * i + 2, R()); w8(l + 4 * i + 3, R()); } w16(l + 4 * n, 0xffff); }
        w32(OUT, R()); w32(OUT + 4, R());
        return Args{{u(ri(0, 3)), OUT, OUT + 4}, {}};
    }, [](A a) { return ri32(sub_00231ff8(a.i[0], hp<int32_t>(a.i[1]), hp<int32_t>(a.i[2]))); });
    check("sprite_cache_get", 0x00249d80, N * 2, Ret::Int32, [] {
        static constexpr OvConst<0x001841f0u> C{};
        w32(C + 0x28, ri(0, 5) ? 1u : 0u); w32(C, T2); w32(C + 4, T);
        int16_t o = 0x200;
        for (int i = 0; i < 6; ++i) { o = static_cast<int16_t>(o + ri(0, 8) * 3); w16(T + 2 * i, static_cast<uint16_t>(o)); }
        for (int i = 0; i < 0x300; ++i) w8(T2 + i, ri(0, 3) ? u(ri(1, 9)) : R());
        for (int i = 0; i < 8; ++i) { w32(C + 0x30 + 16 * i, u(ri(0, 5))); w32(C + 0x34 + 16 * i, u(ri(0, 100))); w32(C + 0x38 + 16 * i, T4 + 0x400 * i); w32(C + 0x3c + 16 * i, R()); }
        w32(OV(0x0015f3f8), R());
        return Args{{u(ri(0, 5))}, {}};
    }, [](A a) { return ri32(sprite_cache_get(a.i[0])); });
    check("sub_00256de0", 0x00256de0, N * 3, Ret::Int32, [] {
        w32(OV(0x0015fbc4), u(ri(0, 5)));
        for (int i = 0; i < 6; ++i) { wfrand(OV(0x0017ecc0) + 16 * i, 3, -10, 10); wf(OV(0x0017ecc0) + 16 * i + 12, rf(0, 120)); wfrand(OV(0x0017eec0) + 0x80 * i, 16, -0.3f, 0.3f); }
        wfrand(M, 4, -10, 10); wrand(OUT, 2);
        return Args{{M, OUT, OUT + 4}, {}};
    }, [](A a) { return ri32(sub_00256de0(a.i[0], hp<float>(a.i[1]), hp<int32_t>(a.i[2]))); });
    check("sub_00259bc8", 0x00259bc8, N * 2, Ret::None, [] {
        w32(OV(0x00173e54), u(ri(0, 63))); w8(M + 0xa4, coin() ? 0xffu : u(ri(0, 63)));
        for (int i = 0; i < 64; ++i) { w32(OV(0x00178100) + i * 64 + 0x34, ri(0, 2) ? M : M2); wf(OV(0x00178100) + i * 64 + 0x2c, rf(0, 5)); }
        wrand(M2, 4); wfrand(M3, 4, coin() ? -0.00001f : -3.0f, coin() ? 0.00001f : 3.0f);
        return Args{{M, R(), R(), M2, M3}, {rf(0, 5)}};
    }, [](A a) { sub_00259bc8(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.f[0]); return uint64_t{0}; });
    check("mat_to_euler", 0x0025d330, N, Ret::None, [] {
        wfrand(M, 4, -3, 3); w32(g::HERO_MOBY, M2);
        Mat4 r; euler_to_mat4_fpu(r, ee_ref<Vec4>(M)); ee_ref<Mat4>(T) = r; wfrand(T + 0x30, 4, -9, 9);
        return Args{{T, OUT}, {}};
    }, [](A a) { mat_to_euler(a.i[0], a.i[1]); return uint64_t{0}; });
    check("spawn_particle_2e", 0x002718d0, N, Ret::Int32, [particles2] {
        particles2(); w32(OV(0x001b2138), T); w8(T, R());
        return Args{{M, M2, R()}, {rf(0, 2), rf(-9, 9)}};
    }, [](A a) { return r32(spawn_particle_2e(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1])); });


    // ======== batch 10
    g_batch = 10;
    check("sub_002090d0", 0x002090d0, N, Ret::None, [] { wfrand(0x0013f3d0, 4, -9, 9); wfrand(0x0013f3e0, 4, -3, 3); wfrand(M, 4, -3, 3); return Args{{M}, {rf(-1, 1), rf(-1, 1), rf(-1, 1)}}; },
          [](A a) { sub_002090d0(a.i[0], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("sub_00211cf8", 0x00211cf8, N, Ret::None, [] { wfrand(0x0013f3e0, 4, -3, 3); return Args{{}, {rf(-1, 1), rf(-1, 1), rf(-1, 1)}}; },
          [](A a) { sub_00211cf8(a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("sub_00232128", 0x00232128, N * 3, Ret::None, [] {
        seed_rand();
        w8(g::PLAYER + 0x20a4, ri(0, 3) ? 0u : 1u); w32(g::PLAYER + 0x11a4, ri(0, 3) ? 2u : 1u);
        w32(g::PLAYER + 0x2080, M3); w8(M3 + 0x53, u(ri(0, 3)));
        w32(g::PLAYER + 3 * 0x50 + 0x10b4, 2); w32(g::PLAYER + 3 * 0x50 + 0x10b8, 2);
        const int n = ri(0, 4); for (int i = 0; i < n; ++i) { w16(OV(0x0017bbf0) + 4 * i, u(ri(0, 3))); w16(OV(0x0017bbf2) + 4 * i, R()); } w16(OV(0x0017bbf0) + 4 * n, 0xffff);
        w32(g::PLAYER + 0x1180, coin() ? M : 0u); w32(g::PLAYER + 0x1184, coin() ? M2 : 0u); wf(g::PLAYER + 0xa90, rf(-3, 3));
        return Args{};
    }, [](A) { sub_00232128(); return uint64_t{0}; });
    auto attach = [] {
        w16(M + 0x34, ri(0, 3) ? 0x20u : 0u); w32(M + 0x78, T); w32(T + 8, ri(0, 4) ? M2 : 0u);
        wfrand(M2, 8, -3, 3); wfrand(M + 0x10, 4, -9, 9); Mat4 r; euler_to_mat4_fpu(r, ee_ref<Vec4>(M2)); ee_ref<Mat4>(M + 0xc0) = r;
        wfrand(M3, 4, -9, 9); wfrand(T2, 4, -3, 3);
        return Args{{R(), M, M3, T2, OUT, OUT + 0x10}, {}};
    };
    check("sub_00260400", 0x00260400, N, Ret::Int32, attach, [](A a) { return ri32(sub_00260400(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5])); });
    check("sub_00260668", 0x00260668, N, Ret::Int32, attach, [](A a) { return ri32(sub_00260668(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5])); });
    check("attach_frame_set", 0x00260738, N, Ret::None, [] { wfrand(M3, 4, -9, 9); wfrand(T2, 4, -3, 3); wfrand(T2 + 0x10, 4, -3, 3); return Args{{OUT, M3, T2, T2 + 0x10}, {}}; },
          [](A a) { attach_frame_set(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });


    // ======== batch 11
    g_batch = 11;
    check("sub_002078a8", 0x002078a8, N * 2, Ret::None, [] {
        static const uint32_t st[] = {0x7f, 0x6b, 0x10};
        w32(g::HERO_STATE, st[ri(0, 2)]); wf(0x0015ed6c, rf(0, 2)); w32(g::PLAYER + 0x2080, M); wfrand(g::PLAYER + 0x6a8, 2, -3, 3);
        wfrand(M, 0x40, -2, 2); w16(g::PLAYER + 0x8bc, ri(0, 3) ? 0u : 1u); w32(g::PLAYER + 0x86c, ri(0, 3) ? M2 : 0u); wfrand(M2, 8, -9, 9);
        wfrand(OV(0x0017a6c0), 4, -3, 3);
        return Args{};
    }, [](A) { sub_002078a8(); return uint64_t{0}; });
    check("sub_002330d0", 0x002330d0, N * 2, Ret::None, [] {
        static const uint32_t ms[] = {0, M, M2, M3};
        w32(g::PLAYER + 0xa88, M3);
        for (int k = 0; k < 7; ++k) { w32(g::PLAYER + k * 0x50 + 0x1090, ms[ri(0, 3)]); w32(g::PLAYER + k * 0x50 + 0x1094, ms[ri(0, 3)]); }
        for (uint32_t o : {0x118cu, 0x1180u, 0x1184u, 0x1624u}) w32(g::PLAYER + o, ms[ri(0, 3)]);
        w8(g::PLAYER + 0x20a4, u(ri(0, 2))); w16(g::PLAYER + 0x22d8, u(ri(0, 1)));
        w8(g::PLAYER + 0x20ae, u(ri(0, 1))); w8(g::PLAYER + 0x20af, u(ri(0, 1))); w32(g::PLAYER + 0x10b8, ri(0, 1) ? 8u : 2u);
        for (uint32_t m : {M, M2, M3}) w16(m + 0x34, R());
        return Args{};
    }, [](A) { sub_002330d0(); return uint64_t{0}; });
    check("sub_00240348", 0x00240348, N * 2, Ret::None, [] {
        wrand(OV(0x00182640) - 0x60, 0x100); wrand(OV(0x001827c0), 16);
        static const int32_t ids[] = {-3, -2, -1, 0, 1, 5, 12};
        return Args{{T, R(), R() & 0xffff, R() & 0x3, R() & 0xff, u(ids[ri(0, 6)])}, {}};
    }, [](A a) { sub_00240348(a.i[0], sx(a.i[1]), sx(a.i[2]), sx(a.i[3]), sx(a.i[4]), sx(a.i[5])); return uint64_t{0}; });


    // ======== batch 12
    g_batch = 12;
    check("sub_002121c0", 0x002121c0, N * 3, Ret::None, [] {
        w16(g::PLAYER + 0x30e, ri(0, 4) ? 3u : 0u); wf(g::PLAYER + 0x2dc, rf(0, 0.2f)); wf(g::PLAYER + 0x108, rf(-0.2f, 1));
        w8(g::PLAYER + 0x257, ri(0, 4) ? 1u : 0u); wfrand(g::PLAYER + 0x200, 2, -0.5f, 0.5f); wf(g::PLAYER + 0x208, rf(-0.2f, 1));
        wfrand(g::PLAYER + 0xe0, 4, -2, 2); wf(0x0015ed70, rf(0, 0.1f));
        return Args{};
    }, [](A) { sub_002121c0(); return uint64_t{0}; });
    check("sub_0023e5e0", 0x0023e5e0, N * 2, Ret::Int32, [] {
        wf(OV(0x0015f5d4), rf(0, 1)); for (int i = 0; i < 8; ++i) w32(OV(0x001804c0) + 0x10 + i * 0x30, ri(0, 2) ? 1u : 0u);
        wrand(M, 4);
        return Args{{M, R()}, {rf(0, 9), rf(0, 2)}};
    }, [](A a) { return ri32(sub_0023e5e0(a.i[0], a.i[1], a.f[0], a.f[1])); });
    check("sub_0024f440", 0x0024f440, N, Ret::None, [] {
        // moby +0x14 -> joint info (count byte +8), +0x18 -> class (fixed count +8, index list +0x1c, sets +0x48)
        w32(M + 0x14, T); w8(T + 8, u(ri(2, 20))); w32(M + 0x18, T2);
        const uint32_t fixed = u(ri(0, 4)); w8(T2 + 8, fixed); w32(T2 + 0x1c, T2 + 0x100);
        for (int i = 0; i < 8; ++i) w8(T2 + 0x104 + i, u(ri(0, 15)));
        for (int s = 0; s < 2; ++s) { w32(T2 + 0x48 + 4 * s, T2 + 0x200 + 0x40 * s); for (int f = 0; f < 4; ++f) w32(T2 + 0x200 + 0x40 * s + 0x1c + 4 * f, T3 + 0x400 * (s * 4 + f)); }
        for (int k = 0; k < 8; ++k) { const uint32_t fr = T3 + 0x400 * k; wrand(fr, 0x100); w16(fr + 6, fixed + u(ri(0, 12))); w16(fr + 0xa, u(ri(0, 8))); w16(fr + 0xe, u(ri(0, 8))); }
        wrand(OUT, 0x100);   // dst (the frames span T3..T3+0x2000, past T4)
        return Args{{M, u(ri(0, 1)), u(ri(0, 3)), OUT}, {}};
    }, [](A a) { sub_0024f440(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });


    // ======== batch 13
    g_batch = 13;
    check("sub_002091d8", 0x002091d8, N, Ret::None, [] { wfrand(0x0013f3d0, 4, -9, 9); wfrand(0x0013f3e0, 4, -3, 3); return Args{{}, {rf(-1, 1), rf(-1, 1), rf(-1, 1)}}; },
          [](A a) { sub_002091d8(a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("spring_angle", 0x0025bc98, N * 3, Ret::Float, [] {
        wf(OUT, rf(-3, 3)); wf(OUT + 4, rf(-0.5f, 0.5f));
        return Args{{OUT, OUT + 4, u(ri(-1, 2))}, {rf(-3, 3), rf(0, 1), rf(0, 1), rf(-0.1f, 1)}};
    }, [](A a) { return rf32(spring_angle(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.f[0], a.f[1], a.f[2], a.f[3])); });
    check("spawn_particle_19", 0x0026d598, N, Ret::None, [particles2] { particles2(); w32(OV(0x001b20e4), T); w8(T, R()); return Args{{M, M2, u(ri(0, 1))}, {}}; },
          [](A a) { spawn_particle_19(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    check("spawn_particle_2d", 0x002715e8, N, Ret::Int32, [particles2] { particles2(); w32(OV(0x001b2134), T); w8(T, R()); return Args{{M, R(), coin() ? 0xffffffffu : R()}, {rf(0, 2), rf(-9, 9)}}; },
          [](A a) { return r32(spawn_particle_2d(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1])); });


    // ======== batch 14
    g_batch = 14;
    check("sub_00215ef8", 0x00215ef8, N * 2, Ret::None, [] {
        w32(g::PLAYER + 0x1184, ri(0, 4) ? M : 0u); wf(M + 0x2c, rf(0.3f, 3)); w8(0x0015edb3, u(ri(0, 1))); wf(0x0015ee18, rf(-9, 9));
        w32(g::HERO_STATE, ri(0, 3) ? 2u : 5u); wf(g::PLAYER + 0x188, rf(-2, 2));
        return Args{};
    }, [](A) { sub_00215ef8(); return uint64_t{0}; });
    check("sub_002332d0", 0x002332d0, N * 2, Ret::Float, [] {
        wfrand(0x0013f3e0, 4, -3, 3); wfrand(g::PLAYER + 0x270, 4, -1, 1); if (ri(0, 3) == 0) { wf(g::PLAYER + 0x270, 0); wf(g::PLAYER + 0x274, 0); }
        return Args{};
    }, [](A) { return rf32(sub_002332d0()); });
    check("sub_002604f0", 0x002604f0, N * 2, Ret::Int32, [] {
        w16(M + 0x34, ri(0, 3) ? 0x20u : 0u); w32(M + 0x78, T); w32(T + 8, ri(0, 4) ? M2 : 0u);
        wfrand(M2, 8, -3, 3); w32(M2 + 0x3c, R()); wfrand(M + 0x10, 4, -9, 9); wfrand(M + 0xc0, 16, -1, 1);
        w8(M + 0x22, u(ri(0, 4))); w8(M3 + 0x22, u(ri(0, 4)));
        wfrand(T2, 4, -9, 9); wfrand(T2 + 0x10, 4, -3, 3); wrand(OUT, 8);
        return Args{{M3, M, T2, T2 + 0x10, OUT, OUT + 0x10}, {}};
    }, [](A a) { return ri32(sub_002604f0(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5])); });


    // ======== batch 15
    g_batch = 15;
    check("sub_00209540", 0x00209540, N * 2, Ret::None, [] {
        constexpr uint32_t PB = 0x01e40000;   // particle index fixed at 7: its +0x1c word is the
        w32(OV(0x0016016c), PB); w32(OV(0x00160170), 7); w32(OV(0x00160174), 3); w32(OV(0x00160178), R());  // ignored w lane
        seed_rand(); w16(g::PLAYER + 0x904, u(ri(0, 10))); wfrand(g::PLAYER + 0x80, 2, -9, 9); wf(g::PLAYER + 0x2f0, rf(-9, 9));
        w32(OV(0x001b2134), T); w8(T, R());
        return Args{{u(ri(1, 10)), u(ri(0, 8))}, {}};
    }, [](A a) { sub_00209540(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; }, 0x01e40000 + 7 * 64 + 0x1c);
    check("sub_00211d78", 0x00211d78, N * 2, Ret::None, [] {
        wfrand(0x0013f4c0, 4, -3, 3); wfrand(0x0013f390, 16, -1, 1); wf(0x0013f4d4, rf(-0.5f, 0.5f)); wfrand(0x0013f3e0, 4, -3, 3);
        return Args{{u(ri(-1, 2))}, {rf(0, 1), rf(0, 1), rf(-0.1f, 1)}};
    }, [](A a) { sub_00211d78(static_cast<int32_t>(a.i[0]), a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("spawn_particle_35", 0x00272060, N, Ret::Int32, [particles2] {
        particles2(); w32(OV(0x001b2154), T); w8(T, R()); wfrand(M3, 4, -9, 9);
        return Args{{M, R(), R(), u(ri(0, 2)), R(), M3}, {rf(0, 2), rf(0, 2), rf(-9, 9)}};
    }, [](A a) { return r32(spawn_particle_35(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.i[4], a.i[5], a.f[0], a.f[1], a.f[2])); });


    // ======== batch 16
    g_batch = 16;
    check("closest_point_on_segment_xy", 0x0025db00, N * 3, Ret::Float, [] {
        wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9); wfrand(M3, 4, -9, 9); wrand(OUT, 4);
        return Args{{OUT, M, M2, M3}, {}};
    }, [](A a) { return rf32(closest_point_on_segment_xy(a.i[0], a.i[1], a.i[2], a.i[3])); });
    check("region_find", 0x002a3cd8, N * 3, Ret::Int32, [] {
        w32(OV(0x001612d8), u(ri(0, 4))); w32(OV(0x001612d4), T); w32(OV(0x001612d0), T2);
        for (int i = 0; i < 4; ++i) {
            w32(T + 16 * i + 0, u(ri(-4000, 2000))); w32(T + 16 * i + 4, u(ri(-4000, 2000))); w32(T + 16 * i + 8, u(ri(-4000, 2000)));
            w16(T + 16 * i + 0xc, u(ri(0, 6000))); w16(T + 16 * i + 0xe, u(ri(0, 6000)));
            const uint32_t r = T2 + i * 0x1190; wf(r, rf(-4, 0)); wf(r + 4, rf(-4, 0)); w16(r + 0x1e, R());
        }
        wf(OV(0x001ca880) + 8, rf(-1, 1)); wf(OV(0x001ca880) + 0xc, rf(-1, 1)); wf(OV(0x001ca880) + 0x10, rf(0.1f, 1)); wf(OV(0x001ca880) + 0x14, rf(0.1f, 1));
        return Args{{}, {rf(-4, 4), rf(-4, 4), rf(-4, 4)}};
    }, [](A a) { return ri32(region_find(a.f[0], a.f[1], a.f[2])); });


    // ======== batch 17
    g_batch = 17;
    check("sub_00207a58", 0x00207a58, N * 2, Ret::None, [] {
        w16(g::PLAYER + 0x22dc, ri(0, 5) ? u(ri(1, 8)) : 0u);
        for (int i = 0; i < 12; ++i) wf(OV(0x001797d8) + 4 * i, ri(0, 5) ? rf(0.5f, 1.5f) : -1.0f);
        w32(g::PLAYER + 0x2080, M3); wfrand(M3 + 0xc0, 8, -2, 2); w8(g::PLAYER + 0x20a4, ri(0, 4) ? 0u : 1u);
        static const uint32_t ms[] = {0, M, M2};
        for (int k = 0; k < 7; ++k) { w32(g::PLAYER + k * 0x50 + 0x1090, ms[ri(0, 2)]); w32(g::PLAYER + k * 0x50 + 0x1094, ms[ri(0, 2)]); }
        for (uint32_t m : {M, M2}) { wfrand(m + 0x10, 4, -9, 9); wfrand(m + 0xc0, 8, -2, 2); }
        wfrand(0x0013f420, 4, -9, 9); w8(g::PLAYER + 0x20b3, u(ri(0, 3))); wfrand(g::PLAYER, 0x20, -1, 1);
        return Args{};
    }, [](A) { sub_00207a58(); return uint64_t{0}; });
    check("moby_update_light", 0x00257024, N * 3, Ret::Int32, [] {
        w32(OV(0x0015fbc4), u(ri(0, 3)));
        for (int i = 0; i < 4; ++i) {
            wfrand(OV(0x0017ecc0) + 16 * i, 3, 0, 40); wf(OV(0x0017ecc0) + 16 * i + 12, rf(0, 400));
            const uint32_t z = OV(0x0017eec0) + 0x80 * i; wfrand(z, 16, -0.05f, 0.05f); wrand(z + 0x40, 4); w32(z + 0x50, R());
        }
        constexpr uint32_t G = 0x01e20000;
        w32(OV(0x0015fbc0), G); w32(OV(0x0015fb80), T3);
        for (int cy = 0; cy < 3; ++cy) for (int cx = 0; cx < 3; ++cx) {
            const uint32_t cell = G + ((cy * 64 + cx) << 2);
            if (ri(0, 3) == 0) { w32(cell, 0); continue; }
            const uint32_t lo = 0x4000 + (cy * 3 + cx) * 0x40;
            w32(cell, lo); const int n = ri(1, 3); w32(G + lo, u(n));
            for (int i = 0; i < n; ++i) w32(G + lo + 4 + 4 * i, u(ri(0, 15)));
        }
        for (int i = 0; i < 16; ++i) { wfrand(T3 + i * 32, 3, 0, 48); wf(T3 + i * 32 + 12, rf(1, 300)); w32(T3 + i * 32 + 16, R()); }
        wfrand(M + 0x10, 3, 0, 40); wrand(M + 0x38, 2); wrand(M + 0x80, 1);
        return Args{{M}, {}};
    }, [](A a) { return r32(moby_update_light(a.i[0])); });


    // ======== batch 18
    g_batch = 18;
    check("sub_00209848", 0x00209848, N * 2, Ret::None, [] {
        constexpr uint32_t PB = 0x01e40000;   // particle index fixed at 7 (its two w lanes are ignored)
        w32(OV(0x0016016c), PB); w32(OV(0x00160170), 7); w32(OV(0x00160174), 3); w32(OV(0x00160178), R());
        seed_rand(); w16(g::PLAYER + 0x906, u(ri(0, 10))); wfrand(g::PLAYER + 0x80, 2, -9, 9); wfrand(g::PLAYER + 0x100, 2, -1, 1);
        wf(g::PLAYER + 0x2f0, rf(-9, 9)); wf(0x0015ed6c, rf(0, 2)); wf(g::PLAYER + 0x22a4, rf(0, 0.5f)); wf(0x0015ed70, rf(0, 2));
        w32(OV(0x001b210c), T); w8(T, R());
        return Args{{u(ri(1, 10)), u(ri(0, 8))}, {}};
    }, [](A a) { sub_00209848(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; },
          0x01e40000 + 7 * 64 + 0x1c, 0x01e40000 + 7 * 64 + 0x2c);
    check("spawn_particle_22", 0x0026f080, N * 2, Ret::Int32, [particles2] {
        particles2(); w32(OV(0x001b2108), T); w8(T, R()); wf(0x0015ed6c, rf(0, 2));
        w32(g::HERO_STATE, coin() ? 0x31u : 0x10u); wf(g::PLAYER + 0x88, rf(-9, 9)); wf(g::PLAYER + 0x2f0, rf(-9, 9));
        return Args{{M, M2}, {rf(0, 2), coin() ? -1.0f : rf(-9, 9)}};
    }, [](A a) { return r32(spawn_particle_22(a.i[0], a.i[1], a.f[0], a.f[1])); });


    // ======== batch 19
    g_batch = 19;
    check("sub_002076a8", 0x002076a8, N * 3, Ret::None, [] {
        w32(g::PLAYER + 0x22f4, u(ri(0, 1))); w32(g::PLAYER + 0x22f8, R()); w32(g::PLAYER + 0x2080, M); wrand(M + 0x38, 2);
        static const int32_t st[] = {0x80, 0x82, 0x76, 0x10};
        w32(g::HERO_STATE, u(st[ri(0, 3)])); w32(g::PLAYER + 0x198, u(ri(-5, 40))); w32(0x0015ed84, ri(0, 1) ? 0xfu : (ri(0, 1) ? 0x11u : 3u));
        static const uint32_t ms[] = {0, M2, M3};
        for (int k = 0; k < 7; ++k) { w32(g::PLAYER + k * 0x50 + 0x1090, ms[ri(0, 2)]); w32(g::PLAYER + k * 0x50 + 0x1094, ms[ri(0, 2)]); }
        w32(g::PLAYER + 0x10b8, u(ri(-1, 8))); for (int i = 0; i < 9; ++i) w8(0x0013e520 + i, u(ri(0, 1)));
        wfrand(OV(0x0017fec0), 0x10 * 256, 0, 255);
        return Args{};
    }, [](A) { sub_002076a8(); return uint64_t{0}; });
    check("sub_00217450", 0x00217450, N, Ret::None, [particles2] {
        particles2(); w32(OV(0x001b2090), T); w8(T, R()); wf(0x0015ed6c, rf(0, 2)); wfrand(0x0013f3d0, 4, -9, 9);
        w32(0x0015ed84, ri(0, 3) ? 3u : 6u); wf(g::PLAYER + 0x2f4, rf(-9, 9)); wf(g::PLAYER + 0x234, rf(0, 1));
        return Args{};
    }, [](A) { sub_00217450(); return uint64_t{0}; });


    // ======== batch 20
    g_batch = 20;
    check("sub_00209638", 0x00209638, N, Ret::None, [particles2] {
        particles2(); w32(OV(0x001b2138), T); w8(T, R()); w16(g::PLAYER + 0x908, u(ri(0, 6)));
        wf(g::PLAYER + 0x98, rf(-3, 3)); wf(0x0015ed6c, rf(0, 2)); wfrand(g::PLAYER + 0x80, 2, -9, 9); wf(g::PLAYER + 0x2f0, rf(-9, 9));
        return Args{{u(ri(0, 5)), u(ri(0, 6))}, {}};
    }, [](A a) { sub_00209638(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("sub_00211e30", 0x00211e30, N * 2, Ret::None, [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 2))); wf(g::PLAYER + 0x98, rf(-3, 3)); wf(g::PLAYER + 0x184, rf(-0.5f, 0.5f)); wf(g::PLAYER + 0x180, rf(-3, 3));
        wfrand(0x0013f4c0, 4, -3, 3); wfrand(0x0013f390, 16, -1, 1); wf(0x0013f4d4, rf(-0.5f, 0.5f)); wfrand(0x0013f3e0, 4, -3, 3);
        return Args{{u(ri(-1, 2))}, {rf(0, 1), rf(0, 1), rf(-0.1f, 1)}};
    }, [](A a) { sub_00211e30(static_cast<int32_t>(a.i[0]), a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("spark_pairs", 0x002a7e20, N, Ret::None, [particles2] {
        particles2(); w32(OV(0x001b2154), T); w8(T, R()); wfrand(M3, 4, -9, 9);
        return Args{{M3, u(ri(-1, 4))}, {}};
    }, [](A a) { spark_pairs(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });


    // ======== batch 21
    g_batch = 21;
    check("sub_00221b68", 0x00221b68, N * 4, Ret::None, [] {
        w8(g::PLAYER + 0x257, ri(0, 3) ? 1u : 0u); wf(g::PLAYER + 0x2dc, rf(-0.2f, 1)); wf(0x0015ed6c, rf(0.5f, 1.5f)); wf(g::PLAYER + 0x160, rf(0, 5));
        wfrand(g::PLAYER + 0xe0, 4, -1.5f, 1.5f); wf(g::PLAYER + 0x229c, rf(0, 0.3f)); w32(g::PLAYER + 0x208c, u(ri(0, 11)));
        w16(g::PLAYER + 0x1ea, ri(0, 2) ? 0u : 3u); wfrand(g::PLAYER + 0x80, 2, -2, 2); wfrand(g::PLAYER + 0x210, 2, -2, 2);
        wf(g::PLAYER + 0x234, rf(0.5f, 4)); wf(g::PLAYER + 0x98, rf(-3, 3)); wf(g::PLAYER + 0x244, rf(-1, 1)); wf(0x0015ed64, rf(0.5f, 1.5f));
        return Args{};
    }, [](A) { sub_00221b68(); return uint64_t{0}; });
    check("moby_init", 0x0024e930, N * 2, Ret::None, [] {
        const uint32_t cls = u(ri(0, 7));
        for (int i = 0; i < 8; ++i) w8(OV(0x00197bc0) + i, u(ri(0, 3)));
        for (int i = 0; i < 4; ++i) { w32(OV(0x00197680) + 4 * i, ri(0, 2) ? R() : 0u); w32(OV(0x00197300) + 4 * i, ri(0, 3) ? T + 0x100 * i : 0u); }
        for (int i = 0; i < 4; ++i) {
            const uint32_t c = T + 0x100 * i; wrand(c, 0x18); w8(c + 0xc, u(ri(0, 2))); w8(c + 0xf, u(ri(0, 1))); w8(c + 6, u(ri(0, 1)));
            w32(c + 0x40, ri(0, 1) ? R() : 0u); w32(c + 0x48, ri(0, 3) ? T2 + 0x100 * i : 0u);
            const uint32_t set = T2 + 0x100 * i; wrand(set, 0x40); w8(set + 0x10, u(ri(0, 3)));
            for (int f = 0; f < 4; ++f) w32(set + 0x1c + 4 * f, T3 + 0x20 * f);
            w32(c + 0x4c, set);
        }
        w32(OV(0x0015ffd8), T4 - 0x10000);
        wrand(M, 0x40);
        return Args{{M, cls}, {}};
    }, [](A a) { moby_init(a.i[0], a.i[1]); return uint64_t{0}; });


    // ======== batch 22
    g_batch = 22;
    auto turnset = [] {
        w8(g::PLAYER + 0x20b3, 0); wf(g::PLAYER + 0x98, rf(-3, 3)); wf(g::PLAYER + 0x184, rf(-0.5f, 0.5f)); wf(g::PLAYER + 0x180, rf(-3, 3));
        wf(0x0015ed64, rf(0.5f, 1.5f)); wf(0x0015ed6c, rf(0.5f, 1.5f)); wf(g::PLAYER + 0x229c, rf(-1, 1)); w16(g::PLAYER + 0x3be, u(ri(0, 1)));
        w32(g::PLAYER + 0x10b4, ri(0, 3) ? 2u : 1u); static const uint32_t ks[] = {3, 0xf, 0x10, 0x11, 0x15}; w32(g::PLAYER + 0x10b8, ks[ri(0, 4)]);
        w8(g::PLAYER + 0x20a8, u(ri(0, 1)));
        return Args{};
    };
    check("sub_00211e98", 0x00211e98, N * 2, Ret::None, turnset, [](A) { sub_00211e98(); return uint64_t{0}; });
    check("sub_00211f38", 0x00211f38, N * 3, Ret::None, turnset, [](A) { sub_00211f38(); return uint64_t{0}; });
    check("moby_spawn", 0x0024e838, N * 2, Ret::Int32, [] {
        constexpr uint32_t MB = 0x01e60000;  // 8 moby slots
        w32(OV(0x0015ffdc), MB); w32(OV(0x0015ffe0), MB + 0x100 * u(ri(0, 8))); w32(OV(0x0015ffd8), MB); w32(OV(0x0015ffe8), T4);
        w32(OV(0x0015f5cc), u(ri(0, 100))); w32(OV(0x0015ffbc), u(ri(0, 2)));
        for (int i = 0; i < 9; ++i) { const uint32_t m = MB + 0x100 * i; wrand(m, 0x40); w8(m + 0x20, ri(0, 2) ? u(ri(0xfe, 0xff)) : u(ri(0, 0xfd))); w32(m + 0x38, u(ri(0, 120))); w32(m + 0x3c, ri(0, 5) ? 0u : 1u); }
        for (int i = 0; i < 8; ++i) w8(OV(0x00197bc0) + i, u(ri(0, 3)));
        for (int i = 0; i < 4; ++i) { w32(OV(0x00197680) + 4 * i, R()); w32(OV(0x00197300) + 4 * i, 0); }
        return Args{{u(ri(0, 7))}, {}};
    }, [](A a) { return r32(moby_spawn(a.i[0])); });


    // ======== batch 23
    g_batch = 23;
    check("target_score", 0x0020d6b0, N * 3, Ret::Float, [] {
        static const uint32_t st[] = {0x14, 1, 0x1e, 5};
        w32(g::HERO_STATE, st[ri(0, 3)]); wfrand(0x0013f3d0, 4, -9, 9); wf(g::PLAYER + 0x2d8, rf(-9, 9)); wfrand(M + 0x10, 4, -9, 9);
        wfrand(OV(0x00166c80) + 0x140, 4, -9, 9); wfrand(OV(0x00166c80) + 0x154, 2, -3, 3); w16(M + 0xa6, u(ri(480, 560)));
        w32(OV(0x00166c00u - 0x6c28), M - 0x100); w32(OV(0x00166c00u - 0x6c20), M + 0x100); w32(OUT, R());
        return Args{{M, OUT}, {rf(-3, 3), rf(0, 15), rf(-1, 2), rf(-1, 2)}};
    }, [](A a) { return rf32(target_score(a.i[0], hp<int32_t>(a.i[1]), a.f[0], a.f[1], a.f[2], a.f[3])); });
    check("sub_00234808", 0x00234808, N * 2, Ret::None, [] {
        for (uint32_t o : {0x11d0u, 0x11d4u, 0x1220u}) w32(g::PLAYER + o, ri(0, 3) ? (o == 0x11d0 ? M : o == 0x11d4 ? M2 : M3) : 0u);
        for (uint32_t m : {M, M2, M3}) { w32(m + 0x90, R()); w32(m + 0x64, 0); w32(m + 0x24, T); }
        w32(T + 0x1c, T2); w32(T2 + 4, T3); wrand(T3, 4); w8(T3, u(ri(0, 4)));
        w32(OV(0x0015f5c4), ri(0, 1) ? 2u : 0u); wf(0x0015ed6c, rf(0, 2)); wfrand(g::PLAYER + 0x22e8, 3, -3, 3);
        w8(g::PLAYER + 0x1d91, u(ri(0, 1))); wrand(g::PLAYER + 0x1d90, 12);
        return Args{};
    }, [](A) { sub_00234808(); return uint64_t{0}; });


    // ======== batch 24
    g_batch = 24;
    check("sub_002400f0", 0x002400f0, N * 2, Ret::None, [] {
        wrand(M, 4); w16(M + 4, u(1 << ri(0, 9))); w16(M + 6, u(1 << ri(0, 9))); w32(0x0015ee8c, R() & 0xffff00); wrand(OV(0x001827c0), 16);
        static const int32_t ids[] = {-3, -2, -1, 0, 1, 7};
        return Args{{T, M, R() & 0x3fff, R() & 0x3f, R() & 3, R() & 0xff, u(ids[ri(0, 5)])}, {}};
    }, [](A a) { sub_002400f0(a.i[0], a.i[1], sx(a.i[2]), sx(a.i[3]), sx(a.i[4]), sx(a.i[5]), sx(a.i[6])); return uint64_t{0}; });
    check("sub_00227d40", 0x00227d40, N * 3, Ret::None, [] {
        w32(g::PLAYER + 0x198, u(ri(0, 20))); wf(g::PLAYER + 0x690, rf(-0.5f, 0.5f)); wf(g::PLAYER + 0x694, rf(-0.1f, 0.1f));
        w32(g::PLAYER + 0x2094, ri(0, 3) ? 3u : 0x11u); w32(g::PLAYER + 0x20a0, ri(0, 2) ? 3u : 0x11u);
        static const uint32_t st[] = {0x36, 0x7f, 3}; w32(g::HERO_STATE, st[ri(0, 2)]); w32(g::PLAYER + 0x208c, ri(0, 2) ? 0x16u : 2u);
        wf(0x0015ed64, rf(0.5f, 1.5f)); wf(0x0015ed6c, rf(0.5f, 1.5f)); wf(0x0015ed60, rf(0.5f, 1.5f));
        wf(g::PLAYER + 0x8fc, rf(-0.2f, 0.2f)); wf(g::PLAYER + 0x69c, rf(-2, 2)); wf(g::PLAYER + 0x6a0, rf(-0.2f, 0.2f));
        wf(g::PLAYER + 0x8a0, rf(-2, 2)); wf(g::PLAYER + 0x2f0, rf(-2, 2));
        if (ri(0, 4) == 0) { wf(g::PLAYER + 0x690, 0.0001f); wf(g::PLAYER + 0x694, 0.00001f); }
        return Args{};
    }, [](A) { sub_00227d40(); return uint64_t{0}; });


    // ======== batch 25
    g_batch = 25;
    check("path_advance", 0x0025d808, N * 4, Ret::Int32, [] {
        const int n = ri(2, 6);
        w32(T, u(n));
        for (int i = 0; i < n; ++i) { wfrand(T + 0x10 + 16 * i, 3, -9, 9); wf(T + 0x1c + 16 * i, rf(0.5f, 3)); }
        const int i0 = ri(0, n - 1);
        w32(OUT, u(i0)); wf(OUT + 4, rf(0, 0.5f));
        return Args{{T, M, OUT, OUT + 4, u(ri(0, 1))}, {ri(0, 6) == 0 ? 0.0f : rf(-6, 6)}};
    }, [](A a) { return ri32(path_advance(a.i[0], a.i[1], hp<int32_t>(a.i[2]), hp<float>(a.i[3]), static_cast<int32_t>(a.i[4]), a.f[0])); });
    check("sound_start", 0x0028d970, N * 3, Ret::Int32, [] {
        seed_rand();
        w8(M + 0x18, u(ri(0, 1))); w16(M + 0x1a, R()); w32(M + 0x10, u(ri(0, 20))); w32(M + 0x14, ri(0, 2) ? u(ri(21, 60)) : ee_read<uint32_t>(M + 0x10));
        wf(M, rf(0, 4)); wf(M + 4, rf(5, 15)); w32(M + 8, u(ri(-50, 50))); w32(M + 0xc, u(ri(0, 200))); w8(M + 0x19, R());
        for (int i = 0; i < 30; ++i) w8(0x0013e550 + i * 0x70 + 0x74, ri(0, 2) ? 3u : 0u);
        w32(g::PLAYER + 0x2080, M2); w32(g::PLAYER + 0x1090, M3); wfrand(M2 + 0x10, 4, -9, 9); wfrand(M3 + 0x10, 4, -9, 9); w16(M3 + 0xa6, coin() ? 0x472u : 3u);
        wfrand(OV(0x00166dc0), 4, -9, 9); wfrand(T, 4, -9, 9);
        static const uint32_t ms[] = {0, M2, M3};
        return Args{{M, R() & 0x1f, ms[ri(0, 2)], coin() ? T : 0u, u(ri(0, 64))}, {}};
    }, [](A a) { return ri32(sound_start(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]))); });


    // ======== batch 26
    g_batch = 26;
    check("closest_point_on_segment", 0x0025dcd8, N * 4, Ret::Float, [] {
        wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9); wfrand(M3, 4, -9, 9); wrand(OUT, 4);
        if (coin()) { wf(M2 + 8, ee_read<float>(M + 8) + rf(-1, 1)); wf(M3 + 8, ee_read<float>(M + 8) + rf(-1, 1)); }
        return Args{{OUT, M, M2, M3}, {rf(-1, 3)}};
    }, [](A a) { return rf32(closest_point_on_segment(a.i[0], a.i[1], a.i[2], a.i[3], a.f[0])); });


    // ======== batch 27
    g_batch = 27;
    check("sub_0020a9e8", 0x0020a9e8, N * 2, Ret::None, [] {
        seed_rand(); w32(g::HERO_STATE, ri(0, 4) ? 0u : 3u); w32(g::PLAYER + 0x2080, M); w8(M + 0x53, ri(0, 4) ? 0u : 1u);
        for (int k = 0; k < 4; ++k) { w32(g::PLAYER + 0x1060 + 4 * k, u(ri(-1, 3))); wfrand(g::PLAYER + 0x1024 + 0x10 * k, 2, -1, 1); }
        if (coin()) wf(g::PLAYER + 0x1058, 0.0f);
        return Args{};
    }, [](A) { sub_0020a9e8(); return uint64_t{0}; });
    check("sub_00213e68", 0x00213e68, N * 4, Ret::None, [] {
        w8(g::PLAYER + 0x20b3, u(ri(0, 3))); wf(g::PLAYER + 0x180, rf(-3, 3)); wf(g::PLAYER + 0x190, rf(0, 2));
        w32(g::PLAYER + 0x208c, ri(0, 1) ? 4u : 2u); w16(g::PLAYER + 0x30a, u(ri(0, 1))); w16(g::PLAYER + 0x4a4, u(ri(0, 1))); w8(g::PLAYER + 0x257, u(ri(0, 1)));
        static const uint32_t st[] = {0xe, 0x81, 3}; w32(g::HERO_STATE, st[ri(0, 2)]); wf(0x0015ed6c, rf(0.5f, 1.5f));
        wfrand(g::PLAYER + 0xe0, 4, -2, 2); if (ri(0, 5) == 0) { wf(g::PLAYER + 0xe0, 0); wf(g::PLAYER + 0xe4, 0); }
        wfrand(0x0013f430, 4, -2, 2); wfrand(0x0013fc70, 4, -2, 2); wf(g::PLAYER + 0x98, rf(-3, 3)); wf(g::PLAYER + 0x168, rf(-2, 2));
        w32(g::PLAYER + 0x2080, M3); wfrand(M3 + 0x40, 4, -3, 3); wfrand(g::PLAYER, 0x20, -1, 1); wf(0x0013f4e0, rf(0, 2));
        return Args{{}, {rf(0, 0.5f)}};
    }, [](A a) { sub_00213e68(a.f[0]); return uint64_t{0}; });


    // ======== batch 28
    g_batch = 28;
    auto mobyenv = [] {
        constexpr uint32_t MB = 0x01e60000;  // 8 moby slots, some free
        w32(OV(0x0015ffdc), MB); w32(OV(0x0015ffe0), MB + 0x100 * u(ri(0, 8))); w32(OV(0x0015ffd8), MB); w32(OV(0x0015ffe8), T4);
        w32(OV(0x0015f5cc), u(ri(50, 100))); w32(OV(0x0015ffbc), u(ri(0, 2)));
        for (int i = 0; i < 9; ++i) { const uint32_t m = MB + 0x100 * i; wrand(m, 0x40); w8(m + 0x20, ri(0, 2) ? 0xfeu : 3u); w32(m + 0x38, u(ri(0, 40))); w32(m + 0x3c, 0); }
        for (int i = 0; i < 8; ++i) w8(OV(0x00197bc0) + i, u(ri(0, 3)));
        for (int i = 0; i < 4; ++i) { const uint32_t c = T3 + 0x100 * i; w32(OV(0x00197680) + 4 * i, R()); w32(OV(0x00197300) + 4 * i, c); wrand(c, 0x20); wf(c + 0x24, rf(0.5f, 2)); w32(c + 0x48, 0); w32(c + 0x40, 0); }
        // class tables are indexed with cls; tests pass small classes only through lbu(0x197bc0+cls)
    };
    check("spawn_moby_a7", 0x002b4138, N, Ret::Int32, [mobyenv] { mobyenv(); for (int i = 0; i < 0xa8; ++i) w8(OV(0x00197bc0) + i, u(ri(0, 3))); return Args{{R()}, {}}; },
          [](A a) { return r32(spawn_moby_a7(a.i[0])); });
    check("spawn_moby_593", 0x002a99b0, N, Ret::Int32, [mobyenv] {
        mobyenv(); for (int i = 0; i < 0x5f4; ++i) w8(OV(0x00197bc0) + i, u(ri(0, 3)));
        wrand(M, 4); return Args{{R(), M, R()}, {rf(0, 2), rf(-9, 9), rf(-9, 9)}};
    }, [](A a) { return r32(spawn_moby_593(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2])); });
    check("spawn_moby_5f3", 0x002a9aa0, N, Ret::Int32, [mobyenv] {
        mobyenv(); for (int i = 0; i < 0x5f4; ++i) w8(OV(0x00197bc0) + i, u(ri(0, 3)));
        w32(OV(0x001613c8), ri(0, 3) ? 0u : 1u); wrand(M, 4); wf(OV(0x0015f5d4), rf(0, 0.7f)); for (int i = 0; i < 8; ++i) w32(OV(0x001804c0) + 0x10 + i * 0x30, ri(0, 2) ? 1u : 0u);
        return Args{{R(), M}, {}};
    }, [](A a) { return r32(spawn_moby_5f3(a.i[0], a.i[1])); });
    check("sub_00262528", 0x00262528, N, Ret::None, [mobyenv] {
        mobyenv(); w32(M + 0x138, u(ri(0, 4))); w32(M + 0x134, M2); w16(M2 + 0xa6, u(ri(0, 7))); wrand(M2 + 0x30, 4);
        return Args{{M, R() & 0xff, u(ri(0, 10))}, {}};
    }, [](A a) { sub_00262528(a.i[0], a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    auto sndenv = [] {
        seed_rand();
        for (int i = 0; i < 30; ++i) w8(0x0013e550 + i * 0x70 + 0x74, ri(0, 2) ? 3u : 0u);
        for (int i = 0; i < 8; ++i) {
            const uint32_t d = T2 + 32 * i; w8(d + 0x18, u(ri(0, 1))); w16(d + 0x1a, R()); w32(d + 0x10, u(ri(0, 20))); w32(d + 0x14, u(ri(21, 60)));
            wf(d, rf(0, 4)); wf(d + 4, rf(5, 15)); w32(d + 8, u(ri(-50, 50))); w32(d + 0xc, u(ri(0, 200))); w8(d + 0x19, R());
        }
        w32(g::PLAYER + 0x2080, M2); w32(g::PLAYER + 0x1090, M3); wfrand(M2 + 0x10, 4, -9, 9); wfrand(M3 + 0x10, 4, -9, 9);
        wfrand(OV(0x00166dc0), 4, -9, 9);
    };
    check("moby_sound_start", 0x0028dbe8, N * 2, Ret::Int32, [sndenv] {
        sndenv(); w32(M2 + 0x24, ri(0, 5) ? T : 0u); w32(T + 0x28, ri(0, 5) ? T2 : 0u); w8(T + 0xd, u(ri(0, 8)));
        return Args{{u(ri(-1, 8)), R() & 0x1f, coin() ? M2 : 0u}, {}};
    }, [](A a) { return ri32(moby_sound_start(static_cast<int32_t>(a.i[0]), a.i[1], a.i[2])); });
    check("level_sound_start", 0x0028de68, N * 2, Ret::Int32, [sndenv] {
        sndenv(); w32(0x0015ed84, u(ri(0, 3))); for (int i = 0; i < 4; ++i) w8(OV(0x001bd820) + i, u(ri(0, 2)));
        w32(OV(0x0015f574), 0); w32(OV(0x0015f5f0), u(ri(0, 8))); w32(OV(0x0015f5f4), T2);
        return Args{{u(ri(0, 1)), 0, u(ri(0, 1)), R() & 0x1f, coin() ? M2 : 0u}, {}};
    }, [](A a) { return ri32(level_sound_start((int32_t)a.i[0], (int32_t)a.i[1], (int32_t)a.i[2], a.i[3], a.i[4])); });
    check("find_best_target", 0x0020d8f8, N * 2, Ret::Int32, [] {
        static const uint32_t ms[] = {M, M2, M3, T4};
        const int n = ri(0, 4);
        for (int i = 0; i < n; ++i) {
            const uint32_t m = ms[i]; w32(OV(0x001aba00) + 4 * i, m); w16(m + 0x34, ri(0, 3) ? 0x1020u : 0x20u); w32(m + 0x78, T + 0x40 * i);
            w32(T + 0x40 * i, T2 + 0x10 * i); wf(T2 + 0x10 * i, ri(0, 4) ? rf(0.5f, 2) : 0.0f); wfrand(m + 0x10, 4, -9, 9); w16(m + 0xa6, u(ri(480, 560)));
        }
        w32(OV(0x001aba00) + 4 * n, 0);
        w32(g::HERO_STATE, ri(0, 3) ? 3u : 1u); wfrand(0x0013f3d0, 4, -9, 9); wfrand(OV(0x00166c80) + 0x140, 4, -9, 9); wfrand(OV(0x00166c80) + 0x154, 2, -3, 3);
        w32(OV(0x00166c00u - 0x6c28), M); w32(OV(0x00166c00u - 0x6c20), M3 + 0x100);
        return Args{{}, {rf(5, 20), rf(-3, 3), rf(-1, 3), rf(-1, 3)}};
    }, [](A a) { return r32(find_best_target(a.f[0], a.f[1], a.f[2], a.f[3])); });


    // ======== batch 29
    g_batch = 29;
    auto heroSnd = [sndenv] {
        sndenv(); w32(M2 + 0x24, T); w32(T + 0x28, T2); w8(T + 0xd, 8);
        w32(M + 0x24, T); wfrand(M + 0x10, 4, -9, 9);
        w8(g::PLAYER + 0x20a4, ri(0, 2) ? 0u : 3u); w32(g::PLAYER + 0xa88, ri(0, 3) ? M : 0u); wfrand(g::PLAYER + 0x80, 4, -9, 9);
    };
    check("hero_sound_start", 0x00216de8, N * 2, Ret::Int32, [heroSnd] { heroSnd(); return Args{{u(ri(0, 8)), R() & 0x1f}, {}}; },
          [](A a) { return ri32(hero_sound_start(static_cast<int32_t>(a.i[0]), a.i[1])); });
    check("hero_sound_slot", 0x00216e48, N * 2, Ret::None, [heroSnd] {
        heroSnd(); for (int k = 0; k < 8; ++k) w32(g::PLAYER + 0x2218 + 4 * k, ri(0, 2) ? 0xffffffffu : 3u);
        return Args{{u(ri(0, 7)), coin() ? M2 : M, u(ri(0, 8))}, {}};
    }, [](A a) { hero_sound_slot(static_cast<int32_t>(a.i[0]), a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    check("moby_update_sound", 0x0024ec80, N * 3, Ret::None, [heroSnd] {
        heroSnd(); w8(M2 + 0x7c, ri(0, 3) ? u(ri(0, 7)) : 0xffu); w8(M2 + 0x7d, ri(0, 2) ? 0xffu : u(ri(0, 7)));
        for (int i = 0; i < 8; ++i) { const uint32_t e = 0x0013e550 + i * 0x70; w32(e + 0x88, coin() ? M2 : M); w16(e + 0x7e, u(ri(0, 7))); w32(e + 0x8c, R()); }
        return Args{{M2}, {}};
    }, [](A a) { moby_update_sound(a.i[0]); return uint64_t{0}; });
    check("wrench_hit_sparks", 0x00217368, N, Ret::None, [heroSnd, particles2] {
        heroSnd(); particles2(); w32(OV(0x001b2154), T3 + 0x800); w8(T3 + 0x800, R());
        w32(g::PLAYER + 0xa64, u(ri(0, 1))); w8(g::PLAYER + 0x20b3, u(ri(0, 1)));
        for (int i = 0; i < 4; ++i) { const uint32_t m = T4 + 0x100 * i; w32(OV(0x00177f00) + 4 * i, ri(0, 4) ? m : 0u); w32(m + 0x24, ri(0, 4) ? T3 + 0x40 * i : 0u); w16(T3 + 0x40 * i + 0x46, ri(0, 1) ? 5u : (ri(0, 1) ? 9u : 2u)); wfrand(m + 0x10, 4, -9, 9); }
        return Args{{M2, u(ri(-1, 4))}, {}};
    }, [](A a) { wrench_hit_sparks(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("place_moby_5f3", 0x002a9b80, N, Ret::None, [mobyenv] {
        mobyenv(); for (int i = 0; i < 0x5f4; ++i) w8(OV(0x00197bc0) + i, u(ri(0, 3)));
        w32(OV(0x001613c8), ri(0, 2) ? 0u : M3); w16(M3 + 0xa6, coin() ? 0x5f3u : 3u); w32(M3 + 0x78, T2); wrand(M, 4);
        wf(OV(0x0015f5d4), rf(0, 0.7f)); for (int i = 0; i < 8; ++i) w32(OV(0x001804c0) + 0x10 + i * 0x30, ri(0, 2) ? 1u : 0u);
        return Args{{R(), M}, {}};
    }, [](A a) { place_moby_5f3(a.i[0], a.i[1]); return uint64_t{0}; });


    // ======== batch 30
    g_batch = 30;
    check("cheat_input", 0x00206e00, N * 6, Ret::None, [] {
        w32(0x0015eea0, ri(0, 4) ? 1u : 0u); w32(0x0015ee20, 0); wf(0x0015ed68, 1.0f);
        w32(g::PLAYER + 0x198, coin() ? 15u : (coin() ? 60u : 7u));
        static const int32_t subs[] = {4, 6, 2}; w32(g::PLAYER + 0x208c, u(subs[ri(0, 2)]));
        static const int32_t sts[] = {0xb, 0x11, 0xa, 0xe, 0x14, 0x13, 0x15, 0x22, 4, 8, 3}; w32(g::HERO_STATE, u(sts[ri(0, 10)]));
        w32(g::PLAYER + 0x450, u(ri(0, 3))); w8(0x0015edb4, u(ri(0, 1))); w32(g::PLAYER + 0xa60, u(ri(0, 3)));
        if (coin()) { w32(g::PLAYER + 0x198, 15); w32(g::PLAYER + 0x208c, 4); w32(g::HERO_STATE, 0xb); w32(g::PLAYER + 0x450, u(ri(0, 1))); }
        w32(g::PLAYER + 0x21d4, u(ri(0, 15))); for (int i = 0; i < 16; ++i) w8(g::PLAYER + 0x21c4 + i, u(ri(1, 2)));
        for (int c = 0; c < 12; ++c) { const int n = ri(0, 4); for (int i = 0; i < 16; ++i) w8(OV(0x00179700) + c * 16 + i, i < n ? u(ri(1, 2)) : 0u); }
        for (int c = 0; c < 12; ++c) { w8(0x0015edb0 + c, u(ri(0, 1))); w8(0x0015edc0 + c, 0); }
        w32(OV(0x0017943c), 2); w32(OV(0x0015f660), T);   // messages 0x4fbe / 0x4fbf -> two different strings
        w32(T + 4, 0x4fbe); w32(T + 0, T2); w32(T + 0x14, 0x4fbf); w32(T + 0x10, T2 + 0x40);
        for (int i = 0; i < 0x80; ++i) w8(T2 + i, (i & 0x3f) < 0x30 ? u(ri(0x20, 0x7e)) : 0u);
        return Args{};
    }, [](A) { cheat_input(); return uint64_t{0}; });
    check("align_to_up", 0x00216130, N * 2, Ret::None, [] {
        Mat4 r; Vec4 e{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(g::PLAYER) = r;
        wfrand(g::PLAYER + 0x30, 4, -9, 9);
        Vec4 u2{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), 1.0f, 0.0f}; vec3_set_length(u2, u2, 1.0f); ee_ref<Vec4>(M) = u2;
        wf(0x0013f3f4, rf(-0.3f, 0.3f)); wfrand(0x0013f3e0, 4, -3, 3);
        return Args{{M, T, coin() ? OUT : 0u}, {rf(0, 1), rf(0, 1), rf(-0.1f, 1)}};
    }, [](A a) { align_to_up(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });


    // ======== batch 31: edge cases found by branch coverage (gcov) of the earlier batches
    g_batch = 31;
    check("cheat_input/edges", 0x00206e00, N * 4, Ret::None, [] {
        w32(0x0015eea0, 0); w32(0x0015ee20, ri(0, 3) ? 1u : 0u); wf(0x0015ed68, 1.0f);
        const int k = ri(0, 3);
        if (k == 0) { w32(g::PLAYER + 0x198, 60); w32(g::HERO_STATE, ri(0, 1) ? 4u : 8u); }
        else if (k == 1) { w32(g::PLAYER + 0x198, 15); w32(g::PLAYER + 0x208c, 6); w32(g::HERO_STATE, 0x13); w32(g::PLAYER + 0xa60, u(ri(0, 2))); }
        else { w32(g::PLAYER + 0x198, 15); w32(g::PLAYER + 0x208c, 4); w32(g::HERO_STATE, 0xb); w32(g::PLAYER + 0x450, ri(0, 1) ? 1u : 3u); }
        w8(0x0015edb4, u(ri(0, 1)));
        w32(g::PLAYER + 0x21d4, u(ri(0, 15))); for (int i = 0; i < 16; ++i) w8(g::PLAYER + 0x21c4 + i, u(ri(1, 3)));
        for (int c = 0; c < 12; ++c) { const int n = ri(0, 3); for (int i = 0; i < 16; ++i) w8(OV(0x00179700) + c * 16 + i, i < n ? u(ri(1, 3)) : 0u); }
        for (int c = 0; c < 12; ++c) { w8(0x0015edb0 + c, u(ri(0, 1))); w8(0x0015edc0 + c, 0); }
        w32(OV(0x0017943c), 0); w32(OV(0x0015f660), T);
        return Args{};
    }, [](A) { cheat_input(); return uint64_t{0}; });
    check("sub_00206cf8/edges", 0x00206cf8, N, Ret::None, [] {
        w16(0x00141d28, 0); w8(0x0013d4c4, 1); w32(g::HERO_SUBSTATE, 0); w32(g::HERO_STATE, 3);
        w32(OV(0x0015f5cc), 0x80u * u(ri(1, 3))); w16(0x00141be8, 0); w16(0x001418f0, 0);
        for (int i = 0; i < 0x25; ++i) { w32(OV(0x00179ac0) + 8 + i * 0x4c, 0); w8(0x0013d4c0 + i, ri(0, 5) ? 1u : 0u); }
        w32(OV(0x00179410), 0); w32(OV(0x00179434), 0xffffffffu); w32(0x001516d0 + 0x50, 0); w32(0x001516d0 + 0x1c, 0xffffffffu);
        for (int i = 0; i < 300; ++i) w16(OV(0x00179450) + 2 * i, u(ri(0, 40)));
        w32(0x0015ee30, u(ri(0, 6))); for (int i = 0; i < 8; ++i) w16(0x00141968 + 8 * i, R() & 0xfff);
        return Args{};
    }, [](A) { sub_00206cf8(); return uint64_t{0}; });
    check("sub_002076a8/edges", 0x002076a8, N, Ret::None, [] {
        w32(g::PLAYER + 0x22f4, 1); w32(g::PLAYER + 0x22f8, R() & 0xff050505u); w32(g::PLAYER + 0x2080, M); wrand(M + 0x38, 2);
        w32(g::HERO_STATE, 3); w32(g::PLAYER + 0x10b8, 0xffffffffu);
        return Args{};
    }, [](A) { sub_002076a8(); return uint64_t{0}; });
    check("sub_0020d4d0/edges", 0x0020d4d0, N, Ret::Int32, [] {
        static const uint32_t ks[] = {0x11, 0xa, 0x14, 0x19, 3};
        w32(g::PLAYER + 0x10b4, 2); w32(g::PLAYER + 0x10b8, ks[ri(0, 4)]);
        return Args{{0}, {}};
    }, [](A a) { return ri32(sub_0020d4d0(static_cast<int32_t>(a.i[0]))); });
    check("sub_0020d658/edges", 0x0020d658, N, Ret::Float, [] {
        w32(g::PLAYER + 0x21ac, u(ri(0, 40))); w32(g::PLAYER + 0x21a8, u(ri(-40, -1)));
        for (int i = -64; i < 32; ++i) wf(g::PLAYER + 0x2128 + 4 * i, rf(-5, 5));
        return Args{{u(ri(0, 40))}, {}};
    }, [](A a) { return rf32(sub_0020d658(static_cast<int32_t>(a.i[0]))); });
    check("align_to_up/edges", 0x00216130, N, Ret::None, [] {
        Mat4 r{}; r.r[0] = Vec4{0, 1, 0, 0}; r.r[1] = Vec4{0, 0, 1, 0}; r.r[2] = Vec4{1, 0, 0, 0}; r.r[3] = Vec4{0, 0, 0, 1};
        ee_ref<Mat4>(g::PLAYER) = r; ee_ref<Vec4>(M) = Vec4{rf(-0.01f, 0.01f), 1.0f, rf(-0.01f, 0.01f), 0.0f};
        wf(0x0013f3f4, rf(-0.3f, 0.3f));
        return Args{{M, T, coin() ? OUT : 0u}, {rf(0, 1), rf(0, 1), rf(-0.1f, 1)}};
    }, [](A a) { align_to_up(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });
    check("sub_00213e68/edges", 0x00213e68, N * 2, Ret::None, [] {
        w8(g::PLAYER + 0x20b3, 0); wf(g::PLAYER + 0x180, rf(-3, 3)); wf(g::PLAYER + 0x190, coin() ? 0.0f : rf(0, 2));
        w32(g::PLAYER + 0x208c, 2); w16(g::PLAYER + 0x30a, 0); w32(g::HERO_STATE, 3);
        wfrand(g::PLAYER + 0xe0, 4, -2, 2); if (coin()) { wf(g::PLAYER + 0xe0, 0); wf(g::PLAYER + 0xe4, 0); }
        wfrand(0x0013f430, 4, -2, 2); wfrand(0x0013fc70, 4, -2, 2); wf(g::PLAYER + 0x98, rf(-3, 3)); wf(g::PLAYER + 0x168, rf(-2, 2));
        return Args{{}, {rf(0, 20)}};
    }, [](A a) { sub_00213e68(a.f[0]); return uint64_t{0}; });
    check("sub_00221b68/edges", 0x00221b68, N, Ret::None, [] {
        w8(g::PLAYER + 0x257, 0); w16(g::PLAYER + 0x1ea, 3);
        wfrand(g::PLAYER + 0x80, 2, -2, 2); wf(g::PLAYER + 0x210, ee_read<float>(g::PLAYER + 0x80) + rf(-0.02f, 0.02f)); wf(g::PLAYER + 0x214, ee_read<float>(g::PLAYER + 0x84) + rf(-0.02f, 0.02f));
        wf(g::PLAYER + 0x234, rf(1, 4)); wf(g::PLAYER + 0x98, rf(-3, 3)); wf(g::PLAYER + 0x244, rf(-1, 1)); wf(0x0015ed64, 1.0f); wf(0x0015ed6c, 1.0f);
        return Args{};
    }, [](A) { sub_00221b68(); return uint64_t{0}; });
    check("sub_00227d40/edges", 0x00227d40, N, Ret::None, [] {
        w32(g::PLAYER + 0x198, 50); w32(g::HERO_STATE, 3); wf(g::PLAYER + 0x690, rf(-0.0009f, 0.0009f)); wf(g::PLAYER + 0x694, rf(-0.0001f, 0.0002f));
        wf(0x0015ed64, 1.0f); wf(0x0015ed6c, 1.0f); wf(0x0015ed60, 1.0f); wf(0x0015ed68, 1.0f);
        return Args{};
    }, [](A) { sub_00227d40(); return uint64_t{0}; });
    check("sub_00234808/edges", 0x00234808, N, Ret::None, [] {
        w32(g::PLAYER + 0x11d0, 0); w32(g::PLAYER + 0x11d4, 0); w32(g::PLAYER + 0x1220, M3);
        w32(M3 + 0x90, R()); w32(M3 + 0x64, 0); w32(M3 + 0x24, T); w32(T + 0x1c, T2); w32(T2 + 4, T3); wrand(T3, 4); w8(T3, u(ri(0, 4)));
        wrand(g::PLAYER + 0x1d90, 12); w8(g::PLAYER + 0x1d91, 0); wf(0x0015ed6c, rf(0, 2)); wfrand(g::PLAYER + 0x22e8, 3, -3, 3);
        return Args{};
    }, [](A) { sub_00234808(); return uint64_t{0}; });
    check("path_advance/edges", 0x0025d808, N * 2, Ret::Int32, [] {
        const int n = ri(2, 5);
        w32(T, u(n));
        for (int i = 0; i < n; ++i) { wfrand(T + 0x10 + 16 * i, 3, -9, 9); wf(T + 0x1c + 16 * i, rf(0.5f, 1)); }
        w32(OUT, u(ri(0, n - 1))); wf(OUT + 4, rf(1.5f, 4));
        return Args{{T, M, OUT, OUT + 4, u(ri(0, 1))}, {rf(0, 2)}};
    }, [](A a) { return ri32(path_advance(a.i[0], a.i[1], hp<int32_t>(a.i[2]), hp<float>(a.i[3]), static_cast<int32_t>(a.i[4]), a.f[0])); });
    check("sub_0024a7e8/edges", 0x0024a7e8, N, Ret::None, [] {
        wfrand(OV(0x00182a70), 0x13 * 4, -300, 300); w32(0x0015ed84, u(ri(-5, -2)));
        return Args{{OUT, OUT + 4, u(ri(-9, -2))}, {rf(-2, 2), rf(-2, 2)}};
    }, [](A a) { sub_0024a7e8(hp<float>(a.i[0]), hp<float>(a.i[1]), static_cast<int32_t>(a.i[2]), a.f[0], a.f[1]); return uint64_t{0}; });
    check("sub_0025abf0/edges", 0x0025abf0, N, Ret::Float, [] { wfrand(M, 4, -9, 9); ee_ref<Vec4>(M2) = ee_ref<Vec4>(M); wf(M2 + 8, rf(-9, 9)); return Args{{M, M2, OUT}, {rf(0.1f, 3), rf(0, 2)}}; },
          [](A a) { return rf32(sub_0025abf0(a.i[0], a.i[1], hp<float>(a.i[2]), a.f[0], a.f[1])); });
    check("sub_0025a420/edges", 0x0025a420, N, Ret::Int32, [] {
        w8(M + 0xa4, u(ri(0, 7))); for (int i = 0; i < 8; ++i) { w32(OV(0x00178100) + i * 64 + 0x34, M); w32(OV(0x00178100) + i * 64 + 0x24, R()); }
        return Args{{M, 0, u(ri(0, 1))}, {}};
    }, [](A a) { return r32(sub_0025a420(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))); });
    check("alloc_particle/edges", 0x002678b8, N, Ret::Int32, [] {
        constexpr uint32_t PB = 0x01e40000;
        w32(OV(0x0016016c), PB); w32(OV(0x00160170), u(ri(0x7f0, 0x7ff))); w32(OV(0x00160174), 0x7ff); w32(OV(0x00160178), R());
        for (int i = 0xf8; i < 0x100; ++i) w8(OV(0x001b1880) + i, ri(0, 3) ? 0xffu : R());
        return Args{{R() & 0xff}, {}};
    }, [](A a) { return r32(alloc_particle(a.i[0])); });
    check("sound_start/edges", 0x0028d970, N, Ret::Int32, [] {
        seed_rand();
        w8(M + 0x18, 0); w16(M + 0x1a, R()); w32(M + 0x10, 5); w32(M + 0x14, 9);
        wf(M, rf(0, 4)); wf(M + 4, rf(5, 15)); w32(M + 8, 40); w32(M + 0xc, 200); w8(M + 0x19, R());
        for (int i = 0; i < 30; ++i) w8(0x0013e550 + i * 0x70 + 0x74, ri(0, 6) ? 3u : 0u);
        w32(g::PLAYER + 0x2080, M2); w32(g::PLAYER + 0x1090, M2); wfrand(M3 + 0x10, 4, -9, 9); w16(M3 + 0xa6, coin() ? 0x472u : 3u);
        wfrand(OV(0x00166dc0), 4, -9, 9);
        return Args{{M, R() & 0x1b, M3, 0u, 0x40}, {}};
    }, [](A a) { return ri32(sound_start(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]))); });
    check("sub_0024f440/edges", 0x0024f440, N, Ret::None, [] {
        w32(M + 0x14, T); w8(T + 8, u(ri(2, 20))); w32(M + 0x18, T2);
        const uint32_t fixed = 2u * u(ri(0, 2)); w8(T2 + 8, fixed); w32(T2 + 0x1c, T2 + 0x100);
        for (int i = 0; i < 8; ++i) w8(T2 + 0x104 + i, u(ri(0, 15)));
        w32(T2 + 0x48, T2 + 0x200); w32(T2 + 0x200 + 0x1c, T3); wrand(T3, 0x100); w16(T3 + 6, fixed / 2); w16(T3 + 0xa, u(ri(0, 8))); w16(T3 + 0xe, u(ri(0, 8)));
        wrand(T4, 0x100);
        return Args{{M, 0, 0, T4}, {}};
    }, [](A a) { sub_0024f440(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
    check("find_best_target/edges", 0x0020d8f8, N, Ret::Int32, [] {
        w32(OV(0x001aba00), M); w32(OV(0x001aba04), M2); w32(OV(0x001aba08), 0);
        w16(M + 0x34, 0x1000); w16(M2 + 0x34, 0x1020); w32(M2 + 0x78, T); w32(T, T2); wf(T2, 1.0f); wfrand(M2 + 0x10, 4, -9, 9);
        w32(g::HERO_STATE, 3); wfrand(0x0013f3d0, 4, -9, 9); w32(OV(0x00166c00u - 0x6c28), 0); w32(OV(0x00166c00u - 0x6c20), 0);
        return Args{{}, {rf(5, 20), rf(-3, 3), rf(-1, 3), rf(-1, 3)}};
    }, [](A a) { return r32(find_best_target(a.f[0], a.f[1], a.f[2], a.f[3])); });


    // ======== batch 32
    g_batch = 32;
    auto heroPose = [] {
        Mat4 r; Vec4 e{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(g::PLAYER) = r;
        wfrand(g::PLAYER + 0x30, 4, -9, 9); wfrand(g::PLAYER + 0x80, 4, -9, 9);
        Vec4 n1{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), 1.0f, 0.0f}; vec3_set_length(n1, n1, 1.0f); ee_ref<Vec4>(g::PLAYER + 0x270) = n1;
        Vec4 n2{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), -1.0f, 0.0f}; vec3_set_length(n2, n2, 1.0f); ee_ref<Vec4>(g::PLAYER + 0x290) = n2;
        wf(0x0013f3f4, rf(-0.3f, 0.3f)); wfrand(0x0013f3e0, 4, -3, 3);
        wf(0x0015ed64, rf(0.5f, 2)); wf(0x0015ed60, rf(0.5f, 2)); wf(0x0015ed6c, rf(0.5f, 2));
        w8(g::PLAYER + 0x20b3, u(ri(0, 2)));
        w16(g::PLAYER + 0x30e, ri(0, 1) ? 0u : u(ri(1, 30))); w16(g::PLAYER + 0x1f8, u(ri(0, 1)));
    };
    check("sub_00212ff0", 0x00212ff0, N * 4, Ret::None, [heroPose] {
        heroPose(); if (ri(0, 2)) w16(g::PLAYER + 0x30e, 0);
        wfrand(g::PLAYER + 0xe0, 4, -2, 2); wfrand(0x0013f3d0, 4, -3, 3);
        wf(g::PLAYER + 0x2dc, rf(-1, 0.3f)); w32(g::HERO_STATE, ri(0, 5) ? 3u : 0x3eu);
        ee_ref<Vec4>(g::PLAYER + 0x2a0) = ee_ref<Vec4>(g::PLAYER + 0x80); wf(g::PLAYER + 0x2a0, ee_read<float>(g::PLAYER + 0x80) + rf(-1, 1)); wf(g::PLAYER + 0x2a4, ee_read<float>(g::PLAYER + 0x84) + rf(-0.3f, 0.3f));
        wf(g::PLAYER + 0x2d8, ee_read<float>(g::PLAYER + 0x88) + rf(-0.2f, 1)); wf(g::PLAYER + 0xe8, rf(-0.3f, 0.3f)); wf(g::PLAYER + 0x108, rf(-0.5f, 0.3f));
        wf(g::PLAYER + 0x304, rf(-1, 1)); w8(g::PLAYER + 0x257, u(ri(0, 1))); wf(g::PLAYER + 0x234, rf(0, 3));
        wfrand(g::PLAYER + 0x210, 2, -9, 9); wf(OV(0x00166c00u - 0x7e9c), rf(0.5f, 2));
        return Args{};
    }, [](A) { sub_00212ff0(); return uint64_t{0}; });
    check("sub_002163f0", 0x002163f0, N * 3, Ret::None, [heroPose] {
        heroPose(); wf(g::PLAYER + 0x2dc, rf(0, 2)); w32(0x0015ed84, u(ri(1, 3)));
        return Args{};
    }, [](A) { sub_002163f0(); return uint64_t{0}; });
    check("hero_delayed_sounds", 0x00216f10, N * 2, Ret::None, [heroSnd] {
        heroSnd(); for (int k = 0; k < 8; ++k) w32(g::PLAYER + 0x2218 + 4 * k, ri(0, 2) ? 0xffffffffu : 3u);
        for (int k = 0; k < 8; ++k) { const uint32_t e = g::PLAYER + 0x21d8 + 8 * k; w16(e, R()); w16(e + 2, u(ri(0, 8))); w16(e + 4, u(ri(-1, 3))); w16(e + 6, R() & 0x1f); }
        return Args{};
    }, [](A) { hero_delayed_sounds(); return uint64_t{0}; });



    // ======== batch 33
    g_batch = 33;
    check("hero_tilt_update", 0x002165b8, N * 3, Ret::None, [heroPose] {
        heroPose(); if (coin()) w8(g::PLAYER + 0x20b3, 0); if (coin()) w16(g::PLAYER + 0x1f8, 0);
        if (ri(0, 4) == 0) w8(g::PLAYER + 0x20b3, u(ri(3, 5)));
        wf(g::PLAYER + 0x2dc, rf(0, 2)); w32(0x0015ed84, u(ri(1, 3)));
        static const int32_t sts[] = {3, 0x25, 0x26, 0x27, 0x77, 0x31, 0x2d, 0x2c, 8};
        w32(g::HERO_STATE, u(ri(0, 2) ? 3 : sts[ri(0, 8)]));
        static const int32_t subs[] = {0, 0x11, 0x15, 0x16, 2};
        w32(g::PLAYER + 0x208c, u(ri(0, 2) ? 0 : subs[ri(0, 4)]));
        wf(g::PLAYER + 0x90, rf(-3, 3)); wf(g::PLAYER + 0x94, rf(-3, 3)); wf(g::PLAYER + 0xa0, rf(-1, 1)); wf(g::PLAYER + 0xa4, rf(-1, 1));
        return Args{};
    }, [](A) { hero_tilt_update(); return uint64_t{0}; });
    check("hero_gait_update", 0x00211380, N * 4, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        w8(P + 0x20a4, u(ri(0, 5)));
        static const int32_t subs[] = {4, 0x11, 0x12, 3, 0};
        w32(P + 0x208c, u(subs[ri(0, 4)]));
        static const int32_t sts[] = {6, 4, 0x7f, 3, 0x10};
        w32(g::HERO_STATE, u(sts[ri(0, 4)]));
        w32(P + 0x198, u(ri(0, 20))); w32(P + 0x420, u(ri(0, 20))); w16(P + 0x41e, u(ri(0, 1))); wf(P + 0x434, rf(0, 2));
        w8(P + 0x12e2, u(ri(0, 1))); w32(P + 0x300, u(ri(0, 1))); w8(P + 0x257, u(ri(0, 1))); w32(P + 0x2094, coin() ? 0x12u : 3u); w8(P + 0x12e4, u(ri(0, 1)));
        wfrand(P + 0x210, 2, -2, 2); wfrand(P + 0x80, 2, -2, 2); wf(P + 0x234, rf(0, 4)); wf(P + 0x238, rf(-1, 1));
        wfrand(P + 0x220, 4, 0, 5); wfrand(P + 0x230, 1, 0, 5);
        wf(0x0015ed60, rf(0.5f, 2)); wf(0x0015ed64, rf(0.5f, 2)); wf(0x0015ed6c, rf(0.5f, 2));
        return Args{};
    }, [](A) { hero_gait_update(); return uint64_t{0}; });
    // a consistent spatial grid: empty cells/pool, some foreign bitmap bits, then K mobies inserted by the port
    auto gridenv = [] {
        for (uint32_t a = OV(0x0019b7e0); a < OV(0x0019b7e0) + 0x4000; a += 4) w32(a, 0);
        for (uint32_t a = OV(0x001ab7e0); a < OV(0x001ab7e0) + 0xc0; a += 4) w32(a, ri(0, 3) ? 0u : (R() & 0x0f0f0f0fu));
        const int k = ri(1, 47);
        const int spread = ri(0, 2) ? 1 : 4;   // dense: up to ~47 ids per cell (runs of 1..4 blocks)
        for (int i = 0; i < 48; ++i) { const uint32_t m = T4 + 0x100 * i; w32(m + 0xa0, 0xffffffffu); w32(m + 0xac, u(0x100 + i)); }
        for (int i = 0; i < k; ++i) {
            const uint32_t x0 = u(ri(0, spread)), y0 = u(ri(0, spread));
            moby_grid_move(T4 + 0x100 * i, x0 | y0 << 8 | (x0 + u(ri(0, 3))) << 16 | (y0 + u(ri(0, 3))) << 24);
        }
        for (int j = ri(0, k); j > 0; --j) {   // some moves/removals so cells also sit in shrunk states
            const uint32_t x0 = u(ri(0, spread)), y0 = u(ri(0, spread));
            moby_grid_move(T4 + 0x100 * u(ri(0, k - 1)), coin() ? 0x80807f7fu : (x0 | y0 << 8 | (x0 + u(ri(0, 2))) << 16 | (y0 + u(ri(0, 2))) << 24));
        }
        return k;
    };
    check("moby_grid_move", 0x00250b20, N * 4, Ret::None, [gridenv] {
        const int k = gridenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k));   // index k: a moby not in the grid yet
        const uint32_t x0 = u(ri(0, 5)), y0 = u(ri(0, 5));
        const uint32_t r = ri(0, 5) ? (x0 | y0 << 8 | (x0 + u(ri(0, 3))) << 16 | (y0 + u(ri(0, 3))) << 24) : 0x80807f7fu;
        return Args{{m, r}, {}};
    }, [](A a) { moby_grid_move(a.i[0], a.i[1]); return uint64_t{0}; });
    check("moby_retire", 0x0024eb68, N * 2, Ret::None, [gridenv] {
        const int k = gridenv();
        w32(OV(0x0015ffdc), T4 + 0x100 * u(ri(0, 48))); w32(OV(0x0015f5cc), R());
        return Args{{T4 + 0x100 * u(ri(0, k))}, {}};
    }, [](A a) { moby_retire(a.i[0]); return uint64_t{0}; });



    // ======== batch 34
    g_batch = 34;
    check("hero_release_attached", 0x00228fa8, N * 2, Ret::None, [gridenv] {
        const int k = gridenv();
        w32(OV(0x0015ffdc), T4 + 0x100 * u(ri(0, 48))); w32(OV(0x0015f5cc), R());
        w32(g::PLAYER + 0x1614, coin() ? T4 + 0x100 * u(ri(0, k - 1)) : 0u);
        w32(g::PLAYER + 0x1180, coin() ? M : 0u); w32(g::PLAYER + 0x1184, coin() ? M2 : 0u); w16(M + 0x34, R()); w16(M2 + 0x34, R());
        return Args{};
    }, [](A) { hero_release_attached(); return uint64_t{0}; });
    check("retire_children", 0x00262840, N * 2, Ret::None, [gridenv] {
        const int k = gridenv();
        w32(OV(0x0015ffdc), T4 + 0x100 * u(ri(0, 48))); w32(OV(0x0015f5cc), R());
        w32(M + 0x13c, u(ri(0, 3))); const int n = ri(0, 6); w32(M + 0x138, u(n));
        bool used[48] = {};
        for (int i = 0; i < 6; ++i) {
            const int j = ri(0, k - 1);
            const bool ok = !used[j] && coin(); if (ok) used[j] = true;
            w32(M + 0x120 + 4 * i, ok ? T4 + 0x100 * u(j) : 0u);
        }
        return Args{{M}, {}};
    }, [](A a) { retire_children(a.i[0]); return uint64_t{0}; });



    // ======== batch 35
    g_batch = 35;
    check("item_list_build", 0x0029a400, N * 4, Ret::None, [] {
        for (int i = 0; i < 12; ++i) w8(0x0015edd0 + i, ri(0, 5) ? (coin() ? 0x40u : 0u) | u(ri(0, 0x24)) : 0xffu);
        for (int i = 0; i < 0x40; ++i) { w16(OV(0x001c40b0) + 0x18 * i + 8, ri(0, 3) ? 1u : 0u); w32(OV(0x001c2108) + 0x14 * i, R()); }
        for (int i = 0; i < 0x25; ++i) w8(0x0013d4c0 + i, ri(0, 2) ? u(ri(1, 3)) : 0u);
        wrand(OV(0x001ca4c0) + 0xd0, 0x300);
        return Args{{u(ri(0, 1))}, {}};
    }, [](A a) { item_list_build(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });



    // ======== batch 36
    g_batch = 36;
    auto fbits = [](float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; };
    check("moby_anim_advance", 0x00250480, N * 6, Ret::None, [heroSnd, fbits] {
        heroSnd();
        w32(OV(0x00166c00u - 0x7634), R());
        for (int i = 0; i < 16; ++i) wf(T4 + 0x10 * i, rf(0.5f, 2));
        for (int k = 0; k < 4; ++k) {
            const uint32_t a = T3 + 0x100 * u(k); const int nfr = ri(1, 8);
            w32(T + 0x48 + 4 * k, a); w8(a + 0x10, u(nfr)); w8(a + 0x12, R()); w32(a + 0x18, ri(0, 1) ? 0u : fbits(rf(0.3f, 1.5f)));
            for (int i = 0; i < nfr; ++i) w32(a + 0x1c + 4 * i, T4 + 0x10 * u(ri(0, 15)));
            for (int e = 0; e < 6; ++e) { w16(a + 0x1c + 4 * nfr + 4 * e, u(ri(0, 9))); w16(a + 0x1e + 4 * nfr + 4 * e, u(ri(0, nfr * 16 + 8))); }
        }
        const int cur = ri(0, 3), next = ri(0, 2) ? cur : ri(0, 3);
        const int nfr = ee_read<uint8_t>(T3 + 0x100 * u(cur) + 0x10);
        w8(M2 + 0x52, u(cur)); w8(M2 + 0x53, u(next));
        const int f0 = ri(0, nfr - 1); w8(M2 + 0x50, u(f0)); w8(M2 + 0x51, u((f0 + 1) % nfr));
        wf(M2 + 0x54, ri(0, 6) ? rf(0, 0.999f) : rf(-0.5f, 1.5f));
        wf(M2 + 0x58, ri(0, 8) ? (coin() ? rf(0.05f, 2.5f) : rf(-2.5f, -0.05f)) : 0.0f);
        wf(M2 + 0x5c, ri(0, 8) ? rf(0.3f, 1.5f) : 0.0f);
        if (ri(0, 6) == 0) { wf(M2 + 0x54, 1.0f - rf(0, 0.003f)); wf(M2 + 0x58, rf(0, 0.001f)); }
        w32(M2 + 0x68, T4 + 0x10 * u(ri(0, 15))); w32(M2 + 0x6c, T4 + 0x10 * u(ri(0, 15)));
        w32(M2 + 0x7c, ri(0, 4) ? (u(ri(0, 7)) | (coin() ? 0xff00u : u(ri(0, 3)) << 8) | u(ri(0, 4)) << 16 | (R() & 0xff000000u)) : (0xffffu | (R() & 0xffff0000u)));
        w8(M2 + 0x7d, ri(0, 2) ? 0xffu : u(ri(0, 7)));
        return Args{{M2}, {}};
    }, [](A a) { moby_anim_advance(a.i[0]); return uint64_t{0}; });


    // ======== batch 37
    g_batch = 37;
    check("region_release", 0x0023edf0, N * 3, Ret::None, [] {
        const uint32_t id = u(ri(0, 7));
        const uint32_t r = OV(0x001804c0) + id * 0x30;
        w32(r + 0xc, T); for (int i = 0; i < 48; ++i) w16(T + 2 * i, u(ri(0, 15)));
        w16(r + 0, u(ri(0, 10))); w16(r + 2, u(ri(0, 12))); w16(r + 8, u(ri(0, 10))); w16(r + 0xa, u(ri(0, 12))); w16(r + 4, u(ri(0, 10))); w16(r + 6, u(ri(0, 12)));
        w32(OV(0x00160fc0), T2); w32(OV(0x00160f4c), T3); w32(OV(0x00160494), T4);
        auto stack = [id] { uint32_t s = 0xffff; for (int k = 0; k < 4; ++k) if (coin()) { const uint32_t n = coin() ? id : u(ri(0, 14)); s = ((s << 4) | n) & 0xffff; } if (ri(0, 4) == 0) s = R() & 0xffff; return s; };
        for (int i = 0; i < 16; ++i) { w16(T2 + 0x20 * i + 0x1e, stack()); w8(T2 + 0x20 * i + 0x1b, 0); w16(T3 + 0x40 * i + 0x36, stack()); w8(T3 + 0x40 * i + 0x35, 0); w16(T4 + 0x20 * i + 0x1e, stack()); w8(T4 + 0x20 * i + 0x1b, 0); }
        return Args{{id}, {}};
    }, [](A a) { region_release(a.i[0]); return uint64_t{0}; });


    check("moby_class_relocate", 0x002406b0, N * 3, Ret::None, [] {
        constexpr uint32_t C = T3;
        for (uint32_t a = C; a < C + 0x1000; a += 4) w32(a, 0);
        wrand(OV(0x00182640) - 0x60, 0x100); wrand(OV(0x001827c0), 16); w32(0x0015ee8c, R() & 0xffff00); wrand(M, 0x200);
        for (int i = 0; i < 0x100; ++i) w8(OUT + i, u(ri(0, 0xfe)));
        const int n4 = ri(0, 2), n5 = ri(0, 1), n6 = ri(0, 1); w8(C + 4, u(n4)); w8(C + 5, u(n5)); w8(C + 6, u(n6));
        const int n = n4 + n5 + n6;
        w32(C + 0, coin() || n ? 0x100u : 0u);
        for (int k = 0; k < n; ++k) {
            const uint32_t e = C + 0x100 + 16 * u(k); const uint32_t q = 4u * u(ri(0, 2)), lo = q + u(ri(0, 2));
            w32(e, 0x400 + 0x100 * u(k)); w32(e + 4, q << 16 | lo); w32(e + 8, R() & 0xfff);
            const uint32_t pk = C + 0x400 + 0x100 * u(k) + (lo - q) * 16;
            for (uint32_t g = 0; g < q / 4; ++g) {
                const uint32_t s = pk + 0x40 * g; w32(s, R() & 0x3fff); w32(s + 4, R() & 0x3f); w32(s + 0x10, R() & 3); w32(s + 0x14, R() & 0xff);
                w32(s + 0x20, ri(0, 3) ? u(ri(0, 15)) : u(ri(-3, -1)));
            }
        }
        for (uint32_t off = 0x10; off <= 0x18; off += 4) w32(C + off, coin() ? R() & 0xfff0 : 0u);
        w32(C + 0x28, coin() ? R() & 0xfff0 : 0u);
        if (coin()) { w32(C + 0x1c, 0x900); const int c = ri(0, 4); w32(C + 0x900, u(c)); for (int i = 0; i < c; ++i) w32(C + 0x904 + 4 * u(i), R() & 0xffff); }
        if (coin()) {
            w32(C + 0x20, 0x980); const int c = ri(1, 4);
            for (int i = 0; i < c; ++i) {
                const uint32_t e = C + 0x980 + 16 * u(i); const int len = ri(0, 8);
                for (int b = 0; b < len; ++b) w8(e + u(b), u(ri(0, 0xfe)));
                w8(e + u(len), 0xff); w32(e + 0xc, i == c - 1 ? 0x90000000u : R() & 0xffff);
            }
        }
        const int sm = ri(0, 3); w8(C + 0xc, u(sm));
        for (int i = 0; i < sm; ++i) {
            if (ri(0, 3) == 0) { w32(C + 0x48 + 4 * u(i), 0); continue; }
            const uint32_t o = 0xa00 + 0x40 * u(i); w32(C + 0x48 + 4 * u(i), o);
            w32(C + o + 0x14, coin() ? R() & 0xffff : 0u); const int c = ri(0, 4); w8(C + o + 0x10, u(c));
            for (int j = 0; j < c; ++j) w32(C + o + 0x1c + 4 * u(j), R() & 0xffff);
        }
        const uint32_t idx = u(ri(0, 7)); w8(OV(0x00197bc0) + idx, u(ri(0, 15)));
        return Args{{C, coin() ? M : 0u, OUT, idx}, {}};
    }, [](A a) { moby_class_relocate(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });

    check("region_height_sample", 0x002a4530, N * 3, Ret::Int32, [] {
        const float ox = rf(-2, 0), oy = rf(-2, 0), gx = rf(-0.5f, 0.5f), gy = rf(-0.5f, 0.5f), cx = rf(0.1f, 0.3f), cy = rf(0.1f, 0.3f);
        wf(OV(0x001ca880) + 8, gx); wf(OV(0x001ca880) + 0xc, gy); wf(OV(0x001ca880) + 0x10, cx); wf(OV(0x001ca880) + 0x14, cy);
        const uint32_t r = T2; wfrand(r, 0x1190 / 4, -1, 1); wf(r, ox - gx); wf(r + 4, oy - gy); wf(r + 8, rf(-5, 5)); w16(r + 0x1e, 0xffff);
        const float x = ox + rf(0.001f, 16 * cx - 0.01f), y = oy + rf(0.001f, 16 * cy - 0.01f), z = rf(-3, 3);
        if (ri(0, 3) == 0) { wf(r, ox - gx - 16 * cx + 0.3f * cx); }   // ix lands on the last column (15)
        w32(OV(0x001612d8), ri(0, 6) ? 1u : 0u); w32(OV(0x001612d4), T); w32(OV(0x001612d0), T2);
        w32(T, u(static_cast<int32_t>(ox * 1024) - 3000)); w32(T + 4, u(static_cast<int32_t>(oy * 1024) - 3000)); w32(T + 8, u(static_cast<int32_t>(z * 1024) - 100));
        w16(T + 0xc, 0xffff); w16(T + 0xe, 0xffff);
        w32(OV(0x00166c00u - 0x59e0), u(ri(0, 2)));
        return Args{{coin() ? OUT : 0u, coin() ? OUT + 0x10 : 0u}, {x, y, z}};
    }, [](A a) { return ri32(region_height_sample(a.i[0], a.i[1], a.f[0], a.f[1], a.f[2])); },
       OUT + 0x1c);   // n.w is the stale VU0 vf3.w left in the register by earlier code (see vec3_cross_ba)


    // ======== batch 38
    g_batch = 38;
    check("region_clear", 0x0023e838, N, Ret::None, [] {
        const uint32_t id = u(ri(0, 7));
        const uint32_t r = OV(0x001804c0) + id * 0x30;
        wrand(OV(0x001802c0) + id * 0x20, 8); wrand(r, 12);
        w32(r + 0xc, T); for (int i = 0; i < 48; ++i) w16(T + 2 * i, u(ri(0, 15)));
        w16(r + 0, u(ri(0, 10))); w16(r + 2, u(ri(0, 12))); w16(r + 8, u(ri(0, 10))); w16(r + 0xa, u(ri(0, 12))); w16(r + 4, u(ri(0, 10))); w16(r + 6, u(ri(0, 12)));
        w32(OV(0x00160fc0), T2); w32(OV(0x00160f4c), T3); w32(OV(0x00160494), T4);
        for (int i = 0; i < 16; ++i) { w16(T2 + 0x20 * i + 0x1e, R()); w16(T3 + 0x40 * i + 0x36, R()); w16(T4 + 0x20 * i + 0x1e, R()); }
        return Args{{id}, {}};
    }, [](A a) { region_clear(a.i[0]); return uint64_t{0}; });
    check("surface_height", 0x00259e58, N * 3, Ret::Float, [] {
        const float ox = rf(-2, 0), oy = rf(-2, 0), cx = rf(0.1f, 0.3f), cy = rf(0.1f, 0.3f);
        wf(OV(0x001ca880) + 8, 0); wf(OV(0x001ca880) + 0xc, 0); wf(OV(0x001ca880) + 0x10, cx); wf(OV(0x001ca880) + 0x14, cy);
        const uint32_t r = T2; wfrand(r, 0x1190 / 4, -1, 1); wf(r, ox); wf(r + 4, oy); wf(r + 8, rf(-5, 5)); w16(r + 0x1e, 0xffff);
        const float x = ox + rf(coin() ? 0.001f : -3.0f, 16 * cx - 0.01f), y = oy + rf(0.001f, 16 * cy - 0.01f), z = rf(-1, 1);
        w32(OV(0x001612d8), ri(0, 2) ? 1u : 0u); w32(OV(0x001612d4), T); w32(OV(0x001612d0), T2);
        w32(T, u(static_cast<int32_t>(ox * 1024))); w32(T + 4, u(static_cast<int32_t>(oy * 1024) - 3000)); w32(T + 8, u(static_cast<int32_t>(z * 1024) - 100));
        w16(T + 0xc, 0xffff); w16(T + 0xe, 0xffff); w32(OV(0x00166c00u - 0x59e0), u(ri(0, 2)));
        w32(OV(0x001612dc), u(ri(0, 1))); wf(OV(0x001612e8), z + rf(-0.7f, 0.7f)); wf(OV(0x001612e0), x + rf(-2, 2)); wf(OV(0x001612e4), y + rf(-2, 2)); wf(OV(0x001612ec), rf(0, 3));
        wf(M, x); wf(M + 4, y); wf(M + 8, z); wf(M + 12, rf(-1, 1));
        return Args{{M, coin() ? OUT : 0u}, {}};
    }, [](A a) { return rf32(surface_height(a.i[0], a.i[1])); }, OUT + 0xc);


    check("spawn_particle_1c", 0x0026de90, N * 2, Ret::None, [particles2] {
        particles2(); w32(OV(0x001b20f0), T); w8(T, R()); wfrand(M, 4, -9, 9);
        Mat4 r; Vec4 e{rf(-3, 3), rf(-3, 3), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(T2) = r; wfrand(T2 + 0x30, 4, -9, 9);
        w32(0x0015ed84, u(ri(1, 3)));
        return Args{{M, coin() ? T2 : 0u}, {rf(0, 0.2f)}};
    }, [](A a) { spawn_particle_1c(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });

    check("remove_moby_5f3", 0x002a9be0, N * 2, Ret::None, [gridenv] {
        const int k = gridenv();
        w32(OV(0x0015ffdc), T4 + 0x100 * u(ri(0, 48))); w32(OV(0x0015f5cc), R()); wrand(OV(0x0015f320), 8);
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        w32(OV(0x001613c8), ri(0, 5) ? m : 0u); w16(m + 0xa6, ri(0, 4) ? 0x5f3u : 3u); w8(m + 0x20, ri(0, 3) ? 0u : 2u);
        w32(m + 0x78, M); const uint32_t id = u(ri(0, 7)); w16(M + 0x2c, coin() ? id : 0xffffu);
        const uint32_t r = OV(0x001804c0) + id * 0x30; wrand(OV(0x001802c0) + id * 0x20, 8); w32(r + 0xc, T);
        w16(r + 0, 0); w16(r + 2, u(ri(0, 4))); w16(r + 8, 0); w16(r + 0xa, u(ri(0, 4))); w16(r + 4, 0); w16(r + 6, u(ri(0, 4)));
        for (int i = 0; i < 8; ++i) w16(T + 2 * i, u(ri(0, 7)));
        w32(OV(0x00160fc0), T2); w32(OV(0x00160f4c), T3); w32(OV(0x00160494), T3 + 0x800);
        for (int i = 0; i < 8; ++i) { w16(T2 + 0x20 * i + 0x1e, R()); w16(T3 + 0x40 * i + 0x36, R()); w16(T3 + 0x800 + 0x20 * i + 0x1e, R()); }
        return Args{};
    }, [](A) { remove_moby_5f3(); return uint64_t{0}; });

    check("hero_head_look", 0x002056a8, N * 3, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        wf(OV(0x0017a680) + 0x278, rf(-1.5f, 1.5f)); wf(OV(0x0017a680) + 0x274, rf(-1.5f, 1.5f));
        w32(P + 0x2080, M); w8(M + 0x53, u(ri(0, 3)));
        seed_rand(); w32(P + 0xff0, ri(0, 2) ? u(ri(0, 11)) : 0u); w32(P + 0xff8, u(ri(1, 50))); w32(P + 0xff4, u(ri(0, 100))); w32(OV(0x0015f5cc), u(ri(0, 100)));
        w32(M + 0x24, T2); w32(T2 + 0x1c, T3);
        for (int i = 0; i < 4; ++i) { w32(T3 + i * 4 + 4, T4 + i * 0x20); w8(T4 + i * 0x20, u(ri(0, 4))); for (int k = 1; k < 12; ++k) w8(T4 + i * 0x20 + k, R()); }
        wrand(OV(0x0017c040), 0x150 / 4); wfrand(OV(0x0017c190), 12, 0, 1);
        for (int i = 0; i < 7; ++i) w32(OV(0x0017c1c0) + 4 * i, u(ri(0, 3)));
        uint32_t prev = 0; w32(M + 0x64, 0);
        for (int i = 0; i < 7; ++i) {
            const uint32_t n = P + 0xd30 + 0x40 * u(i); wrand(n, 16); const bool on = coin();
            w8(n + 1, on ? 1u : 0u); w32(n + 8, 0);
            if (on) { if (prev) w32(prev + 8, n); else w32(M + 0x64, n); prev = n; }
        }
        return Args{};
    }, [](A) { hero_head_look(); return uint64_t{0}; });

    check("hero_body_lean", 0x00215b68, N * 4, Ret::None, [] {
        static const int32_t sts[] = {0x44, 0x4f, 3, 3};
        w32(g::HERO_STATE, u(sts[ri(0, 3)])); w32(g::PLAYER + 0x208c, u(ri(0, 5)));
        wf(g::PLAYER + 0x188, rf(-3, 3)); wf(g::PLAYER + 0x164, rf(-2, 8)); wf(0x0015ed6c, rf(0.5f, 2));
        return Args{};
    }, [](A) { hero_body_lean(); return uint64_t{0}; });

    check("hero_aim_bones", 0x00214fe8, N * 4, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        w16(P + 0x308, ri(0, 5) ? 1u : 0u);
        Mat4 r; Vec4 e{rf(-3, 3), rf(-3, 3), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(P + 0x40) = r; wfrand(P + 0x70, 4, -9, 9);
        static const int32_t sts[] = {0x3f, 0x70, 0x71, 4, 3};
        w32(g::HERO_STATE, u(sts[ri(0, 4)])); w32(P + 0x2fc, u(ri(0, 1))); wf(0x0015ed64, rf(0.5f, 2));
        return Args{};
    }, [](A) { hero_aim_bones(); return uint64_t{0}; });

    check("bone_node_update", 0x00205168, N * 4, Ret::None, [] {
        const uint32_t n = T;
        wrand(n, 0x30);
        w16(n + 0xa2, u(ri(0, 6))); w8(0x001413f4, u(ri(0, 1))); w32(g::HERO_MOBY, M); w32(0x001404d4, M); w32(0x001403e0, ri(0, 3) ? M : 0u);
        const bool rest = ri(0, 2) == 0;
        for (int i = 0; i < 3; ++i) {
            wf(n + 0x40 + 4 * i, rest ? rf(-0.004f, 0.004f) : rf(-3, 3)); wf(n + 0x50 + 4 * i, rf(-1, 1));
            wf(n + 0x60 + 4 * i, rest && ri(0, 5) ? 0.0f : rf(-3, 3)); wf(n + 0x70 + 4 * i, rest ? rf(-0.0025f, 0.0025f) : rf(-1, 1));
            wf(n + 0x80 + 4 * i, rf(-1, 1)); wf(n + 0x90 + 4 * i, rest && ri(0, 5) ? 0.0f : rf(-1, 1));
        }
        wf(n + 0xa4, rf(0, 1)); wf(n + 0xa8, rf(0, 1)); wf(n + 0xac, ri(0, 3) ? 1.0f : rf(0.5f, 2)); w16(n + 0xa0, u(ri(0, 3)));
        w8(0x0015edb5, u(ri(0, 1))); wf(0x0015ed64, rf(0.5f, 2));
        // anim node list of M (0x24f590/0x24f608)
        w8(n + 1, u(ri(0, 1))); w32(n + 8, 0); w32(M + 0x64, ee_read<uint8_t>(n + 1) ? n : 0u);
        w32(M + 0x24, T2); w32(T2 + 0x1c, T3);
        for (int i = 0; i < 4; ++i) { w32(T3 + i * 4 + 4, T4 + i * 0x20); w8(T4 + i * 0x20, u(ri(0, 4))); for (int k = 1; k < 12; ++k) w8(T4 + i * 0x20 + k, R()); }
        return Args{{n}, {}};
    }, [](A a) { bone_node_update(a.i[0]); return uint64_t{0}; });

    check("path_closest_point", 0x0025df68, N * 4, Ret::Int32, [] {
        const int n = ri(0, 6) == 0 ? 0 : ri(1, 6);
        w32(T, u(n));
        for (int i = 0; i < 6; ++i) { wfrand(T + 0x10 + 16 * i, 3, -9, 9); wf(T + 0x1c + 16 * i, rf(0.5f, 3)); }
        wfrand(M, 4, -9, 9); if (n && coin()) { wf(M, ee_read<float>(T + 0x10) + rf(-1, 1)); wf(M + 4, ee_read<float>(T + 0x14) + rf(-1, 1)); wf(M + 8, ee_read<float>(T + 0x18) + rf(-1, 1)); }
        wrand(OUT, 8);
        return Args{{T, M, OUT, OUT + 0x10, OUT + 0x14, u(ri(0, 1))}, {ri(0, 3) ? rf(5, 30) : rf(0, 2), rf(0.3f, 2), coin() ? 0.0f : rf(0.5f, 3)}};
    }, [](A a) { return ri32(path_closest_point(a.i[0], a.i[1], a.i[2], hp<int32_t>(a.i[3]), hp<float>(a.i[4]), static_cast<int32_t>(a.i[5]), a.f[0], a.f[1], a.f[2])); });

    check("bone_nodes_update", 0x002054e8, N, Ret::None, [] {
        w8(0x001413f4, u(ri(0, 1))); w32(g::HERO_MOBY, M); w32(0x001404d4, M); w32(0x001403e0, M);
        w8(0x0015edb5, u(ri(0, 1))); wf(0x0015ed64, rf(0.5f, 2)); w32(M + 0x64, 0);
        w32(M + 0x24, T2); w32(T2 + 0x1c, T3);
        for (int i = 0; i < 4; ++i) { w32(T3 + i * 4 + 4, T4 + i * 0x20); w8(T4 + i * 0x20, u(ri(0, 4))); for (int k = 1; k < 12; ++k) w8(T4 + i * 0x20 + k, R()); }
        uint32_t prev = 0;
        for (int k = 0; k < 31; ++k) {
            const uint32_t n = g::FX_PARAMS + 0xb0 * u(k);
            wrand(n, 0x10); w16(n + 0xa2, u(ri(0, 6))); const bool rest = coin();
            for (int i = 0; i < 3; ++i) {
                wf(n + 0x40 + 4 * i, rest ? 0.0f : rf(-3, 3)); wf(n + 0x50 + 4 * i, rf(-1, 1)); wf(n + 0x60 + 4 * i, rest ? 0.0f : rf(-3, 3));
                wf(n + 0x70 + 4 * i, rest ? 0.0f : rf(-1, 1)); wf(n + 0x80 + 4 * i, rf(-1, 1)); wf(n + 0x90 + 4 * i, rest ? 0.0f : rf(-1, 1));
            }
            wf(n + 0xa4, rf(0, 1)); wf(n + 0xa8, rf(0, 1)); wf(n + 0xac, 1.0f); w16(n + 0xa0, u(ri(0, 3)));
            const bool on = coin(); w8(n + 1, on ? 1u : 0u); w32(n + 8, 0);
            if (on) { if (prev) w32(prev + 8, n); else w32(M + 0x64, n); prev = n; }
        }
        return Args{};
    }, [](A) { bone_nodes_update(); return uint64_t{0}; });
    check("path_push_out", 0x00262030, N * 4, Ret::Int32, [] {
        const uint32_t idx = u(ri(0, 3)); w32(OV(0x001b04b0) + idx * 4, T);
        const int n = ri(1, 6); w32(T, u(n));
        for (int i = 0; i < n; ++i) { wfrand(T + 0x10 + 16 * i, 3, -3, 3); wf(T + 0x1c + 16 * i, ri(0, 3) ? rf(0.5f, 3) : 0.0f); }
        Vec4 ax{0.0f, 0.0f, 1.0f, 0.0f}; if (coin()) { ax = Vec4{rf(-1, 1), rf(-1, 1), rf(-1, 1), 0.0f}; vec3_set_length(ax, ax, 1.0f); }
        ee_ref<Vec4>(0x0013f5e0) = ax;
        wfrand(M, 4, -3, 3); wrand(OUT, 4);
        return Args{{idx, M, OUT}, {rf(0.3f, 3)}};
    }, [](A a) { return ri32(path_push_out(a.i[0], a.i[1], a.i[2], a.f[0])); });

    check("hero_path_push_out", 0x002347c0, N * 2, Ret::None, [] {
        const uint32_t idx = u(ri(0, 3)); w16(g::PLAYER + 0x22da, idx); w32(OV(0x001b04b0) + idx * 4, T);
        const int n = ri(1, 6); w32(T, u(n));
        for (int i = 0; i < n; ++i) { wfrand(T + 0x10 + 16 * i, 3, -3, 3); wf(T + 0x1c + 16 * i, ri(0, 3) ? rf(0.5f, 3) : 0.0f); }
        Vec4 ax{0.0f, 0.0f, 1.0f, 0.0f}; ee_ref<Vec4>(0x0013f5e0) = ax;
        wfrand(g::PLAYER + 0x80, 4, -3, 3); wf(g::PLAYER + 0x234, rf(0.3f, 3)); w16(g::PLAYER + 0x1f6, R());
        return Args{};
    }, [](A) { hero_path_push_out(); return uint64_t{0}; });
    check("find_rail", 0x0020cd08, N * 4, Ret::Int32, [] {
        const int nr = ri(0, 4); w32(OV(0x0015f710), u(nr)); w32(OV(0x0015f70c), T3);
        wfrand(M, 4, -3, 3);
        for (int r = 0; r < 4; ++r) {
            const uint32_t e = T3 + 0x20 * u(r), path = T + 0x100 * u(r);
            wfrand(e, 4, -3, 3); wf(e + 0xc, rf(0, 12)); w32(e + 0x10, path); w32(e + 0x14, u(ri(0, 1)));
            const int n = ri(0, 8) == 0 ? 0 : ri(2, 6); w32(path, u(n));
            for (int i = 0; i < n; ++i) {
                wf(path + 0x10 + 16 * i, ee_read<float>(M) + rf(-2, 2)); wf(path + 0x14 + 16 * i, ee_read<float>(M + 4) + rf(-2, 2));
                wf(path + 0x18 + 16 * i, ee_read<float>(M + 8) + rf(-2, 2)); wf(path + 0x1c + 16 * i, rf(0.5f, 3));
            }
        }
        w32(g::PLAYER + 0x208c, u(ri(0, 3))); w16(g::PLAYER + 0x1ca, u(ri(0, 1)));
        wrand(OUT, 12);
        const uint32_t skip = coin() ? 0u : T + 0x100 * u(ri(0, 3)), only = coin() ? 0u : T + 0x100 * u(ri(0, 3));
        return Args{{M, OUT, OUT + 0x10, OUT + 0x20, OUT + 0x24, OUT + 0x28, skip, only}, {}};
    }, [](A a) { return ri32(find_rail(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6], a.i[7])); });

    check("hero_rot_smooth", 0x0020f160, N * 4, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        Mat4 base; Vec4 e0{rf(-3, 3), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(base, e0);
        for (int k = 0; k < 16; ++k) {
            Mat4 r; Vec4 e{e0.x + rf(-0.1f, 0.1f), e0.y + rf(-0.1f, 0.1f), e0.z + (ri(0, 6) ? rf(-0.1f, 0.1f) : rf(-2, 2)), 0.0f}; euler_to_mat4_fpu(r, e);
            for (int row = 0; row < 3; ++row) ee_ref<Vec4>(P + 0x12f0 + 0x30 * u(k) + 16 * u(row)) = r.r[row];
        }
        w32(P + 0x21b8, u(ri(0, 15))); w32(P + 0x21bc, u(ri(0, 16))); w32(P + 0x21c0, u(ri(0, 16)));
        w32(P + 0x208c, ri(0, 3) ? 0u : 0x16u); w32(g::HERO_STATE, u(ri(1, 3)));
        Mat4 s; Vec4 e{e0.x, e0.y, e0.z + rf(-0.2f, 0.2f), 0.0f}; euler_to_mat4_fpu(s, e); ee_ref<Mat4>(T) = s; wfrand(T + 0x30, 4, -9, 9);
        wrand(M, 0x20);
        return Args{{M, T}, {}};
    }, [](A a) { hero_rot_smooth(a.i[0], a.i[1]); return uint64_t{0}; });

    check("hero_stick_move", 0x002118c8, N * 5, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        const int k = ri(0, 5);
        wf(P + 0x1d20, k == 0 ? 0.0f : rf(-1.2f, 1.2f)); wf(P + 0x1d24, k == 0 ? 0.0f : (k == 1 ? rf(-0.1f, 0.1f) : rf(-1.2f, 1.2f)));
        if (k == 2) wf(P + 0x1d20, rf(-0.1f, 0.1f));
        w8(P + 0x20b3, u(ri(0, 2))); wf(P + 0x98, rf(-3, 3)); wf(OV(0x00166dd8), rf(-3, 3)); w8(0x0015edb4, u(ri(0, 1)));
        Mat4 r; Vec4 e{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(OV(0x00166fd0)) = r;
        euler_to_mat4_fpu(r, Vec4{0.0f, 0.0f, rf(-3, 3), 0.0f}); w32(g::HERO_MOBY, M); ee_ref<Mat4>(M + 0xc0) = r;
        // sub_002332d0 (airborne heading) inputs
        wfrand(P + 0xe0, 4, -2, 2); wfrand(P + 0x270, 4, -1, 1);
        return Args{{u(ri(0, 5))}, {rf(0, 10)}};
    }, [](A a) { hero_stick_move(static_cast<int32_t>(a.i[0]), a.f[0]); return uint64_t{0}; });

    check("hero_smoke_fx", 0x00209ec8, N * 2, Ret::None, [particles2] {
        particles2(); w32(OV(0x001b20e4), T); w8(T, R()); w32(OV(0x001b2090), T + 4); w8(T + 4, R());
        const uint32_t P = g::PLAYER;
        w16(P + 0x49e, u(ri(0, 2))); w16(P + 0x49c, u(ri(0, 2))); w32(P + 0x198, u(ri(0, 60)));
        wfrand(P + 0x80, 4, -9, 9); wf(P + 0x98, rf(-3, 3)); wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed70, rf(0.5f, 2)); w32(0x0015ed84, u(ri(1, 2)));
        return Args{};
    }, [](A) { hero_smoke_fx(); return uint64_t{0}; });

    auto stickenv = [] {
        const uint32_t P = g::PLAYER;
        wf(P + 0x1d20, coin() ? 0.0f : rf(-1.2f, 1.2f)); wf(P + 0x1d24, rf(-1.2f, 1.2f));
        w8(P + 0x20b3, 0); wf(P + 0x98, rf(-3, 3)); wf(OV(0x00166dd8), rf(-3, 3)); w8(0x0015edb4, u(ri(0, 1)));
        Mat4 r; Vec4 e{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(OV(0x00166fd0)) = r;
        euler_to_mat4_fpu(r, Vec4{0.0f, 0.0f, rf(-3, 3), 0.0f}); w32(g::HERO_MOBY, M3); ee_ref<Mat4>(M3 + 0xc0) = r;
        wf(0x0015ed6c, rf(0.5f, 2));
    };
    check("hero_stick_speed", 0x00211be8, N * 3, Ret::None, [stickenv] {
        stickenv(); static const int32_t sts[] = {0x3f, 0x73, 3}; w32(g::HERO_STATE, u(sts[ri(0, 2)]));
        wfrand(OV(0x0017bdb8), 8, 0, 2);
        return Args{};
    }, [](A) { hero_stick_speed(); return uint64_t{0}; });
    check("hero_stick_quadrant", 0x00221df8, N * 2, Ret::Int32, [stickenv] { stickenv(); return Args{}; },
          [](A) { return ri32(hero_stick_quadrant()); });
    check("hero_lock_heading", 0x00214ed8, N * 3, Ret::None, [stickenv] {
        stickenv();
        const uint32_t P = g::PLAYER;
        w32(P + 0xa58, ri(0, 3) ? 0u : 1u); w32(P + 0xa54, coin() ? 0u : M2); wf(P + 0x229c, rf(0, 1));
        static const uint32_t ms[] = {M, M2, T4};
        const int n = ri(0, 3);
        for (int i = 0; i < n; ++i) {
            const uint32_t m = ms[i]; w32(OV(0x001aba00) + 4 * i, m); w16(m + 0x34, ri(0, 3) ? 0x1020u : 0x20u); w32(m + 0x78, T + 0x40 * i);
            w32(T + 0x40 * i, T2 + 0x10 * i); wf(T2 + 0x10 * i, ri(0, 4) ? rf(0.5f, 2) : 0.0f); wfrand(m + 0x10, 4, -9, 9); w16(m + 0xa6, u(ri(480, 560)));
        }
        w32(OV(0x001aba00) + 4 * n, 0);
        w32(g::HERO_STATE, 3); wfrand(0x0013f3d0, 4, -9, 9); wfrand(OV(0x00166c80) + 0x140, 4, -9, 9); wfrand(OV(0x00166c80) + 0x154, 2, -3, 3);
        w32(OV(0x00166c00u - 0x6c28), M); w32(OV(0x00166c00u - 0x6c20), M3 + 0x100);
        return Args{{}, {rf(5, 20), rf(-1, 3), rf(-1, 3)}};
    }, [](A a) { hero_lock_heading(a.f[0], a.f[1], a.f[2]); return uint64_t{0}; });

    check("hero_rail_grab_check", 0x0020d330, N * 4, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        w16(P + 0x1e4, ri(0, 5) ? 0u : 1u); w32(P + 0x208c, u(ri(0, 2) ? 2 : ri(3, 5))); w16(P + 0x41e, u(ri(0, 1)));
        Mat4 r; Vec4 e{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(P) = r; wfrand(P + 0x30, 4, -9, 9);
        wfrand(P + 0x80, 4, -3, 3); wrand(P + 0x5e0, 12); w32(0x0013cae4, R());
        Vec4 hand; sub_0020d2f8_v(hand);
        const int nr = ri(0, 3); w32(OV(0x0015f710), u(nr)); w32(OV(0x0015f70c), T3);
        for (int k = 0; k < 3; ++k) {
            const uint32_t ent = T3 + 0x20 * u(k), path = T + 0x100 * u(k);
            ee_ref<Vec4>(ent) = hand; wf(ent + 0xc, rf(0, 12)); w32(ent + 0x10, path); w32(ent + 0x14, u(ri(0, 1)));
            const int n = ri(2, 5); w32(path, u(n));
            for (int i = 0; i < n; ++i) {
                wf(path + 0x10 + 16 * i, hand.x + rf(-1.5f, 1.5f)); wf(path + 0x14 + 16 * i, hand.y + rf(-1.5f, 1.5f));
                wf(path + 0x18 + 16 * i, hand.z + rf(-1.2f, 0.6f)); wf(path + 0x1c + 16 * i, rf(0.5f, 3));
            }
        }
        w16(P + 0x1ca, u(ri(0, 1)));
        return Args{};
    }, [](A) { hero_rail_grab_check(); return uint64_t{0}; });

    check("hero_rail_land_check", 0x0020cf58, N * 5, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        w8(0x0013d4dd, ri(0, 5) ? 1u : 0u); w16(P + 0x1c8, ri(0, 5) ? 0u : 1u);
        static const int32_t lv[] = {0x10, 0xe, 6, 3, 3, 3};
        const int32_t level = lv[ri(0, 5)]; w32(0x0015ed84, u(level));
        static const float spots[][3] = {{115, 264, 133}, {257, 301, 128}, {157, 265, 51}, {162, 341, 48}, {308, 104, 69}, {177, 304, 143}, {0, 0, 0}};
        const int sp = ri(0, 6);
        wf(P + 0x80, spots[sp][0] + rf(-9, 9)); wf(P + 0x84, spots[sp][1] + rf(-9, 9)); wf(P + 0x88, spots[sp][2] + rf(-5, 5)); wf(P + 0x8c, rf(-1, 1));
        w32(P + 0x208c, u(ri(0, 5))); w32(g::HERO_STATE, u(ri(0, 2) ? 0x29 + ri(0, 1) : 3)); w32(P + 0x570, u(ri(0, 1))); w32(P + 0x5c4, u(ri(0, 1)));
        w16(P + 0x41e, u(ri(0, 1))); w16(P + 0x1ca, u(ri(0, 1))); wrand(P + 0x500, 4);
        const Vec4 pos = ee_ref<Vec4>(P + 0x80);
        w32(OV(0x0015f710), u(ri(0, 3))); w32(OV(0x0015f70c), T3);
        for (int k = 0; k < 3; ++k) {
            const uint32_t ent = T3 + 0x20 * u(k), path = T + 0x100 * u(k);
            ee_ref<Vec4>(ent) = pos; wf(ent + 0xc, rf(0, 12)); w32(ent + 0x10, path); w32(ent + 0x14, u(ri(0, 1)));
            const int n = ri(2, 5); w32(path, u(n));
            for (int i = 0; i < n; ++i) {
                wf(path + 0x10 + 16 * i, pos.x + rf(-1.5f, 1.5f)); wf(path + 0x14 + 16 * i, pos.y + rf(-1.5f, 1.5f));
                wf(path + 0x18 + 16 * i, pos.z + rf(-0.7f, 0.7f)); wf(path + 0x1c + 16 * i, rf(0.5f, 3));
            }
        }
        w32(P + 0x560, coin() ? T : 0u);
        return Args{};
    }, [](A) { hero_rail_land_check(); return uint64_t{0}; });

    check("hero_stick_turn_frames", 0x00221d98, N * 2, Ret::Int32, [stickenv] { stickenv(); w32(0x0015ed84, u(ri(1, 3))); return Args{}; },
          [](A) { return ri32(hero_stick_turn_frames()); });

    check("hero_head_motion", 0x0020a540, N * 5, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        seed_rand();
        w8(0x0015edb3, u(ri(0, 1))); w32(P + 0x1184, u(ri(0, 1))); wf(OV(0x00166c00u - 0x7de8), rf(0, 2)); wf(0x0015ee14, rf(0, 2)); w8(0x0015edb1, u(ri(0, 1)));
        w32(P + 0x260, u(ri(0, 1))); wf(P + 0x258, rf(0, 3)); wf(P + 0x248, rf(0, 2)); wf(P + 0x25c, rf(0, 1)); wf(0x0015ed64, rf(0.5f, 2));
        w32(g::HERO_STATE, ri(0, 4) ? 0u : 3u); w32(P + 0x2080, M); static const uint32_t an[] = {0, 1, 2, 0x3a, 5}; w8(M + 0x53, an[ri(0, 4)]);
        w32(P + 0x2274, ri(0, 4) ? 0u : 1u); w32(P + 0x1010, u(ri(-1, 3))); w32(0x0015ed84, u(ri(1, 2)));
        wf(P + 0x1004, rf(-0.5f, 0.5f)); wf(P + 0x1008, rf(-1, 1)); wf(P + 0x1014, rf(0, 1)); wf(P + 0x1018, rf(0, 1));
        return Args{};
    }, [](A) { hero_head_motion(); return uint64_t{0}; });

    check("clank_glow_blink", 0x002059d8, N * 5, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        seed_rand();
        w8(0x0015edb3, u(ri(0, 1))); wf(0x0015ee18, rf(0, 2));
        w8(P + 0x20a4, u(ri(0, 3))); w32(P + 0x2080, M); w32(P + 0x1184, ri(0, 4) ? M : 0u); w16(P + 0x22d8, ri(0, 3) ? 0u : 1u);
        w16(M + 0x34, R()); w32(P + 0x22a8, u(ri(0, 2))); w32(OV(0x0015f5cc), R() & 0xffff); w16(P + 0x1ee, ri(0, 1) ? 0u : u(ri(1, 50)));
        w32(0x0015ed84, u(ri(1, 2))); w16(P + 0xffc, u(ri(0, 2))); w8(M + 0x53, u(ri(0, 2))); w16(P + 0xffe, ri(0, 2) ? 0u : u(ri(1, 21)));
        wf(M + 0x2c, rf(0.5f, 2)); wfrand(OV(0x0017c1e0), 0xc0 / 4, -2, 2); wfrand(OV(0x0017c2a0), 0x16, 0, 1);
        w32(M + 0x24, T2); w32(T2 + 0x1c, T3);
        for (int i = 0; i < 4; ++i) { w32(T3 + i * 4 + 4, T4 + i * 0x20); w8(T4 + i * 0x20, u(ri(0, 4))); for (int k = 1; k < 12; ++k) w8(T4 + i * 0x20 + k, R()); w32(OV(0x00166c00u - 0x7508) + 4 * u(i), u(ri(0, 3))); }
        uint32_t prev = 0; w32(M + 0x64, 0);
        for (int i = 0; i < 4; ++i) {
            const uint32_t n = P + 0xef0 + 0x40 * u(i); wrand(n, 16); const bool on = coin();
            w8(n + 1, on ? 1u : 0u); w32(n + 8, 0);
            if (on) { if (prev) w32(prev + 8, n); else w32(M + 0x64, n); prev = n; }
        }
        return Args{};
    }, [](A) { clank_glow_blink(); return uint64_t{0}; });

    check("hero_anim_advance", 0x00232670, N * 6, Ret::None, [heroSnd] {
        heroSnd();
        const uint32_t P = g::PLAYER;
        w32(P + 0x2080, M2); w32(OV(0x00166c00u - 0x763c), u(ri(0, 7))); w8(P + 0x20a4, 0); w32(P + 0x11a4, 0);
        for (int i = 0; i < 16; ++i) wf(T4 + 0x10 * i, rf(0.5f, 2));
        for (int k = 0; k < 4; ++k) {
            const uint32_t a = T3 + 0x100 * u(k); const int nfr = ri(1, 8);
            w32(T + 0x48 + 4 * k, a); w8(a + 0x10, u(nfr)); w8(a + 0x12, u(ri(0, 4)));
            for (int i = 0; i < nfr; ++i) w32(a + 0x1c + 4 * i, T4 + 0x10 * u(ri(0, 15)));
            for (int e = 0; e < 6; ++e) w32(a + 0x1c + 4 * nfr + 4 * e, u(ri(0, 9)) | u(ri(0, nfr * 16 + 8)) << 16);
        }
        const int cur = ri(0, 3), next = ri(0, 2) ? cur : ri(0, 3);
        const int nfr = ee_read<uint8_t>(T3 + 0x100 * u(cur) + 0x10);
        w8(M2 + 0x52, u(cur)); w8(M2 + 0x53, u(next));
        const int f0 = ri(0, nfr - 1); w8(M2 + 0x50, u(f0)); w8(M2 + 0x51, u((f0 + 1) % nfr));
        wf(M2 + 0x54, ri(0, 6) ? rf(0, 0.999f) : rf(-0.02f, 1.02f)); w8(M2 + 0x7e, u(ri(0, 1)));
        wf(P + 0xa90, ri(0, 6) ? rf(0.05f, 2.5f) : 0.0f); wf(P + 0xa94, rf(0.3f, 1.5f));
        w32(P + 0xaa0, ri(0, 3) ? 0xffffffffu : u(ri(0, 2))); w32(P + 0xaa4, u(ri(0, 5))); wfrand(OV(0x0017bdf0), 75, -0.5f, 1.8f);
        w32(M2 + 0x68, T4 + 0x10 * u(ri(0, 15))); w32(M2 + 0x6c, T4 + 0x10 * u(ri(0, 15)));
        w32(P + 0xab8, ri(0, 4) ? 0u : 1u); w32(P + 0xab4, u(ri(0, nfr - 1))); w32(P + 0xab0, ri(0, 2) ? 0xffffffffu : u(ri(0, nfr - 1))); wf(OV(0x0015f708), rf(0.3f, 1.5f));
        w8(M2 + 0x7c, ri(0, 3) ? u(ri(0, 7)) : 0xffu); w8(M2 + 0x7d, ri(0, 2) ? 0xffu : u(ri(0, 7)));
        for (int i = 0; i < 8; ++i) { const uint32_t e = 0x0013e550 + i * 0x70; w32(e + 0x88, coin() ? M2 : M); w16(e + 0x7e, u(ri(0, 7))); }
        wf(P + 0xaa8, rf(0, 10)); wrand(P + 0xa98, 2);
        return Args{};
    }, [](A) { hero_anim_advance(); return uint64_t{0}; });

    auto vvenv = [] {
        const uint32_t P = g::PLAYER;
        static const int32_t sts[] = {0xe, 0xe, 0x12, 0x1c, 0x4c, 3, 0x3e};
        w32(g::HERO_STATE, u(sts[ri(0, 6)])); w8(P + 0x20b3, u(ri(0, 2) ? 0 : ri(1, 2)));
        w32(P + 0x2080, M); wfrand(M + 0x40, 4, -1, 1); wfrand(P + 0x270, 4, -1, 1); wfrand(0x0013f5e0, 4, -1, 1); w16(P + 0x30e, u(ri(0, 1)));
        Mat4 r; Vec4 e{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(P) = r; ee_ref<Mat4>(P + 0x40) = r;
        wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed70, rf(0.005f, 0.05f)); w32(0x0015ed84, u(ri(1, 2)));
        w8(0x0013d4c2, u(ri(0, 1))); w16(P + 0x22d8, u(ri(0, 1))); for (int k = 0; k < 4; ++k) { w32(P + 0x10b4 + 0x50 * u(k), u(ri(0, 2))); w32(P + 0x10b8 + 0x50 * u(k), u(ri(2, 4))); }
        wfrand(P + 0x80, 4, -3, 3); wfrand(P + 0x400, 4, -3, 3); wfrand(P + 0xe0, 4, -0.3f, 0.3f); wfrand(P + 0x110, 4, -0.3f, 0.3f);
        w32(P + 0x198, u(ri(0, 30))); w32(P + 0x420, u(ri(0, 30)));
        wf(P + 0x428, ri(0, 2) ? 0.0f : rf(-0.1f, 0.3f)); wf(P + 0x42c, ri(0, 2) ? 0.0f : rf(0, 0.3f)); w32(0x0013cae0, R());
        wf(P + 0x430, rf(0, 2)); wf(P + 0x48c, rf(0, 2)); wf(P + 0x488, rf(0, 1)); w16(P + 0x498, u(ri(1, 10))); wf(P + 0x4a0, rf(0.001f, 0.05f));
        w16(P + 0x41c, u(ri(0, 1))); w32(P + 0x3d0, u(ri(0, 10))); w32(P + 0x3d4, u(ri(5, 40)));
        w32(P + 0x3e4, T); for (int i = 0; i < 12; ++i) { wf(T + 12 * u(i), ri(0, 4) ? rf(-0.3f, 0.3f) : -999999.0f); wf(T + 12 * u(i) + 4, rf(-0.1f, 0.1f)); w32(T + 12 * u(i) + 8, u(ri(1, 4))); }
        w32(P + 0x3e8, u(ri(-1, 3))); w32(P + 0x3ec, u(ri(0, 3))); wf(P + 0x3f0, rf(-0.3f, 0.3f));
    };
    check("hero_vertical_velocity", 0x00214108, N * 6, Ret::None, [vvenv] { vvenv(); return Args{}; }, [](A) { hero_vertical_velocity(); return uint64_t{0}; });
    check("hero_vertical_velocity/jump", 0x00214108, N * 3, Ret::None, [vvenv] {
        vvenv(); const uint32_t P = g::PLAYER;
        w32(g::HERO_STATE, 0xe); w8(P + 0x20b3, 0); w8(0x0013d4c2, 1); w16(P + 0x22d8, 0); w32(P + 0x10b4 + 0xf0, 2); w32(P + 0x10b8 + 0xf0, u(ri(2, 3)));
        wf(P + 0x400 + 8, 0.0f); wf(P + 0x88, rf(0, 4)); w32(P + 0x198, u(ri(0, 6))); w32(P + 0x420, 0); wf(P + 0xe8, rf(-1, 1)); wf(0x0015ed70, rf(0.005f, 0.05f));
        return Args{};
    }, [](A) { hero_vertical_velocity(); return uint64_t{0}; });

    check("hero_horizontal_velocity", 0x00214658, N * 8, Ret::None, [stickenv] {
        stickenv();
        const uint32_t P = g::PLAYER;
        static const int32_t sts[] = {0xa, 0x10, 0x1c, 0x4c, 0xb, 0xc, 0x11, 0x2d, 0xf, 3, 3};
        w32(g::HERO_STATE, u(sts[ri(0, 10)])); w8(P + 0x20b3, u(ri(0, 3) ? 0 : ri(1, 2)));
        w32(P + 0x2080, M3); wfrand(M3 + 0x40, 4, -1, 1);
        Mat4 r; Vec4 e{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(P) = r; ee_ref<Mat4>(P + 0x40) = r;
        wf(0x0015ed64, rf(0.5f, 2)); wf(0x0015ed70, rf(0.005f, 0.05f)); w32(0x0015ed84, u(ri(1, 2)));
        wf(P + 0x930, rf(0, 3)); wf(P + 0x934, rf(-3, 3)); wf(P + 0x938, rf(-3, 3)); wrand(0x0013fc70, 4); wfrand(P + 0x920, 4, -1, 1);
        wfrand(P + 0xe0, 4, -2, 2); wfrand(P + 0x110, 4, -2, 2); wf(P + 0x164, rf(-1, 1)); wf(P + 0x168, rf(-2, 2));
        wf(P + 0x180, rf(-3, 3)); wf(P + 0x184, rf(-0.5f, 0.5f)); wf(P + 0x190, rf(0, 3)); wf(P + 0x194, rf(-1, 1)); wf(P + 0x98, rf(-3, 3));
        w32(P + 0x198, u(ri(0, 80))); wf(P + 0x3f4, rf(0, 3)); wf(P + 0x4a0, rf(0, 0.05f)); wf(P + 0xaa8, rf(0, 35)); wf(P + 0x45c, rf(0, 3));
        wf(P + 0x438, rf(-3, 3)); wf(P + 0x43c, rf(-3, 3)); w32(P + 0x450, u(ri(2, 3))); wf(P + 0x454, rf(-1, 1)); wf(P + 0x458, rf(0, 3)); w16(P + 0x41e, u(ri(0, 1))); w32(P + 0x44c, u(ri(0, 20)));
        wf(P + 0x47c, rf(-3, 3)); wfrand(P + 0x470, 4, -1, 1); w16(P + 0x308, u(ri(0, 1))); wf(P + 0x414, rf(0, 5));
        wf(OV(0x0017bdd0), rf(0, 3)); w32(P + 0x208c, u(ri(0, 4))); wf(P + 0x944, rf(0, 0.2f)); w8(P + 0x12e2, u(ri(0, 1))); w16(P + 0x30a, u(ri(0, 1)));
        w16(P + 0x4a8, u(ri(0, 1))); wf(P + 0x2e0, rf(0, 0.6f)); w16(P + 0x4a4, u(ri(0, 1))); w8(P + 0x257, u(ri(0, 1)));
        wfrand(0x0013f4c0, 4, -3, 3); wf(0x0013f4d4, rf(-0.5f, 0.5f)); wfrand(0x0013f3e0, 4, -3, 3); wfrand(0x0013f430, 4, -2, 2);
        return Args{};
    }, [](A) { hero_horizontal_velocity(); return uint64_t{0}; });

    check("moby_update_transform", 0x00250df8, N * 4, Ret::None, [gridenv] {
        const int k = gridenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k));
        w8(m + 0x20, ri(0, 6) ? u(ri(0, 3)) : 0xfeu); w16(m + 0x34, (ri(0, 3) ? 0u : 0x100u) | (ri(0, 3) ? 0u : 0x8000u) | (R() & 0x00ffu));
        w32(m + 0x24, T); wf(m + 0x2c, rf(0.5f, 2)); wfrand(m + 0x40, 4, -3, 3); wfrand(m + 0xc0, 12, -1, 1);
        for (int a = 0; a < 4; ++a) { const uint32_t h = T2 + 0x40 * u(a); w32(T + 0x48 + 4 * u(a), h); wfrand(h, 3, -1, 1); wf(h + 12, rf(500, 30000)); }
        const int cur = ri(0, 4) ? ri(0, 3) : 0xff, nxt = ri(0, 2) ? cur : ri(0, 3);
        w8(m + 0x52, u(cur)); w8(m + 0x53, u(nxt == 0xff ? 0 : nxt)); w8(m + 0x50, u(ri(0, 7))); wf(m + 0x54, rf(0, 1)); w8(m + 0x71, u(ri(0, 3)));
        wfrand(OV(0x00196d00), 32, -1, 1); for (int i = 0; i < 8; ++i) wf(OV(0x00196d00) + 16 * u(i) + 12, rf(500, 30000));
        wfrand(m + 0xf0, 3, -1, 1); wf(m + 0xfc, rf(500, 30000));
        wf(m + 0x10, rf(0, 80)); wf(m + 0x14, rf(0, 80)); wf(m + 0x18, rf(-50, 50)); wf(m + 0x1c, rf(-1, 1));
        w16(m + 0xa8, R()); w32(m + 0x94, u(ri(0, 3)));
        return Args{{m}, {}};
    }, [](A a) { moby_update_transform(a.i[0]); return uint64_t{0}; });

    auto hmenv = [gridenv] {
        const int k = gridenv();
        const uint32_t P = g::PLAYER, hm = T4 + 0x100 * u(ri(0, k));
        static const uint32_t ms[] = {0, M, M2, M3};
        w32(P + 0xa88, M3);
        for (int i = 0; i < 7; ++i) { w32(P + i * 0x50 + 0x1090, ms[ri(0, 3)]); w32(P + i * 0x50 + 0x1094, ms[ri(0, 3)]); }
        w32(P + 0x118c, ms[ri(0, 3)]); w8(P + 0x20a4, u(ri(0, 2))); w32(P + 0x1624, ms[ri(0, 3)]); w32(P + 0x1620, ms[ri(0, 3)]);
        w32(P + 0x10b8, ri(0, 2) ? 8u : 3u); w32(OV(0x0015f5c4), ri(0, 3) ? 0u : 1u); w8(P + 0x20a5, u(ri(0, 1)));
        for (uint32_t m : {M, M2, M3}) { w16(m + 0x34, R()); w8(m + 0x20, ri(0, 3) ? 1u : 0u); }
        w32(P + 0x2080, hm); w8(hm + 0x20, u(ri(0, 3))); w16(hm + 0x34, R() & 0x7eff);
        w32(hm + 0x24, T); wf(hm + 0x2c, rf(0.5f, 2));
        for (int a = 0; a < 4; ++a) { const uint32_t h = T2 + 0x40 * u(a); w32(T + 0x48 + 4 * u(a), h); wfrand(h, 3, -1, 1); wf(h + 12, rf(500, 30000)); }
        w8(hm + 0x52, u(ri(0, 3))); w8(hm + 0x53, ee_read<uint8_t>(hm + 0x52)); w8(hm + 0x71, u(ri(0, 3))); wfrand(hm + 0xf0, 4, -1, 1);
        wf(P + 0x80, rf(0, 80)); wf(P + 0x84, rf(0, 80)); wf(P + 0x88, rf(-50, 50)); wf(P + 0x8c, rf(-1, 1)); wfrand(P + 0x90, 4, -3, 3);
        Mat4 r; Vec4 e{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(P) = r;
        w32(P + 0x1cc, ri(0, 3) ? 0u : 1u); w32(hm + 0x94, ri(0, 3) ? OUT : 0u); wrand(OUT, 24);
        wf(P + 0x224, rf(0, 2)); wf(P + 0x234, rf(0, 2)); wf(P + 0x220, rf(0, 3)); w8(P + 0x20b3, 0);
    };
    check("hero_moby_sync", 0x00208a10, N * 3, Ret::None, [hmenv] { hmenv(); return Args{}; }, [](A) { hero_moby_sync(); return uint64_t{0}; });
    check("hero_moby_update", 0x00208820, N * 3, Ret::None, [hmenv] {
        hmenv(); const uint32_t P = g::PLAYER;
        static const int32_t sts[] = {0x7f, 0x6b, 0x6a, 3}; w32(g::HERO_STATE, u(sts[ri(0, 3)])); w32(P + 0x208c, u(ri(3, 4)));
        w16(P + 0x8bc, u(ri(0, 1))); w32(P + 0x86c, 0); wf(P + 0x6a8, rf(-1, 1)); wf(P + 0x6ac, rf(-1, 1)); wf(P + 0x6b0, rf(-1, 1)); wf(P + 0x6b4, rf(-1, 1));
        w16(P + 0x22d8, u(ri(0, 1))); w32(P + 0x1180, 0); w32(P + 0x1184, 0); w8(0x0015edb5, u(ri(0, 1))); w16(ee_read<uint32_t>(P + 0x2080) + 0xa6, u(ri(0, 1)));
        return Args{};
    }, [](A) { hero_moby_update(); return uint64_t{0}; });

    check("trail_update", 0x00262608, N * 3, Ret::None, [gridenv] {
        const int k = gridenv();
        constexpr uint32_t O = T3;
        w32(OV(0x0015ffdc), T4 + 0x100 * u(ri(0, 48))); w32(OV(0x0015f5cc), R());
        w32(O + 0x13c, ri(0, 5) ? 1u : 0u); w32(O + 0x134, ri(0, 6) ? M : 0u); w8(M + 0x20, ri(0, 6) ? 1u : 0xfdu);
        w8(M + 0x52, u(ri(0, 3))); w8(M + 0x53, u(ri(0, 3))); w8(M + 0x50, u(ri(0, 3))); w8(M + 0x51, u(ri(0, 3))); wf(M + 0x54, rf(0, 1));
        wfrand(M + 0x10, 4, 0, 60); wfrand(M + 0x40, 4, -3, 3); wfrand(O, 0x40, -3, 3);
        w16(O + 0x130, u(ri(0, 7))); w16(O + 0x132, u(ri(0, 8)));
        const int n = ri(0, 4) < k ? ri(0, 4) : 0; w32(O + 0x138, u(n));
        for (int i = 0; i < 4; ++i) {
            const uint32_t c = T4 + 0x100 * u(i);
            w32(O + 0x120 + 4 * u(i), i < n ? c : 0u); w32(O + 0x110 + 4 * u(i), u(ri(0, 9)));
            w8(c + 0x20, 1); w16(c + 0x34, R() & 0x7eff); w32(c + 0x24, T); wf(c + 0x2c, rf(0.5f, 2)); w32(c + 0x94, 0); w8(c + 0x23, R());
        }
        for (int a = 0; a < 4; ++a) {
            const uint32_t h = T2 + 0x80 * u(a); w32(T + 0x48 + 4 * u(a), h); wfrand(h, 3, -1, 1); wf(h + 12, rf(500, 30000));
            w8(h + 0x11, R()); w8(h + 0x12, R()); for (int f = 0; f < 4; ++f) w32(h + 0x1c + 4 * u(f), R());
        }
        return Args{{O, u(ri(0, 2) ? 0 : ri(1, 300))}, {}};
    }, [](A a) { trail_update(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });

    auto spawnenv = [mobyenv] {
        mobyenv();
        w8(OV(0x00197bc0) + 0x1df, u(ri(0, 3))); w8(OV(0x00197bc0) + 0x307, u(ri(0, 3)));
        for (int i = 0; i < 4; ++i) {
            const uint32_t c = T3 + 0x100 * u(i);
            for (int a = 0; a < 4; ++a) { const uint32_t h = T2 + 0x40 * u(a); w32(c + 0x48 + 4 * u(a), h); wfrand(h, 3, -1, 1); wf(h + 12, rf(500, 30000)); }
        }
        const uint32_t P = g::PLAYER; w32(P + 0x2080, M3); wrand(M3 + 0x90, 4);
        wfrand(P + 0x80, 4, -9, 9); w8(P + 0x1ff6, R()); for (int i = 0; i < 6; ++i) w32(P + 0x2020 + 4 * u(i), ri(0, 1) ? 0u : M);
        wfrand(M, 4, -9, 9);
    };
    check("spawn_moby_1df", 0x002ce1f0, N * 2, Ret::Int32, [spawnenv] { spawnenv(); return Args{{R(), M}, {}}; },
          [](A a) { return r32(spawn_moby_1df(a.i[0], a.i[1])); });
    check("spawn_moby_307", 0x002d7e90, N * 2, Ret::Int32, [spawnenv] { spawnenv(); return Args{{M}, {rf(0.5f, 2)}}; },
          [](A a) { return r32(spawn_moby_307(a.i[0], a.f[0])); });

    check("use_agents", 0x002cdcc0, N * 2, Ret::None, [spawnenv] {
        spawnenv(); const uint32_t P = g::PLAYER;
        w16(OV(0x001c40b0) + 0x18 * 0x18 + 0xe, u(ri(0, 3) ? 1 : 0)); w32(0x0013d428 + 0x18 * 4, u(ri(0, 3))); w32(0x0013dea0 + 0x18 * 4, u(ri(0, 9)));
        w8(P + 0x1ff6, u(ri(0, 7))); wf(P + 0x98, rf(-3, 3)); w32(P + 0x2080, M3); wfrand(M3 + 0xc0, 12, -1, 1);
        w16(0x00141680 + 0xc0, ri(0, 3) ? R() & 0xffff : 0xffffu); w16(0x00141680 + 0xc2, u(ri(0, 3))); w32(0x00141680 + 0xc4, R());
        w32(0x0015eea4, u(ri(0, 3000))); w32(0x0015ed84, u(ri(0, 20)));
        return Args{};
    }, [](A) { use_agents(0); return uint64_t{0}; });

    check("hero_land_fx", 0x00209ca8, N * 2, Ret::None, [spawnenv, particles2] {
        spawnenv(); particles2(); w32(OV(0x001b2134), T4 + 0x800); w8(T4 + 0x800, R()); w32(OV(0x001b210c), T4 + 0x804); w8(T4 + 0x804, R());
        const uint32_t P = g::PLAYER; wfrand(P + 0x80, 4, -9, 9); wf(P + 0x2f0, rf(-9, 9)); wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed70, rf(0.005f, 0.05f));
        return Args{{u(ri(0, 4)), u(ri(0, 4)), u(ri(0, 1))}, {}};
    }, [](A a) { hero_land_fx(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2])); return uint64_t{0}; });

    check("hero_run_lean", 0x00215340, N * 5, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        static const int32_t sts[] = {2, 2, 8, 0x81, 0x81, 3};
        w32(g::HERO_STATE, u(sts[ri(0, 5)])); w32(P + 0x2088, u(ri(0, 2))); w32(P + 0x208c, u(ri(1, 5)));
        wf(P + 0x188, rf(-3, 3)); wf(P + 0x164, rf(0, 8)); wf(0x0015ed6c, rf(0.5f, 2)); wf(P + 0x100, rf(-2, 2)); wf(P + 0x104, rf(-2, 2)); wf(P + 0x98, rf(-3, 3));
        return Args{};
    }, [](A) { hero_run_lean(); return uint64_t{0}; });

    check("hero_platform_ride", 0x00233f80, N * 6, Ret::None, [] {
        const uint32_t P = g::PLAYER;
        w16(P + 0x30e, ri(0, 2) ? 0u : u(ri(1, 50))); w32(P + 0x2fc, ri(0, 3) ? M : 0u); w32(P + 0x360, ri(0, 2) ? M : 0u); w32(P + 0x364, u(ri(0, 3)));
        static const int32_t sts[] = {0x1c, 0x18, 0x19, 1, 0x1e, 3}; w32(g::HERO_STATE, u(sts[ri(0, 5)])); w32(P + 0x208c, u(ri(0, 2) ? ri(0, 3) : 0xc)); w32(P + 0x4f8, ri(0, 1) ? M : 0u);
        w32(P + 0x2080, M3); w16(M + 0x34, ri(0, 5) ? 0x20u : 0u); w32(M + 0x78, T); w32(T + 8, ri(0, 6) ? M2 : 0u); w32(M2 + 0x3c, u(ri(0, 7)));
        wfrand(M2, 4, -1, 1); wfrand(M2 + 0x10, 4, -1, 1); w8(M2 + 0x22, u(ri(0, 3))); w8(M + 0x22, u(ri(0, 3))); w8(M3 + 0x22, u(ri(0, 3)));
        wfrand(M + 0x10, 4, -9, 9); { Mat4 r; euler_to_mat4_fpu(r, Vec4{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}); ee_ref<Mat4>(M + 0xc0) = r; }
        wfrand(P + 0x80, 4, -9, 9); wfrand(P + 0x90, 4, -2, 2); wf(P + 0x98, rf(-3, 3)); wfrand(P + 0x150, 4, -0.1f, 0.1f); if (coin()) wfrand(P + 0x150, 4, 0, 0);
        w16(P + 0x1f6, u(ri(0, 1))); wf(P + 0x184, ri(0, 1) ? 0.0f : rf(-1, 1)); wfrand(P + 0x310, 12, -9, 9); wfrand(P + 0x350, 4, -100, 100);
        wfrand(P + 0x4d0, 4, -9, 9); wf(P + 0x4e4, rf(-3, 3)); w8(P + 0x20a5, u(ri(0, 1))); wfrand(P + 0x140, 4, -1, 1); wfrand(P + 0xf0, 4, -1, 1);
        wf(0x0015ed70, rf(0.005f, 0.05f));
        return Args{};
    }, [](A) { hero_platform_ride(); return uint64_t{0}; });


    // ======== batch 39
    g_batch = 39;
    check("map_side", 0x0024abf8, N * 3, Ret::Int32, [] {
        auto v = [] { return ri(0, 3) ? u(ri(-600, 600)) : R(); };
        return Args{{v(), v(), v(), v(), v(), v()}, {}};
    }, [](A a) { auto s = [&](int k) { return static_cast<int32_t>(a.i[k]); }; return ri32(map_side(s(0), s(1), s(2), s(3), s(4), s(5))); });

    {   // every per-level reveal condition in the table 0x183f90 (19 levels x 8), called directly
        std::vector<uint32_t> fns;
        for (uint32_t i = 0; i < 19 * 8; ++i) { const uint32_t f = ee_read<uint32_t>(OV(0x00183f90) + 4 * i); if (f != 0 && std::find(fns.begin(), fns.end(), f) == fns.end()) fns.push_back(f); }
        for (const uint32_t lf : fns) {
            const uint32_t fn = ovl_code_to_veldin(lf);  // (the table holds the level's code addresses; check() takes Veldin's)
            if (fn == 0) { std::printf("map_condition/%06x: level-only code, skipped\n", lf); ++g_skipped; continue; }
            static char names[64][24]; static int nn = 0; std::snprintf(names[nn], 24, "map_condition/%06x", fn);
            check(names[nn++], fn, N, Ret::Int32, [] {
                const uint32_t P = g::PLAYER;
                for (uint32_t i = 0; i < 8; ++i) w32(OV(0x001844a8) + 4 * i, ri(0, 2) ? 1u : 0u);
                for (uint32_t a = 0x0013d390; a < 0x0013d410; ++a) w8(a, u(ri(0, 1)));
                w32(OV(0x00166c00u - 0x6e04), u(ri(0, 1)));
                static const int32_t subs[] = {0xf, 0x10, 0x11, 0x12, 3}; w32(P + 0x208c, u(subs[ri(0, 4)])); w8(P + 0x12e4, u(ri(0, 3) ? 0 : 1));
                return Args{{u(ri(0, 511)), u(ri(0, 511))}, {rf(0, 512), rf(0, 512), rf(0, 250)}};
            }, [lf](A a) { return ri32(map_condition(lf, static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), a.f[0], a.f[1], a.f[2])); });
        }
    }

    check("map_reveal", 0x002484e0, N * 6, Ret::None, [] {
        static const uint32_t C = OV(0x001841f0), FOG = 0x01e80000, OFS = 0x01e88000, DAT = 0x01e89000, BUF = 0x01e8c000;
        const uint32_t P = g::PLAYER;
        w32(OV(0x00184218), ri(0, 9) ? 1u : 0u); const bool alt = ri(0, 4) == 0; w32(OV(0x00166c00u - 0x6e08), alt ? 1u : 0u);
        for (uint32_t i = 0; i < 0x13; ++i) { wf(OV(0x00182a70) + 16 * i, 0.0f); wf(OV(0x00182a74) + 16 * i, 1.0f); wf(OV(0x00182a78) + 16 * i, 0.0f); wf(OV(0x00182a7c) + 16 * i, 1.0f); }
        const int32_t level = ri(0, 18); w32(0x0015ed84, u(level));
        const int32_t tx = ri(-20, 530), ty = ri(-20, 530); const float fx = rf(0, 0.99f), fy = rf(0, 0.99f);
        if (alt && level == 6) { wf(OUT, 740.0f - static_cast<float>(ty) - fy); wf(OUT + 4, static_cast<float>(tx) + fx - 1053.0f); }
        else { wf(OUT, static_cast<float>(tx) + fx); wf(OUT + 4, static_cast<float>(ty) + fy); }
        const float hz = rf(0, 250); wf(OUT + 8, ri(0, 2) ? hz : rf(0, 250)); w32(P + 0x2080, M); wf(M + 0x18, hz);
        for (uint32_t r = 0; r < 16; ++r) {
            const uint32_t E = OV(0x00182ba0) + u(level) * 256 + r * 16;
            if (coin()) { const float h = rf(0, 200); wf(E, h); wf(E + 4, ri(0, 4) ? h : h + 1.0f); }
            else { const float lo = rf(0, 200); wf(E, lo); wf(E + 4, lo + rf(0, 80)); }
            uint32_t f = 0; if (ri(0, 2)) for (int b = 0; b < 16; ++b) if (ri(0, 4) == 0) f |= 1u << b;
            w32(E + 8, f); w32(E + 0xc, u(ri(0, 7)));
        }
        for (uint32_t i = 0; i < 8; ++i) w32(OV(0x001844a8) + 4 * i, ri(0, 2) ? 1u : 0u);
        for (uint32_t a = 0x0013d390; a < 0x0013d410; ++a) w8(a, u(ri(0, 1)));
        w32(OV(0x00166c00u - 0x6e04), u(ri(0, 1)));
        static const int32_t subs[] = {0xf, 0x10, 0x11, 0x12, 3}; w32(P + 0x208c, u(subs[ri(0, 4)]));
        w8(P + 0x12e4, u(ri(0, 3) ? 0 : 1)); w16(P + 0x308, u(ri(0, 1)));
        w32(C + 0xc, FOG); wrand(FOG, 0x2000); for (int k = 0; k < 64; ++k) w8(FOG + (R() & 0x7fff), 0);
        wrand(C + 0x1a4, 32);
        w32(C + 0x28, ri(0, 9) ? 1u : 0u); w32(C, DAT); w32(C + 4, OFS);
        int16_t o = 0x200;
        for (int i = 0; i < 256; ++i) { o = static_cast<int16_t>(o + ri(0, 4) * 3); w16(OFS + 2 * i, static_cast<uint16_t>(o)); }
        wrand(DAT, 0x400);
        const int32_t kx = (tx < 0 ? 0 : tx) >> 5, ky = (ty < 0 ? 0 : ty) >> 5;
        for (uint32_t i = 0; i < 8; ++i) {
            const uint32_t key = ri(0, 3) ? u(kx + ri(-1, 1) + (ky + ri(-1, 1)) * 16) : u(ri(0, 255));
            w32(C + 0x30 + 16 * i, key); w32(C + 0x34 + 16 * i, u(ri(0, 100))); w32(C + 0x38 + 16 * i, BUF + 0x400 * i); w32(C + 0x3c + 16 * i, R());
        }
        wrand(BUF, 0x900); w32(OV(0x0015f3f8), R());
        return Args{{OUT}, {}};
    }, [](A a) { map_reveal(a.i[0]); return uint64_t{0}; });

    check("region_height_splash", 0x002a3ec8, N * 6, Ret::None, [] {
        static const uint32_t RG = 0x01e80000, S = OV(0x001ca880);
        const int n = ri(0, 3);
        wf(S + 8, rf(-50, 50)); wf(S + 0xc, rf(-50, 50)); const float dx = rf(0.5f, 4), dy = rf(0.5f, 4); wf(S + 0x10, dx); wf(S + 0x14, dy);
        w32(OV(0x00166c00u - 0x59e8), u(ri(0, 2)));
        for (int i = 0; i < 4; ++i) {
            const uint32_t p = RG + 0x1190u * u(i);
            wfrand(p + 0x50, 0x1140 / 4, -3, 3); wf(p, rf(-100, 100)); wf(p + 4, rf(-100, 100));
            for (uint32_t k = 0xc; k < 0x14; ++k) w8(p + k, ri(0, 2) ? u(ri(0, 3)) : 0xffu);
        }
        const uint32_t t = RG + 0x1190u * u(ri(0, 3));
        const float x = ee_read<float>(t) + ee_read<float>(S + 8) + dx * rf(-4, 20), y = ee_read<float>(t + 4) + ee_read<float>(S + 0xc) + dy * rf(-4, 20);
        return Args{{RG, u(n), u(ri(0, 1))}, {x, y, ri(0, 3) ? rf(0, 5) : rf(0, 20), rf(-5, 5)}};
    }, [](A a) { region_height_splash(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]), a.f[0], a.f[1], a.f[2], a.f[3]); return uint64_t{0}; });

    {
        // (stage 5, task C2a) the port keeps its chunk list and vertices on the stack, the original at 0x70000000 + 4 i and
        // 0x70002000 + 16 i: those scratch words are left out of this check (A2's ranges; the function calls nothing).
        const CollisionScratchIgnored spr_ignored;
        // Chunks (16 bytes from CM + 0x10: u16 centre x, y, z, radius, u8 triangles, u16 vertices, the vertices' address;
        // vertices u16 x, y, z, w, then triangles u8 a, b, c, flags) around the query. Besides the plain cases: a chunk with
        // 1 or 2 vertices (the pair loop's single pass), one with more than 256 (the indices past 255 are written but never
        // read: the stack's buffer drops them), many triangles; and one case in ten of 1018 .. 1030 chunks over the query
        // (their vertices in 8 blocks, chunk i using block i % 8): the list's end wraps at 1024 entries (& 0xfff), so 1024 + j
        // chunks test only the first j (none for exactly 1024), and a walk of all of them would meet other blocks' triangles.
        check("sphere_mesh_collide", 0x001f3170, N * 8, Ret::Int32, [] {
            constexpr uint32_t CM = 0x01e80000;
            if (ri(0, 15) == 0) { w32(OV(0x00166c00u - 0x75e8), 0); return Args{{OUT}, {1.0f}}; }
            w32(OV(0x00166c00u - 0x75e8), CM);
            auto tris = [](uint32_t td, int nt, int nv) {
                const int top = std::min(nv, 256) - 1;
                for (int t = 0; t < nt; ++t) { w8(td + 4 * t, u(ri(0, top))); w8(td + 4 * t + 1, u(ri(0, top))); w8(td + 4 * t + 2, u(ri(0, top))); w8(td + 4 * t + 3, R()); }
            };
            auto verts = [](uint32_t vd, int nv, int32_t x, int32_t y, int32_t z, int32_t rr) {
                for (int v = 0; v < nv + 1; ++v) { w16(vd + 8 * v, u(x + ri(-rr, rr))); w16(vd + 8 * v + 2, u(y + ri(-rr, rr))); w16(vd + 8 * v + 4, u(z + ri(-rr, rr))); w16(vd + 8 * v + 6, R()); }
            };
            const float s = 1.0f / 64.0f;
            if (ri(0, 9) == 0) {  // the list's wrap
                const int ne = ri(0, 2) == 0 ? ri(1018, 1023) : 1024 + ri(0, 6);
                w32(CM, u(ne));
                const int32_t cx = ri(6000, 7000), cy = ri(6000, 7000), cz = ri(6000, 7000);
                uint32_t blk[8]; int bnv[8], bnt[8];
                for (int b = 0; b < 8; ++b) {
                    blk[b] = CM + 0x8000 + 0x400 * u(b); bnv[b] = ri(3, 14); bnt[b] = ri(1, 12);
                    verts(blk[b], bnv[b], cx, cy, cz, 300);
                    tris(blk[b] + 8 * u(bnv[b]), bnt[b], bnv[b]);
                }
                for (int i = 0; i < ne; ++i) {
                    const uint32_t e = CM + 0x10 + 0x10 * u(i);
                    const int b = i % 8;
                    w16(e, u(cx + ri(-50, 50))); w16(e + 2, u(cy + ri(-50, 50))); w16(e + 4, u(cz + ri(-50, 50))); w16(e + 6, 800);
                    w8(e + 8, u(bnt[b])); w16(e + 0xa, u(bnv[b])); w32(e + 0xc, blk[b]);
                }
                wf(OUT, cx * s + rf(-2, 2)); wf(OUT + 4, cy * s + rf(-2, 2)); wf(OUT + 8, cz * s + rf(-2, 2)); wf(OUT + 12, rf(-9, 9));
                return Args{{OUT}, {rf(0.2f, 5)}};
            }
            const int ne = ri(0, 4); w32(CM, u(ne));
            int32_t cx[5], cy[5], cz[5];
            for (int i = 0; i < ne; ++i) {
                const uint32_t e = CM + 0x10 + 0x10 * u(i), vd = CM + 0x2000 + 0x1000 * u(i);
                cx[i] = ri(6000, 7000); cy[i] = ri(6000, 7000); cz[i] = ri(6000, 7000); const int32_t RR = ri(64, 400);
                w16(e, u(cx[i])); w16(e + 2, u(cy[i])); w16(e + 4, u(cz[i])); w16(e + 6, u(RR));
                const int pick = ri(0, 7);
                const int nv = pick == 0 ? ri(257, 340) : pick == 1 ? ri(1, 2) : ri(3, 14), nt = ri(0, 3) == 0 ? ri(12, 40) : ri(0, 12);
                w8(e + 8, u(nt)); w16(e + 0xa, u(nv)); w32(e + 0xc, vd);
                verts(vd, nv, cx[i], cy[i], cz[i], RR);
                tris(vd + 8 * u(nv), nt, nv);
            }
            const int k = ne > 0 ? ri(0, ne - 1) : 0;
            wf(OUT, (ne ? cx[k] : 6500) * s + rf(-4, 4)); wf(OUT + 4, (ne ? cy[k] : 6500) * s + rf(-4, 4)); wf(OUT + 8, (ne ? cz[k] : 6500) * s + rf(-4, 4)); wf(OUT + 12, rf(-9, 9));
            return Args{{OUT}, {rf(0.2f, 5)}};
        }, [](A a) { return ri32(sphere_mesh_collide(a.i[0], a.f[0])); });
    }

    check("moby_pose_blend", 0x00252990, N * 6, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, DST = 0x01e90000;
        const int n = ri(1, 24), bq = ri(n, 40);
        std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
        for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
        w16(M + 2, u(n)); w32(M + 0xc, T); for (int k = 0; k < n; ++k) w8(T + u(k), u(js[static_cast<size_t>(k)]));
        w32(M + 0x14, M2); w8(M2 + 8, u(bq)); w32(M2 + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1);
        w32(M + 0x10, DST); w32(M + 0x24, ri(0, 3) == 0 ? 0u : [] { const float f = rf(0, 1); uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x20 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, 8), np = ri(0, 8), nq = (bq + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 4, R() | 1);
            for (int r = 0; r < nr + np; ++r) w8(F + 0x10 + u(bq + r) * 8 + 6, ri(0, 7) ? u(js[static_cast<size_t>(ri(0, n - 1))]) : u(ri(0, 255)));
        }
        w32(M + 0x38, FA); w32(M + 0x3c, ri(0, 3) ? FB : FA);
        return Args{{M}, {}};
    }, [](A a) { moby_pose_blend(a.i[0]); return uint64_t{0}; });

    check("moby_pose_matrices", 0x002531e0, N * 8, Ret::A3, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000, DST = 0x01e90000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24), kq = n + ri(0, 3);
        auto jt = [n] { return S + u(ri(0, 9) ? ri(0, n - 1) : ri(n, n + 3)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(kq)); w32(M2 + 0x18, BIND);
        wrand(S + 0x3000 + u(n) * 16, 0x40);  // stale scratchpad past the bind pose
        for (int k = 0; k < n + 4; ++k) { wfrand(BIND + 16 * u(k), 3, -9, 9); w32(BIND + 16 * u(k) + 12, S + u(ri(0, k)) * 64); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = rf(0, 1); uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, 8), np = ri(0, 8), nq = (kq + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);  // the DMA-done sentinel must be nonzero
            // joints are unique within a frame's rotation list and within its translation list
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const int k = r < nr ? r : r - nr; const std::vector<int>& v = r < nr ? pr : pp;
                w8(F + 0x10 + u(kq + r) * 8 + 6, k < n && ri(0, 12) ? u(v[static_cast<size_t>(k)]) : u(ri(n, 255)));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 1)) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 8);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 1)) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        static const int32_t outs[] = {0, -1, static_cast<int32_t>(DST)};
        const uint32_t oo = u(outs[ri(0, 2)]);
        return Args{{M, u(n), oo}, {}};
    }, [](A a) { return uint64_t{moby_pose_matrices(a.i[0], static_cast<int32_t>(a.i[1]), static_cast<int32_t>(a.i[2]))}; });

    check("moby_pose_capture", 0x00251ce8, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, R() | 1); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        return Args{{M, u(ri(0, 3)) | (ri(0, 3) == 0 ? 0x100u : 0u) | (ri(0, 3) == 0 ? 0x200u : 0u)}, {}};
    }, [](A a) { moby_pose_capture(a.i[0], a.i[1]); return uint64_t{0}; });

    check("moby_pose_eval", 0x00254b58, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, R() | 1); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        return Args{{M}, {}};
    }, [](A a) { moby_pose_eval(a.i[0]); return uint64_t{0}; });

    check("moby_pose_matrices_masked", 0x00254108, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t MASK = 0x01e8e000; const int cnt = ri(1, n);
        for (int k = 0; k < 0x7f; ++k) w8(MASK + u(k), k < cnt ? u(ri(0, 2) ? 1 : 0) : u(ri(0, 1)));
        w8(MASK + u(cnt - 1), 1); w8(MASK + 0x7f, u(cnt));
        return Args{{M, MASK}, {}};
    }, [](A a) { moby_pose_matrices_masked(a.i[0], a.i[1]); return uint64_t{0}; });

    check("moby_anim_step", 0x0024f190, N * 6, Ret::None, [] {
        constexpr uint32_t CL = 0x01e70000, BIND = 0x01e70100, SETS = 0x01e71000, FR = 0x01e72000, DST = 0x01e90000;
        const int n = ri(1, 16), bq = ri(n, 24);
        std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
        for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
        w16(M + 2, u(n)); w32(M + 0xc, T); for (int k = 0; k < n; ++k) w8(T + u(k), u(js[static_cast<size_t>(k)]));
        w32(M + 0x14, CL); w8(CL + 8, u(bq)); w32(CL + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1);
        w32(M + 0x10, DST);
        int cnt[2];
        for (int st = 0; st < 2; ++st) {
            const uint32_t SE = SETS + 0x100 * u(st); cnt[st] = ri(1, 4);
            w32(CL + 0x48 + 4 * u(st), SE); w8(SE + 0x10, u(cnt[st]));
            for (int f = 0; f < cnt[st]; ++f) {
                const uint32_t F = FR + 0x400 * u(st * 4 + f); w32(SE + 0x1c + 4 * u(f), F);
                const int nr = ri(0, 6), np = ri(0, 6), nq = (bq + nr + np + 1) / 2;
                wf(F, rf(0.8f, 1.2f)); w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
                wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 4, R() | 1);
                for (int r = 0; r < nr + np; ++r) w8(F + 0x10 + u(bq + r) * 8 + 6, ri(0, 7) ? u(js[static_cast<size_t>(ri(0, n - 1))]) : u(ri(0, 255)));
            }
        }
        const int s0 = ri(0, 1), s1 = ri(0, 1);
        w8(M + 0x22, u(s0)); w8(M + 0x20, u(ri(0, cnt[s0] - 1))); w8(M + 0x23, u(s1)); w8(M + 0x21, u(ri(0, cnt[s1] - 1)));
        w8(M + 5, R()); w8(M + 6, 0); w8(M + 7, 0);
        wf(M + 0x24, ri(0, 5) ? rf(0, 1.3f) : rf(0.985f, 1.0f)); wf(M + 0x28, ri(0, 5) ? rf(-0.3f, 1.5f) : 0.0f); wf(M + 0x2c, ri(0, 5) ? rf(0.8f, 1.2f) : 0.0f);
        return Args{{R(), M}, {}};
    }, [](A a) { moby_anim_step(a.i[1]); return uint64_t{0}; });

    check("moby_chain_matrices", 0x00253fa0, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < 6; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        return Args{{M, u(k), IDS, DSTM}, {}};
    }, [](A a) { moby_chain_matrices(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3]); return uint64_t{0}; });

    check("moby_pose_encode", 0x002092c8, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, R() | 1); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t LST = 0x01e8e000, OUTF = 0x01e8f000;
        const int len = ri(0, 8); for (int i = 0; i < len; ++i) w32(LST + 4 + 4 * u(i), u(ri(0, n - 1))); w32(LST + 4 + 4 * u(len), 0xffffffffu);
        const bool hero = ri(0, 3) == 0; w32(0x001413d0, M);
        return Args{{LST, OUTF, M2, u(ri(-1, 3)), hero ? 0u : M}, {}};
    }, [](A a) { moby_pose_encode(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3]), a.i[4]); return uint64_t{0}; });

    check("moby_anim_set", 0x00257880, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, R() | 1); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        if (ri(0, 1)) { w32(M + 0x60, 0); w32(M + 0x64, 0); }
        if (ri(0, 2) == 0) w32(M + 0x54, 0);
        for (int i = 0; i < 4; ++i) { const uint32_t SP = 0x01e8d000 + 0x80 * u(i); w32(M2 + 0x48 + 4 * u(i), SP); wrand(SP, 0x20); w8(SP + 0x10, u(ri(1, 8))); }
        for (int i = 0; i < 16; ++i) w32(OV(0x00196cc0) + 4 * u(i), ri(0, 3) ? u(ri(1, 40)) : 0u);
        w8(M + 0x52, ri(0, 1) ? 0xffu : u(ri(0, 3))); wrand(M + 0xf0, 4); w8(M + 0x70, R());
        return Args{{M, u(ri(0, 3)), u(ri(-1, 9)), u(ri(-1, 12))}, {}};
    }, [](A a) { moby_anim_set(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), static_cast<int32_t>(a.i[3])); return uint64_t{0}; });

    check("moby_chain_world", 0x0024f8f0, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < 6; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
        return Args{{M, u(k), IDS, DSTM}, {}};
    }, [](A a) { moby_chain_world(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], a.i[3]); return uint64_t{0}; });

    check("hero_hands_anim", 0x00231e78, N * 6, Ret::None, [] {
        constexpr uint32_t HA = 0x01e70000, HB = 0x01e70400, CLS = 0x01e70800, SETS = 0x01e71000;
        const uint32_t P = g::PLAYER;
        seed_rand();
        w32(P + 0x11a4, ri(0, 5) ? 2u : 1u); w32(P + 0x1180, ri(0, 7) ? HA : 0u); w32(P + 0x1184, ri(0, 7) ? HB : 0u);
        for (int i = 0; i < 4; ++i) { const uint32_t SP = SETS + 0x80 * u(i); w32(CLS + 0x48 + 4 * u(i), SP); wrand(SP, 0x20); w8(SP + 0x10, u(ri(1, 8))); }
        for (uint32_t H : {HA, HB}) {
            wrand(H, 0x40); w32(H + 0x24, CLS); w32(H + 0x54, 0); w32(H + 0x60, 0); w32(H + 0x64, 0);
            w8(H + 0x53, u(ri(0, 3))); w8(H + 0x52, ri(0, 1) ? 0xffu : u(ri(0, 3)));
        }
        w32(P + 3 * 0x50 + 0x10b4, ri(0, 4) ? 2u : 1u); w32(P + 3 * 0x50 + 0x10b8, u(ri(1, 5)));
        static const uint32_t lists[] = {OV(0x0017bbf0), OV(0x0017bbd0), OV(0x0015f6b8)};
        for (uint32_t l : lists) { const int n = ri(0, 4); for (int i = 0; i < n; ++i) { w16(l + 4 * u(i), u(ri(0, 3))); w8(l + 4 * u(i) + 2, u(ri(0, 3))); w8(l + 4 * u(i) + 3, u(ri(0, 3))); } w16(l + 4 * u(n), 0xffff); }
        w32(P + 0x208c, u(ri(0, 1))); w32(P + 0x2090, ri(0, 1) ? 8u : 3u);
        return Args{{u(ri(0, 2)), u(ri(-1, 9))}, {rf(0, 20)}};
    }, [](A a) { hero_hands_anim(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), a.f[0]); return uint64_t{0}; });

    check("hero_hold_moby_start", 0x00217658, N * 6, Ret::None, [heroSnd, mobyenv] {
        constexpr uint32_t HM = 0x01e70000, TG = 0x01e70400, CLS = 0x01e70800, SETS = 0x01e71000;
        const uint32_t P = g::PLAYER;
        heroSnd(); mobyenv();
        static const int32_t sts[] = {1, 0x1e, 1, 3};
        w32(P + 0x2084, u(sts[ri(0, 3)])); w32(P + 0x198, u(ri(0, 60)));
        w32(P + 0x1090, ri(0, 7) ? HM : 0u);
        wrand(HM, 0x40); w16(HM + 0xa6, ri(0, 5) ? 0x47u : 0x12u); w32(HM + 0x78, TG); w32(HM + 0x24, CLS); w32(HM + 0x54, 0); w32(HM + 0x60, 0); w32(HM + 0x64, 0);
        w8(HM + 0x53, u(ri(0, 7))); w8(HM + 0x52, 0xff);
        for (int i = 0; i < 7; ++i) { const uint32_t SP = SETS + 0x80 * u(i); w32(CLS + 0x48 + 4 * u(i), SP); wrand(SP, 0x20); w8(SP + 0x10, u(ri(1, 8))); }
        wrand(TG, 0x20); w16(TG + 0x7e, u(ri(0, 1))); wfrand(TG + 0x50, 4, -20, 20);
        wfrand(M2 + 0xc0, 16, -1, 1); wfrand(M2 + 0x10, 4, -9, 9);
        wfrand(OV(0x00166c80) + 0x150, 3, -3, 3);
        wf(P + 0x248, rf(0, 2)); wf(P + 0x250, rf(0, 1)); w8(P + 0x254, u(ri(0, 1)));
        wf(0x0015ed6c, rf(0.5f, 2));
        wrand(0x00140b00 + 0x130, 4); w32(0x00140b00 + 0x134, M3); w32(0x00140b00 + 0x138, u(ri(0, 4))); w16(M3 + 0xa6, u(ri(0, 7))); wrand(M3 + 0x30, 4);
        return Args{};
    }, [](A) { hero_hold_moby_start(); return uint64_t{0}; });

    check("hero_layers_update", 0x0020dd20, N * 8, Ret::None, [] {
        constexpr uint32_t HC = 0x01e70000, BIND = 0x01e70100, CHT = 0x01e70800, SETS = 0x01e71000, FRS = 0x01e72000;
        static const uint32_t LC = 0x01e7f000, LST = 0x01e7f100, HM = 0x01e7f800, POOL = OV(0x0018eb40);
        const uint32_t P = g::PLAYER;
        const int bq = ri(4, 12);
        w8(HC + 8, u(bq)); w32(HC + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1); w32(HC + 0x1c, CHT);
        for (int c = 0; c < 4; ++c) {
            const uint32_t t = CHT + 0x100 + 0x20 * u(c); const int cnt = ri(1, bq);
            w32(CHT + 4 + 4 * u(c), t); w8(t, 0); w8(t + 2, u(cnt));
            std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
            for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
            for (int j = 0; j < cnt; ++j) w8(t + 4 + u(j), u(js[static_cast<size_t>(j)]));
        }
        const int fixed = ri(0, 3);
        w8(LC + 8, u(fixed)); w32(LC + 0x1c, LST); for (int j = 0; j < 8; ++j) w8(LST + 4 + u(j), u(ri(0, bq - 1)));
        for (int s = 0; s < 4; ++s) {
            const uint32_t SE = SETS + 0x100 * u(s); const int cnt = ri(1, 4);
            w32(HC + 0x48 + 4 * u(s), SE); w32(LC + 0x48 + 4 * u(s), SE); w8(SE + 0x10, u(cnt)); w8(SE + 0x11, R());
            for (int f = 0; f < 4; ++f) {  // all four frames valid: frame indexes up to 3 occur
                const uint32_t F = FRS + 0x400 * u(s * 4 + f); w32(SE + 0x1c + 4 * u(f), F);
                const int nr = ri(0, 4), np = ri(0, 4), nq = (fixed + nr + np + 1) / 2 + 1;
                wrand(F, 0x80); wf(F, rf(0.8f, 1.2f)); w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            }
        }
        for (int c = 0; c < 4; ++c) w32(OV(0x00197300) + 4 * u(c), LC);
        for (int i = 0; i < 3; ++i) w8(OV(0x00197bc0) + u(i), u(ri(0, 3)));
        for (int i = 0; i < 2; ++i) w32(OV(0x0015f6a0) + 4 * u(i), u(ri(0, 3)));
        wrand(HM, 0x40); w32(HM + 0x24, HC); w32(HM + 0x60, 0); w8(HM + 0x52, u(ri(0, 3))); w8(HM + 0x53, ri(0, 1) ? ee_read<uint8_t>(HM + 0x52) : u(ri(0, 3)));
        w8(HM + 0x50, u(ri(0, 3))); w8(HM + 0x51, u(ri(0, 3))); wf(HM + 0x54, rf(0, 1)); w8(HM + 0xa5, u(ri(0, 3)));
        w32(P + 0x2080, HM);
        for (int i = 0; i < 6; ++i) w8(POOL + 0x40 * u(i) + 4, u(ri(0, 1)));
        w8(POOL + 0x40 * u(ri(0, 3)) + 4, 0);  // the 6-slot pool never overflows in the game
        for (int i = 0; i < 2; ++i) {
            const uint32_t L = 0x00140050 + 4 * u(i);
            if (ri(0, 2) == 0) { w32(L, 0); continue; }
            const uint32_t e = POOL + 0x40 * u(4 + i); const uint32_t t = CHT + 0x100 + 0x20 * u(ri(0, 3));
            wrand(e, 0x10); w8(e + 4, 1); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * u(4 + i));
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, ee_read<uint32_t>(HM + 0x60)); w32(HM + 0x60, e);
            w8(e + 0x20, u(ri(0, 3))); w8(e + 0x21, u(ri(0, 3))); w8(e + 0x22, u(ri(0, 3))); w8(e + 0x23, u(ri(0, 3)));
            wf(e + 8, rf(0, 1)); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1)); w16(e + 0x30, u(ri(0, 1)));
            w8(e + 0x32, u(ri(0, 1))); w8(e + 0x33, u(ri(0, 1))); w32(e + 0x34, ri(0, 3) ? 0u : 1u);
            w32(L, e);
        }
        w8(P + 0x20ab, u(ri(0, 2))); w8(P + 0x20af, ri(0, 3) ? 0u : 1u); w32(P + 0xd08, u(ri(0, 1))); w32(P + 0xd14, u(ri(0, 1))); w32(P + 0xa9c, u(ri(0, 1)));
        wf(0x0013fdf8, rf(0, 50)); wf(0x0015ed60, rf(0.5f, 2)); wf(0x0015ed64, rf(0.5f, 2));
        return Args{};
    }, [](A) { hero_layers_update(); return uint64_t{0}; });

    check("moby_chain_world_one", 0x0024f728, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < 6; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
        (void)k; return Args{{M, u(ri(0, 5)), DSTM}, {}};
    }, [](A a) { moby_chain_world_one(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });

    // moby M posed for moby_chain_point (class M2, frames, layers, overrides), nch chains
    auto chainenv = [](int nch) {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000; (void)DSTM;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < nch; ++c) {
            const uint32_t C = CHN + 0x10 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
    };
    check("moby_chain_point", 0x0024f7c8, N * 8, Ret::None, [chainenv] {
        chainenv(6);
        wrand(OUT, 4); return Args{{M, u(ri(0, 5)), OUT}, {}};
    }, [](A a) { moby_chain_point(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });

    check("hero_chain_world", 0x00209240, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000; (void)DSTM;
        w32(M2 + 0x1c, CHT);
        int maxid = 0; for (int i = 0; i < 9; ++i) maxid = std::max(maxid, static_cast<int>(ee_read<uint32_t>(OV(0x001e7c10) + 4 * u(i))));
        for (int c = 0; c <= maxid; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
        (void)k; const bool hero = coin(); w32(0x001413d0, M);
        return Args{{hero ? 0u : M}, {}};
    }, [](A a) { hero_chain_world(a.i[0]); return uint64_t{0}; });

    check("moby_anim_rec_start", 0x0024f3e0, N * 6, Ret::None, [] {
        constexpr uint32_t CL = 0x01e70000, BIND = 0x01e70100, SETS = 0x01e71000, FR = 0x01e72000, DST = 0x01e90000;
        const int n = ri(1, 16), bq = ri(n, 24);
        std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
        for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
        w16(M + 2, u(n)); w32(M + 0xc, T); for (int k = 0; k < n; ++k) w8(T + u(k), u(js[static_cast<size_t>(k)]));
        w32(M + 0x14, CL); w8(CL + 8, u(bq)); w32(CL + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1);
        w32(M + 0x10, DST);
        int cnt[2];
        for (int st = 0; st < 2; ++st) {
            const uint32_t SE = SETS + 0x100 * u(st); cnt[st] = ri(1, 4);
            w32(CL + 0x48 + 4 * u(st), SE); w8(SE + 0x10, u(cnt[st]));
            for (int f = 0; f < cnt[st]; ++f) {
                const uint32_t F = FR + 0x400 * u(st * 4 + f); w32(SE + 0x1c + 4 * u(f), F);
                const int nr = ri(0, 6), np = ri(0, 6), nq = (bq + nr + np + 1) / 2;
                wf(F, rf(0.8f, 1.2f)); w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
                wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 4, R() | 1);
                for (int r = 0; r < nr + np; ++r) w8(F + 0x10 + u(bq + r) * 8 + 6, ri(0, 7) ? u(js[static_cast<size_t>(ri(0, n - 1))]) : u(ri(0, 255)));
            }
        }
        const int s0 = ri(0, 1), s1 = ri(0, 1);
        w8(M + 0x22, u(s0)); w8(M + 0x20, u(ri(0, cnt[s0] - 1))); w8(M + 0x23, u(s1)); w8(M + 0x21, u(ri(0, cnt[s1] - 1)));
        w8(M + 5, R()); w8(M + 6, 0); w8(M + 7, 0);
        wf(M + 0x24, ri(0, 5) ? rf(0, 1.3f) : rf(0.985f, 1.0f)); wf(M + 0x28, ri(0, 5) ? rf(-0.3f, 1.5f) : 0.0f); wf(M + 0x2c, ri(0, 5) ? rf(0.8f, 1.2f) : 0.0f);
        const int so = ri(0, 1), sn = ri(0, 1); w8(M3 + 0x53, u(so)); w8(M3 + 0x51, u(ri(0, cnt[so] - 1)));
        return Args{{M3, M, u(sn), u(ri(0, cnt[sn] - 1)), u(ri(1, 20)), u(ri(0, 1))}, {}};
    }, [](A a) { moby_anim_rec_start(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), static_cast<int32_t>(a.i[5])); return uint64_t{0}; });

    check("hero_hands_anim_start", 0x00231d08, N * 6, Ret::None, [] {
        constexpr uint32_t HA = 0x01e70000, HB = 0x01e70400, CLS = 0x01e70800, SETS = 0x01e71000;
        const uint32_t P = g::PLAYER;
        seed_rand();
        w32(P + 0x11a4, ri(0, 5) ? 2u : 1u); w32(P + 0x1180, ri(0, 7) ? HA : 0u); w32(P + 0x1184, ri(0, 7) ? HB : 0u);
        for (int i = 0; i < 4; ++i) { const uint32_t SP = SETS + 0x80 * u(i); w32(CLS + 0x48 + 4 * u(i), SP); wrand(SP, 0x20); w8(SP + 0x10, u(ri(1, 8))); }
        for (uint32_t H : {HA, HB}) {
            wrand(H, 0x40); w32(H + 0x24, CLS); w32(H + 0x54, 0); w32(H + 0x60, 0); w32(H + 0x64, 0);
            w8(H + 0x53, u(ri(0, 3))); w8(H + 0x52, ri(0, 1) ? 0xffu : u(ri(0, 3)));
        }
        w32(P + 3 * 0x50 + 0x10b4, ri(0, 4) ? 2u : 1u); w32(P + 3 * 0x50 + 0x10b8, u(ri(1, 5)));
        static const uint32_t lists[] = {OV(0x0017bbf0), OV(0x0017bbd0), OV(0x0015f6b8)};
        for (uint32_t l : lists) { const int n = ri(0, 4); for (int i = 0; i < n; ++i) { w16(l + 4 * u(i), u(ri(0, 3))); w8(l + 4 * u(i) + 2, u(ri(0, 3))); w8(l + 4 * u(i) + 3, u(ri(0, 3))); } w16(l + 4 * u(n), 0xffff); }
        w32(P + 0x208c, u(ri(0, 1))); w32(P + 0x2090, ri(0, 1) ? 8u : 3u);
        if (coin()) w8(HA + 0x53, 1);
        return Args{};
    }, [](A) { hero_hands_anim_start(); return uint64_t{0}; });

    check("hero_pair_anim", 0x00232220, N * 6, Ret::None, [] {
        constexpr uint32_t HA = 0x01e70000, HB = 0x01e70400, CLS = 0x01e70800, SETS = 0x01e71000;
        const uint32_t P = g::PLAYER;
        seed_rand();
        w32(P + 0x11a4, ri(0, 5) ? 2u : 1u); w32(P + 0x1180, ri(0, 7) ? HA : 0u); w32(P + 0x1184, ri(0, 7) ? HB : 0u);
        for (int i = 0; i < 4; ++i) { const uint32_t SP = SETS + 0x80 * u(i); w32(CLS + 0x48 + 4 * u(i), SP); wrand(SP, 0x20); w8(SP + 0x10, u(ri(1, 8))); }
        for (uint32_t H : {HA, HB}) {
            wrand(H, 0x40); w32(H + 0x24, CLS); w32(H + 0x54, 0); w32(H + 0x60, 0); w32(H + 0x64, 0);
            w8(H + 0x53, u(ri(0, 3))); w8(H + 0x52, ri(0, 1) ? 0xffu : u(ri(0, 3)));
        }
        w32(P + 3 * 0x50 + 0x10b4, ri(0, 4) ? 2u : 1u); w32(P + 3 * 0x50 + 0x10b8, u(ri(1, 5)));
        static const uint32_t lists[] = {OV(0x0017bbf0), OV(0x0017bbd0), OV(0x0015f6b8)};
        for (uint32_t l : lists) { const int n = ri(0, 4); for (int i = 0; i < n; ++i) { w16(l + 4 * u(i), u(ri(0, 3))); w8(l + 4 * u(i) + 2, u(ri(0, 3))); w8(l + 4 * u(i) + 3, u(ri(0, 3))); } w16(l + 4 * u(n), 0xffff); }
        w32(P + 0x208c, u(ri(0, 1))); w32(P + 0x2090, ri(0, 1) ? 8u : 3u);
        w32(P + 0x2278, ri(0, 7) ? HA : 0u); w32(P + 0x227c, ri(0, 7) ? HB : 0u); if (coin()) w8(HA + 0x53, 0);
        const int n = ri(0, 6); for (int i = 0; i < n; ++i) { w16(T + 4 * u(i), u(ri(0, 10))); w16(T + 4 * u(i) + 2, u(ri(0, 3))); }
        w16(T + 4 * u(n), 0xffff); w32(0x001415cc, ri(0, 5) ? T : 0u);
        return Args{{u(ri(-1, 12)), u(ri(-1, 9))}, {rf(0, 20)}};
    }, [](A a) { hero_pair_anim(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), a.f[0]); return uint64_t{0}; });

    check("hero_layers_start", 0x0020e360, N * 8, Ret::None, [] {
        constexpr uint32_t HC = 0x01e70000, BIND = 0x01e70100, CHT = 0x01e70800, SETS = 0x01e71000, FRS = 0x01e72000;
        static const uint32_t LC = 0x01e7f000, LST = 0x01e7f100, HM = 0x01e7f800, POOL = OV(0x0018eb40);
        const uint32_t P = g::PLAYER;
        const int bq = ri(4, 12);
        w8(HC + 8, u(bq)); w32(HC + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1); w32(HC + 0x1c, CHT);
        for (int c = 0; c < 4; ++c) {
            const uint32_t t = CHT + 0x100 + 0x20 * u(c); const int cnt = ri(1, bq);
            w32(CHT + 4 + 4 * u(c), t); w8(t, 0); w8(t + 2, u(cnt));
            std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
            for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
            for (int j = 0; j < cnt; ++j) w8(t + 4 + u(j), u(js[static_cast<size_t>(j)]));
        }
        const int fixed = ri(0, 3);
        w8(LC + 8, u(fixed)); w32(LC + 0x1c, LST); for (int j = 0; j < 8; ++j) w8(LST + 4 + u(j), u(ri(0, bq - 1)));
        for (int s = 0; s < 4; ++s) {
            const uint32_t SE = SETS + 0x100 * u(s); const int cnt = ri(1, 4);
            w32(HC + 0x48 + 4 * u(s), SE); w32(LC + 0x48 + 4 * u(s), SE); w8(SE + 0x10, u(cnt)); w8(SE + 0x11, R());
            for (int f = 0; f < 4; ++f) {  // all four frames valid: frame indexes up to 3 occur
                const uint32_t F = FRS + 0x400 * u(s * 4 + f); w32(SE + 0x1c + 4 * u(f), F);
                const int nr = ri(0, 4), np = ri(0, 4), nq = (fixed + nr + np + 1) / 2 + 1;
                wrand(F, 0x80); wf(F, rf(0.8f, 1.2f)); w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            }
        }
        for (int c = 0; c < 4; ++c) w32(OV(0x00197300) + 4 * u(c), LC);
        for (int i = 0; i < 3; ++i) w8(OV(0x00197bc0) + u(i), u(ri(0, 3)));
        for (int i = 0; i < 2; ++i) w32(OV(0x0015f6a0) + 4 * u(i), u(ri(0, 3)));
        wrand(HM, 0x40); w32(HM + 0x24, HC); w32(HM + 0x60, 0); w8(HM + 0x52, u(ri(0, 3))); w8(HM + 0x53, ri(0, 1) ? ee_read<uint8_t>(HM + 0x52) : u(ri(0, 3)));
        w8(HM + 0x50, u(ri(0, 3))); w8(HM + 0x51, u(ri(0, 3))); wf(HM + 0x54, rf(0, 1)); w8(HM + 0xa5, u(ri(0, 3)));
        w32(P + 0x2080, HM);
        for (int i = 0; i < 6; ++i) w8(POOL + 0x40 * u(i) + 4, u(ri(0, 1)));
        w8(POOL + 0x40 * u(ri(0, 3)) + 4, 0);  // the 6-slot pool never overflows in the game
        for (int i = 0; i < 2; ++i) {
            const uint32_t L = 0x00140050 + 4 * u(i);
            if (ri(0, 2) == 0) { w32(L, 0); continue; }
            const uint32_t e = POOL + 0x40 * u(4 + i); const uint32_t t = CHT + 0x100 + 0x20 * u(ri(0, 3));
            wrand(e, 0x10); w8(e + 4, 1); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * u(4 + i));
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, ee_read<uint32_t>(HM + 0x60)); w32(HM + 0x60, e);
            w8(e + 0x20, u(ri(0, 3))); w8(e + 0x21, u(ri(0, 3))); w8(e + 0x22, u(ri(0, 3))); w8(e + 0x23, u(ri(0, 3)));
            wf(e + 8, rf(0, 1)); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1)); w16(e + 0x30, u(ri(0, 1)));
            w8(e + 0x32, u(ri(0, 1))); w8(e + 0x33, u(ri(0, 1))); w32(e + 0x34, ri(0, 3) ? 0u : 1u);
            w32(L, e);
        }
        w8(P + 0x20ab, u(ri(0, 2))); w8(P + 0x20af, ri(0, 3) ? 0u : 1u); w32(P + 0xd08, u(ri(0, 1))); w32(P + 0xd14, u(ri(0, 1))); w32(P + 0xa9c, u(ri(0, 1)));
        wf(0x0013fdf8, rf(0, 50)); wf(0x0015ed60, rf(0.5f, 2)); wf(0x0015ed64, rf(0.5f, 2));
        for (int i = 0; i < 6; ++i) { w8(POOL + 0x40 * u(i) + 6, 0); w8(POOL + 0x40 * u(i) + 7, 0); }
        w32(P + 0xd08, ri(0, 2) ? 0u : POOL + 0x100); w32(P + 0xd0c, ri(0, 2) ? 0u : POOL + 0x140);
        w8(P + 0x20a4, ri(0, 7) ? 0u : 1u); w8(P + 0x20a8, ri(0, 7) ? 1u : 0u); w8(P + 0x20aa, ri(0, 7) ? 1u : 0u);
        w32(P + 0x10b4, 2); w32(P + 0x10b8, u(ri(0, 0x24)));
        for (int i = 0; i < 0x25; ++i) w32(OV(0x00179ac0) + 0x4c * u(i) + 0x28, ri(0, 5) ? u(ri(0, 3)) : 0xffffffffu);
        return Args{{ri(0, 7) ? 0u : 1u, ri(0, 7) ? 1u : 0u}, {}};
    }, [](A a) { hero_layers_start(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; });

    // hero moby M (class M2) ready for hero_anim_set: bind pose, frames, layers / overrides
    auto animenv = [] {
        constexpr uint32_t HCLS = 0x01e90000;  // the hero class (anim set table for 0x80 sets)
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(g_anim_nmin, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, HCLS); w8(HCLS + 8, u(n)); w32(HCLS + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, R() | 1); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        const uint32_t P = g::PLAYER;
        for (int i = 0; i < 4; ++i) { const uint32_t SP = 0x01e8d000 + 0x80 * u(i); wrand(SP, 0x20); w8(SP + 0x10, u(ri(1, 8))); for (int f = 0; f < 8; ++f) w32(SP + 0x1c + 4 * u(f), ri(0, 1) ? FA : FB); }  // frames with their DMA sentinel
        for (int i = 0; i < 0x80; ++i) w32(HCLS + 0x48 + 4 * u(i), 0x01e8d000 + 0x80 * u(ri(0, 3)));
        for (int i = 0; i < 16; ++i) w32(OV(0x00196cc0) + 4 * u(i), ri(0, 3) ? u(ri(1, 40)) : 0u);
        w8(M + 0x52, ri(0, 1) ? 0xffu : u(ri(0, 3))); wrand(M + 0xf0, 4);
        w32(P + 0x2080, M); w32(P + 0xa98, R()); w8(0x0015edb5, u(ri(0, 1)));
        for (int i = 0; i < 8; ++i) w32(OV(0x0015f6e0) + 4 * u(i), u(ri(1, 20)));
        w32(P + 0x11a4, ri(0, 1) ? 2u : 1u); w32(P + 0x1180, 0); w32(P + 0x1184, 0); w32(P + 0x2278, 0); w8(P + 0x20a4, u(ri(0, 1)));
    };
    check("hero_anim_set", 0x002323b8, N * 8, Ret::None, [animenv] {
        animenv();
        static const uint32_t sets[] = {0x31, 0x32, 0, 1, 2, 3};
        return Args{{sets[ri(0, 5)], u(ri(0, 7))}, {ri(0, 2) ? rf(0.5f, 20) : -static_cast<float>(ri(0, 6))}};
    }, [](A a) { hero_anim_set(a.i[0], a.i[1], a.f[0]); return uint64_t{0}; });


    // ======== batch 40
    g_batch = 40;
    check("hero_extra_layer_update", 0x0020da68, N * 8, Ret::None, [] {
        constexpr uint32_t HC = 0x01e70000, BIND = 0x01e70100, CHT = 0x01e70800, SETS = 0x01e71000, FRS = 0x01e72000;
        static const uint32_t LC = 0x01e7f000, LST = 0x01e7f100, HM = 0x01e7f800, POOL = OV(0x0018eb40);
        const uint32_t P = g::PLAYER;
        const int bq = ri(4, 12);
        w8(HC + 8, u(bq)); w32(HC + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1); w32(HC + 0x1c, CHT);
        for (int c = 0; c < 4; ++c) {
            const uint32_t t = CHT + 0x100 + 0x20 * u(c); const int cnt = ri(1, bq);
            w32(CHT + 4 + 4 * u(c), t); w8(t, 0); w8(t + 2, u(cnt));
            std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
            for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
            for (int j = 0; j < cnt; ++j) w8(t + 4 + u(j), u(js[static_cast<size_t>(j)]));
        }
        const int fixed = ri(0, 3);
        w8(LC + 8, u(fixed)); w32(LC + 0x1c, LST); for (int j = 0; j < 8; ++j) w8(LST + 4 + u(j), u(ri(0, bq - 1)));
        for (int s = 0; s < 4; ++s) {
            const uint32_t SE = SETS + 0x100 * u(s); const int cnt = ri(1, 4);
            w32(HC + 0x48 + 4 * u(s), SE); w32(LC + 0x48 + 4 * u(s), SE); w8(SE + 0x10, u(cnt)); w8(SE + 0x11, R());
            for (int f = 0; f < 4; ++f) {  // all four frames valid: frame indexes up to 3 occur
                const uint32_t F = FRS + 0x400 * u(s * 4 + f); w32(SE + 0x1c + 4 * u(f), F);
                const int nr = ri(0, 4), np = ri(0, 4), nq = (fixed + nr + np + 1) / 2 + 1;
                wrand(F, 0x80); wf(F, rf(0.8f, 1.2f)); w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            }
        }
        for (int c = 0; c < 4; ++c) w32(OV(0x00197300) + 4 * u(c), LC);
        for (int i = 0; i < 3; ++i) w8(OV(0x00197bc0) + u(i), u(ri(0, 3)));
        for (int i = 0; i < 2; ++i) w32(OV(0x0015f6a0) + 4 * u(i), u(ri(0, 3)));
        wrand(HM, 0x40); w32(HM + 0x24, HC); w32(HM + 0x60, 0); w8(HM + 0x52, u(ri(0, 3))); w8(HM + 0x53, ri(0, 1) ? ee_read<uint8_t>(HM + 0x52) : u(ri(0, 3)));
        w8(HM + 0x50, u(ri(0, 3))); w8(HM + 0x51, u(ri(0, 3))); wf(HM + 0x54, rf(0, 1)); w8(HM + 0xa5, u(ri(0, 3)));
        w32(P + 0x2080, HM);
        for (int i = 0; i < 6; ++i) w8(POOL + 0x40 * u(i) + 4, u(ri(0, 1)));
        w8(POOL + 0x40 * u(ri(0, 3)) + 4, 0);  // the 6-slot pool never overflows in the game
        for (int i = 0; i < 2; ++i) {
            const uint32_t L = 0x00140050 + 4 * u(i);
            if (ri(0, 2) == 0) { w32(L, 0); continue; }
            const uint32_t e = POOL + 0x40 * u(4 + i); const uint32_t t = CHT + 0x100 + 0x20 * u(ri(0, 3));
            wrand(e, 0x10); w8(e + 4, 1); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * u(4 + i));
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, ee_read<uint32_t>(HM + 0x60)); w32(HM + 0x60, e);
            w8(e + 0x20, u(ri(0, 3))); w8(e + 0x21, u(ri(0, 3))); w8(e + 0x22, u(ri(0, 3))); w8(e + 0x23, u(ri(0, 3)));
            wf(e + 8, rf(0, 1)); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1)); w16(e + 0x30, u(ri(0, 1)));
            w8(e + 0x32, u(ri(0, 1))); w8(e + 0x33, u(ri(0, 1))); w32(e + 0x34, ri(0, 3) ? 0u : 1u);
            w32(L, e);
        }
        w8(P + 0x20ab, u(ri(0, 2))); w8(P + 0x20af, ri(0, 3) ? 0u : 1u); w32(P + 0xd08, u(ri(0, 1))); w32(P + 0xd14, u(ri(0, 1))); w32(P + 0xa9c, u(ri(0, 1)));
        wf(0x0013fdf8, rf(0, 50)); wf(0x0015ed60, rf(0.5f, 2)); wf(0x0015ed64, rf(0.5f, 2));
        for (int i = 0; i < 6; ++i) { w8(POOL + 0x40 * u(i) + 6, 0); w8(POOL + 0x40 * u(i) + 7, 0); }
        constexpr uint32_t TM = 0x01e7fc00, PVT = 0x01e7fd00, PVS = 0x01e7fe00;
        w32(CHT + 4 + 4 * 0x18, CHT + 0x100);  // the layer uses chain 0x18
        for (int k = 4; k < 0x84; ++k) w32(HC + 0x48 + 4 * u(k), SETS + 0x100 * u(k & 3));  // sets up to 0x83 occur
        {   // an existing layer record in pool slot 4
            const uint32_t e = POOL + 0x100, t = CHT + 0x100;
            wrand(e, 0x10); w8(e + 4, 1); w8(e + 6, 0); w8(e + 7, 0); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * 4);
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, 0);
            for (uint32_t k = 0x20; k < 0x24; ++k) w8(e + k, u(ri(0, 3)));
            wf(e + 8, rf(0, 1)); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1));
        }
        w32(P + 0xd10, ri(0, 2) ? 0u : POOL + 0x100);
        w32(P + 0x2274, ri(0, 2) ? 0u : TM); wrand(TM, 0x40); w32(TM + 0x78, PVT); w32(PVT, ri(0, 3) ? PVS : 0u); w8(PVS + 0x3c, u(ri(0, 5)));
        w16(P + 0x1dc, ri(0, 3) ? 0u : 1u); w16(P + 0x22ce, u(ri(-2, 6))); w32(P + 0x22a8, u(ri(0, 2)));
        static const uint32_t cur[] = {0x54, 0x82, 0x5a, 1, 2}; w8(HM + 0x53, cur[ri(0, 4)]);
        w32(P + 0x208c, ri(0, 3) ? u(ri(0, 2)) : 0x11u); w8(0x0013d4c6, u(ri(0, 1))); w32(P + 0x22a0, u(ri(0, 15000)));
        w32(P + 0x10b4, ri(0, 3) ? 2u : 1u); w32(P + 0x10b8, u(ri(0xe, 0x14))); w8(P + 0x20a8, u(ri(0, 1)));
        return Args{};
    }, [](A) { hero_extra_layer_update(); return uint64_t{0}; });

    check("hero_smoke_puffs", 0x00209a40, N * 4, Ret::None, [particles2] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000; (void)DSTM;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < 0x18; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
        (void)k; (void)IDS; (void)DSTM;
        particles2(); w32(OV(0x001b2108), T); w8(T, R()); wf(0x0015ed6c, rf(0.5f, 2));
        const uint32_t P = g::PLAYER;
        w32(g::HERO_STATE, coin() ? 0x31u : 0x10u); wfrand(P + 0x80, 4, -9, 9); wf(P + 0x2f0, rf(-9, 9));
        wfrand(P + 0x100, 4, -1, 1); w32(P + 0x2084, ri(0, 1) ? 0x34u : 3u); w32(P + 0x2080, M);
        return Args{{u(ri(0, 3)), u(ri(0, 2))}, {}};
    }, [](A a) { hero_smoke_puffs(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; });

    check("hero_rider_attach", 0x002061f0, N * 4, Ret::None, [gridenv, heroSnd] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000; (void)DSTM;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < 6; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
        (void)k;
        heroSnd(); w32(OV(0x00166c00u - 0x7634), R()); w32(M + 0x24, M2); w32(M2 + 0x18, BIND); w32(M2 + 0x1c, CHT);  // heroSnd reuses M / M2
        const int kk = gridenv(); const uint32_t rm = T4 + 0x100 * u(ri(0, kk));
        constexpr uint32_t FRM = 0x01e78000;
        for (int i = 0; i < 16; ++i) wf(FRM + 0x10 * u(i), rf(0.5f, 2));
        for (int a = 0; a < 4; ++a) {
            const uint32_t h = T2 + 0x80 * u(a); const int nfr = ri(1, 4);
            w32(T + 0x48 + 4 * u(a), h); wfrand(h, 3, -1, 1); wf(h + 12, rf(500, 30000));
            w8(h + 0x10, u(nfr)); w8(h + 0x11, R()); w8(h + 0x12, R()); w32(h + 0x18, 0);
            for (int i = 0; i < nfr; ++i) w32(h + 0x1c + 4 * u(i), FRM + 0x10 * u(ri(0, 15)));
            for (int e = 0; e < 6; ++e) { w16(h + 0x1c + 4 * u(nfr) + 4 * u(e), u(ri(0, 9))); w16(h + 0x1e + 4 * u(nfr) + 4 * u(e), u(ri(0, nfr * 16 + 8))); }
        }
        w32(T + 0x48 + 4 * 0x81, T2 + 0x80 * u(ri(0, 3)));
        w8(rm + 0x20, ri(0, 6) ? u(ri(0, 3)) : 0xfeu); w16(rm + 0x34, R() & 0xffffu); w32(rm + 0x24, T); wf(rm + 0x2c, rf(0.5f, 2));
        w8(rm + 0x52, u(ri(0, 3))); w8(rm + 0x53, u(ri(0, 3))); w8(rm + 0x50, u(ri(0, 7))); w32(rm + 0x54, 0); w32(rm + 0x60, 0); w32(rm + 0x64, 0); w8(rm + 0x71, u(ri(0, 3)));
        wfrand(OV(0x00196d00), 32, -1, 1); for (int i = 0; i < 8; ++i) wf(OV(0x00196d00) + 16 * u(i) + 12, rf(500, 30000));
        wfrand(rm + 0xf0, 3, -1, 1); wf(rm + 0xfc, rf(500, 30000)); w16(rm + 0xa8, R()); w32(rm + 0x94, u(ri(0, 3)));
        w32(g::PLAYER + 0xa88, rm); w32(g::PLAYER + 0x2080, M); w16(M + 0x34, R() & 0xffffu);
        return Args{};
    }, [](A) { hero_rider_attach(); return uint64_t{0}; });

    check("hero_hold_release", 0x0020e698, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, R() | 1); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        const uint32_t P = g::PLAYER;
        for (int i = 0; i < 4; ++i) { const uint32_t SP = 0x01e8d000 + 0x80 * u(i); wrand(SP, 0x20); w8(SP + 0x10, u(ri(1, 8))); }
        for (int i = 0; i < 0x34; ++i) w32(M2 + 0x48 + 4 * u(i), 0x01e8d000 + 0x80 * u(ri(0, 3)));
        for (int i = 0; i < 16; ++i) w32(OV(0x00196cc0) + 4 * u(i), ri(0, 3) ? u(ri(1, 40)) : 0u);
        w8(M + 0x52, ri(0, 1) ? 0xffu : u(ri(0, 3))); wrand(M + 0xf0, 4);
        w32(P + 0x2080, M); w32(P + 0xa98, R()); w8(0x0015edb5, u(ri(0, 1)));
        for (int i = 0; i < 8; ++i) w32(OV(0x0015f6e0) + 4 * u(i), u(ri(1, 20)));
        w32(P + 0x11a4, ri(0, 1) ? 2u : 1u); w32(P + 0x1180, 0); w32(P + 0x1184, 0); w32(P + 0x2278, 0); w8(P + 0x20a4, u(ri(0, 1)));
        w8(P + 0x20a8, ri(0, 5) ? 1u : 0u); w8(P + 0x20aa, u(ri(0, 1))); w32(P + 0xd08, ri(0, 2) ? 0u : OV(0x0018eb40u));
        w32(P + 0x2084, ri(0, 3) ? 0u : 4u); w32(P + 0x2294, ri(0, 2) ? ee_read<uint8_t>(M + 0x53) : 0x33u);
        w8(P + 0x20a4, u(ri(0, 3))); w32(P + 0x22a8, u(ri(0, 1))); w8(P + 0x12e2, u(ri(0, 1)));
        return Args{};
    }, [](A) { hero_hold_release(); return uint64_t{0}; });

    check("hero_hold_start", 0x0020e4c8, N * 8, Ret::None, [] {
        constexpr uint32_t HC = 0x01e70000, BIND = 0x01e70100, CHT = 0x01e70800, SETS = 0x01e71000, FRS = 0x01e72000;
        static const uint32_t LC = 0x01e7f000, LST = 0x01e7f100, HM = 0x01e7f800, POOL = OV(0x0018eb40);
        const uint32_t P = g::PLAYER;
        const int bq = ri(4, 12);
        w8(HC + 8, u(bq)); w32(HC + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1); w32(HC + 0x1c, CHT);
        for (int c = 0; c < 4; ++c) {
            const uint32_t t = CHT + 0x100 + 0x20 * u(c); const int cnt = ri(1, bq);
            w32(CHT + 4 + 4 * u(c), t); w8(t, 0); w8(t + 2, u(cnt));
            std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
            for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
            for (int j = 0; j < cnt; ++j) w8(t + 4 + u(j), u(js[static_cast<size_t>(j)]));
        }
        const int fixed = ri(0, 3);
        w8(LC + 8, u(fixed)); w32(LC + 0x1c, LST); for (int j = 0; j < 8; ++j) w8(LST + 4 + u(j), u(ri(0, bq - 1)));
        for (int s = 0; s < 4; ++s) {
            const uint32_t SE = SETS + 0x100 * u(s); const int cnt = ri(1, 4);
            w32(HC + 0x48 + 4 * u(s), SE); w32(LC + 0x48 + 4 * u(s), SE); w8(SE + 0x10, u(cnt)); w8(SE + 0x11, R());
            for (int f = 0; f < 4; ++f) {  // all four frames valid: frame indexes up to 3 occur
                const uint32_t F = FRS + 0x400 * u(s * 4 + f); w32(SE + 0x1c + 4 * u(f), F);
                const int nr = ri(0, 4), np = ri(0, 4), nq = (fixed + nr + np + 1) / 2 + 1;
                wrand(F, 0x80); wf(F, rf(0.8f, 1.2f)); w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            }
        }
        for (int c = 0; c < 4; ++c) w32(OV(0x00197300) + 4 * u(c), LC);
        for (int i = 0; i < 3; ++i) w8(OV(0x00197bc0) + u(i), u(ri(0, 3)));
        for (int i = 0; i < 2; ++i) w32(OV(0x0015f6a0) + 4 * u(i), u(ri(0, 3)));
        wrand(HM, 0x40); w32(HM + 0x24, HC); w32(HM + 0x60, 0); w8(HM + 0x52, u(ri(0, 3))); w8(HM + 0x53, ri(0, 1) ? ee_read<uint8_t>(HM + 0x52) : u(ri(0, 3)));
        w8(HM + 0x50, u(ri(0, 3))); w8(HM + 0x51, u(ri(0, 3))); wf(HM + 0x54, rf(0, 1)); w8(HM + 0xa5, u(ri(0, 3)));
        w32(P + 0x2080, HM);
        for (int i = 0; i < 6; ++i) w8(POOL + 0x40 * u(i) + 4, u(ri(0, 1)));
        w8(POOL + 0x40 * u(ri(0, 3)) + 4, 0);  // the 6-slot pool never overflows in the game
        for (int i = 0; i < 2; ++i) {
            const uint32_t L = 0x00140050 + 4 * u(i);
            if (ri(0, 2) == 0) { w32(L, 0); continue; }
            const uint32_t e = POOL + 0x40 * u(4 + i); const uint32_t t = CHT + 0x100 + 0x20 * u(ri(0, 3));
            wrand(e, 0x10); w8(e + 4, 1); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * u(4 + i));
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, ee_read<uint32_t>(HM + 0x60)); w32(HM + 0x60, e);
            w8(e + 0x20, u(ri(0, 3))); w8(e + 0x21, u(ri(0, 3))); w8(e + 0x22, u(ri(0, 3))); w8(e + 0x23, u(ri(0, 3)));
            wf(e + 8, rf(0, 1)); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1)); w16(e + 0x30, u(ri(0, 1)));
            w8(e + 0x32, u(ri(0, 1))); w8(e + 0x33, u(ri(0, 1))); w32(e + 0x34, ri(0, 3) ? 0u : 1u);
            w32(L, e);
        }
        w8(P + 0x20ab, u(ri(0, 2))); w8(P + 0x20af, ri(0, 3) ? 0u : 1u); w32(P + 0xd08, u(ri(0, 1))); w32(P + 0xd14, u(ri(0, 1))); w32(P + 0xa9c, u(ri(0, 1)));
        wf(0x0013fdf8, rf(0, 50)); wf(0x0015ed60, rf(0.5f, 2)); wf(0x0015ed64, rf(0.5f, 2));
        for (int i = 0; i < 6; ++i) { w8(POOL + 0x40 * u(i) + 6, 0); w8(POOL + 0x40 * u(i) + 7, 0); }
        w32(P + 0xd08, ri(0, 2) ? 0u : POOL + 0x100); w32(P + 0xd0c, ri(0, 2) ? 0u : POOL + 0x140);
        w8(P + 0x20a4, ri(0, 7) ? 0u : 1u); w8(P + 0x20a8, ri(0, 7) ? 1u : 0u); w8(P + 0x20aa, ri(0, 7) ? 1u : 0u);
        w32(P + 0x10b4, 2); w32(P + 0x10b8, u(ri(0, 0x24)));
        for (int i = 0; i < 0x25; ++i) {
            const uint32_t e = OV(0x00179ac0) + 0x4c * u(i);
            w32(e + 0x18, u(ri(0, 1))); w32(e + 0x24, u(ri(0, 3))); w32(e + 0x28, ri(0, 5) ? u(ri(0, 3)) : 0xffffffffu); w32(e + 0x2c, u(ri(0, 3))); w32(e + 0x30, u(ri(0, 2)));
        }
        w32(P + 0x10b8, u(ri(-1, 0x24)));
        w32(P + 0x208c, ri(0, 2) ? 0u : (ri(0, 1) ? 0xcu : 3u)); w16(P + 0x22c8, u(ri(0, 3) ? 0 : 1));
        w32(HM + 0x54, 0); w32(HM + 0x60, 0); w32(HM + 0x64, 0); w32(HM + 0x68, FRS); w32(HM + 0x6c, FRS + 0x400);
        w32(P + 0x11a4, 1); w32(P + 0x2278, 0); w32(P + 0xa98, R());
        return Args{};
    }, [](A) { hero_hold_start(); return uint64_t{0}; });

    check("hero_run_gait_update", 0x00228c18, N * 8, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, R() | 1); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        const uint32_t P = g::PLAYER;
        for (int i = 0; i < 4; ++i) { const uint32_t SP = 0x01e8d000 + 0x80 * u(i); wrand(SP, 0x20); w8(SP + 0x10, u(ri(1, 8))); }
        for (int i = 0; i < 0x34; ++i) w32(M2 + 0x48 + 4 * u(i), 0x01e8d000 + 0x80 * u(ri(0, 3)));
        for (int i = 0; i < 16; ++i) w32(OV(0x00196cc0) + 4 * u(i), ri(0, 3) ? u(ri(1, 40)) : 0u);
        w8(M + 0x52, ri(0, 1) ? 0xffu : u(ri(0, 3))); wrand(M + 0xf0, 4);
        w32(P + 0x2080, M); w32(P + 0xa98, R()); w8(0x0015edb5, u(ri(0, 1)));
        for (int i = 0; i < 8; ++i) w32(OV(0x0015f6e0) + 4 * u(i), u(ri(1, 20)));
        w32(P + 0x11a4, ri(0, 1) ? 2u : 1u); w32(P + 0x1180, 0); w32(P + 0x1184, 0); w32(P + 0x2278, 0); w8(P + 0x20a4, u(ri(0, 1)));
        static const uint32_t modes[] = {0, 0, 0, 1, 3};
        w8(P + 0x20a4, modes[ri(0, 4)]); wf(P + 0x160, rf(0, 0.05f)); w32(P + 0x1a0, u(ri(0, 12))); w16(P + 0x3bc, ri(0, 5) ? 0u : 1u);
        w32(P + 0x2088, u(ri(0, 1))); wf(P + 0x194, rf(0, 0.2f)); wf(0x0015ed6c, rf(0.5f, 2));
        for (int g = -3; g <= 5; ++g) { wf(OV(0x0017bdb8) + static_cast<uint32_t>(g * 16), 0.02f + 0.05f * static_cast<float>(g)); wf(OV(0x0017bdb8) + static_cast<uint32_t>(g * 16) + 4, 0.06f + 0.05f * static_cast<float>(g)); }
        for (int i = 0; i < 4; ++i) w32(OV(0x0015f6c8) + 4 * u(i), u(ri(-3, 3)));
        w8(M + 0x51, u(ri(0, 7)));
        return Args{};
    }, [](A) { hero_run_gait_update(); return uint64_t{0}; });


    // ======== batch 41
    g_batch = 41;
    check("hero_hold_update", 0x0020e728, N * 8, Ret::None, [] {
        constexpr uint32_t HC = 0x01e70000, BIND = 0x01e70100, CHT = 0x01e70800, SETS = 0x01e71000, FRS = 0x01e72000;
        static const uint32_t LC = 0x01e7f000, LST = 0x01e7f100, HM = 0x01e7f800, POOL = OV(0x0018eb40);
        const uint32_t P = g::PLAYER;
        const int bq = ri(4, 12);
        w8(HC + 8, u(bq)); w32(HC + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1); w32(HC + 0x1c, CHT);
        for (int c = 0; c < 4; ++c) {
            const uint32_t t = CHT + 0x100 + 0x20 * u(c); const int cnt = ri(1, bq);
            w32(CHT + 4 + 4 * u(c), t); w8(t, 0); w8(t + 2, u(cnt));
            std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
            for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
            for (int j = 0; j < cnt; ++j) w8(t + 4 + u(j), u(js[static_cast<size_t>(j)]));
        }
        const int fixed = ri(0, 3);
        w8(LC + 8, u(fixed)); w32(LC + 0x1c, LST); for (int j = 0; j < 8; ++j) w8(LST + 4 + u(j), u(ri(0, bq - 1)));
        for (int s = 0; s < 4; ++s) {
            const uint32_t SE = SETS + 0x100 * u(s); const int cnt = ri(1, 4);
            w32(HC + 0x48 + 4 * u(s), SE); w32(LC + 0x48 + 4 * u(s), SE); w8(SE + 0x10, u(cnt)); w8(SE + 0x11, R());
            for (int f = 0; f < 4; ++f) {  // all four frames valid: frame indexes up to 3 occur
                const uint32_t F = FRS + 0x400 * u(s * 4 + f); w32(SE + 0x1c + 4 * u(f), F);
                const int nr = ri(0, 4), np = ri(0, 4), nq = (fixed + nr + np + 1) / 2 + 1;
                wrand(F, 0x80); wf(F, rf(0.8f, 1.2f)); w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            }
        }
        for (int c = 0; c < 4; ++c) w32(OV(0x00197300) + 4 * u(c), LC);
        for (int i = 0; i < 3; ++i) w8(OV(0x00197bc0) + u(i), u(ri(0, 3)));
        for (int i = 0; i < 2; ++i) w32(OV(0x0015f6a0) + 4 * u(i), u(ri(0, 3)));
        wrand(HM, 0x40); w32(HM + 0x24, HC); w32(HM + 0x60, 0); w8(HM + 0x52, u(ri(0, 3))); w8(HM + 0x53, ri(0, 1) ? ee_read<uint8_t>(HM + 0x52) : u(ri(0, 3)));
        w8(HM + 0x50, u(ri(0, 3))); w8(HM + 0x51, u(ri(0, 3))); wf(HM + 0x54, rf(0, 1)); w8(HM + 0xa5, u(ri(0, 3)));
        w32(P + 0x2080, HM);
        for (int i = 0; i < 6; ++i) w8(POOL + 0x40 * u(i) + 4, u(ri(0, 1)));
        w8(POOL + 0x40 * u(ri(0, 3)) + 4, 0);  // the 6-slot pool never overflows in the game
        for (int i = 0; i < 2; ++i) {
            const uint32_t L = 0x00140050 + 4 * u(i);
            if (ri(0, 2) == 0) { w32(L, 0); continue; }
            const uint32_t e = POOL + 0x40 * u(4 + i); const uint32_t t = CHT + 0x100 + 0x20 * u(ri(0, 3));
            wrand(e, 0x10); w8(e + 4, 1); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * u(4 + i));
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, ee_read<uint32_t>(HM + 0x60)); w32(HM + 0x60, e);
            w8(e + 0x20, u(ri(0, 3))); w8(e + 0x21, u(ri(0, 3))); w8(e + 0x22, u(ri(0, 3))); w8(e + 0x23, u(ri(0, 3)));
            wf(e + 8, rf(0, 1)); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1)); w16(e + 0x30, u(ri(0, 1)));
            w8(e + 0x32, u(ri(0, 1))); w8(e + 0x33, u(ri(0, 1))); w32(e + 0x34, ri(0, 3) ? 0u : 1u);
            w32(L, e);
        }
        w8(P + 0x20ab, u(ri(0, 2))); w8(P + 0x20af, ri(0, 3) ? 0u : 1u); w32(P + 0xd08, u(ri(0, 1))); w32(P + 0xd14, u(ri(0, 1))); w32(P + 0xa9c, u(ri(0, 1)));
        wf(0x0013fdf8, rf(0, 50)); wf(0x0015ed60, rf(0.5f, 2)); wf(0x0015ed64, rf(0.5f, 2));
        for (int i = 0; i < 6; ++i) { w8(POOL + 0x40 * u(i) + 6, 0); w8(POOL + 0x40 * u(i) + 7, 0); }
        w32(P + 0xd08, ri(0, 2) ? 0u : POOL + 0x100); w32(P + 0xd0c, ri(0, 2) ? 0u : POOL + 0x140);
        w8(P + 0x20a4, ri(0, 7) ? 0u : 1u); w8(P + 0x20a8, ri(0, 7) ? 1u : 0u); w8(P + 0x20aa, ri(0, 7) ? 1u : 0u);
        w32(P + 0x10b4, 2); w32(P + 0x10b8, u(ri(0, 0x24)));
        for (int i = 0; i < 0x25; ++i) {
            const uint32_t e = OV(0x00179ac0) + 0x4c * u(i);
            w32(e + 0x18, u(ri(0, 1))); w32(e + 0x24, u(ri(0, 3))); w32(e + 0x28, ri(0, 5) ? u(ri(0, 3)) : 0xffffffffu); w32(e + 0x2c, u(ri(0, 3))); w32(e + 0x30, u(ri(0, 2)));
        }
        w32(P + 0x10b8, u(ri(-1, 0x24)));
        w32(P + 0x208c, ri(0, 2) ? 0u : (ri(0, 1) ? 0xcu : 3u)); w16(P + 0x22c8, u(ri(0, 3) ? 0 : 1));
        w32(HM + 0x54, 0); w32(HM + 0x60, 0); w32(HM + 0x64, 0); w32(HM + 0x68, FRS); w32(HM + 0x6c, FRS + 0x400);
        w32(P + 0x11a4, 1); w32(P + 0x2278, 0); w32(P + 0xa98, R());
        for (int sl = 0; sl < 2; ++sl) {   // layer records in pool slots 4 / 5
            const uint32_t e = POOL + 0x40 * u(4 + sl), t = CHT + 0x100;
            wrand(e, 0x10); w8(e + 4, 1); w8(e + 5, R()); w8(e + 6, 0); w8(e + 7, 0); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * u(4 + sl));
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, 0);
            for (uint32_t q = 0x20; q < 0x24; ++q) w8(e + q, u(ri(0, 3)));
            wf(e + 8, ri(0, 3) ? rf(0, 1) : 0.0f); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1));
            w32(P + 0xd08 + 4 * u(sl), ri(0, 3) ? e : 0u);
        }
        w32(P + 0x2298, ri(0, 1) ? ee_read<uint32_t>(P + 0x10b8) : 0x99u); w32(P + 0xd14, u(ri(0, 2)));
        w32(0x00140408, u(ri(0, 15))); for (int i = 0; i < 16; ++i) { w16(OV(0x001c40b0) + i * 0x18 + 0xe, coin() ? 0u : R()); w32(0x0013d428 + i * 4, R()); }
        if (ri(0, 2) == 0) w32(P + 0x208c, 0xf);
        w32(P + 0x5d8, ri(0, 1) ? M3 : 0u); wfrand(M3 + 0x10, 4, -20, 20); wfrand(P + 0x80, 4, -20, 20); wfrand(P + 0x90, 4, -3, 3);
        wf(P + 0x57c, rf(-3, 3)); wf(P + 0x5d0, rf(-3, 3)); wf(P + 0x580, rf(-3, 3)); wfrand(HM + 0xc0, 12, -1, 1);
        return Args{};
    }, [](A) { hero_hold_update(); return uint64_t{0}; });

    check("hero_look_fx", 0x0020b1d8, N * 4, Ret::None, [particles2] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000; (void)DSTM;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < 0x18; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
        (void)k; (void)IDS; (void)DSTM;
        particles2(); w32(OV(0x001b2108), T); w8(T, R()); wf(0x0015ed6c, rf(0.5f, 2));
        const uint32_t P = g::PLAYER;
        w32(g::HERO_STATE, coin() ? 0x31u : 0x10u); wfrand(P + 0x80, 4, -9, 9); wf(P + 0x2f0, rf(-9, 9));
        wfrand(P + 0x100, 4, -1, 1); w32(P + 0x2084, ri(0, 1) ? 0x34u : 3u); w32(P + 0x2080, M);
        w32(OV(0x001b20f0), T); w32(0x0015ed84, u(ri(1, 3)));
        w32(g::HERO_SUBSTATE, ri(0, 7) ? 3u : 0x11u); w32(g::HERO_STATE, 3);
        for (uint32_t o = 0x2c0; o <= 0x2d4; o += 4) wf(P + o, ri(0, 2) ? rf(-0.05f, 0.05f) : rf(-2, 2));
        w16(P + 0x308, u(ri(0, 1))); w32(P + 0x10e0, u(ri(0, 1))); w16(P + 0x12ae, ri(0, 5) ? (R() & 0xff7fu) : R());
        w32(P + 0x2084, ri(0, 1) ? 3u : 1u); w32(P + 0x208c, ri(0, 1) ? 4u : 2u); w32(P + 0x418, u(ri(-1, 12)));
        w32(P + 0x12a8, u(ri(-1, 3))); w16(P + 0x12ac, u(ri(0, 3))); wf(P + 0x12a0, rf(0, 0.2f)); wf(P + 0x12a4, rf(0, 0.1f));
        wfrand(M + 0xc0, 12, -1, 1);
        return Args{};
    }, [](A) { hero_look_fx(); return uint64_t{0}; });

    check("hero_gadget_glow", 0x002080b0, N * 6, Ret::None, [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(1, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000; (void)DSTM;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < 0x18; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
        (void)k; (void)IDS; (void)DSTM;
        const uint32_t P = g::PLAYER;
        w16(P + 0x1eb0, u(ri(0, 3))); w16(P + 0x1eb2, u(ri(0, 1))); w32(OV(0x0015f424), u(ri(0, 0x45)));
        w32(P + 0x1158, ri(0, 1) ? 5u : 4u); w32(P + 0x1130, ri(0, 3) ? M : 0u); w16(M + 0x34, R() & 0xffffu);
        wf(P + 0x1628, rf(-3, 3)); wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed60, rf(0.5f, 2)); w32(P + 0x2080, M3);
        w32(P + 0x1090, ri(0, 5) ? M : 0u); w32(P + 0x10b8, u(ri(0xc, 0x15))); wf(P + 0x10bc, rf(-3.1f, 3.1f));
        w32(P + 0x10b0, u(ri(0, 200))); w8(P + 0x20a8, u(ri(0, 1))); w32(M + 0x90, R());
        wfrand(M + 0xc0, 12, -1, 1);
        return Args{};
    }, [](A) { hero_gadget_glow(); return uint64_t{0}; });

    check("moby_update_transform_kept", 0x00250f98, N * 4, Ret::None, [gridenv] {
        const int k = gridenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k));
        w8(m + 0x20, ri(0, 6) ? u(ri(0, 3)) : 0xfeu); w16(m + 0x34, (ri(0, 3) ? 0u : 0x100u) | (ri(0, 3) ? 0u : 0x8000u) | (R() & 0x00ffu));
        w32(m + 0x24, T); wf(m + 0x2c, rf(0.5f, 2)); wfrand(m + 0x40, 4, -3, 3); wfrand(m + 0xc0, 12, -1, 1);
        for (int a = 0; a < 4; ++a) { const uint32_t h = T2 + 0x40 * u(a); w32(T + 0x48 + 4 * u(a), h); wfrand(h, 3, -1, 1); wf(h + 12, rf(500, 30000)); }
        const int cur = ri(0, 4) ? ri(0, 3) : 0xff, nxt = ri(0, 2) ? cur : ri(0, 3);
        w8(m + 0x52, u(cur)); w8(m + 0x53, u(nxt == 0xff ? 0 : nxt)); w8(m + 0x50, u(ri(0, 7))); wf(m + 0x54, rf(0, 1)); w8(m + 0x71, u(ri(0, 3)));
        wfrand(OV(0x00196d00), 32, -1, 1); for (int i = 0; i < 8; ++i) wf(OV(0x00196d00) + 16 * u(i) + 12, rf(500, 30000));
        wfrand(m + 0xf0, 3, -1, 1); wf(m + 0xfc, rf(500, 30000));
        wf(m + 0x10, rf(0, 80)); wf(m + 0x14, rf(0, 80)); wf(m + 0x18, rf(-50, 50)); wf(m + 0x1c, rf(-1, 1));
        w16(m + 0xa8, R()); w32(m + 0x94, u(ri(0, 3)));
        return Args{{m}, {}};
    }, [](A a) { moby_update_transform_kept(a.i[0]); return uint64_t{0}; });

    // ======== batch 42
    g_batch = 42;
    auto slotEnv = [heroSnd] {
        heroSnd();
        const uint32_t P = g::PLAYER;
        constexpr uint32_t CL = 0x01e90000, SET = 0x01e90400, MA = 0x01e91000, MB = 0x01e91200, MD = 0x01e92000;
        for (int i = 0; i < 8; ++i) w32(CL + 0x48 + 4 * u(i), SET + 0x100 * u(i));
        for (int i = 0; i < 8; ++i) { const uint32_t S = SET + 0x100 * u(i); w8(S + 0x10, u(ri(1, 8))); w8(S + 0x11, R()); w8(S + 0x12, R()); wrand(S + 0x1c, 8); }
        for (uint32_t m : {MA, MB}) {
            w32(m + 0x24, CL); w8(m + 0x52, ri(0, 3) ? u(ri(0, 3)) : 0xffu); w8(m + 0x50, u(ri(0, 5))); w8(m + 0x51, R()); w8(m + 0x53, u(ri(0, 3)));
            w32(m + 0x54, 0); w32(m + 0x60, 0); w32(m + 0x64, 0); w8(m + 0x70, R());
        }
        auto code = [] { static const uint32_t c[] = {0, 0, 0, 2, 4, 6, 8, 9, 0x17, 0x18, 0x1c, 0x1d, 0x1f, 0x24, 0x26}; return ri(0, 3) ? c[ri(0, 14)] : u(ri(0, 0x28)); };
        for (int i = 0; i < 7; ++i) {
            const uint32_t q = P + 0x50 * u(i);
            w32(q + 0x10b8, code()); w8(q + 0x10ac, u(ri(0, 3) ? ri(0, 2) : ri(0, 5))); w8(q + 0x10ad, u(ri(0, 4)));
            w16(q + 0x10a8, ri(0, 1) ? 0u : u(ri(0, 3))); w8(q + 0x10aa, R()); w32(q + 0x10b0, R()); w32(q + 0x10b4, 2);
            w32(q + 0x1090, ri(0, 2) ? 0u : (ri(0, 1) ? MA : MB)); w32(q + 0x1094, ri(0, 2) ? 0u : (ri(0, 1) ? MA : MB));
            w32(0x00141660 + 4 * u(i), code());
            w32(P + 0x20b8 + 4 * u(i), ri(0, 1) ? 0u : code()); w32(P + 0x20d4 + 4 * u(i), code());
            w32(P + 0x20f0 + 4 * u(i), code()); w32(P + 0x210c + 4 * u(i), u(ri(0, 1)));
        }
        w8(P + 0x20a6, u(ri(0, 1))); w8(P + 0x20a7, ri(0, 2) ? 0u : 1u); w8(P + 0x20ac, ri(0, 2) ? 0u : 1u); w8(P + 0x20a8, 0);
        w32(0x0013c940 + 0x1a4, R());
        static const uint32_t hs[] = {0x12, 0x76, 0x6a, 0x82, 8, 3, 0x66, 0x65, 0x67};
        static const uint32_t ss[] = {0xf, 0x11, 0x12, 0, 1, 2, 4, 3};
        w32(P + 0x2084, hs[ri(0, 8)]); w32(P + 0x208c, ss[ri(0, 7)]);
        w32(P + 0x1108, ri(0, 1) ? 0u : code()); w32(P + 0x1158, ri(0, 1) ? 6u : code()); w32(P + 0x11a8, ri(0, 1) ? 4u : u(ri(0, 5)));
        w32(P + 0x20f8, u(ri(0, 1))); w32(P + 0x20fc, u(ri(0, 1))); w8(P + 0x12e7, u(ri(0, 1))); w8(P + 0x22cb, ri(0, 2) ? 0u : 1u);
        w16(P + 0x22de, ri(0, 1) ? 0u : 5u); w8(P + 0x1ff5, ri(0, 4) ? 0u : 1u); w8(P + 0x12e4, u(ri(0, 1))); w8(P + 0x20a4, ri(0, 2) ? 0u : 1u);
        w16(OV(0x001c40b0) + 0x18 * 0x18 + 0xe, 1); w32(0x0013d428 + 0x18 * 4, 0);
        for (uint32_t a : {0x0013d4ddu, 0x0013d4dcu, 0x0013d4c6u, 0x0013d4c4u}) w8(a, u(ri(0, 1)));
        w32(OV(0x0015f5c4), ri(0, 3) ? 0u : 1u);
        w32(P + 0xd00, ri(0, 1) ? 0u : MD); w32(P + 0xd04, ri(0, 1) ? 0u : MD + 0x40);
        static const uint32_t s0[] = {8, 9, 0x17, 8};
        static const uint32_t r0[] = {0x18, 0x24, 0x1f, 0};
        if (ri(0, 1)) w32(P + 0x10b8, s0[ri(0, 3)]);
        if (ri(0, 1)) w32(P + 0x20b8, r0[ri(0, 3)]);
    };
    check("hero_slot_update", 0x0020fea0, N * 24, Ret::None, [slotEnv] {
        slotEnv();
        return Args{{ri(0, 1) ? 0u : u(ri(1, 6))}, {}};
    }, [](A a) { hero_slot_update(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });


    auto poseM = [] {
        constexpr uint32_t BIND = 0x01e80000, FA = 0x01e84000, FB = 0x01e86000, LAY = 0x01e88000, OVR = 0x01e8c000;
        constexpr uint32_t S = 0x70000000u;
        const int n = ri(12, 24);
        auto jt = [n] { return S + u(ri(0, n - 1)) * 64; };
        w32(M + 0x24, M2); w8(M2 + 8, u(n)); w32(M2 + 0x18, BIND);
        for (int k = 0; k < n; ++k) { for (int i = 0; i < 3; ++i) wf(BIND + 16 * u(k) + 4 * u(i), static_cast<float>(ri(-3, 3))); w32(BIND + 16 * u(k) + 12, ri(0, 3) ? S + u(ri(0, n - 1)) * 64 : 0u); }
        w32(M + 0x54, ri(0, 3) == 0 ? 0u : [] { const float f = ri(0, 3) ? rf(0, 1) : 1.0f; uint32_t b; std::memcpy(&b, &f, 4); return b; }());
        for (int i = 0; i < 4; ++i) w8(M + 0x50 + u(i), u(ri(0, 1)));
        for (uint32_t F : {FA, FB}) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1);
            std::vector<int> pr(static_cast<size_t>(n)), pp(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) pr[static_cast<size_t>(i)] = pp[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) { std::swap(pr[static_cast<size_t>(i)], pr[static_cast<size_t>(ri(0, i))]); std::swap(pp[static_cast<size_t>(i)], pp[static_cast<size_t>(ri(0, i))]); }
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        }
        w32(M + 0x68, FA); w32(M + 0x6c, ri(0, 3) ? FB : FA);
        uint32_t lay = 0, ovr = 0;
        if (ri(0, 2) == 0) {
            const int nl = ri(1, 2);
            for (int l = 0; l < nl; ++l) {
                const uint32_t L = LAY + 0x1000 * u(l), RC = L + 0x100; const int cnt = ri(1, 6);
                w16(L + 2, u(cnt)); w32(L + 0x10, RC); wf(L + 8, rf(0, 1)); w32(L + 0x1c, l + 1 < nl ? L + 0x1000 : 0u);
                for (int k = 0; k < cnt; ++k) { wfrand(RC + 0x30 * u(k), 4, -1, 1); wfrand(RC + 0x30 * u(k) + 0x10, 4, 0.5f, 2); w32(RC + 0x30 * u(k) + 0x1c, ri(0, 1) ? 0u : R()); wfrand(RC + 0x30 * u(k) + 0x20, 3, -50, 50); w32(RC + 0x30 * u(k) + 0x2c, jt()); }
            }
            lay = LAY;
        }
        if (ri(0, 2) == 0) {
            const int no = ri(1, 4);
            for (int o = 0; o < no; ++o) {
                const uint32_t O = OVR + 0x40 * u(o);
                w8(O + 3, u(ri(0, 1))); w32(O + 4, jt()); w32(O + 8, o + 1 < no ? O + 0x40 : 0u); wf(O + 0xc, rf(0, 1));
                wfrand(O + 0x10, 4, -1, 1); wfrand(O + 0x20, 4, 0.5f, 2); wfrand(O + 0x30, 4, -50, 50);
            }
            ovr = OVR;
        }
        w32(M + 0x60, lay); w32(M + 0x64, ovr);
        constexpr uint32_t CHT = 0x01e8e000, CHN = 0x01e8e100, IDS = 0x01e8e800, DSTM = 0x01e8f000; (void)DSTM;
        w32(M2 + 0x1c, CHT);
        for (int c = 0; c < 0x18; ++c) {
            const uint32_t C = CHN + 0x20 * u(c); const int len = ri(1, 4);
            w32(CHT + 4 + 4 * u(c), C); w16(C, u(len)); for (int j = 0; j < len; ++j) w8(C + 4 + u(j), u(ri(0, n - 1)));
        }
        const int k = ri(1, 4); for (int i = 0; i < k; ++i) w32(IDS + 4 * u(i), u(ri(0, 5)));
        wfrand(M + 0xc0, 16, -1, 1); wfrand(M + 0x10, 4, -50, 50); wf(M + 0x2c, rf(100, 2000));
        (void)k; (void)IDS; (void)DSTM;
    };
    check("hero_slot_mobys_update", 0x0020f580, N * 8, Ret::None, [poseM] {
        poseM();
        const uint32_t P = g::PLAYER;
        constexpr uint32_t FA = 0x01e84000, FB = 0x01e86000, SR = 0x01e8d000, G = 0x01e93000;
        constexpr uint32_t MC = 0x01e96000, CT = 0x01e96100, SRS = 0x01e96400, FS = 0x01e97000;
        auto mobyanim = [](uint32_t m, uint32_t sets) {
            const int cur = ri(0, 3);
            const int nfr = ee_read<uint8_t>(sets + 0x100 * u(cur) + 0x10);
            w8(m + 0x52, u(cur)); w8(m + 0x53, ri(0, 2) ? u(cur) : u(ri(0, 3)));
            const int f0 = ri(0, nfr - 1); w8(m + 0x50, u(f0)); w8(m + 0x51, u((f0 + 1) % nfr));
            wf(m + 0x54, rf(0, 0.999f)); wf(m + 0x58, rf(0.05f, 1.5f)); wf(m + 0x5c, rf(0.3f, 1.5f));
            w8(m + 0x7c, 0xff); w8(m + 0x7d, 0xff); w32(m + 0x94, 0); w8(m + 0x20, 0); w8(m + 0x71, R());
            wfrand(m + 0xf0, 3, -1, 1); wf(m + 0xfc, rf(500, 30000)); wf(m + 0x2c, rf(0.5f, 2)); wfrand(m + 0x40, 4, -3, 3);
            wfrand(m + 0x10, 4, -50, 50); wfrand(m + 0xc0, 12, -1, 1); w16(m + 0x34, R() & 0xffffu); wrand(m + 0x38, 2);
        };
        auto mksets = [](uint32_t cls, uint32_t sets, uint32_t f0, uint32_t f1) {
            for (int s = 0; s < 4; ++s) {
                const uint32_t a = sets + 0x100 * u(s); const int nfr = ri(1, 4);
                w32(cls + 0x48 + 4 * u(s), a); wfrand(a, 3, -1, 1); wf(a + 12, rf(500, 30000));
                w8(a + 0x10, u(nfr)); w8(a + 0x11, R()); w8(a + 0x12, R()); wf(a + 0x18, rf(0.3f, 1.5f));
                for (int i = 0; i < nfr; ++i) w32(a + 0x1c + 4 * u(i), ri(0, 1) ? f0 : f1);
                for (int e = 0; e < 6; ++e) w32(a + 0x1c + 4 * u(nfr) + 4 * u(e), 0);
            }
        };
        auto perm = [](int n) {
            std::vector<int> v(static_cast<size_t>(n)); for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = i;
            for (int i = n - 1; i > 0; --i) std::swap(v[static_cast<size_t>(i)], v[static_cast<size_t>(ri(0, i))]);
            return v;
        };
        // a key frame for an n-joint class: n rotation keys, then rotation/scale and translation records
        auto mkframe = [perm](uint32_t F, int n) {
            const int nr = ri(0, n), np = ri(0, n), nq = (n + nr + np + 1) / 2;
            w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            wrand(F + 0x10, nq * 4); w32(F + 0x10 + u(nq) * 16 - 16, R() | 1); w32(F + 0x10 + u(nq) * 16 - 4, R() | 1);
            const std::vector<int> pr = perm(n), pp = perm(n);
            for (int r = 0; r < nr + np; ++r) {
                const uint32_t a = F + 0x10 + u(n + r) * 8; const bool rot = r < nr;
                for (uint32_t i = 0; i < 3; ++i) w16(a + 2 * i, rot ? (ri(0, 2) ? 0x1000u : R() & 0xffff) : u(ri(-3, 3)));
                w8(a + 6, u(rot ? pr[static_cast<size_t>(r)] : pp[static_cast<size_t>(r - nr)]));
            }
        };
        mksets(M2, SR, FA, FB);
        { const uint32_t nb = ee_read<uint8_t>(M2 + 8); if (ee_read<uint64_t>(0x01e80000 + 16 * (nb - 1) + 8) == 0) w32(0x01e80000 + 16 * (nb - 1) + 12, 0x70000000u); }  // bind DMA sentinel
        for (uint32_t a = OV(0x0017c480); a < OV(0x0017ca80); a += 4) w32(a, R() | 1);  // stale key-frame buffers (DMA sentinel words nonzero)
        for (uint32_t o = 0; o < 0x100; o += 4) w32(M3 + o, ee_read<uint32_t>(M + o));  // the hero moby: a posed copy, not a slot moby
        // The slot moby has its own class whose joints are the keys the pose encoder writes for it
        // (identity + list joints + padding), as a weapon model has.
        const bool pad6 = ri(0, 1) != 0;
        const int len = pad6 ? 7 : ri(1, 4), ns = 1 + len;
        const std::vector<int> ids = perm(12);
        for (uint32_t lst : {OV(0x0017a5c0u), OV(0x0017a608u), OV(0x0017a650u), OV(0x0017a668u), OV(0x0015f6a8u)}) {  // joints of the hero class
            const int l = lst == OV(0x0017a608u) && pad6 ? 1 : (pad6 && (lst == OV(0x0017a650u) || lst == OV(0x0017a668u)) ? 4 : len);
            for (int i = 0; i < l; ++i) w32(lst + 4 + 4 * u(i), u(ids[static_cast<size_t>(i)]));
            w32(lst + 4 + 4 * u(l), 0xffffffffu);
        }
        for (uint32_t o = 0; o < 0x48; o += 4) w32(MC + o, ee_read<uint32_t>(M2 + o));
        w8(MC + 8, u(ns)); w32(MC + 0x1c, CT);
        for (int k = 0; k < ns; ++k)  // parents within the small class; bind DMA sentinel (last dword) nonzero
            w32(0x01e80000 + 16 * u(k) + 12, ri(0, 3) ? 0x70000000u + 64 * u(ri(0, ns - 1)) : 0u);
        if (ee_read<uint64_t>(0x01e80000 + 16 * u(ns - 1) + 8) == 0) w32(0x01e80000 + 16 * u(ns - 1) + 12, 0x70000000u);
        for (int c = 0; c < 0x18; ++c) {
            const uint32_t C = CT + 0x80 + 0x10 * u(c); const int cl = ri(1, std::min(4, ns)); const std::vector<int> js = perm(ns);
            w32(CT + 4 + 4 * u(c), C); w16(C, u(cl)); for (int j = 0; j < cl; ++j) w8(C + 4 + u(j), u(js[static_cast<size_t>(j)]));
        }
        mkframe(FS, ns); mkframe(FS + 0x400, ns);
        mksets(MC, SRS, FS, FS + 0x400);
        w32(M + 0x24, MC); w32(M + 0x68, FS); w32(M + 0x6c, FS + 0x400);
        w32(M + 0x60, 0); w32(M + 0x64, 0);  // slot mobys carry no anim layers / overrides (the hero copy keeps them)
        mobyanim(M, SRS); w16(M + 0xa6, pad6 ? 0x1b1u : R() & 0xffffu);
        mobyanim(G, SR); w32(G + 0x24, M2); w16(G + 0x34, ri(0, 1) ? 0u : (R() & 0xffffu));
        static const uint32_t kinds[] = {0x11, 0xa, 0x14, 0x19, 5, 6, 7, 0x1c, 0x1d, 1, 2, 3};
        for (int s = 0; s < 7; ++s) {
            const uint32_t q = P + 0x50 * u(s);
            w32(q + 0x1090, ri(0, 3) ? M : 0u); w32(q + 0x1094, ri(0, 3) ? M : 0u);
            uint32_t kd = ri(0, 2) ? kinds[ri(0, 11)] : u(ri(0, 0x26));
            if (pad6 && (kd == 0x1c || kd == 0x1d)) kd = 0x11;  // s3 lists are 4 joints long: not this class
            w32(q + 0x10b8, kd); w32(q + 0x10b4, ri(0, 4) ? 2u : 1u); w8(q + 0x10aa, ri(0, 3) ? 0u : 1u);
        }
        for (uint32_t kd = 0; kd < 0x28; ++kd) { w32(OV(0x00179ac0) + kd * 0x4c + 0xc, u(ri(0, 7))); w32(OV(0x00179ac0) + kd * 0x4c + 0x18, ri(0, 2) ? 0u : 1u); }
        for (int i = 0; i < 8; ++i) {
            Mat4 r; Vec4 e{rf(-3, 3), rf(-1.5f, 1.5f), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e);
            ee_ref<Mat4>(P + 0xac0 + 64 * u(i)) = r; wfrand(P + 0xaf0 + 64 * u(i), 3, -50, 50); wf(P + 0xafc + 64 * u(i), 1.0f);
        }
        static const uint32_t modes[] = {0, 1, 2, 6};
        w32(OV(0x0015f5c4), modes[ri(0, 3)]); w32(OV(0x00166c00u - 0x7570), u(ri(0, 1))); w8(0x0015edb5, u(ri(0, 1)));
        // vec3_cross_ba leaves row 2's w with the stale VU vf3.w (port: 0), and the glow moby's matrix
        // picks that lane up, so the cross product and the glow update are kept apart.
        if (ee_read<uint8_t>(0x0015edb5) != 0) w32(P + 3 * 0x50 + 0x1094, 0);
        w32(OV(0x0015f5cc), u(ri(0, 10000)));
        w32(P + 0x2080, M3); wfrand(P + 0x80, 4, -50, 50); wfrand(P + 0x90, 4, -3, 3);
        w16(P + 0x1eb2, u(ri(0, 1))); w32(OV(0x0015f424), u(ri(0, 0x45)));
        for (int k = 0; k < 16; ++k) {
            Mat4 r; Vec4 e{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e);
            for (int row = 0; row < 3; ++row) ee_ref<Vec4>(P + 0x12f0 + 0x30 * u(k) + 16 * u(row)) = r.r[row];
        }
        w32(P + 0x21b8, u(ri(0, 15))); w32(P + 0x21bc, u(ri(0, 16))); w32(P + 0x21c0, u(ri(0, 16)));
        w32(P + 0x208c, ri(0, 3) ? 0u : 0x16u); w32(g::HERO_STATE, u(ri(1, 3)));
        w32(P + 0x118c, ri(0, 2) ? G : 0u);
        constexpr uint32_t MB = 0x01e60000;
        w32(OV(0x0015ffdc), MB); w32(OV(0x0015ffe0), MB + 0x100 * u(ri(0, 4))); w32(OV(0x0015ffd8), MB); w32(OV(0x0015ffe8), T4); w32(OV(0x0015ffbc), u(ri(0, 2)));
        for (int i = 0; i < 4; ++i) { const uint32_t m = MB + 0x100 * u(i); wrand(m, 0x40); w8(m + 0x20, ri(0, 2) ? 0xfeu : 3u); w32(m + 0x38, 0); w32(m + 0x3c, 0); }
        w8(OV(0x00197bc0) + 0x4b4, 2); w32(OV(0x00197300) + 8, M2); w32(OV(0x00197680) + 8, 0);  // spawned glow moby gets class M2
        w32(M2 + 0x10, 0); wf(M2 + 0x24, 1.0f); w32(M2 + 0x40, 0); w8(M2 + 6, 0); w8(M2 + 0xc, 0); w8(M2 + 0xe, 0); w8(M2 + 0xf, 0); w16(M2 + 0x44, 0);
        return Args{};
    }, [](A) { hero_slot_mobys_update(); return uint64_t{0}; }, M + 0xec);  // w of vec3_cross_ba: stale VU vf3.w

    auto holdEnv = [] {
        constexpr uint32_t HC = 0x01e70000, BIND = 0x01e70100, CHT = 0x01e70800, SETS = 0x01e71000, FRS = 0x01e72000;
        static const uint32_t LC = 0x01e7f000, LST = 0x01e7f100, HM = 0x01e7f800, POOL = OV(0x0018eb40);
        const uint32_t P = g::PLAYER;
        const int bq = ri(4, 12);
        w8(HC + 8, u(bq)); w32(HC + 0x18, BIND); wrand(BIND, bq * 4); w32(BIND + u(bq) * 16 - 4, R() | 1); w32(HC + 0x1c, CHT);
        for (int c = 0; c < 4; ++c) {
            const uint32_t t = CHT + 0x100 + 0x20 * u(c); const int cnt = ri(1, bq);
            w32(CHT + 4 + 4 * u(c), t); w8(t, 0); w8(t + 2, u(cnt));
            std::vector<int> js(static_cast<size_t>(bq)); for (int i = 0; i < bq; ++i) js[static_cast<size_t>(i)] = i;
            for (int i = bq - 1; i > 0; --i) std::swap(js[static_cast<size_t>(i)], js[static_cast<size_t>(ri(0, i))]);
            for (int j = 0; j < cnt; ++j) w8(t + 4 + u(j), u(js[static_cast<size_t>(j)]));
        }
        const int fixed = ri(0, 3);
        w8(LC + 8, u(fixed)); w32(LC + 0x1c, LST); for (int j = 0; j < 8; ++j) w8(LST + 4 + u(j), u(ri(0, bq - 1)));
        for (int s = 0; s < 4; ++s) {
            const uint32_t SE = SETS + 0x100 * u(s); const int cnt = ri(1, 4);
            w32(HC + 0x48 + 4 * u(s), SE); w32(LC + 0x48 + 4 * u(s), SE); w8(SE + 0x10, u(cnt)); w8(SE + 0x11, R());
            for (int f = 0; f < 4; ++f) {  // all four frames valid: frame indexes up to 3 occur
                const uint32_t F = FRS + 0x400 * u(s * 4 + f); w32(SE + 0x1c + 4 * u(f), F);
                const int nr = ri(0, 4), np = ri(0, 4), nq = (fixed + nr + np + 1) / 2 + 1;
                wrand(F, 0x80); wf(F, rf(0.8f, 1.2f)); w16(F + 6, u(nq)); w16(F + 0xa, u(nr)); w16(F + 0xe, u(np));
            }
        }
        for (int c = 0; c < 4; ++c) w32(OV(0x00197300) + 4 * u(c), LC);
        for (int i = 0; i < 3; ++i) w8(OV(0x00197bc0) + u(i), u(ri(0, 3)));
        for (int i = 0; i < 2; ++i) w32(OV(0x0015f6a0) + 4 * u(i), u(ri(0, 3)));
        wrand(HM, 0x40); w32(HM + 0x24, HC); w32(HM + 0x60, 0); w8(HM + 0x52, u(ri(0, 3))); w8(HM + 0x53, ri(0, 1) ? ee_read<uint8_t>(HM + 0x52) : u(ri(0, 3)));
        w8(HM + 0x50, u(ri(0, 3))); w8(HM + 0x51, u(ri(0, 3))); wf(HM + 0x54, rf(0, 1)); w8(HM + 0xa5, u(ri(0, 3)));
        w32(P + 0x2080, HM);
        for (int i = 0; i < 6; ++i) w8(POOL + 0x40 * u(i) + 4, u(ri(0, 1)));
        w8(POOL + 0x40 * u(ri(0, 3)) + 4, 0);  // the 6-slot pool never overflows in the game
        for (int i = 0; i < 2; ++i) {
            const uint32_t L = 0x00140050 + 4 * u(i);
            if (ri(0, 2) == 0) { w32(L, 0); continue; }
            const uint32_t e = POOL + 0x40 * u(4 + i); const uint32_t t = CHT + 0x100 + 0x20 * u(ri(0, 3));
            wrand(e, 0x10); w8(e + 4, 1); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * u(4 + i));
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, ee_read<uint32_t>(HM + 0x60)); w32(HM + 0x60, e);
            w8(e + 0x20, u(ri(0, 3))); w8(e + 0x21, u(ri(0, 3))); w8(e + 0x22, u(ri(0, 3))); w8(e + 0x23, u(ri(0, 3)));
            wf(e + 8, rf(0, 1)); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1)); w16(e + 0x30, u(ri(0, 1)));
            w8(e + 0x32, u(ri(0, 1))); w8(e + 0x33, u(ri(0, 1))); w32(e + 0x34, ri(0, 3) ? 0u : 1u);
            w32(L, e);
        }
        w8(P + 0x20ab, u(ri(0, 2))); w8(P + 0x20af, ri(0, 3) ? 0u : 1u); w32(P + 0xd08, u(ri(0, 1))); w32(P + 0xd14, u(ri(0, 1))); w32(P + 0xa9c, u(ri(0, 1)));
        wf(0x0013fdf8, rf(0, 50)); wf(0x0015ed60, rf(0.5f, 2)); wf(0x0015ed64, rf(0.5f, 2));
        for (int i = 0; i < 6; ++i) { w8(POOL + 0x40 * u(i) + 6, 0); w8(POOL + 0x40 * u(i) + 7, 0); }
        w32(P + 0xd08, ri(0, 2) ? 0u : POOL + 0x100); w32(P + 0xd0c, ri(0, 2) ? 0u : POOL + 0x140);
        w8(P + 0x20a4, ri(0, 7) ? 0u : 1u); w8(P + 0x20a8, ri(0, 7) ? 1u : 0u); w8(P + 0x20aa, ri(0, 7) ? 1u : 0u);
        w32(P + 0x10b4, 2); w32(P + 0x10b8, u(ri(0, 0x24)));
        for (int i = 0; i < 0x25; ++i) {
            const uint32_t e = OV(0x00179ac0) + 0x4c * u(i);
            w32(e + 0x18, u(ri(0, 1))); w32(e + 0x24, u(ri(0, 3))); w32(e + 0x28, ri(0, 5) ? u(ri(0, 3)) : 0xffffffffu); w32(e + 0x2c, u(ri(0, 3))); w32(e + 0x30, u(ri(0, 2)));
        }
        w32(P + 0x10b8, u(ri(-1, 0x24)));
        w32(P + 0x208c, ri(0, 2) ? 0u : (ri(0, 1) ? 0xcu : 3u)); w16(P + 0x22c8, u(ri(0, 3) ? 0 : 1));
        w32(HM + 0x54, 0); w32(HM + 0x60, 0); w32(HM + 0x64, 0); w32(HM + 0x68, FRS); w32(HM + 0x6c, FRS + 0x400);
        w32(P + 0x11a4, 1); w32(P + 0x2278, 0); w32(P + 0xa98, R());
        for (int sl = 0; sl < 2; ++sl) {   // layer records in pool slots 4 / 5
            const uint32_t e = POOL + 0x40 * u(4 + sl), t = CHT + 0x100;
            wrand(e, 0x10); w8(e + 4, 1); w8(e + 5, R()); w8(e + 6, 0); w8(e + 7, 0); w16(e + 2, ee_read<uint8_t>(t + 2)); w32(e + 0xc, t + 4); w32(e + 0x10, OV(0x00186d40) + 0x1500 * u(4 + sl));
            w32(e + 0x14, HC); w32(e + 0x18, LC); w32(e + 0x1c, 0);
            for (uint32_t q = 0x20; q < 0x24; ++q) w8(e + q, u(ri(0, 3)));
            wf(e + 8, ri(0, 3) ? rf(0, 1) : 0.0f); wf(e + 0x24, rf(0, 1)); wf(e + 0x28, rf(0, 1)); wf(e + 0x2c, rf(0.1f, 1));
            w32(P + 0xd08 + 4 * u(sl), ri(0, 3) ? e : 0u);
        }
        w32(P + 0x2298, ri(0, 1) ? ee_read<uint32_t>(P + 0x10b8) : 0x99u); w32(P + 0xd14, u(ri(0, 2)));
        w32(0x00140408, u(ri(0, 15))); for (int i = 0; i < 16; ++i) { w16(OV(0x001c40b0) + i * 0x18 + 0xe, coin() ? 0u : R()); w32(0x0013d428 + i * 4, R()); }
        if (ri(0, 2) == 0) w32(P + 0x208c, 0xf);
        w32(P + 0x5d8, ri(0, 1) ? M3 : 0u); wfrand(M3 + 0x10, 4, -20, 20); wfrand(P + 0x80, 4, -20, 20); wfrand(P + 0x90, 4, -3, 3);
        wf(P + 0x57c, rf(-3, 3)); wf(P + 0x5d0, rf(-3, 3)); wf(P + 0x580, rf(-3, 3)); wfrand(HM + 0xc0, 12, -1, 1);
    };
    check("hero_layers_tick", 0x0020ea50, N * 4, Ret::None, [holdEnv] {
        holdEnv();
        constexpr uint32_t HC = 0x01e70000, CHT = 0x01e70800, SETS = 0x01e71000;
        constexpr uint32_t TM = 0x01e7fc00, PVT = 0x01e7fd00, PVS = 0x01e7fe00;
        const uint32_t P = g::PLAYER;
        // The fixtures of the three updates share the pool slots and the hero moby, so one of them
        // does the real work per case and the others take their idle paths.
        const int busy = ri(0, 2);
        if (busy != 0) { w32(0x00140050, 0); w32(0x00140054, 0); }
        if (busy != 1) { w32(P + 0xd08, 0); w32(P + 0xd0c, 0); }
        w32(P + 0xd10, 0);
        for (uint32_t i = 0; i < 4; ++i)  // several layers may start in one tick: free pool slots for each
            if (ee_read<uint32_t>(0x00140050) != OV(0x0018eb40) + 0x40 * i && ee_read<uint32_t>(0x00140054) != OV(0x0018eb40) + 0x40 * i) w8(OV(0x0018eb40) + 0x40 * i + 4, 0);
        w32(CHT + 4 + 4 * 0x18, CHT + 0x100);  // the extra layer uses chain 0x18
        for (int k = 4; k < 0x84; ++k) w32(HC + 0x48 + 4 * u(k), SETS + 0x100 * u(k & 3));
        w32(P + 0x2274, busy == 2 && ri(0, 3) ? TM : 0u); wrand(TM, 0x40); w32(TM + 0x78, PVT); w32(PVT, ri(0, 3) ? PVS : 0u); w8(PVS + 0x3c, u(ri(0, 5)));
        w16(P + 0x1dc, ri(0, 3) ? 0u : 1u); w16(P + 0x22ce, u(ri(-2, 6))); w32(P + 0x22a8, u(ri(0, 2)));
        return Args{};
    }, [](A) { hero_layers_tick(); return uint64_t{0}; });

    auto slotsEnv = [slotEnv] {
        slotEnv();
        const uint32_t P = g::PLAYER;
        constexpr uint32_t SET = 0x01e90400, MA = 0x01e91000, MB = 0x01e91200;
        for (int i = 0; i < 8; ++i) { wfrand(SET + 0x100 * u(i), 3, -1, 1); wf(SET + 0x100 * u(i) + 12, rf(500, 30000)); }
        ovl_register_moby_fn(0x00250df8, moby_update_transform);  // a ported moby function as the slot mobys' +0x74
        for (uint32_t m : {MA, MB}) {
            w8(m + 0x20, ri(0, 4) ? 0u : (ri(0, 1) ? 0xfeu : 0xfdu)); w8(m + 0x70, R()); w8(m + 0x53, ri(0, 1) ? 0u : u(ri(1, 3)));
            w32(m + 0x74, ri(0, 2) ? OVF(0x00250df8u) : 0u); w32(m + 0x94, 0); w32(m + 0xa0, 0xffffffffu); wf(m + 0x2c, rf(0.5f, 2));
            wfrand(m + 0x10, 4, -50, 50); wfrand(m + 0x40, 4, -3, 3); w16(m + 0x34, R() & 0xffffu); w8(m + 0x71, R()); wfrand(m + 0xf0, 4, -1, 1);
        }
        for (int k = 0; k < 7; ++k) {
            const uint32_t q = P + 0x50 * u(k);
            static const uint32_t sts[] = {0, 1, 2, 6};
            w32(q + 0x10b4, sts[ri(0, 4)]); w32(q + 0x10b0, R()); w8(q + 0x10ab, R()); w32(q + 0x10a4, R());
        }
        w32(OV(0x00166c00u - 0x7808), u(ri(0, 30))); w32(OV(0x0015f3f8), u(ri(0, 30))); w32(OV(0x0015ffdc), ri(0, 1) ? MA : MB + 0x100); w32(OV(0x0015f5cc), R());
        for (int i = 0; i < 3; ++i) w16(P + 0x1990 + 0xb0 * u(i), 0xffffu);
        if (ri(0, 2) == 0) {   // an active bone node (bone_node_update), not linked to the hero moby
            constexpr uint32_t HB = 0x01e95000, CB = 0x01e95100, CH = 0x01e95200, CR = 0x01e95300;
            const uint32_t n = P + 0x18f0 + 0xb0 * u(ri(0, 2));
            wrand(n, 0x30);
            w16(n + 0xa0, u(ri(0, 3))); w8(0x001413f4, u(ri(0, 1))); w32(g::HERO_MOBY, HB); w32(0x001404d4, HB);
            // Kind 5 nodes attach to the slot-0 moby (its overrides +0x64): give slot 0 its own moby MB and
            // no anim restart on it afterwards (that would capture a pose these slot mobys don't have).
            for (int k = 1; k < 7; ++k)
                for (uint32_t o : {0x1090u, 0x1094u}) if (ee_read<uint32_t>(P + 0x50 * u(k) + o) == MB) w32(P + 0x50 * u(k) + o, MA);
            w32(0x001403e0, MB); w32(P + 0x1094, ee_read<uint32_t>(P + 0x1094) == MB ? MA : ee_read<uint32_t>(P + 0x1094));
            w8(MB + 0x70, ee_read<uint8_t>(MB + 0x70) & 0xfdu); w32(MB + 0x54, 0); w32(MB + 0x60, 0); w32(MB + 0x64, 0);
            for (int i = 0; i < 3; ++i) {
                wf(n + 0x40 + 4 * u(i), rf(-3, 3)); wf(n + 0x50 + 4 * u(i), rf(-1, 1)); wf(n + 0x60 + 4 * u(i), rf(-3, 3));
                wf(n + 0x70 + 4 * u(i), rf(-1, 1)); wf(n + 0x80 + 4 * u(i), rf(-1, 1)); wf(n + 0x90 + 4 * u(i), rf(-1, 1));
            }
            wf(n + 0xa4, rf(0, 1)); wf(n + 0xa8, rf(0, 1)); wf(n + 0xac, ri(0, 3) ? 1.0f : rf(0.5f, 2));
            wf(0x0015ed64, rf(0.5f, 2));
            w8(n + 1, 0); w32(n + 8, 0); w32(HB + 0x64, 0); w32(HB + 0x24, CB); w32(CB + 0x1c, CH); w32(0x01e90000 + 0x1c, CH);
            for (int i = 0; i < 4; ++i) { w32(CH + 4 * u(i) + 4, CR + 0x20 * u(i)); w8(CR + 0x20 * u(i), u(ri(0, 4))); for (int k = 1; k < 12; ++k) w8(CR + 0x20 * u(i) + u(k), R()); }
        }
    };
    check("hero_slot_close", 0x0020fca8, N * 4, Ret::None, [slotsEnv] {
        slotsEnv();
        return Args{{u(ri(0, 6)), R()}, {}};
    }, [](A a) { hero_slot_close(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("hero_slots_update", 0x00210748, N * 8, Ret::None, [slotsEnv] {
        slotsEnv();
        return Args{};
    }, [](A) { hero_slots_update(); return uint64_t{0}; });

    // ======== batch 43
    g_batch = 43;
    // mobys in the grid (gridenv), each with collision data: vertices / triangles / primitive records,
    // joint positions from the 8-entry cache ring (no pose evaluation here)
    auto collenv = [gridenv] {
        const int k = gridenv();
        static const uint32_t OUTB = OV(0x00173e40), RING = 0x01e9c000, BUF = 0x01ea0000, CD = 0x01eb0000;
        w32(OV(0x00166c00u - 0x6c28), T4 - 0x10000);  // moby id i -> T4 + (i - 0x100) * 0x100
        w32(OUTB + 0x10, ri(0, 15) ? u(ri(0, 50)) : 0xffffffffu); w32(OUTB + 4, BUF); w32(OUTB + 8, RING);
        w32(OUTB + 0xc, u(ri(0, 7))); w32(OUTB + 0x14, u(ri(0, 63)));
        wrand(RING, 16); wfrand(BUF, 0x1000, -1500, 1500);  // joint positions in 1024ths
        for (int c = 0; c < 64; ++c) {  // contact records
            const uint32_t o = OV(0x00178100) + 0x40 * u(c);
            w32(o + 0x2c, u(ri(0, 3))); w32(o + 0x34, ri(0, 1) ? T4 + 0x100 * u(ri(0, 47)) : R());
        }
        int cached = 0;  // every moby with joints is in the ring
        for (int i = 0; i < 48; ++i) {
            const uint32_t m = T4 + 0x100 * u(i);
            const uint32_t rect = ee_read<uint32_t>(m + 0xa0);
            const float x0 = static_cast<float>(rect & 0xff) * 16, y0 = static_cast<float>((rect >> 8) & 0xff) * 16;
            const float w = static_cast<float>(((rect >> 16) & 0xff) - (rect & 0xff) + 1) * 16;
            const float mx = (rect & 0x80) ? rf(5, 60) : x0 + rf(0, std::max(w, 1.0f)), my = (rect & 0x80) ? rf(5, 60) : y0 + rf(0, 16);
            wf(m + 0x10, mx); wf(m + 0x14, my); wf(m + 0x18, rf(5, 60)); wf(m + 0x1c, 1.0f);
            wf(m + 0, mx * 1024 + rf(-500, 500)); wf(m + 4, my * 1024 + rf(-500, 500)); wf(m + 8, ee_read<float>(m + 0x18) * 1024 + rf(-500, 500)); wf(m + 0xc, rf(2000, 8000));
            Mat4 rr; Vec4 e{rf(-3, 3), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(rr, e);
            ee_ref<Vec4>(m + 0xc0) = rr.r[0]; ee_ref<Vec4>(m + 0xd0) = rr.r[1]; ee_ref<Vec4>(m + 0xe0) = rr.r[2];
            wf(m + 0x2c, rf(0.5f, 2)); w32(m + 0x98, ri(0, 2) ? 0u : R()); w32(m + 0x9c, u(ri(0, 52)));
            w32(m + 0xa8, R()); w8(m + 0xa4, ri(0, 1) ? 0xffu : u(ri(0, 63))); w16(m + 0x34, R() & 0xffffu);
            if (ee_read<uint8_t>(m + 0xa4) < 0x40 && ri(0, 1)) w32(OV(0x00178100) + 0x40 * u(ee_read<uint8_t>(m + 0xa4)) + 0x34, m);  // its own record
            // collision data
            const uint32_t col = CD + 0x400 * u(i);
            w32(m + 0x94, ri(0, 5) ? col : 0u);
            const int nprim = ri(0, 4), npair = ri(0, 3) ? ri(1, 8) : 0, ntri = npair ? ri(0, 30) : 0;
            const int nj = (ri(0, 2) || cached >= 8) ? 0 : ri(1, 12);
            w16(col, ri(0, 1) ? u(nj) : 0u); w16(col + 2, u(nj));
            if (nj != 0) {  // cached joint positions
                w32(RING + 8 * u(cached++), ee_read<uint32_t>(m + 0xa8));
            }
            w32(col + 4, u(nprim * 0x20)); w32(col + 8, u(ntri * 4)); w32(col + 0xc, u(npair * 0x10));
            for (int r = 0; r < nprim; ++r) {
                const uint32_t pr = col + 0x10 + 0x20 * u(r);
                static const uint8_t types[] = {1, 2, 3, 4, 5, 0};
                const uint8_t ty = types[ri(0, 5)];
                w8(pr, ty); w16(pr + 2, (r == nprim - 1 ? 0x8000u : 0u) | u(ri(0, 3)));
                if (ty == 2) w32(pr + 4, u(ri(0, 15)));
                else if (ty == 3) wf(pr + 4, ri(0, 1) ? rf(0, 3000) : rf(0, 300));
                else { w16(pr + 4, u(ri(0, 15))); w16(pr + 6, u(ri(0, 15))); }
                wf(pr + 8, rf(-1, 1)); wf(pr + 0xc, ri(0, 1) ? rf(50, 1500) : rf(500, 3000)); wfrand(pr + 0x10, 3, -2000, 2000); wf(pr + 0x1c, ri(0, 1) ? rf(50, 1500) : rf(500, 3000));
            }
            strip_jointless_sets(col);
            const uint32_t vb = col + 0x10 + u(nprim * 0x20);
            for (int v = 0; v < npair * 2; ++v) { for (int c = 0; c < 3; ++c) w16(vb + 8 * u(v) + 2 * u(c), u(ri(-1000, 1000))); w16(vb + 8 * u(v) + 6, R()); }
            const uint32_t tb = vb + u(npair * 0x10);
            for (int tr = 0; tr < ntri; ++tr) {
                for (int c = 0; c < 3; ++c) w8(tb + 4 * u(tr) + u(c), u(ri(0, npair * 2 - 1)));
                w8(tb + 4 * u(tr) + 3, ri(0, 2) ? u(ri(0, 0x1f)) : R());
            }
        }
        return k;
    };
    {
        // (stage 5, task C2a) the port walks the grid's cells directly and keeps the vertices on the stack, the original lists
        // the cells at 0x70003800 and the vertices at 0x70002000: those scratch words are left out of this check (A2's
        // ranges). The ranges also hide what moby_pose_matrices (the posed case below) writes inside them: the joint
        // positions at 0x70000000 .. (read back by the primitive tests, so a wrong one still shows in the hit list) and its
        // chunk transfers at +0x2000 / +0x3000; its saved registers at 0x70003700 .. 0x7000372c are outside them, compared.
        const CollisionScratchIgnored spr_ignored;
        check("mobys_in_sphere", 0x001f2868, N * 48, Ret::Int32, [collenv, poseM] {
            const int k = collenv();
            uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
            for (int tries = 0; tries < 8 && (ee_read<uint32_t>(m + 0xa0) & 0x80) != 0; ++tries) m = T4 + 0x100 * u(ri(0, k - 1));  // one in the grid
            if (ri(0, 2) == 0 && ee_read<uint32_t>(m + 0x94) != 0) {
                // a posed moby not in the cache: its joints come from moby_pose_matrices (poseM's moby and class)
                poseM();
                const uint32_t nb = ee_read<uint8_t>(M2 + 8);
                for (uint32_t j = 0; j < nb; ++j) w32(0x01e80000 + 16 * j + 12, 0x70000000u + u(ri(0, static_cast<int>(j))) * 64);  // parents: itself or earlier (nonzero: DMA sentinel too)
                w32(m + 0x24, M2);
                for (uint32_t o = 0x50; o < 0x70; o += 4) w32(m + o, ee_read<uint32_t>(M + o));
                w32(m + 0x60, 0); w32(m + 0x64, 0);
                const uint32_t col = ee_read<uint32_t>(m + 0x94);
                w16(col, ri(0, 1) ? nb : 0u); w16(col + 2, nb); strip_jointless_sets(col);
                for (uint32_t e = 0; e < 8; ++e) if (ee_read<uint32_t>(0x01e9c000 + 8 * e) == ee_read<uint32_t>(m + 0xa8)) w32(0x01e9c000 + 8 * e, 0);
                for (int i = 0; i < 48; ++i) {  // the ring slot it takes may evict another moby's joints: no other moby has any
                    const uint32_t c2 = ee_read<uint32_t>(T4 + 0x100 * u(i) + 0x94);
                    if (T4 + 0x100 * u(i) != m && c2 != 0) { w16(c2, 0); w16(c2 + 2, 0); strip_jointless_sets(c2); }
                }
            }
            const float r = rf(0.3f, 3);
            wf(OUT, std::max(ee_read<float>(m + 0x10) + rf(-1.5f, 1.5f), r + 0.01f)); wf(OUT + 4, std::max(ee_read<float>(m + 0x14) + rf(-1.5f, 1.5f), r + 0.01f));
            wf(OUT + 8, ee_read<float>(m + 0x18) + rf(-1.5f, 1.5f)); wf(OUT + 12, rf(-9, 9));
            float rad = ri(0, 20) ? r : 0.0f;
            uint32_t ign = ri(0, 3) ? 0u : m;
            const uint32_t col = ee_read<uint32_t>(m + 0x94);
            if (col != 0 && ee_read<uint32_t>(col + 8) != 0 && ri(0, 1)) {
                // borderline: a sphere just reaching (or missing) a point near an edge of one of its triangles
                const uint32_t vb = col + 0x10 + ee_read<uint32_t>(col + 4), tb = vb + ee_read<uint32_t>(col + 0xc);
                const uint32_t tri = tb + 4 * u(ri(0, static_cast<int>(ee_read<uint32_t>(col + 8) / 4) - 1));
                const float sc = ee_read<float>(m + 0x2c);
                auto world = [&](uint32_t vi) {
                    const uint32_t va = vb + 8 * vi; float w[3];
                    for (int c = 0; c < 3; ++c) {
                        w[c] = ee_read<float>(m + 0x10 + 4 * u(c));
                        for (int j = 0; j < 3; ++j) w[c] += ee_read<float>(m + 0xc0 + 0x10 * u(j) + 4 * u(c)) * static_cast<float>(ee_read<int16_t>(va + 2 * u(j))) * sc / 1024.0f;
                    }
                    return std::array<float, 3>{w[0], w[1], w[2]};
                };
                const auto A = world(ee_read<uint8_t>(tri)), B = world(ee_read<uint8_t>(tri + 1 + u(ri(0, 1))));
                const float t = rf(-0.2f, 1.2f), L = rf(0.05f, 1.5f);
                float P[3], len2 = 0; float dir[3];
                for (int c = 0; c < 3; ++c) { dir[c] = rf(-1, 1); len2 += dir[c] * dir[c]; }
                for (int c = 0; c < 3; ++c) P[c] = A[c] + t * (B[c] - A[c]) + dir[c] / std::sqrt(len2 + 1e-6f) * L;
                rad = L * rf(0.97f, 1.03f);
                if (P[0] > rad && P[1] > rad && P[2] > rad && P[0] + rad < 1000 && P[1] + rad < 1000) {
                    wf(OUT, P[0]); wf(OUT + 4, P[1]); wf(OUT + 8, P[2]); ign = 0;
                    w32(m + 0x9c, 0xffffu);
                }
            }
            if (ri(0, 15) == 0) wf(OUT, ri(0, 1) ? rf(-1, 0.1f) : rf(1020, 1030));
            wrand(OUT + 0x40, 9); w32(OUT + 0x40 + 0x1c, u(ri(0, 3)));
            const uint32_t fl = (ri(0, 1) ? 4u : 0u) | (ri(0, 2) ? 0u : 0x10u) | (ri(0, 2) ? 0u : 0x20u) | (ri(0, 2) ? 0u : 0x80u) | (u(ri(0, 0x1f)) << 8);
            return Args{{OUT, fl, ign, ri(0, 1) ? OUT + 0x40 : 0u}, {rad}};
        }, [](A a) { return ri32(mobys_in_sphere(a.i[0], a.f[0], a.i[1], a.i[2], a.i[3])); });
    }

    // the level collision tree around (x, y, z) (units): a 4x4x4 block of 4-unit cells, chunks of packed
    // vertices (x, y 10 bits << 6, z 12 bits << 4, relative to the cell centre), triangles and quads
    auto treeenv = [](float x, float y, float z) {
        constexpr uint32_t TR = 0x01ec0000;
        w32(OV(0x00173e40), TR);
        const int32_t cx0 = static_cast<int32_t>(x / 4) - 1 - ri(0, 1), cy0 = static_cast<int32_t>(y / 4) - 1 - ri(0, 1), cz0 = static_cast<int32_t>(z / 4) - 1 - ri(0, 1);
        uint32_t data = TR + 0x400;
        w16(TR, u(cz0)); w16(TR + 2, u(ri(0, 3) ? 4 : ri(1, 3)));
        for (int k = 0; k < 4; ++k) {
            const uint32_t yn = TR + 0x40 + 0x80 * u(k);
            w16(TR + 4 + 2 * u(k), ri(0, 7) ? (yn - TR) >> 2 : 0u);
            w16(yn, u(cy0)); w16(yn + 2, u(ri(0, 3) ? 4 : ri(1, 3)));
            for (int j = 0; j < 4; ++j) {
                const uint32_t xn = TR + 0x240 + 0x80 * u(k * 4 + j) - 0x200 + 0x200;
                const uint32_t xnode = TR + 0x2000 + 0x20 * u(k * 4 + j);
                (void)xn;
                w32(yn + 4 + 4 * u(j), ri(0, 7) ? xnode - TR : 0u);
                w16(xnode, u(cx0)); w16(xnode + 2, u(ri(0, 3) ? 4 : ri(1, 3)));
                for (int i = 0; i < 4; ++i) {
                    if (ri(0, 2) == 0) { w32(xnode + 4 + 4 * u(i), 0); continue; }
                    data = (data + 0xf) & ~0xfu;
                    if (data < TR + 0x3000) data = TR + 0x3000;
                    const int nv = ri(0, 3) ? ri(3, 12) : 0, nt = nv ? ri(0, 12) : 0, nq = nt ? ri(0, std::min(nt, 4)) : 0;
                    const uint32_t c = data;
                    w8(c, u(nt)); w8(c + 1, R()); w8(c + 2, u(nv)); w8(c + 3, u(nq));
                    auto small = [](int bits, int lim) { const int v = ri(-lim, lim); return static_cast<uint32_t>(v) & ((1u << bits) - 1); };
                    for (int v = 0; v < nv + 1; ++v)
                        w32(c + 4 + 4 * u(v), ri(0, 7) ? (small(10, 50) | small(10, 50) << 10 | small(12, 190) << 20) : R());
                    const uint32_t tb = c + 4 + 4 * u(nv);
                    for (int tr = 0; tr < nt; ++tr) {
                        for (int q = 0; q < 3; ++q) w8(tb + 4 * u(tr) + u(q), u(ri(0, nv - 1)));
                        w8(tb + 4 * u(tr) + 3, ri(0, 2) ? u(ri(0, 0x1f)) : R());
                    }
                    for (int q = 0; q < nq; ++q) w8(tb + 4 * u(nt) + u(q), u(ri(0, nv - 1)));
                    const uint32_t size = 4 + 4 * u(nv + 1) + 4 * u(nt) + u(nq);
                    const uint32_t qwc = (size + 15) / 16;
                    w32(xnode + 4 + 4 * u(i), ((c - TR) << 8) | qwc);
                    data = c + qwc * 16;
                }
            }
        }
    };
    {
    // (stage 5, task C2b) sphere_collide and capsule_collide keep their chunk buffers, vertices and cell lists on the stack
    // (the original: 0x70000000 / 0x70001000, 0x70002000, 0x70003800): A2's collision scratch ranges are left out of these two
    // checks. They also hide what moby_pose_matrices (the posed case) writes inside them: the joint positions at 0x70000000 ..,
    // read back by the primitive tests, so a wrong one still shows in the contact.
    const CollisionScratchIgnored spr_ignored;
    check("sphere_collide", 0x001f0d60, N * 32, Ret::Int32, [collenv, treeenv, poseM] {
        const int k = collenv();
        uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        for (int tries = 0; tries < 8 && (ee_read<uint32_t>(m + 0xa0) & 0x80) != 0; ++tries) m = T4 + 0x100 * u(ri(0, k - 1));
        if (ri(0, 3) == 0 && ee_read<uint32_t>(m + 0x94) != 0) {
            poseM();
            const uint32_t nb = ee_read<uint8_t>(M2 + 8);
            for (uint32_t j = 0; j < nb; ++j) w32(0x01e80000 + 16 * j + 12, 0x70000000u + u(ri(0, static_cast<int>(j))) * 64);
            w32(m + 0x24, M2);
            for (uint32_t o = 0x50; o < 0x70; o += 4) w32(m + o, ee_read<uint32_t>(M + o));
            w32(m + 0x60, 0); w32(m + 0x64, 0);
            const uint32_t col = ee_read<uint32_t>(m + 0x94);
            w16(col, ri(0, 1) ? nb : 0u); w16(col + 2, nb); strip_jointless_sets(col);
            for (uint32_t e = 0; e < 8; ++e) if (ee_read<uint32_t>(0x01e9c000 + 8 * e) == ee_read<uint32_t>(m + 0xa8)) w32(0x01e9c000 + 8 * e, 0);
            for (int i = 0; i < 48; ++i) {
                const uint32_t c2 = ee_read<uint32_t>(T4 + 0x100 * u(i) + 0x94);
                if (T4 + 0x100 * u(i) != m && c2 != 0) { w16(c2, 0); w16(c2 + 2, 0); strip_jointless_sets(c2); }
            }
        }
        const float r = rf(0.3f, 3);
        float px = ee_read<float>(m + 0x10) + rf(-1.5f, 1.5f), py = ee_read<float>(m + 0x14) + rf(-1.5f, 1.5f), pz = ee_read<float>(m + 0x18) + rf(-1.5f, 1.5f);
        if (ri(0, 1)) {  // near a cell centre of the level instead
            px = 4.0f * static_cast<float>(ri(10, 30)) + 2 + rf(-2.5f, 2.5f); py = 4.0f * static_cast<float>(ri(10, 30)) + 2 + rf(-2.5f, 2.5f); pz = 4.0f * static_cast<float>(ri(3, 12)) + 2 + rf(-2.5f, 2.5f);
        }
        px = std::max(px, r + 0.01f); py = std::max(py, r + 0.01f); pz = std::max(pz, r + 0.01f);
        treeenv(px, py, pz);
        wf(OUT, px); wf(OUT + 4, py); wf(OUT + 8, pz); wf(OUT + 12, rf(-9, 9));
        if (ri(0, 15) == 0) wf(OUT, ri(0, 1) ? rf(-1, 0.1f) : rf(1020, 1030));
        const uint32_t fl = (ri(0, 3) ? 0u : 1u) | (ri(0, 3) ? 0u : 2u) | (ri(0, 1) ? 4u : 0u) | (ri(0, 2) ? 0u : 0x10u) | (ri(0, 2) ? 0u : 0x20u) | (ri(0, 2) ? 0u : 0x80u) | (u(ri(0, 0x1f)) << 8);
        return Args{{OUT, fl, ri(0, 3) ? 0u : m}, {ri(0, 20) ? r : 0.0f}};
    }, [](A a) { return ri32(sphere_collide(a.i[0], a.f[0], a.i[1], a.i[2])); },
       OV(0x00173e7c));  // w of the pushed-out position: vf2.w, stale from earlier VU0 code when this call loads no vf2
    check("capsule_collide", 0x001f19a0, N * 32, Ret::Int32, [collenv, treeenv, poseM] {
        const int k = collenv();
        uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        for (int tries = 0; tries < 8 && (ee_read<uint32_t>(m + 0xa0) & 0x80) != 0; ++tries) m = T4 + 0x100 * u(ri(0, k - 1));
        if (ri(0, 3) == 0 && ee_read<uint32_t>(m + 0x94) != 0) {
            poseM();
            const uint32_t nb = ee_read<uint8_t>(M2 + 8);
            for (uint32_t j = 0; j < nb; ++j) w32(0x01e80000 + 16 * j + 12, 0x70000000u + u(ri(0, static_cast<int>(j))) * 64);
            w32(m + 0x24, M2);
            for (uint32_t o = 0x50; o < 0x70; o += 4) w32(m + o, ee_read<uint32_t>(M + o));
            w32(m + 0x60, 0); w32(m + 0x64, 0);
            const uint32_t col = ee_read<uint32_t>(m + 0x94);
            w16(col, ri(0, 1) ? nb : 0u); w16(col + 2, nb); strip_jointless_sets(col);
            for (uint32_t e = 0; e < 8; ++e) if (ee_read<uint32_t>(0x01e9c000 + 8 * e) == ee_read<uint32_t>(m + 0xa8)) w32(0x01e9c000 + 8 * e, 0);
            for (int i = 0; i < 48; ++i) {
                const uint32_t c2 = ee_read<uint32_t>(T4 + 0x100 * u(i) + 0x94);
                if (T4 + 0x100 * u(i) != m && c2 != 0) { w16(c2, 0); w16(c2 + 2, 0); strip_jointless_sets(c2); }
            }
        }
        const float r = rf(0.3f, 3);
        float px = ee_read<float>(m + 0x10) + rf(-1.5f, 1.5f), py = ee_read<float>(m + 0x14) + rf(-1.5f, 1.5f), pz = ee_read<float>(m + 0x18) + rf(-1.5f, 1.5f);
        if (ri(0, 1)) {  // near a cell centre of the level instead
            px = 4.0f * static_cast<float>(ri(10, 30)) + 2 + rf(-2.5f, 2.5f); py = 4.0f * static_cast<float>(ri(10, 30)) + 2 + rf(-2.5f, 2.5f); pz = 4.0f * static_cast<float>(ri(3, 12)) + 2 + rf(-2.5f, 2.5f);
        }
        px = std::max(px, r + 0.01f); py = std::max(py, r + 0.01f); pz = std::max(pz, r + 0.01f);
        for (int i = 0; i < 48; ++i) {  // some joint segments upright (the xy-degenerate branch)
            const uint32_t col = ee_read<uint32_t>(T4 + 0x100 * u(i) + 0x94);
            if (col == 0) continue;
            const uint32_t np = ee_read<uint32_t>(col + 4) / 0x20;
            for (uint32_t r2 = 0; r2 < np; ++r2) {
                const uint32_t pr = col + 0x10 + 0x20 * r2; const int8_t ty = static_cast<int8_t>(ee_read<uint8_t>(pr));
                if ((ty >= 1 && ty <= 3) || ri(0, 1)) continue;
                const uint32_t ja = ee_read<uint16_t>(pr + 4) & 0xf, jb = ee_read<uint16_t>(pr + 6) & 0xf;
                const uint32_t mm = T4 + 0x100 * u(i);
                const float sc = ee_read<float>(mm + 0x2c), dz = rf(-2000, 2000);
                for (uint32_t sl = 0; sl < 8; ++sl) {  // B = A + a world-vertical offset (through this moby's rows)
                    if (ee_read<uint32_t>(0x01e9c000 + 8 * sl) != ee_read<uint32_t>(mm + 0xa8)) continue;
                    const uint32_t b = 0x01ea0000 + 0x800 * sl;
                    for (uint32_t c = 0; c < 3; ++c)
                        wf(b + 16 * jb + 4 * c, ee_read<float>(b + 16 * ja + 4 * c) + dz * ee_read<float>(mm + 0xc8 + 0x10 * c) / sc);
                }
            }
        }
        treeenv(px, py, pz);
        wf(OUT, px); wf(OUT + 4, py); wf(OUT + 8, pz); wf(OUT + 12, rf(-9, 9));
        if (ri(0, 15) == 0) wf(OUT, ri(0, 1) ? rf(-1, 0.1f) : rf(1020, 1030));
        const uint32_t fl = (ri(0, 3) ? 0u : 1u) | (ri(0, 3) ? 0u : 2u) | (ri(0, 1) ? 4u : 0u) | (ri(0, 2) ? 0u : 0x10u) | (ri(0, 2) ? 0u : 0x20u) | (ri(0, 2) ? 0u : 0x80u) | (u(ri(0, 0x1f)) << 8);
        return Args{{OUT, fl, ri(0, 3) ? 0u : m}, {ri(0, 20) ? r : 0.0f, ri(0, 20) ? rf(0.1f, 3) : 0.0f}};
    }, [](A a) { return ri32(capsule_collide(a.i[0], a.f[0], a.f[1], a.i[1], a.i[2])); },
       OV(0x00173e7c));  // w of the pushed-out position: vf2.w, stale from earlier VU0 code when this call loads no vf2
    }

    check("segment_collide", 0x001efc70, N * 48, Ret::Int32, [collenv, treeenv, poseM] {
        const int k = collenv();
        uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        for (int tries = 0; tries < 8 && (ee_read<uint32_t>(m + 0xa0) & 0x80) != 0; ++tries) m = T4 + 0x100 * u(ri(0, k - 1));
        if (ri(0, 3) == 0 && ee_read<uint32_t>(m + 0x94) != 0) {
            poseM();
            const uint32_t nb = ee_read<uint8_t>(M2 + 8);
            for (uint32_t j = 0; j < nb; ++j) w32(0x01e80000 + 16 * j + 12, 0x70000000u + u(ri(0, static_cast<int>(j))) * 64);
            w32(m + 0x24, M2);
            for (uint32_t o = 0x50; o < 0x70; o += 4) w32(m + o, ee_read<uint32_t>(M + o));
            w32(m + 0x60, 0); w32(m + 0x64, 0);
            const uint32_t col = ee_read<uint32_t>(m + 0x94);
            w16(col, ri(0, 1) ? nb : 0u); w16(col + 2, nb); strip_jointless_sets(col);
            for (uint32_t e = 0; e < 8; ++e) if (ee_read<uint32_t>(0x01e9c000 + 8 * e) == ee_read<uint32_t>(m + 0xa8)) w32(0x01e9c000 + 8 * e, 0);
            for (int i = 0; i < 48; ++i) {
                const uint32_t c2 = ee_read<uint32_t>(T4 + 0x100 * u(i) + 0x94);
                if (T4 + 0x100 * u(i) != m && c2 != 0) { w16(c2, 0); w16(c2 + 2, 0); strip_jointless_sets(c2); }
            }
        }
        float px = ee_read<float>(m + 0x10) + rf(-3, 3), py = ee_read<float>(m + 0x14) + rf(-3, 3), pz = ee_read<float>(m + 0x18) + rf(-3, 3);
        if (ri(0, 1)) { px = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); py = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); pz = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4); }
        const float len = ri(0, 3) ? rf(0.2f, 6) : rf(6, 14);
        float dx = rf(-1, 1), dy = rf(-1, 1), dz = rf(-1, 1);
        const float n = std::sqrt(dx * dx + dy * dy + dz * dz) + 1e-3f; dx *= len / n; dy *= len / n; dz *= len / n;
        if (ri(0, 7) == 0) { dx = 0; dy = 0; }   // straight down / up
        float qx = px + dx, qy = py + dy, qz = pz + dz;
        if (ri(0, 2) == 0) { qx = px - (dx * 0.5f); qy = py - (dy * 0.5f); qz = pz - dz * 0.5f; px += dx * 0.5f; py += dy * 0.5f; pz += dz * 0.5f; }  // through the moby
        auto cl = [](float v) { return std::min(std::max(v, 0.5f), 1000.0f); };
        px = cl(px); py = cl(py); pz = cl(pz); qx = cl(qx); qy = cl(qy); qz = cl(qz);
        if (ri(0, 31) == 0) { qx = px; qy = py; qz = pz; }
        treeenv((px + qx) / 2, (py + qy) / 2, (pz + qz) / 2);
        // aim at a triangle: of the moby, or of a level chunk (a point inside it, through it)
        std::array<float, 3> tgt{}; bool aim = false;
        const uint32_t mcol = ee_read<uint32_t>(m + 0x94);
        if (ri(0, 1) && mcol != 0 && ee_read<uint32_t>(mcol + 8) != 0) {
            const uint32_t vb = mcol + 0x10 + ee_read<uint32_t>(mcol + 4), tb = vb + ee_read<uint32_t>(mcol + 0xc);
            const uint32_t tri = tb + 4 * u(ri(0, static_cast<int>(ee_read<uint32_t>(mcol + 8) / 4) - 1));
            const float sc = ee_read<float>(m + 0x2c);
            const float wa = rf(0, 1), wb = rf(0, 1 - wa);
            for (int c = 0; c < 3; ++c) {
                float w = ee_read<float>(m + 0x10 + 4 * u(c));
                for (int vi = 0; vi < 3; ++vi) {
                    const uint32_t va = vb + 8 * ee_read<uint8_t>(tri + u(vi));
                    const float bw = vi == 0 ? wa : vi == 1 ? wb : 1 - wa - wb;
                    for (int j = 0; j < 3; ++j) w += bw * ee_read<float>(m + 0xc0 + 0x10 * u(j) + 4 * u(c)) * static_cast<float>(ee_read<int16_t>(va + 2 * u(j))) * sc / 1024.0f;
                }
                tgt[static_cast<size_t>(c)] = w;
            }
            aim = true;
        } else if (ri(0, 1)) {
            constexpr uint32_t TR = 0x01ec0000;
            for (int tries = 0; tries < 20 && !aim; ++tries) {
                const int kz = ri(0, ee_read<uint16_t>(TR + 2) - 1);
                const uint32_t zo = ee_read<uint16_t>(TR + 4 + 2 * u(kz)); if (zo == 0) continue;
                const uint32_t yn = TR + (zo << 2); const int ky = ri(0, ee_read<uint16_t>(yn + 2) - 1);
                const uint32_t yo = ee_read<uint32_t>(yn + 4 + 4 * u(ky)); if (yo == 0) continue;
                const uint32_t xn = TR + yo; const int kx = ri(0, ee_read<uint16_t>(xn + 2) - 1);
                const uint32_t ch = ee_read<uint32_t>(xn + 4 + 4 * u(kx)); if (ch == 0) continue;
                const uint32_t c = TR + (ch >> 8); const int nt = ee_read<uint8_t>(c), nv = ee_read<uint8_t>(c + 2); if (nt == 0) continue;
                const uint32_t tri = c + 4 + 4 * u(nv) + 4 * u(ri(0, nt - 1));
                const int cc[3] = {ee_read<uint16_t>(xn) + kx, ee_read<uint16_t>(yn) + ky, ee_read<uint16_t>(TR) + kz};
                const float wa = rf(0, 1), wb = rf(0, 1 - wa);
                for (int ax = 0; ax < 3; ++ax) {
                    float w = 0;
                    for (int vi = 0; vi < 3; ++vi) {
                        const uint32_t pv = ee_read<uint32_t>(c + 4 + 4 * u(ee_read<uint8_t>(tri + u(vi))));
                        const int v = ax == 0 ? static_cast<int16_t>((pv & 0x3ff) << 6) : ax == 1 ? static_cast<int16_t>(((pv >> 10) & 0x3ff) << 6) : static_cast<int16_t>((pv >> 20) << 4);
                        const float bw = vi == 0 ? wa : vi == 1 ? wb : 1 - wa - wb;
                        w += bw * static_cast<float>((cc[ax] << 12) + 0x800 + v);
                    }
                    tgt[static_cast<size_t>(ax)] = w / 1024.0f;
                }
                aim = true;
            }
        }
        if (aim) {
            const float l1 = rf(0.1f, 3), l2 = rf(0.1f, 3);
            px = cl(tgt[0] - dx / len * l1); py = cl(tgt[1] - dy / len * l1); pz = cl(tgt[2] - dz / len * l1);
            qx = cl(tgt[0] + dx / len * l2); qy = cl(tgt[1] + dy / len * l2); qz = cl(tgt[2] + dz / len * l2);
            if (ri(0, 1)) { std::swap(px, qx); std::swap(py, qy); std::swap(pz, qz); }
        }
        wf(OUT, px); wf(OUT + 4, py); wf(OUT + 8, pz); wf(OUT + 12, rf(-9, 9));
        wf(OUT + 0x10, qx); wf(OUT + 0x14, qy); wf(OUT + 0x18, qz); wf(OUT + 0x1c, rf(-9, 9));
        if (ri(0, 15) == 0) wf(OUT, ri(0, 1) ? rf(-1, -0.01f) : rf(1024, 1030));
        wrand(OUT + 0x40, 9); w32(OUT + 0x40 + 0x1c, u(ri(0, 3)));
        const uint32_t fl = (ri(0, 3) ? 0u : 1u) | (ri(0, 3) ? 0u : 2u) | (ri(0, 1) ? 4u : 0u) | (ri(0, 5) ? 0u : 8u) | (ri(0, 2) ? 0u : 0x10u) | (ri(0, 2) ? 0u : 0x20u) | (ri(0, 2) ? 0u : 0x80u) | (u(ri(0, 0x1f)) << 8);
        return Args{{OUT, OUT + 0x10, fl, ri(0, 3) ? 0u : m, ri(0, 1) ? OUT + 0x40 : 0u}, {}};
    }, [](A a) { return ri32(segment_collide(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4])); });

    // ======== batch 44
    g_batch = 44;
    g_strip_jointless = true;  // (see strip_jointless_sets)
    check("floor_probe", 0x00259698, N * 32, Ret::Float, [collenv, treeenv] {
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float px = ee_read<float>(m + 0x10) + rf(-1.5f, 1.5f), py = ee_read<float>(m + 0x14) + rf(-1.5f, 1.5f);
        float pz = ee_read<float>(m + 0x18) + rf(-2, 4);
        if (ri(0, 1)) { px = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); py = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); pz = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4); }
        auto cl = [](float v) { return std::min(std::max(v, 0.5f), 1000.0f); };
        px = cl(px); py = cl(py); pz = cl(pz);
        const float up = ri(0, 3) ? rf(0, 3) : rf(-3, 0);
        treeenv(px, py, std::max(pz - rf(0, 6), 0.5f));
        wf(OUT, px); wf(OUT + 4, py); wf(OUT + 8, pz); wf(OUT + 12, rf(-9, 9));
        const uint32_t fl = (ri(0, 3) ? 0u : 1u) | (ri(0, 1) ? 4u : 0u) | (ri(0, 5) ? 0u : 8u) | (ri(0, 2) ? 0u : 0x10u) | (ri(0, 2) ? 0u : 0x20u) | (u(ri(0, 0x1f)) << 8);
        return Args{{OUT, fl}, {up}};
    }, [](A a) { return rf32(floor_probe(a.f[0], a.i[0], a.i[1])); });

    {
    // (stage 5, task C2b) moby_sphere_contact reaches sphere_collide: A2's collision scratch ranges left out of this check (see
    // batch 43's sphere_collide: the posing's joint positions inside them are read back, so a wrong one still shows)
    const CollisionScratchIgnored spr_ignored;
    check("moby_sphere_contact", 0x002598b0, N * 32, Ret::Int32, [collenv] {
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1)), m2 = T4 + 0x100 * u(ri(0, k - 1));
        w16(m + 0x34, u(ri(0, 0xffff))); wf(m + 0x48, rf(-3.2f, 3.2f)); w16(m + 0xa6, u(ri(0, 0xffff)));
        const float r = rf(0.3f, 3);
        wf(OUT, std::max(ee_read<float>(m2 + 0x10) + rf(-1.5f, 1.5f), r + 0.01f)); wf(OUT + 4, std::max(ee_read<float>(m2 + 0x14) + rf(-1.5f, 1.5f), r + 0.01f));
        wf(OUT + 8, std::max(ee_read<float>(m2 + 0x18) + rf(-1.5f, 1.5f), r + 0.01f)); wf(OUT + 12, rf(-9, 9));
        const uint32_t fl = (ri(0, 3) ? 0u : 1u) | (ri(0, 1) ? 4u : 0u) | (ri(0, 5) ? 0u : 8u) | (ri(0, 2) ? 0u : 0x10u) | (ri(0, 2) ? 0u : 0x20u) | (u(ri(0, 0x1f)) << 8);
        return Args{{m, OUT, R(), R(), R(), fl}, {r, ri(0, 1) ? rf(-9, 9) : std::bit_cast<float>(u(ri(0, 8))), rf(-5, 5)}};
    }, [](A a) { return ri32(moby_sphere_contact(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.i[3], a.i[4], a.i[5], a.f[0], a.f[1], a.f[2])); });
    }

    // hero at (x, y, z) facing yaw (PLAYER matrix: rotation about z, row 3 zero)
    auto heroAt = [](float x, float y, float z, float yaw) {
        wfrand(g::PLAYER, 16, 0, 0);
        const float c = std::cos(yaw), s = std::sin(yaw);
        wf(g::PLAYER + 0x00, c); wf(g::PLAYER + 0x04, s); wf(g::PLAYER + 0x10, -s); wf(g::PLAYER + 0x14, c); wf(g::PLAYER + 0x28, 1.0f);
        wf(g::PLAYER + 0x80, x); wf(g::PLAYER + 0x84, y); wf(g::PLAYER + 0x88, z); wf(g::PLAYER + 0x8c, 1.0f);
    };
    check("hero_wall_probe", 0x0020a240, N * 32, Ret::Int32, [collenv, treeenv, heroAt] {
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float px = ee_read<float>(m + 0x10) + rf(-2, 2), py = ee_read<float>(m + 0x14) + rf(-2, 2), pz = ee_read<float>(m + 0x18) + rf(-1.5f, 0.5f);
        if (ri(0, 1)) { px = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); py = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); pz = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4); }
        auto cl = [](float v) { return std::min(std::max(v, 5.0f), 1000.0f); };
        px = cl(px); py = cl(py); pz = cl(pz);
        const float yaw = ri(0, 1) ? std::atan2(ee_read<float>(m + 0x14) - py, ee_read<float>(m + 0x10) - px) + rf(-0.3f, 0.3f) : rf(-3.2f, 3.2f);
        heroAt(px, py, pz, yaw);
        wf(g::PLAYER + 0x234, rf(0.1f, 1));
        treeenv(px, py, pz);
        wf(OUT + 0x20, rf(-9, 9));
        return Args{{ri(0, 3) ? OUT + 0x20 : 0u}, {rf(-1, 3), rf(0, 4)}};
    }, [](A a) { return ri32(hero_wall_probe(a.i[0], a.f[0], a.f[1])); });

    // a point on one of moby m's triangles or (after treeenv) of a level chunk near it; false if none
    auto aimTri = [](uint32_t m, std::array<float, 3>& tgt) {
        bool aim = false;
        const uint32_t mcol = ee_read<uint32_t>(m + 0x94);
        if (ri(0, 1) && mcol != 0 && ee_read<uint32_t>(mcol + 8) != 0) {
            const uint32_t vb = mcol + 0x10 + ee_read<uint32_t>(mcol + 4), tb = vb + ee_read<uint32_t>(mcol + 0xc);
            const uint32_t tri = tb + 4 * u(ri(0, static_cast<int>(ee_read<uint32_t>(mcol + 8) / 4) - 1));
            const float sc = ee_read<float>(m + 0x2c);
            const float wa = rf(0, 1), wb = rf(0, 1 - wa);
            for (int c = 0; c < 3; ++c) {
                float w = ee_read<float>(m + 0x10 + 4 * u(c));
                for (int vi = 0; vi < 3; ++vi) {
                    const uint32_t va = vb + 8 * ee_read<uint8_t>(tri + u(vi));
                    const float bw = vi == 0 ? wa : vi == 1 ? wb : 1 - wa - wb;
                    for (int j = 0; j < 3; ++j) w += bw * ee_read<float>(m + 0xc0 + 0x10 * u(j) + 4 * u(c)) * static_cast<float>(ee_read<int16_t>(va + 2 * u(j))) * sc / 1024.0f;
                }
                tgt[static_cast<size_t>(c)] = w;
            }
            aim = true;
        } else if (ri(0, 1)) {
            constexpr uint32_t TR = 0x01ec0000;
            for (int tries = 0; tries < 20 && !aim; ++tries) {
                const int kz = ri(0, ee_read<uint16_t>(TR + 2) - 1);
                const uint32_t zo = ee_read<uint16_t>(TR + 4 + 2 * u(kz)); if (zo == 0) continue;
                const uint32_t yn = TR + (zo << 2); const int ky = ri(0, ee_read<uint16_t>(yn + 2) - 1);
                const uint32_t yo = ee_read<uint32_t>(yn + 4 + 4 * u(ky)); if (yo == 0) continue;
                const uint32_t xn = TR + yo; const int kx = ri(0, ee_read<uint16_t>(xn + 2) - 1);
                const uint32_t ch = ee_read<uint32_t>(xn + 4 + 4 * u(kx)); if (ch == 0) continue;
                const uint32_t c = TR + (ch >> 8); const int nt = ee_read<uint8_t>(c), nv = ee_read<uint8_t>(c + 2); if (nt == 0) continue;
                const uint32_t tri = c + 4 + 4 * u(nv) + 4 * u(ri(0, nt - 1));
                const int cc[3] = {ee_read<uint16_t>(xn) + kx, ee_read<uint16_t>(yn) + ky, ee_read<uint16_t>(TR) + kz};
                const float wa = rf(0, 1), wb = rf(0, 1 - wa);
                for (int ax = 0; ax < 3; ++ax) {
                    float w = 0;
                    for (int vi = 0; vi < 3; ++vi) {
                        const uint32_t pv = ee_read<uint32_t>(c + 4 + 4 * u(ee_read<uint8_t>(tri + u(vi))));
                        const int v = ax == 0 ? static_cast<int16_t>((pv & 0x3ff) << 6) : ax == 1 ? static_cast<int16_t>(((pv >> 10) & 0x3ff) << 6) : static_cast<int16_t>((pv >> 20) << 4);
                        const float bw = vi == 0 ? wa : vi == 1 ? wb : 1 - wa - wb;
                        w += bw * static_cast<float>((cc[ax] << 12) + 0x800 + v);
                    }
                    tgt[static_cast<size_t>(ax)] = w / 1024.0f;
                }
                aim = true;
            }
        }
        return aim;
    };
    check("hero_floor_ahead", 0x00229910, N * 32, Ret::Int32, [collenv, treeenv, heroAt, aimTri] {
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float px = ee_read<float>(m + 0x10) + rf(-2, 2), py = ee_read<float>(m + 0x14) + rf(-2, 2), pz = ee_read<float>(m + 0x18) + rf(-0.5f, 1.5f);
        if (ri(0, 1)) { px = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); py = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); pz = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4); }
        auto cl = [](float v) { return std::min(std::max(v, 5.0f), 1000.0f); };
        px = cl(px); py = cl(py); pz = cl(pz);
        const float yaw = rf(-3.2f, 3.2f);
        heroAt(px, py, pz, yaw);
        wf(0x0015ed68, ri(0, 1) ? 1.0f : rf(0.5f, 2));  // frame scale
        wf(g::PLAYER + 0x234, rf(0.1f, 2)); wf(g::PLAYER + 0x164, rf(0, 0.2f)); wf(g::PLAYER + 0x98, rf(-3.2f, 3.2f));
        const float h = ri(0, 3) ? ee_read<float>(g::PLAYER + 0x98) + rf(-0.6f, 0.6f) : rf(-3.2f, 3.2f);
        const float sp = rf(0, 0.3f);
        wf(g::PLAYER + 0x100, sp * std::cos(h)); wf(g::PLAYER + 0x104, sp * std::sin(h));
        w32(g::PLAYER + 0x2080, ri(0, 1) ? m : 0u);
        treeenv(px, py, std::max(pz - 0.5f, 0.5f));
        std::array<float, 3> tg{};
        if (ri(0, 3) && aimTri(m, tg)) {  // the point ahead lies right above / below a triangle
            const float f = ee_read<float>(g::PLAYER + 0x98), r = ee_read<float>(g::PLAYER + 0x234) * 0.75f;
            const float z = ri(0, 1) ? tg[2] - rf(0.3f, 0.5f) : tg[2] + rf(-0.1f, 1.1f);
            heroAt(tg[0] - std::cos(f) * r, tg[1] - std::sin(f) * r, z, yaw);
            wf(g::PLAYER + 0x164, 1.0f); wf(g::PLAYER + 0x100, std::cos(f)); wf(g::PLAYER + 0x104, std::sin(f)); w32(g::PLAYER + 0x2080, 0);
        }
        return Args{{}, {}};
    }, [](A) { return ri32(hero_floor_ahead()); });

    check("hero_edge_brake", 0x00217118, N * 32, Ret::None, [collenv, treeenv, heroAt, aimTri] {
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float px = ee_read<float>(m + 0x10) + rf(-2, 2), py = ee_read<float>(m + 0x14) + rf(-2, 2), pz = ee_read<float>(m + 0x18) + rf(-0.5f, 1.5f);
        if (ri(0, 1)) { px = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); py = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); pz = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4); }
        auto cl = [](float v) { return std::min(std::max(v, 5.0f), 1000.0f); };
        px = cl(px); py = cl(py); pz = cl(pz);
        const float yaw = rf(-3.2f, 3.2f);
        heroAt(px, py, pz, yaw);
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) wf(g::PLAYER + 0x40 + 0x10 * u(r) + 4 * u(c), ee_read<float>(g::PLAYER + 0x10 * u(c) + 4 * u(r)));  // inverse rotation
        wf(g::PLAYER + 0x7c, 0.0f); wf(g::PLAYER + 0x4c, 0.0f); wf(g::PLAYER + 0x5c, 0.0f); wf(g::PLAYER + 0x6c, 0.0f);
        wf(g::PLAYER + 0x70, 0.0f); wf(g::PLAYER + 0x74, 0.0f); wf(g::PLAYER + 0x78, 0.0f);
        const uint32_t mode = ri(0, 3) ? 0u : u(ri(1, 3));
        w8(g::PLAYER + 0x20b3, mode);
        w32(g::PLAYER + 0x2080, ri(0, 2) ? m : T4 + 0x100 * u(ri(0, 47)));
        const uint32_t hm = ee_read<uint32_t>(g::PLAYER + 0x2080);
        if (mode == 0) { wf(hm + 0x40, 0); wf(hm + 0x44, 0); } else { wf(hm + 0x40, rf(-0.3f, 0.3f)); wf(hm + 0x44, rf(-0.3f, 0.3f)); }
        wf(hm + 0x48, rf(-3.2f, 3.2f)); wf(hm + 0x4c, 0);
        float vx = rf(-0.5f, 0.5f), vy = rf(-0.5f, 0.5f);
        if (ri(0, 15) == 0) { vx = 0; vy = 0; }
        wf(g::PLAYER + 0xe0, vx); wf(g::PLAYER + 0xe4, vy); wf(g::PLAYER + 0xe8, rf(-0.3f, 0.3f)); wf(g::PLAYER + 0xec, rf(-1, 1));
        wfrand(0x0013f450, 4, -2, 2); wfrand(0x0013f460, 4, -2, 2); wfrand(0x0013f4a0, 4, -2, 2); wf(0x0013f4e4, rf(-2, 2));
        wf(g::PLAYER + 0x229c, ri(0, 2) ? rf(0, 0.5f) : rf(0.5f, 1)); w32(g::PLAYER + 0x208c, u(ri(5, 7)));
        w16(g::PLAYER + 0x1f4, ri(0, 1) ? 0u : 1u); w8(g::PLAYER + 0x12e7, ri(0, 1) ? 0u : 1u); w16(g::PLAYER + 0x30e, ri(0, 7) ? 0u : 1u);
        w32(g::PLAYER + 0x2084, ri(0, 3) ? u(ri(0, 3)) : 0x20u); w8(g::PLAYER + 0x20a4, u(ri(0, 3)));
        const float kk = rf(0, 4);
        float minl = ri(0, 1) ? 0.0f : rf(0, 1);
        treeenv(px, py, std::max(pz - 0.5f, 0.5f));
        std::array<float, 3> tg{};
        if (mode == 0 && ri(0, 3) && aimTri(m, tg)) {  // the point ahead lies just below a triangle
            minl = 0;
            const float s = ee_read<uint32_t>(g::PLAYER + 0x2084) == 0x20 ? 2.0f : kk;
            heroAt(tg[0] - vx * s, tg[1] - vy * s, tg[2] + rf(-0.3f, 0.7f), yaw);
            for (int r = 0; r < 4; ++r) for (int c = 0; c < 3; ++c) wf(g::PLAYER + 0x40 + 0x10 * u(r) + 4 * u(c), ee_read<float>(g::PLAYER + 0x10 * u(c) + 4 * u(r)));
        }
        return Args{{}, {kk, minl}};
    }, [](A a) { hero_edge_brake(a.f[0], a.f[1]); return uint64_t{0}; });

    // grid mobys (collenv) chained from *(0x15ffe4) through +0x28, some of class cls, with pvars;
    // the hero stands near one of them
    auto pickenv = [collenv, treeenv, heroAt](uint32_t cls) {
        const int k = collenv();
        uint32_t prev = OV(0x0015ffe4) - 0x28;
        for (int i = 0; i < 48; ++i) {
            const uint32_t m = T4 + 0x100 * u(i);
            if (ri(0, 3) == 0) continue;
            w32(prev + 0x28, m); prev = m;
            w16(m + 0xa6, ri(0, 2) ? cls : u(ri(0x2f0, 0x330))); w16(m + 0x34, ri(0, 5) ? 0u : 1u); w8(m + 0x31, u(ri(0, 1)));
            const uint32_t pv = 0x01ed0000 + 0x40 * u(i);
            w32(m + 0x78, pv); w32(pv, ri(0, 1) ? 0u : 1u); wrand(pv + 4, 2); wf(pv + 4, rf(-9, 9));
            w32(pv + 0x1c, ri(0, 2) ? 0xffffffffu : u(ri(0, 3))); wf(pv + 0x20, ri(0, 1) ? 0.0f : rf(1, 30));
        }
        w32(prev + 0x28, 0);
        w32(OV(0x001600ec), T); for (int i = 0; i < 4; ++i) { wfrand(T + i * 128 + 0x30, 4, 0, 200); wfrand(T + i * 128 + 0x40, 16, -0.2f, 0.2f); }
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        const float px = std::max(ee_read<float>(m + 0x10) + rf(-18, 18), 5.0f), py = std::max(ee_read<float>(m + 0x14) + rf(-18, 18), 5.0f);
        const float pz = std::max(ee_read<float>(m + 0x18) + rf(-2, 5), 5.0f);
        heroAt(px, py, pz, 0);
        wf(g::PLAYER + 0x98, ri(0, 1) ? std::atan2(ee_read<float>(m + 0x14) - py, ee_read<float>(m + 0x10) - px) + rf(-0.5f, 0.5f) : rf(-3.2f, 3.2f));
        wf(g::PLAYER + 0x57c, rf(-3.2f, 3.2f));
        static const uint32_t hs[] = {0, 1, 0x14, 0x1e, 5};
        w32(g::HERO_STATE, hs[ri(0, 4)]); wf(g::PLAYER + 0x2d8, pz + rf(-1, 1));
        wfrand(OV(0x00166c80) + 0x140, 4, 0, 200); wfrand(OV(0x00166c80) + 0x154, 2, -3, 3);
        w32(OV(0x00166c00u - 0x6c20), T4 + 0x100 * 48);
        static const uint32_t st[] = {0, 1, 0x1e, 0x2d, 5};
        w32(g::PLAYER + 0x2084, st[ri(0, 4)]);
        static const uint32_t ss[] = {0, 0xd, 0xf, 3};
        w32(g::PLAYER + 0x208c, ss[ri(0, 3)]);
        wrand(g::PLAYER + 0x960, 0x30);
        for (int i = 0; i < 4; ++i) { wf(T + i * 128 + 0x30, px + rf(-3, 3)); wf(T + i * 128 + 0x34, py + rf(-3, 3)); wf(T + i * 128 + 0x38, pz + rf(-3, 3)); }
        if (ri(0, 1)) { wf(OV(0x00166c80) + 0x140, px + rf(-5, 5)); wf(OV(0x00166c80) + 0x144, py + rf(-5, 5)); wf(OV(0x00166c80) + 0x148, pz + rf(-1, 3)); }
        treeenv(px, py, pz);
    };
    check("hero_pick_target_323", 0x00221ee0, N * 48, Ret::None, [pickenv] { pickenv(0x323); return Args{}; },
          [](A) { hero_pick_target_323(); return uint64_t{0}; });
    check("hero_pick_target_2f6", 0x00222158, N * 48, Ret::Int32, [pickenv] { pickenv(0x2f6); return Args{}; },
          [](A) { return ri32(hero_pick_target_2f6()); });

    check("hero_footstep/sound", 0x00205f60, N * 4, Ret::None, [sndenv] {
        sndenv(); w32(0x0015ed84, u(ri(0, 3))); for (int i = 0; i < 4; ++i) w8(OV(0x001bd820) + i, u(ri(0, 2)));
        w32(OV(0x0015f574), 0); w32(OV(0x0015f5f0), u(ri(0, 8))); w32(OV(0x0015f5f4), T2);
        w32(g::PLAYER + 0x10e0, ri(0, 2) ? M3 : 0u); w16(M3 + 0xa6, ri(0, 1) ? 0xadu : 0xacu);
        return Args{{u(ri(0, 1)), u(ri(0, 1)), u(ri(0, 1))}, {}};
    }, [](A a) { hero_footstep(static_cast<int32_t>(a.i[0]), a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });
    check("hero_footstep/print", 0x00205f60, N * 16, Ret::None, [collenv, treeenv, chainenv, particles] {
        collenv();
        chainenv(0x18);
        const float x = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4), y = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4), z = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4);
        wf(M + 0x10, x); wf(M + 0x14, y); wf(M + 0x18, z); wf(M + 0x2c, rf(10, 300));
        w32(g::PLAYER + 0x2080, M); wf(g::PLAYER + 0x98, rf(-3.2f, 3.2f));
        w32(0x0015ed84, ri(0, 5) ? 0xcu : 0xbu); w8(g::PLAYER + 0x12ed, ri(0, 5) ? 0u : 1u); w32(g::PLAYER + 0x2fc, ri(0, 5) ? 0u : 1u);
        particles(); w32(OV(0x001b2198), T); wrand(T, 8);
        treeenv(x, y, std::max(z - 2, 0.5f));
        return Args{{R(), u(ri(0, 1)), 0}, {}};
    }, [](A a) { hero_footstep(static_cast<int32_t>(a.i[0]), a.i[1], static_cast<int32_t>(a.i[2])); return uint64_t{0}; });

    check("hero_wall_lean", 0x0020a318, N * 32, Ret::None, [collenv, treeenv, heroAt] {
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float px = ee_read<float>(m + 0x10) + rf(-2, 2), py = ee_read<float>(m + 0x14) + rf(-2, 2), pz = ee_read<float>(m + 0x18) + rf(-1.5f, 0.5f);
        if (ri(0, 1)) { px = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); py = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); pz = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4); }
        auto cl = [](float v) { return std::min(std::max(v, 5.0f), 1000.0f); };
        px = cl(px); py = cl(py); pz = cl(pz);
        const float yaw = ri(0, 1) ? std::atan2(ee_read<float>(m + 0x14) - py, ee_read<float>(m + 0x10) - px) + rf(-0.3f, 0.3f) : rf(-3.2f, 3.2f);
        heroAt(px, py, pz, yaw);
        wf(g::PLAYER + 0x234, rf(0.1f, 1));
        treeenv(px, py, pz);
        for (int i = 0; i < 48; ++i) w16(T4 + 0x100 * u(i) + 0xa6, u(ri(480, 560)));
        w32(OV(0x00166c00u - 0x6c20), T4 + 0x100 * 47);
        w8(g::PLAYER + 0x20ab, ri(0, 5) ? 1u : 0u); w8(g::PLAYER + 0x20a8, u(ri(0, 1)));
        w32(g::PLAYER + 0x10b4, u(ri(1, 2))); w32(g::PLAYER + 0x10b8, ri(0, 3) ? 3u : 0x1au);
        w32(g::HERO_STATE, ri(0, 3) ? 0u : 0x81u); wf(0x0015ed64, rf(0.5f, 2));
        wrand(g::FX_PARAMS + 0x690, 2); wrand(g::FX_PARAMS + 0x6d0, 3); w16(g::PLAYER + 0x22c8, R());
        return Args{};
    }, [](A) { hero_wall_lean(); return uint64_t{0}; });

    check("hero_idle_event_pick", 0x00229010, N * 16, Ret::Int32, [collenv, treeenv] {
        const int k = collenv();
        seed_rand();
        static const uint32_t P = g::PLAYER, TAB = OV(0x00179890);
        for (int i = 0; i < 5; ++i) {
            const uint32_t e = TAB + 0x70 * u(i);
            w32(e + 0x60, ri(0, 4) ? 0u : 1u); wf(e + 0x58, ri(0, 5) ? (ri(0, 1) ? rf(0.04f, 0.12f) : rf(0.02f, 3)) : 0.0f);
            w32(e + 0x50, ri(0, 2) ? 0xffffffffu : u(ri(0, 3))); w32(e + 0x44, u(ri(0, 1)));
        }
        w16(P + 0x1e8, ri(0, 7) ? 0u : 1u); w32(P + 0x22a8, u(ri(0, 4) ? ri(0, 2) : 1)); w32(OV(0x00179ab0), u(ri(0, 1))); w32(P + 0xa98, R());
        w32(P + 0x10b4, ri(0, 3) ? 2u : 1u); w32(P + 0x10b8, ri(0, 3) ? u(ri(0, 3)) : 0x17u); w32(P + 0x2fc, u(ri(0, 1)));
        w32(0x0015ed84, ri(0, 2) ? 0xcu : 0xbu); w32(P + 0x2094, u(ri(0, 2))); w32(P + 0x198, u(ri(0, 120)));
        wf(0x0015ed68, ri(0, 1) ? 1.0f : rf(0.5f, 2));
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        const float x = std::max(ee_read<float>(m + 0x10) + rf(-1, 1), 5.0f), y = std::max(ee_read<float>(m + 0x14) + rf(-1, 1), 5.0f), z = std::max(ee_read<float>(m + 0x18) - rf(0, 6), 5.0f);
        wf(P + 0xd0, x); wf(P + 0xd4, y); wf(P + 0xd8, z); wf(P + 0xdc, rf(-1, 1));
        w32(P + 0x2080, ri(0, 3) ? 0u : m);
        treeenv(x, y, z + 4);
        w32(OUT, R());
        return Args{{OUT}, {}};
    }, [](A a) { return ri32(hero_idle_event_pick(hp<int32_t>(a.i[0]))); });

    // moby list over the grid mobys, some targetable; OUT = a point near moby m, OUT+0x10 = a
    // direction towards it; returns m
    auto aimenv = [collenv, treeenv] {
        const int k = collenv();
        uint32_t prev = OV(0x0015ffe4) - 0x28;
        for (int i = 0; i < 48; ++i) {
            const uint32_t m = T4 + 0x100 * u(i);
            const uint32_t cls = 0x01ee0000 + 0x80 * u(i), pv = 0x01ee4000 + 0x40 * u(i), pw = 0x01ee8000 + 0x40 * u(i);
            w32(m + 0x78, pv); w32(pv, 0);
            if (ri(0, 3) == 0) continue;
            w32(prev + 0x28, m); prev = m;
            w8(m + 0x20, ri(0, 5) ? u(ri(0, 0x7f)) : 0x80u); w16(m + 0x34, (ee_read<uint16_t>(m + 0x34) & ~0x1020u) | (ri(0, 4) ? 0x1000u : 0u) | (ri(0, 1) ? 0x20u : 0u));
            w32(m + 0x24, ri(0, 7) ? cls : 0u); w16(cls + 0x46, ri(0, 3) ? 5u : u(ri(0, 6)));
            w32(m + 0x78, pv); w32(pv, ri(0, 2) ? pw : 0u); wf(pw + 0x10, rf(0, 2)); w8(pw + 0xa, u(ri(0, 40)));
        }
        w32(prev + 0x28, 0);
        uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        for (int tries = 0; tries < 8 && (ee_read<uint32_t>(m + 0xa0) & 0x80) != 0; ++tries) m = T4 + 0x100 * u(ri(0, k - 1));  // one in the grid
        const float tx = ee_read<float>(m + 0x10), ty = ee_read<float>(m + 0x14), tz = ee_read<float>(m + 0x18);
        const float d = ri(0, 2) ? rf(1, 9) : rf(5, 40), a = rf(-3.2f, 3.2f), b = rf(-0.6f, 0.6f);
        const float fx = std::max(tx + d * std::cos(a) * std::cos(b), 1.0f), fy = std::max(ty + d * std::sin(a) * std::cos(b), 1.0f), fz = std::max(tz + d * std::sin(b), 1.0f);
        wf(OUT, fx); wf(OUT + 4, fy); wf(OUT + 8, fz); wf(OUT + 12, 1.0f);
        float dx = tx - fx + rf(-1, 1), dy = ty - fy + rf(-1, 1), dz = tz - fz + rf(-1, 1);
        if (ri(0, 3) == 0) { dx = rf(-1, 1); dy = rf(-1, 1); dz = rf(-1, 1); }
        wf(OUT + 0x10, dx); wf(OUT + 0x14, dy); wf(OUT + 0x18, dz); wf(OUT + 0x1c, 0.0f);
        wf(OV(0x00166dc0), fx + rf(-2, 2)); wf(OV(0x00166dc4), fy + rf(-2, 2)); wf(OV(0x00166dc8), fz + rf(0, 2)); wf(OV(0x00166dcc), 1.0f);
        wf(g::PLAYER + 0x80, fx + rf(-1, 1)); wf(g::PLAYER + 0x84, fy + rf(-1, 1)); wf(g::PLAYER + 0x88, fz - 1);
        w32(g::PLAYER + 0x2080, M3); wf(M3 + 0x48, ri(0, 1) ? std::atan2(ty - fy, tx - fx) + rf(-1.2f, 1.2f) : rf(-3.2f, 3.2f));
        wf(g::PLAYER + 0x2e4, rf(-0.3f, 0.3f));
        treeenv((fx + tx) / 2, (fy + ty) / 2, (fz + tz) / 2);
        wrand(OUT + 0x20, 3);
        return m;
    };
    check("auto_aim_target", 0x002630c8, N * 32, Ret::Int32, [aimenv] {
        const uint32_t m = aimenv();
        return Args{{ri(0, 2) ? 0u : m, OUT, OUT + 0x10, OUT + 0x20, OUT + 0x24, OUT + 0x28}, {rf(0.02f, 0.8f)}};
    }, [](A a) { return ri32(static_cast<int32_t>(auto_aim_target(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.f[0]))); });

    check("hero_wall_slide", 0x00212318, N * 32, Ret::None, [collenv, treeenv, heroAt, aimTri] {
        constexpr uint32_t P = g::PLAYER;
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float px = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4), py = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4), pz = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4);
        const float yaw = rf(-3.2f, 3.2f);
        heroAt(px, py, pz, yaw);
        w8(P + 0x20b3, 0); w16(P + 0x308, ri(0, 7) ? 0u : 1u); wf(0x0015ed6c, rf(0.5f, 2));
        static const uint32_t ss[] = {0, 0x11, 0x12, 3};
        w32(P + 0x208c, ss[ri(0, 3)]); w8(P + 0x20a4, u(ri(0, 3)));
        wf(P + 0x98, rf(-3.2f, 3.2f)); w32(P + 0x2080, ri(0, 3) ? 0u : m);
        float sp = ri(0, 7) ? rf(0.1f, 1) : rf(0, 0.2f), va = ri(0, 1) ? ee_read<float>(P + 0x98) + rf(-0.5f, 0.5f) : rf(-3.2f, 3.2f);
        wf(P + 0xe0, sp * std::cos(va)); wf(P + 0xe4, sp * std::sin(va)); wf(P + 0xe8, rf(-0.3f, 0.3f)); wf(P + 0xec, rf(-1, 1));
        treeenv(px, py, pz);
        std::array<float, 3> tg{};
        if (ri(0, 3) && aimTri(m, tg)) {  // a triangle just ahead along the velocity or facing
            const float dir = ri(0, 1) ? va : ee_read<float>(P + 0x98);
            const float d = rf(0.1f, 2);
            heroAt(tg[0] - std::cos(dir) * d, tg[1] - std::sin(dir) * d, tg[2] - rf(-0.3f, 1.2f), yaw);
        }
        return Args{{u(ri(0, 3) == 0)}, {}};
    }, [](A a) { hero_wall_slide(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });

    check("hero_shadow_update", 0x00208b60, N * 32, Ret::None, [collenv, treeenv] {
        constexpr uint32_t P = g::PLAYER;
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float x = ee_read<float>(m + 0x10) + rf(-1.5f, 1.5f), y = ee_read<float>(m + 0x14) + rf(-1.5f, 1.5f), z = ee_read<float>(m + 0x18) + rf(0, 3);
        if (ri(0, 1)) { x = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); y = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); z = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4); }
        x = std::max(x, 5.0f); y = std::max(y, 5.0f); z = std::max(z, 5.0f);
        wf(M, x * 1024); wf(M + 4, y * 1024); wf(M + 8, z * 1024); wf(M + 0xc, rf(0.2f, 1.5f) * 1024);
        wfrand(OV(0x0017fec0), 0x40 * 16, 0, 255); wrand(M + 0x38, 2); if (coin()) w8(M + 0x3a, 0);
        w32(P + 0x2080, M); w16(M + 0x34, R()); wrand(M + 0x84, 2);
        static const uint32_t ss[] = {0, 4, 0x12, 0x15, 0x16, 3, 0, 0};
        w32(P + 0x208c, ss[ri(0, 7)]); w32(P + 0x2084, ri(0, 7) ? 0u : 0x7du); w16(P + 0x30e, ri(0, 3) ? 0u : 1u);
        wf(P + 0xd20, ri(0, 3) ? rf(0.9f, 1.55f) : rf(0.6f, 1.6f));
        wf(P + 0x2d8, ri(0, 1) ? rf(1, 9) : rf(-1, 1)); wf(P + 0x2f0, ri(0, 1) ? rf(1, 9) : rf(-1, 1)); wf(P + 0x2f4, ri(0, 1) ? rf(1, 9) : rf(-1, 1));
        if (ri(0, 1)) wf(P + 0x2d8, z - rf(0, 4));
        for (int i = 0; i < 4; ++i) { wfrand(OV(0x0017c300) + 0x10 * u(i), 3, -1, 1); wf(OV(0x0017c30c) + 0x10 * u(i), 0); }
        treeenv(x, y, std::max(z - 2, 0.5f));
        return Args{};
    }, [](A) { hero_shadow_update(); return uint64_t{0}; });

    auto pushenv = [collenv, treeenv, heroAt, aimTri] {
        constexpr uint32_t P = g::PLAYER;
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float x = ee_read<float>(m + 0x10) + rf(-1.5f, 1.5f), y = ee_read<float>(m + 0x14) + rf(-1.5f, 1.5f), z = ee_read<float>(m + 0x18) + rf(-1.5f, 1);
        if (ri(0, 2) == 0) { x = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); y = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4); z = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4); }
        x = std::max(x, 5.0f); y = std::max(y, 5.0f); z = std::max(z, 5.0f);
        treeenv(x, y, z);
        std::array<float, 3> tg{};
        if (ri(0, 1) && aimTri(m, tg)) { x = tg[0] + rf(-0.5f, 0.5f); y = tg[1] + rf(-0.5f, 0.5f); z = tg[2] + rf(-1.2f, 0.3f); }
        heroAt(x, y, z, rf(-3.2f, 3.2f));
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) wf(P + 0x40 + 0x10 * u(r) + 4 * u(c), ee_read<float>(P + 0x10 * u(c) + 4 * u(r)));
        w32(P + 0x1cc, ri(0, 15) ? 0u : 1u); w16(P + 0x1f8, ri(0, 3) ? 0u : 1u);
        static const uint32_t ss[] = {0, 0x11, 0xf, 3, 0, 0};
        w32(P + 0x208c, ss[ri(0, 5)]); w8(P + 0x20b3, ri(0, 3) ? 0u : u(ri(1, 2)));
        w32(P + 0x2084, ri(0, 3) ? 0u : 0x7fu); w32(P + 0x2080, ri(0, 2) ? M3 : m);
        const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
        if (hm == M3) { wf(M3 + 0x40, rf(-0.2f, 0.2f)); wf(M3 + 0x44, rf(-0.2f, 0.2f)); wf(M3 + 0x48, rf(-3, 3)); wf(M3 + 0x4c, 0); }
        wf(P + 0x224, rf(0.2f, 0.8f)); wf(P + 0x220, ee_read<float>(P + 0x224) + rf(-0.1f, 1.2f)); wf(P + 0x234, ri(0, 1) ? rf(0.02f, 0.3f) : rf(0.2f, 1));
        wf(0x0015ed60, rf(0.8f, 1.5f));
        w32(OV(0x00166c00u - 0x75e8), 0);
        wrand(P + 0x200, 8); wrand(P + 0x23c, 1); w8(P + 0x257, 0);
        w16(P + 0x22da, 0);
        if (ri(0, 7) == 0) {
            const uint32_t idx = u(ri(0, 3)); w16(P + 0x22da, idx); w32(OV(0x001b04b0) + idx * 4, T);
            const int n = ri(1, 6); w32(T, u(n));
            for (int i = 0; i < n; ++i) { wf(T + 0x10 + 16 * i, x + rf(-3, 3)); wf(T + 0x14 + 16 * i, y + rf(-3, 3)); wf(T + 0x18 + 16 * i, z + rf(-3, 3)); wf(T + 0x1c + 16 * i, ri(0, 3) ? rf(0.5f, 3) : 0.0f); }
            Vec4 ax{0.0f, 0.0f, 1.0f, 0.0f}; ee_ref<Vec4>(0x0013f5e0) = ax; w16(P + 0x1f6, R());
        }
    };
    {
    // (stage 5, task C2b) hero_collide_push reaches sphere_collide / capsule_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's two checks)
    const CollisionScratchIgnored spr_ignored;
    check("hero_collide_push", 0x002133a8, N * 32, Ret::Int32, [pushenv] {
        pushenv();
        return Args{{ri(0, 1) ? 0xfu : u(ri(0, 3))}, {}};
    }, [](A a) { return ri32(hero_collide_push(static_cast<int32_t>(a.i[0]))); }, OV(0x00173e7c));
    }

    {
    // (stage 5, task C2b) hero_collide_resolve reaches sphere_collide / capsule_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's two checks)
    const CollisionScratchIgnored spr_ignored;
    check("hero_collide_resolve", 0x002137a8, N * 16, Ret::None, [pushenv] {
        constexpr uint32_t P = g::PLAYER;
        pushenv();
        const float x = ee_read<float>(P + 0x80), y = ee_read<float>(P + 0x84), z = ee_read<float>(P + 0x88);
        for (int i = 0; i < 32; ++i) { wf(P + 0x1b00 + 16 * u(i), x + rf(-1, 1)); wf(P + 0x1b04 + 16 * u(i), y + rf(-1, 1)); wf(P + 0x1b08 + 16 * u(i), z + rf(-1, 1)); wf(P + 0x1b0c + 16 * u(i), 1.0f); }
        w32(P + 0x21b0, u(ri(0, 31))); w32(P + 0x21b4, u(ri(0, 20)));
        return Args{};
    }, [](A) { hero_collide_resolve(); return uint64_t{0}; }, OV(0x00173e7c));
    }
    check("hero_footsteps_update", 0x00206080, N * 16, Ret::None, [sndenv] {
        constexpr uint32_t P = g::PLAYER;
        sndenv(); w32(0x0015ed84, u(ri(0, 3))); for (int i = 0; i < 4; ++i) w8(OV(0x001bd820) + i, u(ri(0, 2)));
        w32(OV(0x0015f574), 0); w32(OV(0x0015f5f0), u(ri(0, 8))); w32(OV(0x0015f5f4), T2);
        w32(P + 0x10e0, ri(0, 2) ? M3 : 0u); w16(M3 + 0xa6, ri(0, 1) ? 0xadu : 0xacu);
        wf(0x0015ed68, ri(0, 1) ? 1.0f : rf(0.5f, 2));
        w32(P + 0x2084, ri(0, 2) ? 2u : u(ri(0, 4))); w32(P + 0x198, u(ri(0, 40))); w8(P + 0x12ed, u(ri(0, 2)));
        const uint32_t hm = ee_read<uint32_t>(P + 0x2080); w8(hm + 0x53, u(ri(2, 5))); w32(P + 0xa9c, ri(0, 5) ? 0u : 1u);
        static const float lims[] = {49.5f, 17.0f, 12.5f, 1.0f};
        wf(P + 0xaa8, ri(0, 3) ? lims[ri(0, 3)] + rf(0, 1) : rf(0, 60)); wf(P + 0xaac, rf(0.5f, 3));
        return Args{};
    }, [](A) { hero_footsteps_update(); return uint64_t{0}; });

    check("spawn_projectile_100", 0x002a96f8, N * 16, Ret::Int32, [aimenv, spawnenv] {
        const uint32_t m = aimenv();
        if (1000.0f < ee_read<float>(OUT) || 1000.0f < ee_read<float>(OUT + 4)) { wf(OUT, rf(40, 100)); wf(OUT + 4, rf(40, 100)); wf(OUT + 8, rf(20, 40)); }  // no moby in the grid
        spawnenv();
        for (int i = 0; i < 48; ++i) { const uint32_t pw = 0x01ee8000 + 0x40 * u(i); w8(pw + 0xb, u(ri(0, 1))); w8(pw + 0xc, ri(0, 1) ? 0u : u(ri(1, 50))); w16(pw + 0x1e, R()); }
        w32(OV(0x0015ffe8), 0x01ef0000); w8(OV(0x00197bc0) + 0x100, u(ri(0, 3))); wf(OV(0x001613c4), rf(0.5f, 2)); wf(0x0015ed6c, rf(0.5f, 2));
        wfrand(g::PLAYER + 0x140, 4, -0.3f, 0.3f); w32(g::PLAYER + 0x2080, ri(0, 1) ? M3 : m);
        w16(OV(0x001b0770), u(ri(0, 0x1f)));
        const float dx = ee_read<float>(OUT + 0x10), dy = ee_read<float>(OUT + 0x14), dz = ee_read<float>(OUT + 0x18);
        const float ry = std::atan2(dy, dx), rx = std::atan2(dz, std::sqrt(dx * dx + dy * dy));
        wf(OUT + 0x40, ee_read<float>(OUT) + rf(-2, 2)); wf(OUT + 0x44, ee_read<float>(OUT + 4) + rf(-2, 2)); wf(OUT + 0x48, ee_read<float>(OUT + 8) + rf(-1, 1)); wf(OUT + 0x4c, 1.0f);
        return Args{{OUT + 0x30, OUT, ri(0, 2) ? 0u : m}, {rf(-3, 3), rf(-1, 1), rf(0, 2), ry, rx}};
    }, [](A a) { return r32(spawn_projectile_100(a.i[0], a.i[1], a.i[2], a.f[0], a.f[1], a.f[2], a.f[3], a.f[4])); });

    check("hero_fall_time", 0x00232b90, N * 32, Ret::Float, [collenv, treeenv, heroAt, aimTri] {
        constexpr uint32_t P = g::PLAYER;
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float x = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4), y = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4), z = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4);
        treeenv(x, y, std::max(z - 3, 0.5f));
        std::array<float, 3> tg{};
        if (ri(0, 3) && aimTri(m, tg)) { x = tg[0] + rf(-0.3f, 0.3f); y = tg[1] + rf(-0.3f, 0.3f); z = tg[2] + rf(0.1f, 3); }
        heroAt(x, y, z, rf(-3.2f, 3.2f));
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) wf(P + 0x40 + 0x10 * u(r) + 4 * u(c), ee_read<float>(P + 0x10 * u(c) + 4 * u(r)));
        w8(P + 0x20b3, ri(0, 2) ? 0u : u(ri(1, 3)));
        wf(P + 0xe0, rf(-0.1f, 0.1f)); wf(P + 0xe4, rf(-0.1f, 0.1f)); wf(P + 0xe8, rf(-0.2f, 0.3f)); wf(P + 0xec, 0);
        Vec4 up{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), 1.0f, 0.0f}; ee_ref<Vec4>(P + 0x270) = up;
        w32(P + 0x2080, M3); wfrand(M3 + 0x40, 3, -0.3f, 0.3f);
        const float h = ri(0, 1) ? rf(-1, -0.01f) : z - rf(-1, 3);
        return Args{{}, {rf(1, 60), rf(0.003f, 0.05f), h}};
    }, [](A a) { return rf32(hero_fall_time(a.f[0], a.f[1], a.f[2])); });

    check("hero_look_at_update", 0x0020ac98, N * 32, Ret::None, [collenv, treeenv, heroAt] {
        constexpr uint32_t P = g::PLAYER;
        const int k = collenv();
        const int n = ri(0, 4) ? ri(2, 10) : ri(0, 1);
        for (int i = 0; i < 48; ++i) {
            const uint32_t m = T4 + 0x100 * u(i), pv = 0x01ee4000 + 0x40 * u(i), pw = 0x01ee8000 + 0x40 * u(i);
            w16(m + 0x34, (ee_read<uint16_t>(m + 0x34) & ~0x20u) | (ri(0, 9) ? 0x20u : 0u)); w32(m + 0x78, pv); w32(pv, ri(0, 9) ? pw : 0u);
            w8(pw + 0x38, u(ri(0, 40))); w8(pw + 0x39, ri(0, 4) ? 0u : u(ri(1, 3))); w8(pw + 0x3a, u(ri(0, 30)));
        }
        for (int i = 0; i < n; ++i) w32(OV(0x001aba00) + 4 * u(i), T4 + 0x100 * u(ri(0, k - 1)));
        w32(OV(0x001aba00) + 4 * u(n), 0);
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        const float x = std::max(ee_read<float>(m + 0x10) + rf(-10, 10), 5.0f), y = std::max(ee_read<float>(m + 0x14) + rf(-10, 10), 5.0f), z = std::max(ee_read<float>(m + 0x18) + rf(-3, 3), 5.0f);
        const float yaw = ri(0, 1) ? std::atan2(ee_read<float>(m + 0x14) - y, ee_read<float>(m + 0x10) - x) + rf(-1, 1) : rf(-3.2f, 3.2f);
        heroAt(x, y, z, yaw); wf(P + 0x98, yaw);
        if (n >= 2 && ri(0, 2) == 0) {  // close to the current target, a rival at a similar score
            const uint32_t c = ee_read<uint32_t>(OV(0x001aba00)), r2 = ee_read<uint32_t>(OV(0x001aba04));
            const float a = rf(-3.2f, 3.2f), d = rf(0.2f, 2.5f);
            const float hx = ee_read<float>(c + 0x10) - d * std::cos(a), hy = ee_read<float>(c + 0x14) - d * std::sin(a);
            heroAt(hx, hy, ee_read<float>(c + 0x18), a); wf(P + 0x98, a + rf(-0.3f, 0.3f));
            const float d2 = d + rf(-1.5f, 1.5f), a2 = a + rf(-0.3f, 0.3f);
            wf(r2 + 0x10, hx + d2 * std::cos(a2)); wf(r2 + 0x14, hy + d2 * std::sin(a2)); wf(r2 + 0x18, ee_read<float>(c + 0x18));
            w32(OV(0x0015f5cc), 7u * u(ri(0, 100)));
        }
        treeenv(x, y, z);
        static const uint32_t ss[] = {0, 1, 2, 4, 7, 3, 9, 0};
        w32(P + 0x208c, ss[ri(0, 7)]); w32(P + 0x2080, M3); w8(M3 + 0x53, ri(0, 7) ? 0u : 0x54u);
        w32(OV(0x0015f5cc), ri(0, 1) ? 7u * u(ri(0, 100)) : R());
        w32(P + 0x2274, ri(0, 1) && n > 0 ? ee_read<uint32_t>(OV(0x001aba00) + 4 * u(ri(0, std::min(n - 1, 1)))) : (ri(0, 2) ? 0u : m));
        w32(P + 0x1c0, u(ri(0, 60))); wf(0x0015ed64, rf(0.5f, 2)); wf(0x0015ed68, ri(0, 1) ? 1.0f : rf(0.5f, 2));
        wrand(g::FX_PARAMS + 0x1c0, 4); wrand(g::FX_PARAMS + 0x200, 4); wrand(g::FX_PARAMS + 0x270, 4); wrand(g::FX_PARAMS + 0x2b0, 4);
        return Args{};
    }, [](A) { hero_look_at_update(); return uint64_t{0}; });

    // after collenv: moby slot 47 becomes an axis-aligned box (bottom centre (cx, cy, z0), half
    // sizes hx / hy, height h; outward front faces of surface type `type`) in the moby grid
    auto boxenv = [](float cx, float cy, float z0, float hx, float hy, float h, uint32_t type) {
        const uint32_t m = T4 + 0x100 * 47, col = 0x01eb0000 + 0x400 * 47;
        auto cell = [](float v) { return static_cast<uint32_t>(std::max(v, 0.0f) / 16.0f); };
        moby_grid_move(m, cell(cx - hx) | cell(cy - hy) << 8 | cell(cx + hx) << 16 | cell(cy + hy) << 24);
        wf(m + 0x10, cx); wf(m + 0x14, cy); wf(m + 0x18, z0); wf(m + 0x1c, 1.0f);
        wf(m, cx * 1024); wf(m + 4, cy * 1024); wf(m + 8, (z0 + h / 2) * 1024); wf(m + 0xc, std::sqrt(hx * hx + hy * hy + h * h / 4) * 1024 + 16);
        for (uint32_t o = 0xc0; o < 0xf0; o += 4) wf(m + o, (o - 0xc0) % 0x14 == 0 ? 1.0f : 0.0f);
        wf(m + 0x2c, 1.0f); w16(m + 0x34, 0x4000); w8(m + 0xa4, 0xff); w32(m + 0x9c, 0); w32(m + 0x94, col);
        w16(col, 0); w16(col + 2, 0); w32(col + 4, 0); w32(col + 8, 12 * 4); w32(col + 0xc, 4 * 0x10);
        std::array<std::array<float, 3>, 8> v{};
        for (int i = 0; i < 8; ++i) {
            v[static_cast<size_t>(i)] = {(i & 1) ? hx : -hx, (i & 2) ? hy : -hy, (i & 4) ? h : 0.0f};
            for (int c = 0; c < 3; ++c) w16(col + 0x10 + 8 * u(i) + 2 * u(c), static_cast<uint32_t>(static_cast<int32_t>(std::lround(v[static_cast<size_t>(i)][static_cast<size_t>(c)] * 1024))) & 0xffffu);
            w16(col + 0x16 + 8 * u(i), 0);
        }
        // quads (corner indices) with their outward normal axis / sign
        static const int quads[6][4] = {{4, 5, 7, 6}, {0, 1, 3, 2}, {1, 3, 7, 5}, {0, 2, 6, 4}, {2, 3, 7, 6}, {0, 1, 5, 4}};
        static const float nrm[6][3] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        const uint32_t tb = col + 0x10 + 0x40;
        int t = 0;
        for (int f = 0; f < 6; ++f) {
            for (int half = 0; half < 2; ++half) {
                int a = quads[f][0], b = quads[f][1 + half], c = quads[f][2 + half];
                auto d = [&](int i, int j, int k) { return v[static_cast<size_t>(i)][static_cast<size_t>(k)] - v[static_cast<size_t>(j)][static_cast<size_t>(k)]; };
                // front when (v2 - v0) x (v1 - v0) points along the outward normal
                const float cx2 = d(c, a, 1) * d(b, a, 2) - d(c, a, 2) * d(b, a, 1), cy2 = d(c, a, 2) * d(b, a, 0) - d(c, a, 0) * d(b, a, 2), cz2 = d(c, a, 0) * d(b, a, 1) - d(c, a, 1) * d(b, a, 0);
                if (cx2 * nrm[f][0] + cy2 * nrm[f][1] + cz2 * nrm[f][2] < 0) std::swap(b, c);
                w8(tb + 4 * u(t), u(a)); w8(tb + 4 * u(t) + 1, u(b)); w8(tb + 4 * u(t) + 2, u(c)); w8(tb + 4 * u(t) + 3, type);
                ++t;
            }
        }
        return m;
    };
    check("hero_ledge_check", 0x0020c758, N * 32, Ret::Int32, [collenv, treeenv, heroAt, aimTri, boxenv] {
        constexpr uint32_t P = g::PLAYER;
        const int k = collenv();
        const uint32_t m = T4 + 0x100 * u(ri(0, k - 1));
        float x = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4), y = 4.0f * static_cast<float>(ri(10, 30)) + rf(0, 4), z = 4.0f * static_cast<float>(ri(3, 12)) + rf(0, 4);
        float yaw = rf(-3.2f, 3.2f);
        treeenv(x, y, z);
        wf(P + 0x234, rf(0.2f, 0.6f)); w8(P + 0x20a4, ri(0, 3) ? 0u : 1u); w16(P + 0x308, ri(0, 7) ? 0u : 2u);
        std::array<float, 3> tg{};
        if (ri(0, 3) && aimTri(m, tg)) {
            const float d = ee_read<float>(P + 0x234) + rf(0, 0.4f);
            x = tg[0] - std::cos(yaw) * d; y = tg[1] - std::sin(yaw) * d; z = tg[2] - (ee_read<uint8_t>(P + 0x20a4) ? rf(0.5f, 0.9f) : rf(1.1f, 1.7f));
        }
        if (ri(0, 1)) {  // a box ledge in front of the hero
            const float cx = rf(50, 90), cy = rf(50, 90), hx = rf(0.5f, 4), hy = rf(0.5f, 4), hh = rf(0.5f, 4), z0 = rf(20, 40);
            boxenv(cx, cy, z0, hx, hy, hh, ri(0, 5) ? 0u : u(ri(0, 15)));
            const int side = ri(0, 3);
            const float along = rf(-0.9f, 0.9f), gap = ee_read<float>(P + 0x234) + (ri(0, 3) ? rf(0, 0.25f) : rf(0, 0.6f));
            const float face = side == 0 ? 3.14159f : side == 1 ? 0.0f : side == 2 ? -1.5708f : 1.5708f;  // facing the wall
            x = side == 0 ? cx + hx + gap : side == 1 ? cx - hx - gap : cx + along * hx;
            y = side == 2 ? cy + hy + gap : side == 3 ? cy - hy - gap : cy + along * hy;
            if (side < 2) y = cy + along * hy;
            yaw = face + rf(-0.4f, 0.4f);
            z = z0 + hh - (ee_read<uint8_t>(P + 0x20a4) ? rf(0.65f, 0.9f) : (ri(0, 3) ? rf(1.15f, 1.65f) : rf(0.9f, 1.8f)));
        }
        heroAt(x, y, z, yaw); wf(P + 0x98, yaw); w32(P + 0x2080, ri(0, 2) ? M3 : m);
        wf(P + 0x2d8, ri(0, 1) ? z - rf(-0.5f, 1) : z - rf(0.5f, 2));
        wf(OUT, x); wf(OUT + 4, y); wf(OUT + 8, z); wf(OUT + 12, 1.0f);
        wf(OUT + 0x20, ri(0, 3) ? yaw + rf(-0.6f, 0.6f) : rf(-3.2f, 3.2f));
        return Args{{OUT, OUT + 0x20}, {}};
    }, [](A a) { return ri32(hero_ledge_check(a.i[0], hp<float>(a.i[1]))); });

    check("hero_air_wall_check", 0x0020b5b8, N * 32, Ret::None, [collenv, treeenv, heroAt, boxenv] {
        constexpr uint32_t P = g::PLAYER;
        collenv();
        const float cx = rf(50, 90), cy = rf(50, 90), hx = rf(0.5f, 4), hy = rf(0.5f, 4), hh = rf(1, 5), z0 = rf(20, 40);
        boxenv(cx, cy, z0, hx, hy, hh, ri(0, 5) ? 0u : u(ri(0, 15)));
        wf(P + 0x234, rf(0.2f, 0.6f)); w8(P + 0x20a4, ri(0, 3) ? 0u : 1u); w16(P + 0x308, ri(0, 7) ? 0u : 2u);
        const int side = ri(0, 3);
        const float along = rf(-0.9f, 0.9f), gap = ee_read<float>(P + 0x234) + rf(-0.1f, 1.5f);
        const float face = side == 0 ? 3.14159f : side == 1 ? 0.0f : side == 2 ? -1.5708f : 1.5708f;
        float x = side == 0 ? cx + hx + gap : side == 1 ? cx - hx - gap : cx + along * hx;
        float y = side == 2 ? cy + hy + gap : side == 3 ? cy - hy - gap : cy + along * hy;
        if (side < 2) y = cy + along * hy;
        const float yaw = face + (ri(0, 2) ? rf(-0.4f, 0.4f) : rf(-3.2f, 3.2f));
        const float z = z0 + hh - rf(-0.5f, 3.5f);
        heroAt(x, y, z, yaw); wf(P + 0x98, yaw);
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) wf(P + 0x40 + 0x10 * u(r) + 4 * u(c), ee_read<float>(P + 0x10 * u(c) + 4 * u(r)));
        treeenv(x, y, z);
        w8(P + 0x20b3, 0); w32(P + 0x2080, M3);
        wf(P + 0x2dc, ri(0, 5) ? rf(0.4f, 3) : rf(0, 0.4f)); wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed68, ri(0, 1) ? 1.0f : rf(0.5f, 2));
        wf(P + 0xe0, rf(-0.2f, 0.2f)); wf(P + 0xe4, rf(-0.2f, 0.2f)); wf(P + 0xe8, ri(0, 7) ? rf(-0.3f, 0.3f) : rf(-40, -20)); wf(P + 0xec, 0);
        w32(P + 0x4e8, ri(0, 7) ? 0u : 1u); wf(P + 0x4a0, rf(0, 0.02f));
        const float sp = ri(0, 1) ? 0.0f : rf(0, 0.05f);
        wf(P + 0x100, sp * std::cos(yaw)); wf(P + 0x104, sp * std::sin(yaw));
        wf(P + 0x2d8, z - rf(0, 3));
        w32(P + 0x2084, ri(0, 3) ? 0u : 0x11u); w8(P + 0x255, u(ri(0, 1))); w8(P + 0x254, u(ri(0, 1)));
        w32(P + 0x208c, ri(0, 1) ? 4u : u(ri(0, 6))); w32(P + 0x2094, ri(0, 3) ? 0u : 4u); w32(P + 0x20a0, ri(0, 3) ? 0u : 4u);
        w32(P + 0x22b4, u(ri(0, 1))); wf(P + 0x47c, rf(-3.2f, 3.2f));
        wrand(P + 0x1b4, 1); wrand(P + 0x1f0, 1); wrand(P + 0x460, 4);
        return Args{};
    }, [](A) { hero_air_wall_check(); return uint64_t{0}; });

    check("hero_slide_move", 0x002167d0, N * 32, Ret::None, [collenv, treeenv, heroAt, boxenv] {
        constexpr uint32_t P = g::PLAYER;
        collenv();
        float x = rf(50, 90), y = rf(50, 90), z = rf(20, 40);
        const float yaw = rf(-3.2f, 3.2f);
        if (ri(0, 1)) boxenv(x + std::cos(yaw) * rf(0.5f, 2), y + std::sin(yaw) * rf(0.5f, 2), z - 1, rf(0.3f, 2), rf(0.3f, 2), rf(1, 4), 0);
        heroAt(x, y, z, yaw);
        treeenv(x, y, z);
        const int kk = ri(0, 5);
        wf(P + 0x1d20, kk == 0 ? 0.0f : rf(-1.2f, 1.2f)); wf(P + 0x1d24, kk == 0 ? 0.0f : rf(-1.2f, 1.2f));
        w8(P + 0x20b3, 0); wf(P + 0x98, yaw); wf(OV(0x00166dd8), rf(-3, 3)); w8(0x0015edb4, u(ri(0, 1)));
        Mat4 r; Vec4 e{rf(-1, 1), rf(-1, 1), rf(-3, 3), 0.0f}; euler_to_mat4_fpu(r, e); ee_ref<Mat4>(OV(0x00166fd0)) = r;
        euler_to_mat4_fpu(r, Vec4{0.0f, 0.0f, yaw, 0.0f}); w32(g::HERO_MOBY, M); ee_ref<Mat4>(M + 0xc0) = r;
        w32(P + 0x2080, M3); wf(M3 + 0x48, yaw);
        wf(P + 0x184, rf(-0.5f, 0.5f)); wf(P + 0x180, rf(-3, 3)); wfrand(0x0013f4c0, 4, -3, 3); wf(0x0013f4d4, rf(-0.5f, 0.5f));
        wf(0x0015ed64, rf(0.5f, 2)); wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed70, rf(0.005f, 0.05f)); wf(0x0015ed68, ri(0, 1) ? 1.0f : rf(0.5f, 2));
        static const uint32_t st[] = {0x13, 0x15, 0x14, 0x13};
        w32(P + 0x2084, st[ri(0, 3)]); wf(P + 0x229c, rf(0, 0.5f)); wf(P + 0x190, rf(0, 1));
        wf(P + 0x930, ri(0, 2) ? 0.0f : rf(0, 1)); wf(P + 0x934, rf(-3, 3)); wf(P + 0x938, rf(-3, 3)); wfrand(P + 0x920, 4, -0.3f, 0.3f); wrand(0x0013fc70, 4);
        wfrand(P + 0xe0, 4, -0.4f, 0.4f); wfrand(P + 0x130, 4, -0.4f, 0.4f); w32(0x0013cb14, ri(0, 7) ? 0u : 1u);
        w32(P + 0x198, u(ri(0, 30))); wf(P + 0x160, rf(0, 8));
        Vec4 nn{rf(-0.4f, 0.4f), rf(-0.4f, 0.4f), 1.0f, 0.0f}; ee_ref<Vec4>(P + 0x270) = nn; wf(P + 0x2e0, ri(0, 1) ? rf(0, 0.1f) : rf(0, 0.8f));
        wf(P + 0x2e4, rf(0, 0.5f)); wf(P + 0x3b4, ri(0, 1) ? 0.0f : rf(0, 0.5f)); wf(P + 0x2d8, z - (ri(0, 1) ? rf(0, 0.2f) : rf(0, 2)));
        w16(P + 0x308, 0); w32(P + 0x208c, u(ri(0, 4))); w8(P + 0x20a4, u(ri(0, 2)));
        return Args{};
    }, [](A) { hero_slide_move(); return uint64_t{0}; });

    check("hero_ground_update", 0x002127b8, N * 32, Ret::None, [collenv, treeenv, heroAt, boxenv] {
        constexpr uint32_t P = g::PLAYER;
        collenv();
        float x = rf(50, 90), y = rf(50, 90), z = rf(20, 40);
        const float yaw = rf(-3.2f, 3.2f);
        if (ri(0, 2)) {  // standing on (or near) a box
            const float h = rf(0.5f, 3);
            boxenv(x + rf(-1, 1), y + rf(-1, 1), z - h, rf(0.3f, 2), rf(0.3f, 2), h, ri(0, 3) ? 0u : (ri(0, 1) ? u(ri(0, 15)) : 0xdu));
            z = z + (ri(0, 1) ? rf(0, 0.03f) : rf(-0.3f, 2));
        }
        heroAt(x, y, z, yaw); wf(P + 0x98, yaw);
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) wf(P + 0x40 + 0x10 * u(r) + 4 * u(c), ee_read<float>(P + 0x10 * u(c) + 4 * u(r)));
        treeenv(x, y, std::max(z - 4, 0.5f));
        w8(P + 0x20b3, ri(0, 5) ? 0u : u(ri(1, 3)));
        Vec4 up{0.0f, 0.0f, 1.0f, 0.0f}; ee_ref<Vec4>(P + 0x290) = up; wfrand(P + 0xb0, 4, 0, 100);
        wf(P + 0x234, rf(0.2f, 0.6f)); w32(P + 0x2080, M3); w32(g::HERO_MOBY, M3); wfrand(M3 + 0x40, 3, -0.3f, 0.3f); wf(M3 + 0x4c, 0);
        static const uint32_t ss[] = {0, 2, 3, 0x16, 0xf, 1, 0, 0};
        w32(P + 0x208c, ss[ri(0, 7)]); w32(P + 0x2084, ri(0, 5) ? 0u : u(ri(0x7c, 0x7d)));
        w16(P + 0x30e, R()); w16(P + 0x30c, R()); w32(P + 0x2f8, u(ri(-1, 3))); w32(P + 0x300, u(ri(0, 50)));
        wfrand(P + 0x270, 0x40, -1, 1); w32(0x0015ed84, u(ri(0, 0x12)));
        w32(OV(0x00166c00u - 0x6c20), T4 + 0x100 * 48);
        for (int i = 0; i < 48; ++i) { const uint32_t m = T4 + 0x100 * u(i); w32(m + 0x78, 0x01ee4000 + 0x40 * u(i)); w32(0x01ee4000 + 0x40 * u(i), ri(0, 1) ? 0x01ee8000 + 0x40 * u(i) : 0u); w16(0x01ee8000 + 0x40 * u(i) + 0x1e, R()); }
        w32(OV(0x001612d8), 0); w32(OV(0x001612dc), ri(0, 1)); wf(OV(0x001612e8), z - rf(-1, 1)); wfrand(OV(0x001612e0), 2, 0, 100); wf(OV(0x001612ec), rf(0, 100));
        return Args{};
    }, [](A) { hero_ground_update(); return uint64_t{0}; });

    {
    // (stage 5, task C2b) hero_ledge_grab_check reaches sphere_collide / capsule_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's two checks)
    const CollisionScratchIgnored spr_ignored;
    check("hero_ledge_grab_check", 0x0020bed0, N * 32, Ret::None, [collenv, treeenv, heroAt, boxenv] {
        constexpr uint32_t P = g::PLAYER;
        const int k = collenv();
        wf(P + 0x234, rf(0.2f, 0.6f)); w8(P + 0x20a4, ri(0, 3) ? 0u : (ri(0, 3) ? 1u : 2u)); w16(P + 0x308, ri(0, 7) ? 0u : 2u); w16(P + 0x1b0, ri(0, 7) ? 0u : 1u);
        const float cx = rf(50, 90), cy = rf(50, 90), hx = rf(0.5f, 4), hy = rf(0.5f, 4), hh = rf(0.5f, 4), z0 = rf(20, 40);
        const uint32_t box = boxenv(cx, cy, z0, hx, hy, hh, ri(0, 5) ? u(ri(1, 8)) : u(ri(0, 15)));
        const int side = ri(0, 3);
        const float along = rf(-0.9f, 0.9f), gap = ee_read<float>(P + 0x234) + (ri(0, 3) ? rf(0, 0.25f) : rf(0, 0.6f));
        const float face = side == 0 ? 3.14159f : side == 1 ? 0.0f : side == 2 ? -1.5708f : 1.5708f;
        float x = side == 0 ? cx + hx + gap : side == 1 ? cx - hx - gap : cx + along * hx;
        float y = side == 2 ? cy + hy + gap : side == 3 ? cy - hy - gap : cy + along * hy;
        if (side < 2) y = cy + along * hy;
        const float yaw = face + rf(-0.4f, 0.4f);
        const float z = z0 + hh - (ee_read<uint8_t>(P + 0x20a4) == 1 ? rf(0.65f, 0.9f) : (ri(0, 3) ? rf(1.15f, 1.65f) : rf(0.9f, 1.8f)));
        heroAt(x, y, z, yaw); wf(P + 0x98, yaw); w32(P + 0x2080, M3);
        treeenv(x, y, z);
        wf(P + 0x2d8, ri(0, 1) ? z - rf(-0.5f, 1) : z - rf(0.5f, 2));
        wfrand(P + 0x110, 3, -0.05f, 0.05f); wf(P + 0x11c, 0); wf(P + 0x940, rf(0, 0.02f)); wf(P + 0x4a0, rf(0, 0.02f));
        wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed68, ri(0, 1) ? 1.0f : rf(0.5f, 2));
        static const uint32_t ss[] = {4, 2, 0, 4};
        w32(P + 0x208c, ss[ri(0, 3)]); w16(P + 0x41e, u(ri(0, 1))); wf(P + 0xe8, rf(-1, 6));
        static const uint32_t st[] = {0, 8, 0x1a, 0x1b, 0xe, 0x4d, 0x4e, 0x4f};
        w32(P + 0x2084, st[ri(0, 7)]); wf(P + 0xaa8, rf(30, 45));
        // the box as a grabbable moby (pvars word 0 / word 2)
        const uint32_t pv = 0x01ee4000 + 0x40 * 47, pw = 0x01ee8000 + 0x40 * 47, w2 = 0x01ee8800;
        w16(box + 0x34, 0x4000u | (ri(0, 3) ? 0x20u : 0u)); w32(box + 0x78, pv); w32(pv, ri(0, 1) ? pw : 0u); w32(pv + 8, ri(0, 1) ? w2 : 0u);
        w16(pw + 0x1e, ri(0, 2) ? 1u : 0u); w32(w2 + 0x3c, u(ri(0, 1)));
        (void)k;
        wrand(P + 0x4d0, 4); wrand(P + 0x4e0, 8); w32(P + 0x4f4, R()); wrand(P + 0x4f8, 1);
        return Args{};
    }, [](A) { hero_ledge_grab_check(); return uint64_t{0}; });
    }

    auto movenv = [collenv, treeenv, heroAt, boxenv] {
        constexpr uint32_t P = g::PLAYER;
        collenv();
        float x = rf(50, 90), y = rf(50, 90), z = rf(20, 40);
        const float yaw = rf(-3.2f, 3.2f);
        if (ri(0, 2)) {
            const float h = rf(0.5f, 3);
            boxenv(x + rf(-1.5f, 1.5f), y + rf(-1.5f, 1.5f), z - h, rf(0.3f, 2), rf(0.3f, 2), h + (ri(0, 1) ? rf(0, 2) : 0.0f), ri(0, 3) ? 0u : u(ri(0, 15)));
            z = z + (ri(0, 1) ? rf(0, 0.03f) : rf(-0.3f, 2));
        }
        heroAt(x, y, z, yaw); wf(P + 0x98, yaw);
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) wf(P + 0x40 + 0x10 * u(r) + 4 * u(c), ee_read<float>(P + 0x10 * u(c) + 4 * u(r)));
        treeenv(x, y, std::max(z - 4, 0.5f));
        w8(P + 0x20b3, 0); Vec4 up{0.0f, 0.0f, 1.0f, 0.0f}; ee_ref<Vec4>(P + 0x290) = up;
        wf(P + 0x234, rf(0.2f, 0.6f)); w32(P + 0x2080, M3); w32(g::HERO_MOBY, M3); wfrand(M3 + 0x40, 3, -0.3f, 0.3f); wf(M3 + 0x4c, 0);
        static const uint32_t sts[] = {0, 0x22, 0x14, 3, 0, 0};
        w32(P + 0x2084, sts[ri(0, 5)]);
        static const uint32_t ss[] = {0, 0xf, 0xd, 0xe, 2, 0};
        w32(P + 0x208c, ss[ri(0, 5)]); w32(P + 0x1cc, ri(0, 7) ? 0u : 1u);
        wfrand(P + 0xe0, 3, -0.6f, 0.6f); wf(P + 0xec, 0); wfrand(P + 0x920, 3, -0.1f, 0.1f); wf(P + 0x92c, 0);
        if (ri(0, 2)) wfrand(P + 0xf0, 4, 0, 0); else { wfrand(P + 0xf0, 3, -0.3f, 0.3f); wf(P + 0xfc, rf(-0.2f, 0.2f)); }
        wf(P + 0x2dc, rf(-0.5f, 2)); w16(P + 0x30e, ri(0, 1) ? 0u : u(ri(1, 30))); w16(P + 0x30c, R()); w16(P + 0x1f8, 0);
        w32(P + 0x2f8, u(ri(-1, 3))); w32(P + 0x300, u(ri(0, 50))); w32(0x0015ed84, u(ri(0, 0x12)));
        // platform ride idle, collision resolve history, sub_00212ff0 inputs
        w32(P + 0x360, 0); w32(P + 0x2fc, 0); w32(P + 0x4f8, 0); w32(P + 0x364, 0); wfrand(P + 0x350, 4, -0.1f, 0.1f); wfrand(P + 0x150, 4, 0, 0);
        w16(P + 0x22da, 0); w32(OV(0x00166c00u - 0x75e8), 0); wf(P + 0x224, rf(0.2f, 0.8f)); wf(P + 0x220, ee_read<float>(P + 0x224) + rf(0, 1.2f));
        for (int i = 0; i < 32; ++i) { wf(P + 0x1b00 + 16 * u(i), x + rf(-1, 1)); wf(P + 0x1b04 + 16 * u(i), y + rf(-1, 1)); wf(P + 0x1b08 + 16 * u(i), z + rf(0, 1)); wf(P + 0x1b0c + 16 * u(i), 1.0f); }
        w32(P + 0x21b0, u(ri(0, 31))); w32(P + 0x21b4, u(ri(0, 20)));
        wf(P + 0x304, rf(-1, 1)); wfrand(P + 0x210, 2, -9, 9);
        wf(0x0015ed64, rf(0.5f, 2)); wf(0x0015ed60, rf(0.8f, 1.5f)); wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed70, rf(0.005f, 0.05f));
        w32(OV(0x00166c00u - 0x6c20), T4 + 0x100 * 48);
        for (int i = 0; i < 48; ++i) { const uint32_t m = T4 + 0x100 * u(i); w32(m + 0x78, 0x01ee4000 + 0x40 * u(i)); w32(0x01ee4000 + 0x40 * u(i), 0); w32(0x01ee4000 + 0x40 * u(i) + 8, 0); }
        w32(OV(0x001612d8), 0); w32(OV(0x001612dc), 0);
    };
    {
    // (stage 5, task C2b) hero_move_integrate reaches sphere_collide / capsule_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's two checks)
    const CollisionScratchIgnored spr_ignored;
    check("hero_move_integrate", 0x00213880, N * 32, Ret::None, [movenv] {
        movenv();
        return Args{};
    }, [](A) { hero_move_integrate(); return uint64_t{0}; }, OV(0x00173e7c));
    }

    {
    // (stage 5, task C2b) hero_motion_update reaches sphere_collide / capsule_collide: A2's collision scratch
    // ranges left out of this check (see batch 43's two checks)
    const CollisionScratchIgnored spr_ignored;
    check("hero_motion_update", 0x00221310, N * 32, Ret::None, [movenv] {
        constexpr uint32_t P = g::PLAYER;
        movenv();
        w8(P + 0x20ad, ri(0, 2) ? 0u : 1u); w32(OV(0x0015f5cc), ri(0, 1) ? 15u * u(ri(0, 50)) : R() & 0xffff);
        if (ri(0, 1)) w32(P + 0x300, u(ri(1, 9)));
        for (int i = 0; i < 48; ++i) w16(T4 + 0x100 * u(i) + 0xa6, u(ri(480, 560)));
        wrand(P + 0x248, 8);
        return Args{};
    }, [](A) { hero_motion_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }

    // the 13 camera controller slots with a key table for sub_00235988; no slot flagged 0x20 runs
    // a real controller (start callbacks only reach the port table)
    auto camenv = [] {
        const int n = ri(0, 8);
        for (int i = 0; i < n; ++i) { w16(T + 8 * i, u(ri(0, 12))); w16(T + 8 * i + 2, R()); w32(T + 8 * i + 4, R()); }
        w16(T + 8 * n, 0xffff); wrand(T + 8 * n + 2, 3);
        w32(OV(0x0017e4f4), T); w32(OV(0x0017e4d8), u(ri(0, 1000))); w32(OV(0x0015f5c4), ri(0, 3) ? 0u : 5u);
        for (int i = 0; i < 13; ++i) {
            const uint32_t e = OV(0x0017dc50) + 0x90 * u(i);
            wrand(e, 0x24); w32(e + 4, R() & ~0x20u); w32(e + 0x30, 0); w32(e + 0x10, 0);
            if (ri(0, 1)) { w32(e + 0x20, 0x7535); w32(e + 0x24, 0); w32(e + 0x28, 8); w32(e + 0x2c, 0x001415f8); w32(e + 0x30, OVF(0x002395b0)); w32(e + 0x34, OVF(0x00239630)); w32(e + 0x38, OVF(0x00239810)); }
        }
    };
    check("camera_request", 0x00235700, N * 8, Ret::Int32, [camenv] {
        camenv();
        const uint32_t slot = u(ri(0, 12)), e = OV(0x0017dc50) + 0x90 * slot;
        if (ri(0, 2) == 0) return Args{{slot | (ee_read<uint32_t>(e + 0x24) & 0xfff0), ee_read<uint32_t>(e + 0x20), ee_read<uint32_t>(e + 0x30), ee_read<uint32_t>(e + 0x34), ee_read<uint32_t>(e + 0x38), ee_read<uint32_t>(e + 0x2c), ee_read<uint32_t>(e + 0x28)}, {}};
        const bool startable = ri(0, 1);
        if (startable) w32(e + 4, ee_read<uint32_t>(e + 4) | 0x20u);
        return Args{{slot | (u(ri(0, 0xfff)) << 4), u(ri(0, 14)), 0u, R(), R(), R(), R()}, {}};
    }, [](A a) { return ri32(camera_request(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4], a.i[5], a.i[6])); });
    check("camera_slot_start", 0x00235810, N * 4, Ret::None, [camenv] {
        camenv();
        const uint32_t e = OV(0x0017dc50) + 0x90 * u(ri(0, 12));
        w32(e + 0x30, 0); w32(e + 0x20, u(ri(0, 14)));
        return Args{{e}, {}};
    }, [](A a) { camera_slot_start(a.i[0]); return uint64_t{0}; });
    check("camera_reset", 0x0023aef8, N * 2, Ret::None, [camenv] { camenv(); for (int i = 0; i < 13; ++i) w32(OV(0x0017dc50) + 0x90 * u(i) + 0x30, 0); wrand(OV(0x0015f968), 3); return Args{}; },
          [](A) { camera_reset(); return uint64_t{0}; });
    check("hero_counter_use", 0x002050c0, N * 4, Ret::None, [camenv] {
        camenv(); w32(g::PLAYER + 0x22a8, u(ri(0, 5)));
        return Args{{u(ri(-2, 4))}, {}};
    }, [](A a) { hero_counter_use(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("hero_camera_7_update", 0x00205ea8, N * 4, Ret::None, [camenv] {
        camenv(); w8(0x0013d4db, u(ri(0, 2) != 0)); wf(g::PLAYER + 0x2044, rf(0, 40)); w32(g::PLAYER + 0x2040, u(ri(0, 1))); w32(g::PLAYER + 0x2048, R());
        return Args{};
    }, [](A) { hero_camera_7_update(); return uint64_t{0}; });

    // ======== batches 45 / 46: hero state entry
    g_batch = 45;
    auto stateenv = [animenv, sndenv, spawnenv, particles2](uint32_t state) {
        constexpr uint32_t P = g::PLAYER;
        sndenv();
        spawnenv(); particles2(); w32(OV(0x001b2134), T4 + 0x800); w8(T4 + 0x800, R()); w32(OV(0x001b210c), T4 + 0x804); w8(T4 + 0x804, R()); w32(OV(0x001b2090), T4 + 0x808); w8(T4 + 0x808, R());
        animenv();
        w32(P + 0x2084, ri(0, 3) ? u(ri(0, 0x82)) : state); w32(P + 0x208c, u(ri(0, 4) ? ri(0, 0x13) : ri(0, 0x16)));
        w32(P + 0x2088, R()); { static const uint32_t ps[] = {3, 4, 6, 0x23, 0x2d, 0x33, 0x35, 0}; w32(P + 0x2090, ri(0, 1) ? ps[ri(0, 7)] : u(ri(0, 0x82))); } w32(P + 0x2094, ri(0, 3) ? u(ri(0, 0x16)) : 0x12u); w32(P + 0x198, u(ri(0, 100)));
        w32(OV(0x0015f5c4), ri(0, 7) ? u(ri(0, 1)) : u(ri(2, 6))); w32(P + 0x22a8, u(ri(0, 2)));
        w8(P + 0x20a4, 0); w8(P + 0x20a7, u(ri(0, 1))); w8(P + 0x20a8, u(ri(0, 1)));  // (modes 1..3 lead into other states)
        if (g_oracle_level == 1 && std::getenv("ORACLE_NOMODE") == nullptr && ri(0, 2) == 0) w8(P + 0x20a4, u(ri(1, 3)));  // (Novalis's build: the movement modes' paths must be absent too; Veldin's stream is untouched)
        w32(P + 0xd08, 0x00140000); w32(P + 0xd0c, 0x00140000); wrand(P + 0xd14, 1); wrand(P + 0x2294, 1);  // the item layers stay started
        for (uint32_t o : {0x20a5u, 0x20acu, 0x20adu, 0x20b2u, 0x20afu}) w8(P + o, u(ri(0, 1)));
        w32(P + 0x1670 + 0x138, 0); w32(P + 0x1670 + 0x13c, 0);
        for (int k = 0; k < 8; ++k) { w32(P + 0x2218 + 4 * k, 0xffffffffu); w32(P + 0x2238 + 4 * k, 0); }
        w16(P + 0x308, u(ri(0, 1))); w32(P + 0x1108, ri(0, 2) ? u(ri(0x1c, 0x1d)) : 0u); w32(P + 0x20f4, u(ri(0, 1))); wrand(P + 0x210c, 2);
        w32(P + 0xa84, 0); w32(P + 0xa8c, 0); w32(P + 0xa88, M); w32(P + 0x1620, 0); w32(P + 0x1624, 0); w16(P + 0x1634, 0xffff);
        wfrand(P + 0x110, 4, -0.5f, 0.5f); wfrand(P + 0x150, 4, -0.5f, 0.5f); w8(P + 0x12e2, 0); w8(P + 0x12ea, 0); w16(P + 0x30c, u(ri(0, 1)));
        wf(0x0015ed6c, rf(0.5f, 2)); wf(0x0015ed68, ri(0, 1) ? 1.0f : rf(0.5f, 2)); seed_rand();
        w32(P + 0x10b4, 2); w32(P + 0x10b8, u(ri(0, 0x24))); w32(P + 0x1614, 0);
        wf(P + 0x108, rf(-1, 1)); wf(P + 0x2f0, rf(0, 50)); wf(P + 0x88, rf(0, 50));
        wrand(P + 0x8e0, 8); wrand(P + 0xff8, 1); wrand(P + 0x690, 4); wrand(P + 0x19c, 3);
        w32(P + 0x22a0, u(ri(0, 10000))); w16(P + 0x90e, u(ri(0, 100))); w32(P + 0x910, u(ri(0, 5))); w32(0x0015eea4, u(ri(0, 100000))); w32(0x0015ed84, u(ri(0, 18)));
        for (uint32_t o : {0x60u, 0x62u, 0xe8u, 0xeau}) w16(0x00141848 + o, ri(0, 7) ? u(ri(0, 300)) : 0xffffu); wrand(0x00141848 + 0x64, 1); wrand(0x00141848 + 0xec, 1);
        w8(P + 0x20aa, u(ri(0, 1))); w8(P + 0x20a9, ri(0, 3) ? 0u : 1u); w16(P + 0x308, u(ri(0, 2) ? 0 : ri(1, 2))); w32(P + 0x418, u(ri(0, 10)));
        wf(P + 0x98, rf(-3.2f, 3.2f)); wf(P + 0x180, rf(-3.2f, 3.2f)); wf(P + 0x194, rf(0, 9)); wfrand(P + 0xe0, 4, -0.5f, 0.5f); w32(P + 0x209c, u(ri(0, 3) ? ri(0, 0x30) : 0x23)); w32(P + 0x20a0, u(ri(0, 5)));
        for (int i = 0; i < 8; ++i) w32(OV(0x0015f6d8) + 4 * u(i), u(ri(0, 7)));
        wf(P + 0xaa8, rf(0, 20)); wf(P + 0x168, rf(0, 8)); w32(P + 0x964, M3); wfrand(M3 + 0x10, 4, -9, 9); w32(P + 0x2298, ri(0, 1) ? ee_read<uint32_t>(P + 0x10b8) : u(ri(0, 0x24))); if (ri(0, 2) == 0) w8(M + 0x52, 4);
        for (uint32_t o : {0x60u, 0x62u}) w16(0x00141680 + o, ri(0, 7) ? u(ri(0, 300)) : 0xffffu); wrand(0x00141680 + 0x64, 1); w16(P + 0x22c8, ri(0, 3) ? 0u : 1u);
        for (int i = 0; i < 0x25; ++i) w32(OV(0x00179ac0) + 0x4c * u(i) + 0x24, u(ri(0, 0x7f)));
        w32(P + 0x990, M3); w32(M3 + 0x78, T3 + 0x800); for (uint32_t o = 0; o < 0x30; o += 4) wf(T3 + 0x800 + o, ri(0, 2) ? rf(0.1f, 5) : 0.0f);
        wf(0x0015ed64, rf(0.5f, 2)); wf(P + 0x4a0, rf(0, 0.1f)); wf(P + 0x940, rf(0, 0.1f)); wrand(P + 0x960, 0x30); wrand(P + 0x4b0, 6); w32(P + 0x964, M3); w32(P + 0x990, M3);
        {  // rail paths at T3 + 0xc00 for +0x560 / +0x5f0
            const uint32_t PT = T3 + 0xc00; const int n = ri(2, 6); w32(PT, u(n));
            for (int i = 0; i < n; ++i) { wfrand(PT + 0x10 + 16 * u(i), 3, -9, 9); wf(PT + 0x1c + 16 * u(i), rf(0.5f, 3)); }
            for (uint32_t R : {P + 0x560, P + 0x5f0}) { w32(R, PT); w32(R + 4, u(ri(0, n - 1))); wf(R + 8, rf(0, 0.5f)); }
            w32(P + 0x584, u(ri(0, 1))); w32(P + 0x60c, u(ri(0, 1))); wrand(P + 0x570, 0x14); w32(P + 0x5d8, ri(0, 1) ? M3 : 0u); wrand(P + 0x600, 8);
            wf(P + 0x164, rf(0, 8)); wf(P + 0x410, rf(0, 50)); wrand(P + 0x4ec, 2);
            wf(P + 0x2dc, rf(0, 3)); wf(P + 0x160, rf(0, 9)); wrand(P + 0x940, 2); wrand(P + 0x950, 2); wrand(P + 0x22b0, 2); w8(P + 0x22ca, 0);
        } wrand(OV(0x0015f688), 1); wrand(P + 0x1d0, 1); wrand(P + 0x3b8, 2); w32(OV(0x001612d0), 0); wf(0x0015ed70, rf(0.005f, 0.05f)); wfrand(P + 0x100, 4, -0.5f, 0.5f);
        w32(P + 0xaa0, u(ri(0, 3))); w32(P + 0xaa4, u(ri(0, 20))); w8(M + 0xa4, ri(0, 1) ? 0xffu : u(ri(0, 63)));
        for (int c = 0; c < 64; ++c) { const uint32_t o = OV(0x00178100) + 0x40 * u(c); w32(o + 0x34, ri(0, 3) ? M : R()); w32(o + 0x24, R()); wf(o + 0x2c, rf(0, 4)); }        // the jump family (block 0x224590)
        w16(P + 0x1f2, ri(0, 7) ? 0u : 1u); w8(P + 0x12e4, u(ri(0, 1))); w8(P + 0x12e8, u(ri(0, 1))); wf(P + 0x22a4, rf(0, 2.5f)); w32(P + 0x1660, u(ri(0, 2)));
        wf(P + 0x2f4, rf(0, 50)); wfrand(P + 0x500, 2, -9, 9); wfrand(P + 0x540, 2, -9, 9); wfrand(P + 0x460, 4, -9, 9); wf(0x0015ed60, rf(0.5f, 2));
        w32(0x0015ed80, u(ri(0, 1))); w8(0x0015edb5, u(ri(0, 1))); w32(P + 0x450, u(ri(0, 3)));
        if (ri(0, 2) == 0) { wf(P + 0x80, 331.0f + rf(-8, 8)); wf(P + 0x84, 104.0f + rf(-8, 8)); wf(P + 0x88, 145.0f + rf(-8, 8)); }
        for (int i = 0; i < 0x20; ++i) wf(P + 0x2128 + 4 * u(i), rf(-5, 5)); w32(P + 0x21a8, u(ri(0, 31))); w32(P + 0x21ac, u(ri(0, 32)));
        for (int i = 0; i < 0x20; ++i) w8(OV(0x0017bdd8) + u(i), u(ri(0, 0x7f)));
        if (ri(0, 1)) wf(P + 0x2f0, ee_read<float>(P + 0x88) + rf(-0.7f, -0.3f));
        if (ri(0, 1)) wfrand(P + 0x1d20, 2, -1, 1); else { wf(P + 0x1d20, 0.0f); wf(P + 0x1d24, 0.0f); }
        // the attack family (block 0x225bdc)
        w32(P + 0xa60, u(ri(0, 8))); if (ri(0, 1)) w32(P + 0x208c, 6); if (ri(0, 2) == 0) w32(P + 0x2094, 6); if (ri(0, 2) == 0) w32(P + 0x2084, ri(0, 1) ? 0xbu : 1u);
        w32(P + 0x2098, u(ri(0, 80))); { static const uint8_t k53[] = {0x1a, 0xf, 0x10, 3}; w8(M + 0x53, k53[ri(0, 3)]); }
        if (ri(0, 1)) w32(P + 0x10b8, 10); w32(P + 0xa50, ri(0, 2) ? M3 : 0u); w32(P + 0x1090, ri(0, 2) ? M3 : 0u); wrand(P + 0xa7c, 1); if (ri(0, 1)) w16(M3 + 0xa6, 0x47);
        if (ri(0, 3) == 0) w32(P + 0x208c, 4); if (ri(0, 4) == 0) w32(P + 0x2084, 4); if (ri(0, 1)) w32(P + 0x198, u(ri(0, 12)));
        if (ri(0, 2)) {  // the hero class's sound table (state 0x69 plays sound 10)
            w32(0x01e90000 + 0x28, T2); w8(0x01e90000 + 0xd, 12);
            for (int i = 8; i < 12; ++i) {
                const uint32_t d = T2 + 32 * u(i); w8(d + 0x18, u(ri(0, 1))); w16(d + 0x1a, R()); w32(d + 0x10, u(ri(0, 20))); w32(d + 0x14, u(ri(21, 60)));
                wf(d, rf(0, 4)); wf(d + 4, rf(5, 15)); w32(d + 8, u(ri(-50, 50))); w32(d + 0xc, u(ri(0, 200))); w8(d + 0x19, R());
            }
        }
    };
    for (uint32_t st : {0x00u, 0x67u, 0x41u, 0x3du, 0x80u, 0x7fu, 0x33u, 0x35u, 0x34u, 0x37u, 0x75u, 0x76u, 0x36u, 0x6au, 0x82u, 0x38u, 0x39u, 0x3au, 0x01u, 0x17u, 0x30u, 0x2eu, 0x02u, 0x2fu, 0x3fu, 0x73u, 0x71u, 0x65u, 0x31u, 0x03u, 0x66u, 0x04u, 0x24u, 0x25u, 0x26u, 0x2cu, 0x2du, 0x22u, 0x28u, 0x74u, 0x18u, 0x19u, 0x1au, 0x1bu, 0x79u, 0x06u, 0x77u, 0x81u, 0x08u, 0x07u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu, 0x10u, 0x11u, 0x12u, 0x1cu, 0x29u, 0x2au, 0x3cu, 0x49u, 0x4cu, 0x5eu, 0x69u, 0x13u, 0x14u, 0x15u, 0x20u, 0x23u, 0x51u, 0x70u, 0x2bu, 0x21u, 0x7au, 0x16u, 0x68u, 0x7bu, 0x7cu, 0x7du, 0x43u, 0x4fu, 0x45u, 0x52u, 0x46u, 0x44u, 0x47u, 0x4au, 0x4bu, 0x4du, 0x4eu, 0x5au, 0x53u, 0x60u, 0x61u, 0x5fu, 0x5cu, 0x5bu, 0x62u, 0x5du, 0x59u, 0x58u, 0x55u, 0x54u, 0x57u, 0x56u, 0x42u, 0x1du, 0x1eu, 0x1fu, 0x72u, 0x27u, 0x32u, 0x78u, 0x3bu, 0x63u, 0x64u, 0x05u, 0x3eu, 0x40u, 0x48u, 0x50u, 0x6bu, 0x6fu, 0x7eu}) {
        g_batch = st < 0x40 ? 45 : 46;  // (batch 46: the states from 0x40 on)
        static char nm[32];
        std::snprintf(nm, sizeof nm, "hero_set_state/%02x", st);
        check(nm, 0x002223f8, N * 4, Ret::Int32, [stateenv, st] {
            stateenv(st);
            if (st == 0x10 && ri(0, 1)) { w32(g::PLAYER + 0x2084, 4); w32(g::PLAYER + 0x198, u(ri(5, 9))); }
            if (st == 0x16 && ri(0, 1)) wfrand(g::PLAYER + 0x110, 4, -12, 12);
            if (st == 0x68) {
                if (ri(0, 1)) w32(g::PLAYER + 0x2090, 0x69);
                { static const uint32_t pv[] = {7, 0x69, 3}; w32(g::PLAYER + 0x2084, pv[ri(0, 2)]); } if (ri(0, 2) == 0) w32(g::PLAYER + 0x208c, 0);
            }
            if (st == 0x7c && ri(0, 1)) { w32(0x0015ed84, 6); wrand(M + 0x38, 2); }
            if (st == 0x44 && ri(0, 1)) w32(g::PLAYER + 0x2084, 3);
            if (st == 0x60) { if (ri(0, 1)) w32(g::PLAYER + 0x2084, 0x60); if (ri(0, 1)) w32(g::PLAYER + 0x2090, 0x60); w32(g::PLAYER + 0x198, u(ri(0, 20))); }
            if (st == 0x4a && ri(0, 1)) w32(g::PLAYER + 0x208c, 4);
            if (st == 0x54 && ri(0, 1)) wfrand(g::PLAYER + 0x110, 4, -6, 6);
            if (st == 0x42 && ri(0, 1)) w32(g::PLAYER + 0x570, 0);
            if (st == 0x1e && ri(0, 1)) w8(M + 0x53, ri(0, 1) ? 0u : 0x54u);
            if (st == 0x3c) {  // (the redirect to 0x7c is not ported yet)
                if (ee_read<uint32_t>(0x0015ed84) == 10) w32(0x0015ed84, 6);
                w32(g::PLAYER + 0x22a8, u(ri(2, 5))); w32(g::PLAYER + 0x1660, ri(0, 1) ? 0u : 0xffffffffu);
            }
            return Args{{st, ri(0, 3) ? 1u : u(ri(0, 2))}, {}};
        }, [](A a) { return ri32(hero_set_state(a.i[0], static_cast<int32_t>(a.i[1]))); });
    }

    // ======== batch 47: the per-frame update of the hero state (0x217970)
    g_batch = 47;
    g_anim_nmin = 24;
    auto updenv = [stateenv, movenv, boxenv, heroAt, treeenv](uint32_t st) {  // the hero in state st, inputs and surroundings
        stateenv(st);
        movenv();
        w32(g::PLAYER + 0x2080, M); w32(g::HERO_MOBY, M); wfrand(M + 0x40, 3, -0.3f, 0.3f); wf(M + 0x4c, 0);
        w32(g::PLAYER + 0x2084, st);
        if (ri(0, 1)) w16(g::PLAYER + 0x30e, 0);
        w8(g::PLAYER + 0x257, ri(0, 4) ? 1u : 0u); if (ri(0, 1)) wf(g::PLAYER + 0x2dc, rf(0, 0.2f));
        if (ri(0, 1)) wfrand(g::PLAYER + 0x150, 4, -0.5f, 0.5f); wf(g::PLAYER + 0x108, rf(-0.2f, 1)); wfrand(g::PLAYER + 0x200, 2, -0.5f, 0.5f); wf(g::PLAYER + 0x208, rf(-0.2f, 1)); if (ri(0, 3) == 0) w8(g::PLAYER + 0x20b3, u(ri(1, 3)));
        wfrand(g::PLAYER + 0x120, 3, -0.5f, 0.5f);
        wf(g::PLAYER + 0x184, ri(0, 2) ? (ri(0, 1) ? rf(-2, 2) : rf(-12, 12)) : 0.0f); w32(OV(0x00166c00u - 0x7578), u(ri(-1, 2))); wrand(OV(0x00166c00u - 0x7574), 1); w8(g::PLAYER + 0x20a5, u(ri(0, 1)));
        if (ri(0, 1)) { const uint32_t k = u(ri(0, 0x20)); w8(M + 0x52, k); w8(M + 0x53, k); } w8(M + 0x51, u(ri(0, 10))); wf(g::PLAYER + 0x16c, rf(-1, 1)); wf(g::PLAYER + 0x164, rf(0, 5));
        w8(g::PLAYER + 0x12e7, ri(0, 2) ? 0u : 1u); w8(0x0013d4dc, u(ri(0, 1))); w32(g::PLAYER + 0x2fc, u(ri(0, 1))); wfrand(g::PLAYER + 0x2a0, 4, 10, 90); w32(g::PLAYER + 0x22f4, ri(0, 3) ? 0u : 1u);
        wfrand(g::PLAYER + 0x690, 4, -1, 1); wf(g::PLAYER + 0x69c, ee_read<float>(g::PLAYER + 0x2f0) + rf(-1, 1)); wf(g::PLAYER + 0x6a0, rf(-1, 1)); w32(g::PLAYER + 0x6a4, ri(0, 1) ? 0u : M2);  // (EE floats, no NaN)
        w8(M2 + 0x23, R()); w8(OV(0x00197bc0) + 0x52a, 0x40); w32(OV(0x00197300) + 0x100, 0); w32(OV(0x00197680) + 0x100, R());  // (a class slot without data for the spawns)
        for (int k = 0; k < 0x80; ++k) w32(0x01e80000 + 16 * u(k) + 12, 0x70000000u + 64 * u(ri(0, 7)));  // joints for the chain points (nonzero: DMA sentinels)
        for (uint32_t o = 0; o < 0x200; o += 4) wf(0x70000000u + o, rf(-2, 2));
        for (uint32_t k = 0; k < 0x80; ++k) { w32(OV(0x001b2000) + 4 * k, 0x01efc000 + 4 * k); w8(0x01efc000 + 4 * k, R()); }  // particle textures
        wf(g::PLAYER + 0x229c, rf(0, 1)); wf(g::PLAYER + 0x188, ri(0, 1) ? rf(-0.2f, 0.2f) : rf(-1, 3)); wf(g::PLAYER + 0x164, rf(0, 3));  // running inputs
        w16(g::PLAYER + 0x3be, u(ri(0, 1))); w16(g::PLAYER + 0x3b8, u(ri(0, 1))); wf(g::PLAYER + 0x930, rf(-0.5f, 0.5f)); wfrand(g::PLAYER + 0x920, 3, -0.5f, 0.5f);
        w16(g::PLAYER + 0x1f4, u(ri(0, 1))); w8(g::PLAYER + 0x20a8, u(ri(0, 1))); w32(g::PLAYER + 0x2088, u(ri(0, 2))); wf(OV(0x0017bdc4), rf(0.3f, 1));
        {  // jump inputs
            constexpr uint32_t P = g::PLAYER;
            w32(P + 0x418, u(ri(0, 3))); w16(P + 0x4a8, u(ri(0, 1))); w32(P + 0x420, u(ri(0, 40))); w32(P + 0x198, u(ri(0, 60)));
            wf(P + 0x410, rf(0, 50)); wf(P + 0x2d8, ee_read<float>(P + 0x410) + rf(-8, 8)); if (ri(0, 1)) wf(P + 0x88, ee_read<float>(P + 0x410) + rf(-2, 2));
            w16(P + 0x41c, u(ri(0, 1))); w32(P + 0x3d4, u(ri(0, 60))); wf(P + 0x3f8, ri(0, 1) ? 0.0f : rf(0, 0.1f));
            if (ri(0, 2) == 0) { wf(P + 0xe0, 0); wf(P + 0xe4, 0); wf(P + 0xe8, rf(-0.0005f, 0.0005f)); }
            wf(P + 0x480, rf(0, 1)); wf(P + 0x484, rf(0, 1)); wf(P + 0x424, rf(0, 3)); wf(P + 0x168, rf(0, 12));
            if (ri(0, 2) == 0) { wfrand(P + 0xe0, 2, -8, 8); wf(P + 0xe8, rf(-1, 4)); }
            wf(P + 0x3f4, rf(0, 2)); wf(P + 0x194, rf(0, 3));
            if (ri(0, 3) == 0) wf(P + 0x2dc, rf(0, 6));
            wfrand(P + 0x940, 2, 0, 1); wfrand(P + 0x948, 4, -0.5f, 0.5f); wf(P + 0x4ec, rf(0, 1)); wf(P + 0x4f0, rf(0, 50)); wf(P + 0x4e4, rf(-3.2f, 3.2f)); wfrand(P + 0x4d0, 3, 10, 90);  // (EE floats)
            for (uint32_t mat : {OV(0x00166fd0u), M + 0xc0}) {  // camera and hero moby rotations
                const float q = rf(-3.2f, 3.2f), c = std::cos(q), sn = std::sin(q);
                const float rows[12] = {c, sn, 0, 0, -sn, c, 0, 0, 0, 0, 1, 0};
                for (uint32_t i = 0; i < 12; ++i) wf(mat + 4 * i, rows[i]);
            }
            if (st == 0x2f) { static const uint8_t an[] = {0x37, 0x6d, 0x37, 0x6d, 0}; w8(M + 0x53, an[ri(0, 4)]); }
            if ((st == 0x1a || st == 0x1b || st == 0x4d || st == 0x4e) && ri(0, 2)) {  // hanging on a box edge (as for hero_ledge_grab_check)
                const float cx = rf(50, 90), cy = rf(50, 90), hx = rf(0.5f, 4), hy = rf(0.5f, 4), hh = rf(0.5f, 4), z0 = rf(20, 40);
                const uint32_t box = boxenv(cx, cy, z0, hx, hy, hh, u(ri(1, 8)));
                const uint32_t pv = 0x01ee4000 + 0x40 * 47, pw = 0x01ee8000 + 0x40 * 47;  // grabbable
                w16(box + 0x34, 0x4020u); w32(box + 0x78, pv); w32(pv, pw); w32(pv + 8, 0); w16(pw + 0x1e, 1);
                const int side = ri(0, 3);
                const float along = rf(-0.9f, 0.9f), gap = ee_read<float>(P + 0x234) + rf(0, 0.25f);
                const float face = side == 0 ? 3.14159f : side == 1 ? 0.0f : side == 2 ? -1.5708f : 1.5708f;
                float x = side == 0 ? cx + hx + gap : side == 1 ? cx - hx - gap : cx + along * hx;
                float y = side == 2 ? cy + hy + gap : side == 3 ? cy - hy - gap : cy + along * hy;
                if (side < 2) y = cy + along * hy;
                const bool wet = st == 0x4d || st == 0x4e;
                const float yaw = face + rf(-0.4f, 0.4f), z = z0 + hh - (wet ? rf(0.65f, 0.9f) : rf(1.15f, 1.65f));
                heroAt(x, y, z, yaw); wf(P + 0x98, yaw); treeenv(x, y, z); w8(P + 0x20a4, wet ? 1u : 0u); w16(P + 0x308, 0); w16(P + 0x1b0, 0);
                wf(P + 0x2d8, z - rf(0.5f, 2)); wf(P + 0xaa8, rf(0, 20)); wfrand(P + 0x110, 3, -0.05f, 0.05f); wf(P + 0x940, rf(0, 0.02f)); w32(P + 0x208c, 0);
            }
            if (st == 0x65 || st == 0x66) {  // the scripted spot, near or far; any movement mode
                const float r = ri(0, 1) ? rf(0, 0.25f) : rf(0, 6), q = rf(-3.2f, 3.2f);
                wf(P + 0x1640, ee_read<float>(P + 0x80) + r * std::cos(q)); wf(P + 0x1644, ee_read<float>(P + 0x84) + r * std::sin(q)); wf(P + 0x1648, ee_read<float>(P + 0x88)); wf(P + 0x164c, rf(-3.2f, 3.2f));
                w8(P + 0x20a4, u(ri(0, 3)));
            }
            if (st == 0x60 || st == 0x61) {  // Giant Clank: a punch step; the placed shock moby
                w32(P + 0xa60, u(ri(0, 2)));
                if (ri(0, 1)) { w32(OV(0x001613c8), M2); w16(M2 + 0xa6, ri(0, 3) ? 0x5f3u : 0x10u); w32(M2 + 0x78, 0x01ee4000 + 0x40 * 46); }
            }
            if (st >= 0x38 && st <= 0x3a && ri(0, 2)) { w32(P + 0x2fc, M2); w16(M2 + 0xa6, ri(0, 3) ? 0x155u : 0x10u); wfrand(M2 + 0x10, 3, 10, 90); wf(M2 + 0x48, rf(-3.2f, 3.2f)); }
            if (st == 0x36 || st == 0x37) { w32(0x0013c940 + 0x1d4, u(ri(0, 1))); w32(P + 0x698, u(ri(0, 3))); w16(P + 0x90a, u(ri(0, 1) ? 0 : ri(1, 80))); }
            if (st == 0x6a || st == 0x82) { w32(P + 0x8f0, u(ri(0, 3))); wf(P + 0x8fc, rf(-2, 1)); w32(P + 0x198, u(ri(0, 100))); }
            if (st == 0x24 || st == 0x25 || st == 0x26 || st == 0x2c || st == 0x2e) {  // swingshot: the target, the reel
                wf(P + 0x960, rf(-1, 30)); wf(P + 0x96c, rf(0, 60)); wf(P + 0x970, rf(0, 30)); w32(P + 0x974, u(ri(0, 1))); w32(P + 0x2088, u(ri(0, 2)));
                { const float r = rf(1, 25), q = rf(-3.2f, 3.2f); wf(M3 + 0x10, ee_read<float>(P + 0x80) + r * std::cos(q)); wf(M3 + 0x14, ee_read<float>(P + 0x84) + r * std::sin(q)); wf(M3 + 0x18, ee_read<float>(P + 0x88) + rf(-3, 8)); }
                w32(P + 0x964, ri(0, 5) ? M3 : 0u); w32(P + 0x990, M3); w32(P + 0x994, M3);
                wf(P + 0x97c, rf(1, 8)); wf(P + 0x980, rf(1, 8)); wf(P + 0x9a0, rf(1, 8)); wf(P + 0x9a4, rf(1, 8)); wf(P + 0x9a8, rf(-1, 1)); wf(P + 0x9ac, rf(-3, 3));
                wf(P + 0x9b0, rf(0.5f, 3)); wf(P + 0x9b4, rf(0, 2)); wf(P + 0x9b8, rf(-1, 1)); wf(P + 0x9c4, rf(0, 0.1f)); wf(P + 0x9c8, rf(0, 0.5f)); wf(P + 0x9cc, rf(0, 2));
                w16(P + 0x99c, u(ri(0, 1))); w16(P + 0x9bc, u(ri(0, 2))); w16(P + 0x9be, u(ri(0, 1))); wfrand(P + 0x90, 2, -1, 1); wfrand(P + 0xa0, 2, -1, 1); w32(P + 0xa98, R());
                { const uint32_t pv = 0x01ee4000 + 0x40 * 45; w32(M3 + 0x78, ri(0, 3) ? pv : 0u); wf(pv + 0xc, rf(0, 30)); w32(pv + 0x34, u(ri(0, 1))); } wf(M3 + 0x48, rf(-3, 3));
                w8(M + 0x53, ri(0, 1) ? 0x34u : 0x35u); if (ri(0, 1)) w8(M + 0x52, 0x35);
            }
            wfrand(P + 0x9d0, 8, -1, 1); wf(P + 0x8fc, rf(-2, 1)); wfrand(P + 0x8e8, 2, -1, 1); wf(P + 0x4b0, rf(0, 1)); wf(P + 0x4b4, rf(0, 1)); wf(P + 0x4b8, ri(0, 1) ? 0.0f : 1.0f); w32(P + 0x4c4, u(ri(0, 2)));  // (EE floats)
            if (st == 0x30) { w8(M + 0x70, R()); w8(M + 0x52, ri(0, 1) ? 0x36u : 0u); }
            if (st == 0x33 || st == 0x34 || st == 0x35) {  // swimming: the pressure buttons, the dive-in, the bubble and nozzle timers
                constexpr uint32_t PAD = 0x0013c940; const float k6c = ee_read<float>(0x0015ed6c);
                { static const uint32_t bt[] = {0, 0x40, 0x80, 0xc0, 0x2, 0x8, 0x4a}; w32(PAD + 0x1a0, bt[ri(0, 6)] | (ri(0, 3) ? 0u : R() & ~0xcau)); }
                w32(PAD + 0x1a4, ri(0, 1) ? 0x40u : 0u); wf(PAD + 0x128, ri(0, 3) ? rf(0, 1) : 0.0f); wf(PAD + 0x12c, ri(0, 3) ? rf(0, 1) : 0.0f); w32(PAD + 0x1dc, ri(0, 3) ? 0u : 0x79u);
                w16(P + 0x900, u(ri(0, 30))); w16(P + 0x902, u(ri(0, 30))); w16(P + 0x90a, ri(0, 1) ? 0u : u(ri(1, 80))); w16(P + 0x90c, ri(0, 3) ? u(ri(0, 200)) : 0xffffu); w16(P + 0x90e, u(ri(0, 120)));
                w32(P + 0x8e0, u(ri(0, 50))); wf(P + 0x8e4, rf(-0.1f, 0.1f)); wf(P + 0x8e8, rf(-0.1f, 0.1f)); wf(P + 0x8ec, rf(-0.3f, 0.3f)); w32(P + 0x8f0, u(ri(-1, 3))); w32(P + 0x8f4, u(ri(-1, 3))); w16(P + 0x8f8, u(ri(-1, 3)));
                w32(P + 0x910, u(ri(0, 5))); w32(OV(0x0015f5cc), u(ri(100, 1000))); w32(P + 0x914, ee_read<uint32_t>(OV(0x0015f5cc)) - u(ri(0, 150))); w32(P + 0x22a0, u(ri(0, 3000)));
                wf(P + 0x90, rf(-1, 1)); wf(P + 0x94, rf(-1.5f, 1.5f)); wf(P + 0x194, rf(0, 8) * k6c); wfrand(P + 0x150, 3, -3, 3); w32(P + 0x2088, u(ri(0, 1))); w32(P + 0x198, u(ri(0, 3) ? ri(0, 30) : 1));
                w32(P + 0x2090, ri(0, 1) ? 0x35u : u(ri(0, 0x40))); w32(P + 0xa9c, u(ri(0, 1))); wf(P + 0xaa8, rf(0, 29)); wf(P + 0xaac, rf(0.5f, 2)); w32(P + 0xa98, u(ri(0, 3)));
                w32(OV(0x001612d0), 0); w32(0x0015eea4, u(ri(0, 100000))); w16(0x00141848 + 0x58, ri(0, 7) ? u(ri(0, 100)) : 0xffffu); w16(0x00141848 + 0x5a, u(ri(0, 100)));
                for (uint32_t o = 0; o < 0x100; o += 4) w32(M2 + o, ee_read<uint32_t>(M + o));  // the hydro-pack: a copy of the hero moby
                wfrand(M2 + 0x10, 3, 10, 90); w32(P + 0x1180, ri(0, 3) ? M2 : 0u); w16(M2 + 0xa6, ri(0, 3) ? 0x261u : 0x10u);
                { const int sl = ri(-1, 29); w32(P + 0x222c, u(sl)); if (sl >= 0) { const uint32_t e = 0x0013e550 + 0x70 * u(sl); w32(e + 0x88, ri(0, 1) ? M : M2); w8(e + 0x74, u(ri(0, 1))); } }
            }
            if (st == 0x28 || st == 0x29 || st == 0x2a || st == 0x2b || st == 0x42) {  // on a rail (+0x560), sometimes switching to +0x594
                const uint32_t PT = T3 + 0xc00, P2 = T3 + 0xd00; const int n = ri(2, 6), n2 = ri(2, 6); w32(PT, u(n)); w32(P2, u(n2));
                const float bx = ee_read<float>(P + 0x80), by = ee_read<float>(P + 0x84), bz = ee_read<float>(P + 0x88), k6c = ee_read<float>(0x0015ed6c);
                const float dz = ri(0, 1) ? 2.0f : (ri(0, 1) ? 0.4f : 0.15f), dxy = ri(0, 1) ? 6.0f : 2.0f;
                for (int i = 0; i < n; ++i) { wf(PT + 0x10 + 16 * u(i), bx + rf(-dxy, dxy)); wf(PT + 0x14 + 16 * u(i), by + rf(-dxy, dxy)); wf(PT + 0x18 + 16 * u(i), bz + rf(-dz, dz)); wf(PT + 0x1c + 16 * u(i), rf(0.5f, 3)); }
                for (int i = 0; i < n2; ++i) { wf(P2 + 0x10 + 16 * u(i), bx + rf(-6, 6)); wf(P2 + 0x14 + 16 * u(i), by + rf(-6, 6)); wf(P2 + 0x18 + 16 * u(i), bz + rf(-2, 2)); wf(P2 + 0x1c + 16 * u(i), rf(0.5f, 3)); }
                if (ri(0, 3) == 0) for (uint32_t o = 0; o < 12; o += 4) wf(PT + 0x20 + o, ee_read<float>(PT + 0x10 + o));  // a repeated point (zero heading)
                w32(P + 0x560, PT); w32(P + 0x564, u(ri(0, 2) ? ri(0, n - 1) : n - 1)); wf(P + 0x568, rf(0, 0.9f)); w32(P + 0x584, u(ri(0, 1)));
                w32(P + 0x594, P2); w32(P + 0x598, u(ri(0, n2 - 1))); wf(P + 0x59c, rf(0, 0.9f)); w32(P + 0x5a0, u(ri(0, 1)));
                w32(P + 0x590, ri(0, 2) ? 0u : 1u); w32(P + 0x5a4, u(ri(0, 1) ? ri(0, 3) : ri(40, 60)));
                w32(P + 0x570, ri(0, 3) ? 0u : u(ri(1, 5))); wf(P + 0x574, rf(0, 14) * k6c); w32(P + 0x578, ri(0, 1) ? 1u : 0xffffffffu);
                wf(P + 0x57c, rf(-3.2f, 3.2f)); wf(P + 0x580, rf(-1, 1)); wf(P + 0x588, rf(-0.3f, 0.3f)); w32(P + 0x58c, u(ri(0, 1)));
                w32(P + 0x5a8, u(ri(-1, 3))); wf(P + 0x5ac, (ri(0, 1) ? rf(-4, 0) : rf(0, 14)) * k6c); wf(P + 0x5b0, ri(0, 2) ? rf(0, 14) * k6c : 0.0f); w32(P + 0x5c4, u(ri(0, 1)));
                wf(P + 0x5cc, rf(0, 0.4f)); wf(P + 0x5d0, rf(-0.8f, 0.8f)); wf(P + 0x5d4, rf(-0.1f, 0.1f)); wf(P + 0x5dc, rf(0, 20) * k6c);
                if (ee_read<uint32_t>(P + 0x564) == u(n - 1) && ri(0, 1)) { wf(P + 0x568, 0); w32(P + 0x584, 0); }
                if (ri(0, 2) == 0) { const uint32_t q = PT + 0x10 + 16 * u(ri(0, n - 1)); wf(P + 0x80, ee_read<float>(q) + rf(-0.3f, 0.3f)); wf(P + 0x84, ee_read<float>(q + 4) + rf(-0.3f, 0.3f)); wf(P + 0x88, ee_read<float>(q + 8) + rf(-0.3f, 0.3f)); }
                wf(P + 0x550, bx + rf(-5, 5)); wf(P + 0x554, by + rf(-5, 5)); wf(P + 0x508, bz + rf(-1, 1)); wf(P + 0x2dc, ri(0, 1) ? rf(0, 0.8f) : rf(0.9f, 2));
                w32(P + 0x19c, u(ri(0, 120))); w32(P + 0x198, u(ri(0, 40))); w32(P + 0x418, u(ri(0, 2) ? 0 : 1)); wf(P + 0x160, rf(0, 0.4f));
                w32(P + 0x10b8, ri(0, 3) ? u(ri(0, 0x24)) : 0xcu); w32(OV(0x00179ac0) + 0x4c * ee_read<uint32_t>(P + 0x10b8) + 0x18, u(ri(0, 1))); w8(P + 0x20aa, u(ri(0, 1)));
                w32(P + 0x5d8, ri(0, 2) ? 0u : M3); { static const uint8_t an[] = {0x31, 0x4a, 0x4b, 0}; w8(M + 0x53, an[ri(0, 3)]); w8(M + 0x52, ri(0, 1) ? 0x31u : 0u); }
                { const int sl = ri(-1, 29); w32(P + 0x2218, u(sl)); if (sl >= 0) { const uint32_t e = 0x0013e550 + 0x70 * u(sl); w32(e + 0x88, ri(0, 1) ? M : M2); w8(e + 0x74, u(ri(0, 1))); } }
            }
            if (st == 0x74) { if (ri(0, 1)) w8(M + 0x53, 0x66); if (ri(0, 2)) w32(P + 0x600, 0); w32(OV(0x0015f5cc), R()); wf(P + 0x604, rf(0, 1)); wf(P + 0x618, rf(-1, 1)); wfrand(P + 0x610, 2, -3, 3); }
            if (st == 0x13 || st == 0x14 || st == 0x15 || st == 0x20 || st == 0x23 || st == 0x51 || st == 0x60 || st == 0x61 || st == 0x5f || st == 0x5b) {  // the attacks: a target nearby, any point of the swing, diving
                w32(P + 0xa54, ri(0, 2) ? M2 : 0u); w32(P + 0xa58, ri(0, 3) ? 0u : 1u);
                wf(M2 + 0x10, ee_read<float>(P + 0x80) + rf(-5, 5)); wf(M2 + 0x14, ee_read<float>(P + 0x84) + rf(-5, 5));
                wf(P + 0xaa8, ri(0, 1) ? rf(0, 30) : rf(30, 90)); w32(P + 0xa9c, ri(0, 2) ? 0u : 1u); if (ri(0, 1)) wf(P + 0xe8, rf(-60, 0));
                if (ri(0, 2) == 0) w32(P + 0x2090, 1); w32(P + 0x2098, u(ri(0, 40)));
                wf(P + 0xaac, rf(0.5f, 3)); if (ri(0, 1)) { static const float ev[] = {10, 27, 33, 84, 0, 30, 4}; wf(P + 0xaa8, ev[ri(0, 6)] + rf(0, 2)); }  // frame events
            }
            w32(0x0013c940 + 0x1a0, R()); w32(0x0013c940 + 0x1a4, R()); w8(P + 0x12e2, ri(0, 3) ? 0u : 1u);
            w32(P + 0x10b4 + 0xf0, ri(0, 1) ? 2u : 0u); w32(P + 0x10b8 + 0xf0, u(ri(0, 3)));
            for (uint32_t o : {0x2224u, 0x2228u, 0x222cu}) {  // held sound slots, some live on the hero moby
                const int sl = ri(-1, 29); w32(P + o, u(sl));
                if (sl >= 0) { const uint32_t e = 0x0013e550 + 0x70 * u(sl); w32(e + 0x88, ri(0, 1) ? M : M2); w8(e + 0x74, u(ri(0, 1))); }
            }
            if (st == 0x12 && ri(0, 1)) w32(P + 0x198, u(frame_scale_i(0x14)));
            if (ri(0, 1)) { w16(P + 0x22d8, 0); w32(P + 0x11a8, 3); }
            { const uint32_t O = P + 0x1670; w32(O + 0x13c, u(ri(0, 1))); w32(O + 0x134, M); w32(O + 0x138, 0); wfrand(O, 0x40, -3, 3); w16(O + 0x130, u(ri(0, 7))); w16(O + 0x132, u(ri(0, 8))); }
        }
        {  // the hero class's joint chains (class +0x1c) for the chain points
            constexpr uint32_t CT = 0x01e91000; w32(0x01e90000 + 0x1c, CT);
            for (int c = 0; c < 0x20; ++c) {
                const uint32_t ch = CT + 0x100 + 0x10 * u(c); const int cnt = ri(1, 4);
                w32(CT + 4 + 4 * u(c), ch); w16(ch, u(cnt)); for (int j = 0; j < cnt; ++j) w8(ch + 4 + u(j), u(ri(0, 23)));
            }
        }
        if (ri(0, 2) == 0) { w16(g::PLAYER + 0x30e, u(ri(1, 30))); wf(g::PLAYER + 0x2dc, rf(0, 0.14f)); wf(g::PLAYER + 0x108, rf(0.1f, 1)); w8(g::PLAYER + 0x257, 1); wf(g::PLAYER + 0x208, rf(0.05f, 0.5f)); { const float q = rf(-3.2f, 3.2f); wf(g::PLAYER + 0x200, std::cos(q)); wf(g::PLAYER + 0x204, std::sin(q)); } }  // steep ground on a platform (0x2121c0)
    };
    for (uint32_t st : {0x40u, 0x67u, 0x63u, 0x64u, 0x68u, 0x7bu, 0x7cu, 0x7du, 0x43u, 0x47u, 0x53u, 0x57u, 0x59u, 0x5au, 0x62u, 0x46u, 0x5du, 0x56u, 0x52u, 0x58u, 0x45u, 0x55u, 0x54u, 0x05u, 0x3eu, 0x7eu, 0x00u, 0x01u, 0x03u, 0x04u, 0x17u, 0x1du, 0x1eu, 0x1fu, 0x3du, 0x70u, 0x71u, 0x72u, 0x80u, 0x7fu, 0x02u, 0x73u, 0x07u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu, 0x10u, 0x11u, 0x12u, 0x1cu, 0x3cu, 0x49u, 0x4cu, 0x5eu, 0x69u, 0x06u, 0x2du, 0x77u, 0x81u, 0x08u, 0x14u, 0x13u, 0x15u, 0x21u, 0x7au, 0x20u, 0x23u, 0x3fu, 0x2fu, 0x31u, 0x74u, 0x18u, 0x19u, 0x4au, 0x4bu, 0x1au, 0x1bu, 0x79u, 0x16u, 0x65u, 0x66u, 0x51u, 0x4fu, 0x4du, 0x4eu, 0x44u, 0x50u, 0x60u, 0x61u, 0x5fu, 0x5cu, 0x5bu, 0x76u, 0x75u, 0x6au, 0x82u, 0x36u, 0x37u, 0x38u, 0x39u, 0x3au, 0x30u, 0x24u, 0x2eu, 0x25u, 0x26u, 0x2cu, 0x22u, 0x28u, 0x29u, 0x2au, 0x2bu, 0x42u, 0x33u, 0x34u, 0x35u}) {
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_state_update/%02x", st);
        // (stage 5, task C2b) the hero's state update reaches sphere_collide / capsule_collide from many states (measured on
        // levels 14 .. 17: 0x10, 0x1a, 0x1b, 0x20, 0x22, 0x2b, 0x4d, 0x4e, 0x5b, 0x5c, 0x60, a different subset on each level's
        // cases): A2's collision scratch ranges left out of the checks of every state (see batch 43's checks of them)
        const CollisionScratchIgnored spr_ignored;
        check(nm, 0x00217970, N * 2, Ret::None, [updenv, st] {
            updenv(st);
            return Args{};
        }, [](A) { hero_state_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }

    // ======== batch 48: the hero's input checks (0x229b70 and its helpers)
    g_batch = 48;
    auto padhist = [] {  // the pad ring: 30 button words at +0x1e0, newest index +0x18e, fill +0x190
        w32(g::PAD + 0x190, u(ri(0, 30))); w16(g::PAD + 0x18e, u(ri(0, 29)));
        for (int i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * u(i), ri(0, 2) ? 0u : (R() & 0x1f04au));
    };
    auto inpenv = [updenv, padhist](uint32_t st) {
        updenv(st);
        padhist();
        constexpr uint32_t P = g::PLAYER;
        w32(g::PAD + 0x1a0, ri(0, 2) ? 0u : (R() & 0xffu)); wf(P + 0x248, rf(0, 4)); wf(P + 0x250, rf(0, 2)); w8(P + 0x254, u(ri(0, 1)));
        w16(P + 0x22d8, ri(0, 3) ? 0u : 1u); w8(0x0013d4c2, u(ri(0, 1))); w8(0x0013d4c4, u(ri(0, 1))); w8(0x0013d4c6, ri(0, 3) ? 0u : 1u);
        w32(P + 0x1b8, ri(0, 2) ? 0u : 1u); w32(P + 0x1bc, u(ri(0, 12))); w8(P + 0x20a8, u(ri(0, 1))); w8(P + 0x20ab, ri(0, 3) ? 0u : 1u);
        w32(P + 0x1c4, ri(0, 3) ? 0u : 1u); wf(P + 0x168, rf(0, 4) * ee_read<float>(0x0015ed6c));
        { static const uint32_t md[] = {0x11, 0x12, 4, 3, 7, 0x14, 0, 1}; w32(P + 0x208c, md[ri(0, 7)]); }
        w32(P + 0x22a0, ri(0, 2) ? u(ri(0, 300)) : u(ri(0, 10000))); wf(P + 0x2f0, ee_read<float>(P + 0x88) + rf(-3, 3)); wf(P + 0x94, rf(-1.5f, 1.5f));
        wf(P + 0x108, rf(-1, 1) * ee_read<float>(0x0015ed6c) * 2.0f); w8(P + 0x12e4, u(ri(0, 1))); w16(P + 0x41e, u(ri(0, 1))); wf(P + 0x22a4, rf(0, 1.6f));
        wf(P + 0xe8, rf(-1, 1)); w32(OV(0x0015f5cc), ri(0, 1) ? u(ri(0, 8)) * 16u : R()); wf(OV(0x00173e68), ee_read<float>(P + 0x88) + rf(-2, 2));
        w8(P + 0x12e2, ri(0, 3) ? 0u : 1u); w8(P + 0x20a9, ri(0, 3) ? 0u : 1u); w32(P + 0x418, u(ri(0, 30))); w32(P + 0xa98, u(ri(0, 3)));
        w32(OV(0x0015f6dc), u(ri(0, 20))); w32(P + 0x198, u(ri(0, 20)));
        w32(P + 0x2284, R()); wfrand(P + 0x150, 3, -6, 6); w32(P + 0xab0, R()); w32(P + 0xab4, R());
        for (uint32_t k = 0; k < 4; ++k) w8(0x01e8d000 + 0x80 * k + 0x10, u(ri(0x10, 0x18)));  // the sets' animation counts
    };
    static const uint32_t inp_states[] = {0, 2, 3, 4, 7, 0xe, 0x10, 0x12, 0x13, 0x33, 0x6a, 0x76, 0x1, 0x8, 0x30};
    auto inp_state = [] { return inp_states[ri(0, sizeof inp_states / 4 - 1)]; };
    check("hero_try_item_stance", 0x002276c0, N * 4, Ret::Int32, [inpenv, inp_state] { inpenv(inp_state()); return Args{}; },
          [](A) { return ri32(hero_try_item_stance()); }, OV(0x00173e7c));
    // (task 0392) hero_land_fx 0x209ca8's particles copy its stack vectors whole (lq / sq in spawn_particle_23 0x26f548: pos.w into
    // the record's +0x1c): a stale stack lane, 0 in the port, whatever the original's stack held there (1.0 on levels 1 / 2 / 5,
    // whose jump reaches the land effects through one level function more: L5:0x24cee8): a lone w lane is skipped
    g_ignore_lone_w_lanes = true;
    check("hero_try_jump", 0x00228b78, N * 4, Ret::Int32, [inpenv, inp_state] {
        inpenv(inp_state());
        return Args{{u(ri(0, 12)), u(ri(0, 1))}, {}};
    }, [](A a) { return ri32(hero_try_jump(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1]))); }, OV(0x00173e7c));
    g_ignore_lone_w_lanes = false;
    check("hero_water_air", 0x00227778, N * 4, Ret::Int32, [inpenv, inp_state, boxenv] {
        inpenv(inp_state());
        if (ri(0, 2)) w32(g::PLAYER + 0x208c, ri(0, 1) ? 0x11u : 0x12u);
        if (ri(0, 1)) w32(g::PLAYER + 0x22a0, u(ri(-5, 15)));
        if (ri(0, 1)) wf(g::PLAYER + 0x2f0, ee_read<float>(g::PLAYER + 0x88) + rf(-0.6f, 2.5f));
        if (ri(0, 1)) {  // on the surface under a ceiling (a box's underside) the line up from it meets
            const float x = ee_read<float>(g::PLAYER + 0x80), y = ee_read<float>(g::PLAYER + 0x84), z = ee_read<float>(g::PLAYER + 0x88);
            boxenv(x, y, z + 0.5f, 1, 1, 1, 0);
            w32(g::PLAYER + 0x208c, 0x12); wf(g::PLAYER + 0x2f0, z + 0.5f - rf(0, 0.35f));
        }
        return Args{};
    },
          [](A) { return ri32(hero_water_air()); }, OV(0x00173e7c));
    check("hero_water_entry", 0x002279b0, N * 4, Ret::Int32, [inpenv, inp_state] {
        inpenv(inp_state());
        constexpr uint32_t P = g::PLAYER;
        if (ri(0, 3)) {  // in the water, near its surface
            w8(P + 0x12e4, 1); { static const uint32_t md[] = {0x11, 4, 0, 7, 0x11}; w32(P + 0x208c, md[ri(0, 4)]); }
            const float z = ee_read<float>(P + 0x88);
            wf(P + 0x2f0, ri(0, 1) ? z + 0.45f + rf(-0.4f, 0.4f) : z + rf(-1.5f, 1.5f)); w32(P + 0x2088, u(ri(0, 2))); wf(P + 0xe8, rf(-0.5f, 0.5f));
            if (ri(0, 3) == 0) w32(P + 0x2084, 0x12);
        }
        return Args{};
    },
          [](A) { return ri32(hero_water_entry()); }, OV(0x00173e7c));
    check("hero_land", 0x002293a8, N * 4, Ret::None, [inpenv, inp_state] { inpenv(inp_state()); return Args{}; },
          [](A) { hero_land(); return uint64_t{0}; }, OV(0x00173e7c));
    check("hero_hurt", 0x00210ce8, N * 10, Ret::Int32, [inpenv, inp_state] {
        inpenv(inp_state());
        constexpr uint32_t P = g::PLAYER;
        const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
        const uint32_t idx = u(ri(0, 63)), rec = OV(0x00178100) + 64 * idx;
        w8(hm + 0xa4, ri(0, 7) ? idx : 0xffu); w32(rec + 0x34, ri(0, 7) ? hm : M2);
        w32(rec + 0x24, ri(0, 1) ? 7u : u(ri(0, 7))); w32(rec + 0x30, u(ri(0, 1))); wfrand(rec + 0x10, 3, -2, 2); w32(rec + 0x1c, ri(0, 1) ? 0x45afdf66u : 0x3f800000u);
        wf(rec + 0x2c, rf(0, 40)); w8(rec + 0x28, ri(0, 1) ? 4u : u(ri(0, 8))); w32(rec + 0x20, ri(0, 2) ? M2 : 0u);
        { static const uint16_t cl[] = {0x4eb, 0x558, 0x28f, 0x7b, 0x29d, 0x10}; w16(M2 + 0xa6, cl[ri(0, 5)]); } wfrand(M2 + 0x10, 3, 10, 90);
        w8(P + 0x20a4, ri(0, 1) ? 0u : u(ri(0, 4))); { static const uint32_t md[] = {0x11, 0x12, 3, 0x16, 0xf, 0x14, 7, 0, 0}; w32(P + 0x208c, md[ri(0, 8)]); }
        w32(P + 0x1c0, ri(0, 5) ? 0u : 1u); w32(OV(0x0015f5c4), ri(0, 5) ? 0u : 1u); w32(P + 0x1630, u(ri(-5, 60))); w8(P + 0x12e7, u(ri(0, 1)));
        w32(0x0015ed84, ri(0, 1) ? u(ri(0, 0x12)) : (ri(0, 1) ? 0xfu : 0x11u)); w32(0x0015eea8, R());
        w32(P + 0x2280, R()); if (ri(0, 7) == 0) w32(P + 0x2084, 0x32);
        return Args{{u(ri(0, 3) ? 1 : 0)}, {}};
    }, [](A a) { return ri32(hero_hurt(static_cast<int32_t>(a.i[0]))); }, OV(0x00173e7c));
    auto itemenv = [inpenv, inp_state](bool wrench) {  // an item in hand and its button
        constexpr uint32_t P = g::PLAYER;
        static const uint32_t sts[] = {0, 1, 0x1e, 3, 7, 8, 9, 6, 0xb, 0xe, 0x2d, 0x11, 0x15, 0x13, 0x23, 0x2c, 0x81, 0x16, 2};
        const uint32_t st = sts[ri(0, 18)];
        inpenv(ri(0, 2) ? st : inp_state());
        w32(P + 0x2084, st);
        static const uint32_t items[] = {8, 8, 8, 0xc, 0xc, 0xa, 0x11, 0x14, 0x18, 0x19, 0xf, 0x12, 0x15, 0x1f, 9, 0x21};
        w32(P + 0x10b8, items[ri(0, 15)]); w32(P + 0x10b4, ri(0, 7) ? 2u : 1u); w32(P + 0x1090, ri(0, 7) ? M2 : 0u); w8(P + 0x20ac, ri(0, 3) ? 0u : 1u);
        w8(P + 0x20a4, ri(0, 5) ? 0u : 1u); w16(P + 0x308, u(ri(0, 2)));
        const uint32_t mask = ri(0, 1) ? 0x80u : 0x20u; w32(P + 0x10a0, mask); w32(P + 0x10b0, u(ri(0, 25)));
        w32(g::PAD + 0x1e0 + 4 * ((ee_read<uint16_t>(g::PAD + 0x18e) + 29u - u(ri(0, 6))) % 30u), ri(0, 2) ? mask : 0u); w32(g::PAD + 0x190, 30);
        w32(g::PAD + 0x1a0, (ri(0, 1) ? mask : 0u) | (ri(0, 1) ? 0xau : 0u) | (ri(0, 2) ? 0u : 8u)); w32(g::PAD + 0x1a4, (ri(0, 2) ? 0u : mask) | (ri(0, 1) ? 8u : 0u));
        for (uint32_t o = 0; o < 0x100; o += 4) w32(M2 + o, ee_read<uint32_t>(M + o));  // the item moby: a copy of the hero moby
        { static const uint16_t cl[] = {0x47, 0x47, 0x1e3, 0x10}; w16(M2 + 0xa6, cl[ri(0, 3)]); } w8(M2 + 0x20, ri(0, 3) ? 0u : 1u); w32(M2 + 0x78, 0x01ee4000 + 0x40 * 44); w32(0x01ee4000 + 0x40 * 44 + 0xc, u(ri(-1, 29)));
        w8(P + 0x12ea, ri(0, 3) ? 0u : 1u); w32(P + 0x5fc, ri(0, 1) ? 0u : 1u); w32(P + 0x450, u(ri(0, 3))); wf(P + 0xaa8, rf(0, 40)); wf(P + 0x2dc, rf(0, 1.2f));
        w32(P + 0xa60, u(ri(0, 2))); w32(P + 0xa9c, ri(0, 2) ? 0u : 1u); w16(P + 0x99e, u(ri(0, 1))); w32(P + 0x968, ri(0, 1) ? 0u : M3); wf(P + 0x998, rf(0, 20)); wf(P + 0x984, rf(0, 20)); w16(P + 0x1e6, ri(0, 3) ? 0u : 1u);
        w32(P + 0x4ac, u(ri(0, 1))); for (uint32_t i = 0; i < 0x24; ++i) { w16(OV(0x001c40b0) + 0x18 * i + 0xe, u(ri(0, 1))); w32(0x0013d428 + 4 * i, u(ri(0, 1))); }  // (P + 0x10b8 is the index of sub_00233e98)
        w32(0x00141660, ri(0, 1) ? 0x1fu : 0u); w16(P + 0x22de, ri(0, 1) ? 0u : 3u); w32(OV(0x0015f674), u(ri(-1, 3))); w32(P + 0x300, u(ri(0, 1))); w8(0x0013d4dc, u(ri(0, 1)));
        w32(P + 0x10b4 + 0xf0, ri(0, 1) ? 2u : 0u); w32(P + 0x10b8 + 0xf0, u(ri(2, 3))); w8(P + 0x22ca, 1);
        w32(P + 0xd08, 0); w32(P + 0xd0c, 0);  // no hand layers (hero_hold_start's own test covers them)
        if (wrench) {  // the wrench at its thresholds
            w32(P + 0x10b8, 8); w32(P + 0x198, u(ri(0, 40))); w16(P + 0x41e, u(ri(0, 1)));
            { static const uint32_t md[] = {0, 1, 4, 6, 6, 6, 7, 2}; w32(P + 0x208c, md[ri(0, 7)]); } w32(P + 0x418, u(ri(0, 1)));
            static const float t[] = {17.5f, 18.5f, 19.5f, 20.5f, 31.5f, 32.5f, 5.0f};
            wf(P + 0xaa8, ri(0, 2) ? t[ri(0, 6)] : rf(0, 40)); wf(P + 0x2dc, ri(0, 1) ? rf(0.05f, 0.65f) : rf(0, 1.2f));
            const uint32_t back = u(ri(0, 12));
            for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, 0);
            w32(g::PAD + 0x1e0 + 4 * ((ee_read<uint16_t>(g::PAD + 0x18e) + 29u - back) % 30u), ri(0, 3) ? ee_read<uint32_t>(P + 0x10a0) : 0u);
        }
        for (uint32_t k = 0; k < 0x24; ++k) { w32(OV(0x00179ac0) + 0x4c * k + 0x28, 0xffffffffu); w32(OV(0x00179ac0) + 0x4c * k + 0x2c, 0xffffffffu); }
    };
    check("hero_item_use", 0x00227fa0, N * 16, Ret::Int32, [itemenv] { itemenv(false); return Args{}; },
          [](A) { return ri32(hero_item_use()); }, OV(0x00173e7c));
    for (uint32_t it : {0xcu, 0xau, 0x11u, 0xfu, 0x12u, 0x15u, 0x1fu}) {  // one check per other item, its gates open
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_item_use/%02x", it);
        check(nm, 0x00227fa0, N * 8, Ret::Int32, [itemenv, it] {
            itemenv(false);
            constexpr uint32_t P = g::PLAYER;
            w32(P + 0x10b8, it); w32(P + 0x10b4, 2); w32(P + 0x1090, M2); w8(P + 0x20a4, 0); w8(P + 0x20ac, 0);
            static const uint32_t md[] = {0, 1, 2, 4, 5, 0xc, 0xf, 3, 7, 6};
            w32(P + 0x208c, md[ri(0, 9)]); w32(P + 0x418, u(ri(0, 1))); w32(P + 0x10b0, u(ri(0, 25))); w16(P + 0x4ac, u(ri(0, 1)));
            static const uint32_t ss[] = {0, 1, 3, 0x1e, 0x23, 0x2c, 7};
            w32(P + 0x2084, ss[ri(0, 6)]); wf(P + 0xaa8, rf(0, 40)); w8(P + 0x20a8, ri(0, 3) ? 0u : 1u); w32(P + 0x198, u(ri(0, 30)));
            if (it == 0x1f) { w16(M2 + 0xa6, ri(0, 3) ? 0x1e3u : 0x10u); w32(0x00141660, ri(0, 3) ? 0x1fu : 0u); w8(P + 0x12e4, 0); }
            const uint32_t mask = ee_read<uint32_t>(P + 0x10a0), back = u(ri(0, 12));
            for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, 0);
            w32(g::PAD + 0x1e0 + 4 * ((ee_read<uint16_t>(g::PAD + 0x18e) + 29u - back) % 30u), ri(0, 3) ? mask : 0u);
            return Args{};
        }, [](A) { return ri32(hero_item_use()); }, OV(0x00173e7c));
    }
    check("hero_item_use/wrench", 0x00227fa0, N * 16, Ret::Int32, [itemenv] {
        itemenv(true);
        constexpr uint32_t P = g::PLAYER;
        static const uint32_t ws[] = {1, 0x1e, 0, 7, 8, 9, 6, 0xb, 0xe, 0x2d, 0x11, 0x15, 0x15, 0x13};
        w32(P + 0x2084, ws[ri(0, 13)]); w8(P + 0x12ea, ri(0, 5) ? 0u : 1u); w32(P + 0xa9c, ri(0, 3) ? 0u : 1u);
        w16(M2 + 0xa6, ri(0, 3) ? 0x47u : 0x10u); w8(M2 + 0x20, ri(0, 5) ? 0u : 1u); w32(P + 0x10b4, 2); w32(P + 0x1090, M2); w8(P + 0x20a4, 0); w8(P + 0x20ac, 0);
        return Args{};
    }, [](A) { return ri32(hero_item_use()); }, OV(0x00173e7c));
    g_ignore_lone_w_lanes = true;  // (task 0392: hero_land_fx's stale pos.w, as hero_try_jump above)
    check("hero_jump_input", 0x00229660, N * 4, Ret::Int32, [inpenv, inp_state] {
        inpenv(inp_state());
        constexpr uint32_t P = g::PLAYER;
        if (ri(0, 3)) {  // jump pressed lately, a while into the state, a wall or ledge kind in slot 3
            w32(P + 0x198, u(ri(6, 30))); w32(g::PAD + 0x190, 30); w32(g::PAD + 0x1e0 + 4 * ((ee_read<uint16_t>(g::PAD + 0x18e) + 29u) % 30u), 0x40);
            w32(P + 0x10b4 + 0xf0, 2); w32(P + 0x10b8 + 0xf0, u(ri(1, 3))); wf(P + 0x229c, rf(0.5f, 1)); if (ri(0, 1)) w16(P + 0x22d8, 0);
        }
        return Args{};
    },
          [](A) { return ri32(hero_jump_input()); }, OV(0x00173e7c));
    g_ignore_lone_w_lanes = false;

    // ======== batch 49: the hero's input -> state transitions (0x229b70), per state
    g_batch = 49;
    // (task 0392) Batches 49..55: a null pointer is RAM, as in the runtime. The states that lose the hero (dying 0x3d / 0x47 / 0x57 / 0x62 /
    // 0x6a / 0x80 / 0x82, warping 0x7f, the falls) run hero_lost's screen fade 0x1f9838, whose packets go at the cursor 0x1611c0 = the
    // packet buffer *(0x173e04): 0 in these synthetic states (no level memory layout), so both sides write the fade's packets at
    // address 0 (the original does; the port threw).
    const bool null_ram_49 = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    auto transenv = [itemenv](uint32_t st) {
        itemenv(false);
        constexpr uint32_t P = g::PLAYER;
        w32(P + 0x2084, st);
        if (ri(0, 3)) { w32(P + 0x1090, 0); }  // mostly no item use
        if (ri(0, 3)) { const uint32_t hm = ee_read<uint32_t>(P + 0x2080); w8(hm + 0xa4, 0xff); }  // mostly no hit
        if (ri(0, 2)) w8(P + 0x12e4, 0);  // mostly out of the water
        w32(P + 0x22a8, u(ri(0, 3) ? 1 : 0)); w16(P + 0x22d4, u(ri(0, 1) ? 0 : ri(1, 20))); w16(P + 0x22d6, u(ri(0, 1) ? 0 : ri(1, 20)));
        w32(g::PAD + 0x1b8, ri(0, 3) ? 0u : 0x10u); w32(g::PAD + 0x1c4, ri(0, 7) ? 0u : 0x10u); w32(OV(0x0015f594), u(ri(0, 1)));
        wf(OV(0x0015f638), ee_read<float>(P + 0x88) + (ri(0, 7) ? -50.0f : 1.0f)); w32(0x0015ed8c, u(ri(0, 0x10)));
        w32(P + 0x2090, ri(0, 1) ? u(ri(0, 0x30)) : (ri(0, 1) ? 1u : 0x1eu)); w32(P + 0x1614, 0);  // (no held moby to retire: M2 is no grid moby)
        w32(P + 0x11a4, u(ri(1, 2))); w32(P + 0x1180, ri(0, 1) ? M2 : 0u); w32(P + 0x1184, ri(0, 1) ? M2 : 0u); w8(M2 + 0x70, u(ri(0, 3))); w8(M2 + 0x53, u(ri(0, 2)));  // (the packs: the hero-moby copy M2)
        w32(P + 0x22c4, u(ri(-1, 2))); w32(P + 0x56c, ri(0, 7) ? 0u : 1u); wf(P + 0x2e0, rf(0, 0.2f)); w32(P + 0x1d4, u(ri(0, 1)));
        for (uint32_t o : {0x254u, 0x258u}) wf(OV(0x0017a680) + o, rf(0, 0.8f));
        if (ee_read<uint32_t>(P + 0x10b4) != 2) w8(P + 0x20aa, 0);  // (slot 0 empty: no hand layers from entry -1)
        if (ri(0, 3) == 0) wf(P + 0x194, 0); w8(P + 0x12e3, u(ri(0, 1))); w16(P + 0x1e0, ri(0, 2) ? 0u : 1u);
        { const uint32_t hm = ee_read<uint32_t>(P + 0x2080); if (ee_read<uint32_t>(hm + 0x6c) - 0x00100000u >= 0x01f00000u) w32(hm + 0x6c, ee_read<uint32_t>(hm + 0x68)); }  // (a sane next key frame)
        {  // (task 0392) and sane key frames in every set the state may switch to: the fixture's set records list 8 frames, the rest of
           // their 0x80 bytes is random, and a set started at a later frame (hero_anim_set 0x2323b8 -> 0x24ebc0: +0x6c from the set's
           // frame +0x51; Rilgar's state 0x6b, frame 9) gave the hero moby a random key frame pointer that 0x208f20 reads
            const uint32_t hm = ee_read<uint32_t>(P + 0x2080), cls = ee_read<uint32_t>(hm + 0x24);
            if (cls - 0x00100000u < 0x01f00000u)
                for (uint32_t s = 0; s < 0x80; ++s) {
                    const uint32_t set = ee_read<uint32_t>(cls + 0x48 + 4 * s);
                    if (set - 0x01e80000u >= 0x00020000u) continue;  // (the fixture's set records only, animenv's 0x1e8d000..)
                    for (uint32_t f = 1; f < 25; ++f)
                        if (ee_read<uint32_t>(set + 0x1c + 4 * f) - 0x00100000u >= 0x01f00000u) w32(set + 0x1c + 4 * f, ee_read<uint32_t>(set + 0x1c));
                }
        }
        w32(OV(0x00160170), 0x800);  // the particle pool full: hero_land_fx's puffs take a stale stack lane as pos.w (tested elsewhere)
        if (st == 0x7f || st == 0x75 || st == 0x76) w32(P + 0x198, u(ri(0, 1) ? ri(0, 300) : (st == 0x7f ? ri(215, 225) : ri(45, 55))));  // (around the timers)
        if (st == 0x38 || st == 0x39) w32(P + 0x10b8, ri(0, 1) ? 0x16u : 0x17u);
        if (st == 0x34 && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0x3c);
        if (st == 0x40) w32(g::PAD + 0x1a0, u(ri(0, 3) ? 0 : (1 << ri(0, 3))));
        if (st == 0) {  // standing: the idle events, the stand animation, the other buttons
            for (int i = 0; i < 5; ++i) {
                const uint32_t e = OV(0x00179890) + 0x70 * u(i);
                w32(e + 0x60, ri(0, 4) ? 0u : 1u); wf(e + 0x58, ri(0, 5) ? (ri(0, 1) ? rf(0.04f, 0.12f) : rf(0.02f, 3)) : 0.0f);
                w32(e + 0x50, ri(0, 2) ? 0xffffffffu : u(ri(0, 3))); w32(e + 0x44, u(ri(0, 1))); w32(e + 0x48, u(ri(0, 3))); wf(e + 0x5c, rf(0, 3));
            }
            w16(P + 0x1e8, ri(0, 3) ? 0u : 1u); w32(P + 0x198, u(ri(0, 120))); w32(OV(0x00179ab0), u(ri(0, 1)));
            if (ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0);
            if (ri(0, 3) == 0) { w32(P + 0x10b4, 2); w32(P + 0x10b8, 0x1b); }
            w32(g::PAD + 0x1a0, ee_read<uint32_t>(g::PAD + 0x1a0) | (ri(0, 3) ? 0u : u(ri(1, 5))));
            w16(P + 0x30c, u(ri(0, 1))); w16(P + 0x30e, u(ri(0, 1) ? 0 : ri(1, 10))); wf(P + 0x2dc, rf(0, 0.8f)); wf(P + 0x229c, rf(0, 1));
            w32(0x0015ed84, ri(0, 3) ? 0xcu : u(ri(0, 0x12)));
        }
    };
    for (uint32_t st : {0x00u, 0x20u, 0x23u, 0x05u, 0x1du, 0x3eu, 0x6bu, 0x01u, 0x17u, 0x30u, 0x2eu, 0x03u, 0x04u, 0x70u, 0x71u, 0x31u, 0x02u, 0x2fu, 0x73u, 0x7eu, 0x07u, 0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu, 0x10u, 0x11u, 0x12u, 0x1cu, 0x3cu, 0x69u, 0x77u, 0x81u, 0x08u, 0x06u, 0x2du, 0x79u, 0x13u, 0x14u, 0x15u, 0x21u, 0x7au, 0x40u, 0x41u, 0x3du, 0x47u, 0x57u, 0x62u, 0x6au, 0x80u, 0x82u, 0x7fu, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x75u, 0x76u, 0x38u, 0x39u, 0x3au, 0x24u, 0x25u, 0x26u, 0x2cu, 0x74u}) {
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_input_update/%02x", st);
        check(nm, 0x00229b70, N * 8, Ret::None, [transenv, st] { transenv(st); return Args{}; },
              [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }
    // ======== batch 50: the focused variants of batch 49
    g_batch = 50;
    check("hero_input_update/pre", 0x00229b70, N * 8, Ret::None, [transenv] {  // the checks before the per-state part
        constexpr uint32_t P = g::PLAYER;
        static const uint32_t sts[] = {0, 0, 0x20, 0x23, 0x05, 0x3e};  // (ported states; 0 or 2 for the 0x3d check)
        transenv(sts[ri(0, 5)]);
        const uint32_t hm = ee_read<uint32_t>(P + 0x2080);
        if (ri(0, 2) == 0) {  // a hit
            const uint32_t idx = u(ri(0, 63)), rec = OV(0x00178100) + 64 * idx;
            w8(hm + 0xa4, idx); w32(rec + 0x34, hm); w32(rec + 0x24, 7); w32(rec + 0x30, u(ri(0, 1))); wfrand(rec + 0x10, 3, -2, 2);
            w32(rec + 0x20, 0); w32(P + 0x1c0, 0); w32(OV(0x0015f5c4), 0); w8(P + 0x20a4, 0); w32(P + 0x208c, 0);
        }
        w32(g::PAD + 0x1a4, ee_read<uint32_t>(g::PAD + 0x1a4) | (ri(0, 1) ? 0x10u : 0u)); w8(P + 0x10ac, ri(0, 3) ? 0u : 2u); w32(OV(0x0015f594), ri(0, 3) ? 0u : 1u);
        w8(P + 0x20ac, ri(0, 2) ? 0u : 1u); w8(P + 0x20a7, ri(0, 2) ? 0u : 1u); w16(P + 0x1b2, u(ri(0, 1))); w8(P + 0x20a4, ri(0, 1) ? 0u : u(ri(0, 4)));
        w32(P + 0x22a8, ri(0, 3) ? 1u : 0u); w32(0x00141660, u(ri(0, 0x20))); w32(P + 0x10b8, ri(0, 2) ? 8u : u(ri(0, 0x1f)));  // (not item 0x20: its mode start 0x29a6b0 is not ported)
        return Args{};
    }, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    check("hero_input_update/02-jump", 0x00229b70, N * 8, Ret::None, [transenv] {  // running: the jump press and the long jump
        transenv(2);
        constexpr uint32_t P = g::PLAYER;
        w8(P + 0x12ea, 0); w16(P + 0x30e, ri(0, 3) ? 0u : u(ri(1, 8))); w16(P + 0x1e2, u(ri(0, 1))); w32(P + 0x1d4, u(ri(0, 1)));
        for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, 0);
        w32(g::PAD + 0x1e0 + 4 * ((ee_read<uint16_t>(g::PAD + 0x18e) + 29u - u(ri(0, 10))) % 30u), ri(0, 3) ? 0x40u : 0u); w32(g::PAD + 0x190, 30);
        w32(g::PAD + 0x1a0, ri(0, 1) ? 0xau : 0u); w32(P + 0x2088, u(ri(0, 2)));
        const float band = ee_read<float>(OV(0x0017bdb8) + 0xc); wf(P + 0x190, band + rf(-0.5f, 0.5f));
        wf(P + 0x98, rf(-3.2f, 3.2f)); wf(P + 0x180, ee_read<float>(P + 0x98) + rf(-1.6f, 1.6f));
        wf(P + 0x164, ee_read<float>(OV(0x0017bdb8) + 0x18) * ee_read<float>(0x0015ed6c) * rf(0.6f, 1.1f)); w32(P + 0x1090, 0);
        return Args{};
    }, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    check("hero_input_update/02-late", 0x00229b70, N * 8, Ret::None, [transenv] {  // running on the ground, no jump: the rest
        static const uint32_t sts[] = {2, 2, 0x2f, 0x73, 0x7e};
        const uint32_t st = sts[ri(0, 4)];
        transenv(st);
        constexpr uint32_t P = g::PLAYER;
        w8(P + 0x12ea, 0); w16(P + 0x30e, ri(0, 7) ? 0u : 1u); for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, 0);
        w32(g::PAD + 0x1a0, ri(0, 2) ? 0u : u(ri(1, 15))); w8(P + 0x12e2, u(ri(0, 1))); wf(P + 0x164, rf(0, 3) * ee_read<float>(0x0015ed6c)); wf(P + 0x2e0, rf(0, 0.2f));
        wf(P + 0x229c, ri(0, 1) ? rf(0, 0.2f) : rf(0, 1)); w32(P + 0x1ac, u(ri(0, 20))); w32(P + 0xa9c, ri(0, 3) ? 0u : 1u);
        { static const uint8_t an[] = {0x37, 0x6d, 0, 0x54}; w8(ee_read<uint32_t>(P + 0x2080) + 0x53, an[ri(0, 3)]); }
        w8(P + 0x20a9, u(ri(0, 1))); w16(P + 0x308, ri(0, 3) ? 0u : 1u); w32(P + 0x56c, ri(0, 7) ? 0u : 1u); w32(P + 0x2094, u(ri(0, 3)));
        w32(g::PAD + 0x1a4, ri(0, 1) ? 0x10000u : 0u); w8(P + 0x20aa, u(ri(0, 1))); wf(P + 0x160, rf(0, 0.4f) * ee_read<float>(0x0015ed6c) * 10.0f);
        w32(P + 0x198, u(ri(0, 40))); w32(g::PAD + 0x1d4, u(ri(0, 1))); wfrand(P + 0xe0, 2, -0.2f, 0.2f); wf(P + 0x3b0, rf(-0.5f, 1)); w32(P + 0x1090, 0);
        return Args{};
    }, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    check("hero_input_update/2f-board", 0x00229b70, N * 8, Ret::None, [transenv] {  // on the board, no buttons: its animations
        transenv(0x2f);
        constexpr uint32_t P = g::PLAYER;
        w8(P + 0x12ea, 0); w16(P + 0x30e, 0); for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, 0);
        w32(g::PAD + 0x1a0, 0); w8(P + 0x12e2, 1); wf(P + 0x2e0, rf(0, 0.2f)); wf(P + 0x229c, ri(0, 1) ? rf(0, 0.2f) : rf(0, 1));
        w32(P + 0x1ac, u(ri(0, 20))); w32(P + 0xa9c, ri(0, 3) ? 0u : 1u); w32(P + 0x22a8, ri(0, 1) ? 1u : 0u); w8(P + 0x20a4, 0);
        { static const uint8_t an[] = {0x37, 0x6d, 0x6e, 0x54, 0}; w8(ee_read<uint32_t>(P + 0x2080) + 0x53, an[ri(0, 4)]); }
        wfrand(P + 0x100, 2, -0.3f, 0.3f); w32(P + 0x56c, 0); w32(P + 0x1090, 0);
        return Args{};
    }, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    check("hero_input_update/02-stop", 0x00229b70, N * 8, Ret::None, [transenv] {  // running, the stick let go: skid or stop
        transenv(2);
        constexpr uint32_t P = g::PLAYER;
        const float k = ee_read<float>(0x0015ed6c);
        w8(P + 0x12ea, 0); w16(P + 0x30e, 0); for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, 0);
        w32(g::PAD + 0x1a0, 0); w8(P + 0x12e2, 0); w8(P + 0x20a9, 0); w16(P + 0x308, 0); w32(P + 0x56c, 0); w8(P + 0x20aa, ri(0, 3) ? 0u : 1u);
        wf(P + 0x229c, rf(0, 0.25f)); wf(P + 0x160, k * rf(1, 6)); w32(P + 0x198, u(ri(0, 40))); w32(P + 0x1090, 0);
        for (uint32_t i = 0; i < 30; ++i) wf(g::PAD + 0x2d0 + 4 * i, rf(0, 1));  // the stick magnitude history
        return Args{};
    }, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    check("hero_input_update/jump-air", 0x00229b70, N * 16, Ret::None, [transenv] {  // in the air: the arc and the presses
        static const uint32_t sts[] = {7, 7, 9, 9, 0xa, 0xb, 0xd, 0xe, 0xe, 0xf, 0x10, 0x11, 0x12, 0x1c, 0x3c, 0x69};
        const uint32_t st = sts[ri(0, 15)];
        transenv(st);
        constexpr uint32_t P = g::PLAYER;
        const float k = ee_read<float>(0x0015ed6c);
        w8(P + 0x12ea, 0); w32(P + 0x56c, 0); w32(P + 0x5fc, ri(0, 7) ? 0u : 1u); w32(P + 0x1090, 0); w16(P + 0x22d8, ri(0, 3) ? 0u : 1u);
        const int32_t t = ri(0, 60); w32(P + 0x198, u(t)); w32(P + 0x420, u(ri(0, 5) == 0 ? t - 1 : ri(0, 20))); w16(P + 0x49a, u(ri(0, 4) ? 1000 : ri(0, 60)));
        w32(P + 0x418, ri(0, 2) ? 0u : u(ri(1, 6))); w16(P + 0x41e, u(ri(0, 1))); w16(P + 0x41c, u(ri(0, 1))); w16(P + 0x30e, ri(0, 2) ? 0u : u(ri(1, 6)));
        w32(P + 0x3d0, u(ri(0, 40))); w32(P + 0x3d4, u(ri(0, 60))); w32(P + 0x3e0, u(ri(0, 60))); wf(P + 0x3d8, rf(0, 30));
        wf(P + 0x440, rf(0, 40)); wf(P + 0x444, rf(10, 40)); wf(P + 0x448, rf(0, 20)); w32(P + 0x44c, u(ri(0, 30))); wf(P + 0x494, rf(0, 2)); wf(P + 0x4a0, -rf(0.01f, 0.05f));
        wf(P + 0xa90, rf(0, 3)); wf(P + 0xaa8, rf(0, 45)); wf(P + 0x2dc, rf(0, 5)); w16(P + 0x4a6, u(ri(0, 30))); w16(P + 0x4aa, u(ri(0, 30))); wf(P + 0x490, rf(0, 3));
        w32(P + 0x1b4, u(ri(0, 1))); w8(P + 0x4ae, u(ri(0, 1))); w8(P + 0x4af, u(ri(0, 1))); w32(P + 0x300, u(ri(0, 1))); w32(P + 0x4e8, ri(0, 7) ? 0u : 1u);
        w32(P + 0x2f8, u(ri(0, 2))); w32(P + 0x450, u(ri(0, 3))); w16(P + 0x30a, u(ri(0, 1))); wf(P + 0x248, rf(0, 3)); w8(P + 0x257, u(ri(0, 1)));
        wfrand(P + 0xe0, 3, -1, 1); wfrand(P + 0x100, 3, -1, 1); wf(P + 0x168, rf(0, 3) * k); wf(P + 0x190, rf(0, 0.5f)); wf(P + 0x164, rf(0, 4) * k); wf(P + 0x229c, rf(0, 1));
        const uint32_t hm = ee_read<uint32_t>(P + 0x2080); w8(hm + 0x51, u(ri(0, 20))); { static const uint8_t an[] = {8, 9, 7, 0}; const uint8_t a = an[ri(0, 3)]; w8(hm + 0x52, a); w8(hm + 0x53, ri(0, 2) ? a : 0u); }
        w32(P + 0x10b4 + 0xf0, ri(0, 1) ? 2u : 0u); w32(P + 0x10b8 + 0xf0, u(ri(2, 4))); w8(0x0013d4c2, u(ri(0, 1)));
        for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, ri(0, 5) ? 0u : (ri(0, 1) ? 0x40u : (ri(0, 1) ? 0xau : 0x1f04au)));
        w32(g::PAD + 0x190, 30); w32(g::PAD + 0x1a0, (ri(0, 1) ? 0x40u : 0u) | (ri(0, 1) ? 0xau : 0u));
        return Args{};
    }, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    auto wideenv = [transenv](uint32_t st) {  // the state with its timers, presses and history spread wide
        transenv(st);
        constexpr uint32_t P = g::PLAYER;
        w32(P + 0x198, u(ri(0, 1) ? ri(0, 40) : ri(0, 200))); w32(P + 0x1090, 0); w8(P + 0x12ea, ri(0, 3) ? 0u : 1u);
        { static const uint32_t pr[] = {0, 0, 0x40, 8, 0x48, 0x10000}; w32(g::PAD + 0x1a4, pr[ri(0, 5)]); }
        { static const uint32_t bt[] = {0, 0, 0x40, 8, 0x48, 0xa, 0x4a, 5}; w32(g::PAD + 0x1a0, bt[ri(0, 7)]); }
        w32(g::PAD + 0x1c0, ri(0, 1) ? 0x40u : 0u); w32(P + 0x1b4, u(ri(0, 1))); w32(P + 0x4e8, ri(0, 5) ? 0u : 1u);
        { static const uint32_t pv[] = {7, 9, 0xe, 0x11, 0x10, 0, 2, 0x13}; w32(P + 0x2090, pv[ri(0, 7)]); }
        w32(OV(0x0015f678), u(ri(-1, 2))); wf(P + 0x88, rf(0, 100)); w32(0x0015ed84, ri(0, 1) ? u(ri(0, 0x12)) : (ri(0, 1) ? 3u : (ri(0, 1) ? 6u : 0x10u)));
        w32(P + 0x418, ri(0, 3) ? 0u : u(ri(1, 6)));
        for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, ri(0, 4) ? 0u : 0x40u);
        wf(P + 0xa70, ri(0, 2) ? rf(0.1f, 2) : 0.0f); wf(P + 0xaa8, rf(0, 40)); w32(P + 0xa60, u(ri(0, 2))); w32(P + 0xa9c, ri(0, 2) ? 0u : 1u); w16(P + 0x1f2, u(ri(0, 1)));
        wf(0x0015ed70, rf(0.01f, 0.03f));  // (a sane gravity scale: long predicted falls overrun the scratchpad's cell list)
    };
    for (uint32_t st : {0x77u, 0x81u, 0x08u, 0x06u, 0x2du, 0x79u, 0x13u, 0x14u, 0x15u, 0x21u, 0x7au, 0x40u, 0x41u, 0x3du, 0x47u, 0x57u, 0x62u, 0x6au, 0x80u, 0x82u, 0x7fu, 0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x75u, 0x76u, 0x38u, 0x39u, 0x3au, 0x24u, 0x25u, 0x26u, 0x2cu, 0x74u}) {
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_input_update/wide-%02x", st);
        check(nm, 0x00229b70, N * 8, Ret::None, [wideenv, st] { wideenv(st); return Args{}; },
              [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }
    check("hero_input_update/00-late", 0x00229b70, N * 8, Ret::None, [transenv] {  // standing, past the early exits
        transenv(0);
        constexpr uint32_t P = g::PLAYER;
        w32(g::PAD + 0x1a0, ri(0, 1) ? 0xau : 0u); w8(P + 0x22ca, 0); w8(P + 0x12ea, 0); w16(P + 0x30e, 0); w32(P + 0x1c4, 0);
        for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, ri(0, 7) ? 0u : 0x40u);
        w32(P + 0x2090, ri(0, 1) ? u(ri(0, 0x30)) : (ri(0, 1) ? 1u : 0x1eu)); w32(P + 0x198, u(ri(0, 40))); w16(P + 0x308, ri(0, 3) ? 0u : 1u);
        w32(P + 0x300, u(ri(0, 1))); w8(ee_read<uint32_t>(P + 0x2080) + 0x53, ri(0, 1) ? 0x1au : 0u); wf(P + 0xaa8, rf(50, 150));
        w32(P + 0x56c, ri(0, 5) ? 0u : 1u); w8(P + 0x12e2, ri(0, 3) ? 0u : 1u); wf(P + 0x2e0, rf(0, 0.2f)); wf(P + 0x229c, rf(0, 1));
        wf(P + 0x180, rf(-3.2f, 3.2f)); wf(OV(0x00166dd8), rf(-3.2f, 3.2f)); w32(P + 0x1090, 0);
        return Args{};
    }, [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));

    // ======== batch 51: hero_input_update, the rest of the states
    g_batch = 51;
    auto t2env = [wideenv, transenv, boxenv, heroAt, treeenv](uint32_t st, bool wide) {
        if (wide) wideenv(st); else transenv(st);
        constexpr uint32_t P = g::PLAYER;
        const float fs = ee_read<float>(0x0015ed6c);
        // walking to a point: the point near (+0x1640), its facing (+0x164c), the speed around the stop
        for (uint32_t i = 0; i < 2; ++i) wf(P + 0x1640 + 4 * i, ee_read<float>(P + 0x80 + 4 * i) + (ri(0, 1) ? rf(-0.15f, 0.15f) : rf(-1, 1)));
        wf(P + 0x164c, ee_read<float>(P + 0x98) + (ri(0, 1) ? rf(-0.05f, 0.05f) : rf(-3, 3)));
        wf(P + 0x160, fs * rf(1.5f, 4)); w8(P + 0x20a4, u(ri(0, 1))); w32(P + 0x1650, ri(0, 3) ? 0u : 1u);
        // the timed states: +0x1660, the time in the state, the last floor
        w32(P + 0x1660, u(ri(0, 2))); w32(P + 0x198, u(ri(0, 1) ? ri(0, 220) : ri(0, 60)));
        wf(P + 0x2f4, ee_read<float>(P + 0x88) + rf(-0.5f, 2));
        if (ri(0, 2) == 0) { static const int32_t edge[] = {15, 40, 65, 100, 150, 170}; w32(P + 0x198, u(edge[ri(0, 5)] + ri(-1, 1))); }  // (the timers' edges)
        { const bool none = ri(0, 1) != 0; for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, none || ri(0, 5) ? 0u : 0x40u); }
        // the board's stance
        w32(g::PAD + 0x1d4, u(ri(0, 1))); w32(P + 0x300, ri(0, 2) ? 0u : 1u); w8(P + 0x12e7, u(ri(0, 1)));
        w16(P + 0x30e, u(ri(0, 1) ? 0 : ri(1, 12))); wf(P + 0x2dc, rf(0, 1.5f)); if (ri(0, 1)) w32(P + 0x198, u(ri(25, 35))); wf(P + 0x90, rf(-1.2f, 1.2f)); wf(P + 0x94, rf(-1.2f, 1.2f));
        if (st == 0x3f && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0x5b);
        if (ri(0, 2)) w32(g::PAD + 0x1a0, ee_read<uint32_t>(g::PAD + 0x1a0) & ~0xau);
        // holding on with the item's button: +0x10a0 the button, +0x96c the hold time, +0x960 the speed
        w32(P + 0x10a0, ri(0, 1) ? 0x80u : 0x20u); if (ri(0, 1)) w32(g::PAD + 0x1a0, ee_read<uint32_t>(g::PAD + 0x1a0) | (ri(0, 1) ? 0x80u : 0x20u));
        wf(P + 0x96c, rf(0, 40)); wf(P + 0x960, fs * rf(0, 10)); w32(P + 0x2088, u(ri(0, 2))); w32(P + 0x988, 0);
        if (st == 0x24 && ri(0, 1)) w32(P + 0x10b8, 0xc);
        if ((st == 0x25 || st == 0x22) && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0xa);
        // the flip: the previous state, +0x209c, the fall prediction's inputs
        { static const uint32_t pv[] = {8, 6, 0xf, 0xa, 0x10, 0x12, 0x3c, 0, 2, 7}; w32(P + 0x2090, pv[ri(0, 9)]); }
        w32(P + 0x209c, ri(0, 1) ? 0x10u : 0u); w32(g::PAD + 0x1a4, ri(0, 1) ? 0x40u : 0u);
        wf(P + 0xa94, rf(0.3f, 2)); wf(P + 0xaa8, rf(0, 60)); w32(P + 0xa9c, ri(0, 3) ? 0u : 1u);
        wf(0x0015ed70, rf(0.01f, 0.03f)); wf(P + 0x4b0, rf(0.005f, 0.03f));  // (a sane gravity: long predicted falls overrun the scratchpad) wf(P + 0x4b8, ri(0, 3) ? rf(0.1f, 2) : 0.0f); w16(P + 0x30c, u(ri(0, 3) ? 1 : 0)); wf(P + 0x4c0, rf(5, 40));
        if (ri(0, 2) == 0) w32(P + 0x198, u(ri(0, 15)));
        // the ledge states: the grab / shimmy animations, +0x600, the stick off the sides
        if (st == 0x74 && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0x73);
        if (st == 0x19 && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0x20);
        w32(P + 0x600, u(ri(0, 10))); wf(P + 0xaa8, rf(0, 30)); w32(g::PAD + 0x1a4, ri(0, 3) ? (ee_read<uint32_t>(g::PAD + 0x1a4) & ~0xau) : 2u);
        // the 0x43 set: +0x22fc's countdown, +0x12ea, the combo step and its time; state 0x16's level / air
        w16(P + 0x22fc, ri(0, 1) ? 0u : u(ri(0, 60))); w8(P + 0x12ea, ri(0, 3) ? 0u : 1u); w8(P + 0x12e5, u(ri(0, 1)));
        if (st == 0x43 && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, u(ri(0, 1) ? 0x12 : ri(0, 1)));
        if (st == 0x16) { w32(0x0015ed84, ri(0, 1) ? 0x10u : u(ri(0, 0x12))); w32(P + 0x2094, ri(0, 1) ? 0xfu : 0u); wf(P + 0xe8, rf(-1, 0.5f)); w32(P + 0x22a8, u(ri(0, 1))); w32(P + 0x2090, ri(0, 1) ? 0x81u : 0u); }
        if (st == 0x51) { w32(P + 0xa60, u(ri(0, 3))); w32(P + 0xa9c, ri(0, 3) ? 0u : 1u); wf(P + 0xaa8, rf(0, 40)); }
        if (st == 0x49 || st == 0x4c || st == 0x5e) {  // the arc's frames around now, landed or not
            const int32_t now = ri(5, 60); w32(P + 0x198, u(now)); w32(P + 0x420, u(ri(0, 3) ? now - ri(1, 4) : now + 1));
            w32(P + 0x3d0, u(now + ri(-5, 5))); w32(P + 0x3d4, u(now + ri(-2, 8))); w32(P + 0x3e0, u(now + ri(-3, 12))); w16(P + 0x41c, u(ri(0, 1))); w16(P + 0x41e, u(ri(0, 2) ? 1 : 0));
            w32(P + 0x418, ri(0, 1) ? 0u : u(ri(0, 1) ? 1 : ri(1, 20))); wf(P + 0x440, rf(5, 30)); wf(P + 0x448, rf(5, 30)); wf(P + 0x494, rf(0.3f, 1)); wf(P + 0x444, rf(5, 30)); wf(P + 0x4a0, rf(0.005f, 0.03f)); wf(P + 0x2dc, rf(0, 3));
            wf(P + 0x180, ee_read<float>(P + 0x98) + rf(-2.5f, 2.5f)); wf(P + 0x190, ri(0, 1) ? rf(0, 0.3f) : rf(0, 3)); wf(P + 0x229c, rf(0, 1)); w32(P + 0x1c4, ri(0, 3) ? 0u : 1u); w32(P + 0x4e8, ri(0, 5) ? 0u : 1u);
            wfrand(P + 0xe0, 3, -0.2f, 0.2f);
        }
        if (st == 0x51 || st == 0x43 || st == 0x49 || st == 0x4c) for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, ri(0, 3) ? 0u : (ri(0, 1) ? 0x80u : 0x40u));  // (square and X presses)
        if (st >= 0x19 && st <= 0x1b && ri(0, 3)) {  // hanging on a box edge (as in batch 47), the stick along it
            const float cx = rf(50, 90), cy = rf(50, 90), hx = rf(0.5f, 4), hy = rf(0.5f, 4), hh = rf(0.5f, 4), z0 = rf(20, 40);
            boxenv(cx, cy, z0, hx, hy, hh, 0);
            const int side = ri(0, 3);
            const float along = ri(0, 1) ? rf(-0.9f, 0.9f) : (ri(0, 1) ? rf(0.85f, 1.1f) : rf(-1.1f, -0.85f)), gap = ee_read<float>(P + 0x234) + rf(0, 0.25f);
            const float face = side == 0 ? 3.14159f : side == 1 ? 0.0f : side == 2 ? -1.5708f : 1.5708f;
            float x = side == 0 ? cx + hx + gap : side == 1 ? cx - hx - gap : cx + along * hx;
            float y = side == 2 ? cy + hy + gap : side == 3 ? cy - hy - gap : cy + along * hy;
            if (side < 2) y = cy + along * hy;
            const float yaw = face + rf(-0.4f, 0.4f), z = z0 + hh - rf(1.15f, 1.65f);
            heroAt(x, y, z, yaw); wf(P + 0x98, yaw); treeenv(x, y, z); w8(P + 0x20a4, 0); w16(P + 0x308, 0);
            wf(P + 0x2d8, z - rf(0.5f, 2));
        }
        if (st >= 0x19 && st <= 0x1b) { const float y = ee_read<float>(P + 0x98); static const float off[] = {1.5708f, -1.5708f, 3.1416f, 0.0f}; wf(P + 0x180, y + off[ri(0, 3)] + rf(-0.9f, 0.9f)); wf(P + 0x229c, rf(0, 1)); }
    };
    for (uint32_t st : {0x65u, 0x66u, 0x67u, 0x68u, 0x7bu, 0x7cu, 0x7du, 0x3fu, 0x24u, 0x25u, 0x26u, 0x2cu, 0x22u, 0x74u, 0x18u, 0x19u, 0x1au, 0x1bu, 0x16u, 0x43u, 0x51u, 0x49u, 0x4cu}) {
        for (int w = 0; w < 2; ++w) {
            static char nm[40];
            std::snprintf(nm, sizeof nm, "hero_input_update/%s%02x", w ? "wide-" : "", st);
            check(nm, 0x00229b70, N * 8, Ret::None, [t2env, st, w] { t2env(st, w != 0); return Args{}; },
                  [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
        }
    }

    // ======== batch 52: hero_input_update, the 0x43 set and the rest
    g_batch = 52;
    auto t3env = [t2env, boxenv, heroAt, treeenv](uint32_t st, bool wide) {
        t2env(st, wide);
        constexpr uint32_t P = g::PLAYER;
        const float fs = ee_read<float>(0x0015ed6c);
        wf(P + 0x160, fs * rf(0, 3)); w32(P + 0xa9c, ri(0, 3) ? 0u : 1u); wf(P + 0x2e0, rf(0, 1)); wf(P + 0x229c, rf(0, 1)); w32(P + 0x1c4, ri(0, 3) ? 0u : 1u);
        w16(P + 0x30e, u(ri(0, 1) ? 0 : ri(1, 10))); wf(P + 0x2dc, rf(0, 3)); w32(P + 0x4e8, ri(0, 5) ? 0u : 1u); w32(P + 0x22a8, u(ri(0, 2) ? 1 : 0));
        if (st == 0x44 && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, u(ri(2, 3)));
        if (st == 0x45) { w32(0x0015ed84, ri(0, 1) ? 6u : u(ri(0, 0x12))); wf(P + 0x88, rf(100, 130)); wf(P + 0x108, fs * rf(-12, 2)); wf(P + 0xe8, fs * rf(-12, 2)); }
        if (st == 0x4f || st == 0x52) { w32(P + 0x198, u(ri(0, 1) ? ri(0, 40) : ri(290, 310))); wf(P + 0x2d8, ee_read<float>(P + 0x88) + rf(-1, 1)); }
        w32(g::PAD + 0x1a0, ri(0, 1) ? 0u : (ri(0, 2) ? 0x40u : 2u));
        if (st == 0x46 || st == 0x5d || st == 0x56) { w32(P + 0x198, u(ri(25, 50))); }
        if (st == 0x5e && ri(0, 1)) {  // a free moby slot for the landing's light shaft (as in moby_spawn's check)
            constexpr uint32_t MB = 0x01e60000;
            w32(OV(0x0015ffdc), MB); w32(OV(0x0015ffe0), MB + 0x200); w32(OV(0x0015ffe8), T4); w32(OV(0x0015ffbc), 0);
            for (int i = 0; i < 3; ++i) { const uint32_t m = MB + 0x100 * u(i); w8(m + 0x20, i == 2 ? 0x10u : 0xfeu); w32(m + 0x38, 0); w32(m + 0x3c, 0); }
            w32(P + 0x418, 0); w16(P + 0x30e, 0); w16(P + 0x41e, 1);
        }
        if (st >= 0x53 && st <= 0x59) {  // the 0x53 set: the swing timer, circle pressed / held, the landing lock
            w16(P + 0x22de, ri(0, 1) ? 0u : u(ri(1, 18))); w32(g::PAD + 0x1a4, ri(0, 3) ? 0u : 0x20u); w32(P + 0x198, u(ri(0, 1) ? ri(0, 60) : ri(290, 310)));
            if (st == 0x53 && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0);
            const bool none = ri(0, 2) == 0;
            for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, none || ri(0, 5) ? 0u : 0x80u);
        }
        if (st == 0x1e) {  // releasing the ready stance: the pack bytes, R1 / L1 and L1 / L2
            w8(P + 0x20a4, ri(0, 2) ? 0u : 1u); w8(P + 0x20aa, u(ri(0, 1))); w8(P + 0x20a5, u(ri(0, 1)));
            static const uint32_t bt[] = {0, 2, 8, 1, 4, 0xa, 3};
            w32(g::PAD + 0x1a0, bt[ri(0, 6)]);
            if (ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0xd); w32(OV(0x0016c780), u(ri(0, 3)));
        }
        if (st >= 0x5b && st <= 0x61) {  // the 0x5a set: its timers, the attack's frames, circle held, all four buttons
            w32(P + 0x1630, u(ri(0, 1))); w16(P + 0x1636, u(ri(0, 1))); wf(P + 0xaa8, rf(15, 32)); w32(P + 0x198, u(ri(0, 40)));
            if (st == 0x60 && ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0xb);
            w32(g::PAD + 0x1a0, ri(0, 1) ? 0x20u : 0u);
            static const uint32_t bt[] = {0x40, 0x80, 0x10, 0x20};
            const bool none = ri(0, 2) == 0;
            for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, none || ri(0, 5) ? 0u : bt[ri(0, 3)]);
        }
        if (st == 0x5a) {  // the idle animations, all four buttons in the history, +0x1636
            if (ri(0, 1)) w8(ee_read<uint32_t>(P + 0x2080) + 0x53, 0); w32(P + 0x198, u(ri(0, 100))); w16(P + 0x1636, u(ri(0, 1)));
            static const uint32_t bt[] = {0x40, 0x80, 0x10, 0x20};
            const bool none = ri(0, 2) == 0;
            for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, none || ri(0, 5) ? 0u : bt[ri(0, 3)]);
            w32(g::PAD + 0x1a0, ri(0, 3) ? 0u : 1u);
        }
        if ((st == 0x4b || st == 0x4d || st == 0x4e) && ri(0, 3)) {  // hanging on a box edge, the 0x43 set's sizes
            const float cx = rf(50, 90), cy = rf(50, 90), hx = rf(0.5f, 4), hy = rf(0.5f, 4), hh = rf(0.5f, 4), z0 = rf(20, 40);
            boxenv(cx, cy, z0, hx, hy, hh, 0);
            const int side = ri(0, 3);
            const float along = ri(0, 1) ? rf(-0.9f, 0.9f) : (ri(0, 1) ? rf(0.85f, 1.1f) : rf(-1.1f, -0.85f)), gap = ee_read<float>(P + 0x234) + rf(0, 0.25f);
            const float face = side == 0 ? 3.14159f : side == 1 ? 0.0f : side == 2 ? -1.5708f : 1.5708f;
            float x = side == 0 ? cx + hx + gap : side == 1 ? cx - hx - gap : cx + along * hx;
            float y = side == 2 ? cy + hy + gap : side == 3 ? cy - hy - gap : cy + along * hy;
            if (side < 2) y = cy + along * hy;
            const float yaw = face + rf(-0.4f, 0.4f), z = z0 + hh - rf(0.65f, 0.9f);
            heroAt(x, y, z, yaw); wf(P + 0x98, yaw); treeenv(x, y, z); w8(P + 0x20a4, 1); w16(P + 0x308, 0);
            wf(P + 0x2d8, z - rf(0.5f, 2));
        }
        if (st == 0x4b || st == 0x4d || st == 0x4e) { const float y = ee_read<float>(P + 0x98); static const float off[] = {1.5708f, -1.5708f, 3.1416f, 0.0f}; wf(P + 0x180, y + off[ri(0, 3)] + rf(-0.9f, 0.9f)); }
    };
    for (uint32_t st : {0x44u, 0x50u, 0x45u, 0x4fu, 0x52u, 0x46u, 0x4au, 0x4bu, 0x4du, 0x4eu, 0x5au, 0x5eu, 0x5bu, 0x5cu, 0x60u, 0x61u, 0x5fu, 0x5du, 0x53u, 0x54u, 0x55u, 0x56u, 0x58u, 0x59u, 0x1eu}) {
        for (int w = 0; w < 2; ++w) {
            static char nm[40];
            std::snprintf(nm, sizeof nm, "hero_input_update/%s%02x", w ? "wide-" : "", st);
            // (stage 5, task C2b) these states' input update reaches sphere_collide / capsule_collide (measured on level 15: 0x5c,
            // 0x5e, wide and not), each level's cases a different subset: A2's collision scratch ranges left out of the checks of
            // every state of this batch (see batch 43's checks of them)
            const CollisionScratchIgnored spr_ignored;
            check(nm, 0x00229b70, N * 8, Ret::None, [t3env, st, w] { t3env(st, w != 0); return Args{}; },
                  [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
        }
    }

    // ======== batch 53: hero_input_update, the rail states
    g_batch = 53;
    auto railenv = [t2env](uint32_t st, bool wide) {
        t2env(st, wide);
        constexpr uint32_t P = g::PLAYER;
        const uint32_t PT = T3 + 0xc00, P2 = T3 + 0xd00; const int n = ri(2, 6), n2 = ri(2, 6); w32(PT, u(n)); w32(P2, u(n2));
        const float bx = ee_read<float>(P + 0x80), by = ee_read<float>(P + 0x84), bz = ee_read<float>(P + 0x88), k6c = ee_read<float>(0x0015ed6c);
        const float dz = ri(0, 1) ? 2.0f : (ri(0, 1) ? 0.4f : 0.15f), dxy = ri(0, 1) ? 6.0f : 2.0f;
        for (int i = 0; i < n; ++i) { wf(PT + 0x10 + 16 * u(i), bx + rf(-dxy, dxy)); wf(PT + 0x14 + 16 * u(i), by + rf(-dxy, dxy)); wf(PT + 0x18 + 16 * u(i), bz + rf(-dz, dz)); wf(PT + 0x1c + 16 * u(i), rf(0.5f, 3)); }
        for (int i = 0; i < n2; ++i) { wf(P2 + 0x10 + 16 * u(i), bx + rf(-6, 6)); wf(P2 + 0x14 + 16 * u(i), by + rf(-6, 6)); wf(P2 + 0x18 + 16 * u(i), bz + rf(-2, 2)); wf(P2 + 0x1c + 16 * u(i), rf(0.5f, 3)); }
        w32(P + 0x560, PT); w32(P + 0x564, u(ri(0, 2) ? ri(0, n - 1) : n - 1)); wf(P + 0x568, rf(0, 0.9f)); w32(P + 0x584, u(ri(0, 1)));
        w32(P + 0x594, P2); w32(P + 0x598, u(ri(0, n2 - 1))); wf(P + 0x59c, rf(0, 0.9f)); w32(P + 0x5a0, u(ri(0, 1)));
        w32(P + 0x590, ri(0, 2) ? 0u : 1u); w32(P + 0x5a4, u(ri(0, 3)));
        w32(P + 0x570, ri(0, 2) ? 0u : u(ri(1, 20))); wf(P + 0x574, rf(0, 14) * k6c); w32(P + 0x56c, u(ri(0, 1)));
        wf(P + 0x57c, rf(-3.2f, 3.2f)); w32(P + 0x58c, u(ri(0, 1))); w32(P + 0x5a8, u(ri(-1, 3)));
        w32(P + 0x5b8, u(ri(0, 1))); w16(P + 0x5bc, u(ri(0, 1))); w16(P + 0x5be, ri(0, 3) ? 0u : 1u); w32(P + 0x5c0, u(ri(0, 1))); w32(P + 0x5c4, u(ri(0, 1))); wf(P + 0x5c8, rf(0.005f, 0.03f));
        wfrand(P + 0x500, 3, -9, 9); wf(P + 0x500, bx + rf(-2, 2)); wf(P + 0x504, by + rf(-2, 2)); wf(P + 0x508, bz + rf(-1, 1)); wf(P + 0x50c, 1.0f);
        wf(P + 0x550, bx + rf(-5, 5)); wf(P + 0x554, by + rf(-5, 5)); wf(P + 0x558, bz + rf(-1, 2));
        wf(P + 0x2dc, ri(0, 1) ? rf(0, 2) : rf(0, 9)); wf(P + 0x164, rf(0.1f, 8)); wf(P + 0x160, rf(0, 0.4f)); wf(P + 0x4a0, rf(0.005f, 0.03f)); wf(0x0015ed70, rf(0.01f, 0.03f));
        wfrand(P + 0xe0, 3, -0.5f, 0.5f); wfrand(P + 0x100, 3, -0.5f, 0.5f); w32(P + 0x22a8, u(ri(0, 3)));
        w32(P + 0x198, u(ri(0, 40))); w32(P + 0x420, u(ri(0, 30))); w16(P + 0x41e, u(ri(0, 1))); w32(P + 0x418, u(ri(0, 2) ? 0 : ri(1, 3))); w16(P + 0x30e, u(ri(0, 1)));
        wf(P + 0x440, rf(5, 30)); wf(P + 0x444, rf(5, 30)); w32(P + 0xa60, u(ri(0, 2))); wf(P + 0xaa8, rf(0, 40)); w8(P + 0x12ee, ri(0, 3) ? 0u : 1u);
        if (ri(0, 3) == 0) { w32(0x0015ed84, ri(0, 1) ? 0x10u : 8u); if (ri(0, 1)) { wf(P + 0x80, 342.0f + rf(-8, 8)); wf(P + 0x84, 111.1f + rf(-8, 8)); wf(P + 0x88, 42.0f + rf(-3, 3)); } }
        w8(P + 0x257, u(ri(0, 1))); w32(P + 0x23c, ri(0, 1) ? M3 : 0u); w16(M3 + 0xa6, ri(0, 1) ? 0x101u : 0x10u);
        if (st == 0x42 && ri(0, 1)) wf(P + 0x88, ee_read<float>(P + 0x508) + rf(-0.4f, 0.4f));
        if (st == 0x29 && ri(0, 1)) {  // a parallel rail (P2 = PT shifted to one side) in the rail table, the stick toward it
            const float yaw = ee_read<float>(P + 0x57c), sd = ri(0, 1) ? 1.5708f : -1.5708f, off = (ri(0, 1) ? 2.0f + 0.75f * static_cast<float>(ri(0, 3)) : rf(1, 5)) + rf(-0.3f, 0.3f);
            const float ox = std::cos(yaw + sd) * off, oy = std::sin(yaw + sd) * off;
            w32(P2, u(n));
            for (int i = 0; i < n; ++i) for (uint32_t c = 0; c < 4; ++c) w32(P2 + 0x10 + 16 * u(i) + 4 * c, ee_read<uint32_t>(PT + 0x10 + 16 * u(i) + 4 * c));
            for (int i = 0; i < n; ++i) { wf(P2 + 0x10 + 16 * u(i), ee_read<float>(P2 + 0x10 + 16 * u(i)) + ox); wf(P2 + 0x14 + 16 * u(i), ee_read<float>(P2 + 0x14 + 16 * u(i)) + oy); }
            const uint32_t TB = T3 + 0xe00; w32(OV(0x0015f70c), TB); w32(OV(0x0015f710), u(ri(1, 2)));
            wf(TB, bx); wf(TB + 4, by); wf(TB + 8, bz); wf(TB + 0xc, 100.0f); w32(TB + 0x10, P2); w32(TB + 0x14, 0);
            wf(TB + 0x20, bx); wf(TB + 0x24, by); wf(TB + 0x28, bz); wf(TB + 0x2c, 100.0f); w32(TB + 0x30, PT); w32(TB + 0x34, 0);
            wf(P + 0x180, yaw + sd + rf(-0.6f, 0.6f)); wf(P + 0x229c, rf(0.5f, 1)); w16(P + 0x41e, 0); w8(P + 0x12ee, 0); w32(P + 0x590, 0); w32(P + 0x198, u(ri(0, 8))); w16(P + 0x1ca, u(ri(0, 1)));
            if (ri(0, 1)) { const uint32_t q = PT + 0x10 + 16 * ee_read<uint32_t>(P + 0x564); wf(P + 0x80, ee_read<float>(q)); wf(P + 0x84, ee_read<float>(q + 4)); wf(P + 0x88, ee_read<float>(q + 8)); }
        }
        if (ri(0, 1)) {  // staying on the rail: moving along it, no end flag, sometimes at level 8's no-jump spot
            for (uint32_t o = 0; o < 16; o += 4) w32(P + 0x100 + o, ee_read<uint32_t>(P + 0xe0 + o));
            w16(P + 0x5be, 0); w32(0x0015ed84, ri(0, 1) ? 8u : 3u);
            if (ri(0, 1)) { wf(P + 0x80, 342.0f + rf(-4.5f, 4.5f)); wf(P + 0x84, 111.1f + rf(-4.5f, 4.5f)); wf(P + 0x88, 42.0f + rf(-3, 3)); wf(OV(0x0015f638), -100.0f); }
            if (ri(0, 2) == 0) wf(P + 0x2dc, rf(7, 9));
        }
        { static const uint32_t bt[] = {0x40, 0x80}; const bool none = ri(0, 2) == 0; for (uint32_t i = 0; i < 30; ++i) w32(g::PAD + 0x1e0 + 4 * i, none || ri(0, 5) ? 0u : bt[ri(0, 1)]); }
    };
    for (uint32_t st : {0x28u, 0x29u, 0x2au, 0x2bu, 0x42u}) {
        for (int w = 0; w < 2; ++w) {
            static char nm[40];
            std::snprintf(nm, sizeof nm, "hero_input_update/%s%02x", w ? "wide-" : "", st);
            check(nm, 0x00229b70, N * 8, Ret::None, [railenv, st, w] { railenv(st, w != 0); return Args{}; },
                  [](A) { hero_input_update(); return uint64_t{0}; }, OV(0x00173e7c));
        }
    }

    // ======== batch 54: the hero update root's helpers (0x2070d0)
    g_batch = 54;
    auto rootenv = [transenv](uint32_t st) {  // the hero in state st, with the root's own inputs spread
        transenv(st);
        constexpr uint32_t P = g::PLAYER;
        w8(P + 0x20ad, u(ri(0, 1))); w8(P + 0x20a8, u(ri(0, 1))); w32(g::PAD + 0x1a0, u(ri(0, 7))); w32(g::PAD + 0x1d4, u(ri(0, 1)));
        for (uint32_t o = 0x1b0; o < 0x1fa; o += 2) w16(P + o, ri(0, 2) ? 0u : u(ri(-2, 5)));
        w16(P + 0x22de, ri(0, 1) ? 0u : u(ri(1, 2))); w32(P + 0x22a8, u(ri(0, 2))); w32(OV(0x0015f5c4), ri(0, 3) ? 0u : 1u);
        w8(P + 0x20a4, ri(0, 1) ? 0u : u(ri(1, 3))); w16(P + 0x1636, u(ri(0, 3))); w8(0x0013d4e2, u(ri(0, 1)));
        for (uint32_t e = OV(0x001798f0); e < OV(0x001798f0) + 0x230; e += 0x70) w32(e, u(ri(-1, 3)));
        w16(P + 0x30e, u(ri(0, 1))); wfrand(P + 0xe0, 3, -0.1f, 0.1f); w32(P + 0x21b4, ri(0, 2) ? 0x20u : u(ri(0, 31))); w32(P + 0x21b0, u(ri(8, 31)));
        { const float step = ee_read<float>(0x0015ed6c) * rf(0.6f, 1.4f); for (uint32_t i = 0; i < 32; ++i) { wf(P + 0x1b00 + 16 * i, step * static_cast<float>(i) + rf(-0.1f, 0.1f) * step); wf(P + 0x1b04 + 16 * i, rf(-0.1f, 0.1f)); wf(P + 0x1b08 + 16 * i, rf(0, 0.2f)); wf(P + 0x1b0c + 16 * i, 1.0f); } }  // (a steady walk: the average step near fs6c)
        wf(P + 0x2d8, ee_read<float>(P + 0x88) - rf(0, 8)); if (ri(0, 7) == 0) wf(P + 0x88, rf(0, 2));
        w32(P + 0xa84, M2); w32(P + 0xa88, M); w32(P + 0xa8c, 0);
        constexpr uint32_t MB = 0x01e60000;  // a free moby slot for the 0x27a spawn
        w32(OV(0x0015ffdc), MB); w32(OV(0x0015ffe0), MB + 0x200); w32(OV(0x0015ffe8), T4); w32(OV(0x0015ffbc), 0);
        for (int i = 0; i < 3; ++i) { const uint32_t m = MB + 0x100 * u(i); w8(m + 0x20, 0x10u); w32(m + 0x38, 0); w32(m + 0x3c, 0); }  // (all taken: the 0x27a spawn fails, entering mode 3 is checked on its own)
        for (uint32_t o = 0; o < 0x100; o += 4) w32(M2 + o, ee_read<uint32_t>(M + o));
        w32(OV(0x0015fbc4), 0); w32(OV(0x0015fbc0), 0x01e20000); for (uint32_t o = 0; o < 0x4000; o += 4) w32(0x01e20000 + o, 0);  // (no light zones, an empty light grid)
    };
    for (uint32_t st : {0x00u, 0x02u, 0x06u, 0x07u, 0x28u, 0x33u, 0x43u, 0x53u, 0x5au}) {
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_timers_update/%02x", st);
        check(nm, 0x002215c8, N * 8, Ret::None, [rootenv, st] { rootenv(st); return Args{}; },
              [](A) { hero_timers_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }
    // (no item in hand: an item's stand / run animation index can lie past the hero class's sets, and
    // the original's moby_pose_capture then polls the scratchpad forever for the end of a key-frame DMA;
    // a rare case still does so with other ORACLE_SEEDs, about one in 2000)
    check("hero_mode_enter", 0x00210a08, N * 8, Ret::None, [rootenv] {
        rootenv(static_cast<uint32_t>(ri(0, 1) ? 0 : 2));
        w8(g::PLAYER + 0x20aa, 0); w8(g::PLAYER + 0x20a8, 0); w32(g::PLAYER + 0x10b4, 0); w32(g::PLAYER + 0x10b8, 0);  // (no item: the plain stand / run animations)
        static const uint32_t sts[] = {0, 2, 6, 7};  // (states whose animations the hero-moby copy M2 has; 1 picks an item set)
        return Args{{u(ri(0, 2)), sts[ri(0, 3)], M2}, {}};  // (mode 3 takes the 0x53 set's animations, which M2 lacks)
    }, [](A a) { hero_mode_enter(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; }, OV(0x00173e7c));

    for (uint32_t st : {0x00u, 0x02u, 0x06u, 0x07u, 0x13u, 0x28u, 0x33u, 0x43u}) {  // the control tick: the state update and the rest
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_tick/%02x", st);
        // (stage 5, task C2b) the control tick in every state checked here reaches sphere_collide / capsule_collide: A2's collision
        // scratch ranges left out of these checks (see batch 43's checks of them)
        const CollisionScratchIgnored spr_ignored;
        check(nm, 0x00211670, N * 2, Ret::None, [updenv, st] {
            updenv(st);
            constexpr uint32_t P = g::PLAYER;
            wf(g::PAD + 0x108, ri(0, 1) ? rf(-0.2f, 0.2f) : rf(-1, 1)); wf(g::PAD + 0x10c, ri(0, 1) ? rf(-0.2f, 0.2f) : rf(-1, 1)); w32(g::PAD + 0x1b0, R() & 0xf000u);
            w32(P + 0x21a8, u(ri(0, 31))); w32(P + 0x21ac, u(ri(28, 32))); w32(P + 0x21b0, u(ri(0, 31))); w32(P + 0x21b4, u(ri(28, 32)));
            wfrand(P + 0x90, 3, -3.2f, 3.2f);
            return Args{};
        }, [](A) { hero_tick(); return uint64_t{0}; }, OV(0x00173e7c));
    }

    for (uint32_t st : {0x00u, 0x02u, 0x06u, 0x3cu, 0x68u, 0x69u, 0x7bu, 0x7du, 0x7fu}) {  // the surface under the hero
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_surface_update/%02x", st);
        check(nm, 0x0020b960, N * 4, Ret::None, [rootenv, st] {
            rootenv(st);
            constexpr uint32_t P = g::PLAYER;
            { static const int kinds[] = {-1, 0, 1, 2, 2, 3, 3, 4, 0xb, 0xb, 0xd, 0xd, 0xe, 7, 8, 9, 0xc}; w16(P + 0x12e0, u(ri(0, 1) ? kinds[ri(0, 16)] : ri(-1, 0xe))); }
            wrand(P + 0x12e2, 3); w8(P + 0x12ed, u(ri(0, 1))); if (ri(0, 1)) w8(P + 0x20a4, 0);
            w16(P + 0x30c, u(ri(0, 1))); wf(P + 0x2dc, rf(0, 1)); w32(P + 0x10e0, ri(0, 1) ? M2 : 0u); w16(M2 + 0xa6, ri(0, 1) ? 0xadu : 0x10u);
            const float z = ee_read<float>(P + 0x88);
            wf(P + 0x2d8, z - rf(0, 1)); wf(P + 0x2f0, z + (ri(0, 1) ? rf(0, 1.3f) : rf(-1.5f, 1.5f))); wf(P + 0x2f4, z + (ri(0, 1) ? rf(0, 1.3f) : rf(-1.5f, 1.5f))); wf(P + 0x108, rf(-1, 0.5f));
            w32(P + 0x300, u(ri(0, 1))); w32(P + 0x208c, ri(0, 1) ? u(ri(0, 0x16)) : (ri(0, 1) ? 0x10u : 7u)); w16(P + 0x41e, u(ri(0, 1)));
            if (ri(0, 3) == 0) w32(0x0015ed84, 0xd);
            return Args{};
        }, [](A) { hero_surface_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }

    check("hero_equipment_spawn", 0x0020ea80, N * 4, Ret::None, [rootenv] {  // the missing equipment mobys spawned
        rootenv(0);
        constexpr uint32_t P = g::PLAYER;
        constexpr uint32_t MB = 0x01e60000;  // free moby slots this time
        w32(OV(0x0015ffdc), MB); w32(OV(0x0015ffe0), MB + 0x100 * 8); w32(OV(0x0015ffe8), T4);
        for (int i = 0; i < 9; ++i) { const uint32_t m = MB + 0x100 * u(i); w8(m + 0x20, ri(0, 3) ? 0xfeu : 0x10u); w32(m + 0x38, 0); w32(m + 0x3c, 0); }
        for (uint32_t o : {0x1090u, 0x1180u, 0x1184u, 0x1130u, 0x10e0u, 0x11d0u, 0x11d4u, 0x1220u}) w32(P + o, ri(0, 2) ? 0u : M3);
        w32(P + 0x10a4, u(ri(0, 3))); w32(OV(0x0015f3f8), u(ri(0, 4))); w32(P + 0x20b8, ri(0, 2) ? 0u : (ri(0, 1) ? 0x24u : 5u));
        static const uint32_t its[] = {0, 0, 8, 3, 0xa, 0x10, 0x16};
        w32(P + 0x20d4, its[ri(0, 6)]); w32(0x0015ed90, u(ri(0, 1))); w32(0x00141660, its[ri(0, 6)]);
        w32(P + 0x20e0, ri(0, 1) ? 0u : u(ri(2, 3))); w32(0x0014166c, ri(0, 1) ? 0u : u(ri(2, 3))); w32(0x0015ed94, u(ri(0, 1))); w16(P + 0x22d8, u(ri(0, 1)));
        w32(0x00141668, ri(0, 1) ? 0u : 0x1du); w32(P + 0x20dc, ri(0, 1) ? 0u : 0x1du); w32(0x00141664, ri(0, 1) ? 0u : 0x1bu);
        w8(0x0013d4e1, u(ri(0, 1))); w8(0x0013d4e2, u(ri(0, 1))); w8(0x0013d4e3, u(ri(0, 1)));
        {  // the item's class already in the gadget slot (its load is item_class_load's check, on level data)
            uint32_t item = ee_read<uint32_t>(P + 0x20d4);
            if (item == 0) item = ee_read<uint32_t>(0x0015ed90) != 0 ? 8u : ee_read<uint32_t>(0x00141660);
            if (item == 0) item = 8;
            w32(OV(0x0016000c), 0); w32(OV(0x001afbc0), ee_read<uint32_t>(OV(0x00179ac0) + item * 0x4c + 0x10));
        }
        return Args{};
    }, [](A) { hero_equipment_spawn(); return uint64_t{0}; }, OV(0x00173e7c));

    for (uint32_t st : {0x00u, 0x02u, 0x12u, 0x13u}) {
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_items_update/%02x", st);
        check(nm, 0x00210928, N * 2, Ret::None, [rootenv, st] {
            rootenv(st);
            constexpr uint32_t P = g::PLAYER;
            w16(P + 0x308, u(ri(0, 2) ? 0 : ri(1, 2))); w32(P + 0x208c, ri(0, 1) ? u(ri(0, 0x18)) : u(ri(0x15, 0x16)));
            w32(P + 0x11a8, u(ri(2, 3)));  // (the pack kind the pack mobys were spawned with)
            w32(OV(0x00166c00u - 0x7570), 0);  // (no smoothed hand turn: hero_rot_smooth leaves a stack lane in the row w, garbage here)
            for (uint32_t k = 0; k < 7; ++k) if (ee_read<uint32_t>(P + 0x10b8 + 0x50 * k) == 0) { w32(P + 0x1090 + 0x50 * k, 0); w32(P + 0x1094 + 0x50 * k, 0); }  // (no moby in a slot of kind 0)
            return Args{};
        }, [](A) { hero_items_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }

    // ======== batch 55: the hero's whole frame (0x2070d0)
    g_batch = 55;
    // (falling 6 lands into the spark / layer animations and grinding 0x28 wants the rail's extra layer:
    // both read the fixture class's key frames, which are not real; their parts are checked on their own)
    g_ignore_lone_w_lanes = true;  // (task 0392: a landing's hero_land_fx particles, their stale pos.w: see hero_try_jump, batch 48)
    for (uint32_t st : {0x00u, 0x02u, 0x07u, 0x13u, 0x33u}) {
        static char nm[40];
        std::snprintf(nm, sizeof nm, "hero_frame_update/%02x", st);
        // (stage 5, task C2b) the hero's frame in every state checked here reaches sphere_collide / capsule_collide: A2's collision
        // scratch ranges left out of these checks (see batch 43's checks of them)
        const CollisionScratchIgnored spr_ignored;
        check(nm, 0x002070d0, N * 2, Ret::None, [updenv, st] {
            updenv(st);
            constexpr uint32_t P = g::PLAYER;
            wf(M + 0x2c, rf(0.2f, 1)); wfrand(M + 0x40, 3, -3.2f, 3.2f);  // (a sane hero moby scale and rotation)
            w32(P + 0x12a8, 0); w16(P + 0x12ac, 0);  // (no look-fx sparks: they read the hand joints of the fixture class)
            wfrand(P + 0x90, 3, -3.2f, 3.2f);  // (wrapped hero angles: the VU sine has no range reduction)
            w32(P + 0x10b8, 0); w8(P + 0x20a8, 0); w8(P + 0x20aa, 0);  // (no item in hand: its layer animations are not in the fixture class)
            w8(P + 0x20a4, 0); w32(OV(0x00166c00u - 0x7570), 0); w32(P + 0xd08, 0); w32(P + 0xd0c, 0); w32(P + 0x1614, 0); w32(P + 0xd00, 0); w32(P + 0xd04, 0); w8(P + 0x20ab, 0); w8(P + 0x20af, 0); w32(P + 0xd10, 0); w32(P + 0x2274, 0); w32(P + 0x1dc, 0); if (ee_read<uint32_t>(P + 0x22a8) == 1) w32(P + 0x22a8, 2); if (ee_read<uint32_t>(P + 0x208c) == 0x11) w32(P + 0x208c, 0);  // (no anim / extra / hand layers or held moby: their fixture frames are not real)
            w32(OV(0x0015fbc4), 0); w32(OV(0x0015fbc0), 0x01e20000); for (uint32_t o = 0; o < 0x4000; o += 4) w32(0x01e20000 + o, 0);  // (no light zones, an empty light grid)
            { constexpr uint32_t MB = 0x01e60000; w32(OV(0x0015ffdc), MB); w32(OV(0x0015ffe0), MB + 0x300); w32(OV(0x0015ffe8), T4); for (int i = 0; i < 3; ++i) w8(MB + 0x100 * u(i) + 0x20, 0x10); }  // (no free moby slot)
            w32(P + 0x11a8, u(ri(2, 3))); for (uint32_t k = 0; k < 7; ++k) { w32(P + 0x1090 + 0x50 * k, 0); w32(P + 0x1094 + 0x50 * k, 0); w32(P + 0x10b4 + 0x50 * k, 0); }  // (empty hand slots: their mobys are checked on their own)
            if (ri(0, 7) == 0) wf(P + 0x80, ri(0, 1) ? 1.0f : 1030.0f);  // (off the map)
            wf(g::PAD + 0x108, rf(-1, 1)); wf(g::PAD + 0x10c, rf(-1, 1)); w32(g::PAD + 0x1b0, R() & 0xf000u);
            w32(P + 0x21a8, u(ri(0, 31))); w32(P + 0x21ac, u(ri(28, 32))); w32(P + 0x21b0, u(ri(0, 31))); w32(P + 0x21b4, u(ri(28, 32)));
            {  // (task 0392) the item's class already in the gadget slot, as batch 54's hero_equipment_spawn has it: the empty hand slot +0x1090
               // spawns the item, and item_class_load would unpack the synthetic state's gadget data (not a WAD: the decompressor's teq;
               // item_class_load is proved on level data by batch 70)
                uint32_t item = ee_read<uint32_t>(P + 0x20d4);
                if (item == 0) item = ee_read<uint32_t>(0x0015ed90) != 0 ? 8u : ee_read<uint32_t>(0x00141660);
                if (item == 0) item = 8;
                w32(OV(0x0016000c), 0); w32(OV(0x001afbc0), ee_read<uint32_t>(OV(0x00179ac0) + item * 0x4c + 0x10));
            }
            return Args{};
        }, [](A) { hero_frame_update(); return uint64_t{0}; }, OV(0x00173e7c));
    }
    g_ignore_lone_w_lanes = false;

    g_ee_null_is_ram = null_ram_49;

    // ======== batch 56: the pad state (0x13c940)
    g_batch = 56;
    auto padenv = [] {  // a pad mid-game: previous sets, history, a controller packet at OUT
        constexpr uint32_t PD = g::PAD;
        w32(PD + 0x1ac, R() & 0xffffu); w32(PD + 0x1bc, R() & 0xffffu); w32(PD + 0x348, R() & 0xffffu);
        w16(PD + 0x18e, u(ri(0, 29))); w32(PD + 0x190, u(ri(0, 30))); w32(PD + 0x1cc, ri(0, 5) ? 0u : u(ri(1, 2)));
        for (uint32_t i = 0; i < 30; ++i) { wf(PD + 0x2d0 + 4 * i, ri(0, 1) ? rf(0, 0.3f) : rf(0.8f, 1.2f)); wf(PD + 0x258 + 4 * i, rf(-3.2f, 3.2f)); }
        w8(PD + 0x34e, ri(0, 2) ? 0u : u(ri(1, 8))); w8(PD + 0x34d, u(ri(0, 8))); w8(PD + 0x34f, u(ri(0, 3)));
        w8(0x0015edb4, ri(0, 3) ? 0u : 1u); wf(0x0015ed68, rf(0.5f, 2));
        const uint32_t b = R() & (ri(0, 1) ? 0xffffu : 0x0ff0u); w8(OUT, ~(b >> 8) & 0xff); w8(OUT + 1, ~b & 0xff);
        for (uint32_t i = 2; i < 6; ++i) w8(OUT + i, ri(0, 2) ? 0x7fu : u(ri(0, 255)));
        for (uint32_t i = 6; i < 0x14; ++i) w8(OUT + i, u(ri(0, 255)));
        if (ri(0, 1)) { const bool hard = ri(0, 1) != 0; w8(OUT + 4, hard ? (ri(0, 1) ? 0u : 255u) : 0x7fu); w32(PD + 0x1ac, ee_read<uint32_t>(PD + 0x1a0)); }
        static const int lens[] = {2, 6, 6, 0x12, 0x12};
        return Args{{PD, OUT, u(lens[ri(0, 4)])}, {}};
    };
    check("pad_build", 0x002671b0, N * 16, Ret::None, padenv, [](A a) { pad_build(a.i[0], hp<uint8_t>(a.i[1]), static_cast<int32_t>(a.i[2])); return uint64_t{0}; });

    check("hero_init", 0x00204c60, N * 2, Ret::None, [updenv] {  // the level start: the hero moby found and placed
        updenv(0);
        constexpr uint32_t MB = 0x01e60000;
        w32(OV(0x0015ffd8), MB); w32(OV(0x0015ffdc), MB + 0x100 * u(ri(0, 4)));
        for (uint32_t i = 0; i < 4; ++i) { for (uint32_t o = 0; o < 0x100; o += 4) w32(MB + 0x100 * i + o, ee_read<uint32_t>(M + o)); w16(MB + 0x100 * i + 0xa6, ri(0, 2) ? u(ri(1, 9)) : 0u); wf(MB + 0x100 * i + 0x48, rf(-3.2f, 3.2f)); }
        w32(OV(0x00160540), u(ri(0, 1))); w32(0x00141660, u(ri(0, 1) ? 0 : ri(1, 0x24))); w32(0x0015eda0, u(ri(1, 8)));
        w32(OV(0x0015fbc4), 0); w32(OV(0x0015fbc0), 0x01e20000); for (uint32_t o = 0; o < 0x4000; o += 4) w32(0x01e20000 + o, 0);
        return Args{};
    }, [](A) { hero_init(); return uint64_t{0}; }, OV(0x00173e7c));

    // ======== batch 57: the level setup's loaders (0x244110), task 0293
    g_batch = 57;
    check("sub_0023f0d8", 0x0023f0d8, N, Ret::None, [] { wrand(T, 8); return Args{{T}, {}}; },
          [](A a) { sub_0023f0d8(a.i[0]); return uint64_t{0}; });
    check("sub_0023f120", 0x0023f120, N, Ret::None, [] {
        const int n = ri(0, 6); wrand(T, 0x100); w32(T, u(n));
        const uint32_t t = T + 0x10 + u(n) * 32;
        for (int i = 0; i < n; ++i) w32(t + 4 * u(i), T2 + 0x10 * u(ri(0, 15)) - t);
        wrand(T2, 0x40);
        return Args{{T}, {}};
    }, [](A a) { sub_0023f120(a.i[0]); return uint64_t{0}; });
    check("sub_0023f630", 0x0023f630, N, Ret::None, [] {
        const int c = ri(0, 8); wrand(T, 0x80); w32(T, u(c)); w32(T + 8, 0x100); w32(T + 0xc, 0x10 * u(ri(1, 16)));
        for (int i = 0; i < c; ++i) w32(T + 0x10 + 4 * u(i), ri(0, 3) ? R() & 0xfff : 0u);
        wrand(T + 0x100, 0x40); wrand(OV(0x00160180), 1);
        const int n = ri(0, 6);
        for (int i = 0; i < 4 * n; ++i) w32(T2 + 4 * u(i), (i & 3) == 3 ? u(1 << ri(0, 12)) + (R() & 3) : R() & 0xfff);
        return Args{{T, u(ri(0, 0x400)), T2, u(n)}, {}};
    }, [](A a) { sub_0023f630(a.i[0], static_cast<int32_t>(a.i[1]), a.i[2], static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
    check("sub_0023f768", 0x0023f768, N, Ret::None, [] {
        const int n = ri(0, 8); wrand(OV(0x0015f41c), 1);
        for (int i = 0; i < 4 * n; ++i) w32(T + 4 * u(i), (i & 3) >= 2 ? u(1 << ri(0, 12)) : R());
        return Args{{T, u(n)}, {}};
    }, [](A a) { sub_0023f768(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    auto classenv = [] {
        const uint32_t idx = u(ri(0, 30)); w32(OV(0x0015ffc0), idx);
        w32(OV(0x00197300) + idx * 4, ri(0, 1) ? T3 : 0u);
        for (uint32_t a = T3; a < T3 + 0x100; a += 4) w32(a, 0);
        w32(T3 + 0x2c, R()); wrand(OUT, 4);
        uint32_t cls;
        const int k = ri(0, 3);
        if (k == 0) cls = u(ri(0, 0x7ff));
        else { const int row = ri(0, 40); cls = ee_read<uint32_t>(OV(0x001ea300) + 0xc * u(row)) & 0x7ff; }
        return cls;
    };
    check("class_update_fn_set", 0x00264688, N, Ret::None, [classenv] { return Args{{classenv()}, {}}; },
          [](A a) { class_update_fn_set(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("moby_class_register", 0x002409b8, N, Ret::None, [classenv] {
        const uint32_t cls = classenv();
        const uint32_t hdr = coin() ? T3 : 0u;
        if (hdr != 0) { for (uint32_t off = 0x10; off <= 0x18; off += 4) w32(T3 + off, coin() ? R() & 0xfff0 : 0u); w32(T3 + 0x28, coin() ? R() & 0xfff0 : 0u); }
        return Args{{hdr, 0, OUT, cls}, {}};
    }, [](A a) { moby_class_register(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
    check("level_collision_set", 0x00241728, N, Ret::None, [] {
        wrand(T, 0x100); w32(T, coin() ? R() & 0xfff0 : 0u);
        const int n = ri(0, 6);
        if (coin()) { w32(T + 4, 0x100); w32(T + 0x100, u(n)); } else w32(T + 4, 0);
        return Args{{T}, {}};
    }, [](A a) { level_collision_set(a.i[0]); return uint64_t{0}; });
    check("sub_00241938", 0x00241938, 4, Ret::None, [] { return Args{{R(), R()}, {}}; }, [](A a) { sub_00241938(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("sub_00284e00", 0x00284e00, 4, Ret::None, [] { wrand(OV(0x001ba4d0), 0x40); wrand(OV(0x001ba5d0), 0x300); return Args{}; },
          [](A) { sub_00284e00(); return uint64_t{0}; });

    auto texenv = [] {  // a texture table (16 entries of 16 bytes) at T4, the GS base 0x15ee8c
        for (int i = 0; i < 16; ++i) {
            const uint32_t e = T4 + 16 * u(i);
            w16(e, 1u << ri(0, 9)); w16(e + 2, 1u << ri(0, 9)); w16(e + 4, ri(0, 3) ? 1u << ri(2, 10) : R() & 0xffff); w16(e + 6, 1u << ri(0, 10));
            w16(e + 8, u(ri(0, 0x40))); w16(e + 0xa, R() & 0x3fff); w16(e + 0xc, R() & 0x3fff); w16(e + 0xe, R() & 0x3fff);
        }
        w32(0x0015ee8c, R() & 0x3fff00);
        for (int i = 0; i < 16; ++i) w8(OUT + u(i), u(ri(0, 15)));
    };
    check("tie_class_register", 0x00240aa8, N, Ret::None, [texenv] {
        texenv();
        for (uint32_t a = T3; a < T3 + 0x1000; a += 4) w32(a, 0);
        wf(T3 + 0x48, rf(0.1f, 50)); w32(OV(0x00160fbc), u(ri(0, 20)));
        for (uint32_t i = 0; i < 3; ++i) {
            if (coin()) continue;
            const int c = ri(0, 4); w32(T3 + 4 * i, 0x100 + 0x100 * i); w8(T3 + 0x20 + i, u(c));
            for (int k = 0; k < c; ++k) { w32(T3 + 0x100 + 0x100 * i + 16 * u(k), R() & 0xff0); wrand(T3 + 0x104 + 0x100 * i + 16 * u(k), 3); }
        }
        w32(T3 + 0xc, R() & 0xfff0); w32(T3 + 0x2c, 0x400);
        const int np = ri(0, 4); w8(T3 + 0x23, u(np)); wrand(T3 + 0x400, np * 20);
        return Args{{T3, coin() ? T4 : 0u, OUT, u(ri(0, 0x7ff))}, {}};
    }, [](A a) { tie_class_register(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
    check("shrub_class_register", 0x00240e80, N, Ret::None, [texenv] {
        texenv();
        for (uint32_t a = T3; a < T3 + 0x1000; a += 4) w32(a, 0);
        wf(T3 + 0x10, rf(0.1f, 50)); w32(OV(0x0016048c), u(ri(0, 20)));
        w32(T3 + 0x2c, coin() ? R() & 0xfff0 : 0u);
        const int n = ri(0, 3); w16(T3 + 0x28, u(n));
        for (int i = 0; i < n; ++i) {
            const uint32_t m = 0x200 + 0x200 * u(i); w32(T3 + 0x40 + 8 * u(i), m);
            const int c = ri(0, 3), k = ri(0, 2); w32(T3 + m + 0x10, u(c)); w32(T3 + m + 0x14, u(k));
            const uint32_t pk = T3 + m + 0x10 + u(k) * 16 + 0x10;
            for (int t = 0; t < c; ++t) { wrand(pk + 0x40 * u(t), 16); w32(pk + 0x40 * u(t) + 0x20, u(ri(0, 15))); }
        }
        if (coin()) { w32(T3 + 0x1c, 0x900); wrand(T3 + 0x900, 16); } else w32(T3 + 0x1c, 0);
        for (int i = 0; i < 8; ++i) w16(T2 + 2 * u(i), i < 2 ? 1u << ri(0, 10) : R() & 0x3fff);
        if (coin()) w16(T2, R() & 0xffff);
        return Args{{T3, coin() ? T4 : 0u, OUT, coin() ? T2 : 0u, u(ri(0, 0x7ff))}, {}};
    }, [](A a) { shrub_class_register(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4]); return uint64_t{0}; });
    check("lod_bands_set", 0x00293f00, N, Ret::None, [] {
        wf(OV(0x00160f60), rf(10, 200)); wf(OV(0x00160f64), rf(5, 100)); wf(OV(0x00160f68), rf(1, 50)); wf(OV(0x0016cc60), rf(0.5f, 3)); wrand(OV(0x001c28f0), 16);
        return Args{};
    }, [](A) { lod_bands_set(); return uint64_t{0}; });
    check("moby_lod_set", 0x00241458, N, Ret::None, [texenv] {
        texenv();
        for (uint32_t a = T3; a < T3 + 0x1000; a += 4) w32(a, 0);
        const int n = ri(0, 3); w32(T3, 0x100); w32(T3 + 4, u(n)); wf(T3 + 8, rf(1, 40)); wf(OV(0x0016cc60), rf(0.5f, 3));
        for (int i = 0; i < n; ++i) {
            const uint32_t e = T3 + 0x100 + 0x40 * u(i); const int c = ri(0, 3);
            w32(e + 0x10, 0x300 + 0x200 * u(i)); w16(e + 0x1c, 0x10 * u(ri(0, 4))); w8(e + 0x28, u(c));
            const uint32_t pk = T3 + 0x100 + 0x300 + 0x200 * u(i) + ee_read<uint16_t>(e + 0x1c);
            for (int t = 0; t < c; ++t) { wrand(pk + 0x50 * u(t), 20); w32(pk + 0x50 * u(t), u(ri(0, 15))); }
        }
        return Args{{T3, T4}, {}};
    }, [](A a) { moby_lod_set(a.i[0], a.i[1]); return uint64_t{0}; });
    check("sub_00241788", 0x00241788, 8, Ret::None, [] {
        const int n = ri(0, 5);
        for (int i = 0; i < n * 8; ++i) w32(T + 4 * u(i), R() & 0xffff);
        return Args{{T, u(n)}, {}};
    }, [](A a) { sub_00241788(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });

    check("wad_decompress", 0x0024d8d0, 6, Ret::Int32, [] {  // real compressed blocks of the Veldin level file
        static std::vector<std::byte> wad;
        if (wad.empty()) disc_read_level_wad(0, wad);
        static const uint32_t blocks[] = {0x382b40, 0x3b9540};
        const uint32_t off = blocks[ri(0, 1)];
        uint32_t size = 0; std::memcpy(&size, wad.data() + off + 3, 4);
        constexpr uint32_t SRC = 0x01800000, DST = 0x01a00000;
        for (uint32_t i = 0; i < size + 0x4000; ++i) w8(SRC + i, off + i < wad.size() ? static_cast<uint8_t>(wad[off + i]) : 0);
        wrand(0x70000000, 0x1000);
        return Args{{SRC, DST}, {}};
    }, [](A a) { return ri32(wad_decompress(a.i[0], a.i[1])); });

    // ======== batch 58: the gameplay loader's leaves (0x241940) and the projection setup
    g_batch = 58;
    static constexpr OvConst<0x0016ca40u> PS{};
    auto projenv = [] {
        wrand(PS, 0x100);
        w32(0x0015ed80, u(ri(0, 1)));
        wf(PS + 0xb0, rf(0.3f, 1.2f)); wf(PS + 0x200, rf(150, 400)); wf(PS + 0x204, rf(100, 300));
        wf(PS + 0x208, rf(600, 1600)); wf(PS + 0x20c, rf(400, 1200)); wf(PS + 0xa0, rf(1, 64)); wf(PS + 0xa4, rf(1000, 1e6f));
        wf(PS + 0x218, rf(0, 100)); wf(PS + 0x21c, rf(200, 600000)); wf(PS + 0x228, rf(0, 255)); wf(PS + 0x22c, rf(0, 255));
        wf(OV(0x00160f60), rf(10, 200)); wf(OV(0x00160f64), rf(5, 100)); wf(OV(0x00160f68), rf(1, 50));
        wrand(OV(0x001c2800), 0x100); wrand(OV(0x00160bf0), 1); wrand(OV(0x001607e0), 4);
    };
    check("screen_setup", 0x001f7848, N, Ret::None, [] {
        wrand(PS + 0x1f0, 0x20); wrand(0x0013e500, 8);
        w16(0x001518d0, ri(0, 3) ? u(ri(256, 720)) : R()); w16(0x001518d2, ri(0, 3) ? u(ri(200, 512)) : R());
        return Args{};
    }, [](A) { screen_setup(); return uint64_t{0}; });
    check("projection_setup", 0x001f7980, N, Ret::None, [projenv] { projenv(); return Args{}; }, [](A) { projection_setup(); return uint64_t{0}; });
    check("fog_setup", 0x001f7170, N, Ret::None, [projenv] {
        projenv(); w32(OV(0x00167014), u(ri(0, 1))); wrand(OV(0x0016017c), 1); wrand(OV(0x0015f458), 1);
        for (uint32_t b : {OV(0x0015f444u), OV(0x00161204u)}) { w8(b, R()); w8(b + 1, R()); w8(b + 2, R()); wf(b + 4, rf(0, 100)); wf(b + 8, rf(200, 600000)); wf(b + 0xc, rf(0, 255)); wf(b + 0x10, rf(0, 255)); }
        return Args{};
    }, [](A) { fog_setup(); return uint64_t{0}; });
    check("frame_rate_set", 0x00261358, N, Ret::None, [] { wrand(0x0015ed60, 8); return Args{{u(ri(0, 1))}, {}}; },
          [](A a) { frame_rate_set(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("bg_color_set", 0x00200ef0, N, Ret::None, [] { return Args{{R() & 0xff, R() & 0xff, R() & 0xff}, {}}; },
          [](A a) { bg_color_set(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    check("sub_00284d90", 0x00284d90, N, Ret::Int32, [] {
        const uint32_t idx = u(ri(0, 3)); const uint32_t r = 0x0014d590 + idx * 0x100;
        for (int k = 0; k < 64; ++k) w16(r + 4 * u(k), ri(0, 2) ? u(ri(1, 60)) : 0u);
        if (coin()) for (int k = 0; k < 64; ++k) w16(r + 4 * u(k), u(ri(1, 60)));
        return Args{{idx, u(ri(-2, 60))}, {}};
    }, [](A a) { return ri32(sub_00284d90(a.i[0], static_cast<int32_t>(a.i[1]))); });
    check("sub_001f83d0", 0x001f83d0, 4, Ret::None, [] { wrand(OV(0x0015f2f0), 0x50); wrand(OV(0x00161200), 0x40); return Args{}; }, [](A) { sub_001f83d0(); return uint64_t{0}; });
    check("sub_001fb028", 0x001fb028, 4, Ret::None, [] { wrand(OV(0x0015f45c), 1); return Args{}; }, [](A) { sub_001fb028(); return uint64_t{0}; });
    check("level_slot_set", 0x0024a798, N, Ret::None, [] {
        w32(0x0015ed84, u(ri(0, 0x14))); for (int i = 0; i < 0x14; ++i) w32(OV(0x001c44b8) + 4 * u(i), u(ri(0, 40)));
        return Args{{R(), u(ri(0, 8))}, {}};
    }, [](A a) { level_slot_set(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("sub_00266d48", 0x00266d48, 4, Ret::None, [] { wrand(0x0013cacc, 2); return Args{}; }, [](A) { sub_00266d48(); return uint64_t{0}; });
    check("sub_0028df58", 0x0028df58, N, Ret::None, [] {
        const int row = ri(0, 30); w16(T, ri(0, 3) ? ee_read<uint32_t>(OV(0x001ea980) + 8 * u(row)) : R()); wrand(T + 2, 2);
        return Args{{T}, {}};
    }, [](A a) { sub_0028df58(a.i[0]); return uint64_t{0}; });
    auto shipenv = [] {
        w32(M + 0x24, T);
        for (int i = 0; i < 4; ++i) { w32(T + 0x48 + 4 * i, T2 + 0x100 * i); wrand(T2 + 0x100 * i, 0x40); w8(T2 + 0x100 * i + 0x10, u(ri(1, 12))); w8(T2 + 0x100 * i + 0x11, R()); w8(T2 + 0x100 * i + 0x12, R()); }
        for (int i = 0; i < 4; ++i) for (int f = 0; f < 16; ++f) w32(T2 + 0x100 * i + 0x1c + 4 * f, T3 + 0x10 * f);
        wfrand(T3, 64, -9, 9); w32(T + 0x10, R());
        w8(M + 0x51, u(ri(0, 12))); w8(M + 0x70, R()); w16(M + 0xa6, u(ri(0x213, 0x218))); w16(M + 0x34, R()); w32(M + 0x78, OUT); wrand(OUT, 4);
        w32(0x0013e030, coin() ? M2 : 0u); w16(M2 + 0x34, R()); w32(M2 + 0x94, R());
        w32(0x0015ed84, ri(0, 2) ? u(ri(0, 18)) : 0xdu);
    };
    check("ship_hide", 0x0028e990, N, Ret::None, [shipenv] { shipenv(); return Args{}; }, [](A) { ship_hide(); return uint64_t{0}; });
    check("ship_init", 0x0028e8a0, N, Ret::None, [shipenv] { shipenv(); return Args{{M}, {}}; }, [](A a) { ship_init(a.i[0]); return uint64_t{0}; });
    check("hero_level_reset", 0x00204f80, N, Ret::None, [updenv] { updenv(u(ri(0, 1)) ? 2u : 0u); return Args{}; }, [](A) { hero_level_reset(); return uint64_t{0}; });

    // ======== batch 59: the level setup on the real level read from the disc (task 0293)
    g_batch = 59;
    // ======== batch 60: the camera system (0x1eb188..0x1ee6b8) and its math
    g_batch = 60;
    if (g_only_batch == 0 || g_only_batch == 60) g_ignored_words = vcross_lanes();
    static const uint32_t CAMS = OV(0x00167150), LIVE = OV(0x00169890), HERO_M = M2 + 0x200, PLAT = M2 + 0x400;
    auto unitrows = [](uint32_t a) {  // three rows of a random rotation (yaw, pitch, roll), w random
        const float y = rf(-3.1f, 3.1f), p = rf(-1.5f, 1.5f), r = rf(-3.1f, 3.1f);
        const float cy = std::cos(y), sy = std::sin(y), cp = std::cos(p), sp = std::sin(p), cr = std::cos(r), sr = std::sin(r);
        const float m[3][3] = {{cy * cp, sy * cp, -sp}, {cy * sp * sr - sy * cr, sy * sp * sr + cy * cr, cp * sr}, {cy * sp * cr + sy * sr, sy * sp * cr - cy * sr, cp * cr}};
        for (int i = 0; i < 3; ++i) { for (int j = 0; j < 3; ++j) wf(a + 16 * u(i) + 4 * u(j), m[i][j]); wf(a + 16 * u(i) + 12, rf(-1, 1)); }
    };
    auto camsysenv = [unitrows] {
        const int ncam = ri(1, 48);
        w32(OV(0x0015ef54), u(ncam)); w32(OV(0x0015ef50), T2);
        for (int i = 0; i < 48; ++i) {
            const uint32_t c = CAMS + 0xa0 * u(i), e = T2 + 0x20 * u(i), d = T3 + 0x40 * u(i);
            unitrows(c); wfrand(c + 0x30, 4, -50, 50); wfrand(c + 0x40, 12, -5, 5);
            wfrand(c + 0x64, 3, -50, 50); w32(c + 0x70, OV(0x00169110)); w32(c + 0x74, u(ri(0, 8)));
            wf(c + 0x78, coin() ? rf(-0.05f, 0.05f) : -1.0f); w8(c + 0x7c, u(ri(0, 5))); w8(c + 0x7d, u(ri(0, 2)));
            w16(c + 0x7e, u(ri(0, 6))); w16(c + 0x80, 0xffff); w16(c + 0x82, 0xffff); w16(c + 0x84, u(ri(0, ncam - 1)));
            w16(c + 0x86, u(ri(1, 7))); w16(c + 0x8c, u(ri(5, 7)));  // (type rows 0..4, the follow / look / id 5..7 cameras, need a level: batches 61, 70, 72) w16(c + 0x8e, u(ri(0, 1)));
            w32(LIVE + 4 * u(i), u(ri(0, 1)));
            wrand(e, 8); w16(e + 0xc, u(ri(0, 0x17))); w32(e + 0x1c, ri(0, 7) ? d : 0u);
            wrand(d, 16); w32(d + 0xc, 0xffffffffu); w8(d + 0x1c, u(ri(0, 5))); w8(d + 0x1d, u(ri(0, 7))); w8(d + 0x1f, u(ri(0, 8)));
            w32(d + 0x24, u(ri(-1, 3)));
        }
        w32(OV(0x00166e00), CAMS + 0xa0 * u(ri(0, 47))); w32(OV(0x00166e04), CAMS + 0xa0 * u(ri(0, 47)));
        wfrand(OV(0x00166dc0), 0x10, -50, 50); wfrand(OV(0x00166e10), 0x40, -2, 2); wfrand(OV(0x00166ef0), 0x50, -2, 2);
        w16(OV(0x00166ef0), u(ri(0, 3))); w8(OV(0x00166ef2), u(ri(0, 2))); w8(OV(0x00166ef3), u(ri(0, 2)));
        { const int fr = ri(1, 30); w32(OV(0x00166f6c), u(ri(-1, fr))); wf(OV(0x00166f70), 1.0f / static_cast<float>(fr)); w32(OV(0x00166f74), u(ri(1, 40))); }  // frames left, 1 / frames, count
        wf(OV(0x00166f00), coin() ? 1.0f : rf(0, 1)); wf(OV(0x00166f0c), coin() ? 1.0f : rf(0, 1));
        unitrows(OV(0x00166fd0)); w32(OV(0x00166ed4), 0);
        for (uint32_t q : {OV(0x00166f10u), OV(0x00166f40u), OV(0x00166fa0u), OV(0x00166fc0u)}) { float v[4]; float n = 0; for (float& x : v) { x = rf(-1, 1); n += x * x; } n = std::sqrt(n); for (int k = 0; k < 4; ++k) wf(q + 4 * u(k), v[k] / n); } w32(OV(0x00166ee4), coin() ? PLAT : 0u); w32(OV(0x0015ef4c), 0);
        w32(0x001413d0, HERO_M); unitrows(HERO_M + 0xc0);
        wfrand(0x0013f3d0, 4, -50, 50); wfrand(0x0013f490, 4, -0.3f, 0.3f); wfrand(0x0013f5e0, 4, -1, 1); wf(0x0013f3e8, rf(-50, 50));
        wf(0x0013f640, rf(-50, 50));
        w32(0x001415d4, coin() ? 0x50u : u(ri(0, 8))); w32(0x001413d4, coin() ? 0x11u : u(ri(0, 0x80))); w32(0x001413dc, u(ri(0x10, 0x13)));
        w32(0x0013f64c, ri(0, 2) ? PLAT : 0u); w16(PLAT + 0xa6, coin() ? u(ri(0, 0x500)) : 0x336u); wf(PLAT + 0x18, rf(-50, 50));
        w32(OV(0x0015f70c), OUT); wrand(OUT, 0x40); w32(0x0013f8b0, ee_read<uint32_t>(OUT + 0x10 + 0x20 * u(ri(0, 3)))); w32(0x0013f8c0, u(ri(0, 1)));
        w32(OV(0x0016c06c), u(ri(0, 1))); wf(0x0015ed60, rf(0.5f, 1.5f)); w32(0x0015ed84, u(ri(0, 1))); w32(OV(0x0015f5c4), u(ri(0, 6)));
        for (uint32_t b : {0x00140634u, 0x00140635u, 0x00140636u, 0x0014063bu, 0x0014063cu}) w8(b, ri(0, 3) == 0 ? 1u : 0u);
        wfrand(OV(0x0015f3fc), 1, 0, 2); wf(OV(0x00166ed8), coin() ? 0.0f : rf(0, 1)); w32(OV(0x001ca500), u(ri(0, 1)));
        w32(OV(0x00167010), 0);  // no per-frame camera hook
    };
    auto qrand = [](uint32_t a) { wfrand(a, 4, -1, 1); };
    check("angle_wrap", 0x00200178, N * 2, Ret::Float, [] { return Args{{}, {coin() ? rf(-4, 4) : rf(-40, 40)}}; },
          [](A a) { return rf32(angle_wrap(a.f[0])); });
    check("quat_nlerp", 0x001ffea8, N * 2, Ret::None, [qrand] { qrand(M); qrand(M + 0x10); return Args{{OUT, M, M + 0x10}, {rf(0, 1)}}; },
          [](A a) { quat_nlerp(VV(a.i[0]), VV(a.i[1]), VV(a.i[2]), a.f[0]); return uint64_t{0}; });
    check("quat_to_mat3", 0x001fffe8, N, Ret::None, [qrand] { qrand(M); wrand(OUT, 16); return Args{{M, OUT}, {}}; },
          [](A a) { quat_to_mat3(VV(a.i[0]), MM(a.i[1])); return uint64_t{0}; });
    check("quat_to_mat4", 0x00200060, N, Ret::None, [qrand] { qrand(M); return Args{{M, OUT}, {}}; },
          [](A a) { quat_to_mat4(VV(a.i[0]), MM(a.i[1])); return uint64_t{0}; });
    check("cos_ease", 0x00257e58, N * 2, Ret::Float, [] { const float t = ri(0, 4) == 0 ? static_cast<float>(ri(0, 1)) : rf(0, 1); return Args{{}, {rf(-5, 5), rf(-5, 5), t}}; },
          [](A a) { return rf32(cos_ease(a.f[0], a.f[1], a.f[2])); });
    check("mat_to_quat_at", 0x0025d168, N * 3, Ret::None, [unitrows] {
        unitrows(M); if (coin()) { wf(M, -std::fabs(ee_read<float>(M))); wf(M + 0x14, -std::fabs(ee_read<float>(M + 0x14))); }
        return Args{{OUT, M}, {}};
    }, [](A a) { mat_to_quat_at(a.i[0], a.i[1]); return uint64_t{0}; });
    check("quat_rotate", 0x0025fb78, N, Ret::None, [qrand] { qrand(M); qrand(M + 0x10); return Args{{OUT, M, M + 0x10}, {}}; },
          [](A a) { quat_rotate(VV(a.i[0]), VV(a.i[1]), VV(a.i[2])); return uint64_t{0}; });
    check("vec_rotate_axis", 0x0025fc08, N * 2, Ret::None, [qrand] { qrand(M); qrand(M + 0x10); return Args{{OUT, M, M + 0x10}, {coin() ? rf(-4, 4) : 0.0f}}; },
          [](A a) { vec_rotate_axis(VV(a.i[0]), VV(a.i[1]), VV(a.i[2]), a.f[0]); return uint64_t{0}; });
    check("camera_spring", 0x001eb240, N * 3, Ret::Float, [] {
        wf(M, rf(-1, 1)); return Args{{M}, {rf(-5, 5), rf(-5, 5), rf(0, 0.5f), rf(0, 0.5f), coin() ? 0.0f : rf(0, 0.3f)}};
    }, [](A a) { return rf32(camera_spring(a.f[0], a.f[1], a.f[2], a.f[3], a.f.size() > 4 ? a.f[4] : 0.0f, a.i[0])); });
    check("camera_angles", 0x001ebb30, N * 2, Ret::None, [unitrows] {
        wfrand(M, 4, -20, 20); wfrand(M + 0x10, 4, -20, 20); unitrows(M + 0x20); wrand(OUT, 4);
        return Args{{OUT, M, M + 0x10, M + 0x20, M + 0x30, M + 0x40}, {}};
    }, [](A a) { camera_angles(a.i[0], VV(a.i[1]), VV(a.i[2]), VV(a.i[3]), VV(a.i[4]), VV(a.i[5])); return uint64_t{0}; });
    check("camera_type_row", 0x001ed230, N, Ret::Int32, [] { return Args{{u(ri(-2, 0x18))}, {}}; },
          [](A a) { return ri32(camera_type_row(static_cast<int32_t>(a.i[0]))); });
    check("camera_masks", 0x001ecf40, N * 2, Ret::None, [camsysenv] { camsysenv(); return Args{}; }, [](A) { camera_masks(); return uint64_t{0}; });
    check("camera_timer_tick", 0x001ed060, N, Ret::None, [camsysenv] { camsysenv(); return Args{}; }, [](A) { camera_timer_tick(); return uint64_t{0}; });
    check("camera_track", 0x001eca70, N * 2, Ret::None, [camsysenv] { camsysenv(); return Args{}; }, [](A) { camera_track(); return uint64_t{0}; });
    check("camera_shake", 0x001ec960, N * 2, Ret::None, [camsysenv] {
        camsysenv(); wf(M, rf(-1, 1)); w32(M + 8, u(ri(0, 20))); w32(M + 0xc, u(ri(0, 25))); return Args{{M, u(ri(0, 1))}, {}};
    }, [](A a) { camera_shake(a.i[0], static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("camera_accept", 0x001eb810, N * 4, Ret::Int32, [camsysenv] {
        camsysenv(); return Args{{CAMS + 0xa0 * u(ri(0, 47)), coin() ? CAMS + 0xa0 * u(ri(0, 47)) : 0u}, {}};
    }, [](A a) { return ri32(camera_accept(a.i[0], a.i[1])); });
    check("camera_switch", 0x001eb510, N * 3, Ret::None, [camsysenv] { camsysenv(); return Args{{CAMS + 0xa0 * u(ri(0, 47))}, {}}; },
          [](A a) { camera_switch(a.i[0]); return uint64_t{0}; });
    check("camera_select", 0x001eba20, N * 2, Ret::Int32, [camsysenv] { camsysenv(); return Args{}; }, [](A) { return ri32(camera_select()); });
    check("camera_trans_begin", 0x001ebea0, N * 3, Ret::None, [camsysenv] { camsysenv(); return Args{{CAMS + 0xa0 * u(ri(0, 47))}, {}}; },
          [](A a) { camera_trans_begin(a.i[0]); return uint64_t{0}; });
    check("camera_blend", 0x001ec0f8, N * 2, Ret::Int32, [camsysenv] { camsysenv(); return Args{{CAMS + 0xa0 * u(ri(0, 47)), OV(0x00166f00)}, {}}; },
          [](A a) { return ri32(camera_blend(a.i[0], a.i[1])); });
    check("camera_orbit", 0x001ec2d8, N * 3, Ret::Int32, [camsysenv] { camsysenv(); return Args{{CAMS + 0xa0 * u(ri(0, 47)), OV(0x00166f60)}, {}}; },
          [](A a) { return ri32(camera_orbit(a.i[0], a.i[1])); }, OV(0x00166fec), OV(0x00166ffc));  // w of the crossed rows: a stale VU0 lane (vf3.w)
    check("camera_trans_step", 0x001ec8b0, N * 2, Ret::None, [camsysenv] { camsysenv(); return Args{{CAMS + 0xa0 * u(ri(0, 47))}, {}}; },
          [](A a) { camera_trans_step(a.i[0]); return uint64_t{0}; }, OV(0x00166fec), OV(0x00166ffc));
    check("camera_fog", 0x001ee6b8, N * 2, Ret::None, [] {
        w32(OV(0x0015fbc4), u(ri(0, 5)));
        for (int i = 0; i < 6; ++i) {
            wfrand(OV(0x0017ecc0) + 16 * i, 3, -10, 10); wf(OV(0x0017ecc0) + 16 * i + 12, rf(0, 120));
            const uint32_t r = OV(0x0017eec0) + 0x80 * u(i); wfrand(r, 16, -0.3f, 0.3f); w32(r + 0x50, R()); wrand(r + 0x54, 2); wfrand(r + 0x5c, 8, 0, 1);
        }
        wfrand(M, 4, -10, 10); return Args{{M}, {}};
    }, [](A a) { camera_fog(a.i[0]); return uint64_t{0}; });

    // ======== batch 62: the overlay's start-up tables (0x23d028)
    g_batch = 62;
    auto invenv = [] {
        w32(0x0015ed84, ri(0, 3) == 0 ? u(ri(0x13, 0x14)) : u(ri(0, 0x12)));
        for (uint32_t i = 0; i < 0x28; ++i) { w8(0x0013d4c0 + i, ri(0, 3) == 0 ? 1u : 0u); w8(0x0013d4e8 + i, R() & 1); }
        for (uint32_t i = 0; i < 12; ++i) w8(0x0015edd0 + i, ri(0, 2) == 0 ? 0xffu : (u(ri(0, 0x25)) | (coin() ? 0x40u : 0u)));
        w8(0x0015eddc, 0xff); w8(0x0015eddd, 0xff); w8(0x0015edde, 0xff); w8(0x0015eddf, 0xff);
        for (uint32_t i = 0; i < 8; ++i) w32(0x00141ea0 + i * 4, ri(0, 2) == 0 ? u(ri(1, 0x24)) : 0u);
        for (uint32_t i = 0; i < 0x14; ++i) w8(0x0013dd40 + i, ri(0, 3) == 0 ? 1u : 0u);
        wrand(0x0013d510, 0x14); wrand(0x0013d428, 0x25);
        w32(0x0015ee24, R() & 0xffff);
    };
    check("sub_002a07c8", 0x002a07c8, N, Ret::None, [invenv] { invenv(); return Args{}; }, [](A) { sub_002a07c8(); return uint64_t{0}; });
    check("particle_table_init", 0x00268900, 4, Ret::None, [] { wrand(OV(0x001b1e80), 0x52); return Args{}; }, [](A) { particle_table_init(); return uint64_t{0}; });
    check("level_arrival_message", 0x00262d38, N, Ret::None, [invenv] { invenv(); return Args{{u(ri(-3, 0x16))}, {}}; },
          [](A a) { level_arrival_message(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("level_mark_visited", 0x002607d0, N, Ret::None, [invenv] { invenv(); return Args{{u(ri(0, 0x12))}, {}}; },
          [](A a) { level_mark_visited(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("quick_select_add_level_item", 0x0023d3d8, N, Ret::None, [invenv] { invenv(); return Args{{u(ri(0, 0x12))}, {}}; },
          [](A a) { quick_select_add_level_item(static_cast<int32_t>(a.i[0])); return uint64_t{0}; });
    check("item_give", 0x00260860, N * 2, Ret::None, [invenv] { invenv(); return Args{{u(ri(0, 0x24)), u(ri(0, 1))}, {}}; },
          [](A a) { item_give(static_cast<int32_t>(a.i[0]), static_cast<int32_t>(a.i[1])); return uint64_t{0}; });
    check("inventory_level_start", 0x0023d198, N * 2, Ret::None, [invenv] { invenv(); return Args{}; }, [](A) { inventory_level_start(); return uint64_t{0}; });
    check("overlay_init_tables", 0x0023d028, 8, Ret::None, [invenv] { invenv(); wrand(OV(0x0016ca40), 0x100); w16(0x001518d0, u(ri(256, 720))); w16(0x001518d2, u(ri(200, 512)));
        wf(OV(0x0016caf0), rf(0.3f, 1.2f)); return Args{}; }, [](A) { overlay_init_tables(); return uint64_t{0}; });


    // ======== batch 64: the overlay's main loop helpers (0x245c28) and the view setup 0x1f6e48
    g_batch = 64;
    check("sparkle_add", 0x001fe990, N, Ret::Int32, [] {
        for (uint32_t k = 0; k < 16; ++k) w16(OV(0x0016ea40) + k * 0x20 + 0x10, ri(0, 3) == 0 ? 0u : u(ri(-2, 30)));
        wfrand(OUT, 4, -9, 9); return Args{{OUT, R() & 0xffff}, {rf(0, 3)}};
    }, [](A a) { return r32(static_cast<uint32_t>(sparkle_add(ee_ref<Vec4>(a.i[0]), a.i[1], a.f[0]))); });
    check("frame_counters_reset", 0x001f6d70, 8, Ret::None, [] { w32(OV(0x00169fe4), R()); w32(OV(0x00169fe8), R()); w32(OV(0x00169e30), R()); return Args{}; },
          [](A) { frame_counters_reset(); return uint64_t{0}; });
    check("music_volumes_default", 0x00265860, 4, Ret::None, [] { wrand(0x00151700, 0x14); return Args{}; }, [](A) { music_volumes_default(); return uint64_t{0}; });
    check("autosave_tick", 0x00284c50, N, Ret::None, [] {
        w32(OV(0x0015f5ac), ri(0, 3) == 0 ? 1u : 0u); w32(0x0015ee24, coin() ? u(ri(0, 200)) : u(ri(0, 4)) * 0x32 - 1); w32(0x0015ed80, R() & 1); return Args{};
    }, [](A) { autosave_tick(); return uint64_t{0}; });
    check("render_enables_init", 0x001f3638, 4, Ret::None, [] { wrand(OV(0x00169ff0), 0x14); return Args{}; }, [](A) { render_enables_init(); return uint64_t{0}; });
    g_extra_noop_hooks = {OVF(0x001f3770), 0x001221b0};
    check("debug_strings_flush", 0x001f36f8, N, Ret::None, [] { w32(OV(0x0015efc4), u(ri(-1, 4))); wrand(OV(0x0016a840), 0x10); w32(OV(0x0015efc0), R()); return Args{}; },
          [](A) { debug_strings_flush(); return uint64_t{0}; });
    check("packet_call_151b60", 0x00201350, 8, Ret::None, [] { w32(OV(0x001611c0), M + 0x10 * u(ri(0, 8))); return Args{}; },
          [](A) { packet_call_151b60(); return uint64_t{0}; });
    check("packet_call_display", 0x00200f40, N, Ret::None, [] { w32(OV(0x001611c0), coin() ? M + 0x10 * u(ri(0, 8)) : 0u); w32(0x0015eeb8, R()); return Args{}; },
          [](A) { packet_call_display(); return uint64_t{0}; });
    check("start_mode_select", 0x0028eed0, N, Ret::None, [] {  // the gameplay starts (the planet arrival's: batch 75)
        static const uint32_t lv[] = {0, 0x14, 0x15};
        const int k = ri(0, 4);
        w32(0x0015ed84, k < 3 ? lv[k] : k == 3 ? 1u : 0xeu); w8(0x0013dd43, 0); w8(0x0013d3f0, 0); w16(0x0013e05a, coin() ? 0u : R());
        w32(OV(0x0015f5c4), R()); w32(OV(0x00173e04), R()); w32(OV(0x00173e08), R()); return Args{};
    }, [](A) { start_mode_select(); return uint64_t{0}; });
    g_extra_noop_hooks.clear();
    check("render_view_setup", 0x001f6e48, N, Ret::None, [] {
        w32(OV(0x0016c06c), 0);
        for (uint32_t r = 0; r < 3; ++r) wfrand(OV(0x00166fd0) + 0x10 * r, 4, -1, 1);
        wfrand(OV(0x0016cb00), 0x30, -2, 2); wfrand(OV(0x0016cbe0), 4, -1, 1); wfrand(OV(0x0016cc00), 1, 0.1f, 3); wfrand(OV(0x00166dc0), 4, -300, 300);
        return Args{};
    }, [](A) { render_view_setup(); return uint64_t{0}; });
    check("draw_callback_add", 0x001f9598, N, Ret::None, [] { w32(OV(0x0015f428), u(ri(0x3c, 0x42))); return Args{{R(), R()}, {}}; },
          [](A a) { draw_callback_add(a.i[0], a.i[1]); return uint64_t{0}; });
    check("mem_or128", 0x001ff150, N, Ret::None, [] { wrand(M, 0x40); wrand(M2, 0x40); return Args{{OUT, M, M2, u(ri(1, 4)) * 0x10}, {}}; },
          [](A a) { mem_or128(a.i[0], a.i[1], a.i[2], static_cast<int32_t>(a.i[3])); return uint64_t{0}; });
}

}  // namespace rac1::oracle
