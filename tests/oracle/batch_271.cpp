// Oracle batch 271 (levels 17 / 18 port, 0.3.0): what the level start's checks of batches 260 / 270 cannot reach, and the end of the game.
// Run with ORACLE_LEVEL=17 / ORACLE_LEVEL=18 build/tests/ovl_oracle --batch 271 (the level's own original against the port; needs the ISO).
//  - the Gold Bolts (class 1134, levels 17 and 18) in their states 0 / 1 / 2 and the Codebot (class 1428, level 17) in its states 0 / 2: they ask
//    for a level change (0x24d430: the autosave, the console clock), which batches 260 / 270 stub on the original's side; here it runs on both
//    sides, the clock / the disc the host's on both (as batches 138 / 141);
//  - HUD slot 7's draw, each level's own (L17:0x2355b8, L18:0x23d1a8), on the element the hero code requests (the Metal Detector's, a frame
//    played);
//  - class 1249's (Drek, the NPC model) and the end menu's moby's empty updates (Veldin's 0x2df998 / 0x283900);
//  - on RAM dumps of level 17 (p17_1002 / p17_1005 / p17_1009): particle types 0x47 / 0x4f's updates (Veldin's 0x274e48 / 0x275d68,
//    particle_types4_tr.cpp) on the live particles of the types (the respawn's effect);
//  - on RAM dumps of level 18 (tools/oracle/drek_veldin_tour.sh dumps: assets/dumps/oracle/drek_veldin/*.bin; each check is skipped without its
//    dump):
//      fight5000 / end18000: the level's own classes alive there (as the game left them; class 331, the Trackdrive, is spawned after the
//      level's start);
//      fight4200 / fight5000 / fight6000: the final boss (class 1422, L18:0x2f2bf0) in its fight, in the states the game left and in its other
//      states (the pointers its earlier states set are there: the level start's checks skip its states 9 .. 11), and the draw function of the
//      HUD element it adds (its health bar, L18:0x23cd60);
//      end5700 / end9000: the credits (game mode 7, Veldin's code shared by every level: credits_tr.cpp, 0x299c48 update / 0x1f8f78 draw;
//      the start 0x299a68 on the fight's dump), their pages' counters varied;
//      end18000 / end18300: the menu at the end of the game (menu state 0x21: end_menu_tr.cpp, Veldin's code) and the page of its state
//      0x23: every widget function of the two pages, on the pages' own widgets, the pad varied.
// Dumps (recorded with this port, on the NTSC-U disc; the code that leads to them is proved by batches 260 / 270 and this one):
//   tools/oracle/drek_veldin_tour.sh dumps   (writes the nine dumps; see the script's header for the scenario of each)

#include "ovl_oracle.h"

#include <game/lib/libcdvd.h>
#include <game/lib/libmc.h>
#include <game/ovl/level_change.h>
#include <runtime/kernel/disc.h>
#include <game/ovl/credits_tr.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/end_menu_tr.h>
#include <game/ovl/hud_slot7_tr.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_types4_tr.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_orbit_mobys.h>

#include <csetjmp>
#include <csignal>
#include <cmath>
#include <cstring>
#include <map>
#include <string>

namespace rac1::oracle {

namespace {
// A check whose original is level `lvl`'s own function at `laddr` (not a Veldin address to translate).
void lcheck(int lvl, const char* name, uint32_t laddr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != lvl) {
        std::printf("%-26s L%d:0x%08x  skipped (level %d only: run with ORACLE_LEVEL=%d)\n", name, lvl, laddr, lvl, lvl);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L%d:0x%08x\n", name, lvl, laddr);
    if (!check_fn(name, laddr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// a RAM dump of a headless run (32 MB + the scratchpad), empty when the file is missing
const std::vector<uint8_t>& dump(const std::string& path) {
    static std::map<std::string, std::vector<uint8_t>> cache;
    std::vector<uint8_t>& d = cache[path];
    if (d.empty()) {
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            d.resize(0x02000000 + 0x4000);
            if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
            std::fclose(f);
        }
    }
    return d;
}
void load_dump(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
// A forced state that sends the port's code through a pointer outside RAM (a pointer the boss's earlier states of another phase set: the
// interpreter wraps it, the port refuses) is not a state of the fight the dump holds: another is dealt (as batch 124's trial runs)
sigjmp_buf g_trial_jmp;
void trial_segv(int) { siglongjmp(g_trial_jmp, 1); }
bool trial_run(void (*port)(uint32_t), uint32_t m) {
    struct sigaction sa {}, old {};
    sa.sa_handler = trial_segv;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &sa, &old);
    bool ok = true;
    const uint32_t stack_top = ee_stack_ptr();
    if (sigsetjmp(g_trial_jmp, 1) != 0) ok = false;
    else {
        try { port(m); } catch (const std::exception&) { ok = false; }
    }
    ee_stack_ptr() = stack_top;
    sigaction(SIGSEGV, &old, nullptr);
    return ok;
}
}  // namespace

namespace {
// The end menu's widgets in the states the dumps do not hold (each dump has one: the panel's model loaded, the epilogue at one
// step), so that every branch of the functions is compared: before, the panel's update changed one word (end18000) or none
// (end18300) in every case, the page's update / draw one step each, its focus-in nothing (its words already 0).
void end_menu_vary(uint32_t fn, uint32_t w) {
    switch (fn) {
        case 0x00283530: {  // the panel's update: its state +0x34, the moby's state +0x20 (0 .. 7, the table 0x1e9350's cases),
                            // its counter +0xbc, the dialogue's frames +0x52 / +0x53, its animation's end flag +0x70, the menu state
            const uint32_t m = ee_read<uint32_t>(w + 0x44);
            // (states 0 / 1 read the model from the disc and unpack it: the IOP's read, not run here; 2 the script, 3 failed)
            w32(w + 0x34, ri(0, 5) == 0 ? 3u : 2u);
            if (m >= 0x00100000u && m < 0x02000000u) {
                w8(m + 0x20, ri(0, 5) == 0 ? u(ri(7, 8)) : u(ri(0, 6)));
                static const uint32_t counts[] = {0, 0, 0, 1, 0x76, 0x77, 0x78, 0x79, 0xee, 0xef, 0xf0, 0xff};
                w8(m + 0xbc, ri(0, 5) == 0 ? u(ri(0, 0xff)) : counts[ri(0, 11)]);  // (the counter about its limits 0x78 / 0xf0)
                w8(m + 0x52, u(ri(0, 2)));
                w8(m + 0x53, u(ri(0, 2)));
                w8(m + 0x70, u(ri(0, 3)));
            }
            w16(0x0015172a, u(ri(0, 7)));
            w32(0x0015ee20, u(ri(0, 1)));
            w32(0x0015ed88, u(ri(0, 6)));
            break;
        }
        case 0x0027fed8:  // the choice's update: the memory card's state 0x15eeb0 (1 / 0x10: a save to make first) under cross
            w32(0x0015eeb0, coin() ? (coin() ? 1u : 0x10u) : u(ri(0, 0x18)));
            if (coin()) w32(0x0013cae4, coin() ? 0x20u : 0x40u);
            break;
        case 0x00280020:  // the page's focus-in: what it clears
            wrand(w + 0x3c, 2);
            w32(w + 0x50, u(ri(0, 20)));
            break;
        case 0x00280038:    // the page's update: its step +0x50 (0 .. 19, beyond), timer +0x40, the end of its scroll +0x54, the
        case 0x002803b0: {  // cheats' flag 0x1b9dcc and the root page's word +0x124; the draw: the step's text (a list or one id)
            // (the widget's box laid out as a shown page's (+0x18 x, +0x1c y, +0x20 width, +0x24 height): the dumps' page is not
            // shown, its box 0 x 0, where every character is a line and a text of more than 32 lines runs past the original
            // text box's line arrays (0x1fbf48, batch 67's note) into its caller's frame: a state the game never reaches)
            w32(w + 0x18, u(ri(0x20, 0x60)));
            w32(w + 0x1c, u(ri(0x30, 0x80)));
            w32(w + 0x20, u(ri(0x160, 0x1c0)));
            w32(w + 0x24, u(ri(0xc0, 0x150)));
            const uint32_t step = ri(0, 7) == 0 ? u(ri(20, 22)) : ri(0, 5) == 0 ? 19u : u(ri(0, 19));  // (19: the epilogue's end)
            w32(w + 0x50, step);
            w32(w + 0x40, coin() ? 0u : u(ri(1, 300)));
            w32(w + 0x54, u(ri(0, 1)));
            w32(w + 0x3c, u(ri(0, 0x800)));
            w32(OV(0x001b9dcc), u(ri(0, 1)));
            w32(OV(0x001b9cf0) + 0x124, u(ri(0, 1)));
            w32(OV(0x001b9cf0) + 0xdc, u(ri(0, 1)));
            if (ri(0, 2) == 0) w32(0x0013cae0, coin() ? 0xfu | (R() & 0x10u) : R() & 0xffu);  // (the shoulders held: the fast scroll)
            if (ri(0, 2) == 0) w32(0x0013c940 + 0x1c4, coin() ? 0x10u : 0xd00u);  // (triangle: back; start / select: skip)
            static const uint32_t lists[] = {0x001b9198, 0x001b9248, 0x001b9308, 0x001b9398, 0x001b93b0};
            static const uint32_t ids[] = {0x50d5, 0x5137, 0x5143, 0x5175, 0x50a9};
            if (step < 20) {  // (the draw's case of the step, read from the game's table 0x1e9300: a list, one text, nothing)
                const uint32_t c = ovl_code_to_veldin(ee_read<uint32_t>(OV(0x001e9300) + step * 4));
                if (c == 0x00280478u) w32(w + 0x34, ovl_xlate_data(lists[ri(0, 4)]));
                else if (c == 0x00280538u) w32(w + 0x34, ids[ri(0, 4)]);
            }
            if (ri(0, 3) == 0) w32(w + 0x34, ovl_xlate_data(lists[ri(0, 4)]));
            break;
        }
        default:
            break;
    }
}
}  // namespace

void batch_271() {
    if (g_only_batch != 271) return;
    g_batch = 271;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    g_ignored_words.push_back(OV(0x00160188));  // (the particle update's saved return address)
    g_ignored_words.push_back(OV(0x0016018c));
    for (uint32_t a = 0; a < 0x100; a += 4) g_ignored_words.push_back(a);  // (the null page: a stale pointer of a random state)
    g_extra_noop_hooks.clear();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime
    // the console clock / the disc reads the originals poll (IOP calls): the host's on both sides, as batches 138 / 141 do
    const std::time_t saved_clock = g_host_clock_fixed;
    g_host_clock_fixed = 1759658400;
    g_extra_hooks = {{0x00121780, [](EeCpu& c) { c.set32(2, cd_read_clock(c.r32(4))); }},
                     {0x0012d6d8, [](EeCpu& c) { clock_to_local(c.r32(4)); }},
                     {0x00121450, [](EeCpu& c) { c.set32(2, cd_read(c.r32(4), c.r32(5), c.r32(6), c.r32(7))); }},
                     // (the end menu's new game: the disc's default save read by the host on both sides, the card's calls the host's, as batch 85)
                     {OVF(0x00266040), [](EeCpu& c) { c.set32(2, disc_read_wait(c.r32(4), c.r32(5), c.r32(6))); }},
                     {0x00121630, [](EeCpu& c) { c.set32(2, cd_get_error()); }},
                     {0x001216c8, [](EeCpu& c) { c.set32(2, cd_break()); }},
                     {0x00123d68, [](EeCpu& c) { c.set32(2, mc_get_info(static_cast<int32_t>(c.r32(4)), static_cast<int32_t>(c.r32(5)), c.r32(6), c.r32(7), c.r32(8))); }},
                     {0x00123c30, [](EeCpu& c) { c.set32(2, mc_sync(static_cast<int32_t>(c.r32(4)), c.r32(5), c.r32(6))); }}};
    // 989snd's IOP server (as batch 112's): replies a function of the command; the stream system's disc reads (command 0x38) done as the IRX
    // does them, so the streams' waits (cutscene_start's, the credits' pages) end on both sides
    sif_rpc_register(0x123456, [](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
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

    // the level after its first gameplay frame (the port's)
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            w32(0x0013f2e8, 0xffffffffu);  // (the sound listener's cached points refreshed on the first frame, as in the game: task 0323)
            gameplay_frame();
            render_view_setup();  // (the view matrices of the render frame: zero after an update alone)
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    auto mobys_with = [](uint32_t fn) {
        std::vector<uint32_t> v;
        for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
            if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == fn) v.push_back(m);
        return v;
    };
    auto pick = [](const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; };

    // ---- the Gold Bolts (1134) and level 17's Codebot (1428): their states 0 / 1 / 2 (the level change request on both sides), the hero at the
    // bolt or not, alive and in a state that may collect, the bolt already collected or not (as batch 141 checks Blarg's)
    {
        // (the Codebot's state 1 starts its scene: cutscene_start (Veldin's port, batches 76 / 80 / 112) waits on the 989snd replies its stream reads
        // ask for, which the interpreter's stand-in of the IOP leaves pending at the level's start: its states 0 and 2 here, its state 1 in batch 260)
        struct Gb { int lvl; const char* name; uint32_t addr; void (*fn)(uint32_t); int states; uint32_t skip; };
        static const Gb gbs[] = {{17, "gold_bolt1134_l17", 0x002e3ff8, vo_class1134_update_l17, 3, 0}, {17, "codebot1428_l17", 0x002f1790, vo_class1428_update_l17, 3, 0x2},
                                 {18, "gold_bolt1134_l18", 0x002ecc40, vf_class1134_update_l18, 3, 0}};
        for (const Gb& g : gbs) {
            if (g_oracle_level != g.lvl) { lcheck(g.lvl, g.name, g.addr, 0, Ret::None, nullptr, nullptr); continue; }
            level();
            const std::vector<uint32_t> ms = mobys_with(g.addr);
            if (ms.empty()) { std::printf("%-26s L%d:0x%08x  no moby on this level\n", g.name, g.lvl, g.addr); continue; }
            for (int pass = 0; pass < 8; ++pass) {
                const int st = pass % g.states;
                if ((g.skip & (1u << st)) != 0) continue;
                lcheck(g.lvl, g.name, g.addr, 25, Ret::None, [level, ms, pick, st, pass] {
                    level();
                    const uint32_t m = pick(ms);
                    const uint32_t pv = ee_read<uint32_t>(m + 0x78), P = 0x0013f350;
                    w8(m + 0x20, u(st));
                    Vec4 q = ee_ref<Vec4>(m + 0x10);
                    if (ri(0, 3) != 0) { q.x += rf(-2.0f, 2.0f); q.y += rf(-2.0f, 2.0f); q.z += rf(-2.5f, 2.5f); }
                    else { q.x += rf(-10.0f, 10.0f); q.y += rf(-10.0f, 10.0f); }
                    ee_ref<Vec4>(P + 0x80) = q;
                    w32(P + 0x22a8, ri(0, 4) == 0 ? 0u : 1u);
                    { static const uint8_t hs[] = {0, 3, 3, 1, 7}; w8(P + 0x20a4, hs[ri(0, 4)]); }
                    if (coin()) w16(pv + 0xc, ri(0, 2) == 0 ? 0u : 1u);
                    const int32_t idx = static_cast<int32_t>(ee_read<uint32_t>(pv));
                    if (idx >= 0 && idx < 4) w8(0x0014bec0 + ee_read<uint32_t>(0x0015ed84) * 4 + static_cast<uint32_t>(idx), ri(0, 3) == 0 ? 1u : 0u);
                    if (pass >= 6) w8(m + 0x53, R() & 3);
                    w32(0x0015eea4, R() & 0xffff);
                    w32(OV(0x0015f5cc), R() & 0xffff);
                    seed_rand();
                    return Args{{m}, {}};
                }, [g](A a) { g.fn(a.i[0]); return uint64_t{0}; });
            }
        }
    }

    // ---- HUD slot 7 (the element the hero code requests while PLAYER+0x2048 is set): its draw, each level's own; the element made by a frame
    // played with the Metal Detector owned and something buried near (the port's), then the element's fields varied (as batch 160 checks Batalia's)
    {
        struct H7 { int lvl; const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const H7 h7s[] = {{17, "hud_slot7_draw_l17", 0x002355b8, hud_slot7_draw_tr}, {18, "hud_slot7_draw_l18", 0x0023d1a8, hud_slot7_draw_tr}};
        for (const H7& h : h7s) {
            if (g_oracle_level != h.lvl) { lcheck(h.lvl, h.name, h.addr, 0, Ret::None, nullptr, nullptr); continue; }
            static std::vector<uint8_t> s7;
            level();
            w8(0x0013d4db, 1);  // (the Metal Detector owned, something buried near: 0x205ea8 sets +0x2048 from +0x2044 < 20 with +0x2040 set)
            wf(0x0013f350 + 0x2044, 10.0f);
            w32(0x0013f350 + 0x2040, 1);
            w32(OV(0x0015f3f8), ee_read<uint32_t>(OV(0x0015f3f8)) + 1);
            gameplay_frame();
            uint8_t* ram = ee_get_ram_base();
            s7.assign(ram, ram + 0x02000000);
            std::vector<uint32_t> es;
            for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
                if (ee_read<uint32_t>(e + 0x30) == ovl_code_lookup(0x00235dc0)) es.push_back(e);
            if (es.empty()) { std::printf("%-26s L%d:0x%08x  no slot 7 element\n", h.name, h.lvl, h.addr); continue; }
            lcheck(h.lvl, h.name, h.addr, 100, Ret::None, [es, pick] {
                std::memcpy(ee_get_ram_base(), s7.data(), s7.size());
                const uint32_t e = pick(es);
                if (coin()) w32(e + 0x7c, u(ri(-2, 300)));
                if (coin()) w32(e + 0x40, u(ri(0, 8)));
                if (coin()) w8(e + 0x70, u(ri(0, 0xff)));
                if (coin()) w8(e + 0x71, u(ri(0, 0xff)));
                if (coin()) w32(e + 0x74, u(ri(0, 0x200)));
                if (coin()) w32(e + 0x78, u(ri(0, 0x200)));
                if (coin()) w32(0x0013e504, u(ri(0, 99999)));
                const uint32_t a = ee_read<uint32_t>(e + 0x2c);
                if (a != 0 && coin()) w32(a, u(ri(0, 2)));
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{e}, {}};
            }, [h](A a) { h.fn(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- the empty updates (a `jr ra`): Veldin's 0x2df998 (class 1249, Drek's NPC model) and 0x283900 (the end menu's moby, class 1957)
    if (g_oracle_level == 18) {
        check("drek_npc_update", 0x002df998, 20, Ret::None, [level, mobys_with] {
            level();
            const std::vector<uint32_t> ms = mobys_with(ORACLE_LADDR(18, 0x002eeb98));
            return Args{{ms.empty() ? 0x00180000u : ms[0]}, {}};
        }, [](A a) { drek_npc_update(a.i[0]); return uint64_t{0}; });
        check("end_menu_moby_update", 0x00283900, 20, Ret::None, [level] { level(); return Args{{0x00180000u}, {}}; },
              [](A a) { end_menu_moby_update(a.i[0]); return uint64_t{0}; });
    }

    // ---- level 17's dumps p17_1002 / p17_1005 / p17_1009 (the hero put where the level kills him: the respawn's effect): the live particles of
    // types 0x47 / 0x4f (Veldin's code, particle_types4_tr.cpp), their timers varied (as batch 160 checks Batalia's)
    for (const char* f : {"p17_1002", "p17_1005", "p17_1009"}) {
        const std::vector<uint8_t>& d = dump(dump_path(std::string("drek_veldin/") + f + ".bin"));
        if (g_oracle_level != 17) { skip_check(f, 0x00274e48, "level 17's dumps only (ORACLE_LEVEL=17)"); continue; }
        if (d.empty()) { skip_check(f, 0x00274e48, "dump drek_veldin/ missing: tools/oracle/drek_veldin_tour.sh dumps"); continue; }
        struct Pt { const char* name; uint32_t addr; uint8_t kind; void (*fn)(uint32_t); };
        static const Pt pts[] = {{"particle_47_update", 0x00274e48, 0x47, particle_47_update}, {"particle_4f_update", 0x00275d68, 0x4f, particle_4f_update}};
        for (const Pt& pt : pts) {
            load_dump(d);
            std::vector<uint32_t> qs;
            const uint32_t pool = ee_read<uint32_t>(OV(0x0016016c));
            const int hi = static_cast<int>(ee_read<uint32_t>(OV(0x00160174)));
            for (int i = 0; i <= hi && i < 4096; ++i) {
                const uint32_t q = pool + 0x40u * static_cast<uint32_t>(i);
                if (static_cast<int8_t>(ee_read<uint8_t>(q + 1)) >= 0 && ee_read<uint8_t>(q) == pt.kind) qs.push_back(q);
            }
            const std::string nm = std::string(pt.name) + "_" + f;
            if (qs.empty()) { std::printf("%-26s 0x%08x  no live particle of the type on the dump\n", nm.c_str(), pt.addr); continue; }
            check(nm.c_str(), pt.addr, 100, Ret::None, [&d, qs, pick] {
                load_dump(d);
                const uint32_t q = pick(qs);
                if (ri(0, 3) == 0) w16(q + 0xa, u(ri(-1, 3)));
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{q}, {}};
            }, [pt](A a) { pt.fn(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- level 18's dumps: the boss's fight, the credits, the menu at the end of the game
    auto dumpfile = [](const char* nm) { return dump_path(std::string("drek_veldin/") + nm + ".bin"); };
    const bool l18 = g_oracle_level == 18;
    // the boss in its fight (the dump's state; the fight's states forced on the dump's boss, whose pointers its earlier states set)
    for (const char* f : {"fight4200", "fight5000", "fight6000"}) {
        const std::vector<uint8_t>& d = dump(dumpfile(f));
        const std::string nm = std::string("boss1422_") + f, nh = std::string("boss_hud_draw_") + f;
        if (!l18) { lcheck(18, nm.c_str(), 0x002f2bf0, 0, Ret::None, nullptr, nullptr); continue; }
        if (d.empty()) { skip_check(nm.c_str(), 0x002f2bf0, "dump drek_veldin/ missing: tools/oracle/drek_veldin_tour.sh dumps"); continue; }
        const uint32_t boss = 0x01f07c80;
        if (ee_read<uint32_t>(boss + 0x74) != 0 && static_cast<int16_t>(d[boss + 0xa6] | (d[boss + 0xa7] << 8)) != 1422) {
            skip_check(nm.c_str(), 0x002f2bf0, "the dump's boss is not at 0x1f07c80");
            continue;
        }
        for (int pass = 0; pass < 4; ++pass)
            lcheck(18, nm.c_str(), 0x002f2bf0, 30, Ret::None, [&d, boss, pass] {
                load_dump(d);
                const uint32_t pv = ee_read<uint32_t>(boss + 0x78);
                if (pass >= 1) {  // another of the fight's states (7 .. 21) on the dump's boss, dealt again while the port refuses it (trial runs)
                    static const uint8_t fs[] = {7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
                    static std::vector<uint8_t> saved;
                    for (int attempt = 0; attempt < 30; ++attempt) {
                        load_dump(d);
                        w8(boss + 0x20, fs[ri(0, 14)]);
                        uint8_t* ram = ee_get_ram_base();
                        saved.assign(ram, ram + 0x02000000);
                        const bool ok = trial_run(vf_class1422_update_l18, boss);
                        std::memcpy(ram, saved.data(), saved.size());
                        if (ok) break;
                    }
                }
                if (pass >= 2) {  // the boss's floats moved as typed (the pointers left)
                    for (uint32_t o = 0x20; o < 0x400; o += 4) {
                        if (ri(0, 9) != 0) continue;
                        const uint32_t v = ee_read<uint32_t>(pv + o), ex = v & 0x7f800000u;
                        if (ex >= 0x30000000u && ex < 0x50000000u) wf(pv + o, ee_read<float>(pv + o) * rf(0.5f, 1.5f));
                    }
                }
                if (pass == 3) {  // the hero about the arena
                    Vec4 q = ee_ref<Vec4>(boss + 0x10);
                    const float r = rf(2, 40), a = rf(-3.1f, 3.1f);
                    q.x += r * std::cos(a); q.y += r * std::sin(a);
                    ee_ref<Vec4>(0x0013f3d0) = q;
                }
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
                return Args{{boss}, {}};
            }, [](A a) { vf_class1422_update_l18(a.i[0]); return uint64_t{0}; });
        // the boss's HUD element (its health bar): the elements whose draw function (+0x18) is L18:0x23cd60
        load_dump(d);
        std::vector<uint32_t> es;
        for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
            if (ee_read<uint32_t>(e + 0x18) == ORACLE_LADDR(18, 0x0023cd60)) es.push_back(e);
        if (es.empty()) { std::printf("%-26s L18:0x0023cd60  no element of the boss on this dump\n", nh.c_str()); continue; }
        lcheck(18, nh.c_str(), 0x0023cd60, 60, Ret::None, [&d, es, pick, boss] {
            load_dump(d);
            const uint32_t e = pick(es);
            if (coin()) w32(e + 0x24, u(ri(0, 0x40)));
            if (coin()) w32(e + 0x7c, u(ri(0, 0x40)));
            if (coin()) wf(ee_read<uint32_t>(boss + 0x78) + 0x200, rf(0, 100));
            w32(OV(0x0015f5cc), R());
            return Args{{e}, {}};
        }, [](A a) { vf_hud_boss_draw_l18(a.i[0]); return uint64_t{0}; });
    }
    // the level's own classes alive on the dumps (as the game left them: the fight's and the ending's states; class 331, the Trackdrive, has no
    // moby at the level's start), the hero about them, the frame counter varied
    for (const char* f : {"fight5000", "end18000"}) {
        const std::vector<uint8_t>& d = dump(dumpfile(f));
        if (!l18 || d.empty()) continue;
        struct Cl { const char* name; uint32_t addr; void (*fn)(uint32_t); };
        static const Cl cls[] = {
            {"class41", 0x002a7220, vf_class41_update_l18},
            {"class49", 0x002a76e0, vf_class49_update_l18},
            {"class50", 0x002a7b90, vf_class50_update_l18},
            {"class256", 0x002aa3b0, vf_class256_update_l18},
            {"class1427", 0x002ab7c0, vf_class1427_update_l18},
            {"class1523", 0x002abaa0, vf_class1523_update_l18},
            {"class331", 0x002ce7d0, vf_class331_update_l18},
            {"class341", 0x002ce800, vf_class341_update_l18},
            {"class419", 0x002ceaa0, empty_update},
            {"class564", 0x002d5050, vf_class564_update_l18},
            {"class568", 0x002d5918, vf_class568_update_l18},
            {"class582", 0x002d62e8, vf_class582_update_l18},
            {"class583", 0x002d6600, vf_class583_update_l18},
            {"class586", 0x002d7b20, vf_class586_update_l18},
            {"class587", 0x002d82c0, vf_class587_update_l18},
            {"class605", 0x002d8ba8, vf_class605_update_l18},
            {"class615", 0x002d8fd0, vf_class615_update_l18},
            {"class624", 0x002db460, vf_class624_update_l18},
            {"class628", 0x002dc260, vf_class628_update_l18},
            {"class638", 0x002dc918, vf_class638_update_l18},
            {"class644", 0x002df608, vf_class644_update_l18},
            {"class696", 0x002dfda8, vf_class696_update_l18},
            {"class830", 0x002e1508, vf_class830_update_l18},
            {"class758", 0x002e19d8, vf_class758_update_l18},
            {"class805", 0x002e1f40, vf_class805_update_l18},
            {"class832", 0x002e4168, vf_class832_update_l18},
            {"class885", 0x002e9768, vf_class885_update_l18},
            {"class983", 0x002e9e70, vf_class983_update_l18},
            {"class1021", 0x002ec3e8, vf_class1021_update_l18},
            {"class1039", 0x002ec858, vf_class1039_update_l18},
            {"class1134", 0x002ecc40, vf_class1134_update_l18},
            {"class1135", 0x002edb78, vf_class1135_update_l18},
            {"class1355", 0x002efb88, vf_class1355_update_l18},
            {"class1356", 0x002f0920, vf_class1356_update_l18},
            {"class1381", 0x002f16f0, vf_class1381_update_l18},
            {"class1392", 0x002f1e68, vf_class1392_update_l18},
            {"class1402", 0x002f2310, vf_class1402_update_l18},
            {"class1422", 0x002f2bf0, vf_class1422_update_l18},
            {"class1432", 0x002f7ab0, vf_class1432_update_l18},
            {"class1434", 0x002f7ad8, vf_class1434_update_l18},
            {"class1454", 0x002f7c40, vf_class1454_update_l18},
            {"class1563", 0x002f88e8, vf_class1563_update_l18},
            {"class1568", 0x002fa640, vf_class1568_update_l18},
            {"class1584", 0x002fa728, vf_class1584_update_l18},
            {"class1750", 0x002fad08, vf_class1750_update_l18},
            {"class1799", 0x002fad28, vf_class1799_update_l18},
            {"class1890", 0x002fae48, vf_class1890_update_l18},
            {"class1898", 0x002fb548, vf_class1898_update_l18},
            {"class1899", 0x002fb868, vf_class1899_update_l18},
            {"class1906", 0x002fbb98, vf_class1906_update_l18},
        };
        load_dump(d);
        for (const Cl& c : cls) {
            if (c.addr == 0x002f2bf0) continue;  // (the boss: above)
            std::vector<uint32_t> ms;
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)); m < ee_read<uint32_t>(OV(0x0015ffe0)); m += 0x100)
                if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x74) == c.addr) ms.push_back(m);
            if (ms.empty()) continue;
            const std::string nm = std::string(c.name) + "_" + f;
            lcheck(18, nm.c_str(), c.addr, 12, Ret::None, [&d, ms, pick] {
                load_dump(d);
                const uint32_t m = pick(ms);
                Vec4 q = ee_ref<Vec4>(m + 0x10);
                const float r = coin() ? rf(0, 6) : rf(6, 40), a = rf(-3.1f, 3.1f);
                q.x += r * std::cos(a); q.y += r * std::sin(a); q.z += rf(-2, 4);
                if (coin()) ee_ref<Vec4>(0x0013f3d0) = q;
                w32(OV(0x0015f5cc), R() & 0xffff);
                seed_rand();
                return Args{{m}, {}};
            }, [c](A a) { c.fn(a.i[0]); return uint64_t{0}; });
        }
    }
    // the credits' start, on the fight's dump (the boss's last steps call it); the DMA kick / FlushCache / the vsync wait stubbed on both sides
    {
        const std::vector<uint8_t>& d = dump(dumpfile("fight6000"));
        if (!l18) skip_check("credits_start", 0x00299a68, "level 18's dumps only (ORACLE_LEVEL=18)");
        else if (d.empty()) skip_check("credits_start", 0x00299a68, "dump drek_veldin/ missing: tools/oracle/drek_veldin_tour.sh dumps");
        else {
            g_extra_noop_hooks = {OVF(0x002a08c8), 0x00122298, 0x00118a80};
            check("credits_start", 0x00299a68, 30, Ret::None, [&d] {
                load_dump(d);
                w32(0x0015ec8c, 1);  // (the stream system up, the disc idle, as batch 112's)
                w32(0x00137b00, 0);
                if (coin()) w32(OV(0x0015ed80), u(ri(0, 1)));
                return Args{{0}, {}};
            }, [](A) { credits_start_tr(0); return uint64_t{0}; });
            g_extra_noop_hooks.clear();
        }
    }
    // the credits running (game mode 7): the update and the draw on the dumps, the pages' counters (0x15f620 .. 0x15f626) and the fade varied
    for (const char* f : {"end5700", "end9000"}) {
        const std::vector<uint8_t>& d = dump(dumpfile(f));
        const std::string nu = std::string("credits_update_") + f, nd = std::string("credits_draw_") + f;
        if (!l18) { skip_check(nu.c_str(), 0x00299c48, "level 18's dumps only (ORACLE_LEVEL=18)"); continue; }
        if (d.empty()) { skip_check(nu.c_str(), 0x00299c48, "dump drek_veldin/ missing: tools/oracle/drek_veldin_tour.sh dumps"); continue; }
        auto gen = [&d] {
            load_dump(d);
            if (ri(0, 2) == 0) w16(OV(0x0015f620), u(ri(0, 1)));
            if (ri(0, 2) == 0) w16(OV(0x0015f622), u(ri(0, 0x2a)));
            if (ri(0, 2) == 0) w16(OV(0x0015f624), u(ri(-1, 0x26)));
            if (ri(0, 2) == 0) w16(OV(0x0015f626), u(ri(0, 0x200)));
            if (ri(0, 3) == 0) wf(OV(0x0015f3fc), rf(0, 1));
            // (the frame against one text entry's window (0x173080, 0xb7 entries of 0x10: +8 its first frame, +0xa its length): its
            // box's and text's fades in and out, its edges; before, the frame 0 .. 0x200 met the first entries' middles alone)
            if (coin()) {
                const uint32_t e = OV(0x00173080) + u(ri(0, 5) == 0 ? 0xb6 : ri(0, 0xb6)) * 0x10;  // (the last entry too)
                const int32_t start = ee_read<int16_t>(e + 8), len = ee_read<int16_t>(e + 0xa);
                const int k = ri(0, 5);
                const int32_t at = k == 0 ? ri(0, 0x10) : k == 1 ? ri(0x10, 0x20) : k == 2 ? len - ri(0, 0x10) : k == 3 ? len - ri(0x10, 0x20) : k == 4 ? ri(0, len) : len + ri(-1, 2);
                w16(OV(0x0015f626), u(start + at));
            }
            // (the update: the 0x41 build's page jumps (0x13d1d2), the button that ends the credits (0x13cae4 & 0x800), the slide's
            // last frame (the page's end 0x173bf0 - 0x24, the frame about it); the screen's height the upload's chunks are cut by)
            w8(0x0013d1d2, coin() ? 0x41u : 0u);
            if (ri(0, 5) == 0) w32(0x0013cae4, coin() ? 0x800u : 0x400u);
            // (and the update's ways one by one: a slide's last frame (state 0; the last pages 0x27 / 0x2b end the credits), the
            // dissolve's columns (state 1, steps below 32), its end (steps from 36: the next slide, the 0x41 build's jumps from the
            // pages 8, 0x11, 0x25))
            switch (ri(0, 4)) {
                case 0: {
                    const int32_t page = coin() ? (coin() ? 0x27 : 0x2b) : ee_read<int16_t>(OV(0x0015f622));
                    w16(OV(0x0015f622), u(page));
                    if (page >= 0 && page < 0x2b) w16(OV(0x0015f626), u(ee_read<int32_t>(OV(0x00173bf0) + u(page) * 4) - 0x24 + ri(-2, 2)));
                    w16(OV(0x0015f620), 0);
                    break;
                }
                case 1:
                    w16(OV(0x0015f620), 1);
                    w16(OV(0x0015f624), u(ri(-1, 31)));
                    break;
                case 2: {
                    static const uint32_t pages[] = {8, 0x11, 0x25, 0x11};
                    w16(OV(0x0015f620), 1);
                    w16(OV(0x0015f624), u(ri(32, 40)));
                    w16(OV(0x0015f622), coin() ? pages[ri(0, 3)] : u(ri(0, 0x2a)));
                    break;
                }
                default:
                    break;
            }
            if (ri(0, 3) == 0) w32(0x0013e504, coin() ? 512u : u(ri(0x60, 0x220)));
            if (coin()) w32(0x0015ed80, u(ri(0, 1)));  // (the 50 Hz build's slides: the other table of the disc's files)
            w32(OV(0x0015f5cc), R());
            return Args{{0}, {}};
        };
        g_extra_noop_hooks = {OVF(0x002a08c8), 0x00122298, 0x00118a80};
        check(nu.c_str(), 0x00299c48, 150, Ret::None, gen, [](A) { credits_update_tr(0); return uint64_t{0}; });
        check(nd.c_str(), 0x001f8f78, 150, Ret::None, gen, [](A) { credits_draw_tr(0); return uint64_t{0}; });
        g_extra_noop_hooks.clear();
    }
    // the menu at the end of the game (menu state 0x21) and the page of its state 0x23: every widget function of the two pages (+0 update, +4 draw,
    // +8 focus in, +0xc focus out of each widget at page + 0x44 + 4 i) on its own widget, a1 = 0 / 1, the pad's buttons varied
    for (const char* f : {"end18000", "end18300"}) {
        const std::vector<uint8_t>& d = dump(dumpfile(f));
        const std::string nm0 = std::string("end_menu_") + f;
        if (!l18) { skip_check(nm0.c_str(), 0x00283530, "level 18's dumps only (ORACLE_LEVEL=18)"); continue; }
        if (d.empty()) { skip_check(nm0.c_str(), 0x00283530, "dump drek_veldin/ missing: tools/oracle/drek_veldin_tour.sh dumps"); continue; }
        struct Wf { uint32_t veldin; const char* name; uint32_t (*fn)(uint32_t, uint32_t); };
        static const Wf wfs[] = {{0x00283530, "panel_update", end_menu_panel_update_tr}, {0x002838b0, "panel_draw", end_menu_panel_draw_tr},
                                 {0x002833d0, "panel_focus_in", end_menu_panel_focus_in_tr}, {0x002834a8, "panel_focus_out", end_menu_panel_focus_out_tr},
                                 {0x0027fed8, "choice_update", end_menu_choice_update_tr}, {0x002806e0, "choice_draw", end_menu_choice_draw_tr},
                                 {0x00280038, "page23_update", end_menu_page23_update_tr}, {0x002803b0, "page23_draw", end_menu_page23_draw_tr},
                                 {0x00280020, "page23_focus_in", end_menu_page23_focus_in_tr}};
        load_dump(d);
        // the widgets of the two pages (Veldin's page tables 0x1b71f0 / 0x1b6b38 on the level's overlay) whose functions these are
        std::map<uint32_t, std::vector<uint32_t>> own;
        for (uint32_t page : {OV(0x001b71f0), OV(0x001b6b38)})
            for (uint32_t i = 0; i < 0xe; ++i) {
                const uint32_t w = ee_read<uint32_t>(page + 0x44 + 4 * i);
                if (w == 0) continue;
                for (uint32_t k = 0; k < 0x10; k += 4) {
                    const uint32_t fn = ee_read<uint32_t>(w + k);
                    if (fn != 0) own[ovl_code_to_veldin(fn)].push_back(w);
                }
            }
        g_extra_noop_hooks = {OVF(0x002a08c8), 0x00122298, 0x00118a80};
        // (the panel's model drawn by the moby pass 0x24ff08: the w lanes of its rows (sqc2 of VU0 registers) hold what the checks before left in
        // the VU0 registers, the interpreter's and the port's: a lone w lane is not compared, as batch 63 does; alone the check matches them too)
        for (const Wf& wf_ : wfs) {
            // (only the panel's draw: before, every widget check left out every lone word at +0xc of a quadword, anywhere in
            // RAM, which hid the panel update's dialogue request 0x1516ec; since audit 2)
            g_ignore_lone_w_lanes = wf_.veldin == 0x002838b0;
            const std::string nm = std::string("end_menu_") + wf_.name + "_" + f;
            if (own[wf_.veldin].empty()) { std::printf("%-26s 0x%08x  no widget of the function on the dump's pages\n", nm.c_str(), wf_.veldin); continue; }
            const std::vector<uint32_t> ws = own[wf_.veldin];
            const uint32_t fn = wf_.veldin;
            check(nm.c_str(), wf_.veldin, 120, Ret::Int32, [&d, ws, pick, fn] {
                load_dump(d);
                const uint32_t w = pick(ws);
                // the pad: a packet of held buttons (cross / circle / the d-pad mostly), as batch 91 builds them
                if (coin()) {
                    static const uint16_t bs[] = {0x0040, 0x0020, 0x1000, 0x4000, 0x0800, 0x0010, 0};
                    const uint16_t low = static_cast<uint16_t>(~bs[ri(0, 6)]);
                    const uint8_t pk[6] = {static_cast<uint8_t>(low >> 8), static_cast<uint8_t>(low & 0xff), 0x7f, 0x7f, 0x7f, 0x7f};
                    pad_read_packet(0x0013c940, pk, 6);
                }
                end_menu_vary(fn, w);
                w32(OV(0x0015f5cc), R());
                seed_rand();
                return Args{{w, u(ri(0, 1))}, {}};
            }, [wf_](A a) { return r32(wf_.fn(a.i[0], a.i[1])); });
        }
        g_ignore_lone_w_lanes = false;
        g_extra_noop_hooks.clear();
    }

    g_extra_hooks.clear();
    g_host_clock_fixed = saved_clock;
    g_extra_noop_hooks.clear();
    g_ee_null_is_ram = null_ram;
    g_ignored_words = saved_ignored;
}

}  // namespace rac1::oracle
