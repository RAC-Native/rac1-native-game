// Oracle batch 192 (fix/gameplay, after issue #26): the particle types whose updates were never registered although ported level
// code spawns them (particles.cpp dispatches every particle's update through ovl_call_moby_fn: an unregistered one logged TODO_PORT
// and the particle never moved nor expired). Veldin's code, slots of the particle table 0x1b1e80 (particle_types_tr.cpp, regen group
// veldin_particles), each level's own copy as the original (ORACLE_LEVEL=N):
//  - type 0x0a (0x26a880): the smoke of L1:0x27f660 (Novalis' jets), L3:0x258a30 (Kerwan's class 845), L4:0x25d0e8 (Eudora);
//  - type 0x0e (0x26ba68): class 156's (L1:0x280970, L6:0x27a850, L8:0x275bd8, L9:0x28a428), following its moby (+0x24 0, +0x28);
//  - type 0x27 (0x26fc38): Batalia's L8:0x279f00 (from bat_2b8228_l8), following its moby (+0x24), the colour cycle +0x28;
//  - type 0x49 (0x275320): Batalia's class 1400 (L8:0x27f5f0), on the level's height grid (0x15fc98...).
// The records: made by the level's ported spawner (on its level), or alloc_particle(type) with their fields varied; then advanced by
// 0..40 frames of the port's update (the original's states later in the particle's life), the last frames (+0xa) too.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 192, N in 0 1 3 4 6 8 9.

#include "ovl_oracle.h"

#include <game/ovl/shared_class156.h>
#include <game/ovl/batalia_mobys.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/kerwan_classes_tr.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_types_tr.h>
#include <game/ovl/shared_class1400.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace rac1::oracle {

void batch_192() {
    if (g_only_batch != 192) return;
    g_batch = 192;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // the level after its first gameplay frame (the port's), as batch 140's
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            if (g_oracle_level == 14) w8(0x0013d3f0, 1);  // (Oltanis: the arrival scene seen, as batch 221: class 684 would start scene 0, its stream read from the disc)
            gameplay_frame();
            render_view_setup();
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    level();
    std::vector<uint32_t> ms;
    for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe) ms.push_back(m);
    auto moby = [ms]() -> uint32_t { return ms.empty() ? M : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))]; };
    const int L = g_oracle_level;
    auto alive = [](uint32_t q, uint32_t type) { return q != 0 && static_cast<int8_t>(ee_read<uint8_t>(q + 1)) >= 0 && ee_read<uint8_t>(q) == type; };
    // a point about the camera (w = 1)
    auto near = [](uint32_t p, float spread) {
        const Vec4 c = VV(OV(0x00166dc0));
        VV(p) = Vec4{c.x + rf(-spread, spread), c.y + rf(-spread, spread), c.z + rf(-spread / 3, spread / 3), 1.0f};
    };
    // the record a spawner made: the one of the type alive after it and not before (0: none)
    auto spawned = [](uint32_t type, const std::function<void()>& fn) -> uint32_t {
        auto list = [type] {
            std::vector<uint32_t> v;
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            const int hi = static_cast<int>(ee_read<uint32_t>(OV(0x00160174)));
            for (int i = 0; i <= hi && i < 4096; ++i) {
                const uint32_t q = pool + 0x40u * static_cast<uint32_t>(i);
                if (static_cast<int8_t>(ee_read<uint8_t>(q + 1)) >= 0 && ee_read<uint8_t>(q) == type) v.push_back(q);
            }
            return v;
        };
        const std::vector<uint32_t> before = list();
        fn();
        for (uint32_t q : list())
            if (std::find(before.begin(), before.end(), q) == before.end()) return q;
        return 0;
    };
    // the record: the level's spawner's (when this level has one ported) or a new one of the type, its fields varied; then `adv`
    // frames of the port's update, while it lives (a record freed on the way: a new one, not advanced)
    auto make = [&](uint32_t type, void (*upd)(uint32_t), const std::function<uint32_t()>& spawn,
                    const std::function<void(uint32_t)>& vary) -> uint32_t {
        uint32_t q = (spawn && ri(0, 2) != 0) ? spawn() : 0;
        if (q == 0) {
            q = alloc_particle(type);
            if (q == 0) return 0;
            near(q + 0x10, 20.0f);
            wfrand(q + 0x20, 3, -0.5f, 0.5f);
            w8(q + 2, R() & 0xff);
            w8(q + 8, R() & 0xff);
            wf(q + 0xc, rf(0.0f, 60000.0f));
            w16(q + 0xa, u(ri(0, 5) == 0 ? ri(-1, 2) : ri(3, 200)));
            vary(q);
        }
        const uint32_t s = q;
        for (int k = ri(0, 3) == 0 ? 0 : ri(1, 40); k > 0 && alive(q, type); --k) upd(q);
        if (!alive(q, type)) {  // (freed while advanced: a fresh one)
            q = alloc_particle(type);
            if (q == 0) return s;
            near(q + 0x10, 20.0f);
            w16(q + 0xa, u(ri(-1, 60)));
            vary(q);
        }
        if (ri(0, 4) == 0) w16(q + 0xa, u(ri(-1, 2)));  // (its last frames)
        wf(0x0015ed60, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);  // (the frame scale)
        seed_rand();
        return q;
    };
    const uint32_t P = TEST_AREA + 0x6000, V = P + 0x10;  // (the spawners' position / velocity arguments)

    // ---- type 0x0a: the smoke (position +0x10, velocity +0x20, its size +0x38 and alpha +0x3c, the counter +8)
    {
        auto pv = [P, V, near] { near(P, 20.0f); VV(V) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.1f, 0.4f), 0.0f}; };
        std::function<uint32_t()> sp;
        if (L == 1 || L == 3 || L == 4) sp = [pv, P, V, spawned] { return spawned(0x0a, [&] { pv(); smoke_particle_l1(P, V); }); };
        check("particle_0a_update", 0x0026a880, 200, Ret::None, [&] {
            level();
            const uint32_t q = make(0x0a, particle_0a_update, sp, [](uint32_t q) {
                wf(q + 0x38, ri(0, 3) == 0 ? rf(0.25f, 0.6f) : rf(0.0f, 0.25f));
                wf(q + 0x3c, ri(0, 4) == 0 ? rf(-0.05f, 0.05f) : rf(0.0f, 1.0f));
                w32(q + 4, R());
            });
            return Args{{q}, {}};
        }, [](A a) { particle_0a_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- type 0x0e: class 156's (the rate +0x20 added to +8; +0x24 0: at the moby +0x28's position, moving to the camera)
    {
        std::function<uint32_t()> sp;
        TrRet (*s156)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float) = nullptr;
        if (L == 1 || L == 6 || L == 8 || L == 9 || s156 != nullptr) sp = [s156, moby, L, spawned] {
            const uint32_t flag = coin() ? 1u : 0u, m = moby();
            return spawned(0x0e, [&] {
                if (L == 1 || L == 6 || L == 8 || L == 9) class156_trail(flag, m);
                else s156(flag, m, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            });
        };
        check("particle_0e_update", 0x0026ba68, 200, Ret::None, [&] {
            level();
            const uint32_t q = make(0x0e, particle_0e_update, sp, [moby](uint32_t q) {
                w8(q + 0x20, R() & 0xff);
                w32(q + 0x24, ri(0, 2) == 0 ? 1u : 0u);
                w32(q + 0x28, moby());
            });
            if (q != 0 && ri(0, 3) == 0) w32(q + 0x24, coin() ? 0u : 1u);
            return Args{{q}, {}};
        }, [](A a) { particle_0e_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- type 0x27: Batalia's (L8:0x279f00): following its moby (+0x24), the colour cycle +0x28 (0..30), the $gp globals
    // 0x160200..0x16020c it copies (size, colour, distance, rate)
    {
        std::function<uint32_t()> sp;
        if (L == 8) sp = [moby] { return bat_279f00_l8(moby(), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0).v0; };
        check("particle_27_update", 0x0026fc38, 200, Ret::None, [&] {
            level();
            if (coin()) {
                wf(OV(0x00160200), rf(0.0f, 4000.0f));
                w32(OV(0x00160204), R());
                wf(OV(0x00160208), rf(0.0f, 3.0f));
                w8(OV(0x0016020c), R() & 0xff);
            }
            const uint32_t q = make(0x27, particle_27_update, sp, [moby](uint32_t q) {
                w32(q + 0x24, moby());
                w32(q + 0x28, u(ri(0, 30)));
            });
            return Args{{q}, {}};
        }, [](A a) { particle_27_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- type 0x49: Batalia's class 1400 (L8:0x27f5f0, kinds 0 / 1 / other): x / y inside the height grid (0..512) or out of it,
    // the level +0x2c against z
    {
        std::function<uint32_t()> sp;
        if (L == 8) sp = [P, V, moby, spawned] {
            const uint32_t m = moby();
            VV(P) = VV(m + 0x10);
            VV(V) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-0.2f, 0.4f), 0.0f};
            const float z = VV(P).z + rf(-2.0f, 2.0f);
            const uint32_t kind = u(ri(0, 2));
            return spawned(0x49, [&] { class1400_mote(P, static_cast<int32_t>(kind), V, z); });
        };
        check("particle_49_update", 0x00275320, 200, Ret::None, [&] {
            level();
            const uint32_t q = make(0x49, particle_49_update, sp, [moby](uint32_t q) {
                if (ri(0, 3) != 0) {
                    const Vec4 h = VV(moby() + 0x10);
                    VV(q + 0x10) = Vec4{h.x + rf(-10, 10), h.y + rf(-10, 10), h.z + rf(-3, 3), 1.0f};
                } else {
                    VV(q + 0x10) = Vec4{rf(-20, 540), rf(-20, 540), rf(-50, 200), 1.0f};
                }
                wf(q + 0x2c, ee_read<float>(q + 0x18) + rf(-3.0f, 3.0f));
            });
            return Args{{q}, {}};
        }, [](A a) { particle_49_update(a.i[0]); return uint64_t{0}; });
    }

    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
