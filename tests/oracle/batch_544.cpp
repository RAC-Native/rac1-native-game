// Oracle batch 544 (#150 stage 5, task LD2): the grids' host copies (src/game/level/level_grids.h) against the lookups' EE
// reads - a host check at the boundary, no original code run. On the level after its setup (level_setup_body):
//  - grids_zone_cells: the zone grid *(0x15f600)'s copy; every cell of its extent walked from the EE bytes here (each z of
//    the z row and one past either end, each y of its y row and one past, each x of its x row and one past) and 20000
//    coordinates at random (around the grid, far outside, the int32 ends): zone_grid_cell 0x1f7278 with RAC1_LD_GRIDS=ee and
//    =native (the check mode off: the copy alone) gives the same mask address; every mask found equals EE memory's bytes as
//    mem_copy128 reads them; a fault put into EE memory (a cell's entry, a mask's word) is found by the check mode / the
//    comparison, then put back
//  - grids_zones_update: 200 cases of zones_from_camera 0x1f7408 (the camera in or about the grid's cells or anywhere, the
//    modes 0x15f608 and 0x16c06c, the last list 0x15f610 none, the scratch list or one of the grid's masks, the octant box
//    *0x15f604 or none) run with `ee` and `native` from the same RAM: the same RAM after
//  - grids_cam_cells: the camera-volume grid *(0x15ef80)'s copy: its 4161 cell words (x, y up to 64), each held list's count
//    and records (+0 .. +0x14) equal EE memory; the cells not held counted; the sphere indices cam0_volume_pass 0x2e9aa0 takes,
//    walked from EE here, equal the copy's
//  - grids_cam_find: cam_volume_find 0x1ee1b0 on a lattice of positions covering the 64 x 64 cells and one cell past every
//    side (each cell's 0, 4, 8, 12 and 15.99 units, so every edge), each with r = 0, 0.25 and 3 and z about a record of the
//    cell, with `ee` and `native`: the same record address; 2000 more with the check mode on; cam0_volume_pass both ways
//  - grids_cam_synthetic: no level's grid has records in its 64 x 64 cells (grids_cam_cells: the lookups find records only
//    in the 65 words past the table, from EE memory): a grid in the test area whose records name the level's own shapes
//    (types 3 boxes, 5 spheres, 6 cylinders, 7 volumes, and a type no test knows), each in the cell of its shape's centre;
//    cam_volume_find about each shape (30 points, r = 0, 0.25 and 3) both ways, a tenth with the check mode; the pass's
//    spheres and RAM both ways
//  - grids_keys (the check mode off, then on): small grids of both kinds in the test area, A and B: A, B, A again takes the
//    kept A (no parse); A's bytes changed while B is current: parsed again at the switch to A, the lookup as EE's; a level's
//    start (ld_grids_level_started): parsed again for the same pointer; an edit in place with no switch: with the check mode
//    found at the next lookup (parsed again), without it the kept copy is used (the limit level_grids.h states: printed, not
//    a failure). The RAM is put back.
//  - grids_title (level 0 only, run alone: ORACLE_ONLY=grids_title): the title's word *(0x15f600) (batch 89's state: the boot module started, the title
//    loaded and 70 passes of its loop; the front end keeps a level number there, front_end.cpp): no grid, so no copy is held
//    and the lookups read EE memory, the same values (200 lookups); a grid there would be compared as grids_zone_cells'.
// No section switch: a level-built state has no session; the arena mod's section test with RAC1_LD_GRIDS_CHECK=1 covers the
// sections' grids (followed by their pointers).
// Run: ORACLE_LEVEL=N build/tests/ovl_oracle --batch 544, N = 0 .. 18.

#include "ovl_oracle.h"

#include <game/boot/boot_start.h>
#include <game/boot/front_end.h>
#include <game/boot/front_image.h>
#include <game/boot/title.h>
#include <game/level/level_data.h>
#include <game/level/level_grids.h>
#include <game/lib/libmc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/render_toggles.h>
#include <runtime/kernel/disc.h>
#include <runtime/sif/sif_rpc.h>

#include <climits>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

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
uint32_t rd16(uint32_t a) { return ee_read<uint16_t>(a); }
uint32_t rd32(uint32_t a) { return ee_read<uint32_t>(a); }

// the level after its setup (no frame yet), kept and put back
std::vector<uint8_t> g_snap;
void level() {
    uint8_t* ram = ee_get_ram_base();
    if (g_snap.empty()) {
        veldin_boot();
        level_setup_body(1, 0);
        g_snap.assign(ram, ram + 0x02000000);
    } else {
        std::memcpy(ram, g_snap.data(), g_snap.size());
    }
}

uint32_t zone_cell_as(bool native, int32_t x, int32_t y, int32_t z) {
    ld_grids_set_native(native);
    const uint32_t r = zone_grid_cell(x, y, z);
    ld_grids_set_native(true);
    return r;
}

struct XYZ {
    int32_t x, y, z;
};

// the zone grid at `g` walked from the EE bytes (zone_grid_cell's offsets): every coordinate of its rows and one past either
// end of each row
std::vector<XYZ> zone_extent(uint32_t g) {
    std::vector<XYZ> out;
    const int32_t z0 = static_cast<int32_t>(rd16(g + 4)), zn = static_cast<int32_t>(rd16(g + 6));
    for (int32_t zi = -1; zi <= zn; ++zi) {
        const uint32_t zr = zi >= 0 && zi < zn ? rd16(g + 8 + static_cast<uint32_t>(zi) * 2) : 0;
        if (zr == 0) {
            out.push_back(XYZ{0, 0, z0 + zi});
            continue;
        }
        const uint32_t Y = g + zr * 4;
        const int32_t y0 = static_cast<int32_t>(rd16(Y)), yn = static_cast<int32_t>(rd16(Y + 2));
        for (int32_t yi = -1; yi <= yn; ++yi) {
            const uint32_t yo = yi >= 0 && yi < yn ? rd16(Y + 4 + static_cast<uint32_t>(yi) * 2) : 0;
            if (yo == 0) {
                out.push_back(XYZ{0, y0 + yi, z0 + zi});
                continue;
            }
            const uint32_t X = g + yo * 4;
            const int32_t x0 = static_cast<int32_t>(rd16(X)), xn = static_cast<int32_t>(rd16(X + 2));
            for (int32_t xi = -1; xi <= xn; ++xi) out.push_back(XYZ{x0 + xi, y0 + yi, z0 + zi});
        }
    }
    return out;
}

// the extent and `nrand` random coordinates: ee against native; the masks found against EE memory. Returns false on a failure
bool zone_compare(const char* name, uint32_t g, int nrand, std::vector<XYZ>* cells_out) {
    const std::shared_ptr<const ZoneGridCopy> c = ZoneGridCopy::parse(g);
    if (c == nullptr) {
        int bad = 0;
        for (int i = 0; i < 200; ++i) {
            const int32_t x = ri(-100, 400), y = ri(-100, 400), z = ri(-100, 400);
            if (zone_cell_as(false, x, y, z) != zone_cell_as(true, x, y, z)) ++bad;
        }
        if (bad != 0) return fail(name, "no copy for " + hex(g) + ", and " + std::to_string(bad) + " lookups differ");
        std::printf("%-26s ok: no copy for 0x%08x (not held): the lookups read EE memory (200 equal)\n", name, g);
        return true;
    }
    std::vector<XYZ> pts = zone_extent(g);
    const size_t walked = pts.size();
    int32_t lo[3] = {INT_MAX, INT_MAX, INT_MAX}, hi[3] = {INT_MIN, INT_MIN, INT_MIN};
    for (const XYZ& p : pts) {
        const int32_t v[3] = {p.x, p.y, p.z};
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], v[k]);
            hi[k] = std::max(hi[k], v[k]);
        }
    }
    const int32_t ends[] = {INT_MIN, INT_MIN + 1, -70000, -1, 0, 65535, 65536, 70000, INT_MAX};
    for (int i = 0; i < nrand; ++i) {
        XYZ p{};
        int32_t* v[3] = {&p.x, &p.y, &p.z};
        for (int k = 0; k < 3; ++k) {
            const int m = ri(0, 9);
            *v[k] = m < 7 ? ri(lo[k] - 8, hi[k] + 8) : m < 9 ? ri(-100000, 100000) : ends[ri(0, 8)];
        }
        pts.push_back(p);
    }
    int bad = 0, found = 0, mbad = 0;
    std::string why;
    for (const XYZ& p : pts) {
        const uint32_t e = zone_cell_as(false, p.x, p.y, p.z), n = zone_cell_as(true, p.x, p.y, p.z);
        if (e != n) {
            if (bad++ == 0) why = "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z) + "): EE " + hex(e) + ", copy " + hex(n);
            continue;
        }
        if (e == 0) continue;
        ++found;
        if (cells_out != nullptr) cells_out->push_back(p);
        const uint8_t* m = c->mask(e);
        if (m == nullptr || std::memcmp(m, ee_get_ram_base() + (e & ~15u), 0x80) != 0) {
            if (mbad++ == 0 && why.empty()) why = "mask " + hex(e) + (m == nullptr ? " not in the copy" : " differs from EE memory");
        }
    }
    if (bad != 0 || mbad != 0) return fail(name, std::to_string(bad) + " lookups and " + std::to_string(mbad) + " masks differ; first " + why);
    // the faults: a cell's entry changed (the check mode's walk finds it), a mask's word (the comparison finds it)
    bool entry_found = true, mask_found = true;
    for (const XYZ& p : pts) {
        if (zone_cell_as(false, p.x, p.y, p.z) == 0) continue;
        // (the x entry's address: the walk again)
        const uint32_t zr = rd16(g + 8 + static_cast<uint32_t>(p.z - static_cast<int32_t>(rd16(g + 4))) * 2), Y = g + zr * 4;
        const uint32_t X = g + rd16(Y + 4 + static_cast<uint32_t>(p.y - static_cast<int32_t>(rd16(Y))) * 2) * 4;
        const uint32_t ea = X + 4 + static_cast<uint32_t>(p.x - static_cast<int32_t>(rd16(X))) * 2, ev = rd16(ea);
        w16(ea, ev ^ 1u);
        bool b = false;
        c->cell<true>(p.x, p.y, p.z, &b);
        entry_found = b;
        w16(ea, ev);
        const uint32_t ma = zone_cell_as(false, p.x, p.y, p.z) & ~15u, mv = rd32(ma + 0x40);
        w32(ma + 0x40, ~mv);
        mask_found = std::memcmp(c->mask(zone_cell_as(false, p.x, p.y, p.z)), ee_get_ram_base() + ma, 0x80) != 0;
        w32(ma + 0x40, mv);
        break;
    }
    if (!entry_found || !mask_found) return fail(name, std::string("a fault not found: ") + (!entry_found ? "a cell's entry" : "a mask's word"));
    std::printf("%-26s ok: grid 0x%08x, %zu rows, %zu cells, %d masks, %zu source bytes; %zu walked + %d random lookups equal (%d masks found, "
                "equal to EE memory); the faults found\n", name, g, c->row_count(), c->cell_count(), c->mask_count(), c->source_bytes(), walked, nrand, found);
    return true;
}

void check_zone_cells(std::vector<XYZ>* cells) {
    const char* name = "grids_zone_cells";
    if (!selected(name)) return;
    level();
    zone_compare(name, lvd::zone_grid(), 20000, cells);
}

void check_zones_update(const std::vector<XYZ>& cells) {
    const char* name = "grids_zones_update";
    if (!selected(name)) return;
    level();
    const uint32_t g = lvd::zone_grid();
    const std::shared_ptr<const ZoneGridCopy> c = ZoneGridCopy::parse(g);
    uint8_t* ram = ee_get_ram_base();
    std::vector<uint8_t> in, out;
    int bad = 0, kinds[3] = {0, 0, 0};
    std::string why;
    for (int i = 0; i < 200; ++i) {
        level();
        float pos[3];
        if (!cells.empty() && ri(0, 4) != 0) {
            const XYZ& p = cells[static_cast<size_t>(ri(0, static_cast<int32_t>(cells.size()) - 1))];
            const int32_t v[3] = {p.x, p.y, p.z};
            for (int k = 0; k < 3; ++k) pos[k] = static_cast<float>(v[k]) * 4.0f + rf(-3.0f, 7.0f);
        } else {
            for (float& f : pos) f = rf(-200.0f, 1200.0f);
        }
        for (int k = 0; k < 3; ++k) wf(OV(0x00166dc0) + 4 * u(k), pos[k]);
        w32(OV(0x0015f608), ri(0, 3) == 0 ? u(ri(0, 3)) : 0u);
        w32(OV(0x0016c06c), ri(0, 5) == 0 ? 1u : 0u);
        const int last = ri(0, 2);
        uint32_t lv = 0;
        if (last == 1) lv = OV(0x00173d80u);
        if (last == 2 && c != nullptr && c->mask_count() > 0) lv = c->masks() + u(ri(0, c->mask_count() - 1)) * 0x80;
        w32(OV(0x0015f610), lv);
        wrand(OV(0x00173d80), 0x20);
        if (coin()) {
            w32(OV(0x0015f604), M);
            wfrand(M, 3, -100, 400);
            wrand(M + 0x10, 0x100);
        } else {
            w32(OV(0x0015f604), 0);
        }
        in.assign(ram, ram + 0x02000000);
        ld_grids_set_native(false);
        zones_from_camera();
        ld_grids_set_native(true);
        out.assign(ram, ram + 0x02000000);
        std::memcpy(ram, in.data(), in.size());
        ld_grids_set_check(false);
        zones_from_camera();
        ld_grids_set_check(true);
        ++kinds[lw(OV(0x0015f60c)) == 0 ? 0 : lwu(OV(0x0015f610)) == OV(0x00173d80u) ? 1 : 2];
        if (std::memcmp(ram, out.data(), out.size()) != 0) {
            if (bad++ == 0) {
                size_t k = 0;
                while (ram[k] == out[k]) ++k;
                why = "case " + std::to_string(i) + ": RAM differs first at " + hex(static_cast<uint32_t>(k));
            }
        }
    }
    level();
    if (bad != 0) {
        fail(name, std::to_string(bad) + " of 200 cases differ; first " + why);
        return;
    }
    std::printf("%-26s ok: 200 cases equal (%d in a cell, %d from the neighbours, %d by the mode)\n", name, kinds[0], kinds[1], kinds[2]);
}

void check_cam_cells() {
    const char* name = "grids_cam_cells";
    if (!selected(name)) return;
    level();
    const uint32_t tab = lvd::cam_volume_grid();
    const std::shared_ptr<const CamGridCopy> c = CamGridCopy::parse(tab);
    if (c == nullptr) {
        std::printf("%-26s ok: no copy for 0x%08x (not held): the lookups read EE memory\n", name, tab);
        return;
    }
    int bad = 0;
    std::string why;
    auto diff = [&](const std::string& w) {
        if (bad++ == 0) why = w;
    };
    for (int32_t k = 0; k < CamGridCopy::kCells; ++k) {
        const uint32_t a = tab + u(k) * 4;
        if (c->word(k) != rd32(a)) diff("cell " + std::to_string(k) + " word");
        const int32_t li = c->list_of(k);
        if ((rd32(a) == 0) != (li == -1)) diff("cell " + std::to_string(k) + " none or not");
        if (li < 0) continue;
        const CamGridCopy::List& L = c->list(li);
        if (L.ee != rd32(a) + tab || L.count != static_cast<int32_t>(rd32(L.ee))) diff("cell " + std::to_string(k) + " list");
        for (int32_t i = 0; i < L.count; ++i) {
            const uint32_t e = L.ee + 0x10 + u(i) * 0x30;
            const CamGridCopy::Record& R = c->record(L, i);
            for (uint32_t w = 0; w < 4; ++w)
                if (R.w[w] != rd32(e + w * 4)) diff("record " + hex(e) + " word " + std::to_string(w));
            if (R.type != static_cast<int32_t>(rd32(e + 0x10)) || R.index != static_cast<int32_t>(rd32(e + 0x14))) diff("record " + hex(e) + " type / index");
        }
    }
    // cam0_volume_pass's spheres, walked from EE memory
    std::vector<int32_t> spheres;
    for (int32_t k = 0; k < 64 * 64; ++k) {
        const uint32_t off = rd32(tab + u(k) * 4);
        if (off == 0) continue;
        const uint32_t list = off + tab;
        for (int32_t i = 0; i < static_cast<int32_t>(rd32(list)); ++i)
            if (static_cast<int32_t>(rd32(list + 0x10 + u(i) * 0x30 + 0x10)) == 5) spheres.push_back(static_cast<int32_t>(rd32(list + 0x10 + u(i) * 0x30 + 0x14)));
    }
    if (c->all_held() && spheres != c->pass_spheres()) diff("the pass's spheres");
    if (bad != 0) {
        fail(name, std::to_string(bad) + " differences; first " + why);
        return;
    }
    std::string unheld;  // (the first cells not held: their word, the list's address and count word)
    for (int32_t k = 0, n = 0; k < CamGridCopy::kCells && n < 3; ++k) {
        if (c->list_of(k) != -2) continue;
        ++n;
        const uint32_t l = c->word(k) + tab;
        unheld += " cell " + std::to_string(k) + " word " + hex(c->word(k)) + " list " + hex(l) + (l < 0x02000000u - 4 ? " count " + hex(rd32(l)) : "") + ";";
    }
    std::printf("%-26s ok: grid 0x%08x, %zu lists, %zu records, %d cells not held (read EE)%s %zu source bytes; the pass's %zu spheres equal\n", name, tab,
                c->list_count(), c->record_count(), c->unheld_cells(), unheld.empty() ? "," : (":" + unheld).c_str(), c->source_bytes(), spheres.size());
}

void check_cam_find() {
    const char* name = "grids_cam_find";
    if (!selected(name)) return;
    level();
    const uint32_t tab = lvd::cam_volume_grid();
    const std::shared_ptr<const CamGridCopy> c = CamGridCopy::parse(tab);
    const float hz = ee_read<float>(0x0013f3d8);  // (the hero's z)
    const float offs[] = {0.0f, 4.0f, 8.0f, 12.0f, 15.99f};
    const float rs[] = {0.0f, 0.25f, 3.0f};
    int n = 0, bad = 0, hits = 0, nchk = 0, threw = 0;
    std::string why;
    for (int32_t cy = -1; cy <= 65; ++cy) {
        for (int32_t cx = -1; cx <= 65; ++cx) {
            for (const float oy : offs) {
                for (const float ox : offs) {
                    Vec4 p{static_cast<float>(cx) * 16.0f + ox, static_cast<float>(cy) * 16.0f + oy, hz, 1.0f};
                    // z about one of the cell's records, when it has some
                    if (c != nullptr && cx >= 0 && cy >= 0 && cx <= 64 && cy <= 64) {
                        const int32_t li = c->list_of(cy * 64 + cx);
                        if (li >= 0 && c->list(li).count > 0) {
                            const CamGridCopy::List& L = c->list(li);
                            p.z = u2f(c->record(L, ri(0, L.count - 1)).w[2]) + rf(-8.0f, 8.0f);
                        }
                    }
                    for (const float r : rs) {
                        // (a cell past the table may name a list outside RAM: the EE read throws, on both sides alike)
                        auto find = [&p, r]() -> uint32_t {
                            try {
                                return cam_volume_find(p, r);
                            } catch (const std::exception&) {
                                return 0xdeadbeefu;
                            }
                        };
                        ld_grids_set_native(false);
                        const uint32_t e = find();
                        ld_grids_set_native(true);
                        const bool chk = ri(0, 199) == 0;
                        ld_grids_set_check(chk);
                        const uint32_t v = find();
                        ld_grids_set_check(true);
                        if (e == 0xdeadbeefu) ++threw;
                        nchk += chk ? 1 : 0;
                        ++n;
                        if (e != 0 && e != 0xdeadbeefu) ++hits;
                        if (e != v && bad++ == 0)
                            why = "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ", " + std::to_string(p.z) + ") r " + std::to_string(r) + ": EE " + hex(e) +
                                  ", copy " + hex(v);
                    }
                }
            }
        }
    }
    // cam0_volume_pass both ways: the same RAM (its sphere hook is empty in this build: the order is grids_cam_cells')
    uint8_t* ram = ee_get_ram_base();
    std::vector<uint8_t> out;
    ld_grids_set_native(false);
    cam0_volume_pass();
    ld_grids_set_native(true);
    out.assign(ram, ram + 0x02000000);
    cam0_volume_pass();
    const bool pass_same = std::memcmp(ram, out.data(), out.size()) == 0;
    if (bad != 0 || !pass_same) {
        fail(name, std::to_string(bad) + " of " + std::to_string(n) + " lookups differ" + (pass_same ? "" : ", the pass's RAM differs") + "; first " + why);
        return;
    }
    std::printf("%-26s ok: grid 0x%08x, %d lookups equal (%d found a record, %d with the check mode, %d read outside RAM on both sides), the pass equal\n", name,
                tab, n, hits, nchk, threw);
}

// small grids in the test area: zone grids at Z(i) (masks from +0x100), camera-volume grids at C(i) (cell (1, 2)'s list at +0x4200)
constexpr uint32_t kZone[2] = {TEST_AREA + 0x0000, TEST_AREA + 0x0400};
constexpr uint32_t kCam[2] = {TEST_AREA + 0x1000, TEST_AREA + 0x7000};
void make_zone(uint32_t g, uint32_t salt) {
    for (uint32_t a = 0; a < 0x200; a += 4) w32(g + a, 0);
    w32(g, 0x100);
    w16(g + 4, 10);
    w16(g + 6, 2);
    w16(g + 8, 0x10 / 4);
    w16(g + 0xa, 0);
    w16(g + 0x10, 20);
    w16(g + 0x12, 2);
    w16(g + 0x14, 0x20 / 4);
    w16(g + 0x16, 0x20 / 4);
    w16(g + 0x20, 30);
    w16(g + 0x22, 3);
    w16(g + 0x24, 0);
    w16(g + 0x26, 0xffff);
    w16(g + 0x28, 1);
    for (uint32_t a = 0; a < 0x100; a += 4) w32(g + 0x100 + a, a * 0x01010101u ^ salt);
}
void make_cam(uint32_t t, uint32_t salt) {
    for (uint32_t a = 0; a < 0x4300; a += 4) w32(t + a, 0);
    w32(t + (2 * 64 + 1) * 4, 0x4200);
    w32(t + 0x4200, 2);
    for (uint32_t i = 0; i < 2; ++i) {
        const uint32_t e = t + 0x4210 + i * 0x30;
        wf(e, 24.0f);
        wf(e + 4, 40.0f);
        wf(e + 8, static_cast<float>(salt));
        wf(e + 0xc, 0.5f);
        w32(e + 0x10, 99);  // (a type no test knows: never found)
        w32(e + 0x14, i);
    }
}

// the level's own camera volumes have no records in their 64 x 64 cells (every level: grids_cam_cells), so a grid of records
// naming the level's real shapes is laid in the test area: each record in the cell of its shape's centre (types 3, 5, 6, 7
// and one no test knows), its centre the shape's, radius 50; cam_volume_find about each shape both ways, the pass both ways
void check_cam_synthetic() {
    const char* name = "grids_cam_synthetic";
    if (!selected(name)) return;
    level();
    const uint32_t t = kCam[0];
    for (uint32_t a = t; a < TEST_AREA + TEST_AREA_SIZE; a += 4) w32(a, 0);
    struct Rec {
        float c[3];
        int32_t type, index;
    };
    std::vector<std::vector<Rec>> cells(64 * 64);
    const int32_t types[] = {3, 5, 6, 7, 2};
    std::vector<std::pair<int32_t, Rec>> placed;
    for (const int32_t type : types) {
        const int32_t n = type == 3 ? lvd::box_count(lvd::BOXES) : type == 5 ? lvd::box_count(lvd::SPHERES) : type == 6 ? lvd::box_count(lvd::CYLINDERS)
                          : type == 7 ? lvd::volume_count() : 8;
        for (int32_t k = 0; k < std::min(n, 12); ++k) {
            const int32_t idx = type == 2 ? k : ri(0, n - 1);
            const uint32_t sh = type == 3 ? lvd::box(lvd::BOXES, idx) : type == 5 ? lvd::box(lvd::SPHERES, idx) : type == 6 ? lvd::box(lvd::CYLINDERS, idx)
                                : type == 7 ? lvd::volume(idx) : 0u;
            Rec r{{rf(16.0f, 1000.0f), rf(16.0f, 1000.0f), rf(0.0f, 200.0f)}, type, idx};
            if (sh != 0)
                for (int i = 0; i < 3; ++i) r.c[i] = ee_read<float>(sh + 0x30 + 4 * u(i));
            const int32_t cx = float_to_int(r.c[0] * 0.0625f), cy = float_to_int(r.c[1] * 0.0625f);
            if (cx < 0 || cy < 0 || cx >= 64 || cy >= 64) continue;
            cells[static_cast<size_t>(cy * 64 + cx)].push_back(r);
            placed.push_back({cy * 64 + cx, r});
        }
    }
    uint32_t at = t + 0x4200;
    for (int32_t c = 0; c < 64 * 64; ++c) {
        const std::vector<Rec>& v = cells[static_cast<size_t>(c)];
        if (v.empty() || at + 0x10 + v.size() * 0x30 > TEST_AREA + TEST_AREA_SIZE) continue;
        w32(t + u(c) * 4, at - t);
        w32(at, u(static_cast<int32_t>(v.size())));
        for (size_t i = 0; i < v.size(); ++i) {
            const uint32_t e = at + 0x10 + u(static_cast<int32_t>(i)) * 0x30;
            for (int k = 0; k < 3; ++k) wf(e + 4 * u(k), v[i].c[k]);
            wf(e + 0xc, 50.0f);
            w32(e + 0x10, u(v[i].type));
            w32(e + 0x14, u(v[i].index));
        }
        at += 0x10 + u(static_cast<int32_t>(v.size())) * 0x30;
    }
    w32(OV(0x0015ef80), t);
    const std::shared_ptr<const CamGridCopy> cp = ld_grids_cam(t);
    int n = 0, bad = 0, hits = 0;
    int by_type[8] = {};
    std::string why;
    for (const auto& [cell, r] : placed) {
        for (int k = 0; k < 30; ++k) {
            const Vec4 p{r.c[0] + rf(-20.0f, 20.0f), r.c[1] + rf(-20.0f, 20.0f), r.c[2] + rf(-20.0f, 20.0f), 1.0f};
            for (const float rr : {0.0f, 0.25f, 3.0f}) {
                ld_grids_set_native(false);
                const uint32_t e = cam_volume_find(p, rr);
                ld_grids_set_native(true);
                const bool chk = ri(0, 9) == 0;
                ld_grids_set_check(chk);
                const uint32_t v = cam_volume_find(p, rr);
                ld_grids_set_check(true);
                ++n;
                if (e != 0) {
                    ++hits;
                    ++by_type[static_cast<int32_t>(rd32(e + 0x10)) & 7];
                }
                if (e != v && bad++ == 0) why = "type " + std::to_string(r.type) + " index " + std::to_string(r.index) + ": EE " + hex(e) + ", copy " + hex(v);
            }
        }
    }
    // the pass: the copy's spheres against the EE walk, and both ways the same RAM
    std::vector<int32_t> spheres;
    for (int32_t c = 0; c < 64 * 64; ++c) {
        const uint32_t off = rd32(t + u(c) * 4);
        if (off == 0) continue;
        for (int32_t i = 0; i < static_cast<int32_t>(rd32(off + t)); ++i)
            if (static_cast<int32_t>(rd32(off + t + 0x10 + u(i) * 0x30 + 0x10)) == 5) spheres.push_back(static_cast<int32_t>(rd32(off + t + 0x10 + u(i) * 0x30 + 0x14)));
    }
    const bool pass_list = cp != nullptr && cp->all_held() && cp->pass_spheres() == spheres;
    uint8_t* ram = ee_get_ram_base();
    std::vector<uint8_t> out;
    ld_grids_set_native(false);
    cam0_volume_pass();
    ld_grids_set_native(true);
    out.assign(ram, ram + 0x02000000);
    cam0_volume_pass();
    const bool pass_same = std::memcmp(ram, out.data(), out.size()) == 0;
    const size_t recs = cp != nullptr ? cp->record_count() : 0;
    level();
    if (cp == nullptr || bad != 0 || !pass_list || !pass_same) {
        fail(name, std::to_string(bad) + " of " + std::to_string(n) + " lookups differ" + (cp == nullptr ? ", no copy" : "") + (pass_list ? "" : ", the pass's spheres differ") +
                       (pass_same ? "" : ", the pass's RAM differs") + "; first " + why);
        return;
    }
    std::printf("%-26s ok: %zu records on the level's shapes, %d lookups equal (%d found a record: type 3 %d, 5 %d, 6 %d, 7 %d), the pass's %zu spheres "
                "and RAM equal\n", name, recs, n, hits, by_type[3], by_type[5], by_type[6], by_type[7], spheres.size());
}

void check_keys() {
    const char* name = "grids_keys";
    if (!selected(name)) return;
    level();
    ld_grids_set_check(false);
    std::vector<std::string> bad;
    auto expect = [&](bool ok, const char* what) {
        if (!ok) bad.push_back(what);
    };
    // the zone grids
    make_zone(kZone[0], 0);
    make_zone(kZone[1], 0x5a5a5a5au);
    auto at = [](uint32_t g) {
        w32(OV(0x0015f600), g);
        return zone_grid_cell(30, 20, 10);
    };
    LdGridsStats s0 = ld_grids_stats();
    expect(at(kZone[0]) == kZone[0] + 0x100, "zone A's first cell");
    expect(at(kZone[1]) == kZone[1] + 0x100, "zone B's first cell");
    LdGridsStats s1 = ld_grids_stats();
    expect(at(kZone[0]) == kZone[0] + 0x100 && ld_grids_stats().parses == s1.parses && ld_grids_stats().reused == s1.reused + 1, "zone A kept");
    expect(s1.parses == s0.parses + 2, "zone A and B parsed");
    w32(OV(0x0015f600), kZone[1]);
    (void)zone_grid_cell(30, 20, 10);
    w16(kZone[0] + 0x24, 1);  // (A's cell (30, 20, 10): mask 1 now, while B is current)
    s1 = ld_grids_stats();
    expect(at(kZone[0]) == kZone[0] + 0x180 && ld_grids_stats().parses == s1.parses + 1, "zone A changed behind B: parsed again");
    ld_grids_level_started();
    s1 = ld_grids_stats();
    expect(at(kZone[0]) == kZone[0] + 0x180 && ld_grids_stats().parses == s1.parses + 1, "zone A after a level's start: parsed again");
    w16(kZone[0] + 0x24, 0);  // (an edit in place, A current)
    const uint32_t kept = at(kZone[0]);
    ld_grids_set_check(true);
    s1 = ld_grids_stats();
    expect(at(kZone[0]) == kZone[0] + 0x100 && ld_grids_stats().stale == s1.stale + 1, "zone A edited in place: found by the check mode");
    ld_grids_set_check(false);
    // the camera-volume grids
    make_cam(kCam[0], 1);
    make_cam(kCam[1], 2);
    auto rec_z = [](uint32_t t) {
        w32(OV(0x0015ef80), t);
        const Vec4 p{24.0f, 40.0f, 0.0f, 1.0f};
        (void)cam_volume_find(p, 0.0f);
        const std::shared_ptr<const CamGridCopy> c = ld_grids_cam(t);
        const int32_t li = c != nullptr ? c->list_of(2 * 64 + 1) : -1;
        return li >= 0 ? c->record(c->list(li), 0).w[2] : 0u;
    };
    s0 = ld_grids_stats();
    expect(rec_z(kCam[0]) == f2u(1.0f), "camera grid A's record");
    expect(rec_z(kCam[1]) == f2u(2.0f), "camera grid B's record");
    s1 = ld_grids_stats();
    expect(rec_z(kCam[0]) == f2u(1.0f) && ld_grids_stats().parses == s1.parses && s1.parses == s0.parses + 2, "camera grid A kept");
    w32(OV(0x0015ef80), kCam[1]);
    (void)cam_volume_find(Vec4{24.0f, 40.0f, 0.0f, 1.0f}, 0.0f);
    wf(kCam[0] + 0x4210 + 8, 3.0f);
    s1 = ld_grids_stats();
    expect(rec_z(kCam[0]) == f2u(3.0f) && ld_grids_stats().parses == s1.parses + 1, "camera grid A changed behind B: parsed again");
    ld_grids_level_started();
    s1 = ld_grids_stats();
    expect(rec_z(kCam[0]) == f2u(3.0f) && ld_grids_stats().parses == s1.parses + 1, "camera grid A after a level's start: parsed again");
    wf(kCam[0] + 0x4210 + 8, 4.0f);  // (in place, A current)
    const uint32_t kept_cam = rec_z(kCam[0]);
    ld_grids_set_check(true);
    s1 = ld_grids_stats();
    expect(rec_z(kCam[0]) == f2u(4.0f) && ld_grids_stats().stale == s1.stale + 1, "camera grid A edited in place: found by the check mode");
    level();
    if (!bad.empty()) {
        std::string w;
        for (const std::string& b : bad) w += (w.empty() ? "" : "; ") + b;
        fail(name, w);
        return;
    }
    std::printf("%-26s ok: A / B / A kept, a changed grid parsed again at its switch, a level's start parsed again, an edit in place found by the check "
                "mode (without it the kept copy: zone %s, camera %s) - both grids\n", name, kept == kZone[0] + 0x180 ? "kept" : "not kept",
                kept_cam == f2u(3.0f) ? "kept" : "not kept");
}

// 989snd's IOP (batch 85's): replies a function of the command, the stream system's disc reads
// (0x38) done at once
void iop_stand_in() {  // (batch 89's copy)
    auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
        const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
        return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
    };
    sif_rpc_register(0x123456, [result](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
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
}

void check_title() {
    const char* name = "grids_title";
    // (alone only: ORACLE_ONLY=grids_title on level 0; a level's set-up after the front end's state does not run)
    const char* only = std::getenv("ORACLE_ONLY");
    if (!selected(name) || g_oracle_level != 0 || only == nullptr || std::strcmp(only, name) != 0) return;
    // (batch 89's set-up: the null page as RAM, the front end's oracle mode, the IOP's stand-in, the world chain kept)
    const bool null_ram = g_ee_null_is_ram, chain_forced = g_world_chain_forced;
    g_ee_null_is_ram = true;
    g_oracle_front = true;
    g_world_chain_forced = true;
    iop_stand_in();
    front_image_load();
    ovl_reloc_set_level(OVL_FRONT);
    mc_reset();
    boot_start();
    ee_write<uint16_t>(0x001518d0, 512);
    ee_write<uint16_t>(0x001518d2, 416);
    w32(0x0015ec8c, 1);
    w32(0x00137b00, 0);
    // the title's sound tables (the main after its intro; the bank's handle as the IOP gave it), as batch 89's
    const uint32_t bank = 0x100000;
    sw(0x001861e0 + 0x9c, bank);
    sw(0x00186100 + 0xdc, bank);
    sw(0x0015f634, 0x00186100);
    sw(0x0015f630, 7);
    for (uint32_t o : {0x1cu, 0x3cu, 0x5cu, 0x7cu, 0x9cu, 0xbcu}) sw(0x00186100 + o, bank);
    for (uint32_t o : {0x1cu, 0x3cu, 0x5cu, 0x7cu}) sw(0x001861e0 + o, bank);
    title_loop_start();
    title_loop_ready();
    for (int i = 0; i < 70; ++i) title_loop_frame();
    zone_compare(name, lwu(OV(0x0015f600)), 5000, nullptr);
    g_ee_null_is_ram = null_ram;
    g_oracle_front = false;
    g_world_chain_forced = chain_forced;
}

}  // namespace

void batch_544() {
    if (g_only_batch != 544) return;
    g_batch = 544;
    std::printf("batch 544: the grids' host copies on level %d\n", g_oracle_level);
    ld_grids_set_native(true);
    ld_grids_set_check(true);
    check_title();  // (first, as batch 89's states: from the process's start; with ORACLE_ONLY=grids_title only)
    std::vector<XYZ> cells;
    check_zone_cells(&cells);
    check_zones_update(cells);
    check_cam_cells();
    check_cam_find();
    check_cam_synthetic();
    check_keys();
    const LdGridsStats st = ld_grids_stats();
    std::printf("  (the copies: %llu parses in %.1f ms, %llu kept copies taken again, %llu found stale, %llu parse errors, %llu pointers not held, %llu "
                "values differed in the check mode)\n",
                static_cast<unsigned long long>(st.parses), st.parse_ms, static_cast<unsigned long long>(st.reused), static_cast<unsigned long long>(st.stale),
                static_cast<unsigned long long>(st.parse_errors), static_cast<unsigned long long>(st.unheld), static_cast<unsigned long long>(st.differs));
    if (st.parse_errors != 0) fail("grids_parse_errors", std::to_string(st.parse_errors) + " parses differed from EE memory right after them");
}

}  // namespace rac1::oracle
