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
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_types_tr.h>

#include <algorithm>
#include <cstring>
#include <functional>
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
                const TrRet r = ovl_270758_tr(P, P + 0x10, u(ri(0, 8)) & 0xff, coin() ? 0u : u(ri(0, 0x88)), 0, 0, 0, 0, 0, 0, 0, 0);
                if (r.v0 != 0 && std::find(before.begin(), before.end(), r.v0) == before.end()) q = r.v0;
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
    }, [](A a) { return uint64_t{ovl_270758_tr(a.i[0], a.i[1], a.i[2], a.i[3], 0, 0, 0, 0, 0, 0, 0, 0).v0}; });

    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
