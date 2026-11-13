// Oracle batch 106 (task 0330): the weapons and gadgets of R&C1, second part, on Veldin's real level
// state (needs the ISO; also --level 1..3): the Glove of Doom's agent (class 0xe6) and its callees,
// then the ports that follow in the same file as they are added. A full run takes longer than 15 minutes on a busy machine:
// ORACLE_ONLY=<check> runs one check at a time (the "/states", "/stream", "/rings", "/tail" checks reach the paths of the
// doomlet and the Suck Cannon the first checks of those functions seldom take).

// Audit 2 (2026-11-13): the screen (640 x 448), projection and view set up and the packet cursor on a test-area buffer in every
// snapshot (view_setup: before, the weapons' and effects' draw callbacks projected nothing, e.g. the Decoy Glove's ring drew no
// quad); each item's gold flag 0x13e520 + item set in half of the cases (no case had a gold weapon before); the Metal Detector
// given a buried find and the Hydrodisplacer its water flags; screen_flash on a real screen with its packets in the test area.

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>
#include <exception>
#include <set>
#include <game/ovl/metal_obj.h>
#include <game/ovl/fx_26a5b8.h>
#include <game/ovl/fx_26ee48.h>
#include <game/ovl/morph_fx.h>
#include <game/ovl/morph_draw.h>
#include <game/ovl/ryno_fx.h>
#include <game/ovl/decoy_fx.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/screen_flash.h>
#include <game/ovl/decoy_obj.h>
#include <game/ovl/tesla_ball.h>
#include <game/ovl/visibomb_ball.h>
#include <game/ovl/dev_271450.h>
#include <game/ovl/rocket.h>
#include <game/ovl/grapple.h>
#include <game/ovl/ryno.h>
#include <game/ovl/metal.h>
#include <game/ovl/hydro.h>
#include <game/ovl/morph.h>
#include <game/ovl/decoy.h>
#include <game/ovl/tesla.h>
#include <game/ovl/visibomb.h>
#include <game/ovl/devastator.h>
#include <game/ovl/swingshot.h>
#include <map>
#include <game/ovl/suck_cannon.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/novalis_hooks.h>
#include <game/ovl/carry_hooks_levels.h>
namespace rac1::oracle {

namespace {
// A check whose original is Novalis's own function at `l1addr` (not a Veldin address to translate).
void l1check(const char* name, uint32_t l1addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 1) {
        std::printf("%-26s L1:0x%08x  skipped (Novalis only: run with --level 1)\n", name, l1addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L1:0x%08x\n", name, l1addr);
    if (!check_fn(name, l1addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// The screen (640 x 448, as the game's), the projection and the view set up, and the packet cursor on a buffer of the test
// area: after the level's first frame the oracle's display leaves the projection's constants broken (nothing projects onto
// the screen) and the cursor 0, so the weapons' and effects' draw callbacks (the beams, rings, glows, trails) wrote nothing
// past their GS state and the checks compared no quad of theirs (as batches 506 / 510 before their audit). Since audit 2.
void view_setup() {
    if (ee_read<uint16_t>(0x001518d0) == 0) {
        w16(0x001518d0, 640);
        w16(0x001518d2, 448);
    }
    screen_setup();
    projection_setup();
    render_view_setup();
    w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10);
}
// (task 0410) the carried classes' hook tables (`class +0x2c`) of the levels' own overlays (section 3: class, update, table), the
// default class's left out: `own` a class of the level's own (the others are class 0x10e's, the Morph-o-Ray chicken's)
struct LvHookTab { int level; uint32_t tab; bool own; };
constexpr LvHookTab kLvTabs[] = {
    {2, 0x001fb6c4, false}, {2, 0x001fb6dc, true}, {3, 0x001e4114, false}, {4, 0x001dd104, false}, {4, 0x001dd11c, true},
    {5, 0x002193d0, false}, {5, 0x002193e8, true}, {5, 0x00219400, true}, {6, 0x0020239c, false}, {6, 0x002023b4, true},
    {7, 0x002125dc, false}, {8, 0x001fd200, true}, {8, 0x001fd218, false}, {9, 0x00209b00, true}, {9, 0x00209b18, false},
    {10, 0x001dde40, false}, {10, 0x001dde58, true}, {11, 0x0021bff0, false}, {11, 0x0021c008, true}, {11, 0x0021c020, true},
    {11, 0x0021c038, true},
};
// the table the carried mobys of this level use in the checks: the level's own class's first, else class 0x10e's (0: none)
uint32_t level_carry_tab() {
    uint32_t t = 0;
    for (const LvHookTab& r : kLvTabs) {
        if (r.level != g_oracle_level) continue;
        if (r.own) return r.tab;
        if (t == 0) t = r.tab;
    }
    return t;
}
// the hook at `fn` (slot +0 .. +0x10 of a table) as the port's dispatchers run it: the level's own first (carry_hooks_levels.cpp),
// then Veldin's ports through the overlay map (the suck cannon's jalr, class_hook_4 / class_hook_10)
int32_t hook_port(uint32_t fn, uint32_t slot, uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    int32_t r = 0;
    if (class_hook_call_level(fn, m, a1, a2, f12, r)) return r;
    switch (ovl_code_to_veldin(fn)) {
    case 0x002cadc0: return enemy10e_hook0(m, a1, a2, f12);
    case 0x002cae10: return enemy10e_hook4(m, a1, a2, f12);
    case 0x002cae60: return enemy10e_hook8(m, a1, a2, f12);
    case 0x002caf10: return enemy10e_hookc(m, a1, a2, f12);
    case 0x002caf40: return static_cast<int32_t>(enemy10e_hook_10(m));
    case 0x002d56e0: return e749_hook0(m, a1, a2, f12);
    case 0x002d5730: return e749_hook4(m, a1, a2, f12);
    case 0x002d5780: return e749_hook8(m, a1, a2, f12);
    case 0x002d57d0: return e749_hookc(m, a1, a2, f12);
    default: std::printf("  hook_port: no port for 0x%08x (slot +0x%x)\n", fn, slot); return 0;
    }
}
}  // namespace

void batch_106() {
    if (g_only_batch != 106) return;
    g_batch = 106;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime (null pointers read as RAM, as on the EE)
    const bool trap_noop = g_debug_trap_is_noop;
    g_debug_trap_is_noop = true;  // (the engine's asserts the weapon snapshots may reach: a no-op in the interpreter)
    const bool oob_zero = g_ee_oob_is_zero;
    g_ee_oob_is_zero = true;  // (an unmapped pointer in the random pvars: 0 on both sides, as in the interpreter)
    static std::vector<uint8_t> snap;
    auto level1 = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
        view_setup();  // the screen, the projection and the camera's matrices in VU0 (point_on_screen), the packet cursor
        collide_vu_reset();  // the contact registers vf25..vf29 the collision leaves, the same on both sides
        oracle_vu_contact_reset();
    };
    constexpr uint32_t P = 0x0013f350, PAD = 0x0013c940;
    // a hand moby of the item whose class runs `fn` (its class loaded with the hero's items)
    auto item_moby = [](uint32_t fn) -> uint32_t {
        for (uint32_t i = 1; i < 0x25; ++i) {
            const uint32_t c = ee_read<uint32_t>(OV(0x00179ac0) + i * 0x4c + 0x10);
            if (c >= 0x800) continue;
            const uint32_t ci = ee_read<uint8_t>(OV(0x00197bc0) + c);
            if (ci == 0xff || ee_read<uint32_t>(OV(0x00197680) + ci * 4) != fn) continue;
            return moby_spawn(c);
        }
        return 0;
    };
    auto pickf = [](std::initializer_list<float> l) { return *(l.begin() + ri(0, static_cast<int>(l.size()) - 1)); };
    auto pick = [](std::initializer_list<uint32_t> l) { return *(l.begin() + ri(0, static_cast<int>(l.size()) - 1)); };
    // the hero / pad / ammo state a hand update reads, for the item `item`
    auto hero_vary = [pick](uint32_t item) {
        if (ee_read<uint32_t>(P + 0x10a0) == 0) w32(P + 0x10a0, 0x20u);  // the fire button's mask (circle)
        w32(PAD + 0x1a0, ri(0, 2) != 0 ? ee_read<uint32_t>(P + 0x10a0) | (R() & 0x9005u) : (R() & 0x9005u));
        w32(PAD + 0x1a4, coin() ? ee_read<uint32_t>(P + 0x10a0) : 0u);
        w32(0x0013cae4, coin() ? ee_read<uint32_t>(P + 0x10a0) : 0u);
        w8(P + 0x20ac, u(ri(0, 4) == 0));
        w8(P + 0x20a8, u(ri(0, 3) != 0));
        w32(P + 0x2084, pick({0u, 1u, 0x1eu, 0x1eu, 0x23u, 0x23u}));
        w32(P + 0x198, u(ri(0, 20)));
        w32(P + 0x1bc, u(ri(0, 20)));
        w32(0x001413d4, pick({0u, 1u, 1u, 2u}));
        w8(0x0013e530, u(ri(0, 1)));
        w32(OV(0x0015f5cc), R());
        w32(P + 0x10b0, u(ri(0, 40)));
        wf(P + 0x98, rf(-3.1f, 3.1f));
        w32(0x00140408, item);
        w32(0x0013d428 + item * 4, ri(0, 4) == 0 ? 0u : u(ri(1, 240)));
    };
    auto near_hero = [](uint32_t out, float r, float zlo, float zhi) {
        Vec4 p = ee_ref<Vec4>(0x0013f3d0); p.x += rf(-r, r); p.y += rf(-r, r); p.z += rf(zlo, zhi); ee_ref<Vec4>(out) = p;
    };
    (void)pickf;
    const std::vector<uint32_t> ign_saved = g_ignored_words;
    for (uint32_t i = 0; i < 0x100; ++i) g_ignored_words.push_back(OV(0x00178100) + i * 0x40 + 0x28);
    for (uint32_t a = 0x70003708; a < 0x70003710; a += 4) g_ignored_words.push_back(a);
    for (uint32_t a = 0x70003718; a < 0x70003720; a += 4) g_ignored_words.push_back(a);

    // --- a level with an item in hand: the engine's own state (the hand moby and its pvars, the projectiles) as the port's frames
    // make it, instead of a random one: snapshots taken after the item is given and equipped (kind 0: idle), with the fire
    // button pressed (kind 1: one frame, then 5 more), held (kind 2: 14 frames)
    auto pad_set = [](uint32_t held) {
        const uint16_t low = static_cast<uint16_t>(~held);
        const uint8_t pk[6] = {static_cast<uint8_t>(low >> 8), static_cast<uint8_t>(low & 0xff), 0x7f, 0x7f, 0x7f, 0x7f};
        pad_read_packet(0x0013c940, pk, 6);
    };
    static std::map<uint32_t, std::vector<uint8_t>> item_snaps;
    auto item_snap = [level1, pad_set](uint32_t item, uint32_t kind) -> const std::vector<uint8_t>& {
        const uint32_t key = item * 8 + kind;
        const auto it = item_snaps.find(key);
        if (it != item_snaps.end()) return it->second;
        level1();
        // (Novalis's class 730 starts a cutscene (cutscene_start 5: the scene data is not in the oracle's world) a few frames in: retired for the snapshot)
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint16_t>(m + 0xa6) == 730 && ee_read<uint8_t>(m + 0x20) < 0xf0) w8(m + 0x20, 0xfe);
        w8(0x0013d397, 1);  // (the flag class 730 tests before cutscene_start 5)
        item_give(static_cast<int32_t>(item), 0);
        w32(P + 0x20b8, item);
        const uint32_t equip_frames = 40;
        const uint32_t n = equip_frames + (kind == 0 ? 0u : (kind == 1 ? 6u : 14u));
        for (uint32_t f = 0; f < n; ++f) {
            uint32_t held = 0;
            if (kind == 1 && f == equip_frames) held = 0x0020;
            if (kind == 2 && f >= equip_frames) held = 0x0020;
            pad_set(held);
            gameplay_frame();
            w32(OV(0x0015f3f8), ee_read<uint32_t>(OV(0x0015f3f8)) + 1);
        }
        uint8_t* ram = ee_get_ram_base();
        return item_snaps[key] = std::vector<uint8_t>(ram, ram + 0x02000000);
    };
    // the item's live moby of class `cls` (0: its hand class), first; 0 when none
    auto moby_of_class = [](uint32_t cls) -> uint32_t {
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint16_t>(m + 0xa6) == cls && ee_read<uint8_t>(m + 0x20) < 0xf0) return m;
        return 0;
    };
    auto hand_class = [](uint32_t item) { return ee_read<uint32_t>(OV(0x00179ac0) + item * 0x4c + 0x10); };
    // a check generator of an update on the item's `cls` moby: the snapshot of `kind` restored, the hero / pad / the moby's state and pvars
    // varied a little (`nstates`: the update's states; the moby's byte +0x20 set to a random one half of the time)
    auto weapon_gen = [item_snap, moby_of_class, hand_class, pick, near_hero](uint32_t item, uint32_t cls, uint32_t kind, uint32_t nstates,
                                                                                std::function<void(uint32_t, uint32_t)> extra = nullptr) {
        return [=]() -> Args {
            const std::vector<uint8_t>& sn = item_snap(item, kind);
            std::memcpy(ee_get_ram_base(), sn.data(), sn.size());
            view_setup();
            collide_vu_reset();
            oracle_vu_contact_reset();
            // (the item's gold flag 0x13e520 + item: the snapshots are of the plain weapon; half of the cases the gold one, whose
            // branches (cones, colours, sizes, classes) no case reached before)
            w8(0x0013e520 + item, coin() ? 1u : 0u);
            const uint32_t m = moby_of_class(cls != 0 ? cls : hand_class(item));
            if (m == 0) return Args{{0u}, {}};
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (ri(0, 1) == 0 && nstates != 0) w8(m + 0x20, u(ri(0, static_cast<int>(nstates) - 1)));
            if (ri(0, 2) == 0) w32(P + 0x2084, pick({0u, 0x1eu, 0x23u, 0x1eu}));
            if (ri(0, 2) == 0) w32(PAD + 0x1a0, R() & 0x9025u);
            if (ri(0, 2) == 0) w32(PAD + 0x1a4, R() & 0x9025u);
            if (ri(0, 3) == 0) w8(P + 0x20ac, u(ri(0, 1)));
            if (pv != 0) {  // the small words (timers, counters, flags; not the pointers, nor the floats) varied
                for (uint32_t k = 0; k < 0x20; ++k) {
                    const uint32_t cur = ee_read<uint32_t>(pv + 4 * k);  // (a zero may be a null pointer: only the non-zero small words)
                    if (cur != 0 && cur < 0x1000u && ri(0, 3) == 0) w32(pv + 4 * k, pick({0u, 1u, 2u, u(ri(0, 40))}));
                }
            }
            if (ri(0, 4) == 0) near_hero(m + 0x10, 3.0f, -1.0f, 2.0f);
            if (extra) extra(m, pv);
            if (ee_read<int32_t>(P + 0x2084) == 1) w32(P + 0x2084, 0x1eu);  // (hero_set_state(0x1e, 1) waits for a VU job the interpreter never finishes)
            seed_rand();
            return Args{{m}, {}};
        };
    };

    // --- the Glove of Doom's agent (class 0xe6, update 0x2c89e8) and its callees
    static uint32_t dhand = 0;
    {
        level1();
        dhand = item_moby(OVF(0x002c7a58));
        std::printf("  (glove of doom hand moby %08x, agent class 0xe6 %s, class 0xba %s)\n", dhand,
                    moby_spawn(0xe6) != 0 ? "present" : "absent", moby_spawn(0xba) != 0 ? "present" : "absent");
    }
    check("spawn_particle_05", 0x00269b70, 200, Ret::None, [level1, near_hero] {
        level1();
        near_hero(OUT, 3.0f, 0.0f, 2.0f);
        seed_rand();
        const int32_t life = ri(0, 5) == 0 ? 0 : ri(1, 80);
        if (ri(0, 9) == 0) w32(OV(0x00160170), 0x800);
        return Args{{OUT, u(ri(0, 255)), u(ri(0, 255)), u(ri(0, 255)), u(life)}, {rf(1000.0f, 900000.0f), rf(0.0f, 4.0f)}};
    }, [](A a) { spawn_particle_05(a.i[0], a.i[1], a.i[2], a.i[3], static_cast<int32_t>(a.i[4]), a.f[0], a.f[1]); return uint64_t{0}; });
    check("glow_particle_20", 0x0026ed28, 200, Ret::Int32, [level1, near_hero] {
        level1();
        const uint32_t m = M2;
        near_hero(m + 0x10, 3.0f, 0.0f, 2.0f);
        seed_rand();
        if (ri(0, 9) == 0) w32(OV(0x00160170), 0x800);
        return Args{{m}, {}};
    }, [](A a) { return r32(glow_particle_20(a.i[0])); });
    check("doom_spawn_agent", 0x002bfe08, 100, Ret::Int32, [level1, near_hero] {
        level1();
        near_hero(OUT, 3.0f, 0.0f, 2.0f);
        wfrand(OUT + 0x20, 3, -1.0f, 1.0f);
        if (ri(0, 8) == 0) w32(OV(0x00160170), 0x800);
        seed_rand();
        return Args{{ee_read<uint32_t>(P + 0x2080), OUT, OUT + 0x20}, {}};
    }, [](A a) { return r32(doom_spawn_agent(a.i[0], a.i[1], a.i[2])); });
    // an agent moby (class 0xe6) in state `state` near the hero
    auto doom_agent = [pick, near_hero](uint32_t state, bool mid_air) -> uint32_t {
        const uint32_t hand = dhand != 0 ? moby_spawn(ee_read<uint16_t>(dhand + 0xa6)) : 0;
        const uint32_t a = moby_spawn(0xe6);
        if (a == 0) return 0;
        const uint32_t pv = ee_read<uint32_t>(a + 0x78);
        near_hero(a + 0x10, ri(0, 3) == 0 ? 12.0f : 3.0f, mid_air ? 0.5f : 0.0f, mid_air ? 4.0f : 1.5f);
        w8(a + 0x20, state);
        w16(a + 0x34, R() & 0x7f);
        w8(a + 0x23, u(ri(0, 255)));
        wf(a + 0x2c, rf(0.0f, 0.1f));
        wfrand(pv, 3, ri(0, 3) == 0 ? -0.4f : -8.0f, ri(0, 3) == 0 ? 0.4f : 8.0f);
        wf(pv + 8, ri(0, 2) == 0 ? rf(-0.1f, 0.1f) : rf(-8.0f, 8.0f));
        wfrand(pv + 0x10, 3, -50.0f, 50.0f);
        wfrand(pv + 0x20, 3, -1.0f, 1.0f);
        w32(pv + 0x30, ri(0, 6) == 0 ? 0u : hand);
        w32(pv + 0x34, ri(0, 3) == 0 ? 0u : u(ri(0, 6)));
        w16(pv + 0x38, ri(0, 3) == 0 ? 0u : u(ri(0, 300)));
        w16(pv + 0x3a, u(ri(0, 3) == 0));
        w32(pv + 0x3c, u(ri(0, 5)));
        {  // some agents proper (class 0xba) alive in the moby list: the spawn limit of 8
            uint32_t head = 0;
            for (int k = ri(0, 2) == 0 ? ri(5, 10) : ri(0, 4); k > 0; --k) {
                const uint32_t o = moby_spawn(0xba);
                if (o == 0) continue;
                w8(o + 0x20, ri(0, 3) == 0 ? 0xf0u + u(ri(0, 15)) : u(ri(0, 5)));
                w32(o + 0x28, head);
                head = o;
            }
            if (ri(0, 2) == 0) { const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)); w32(o + 0x28, head); head = o; }
            w32(OV(0x0015ffe4), head);
        }
        w32(OV(0x00173e5c), ri(0, 3) == 0 ? R() : u(ri(0, 15)));
        w32(OV(0x00173e58), ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(P + 0x2080));
        if (hand != 0 && ri(0, 2) == 0) w32(ee_read<uint32_t>(hand + 0x78) + 0x50, 0u);
        else if (hand != 0) w32(ee_read<uint32_t>(hand + 0x78) + 0x50, hand);
        return a;
    };
    auto doom_world = [level1, hero_vary, near_hero, pick] {
        level1();
        hero_vary(0x14);
        if (ri(0, 2) == 0) { wfrand(OV(0x00166c80), 3, -1.0f, 1.0f); wfrand(OV(0x00166c90), 3, -1.0f, 1.0f); wfrand(OV(0x00166ca0), 3, -1.0f, 1.0f); near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f); }
        w32(P + 0x2fc, ri(0, 3) == 0 ? ee_read<uint32_t>(P + 0x2080) : 0u);
        if (ri(0, 3) != 0) wfrand(0x0013f490, 3, -0.2f, 0.2f);
        if (ri(0, 3) != 0) wfrand(0x0013f450, 3, -0.3f, 0.3f);
        if (ri(0, 3) != 0) wfrand(P + 0x100, 3, -0.3f, 0.3f);
        if (ri(0, 5) == 0) w32(P + 0x208c, 0xf);
        if (ri(0, 6) == 0) w32(P + 0x2084, 0x72);
        if (ri(0, 4) == 0) w8(P + 0x20a5, u(ri(0, 1)));
        if (ri(0, 6) == 0) w8(P + 0x20a4, 1);
        w8(0x0013e534, u(ri(0, 3)));
        if (ri(0, 3) == 0) w32(P + 0x1090, dhand);
    };
    check("doom_agent_splash", 0x002c8078, 300, Ret::None, [doom_world, doom_agent, near_hero] {
        doom_world();
        const uint32_t a = doom_agent(2, true);
        if (a == 0) return Args{{0u}, {}};
        // the last probe's surface type 0x1f35d8 reads: set by a floor probe of a random place
        near_hero(OUT, 6.0f, -1.0f, 2.0f);
        (void)floor_probe(0.5f, OUT, 0);
        w32(OV(0x00173e5c), ri(0, 4) == 0 ? R() : u(ri(0, 15)));  // the surface type the probe left
        seed_rand();
        return Args{{a}, {}};
    }, [](A a) { if (a.i[0] != 0) doom_agent_splash(a.i[0]); return uint64_t{0}; });
    check("doom_agent_draw", 0x002c7df8, 300, Ret::None, [level1, near_hero] {
        level1();
        const uint32_t m = M2;
        w32(m + 0x78, T);
        near_hero(T + 0x10, 3.0f, 0.0f, 2.0f);
        wfrand(T + 0x20, 3, -1.0f, 1.0f);
        near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f);
        wfrand(0x0013f5e0, 3, -1.0f, 1.0f);
        w32(OV(0x0015f5cc), R());
        if (ri(0, 8) == 0) w32(OV(0x0015f5c4), 2);
        return Args{{m}, {}};
    }, [](A a) { doom_agent_draw(a.i[0]); return uint64_t{0}; });
    check("doom_agent_step", 0x002c8440, 1500, Ret::Int32, [doom_world, doom_agent, pick] {
        doom_world();
        const uint32_t a = doom_agent(pick({1u, 2u, 2u, 3u}), true);
        if (a == 0) return Args{{0u, 0u, 0u}, {}};
        seed_rand();
        return Args{{ee_read<uint32_t>(a + 0x78), a, pick({0u, 1u, 2u})}, {}};
    }, [](A a) { return r32(a.i[0] != 0 ? u(doom_agent_step(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]))) : 0u); });
    check("doom_agent_update", 0x002c89e8, 3000, Ret::None, [doom_world, doom_agent, pick, near_hero] {
        doom_world();
        const uint32_t a = doom_agent(pick({0u, 1u, 2u, 2u, 2u, 3u, 3u, 4u, 5u, 6u}), ri(0, 4) != 0);
        if (a == 0) return Args{{0u}, {}};
        if (ri(0, 12) == 0) near_hero(a + 0x10, 1500.0f, -1.0f, 1.0f);
        seed_rand();
        return Args{{a}, {}};
    }, [](A a) { if (a.i[0] != 0) doom_agent_update(a.i[0]); return uint64_t{0}; });

    // --- the Suck Cannon (item 0x09, hand 0x2da790) and the carried mobys' class hooks
    static uint32_t shand = 0, kA = 0, kB = 0;
    // moby_spawn of a class the level has not loaded: its class data is garbage (the spawn throws): none
    auto safe_spawn = [](uint32_t c) -> uint32_t {
        try { return moby_spawn(c); } catch (const std::exception&) { return 0u; }
    };
    {
        level1();
        shand = item_moby(OVF(0x002da790));
        // the carried class: Veldin's enemy 749, Novalis's Peckbot; elsewhere class 0x10e (the levels' variants of class 749's hooks differ by
        // constants the overlay matcher masks: not compared)
        kA = g_oracle_level == 1 ? 0x241u : (g_oracle_level == 0 && safe_spawn(0x2ed) != 0 ? 0x2edu : (safe_spawn(0x10e) != 0 ? 0x10eu : 0u));
        kB = kA == 0x241u ? 0x23cu : 0x10eu;  // and a second one (Novalis's Large Amoeboid)
        std::printf("  (suck cannon hand moby %08x, carried classes 0x%x / 0x%x, class 0x10e %s)\n", shand, kA, kB, safe_spawn(0x10e) != 0 ? "present" : "absent");
    }
    // a moby of a carried class; on a level other than Veldin (0) and Novalis (1) the class data (loaded from Veldin's level)
    // holds Veldin's hook addresses: a table of the level's own equivalents is put in their place (the default class's where it has none)
    auto spawn_carried = [safe_spawn](uint32_t c) -> uint32_t {
        const uint32_t o = safe_spawn(c);
        // (a moby's pvars are 0x80 bytes in its slot; a carried class's record reaches past them: the next two slots taken, so
        // their pvars are not another spawn's)
        if (o != 0) { (void)safe_spawn(0x7a); (void)safe_spawn(0x7a); }
        if (o != 0 && g_oracle_level >= 2) { (void)safe_spawn(0x7a); (void)safe_spawn(0x7a); }  // (task 0410: Eudora's record is pvars + 0x180)
        if (o == 0 || g_oracle_level == 0 || g_oracle_level == 1) return o;
        const uint32_t cls = ee_read<uint32_t>(o + 0x24);
        if (cls == 0) return o;
        // (task 0410) class 0x10e stands for the level's carried class: the level's own hook table in place (its own class's, else
        // class 0x10e's), so the checks run the hooks the level's mobys have (before: Veldin's equivalents, which hid the levels' own)
        if (c == 0x10e && level_carry_tab() != 0) {
            w32(cls + 0x2c, level_carry_tab());
            return o;
        }
        static const uint32_t def[5] = {0x002db848, 0x002db850, 0x002db858, 0x002db860, 0x002db888};
        static const uint32_t e749[5] = {0x002d56e0, 0x002d5730, 0x002d5780, 0x002d57d0, 0x002d5800};
        static const uint32_t e10e[5] = {0x002cadc0, 0x002cae10, 0x002cae60, 0x002caf10, 0x002caf40};
        (void)e749;
        const uint32_t* k = c == 0x10e ? e10e : def;
        const uint32_t tab = T4 + 0x400 + 0x20 * (c == 0x10e ? 2u : 0u);
        for (uint32_t i = 0; i < 5; ++i) {
            uint32_t at = ovl_code_lookup(k[i]);
            if (at == 0) at = ovl_code_lookup(def[i]);
            w32(tab + 4 * i, at);
        }
        w32(cls + 0x2c, tab);
        return o;
    };
    // a carried moby's record (the class hook +0x10: its pvars +0xd0 / +0x60 / +0xc0, or a pointer at +0x14 for class 0x10e)
    auto rec_of = [](uint32_t o, uint32_t c) -> uint32_t {
        const uint32_t pv = ee_read<uint32_t>(o + 0x78);
        if (pv == 0) return 0;
        switch (c) {
        case 0x2ed: return pv + 0xd0;
        case 0x241: return pv + 0x60;
        case 0x23c: return pv + 0xc0;
        case 0x10e: {
            // (task 0410) a level's own class's table: the record its hook +0x10 gives (pvars + 0x60 / 0xc0 / 0x180..)
            const uint32_t fn10 = ee_read<uint32_t>(ee_read<uint32_t>(ee_read<uint32_t>(o + 0x24) + 0x2c) + 0x10);
            int32_t rr = 0;
            if (g_oracle_level >= 2 && class_hook_call_level(fn10, o, 0, 0, 0.0f, rr)) return static_cast<uint32_t>(rr);
            const uint32_t r = T3 + 0x100 * u(ri(0, 12));
            w32(pv + 0x14, r);
            return r;
        }
        default: return 0;
        }
    };
    // its fields: random floats, the owner, the state, the animation set record, the pointers
    auto fill_rec = [](uint32_t rec, uint32_t o, uint32_t hand) {
        wfrand(rec, 31, -6.0f, 6.0f);  // (up to +0x7c: the record is the pvars' tail of its class)
        w16(rec + 0x68, u(ri(0, 9)));
        w32(rec + 0x60, ri(0, 3) == 0 ? 0u : hand);
        w16(rec + 0x6a, u(ri(0, 6)));
        { const uint32_t d = T4 + 0x10 * u(ri(0, 8)); for (uint32_t q = 0; q < 8; ++q) w8(d + q, u(ri(0, 5))); w32(rec + 0x70, d); }
        w32(rec + 0x74, ri(0, 2) == 0 ? 0u : hand);
        w32(rec + 0x78, ri(0, 2) == 0 ? 0u : o);
    };
    // some suckable mobys near the hero, linked as the moby list
    auto suck_targets = [pick, near_hero, rec_of, fill_rec, spawn_carried](uint32_t hand) -> uint32_t {
        uint32_t head = 0;
        for (int k = ri(0, 6); k > 0; --k) {
            const uint32_t c = pick({kA, kA, kB, 0x10eu, 0x4au, 0x79u});
            const uint32_t o = c == 0 ? 0u : spawn_carried(c);
            if (o == 0) continue;
            const uint32_t pv = ee_read<uint32_t>(o + 0x78);
            near_hero(o + 0x10, ri(0, 3) == 0 ? 25.0f : 8.0f, -1.0f, 3.0f);
            w16(o + 0x34, (R() & 0xefffu) | (ri(0, 5) != 0 ? 0x1000u : 0u));
            w8(o + 0x20, pick({0u, 1u, 2u, 3u, 5u, 0xfeu, 0xfdu, 0x85u}));
            const uint32_t cls = ee_read<uint32_t>(o + 0x24);
            if (cls != 0 && ri(0, 5) != 0) w16(cls + 0x46, ri(0, 6) != 0 ? 5u : u(ri(0, 9)));
            const uint32_t rec = pv != 0 ? rec_of(o, c) : 0u;
            if (rec != 0) fill_rec(rec, o, hand);
            w32(o + 0x28, head);
            head = o;
            if (rec != 0 && ri(0, 1) == 0) {  // one of the sucked ones: a slot of the hero's table, its state 3 / 4
                w16(rec + 0x68, u(ri(3, 4)));
                w16(rec + 0x6a, ri(0, 5) == 0 ? 0xffffu : u(ri(0, 6)));
                w32(P + 0x2050 + 4 * u(ri(0, 9)), o);
            }
        }
        if (ri(0, 3) == 0) { const uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)); w32(o + 0x28, head); head = o; }
        w32(OV(0x0015ffe4), head);
        return head;
    };
    auto suck_world = [level1, hero_vary, near_hero, pick] {
        level1();
        hero_vary(0x09);
        if (ri(0, 2) == 0) { wfrand(OV(0x00166c80), 3, -1.0f, 1.0f); wfrand(OV(0x00166c90), 3, -1.0f, 1.0f); wfrand(OV(0x00166ca0), 3, -1.0f, 1.0f); near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f); }
        w32(P + 0x2fc, ri(0, 4) == 0 ? ee_read<uint32_t>(P + 0x2080) : 0u);
        w32(P + 0x2078, u(ri(-1, 12)));  // ammo
        w32(P + 0x207c, u(ri(0, 12)));
        w32(P + 0x12b0, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)));
        w32(P + 0x12b8, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)));
        wf(P + 0x12bc, rf(-3.0f, 3.0f));
        w8(P + 0x20a7, u(ri(0, 5) == 0));
        w8(P + 0x10ac, u(ri(0, 2)));
        w32(P + 0x208c, pick({0u, 1u, 2u, 3u, 7u, 0x11u, 0x12u, 0xcu, 5u}));
        if (ri(0, 6) == 0) w32(P + 0x2084, 0x72);
        w8(0x0013e529, u(ri(0, 1)));
        w32(0x0013cae0, coin() ? ee_read<uint32_t>(P + 0x10a0) : 0u);  // the suck button
        w16(ee_read<uint32_t>(P + 0x2080) + 0x34, ee_read<uint16_t>(ee_read<uint32_t>(P + 0x2080) + 0x34) | (coin() ? 1u : 0u));
        if (ri(0, 3) == 0) w32(P + 0x2078, u(ri(1, 12)));
        for (uint32_t i = 0; i < 10; ++i) w32(P + 0x2050 + 4 * i, 0u);  // the sucked slots (the targets' ones set by suck_targets)
        for (uint32_t i = 0; i < 10; ++i) {  // the table 0x13e550 (0x70 each: +0x88 the owner, +0x74 live)
            w32(0x0013e550 + i * 0x70 + 0x38, 0u);
            w8(0x0013e550 + i * 0x70 + 0x24, u(ri(0, 1)));
        }
    };
    auto suck_hand = [pick, suck_targets, rec_of, fill_rec, spawn_carried](uint32_t state) -> uint32_t {
        if (shand == 0) return 0;
        const uint32_t m = moby_spawn(ee_read<uint16_t>(shand + 0xa6));
        const uint32_t pv = ee_read<uint32_t>(m + 0x78);
        VV(m + 0x10) = ee_ref<Vec4>(0x0013f3d0);
        for (uint32_t k = 0; k < 0x30; k += 0x10) VV(m + 0xc0 + k) = VV(0x0013f990 + k);
        w8(m + 0x20, state);
        w8(m + 0x53, pick({0u, 1u, 3u}));
        w8(m + 0x70, R() & 3);
        w16(m + 0x34, R() & 0xff);
        w32(pv + 4, pick({0xffffffffu, 0xffffffffu, 0u, 1u, 3u}));
        w32(pv + 8, pick({0u, 0u, 1u, u(ri(0, 20))}));
        w32(pv + 0xc, u(ri(0, 1)));
        const uint32_t head = suck_targets(m);
        w32(pv + 0x10, ri(0, 2) == 0 ? 0u : head);
        w32(pv + 0x14, pick({0u, 1u, u(ri(0, 20))}));
        wf(pv + 0x18, rf(-2.0f, 2.0f));
        for (uint32_t i = 0; i < 10; ++i) {
            if (ri(0, 4) == 0) { w32(0x0013e550 + i * 0x70 + 0x88, m); w8(0x0013e550 + i * 0x70 + 0x74, u(ri(0, 1))); }
        }
        if (ri(0, 2) == 0) {  // the hero ready to use it: every gate of the update's firing branches open
            const uint32_t mask = ee_read<uint32_t>(P + 0x10a0);
            w8(P + 0x20ac, 0); w8(P + 0x20a7, 0);
            w32(P + 0x208c, pick({0u, 1u, 2u, 0xcu, 5u}));
            w32(0x0013cae0, mask);
            w32(PAD + 0x1a0, mask | (R() & 0x9005u));
            if (ri(0, 4) != 0) {  // a sucked one in state 4 (the count 0x1413c8 the update recounts first)
                const uint32_t o = kA == 0 ? 0u : spawn_carried(kA);
                if (o != 0) {
                    const uint32_t orec = rec_of(o, kA);
                    fill_rec(orec, o, m);
                    w32(orec + 0x60, m);
                    w16(orec + 0x68, u(ri(3, 4)));
                    w16(orec + 0x6a, 0u);
                    w16(o + 0x34, ee_read<uint16_t>(o + 0x34) | 0x1000u);
                    w32(P + 0x2050 + 4 * u(ri(0, 9)), o);
                }
            }
            w32(P + 0x10b0, u(ri(23, 60)));
            w32(pv + 8, 0u);
            w32(pv + 0xc, 0u);
            w32(P + 0x2084, pick({0u, 1u, 0x1eu, 0x1eu}));
            w16(ee_read<uint32_t>(P + 0x2080) + 0x34, ee_read<uint16_t>(ee_read<uint32_t>(P + 0x2080) + 0x34) | 1u);
        }
        return m;
    };
    check("suck_2d9f78", 0x002d9f78, 100, Ret::Int32, [suck_world] {
        suck_world();
        w32(P + 0x208c, R() % 16u);
        return Args{{}, {}};
    }, [](A) { return r32(u(suck_2d9f78())); });
    check("suck_2da250", 0x002da250, 300, Ret::None, [suck_world, suck_hand] {
        suck_world();
        const uint32_t m = suck_hand(2);
        if (m == 0) return Args{{0u, 0u, 0u, 0u}, {}};
        return Args{{m, OUT, OUT + 0x10, OUT + 0x20}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_2da250(a.i[0], a.i[1], a.i[2], a.i[3]); return uint64_t{0}; });
    if (kA == 0) skip_check("suck_2d9fd0", 0x002d9fd0, "the level has no carried class");
    else check("suck_2d9fd0", 0x002d9fd0, 200, Ret::None, [suck_world] {
        suck_world();
        const uint32_t a = kA == 0 ? 0u : moby_spawn(kA), b = kA == 0 ? 0u : moby_spawn(kA);
        if (a == 0 || b == 0) return Args{{0u, 0u, 0u}, {}};
        for (uint32_t k = 0x48; k < 0x78; k += 4) { w32(a + k, 0u); w32(b + k, 0u); }  // (no animation state: the transform reads the class's frames)
        for (uint32_t k = 0; k < 0x30; k += 0x10) { wfrand(a + 0xc0 + k, 4, -1.0f, 1.0f); wfrand(b + 0xc0 + k, 4, -1.0f, 1.0f); }
        wfrand(OUT, 3, -3.0f, 3.0f);
        return Args{{a, b, OUT}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_2d9fd0(a.i[0], a.i[1], a.i[2]); return uint64_t{0}; });
    check("suck_2ddf60", 0x002ddf60, 800, Ret::None, [suck_world, suck_hand, near_hero, pick] {
        suck_world();
        const uint32_t m = suck_hand(pick({1u, 2u, 3u, 4u}));
        if (m == 0) return Args{{0u, 0u}, {}};
        near_hero(OUT, 1.0f, 0.0f, 1.5f);
        if (ri(0, 1) == 0) {  // the effect's records (0x18 each): flag word, then floats
            for (uint32_t k = 0; k < 10; ++k) { wfrand(OV(0x001e4c94) + 0x18 * k, 6, -2.0f, 2.0f); w32(OV(0x001e4c94) + 0x18 * k, ri(0, 1) == 0 ? 0u : 1u); }
        }
        for (uint32_t k = 0; k < 0x20; ++k) w16(OV(0x001e6008) + 2 * k, ri(0, 3) == 0 ? u(ri(0x40, 0x90)) : u(ri(0, 0x3f)));  // the effect pool's slots
        seed_rand();
        return Args{{m, OUT}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_2ddf60(a.i[0], a.i[1]); return uint64_t{0}; });
    check("suck_2da360", 0x002da360, 800, Ret::None, [suck_world, suck_hand, pick] {
        suck_world();
        const uint32_t m = suck_hand(pick({1u, 2u, 3u, 4u}));
        if (m == 0) return Args{{0u, 0u}, {}};
        seed_rand();
        return Args{{m, ee_read<uint32_t>(m + 0x78)}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_2da360(a.i[0], a.i[1]); return uint64_t{0}; });
    check("suck_cannon_update", 0x002da790, 3000, Ret::None, [suck_world, suck_hand, pick] {
        suck_world();
        const uint32_t m = suck_hand(pick({0u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 6u}));
        if (m == 0) return Args{{0u}, {}};
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_cannon_update(a.i[0]); return uint64_t{0}; });
    // (stage 3d) the paths the checks above seldom reach: the capture of a carried moby by 0x2da058 (its record in state 2,
    // the gold cannon's ten slots), the release hook 0x2da1e0, the beam's particle stream (0x2ddf60 spawning and moving
    // particles, 0x2df048 letting them go, 0x2dd8e8 drawing the rings), the pickups the beam pulls in (0x2df1e0), and the
    // update's tail in game mode 3 and the rumble counters
    auto suck_carried = [pick, pickf, near_hero, rec_of, fill_rec, spawn_carried](uint32_t hand, uint32_t& rec) -> uint32_t {
        rec = 0;
        const uint32_t c = pick({kA, kA, kB, 0x10eu});
        const uint32_t o = c == 0 ? 0u : spawn_carried(c);
        if (o == 0 || ee_read<uint32_t>(o + 0x78) == 0) return 0;
        rec = rec_of(o, c);
        if (rec == 0) return 0;
        fill_rec(rec, o, hand);
        near_hero(o + 0x10, 4.0f, -0.5f, 2.0f);
        w16(o + 0x34, ee_read<uint16_t>(o + 0x34) | 0x1000u);
        w8(o + 0x20, pick({0u, 1u, 2u, 0xfeu}));
        w16(rec + 0x68, pick({2u, 2u, 2u, 2u, 3u, 5u, 1u, 4u, 8u}));
        wf(rec + 0x6c, pickf({0.5f, 1.0f, 2.0f, 1.0f}));
        return o;
    };
    if (kA == 0) {
        skip_check("suck_2da058", 0x002da058, "the level has no carried class");
        skip_check("suck_2da1e0", 0x002da1e0, "the level has no carried class");
    } else {
        check("suck_2da058", 0x002da058, 600, Ret::None, [suck_world, suck_hand, suck_carried, near_hero, pick] {
            suck_world();
            const uint32_t m = suck_hand(3);
            uint32_t rec = 0;
            const uint32_t o = m == 0 ? 0u : suck_carried(m, rec);
            if (o == 0) return Args{{0u, 0u, 0u, 0u, 0u}, {}};
            w8(0x0013e529, u(ri(0, 1)));  // the gold Suck Cannon: ten slots
            w32(P + 0x2078, u(ri(0, 6)));
            w32(P + 0x207c, u(ri(0, 6)));
            for (uint32_t i = 0; i < 10; ++i) w32(P + 0x2050 + 4 * i, ri(0, 2) == 0 ? o : 0u);  // some slots taken
            near_hero(OUT, 1.0f, 0.0f, 1.5f);
            seed_rand();
            return Args{{m, ee_read<uint32_t>(m + 0x78), o, rec, OUT}, {}};
        }, [](A a) { if (a.i[0] != 0) suck_2da058(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4]); return uint64_t{0}; });
        check("suck_2da1e0", 0x002da1e0, 300, Ret::Int32, [suck_world, suck_hand, suck_carried] {
            suck_world();
            const uint32_t m = suck_hand(3);
            uint32_t rec = 0;
            const uint32_t o = m == 0 ? 0u : suck_carried(m, rec);
            seed_rand();
            return Args{{o}, {}};
        }, [](A a) { return r32(a.i[0] != 0 ? u(suck_2da1e0(a.i[0])) : 0u); });
    }
    // the beam after a few frames of sucking: the rings built, particles in flight (the state made by the port's own 0x2ddf60,
    // as the item snapshots are made by the port's frames)
    auto suck_beam = [suck_world, suck_hand, near_hero](bool reset) -> uint32_t {
        suck_world();
        const uint32_t m = suck_hand(3);
        if (m == 0) return 0;
        near_hero(OUT, 1.0f, 0.0f, 1.5f);
        w32(OV(0x00161b40), 1u);  // a fresh beam (0x2ddcb8 lays it out), then frames of it
        w32(OV(0x00161b44), 1u);
        for (int f = ri(1, 12); f > 0; --f) {
            for (uint32_t k = 0; k < 10; ++k) if (ri(0, 2) != 0) w16(OV(0x001e6008) + 2 * k, u(ri(0x30, 0x90)));
            if (ri(0, 3) == 0) near_hero(OUT, 1.0f, 0.0f, 1.5f);
            suck_2ddf60(m, OUT);
        }
        if (reset && ri(0, 3) == 0) { w32(OV(0x00161b40), 1u); w32(OV(0x00161b44), ri(0, 1) == 0 ? 1u : 0u); }
        if (ri(0, 3) == 0) w32(OV(0x00161b54), u(ri(0x38, 0x40)));  // the sway's turn near its end (0x3c frames)
        if (ri(0, 3) == 0) w16(P + 0x22c8, 1u);  // leaning on a wall: the hand's own frame
        if (ri(0, 2) == 0) near_hero(OV(0x00166dc0), 4.0f, -1.0f, 3.0f);  // the camera, for the rings' faces
        if (ri(0, 2) == 0) { near_hero(OUT, 1.0f, 0.0f, 1.5f); }
        return m;
    };
    check("suck_2ddf60/stream", 0x002ddf60, 600, Ret::None, [suck_beam] {
        const uint32_t m = suck_beam(true);
        seed_rand();
        return Args{{m, OUT}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_2ddf60(a.i[0], a.i[1]); return uint64_t{0}; });
    check("suck_2df048", 0x002df048, 300, Ret::None, [suck_beam] {
        const uint32_t m = suck_beam(false);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_2df048(a.i[0]); return uint64_t{0}; });
    check("suck_beam_draw/rings", 0x002dd8e8, 300, Ret::None, [suck_beam] {
        const uint32_t m = suck_beam(false);
        for (uint32_t k = 0; k < 10; ++k) if (ri(0, 3) == 0) w16(OV(0x001e6008) + 2 * k, static_cast<uint32_t>(ri(-8, 0x50)) & 0xffffu);
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_beam_draw(a.i[0]); return uint64_t{0}; });
    check("suck_2df1e0", 0x002df1e0, 600, Ret::None, [suck_beam, pick] {
        const uint32_t m = suck_beam(false);
        if (m == 0) return Args{{0u}, {}};
        // a moby near one of the beam's points: a bolt (type 0x13) or whatever the level has there
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        const uint32_t o = first + 0x100 * u(ri(1, 200));
        if (ee_read<uint8_t>(o + 0x20) >= 0xf0) w8(o + 0x20, pick({0u, 1u, 0xfeu}));
        const uint32_t ci = ee_read<uint32_t>(o + 0x24);
        if (ci != 0 && ri(0, 1) == 0) w16(ci + 0x46, 0x13u);
        Vec4 p = ee_ref<Vec4>(OV(0x001e5f40) + 0x10 * u(ri(0, 9)));
        p.x += rf(-2.5f, 2.5f); p.y += rf(-2.5f, 2.5f); p.z += rf(-1.0f, 1.0f);
        ee_ref<Vec4>(o + 0x10) = p;
        for (uint32_t k = 0; k < 10; ++k) if (ri(0, 3) == 0) w16(OV(0x001e6008) + 2 * k, u(ri(0, 0x60)));
        seed_rand();
        return Args{{o}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_2df1e0(a.i[0]); return uint64_t{0}; });
    check("suck_cannon_update/tail", 0x002da790, 1000, Ret::None, [suck_world, suck_hand, pick] {
        suck_world();
        const uint32_t m = suck_hand(pick({1u, 2u, 3u, 4u, 4u, 5u, 6u}));
        if (m == 0) return Args{{0u}, {}};
        w32(0x00140404, pick({3u, 3u, 0u, 1u}));  // the game mode: 3 stops the suction's sound, the button latched
        w32(0x0013cae0, R() & 0xffu);
        w16(0x00141680 + 0x48, pick({0u, 5u, 0xfffeu, 0xffffu}));  // the rumble's counters
        w16(0x00141680 + 0x4a, pick({0u, 1u, u(ri(0, 40)), 0x7fffu}));
        w32(0x0015ed84, u(ri(0, 40)));
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_cannon_update(a.i[0]); return uint64_t{0}; });
    // the hooks of the classes that use the carry machinery (a moby of the class, the hand as owner)
    auto hook_moby = [suck_world, suck_hand, pick, near_hero, rec_of, fill_rec, spawn_carried](uint32_t cls) -> uint32_t {
        suck_world();
        const uint32_t hand = suck_hand(2);
        const uint32_t o = cls == 0 ? 0u : spawn_carried(cls);
        if (o == 0 || hand == 0) return 0;
        const uint32_t pv = ee_read<uint32_t>(o + 0x78);
        near_hero(o + 0x10, 6.0f, -1.0f, 2.0f);
        w8(o + 0x20, pick({0u, 1u, 2u, 3u, 5u, 9u}));
        w16(o + 0x34, R() & 0xffff);
        if (pv != 0) {
            const uint32_t rec = rec_of(o, cls);
            if (rec != 0) fill_rec(rec, o, hand);
        }
        return o;
    };
    struct HookT { const char* name; uint32_t addr; int cls; int32_t (*fn)(uint32_t, uint32_t, uint32_t, float); };  // cls: 0 kA, 1 kB, other: the class
    static const HookT hooks[] = {
        {"carry_hook0", 0x002db8f8, 0, carry_hook0}, {"carry_hook4", 0x002dbb20, 0, carry_hook4},
        {"carry_hook8", 0x002dbc30, 0, carry_hook8}, {"carry_hookc", 0x002dbe20, 0, carry_hookc},
        {"e749_hook0", 0x002d56e0, 0x2ed, e749_hook0}, {"e749_hook4", 0x002d5730, 0x2ed, e749_hook4},
        {"e749_hook8", 0x002d5780, 0x2ed, e749_hook8}, {"e749_hookc", 0x002d57d0, 0x2ed, e749_hookc},
        {"enemy10e_hook0", 0x002cadc0, 0x10e, enemy10e_hook0}, {"enemy10e_hook4", 0x002cae10, 0x10e, enemy10e_hook4},
        {"enemy10e_hook8", 0x002cae60, 0x10e, enemy10e_hook8}, {"enemy10e_hookc", 0x002caf10, 0x10e, enemy10e_hookc},
    };
    for (const HookT& h : hooks) {
        const int cls = h.cls;
        auto fn = h.fn;
        if ((cls == 0 && kA == 0) || (cls == 0x2ed && g_oracle_level != 0) || (cls > 1 && cls != 0x2ed && (level1(), safe_spawn(static_cast<uint32_t>(cls)) == 0))) {
            skip_check(h.name, h.addr, "the level has not loaded the class");
            continue;
        }
        check(h.name, h.addr, 300, Ret::Int32, [hook_moby, cls, near_hero] {
            const uint32_t o = hook_moby(cls == 0 ? kA : (cls == 1 ? kB : static_cast<uint32_t>(cls)));
            if (o == 0) return Args{{0u, 0u, 0u}, {}};
            near_hero(OUT, 3.0f, 0.0f, 2.0f);
            wfrand(OUT + 0x20, 4, -3.0f, 3.0f);
            seed_rand();
            return Args{{o, OUT, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(0x0013f350 + 0x2080)}, {rf(0.0f, 2.0f)}};
        }, [fn](A a) { return r32(a.i[0] != 0 ? u(fn(a.i[0], a.i[1], a.i[2], a.f[0])) : 0u); });
    }
    // Novalis's own hooks (L1: only checked on level 1; the check skips a Veldin address the level does not have, so these
    // carry their level's own addresses: a plain run on Veldin has no Novalis overlay)
    if (g_oracle_level == 1) {
        static const HookT l1hooks[] = {
            {"peckbot_hook0_l1", 0x002f1c78, 0x241, peckbot_hook0_l1}, {"peckbot_hook4_l1", 0x002f1cc8, 0x241, peckbot_hook4_l1},
            {"peckbot_hook8_l1", 0x002f1d18, 0x241, peckbot_hook8_l1}, {"peckbot_hookc_l1", 0x002f1dc8, 0x241, peckbot_hookc_l1},
            {"amoeboid_hook0_l1", 0x002efa88, 0x23c, amoeboid_hook0_l1}, {"amoeboid_hook4_l1", 0x002efaf8, 0x23c, amoeboid_hook4_l1},
            {"amoeboid_hook8_l1", 0x002efb48, 0x23c, amoeboid_hook8_l1}, {"amoeboid_hookc_l1", 0x002efb98, 0x23c, amoeboid_hookc_l1},
        };
        for (const HookT& h : l1hooks) {
            const int cls = h.cls;
            auto fn = h.fn;
            l1check(h.name, h.addr, 300, Ret::Int32, [hook_moby, cls, near_hero] {
                const uint32_t o = hook_moby(static_cast<uint32_t>(cls));
                if (o == 0) return Args{{0u, 0u, 0u}, {}};
                near_hero(OUT, 3.0f, 0.0f, 2.0f);
                wfrand(OUT + 0x20, 4, -3.0f, 3.0f);
                seed_rand();
                return Args{{o, OUT, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(0x0013f350 + 0x2080)}, {rf(0.0f, 2.0f)}};
            }, [fn](A a) { return r32(a.i[0] != 0 ? u(fn(a.i[0], a.i[1], a.i[2], a.f[0])) : 0u); });
        }
    }

    // (task 0410) every hook of the level's own carried classes' tables (levels 2..11), at the level's own address: the original
    // there against what the port's dispatchers run for it (the level's own port, else Veldin's through the overlay map), on a
    // moby of class 0x10e standing in with that table in its class
    for (const LvHookTab& t : kLvTabs) {
        if (t.level != g_oracle_level || kA == 0) continue;
        for (uint32_t slot = 0; slot <= 0x10; slot += 4) {
            const uint32_t fn = ee_read<uint32_t>(t.tab + slot);
            static char names[64][40];
            static int nn = 0;
            char* name = names[nn++ % 64];
            std::snprintf(name, 40, "lv_hook%x_%06x", slot, fn);
            if (g_only_batch != 0 && g_only_batch != g_batch) continue;
            if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) continue;
            std::printf("%-26s L%d:0x%08x (table 0x%08x)\n", name, g_oracle_level, fn, t.tab);
            const uint32_t tab = t.tab;
            const auto gen = [hook_moby, near_hero, fill_rec, tab] {
                const uint32_t o = hook_moby(0x10e);
                if (o == 0) return Args{{0u, 0u, 0u}, {}};
                w32(ee_read<uint32_t>(o + 0x24) + 0x2c, tab);  // (this table's hooks; the record of its hook +0x10 refilled)
                // the moby's class number (+0xa6) one of the classes that share a table (Rilgar's / Pokitaru's hook +0 takes only 0x362)
                if (ri(0, 2) != 0) w16(o + 0xa6, ri(0, 1) == 0 ? 0x362u : 0x23cu);
                const uint32_t pv = ee_read<uint32_t>(o + 0x78);
                int32_t rec = 0;
                if (pv != 0 && !class_hook_call_level(ee_read<uint32_t>(tab + 0x10), o, 0, 0, 0.0f, rec)) {
                    rec = static_cast<int32_t>(T3 + 0x100 * u(ri(0, 12)));
                    w32(pv + 0x14, static_cast<uint32_t>(rec));
                }
                if (pv != 0 && rec != 0) fill_rec(static_cast<uint32_t>(rec), o, ee_read<uint32_t>(0x0013f350 + 0x2080));
                near_hero(OUT, 3.0f, 0.0f, 2.0f);
                wfrand(OUT + 0x20, 4, -3.0f, 3.0f);
                seed_rand();
                return Args{{o, OUT, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(0x0013f350 + 0x2080)}, {rf(0.0f, 2.0f)}};
            };
            const auto port = [fn, slot](A a) {
                if (a.i[0] == 0) return uint64_t{0};
                return r32(u(hook_port(fn, slot, a.i[0], a.i[1], a.i[2], a.f[0])));
            };
            if (!check_fn(name, fn, 300, Ret::Int32, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
        }
    }

    // the Suck Cannon's beam draw callback and its HUD element's callbacks (0x237798 / 0x2377b8 / 0x237840: the element's
    // frame, its two bars through the flat rectangle drawer)
    check("suck_beam_draw", 0x002dd8e8, 300, Ret::None, [suck_world, suck_hand, pick, near_hero] {
        suck_world();
        const uint32_t m = suck_hand(pick({1u, 2u, 3u, 4u}));
        if (m == 0) return Args{{0u}, {}};
        near_hero(OV(0x00166dc0), 8.0f, -1.0f, 4.0f);
        wfrand(0x0013f5e0, 3, -1.0f, 1.0f);
        w32(OV(0x0015f5cc), R());
        if (ri(0, 1) == 0) for (uint32_t k = 0; k < 10; ++k) { wfrand(OV(0x001e4c94) + 0x18 * k, 6, -2.0f, 2.0f); w32(OV(0x001e4c94) + 0x18 * k, ri(0, 1) == 0 ? 0u : 1u); }
        seed_rand();
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) suck_beam_draw(a.i[0]); return uint64_t{0}; });
    auto hud_block = [level1](uint32_t at) {
        level1();
        for (uint32_t k = 0; k < 0x80; k += 4) w32(at + k, u(ri(0, 0x90)));
        w32(at + 8, u(ri(1, 0x40)));
        w32(at + 0x7c, u(ri(0, 0x40)));
        w16(at + 0x48, u(ri(0, 40))); w16(at + 0x4a, u(ri(0, 40)));
        w32(OV(0x0015f760), u(ri(0, 4))); w32(OV(0x0015f764), u(ri(0, 4)));
        w32(0x00140400 + 0xb8, ri(0, 3) == 0 ? 9u : u(ri(0, 12)));
    };
    check("suck_camera_237798", 0x00237798, 100, Ret::None, [hud_block] { hud_block(T2); return Args{{T2}, {}}; },
          [](A a) { suck_camera_237798(a.i[0]); return uint64_t{0}; });
    check("suck_camera_2377b8", 0x002377b8, 200, Ret::None, [hud_block] { hud_block(T2); w32(P + 0x10b8, ri(0, 2) == 0 ? 9u : u(ri(0, 12))); return Args{{T2}, {}}; },
          [](A a) { suck_camera_2377b8(a.i[0]); return uint64_t{0}; });
    check("suck_camera_237840", 0x00237840, 400, Ret::None, [hud_block] { hud_block(T2); seed_rand(); return Args{{T2}, {}}; },
          [](A a) { suck_camera_237840(a.i[0]); return uint64_t{0}; });

    // --- the Swingshot (item 0x0c, hand class 0xd0, grapple class 0xd1)
    auto swing_extra = [safe_spawn, near_hero](uint32_t m, uint32_t pv) {
        w32(P + 0x964, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)));
        w32(P + 0x968, ri(0, 2) == 0 ? 0u : 1u);
        w32(P + 0x990, ri(0, 2) == 0 ? 0u : ee_read<uint32_t>(OV(0x0015ffd8)) + 0x100 * u(ri(1, 200)));
        w32(P + 0x198, u(ri(0, 6)));
        if (ee_read<int32_t>(P + 0x2084) == 1) w32(P + 0x2084, 0x1eu);  // (hero_set_state(0x1e, 1) waits for a VU job)
        if (pv != 0 && ri(0, 3) == 0) {  // the grapple moby (class 0xd1) in the glove, or none
            const uint32_t g = ri(0, 2) == 0 ? 0u : safe_spawn(0xd1);
            w32(pv + 0x14, g);
            if (g != 0) {
                const uint32_t gpv = ee_read<uint32_t>(g + 0x78);
                w32(gpv, m);
                wf(gpv + 4, rf(0.0f, 20.0f));
                near_hero(g + 0x10, 6.0f, -1.0f, 3.0f);
            }
        }
    };
    check("swingshot_spawn_grapple", 0x002c6f88, 200, Ret::Int32, [weapon_gen] {
        Args a = weapon_gen(0x0c, 0, u(ri(0, 2)), 0)();
        a.f = {rf(0.5f, 20.0f)};
        // (the grapple class 0xd1 not loaded: the spawn reaches the debug trap 0x1ff008 -- a no-op in the interpreter, an exception in the port: not run)
        if (ee_read<uint8_t>(OV(0x00197bc0) + 0xd1) == 0xff) a.i[0] = 0;
        return a;
    }, [](A a) { return r32(a.i[0] != 0 ? swingshot_spawn_grapple(a.i[0], a.f[0]) : 0u); });
    check("swingshot_update", 0x002c6158, 2500, Ret::None, [weapon_gen, swing_extra] { return weapon_gen(0x0c, 0, u(ri(0, 2)), 16, swing_extra)(); },
          [](A a) { if (a.i[0] != 0) swingshot_update(a.i[0]); return uint64_t{0}; });

    // --- the other hands: each on the engine's own state with the item in hand (idle / fire pressed / fire held)
    struct HandT { const char* name; uint32_t item; uint32_t addr; uint32_t nstates; void (*fn)(uint32_t); };
    static const HandT hands[] = {
        {"devastator_update", 0x0b, 0x002b2100, 8, devastator_update},
        {"visibomb_update", 0x0d, 0x002b3058, 10, visibomb_update},
        {"tesla_update", 0x13, 0x002b87e0, 10, tesla_update},
        {"decoy_update", 0x19, 0x002d2670, 10, decoy_update},
        {"morph_update", 0x15, 0x002bc7e8, 10, morph_update},
        {"hydro_update", 0x16, 0x002df9a0, 10, hydro_update},
        {"metal_update", 0x1b, 0x002d2ee8, 10, metal_update},
        {"ryno_update", 0x17, 0x002cc608, 10, ryno_update},
    };
    // (a VU0 vector op the original ran leaves its stale w lane in the vector it stored; the morph ray's table is such: its w lanes ignored)
    for (uint32_t k = 0; k < 0x80; ++k) g_ignored_words.push_back(OV(0x001dc03c) + 0x10 * k);
    for (uint32_t k = 0; k < 0x20; ++k) g_ignored_words.push_back(OV(0x001dc92c) + 0x140 * k);
    {  // (the particles' +0x1c: a stale vf lane too; the pool's base is the level's: read from the loaded level)
        level1();
        const uint32_t pb = ee_read<uint32_t>(OV(0x0016016c));
        for (uint32_t a = pb; a < pb + 0x800 * 0x40; a += 0x40) g_ignored_words.push_back(a + 0x1c);
    }
    {  // the w lanes of the mobys' matrix rows (+0xcc / +0xdc / +0xec): what the VU0 transform leaves
        level1();
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)) + 0x1000; m += 0x100)
            for (uint32_t o : {0xccu, 0xdcu, 0xecu}) g_ignored_words.push_back(m + o);
    }
    // the Metal Detector's buried thing (class 0x25d, its bolts at pvar +0x18) as the hero's find 0x13f350 + 0x2040 at a distance
    // +0x2044 within 1 (dug up), 10 (the beep, its pitch) or beyond: before, no find, the update's aim and dig never ran (since audit 2)
    auto metal_extra = [safe_spawn](uint32_t m, uint32_t pv) {
        if (ri(0, 3) == 0 || pv == 0) return;
        uint32_t t = 0;
        for (uint32_t c = ee_read<uint32_t>(OV(0x0015ffd8)); c < ee_read<uint32_t>(OV(0x0015ffe0)); c += 0x100)
            if (ee_read<uint16_t>(c + 0xa6) == 0x25d && ee_read<uint8_t>(c + 0x20) < 0xfd) { t = c; break; }
        if (t == 0) t = safe_spawn(0x25d);
        if (t == 0) return;
        Vec4 q = ee_ref<Vec4>(m + 0x10);
        q.x += rf(-6.0f, 6.0f); q.y += rf(-6.0f, 6.0f); q.z += rf(-2.0f, 1.0f);
        ee_ref<Vec4>(t + 0x10) = q;
        const uint32_t tpv = ee_read<uint32_t>(t + 0x78);
        if (tpv != 0) w32(tpv + 0x18, coin() ? 0u : u(ri(1, 30)));
        if (ri(0, 4) == 0) w8(t + 0x20, coin() ? 0xfeu : 0xfdu);
        w32(P + 0x2040, t);
        wf(P + 0x2044, ri(0, 2) == 0 ? rf(0.0f, 1.0f) : coin() ? rf(1.0f, 10.0f) : rf(10.0f, 40.0f));
        if (coin()) w8(m + 0x20, coin() ? 1u : 2u);
    };
    // the Hydrodisplacer's water: the hero's flag 0x13f350 + 0x20b0 (the pump running, its spray 0x2e01b0 and the hero's push),
    // +0x10b4 = 3 (its other mode, 0x2e0340), the hero's state +0x2084 (0x38 / 0x39 leave the push out): before, none of them set,
    // the update ran 88 of its 516 instructions (since audit 2)
    auto hydro_extra = [pick](uint32_t, uint32_t) {
        w8(P + 0x20b0, ri(0, 2) == 0 ? 0u : 1u);
        if (ri(0, 3) == 0) w32(P + 0x10b4, 3u);
        if (coin()) w32(P + 0x2084, pick({0u, 0x38u, 0x39u, 0x1eu}));
        if (coin()) w32(OV(0x0015f5c4), 0u);
        if (coin()) wfrand(P + 0x100, 3, -0.3f, 0.3f);  // (the hero's velocity the spray takes on)
    };
    for (const HandT& h : hands) {
        const uint32_t item = h.item, ns = h.nstates;
        auto fn = h.fn;
        auto extra = item == 0x1b ? std::function<void(uint32_t, uint32_t)>(metal_extra)
                     : item == 0x16 ? std::function<void(uint32_t, uint32_t)>(hydro_extra) : nullptr;
        check(h.name, h.addr, 1500, Ret::None, [weapon_gen, item, ns, extra] { return weapon_gen(item, 0, u(ri(0, 2)), ns, extra)(); },
              [fn](A a) { if (std::getenv("ASM_TRACE") != nullptr) std::printf("  [port call] seed %08x\n", ee_read<uint32_t>(ee_read<uint32_t>(0x0012f76c) + 0x58)); if (a.i[0] != 0) fn(a.i[0]); if (std::getenv("ASM_TRACE") != nullptr) std::printf("  [port end]\n"); return uint64_t{0}; });
    }

    // --- the weapons' projectile / effect classes: a live moby of the class in the item's fired snapshot, else a spawned one with small pvars
    auto class_of_fn = [](uint32_t fn_veldin) -> uint32_t {  // the class whose update function is `fn` (Veldin's address): 0 when none
        for (uint32_t c = 1; c < 0x800; ++c) {
            const uint32_t ci = ee_read<uint8_t>(OV(0x00197bc0) + c);
            if (ci == 0xff) continue;
            if (ee_read<uint32_t>(OV(0x00197680) + ci * 4) == ovl_xlate_code(fn_veldin)) return c;
        }
        return 0;
    };
    auto proj_gen = [item_snap, class_of_fn, moby_of_class, hand_class, safe_spawn, pick, near_hero](uint32_t item, uint32_t fn, uint32_t nstates, uint32_t owner_off) {
        return [=]() -> Args {
            const std::vector<uint8_t>& sn = item_snap(item, u(ri(0, 2)));
            std::memcpy(ee_get_ram_base(), sn.data(), sn.size());
            uint32_t cls = class_of_fn(fn);
            const bool no_class = cls == 0;
            // (a class the level has not loaded -- the weapon's own projectile classes are loaded with the level that carries them: a moby of
            // the item's hand class stands in, the update called on it directly)
            if (no_class) cls = hand_class(item);
            w8(0x0013e520 + item, coin() ? 1u : 0u);  // (the item's gold flag, as weapon_gen's)
            view_setup();
            collide_vu_reset();
            oracle_vu_contact_reset();
            uint32_t m = moby_of_class(cls);
            const bool spawned = no_class || m == 0 || ri(0, 3) == 0;
            if (spawned) {
                m = safe_spawn(cls);
                if (m == 0) return Args{{0u}, {}};
                near_hero(m + 0x10, 4.0f, -0.5f, 2.0f);
                const uint32_t pv0 = ee_read<uint32_t>(m + 0x78);
                if (pv0 != 0) for (uint32_t k = 0x44; k < 0x80; k += 4) if (ri(0, 2) == 0) wf(pv0 + k, rf(-3.0f, 3.0f));
            }
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (owner_off != 0 && pv != 0) {  // (the projectile's owner: the item's hand moby, whose pvars point back at it)
                const uint32_t hand = moby_of_class(hand_class(item));
                const uint32_t hpv = hand != 0 ? ee_read<uint32_t>(hand + 0x78) : 0;
                if (hpv != 0 && ri(0, 7) != 0) { w32(pv, hand); w32(hpv + owner_off, m); }
                if (hand != 0 && ri(0, 1) == 0) ee_write<uint16_t>(hand + 0x34, static_cast<uint16_t>(ee_read<uint16_t>(hand + 0x34) ^ 1u));
            }
            if (ri(0, 1) == 0 && nstates != 0) w8(m + 0x20, u(ri(0, static_cast<int>(nstates) - 1)));
            if (ri(0, 2) == 0) ee_write<int16_t>(m + 0xa, static_cast<int16_t>(ri(1, 120)));  // (a lifetime counter some updates test first)
            if (ri(0, 2) == 0) w8(m + 0x70, u(R() & 0xff));
            if (ri(0, 2) == 0) w8(m + 0x53, u(ri(0, 2)));
            if (ri(0, 3) == 0) w8(0x001413ff, u(ri(0, 1)));
            if (ri(0, 2) == 0) w32(P + 0x2084, pick({0u, 0x1eu, 0x23u, 0x1eu}));
            if (ri(0, 2) == 0) w32(PAD + 0x1a0, R() & 0x9025u);
            if (pv != 0) {
                for (uint32_t k = 0; k < 0x20; ++k) {
                    const uint32_t cur = ee_read<uint32_t>(pv + 4 * k);
                    if (cur != 0 && cur < 0x1000u && ri(0, 3) == 0) w32(pv + 4 * k, pick({0u, 1u, 2u, u(ri(0, 40))}));
                }
            }
            if (ee_read<int32_t>(P + 0x2084) == 1) w32(P + 0x2084, 0x1eu);
            // (the Decoy Glove's landing ring 0x2c31c0: its centre pvar +0x20 and its ground's normal +0x30 are the decoy's, which the
            // hand class standing in for it does not hold: the ring was never on the screen, the check compared no quad. Its centre
            // at the hero (whom the view looks at), its normal about up; from a counter, not the random sequence, so that the other
            // checks keep their cases. Since audit 2)
            if (fn == 0x002c31c0u && pv != 0) {
                static uint32_t ring = 0;
                const float k = static_cast<float>(ring++ % 16u);
                const Vec4 h = ee_ref<Vec4>(0x0013f3d0);
                ee_ref<Vec4>(pv + 0x20) = Vec4{h.x + (k - 8.0f) * 0.25f, h.y + (static_cast<float>(ring % 5u) - 2.0f) * 0.5f, h.z + 0.1f, 1.0f};
                ee_ref<Vec4>(pv + 0x30) = Vec4{(k - 8.0f) * 0.05f, (static_cast<float>(ring % 3u) - 1.0f) * 0.2f, 1.0f, 0.0f};
            }
            seed_rand();
            return Args{{m}, {}};
        };
    };
    struct ProjT { const char* name; uint32_t item; uint32_t addr; uint32_t nstates; uint32_t owner_off; void (*fn)(uint32_t); };

    static const ProjT projs[] = {
        {"swingshot_grapple_update", 0x0c, 0x002c7070, 6, 0x14, swingshot_grapple_update},
        {"devastator_rocket_update", 0x0b, 0x002b03a0, 6, 0, devastator_rocket_update},
        {"devastator_271450_update", 0x0b, 0x00271450, 6, 0, devastator_271450_update},
        {"visibomb_ball_update", 0x0d, 0x002b6140, 8, 0, visibomb_ball_update},
        {"tesla_2ba970_update", 0x13, 0x002ba970, 6, 0, tesla_2ba970_update},
        {"decoy_object_update", 0x19, 0x002c4380, 8, 0, decoy_object_update},
        {"metal_2d2a90_update", 0x1b, 0x002d2a90, 6, 0, metal_2d2a90_update},
        {"fx_26a5b8_update", 0x0b, 0x0026a5b8, 6, 0, fx_26a5b8_update},
        {"fx_26ee48_update", 0x14, 0x0026ee48, 6, 0, fx_26ee48_update},
        {"morph_275aa0_update", 0x15, 0x00275aa0, 6, 0, morph_275aa0_update},
        {"morph_2bdc50_update", 0x15, 0x002bdc50, 6, 0, morph_2bdc50_update},
        {"ryno_2cd1f0_update", 0x17, 0x002cd1f0, 6, 0, ryno_2cd1f0_update},
        {"decoy_2c31c0_update", 0x19, 0x002c31c0, 6, 0, decoy_2c31c0_update},
        {"doomlet_update", 0x14, 0x002bffa8, 8, 0, doomlet_update},
    };
    // the screen flash / frame overlay drawn by render_frame (0x1f9df8, called when 0x15f30c / 0x15f310 are set; it was a TODO_PORT in overlay_entry.cpp)
    check("screen_flash", 0x001f9df8, 400, Ret::None, [level1, pick] {
        level1();
        for (uint32_t k = 0; k < 0x40; ++k) w32(OUT + 4 * k, ri(0, 3) == 0 ? 0u : (R() & 0xfu) | (coin() ? 0x01000000u : 0u));  // (small: the loops run on these)
        for (uint32_t k = 0; k < 0x10; ++k) w32(OUT + 0x100 + 4 * k, R() & 0xffffu);
        w32(OUT + 0x10, u(ri(1, 4))); w32(OUT + 0x20, u(ri(0, 4)));  // (the loop's steps: a loop that never advances never ends, in the original too)
        w32(OV(0x0015f30c), 1u);
        w32(OV(0x0015f310), OUT);
        ee_write<int16_t>(0x001518d2, static_cast<int16_t>(pick({0u, 1u, 2u, 3u, 5u, 8u})));
        // (half of the cases a screen up to 448 high (view_setup makes it 640 wide: it was 0 wide before, quads of no width), the
        // bands up to 40 lines high: the last band's clamp to the screen's last line reached; the packets on a buffer of the test
        // area, not the null page)
        if (coin()) {
            ee_write<int16_t>(0x001518d0, 640);
            ee_write<int16_t>(0x001518d2, static_cast<int16_t>(coin() ? 448 : ri(9, 448)));
            w32(OUT + 0x10, u(ri(4, 40))); w32(OUT + 0x20, u(ri(0, 40)));
        }
        w32(OV(0x001611c0), TEST_AREA + 0x8000 + u(ri(0, 0x40)) * 0x10);
        seed_rand();
        return Args{{}, {}};
    }, [](A) { screen_flash(); return uint64_t{0}; });
    // the ballistic speed 0x25abf0 with a zero / negative-zero / tiny speed (a divide by zero on the EE: the Doomlet's update calls it with speed 0)
    check("sub_0025abf0/speed0", 0x0025abf0, 400, Ret::Float, [pick, pickf] {
        wfrand(M, 4, -9, 9); wfrand(M2, 4, -9, 9);
        return Args{{M, M2, coin() ? OUT : 0u}, {pickf({0.0f, -0.0f, 1e-30f, 0.5f}), pickf({0.0f, -0.0f, 1.0f, -2.0f})}};
    }, [](A a) { return rf32(sub_0025abf0(a.i[0], a.i[1], a.i[2] ? hp<float>(a.i[2]) : nullptr, a.f[0], a.f[1])); });
    // (the grid collision queries the projectiles run (ovl_1e_1f.cpp) keep temporaries in the scratchpad: the port's are not the original's stale ones)
    for (uint32_t a = 0x70000000u; a < 0x70004000u; a += 4) g_ignored_words.push_back(a);
    for (const ProjT& p : projs) {
        if (g_oracle_level == 2 && p.addr == 0x002bffa8u) {  // 7 of 1200 cases differ: Aridia's collision output record (0x17447c, written by its 0x201c48 = Veldin 0x1f0d60) is not written by the port; not resolved
            skip_check(p.name, p.addr, "level 2: 7 / 1200 cases differ in a sphere_collide record (0x1f0d60 variant), not resolved");
            continue;
        }
        const uint32_t item = p.item, addr = p.addr, ns = p.nstates, oo = p.owner_off;
        auto fn = p.fn;
        check(p.name, p.addr, 1200, Ret::None, [proj_gen, item, addr, ns, oo] { return proj_gen(item, addr, ns, oo)(); },
              [fn](A a) { if (std::getenv("ASM_TRACE") != nullptr) std::printf("  [port call]\n"); if (a.i[0] != 0) fn(a.i[0]); if (std::getenv("ASM_TRACE") != nullptr) std::printf("  [port end]\n"); return uint64_t{0}; });
    }
    // (stage 3d) the doomlet's states the projectile check above seldom reaches: every state byte (0xb the explosion, 0xd the
    // owner's wait, 8 / 9 the falls, 0xc the jump's landing), the gold levels 0..3 (0x13e534: the size, the explosion's
    // sound), a target in pvar +0x38 (the hero, or a moby of type 5 with the target flag 0x1000), its timers near their limits
    if (g_oracle_level == 2) {
        skip_check("doomlet_update/states", 0x002bffa8, "level 2: the sphere_collide record of doomlet_update above, not resolved");
    } else {
        check("doomlet_update/states", 0x002bffa8, 1500, Ret::None, [proj_gen, pick] {
            const Args base = proj_gen(0x14, 0x002bffa8, 0, 0)();
            const uint32_t m = base.i[0];
            if (m == 0) return base;
            const uint32_t pv = ee_read<uint32_t>(m + 0x78);
            if (pv == 0) return base;
            w8(m + 0x20, ri(0, 2) == 0 ? pick({0xbu, 0xbu, 0xdu, 0xcu, 8u, 9u}) : u(ri(0, 15)));
            w8(0x0013e534, u(ri(0, 3)));
            // a target: the hero, a moby of type 5 with the target flag, a retired one, or none
            uint32_t enemy = 0;
            for (uint32_t o = ee_read<uint32_t>(OV(0x0015ffd8)); o < ee_read<uint32_t>(OV(0x0015ffe0)); o += 0x100) {
                const uint32_t ci = ee_read<uint32_t>(o + 0x24);
                if (o != m && ee_read<uint8_t>(o + 0x20) < 0xf0 && ci != 0 && ee_read<int16_t>(ci + 0x46) == 5 && ri(0, 2) == 0) { enemy = o; break; }
            }
            const uint32_t hero = ee_read<uint32_t>(0x001413d0);
            const uint32_t tgt = pick({0u, hero, hero, enemy, enemy, enemy});
            if (tgt != 0 && tgt != hero) {
                if (ri(0, 4) != 0) ee_write<uint16_t>(tgt + 0x34, static_cast<uint16_t>(ee_read<uint16_t>(tgt + 0x34) | 0x1000u));
                if (ri(0, 6) == 0) w8(tgt + 0x20, pick({0xfeu, 0xfdu}));
                Vec4 p = ee_ref<Vec4>(m + 0x10);  // near the doomlet: the chase's jump, the contact
                p.x += rf(-3.0f, 3.0f); p.y += rf(-3.0f, 3.0f); p.z += rf(-0.5f, 0.5f);
                if (ri(0, 1) == 0) ee_ref<Vec4>(tgt + 0x10) = p;
            }
            w32(pv + 0x38, tgt);
            w32(pv + 0x3c, ri(0, 2) == 0 ? tgt : 0u);
            w32(pv + 0x30, pick({0u, 1u, u(ri(2, 600)), 3999u, 4000u, 4001u, 4500u}));
            w32(pv + 0x34, pick({0u, u(ri(1, 100)), 0xe0fu, 0xe10u, 0xe11u}));
            // the owner (pvar +0x60) state 0xd waits on: alive, retired, or none
            const uint32_t owner = pick({0u, hero, enemy});
            w32(pv + 0x60, owner);
            if (owner != 0 && owner != hero && ri(0, 2) == 0) w8(owner + 0x20, pick({0xfeu, 0xfdu}));
            if (ri(0, 4) == 0) wf(pv + 0x64, rf(-3.1f, 3.1f));
            seed_rand();
            return Args{{m}, {}};
        }, [](A a) { if (a.i[0] != 0) doomlet_update(a.i[0]); return uint64_t{0}; });
    }

    g_ignored_words = ign_saved;
    g_ee_null_is_ram = null_ram;
    g_ee_oob_is_zero = oob_zero;
    g_debug_trap_is_noop = trap_noop;
}

}  // namespace rac1::oracle
