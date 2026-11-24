// Oracle batch 540 (#150 stage 5, task C3): the collision scene (src/game/collision/collision_scene.h) against EE memory and
// against the routines' EE reads - a host check at the boundary, no original code run. On the level after its first gameplay
// frame (the port's, as batch 530's):
//  - scene_tree: the tree *(0x173e40) walked here from the EE bytes (a walk of its own, with the routines' lookup rules):
//    every non-empty cell's word and fetched quadwords equal the scene's at that (x, y, z); the scene's own cell walk has the
//    same cells; a second parse is the same; 20000 lookups at random coordinates (inside and around the rows, negative, past
//    the 1024-unit grid) equal coll_tree_cell's on EE memory, word and quadwords; a fault (one fetched word changed in EE
//    memory, then put back) is found by the same comparison
//  - scene_mesh: the hero's mesh list *(0x15f618): its count, every chunk's sphere, counts, vertices (the pair loop's reads:
//    two at least, an odd count one more) and triangle bytes equal EE memory's; a fault found
//  - scene_queries: B540_N (300) queries around the hero and the level's mobys - segment_collide (flags 0, 2, 0x10, 1),
//    sphere_collide, capsule_collide, sphere_mesh_collide - each run with RAC1_COLL_SCENE=ee and =native (the check mode off:
//    the scene alone) from the same RAM, scratchpad and VU0 contact registers: the same return value, RAM, scratchpad and
//    contact registers; the native runs took their cells from the scene (its parse counted)
//  - scene_keys (the check mode off, then on): two small trees in the test area, A and B: A, B, A again takes the kept A (no
//    parse); A's bytes changed while B is current: parsed again at the switch to A; level_collision_set's drop: parsed again
//    for the same pointer; an edit in place with no switch: with the check mode found at the next lookup (parsed again),
//    without it the kept scene is used (the limit collision_scene.h states: printed, not a failure). The RAM is put back.
// A custom level's sections (sections.cpp switches both pointers with coll_set_tree and a plain store) need a session, which
// a level-built state does not have: the mod tours (07, 08) run with RAC1_COLL_SCENE=native RAC1_COLL_SCENE_CHECK=1 cover them.
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 540, N = 0 .. 18.

#include "ovl_oracle.h"

#include <game/collision/collide_hit.h>
#include <game/collision/collision.h>
#include <game/collision/collision_scene.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rac1 {
void collide_vu_save(float out[15]);  // (collide_mesh.cpp)
void collide_vu_restore(const float in[15]);
}  // namespace rac1

namespace rac1::oracle {

namespace {

bool selected(const char* name) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return false;
    const char* only = std::getenv("ORACLE_ONLY");
    return only == nullptr || std::strcmp(only, name) == 0;
}
bool fail(const char* what, const std::string& why) {
    std::printf("%-26s FAIL: %s\n", what, why.c_str());
    ++g_fail;
    return false;
}
std::string hex(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08x", v);
    return b;
}

// the level after its first gameplay frame (the port's), as batch 530's
std::vector<uint8_t> g_snap, g_snap_spr;
float g_snap_vu[15];
void level() {
    uint8_t* ram = ee_get_ram_base();
    uint8_t* spr = hp<uint8_t>(0x70000000u);
    if (g_snap.empty()) {
        veldin_boot();
        level_setup_body(1, 0);
        w32(OV(0x0016c060), 0xf);
        w32(0x0013f2e8, 0xffffffffu);
        if (g_oracle_level == 14) w8(0x0013d3f0, 1);
        gameplay_frame();
        g_snap.assign(ram, ram + 0x02000000);
        g_snap_spr.assign(spr, spr + 0x4000);
        collide_vu_save(g_snap_vu);
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
        std::memcpy(spr, g_snap_spr.data(), g_snap_spr.size());
        collide_vu_restore(g_snap_vu);
    }
}

uint32_t r16(uint32_t a) { return ee_read<uint16_t>(a); }
uint32_t r32(uint32_t a) { return ee_read<uint32_t>(a); }

// a cell of the scene against EE memory at (x, y, z): the word coll_tree_cell finds and the quadwords at its offset
bool cell_same(const CollTreeScene& s, uint32_t tree, int32_t x, int32_t y, int32_t z, std::string* why) {
    const uint32_t w = collide_detail::coll_tree_cell(tree, x, y, z);
    const CollTreeScene::Cell c = s.cell(x, y, z);
    char b[160];
    if (c.word != w) {
        std::snprintf(b, sizeof b, "cell (%d, %d, %d): scene word 0x%08x, EE 0x%08x", x, y, z, c.word, w);
        *why = b;
        return false;
    }
    if (w == 0) return true;
    const uint32_t src = ((w >> 8) + tree) & ~15u;
    for (uint32_t i = 0; i < (w & 0xffu) * 4; ++i) {
        if (c.words[i] != r32(src + i * 4)) {
            std::snprintf(b, sizeof b, "cell (%d, %d, %d) word %u: scene 0x%08x, EE 0x%08x at 0x%08x", x, y, z, i, c.words[i], r32(src + i * 4), src + i * 4);
            *why = b;
            return false;
        }
    }
    return true;
}

// every non-empty cell, walked from the EE bytes here (z rows of y rows of x rows: the routines' lookup's offsets), against the
// scene; returns the differences (the first in `why`) and the cells walked in `cells`
int tree_compare(const CollTreeScene& s, uint32_t tree, size_t* cells, std::string* why) {
    int bad = 0;
    *cells = 0;
    const int32_t z0 = static_cast<int32_t>(r16(tree)), zn = static_cast<int32_t>(r16(tree + 2));
    for (int32_t zi = 0; zi < zn; ++zi) {
        const uint32_t zl = r16(tree + 4 + static_cast<uint32_t>(zi) * 2) << 2;
        if (zl == 0) continue;
        const uint32_t Y = zl + tree;
        const int32_t y0 = static_cast<int32_t>(r16(Y)), yn = static_cast<int32_t>(r16(Y + 2));
        for (int32_t yi = 0; yi < yn; ++yi) {
            const uint32_t yl = r32(Y + 4 + static_cast<uint32_t>(yi) * 4);
            if (yl == 0) continue;
            const uint32_t X = yl + tree;
            const int32_t x0 = static_cast<int32_t>(r16(X)), xn = static_cast<int32_t>(r16(X + 2));
            for (int32_t xi = 0; xi < xn; ++xi) {
                if (r32(X + 4 + static_cast<uint32_t>(xi) * 4) == 0) continue;
                ++*cells;
                std::string w;
                if (!cell_same(s, tree, x0 + xi, y0 + yi, z0 + zi, &w)) {
                    if (bad++ == 0) *why = w;
                }
            }
        }
    }
    return bad;
}

// the mesh list's scene against EE memory: the differences (the first in `why`)
int mesh_compare(const CollMeshScene& m, uint32_t list, size_t* verts, size_t* tris, std::string* why) {
    int bad = 0;
    char b[200];
    auto no = [&](const char* fmt, uint32_t i, uint32_t k, uint32_t sv, uint32_t ev) {
        if (bad++ == 0) {
            std::snprintf(b, sizeof b, fmt, i, k, sv, ev);
            *why = b;
        }
    };
    *verts = *tris = 0;
    if (m.count() != static_cast<int32_t>(r32(list))) no("count %u%u: scene %u, EE %u", 0, 0, static_cast<uint32_t>(m.count()), r32(list));
    for (int32_t i = 0; i < m.count() && i < static_cast<int32_t>(r32(list)); ++i) {
        const uint32_t e = list + 0x10 + static_cast<uint32_t>(i) * 0x10, ui = static_cast<uint32_t>(i);
        const CollMeshScene::Chunk& c = m.chunk(ui);
        for (uint32_t k = 0; k < 4; ++k)
            if (c.s[k] != r16(e + k * 2)) no("chunk %u sphere %u: scene %u, EE %u", ui, k, c.s[k], r16(e + k * 2));
        if (c.ntri != ee_read<uint8_t>(e + 8)) no("chunk %u%u triangles: scene %u, EE %u", ui, 0, c.ntri, ee_read<uint8_t>(e + 8));
        if (static_cast<uint32_t>(c.nv) != r16(e + 0xa)) no("chunk %u%u vertices: scene %u, EE %u", ui, 0, static_cast<uint32_t>(c.nv), r16(e + 0xa));
        const uint32_t vp = r32(e + 0xc);
        for (uint32_t k = 0; k < CollMeshScene::vertex_reads(c.nv); ++k, ++*verts)
            for (uint32_t a = 0; a < 4; ++a)
                if (m.vertex(c, k)[a] != r16(vp + k * 8 + a * 2)) no("chunk %u vertex %u: scene %u, EE %u", ui, k, m.vertex(c, k)[a], r16(vp + k * 8 + a * 2));
        const uint32_t tp = vp + r16(e + 0xa) * 8;
        for (uint32_t t = 0; t < c.ntri; ++t, ++*tris)
            for (uint32_t q = 0; q < 4; ++q)
                if (m.triangle(c, t)[q] != ee_read<uint8_t>(tp + t * 4 + q)) no("chunk %u triangle %u: scene %u, EE %u", ui, t, m.triangle(c, t)[q], ee_read<uint8_t>(tp + t * 4 + q));
    }
    return bad;
}

void check_tree() {
    const char* name = "scene_tree";
    if (!selected(name)) return;
    level();
    const uint32_t tree = coll_tree();
    const auto t0 = std::chrono::steady_clock::now();
    const std::shared_ptr<const CollTreeScene> s = CollTreeScene::parse(tree);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (s == nullptr) {
        fail(name, "the tree at " + hex(tree) + " gives no scene");
        return;
    }
    size_t cells = 0;
    std::string why;
    if (const int bad = tree_compare(*s, tree, &cells, &why); bad != 0) {
        fail(name, std::to_string(bad) + " of " + std::to_string(cells) + " cells differ; first: " + why);
        return;
    }
    const std::vector<CollTreeScene::WalkCell> walk = s->cells_walk();
    if (walk.size() != cells || walk.empty()) {
        fail(name, "the scene walks " + std::to_string(walk.size()) + " cells, EE memory " + std::to_string(cells));
        return;
    }
    for (const CollTreeScene::WalkCell& c : walk) {
        if (collide_detail::coll_tree_cell(tree, c.x, c.y, c.z) != c.word) {
            fail(name, "the scene's walk names a cell EE memory does not have");
            return;
        }
    }
    if (std::string w; !s->same_as(*CollTreeScene::parse(tree), &w)) {
        fail(name, "a second parse differs: " + w);
        return;
    }
    // lookups at random coordinates: inside and around the rows, negative, past the grid
    const int32_t z0 = static_cast<int32_t>(r16(tree)), zn = static_cast<int32_t>(r16(tree + 2));
    int looked = 0, found = 0;
    for (int k = 0; k < 20000; ++k) {
        int32_t x, y, z;
        if (k % 10 == 0) {
            x = static_cast<int32_t>(R());
            y = static_cast<int32_t>(R());
            z = static_cast<int32_t>(R());
        } else if (k % 10 == 1) {
            x = ri(-300, 1300);
            y = ri(-300, 1300);
            z = ri(-300, 1300);
        } else if (k % 2 == 0) {  // near a cell
            const CollTreeScene::WalkCell& c = walk[static_cast<size_t>(ri(0, static_cast<int>(walk.size()) - 1))];
            x = c.x + ri(-2, 2);
            y = c.y + ri(-2, 2);
            z = c.z + ri(-2, 2);
        } else {
            z = ri(z0 - 3, z0 + zn + 3);
            x = ri(-3, 260);
            y = ri(-3, 260);
        }
        ++looked;
        found += s->cell(x, y, z).word != 0 ? 1 : 0;
        if (!cell_same(*s, tree, x, y, z, &why)) {
            fail(name, "lookup " + std::to_string(k) + ": " + why);
            return;
        }
    }
    // the comparison finds a fault: one fetched word of a cell changed in EE memory (then put back)
    const CollTreeScene::WalkCell& c = walk[walk.size() / 2];
    if ((c.word & 0xffu) != 0) {
        const uint32_t a = (((c.word >> 8) + tree) & ~15u) + 4 * static_cast<uint32_t>(ri(0, static_cast<int>((c.word & 0xffu) * 4) - 1));
        const uint32_t keep = r32(a);
        w32(a, keep ^ 0x00010000u);
        size_t n2 = 0;
        std::string w2;
        const int bad = tree_compare(*s, tree, &n2, &w2);
        w32(a, keep);
        if (bad < 1) {
            fail(name, "a fault at " + hex(a) + " not found");
            return;
        }
    }
    std::printf("%-26s ok: tree 0x%08x, %zu cells, %zu rows, %zu words, %zu source bytes, parsed in %.2f ms; %d lookups (%d cells); the fault found\n", name, tree,
                cells, s->row_count(), s->word_count(), s->source_bytes(), ms, looked, found);
}

void check_mesh() {
    const char* name = "scene_mesh";
    if (!selected(name)) return;
    level();
    const uint32_t list = r32(OV(0x0015f618));
    const auto t0 = std::chrono::steady_clock::now();
    const std::shared_ptr<const CollMeshScene> m = CollMeshScene::parse(list);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (m == nullptr) {
        if (list < 0x1000u) {
            std::printf("%-26s ok: no list (0x%08x): no scene, the routine reads EE memory\n", name, list);
            return;
        }
        fail(name, "the list at " + hex(list) + " gives no scene");
        return;
    }
    size_t verts = 0, tris = 0;
    std::string why;
    if (const int bad = mesh_compare(*m, list, &verts, &tris, &why); bad != 0) {
        fail(name, std::to_string(bad) + " differences; first: " + why);
        return;
    }
    if (std::string w; !m->same_as(*CollMeshScene::parse(list), &w)) {
        fail(name, "a second parse differs: " + w);
        return;
    }
    if (m->count() > 0) {  // the fault: one vertex coordinate changed in EE memory
        const uint32_t e = list + 0x10 + static_cast<uint32_t>(ri(0, m->count() - 1)) * 0x10;
        const uint32_t a = r32(e + 0xc) + 2;
        const uint32_t keep = r16(a);
        w16(a, keep ^ 0x40u);
        size_t v2 = 0, t2 = 0;
        std::string w2;
        const int bad = mesh_compare(*m, list, &v2, &t2, &w2);
        w16(a, keep);
        if (bad < 1) {
            fail(name, "a fault at " + hex(a) + " not found");
            return;
        }
    }
    std::printf("%-26s ok: list 0x%08x, %d chunks, %zu vertices, %zu triangles, %zu source bytes, parsed in %.2f ms%s\n", name, list, m->count(), verts, tris,
                m->source_bytes(), ms, m->count() > 0 ? "; the fault found" : "");
}

// ---- the same query with RAC1_COLL_SCENE=ee and =native
uint32_t first_moby() { return ee_read<uint32_t>(OV(0x0015ffd8)); }
uint32_t end_moby() { return ee_read<uint32_t>(OV(0x0015ffe0)); }
std::vector<uint32_t> live_mobys() {
    std::vector<uint32_t> v;
    for (uint32_t m = first_moby(); m < end_moby(); m += 0x100)
        if (ee_read<uint8_t>(m + 0x20) < 0xfe && ee_read<uint32_t>(m + 0x24) != 0) v.push_back(m);
    return v;
}
Vec4 around(const Vec4& at, float r0, float r1, float z0, float z1) {
    const float r = rf(r0, r1), a = rf(-3.1f, 3.1f);
    return Vec4{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z + rf(z0, z1), at.w};
}
// 0x40 bytes for sphere_mesh_collide's position: the test area, or below the EE stack where the level's moby table covers
// it (Kalebo III), as batch 530's
uint32_t scratch() {
    if (end_moby() <= TEST_AREA || first_moby() >= TEST_AREA + TEST_AREA_SIZE) return TEST_AREA;
    return 0x01ffe000u;
}

void check_queries() {
    const char* name = "scene_queries";
    if (!selected(name)) return;
    level();
    const int n = std::getenv("B540_N") != nullptr ? std::atoi(std::getenv("B540_N")) : 300;
    const uint32_t hero = ee_read<uint32_t>(0x001413d0);
    const uint32_t S = scratch();
    uint8_t* ram = ee_get_ram_base();
    uint8_t* spr = hp<uint8_t>(0x70000000u);
    std::vector<uint8_t> ee_ram, ee_spr;
    const bool check_was = coll_scene_check(), native_was = coll_scene_native();
    coll_scene_set_check(false);
    const uint64_t parses0 = coll_scene_stats().parses;
    (void)coll_scene_tree(coll_tree());  // (parsed here, outside the timed queries)
    (void)coll_scene_mesh(r32(OV(0x0015f618)));
    int kinds[4] = {0, 0, 0, 0}, hits[4] = {0, 0, 0, 0}, bad = 0;
    double us[2][4] = {};  // (each kind's time, ee and native, microseconds)
    std::string first;
    for (int k = 0; k < n; ++k) {
        level();
        const std::vector<uint32_t> ms = live_mobys();
        const uint32_t from_m = ms.empty() || coin() ? hero : ms[static_cast<size_t>(ri(0, static_cast<int>(ms.size()) - 1))];
        const int kind = k % 4;
        Vec4 at = around(ee_ref<Vec4>(from_m + 0x10), 0.0f, 3.0f, -0.5f, 2.0f);
        const uint32_t list = r32(OV(0x0015f618));
        if (kind == 3 && list >= 0x1000u && static_cast<int32_t>(r32(list)) > 0 && ri(0, 3) != 0) {  // near a mesh chunk (1/64 units)
            const uint32_t e = list + 0x10 + static_cast<uint32_t>(ri(0, static_cast<int32_t>(r32(list)) - 1)) * 0x10;
            const float rad = static_cast<float>(r16(e + 6)) / 64.0f;
            at = Vec4{static_cast<float>(r16(e)) / 64.0f + rf(-rad, rad), static_cast<float>(r16(e + 2)) / 64.0f + rf(-rad, rad),
                      static_cast<float>(r16(e + 4)) / 64.0f + rf(-rad, rad), 1.0f};
        }
        const uint32_t fl = ri(0, 3) != 0 ? 0u : (ri(0, 2) == 0 ? 2u : (coin() ? 0x10u : 1u));
        const uint32_t ignore = coin() ? from_m : 0u;
        Vec4 to = around(at, 1.0f, ri(0, 3) == 0 ? 120.0f : 25.0f, -8.0f, 8.0f);
        const float r = rf(0.2f, kind == 3 ? 4.0f : 2.5f), h = rf(0.1f, 2.0f);
        auto run = [&]() -> int32_t {
            switch (kind) {
            case 0: return segment_collide_v(at, to, fl, ignore, nullptr);
            case 1: return sphere_collide_v(at, r, fl & ~0x10u, ignore);
            case 2: return capsule_collide_v(at, r, h, fl & ~0x10u, ignore);
            default:
                ee_ref<Vec4>(S) = at;
                return sphere_mesh_collide(S, r);
            }
        };
        coll_scene_set_native(false);
        auto t0 = std::chrono::steady_clock::now();
        const int32_t r_ee = run();
        us[0][kind] += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        float vu_ee[15];
        collide_vu_save(vu_ee);
        ee_ram.assign(ram, ram + 0x02000000);
        ee_spr.assign(spr, spr + 0x4000);
        level();
        coll_scene_set_native(true);
        t0 = std::chrono::steady_clock::now();
        const int32_t r_nat = run();
        us[1][kind] += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        float vu_nat[15];
        collide_vu_save(vu_nat);
        ++kinds[kind];
        hits[kind] += r_ee != 0 ? 1 : 0;
        std::string why;
        if (r_ee != r_nat) why = "return " + std::to_string(r_ee) + " / " + std::to_string(r_nat);
        else if (std::memcmp(vu_ee, vu_nat, sizeof vu_ee) != 0) why = "the contact registers";
        else if (std::memcmp(ee_spr.data(), spr, 0x4000) != 0) why = "the scratchpad";
        else if (std::memcmp(ee_ram.data(), ram, 0x02000000) != 0) {
            uint32_t a = 0;
            while (a < 0x02000000u && std::memcmp(ee_ram.data() + a, ram + a, 4) == 0) a += 4;
            why = "RAM at " + hex(a);
        }
        if (!why.empty() && bad++ == 0) first = "query " + std::to_string(k) + " (kind " + std::to_string(kind) + ", flags " + hex(fl) + "): " + why;
    }
    coll_scene_set_native(native_was);
    coll_scene_set_check(check_was);
    const uint64_t parses = coll_scene_stats().parses - parses0;
    level();
    if (bad != 0) {
        fail(name, std::to_string(bad) + " of " + std::to_string(n) + " queries differ; first: " + first);
        return;
    }
    if (parses == 0 || coll_scene_tree(coll_tree()) == nullptr) {
        fail(name, "the native runs had no scene for the tree");
        return;
    }
    std::printf("%-26s ok: %d queries equal (segment %d / %d hits, sphere %d / %d, capsule %d / %d, mesh %d / %d)\n", name, n, kinds[0], hits[0], kinds[1], hits[1],
                kinds[2], hits[2], kinds[3], hits[3]);
    // (the scene parsed before the first query: the native times are the lookups' alone)
    std::printf("  (a query's time, ee / native, microseconds: segment %.2f / %.2f, sphere %.2f / %.2f, capsule %.2f / %.2f, mesh %.2f / %.2f)\n",
                us[0][0] / kinds[0], us[1][0] / kinds[0], us[0][1] / kinds[1], us[1][1] / kinds[1], us[0][2] / kinds[2], us[1][2] / kinds[2], us[0][3] / kinds[3],
                us[1][3] / kinds[3]);
}

// ---- the kept parts and their keys, on two small trees in the test area
// a tree of one z row, one y row and one x row of `cells` cells of 2 quadwords at `at` (0x400 bytes), its vertices' words
// seeded with `seed`
void small_tree(uint32_t at, int cells, uint32_t seed) {
    for (uint32_t i = 0; i < 0x100; ++i) w32(at + i * 4, 0);
    w16(at, 10);       // z first
    w16(at + 2, 1);    // z count
    w16(at + 4, 0x10 >> 2);
    w16(at + 0x10, 20);  // y row at +0x10
    w16(at + 0x12, 1);
    w32(at + 0x14, 0x20);
    w16(at + 0x20, 30);  // x row at +0x20
    w16(at + 0x22, static_cast<uint32_t>(cells));
    for (int c = 0; c < cells; ++c) {
        const uint32_t cell = 0x100 + static_cast<uint32_t>(c) * 0x20;
        w32(at + 0x24 + static_cast<uint32_t>(c) * 4, (cell << 8) | 2);
        w32(at + cell, 0x00000100u | (1u << 16));  // 1 triangle, 1 vertex (the fetched words are what matters here)
        for (uint32_t k = 1; k < 8; ++k) w32(at + cell + k * 4, seed * 0x9e3779b1u + static_cast<uint32_t>(c) * 131u + k);
    }
}

void check_keys() {
    const char* name = "scene_keys";
    if (!selected(name)) return;
    level();
    const bool check_was = coll_scene_check(), native_was = coll_scene_native();
    coll_scene_set_native(true);
    coll_scene_set_check(false);
    const uint32_t A = TEST_AREA + 0x8000, B = TEST_AREA + 0x9000;
    small_tree(A, 3, 1);
    small_tree(B, 4, 2);
    auto parses = [] { return coll_scene_stats().parses; };
    auto cell_ok = [](uint32_t tree, int c) {
        collide_detail::CollTreeSrc src(tree);
        std::string why;
        return src.scene != nullptr && cell_same(*src.scene, tree, 30 + c, 20, 10, &why);
    };
    std::string err;
    coll_set_tree(A);
    uint64_t p0 = parses();
    const std::shared_ptr<const CollTreeScene> a1 = coll_scene_tree(A);
    if (a1 == nullptr || parses() != p0 + 1) err = "A: no parse";
    coll_set_tree(B);
    const std::shared_ptr<const CollTreeScene> b1 = coll_scene_tree(B);
    if (err.empty() && (b1 == nullptr || parses() != p0 + 2)) err = "B: no parse";
    coll_set_tree(A);
    const std::shared_ptr<const CollTreeScene> a2 = coll_scene_tree(A);
    if (err.empty() && (a2 != a1 || parses() != p0 + 2)) err = "A again: not the kept scene";
    // A's bytes changed while B is current (a section streamed into a freed block): parsed again at the switch to A
    coll_set_tree(B);
    (void)coll_scene_tree(B);
    small_tree(A, 3, 7);
    p0 = parses();
    coll_set_tree(A);
    const std::shared_ptr<const CollTreeScene> a3 = coll_scene_tree(A);
    if (err.empty() && (a3 == a1 || parses() != p0 + 1 || !cell_ok(A, 1))) err = "A changed behind a switch: not parsed again";
    // level_collision_set's drop: the same pointer parsed again
    p0 = parses();
    coll_scene_level_changed();
    const std::shared_ptr<const CollTreeScene> a4 = coll_scene_tree(A);
    if (err.empty() && (a4 == a3 || parses() != p0 + 1)) err = "after a level's drop: not parsed again";
    // an edit in place, no switch: without the check mode the kept scene stays (the stated limit) ...
    w32(A + 0x100 + 0x20 + 8, r32(A + 0x100 + 0x20 + 8) ^ 0x1234u);
    const bool kept_off = !cell_ok(A, 1);  // (true: the kept scene, not the edit)
    // ... with it the next lookup finds it and parses again
    coll_scene_set_check(true);
    const uint64_t stale0 = coll_scene_stats().stale;
    {
        collide_detail::CollTreeSrc src(A);
        const CollTreeScene::Cell c = src.cell(31, 20, 10);
        std::string why;
        if (err.empty() && (coll_scene_stats().stale != stale0 + 1 || src.scene == nullptr || !cell_same(*src.scene, A, 31, 20, 10, &why) || c.word == 0))
            err = "an edit in place with the check mode: not found";
    }
    if (err.empty() && coll_scene_stats().parse_errors != 0) err = "a parse error was counted";
    coll_scene_set_check(check_was);
    coll_scene_set_native(native_was);
    level();
    coll_scene_level_changed();  // (the test trees' parts dropped)
    if (!err.empty()) {
        fail(name, err);
        return;
    }
    std::printf("%-26s ok: A / B / A kept, a changed block parsed again at its switch, a level's drop parsed again, an edit in place found by the check mode (%s without it)\n",
                name, kept_off ? "not seen" : "seen");
}

}  // namespace

void batch_540() {
    if (g_only_batch != 540) return;
    g_batch = 540;
    std::printf("batch 540: the collision scene on level %d\n", g_oracle_level);
    check_tree();
    check_mesh();
    check_queries();
    check_keys();
    const CollSceneStats st = coll_scene_stats();
    std::printf("  (the scene: %llu parses in %.1f ms, %llu kept parts taken again, %llu found stale, %llu parse errors, %llu pointers not held)\n",
                static_cast<unsigned long long>(st.parses), st.parse_ms, static_cast<unsigned long long>(st.reused), static_cast<unsigned long long>(st.stale),
                static_cast<unsigned long long>(st.parse_errors), static_cast<unsigned long long>(st.unheld));
    if (st.parse_errors != 0) fail("scene_parse_errors", std::to_string(st.parse_errors) + " parses differed from EE memory right after them");
}

}  // namespace rac1::oracle
