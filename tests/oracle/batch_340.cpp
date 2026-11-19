// Oracle batch 340 (fix/particles-unported): the 17 particle types whose updates were never registered: 0x03 0x07 0x09 0x11 0x14
// 0x18 0x1d 0x1e 0x21 0x24 0x25 0x26 0x2a 0x36 0x3f 0x4b 0x4c. particles.cpp dispatches every particle's update through
// ovl_call_moby_fn (the table 0x1b1e80, which every level's setup fills with all 81 types); an unregistered update logged
// TODO_PORT and its particle neither moved nor expired. No code of any level allocates one of these types (the alloc_particle
// calls of the 19 overlays, the record type stores: see the header of particle_types_tr.h), except 0x2a, whose spawner 0x270758
// is called only by 0x2a's own update 0x270948: the ports keep a particle that should come from nowhere from hanging.
// The port is Veldin's code transliterated (particle_types_tr.cpp, regen group veldin_particles), registered in
// veldin_extras.cpp; the original is each level's own copy (ORACLE_LEVEL=N: every level has the 17 functions, the overlay map's
// function or data lines place them). The records: alloc_particle(type) with their fields varied (floats, small integers,
// colours, the timer +0xa, the position about the camera; the pointer fields the code reads through: 0x03's +0x30 (a byte it
// clears), 0x14's +0x20 (three rows) and +0x38 (the hero: its chain 0's joint), 0x18's +0x20 (a particle followed), 0x1d's +0x20 (a
// moby: its state; the hero's joint +0x2e), 0x1e's +0x30 / +0x34 (a chain of 0x1e records), 0x24's +0x20 (a word it clears), 0x36's +0x20
// (a path: 0x262360)), or for 0x2a its spawner's (0x270758, the port's); then advanced by 0..40 frames of the port's update (the
// original's states later in the particle's life), the last frames (+0xa) too; the frame scale varied. A state that sends the port
// outside RAM (a joint the moby's model does not have) is dealt again (as batches 119 / 142). 0x270758 itself is checked too. No
// RAM dumps: no play reaches these types (nothing spawns them).
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 340, N in 0 .. 18.

#include "ovl_oracle.h"

#include <game/ovl/shared_particles.h>

#include <game/ovl/empty_class_updates.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_28_tr.h>
#include <game/ovl/particle_44_tr.h>
#include <game/ovl/particle_types_tr.h>
#include <game/ovl/shared_classes_tr.h>
#include <game/ovl/shared_particle_kinds.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <optional>
#include <vector>

namespace rac1::oracle {

namespace {
std::vector<uint32_t> b340_live() {
    std::vector<uint32_t> out;
    const uint32_t base = ee_read<uint32_t>(OV(0x0016016c));
    const int32_t hi = ee_read<int32_t>(OV(0x00160174));
    for (int32_t i = 0; i <= hi && i < 4096; ++i) {
        const uint32_t p = base + u(i) * 64;
        if (ee_read<int8_t>(p + 1) >= 0) out.push_back(p);
    }
    return out;
}
}  // namespace

void batch_340() {
    if (g_only_batch != 340) return;
    g_batch = 340;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    // segment_collide's hit point (the global at Veldin 0x173e90, 3 words; 0x2a's update 0x1efc70, 0x1e's 0x1f0d60): written by the
    // callee from segments built with xyz-only vector calls on the stack, whose stale w lanes the port's EE stack and the interpreter's
    // do not share; no particle code reads it (as batches 117 / 119 / 220 / 221)
    for (uint32_t k = 0; k < 12; k += 4) g_ignored_words.push_back(OV(0x00173e90) + k);
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // the level after its first gameplay frame (the port's), as batches 192 / 370
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            w32(0x0013f2e8, 0xffffffffu);  // (the sound listener's cached points refreshed on the first frame, as batch 270 does)
            if (g_oracle_level == 14) w8(0x0013d3f0, 1);  // (Oltanis: the arrival scene seen, as batch 221's: else its stream is read from the disc)
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
    auto near = [](uint32_t p, float spread) {  // a point about the camera (w = 1)
        const Vec4 c = VV(OV(0x00166dc0));
        VV(p) = Vec4{c.x + rf(-spread, spread), c.y + rf(-spread, spread), c.z + rf(-spread / 3, spread / 3), 1.0f};
    };
    auto alive = [](uint32_t q, uint32_t type) { return q != 0 && ee_read<int8_t>(q + 1) >= 0 && ee_read<uint8_t>(q) == type; };
    // the common fields: colour +4, bytes +2 / +3 / +8 / +9, size +0xc, timer +0xa, position +0x10, then +0x20 .. +0x3c word by
    // word a float (small or large), a small integer or zero
    auto fields = [near](uint32_t q) {
        w32(q + 4, R());
        w8(q + 2, R() & 0xff);
        if (coin()) w8(q + 3, R() & 0xff);
        w8(q + 8, R() & 0xff);
        if (coin()) w8(q + 9, R() & 0xff);
        wf(q + 0xc, ri(0, 3) == 0 ? rf(0.0f, 60000.0f) : rf(-2.0f, 4.0f));
        w16(q + 0xa, u(ri(0, 5) == 0 ? ri(-1, 2) : ri(3, 200)));
        near(q + 0x10, 20.0f);
        for (uint32_t o = 0x20; o < 0x40; o += 4) {
            switch (ri(0, 4)) {
                case 0: wf(q + o, rf(-1.0f, 1.0f)); break;
                case 1: wf(q + o, rf(-30.0f, 30.0f)); break;
                case 2: wf(q + o, rf(0.0f, 3.0f)); break;
                case 3: w32(q + o, u(ri(0, 300))); break;  // (not below 0: as a float, a NaN pattern the game never holds)
                default: w32(q + o, 0); break;
            }
        }
    };
    auto block = [](uint32_t b) {  // three rows of floats
        wfrand(b, 12, -2.0f, 2.0f);
    };

    struct Type {
        uint8_t type;
        uint32_t orig;
        void (*upd)(uint32_t);
        const char* name;
    };
    const Type types[] = {
        {0x03, 0x002694f8, particle_03_update, "particle_03_update"}, {0x07, 0x0026a418, particle_07_update, "particle_07_update"},
        {0x09, 0x0026a6f0, particle_09_update, "particle_09_update"}, {0x11, 0x0026c3c8, empty_update, "particle_11_update"},
        {0x14, 0x0026c860, particle_14_update, "particle_14_update"}, {0x18, 0x0026d470, particle_18_update, "particle_18_update"},
        {0x1d, 0x0026e3a8, particle_1d_update, "particle_1d_update"}, {0x1e, 0x0026e670, particle_1e_update, "particle_1e_update"},
        {0x21, 0x0026f048, timed_particle_update, "particle_21_update"}, {0x24, 0x0026f898, particle_24_update, "particle_24_update"},
        {0x25, 0x0026fa00, particle_25_update, "particle_25_update"}, {0x26, 0x0026fb00, particle_26_update, "particle_26_update"},
        {0x2a, 0x00270948, particle_2a_update, "particle_2a_update"}, {0x36, 0x00272438, particle_36_update, "particle_36_update"},
        {0x3f, 0x002738b0, timed_particle_update, "particle_3f_update"}, {0x4b, 0x002756f0, particle_4b_update, "particle_4b_update"},
        {0x4c, 0x00275810, particle_4c_update, "particle_4c_update"},
    };
    const uint32_t P = TEST_AREA + 0x6000;  // (0x2a's spawner's position and velocity arguments)

    // the type's own pointer fields (read through by its update)
    auto pointers = [&](uint32_t type, uint32_t q) {
        switch (type) {
            case 0x03:  // +0x30: a byte it clears when it is done (while +0x34 is set)
                w8(OUT + 0x10, R() & 0xff);
                w32(q + 0x30, OUT + 0x10);
                break;
            case 0x14:  // +0x20: three rows (lq 0 / 0x10 / 0x20); +0x38: a moby (its chain 0's last joint)
                if (coin()) { block(T3); w32(q + 0x20, T3); } else { w32(q + 0x20, moby()); }
                w32(q + 0x38, ee_read<uint32_t>(OV(0x0015ffd8)));  // (the hero: a model with chains; another may have none)
                break;
            case 0x18: {  // +0x20: the particle it follows (its flag +1, position, +7 / +8 / +0xc); 0 or a dead one frees it
                const int k = ri(0, 5);
                if (k == 0) {
                    w32(q + 0x20, 0);
                } else {
                    const uint32_t o = alloc_particle(u(ri(0, 3) == 0 ? 0x18 : 0x0b));
                    if (o == 0) { w32(q + 0x20, 0); break; }
                    near(o + 0x10, 20.0f);
                    w32(o + 4, R());
                    w8(o + 7, R() & 0xff);
                    w8(o + 8, R() & 0xff);
                    wf(o + 0xc, rf(0.0f, 4.0f));
                    if (k == 1) w8(o + 1, 0xff);  // (freed)
                    w32(q + 0x20, o);
                }
                break;
            }
            case 0x1d: {  // +0x20: a moby (its state +0x20: below 0 frees the particle)
                // (the joint followed +0x2e: none, or one of the hero's chains 0..12 as batch 142's 0x3d: a chain another model does not
                // have reads past its tables, not a game state)
                const int k = ri(0, 5);
                const bool hero = coin();
                const uint32_t m = k == 0 ? 0u : hero ? ee_read<uint32_t>(OV(0x0015ffd8)) : moby();
                if (m != 0 && k == 1) w8(m + 0x20, coin() ? 0xfeu : 0x80u);
                w32(q + 0x20, m);
                w16(q + 0x2e, hero && coin() ? u(ri(0, 12)) : 0xffffu);
                break;
            }
            case 0x36: {  // +0x20: a path (0x262360: a count, then the points from +0x10)
                const int n = ri(1, 8);
                w32(T4, u(n));
                for (int i = 0; i < n; ++i) near(T4 + 0x10 + u(i) * 0x10, 30.0f);
                w32(q + 0x20, T4);
                break;
            }
            case 0x1e: {  // +0x30 / +0x34: the previous and the next record of a chain of 0x1e records (null ends); +0x3c / +0x3e flags
                const int n = ri(0, 4);
                uint32_t prev = 0, cur = q;
                std::vector<uint32_t> chain{q};
                for (int i = 0; i < n; ++i) {
                    const uint32_t o = alloc_particle(0x1e);
                    if (o == 0) break;
                    chain.push_back(o);
                }
                if (ri(0, 2) == 0) std::shuffle(chain.begin() + 1, chain.end(), rng);
                if (ri(0, 2) == 0 && chain.size() > 1) std::swap(chain[0], chain[static_cast<size_t>(ri(1, static_cast<int>(chain.size()) - 1))]);
                for (size_t i = 0; i < chain.size(); ++i) {
                    cur = chain[i];
                    if (cur != q) {
                        near(cur + 0x10, 20.0f);
                        w32(cur + 4, R());
                        w16(cur + 0xa, u(ri(3, 200)));
                        wfrand(cur + 0x20, 4, -10.0f, 10.0f);
                        wf(cur + 0x38, rf(0.0f, 3.0f));
                    }
                    w32(cur + 0x30, prev);
                    w32(cur + 0x34, i + 1 < chain.size() ? chain[i + 1] : 0u);
                    w16(cur + 0x3c, coin() ? 0u : u(ri(0, 3)));
                    w16(cur + 0x3e, coin() ? 0u : u(ri(0, 3)));
                    prev = cur;
                }
                // (the chain walk stops at a record whose +0x38 doubleword has its high bits set)
                if (ri(0, 3) == 0) w32(chain[static_cast<size_t>(ri(0, static_cast<int>(chain.size()) - 1))] + 0x3c, R());
                break;
            }
            case 0x24:  // +0x20: a word it clears when the particle is done
                w32(OUT, R());
                w32(q + 0x20, ri(0, 4) == 0 ? 0u : OUT);
                break;
            default: break;
        }
    };

    for (const Type& t : types) {
        // a random state that sends the port through a pointer outside RAM (a joint the moby's model does not have) is not a game
        // state: dealt again (as batches 119 / 142)
        auto setup = [&]() -> Args {
            level();
            if (coin()) wf(OV(0x001601f4), rf(0.0f, 1.5f));
            if (coin()) wf(OV(0x001601f8), rf(0.0f, 1.5f));
            uint32_t q = 0;
            if (t.type == 0x2a && ri(0, 2) != 0) {  // the spawner's record (the port's 0x270758)
                near(P, 20.0f);
                wfrand(P + 0x10, 3, -0.5f, 0.5f);
                wf(P + 0x1c, 0.0f);
                const std::vector<uint32_t> before = b340_live();
                const uint32_t spawned = particle_2a_spawn(P, P + 0x10, static_cast<uint8_t>(u(ri(0, 8)) & 0xff), coin() ? 0u : u(ri(0, 0x88)));
                if (spawned != 0 && std::find(before.begin(), before.end(), spawned) == before.end()) q = spawned;
            }
            if (q == 0) {
                q = alloc_particle(t.type);
                if (q == 0) return Args{{0}, {}};
                fields(q);
                pointers(t.type, q);
            }
            const uint32_t s = q;
            for (int k = ri(0, 3) == 0 ? 0 : ri(1, 40); k > 0 && alive(q, t.type); --k) t.upd(q);
            if (!alive(q, t.type)) {  // (freed while advanced: a fresh one)
                q = alloc_particle(t.type);
                if (q == 0) return Args{{s}, {}};
                fields(q);
                pointers(t.type, q);
                w16(q + 0xa, u(ri(-1, 60)));
            }
            if (ri(0, 4) == 0) w16(q + 0xa, u(ri(-1, 2)));  // (its last frames)
            if (ri(0, 6) == 0) pointers(t.type, q);           // (what it points at changed meanwhile)
            wf(0x0015ed60, ri(0, 3) == 0 ? rf(0.5f, 3.0f) : 1.0f);  // (the frame scale)
            seed_rand();
            return Args{{q}, {}};
        };
        auto gen = [&]() -> Args {
            static std::vector<uint8_t> saved;
            Args a{{0}, {}};
            for (int attempt = 0; attempt < 40; ++attempt) {
                const uint32_t stack_top = ee_stack_ptr();
                bool ok = true;
                try {
                    a = setup();
                    saved.assign(ee_get_ram_base(), ee_get_ram_base() + 0x02000000);
                    if (a.i[0] != 0) t.upd(a.i[0]);
                } catch (const std::exception&) { ok = false; }
                ee_stack_ptr() = stack_top;
                if (ok) {
                    std::memcpy(ee_get_ram_base(), saved.data(), saved.size());
                    break;
                }
            }
            return a;
        };
        // (stage 5, task C2b) the particle updates named here reach sphere_collide (0x1e's 0x1f0d60 ...), whose chunk buffers,
        // vertices and cell lists the port keeps on the stack: A2's collision scratch ranges are left out of their checks, and
        // only of them (see batch 43's sphere_collide)
        // (0x03's spheres are big enough for the list of cells to pass 0x70003c00, measured: the wide range for it, which also
        // leaves out whatever else its cases write at 0x70003c00 .. 0x70003ff0)
        static const char* const kC2bSpr[] = {"particle_03_update", "particle_1e_update", "particle_36_update",
                                               "particle_2a_update"};  // (0x2a: its update reaches segment_collide, task C2c)
        std::optional<CollisionScratchIgnored> spr_ignored;
        for (const char* n : kC2bSpr)
            if (std::strcmp(n, t.name) == 0) spr_ignored.emplace(std::strcmp(t.name, "particle_03_update") == 0);
        check(t.name, t.orig, 150, Ret::None, gen, [&t](A a) { if (a.i[0] != 0) t.upd(a.i[0]); return uint64_t{0}; });
    }

    // ---- 0x2a's spawner (called by 0x2a's update only): the record it allocates and fills
    check("ovl_270758_tr", 0x00270758, 100, Ret::Int32, [&] {
        level();
        near(P, 20.0f);
        wfrand(P + 0x10, 3, -0.5f, 0.5f);
        wf(P + 0x1c, 0.0f);
        if (ri(0, 6) == 0) wf(0x0015ed60, rf(0.5f, 3.0f));
        seed_rand();
        return Args{{P, P + 0x10, u(ri(0, 8)), coin() ? 0u : R()}, {}};
    }, [](A a) { return uint64_t{particle_2a_spawn(a.i[0], a.i[1], static_cast<uint8_t>(a.i[2]), a.i[3])}; });

    // ---- P0.1: direct L0 checks of local_offset_coord (0x25c230), types 0x1f / 0x28 / 0x44, and 0x03's outer bands
    if (g_oracle_level == 0) {
        check("p01_local_offset_coord", 0x0025c230, 100, Ret::Float, [&] {
            level();
            wfrand(P, 4, -50.0f, 50.0f);
            wfrand(P + 0x10, 16, -2.0f, 2.0f);
            wfrand(P + 0x50, 4, -50.0f, 50.0f);
            return Args{{P, P + 0x10, P + 0x50, u(ri(0, 3))}, {}};
        }, [](A a) {
            return static_cast<uint64_t>(
                fbits(local_offset_coord(ee_ref<Vec4>(a.i[0]), ee_ref<Mat4>(a.i[1]), ee_ref<Vec4>(a.i[2]), static_cast<int32_t>(a.i[3]))));
        });

        check("p01_particle_1f", 0x0026eb88, 150, Ret::None, [&] {
            level();
            const uint32_t q = alloc_particle(0x1f);
            if (q == 0) return Args{{0u}, {}};
            const int mk = ri(0, 7);
            const uint32_t m = mk == 0 ? 0u : moby();
            if (m != 0 && mk == 1) w8(m + 0x20, coin() ? 0xfeu : 0xfdu);
            w32(q + 0x30, m);
            w16(q + 0xa, u(ri(0, 5) == 0 ? ri(0, 2) : ri(3, 60)));
            const Vec4 base = m != 0 ? VV(m + 0x10) : VV(OV(0x00166dc0));
            const float spread = ri(0, 3) == 0 ? 0.3f : 8.0f;
            VV(q + 0x10) = Vec4{base.x + rf(-spread, spread), base.y + rf(-spread, spread), base.z + rf(-spread, spread), 1.0f};
            if (ri(0, 5) == 0) {
                VV(q + 0x20) = Vec4{ee_read<float>(q + 0x10) + rf(-0.03f, 0.03f), ee_read<float>(q + 0x14), ee_read<float>(q + 0x18), 1.0f};
            } else {
                VV(q + 0x20) = Vec4{base.x + rf(-spread, spread), base.y + rf(-spread, spread), base.z + rf(-spread, spread), 1.0f};
            }
            wf(OV(0x001601f4), rf(0.2f, 1.4f));
            wf(OV(0x001601f8), rf(0.2f, 1.4f));
            if (ri(0, 3) == 0) wf(0x0015ed60, 1.2f);
            return Args{{q}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_1f_update(a.i[0]); return uint64_t{0}; });

        {
            const CollisionScratchIgnored spr_ignored;
            check("p01_particle_28", 0x0026ff10, 240, Ret::None, [&] {
                level();
                const uint32_t hero = ee_read<uint32_t>(OV(0x0015ffd8));
                const float hx = ee_read<float>(0x0013f3d0), hy = ee_read<float>(0x0013f3d4), hz = ee_read<float>(0x0013f3d8);
                VV(P) = Vec4{hx + rf(-1.5f, 1.5f), hy + rf(-1.5f, 1.5f), hz + rf(-0.2f, 1.2f), 1.0f};
                if (coin()) {
                    VV(P + 0x10) = Vec4{rf(-0.3f, 0.3f), rf(-0.3f, 0.3f), rf(-2.5f, -0.3f), 0.0f};
                } else {
                    VV(P + 0x10) = Vec4{rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), rf(-0.5f, 0.5f), 0.0f};
                }
                seed_rand();
                const uint32_t q = particle_28_spawn(P, P + 0x10, u(ri(0, 6)), hero);
                if (q == 0) return Args{{0u}, {}};
                const int tk = ri(0, 3);
                w16(q + 0xa, u(tk == 0 ? 0 : tk == 1 ? 1 : ri(2, 30)));
                const uint32_t f18 = ri(0, 2) == 0 ? (0x8u | u(coin())) : (coin() ? 0x40u : u(coin()));
                w8(q + 0x38, f18);
                if ((f18 & 8u) != 0 && ri(0, 5) == 0) w32(q + 4, ee_read<uint32_t>(q + 4) & 0xffffffu);
                wf(q + 0xc, ri(0, 4) == 0 ? rf(100.0f, 3000.0f) : rf(5000.0f, 260000.0f));
                w16(q + 0x34, u(ri(0, 5) == 0 ? ri(-600, -400) : ri(0, 40)));
                w16(q + 0x36, u(ri(1, 12)));
                wf(q + 0x30, ri(0, 3) == 0 ? rf(-0.08f, 0.02f) : ri(0, 2) == 0 ? rf(0.98f, 1.08f) : rf(0.1f, 0.9f));
                if (ri(0, 3) == 0) {
                    wf(0x0015ed60, 1.2f);
                    wf(0x0015ed64, 1.44f);
                    wf(0x0015ed68, 1.2f);
                }
                seed_rand();
                return Args{{q}, {}};
            }, [](A a) { if (a.i[0] != 0) particle_28_update(a.i[0]); return uint64_t{0}; });
        }

        check("p01_particle_44", 0x00274818, 150, Ret::None, [&] {
            level();
            const uint32_t q = alloc_particle(0x44);
            if (q == 0) return Args{{0u}, {}};
            const uint32_t hero = ee_read<uint32_t>(OV(0x0015ffd8));
            const int mk = ri(0, 6);
            w32(q + 0x30, mk == 0 ? 0u : hero);
            w16(q + 0x36, mk == 1 ? u(ee_read<uint16_t>(hero + 0xa6) + 1u) : ee_read<uint16_t>(hero + 0xa6));
            if (mk == 2) w8(hero + 0x20, coin() ? 0xfeu : 0xfdu);
            w8(q + 0x34, u(ri(0, 5)));
            w8(q + 0x35, u(ri(0, 80)));
            wf(q + 0x38, rf(0.1f, 5.0f));
            wf(q + 0x3c, rf(0.5f, 8.0f));
            wf(q + 0x2c, rf(-2.0f, 2.0f));
            seed_rand();
            return Args{{q}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_44_update(a.i[0]); return uint64_t{0}; });

        {
            const CollisionScratchIgnored spr_ignored(true);
            check("p01_particle_03_edges", 0x002694f8, 180, Ret::None, [&] {
                level();
                const uint32_t q = alloc_particle(0x03);
                if (q == 0) return Args{{0u}, {}};
                fields(q);
                pointers(0x03, q);
                const Vec4 c = VV(OV(0x00166dc0));
                const float r = ri(0, 4) == 0 ? rf(3.0f, 15.5f) : ri(0, 2) == 0 ? rf(80.5f, 102.0f) : rf(20.0f, 70.0f);
                const float a = rf(-3.14f, 3.14f);
                VV(q + 0x10) = Vec4{c.x + std::cos(a) * r, c.y + std::sin(a) * r, c.z + rf(-2.0f, 2.0f), 1.0f};
                if (coin()) {
                    const Vec4 h = VV(0x0013f3d0);
                    if (ri(0, 3) == 0) VV(q + 0x10) = Vec4{h.x, h.y, h.z + 0.5f, 1.0f};
                }
                w32(q + 0x3c, u(ri(-1, 5)));
                w32(q + 0x38, u(ri(0, 7)));
                w32(q + 0x34, u(coin()));
                w16(q + 0xa, u(ri(0, 3) == 0 ? ri(0, 2) : ri(3, 180)));
                wf(OV(0x001cb7c4), rf(-3.14f, 3.14f));
                wf(OV(0x001cb7d4), rf(0.5f, 4.0f));
                w32(OV(0x0015f5cc), u(ri(0, 7)));
                if (ri(0, 3) == 0) wf(0x0015ed60, 1.2f);
                seed_rand();
                return Args{{q}, {}};
            }, [](A a) { if (a.i[0] != 0) particle_03_update(a.i[0]); return uint64_t{0}; });
        }

        check("p01_particle_09_edges", 0x0026a6f0, 60, Ret::None, [&] {
            level();
            const uint32_t q = alloc_particle(0x09);
            if (q == 0) return Args{{0u}, {}};
            fields(q);
            w8(q + 3, coin() ? 0x48u : 0x44u);
            w32(q + 0x20, u(ri(1, 60)));
            w32(q + 0x30, u(ri(10, 200)));
            return Args{{q}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_09_update(a.i[0]); return uint64_t{0}; });

        check("p01_particle_1d_edges", 0x0026e3a8, 100, Ret::None, [&] {
            level();
            const uint32_t q = alloc_particle(0x1d);
            if (q == 0) return Args{{0u}, {}};
            const uint32_t hero = ee_read<uint32_t>(OV(0x0015ffd8));
            w32(q + 0x20, hero);
            w16(q + 0xa, u(ri(3, 60)));
            w16(q + 0x2c, u(ri(0, 15)));
            w16(q + 0x2e, coin() ? u(ri(0, 6)) : 0xffffu);
            wf(q + 0xc, coin() ? rf(160000.0f, 240000.0f) : rf(150000.0f, 165000.0f));
            wf(q + 0x24, rf(-0.05f, 0.45f));
            wf(q + 0x28, rf(0.0f, 1.0f));
            wf(q + 0x30, rf(-0.1f, 0.1f));
            wf(q + 0x34, rf(0.0f, 1.0f));
            return Args{{q}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_1d_update(a.i[0]); return uint64_t{0}; });

        check("p01_particle_24_edges", 0x0026f898, 80, Ret::None, [&] {
            level();
            const uint32_t q = alloc_particle(0x24);
            if (q == 0) return Args{{0u}, {}};
            fields(q);
            w32(OUT, ri(0, 4) == 0 ? 0u : 1u);
            w32(q + 0x20, OUT);
            const Vec4 c = VV(OV(0x00166dc0));
            const float d = coin() ? rf(-0.4f, 0.4f) : rf(-3.0f, 3.0f);
            VV(q + 0x30) = Vec4{c.x + d, c.y + d, c.z + d, 1.0f};
            if (coin()) {
                w16(q + 0xa, 1);
                wf(q + 0x24, -1.57f);
                wf(q + 0x3c, 0.0f);
                wf(q + 0x28, -0.99f);
            }
            return Args{{q}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_24_update(a.i[0]); return uint64_t{0}; });

        check("p01_particle_25_edges", 0x0026fa00, 80, Ret::None, [&] {
            level();
            const uint32_t q = alloc_particle(0x25);
            if (q == 0) return Args{{0u}, {}};
            fields(q);
            w16(q + 0xa, u(ri(3, 60)));
            w32(q + 0x34, u(ri(10, 40)));
            const Vec4 c = VV(OV(0x00166dc0));
            const float d = coin() ? rf(-1.5f, 1.5f) : rf(-10.0f, 10.0f);
            VV(q + 0x20) = Vec4{c.x + d, c.y + d, c.z + d, 1.0f};
            return Args{{q}, {}};
        }, [](A a) { if (a.i[0] != 0) particle_25_update(a.i[0]); return uint64_t{0}; });

        // ---- P0.2: direct L0 checks of particle_alpha_dec (0x259430), class 167 (0x2b3c50 / 0x2b4198 / 0x2b3628),
        // vendor_update_tr (0x2a57a8), and infobot (0x2d5988 / 0x2d6cd0 / 0x2d5de8)
        constexpr uint32_t PV = P + 0x200u;
        check("p02_particle_alpha_dec", 0x00259430, 80, Ret::Int32, [&] {
            level();
            const uint32_t a_byte = ri(0, 3) == 0 ? u(ri(0x80, 0xff)) : ri(0, 2) == 0 ? 0u : u(ri(1, 0x7f));
            w32(OUT, (a_byte << 24) | (R() & 0x00ffffffu));
            const int32_t step = ri(0, 3) == 0 ? ri(-4, 0) : ri(1, 48);
            return Args{{OUT, u(step)}, {}};
        }, [](A a) { return uint64_t{static_cast<uint32_t>(particle_alpha_dec(a.i[0], static_cast<int32_t>(a.i[1])))}; });

        check("p02_class167_emit", 0x002b3c50, 80, Ret::None, [&] {
            level();
            std::memset(ee_ram_ptr(M), 0, 0x100);
            std::memset(ee_ram_ptr(PV), 0, 0x80);
            w32(M + 0x78, PV);
            near(M + 0x10, 10.0f);
            mat4_identity(MM(M + 0xc0));
            w32(PV + 0xc, u(coin()));
            near(PV + 0x10, 10.0f);
            static const uint32_t hs[] = {0, 8, 0x81, 0xb};
            w32(g::HERO_STATE, hs[ri(0, 3)]);
            w32(OV(0x0015f5cc), u(ri(0, 5)));
            if (ri(0, 2) == 0) {
                wf(0x0015ed60, 1.2f);
                wf(0x0015ed6c, 0.02f);
                wf(0x0015ed70, 0.0004f);
            }
            seed_rand();
            return Args{{M}, {}};
        }, [](A a) { class167_emit_particles(a.i[0]); return uint64_t{0}; });

        check("p02_class167_update", 0x002b4198, 120, Ret::None, [&] {
            level();
            const uint32_t m = moby_spawn(167);
            if (m == 0) return Args{{0u}, {}};
            std::memset(ee_ram_ptr(PV), 0, 0x80);
            w32(m + 0x78, PV);
            near(m + 0x10, 10.0f);
            mat4_identity(MM(m + 0xc0));
            w8(m + 0x20, u(ri(0, 5)));
            w32(PV + 0, u(ri(0, 3) == 0 ? 1 : ri(2, 6)));
            w32(PV + 0xc, u(coin()));
            near(PV + 0x10, 10.0f);
            for (uint32_t o = 0x20; o < 0x40; o += 4) wf(PV + o, rf(0.01f, 0.25f));
            w32(0x001404f8, ri(0, 4) == 0 ? 0u : 3u);
            static const uint32_t hs[] = {0xb, 0xc, 0xd, 0xe, 0x1c, 0x10, 0x22, 8, 0x81, 0};
            w32(g::HERO_STATE, hs[ri(0, 9)]);
            w16(g::PLAYER + 0x41e, u(coin()));
            wf(g::PLAYER + 0xaa8, rf(2.0f, 20.0f));
            w32(g::PLAYER + 0x198, u(ri(0, 60)));
            w16(g::PLAYER + 0x30e, u(coin()));
            const uint32_t hm = ee_read<uint32_t>(g::HERO_MOBY);
            if (hm != 0) {
                const uint8_t a_id = static_cast<uint8_t>(ri(0, 3));
                w8(hm + 0x52, a_id);
                w8(hm + 0x53, coin() ? a_id : static_cast<uint8_t>(a_id + 1));
            }
            w32(g::PLAYER + 0x11a8, ri(0, 3) == 0 ? 2u : 3u);
            if (ri(0, 2) == 0) {
                wf(0x0015ed60, 1.2f);
                wf(0x0015ed6c, 0.02f);
                wf(0x0015ed70, 0.0004f);
            }
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) class167_update_tr(a.i[0]); return uint64_t{0}; });

        check("p02_class167_draw", 0x002b3628, 60, Ret::None, [&] {
            level();
            const uint32_t pbuf = TEST_AREA + 0x20000u;
            const uint32_t pend = pbuf + 0x8000u;
            std::memset(ee_ram_ptr(pbuf), 0, 0x8000);
            w32(OV(0x001611c0), pbuf);
            w32(OV(0x0015f5f8), pend);
            w32(OV(0x0015f5e0), coin() ? 7u : 0u);
            std::memset(ee_ram_ptr(M), 0, 0x100);
            std::memset(ee_ram_ptr(PV), 0, 0x80);
            w32(M + 0x78, PV);
            const uint32_t hm = ee_read<uint32_t>(g::HERO_MOBY);
            const uint32_t pack = TEST_AREA + 0x10000u;
            if (hm != 0) {
                std::memcpy(ee_ram_ptr(pack), ee_ram_ptr(hm), 0x100);
                w16(pack + 0xa6, ri(0, 7) == 0 ? 0x100u : 0x260u);
            }
            w32(PV + 4, u(coin()));
            w32(PV + 8, ri(0, 7) == 0 ? 0u : pack);
            wf(PV + 0x20, rf(0.01f, 0.2f));
            wf(PV + 0x2c, rf(0.01f, 0.2f));
            if (coin()) {
                for (uint32_t i = 0; i < 8; ++i) wf(OV(0x001da0b4) + i * 8, -6.99f);
            }
            seed_rand();
            return Args{{M}, {}};
        }, [](A a) { class167_draw_tr(a.i[0]); return uint64_t{0}; });

        check("p02_vendor_update", 0x002a57a8, 120, Ret::None, [&] {
            level();
            const uint32_t m = moby_spawn(11);
            if (m == 0) return Args{{0u}, {}};
            std::memset(ee_ram_ptr(PV), 0, 0xa0);
            w32(m + 0x78, PV);
            const uint32_t st = u(ri(0, 3));
            w8(m + 0x20, st);
            const Vec4 hp = VV(g::PLAYER + 0x80);
            const float dxy = ri(0, 3) == 0 ? rf(0.5f, 3.5f) : ri(0, 2) == 0 ? rf(5.0f, 15.5f) : rf(16.5f, 22.0f);
            const float dz = ri(0, 3) == 0 ? rf(-1.5f, 1.5f) : ri(0, 2) == 0 ? rf(-7.5f, 7.5f) : rf(8.5f, 12.0f);
            const float ang = rf(-3.14f, 3.14f);
            VV(m + 0x10) = Vec4{hp.x + std::cos(ang) * dxy, hp.y + std::sin(ang) * dxy, hp.z + dz, 1.0f};
            wf(g::PLAYER + 0x98, coin() ? std::atan2(std::sin(ang), std::cos(ang)) : rf(-3.14f, 3.14f));
            if (st != 0) {
                const uint32_t holo = moby_spawn(0x477);
                w32(PV + 0xc, (st == 3 && ri(0, 3) == 0) ? 0u : holo);
            }
            wf(PV + 0x90, static_cast<float>(ri(0, 10)) * 0.1f);
            wf(OV(0x001613a4), coin() ? 3.12f : rf(-3.1f, 3.1f));
            w32(OV(0x0015f5cc), coin() ? 0u : u(ri(1, 7)));
            w8(m + 0x52, u(coin()));
            w8(m + 0x53, u(ri(0, 2)));
            w32(g::HERO_SUBSTATE, u(ri(0, 2)));
            static const uint32_t hst[] = {0, 3, 0x1d, 0x32};
            w32(g::HERO_STATE, hst[ri(0, 3)]);
            w8(g::PLAYER + 0x20a4, ri(0, 3) == 0 ? 1u : 0u);
            w32(0x00151720, 0);
            w32(0x001516d0 + 0x50, 0);
            w32(0x001516d0 + 0x1c, 0);
            w32(0x0013cae4, coin() ? 0x10u : 0u);
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) vendor_update_tr(a.i[0]); return uint64_t{0}; });

        check("p02_infobot_follow_path", 0x002d5988, 60, Ret::None, [&] {
            level();
            std::memset(ee_ram_ptr(M), 0, 0x100);
            std::memset(ee_ram_ptr(PV), 0, 0xe0);
            w32(M + 0x78, PV);
            const uint32_t path = ee_read<uint32_t>(OV(0x001b04b0));
            if (path == 0 || ee_read<int32_t>(path) < 3) return Args{{0u, 0u, 0u}, {}};
            const int32_t npts = ee_read<int32_t>(path);
            const int32_t pick = coin() ? npts - 1 : ri(0, npts - 1);
            VV(M + 0x10) = VV(path + 0x10u + u(pick) * 0x10u);
            wf(PV + 0x60, rf(0.0f, 0.5f));
            if (ri(0, 2) == 0) {
                wf(0x0015ed60, 1.2f);
                wf(0x0015ed6c, 0.02f);
                wf(0x0015ed70, 0.0004f);
            }
            return Args{{M, path, 3u}, {}};
        }, [](A a) { if (a.i[0] != 0) infobot_follow_path(a.i[0], a.i[1], static_cast<uint8_t>(a.i[2])); return uint64_t{0}; });

        check("p02_infobot_glow", 0x002d6cd0, 60, Ret::None, [&] {
            level();
            std::memset(ee_ram_ptr(M), 0, 0x100);
            std::memset(ee_ram_ptr(PV), 0, 0xe0);
            w32(M + 0x78, PV);
            near(M + 0x10, 10.0f);
            w16(M + 0x34, ri(0, 5) == 0 ? 1u : 0u);
            for (uint32_t i = 0; i < 4; ++i) {
                wf(PV + 0x10 + i * 4, ri(0, 3) == 0 ? 254.0f : ri(0, 2) == 0 ? 0.5f : rf(10.0f, 240.0f));
                wf(PV + 0x20 + i * 4, rf(-3.0f, 3.0f));
                w32(PV + 0x30 + i * 4, u(ri(0, 2) == 0 ? 1 : ri(2, 200)));
                wf(PV + 0x40 + i * 4, rf(1000.0f, 20000.0f));
            }
            w32(OV(0x0015f5c4), ri(0, 4) == 0 ? 2u : 0u);
            seed_rand();
            return Args{{M}, {}};
        }, [](A a) { infobot_glow_particles(a.i[0]); return uint64_t{0}; });

        check("p02_infobot_update", 0x002d5de8, 160, Ret::None, [&] {
            level();
            const uint32_t m = moby_spawn(750);
            if (m == 0) return Args{{0u}, {}};
            std::memset(ee_ram_ptr(PV), 0, 0xe0);
            w32(m + 0x78, PV);
            const uint32_t st = u(ri(0, 11));
            w8(m + 0x20, st);
            near(m + 0x10, 12.0f);
            w32(PV + 0, u(coin()));
            w32(PV + 4, u(ri(0, 1) == 0 ? 0xc : 1));
            w32(PV + 8, coin() ? 0u : 0xffffffffu);
            w8(PV + 0xc, 0xff);
            w8(PV + 0xd, 0xff);
            w8(PV + 0xe, 0xff);
            w32(PV + 0x50, (st == 5 || coin()) ? 0u : 0xffffffffu);
            w32(PV + 0x54, 0xffffffffu);
            w32(PV + 0x58, 0u);
            wf(PV + 0x5c, rf(-3.14f, 3.14f));
            wf(PV + 0x60, rf(0.0f, 0.5f));
            w32(PV + 0x70, (st == 3 || st == 4 || coin()) ? 0u : 0xffffffffu);
            for (uint32_t i = 1; i < 8; ++i) w32(PV + 0x70 + i * 4, 0xffffffffu);
            w32(PV + 0xa8, 0);
            wf(PV + 0xa4, rf(0.0f, 1.0f));
            w32(PV + 0xb0, u(coin()));
            for (uint32_t i = 0; i < 4; ++i) {
                wf(PV + 0x10 + i * 4, rf(10.0f, 240.0f));
                wf(PV + 0x20 + i * 4, rf(-2.0f, 2.0f));
                w32(PV + 0x30 + i * 4, u(ri(1, 200)));
                wf(PV + 0x40 + i * 4, rf(1000.0f, 15000.0f));
            }
            if (st == 2 || st == 3 || st == 7) {
                if (coin()) VV(m + 0x10) = VV(g::PLAYER + 0x80);
            }
            w32(0x001415f8, u(coin()));
            w32(OV(0x0015f5c4), (st == 11 || (st >= 9 && coin())) ? 2u : 0u);
            w8(0x0013dd40 + ee_read<uint32_t>(PV + 4), ri(0, 5) == 0 ? 1u : 0u);
            if (ri(0, 2) == 0) {
                wf(0x0015ed60, 1.2f);
                wf(0x0015ed6c, 0.02f);
                wf(0x0015ed70, 0.0004f);
            }
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) infobot_update_tr(a.i[0]); return uint64_t{0}; });
    }

    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
