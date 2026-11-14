// Oracle batch 530 (#145: the differences the stage-4 family tasks met in existing engine ports, checked on each level's own
// state): the hand-written engine ports against the level's own copy of the original
//  - segment_collide 0x1efc70 (ovl_1e_1f.cpp): lines across the level against its own collision and mobys, from a moby's or
//    the hero's place to another moby 0.4 above its place (as the classes' sight tests), to a moby moved in RAM before the
//    start (its grid cells and bounding sphere where they were), to a point up to 60 away, or 60 .. 200 away; flags 0, 2
//    (no primitives) or 0x10 (both faces), the start's moby ignored or none. (The family tasks' "far lines report a moby
//    where the original reports the world" was batch 514's harness: the Sandmouse's search got three of its six float
//    arguments in the original; with all six this function and the search agree.)
//    After each case the typed hit facade (game/collision/collision.h, #150 task C1) is compared with the raw words the line
//    left (the block 0x173e40 +0x18 .. +0x7f, the hit list, a contact record's address, the +0x18 writer): a mismatch changes
//    the check's return value.
//  - carry_update 0x2dc9f0 (enemy_749.cpp) on the level's own carriable mobys (a class hook +0x10 that is not the default
//    one) and a Morph-o-Ray chicken spawned where its class is loaded: the record in states 1 .. 8 (turning, rising, held by
//    the hero or a moby and handed over, resting, flying with or without a target, falling), fast or slow, by the hero or by
//    another moby (away from its grid cells), low enough to explode, its countdown run out a third of the time (the class's
//    hook +0xc then: carry_hook_c, which until #145 ran only the registered hooks: Veldin's and Veldin finale's class 749,
//    Eudora's class 340 and the chickens of most levels stayed carried); and class_hook_10 0x2db890 and class_hook_4
//    0x2da218 on the same subjects (Veldin finale's class 749: its hooks +0x10 and +4 were TODO_PORTs, so it could not be
//    carried, sucked or handed over). (Task F11's 14
//    differences in 200 cases came from records dealt at random, whose class hooks were not hooks of the level.)
// Subjects: the level after its first gameplay frame (the port's). No RAM dumps. On Kalebo III (16) the level's moby table
// covers the test area 0x1e00000 .. 0x1e10000, so the checks' own words go below the port's EE stack (scratch()).
// B530_N=n: n cases (300 / 200); B530_ST=s: carry_update with the record's state s only.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 530, N = 0 .. 18 (a level without carriable mobys says so).

#include "ovl_oracle.h"

#include <game/collision/collision.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>

namespace rac1::oracle {

namespace {

bool selected(const char* name) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return false;
    const char* only = std::getenv("ORACLE_ONLY");
    return only == nullptr || std::strcmp(only, name) == 0;
}

// the level after its first gameplay frame (the port's), as batch 514's
std::vector<uint8_t> g_snap;
int g_snap_level = -1;
void level() {
    uint8_t* ram = ee_get_ram_base();
    if (g_snap.empty() || g_snap_level != g_oracle_level) {
        veldin_boot();
        level_setup_body(1, 0);
        w32(OV(0x0016c060), 0xf);
        w32(0x0013f2e8, 0xffffffffu);
        if (g_oracle_level == 14) w8(0x0013d3f0, 1);
        gameplay_frame();
        g_snap.assign(ram, ram + 0x02000000);
        g_snap_level = g_oracle_level;
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
    }
}

uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t end_moby() { return ee_read<uint32_t>(OV(0x0015ffe0)); }
bool live(uint32_t m) { return ee_read<uint8_t>(m + 0x20) < 0xfe; }
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (live(m) && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}
uint32_t pick(const std::vector<uint32_t>& v) { return v[static_cast<size_t>(ri(0, static_cast<int>(v.size()) - 1))]; }
Vec4 around(const Vec4& at, float r0, float r1, float z0, float z1) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
}

// 0x40 bytes the checks own: the test area, or where the level's moby table covers it (Kalebo III's: 0x1dfa180 ..
// 0x1e69e80, all of it live mobys there), 4 KB below the port's EE stack (ee_stack_ptr()'s start, 0x1fff000): above what the
// ports' frames take and below what the original's take from the stack top 0x1ffffc0
uint32_t scratch() {
    if (end_moby() <= TEST_AREA || first_moby() >= TEST_AREA + TEST_AREA_SIZE) return TEST_AREA;
    return 0x01ffe000u;
}

// a run of the port that throws (a pointer outside RAM) asks for another state; the RAM restored (batch 514's)
bool trial(const std::function<void()>& port) {
    static std::vector<uint8_t> saved;
    uint8_t* ram = ee_get_ram_base();
    saved.assign(ram, ram + 0x02000000);
    bool ok = true;
    const uint32_t stack_top = ee_stack_ptr();
    try { port(); } catch (const std::exception&) { ok = false; }
    ee_stack_ptr() = stack_top;
    std::memcpy(ram, saved.data(), saved.size());
    return ok;
}

// ---- the typed hit facade (game/collision/collision.h) against the raw words a query left: 0 when every accessor reads them,
// else a mask of the ones that do not (folded into segment_collide_level's return, so a facade that misreads fails that check;
// the RAM itself is compared with the original's as always, so the facade reads what the original wrote)
uint32_t facade_mismatch() {
    const uint32_t B = OV(0x00173e40), H = OV(0x00177f00);
    auto same = [](const Vec4& v, uint32_t at) { return std::memcmp(&v, &ee_ref<Vec4>(at), sizeof(Vec4)) == 0; };
    const CollisionHit h = coll_hit();
    uint32_t bad = 0;
    if (h.moby != ee_read<uint32_t>(B + 0x18) || coll_hit_moby() != ee_read<uint32_t>(B + 0x18)) bad |= 0x00010000u;
    if (h.flags != ee_read<uint32_t>(B + 0x1c) || coll_hit_flags() != ee_read<uint32_t>(B + 0x1c)) bad |= 0x00020000u;
    if (!same(h.point, B + 0x20) || !same(coll_hit_point(), B + 0x20)) bad |= 0x00040000u;
    if (!same(h.pushed, B + 0x30) || !same(coll_hit_pushed(), B + 0x30)) bad |= 0x00080000u;
    if (!same(h.normal, B + 0x40) || !same(coll_hit_normal(), B + 0x40)) bad |= 0x00100000u;
    for (int k = 0; k < 3; ++k)
        if (!same(h.tri[k], B + 0x50 + 0x10 * static_cast<uint32_t>(k)) || !same(coll_hit_tri(k), B + 0x50 + 0x10 * static_cast<uint32_t>(k))) bad |= 0x00200000u << k;
    int n = 0;
    while (n < 0x80 && ee_read<uint32_t>(H + 4 * static_cast<uint32_t>(n)) != 0) ++n;
    if (coll_hit_list() != H || coll_hit_list_count() != n) bad |= 0x01000000u;
    for (int i = 0; i <= n && i < 0x80; ++i)
        if (coll_hit_list_at(i) != ee_read<uint32_t>(H + 4 * static_cast<uint32_t>(i))) bad |= 0x02000000u;
    const int ring = static_cast<int>(ee_read<uint32_t>(B + 0x14) & 0x3f);  // (the ring's next record; no draw from the cases' random sequence)
    if (coll_contact(ring) != OV(0x00178100) + 0x40 * static_cast<uint32_t>(ring)) bad |= 0x04000000u;
    // the writer: a value set and read back raw, then the word put back as it was (the RAM stays the port's)
    const uint32_t keep = ee_read<uint32_t>(B + 0x18);
    coll_hit_set_moby(keep ^ 0x5a5a0000u);
    if (ee_read<uint32_t>(B + 0x18) != (keep ^ 0x5a5a0000u)) bad |= 0x08000000u;
    w32(B + 0x18, keep);
    if (bad != 0) {
        static int shown = 0;
        if (shown++ < 4) std::printf("  [coll_hit facade] mismatch mask %08x\n", bad);
    }
    return bad;
}

// ---- segment_collide on the level's own collision and mobys
void segment_checks() {
    if (!selected("segment_collide_level")) return;
    level();
    std::printf("  (moby table 0x%08x .. 0x%08x; the test area 0x%08x .. 0x%08x)\n", first_moby(), end_moby(), TEST_AREA, TEST_AREA + TEST_AREA_SIZE);
    const int ncase = std::getenv("B530_N") != nullptr ? std::atoi(std::getenv("B530_N")) : 300;
    const uint32_t S = scratch();
    if (S == 0) { skip_check("segment_collide_level", 0x001efc70, "no free memory for the line's end points"); return; }
    check("segment_collide_level", 0x001efc70, ncase, Ret::Int32, [S] {
        level();
        const std::vector<uint32_t> ms = live_mobys();
        const uint32_t from_m = coin() ? ee_read<uint32_t>(0x001413d0) : pick(ms);
        const Vec4 from = around(ee_ref<Vec4>(from_m + 0x10), 0.0f, 1.0f, 0.0f, 1.5f);
        Vec4 to{};
        const int k = ri(0, 3);
        if (k == 0) {  // a moby of the level, 0.4 above its place (within 300: see below)
            to = ee_ref<Vec4>(pick(ms) + 0x10);
            for (int t = 0; t < 40 && std::abs(to.x - from.x) + std::abs(to.y - from.y) + std::abs(to.z - from.z) > 300.0f; ++t)
                to = ee_ref<Vec4>(pick(ms) + 0x10);
            if (std::abs(to.x - from.x) + std::abs(to.y - from.y) + std::abs(to.z - from.z) > 300.0f) to = around(from, 1.0f, 60.0f, -10.0f, 10.0f);
            to.z += 0.4f;
        } else if (k == 3) {  // a moby moved (in RAM only: its grid cells and bounding sphere stay) 1 .. 30 from the start
            const uint32_t t = pick(ms);
            const Vec4 p = around(from, 1.0f, 30.0f, -2.0f, 3.0f);
            ee_ref<Vec4>(t + 0x10) = Vec4{p.x, p.y, p.z, ee_read<float>(t + 0x1c)};
            to = p;
            to.z += 0.4f;
        } else if (k == 1) {  // a point up to 60 away
            to = around(from, 1.0f, 60.0f, -10.0f, 10.0f);
        } else {  // a point 60 .. 200 away
            to = around(from, 60.0f, 200.0f, -30.0f, 30.0f);
        }
        // (lines of at most some 300 units across the three axes: a line that crosses 255 of the 4-unit cells (some 1000)
        // makes the original write its list's end mark past the scratchpad (0x70004000), a write the interpreter drops, the
        // port's memory refuses and the console's TLB does not map; no game line is that long)
        ee_ref<Vec4>(S) = from;
        ee_ref<Vec4>(S + 0x10) = to;
        // (the contact registers vf25 .. vf29 cleared on both sides: a line that hits a moby's primitive, no triangle,
        // stores vf27 as it was, in the original whatever VU0 code last left there, in the port its own last contact)
        collide_vu_reset();
        oracle_vu_contact_reset();
        const uint32_t fl = ri(0, 3) != 0 ? 0u : (coin() ? 2u : 0x10u);
        return Args{{S, S + 0x10, fl, coin() ? from_m : 0u, 0u}, {}};
    }, [](A a) {
        const int32_t r = segment_collide(a.i[0], a.i[1], a.i[2], a.i[3], a.i[4]);
        // the typed hit facade read after each case: a word it does not read as the line left it makes the return differ
        const uint32_t bad = facade_mismatch();
        return static_cast<uint64_t>(static_cast<uint32_t>(r) ^ bad);
    });
}

// ---- carry_update and the class hook +0x10 on the level's own carriable mobys (a class hook +0x10: the carry record)
// the class's hook table (class +0x2c), 0 for none
uint32_t hooks_of(uint32_t m) {
    const uint32_t cls = ee_read<uint32_t>(m + 0x24);
    if (cls < 0x00100000u || cls >= 0x02000000u) return 0;
    const uint32_t h = ee_read<uint32_t>(cls + 0x2c);
    return h >= 0x00100000u && h < 0x02000000u ? h : 0;
}
// a Morph-o-Ray chicken (class 270) spawned by the hero, its carry record at `rec` (pvars +0x14); 0 when the level cannot
uint32_t chicken(uint32_t rec) {
    uint32_t c = 0;
    if (!trial([&c] { c = moby_spawn(0x10e); })) return 0;
    c = moby_spawn(0x10e);
    if (c == 0 || ee_read<uint32_t>(c + 0x78) == 0) return 0;
    const Vec4 q = around(ee_ref<Vec4>(0x0013f3d0), 1.0f, 5.0f, 0.0f, 1.0f);
    ee_ref<Vec4>(c + 0x10) = Vec4{q.x, q.y, q.z, 1.0f};
    moby_update_transform(c);
    for (uint32_t o = 0; o < 0xa0; o += 4) w32(rec + o, 0);
    w32(ee_read<uint32_t>(c + 0x78) + 0x14, rec);
    return c;
}
void carry_checks() {
    const bool run_carry = selected("carry_update_level"), run_hook = selected("class_hook_10_level");
    if (!run_carry && !run_hook && !selected("class_hook_4_level")) return;
    level();
    std::vector<uint32_t> carried;  // (live mobys whose class has a hook +0x10, with pvars)
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100) {
        if (!live(m) || ee_read<uint32_t>(m + 0x78) == 0) continue;
        const uint32_t hooks = hooks_of(m);
        // (not the classes whose hook +0x10 is the default one, 0x2db888's copy: no record, carry_update does nothing)
        if (hooks == 0 || ee_read<uint32_t>(hooks + 0x10) == 0 || ee_read<uint32_t>(hooks + 0x10) == ovl_xlate_code(0x002db888u)) continue;
        if (std::getenv("B530_INFO") != nullptr)
            std::printf("  INFO moby 0x%08x class %d update 0x%08x hooks +4 0x%08x +0xc 0x%08x +0x10 0x%08x\n", m,
                        ee_read<int16_t>(m + 0xa6), ee_read<uint32_t>(m + 0x74), ee_read<uint32_t>(hooks + 4), ee_read<uint32_t>(hooks + 0xc),
                        ee_read<uint32_t>(hooks + 0x10));
        carried.push_back(m);
    }
    const uint32_t S = scratch();
    // (the chickens' records in the test area: not on a level whose moby table covers it)
    const bool chickens = S == TEST_AREA && chicken(TEST_AREA + 0x100) != 0;
    level();
    std::printf("  (carry: %zu subjects of the level%s)\n", carried.size(), chickens ? ", and chickens" : "");
    if (carried.empty() && !chickens) {
        if (run_carry) skip_check("carry_update_level", 0x002dc9f0, "no carriable moby on this level");
        if (run_hook) skip_check("class_hook_10_level", 0x002db890, "no carriable moby on this level");
        if (selected("class_hook_4_level")) skip_check("class_hook_4_level", 0x002da218, "no carriable moby on this level");
        return;
    }
    if (S == 0) { skip_check("carry_update_level", 0x002dc9f0, "no free memory for the animation sets"); return; }
    auto subject = [carried, chickens] {
        if (chickens && (carried.empty() || ri(0, 3) == 0)) return chicken(TEST_AREA + 0x100);
        return pick(carried);
    };
    // the hooks +0x10 and +4 themselves, on the subjects (the originals 0x2db890 / 0x2da218 dispatch through `jalr`); the hook
    // +4 (the hand-over: the moby put at the point a1 names and its record reset) on the subject's record in a held state
    if (run_hook)
        check("class_hook_10_level", 0x002db890, 60, Ret::Int32, [subject] {
            level();
            return Args{{subject()}, {}};
        }, [](A a) { return static_cast<uint64_t>(class_hook_10(a.i[0])); });
    if (selected("class_hook_4_level") && S != 0)
        check("class_hook_4_level", 0x002da218, 60, Ret::Int32, [subject, S] {
            level();
            const uint32_t m = subject();
            uint32_t r = class_hook_10(m);
            if (r == 0) r = ee_read<uint32_t>(m + 0x78) + 0xd0;  // (as the carry check's)
            w16(r + 0x68, u(ri(0, 4)));
            w16(r + 0x6a, u(ri(0, 3)));
            w8(S + 4, 0); w8(S + 6, 0); w8(S + 7, 0);
            w32(r + 0x70, S);
            ee_ref<Vec4>(S + 0x20) = around(ee_ref<Vec4>(m + 0x10), 0.0f, 2.0f, 0.0f, 1.0f);
            return Args{{m, S + 0x20}, {}};
        }, [](A a) { return static_cast<uint64_t>(static_cast<uint32_t>(class_hook_4(a.i[0], a.i[1]))); });
    if (!run_carry) return;
    const int ncase = std::getenv("B530_N") != nullptr ? std::atoi(std::getenv("B530_N")) : 200;
    const int only_st = std::getenv("B530_ST") != nullptr ? std::atoi(std::getenv("B530_ST")) : -1;
    auto deal = [subject, only_st, S] {
        collide_vu_reset();
        oracle_vu_contact_reset();
        const std::vector<uint32_t> ms = live_mobys();
        const uint32_t m = subject();
        // (the record: the hook's, else the chicken's pvars' pointer, for a port that does not know the hook)
        uint32_t r = class_hook_10(m);
        if (r == 0 && ee_read<int16_t>(m + 0xa6) == 0x10e) r = ee_read<uint32_t>(ee_read<uint32_t>(m + 0x78) + 0x14);
        if (r == 0) r = ee_read<uint32_t>(m + 0x78) + 0xd0;  // (a class 749 hook's, pvars + 0xd0, where the port did not know it)
        constexpr uint32_t P = 0x0013f350;
        const uint32_t hero = ee_read<uint32_t>(P + 0x2080);
        w16(r + 0x68, only_st >= 0 ? u(only_st) : u(ri(1, 8)));
        w32(r + 0x60, coin() ? hero : pick(ms));
        wf(r + 0x64, rf(0.5f, 5));
        w16(r + 0x6a, u(ri(0, 3)));  // (the hero's held slot handed over to, as batch 65's)
        w32(P + 0x2050 + 4 * ee_read<uint16_t>(r + 0x6a), pick(ms));
        wfrand(r, 2, -5, 5);
        for (int k = 0; k < 2; ++k) wf(r + k * 4, ee_read<float>(m + 0x10 + 4 * u(k)) + rf(-5, 5));
        wf(r + 0xc, rf(-1, 2));  // (the countdown: run out a third of the time, the class's hook +0xc then)
        wf(r + 0x6c, rf(0, 1.2f));
        wfrand(r + 0x10, 3, -0.4f, 0.4f);
        if (coin()) wfrand(r + 0x10, 3, -3.0f, 3.0f);  // (fast: across grid cells in a frame)
        w32(r + 0x74, ri(0, 2) == 0 ? 0u : (coin() ? hero : pick(ms)));
        wf(r + 0x7c, rf(0, 0.1f));
        w32(r + 0x94, u(ri(0, 400)));
        wf(r + 0x98, rf(0, 0.3f));
        {  // (the animation sets the resting state picks: two the class has)
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            const int sets = std::max(1, static_cast<int>(ee_read<uint8_t>(cls + 0xc)));
            auto aset = [cls, sets] {
                for (int t = 0; t < 16; ++t) {
                    const uint32_t set = u(ri(0, sets - 1));
                    if (ee_read<uint32_t>(cls + 0x48 + set * 4) != 0) return set;
                }
                return 0u;
            };
            if (ee_read<uint32_t>(r + 0x70) == 0 || coin()) {
                w8(S + 4, aset());
                w8(S + 6, aset());
                w8(S + 7, aset());
                w32(r + 0x70, S);
            }
        }
        if (coin()) {  // (the moby by the hero, or by another moby: away from its grid cells)
            const Vec4 q = around(ee_ref<Vec4>(coin() ? 0x0013f3d0 : pick(ms) + 0x10), 0.0f, 3.0f, 0.0f, 3.0f);
            ee_ref<Vec4>(m + 0x10) = Vec4{q.x, q.y, q.z, ee_read<float>(m + 0x1c)};
        }
        if (coin()) wf(m + 0x18, ri(0, 3) == 0 ? rf(0.0f, 2.5f) : rf(1.8f, 2.6f));  // (low: the flight's and the fall's explosion below 2)
        wf(m + 0x48, rf(-3.1f, 3.1f));
        return Args{{m, ee_read<uint32_t>(m + 0x78)}, {}};
    };
    const std::function<uint64_t(const Args&)> port = [](A a) { return static_cast<uint64_t>(static_cast<uint32_t>(carry_update(a.i[0], a.i[1]))); };
    check("carry_update_level", 0x002dc9f0, ncase, Ret::Int32, [deal, port] {
        // (dealt again while the port's run throws: a carrier moby without the chain point the hold reads)
        Args a;
        for (int attempt = 0; attempt < 40; ++attempt) {
            level();
            a = deal();
            if (trial([&port, &a] { port(a); })) break;
        }
        return a;
    }, port);
}

}  // namespace

void batch_530() {
    if (g_only_batch != 530) return;
    g_batch = 530;
    segment_checks();
    carry_checks();
}

}  // namespace rac1::oracle
